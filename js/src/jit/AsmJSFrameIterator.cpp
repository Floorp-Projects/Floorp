/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AsmJSFrameIterator.h"

#include "jit/AsmJS.h"
#include "jit/AsmJSModule.h"
#include "jit/IonMacroAssembler.h"

using namespace js;
using namespace js::jit;

/*****************************************************************************/
// AsmJSFrameIterator implementation

static void *
ReturnAddressFromFP(uint8_t *fp)
{
    return reinterpret_cast<AsmJSFrame*>(fp)->returnAddress;
}

AsmJSFrameIterator::AsmJSFrameIterator(const AsmJSActivation &activation)
  : module_(&activation.module()),
    fp_(activation.fp())
{
    if (!fp_)
        return;
    settle();
}

void
AsmJSFrameIterator::operator++()
{
    JS_ASSERT(!done());
    fp_ += callsite_->stackDepth();
    settle();
}

void
AsmJSFrameIterator::settle()
{
    void *returnAddress = ReturnAddressFromFP(fp_);

    const AsmJSModule::CodeRange *codeRange = module_->lookupCodeRange(returnAddress);
    JS_ASSERT(codeRange);
    codeRange_ = codeRange;

    switch (codeRange->kind()) {
      case AsmJSModule::CodeRange::Entry:
        fp_ = nullptr;
        JS_ASSERT(done());
        return;
      case AsmJSModule::CodeRange::Function:
        callsite_ = module_->lookupCallSite(returnAddress);
        JS_ASSERT(callsite_);
        break;
    }
}

JSAtom *
AsmJSFrameIterator::functionDisplayAtom() const
{
    JS_ASSERT(!done());
    return reinterpret_cast<const AsmJSModule::CodeRange*>(codeRange_)->functionName(*module_);
}

unsigned
AsmJSFrameIterator::computeLine(uint32_t *column) const
{
    JS_ASSERT(!done());
    if (column)
        *column = callsite_->column();
    return callsite_->line();
}

/*****************************************************************************/
// Prologue/epilogue code generation

static void
PushRetAddr(MacroAssembler &masm)
{
#if defined(JS_CODEGEN_ARM)
    masm.push(lr);
#elif defined(JS_CODEGEN_MIPS)
    masm.push(ra);
#else
    // The x86/x64 call instruction pushes the return address.
#endif
}

void
js::GenerateAsmJSFunctionPrologue(MacroAssembler &masm, unsigned framePushed,
                                  Label *maybeOverflowThunk, Label *overflowExit)
{
    // When not in profiling mode, the only way to observe fp (i.e.,
    // AsmJSActivation::fp) is to call out to C++ so, as an optimization, we
    // don't update fp. Technically, the interrupt exit can observe fp at an
    // arbitrary pc, but we don't care about providing an accurate stack in this
    // case. We still need to reserve space for the saved frame pointer, though,
    // to maintain the AsmJSFrame layout.
    PushRetAddr(masm);
    masm.subPtr(Imm32(framePushed + AsmJSFrameBytesAfterReturnAddress), StackPointer);
    masm.setFramePushed(framePushed);

    // Overflow checks are omitted by CodeGenerator in some cases (leaf
    // functions with small framePushed). Perform overflow-checking after
    // pushing framePushed to catch cases with really large frames.
    if (maybeOverflowThunk) {
        // If framePushed is zero, we don't need a thunk.
        Label *target = framePushed ? maybeOverflowThunk : overflowExit;
        masm.branchPtr(Assembler::AboveOrEqual,
                       AsmJSAbsoluteAddress(AsmJSImm_StackLimit),
                       StackPointer,
                       target);
    }
}

void
js::GenerateAsmJSFunctionEpilogue(MacroAssembler &masm, unsigned framePushed,
                                  Label *maybeOverflowThunk, Label *overflowExit)
{
    // Inverse of GenerateAsmJSFunctionPrologue:
    JS_ASSERT(masm.framePushed() == framePushed);
    masm.addPtr(Imm32(framePushed + AsmJSFrameBytesAfterReturnAddress), StackPointer);
    masm.ret();
    masm.setFramePushed(0);

    if (maybeOverflowThunk && maybeOverflowThunk->used()) {
        // The general throw stub assumes that only sizeof(AsmJSFrame) bytes
        // have been pushed. The overflow check occurs after incrementing by
        // framePushed, so pop that before jumping to the overflow exit.
        masm.bind(maybeOverflowThunk);
        masm.addPtr(Imm32(framePushed), StackPointer);
        masm.jump(overflowExit);
    }
}

void
js::GenerateAsmJSStackOverflowExit(MacroAssembler &masm, Label *overflowExit, Label *throwLabel)
{
    masm.bind(overflowExit);

    // If we reach here via the non-profiling prologue, AsmJSActivation::fp has
    // not been updated. To enable stack unwinding from C++, store to it now. If
    // we reached here via the profiling prologue, we'll just store the same
    // value again. Do not update AsmJSFrame::callerFP as it is not necessary in
    // the non-profiling case (there is no return path from this point) and, in
    // the profiling case, it is already correct.
    Register activation = ABIArgGenerator::NonArgReturnVolatileReg0;
    masm.loadAsmJSActivation(activation);
    masm.storePtr(StackPointer, Address(activation, AsmJSActivation::offsetOfFP()));

    // Prepare the stack for calling C++.
    if (unsigned stackDec = StackDecrementForCall(sizeof(AsmJSFrame), ShadowStackSpace))
        masm.subPtr(Imm32(stackDec), StackPointer);

    // No need to restore the stack; the throw stub pops everything.
    masm.assertStackAlignment();
    masm.call(AsmJSImmPtr(AsmJSImm_ReportOverRecursed));
    masm.jump(throwLabel);
}

void
js::GenerateAsmJSEntryPrologue(MacroAssembler &masm)
{
    // Stack-unwinding stops at the entry prologue, so there is no need to
    // update AsmJSActivation::fp. Furthermore, on ARM/MIPS, GlobalReg is not
    // yet initialized, so we can't even if we wanted to.
    PushRetAddr(masm);
    masm.subPtr(Imm32(AsmJSFrameBytesAfterReturnAddress), StackPointer);
    masm.setFramePushed(0);
}

void
js::GenerateAsmJSEntryEpilogue(MacroAssembler &masm)
{
    // Inverse of GenerateAsmJSEntryPrologue:
    JS_ASSERT(masm.framePushed() == 0);
    masm.addPtr(Imm32(AsmJSFrameBytesAfterReturnAddress), StackPointer);
    masm.ret();
    masm.setFramePushed(0);
}

void
js::GenerateAsmJSFFIExitPrologue(MacroAssembler &masm, unsigned framePushed)
{
    // Stack-unwinding from C++ starts unwinding depends on AsmJSActivation::fp.
    PushRetAddr(masm);

    Register activation = ABIArgGenerator::NonArgReturnVolatileReg0;
    masm.loadAsmJSActivation(activation);
    Address fp(activation, AsmJSActivation::offsetOfFP());
    masm.push(fp);
    masm.storePtr(StackPointer, fp);

    if (framePushed)
        masm.subPtr(Imm32(framePushed), StackPointer);

    masm.setFramePushed(framePushed);
}

void
js::GenerateAsmJSFFIExitEpilogue(MacroAssembler &masm, unsigned framePushed)
{
    // Inverse of GenerateAsmJSFFIExitPrologue:
    JS_ASSERT(masm.framePushed() == framePushed);

    if (framePushed)
        masm.addPtr(Imm32(framePushed), StackPointer);

    Register activation = ABIArgGenerator::NonArgReturnVolatileReg0;
    masm.loadAsmJSActivation(activation);
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    masm.pop(Operand(activation, AsmJSActivation::offsetOfFP()));
#else
    Register fp = ABIArgGenerator::NonArgReturnVolatileReg1;
    masm.pop(fp);
    masm.storePtr(fp, Address(activation, AsmJSActivation::offsetOfFP()));
#endif

    masm.ret();
    masm.setFramePushed(0);
}

