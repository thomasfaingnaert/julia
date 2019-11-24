//===-- abi_llvm.cpp - LLVM Target ABI description --------------*- C++ -*-===//
//
//                         LDC – the LLVM D compiler
//
// This file is distributed under the BSD-style LDC license:
//
// Copyright (c) 2007-2012 LDC Team.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice,
//       this list of conditions and the following disclaimer in the documentation
//       and/or other materials provided with the distribution.
//     * Neither the name of the LDC Team nor the names of its contributors may be
//       used to endorse or promote products derived from this software without
//       specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//===----------------------------------------------------------------------===//
//
// This ABI implementation does whatever LLVM decides is fine.
// It can be useful when first porting Julia to a new platform
// (until a platform-specific implementation can be developed).
//
//===----------------------------------------------------------------------===//

struct ABI_LLVMLayout : AbiLayout {

bool use_sret(jl_datatype_t *ty) override
{
    return false;
}

bool needPassByRef(jl_datatype_t *ty, AttrBuilder &ab) override
{
    return false;
}

Type *preferred_llvm_type(jl_datatype_t *ty, bool isret) const override
{
    // Float16 -> half
    if (ty == jl_float16_type)
        return Type::getHalfTy(jl_LLVMContext);

    // Float32 -> float
    if (ty == jl_float32_type)
        return Type::getFloatTy(jl_LLVMContext);

    // NTuple{N, T} -> [N x T]
    // NTuple{N, VecElement{T}} -> <N x T>
    // Tuple{T, U, ...} -> {T, U}
    if (jl_is_tuple_type(ty)) {
        auto num_params = jl_nparams(ty);
        bool all_equal = true;

        auto first_ty = jl_tparam(ty, 0);

        for (size_t i = 0; i < num_params && all_equal; ++i) {
            all_equal = jl_tparam(ty, i) == first_ty;
        }

        // llvm struct
        if (!all_equal) {
            std::vector<Type*> el_types(num_params);

            for (size_t i = 0; i < num_params; ++i) {
                el_types[i] = preferred_llvm_type((jl_datatype_t*)jl_tparam(ty, i), isret);
                if (!el_types[i]) return NULL;
            }

            return StructType::get(jl_LLVMContext, el_types, false);
        }

        // llvm array
        if (!jl_is_vecelement_type(first_ty)) {
            auto preferred_el_ty = preferred_llvm_type((jl_datatype_t*)first_ty, isret);
            if (!preferred_el_ty) return NULL;

            return ArrayType::get(preferred_el_ty, num_params);
        }

        // llvm vector
        else {
            auto preferred_el_ty = preferred_llvm_type((jl_datatype_t*)jl_tparam(first_ty, 0), isret);
            if (!preferred_el_ty) return NULL;

            return VectorType::get(preferred_el_ty, num_params);
        }

    }

    // struct -> {...}
    if (jl_is_structtype(ty)) {
        auto num_params = jl_datatype_nfields(ty);
        std::vector<Type*> el_types(num_params);

        for (size_t i = 0; i < num_params; ++i) {
            el_types[i] = preferred_llvm_type((jl_datatype_t*)jl_field_type(ty, i), isret);
            if (!el_types[i]) return NULL;
        }

        return StructType::get(jl_LLVMContext, el_types, false);
    }

    return NULL;
}

};
