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

#include "wasm/WasmRuntime.h"

#include "mozilla/BinarySearch.h"

#include "jit/MacroAssembler.h"

#include "wasm/WasmInstance.h"
#include "wasm/WasmStubs.h"

using namespace js;
using namespace jit;
using namespace wasm;

using mozilla::BinarySearchIf;

static const unsigned BUILTIN_THUNK_LIFO_SIZE = 128;
static const CodeKind BUILTIN_THUNK_CODEKIND = CodeKind::OTHER_CODE;

static bool
GenerateBuiltinThunk(JSContext* cx, void* func, ABIFunctionType abiType, UniqueBuiltinThunk* thunk)
{
    if (!cx->compartment()->ensureJitCompartmentExists(cx))
        return false;

    LifoAlloc lifo(BUILTIN_THUNK_LIFO_SIZE);
    TempAllocator tempAlloc(&lifo);
    MacroAssembler masm(MacroAssembler::WasmToken(), tempAlloc);

    CallableOffsets offsets = GenerateBuiltinImportExit(masm, abiType, func);

    masm.finish();
    if (masm.oom())
        return false;

    // The executable allocator operates on pointer-aligned sizes.
    uint32_t codeLength = AlignBytes(masm.bytesNeeded(), sizeof(void*));

    ExecutablePool* pool = nullptr;
    ExecutableAllocator& allocator = cx->runtime()->jitRuntime()->execAlloc();
    uint8_t* codeBase = (uint8_t*) allocator.alloc(cx, codeLength, &pool, BUILTIN_THUNK_CODEKIND);
    if (!codeBase)
        return false;

    {
        AutoWritableJitCode awjc(cx->runtime(), codeBase, codeLength);
        AutoFlushICache afc("GenerateBuiltinThunk");

        masm.executableCopy(codeBase);
        memset(codeBase + masm.bytesNeeded(), 0, codeLength - masm.bytesNeeded());

        MOZ_ASSERT(!masm.numSymbolicAccesses(), "doesn't need any patching");
    }

    *thunk = js::MakeUnique<BuiltinThunk>(codeBase, codeLength, pool, offsets);
    return !!*thunk;
}

struct BuiltinMatcher
{
    const uint8_t* address;
    explicit BuiltinMatcher(const uint8_t* address) : address(address) {}
    int operator()(const UniqueBuiltinThunk& thunk) const {
        if (address < thunk->base)
            return -1;
        if (uintptr_t(address) >= uintptr_t(thunk->base) + thunk->size)
            return 1;
        return 0;
    }
};

bool
wasm::Runtime::getBuiltinThunk(JSContext* cx, void* funcPtr, ABIFunctionType abiType,
                               void** thunkPtr)
{
    TypedFuncPtr lookup(funcPtr, abiType);
    auto ptr = builtinThunkMap_.lookupForAdd(lookup);
    if (ptr) {
        *thunkPtr = ptr->value();
        return true;
    }

    UniqueBuiltinThunk thunk;
    if (!GenerateBuiltinThunk(cx, funcPtr, abiType, &thunk))
        return false;

    // Maintain sorted order of thunk addresses.
    size_t i;
    size_t size = builtinThunkVector_.length();
    if (BinarySearchIf(builtinThunkVector_, 0, size, BuiltinMatcher(thunk->base), &i))
        MOZ_CRASH("clobbering memory");

    *thunkPtr = thunk->base;

    return builtinThunkVector_.insert(builtinThunkVector_.begin() + i, Move(thunk)) &&
           builtinThunkMap_.add(ptr, lookup, *thunkPtr);
}

static ABIFunctionType
ToABIFunctionType(const Sig& sig)
{
    const ValTypeVector& args = sig.args();
    ExprType ret = sig.ret();

    uint32_t abiType;
    switch (ret) {
      case ExprType::F32: abiType = ArgType_Float32 << RetType_Shift; break;
      case ExprType::F64: abiType = ArgType_Double << RetType_Shift; break;
      default:            MOZ_CRASH("unhandled ret type");
    }

    for (size_t i = 0; i < args.length(); i++) {
        switch (args[i]) {
          case ValType::F32: abiType |= (ArgType_Float32 << (ArgType_Shift * (i + 1))); break;
          case ValType::F64: abiType |= (ArgType_Double << (ArgType_Shift * (i + 1))); break;
          default:           MOZ_CRASH("unhandled arg type");
        }
    }

    return ABIFunctionType(abiType);
}

bool
wasm::Runtime::getBuiltinThunk(JSContext* cx, void* funcPtr, const Sig& sig, void** thunkPtr)
{
    ABIFunctionType abiType = ToABIFunctionType(sig);
#ifdef JS_SIMULATOR
    funcPtr = Simulator::RedirectNativeFunction(funcPtr, abiType);
#endif
    return getBuiltinThunk(cx, funcPtr, abiType, thunkPtr);
}

BuiltinThunk*
wasm::Runtime::lookupBuiltin(void* pc)
{
    size_t index;
    size_t length = builtinThunkVector_.length();
    if (!BinarySearchIf(builtinThunkVector_, 0, length, BuiltinMatcher((uint8_t*)pc), &index))
        return nullptr;
    return builtinThunkVector_[index].get();
}

void
wasm::Runtime::destroy()
{
    builtinThunkVector_.clear();
    if (builtinThunkMap_.initialized())
        builtinThunkMap_.clear();
}

BuiltinThunk::~BuiltinThunk()
{
    executablePool->release(size, BUILTIN_THUNK_CODEKIND);
}
