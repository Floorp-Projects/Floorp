/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x64/MacroAssembler-x64.h"

#include "jit/Bailouts.h"
#include "jit/BaselineFrame.h"
#include "jit/IonFrames.h"
#include "jit/JitCompartment.h"
#include "jit/MoveEmitter.h"

using namespace js;
using namespace js::jit;

void
MacroAssemblerX64::loadConstantDouble(double d, const FloatRegister &dest)
{
    if (maybeInlineDouble(d, dest))
        return;

    if (!doubleMap_.initialized()) {
        enoughMemory_ &= doubleMap_.init();
        if (!enoughMemory_)
            return;
    }
    size_t doubleIndex;
    if (DoubleMap::AddPtr p = doubleMap_.lookupForAdd(d)) {
        doubleIndex = p->value();
    } else {
        doubleIndex = doubles_.length();
        enoughMemory_ &= doubles_.append(Double(d));
        enoughMemory_ &= doubleMap_.add(p, d, doubleIndex);
        if (!enoughMemory_)
            return;
    }
    Double &dbl = doubles_[doubleIndex];
    JS_ASSERT(!dbl.uses.bound());

    // The constants will be stored in a pool appended to the text (see
    // finish()), so they will always be a fixed distance from the
    // instructions which reference them. This allows the instructions to use
    // PC-relative addressing. Use "jump" label support code, because we need
    // the same PC-relative address patching that jumps use.
    JmpSrc j = masm.movsd_ripr(dest.code());
    JmpSrc prev = JmpSrc(dbl.uses.use(j.offset()));
    masm.setNextJump(j, prev);
}

void
MacroAssemblerX64::loadConstantFloat32(float f, const FloatRegister &dest)
{
    if (maybeInlineFloat(f, dest))
        return;

    if (!floatMap_.initialized()) {
        enoughMemory_ &= floatMap_.init();
        if (!enoughMemory_)
            return;
    }
    size_t floatIndex;
    if (FloatMap::AddPtr p = floatMap_.lookupForAdd(f)) {
        floatIndex = p->value();
    } else {
        floatIndex = floats_.length();
        enoughMemory_ &= floats_.append(Float(f));
        enoughMemory_ &= floatMap_.add(p, f, floatIndex);
        if (!enoughMemory_)
            return;
    }
    Float &flt = floats_[floatIndex];
    JS_ASSERT(!flt.uses.bound());

    // See comment in loadConstantDouble
    JmpSrc j = masm.movss_ripr(dest.code());
    JmpSrc prev = JmpSrc(flt.uses.use(j.offset()));
    masm.setNextJump(j, prev);
}

void
MacroAssemblerX64::finish()
{
    JS_STATIC_ASSERT(CodeAlignment >= sizeof(double));

    if (!doubles_.empty() || !floats_.empty())
        masm.align(sizeof(double));

    for (size_t i = 0; i < doubles_.length(); i++) {
        Double &dbl = doubles_[i];
        bind(&dbl.uses);
        masm.doubleConstant(dbl.value);
    }

    // No need to align on sizeof(float) as we are aligned on sizeof(double);
    for (size_t i = 0; i < floats_.length(); i++) {
        Float &flt = floats_[i];
        bind(&flt.uses);
        masm.floatConstant(flt.value);
    }

    MacroAssemblerX86Shared::finish();
}

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
MacroAssemblerX64::passABIArg(const MoveOperand &from, MoveOp::Kind kind)
{
    MoveOperand to;
    switch (kind) {
      case MoveOp::FLOAT32:
      case MoveOp::DOUBLE: {
        FloatRegister dest;
        if (GetFloatArgReg(passedIntArgs_, passedFloatArgs_++, &dest)) {
            if (from.isFloatReg() && from.floatReg() == dest) {
                // Nothing to do; the value is in the right register already
                return;
            }
            to = MoveOperand(dest);
        } else {
            to = MoveOperand(StackPointer, stackForCall_);
            switch (kind) {
              case MoveOp::FLOAT32: stackForCall_ += sizeof(float);  break;
              case MoveOp::DOUBLE:  stackForCall_ += sizeof(double); break;
              default: MOZ_ASSUME_UNREACHABLE("Unexpected float register class argument kind");
            }
        }
        break;
      }
      case MoveOp::GENERAL: {
        Register dest;
        if (GetIntArgReg(passedIntArgs_++, passedFloatArgs_, &dest)) {
            if (from.isGeneralReg() && from.reg() == dest) {
                // Nothing to do; the value is in the right register already
                return;
            }
            to = MoveOperand(dest);
        } else {
            to = MoveOperand(StackPointer, stackForCall_);
            stackForCall_ += sizeof(int64_t);
        }
        break;
      }
      default:
        MOZ_ASSUME_UNREACHABLE("Unexpected argument kind");
    }

    enoughMemory_ = moveResolver_.addMove(from, to, kind);
}

void
MacroAssemblerX64::passABIArg(const Register &reg)
{
    passABIArg(MoveOperand(reg), MoveOp::GENERAL);
}

void
MacroAssemblerX64::passABIArg(const FloatRegister &reg, MoveOp::Kind kind)
{
    passABIArg(MoveOperand(reg), kind);
}

void
MacroAssemblerX64::callWithABIPre(uint32_t *stackAdjust)
{
    JS_ASSERT(inCall_);
    JS_ASSERT(args_ == passedIntArgs_ + passedFloatArgs_);

    if (dynamicAlignment_) {
        *stackAdjust = stackForCall_
                     + ComputeByteAlignment(stackForCall_ + STACK_SLOT_SIZE,
                                            StackAlignment);
    } else {
        *stackAdjust = stackForCall_
                     + ComputeByteAlignment(stackForCall_ + framePushed_,
                                            StackAlignment);
    }

    reserveStack(*stackAdjust);

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
        testq(rsp, Imm32(StackAlignment - 1));
        j(Equal, &good);
        breakpoint();
        bind(&good);
    }
#endif
}

void
MacroAssemblerX64::callWithABIPost(uint32_t stackAdjust, MoveOp::Kind result)
{
    freeStack(stackAdjust);
    if (dynamicAlignment_)
        pop(rsp);

    JS_ASSERT(inCall_);
    inCall_ = false;
}

void
MacroAssemblerX64::callWithABI(void *fun, MoveOp::Kind result)
{
    uint32_t stackAdjust;
    callWithABIPre(&stackAdjust);
    call(ImmPtr(fun));
    callWithABIPost(stackAdjust, result);
}

void
MacroAssemblerX64::callWithABI(AsmJSImmPtr imm, MoveOp::Kind result)
{
    uint32_t stackAdjust;
    callWithABIPre(&stackAdjust);
    call(imm);
    callWithABIPost(stackAdjust, result);
}

static bool
IsIntArgReg(Register reg)
{
    for (uint32_t i = 0; i < NumIntArgRegs; i++) {
        if (IntArgRegs[i] == reg)
            return true;
    }

    return false;
}

void
MacroAssemblerX64::callWithABI(Address fun, MoveOp::Kind result)
{
    if (IsIntArgReg(fun.base)) {
        // Callee register may be clobbered for an argument. Move the callee to
        // r10, a volatile, non-argument register.
        moveResolver_.addMove(MoveOperand(fun.base), MoveOperand(r10), MoveOp::GENERAL);
        fun.base = r10;
    }

    JS_ASSERT(!IsIntArgReg(fun.base));

    uint32_t stackAdjust;
    callWithABIPre(&stackAdjust);
    call(Operand(fun));
    callWithABIPost(stackAdjust, result);
}

void
MacroAssemblerX64::handleFailureWithHandler(void *handler)
{
    // Reserve space for exception information.
    subq(Imm32(sizeof(ResumeFromException)), rsp);
    movq(rsp, rax);

    // Ask for an exception handler.
    setupUnalignedABICall(1, rcx);
    passABIArg(rax);
    callWithABI(handler);

    IonCode *excTail = GetIonContext()->runtime->jitRuntime()->getExceptionTail();
    jmp(excTail);
}

void
MacroAssemblerX64::handleFailureWithHandlerTail()
{
    Label entryFrame;
    Label catch_;
    Label finally;
    Label return_;
    Label bailout;

    loadPtr(Address(rsp, offsetof(ResumeFromException, kind)), rax);
    branch32(Assembler::Equal, rax, Imm32(ResumeFromException::RESUME_ENTRY_FRAME), &entryFrame);
    branch32(Assembler::Equal, rax, Imm32(ResumeFromException::RESUME_CATCH), &catch_);
    branch32(Assembler::Equal, rax, Imm32(ResumeFromException::RESUME_FINALLY), &finally);
    branch32(Assembler::Equal, rax, Imm32(ResumeFromException::RESUME_FORCED_RETURN), &return_);
    branch32(Assembler::Equal, rax, Imm32(ResumeFromException::RESUME_BAILOUT), &bailout);

    breakpoint(); // Invalid kind.

    // No exception handler. Load the error value, load the new stack pointer
    // and return from the entry frame.
    bind(&entryFrame);
    moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
    loadPtr(Address(rsp, offsetof(ResumeFromException, stackPointer)), rsp);
    ret();

    // If we found a catch handler, this must be a baseline frame. Restore state
    // and jump to the catch block.
    bind(&catch_);
    loadPtr(Address(rsp, offsetof(ResumeFromException, target)), rax);
    loadPtr(Address(rsp, offsetof(ResumeFromException, framePointer)), rbp);
    loadPtr(Address(rsp, offsetof(ResumeFromException, stackPointer)), rsp);
    jmp(Operand(rax));

    // If we found a finally block, this must be a baseline frame. Push
    // two values expected by JSOP_RETSUB: BooleanValue(true) and the
    // exception.
    bind(&finally);
    ValueOperand exception = ValueOperand(rcx);
    loadValue(Address(esp, offsetof(ResumeFromException, exception)), exception);

    loadPtr(Address(rsp, offsetof(ResumeFromException, target)), rax);
    loadPtr(Address(rsp, offsetof(ResumeFromException, framePointer)), rbp);
    loadPtr(Address(rsp, offsetof(ResumeFromException, stackPointer)), rsp);

    pushValue(BooleanValue(true));
    pushValue(exception);
    jmp(Operand(rax));

    // Only used in debug mode. Return BaselineFrame->returnValue() to the caller.
    bind(&return_);
    loadPtr(Address(rsp, offsetof(ResumeFromException, framePointer)), rbp);
    loadPtr(Address(rsp, offsetof(ResumeFromException, stackPointer)), rsp);
    loadValue(Address(rbp, BaselineFrame::reverseOffsetOfReturnValue()), JSReturnOperand);
    movq(rbp, rsp);
    pop(rbp);
    ret();

    // If we are bailing out to baseline to handle an exception, jump to
    // the bailout tail stub.
    bind(&bailout);
    loadPtr(Address(esp, offsetof(ResumeFromException, bailoutInfo)), r9);
    mov(ImmWord(BAILOUT_RETURN_OK), rax);
    jmp(Operand(rsp, offsetof(ResumeFromException, target)));
}

Assembler::Condition
MacroAssemblerX64::testNegativeZero(const FloatRegister &reg, const Register &scratch)
{
    movq(reg, scratch);
    cmpq(scratch, Imm32(1));
    return Overflow;
}

Assembler::Condition
MacroAssemblerX64::testNegativeZeroFloat32(const FloatRegister &reg, const Register &scratch)
{
    movd(reg, scratch);
    cmpl(scratch, Imm32(1));
    return Overflow;
}
