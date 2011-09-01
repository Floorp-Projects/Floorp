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
    masm.mov(Imm32(returnCode), r0);
    masm.startDataTransferM(true, sp);
    masm.transferReg(r4);
    masm.transferReg(r5);
    masm.transferReg(r6);
    masm.transferReg(r7);
    masm.transferReg(r8);
    masm.transferReg(r9);
    masm.transferReg(r10);
    masm.transferReg(r11);
    masm.transferReg(r12);
    masm.transferReg(pc);
    masm.finishDataTransfer();
}

/* This method generates a trampoline on x86 for a c++ function with
 * the following signature:
 *   JSBool blah(void *code, int argc, Value *argv, Value *vp)*
 *                    =r0       =r1          =r2          =r3
 *   ...using standard cdecl calling convention
 *   ...cdecl has no business existing on ARM
 */
IonCode *
IonCompartment::generateEnterJIT(JSContext *cx)
{
    MacroAssembler masm(cx);
    Assembler *aasm = &masm;
    // store all of the callee saved registers.  they may be gone for a while
    masm.startDataTransferM(true, sp, true);
    masm.transferReg(r4);
    masm.transferReg(r5);
    masm.transferReg(r6);
    masm.transferReg(r7);
    masm.transferReg(r8);
    masm.transferReg(r9);
    masm.transferReg(r10);
    masm.transferReg(r11);
    masm.transferReg(r12);
    masm.transferReg(lr);
    masm.finishDataTransfer();
#if 0
    // there are r1 2-word arguments
    // we want 2 word alignment, so this shouldn't matter.
    // After the arguments have been pushed, we want to push an additional 3 words of
    // data, so in all, we want to decrease sp by 4 if it is currently aligned to
    // 8, and not touch it otherwise
    aasm->sub_r(sp, sp, imm8(4));
    aasm->orr_r(sp, sp, imm8(4));
    // subtract off the size of the arguments from the stack pointer, store elsewhere
    aasm->sub_r(r4, sp, lsl(r1, 3));
    // get the final position of the stack pointer into the stack pointer
    aasm->sub_r(sp,r4, imm8(16));
    // get a copy of the number of args to use as a decrement counter, also
    // set the zero condition code
    aasm->mov_r(r5, r1, setCond);


    // loop over arguments, copying them over
    {
        Label header, footer;
        // If there aren't any arguments, don't do anything
        aasm->branch(footer, Zero);
        // get the top of the loop
        masm.bind(&header);
        aasm->sub_r(r5, r5, imm8(1), setCond);
        // we could be more awesome, and unroll this, using a loadm
        // (particularly since the offset is effectively 0)
        // but that seems more error prone, and complex.
        aasm->dataTransferN(true, true, 64, r6, r2, imm8(8), postIndex);
        aasm->dataTransferN(false, true, 64, r6, r4, imm8(8), postIndex);
        aasm->branch(header, NotZero);
        masm.bind(&footer);
    }

// this is all rubbish. and X86
    // We need to ensure that the stack is aligned on a 12-byte boundary, so
    // inside the JIT function the stack is 16-byte aligned. Our stack right
    // now might not be aligned on some platforms (win32, gcc) so we factor
    // this possibility in, and simulate what the new stack address would be.
    //   +argc * 8 for arguments
    //   +4 for pushing alignment
    //   +4 for pushing the callee token
    //   +4 for pushing the return address
    masm.movl(esp, ecx);
    masm.subl(eax, ecx);
    masm.subl(Imm32(12), ecx);
    // could probably be ecx = ecx & 12 -- all the other bits should be 0.

    // ecx = ecx & 15, holds alignment.
    masm.andl(Imm32(15), ecx);
    masm.subl(ecx, esp);

    /***************************************************************
    Loop over argv vector, push arguments onto stack in reverse order
    ***************************************************************/

    // ebx = argv   --argv pointer is in ebp + 16
    masm.movl(Operand(ebp, 16), ebx);

    // eax = argv[8(argc)]  --eax now points one value past the last argument
    masm.addl(ebx, eax);

    // while (eax > ebx)  --while still looping through arguments
    {
        Label header, footer;
        masm.bind(&header);

        masm.cmpl(eax, ebx);
        masm.j(Assembler::BelowOrEqual, &footer);

        // eax -= 8  --move to previous argument
        masm.subl(Imm32(8), eax);

        // Push what eax points to on stack, a Value is 2 words
        masm.push(Operand(eax, 4));
        masm.push(Operand(eax, 0));

        masm.jmp(&header);
        masm.bind(&footer);
    }

    // Push the callee token.
    masm.push(Operand(ebp, 24));

    // Save the stack size so we can remove arguments and alignment after the
    // call.
    masm.movl(Operand(ebp, 12), eax);
    masm.shll(Imm32(3), eax);
    masm.addl(eax, ecx);
    masm.addl(Imm32(4), ecx);
    masm.push(ecx);

    /***************************************************************
        Call passed-in code, get return value and fill in the
        passed in return value pointer
    ***************************************************************/
    // Call code  --code pointer is in ebp + 8
    masm.call(Operand(ebp, 8));

    // Pop arguments off the stack.
    // eax <- 8*argc (size of all arugments we pushed on the stack)
    masm.pop(eax);
    masm.addl(eax, esp);

    // |ebp| could have been clobbered by the inner function. For now, re-grab
    // |vp| directly off the stack:
    //
    //  +32 vp
    //  +28 argv
    //  +24 argc
    //  +20 code
    //  +16 <return>
    //  +12 ebp
    //  +8  ebx
    //  +4  esi
    //  +0  edi
    masm.movl(Operand(esp, 32), eax);
    masm.movl(JSReturnReg_Type, Operand(eax, 4));
    masm.movl(JSReturnReg_Data, Operand(eax, 0));

    /**************************************************************
        Return stack and registers to correct state
    **************************************************************/
    GenerateReturn(masm, JS_TRUE);

    Linker linker(masm);
    return linker.newCode(cx);
#endif
    return NULL;
}

IonCode *
IonCompartment::generateReturnError(JSContext *cx)
{
    MacroAssembler masm(cx);
#if 0

    // Pop arguments off the stack.
    // eax <- 8*argc (size of all arugments we pushed on the stack)
    masm.pop(eax);
    masm.addl(eax, esp);

    GenerateReturn(masm, JS_FALSE);
#endif
    Linker linker(masm);
    return linker.newCode(cx);
}

static void
GenerateBailoutThunk(MacroAssembler &masm, uint32 frameClass)
{
#if 0
    // Push registers such that we can access them from [base + code].
    masm.reserveStack(Registers::Total * sizeof(void *));
    for (uint32 i = 0; i < Registers::Total; i++)
        masm.movl(Register::FromCode(i), Operand(esp, i * sizeof(void *)));

    // Push xmm registers, such that we can access them from [base + code].
    masm.reserveStack(FloatRegisters::Total * sizeof(double));
    for (uint32 i = 0; i < FloatRegisters::Total; i++)
        masm.movsd(FloatRegister::FromCode(i), Operand(esp, i * sizeof(double)));

    // Push the bailout table number.
    masm.push(Imm32(frameClass));

    // Get the stack pointer into a register, pre-alignment.
    masm.movl(esp, eax);

    // Call the bailout function.
    masm.setupUnalignedABICall(1, ecx);
    masm.setABIArg(0, eax);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, Bailout));

    // Common size of a bailout frame.
    uint32 bailoutFrameSize = sizeof(void *) + // frameClass
                              sizeof(double) * FloatRegisters::Total +
                              sizeof(void *) * Registers::Total;
    // Remove the Ion frame.
    if (frameClass == NO_FRAME_SIZE_CLASS_ID) {
        // Stack is:
        //    ... frame ...
        //    snapshotOffset
        //    frameSize
        //    ... bailoutFrame ...
        masm.addl(Imm32(bailoutFrameSize), esp);
        masm.pop(ecx);
        masm.lea(Operand(esp, ecx, TimesOne, sizeof(void *)), esp);
    } else {
        // Stack is:
        //    ... frame ...
        //    bailoutId
        //    ... bailoutFrame ...
        uint32 frameSize = FrameSizeClass::FromClass(frameClass).frameSize();
        masm.addl(Imm32(bailoutFrameSize + sizeof(void *) + frameSize), esp);
    }

    Label exception;

    // Either interpret or handle an exception.
    masm.testl(eax, eax);
    masm.j(Assembler::NonZero, &exception);

    // If we're about to interpret, we've removed the depth of the local ion
    // frame, meaning we're aligned to its (callee, size, return) prefix. Save
    // this prefix in a register.
    masm.movl(esp, eax);

    // Reserve space for Interpret() to store a Value.
    masm.subl(Imm32(sizeof(Value)), esp);
    masm.movl(esp, ecx);

    // Call out to the interpreter.
    masm.setupUnalignedABICall(2, edx);
    masm.setABIArg(0, eax);
    masm.setABIArg(1, ecx);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ThunkToInterpreter));

    // Load the value the interpreter returned.
    masm.movl(Operand(esp, 4), JSReturnReg_Type);
    masm.movl(Operand(esp, 0), JSReturnReg_Data);
    masm.addl(Imm32(8), esp);

    // We're back, aligned to the frame prefix. Check for an exception.
    masm.testl(eax, eax);
    masm.j(Assembler::Zero, &exception);

    // Return to the caller.
    masm.ret();

    masm.bind(&exception);

    // Call into HandleException, passing in the top frame prefix.
    masm.movl(esp, eax);
    masm.setupUnalignedABICall(1, ecx);
    masm.setABIArg(0, eax);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, HandleException));

    // The return value is how much stack to adjust before returning.
    masm.addl(eax, esp);
    masm.ret();
#endif
}

IonCode *
IonCompartment::generateBailoutTable(JSContext *cx, uint32 frameClass)
{
    MacroAssembler masm;

    Label bailout;
    for (size_t i = 0; i < BAILOUT_TABLE_SIZE; i++)
        masm.call(&bailout);
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

