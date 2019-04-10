/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/SwitchEmitter.h"

#include "mozilla/Span.h"

#include "jsutil.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/SharedContext.h"
#include "frontend/SourceNotes.h"
#include "vm/BytecodeUtil.h"
#include "vm/Opcodes.h"
#include "vm/Runtime.h"

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;

bool SwitchEmitter::TableGenerator::addNumber(int32_t caseValue) {
  if (isInvalid()) {
    return true;
  }

  if (unsigned(caseValue + int(JS_BIT(15))) >= unsigned(JS_BIT(16))) {
    setInvalid();
    return true;
  }

  if (intmap_.isNothing()) {
    intmap_.emplace();
  }

  low_ = std::min(low_, caseValue);
  high_ = std::max(high_, caseValue);

  // Check for duplicates, which require a JSOP_CONDSWITCH.
  // We bias caseValue by 65536 if it's negative, and hope that's a rare case
  // (because it requires a malloc'd bitmap).
  if (caseValue < 0) {
    caseValue += JS_BIT(16);
  }
  if (caseValue >= intmapBitLength_) {
    size_t newLength = NumWordsForBitArrayOfLength(caseValue + 1);
    if (!intmap_->resize(newLength)) {
      ReportOutOfMemory(bce_->cx);
      return false;
    }
    intmapBitLength_ = newLength * BitArrayElementBits;
  }
  if (IsBitArrayElementSet(intmap_->begin(), intmap_->length(), caseValue)) {
    // Duplicate entry is not supported in table switch.
    setInvalid();
    return true;
  }
  SetBitArrayElement(intmap_->begin(), intmap_->length(), caseValue);
  return true;
}

void SwitchEmitter::TableGenerator::finish(uint32_t caseCount) {
  intmap_.reset();

#ifdef DEBUG
  finished_ = true;
#endif

  if (isInvalid()) {
    return;
  }

  if (caseCount == 0) {
    low_ = 0;
    high_ = -1;
    return;
  }

  // Compute table length and select condswitch instead if overlarge
  // or more than half-sparse.
  tableLength_ = uint32_t(high_ - low_ + 1);
  if (tableLength_ >= JS_BIT(16) || tableLength_ > 2 * caseCount) {
    setInvalid();
  }
}

uint32_t SwitchEmitter::TableGenerator::toCaseIndex(int32_t caseValue) const {
  MOZ_ASSERT(finished_);
  MOZ_ASSERT(isValid());
  uint32_t caseIndex = uint32_t(caseValue - low_);
  MOZ_ASSERT(caseIndex < tableLength_);
  return caseIndex;
}

uint32_t SwitchEmitter::TableGenerator::tableLength() const {
  MOZ_ASSERT(finished_);
  MOZ_ASSERT(isValid());
  return tableLength_;
}

SwitchEmitter::SwitchEmitter(BytecodeEmitter* bce) : bce_(bce) {}

bool SwitchEmitter::emitDiscriminant(const Maybe<uint32_t>& switchPos) {
  MOZ_ASSERT(state_ == State::Start);
  switchPos_ = switchPos;

  if (switchPos_) {
    // Ensure that the column of the switch statement is set properly.
    if (!bce_->updateSourceCoordNotes(*switchPos_)) {
      return false;
    }
  }

  state_ = State::Discriminant;
  return true;
}

bool SwitchEmitter::emitLexical(Handle<LexicalScope::Data*> bindings) {
  MOZ_ASSERT(state_ == State::Discriminant);
  MOZ_ASSERT(bindings);

  tdzCacheLexical_.emplace(bce_);
  emitterScope_.emplace(bce_);
  if (!emitterScope_->enterLexical(bce_, ScopeKind::Lexical, bindings)) {
    return false;
  }

  state_ = State::Lexical;
  return true;
}

bool SwitchEmitter::validateCaseCount(uint32_t caseCount) {
  MOZ_ASSERT(state_ == State::Discriminant || state_ == State::Lexical);
  if (caseCount > JS_BIT(16)) {
    bce_->reportError(switchPos_, JSMSG_TOO_MANY_CASES);
    return false;
  }
  caseCount_ = caseCount;

  state_ = State::CaseCount;
  return true;
}

bool SwitchEmitter::emitCond() {
  MOZ_ASSERT(state_ == State::CaseCount);

  kind_ = Kind::Cond;

  // After entering the scope if necessary, push the switch control.
  controlInfo_.emplace(bce_, StatementKind::Switch);
  top_ = bce_->bytecodeSection().offset();

  if (!caseOffsets_.resize(caseCount_)) {
    ReportOutOfMemory(bce_->cx);
    return false;
  }

  // The note has two offsets: first tells total switch code length;
  // second tells offset to first JSOP_CASE.
  if (!bce_->newSrcNote3(SRC_CONDSWITCH, 0, 0, &noteIndex_)) {
    return false;
  }

  MOZ_ASSERT(top_ == bce_->bytecodeSection().offset());
  if (!bce_->emitN(JSOP_CONDSWITCH, 0)) {
    return false;
  }

  tdzCacheCaseAndBody_.emplace(bce_);

  state_ = State::Cond;
  return true;
}

bool SwitchEmitter::emitTable(const TableGenerator& tableGen) {
  MOZ_ASSERT(state_ == State::CaseCount);
  kind_ = Kind::Table;

  // After entering the scope if necessary, push the switch control.
  controlInfo_.emplace(bce_, StatementKind::Switch);
  top_ = bce_->bytecodeSection().offset();

  // The note has one offset that tells total switch code length.
  if (!bce_->newSrcNote2(SRC_TABLESWITCH, 0, &noteIndex_)) {
    return false;
  }

  if (!caseOffsets_.resize(tableGen.tableLength())) {
    ReportOutOfMemory(bce_->cx);
    return false;
  }

  MOZ_ASSERT(top_ == bce_->bytecodeSection().offset());
  if (!bce_->emitN(JSOP_TABLESWITCH,
                   JSOP_TABLESWITCH_LENGTH - sizeof(jsbytecode))) {
    return false;
  }

  // Skip default offset.
  jsbytecode* pc = bce_->bytecodeSection().code(top_ + JUMP_OFFSET_LEN);

  // Fill in switch bounds, which we know fit in 16-bit offsets.
  SET_JUMP_OFFSET(pc, tableGen.low());
  SET_JUMP_OFFSET(pc + JUMP_OFFSET_LEN, tableGen.high());

  state_ = State::Table;
  return true;
}

bool SwitchEmitter::emitCaseOrDefaultJump(uint32_t caseIndex, bool isDefault) {
  MOZ_ASSERT(kind_ == Kind::Cond);

  if (isDefault) {
    if (!bce_->emitJump(JSOP_DEFAULT, &condSwitchDefaultOffset_)) {
      return false;
    }
    return true;
  }

  if (caseIndex > 0) {
    // Link the last JSOP_CASE's SRC_NEXTCASE to current JSOP_CASE for the
    // benefit of IonBuilder.
    if (!bce_->setSrcNoteOffset(
            caseNoteIndex_, SrcNote::NextCase::NextCaseOffset,
            bce_->bytecodeSection().offset() - lastCaseOffset_)) {
      return false;
    }
  }

  if (!bce_->newSrcNote2(SRC_NEXTCASE, 0, &caseNoteIndex_)) {
    return false;
  }

  JumpList caseJump;
  if (!bce_->emitJump(JSOP_CASE, &caseJump)) {
    return false;
  }
  caseOffsets_[caseIndex] = caseJump.offset;
  lastCaseOffset_ = caseJump.offset;

  if (caseIndex == 0) {
    // Switch note's second offset is to first JSOP_CASE.
    unsigned noteCount = bce_->bytecodeSection().notes().length();
    if (!bce_->setSrcNoteOffset(noteIndex_, 1, lastCaseOffset_ - top_)) {
      return false;
    }
    unsigned noteCountDelta =
        bce_->bytecodeSection().notes().length() - noteCount;
    if (noteCountDelta != 0) {
      caseNoteIndex_ += noteCountDelta;
    }
  }

  return true;
}

bool SwitchEmitter::prepareForCaseValue() {
  MOZ_ASSERT(kind_ == Kind::Cond);
  MOZ_ASSERT(state_ == State::Cond || state_ == State::Case);

  if (!bce_->emit1(JSOP_DUP)) {
    return false;
  }

  state_ = State::CaseValue;
  return true;
}

bool SwitchEmitter::emitCaseJump() {
  MOZ_ASSERT(kind_ == Kind::Cond);
  MOZ_ASSERT(state_ == State::CaseValue);

  if (!bce_->emit1(JSOP_STRICTEQ)) {
    return false;
  }

  if (!emitCaseOrDefaultJump(caseIndex_, false)) {
    return false;
  }
  caseIndex_++;

  state_ = State::Case;
  return true;
}

bool SwitchEmitter::emitImplicitDefault() {
  MOZ_ASSERT(kind_ == Kind::Cond);
  MOZ_ASSERT(state_ == State::Cond || state_ == State::Case);
  if (!emitCaseOrDefaultJump(0, true)) {
    return false;
  }

  caseIndex_ = 0;

  // No internal state after emitting default jump.
  return true;
}

bool SwitchEmitter::emitCaseBody() {
  MOZ_ASSERT(kind_ == Kind::Cond);
  MOZ_ASSERT(state_ == State::Cond || state_ == State::Case ||
             state_ == State::CaseBody || state_ == State::DefaultBody);

  tdzCacheCaseAndBody_.reset();

  if (state_ == State::Cond || state_ == State::Case) {
    // For cond switch, JSOP_DEFAULT is always emitted.
    if (!emitImplicitDefault()) {
      return false;
    }
  }

  JumpList caseJump;
  caseJump.offset = caseOffsets_[caseIndex_];
  if (!bce_->emitJumpTargetAndPatch(caseJump)) {
    return false;
  }

  JumpTarget here;
  if (!bce_->emitJumpTarget(&here)) {
    return false;
  }
  caseIndex_++;

  tdzCacheCaseAndBody_.emplace(bce_);

  state_ = State::CaseBody;
  return true;
}

bool SwitchEmitter::emitCaseBody(int32_t caseValue,
                                 const TableGenerator& tableGen) {
  MOZ_ASSERT(kind_ == Kind::Table);
  MOZ_ASSERT(state_ == State::Table || state_ == State::CaseBody ||
             state_ == State::DefaultBody);

  tdzCacheCaseAndBody_.reset();

  JumpTarget here;
  if (!bce_->emitJumpTarget(&here)) {
    return false;
  }
  caseOffsets_[tableGen.toCaseIndex(caseValue)] = here.offset;

  tdzCacheCaseAndBody_.emplace(bce_);

  state_ = State::CaseBody;
  return true;
}

bool SwitchEmitter::emitDefaultBody() {
  MOZ_ASSERT(state_ == State::Cond || state_ == State::Table ||
             state_ == State::Case || state_ == State::CaseBody);
  MOZ_ASSERT(!hasDefault_);

  tdzCacheCaseAndBody_.reset();

  if (state_ == State::Cond || state_ == State::Case) {
    // For cond switch, JSOP_DEFAULT is always emitted.
    if (!emitImplicitDefault()) {
      return false;
    }
  }
  JumpTarget here;
  if (!bce_->emitJumpTarget(&here)) {
    return false;
  }
  defaultJumpTargetOffset_ = here;

  tdzCacheCaseAndBody_.emplace(bce_);

  hasDefault_ = true;
  state_ = State::DefaultBody;
  return true;
}

bool SwitchEmitter::emitEnd() {
  MOZ_ASSERT(state_ == State::Cond || state_ == State::Table ||
             state_ == State::CaseBody || state_ == State::DefaultBody);

  tdzCacheCaseAndBody_.reset();

  if (!hasDefault_) {
    // If no default case, offset for default is to end of switch.
    if (!bce_->emitJumpTarget(&defaultJumpTargetOffset_)) {
      return false;
    }
  }
  MOZ_ASSERT(defaultJumpTargetOffset_.offset != -1);

  // Set the default offset (to end of switch if no default).
  jsbytecode* pc;
  if (kind_ == Kind::Cond) {
    pc = nullptr;
    bce_->patchJumpsToTarget(condSwitchDefaultOffset_,
                             defaultJumpTargetOffset_);
  } else {
    // Fill in the default jump target.
    pc = bce_->bytecodeSection().code(top_);
    SET_JUMP_OFFSET(pc, defaultJumpTargetOffset_.offset - top_);
    pc += JUMP_OFFSET_LEN;
  }

  // Set the SRC_SWITCH note's offset operand to tell end of switch.
  // This code is shared between table switch and cond switch.
  static_assert(unsigned(SrcNote::TableSwitch::EndOffset) ==
                    unsigned(SrcNote::CondSwitch::EndOffset),
                "{TableSwitch,CondSwitch}::EndOffset should be same");
  if (!bce_->setSrcNoteOffset(
          noteIndex_, SrcNote::TableSwitch::EndOffset,
          bce_->bytecodeSection().lastNonJumpTargetOffset() - top_)) {
    return false;
  }

  if (kind_ == Kind::Table) {
    // Skip over the already-initialized switch bounds.
    pc += 2 * JUMP_OFFSET_LEN;

    // Use the 'default' offset for missing cases.
    for (uint32_t i = 0, length = caseOffsets_.length(); i < length; i++) {
      if (caseOffsets_[i] == 0) {
        caseOffsets_[i] = defaultJumpTargetOffset_.offset;
      }
    }

    // Allocate resume index range.
    uint32_t firstResumeIndex = 0;
    mozilla::Span<ptrdiff_t> offsets =
        mozilla::MakeSpan(caseOffsets_.begin(), caseOffsets_.end());
    if (!bce_->allocateResumeIndexRange(offsets, &firstResumeIndex)) {
      return false;
    }
    SET_RESUMEINDEX(pc, firstResumeIndex);
  }

  // Patch breaks before leaving the scope, as all breaks are under the
  // lexical scope if it exists.
  if (!controlInfo_->patchBreaks(bce_)) {
    return false;
  }

  if (emitterScope_ && !emitterScope_->leave(bce_)) {
    return false;
  }

  emitterScope_.reset();
  tdzCacheLexical_.reset();

  controlInfo_.reset();

  state_ = State::End;
  return true;
}
