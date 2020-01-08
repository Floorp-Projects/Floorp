/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_BytecodeLocation_h
#define vm_BytecodeLocation_h

#include "js/TypeDecls.h"
#include "vm/BytecodeUtil.h"
#include "vm/StringType.h"

namespace js {

typedef uint32_t RawBytecodeLocationOffset;

class PropertyName;

class BytecodeLocationOffset {
  RawBytecodeLocationOffset rawOffset_;

 public:
  explicit BytecodeLocationOffset(RawBytecodeLocationOffset offset)
      : rawOffset_(offset) {}

  RawBytecodeLocationOffset rawOffset() const { return rawOffset_; }
};

typedef jsbytecode* RawBytecode;

// A immutable representation of a program location
//
class BytecodeLocation {
  RawBytecode rawBytecode_;
#ifdef DEBUG
  const JSScript* debugOnlyScript_;
#endif

  // Construct a new BytecodeLocation, while borrowing scriptIdentity
  // from some other BytecodeLocation.
  BytecodeLocation(const BytecodeLocation& loc, RawBytecode pc)
      : rawBytecode_(pc)
#ifdef DEBUG
        ,
        debugOnlyScript_(loc.debugOnlyScript_)
#endif
  {
    MOZ_ASSERT(isValid());
  }

 public:
  // Disallow the creation of an uninitialized location.
  BytecodeLocation() = delete;

  BytecodeLocation(const JSScript* script, RawBytecode pc)
      : rawBytecode_(pc)
#ifdef DEBUG
        ,
        debugOnlyScript_(script)
#endif
  {
    MOZ_ASSERT(isValid());
  }

  RawBytecode toRawBytecode() const { return rawBytecode_; }

  // Return true if this bytecode location is valid for the given script.
  // This includes the location 1-past the end of the bytecode.
  JS_PUBLIC_API bool isValid(const JSScript* script) const;

  // Return true if this bytecode location is within the bounds of the
  // bytecode for a given script.
  bool isInBounds(const JSScript* script) const;

  uint32_t bytecodeToOffset(const JSScript* script) const;

  uint32_t tableSwitchCaseOffset(const JSScript* script,
                                 uint32_t caseIndex) const;

  uint32_t getJumpTargetOffset(const JSScript* script) const;

  uint32_t getTableSwitchDefaultOffset(const JSScript* script) const;

  uint32_t useCount() const;

  uint32_t defCount() const;

  int32_t jumpOffset() const { return GET_JUMP_OFFSET(rawBytecode_); }
  int32_t codeOffset() const { return GET_CODE_OFFSET(rawBytecode_); }

  PropertyName* getPropertyName(const JSScript* script) const;

  Scope* innermostScope(const JSScript* script) const;

#ifdef DEBUG
  bool hasSameScript(const BytecodeLocation& other) const {
    return debugOnlyScript_ == other.debugOnlyScript_;
  }
#endif

  // Overloaded operators

  bool operator==(const BytecodeLocation& other) const {
    MOZ_ASSERT(this->debugOnlyScript_ == other.debugOnlyScript_);
    return rawBytecode_ == other.rawBytecode_;
  }

  bool operator!=(const BytecodeLocation& other) const {
    return !(other == *this);
  }

  bool operator<(const BytecodeLocation& other) const {
    MOZ_ASSERT(this->debugOnlyScript_ == other.debugOnlyScript_);
    return rawBytecode_ < other.rawBytecode_;
  }

  // It is traditional to represent the rest of the relational operators
  // using operator<, so we don't need to assert for these.
  bool operator>(const BytecodeLocation& other) const { return other < *this; }

  bool operator<=(const BytecodeLocation& other) const {
    return !(other < *this);
  }

  bool operator>=(const BytecodeLocation& other) const {
    return !(*this < other);
  }

  // Return the next bytecode
  BytecodeLocation next() const {
    return BytecodeLocation(*this,
                            rawBytecode_ + GetBytecodeLength(rawBytecode_));
  }

  // Add an offset.
  BytecodeLocation operator+(const BytecodeLocationOffset& offset) {
    return BytecodeLocation(*this, rawBytecode_ + offset.rawOffset());
  }

  // Identity Checks
  bool is(JSOp op) const {
    MOZ_ASSERT(isInBounds());
    return getOp() == op;
  }

  // Accessors:

  uint32_t length() const { return GetBytecodeLength(rawBytecode_); }

  bool isJumpTarget() const { return BytecodeIsJumpTarget(getOp()); }

  bool isJump() const { return IsJumpOpcode(getOp()); }

  bool opHasTypeSet() const { return BytecodeOpHasTypeSet(getOp()); }

  bool fallsThrough() const { return BytecodeFallsThrough(getOp()); }

  uint32_t icIndex() const { return GET_ICINDEX(rawBytecode_); }

  uint32_t local() const { return GET_LOCALNO(rawBytecode_); }

  uint16_t arg() const { return GET_ARGNO(rawBytecode_); }

  bool isEqualityOp() const { return IsEqualityOp(getOp()); }

  bool isStrictEqualityOp() const { return IsStrictEqualityOp(getOp()); }

  bool isDetectingOp() const { return IsDetecting(getOp()); }

  bool isNameOp() const { return IsNameOp(getOp()); }

  // Accessors:
  JSOp getOp() const { return JSOp(*rawBytecode_); }

  BytecodeLocation getJumpTarget() const {
    // The default target of a JSOP_TABLESWITCH also follows this format.
    MOZ_ASSERT(isJump() || is(JSOP_TABLESWITCH));
    return BytecodeLocation(*this,
                            rawBytecode_ + GET_JUMP_OFFSET(rawBytecode_));
  }

  // Return the 'low' parameter to the tableswitch opcode
  int32_t getTableSwitchLow() const {
    MOZ_ASSERT(is(JSOP_TABLESWITCH));
    return GET_JUMP_OFFSET(rawBytecode_ + JUMP_OFFSET_LEN);
  }

  // Return the 'high' parameter to the tableswitch opcode
  int32_t getTableSwitchHigh() const {
    MOZ_ASSERT(is(JSOP_TABLESWITCH));
    return GET_JUMP_OFFSET(rawBytecode_ + (2 * JUMP_OFFSET_LEN));
  }

#ifdef DEBUG
  // To ease writing assertions
  bool isValid() const { return isValid(debugOnlyScript_); }

  bool isInBounds() const { return isInBounds(debugOnlyScript_); }
#endif
};

}  // namespace js

#endif
