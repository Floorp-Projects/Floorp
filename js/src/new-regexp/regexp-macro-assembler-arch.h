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


 private:
  // Push a register on the backtrack stack.
  void Push(js::jit::Register value);

  // Pop a value from the backtrack stack.
  void Pop(js::jit::Register target);

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
  js::jit::Label success_label_;
  js::jit::Label exit_label_;

  Mode mode_;
  int num_registers_;
  int num_capture_registers_;
  js::jit::LiveGeneralRegisterSet savedRegisters_;
};

}  // namespace internal
}  // namespace v8

#endif  // RegexpMacroAssemblerArch_h
