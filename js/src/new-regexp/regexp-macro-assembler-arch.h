/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the NativeRegExpMacroAssembler interface for
// SpiderMonkey. It provides the same interface as each of V8's
// architecture-specific implementations.

#ifndef RegexpMacroAssemblerArch_h
#define RegexpMacroAssemblerArch_h

#include "jit/MacroAssembler.h"
#include "new-regexp/regexp-macro-assembler.h"

namespace v8 {
namespace internal {

class SMRegExpMacroAssembler final : public NativeRegExpMacroAssembler {
 public:
  SMRegExpMacroAssembler(JSContext* cx, Isolate* isolate,
                         js::jit::StackMacroAssembler& masm, Zone* zone,
                         Mode mode, uint32_t num_capture_registers);
  virtual ~SMRegExpMacroAssembler() {} // Nothing to do here

  virtual int stack_limit_slack();
  virtual IrregexpImplementation Implementation();

  virtual bool Succeed();
  virtual void Fail();

  virtual void AdvanceCurrentPosition(int by);
  virtual void PopCurrentPosition();
  virtual void PushCurrentPosition();

  virtual void Backtrack();
  virtual void Bind(Label* label);
  virtual void GoTo(Label* label);
  virtual void PushBacktrack(Label* label);

  virtual void CheckCharacter(uint32_t c, Label* on_equal);
  virtual void CheckNotCharacter(uint32_t c, Label* on_not_equal);
  virtual void CheckCharacterGT(uc16 limit, Label* on_greater);
  virtual void CheckCharacterLT(uc16 limit, Label* on_less);
  virtual void CheckCharacterAfterAnd(uint32_t c, uint32_t mask,
                                      Label* on_equal);
  virtual void CheckNotCharacterAfterAnd(uint32_t c, uint32_t mask,
                                         Label* on_not_equal);
  virtual void CheckNotCharacterAfterMinusAnd(uc16 c, uc16 minus, uc16 mask,
                                              Label* on_not_equal);
  virtual void CheckGreedyLoop(Label* on_tos_equals_current_position);
  virtual void CheckCharacterInRange(uc16 from, uc16 to, Label* on_in_range);
  virtual void CheckCharacterNotInRange(uc16 from, uc16 to,
                                        Label* on_not_in_range);


 private:
  // Push a register on the backtrack stack.
  void Push(js::jit::Register value);

  // Pop a value from the backtrack stack.
  void Pop(js::jit::Register target);

  void CheckAtStartImpl(int cp_offset, Label* on_cond,
                        js::jit::Assembler::Condition cond);
  void CheckCharacterImpl(js::jit::Imm32 c, Label* on_cond,
                          js::jit::Assembler::Condition cond);
  void CheckCharacterAfterAndImpl(uint32_t c, uint32_t and_with, Label* on_cond,
                                  bool negate);
  void CheckCharacterInRangeImpl(uc16 from, uc16 to, Label* on_cond,
                                 js::jit::Assembler::Condition cond);

  void JumpOrBacktrack(Label* to);

  // MacroAssembler methods that take a Label can be called with a
  // null label, which means that we should backtrack if we would jump
  // to that label. This is a helper to avoid writing out the same
  // logic a dozen times.
  inline js::jit::Label* LabelOrBacktrack(Label* to) {
    return to ? to->inner() : &backtrack_label_;
  }

  void CheckBacktrackStackLimit();

  inline int char_size() { return static_cast<int>(mode_); }
  inline js::jit::Scale factor() {
    return mode_ == UC16 ? js::jit::TimesTwo : js::jit::TimesOne;
  }

  JSContext* cx_;
  js::jit::StackMacroAssembler& masm_;

  /*
   * This assembler uses the following registers:
   *
   * - current_character_:
   *     Contains the character (or characters) currently being examined.
   *     Must be loaded using LoadCurrentCharacter before using any of the
   *     dispatch methods. After a matching pass for a global regexp,
   *     temporarily stores the index of capture start.
   * - current_position_:
   *     Current position in input *as negative byte offset from end of string*.
   * - input_end_pointer_:
   *     Points to byte after last character in the input. current_position_ is
   *     relative to this.
   * - backtrack_stack_pointer_:
   *     Points to tip of the (heap-allocated) backtrack stack. The stack grows
   *     downward (like the native stack).
   * - temp0_, temp1_, temp2_:
   *     Scratch registers.
   *
   * The native stack pointer is used to access arguments (InputOutputData),
   * local variables (FrameData), and irregexp's internal virtual registers
   * (see register_location).
   */

  js::jit::Register current_character_;
  js::jit::Register current_position_;
  js::jit::Register input_end_pointer_;
  js::jit::Register backtrack_stack_pointer_;
  js::jit::Register temp0_, temp1_, temp2_;

  js::jit::Label entry_label_;
  js::jit::Label start_label_;
  js::jit::Label backtrack_label_;
  js::jit::Label success_label_;
  js::jit::Label exit_label_;
  js::jit::Label stack_overflow_label_;
  js::jit::Label exit_with_exception_label_;

  Mode mode_;
  int num_registers_;
  int num_capture_registers_;
  js::jit::LiveGeneralRegisterSet savedRegisters_;
};

}  // namespace internal
}  // namespace v8

#endif  // RegexpMacroAssemblerArch_h
