/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_mips_shared_SharedICHelpers_mips_shared_h
#define jit_mips_shared_SharedICHelpers_mips_shared_h

#include "jit/BaselineFrame.h"
#include "jit/BaselineIC.h"
#include "jit/MacroAssembler.h"
#include "jit/SharedICRegisters.h"

namespace js {
namespace jit {

// Distance from sp to the top Value inside an IC stub (no return address on
// the stack on MIPS).
static const size_t ICStackValueOffset = 0;

inline void
EmitRestoreTailCallReg(MacroAssembler& masm)
{
    // No-op on MIPS because ra register is always holding the return address.
}

inline void
EmitRepushTailCallReg(MacroAssembler& masm)
{
    // No-op on MIPS because ra register is always holding the return address.
}

inline void
EmitCallIC(CodeOffset* patchOffset, MacroAssembler& masm)
{
    // Move ICEntry offset into ICStubReg.
    CodeOffset offset = masm.movWithPatch(ImmWord(-1), ICStubReg);
    *patchOffset = offset;

    // Load stub pointer into ICStubReg.
    masm.loadPtr(Address(ICStubReg, ICEntry::offsetOfFirstStub()), ICStubReg);

    // Load stubcode pointer from BaselineStubEntry.
    // R2 won't be active when we call ICs, so we can use it as scratch.
    masm.loadPtr(Address(ICStubReg, ICStub::offsetOfStubCode()), R2.scratchReg());

    // Call the stubcode via a direct jump-and-link
    masm.call(R2.scratchReg());
}

inline void
EmitEnterTypeMonitorIC(MacroAssembler& masm,
                       size_t monitorStubOffset = ICMonitoredStub::offsetOfFirstMonitorStub())
{
    // This is expected to be called from within an IC, when ICStubReg
    // is properly initialized to point to the stub.
    masm.loadPtr(Address(ICStubReg, (uint32_t) monitorStubOffset), ICStubReg);

    // Load stubcode pointer from BaselineStubEntry.
    // R2 won't be active when we call ICs, so we can use it.
    masm.loadPtr(Address(ICStubReg, ICStub::offsetOfStubCode()), R2.scratchReg());

    // Jump to the stubcode.
    masm.branch(R2.scratchReg());
}

inline void
EmitReturnFromIC(MacroAssembler& masm)
{
    masm.branch(ra);
}

inline void
EmitChangeICReturnAddress(MacroAssembler& masm, Register reg)
{
    masm.movePtr(reg, ra);
}

inline void
EmitBaselineTailCallVM(JitCode* target, MacroAssembler& masm, uint32_t argSize)
{
    Register scratch = R2.scratchReg();

    // Compute frame size.
    masm.movePtr(BaselineFrameReg, scratch);
    masm.addPtr(Imm32(BaselineFrame::FramePointerOffset), scratch);
    masm.subPtr(BaselineStackReg, scratch);

    // Store frame size without VMFunction arguments for GC marking.
    masm.subPtr(Imm32(argSize), scratch);
    masm.store32(scratch, Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfFrameSize()));
    masm.addPtr(Imm32(argSize), scratch);

    // Push frame descriptor and perform the tail call.
    // ICTailCallReg (ra) already contains the return address (as we
    // keep it there through the stub calls), but the VMWrapper code being
    // called expects the return address to also be pushed on the stack.
    MOZ_ASSERT(ICTailCallReg == ra);
    masm.makeFrameDescriptor(scratch, JitFrame_BaselineJS, ExitFrameLayout::Size());
    masm.subPtr(Imm32(sizeof(CommonFrameLayout)), StackPointer);
    masm.storePtr(scratch, Address(StackPointer, CommonFrameLayout::offsetOfDescriptor()));
    masm.storePtr(ra, Address(StackPointer, CommonFrameLayout::offsetOfReturnAddress()));

    masm.branch(target);
}

inline void
EmitIonTailCallVM(JitCode* target, MacroAssembler& masm, uint32_t stackSize)
{
    Register scratch = R2.scratchReg();

    masm.loadPtr(Address(sp, stackSize), scratch);
    masm.rshiftPtr(Imm32(FRAMESIZE_SHIFT), scratch);
    masm.addPtr(Imm32(stackSize + JitStubFrameLayout::Size() - sizeof(intptr_t)), scratch);

    // Push frame descriptor and perform the tail call.
    MOZ_ASSERT(ICTailCallReg == ra);
    masm.makeFrameDescriptor(scratch, JitFrame_IonJS, ExitFrameLayout::Size());
    masm.push(scratch);
    masm.push(ICTailCallReg);
    masm.branch(target);
}

inline void
EmitBaselineCreateStubFrameDescriptor(MacroAssembler& masm, Register reg, uint32_t headerSize)
{
    // Compute stub frame size. We have to add two pointers: the stub reg and
    // previous frame pointer pushed by EmitEnterStubFrame.
    masm.movePtr(BaselineFrameReg, reg);
    masm.addPtr(Imm32(sizeof(intptr_t) * 2), reg);
    masm.subPtr(BaselineStackReg, reg);

    masm.makeFrameDescriptor(reg, JitFrame_BaselineStub, headerSize);
}

inline void
EmitBaselineCallVM(JitCode* target, MacroAssembler& masm)
{
    Register scratch = R2.scratchReg();
    EmitBaselineCreateStubFrameDescriptor(masm, scratch, ExitFrameLayout::Size());
    masm.push(scratch);
    masm.call(target);
}

inline void
EmitIonCallVM(JitCode* target, size_t stackSlots, MacroAssembler& masm)
{
    uint32_t descriptor = MakeFrameDescriptor(masm.framePushed(), JitFrame_IonStub,
                                              ExitFrameLayout::Size());
    masm.Push(Imm32(descriptor));
    masm.callJit(target);

    // Remove rest of the frame left on the stack. We remove the return address
    // which is implicitly popped when returning.
    size_t framePop = sizeof(ExitFrameLayout) - sizeof(void*);

    // Pop arguments from framePushed.
    masm.implicitPop(stackSlots * sizeof(void*) + framePop);
}

struct BaselineStubFrame {
    uintptr_t savedFrame;
    uintptr_t savedStub;
    uintptr_t returnAddress;
    uintptr_t descriptor;
};

static const uint32_t STUB_FRAME_SIZE = sizeof(BaselineStubFrame);
static const uint32_t STUB_FRAME_SAVED_STUB_OFFSET = offsetof(BaselineStubFrame, savedStub);

inline void
EmitBaselineEnterStubFrame(MacroAssembler& masm, Register scratch)
{
    MOZ_ASSERT(scratch != ICTailCallReg);

    // Compute frame size.
    masm.movePtr(BaselineFrameReg, scratch);
    masm.addPtr(Imm32(BaselineFrame::FramePointerOffset), scratch);
    masm.subPtr(BaselineStackReg, scratch);

    masm.store32(scratch, Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfFrameSize()));

    // Note: when making changes here, don't forget to update
    // BaselineStubFrame if needed.

    // Push frame descriptor and return address.
    masm.makeFrameDescriptor(scratch, JitFrame_BaselineJS, BaselineStubFrameLayout::Size());
    masm.subPtr(Imm32(STUB_FRAME_SIZE), StackPointer);
    masm.storePtr(scratch, Address(StackPointer, offsetof(BaselineStubFrame, descriptor)));
    masm.storePtr(ICTailCallReg, Address(StackPointer,
                                               offsetof(BaselineStubFrame, returnAddress)));

    // Save old frame pointer, stack pointer and stub reg.
    masm.storePtr(ICStubReg, Address(StackPointer,
                                           offsetof(BaselineStubFrame, savedStub)));
    masm.storePtr(BaselineFrameReg, Address(StackPointer,
                                            offsetof(BaselineStubFrame, savedFrame)));
    masm.movePtr(BaselineStackReg, BaselineFrameReg);

    // Stack should remain aligned.
    masm.assertStackAlignment(sizeof(Value), 0);
}

inline void
EmitBaselineLeaveStubFrame(MacroAssembler& masm, bool calledIntoIon = false)
{
    // Ion frames do not save and restore the frame pointer. If we called
    // into Ion, we have to restore the stack pointer from the frame descriptor.
    // If we performed a VM call, the descriptor has been popped already so
    // in that case we use the frame pointer.
    if (calledIntoIon) {
        masm.pop(ScratchRegister);
        masm.rshiftPtr(Imm32(FRAMESIZE_SHIFT), ScratchRegister);
        masm.addPtr(ScratchRegister, BaselineStackReg);
    } else {
        masm.movePtr(BaselineFrameReg, BaselineStackReg);
    }

    masm.loadPtr(Address(StackPointer, offsetof(BaselineStubFrame, savedFrame)),
                 BaselineFrameReg);
    masm.loadPtr(Address(StackPointer, offsetof(BaselineStubFrame, savedStub)),
                 ICStubReg);

    // Load the return address.
    masm.loadPtr(Address(StackPointer, offsetof(BaselineStubFrame, returnAddress)),
                 ICTailCallReg);

    // Discard the frame descriptor.
    masm.loadPtr(Address(StackPointer, offsetof(BaselineStubFrame, descriptor)), ScratchRegister);
    masm.addPtr(Imm32(STUB_FRAME_SIZE), StackPointer);
}

inline void
EmitStowICValues(MacroAssembler& masm, int values)
{
    MOZ_ASSERT(values >= 0 && values <= 2);
    switch(values) {
      case 1:
        // Stow R0
        masm.Push(R0);
        break;
      case 2:
        // Stow R0 and R1
        masm.Push(R0);
        masm.Push(R1);
    }
}

inline void
EmitUnstowICValues(MacroAssembler& masm, int values, bool discard = false)
{
    MOZ_ASSERT(values >= 0 && values <= 2);
    switch(values) {
      case 1:
        // Unstow R0.
        if (discard)
            masm.addPtr(Imm32(sizeof(Value)), BaselineStackReg);
        else
            masm.popValue(R0);
        break;
      case 2:
        // Unstow R0 and R1.
        if (discard) {
            masm.addPtr(Imm32(sizeof(Value) * 2), BaselineStackReg);
        } else {
            masm.popValue(R1);
            masm.popValue(R0);
        }
        break;
    }
    masm.adjustFrame(-values * sizeof(Value));
}

template <typename AddrType>
inline void
EmitPreBarrier(MacroAssembler& masm, const AddrType& addr, MIRType type)
{
    // On MIPS, $ra is clobbered by patchableCallPreBarrier. Save it first.
    masm.push(ra);
    masm.patchableCallPreBarrier(addr, type);
    masm.pop(ra);
}

inline void
EmitStubGuardFailure(MacroAssembler& masm)
{
    // Load next stub into ICStubReg
    masm.loadPtr(Address(ICStubReg, ICStub::offsetOfNext()), ICStubReg);

    // Return address is already loaded, just jump to the next stubcode.
    MOZ_ASSERT(ICTailCallReg == ra);
    masm.jump(Address(ICStubReg, ICStub::offsetOfStubCode()));
}

} // namespace jit
} // namespace js

#endif /* jit_mips_shared_SharedICHelpers_mips_shared_h */
