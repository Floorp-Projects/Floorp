/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_baseline_helpers_x64_h__) && defined(JS_ION)
#define jsion_baseline_helpers_x64_h__

#include "ion/IonMacroAssembler.h"
#include "ion/BaselineRegisters.h"
#include "ion/BaselineIC.h"

namespace js {
namespace ion {

inline void
EmitRestoreTailCallReg(MacroAssembler &masm)
{
    masm.pop(BaselineTailCallReg);
}

inline void
EmitCallIC(CodeOffsetLabel *patchOffset, MacroAssembler &masm)
{
    // Move ICEntry offset into BaselineStubReg
    CodeOffsetLabel offset = masm.movWithPatch(ImmWord(-1), BaselineStubReg);
    *patchOffset = offset;

    // Load stub pointer into BaselineStubReg
    masm.movq(Operand(BaselineStubReg, (int32_t) ICEntry::offsetOfFirstStub()),
              BaselineStubReg);

    // Load stubcode pointer from BaselineStubEntry into BaselineTailCallReg
    // BaselineTailCallReg will always be unused in the contexts where ICs are called.
    masm.movq(Operand(BaselineStubReg, (int32_t) ICStub::offsetOfStubCode()),
              BaselineTailCallReg);

    // Call the stubcode.
    masm.call(BaselineTailCallReg);
}

inline void
EmitReturnFromIC(MacroAssembler &masm)
{
    masm.ret();
}

inline void
EmitTailCall(IonCode *target, MacroAssembler &masm, uint32_t argSize)
{
    // We an assume during this that R0 and R1 have been pushed.
    masm.movq(BaselineFrameReg, ScratchReg);
    masm.addq(Imm32(BaselineFrame::FramePointerOffset), ScratchReg);
    masm.subq(BaselineStackReg, ScratchReg);

    // Store frame size without VMFunction arguments for GC marking.
    masm.movq(ScratchReg, rdx);
    masm.subq(Imm32(argSize), rdx);
    masm.storePtr(rdx, Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfFrameSize()));

    // Push frame descriptor and perform the tail call.
    masm.makeFrameDescriptor(ScratchReg, IonFrame_BaselineJS);
    masm.push(ScratchReg);
    masm.push(BaselineTailCallReg);
    masm.jmp(target);
}

inline void
EmitStubGuardFailure(MacroAssembler &masm)
{
    // NOTE: This routine assumes that the stub guard code left the stack in the
    // same state it was in when it was entered.

    // BaselineStubEntry points to the current stub.

    // Load next stub into BaselineStubReg
    masm.movq(Operand(BaselineStubReg, ICStub::offsetOfNext()), BaselineStubReg);

    // Load stubcode pointer from BaselineStubEntry into BaselineTailCallReg
    // BaselineTailCallReg will always be unused in the contexts where IC stub guards fail
    masm.movq(Operand(BaselineStubReg, ICStub::offsetOfStubCode()), BaselineTailCallReg);

    // Return address is already loaded, just jump to the next stubcode.
    masm.jmp(Operand(BaselineTailCallReg));
}


} // namespace ion
} // namespace js

#endif

