/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacroAssembler-x64.h"
#include "ion/MoveEmitter.h"
#include "ion/IonFrames.h"

#include "jsscriptinlines.h"

using namespace js;
using namespace js::ion;

void
MacroAssemblerX64::setupABICall(uint32_t args)
{
    JS_ASSERT(!inCall_);
    inCall_ = true;

    args_ = args;
    passedIntArgs_ = 0; 
    passedFloatArgs_ = 0;
    stackForCall_ = ShadowStackSpace;
}

void
MacroAssemblerX64::setupAlignedABICall(uint32_t args)
{
    setupABICall(args);
    dynamicAlignment_ = false;
}

void
MacroAssemblerX64::setupUnalignedABICall(uint32_t args, const Register &scratch)
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

    uint32_t stackAdjust;
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

Assembler::Condition
MacroAssemblerX64::testNegativeZero(const FloatRegister &reg, const Register &scratch)
{
    movqsd(reg, scratch);
    subq(Imm32(1), scratch);
    return Overflow;
}
