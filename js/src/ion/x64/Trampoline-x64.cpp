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
    // Restore non-volatile registers.
#if defined(_WIN64)
    masm.pop(rsi);
    masm.pop(rdi);
#endif
    masm.pop(r15);
    masm.pop(r14);
    masm.pop(r13);
    masm.pop(r12);
    masm.pop(rbx);

    // Restore frame pointer and return.
    masm.pop(rbp);
    masm.movl(Imm32(returnCode), rax);
    masm.ret();
}

/* This method generates a trampoline on x64 for a c++ function with
 * the following signature:
 *   JSBool blah(void *code, int argc, Value *argv, Value *vp)
 *   ...using standard x64 fastcall calling convention
 */
IonCode *
IonCompartment::generateEnterJIT(JSContext *cx)
{
    const Register reg_code = ArgReg0;
    const Register reg_argc = ArgReg1;
    const Register reg_argv = ArgReg2;
    const Register reg_vp   = ArgReg3;
#if defined(_WIN64)
    const Operand token = Operand(rbp, 8 + ShadowStackSpace);
#else
    const Register token = ArgReg4;
#endif

    MacroAssembler masm(cx);

    // Save old stack frame pointer, set new stack frame pointer.
    masm.push(rbp);
    masm.mov(rsp, rbp);

    // Save non-volatile registers.
    masm.push(rbx);
    masm.push(r12);
    masm.push(r13);
    masm.push(r14);
    masm.push(r15);
#if defined(_WIN64)
    masm.push(rdi);
    masm.push(rsi);
#endif

    // Save arguments passed in registers needed after function call.
    masm.push(reg_vp);

    // Remember stack depth without padding and arguments.
    masm.mov(rsp, r14);

    // Remember number of bytes occupied by argument vector
    masm.mov(reg_argc, r13);
    masm.shll(Imm32(3), r13);

    // Guarantee 16-byte alignment.
    // We push argc, callee token, frame size, and return address.
    // The latter two are 16 bytes together, so we only consider argc and the
    // token.
    masm.mov(rsp, r12);
    masm.subq(r13, r12);
    masm.subq(Imm32(8), r12);
    masm.andl(Imm32(0xf), r12);
    masm.subq(r12, rsp);

    /***************************************************************
    Loop over argv vector, push arguments onto stack in reverse order
    ***************************************************************/

    // r13 still stores the number of bytes in the argument vector.
    masm.addq(reg_argv, r13); // r13 points above last argument.

    // while r13 > rdx, push arguments.
    {
        Label header, footer;
        masm.bind(&header);

        masm.cmpq(r13, reg_argv);
        masm.j(AssemblerX86Shared::BelowOrEqual, &footer);

        masm.subq(Imm32(8), r13);
        masm.push(Operand(r13, 0));
        masm.jmp(&header);

        masm.bind(&footer);
    }

    // Push the callee token. Remember on win64, there are 32 bytes of shadow
    // stack space.
    masm.push(token);

    /*****************************************************************
    Push the number of bytes we've pushed so far on the stack and call
    *****************************************************************/
    masm.subq(rsp, r14);
    // Safe to not shift sizeDescriptor: no other consumers.
    masm.orl(Imm32(0x1), r14); // Mark EntryFrame.
    masm.push(r14);

    // Call function.
    masm.call(reg_code);

    // Pop arguments and padding from stack.
    masm.pop(r14);              // sizeDescriptor.
    masm.xorl(Imm32(0x1), r14); // Unmark EntryFrame.
    masm.addq(r14, rsp);        // Remove arguments.

    /*****************************************************************
    Place return value where it belongs, pop all saved registers
    *****************************************************************/
    masm.pop(r12); // vp
    masm.movq(JSReturnReg, Operand(r12, 0));

    GenerateReturn(masm, JS_TRUE);

    Linker linker(masm);
    return linker.newCode(cx);
}

IonCode *
IonCompartment::generateReturnError(JSContext *cx)
{
    MacroAssembler masm(cx);

    masm.pop(r14);              // sizeDescriptor.
    masm.xorl(Imm32(0x1), r14); // Unmark EntryFrame.
    masm.addq(r14, rsp);        // Remove arguments.
    masm.pop(r11);              // Discard |vp|: returning from error.

    GenerateReturn(masm, JS_FALSE);
    
    Linker linker(masm);
    return linker.newCode(cx);
}

IonCode *
IonCompartment::generateArgumentsRectifier(JSContext *cx)
{
    MacroAssembler masm(cx);

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

    Linker linker(masm);
    return linker.newCode(cx);
}

static void
GenerateBailoutThunk(MacroAssembler &masm, uint32 frameClass)
{
    // Push registers such that we can access them from [base + code].
    masm.reserveStack(Registers::Total * sizeof(void *));
    for (uint32 i = 0; i < Registers::Total; i++)
        masm.movq(Register::FromCode(i), Operand(rsp, i * sizeof(void *)));

    // Push xmm registers, such that we can access them from [base + code].
    masm.reserveStack(FloatRegisters::Total * sizeof(double));
    for (uint32 i = 0; i < FloatRegisters::Total; i++)
        masm.movsd(FloatRegister::FromCode(i), Operand(rsp, i * sizeof(double)));

    // Get the stack pointer into a register, pre-alignment.
    masm.movq(rsp, r8);

    // Call the bailout function.
    masm.setupUnalignedABICall(1, rax);
    masm.setABIArg(0, r8);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, Bailout));

    // Stack is:
    //     [frame]
    //     snapshotOffset
    //     frameSize
    //     [bailoutFrame]
    uint32 bailoutFrameSize = sizeof(void *) * Registers::Total +
                              sizeof(double) * FloatRegisters::Total;
    masm.addq(Imm32(bailoutFrameSize), rsp);
    masm.pop(rcx);
    masm.lea(Operand(rsp, rcx, TimesOne, sizeof(void *)), rsp);

    Label exception;

    // Either interpret or handle an exception.
    masm.testl(rax, rax);
    masm.j(Assembler::NonZero, &exception);

    // If we're about to interpret, we've removed the depth of the local ion
    // frame, meaning we're aligned to its (callee, size, return) prefix. Save
    // this prefix in a register.
    masm.movq(rsp, rax);

    // Reserve space for Interpret() to store a Value.
    masm.subq(Imm32(sizeof(Value)), rsp);
    masm.movq(rsp, rcx);

    // Call out to the interpreter.
    masm.setupUnalignedABICall(2, rdx);
    masm.setABIArg(0, rax);
    masm.setABIArg(1, rcx);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ThunkToInterpreter));

    // Load the value the interpreter returned.
    masm.movq(Operand(rsp, 0), JSReturnReg);
    masm.addq(Imm32(8), rsp);

    // We're back, aligned to the frame prefix. Check for an exception.
    masm.testl(rax, rax);
    masm.j(Assembler::Zero, &exception);

    // Return to the caller.
    masm.ret();

    masm.bind(&exception);


    // Call into HandleException, passing in the top frame prefix.
    masm.movq(rsp, rax);
    masm.setupUnalignedABICall(1, rcx);
    masm.setABIArg(0, rax);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, HandleException));

    // The return value is how much stack to adjust before returning.
    masm.addq(rax, rsp);
    masm.ret();
}

IonCode *
IonCompartment::generateBailoutTable(JSContext *cx, uint32 frameClass)
{
    JS_NOT_REACHED("x64 does not use bailout tables");
    return NULL;
}

IonCode *
IonCompartment::generateBailoutHandler(JSContext *cx)
{
    MacroAssembler masm;

    GenerateBailoutThunk(masm, NO_FRAME_SIZE_CLASS_ID);

    Linker linker(masm);
    return linker.newCode(cx);
}

