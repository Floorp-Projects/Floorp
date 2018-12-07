/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2019 Mozilla Foundation
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

#ifndef wasm_gc_h
#define wasm_gc_h

#include "jit/MacroAssembler.h"

namespace js {
namespace wasm {

using namespace js::jit;

// StackArgAreaSizeUnaligned returns the size, in bytes, of the stack arg area
// size needed to pass |argTypes|, excluding any alignment padding beyond the
// size of the area as a whole.  The size is as determined by the platforms
// native ABI.
//
// StackArgAreaSizeAligned returns the same, but rounded up to the nearest 16
// byte boundary.
//
// Note, StackArgAreaSize{Unaligned,Aligned}() must process all the arguments
// in order to take into account all necessary alignment constraints.  The
// signature must include any receiver argument -- in other words, it must be
// the complete native-ABI-level call signature.
template <class T>
static inline size_t StackArgAreaSizeUnaligned(const T& argTypes) {
  ABIArgIter<const T> i(argTypes);
  while (!i.done()) {
    i++;
  }
  return i.stackBytesConsumedSoFar();
}

static inline size_t AlignStackArgAreaSize(size_t unalignedSize) {
  return AlignBytes(unalignedSize, 16u);
}

template <class T>
static inline size_t StackArgAreaSizeAligned(const T& argTypes) {
  return AlignStackArgAreaSize(StackArgAreaSizeUnaligned(argTypes));
}

// Shared write barrier code.
//
// A barriered store looks like this:
//
//   Label skipPreBarrier;
//   EmitWasmPreBarrierGuard(..., &skipPreBarrier);
//   <COMPILER-SPECIFIC ACTIONS HERE>
//   EmitWasmPreBarrierCall(...);
//   bind(&skipPreBarrier);
//
//   <STORE THE VALUE IN MEMORY HERE>
//
//   Label skipPostBarrier;
//   <COMPILER-SPECIFIC ACTIONS HERE>
//   EmitWasmPostBarrierGuard(..., &skipPostBarrier);
//   <CALL POST-BARRIER HERE IN A COMPILER-SPECIFIC WAY>
//   bind(&skipPostBarrier);
//
// The actions are divided up to allow other actions to be placed between them,
// such as saving and restoring live registers.  The postbarrier call invokes
// C++ and will kill all live registers.

// Before storing a GC pointer value in memory, skip to `skipBarrier` if the
// prebarrier is not needed.  Will clobber `scratch`.
//
// It is OK for `tls` and `scratch` to be the same register.

void EmitWasmPreBarrierGuard(MacroAssembler& masm, Register tls,
                             Register scratch, Register valueAddr,
                             Label* skipBarrier);

// Before storing a GC pointer value in memory, call out-of-line prebarrier
// code. This assumes `PreBarrierReg` contains the address that will be updated.
// On ARM64 it also assums that x28 (the PseudoStackPointer) has the same value
// as SP.  `PreBarrierReg` is preserved by the barrier function.  Will clobber
// `scratch`.
//
// It is OK for `tls` and `scratch` to be the same register.

void EmitWasmPreBarrierCall(MacroAssembler& masm, Register tls,
                            Register scratch, Register valueAddr);

// After storing a GC pointer value in memory, skip to `skipBarrier` if a
// postbarrier is not needed.  If the location being set is in an heap-allocated
// object then `object` must reference that object; otherwise it should be None.
// The value that was stored is `setValue`.  Will clobber `otherScratch` and
// will use other available scratch registers.
//
// `otherScratch` cannot be a designated scratch register.

void EmitWasmPostBarrierGuard(MacroAssembler& masm, const Maybe<Register>& object,
                              Register otherScratch, Register setValue,
                              Label* skipBarrier);

}  // namespace wasm
}  // namespace js

#endif  // wasm_gc_h
