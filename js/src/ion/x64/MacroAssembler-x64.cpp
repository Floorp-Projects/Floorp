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
 *   David Anderson <dvander@alliedmods.net>
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

#include "MacroAssembler-x64.h"
#include "ion/MoveEmitter.h"
#include "ion/IonFrames.h"

using namespace js;
using namespace js::ion;

uint32
MacroAssemblerX64::setupABICall(uint32 args)
{
    JS_ASSERT(!inCall_);
    inCall_ = true;

    uint32 stackForArgs = args > NumArgRegs
                          ? (args - NumArgRegs) * sizeof(void *)
                          : 0;
    return stackForArgs;
}

void
MacroAssemblerX64::setupAlignedABICall(uint32 args)
{
    // Find the total number of bytes the stack will have been adjusted by,
    // in order to compute alignment. Include a stack slot for saving the stack
    // pointer, which will be dynamically aligned.
    uint32 stackForCall = setupABICall(args);
    uint32 total = stackForCall + ShadowStackSpace;
    uint32 displacement = total + framePushed_;

    stackAdjust_ = total + ComputeByteAlignment(displacement, StackAlignment);
    dynamicAlignment_ = false;
    reserveStack(stackAdjust_);
}

void
MacroAssemblerX64::setupUnalignedABICall(uint32 args, const Register &scratch)
{
    uint32 stackForCall = setupABICall(args);

    // Find the total number of bytes the stack will have been adjusted by,
    // in order to compute alignment. framePushed_ is bogus or we don't know
    // it for sure, so instead, save the original value of esp and then chop
    // off its low bits. Then, we push the original value of esp.
    movq(rsp, scratch);
    andq(Imm32(~(StackAlignment - 1)), rsp);
    push(scratch);

    uint32 total = stackForCall + ShadowStackSpace;
    uint32 displacement = total + STACK_SLOT_SIZE;

    stackAdjust_ = total + ComputeByteAlignment(displacement, StackAlignment);
    dynamicAlignment_ = true;
    reserveStack(stackAdjust_);
}

void
MacroAssemblerX64::setABIArg(uint32 arg, const MoveOperand &from)
{
    MoveOperand to;
    Register dest;
    if (GetArgReg(arg, &dest)) {
        to = MoveOperand(dest);
    } else {
        // There is no register for this argument, so just move it to its
        // stack slot immediately.
        uint32 disp = GetArgStackDisp(arg);
        to = MoveOperand(StackPointer, disp);
    }
    enoughMemory_ &= moveResolver_.addMove(from, to, Move::GENERAL);
}

void
MacroAssemblerX64::setABIArg(uint32 arg, const Register &reg)
{
    setABIArg(arg, MoveOperand(reg));
}

void
MacroAssemblerX64::callWithABI(void *fun)
{
    JS_ASSERT(inCall_);

    // Position all arguments.
    {
        enoughMemory_ &= moveResolver_.resolve();
        if (!enoughMemory_)
            return;

        MoveEmitter emitter(*this);
        emitter.emit(moveResolver_);
        emitter.finish();
    }

#ifdef DEBUG
    {
        Label good;
        movl(rsp, rax);
        testq(rax, Imm32(StackAlignment - 1));
        j(Equal, &good);
        breakpoint();
        bind(&good);
    }
#endif

    call(ImmWord(fun));

    freeStack(stackAdjust_);
    if (dynamicAlignment_)
        pop(rsp);

    JS_ASSERT(inCall_);
    inCall_ = false;
}

void
MacroAssemblerX64::handleException()
{
    // Reserve space for exception information.
    subq(Imm32(sizeof(ResumeFromException)), rsp);
    movq(rsp, rax);

    // Ask for an exception handler.
    setupUnalignedABICall(1, rcx);
    setABIArg(0, rax);
    callWithABI(JS_FUNC_TO_DATA_PTR(void *, ion::HandleException));

    // Load the error value, load the new stack pointer, and return.
    moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
    movq(Operand(rsp, offsetof(ResumeFromException, stackPointer)), rsp);
    ret();
}

