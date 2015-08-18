/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_MacroAssembler_inl_h
#define jit_MacroAssembler_inl_h

#include "jit/MacroAssembler.h"

namespace js {
namespace jit {

//{{{ check_macroassembler_style
// ===============================================================
// Frame manipulation functions.

uint32_t
MacroAssembler::framePushed() const
{
    return framePushed_;
}

void
MacroAssembler::setFramePushed(uint32_t framePushed)
{
    framePushed_ = framePushed;
}

void
MacroAssembler::adjustFrame(int32_t value)
{
    MOZ_ASSERT_IF(value < 0, framePushed_ >= uint32_t(-value));
    setFramePushed(framePushed_ + value);
}

void
MacroAssembler::implicitPop(uint32_t bytes)
{
    MOZ_ASSERT(bytes % sizeof(intptr_t) == 0);
    MOZ_ASSERT(bytes <= INT32_MAX);
    adjustFrame(-int32_t(bytes));
}

// ===============================================================
// Stack manipulation functions.

CodeOffsetLabel
MacroAssembler::PushWithPatch(ImmWord word)
{
    framePushed_ += sizeof(word.value);
    return pushWithPatch(word);
}

CodeOffsetLabel
MacroAssembler::PushWithPatch(ImmPtr imm)
{
    return PushWithPatch(ImmWord(uintptr_t(imm.value)));
}

// ===============================================================
// Simple call functions.

void
MacroAssembler::call(const CallSiteDesc& desc, const Register reg)
{
    call(reg);
    append(desc, currentOffset(), framePushed());
}

void
MacroAssembler::call(const CallSiteDesc& desc, Label* label)
{
    call(label);
    append(desc, currentOffset(), framePushed());
}

// ===============================================================
// ABI function calls.

void
MacroAssembler::passABIArg(Register reg)
{
    passABIArg(MoveOperand(reg), MoveOp::GENERAL);
}

void
MacroAssembler::passABIArg(FloatRegister reg, MoveOp::Type type)
{
    passABIArg(MoveOperand(reg), type);
}

template <typename T> void
MacroAssembler::callWithABI(const T& fun, MoveOp::Type result)
{
    profilerPreCall();
    callWithABINoProfiler(fun, result);
    profilerPostReturn();
}

void
MacroAssembler::appendSignatureType(MoveOp::Type type)
{
#ifdef JS_SIMULATOR
    signature_ <<= ArgType_Shift;
    switch (type) {
      case MoveOp::GENERAL: signature_ |= ArgType_General; break;
      case MoveOp::DOUBLE:  signature_ |= ArgType_Double;  break;
      case MoveOp::FLOAT32: signature_ |= ArgType_Float32; break;
      default: MOZ_CRASH("Invalid argument type");
    }
#endif
}

ABIFunctionType
MacroAssembler::signature() const
{
#ifdef JS_SIMULATOR
#ifdef DEBUG
    switch (signature_) {
      case Args_General0:
      case Args_General1:
      case Args_General2:
      case Args_General3:
      case Args_General4:
      case Args_General5:
      case Args_General6:
      case Args_General7:
      case Args_General8:
      case Args_Double_None:
      case Args_Int_Double:
      case Args_Float32_Float32:
      case Args_Double_Double:
      case Args_Double_Int:
      case Args_Double_DoubleInt:
      case Args_Double_DoubleDouble:
      case Args_Double_IntDouble:
      case Args_Int_IntDouble:
      case Args_Double_DoubleDoubleDouble:
      case Args_Double_DoubleDoubleDoubleDouble:
        break;
      default:
        MOZ_CRASH("Unexpected type");
    }
#endif // DEBUG

    return ABIFunctionType(signature_);
#else
    // No simulator enabled.
    MOZ_CRASH("Only available for making calls within a simulator.");
#endif
}

//}}} check_macroassembler_style
// ===============================================================

void
MacroAssembler::PushStubCode()
{
    exitCodePatch_ = PushWithPatch(ImmWord(-1));
}

void
MacroAssembler::enterExitFrame(const VMFunction* f)
{
    linkExitFrame();
    // Push the ioncode. (Bailout or VM wrapper)
    PushStubCode();
    // Push VMFunction pointer, to mark arguments.
    Push(ImmPtr(f));
}

void
MacroAssembler::enterFakeExitFrame(JitCode* codeVal)
{
    linkExitFrame();
    Push(ImmPtr(codeVal));
    Push(ImmPtr(nullptr));
}

void
MacroAssembler::leaveExitFrame(size_t extraFrame)
{
    freeStack(ExitFooterFrame::Size() + extraFrame);
}

bool
MacroAssembler::hasEnteredExitFrame() const
{
    return exitCodePatch_.offset() != 0;
}

} // namespace jit
} // namespace js

#endif /* jit_MacroAssembler_inl_h */
