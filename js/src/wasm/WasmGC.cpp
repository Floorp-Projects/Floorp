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

#include "wasm/WasmGC.h"

namespace js {
namespace wasm {

void EmitWasmPreBarrierGuard(MacroAssembler& masm, Register tls,
                             Register scratch, Register valueAddr,
                             Label* skipBarrier) {
  // If no incremental GC has started, we don't need the barrier.
  masm.loadPtr(
    Address(tls, offsetof(TlsData, addressOfNeedsIncrementalBarrier)),
    scratch);
  masm.branchTest32(Assembler::Zero, Address(scratch, 0), Imm32(0x1),
                    skipBarrier);

  // If the previous value is null, we don't need the barrier.
  masm.loadPtr(Address(valueAddr, 0), scratch);
  masm.branchTestPtr(Assembler::Zero, scratch, scratch, skipBarrier);
}

void EmitWasmPreBarrierCall(MacroAssembler& masm, Register tls,
                            Register scratch, Register valueAddr) {
  MOZ_ASSERT(valueAddr == PreBarrierReg);

  masm.loadPtr(Address(tls, offsetof(TlsData, instance)), scratch);
  masm.loadPtr(Address(scratch, Instance::offsetOfPreBarrierCode()), scratch);
#if defined(DEBUG) && defined(JS_CODEGEN_ARM64)
  // The prebarrier assumes that x28 == sp.
  Label ok;
  masm.Cmp(sp, vixl::Operand(x28));
  masm.B(&ok, Assembler::Equal);
  masm.breakpoint();
  masm.bind(&ok);
#endif
  masm.call(scratch);
}

void EmitWasmPostBarrierGuard(MacroAssembler& masm, const Maybe<Register>& object,
                              Register otherScratch, Register setValue,
                              Label* skipBarrier) {
  // If the pointer being stored is null, no barrier.
  masm.branchTestPtr(Assembler::Zero, setValue, setValue, skipBarrier);

  // If there is a containing object and it is in the nursery, no barrier.
  if (object) {
    masm.branchPtrInNurseryChunk(Assembler::Equal, *object, otherScratch,
                                 skipBarrier);
  }

  // If the pointer being stored is to a tenured object, no barrier.
  masm.branchPtrInNurseryChunk(Assembler::NotEqual, setValue, otherScratch,
                               skipBarrier);
}

}  // namespace wasm
}  // namespace js
