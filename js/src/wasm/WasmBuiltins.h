/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
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

#ifndef wasm_builtins_h
#define wasm_builtins_h

#include "wasm/WasmTypes.h"

namespace js {
namespace jit {
struct ResumeFromException;
}
namespace wasm {

class WasmFrameIter;

// These provide argument type information for a subset of the SymbolicAddress
// targets, for which type info is needed to generate correct stackmaps.

extern const SymbolicAddressSignature SASigSinD;
extern const SymbolicAddressSignature SASigCosD;
extern const SymbolicAddressSignature SASigTanD;
extern const SymbolicAddressSignature SASigASinD;
extern const SymbolicAddressSignature SASigACosD;
extern const SymbolicAddressSignature SASigATanD;
extern const SymbolicAddressSignature SASigCeilD;
extern const SymbolicAddressSignature SASigCeilF;
extern const SymbolicAddressSignature SASigFloorD;
extern const SymbolicAddressSignature SASigFloorF;
extern const SymbolicAddressSignature SASigTruncD;
extern const SymbolicAddressSignature SASigTruncF;
extern const SymbolicAddressSignature SASigNearbyIntD;
extern const SymbolicAddressSignature SASigNearbyIntF;
extern const SymbolicAddressSignature SASigExpD;
extern const SymbolicAddressSignature SASigLogD;
extern const SymbolicAddressSignature SASigPowD;
extern const SymbolicAddressSignature SASigATan2D;
extern const SymbolicAddressSignature SASigMemoryGrow;
extern const SymbolicAddressSignature SASigMemorySize;
extern const SymbolicAddressSignature SASigWaitI32;
extern const SymbolicAddressSignature SASigWaitI64;
extern const SymbolicAddressSignature SASigWake;
extern const SymbolicAddressSignature SASigMemCopy;
extern const SymbolicAddressSignature SASigMemCopyShared;
extern const SymbolicAddressSignature SASigDataDrop;
extern const SymbolicAddressSignature SASigMemFill;
extern const SymbolicAddressSignature SASigMemFillShared;
extern const SymbolicAddressSignature SASigMemInit;
extern const SymbolicAddressSignature SASigTableCopy;
extern const SymbolicAddressSignature SASigElemDrop;
extern const SymbolicAddressSignature SASigTableFill;
extern const SymbolicAddressSignature SASigTableGet;
extern const SymbolicAddressSignature SASigTableGrow;
extern const SymbolicAddressSignature SASigTableInit;
extern const SymbolicAddressSignature SASigTableSet;
extern const SymbolicAddressSignature SASigTableSize;
extern const SymbolicAddressSignature SASigRefFunc;
extern const SymbolicAddressSignature SASigPreBarrierFiltering;
extern const SymbolicAddressSignature SASigPostBarrier;
extern const SymbolicAddressSignature SASigPostBarrierFiltering;
extern const SymbolicAddressSignature SASigStructNew;
extern const SymbolicAddressSignature SASigStructNarrow;
#ifdef ENABLE_WASM_EXCEPTIONS
extern const SymbolicAddressSignature SASigExceptionNew;
extern const SymbolicAddressSignature SASigThrowException;
extern const SymbolicAddressSignature SASigGetLocalExceptionIndex;
#endif
extern const SymbolicAddressSignature SASigRefTest;
extern const SymbolicAddressSignature SASigRttSub;

// A SymbolicAddress that NeedsBuiltinThunk() will call through a thunk to the
// C++ function. This will be true for all normal calls from normal wasm
// function code. Only calls to C++ from other exits/thunks do not need a thunk.

bool NeedsBuiltinThunk(SymbolicAddress sym);

// This function queries whether pc is in one of the process's builtin thunks
// and, if so, returns the CodeRange and pointer to the code segment that the
// CodeRange is relative to.

bool LookupBuiltinThunk(void* pc, const CodeRange** codeRange,
                        uint8_t** codeBase);

// EnsureBuiltinThunksInitialized() must be called, and must succeed, before
// SymbolicAddressTarget() or MaybeGetBuiltinThunk(). This function creates all
// thunks for the process. ReleaseBuiltinThunks() should be called before
// ReleaseProcessExecutableMemory() so that the latter can assert that all
// executable code has been released.

bool EnsureBuiltinThunksInitialized();

bool HandleThrow(JSContext* cx, WasmFrameIter& iter,
                 jit::ResumeFromException* rfe);

void* SymbolicAddressTarget(SymbolicAddress sym);

void* ProvisionalLazyJitEntryStub();

void* MaybeGetBuiltinThunk(JSFunction* f, const FuncType& funcType);

void ReleaseBuiltinThunks();

void* AddressOf(SymbolicAddress imm, jit::ABIFunctionType* abiType);

#ifdef WASM_CODEGEN_DEBUG
void PrintI32(int32_t val);
void PrintF32(float val);
void PrintF64(double val);
void PrintPtr(uint8_t* val);
void PrintText(const char* out);
#endif

}  // namespace wasm
}  // namespace js

#endif  // wasm_builtins_h
