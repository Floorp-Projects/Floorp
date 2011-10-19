/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Scheff <ascheff@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jscompartment.h"
#include "assembler/assembler/MacroAssembler.h"
#include "ion/IonCompartment.h"
#include "ion/IonLinker.h"
#include "ion/IonFrames.h"
#include "ion/Bailouts.h"

using namespace js;
using namespace js::ion;

static void
GenerateReturn(MacroAssembler &masm, int returnCode)
{
    // Restore non-volatile registers
    masm.ma_mov(Imm32(returnCode), r0);
    masm.startDataTransferM(IsLoad, sp, IA, WriteBack);
    masm.transferReg(r4);
    masm.transferReg(r5);
    masm.transferReg(r6);
    masm.transferReg(r7);
    masm.transferReg(r8);
    masm.transferReg(r9);
    masm.transferReg(r10);
    masm.transferReg(r11);
    // r12 isn't saved, so it shouldn't be restored.
    masm.transferReg(pc);
    masm.finishDataTransfer();
}

/* This method generates a trampoline on x86 for a c++ function with
 * the following signature:
 *   JSBool blah(void *code, int argc, Value *argv, Value *vp, CalleeToken calleeToken)*
 *                    =r0       =r1          =r2          =r3
 *   ...using standard EABI calling convention

 */
IonCode *
IonCompartment::generateEnterJIT(JSContext *cx)
{
    MacroAssembler masm(cx);
    Assembler *aasm = &masm;
    // store all of the callee saved registers.  they may be gone for a while
    masm.startDataTransferM(IsStore, sp, DB, WriteBack);
    masm.transferReg(r3); // [sp]  save the pointer we'll write our return value into
    masm.transferReg(r4); // [sp,4]
    masm.transferReg(r5); // [sp,8]
    masm.transferReg(r6); // [sp,12]
    masm.transferReg(r7); // [sp,16]
    masm.transferReg(r8); // [sp,20]
    masm.transferReg(r9); // [sp,24]
    masm.transferReg(r10); // [sp,28]
    masm.transferReg(r11); // [sp,32]
    // The abi does not expect r12 (ip) to be preserved
    masm.transferReg(lr);  // [sp,36]
    // OAur 5th argument is located at [sp, 40]
    masm.finishDataTransfer();
    // Load said argument int r10
    aasm->as_dtr(IsLoad, 32, Offset, r10, DTRAddr(sp, DtrOffImm(40)));
    aasm->as_mov(r9, lsl(r1, 3)); // r9 = 8*argc
    // The size of the IonFrame is actually 16, and we pushed r3 when we aren't
    // going to pop it, BUT, we pop the return value, rather than just branching
    // to it, AND we need to access the value pushed by r3, which we need to
    // not be at a negative offsett, so after removing the ionframe and the
    // ionfunction's arguments from the stack, we want the sp to be pointing at
    // the location that r3 was stored in, then we'll pop that value, use it
    // and pop r4-r11, pc to return.
    aasm->as_add(r9, r9, Imm8(16-4));

#if 0
    // This is in case we want to go back to using frames that
    // aren't 8 byte alinged
    // there are r1 2-word arguments to the js code
    // we want 2 word alignment, so this shouldn't matter.
    // After the arguments have been pushed, we want to push an additional 3 words of
    // data, so in all, we want to decrease sp by 4 if it is currently aligned to
    // 8, and not touch it otherwise
    aasm->as_sub(sp, sp, Imm8(4));
    aasm->as_orr(sp, sp, Imm8(4));
#endif
    // Subtract off the size of the arguments from the stack pointer, store elsewhere
    aasm->as_sub(r4, sp, O2RegImmShift(r1, LSL, 3)); //r4 = sp - argc*8
    // Get the final position of the stack pointer into the stack pointer
    aasm->as_sub(sp, r4, Imm8(16)); // sp' = sp - argc*8 - 16
    // Get a copy of the number of args to use as a decrement counter, also
    // Set the zero condition code
    aasm->as_mov(r5, O2Reg(r1), SetCond);

    // Loop over arguments, copying them from an unknown buffer onto the Ion
    // stack so they can be accessed from JIT'ed code.
    {
        Label header, footer;
        // If there aren't any arguments, don't do anything
        aasm->as_b(&footer, Assembler::Zero);
        // Get the top of the loop
        masm.bind(&header);
        aasm->as_sub(r5, r5, Imm8(1), SetCond);
        // We could be more awesome, and unroll this, using a loadm
        // (particularly since the offset is effectively 0)
        // but that seems more error prone, and complex.
        aasm->as_extdtr(IsLoad,  64, true, PostIndex, r6, EDtrAddr(r2, EDtrOffImm(8)));
        aasm->as_extdtr(IsStore, 64, true, PostIndex, r6, EDtrAddr(r4, EDtrOffImm(8)));
        aasm->as_b(&header, Assembler::NonZero);
        masm.bind(&footer);
    }
    masm.startDataTransferM(IsStore, sp, IB, NoWriteBack);
    masm.transferReg(r9);  // [sp',4]  = argc*8+20
    masm.transferReg(r10); // [sp',8]  = callee token
    masm.transferReg(r11); // [sp',12] = fill in the buffer value with junk.
    masm.finishDataTransfer();
    // Throw our return address onto the stack.  this setup seems less-than-ideal
    aasm->as_dtr(IsStore, 32, Offset, pc, DTRAddr(sp, DtrOffImm(0)));
    // Call the function.  using lr as the link register would be *so* nice
    aasm->as_bx(r0);
    // The top of the stack now points to *ABOVE* our return address... great
    // Load off of the stack the size of our local stack
    aasm->as_dtr(IsLoad, 32, Offset, r5, DTRAddr(sp, DtrOffImm(0)));
    // TODO: these can be fused into one!
    aasm->as_add(sp, sp, O2Reg(r5));
    // Reach into our saved arguments, and find the pointer to where we want
    // to write our return value.
    aasm->as_dtr(IsLoad, 32, PostIndex, r5, DTRAddr(sp, DtrOffImm(4)));
    // We're using a load-double here.  In order for that to work,
    // the data needs to be stored in two consecutive registers,
    // make sure this is the case
    ASSERT(JSReturnReg_Type.code() == JSReturnReg_Data.code()+1);
    // The lower reg also needs to be an even regster.
    ASSERT((JSReturnReg_Data.code() & 1) == 0);
    aasm->as_extdtr(IsStore, 64, true, Offset,
                    JSReturnReg_Data, EDtrAddr(r5, EDtrOffImm(0)));
    GenerateReturn(masm, JS_TRUE);
    Linker linker(masm);
    return linker.newCode(cx);
}

IonCode *
IonCompartment::generateReturnError(JSContext *cx)
{
    MacroAssembler masm(cx);
    // This is where the stack size is stored on x86. where is it stored here?
    masm.ma_pop(r0);
    masm.ma_add(r0, sp, sp);

    GenerateReturn(masm, JS_FALSE);
    Linker linker(masm);
    return linker.newCode(cx);
}

IonCode *
IonCompartment::generateArgumentsRectifier(JSContext *cx)
{
    MacroAssembler masm(cx);
#if 0
    // ArgumentsRectifierReg contains the |nargs| pushed onto the current frame.
    // Including |this|, there are (|nargs| + 1) arguments to copy.
    JS_ASSERT(ArgumentsRectifierReg == r8);

    // Load the number of |undefined|s to push into %rcx.
    masm.movq(Operand(rsp, offsetof(IonFrameData, calleeToken_)), rax);
    masm.load16(Operand(rax, offsetof(JSFunction, nargs)), rcx);
    masm.subq(r8, rcx);

    masm.moveValue(UndefinedValue(), r10);

    masm.movq(rsp, rbp); // Save %rsp.

    // Push undefined.
    {
        Label undefLoopTop;
        masm.bind(&undefLoopTop);

        masm.push(r10);
        masm.subl(Imm32(1), rcx);

        masm.testl(rcx, rcx);
        masm.j(Assembler::NonZero, &undefLoopTop);
    }

    // Get the topmost argument.
    masm.movq(r8, r9);
    masm.shlq(Imm32(3), r9); // r9 <- (nargs) * sizeof(Value)

    masm.movq(rbp, rcx);
    masm.addq(Imm32(sizeof(IonFrameData)), rcx);
    masm.addq(r9, rcx);

    // Push arguments, |nargs| + 1 times (to include |this|).
    {
        Label copyLoopTop, initialSkip;

        masm.jump(&initialSkip);

        masm.bind(&copyLoopTop);
        masm.subq(Imm32(sizeof(Value)), rcx);
        masm.subl(Imm32(1), r8);
        masm.bind(&initialSkip);

        masm.mov(Operand(rcx, 0x0), rdx);
        masm.push(rdx);

        masm.testl(r8, r8);
        masm.j(Assembler::NonZero, &copyLoopTop);
    }

    // Construct sizeDescriptor.
    masm.subq(rsp, rbp);
    masm.shll(Imm32(IonFramePrefix::FrameTypeBits), rbp);
    masm.orl(Imm32(IonFramePrefix::RectifierFrame), rbp);

    // Construct IonFrameData.
    masm.push(rax); // calleeToken.
    masm.push(rbp); // sizeDescriptor.

    // Call the target function.
    // Note that this code assumes the function is JITted.
    masm.movq(Operand(rax, offsetof(JSFunction, u.i.script)), rax);
    masm.movq(Operand(rax, offsetof(JSScript, ion)), rax);
    masm.movq(Operand(rax, offsetof(IonScript, method_)), rax);
    masm.movq(Operand(rax, IonCode::OffsetOfCode()), rax);
    masm.call(rax);

    // Remove the rectifier frame.
    masm.pop(rbp);            // rbp <- sizeDescriptor with FrameType.
    masm.shrl(Imm32(IonFramePrefix::FrameTypeBits), rbp); // rbp <- size of pushed arguments.
    masm.pop(r11);            // Discard calleeToken.
    masm.addq(rbp, rsp);      // Discard pushed arguments.

    masm.ret();
#endif
    Linker linker(masm);
    return linker.newCode(cx);
}

static void
GenerateBailoutThunk(MacroAssembler &masm, uint32 frameClass)
{
    // STEP 1a: save our register sets to the stack so Bailout() can
    // read everything.
    // sp % 8 == 0
    masm.startDataTransferM(IsStore, sp, DB, WriteBack);
    // We don't have to push everything, but this is likely easier.
    // setting regs_
    for (uint32 i = 0; i < Registers::Total; i++)
        masm.transferReg(Register::FromCode(i));
    masm.finishDataTransfer();
    // Float transfer hasn't been implemented yet.  this is a NOP.
    // setting fpregs_
    masm.startFloatTransferM(IsStore, sp, DB, WriteBack);
    for (uint32 i = 0; i < FloatRegisters::Total; i++)
        masm.transferFloatReg(FloatRegister::FromCode(i));
    masm.finishFloatTransfer();

    // STEP 1b: Push both the "return address" of the function call (the
    //          address of the instruction after the call that we used to get
    //          here) as well as the callee token onto the stack.  The return
    //          address is currently in r14.  We will proceed by loading the
    //          callee token into a sacrificial register <= r14, then pushing
    //          both onto the stack

    // now place the frameClass onto the stack, via a register
    masm.ma_mov(Imm32(frameClass), r4);
    // And onto the stack.  Since the stack is full, we need to put this
    // one past the end of the current stack. Sadly, the ABI says that we need
    // to always point to the lowest place that has been written.  the OS is
    // free to do whatever it wants below sp.
    masm.startDataTransferM(IsStore, sp, DB, WriteBack);
    // set frameClassId_
    masm.transferReg(r4);
    // Set tableOffset_; higher registers are stored at higher locations on
    // the stack.
    masm.transferReg(lr);
    masm.finishDataTransfer();

    // SP % 8 == 4
    // STEP 1c: Call the bailout function, giving a pointer to the
    //          structure we just blitted onto the stack
    masm.setupAlignedABICall(1);

    // Copy the present stack pointer into a temp register (it happens to be the
    // argument register)
    //masm.as_mov(r0, O2Reg(sp));

    // Decrement sp by another 4, so we keep alignment
    // Not Anymore!  pushing both the snapshotoffset as well as the
    // masm.as_sub(sp, sp, Imm8(4));

    // Set the old (4-byte aligned) value of the sp as the first argument
    masm.setABIArg(0, sp);

    // Sp % 8 == 0
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, Bailout));
    // Common size of a bailout frame.
    uint32 bailoutFrameSize = sizeof(void *) + // frameClass
                              sizeof(double) * FloatRegisters::Total +
                              sizeof(void *) * Registers::Total;

    if (frameClass == NO_FRAME_SIZE_CLASS_ID) {
        // Make sure the bailout frame size fits into the offset for a load
        masm.as_dtr(IsLoad, 32, Offset,
                    r4, DTRAddr(sp, DtrOffImm(offsetof(BailoutStack, frameSize_))));
        // We add 12 to the bailoutFrameSize because:
        // sizeof(uint32) for the tableOffset that was pushed onto the stack
        // sizeof(uintptr_t) for the snapshotOffset;
        // alignment to round the uintptr_t up to a multiple of 8 bytes.
        masm.ma_add(sp, Imm32(bailoutFrameSize+12), sp);
        masm.as_add(sp, sp, O2Reg(r4));
    } else {
        uint32 frameSize = FrameSizeClass::FromClass(frameClass).frameSize();
        masm.ma_add(Imm32(frameSize // the frame that was added when we entered the most recent function
                          + sizeof(void*) // the size of the "return address" that was dumped on the stack
                          + bailoutFrameSize) // everything else that was pushed on the stack
                    , sp);
    }

    Label exception;

    // See if the bailout code says it is an exception
    masm.as_cmp(r0, Imm8(0));
    masm.as_b(&exception, Assembler::NonZero);
    // The actual thunk gets two arguments: the previous frame's
    // stack (sp), and the place where it is going to return
    // its value (namely, right below the *present* sp;
    masm.as_mov(r0, O2Reg(sp));
    // Make room on the stack
    masm.as_sub(sp, sp, Imm8(sizeof(Value)));
    // And make that the second argument
    masm.as_mov(r1, O2Reg(sp));
    // Do the call
    masm.setupAlignedABICall(2);
    // These should be NOPs
    masm.setABIArg(0, r0);
    masm.setABIArg(1, r1);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ThunkToInterpreter));

    // Load the value that the interpreter returned *and* return the stack
    // to its previous location
    masm.as_extdtr(IsLoad, 64, true, PostIndex,
                   JSReturnReg_Data, EDtrAddr(sp, EDtrOffImm(8)));

    // Test for an exception
    masm.as_cmp(r0, Imm8(0));
    masm.as_b(&exception, Assembler::Zero);
    masm.as_dtr(IsLoad, 32, PostIndex, pc, DTRAddr(sp, DtrOffImm(4)));
    masm.bind(&exception);

    // Call into HandleException, passing in the top frame prefix
    masm.as_mov(r0, O2Reg(sp));
    masm.setupAlignedABICall(1);
    masm.setABIArg(0,r0);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, HandleException));
    // The return value is how much stack to adjust before returning.
    masm.as_add(sp, sp, O2Reg(r0));
    // We're going to be returning by the ion calling convention, which returns
    // by ??? (for now, I think ldr pc, [sp]!)
    masm.as_dtr(IsLoad, 32, PostIndex, pc, DTRAddr(sp, DtrOffImm(4)));
}

IonCode *
IonCompartment::generateBailoutTable(JSContext *cx, uint32 frameClass)
{
    MacroAssembler masm;

    Label bailout;
    for (size_t i = 0; i < BAILOUT_TABLE_SIZE; i++)
        masm.ma_bl(&bailout);
    masm.bind(&bailout);

    GenerateBailoutThunk(masm, frameClass);

    Linker linker(masm);
    return linker.newCode(cx);
}

IonCode *
IonCompartment::generateBailoutHandler(JSContext *cx)
{
    MacroAssembler masm;

    GenerateBailoutThunk(masm, NO_FRAME_SIZE_CLASS_ID);

    Linker linker(masm);
    return linker.newCode(cx);
}

