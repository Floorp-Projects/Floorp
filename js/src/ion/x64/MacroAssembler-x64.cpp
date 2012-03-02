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
 * ***** END LICENSE BLOCK ***** */

#include "MacroAssembler-x64.h"
#include "ion/MoveEmitter.h"
#include "ion/IonFrames.h"

using namespace js;
using namespace js::ion;

void
MacroAssemblerX64::setupABICall(uint32 args)
{
    JS_ASSERT(!inCall_);
    inCall_ = true;

    args_ = args;
    passedIntArgs_ = 0; 
    passedFloatArgs_ = 0;
    stackForCall_ = ShadowStackSpace;
}

void
MacroAssemblerX64::setupAlignedABICall(uint32 args)
{
    setupABICall(args);
    dynamicAlignment_ = false;
}

void
MacroAssemblerX64::setupUnalignedABICall(uint32 args, const Register &scratch)
{
    setupABICall(args);
    dynamicAlignment_ = true;

    movq(rsp, scratch);
    andq(Imm32(~(StackAlignment - 1)), rsp);
    push(scratch);
}

void
MacroAssemblerX64::passABIArg(const MoveOperand &from)
{
    MoveOperand to;
    if (from.isDouble()) {
        FloatRegister dest;
        if (GetFloatArgReg(passedIntArgs_, passedFloatArgs_++, &dest)) {
            to = MoveOperand(dest);
        } else {
            to = MoveOperand(StackPointer, stackForCall_);
            stackForCall_ += sizeof(double);
        }
        enoughMemory_ = moveResolver_.addMove(from, to, Move::DOUBLE);
    } else {
        Register dest;
        if (GetIntArgReg(passedIntArgs_++, passedFloatArgs_, &dest)) {
            to = MoveOperand(dest);
        } else {
            to = MoveOperand(StackPointer, stackForCall_);
            stackForCall_ += sizeof(int64_t);
        }
        enoughMemory_ = moveResolver_.addMove(from, to, Move::GENERAL);
    }
}

void
MacroAssemblerX64::passABIArg(const Register &reg)
{
    passABIArg(MoveOperand(reg));
}

void
MacroAssemblerX64::passABIArg(const FloatRegister &reg)
{
    passABIArg(MoveOperand(reg));
}

void
MacroAssemblerX64::callWithABI(void *fun, Result result)
{
    JS_ASSERT(inCall_);
    JS_ASSERT(args_ == passedIntArgs_ + passedFloatArgs_);

    uint32 stackAdjust;
    if (dynamicAlignment_) {
        stackAdjust = stackForCall_
                    + ComputeByteAlignment(stackForCall_ + STACK_SLOT_SIZE,
                                           StackAlignment);
    } else {
        stackAdjust = stackForCall_
                    + ComputeByteAlignment(stackForCall_ + framePushed_,
                                           StackAlignment);
    }

    reserveStack(stackAdjust);

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

    freeStack(stackAdjust);
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
    passABIArg(rax);
    callWithABI(JS_FUNC_TO_DATA_PTR(void *, ion::HandleException));

    // Load the error value, load the new stack pointer, and return.
    moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
    movq(Operand(rsp, offsetof(ResumeFromException, stackPointer)), rsp);
    ret();
}

