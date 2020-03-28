/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "new-regexp/regexp-macro-assembler-arch.h"
#include "new-regexp/regexp-stack.h"

#include "jit/MacroAssembler-inl.h"

namespace v8 {
namespace internal {

using js::jit::AbsoluteAddress;
using js::jit::Address;
using js::jit::AllocatableGeneralRegisterSet;
using js::jit::Assembler;
using js::jit::GeneralRegisterSet;
using js::jit::Imm32;
using js::jit::ImmPtr;
using js::jit::ImmWord;
using js::jit::Register;
using js::jit::StackMacroAssembler;

SMRegExpMacroAssembler::SMRegExpMacroAssembler(JSContext* cx, Isolate* isolate,
                                               StackMacroAssembler& masm,
                                               Zone* zone, Mode mode,
                                               uint32_t num_capture_registers)
    : NativeRegExpMacroAssembler(isolate, zone),
      cx_(cx),
      masm_(masm),
      mode_(mode),
      num_registers_(num_capture_registers),
      num_capture_registers_(num_capture_registers) {
  // Each capture has a start and an end register
  MOZ_ASSERT(num_capture_registers_ % 2 == 0);

  AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());

  temp0_ = regs.takeAny();
  temp1_ = regs.takeAny();
  temp2_ = regs.takeAny();
  input_end_pointer_ = regs.takeAny();
  current_character_ = regs.takeAny();
  current_position_ = regs.takeAny();
  backtrack_stack_pointer_ = regs.takeAny();
  savedRegisters_ = js::jit::SavedNonVolatileRegisters(regs);

  masm_.jump(&entry_label_);  // We'll generate the entry code later
  masm_.bind(&start_label_);  // and continue from here.
}

int SMRegExpMacroAssembler::stack_limit_slack() {
  return RegExpStack::kStackLimitSlack;
}

void SMRegExpMacroAssembler::AdvanceCurrentPosition(int by) {
  if (by != 0) {
    masm_.addPtr(Imm32(by * char_size()), current_position_);
  }
}

void SMRegExpMacroAssembler::Backtrack() {
  // Check for an interrupt. We have to restart from the beginning if we
  // are interrupted, so we only check for urgent interrupts.
  js::jit::Label noInterrupt;
  masm_.branchTest32(
      Assembler::Zero, AbsoluteAddress(cx_->addressOfInterruptBits()),
      Imm32(uint32_t(js::InterruptReason::CallbackUrgent)), &noInterrupt);
  masm_.movePtr(ImmWord(js::RegExpRunStatus_Error), temp0_);
  masm_.jump(&exit_label_);
  masm_.bind(&noInterrupt);

  // Pop code location from backtrack stack and jump to location.
  Pop(temp0_);
  masm_.jump(temp0_);
}

void SMRegExpMacroAssembler::Bind(Label* label) {
  masm_.bind(label->inner());
}

void SMRegExpMacroAssembler::CheckCharacterImpl(Imm32 c, Label* on_cond,
                                                Assembler::Condition cond) {
  masm_.branch32(cond, current_character_, c, LabelOrBacktrack(on_cond));
}

void SMRegExpMacroAssembler::CheckCharacter(uint32_t c, Label* on_equal) {
  CheckCharacterImpl(Imm32(c), on_equal, Assembler::Equal);
}

void SMRegExpMacroAssembler::CheckNotCharacter(uint32_t c,
                                               Label* on_not_equal) {
  CheckCharacterImpl(Imm32(c), on_not_equal, Assembler::NotEqual);
}

void SMRegExpMacroAssembler::CheckCharacterGT(uc16 c, Label* on_greater) {
  CheckCharacterImpl(Imm32(c), on_greater, Assembler::GreaterThan);
}

void SMRegExpMacroAssembler::CheckCharacterLT(uc16 c, Label* on_less) {
  CheckCharacterImpl(Imm32(c), on_less, Assembler::LessThan);
}

// Bitwise-and the current character with mask and then check for a
// match with c.
void SMRegExpMacroAssembler::CheckCharacterAfterAndImpl(uint32_t c,
                                                        uint32_t mask,
                                                        Label* on_cond,
                                                        bool is_not) {
  if (c == 0) {
    Assembler::Condition cond = is_not ? Assembler::NonZero : Assembler::Zero;
    masm_.branchTest32(cond, current_character_, Imm32(mask),
                       LabelOrBacktrack(on_cond));
  } else {
    Assembler::Condition cond = is_not ? Assembler::NotEqual : Assembler::Equal;
    masm_.move32(Imm32(mask), temp0_);
    masm_.and32(current_character_, temp0_);
    masm_.branch32(cond, temp0_, Imm32(c), LabelOrBacktrack(on_cond));
  }
}

void SMRegExpMacroAssembler::CheckCharacterAfterAnd(uint32_t c,
                                                    uint32_t mask,
                                                    Label* on_equal) {
  CheckCharacterAfterAndImpl(c, mask, on_equal, /*is_not =*/false);
}

void SMRegExpMacroAssembler::CheckNotCharacterAfterAnd(uint32_t c,
                                                       uint32_t mask,
                                                       Label* on_not_equal) {
  CheckCharacterAfterAndImpl(c, mask, on_not_equal, /*is_not =*/true);
}


// Subtract minus from the current character, then bitwise-and the
// result with mask, then check for a match with c.
void SMRegExpMacroAssembler::CheckNotCharacterAfterMinusAnd(
    uc16 c, uc16 minus, uc16 mask, Label* on_not_equal) {
  masm_.computeEffectiveAddress(Address(current_character_, -minus), temp0_);
  if (c == 0) {
    masm_.branchTest32(Assembler::NonZero, temp0_, Imm32(mask),
                       LabelOrBacktrack(on_not_equal));
  } else {
    masm_.and32(Imm32(mask), temp0_);
    masm_.branch32(Assembler::NotEqual, temp0_, Imm32(c),
                   LabelOrBacktrack(on_not_equal));
  }
}

// If the current position matches the position stored on top of the backtrack
// stack, pops the backtrack stack and branches to the given label.
void SMRegExpMacroAssembler::CheckGreedyLoop(Label* on_equal) {
  js::jit::Label fallthrough;
  masm_.branchPtr(Assembler::NotEqual, Address(backtrack_stack_pointer_, 0),
                  current_position_, &fallthrough);
  masm_.addPtr(Imm32(sizeof(void*)), backtrack_stack_pointer_);  // Pop.
  JumpOrBacktrack(on_equal);
  masm_.bind(&fallthrough);
}

void SMRegExpMacroAssembler::CheckCharacterInRangeImpl(
    uc16 from, uc16 to, Label* on_cond, Assembler::Condition cond) {
  // x is in [from,to] if unsigned(x - from) <= to - from
  masm_.computeEffectiveAddress(Address(current_character_, -from), temp0_);
  masm_.branch32(cond, temp0_, Imm32(to - from), LabelOrBacktrack(on_cond));
}

void SMRegExpMacroAssembler::CheckCharacterInRange(uc16 from, uc16 to,
                                                   Label* on_in_range) {
  CheckCharacterInRangeImpl(from, to, on_in_range, Assembler::BelowOrEqual);
}

void SMRegExpMacroAssembler::CheckCharacterNotInRange(uc16 from, uc16 to,
                                                      Label* on_not_in_range) {
  CheckCharacterInRangeImpl(from, to, on_not_in_range, Assembler::Above);
}

void SMRegExpMacroAssembler::Fail() {
  masm_.movePtr(ImmWord(js::RegExpRunStatus_Success_NotFound), temp0_);
  masm_.jump(&exit_label_);
}

void SMRegExpMacroAssembler::GoTo(Label* to) {
  masm_.jump(LabelOrBacktrack(to));
}

void SMRegExpMacroAssembler::PopCurrentPosition() { Pop(current_position_); }

void SMRegExpMacroAssembler::PushBacktrack(Label* label) {
  MOZ_ASSERT(!label->is_bound());
  MOZ_ASSERT(!label->patchOffset_.bound());
  label->patchOffset_ = masm_.movWithPatch(ImmPtr(nullptr), temp0_);
  MOZ_ASSERT(label->patchOffset_.bound());

  Push(temp0_);

  CheckBacktrackStackLimit();
}

void SMRegExpMacroAssembler::PushCurrentPosition() { Push(current_position_); }

// Returns true if a regexp match can be restarted (aka the regexp is global).
// The return value is not used anywhere, but we implement it to be safe.
bool SMRegExpMacroAssembler::Succeed() {
  masm_.jump(&success_label_);
  return global();
}

void SMRegExpMacroAssembler::Push(Register source) {
  MOZ_ASSERT(source != backtrack_stack_pointer_);

  masm_.subPtr(Imm32(sizeof(void*)), backtrack_stack_pointer_);
  masm_.storePtr(source, Address(backtrack_stack_pointer_, 0));
}

void SMRegExpMacroAssembler::Pop(Register target) {
  MOZ_ASSERT(target != backtrack_stack_pointer_);

  masm_.loadPtr(Address(backtrack_stack_pointer_, 0), target);
  masm_.addPtr(Imm32(sizeof(void*)), backtrack_stack_pointer_);
}

void SMRegExpMacroAssembler::JumpOrBacktrack(Label* to) {
  if (to) {
    masm_.jump(to->inner());
  } else {
    Backtrack();
  }
}

// Generate a quick inline test for backtrack stack overflow.
// If the test fails, call an OOL handler to try growing the stack.
void SMRegExpMacroAssembler::CheckBacktrackStackLimit() {
  js::jit::Label no_stack_overflow;
  masm_.branchPtr(
      Assembler::BelowOrEqual,
      AbsoluteAddress(isolate()->regexp_stack()->limit_address_address()),
      backtrack_stack_pointer_, &no_stack_overflow);

  masm_.call(&stack_overflow_label_);

  // Exit with an exception if the call failed
  masm_.branchTest32(Assembler::Zero, temp0_, temp0_,
                     &exit_with_exception_label_);

  masm_.bind(&no_stack_overflow);
}

// This is only used by tracing code.
// The return value doesn't matter.
RegExpMacroAssembler::IrregexpImplementation
SMRegExpMacroAssembler::Implementation() {
  return kBytecodeImplementation;
}
}  // namespace internal
}  // namespace v8
