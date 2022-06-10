/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x86_SharedICHelpers_x86_inl_h
#define jit_x86_SharedICHelpers_x86_inl_h

#include "jit/BaselineFrame.h"
#include "jit/SharedICHelpers.h"

#include "jit/MacroAssembler-inl.h"

namespace js {
namespace jit {

inline void EmitBaselineTailCallVM(TrampolinePtr target, MacroAssembler& masm,
                                   uint32_t argSize) {
  // We assume during this that R0 and R1 have been pushed.

  // Compute frame size.
  masm.movl(FramePointer, eax);
  masm.addl(Imm32(BaselineFrame::FramePointerOffset), eax);
  masm.subl(BaselineStackReg, eax);

#ifdef DEBUG
  // Store frame size without VMFunction arguments for debug assertions.
  masm.movl(eax, ebx);
  masm.subl(Imm32(argSize), ebx);
  Address frameSizeAddr(FramePointer,
                        BaselineFrame::reverseOffsetOfDebugFrameSize());
  masm.store32(ebx, frameSizeAddr);
#endif

  // Push frame descriptor and perform the tail call.
  masm.makeFrameDescriptor(eax, FrameType::BaselineJS, ExitFrameLayout::Size());
  masm.push(eax);
  masm.push(ICTailCallReg);
  masm.jump(target);
}

inline void EmitBaselineCreateStubFrameDescriptor(MacroAssembler& masm,
                                                  Register reg,
                                                  uint32_t headerSize) {
  // Compute stub frame size.
  masm.movl(FramePointer, reg);
  masm.addl(Imm32(BaselineStubFrameLayout::FramePointerOffset), reg);
  masm.subl(BaselineStackReg, reg);

  masm.makeFrameDescriptor(reg, FrameType::BaselineStub, headerSize);
}

inline void EmitBaselineCallVM(TrampolinePtr target, MacroAssembler& masm) {
  EmitBaselineCreateStubFrameDescriptor(masm, eax, ExitFrameLayout::Size());
  masm.push(eax);
  masm.call(target);
}

inline void EmitBaselineEnterStubFrame(MacroAssembler& masm, Register scratch) {
  // Compute frame size. Because the return address is still on the stack,
  // this is:
  //
  //   FramePointer
  //   + BaselineFrame::FramePointerOffset
  //   - BaselineStackReg
  //   - sizeof(return address)
  //
  // The two constants cancel each other out, so we can just calculate
  // FramePointer - BaselineStackReg.

  static_assert(
      BaselineFrame::FramePointerOffset == sizeof(void*),
      "FramePointerOffset must be the same as the return address size");

  masm.movl(FramePointer, scratch);
  masm.subl(BaselineStackReg, scratch);

#ifdef DEBUG
  Address frameSizeAddr(FramePointer,
                        BaselineFrame::reverseOffsetOfDebugFrameSize());
  masm.store32(scratch, frameSizeAddr);
#endif

  // Note: when making changes here,  don't forget to update StubFrameSize
  // if needed.

  // Push the return address that's currently on top of the stack.
  masm.Push(Operand(BaselineStackReg, 0));

  // Replace the original return address with the frame descriptor.
  masm.makeFrameDescriptor(scratch, FrameType::BaselineJS,
                           BaselineStubFrameLayout::Size());
  masm.storePtr(scratch, Address(BaselineStackReg, sizeof(uintptr_t)));

  // Save old frame pointer, stack pointer and stub reg.
  masm.Push(FramePointer);
  masm.mov(BaselineStackReg, FramePointer);

  masm.Push(ICStubReg);
}

}  // namespace jit
}  // namespace js

#endif /* jit_x86_SharedICHelpers_x86_inl_h */
