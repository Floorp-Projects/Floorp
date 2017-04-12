/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2017 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef wasm_runtime_h
#define wasm_runtime_h

#include "NamespaceImports.h"

#include "jit/IonTypes.h"
#include "wasm/WasmTypes.h"

using mozilla::HashGeneric;

namespace js {

namespace jit {
    class ExecutablePool;
}

namespace wasm {

void*
AddressOf(SymbolicAddress imm, jit::ABIFunctionType*);

struct TypedFuncPtr
{
    void* funcPtr;
    jit::ABIFunctionType abiType;

    TypedFuncPtr(void* funcPtr, jit::ABIFunctionType abiType)
      : funcPtr(funcPtr),
        abiType(abiType)
    {}

    typedef TypedFuncPtr Lookup;
    static HashNumber hash(const Lookup& l) {
        return HashGeneric(l.funcPtr, uint32_t(l.abiType));
    }
    static bool match(const TypedFuncPtr& lhs, const Lookup& rhs) {
        return lhs.funcPtr == rhs.funcPtr && lhs.abiType == rhs.abiType;
    }
};

typedef HashMap<TypedFuncPtr, void*, TypedFuncPtr, SystemAllocPolicy> BuiltinThunkMap;

struct BuiltinThunk
{
    jit::ExecutablePool* executablePool;
    CodeRange codeRange;
    size_t size;
    uint8_t* base;

    BuiltinThunk(uint8_t* base, size_t size, jit::ExecutablePool* executablePool,
                 CallableOffsets offsets)
      : executablePool(executablePool),
        codeRange(CodeRange(CodeRange::ImportNativeExit, offsets)),
        size(size),
        base(base)
    {}
    ~BuiltinThunk();
};

typedef UniquePtr<BuiltinThunk> UniqueBuiltinThunk;
typedef Vector<UniqueBuiltinThunk, 4, SystemAllocPolicy> BuiltinThunkVector;

// wasm::Runtime contains all the needed information for wasm that has the
// lifetime of a JSRuntime.

class Runtime
{
    BuiltinThunkMap builtinThunkMap_;
    BuiltinThunkVector builtinThunkVector_;

    bool getBuiltinThunk(JSContext* cx, void* funcPtr, jit::ABIFunctionType type, void** thunkPtr);

  public:
    bool init() { return builtinThunkMap_.init(); }
    void destroy();

    bool getBuiltinThunk(JSContext* cx, void* funcPtr, const Sig& sig, void** thunkPtr);
    BuiltinThunk* lookupBuiltin(void* pc);
};

} // namespace wasm
} // namespace js

#endif // wasm_runtime_h
