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
using js::jit::BaseIndex;
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

void SMRegExpMacroAssembler::AdvanceRegister(int reg, int by) {
  MOZ_ASSERT(reg >= 0 && reg < num_registers_);
  if (by != 0) {
    masm_.addPtr(Imm32(by), register_location(reg));
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

// Check if current_position + cp_offset is the input start
void SMRegExpMacroAssembler::CheckAtStartImpl(int cp_offset, Label* on_cond,
                                              Assembler::Condition cond) {
  Address addr(current_position_, cp_offset * char_size());
  masm_.computeEffectiveAddress(addr, temp0_);

  masm_.branchPtr(cond, inputStart(), temp0_,
                  LabelOrBacktrack(on_cond));
}

void SMRegExpMacroAssembler::CheckAtStart(int cp_offset, Label* on_at_start) {
  CheckAtStartImpl(cp_offset, on_at_start, Assembler::Equal);
}

void SMRegExpMacroAssembler::CheckNotAtStart(int cp_offset,
                                             Label* on_not_at_start) {
  CheckAtStartImpl(cp_offset, on_not_at_start, Assembler::NotEqual);
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

void SMRegExpMacroAssembler::CheckBitInTable(Handle<ByteArray> table,
                                             Label* on_bit_set) {
  // Claim ownership of the ByteArray from the current HandleScope.
  // ByteArrays are allocated on the C++ heap and are (eventually)
  // owned by the RegExpShared.
  PseudoHandle<ByteArrayData> rawTable = table->takeOwnership(isolate());

  masm_.movePtr(ImmPtr(rawTable->data()), temp0_);

  masm_.move32(Imm32(kTableMask), temp1_);
  masm_.and32(current_character_, temp1_);

  masm_.load8ZeroExtend(BaseIndex(temp0_, temp1_, js::jit::TimesOne), temp0_);
  masm_.branchTest32(Assembler::NonZero, temp0_, temp0_,
                     LabelOrBacktrack(on_bit_set));

  // Transfer ownership of |rawTable| to the |tables_| vector.
  AddTable(std::move(rawTable));
}

// Checks whether the given offset from the current position is
// inside the input string.
void SMRegExpMacroAssembler::CheckPosition(int cp_offset,
                                           Label* on_outside_input) {
  // Note: current_position_ is a (negative) byte offset relative to
  // the end of the input string.
  if (cp_offset >= 0) {
    //      end + current + offset >= end
    // <=>        current + offset >= 0
    // <=>        current          >= -offset
    masm_.branchPtr(Assembler::GreaterThanOrEqual, current_position_,
                    ImmWord(-cp_offset * char_size()),
                    LabelOrBacktrack(on_outside_input));
  } else {
    // Compute offset position
    masm_.computeEffectiveAddress(
        Address(current_position_, cp_offset * char_size()), temp0_);

    // Compare to start of input.
    masm_.branchPtr(Assembler::GreaterThanOrEqual, inputStart(), temp0_,
                    LabelOrBacktrack(on_outside_input));
  }
}

// This function attempts to generate special case code for character classes.
// Returns true if a special case is generated.
// Otherwise returns false and generates no code.
bool SMRegExpMacroAssembler::CheckSpecialCharacterClass(uc16 type,
                                                        Label* on_no_match) {
  js::jit::Label* no_match = LabelOrBacktrack(on_no_match);

  // Note: throughout this function, range checks (c in [min, max])
  // are implemented by an unsigned (c - min) <= (max - min) check.
  switch (type) {
    case 's': {
      // Match space-characters
      if (mode_ != LATIN1) {
        return false;
      }
      js::jit::Label success;
      // One byte space characters are ' ', '\t'..'\r', and '\u00a0' (NBSP).

      // Check ' '
      masm_.branch32(Assembler::Equal, current_character_, Imm32(' '),
                     &success);

      // Check '\t'..'\r'
      masm_.computeEffectiveAddress(Address(current_character_, -'\t'),
                                    temp0_);
      masm_.branch32(Assembler::BelowOrEqual, temp0_, Imm32('\r' - '\t'),
                     &success);

      // Check \u00a0.
      masm_.branch32(Assembler::NotEqual, temp0_, Imm32(0x00a0 - '\t'),
                     no_match);

      masm_.bind(&success);
      return true;
    }
    case 'S':
      // The emitted code for generic character classes is good enough.
      return false;
    case 'd':
      // Match latin1 digits ('0'-'9')
      masm_.computeEffectiveAddress(Address(current_character_, -'0'), temp0_);
      masm_.branch32(Assembler::Above, temp0_, Imm32('9' - '0'), no_match);
      return true;
    case 'D':
      // Match anything except latin1 digits ('0'-'9')
      masm_.computeEffectiveAddress(Address(current_character_, -'0'), temp0_);
      masm_.branch32(Assembler::BelowOrEqual, temp0_, Imm32('9' - '0'),
                     no_match);
      return true;
    case '.':
      // Match non-newlines. This excludes '\n' (0x0a), '\r' (0x0d),
      // U+2028 LINE SEPARATOR, and U+2029 PARAGRAPH SEPARATOR.
      // See https://tc39.es/ecma262/#prod-LineTerminator

      // To test for 0x0a and 0x0d efficiently, we XOR the input with 1.
      // This converts 0x0a to 0x0b, and 0x0d to 0x0c, allowing us to
      // test for the contiguous range 0x0b..0x0c.
      masm_.move32(current_character_, temp0_);
      masm_.xor32(Imm32(0x01), temp0_);
      masm_.sub32(Imm32(0x0b), temp0_);
      masm_.branch32(Assembler::BelowOrEqual, temp0_, Imm32(0x0c - 0x0b),
                     no_match);

      if (mode_ == UC16) {
        // Compare original value to 0x2028 and 0x2029, using the already
        // computed (current_char ^ 0x01 - 0x0b). I.e., check for
        // 0x201d (0x2028 - 0x0b) or 0x201e.
        masm_.sub32(Imm32(0x2028 - 0x0b), temp0_);
        masm_.branch32(Assembler::BelowOrEqual, temp0_, Imm32(0x2029 - 0x2028),
                       no_match);
      }
      return true;
    case 'w':
      // \w matches the set of 63 characters defined in Runtime Semantics:
      // WordCharacters. We use a static lookup table, which is defined in
      // regexp-macro-assembler.cc.
      // Note: if both Unicode and IgnoreCase are true, \w matches a
      // larger set of characters. That case is handled elsewhere.
      if (mode_ != LATIN1) {
        masm_.branch32(Assembler::Above, current_character_, Imm32('z'),
                       no_match);
      }
      static_assert(arraysize(word_character_map) > unibrow::Latin1::kMaxChar);
      masm_.movePtr(ImmPtr(word_character_map), temp0_);
      masm_.load8ZeroExtend(
          BaseIndex(temp0_, current_character_, js::jit::TimesOne), temp0_);
      masm_.branchTest32(Assembler::Zero, temp0_, temp0_, no_match);
      return true;
    case 'W': {
      // See 'w' above.
      js::jit::Label done;
      if (mode_ != LATIN1) {
        masm_.branch32(Assembler::Above, current_character_, Imm32('z'), &done);
      }
      static_assert(arraysize(word_character_map) > unibrow::Latin1::kMaxChar);
      masm_.movePtr(ImmPtr(word_character_map), temp0_);
      masm_.load8ZeroExtend(
          BaseIndex(temp0_, current_character_, js::jit::TimesOne), temp0_);
      masm_.branchTest32(Assembler::NonZero, temp0_, temp0_, no_match);
      if (mode_ != LATIN1) {
        masm_.bind(&done);
      }
      return true;
    }
      ////////////////////////////////////////////////////////////////////////
      // Non-standard classes (with no syntactic shorthand) used internally //
      ////////////////////////////////////////////////////////////////////////
    case '*':
      // Match any character
      return true;
    case 'n':
      // Match newlines. The opposite of '.'. See '.' above.
      masm_.move32(current_character_, temp0_);
      masm_.xor32(Imm32(0x01), temp0_);
      masm_.sub32(Imm32(0x0b), temp0_);
      if (mode_ == LATIN1) {
        masm_.branch32(Assembler::Above, temp0_, Imm32(0x0c - 0x0b), no_match);
      } else {
        MOZ_ASSERT(mode_ == UC16);
        js::jit::Label done;
        masm_.branch32(Assembler::BelowOrEqual, temp0_, Imm32(0x0c - 0x0b),
                       &done);

        // Compare original value to 0x2028 and 0x2029, using the already
        // computed (current_char ^ 0x01 - 0x0b). I.e., check for
        // 0x201d (0x2028 - 0x0b) or 0x201e.
        masm_.sub32(Imm32(0x2028 - 0x0b), temp0_);
        masm_.branch32(Assembler::Above, temp0_, Imm32(0x2029 - 0x2028),
                       no_match);
        masm_.bind(&done);
      }
      return true;

      // No custom implementation
    default:
      return false;
  }
}

void SMRegExpMacroAssembler::Fail() {
  masm_.movePtr(ImmWord(js::RegExpRunStatus_Success_NotFound), temp0_);
  masm_.jump(&exit_label_);
}

void SMRegExpMacroAssembler::GoTo(Label* to) {
  masm_.jump(LabelOrBacktrack(to));
}

void SMRegExpMacroAssembler::IfRegisterGE(int reg, int comparand,
                                          Label* if_ge) {
  masm_.branchPtr(Assembler::GreaterThanOrEqual, register_location(reg),
                  ImmWord(comparand), LabelOrBacktrack(if_ge));
}

void SMRegExpMacroAssembler::IfRegisterLT(int reg, int comparand,
                                          Label* if_lt) {
  masm_.branchPtr(Assembler::LessThan, register_location(reg),
                  ImmWord(comparand), LabelOrBacktrack(if_lt));
}

void SMRegExpMacroAssembler::IfRegisterEqPos(int reg, Label* if_eq) {
  masm_.branchPtr(Assembler::Equal, register_location(reg), current_position_,
                  LabelOrBacktrack(if_eq));
}

// This is a word-for-word identical copy of the V8 code, which is
// duplicated in at least nine different places in V8 (one per
// supported architecture) with no differences outside of comments and
// formatting. It should be hoisted into the superclass. Once that is
// done upstream, this version can be deleted.
void SMRegExpMacroAssembler::LoadCurrentCharacterImpl(int cp_offset,
                                                      Label* on_end_of_input,
                                                      bool check_bounds,
                                                      int characters,
                                                      int eats_at_least) {
  // It's possible to preload a small number of characters when each success
  // path requires a large number of characters, but not the reverse.
  MOZ_ASSERT(eats_at_least >= characters);
  MOZ_ASSERT(cp_offset < (1 << 30));  // Be sane! (And ensure negation works)

  if (check_bounds) {
    if (cp_offset >= 0) {
      CheckPosition(cp_offset + eats_at_least - 1, on_end_of_input);
    } else {
      CheckPosition(cp_offset, on_end_of_input);
    }
  }
  LoadCurrentCharacterUnchecked(cp_offset, characters);
}

// Load the character (or characters) at the specified offset from the
// current position. Zero-extend to 32 bits.
void SMRegExpMacroAssembler::LoadCurrentCharacterUnchecked(int cp_offset,
                                                           int characters) {
  BaseIndex address(input_end_pointer_, current_position_, js::jit::TimesOne,
                    cp_offset * char_size());
  if (mode_ == LATIN1) {
    if (characters == 4) {
      masm_.load32(address, current_character_);
    } else if (characters == 2) {
      masm_.load16ZeroExtend(address, current_character_);
    } else {
      MOZ_ASSERT(characters == 1);
      masm_.load8ZeroExtend(address, current_character_);
    }
  } else {
    MOZ_ASSERT(mode_ == UC16);
    if (characters == 2) {
      masm_.load32(address, current_character_);
    } else {
      MOZ_ASSERT(characters == 1);
      masm_.load16ZeroExtend(address, current_character_);
    }
  }
}

void SMRegExpMacroAssembler::PopCurrentPosition() { Pop(current_position_); }

void SMRegExpMacroAssembler::PopRegister(int register_index) {
  Pop(temp0_);
  masm_.storePtr(temp0_, register_location(register_index));
}

void SMRegExpMacroAssembler::PushBacktrack(Label* label) {
  MOZ_ASSERT(!label->is_bound());
  MOZ_ASSERT(!label->patchOffset_.bound());
  label->patchOffset_ = masm_.movWithPatch(ImmPtr(nullptr), temp0_);
  MOZ_ASSERT(label->patchOffset_.bound());

  Push(temp0_);

  CheckBacktrackStackLimit();
}

void SMRegExpMacroAssembler::PushCurrentPosition() { Push(current_position_); }

void SMRegExpMacroAssembler::PushRegister(int register_index,
                                          StackCheckFlag check_stack_limit) {
  masm_.loadPtr(register_location(register_index), temp0_);
  Push(temp0_);
  if (check_stack_limit) {
    CheckBacktrackStackLimit();
  }
}

void SMRegExpMacroAssembler::ReadCurrentPositionFromRegister(int reg) {
  masm_.loadPtr(register_location(reg), current_position_);
}

void SMRegExpMacroAssembler::WriteCurrentPositionToRegister(int reg,
                                                            int cp_offset) {
  if (cp_offset == 0) {
    masm_.storePtr(current_position_, register_location(reg));
  } else {
    Address addr(current_position_, cp_offset * char_size());
    masm_.computeEffectiveAddress(addr, temp0_);
    masm_.storePtr(temp0_, register_location(reg));
  }
}

// Note: The backtrack stack pointer is stored in a register as an
// offset from the stack top, not as a bare pointer, so that it is not
// corrupted if the backtrack stack grows (and therefore moves).
void SMRegExpMacroAssembler::ReadStackPointerFromRegister(int reg) {
  masm_.loadPtr(register_location(reg), backtrack_stack_pointer_);
  masm_.addPtr(backtrackStackBase(), backtrack_stack_pointer_);
}
void SMRegExpMacroAssembler::WriteStackPointerToRegister(int reg) {
  masm_.movePtr(backtrack_stack_pointer_, temp0_);
  masm_.subPtr(backtrackStackBase(), temp0_);
  masm_.storePtr(temp0_, register_location(reg));
}

// When matching a regexp that is anchored at the end, this operation
// is used to try skipping the beginning of long strings. If the
// maximum length of a match is less than the length of the string, we
// can skip the initial len - max_len bytes.
void SMRegExpMacroAssembler::SetCurrentPositionFromEnd(int by) {
  js::jit::Label after_position;
  masm_.branchPtr(Assembler::GreaterThanOrEqual, current_position_,
                  ImmWord(-by * char_size()), &after_position);
  masm_.movePtr(ImmWord(-by * char_size()), current_position_);

  // On RegExp code entry (where this operation is used), the character before
  // the current position is expected to be already loaded.
  // We have advanced the position, so it's safe to read backwards.
  LoadCurrentCharacterUnchecked(-1, 1);
  masm_.bind(&after_position);
}

void SMRegExpMacroAssembler::SetRegister(int register_index, int to) {
  MOZ_ASSERT(register_index >= num_capture_registers_);
  masm_.storePtr(ImmWord(to), register_location(register_index));
}

// Returns true if a regexp match can be restarted (aka the regexp is global).
// The return value is not used anywhere, but we implement it to be safe.
bool SMRegExpMacroAssembler::Succeed() {
  masm_.jump(&success_label_);
  return global();
}

// Capture registers are initialized to input[-1]
void SMRegExpMacroAssembler::ClearRegisters(int reg_from, int reg_to) {
  MOZ_ASSERT(reg_from <= reg_to);
  masm_.loadPtr(inputStart(), temp0_);
  masm_.subPtr(Imm32(char_size()), temp0_);
  for (int reg = reg_from; reg <= reg_to; reg++) {
    masm_.storePtr(temp0_, register_location(reg));
  }
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
