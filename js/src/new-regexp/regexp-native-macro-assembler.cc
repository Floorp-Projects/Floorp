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

using js::jit::Address;
using js::jit::AllocatableGeneralRegisterSet;
using js::jit::GeneralRegisterSet;
using js::jit::Imm32;
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

void SMRegExpMacroAssembler::Fail() {
  masm_.movePtr(ImmWord(js::RegExpRunStatus_Success_NotFound), temp0_);
  masm_.jump(&exit_label_);
}

void SMRegExpMacroAssembler::PopCurrentPosition() { Pop(current_position_); }

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

// This is only used by tracing code.
// The return value doesn't matter.
RegExpMacroAssembler::IrregexpImplementation
SMRegExpMacroAssembler::Implementation() {
  return kBytecodeImplementation;
}
}  // namespace internal
}  // namespace v8
