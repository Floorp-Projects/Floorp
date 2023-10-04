/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// [SMDOC] Column numbers
//
// Inside SpiderMonkey, column numbers are represented as 32-bit unsigned
// integers, either with 0-origin or 1-origin. Also, some parts of the engine
// use the highest bit of a column number as a tag to indicate Wasm frame.
//
// In a 0-origin context, column 0 is the first character of the line.
// In a 1-origin context, column 1 is the first character of the line,
// for example:
//
//           function foo() { ... }
//           ^              ^
// 0-origin: 0              15
// 1-origin: 1              16
//
// The column 0 in 1-origin is an invalid sentinel value used in some places,
// such as differential testing (bug 1848467).
//
// These classes help figuring out which origin and tag the column number
// uses, and also help converting between them.
//
// Eventually all SpiderMonkey API and internal use should switch to 1-origin
// (bug 1144340).

#ifndef js_ColumnNumber_h
#define js_ColumnNumber_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_IMPLICIT

#include <limits>    // std::numeric_limits
#include <stdint.h>  // uint32_t

namespace JS {

// Wasm function index.
//
// This class is used as parameter or return type of
// TaggedColumnNumberWithOrigin class below.
struct WasmFunctionIndex {
  // TaggedColumnNumberWithOrigin uses the highest bit as a tag.
  static constexpr uint32_t Limit = std::numeric_limits<int32_t>::max() / 2;

  // For wasm frames, the function index is returned as the column with the
  // high bit set. In paths that format error stacks into strings, this
  // information can be used to synthesize a proper wasm frame. But when raw
  // column numbers are handed out, we just fix them to the first column to
  // avoid confusion.
  static constexpr uint32_t DefaultBinarySourceColumnNumberZeroOrigin = 0;
  static constexpr uint32_t DefaultBinarySourceColumnNumberOneOrigin = 1;

 private:
  uint32_t value_ = 0;

 public:
  constexpr WasmFunctionIndex() = default;
  constexpr WasmFunctionIndex(const WasmFunctionIndex& other) = default;

  inline explicit WasmFunctionIndex(uint32_t value) : value_(value) {
    MOZ_ASSERT(valid());
  }

  uint32_t value() const { return value_; }

  bool valid() const { return value_ <= Limit; }
};

// The offset between 2 column numbers.
struct ColumnNumberOffset {
 private:
  int32_t value_ = 0;

 public:
  constexpr ColumnNumberOffset() = default;
  constexpr ColumnNumberOffset(const ColumnNumberOffset& other) = default;

  inline explicit ColumnNumberOffset(int32_t value) : value_(value) {}

  static constexpr ColumnNumberOffset zero() { return ColumnNumberOffset(); }

  bool operator==(const ColumnNumberOffset& rhs) const {
    return value_ == rhs.value_;
  }

  bool operator!=(const ColumnNumberOffset& rhs) const {
    return !(*this == rhs);
  }

  int32_t value() const { return value_; }
};

namespace detail {

template <typename T>
struct TaggedColumnNumberWithOrigin;

// Shared implementation of {,Limited}ColumnNumber{Zero,One}Origin classes.
//
// Origin can be either 0 or 1.
// LimitValue being 0 means there's no limit.
template <uint32_t Origin, uint32_t LimitValue = 0>
struct ColumnNumberWithOrigin {
  static_assert(Origin == 0 || Origin == 1);

 protected:
  uint32_t value_ = Origin;

  template <typename T>
  friend struct TaggedColumnNumberWithOrigin;

 public:
  constexpr ColumnNumberWithOrigin() = default;
  ColumnNumberWithOrigin(const ColumnNumberWithOrigin& other) = default;
  ColumnNumberWithOrigin& operator=(const ColumnNumberWithOrigin& other) =
      default;

  explicit ColumnNumberWithOrigin(uint32_t value) : value_(value) {
    MOZ_ASSERT(valid());
  }

  static constexpr ColumnNumberWithOrigin<Origin, LimitValue> zero() {
    return ColumnNumberWithOrigin<Origin, LimitValue>();
  }

  bool operator==(const ColumnNumberWithOrigin<Origin, LimitValue>& rhs) const {
    return value_ == rhs.value_;
  }

  bool operator!=(const ColumnNumberWithOrigin<Origin, LimitValue>& rhs) const {
    return !(*this == rhs);
  }

  ColumnNumberWithOrigin<Origin, LimitValue> operator+(
      const ColumnNumberOffset& offset) const {
    MOZ_ASSERT(valid());
    MOZ_ASSERT(ptrdiff_t(value_) + offset.value() >= 0);
    return ColumnNumberWithOrigin<Origin, LimitValue>(value_ + offset.value());
  }

  ColumnNumberWithOrigin<Origin, LimitValue> operator-(
      const ColumnNumberOffset& offset) const {
    MOZ_ASSERT(valid());
    MOZ_ASSERT(ptrdiff_t(value_) - offset.value() >= 0);
    return ColumnNumberWithOrigin<Origin, LimitValue>(value_ - offset.value());
  }
  ColumnNumberOffset operator-(
      const ColumnNumberWithOrigin<Origin, LimitValue>& other) const {
    MOZ_ASSERT(valid());
    return ColumnNumberOffset(int32_t(value_) -
                              int32_t(other.zeroOriginValue()));
  }

  ColumnNumberWithOrigin<Origin, LimitValue>& operator+=(
      const ColumnNumberOffset& offset) {
    MOZ_ASSERT(valid());
    MOZ_ASSERT(ptrdiff_t(value_) + offset.value() >= 0);
    value_ += offset.value();
    MOZ_ASSERT(valid());
    return *this;
  }
  ColumnNumberWithOrigin<Origin, LimitValue>& operator-=(
      const ColumnNumberOffset& offset) {
    MOZ_ASSERT(valid());
    MOZ_ASSERT(ptrdiff_t(value_) - offset.value() >= 0);
    value_ -= offset.value();
    MOZ_ASSERT(valid());
    return *this;
  }

  bool operator<(const ColumnNumberWithOrigin<Origin, LimitValue>& rhs) const {
    MOZ_ASSERT(valid());
    MOZ_ASSERT(rhs.valid());
    return value_ < rhs.value_;
  }
  bool operator<=(const ColumnNumberWithOrigin<Origin, LimitValue>& rhs) const {
    MOZ_ASSERT(valid());
    MOZ_ASSERT(rhs.valid());
    return value_ <= rhs.value_;
  }
  bool operator>(const ColumnNumberWithOrigin<Origin, LimitValue>& rhs) const {
    MOZ_ASSERT(valid());
    MOZ_ASSERT(rhs.valid());
    return value_ > rhs.value_;
  }
  bool operator>=(const ColumnNumberWithOrigin<Origin, LimitValue>& rhs) const {
    MOZ_ASSERT(valid());
    MOZ_ASSERT(rhs.valid());
    return value_ >= rhs.value_;
  }

  // Convert between origins.
  uint32_t zeroOriginValue() const {
    MOZ_ASSERT(valid());

    if constexpr (Origin == 0) {
      return value_;
    }

    if (value_ == 0) {
      // 1-origin uses 0 as special value, but 0-origin doesn't have it.
      return 0;
    }
    return value_ - 1;
  }
  uint32_t oneOriginValue() const {
    MOZ_ASSERT(valid());

    if constexpr (Origin == 0) {
      return value_ + 1;
    }

    return value_;
  }

  uint32_t* addressOfValueForTranscode() { return &value_; }

  bool valid() const {
    if constexpr (LimitValue == 0) {
      return true;
    }

    return value_ <= LimitValue;
  }
};

// See the comment for LimitedColumnNumberZeroOrigin below
static constexpr uint32_t ColumnNumberZeroOriginLimit =
    std::numeric_limits<int32_t>::max() / 2 - 1;
static constexpr uint32_t ColumnNumberOneOriginLimit =
    std::numeric_limits<int32_t>::max() / 2;

}  // namespace detail

// Column number in 0-origin with 31-bit limit.
//
// Various parts of the engine requires the column number be represented in
// 31 bits.
//
// See:
//  * TaggedColumnNumberWithOrigin
//  * TokenStreamAnyChars::checkOptions
//  * SourceNotes::isRepresentable
//  * WasmFrameIter::computeLine
struct LimitedColumnNumberZeroOrigin
    : public detail::ColumnNumberWithOrigin<
          0, detail::ColumnNumberZeroOriginLimit> {
 private:
  using Base =
      detail::ColumnNumberWithOrigin<0, detail::ColumnNumberZeroOriginLimit>;

 public:
  static constexpr uint32_t Limit = detail::ColumnNumberZeroOriginLimit;

  static_assert(uint32_t(Limit + Limit) > Limit,
                "Adding Limit should not overflow");

  using Base::Base;

  LimitedColumnNumberZeroOrigin() = default;
  LimitedColumnNumberZeroOrigin(const LimitedColumnNumberZeroOrigin& other) =
      default;
  MOZ_IMPLICIT LimitedColumnNumberZeroOrigin(const Base& other) : Base(other) {}

  explicit LimitedColumnNumberZeroOrigin(
      const detail::ColumnNumberWithOrigin<
          1, detail::ColumnNumberOneOriginLimit>& other)
      : Base(other.zeroOriginValue()) {}

  static LimitedColumnNumberZeroOrigin limit() {
    return LimitedColumnNumberZeroOrigin(Limit);
  }

  // Convert from column number without limit.
  // Column number above the limit is saturated to the limit.
  static LimitedColumnNumberZeroOrigin fromUnlimited(uint32_t value) {
    if (value > Limit) {
      return LimitedColumnNumberZeroOrigin(Limit);
    }
    return LimitedColumnNumberZeroOrigin(value);
  }
  static LimitedColumnNumberZeroOrigin fromUnlimited(
      const ColumnNumberWithOrigin<0, 0>& value) {
    return fromUnlimited(value.zeroOriginValue());
  }
};

// Column number in 1-origin with 31-bit limit.
struct LimitedColumnNumberOneOrigin
    : public detail::ColumnNumberWithOrigin<
          1, detail::ColumnNumberOneOriginLimit> {
 private:
  using Base =
      detail::ColumnNumberWithOrigin<1, detail::ColumnNumberOneOriginLimit>;

 public:
  static constexpr uint32_t Limit = detail::ColumnNumberOneOriginLimit;

  static_assert(uint32_t(Limit + Limit) > Limit,
                "Adding Limit should not overflow");

  using Base::Base;

  LimitedColumnNumberOneOrigin() = default;
  LimitedColumnNumberOneOrigin(const LimitedColumnNumberOneOrigin& other) =
      default;
  MOZ_IMPLICIT LimitedColumnNumberOneOrigin(const Base& other) : Base(other) {}

  explicit LimitedColumnNumberOneOrigin(
      const detail::ColumnNumberWithOrigin<
          0, detail::ColumnNumberZeroOriginLimit>& other)
      : Base(other.oneOriginValue()) {}

  static LimitedColumnNumberOneOrigin limit() {
    return LimitedColumnNumberOneOrigin(Limit);
  }

  static LimitedColumnNumberOneOrigin fromUnlimited(uint32_t value) {
    if (value > Limit) {
      return LimitedColumnNumberOneOrigin(Limit);
    }
    return LimitedColumnNumberOneOrigin(value);
  }
  static LimitedColumnNumberOneOrigin fromUnlimited(
      const ColumnNumberWithOrigin<1, 0>& value) {
    return fromUnlimited(value.oneOriginValue());
  }
};

// Column number in 0-origin.
struct ColumnNumberZeroOrigin : public detail::ColumnNumberWithOrigin<0> {
 private:
  using Base = detail::ColumnNumberWithOrigin<0>;

 public:
  using Base::Base;
  using Base::operator=;

  ColumnNumberZeroOrigin() = default;
  ColumnNumberZeroOrigin(const ColumnNumberZeroOrigin& other) = default;
  ColumnNumberZeroOrigin& operator=(ColumnNumberZeroOrigin&) = default;

  MOZ_IMPLICIT ColumnNumberZeroOrigin(const Base& other) : Base(other) {}

  explicit ColumnNumberZeroOrigin(
      const detail::ColumnNumberWithOrigin<1>& other)
      : Base(other.zeroOriginValue()) {}

  explicit ColumnNumberZeroOrigin(const LimitedColumnNumberZeroOrigin& other)
      : Base(other.zeroOriginValue()) {}
  explicit ColumnNumberZeroOrigin(const LimitedColumnNumberOneOrigin& other)
      : Base(other.zeroOriginValue()) {}
};

// Column number in 1-origin.
struct ColumnNumberOneOrigin : public detail::ColumnNumberWithOrigin<1> {
 private:
  using Base = detail::ColumnNumberWithOrigin<1>;

 public:
  using Base::Base;
  using Base::operator=;

  ColumnNumberOneOrigin() = default;
  ColumnNumberOneOrigin(const ColumnNumberOneOrigin& other) = default;
  ColumnNumberOneOrigin& operator=(ColumnNumberOneOrigin&) = default;

  MOZ_IMPLICIT ColumnNumberOneOrigin(const Base& other) : Base(other) {}

  explicit ColumnNumberOneOrigin(const detail::ColumnNumberWithOrigin<0>& other)
      : Base(other.oneOriginValue()) {}

  explicit ColumnNumberOneOrigin(const LimitedColumnNumberZeroOrigin& other)
      : Base(other.oneOriginValue()) {}
  explicit ColumnNumberOneOrigin(const LimitedColumnNumberOneOrigin& other)
      : Base(other.oneOriginValue()) {}
};

namespace detail {

// Either column number with limit, or Wasm function index.
//
// In order to pass the Wasm frame's (url, bytecode-offset, func-index) tuple
// through the existing (url, line, column) tuple, it tags the highest bit of
// the column to indicate "this is a wasm frame".
//
// When knowing clients see this bit, they shall render the tuple
// (url, line, column|bit) as "url:wasm-function[column]:0xline" according
// to the WebAssembly Web API's Developer-Facing Display Conventions.
//   https://webassembly.github.io/spec/web-api/index.html#conventions
// The wasm bytecode offset continues to be passed as the JS line to avoid
// breaking existing devtools code written when this used to be the case.
//
// 0b0YYYYYYY_YYYYYYYY_YYYYYYYY_YYYYYYYY LimitedColumnNumberT
// 0b1YYYYYYY_YYYYYYYY_YYYYYYYY_YYYYYYYY WasmFunctionIndex
//
// The tagged colum number shouldn't escape the JS engine except for the
// following places:
//   * SavedFrame API which can directly access WASM frame's info
//   * ubi::Node API which can also directly access WASM frame's info
template <typename LimitedColumnNumberT>
struct TaggedColumnNumberWithOrigin {
  static constexpr uint32_t WasmFunctionTag = 1u << 31;

  static_assert((WasmFunctionIndex::Limit & WasmFunctionTag) == 0);
  static_assert((LimitedColumnNumberT::Limit & WasmFunctionTag) == 0);

 protected:
  uint32_t value_ = 0;

  explicit TaggedColumnNumberWithOrigin(uint32_t value) : value_(value) {}

 public:
  constexpr TaggedColumnNumberWithOrigin() = default;
  TaggedColumnNumberWithOrigin(
      const TaggedColumnNumberWithOrigin<LimitedColumnNumberT>& other) =
      default;

  explicit TaggedColumnNumberWithOrigin(const LimitedColumnNumberT& other)
      : value_(other.value_) {
    MOZ_ASSERT(isLimitedColumnNumber());
  }
  explicit TaggedColumnNumberWithOrigin(const WasmFunctionIndex& other)
      : value_(other.value() | WasmFunctionTag) {
    MOZ_ASSERT(isWasmFunctionIndex());
  }

  static TaggedColumnNumberWithOrigin<LimitedColumnNumberT> fromRaw(
      uint32_t value) {
    return TaggedColumnNumberWithOrigin<LimitedColumnNumberT>(value);
  }

  bool operator==(
      const TaggedColumnNumberWithOrigin<LimitedColumnNumberT>& rhs) const {
    return value_ == rhs.value_;
  }

  bool operator!=(
      const TaggedColumnNumberWithOrigin<LimitedColumnNumberT>& rhs) const {
    return !(*this == rhs);
  }

  bool isLimitedColumnNumber() const { return !isWasmFunctionIndex(); }

  bool isWasmFunctionIndex() const { return !!(value_ & WasmFunctionTag); }

  LimitedColumnNumberT toLimitedColumnNumber() const {
    MOZ_ASSERT(isLimitedColumnNumber());
    return LimitedColumnNumberT(value_);
  }

  WasmFunctionIndex toWasmFunctionIndex() const {
    MOZ_ASSERT(isWasmFunctionIndex());
    return WasmFunctionIndex(value_ & ~WasmFunctionTag);
  }

  uint32_t zeroOriginValue() const {
    return isWasmFunctionIndex()
               ? WasmFunctionIndex::DefaultBinarySourceColumnNumberZeroOrigin
               : toLimitedColumnNumber().zeroOriginValue();
  }
  uint32_t oneOriginValue() const {
    return isWasmFunctionIndex()
               ? WasmFunctionIndex::DefaultBinarySourceColumnNumberOneOrigin
               : toLimitedColumnNumber().oneOriginValue();
  }

  uint32_t rawValue() const { return value_; }

  uint32_t* addressOfValueForTranscode() { return &value_; }
};

}  // namespace detail

class TaggedColumnNumberZeroOrigin
    : public detail::TaggedColumnNumberWithOrigin<
          LimitedColumnNumberZeroOrigin> {
 private:
  using Base =
      detail::TaggedColumnNumberWithOrigin<LimitedColumnNumberZeroOrigin>;

 public:
  using Base::Base;
  using Base::WasmFunctionTag;

  TaggedColumnNumberZeroOrigin() = default;
  TaggedColumnNumberZeroOrigin(const TaggedColumnNumberZeroOrigin& other) =
      default;
  MOZ_IMPLICIT TaggedColumnNumberZeroOrigin(const Base& other) : Base(other) {}

  explicit TaggedColumnNumberZeroOrigin(
      const detail::TaggedColumnNumberWithOrigin<LimitedColumnNumberOneOrigin>&
          other)
      : Base(other.isWasmFunctionIndex() ? other.rawValue()
                                         : other.zeroOriginValue()) {}
};

class TaggedColumnNumberOneOrigin : public detail::TaggedColumnNumberWithOrigin<
                                        LimitedColumnNumberOneOrigin> {
 private:
  using Base =
      detail::TaggedColumnNumberWithOrigin<LimitedColumnNumberOneOrigin>;

 public:
  using Base::Base;
  using Base::WasmFunctionTag;

  TaggedColumnNumberOneOrigin() = default;
  TaggedColumnNumberOneOrigin(const TaggedColumnNumberOneOrigin& other) =
      default;
  MOZ_IMPLICIT TaggedColumnNumberOneOrigin(const Base& other) : Base(other) {}

  explicit TaggedColumnNumberOneOrigin(
      const detail::TaggedColumnNumberWithOrigin<LimitedColumnNumberZeroOrigin>&
          other)
      : Base(other.isWasmFunctionIndex() ? other.rawValue()
                                         : other.oneOriginValue()) {}

  static TaggedColumnNumberOneOrigin forDifferentialTesting() {
    return TaggedColumnNumberOneOrigin(LimitedColumnNumberOneOrigin(0));
  }
};

}  // namespace JS

#endif /* js_ColumnNumber_h */
