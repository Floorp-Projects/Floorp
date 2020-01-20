/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RollingNumber_h_
#define mozilla_RollingNumber_h_

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include <limits>

namespace mozilla {

// Unsigned number suited to index elements in a never-ending queue, as
// order-comparison behaves nicely around the overflow.
//
// Additive operators work the same as for the underlying value type, but
// expect "small" jumps, as should normally happen when manipulating indices.
//
// Comparison functions are different, they keep the ordering based on the
// distance between numbers, modulo the value type range:
// If the distance is less than half the range of the value type, the usual
// ordering stays.
// 0 < 1, 2^23 < 2^24
// However if the distance is more than half the range, we assume that we are
// continuing along the queue, and therefore consider the smaller number to
// actually be greater!
// uint(-1) < 0.
//
// The expected usage is to always work on nearby rolling numbers: slowly
// incrementing/decrementing them, and translating&comparing them within a
// small window.
// To enforce this usage during development, debug-build assertions catch API
// calls involving distances of more than a *quarter* of the type range.
// In non-debug builds, all APIs will still work as consistently as possible
// without crashing, but performing operations on "distant" nunbers could lead
// to unexpected results.
template <typename T>
class RollingNumber {
  static_assert(!std::numeric_limits<T>::is_signed,
                "RollingNumber only accepts unsigned number types");

 public:
  using ValueType = T;

  RollingNumber() : mIndex(0) {}

  explicit RollingNumber(ValueType aIndex) : mIndex(aIndex) {}

  RollingNumber(const RollingNumber&) = default;
  RollingNumber& operator=(const RollingNumber&) = default;

  ValueType Value() const { return mIndex; }

  // Normal increments/decrements.

  RollingNumber& operator++() {
    ++mIndex;
    return *this;
  }

  RollingNumber operator++(int) { return RollingNumber{mIndex++}; }

  RollingNumber& operator--() {
    --mIndex;
    return *this;
  }

  RollingNumber operator--(int) { return RollingNumber{mIndex--}; }

  RollingNumber& operator+=(const ValueType& aIncrement) {
    MOZ_ASSERT(aIncrement <= MaxDiff);
    mIndex += aIncrement;
    return *this;
  }

  RollingNumber operator+(const ValueType& aIncrement) const {
    RollingNumber n = *this;
    return n += aIncrement;
  }

  RollingNumber& operator-=(const ValueType& aDecrement) {
    MOZ_ASSERT(aDecrement <= MaxDiff);
    mIndex -= aDecrement;
    return *this;
  }

  // Translate a RollingNumber by a negative value.
  RollingNumber operator-(const ValueType& aDecrement) const {
    RollingNumber n = *this;
    return n -= aDecrement;
  }

  // Distance between two RollingNumbers, giving a value.
  ValueType operator-(const RollingNumber& aOther) const {
    ValueType diff = mIndex - aOther.mIndex;
    MOZ_ASSERT(diff <= MaxDiff);
    return diff;
  }

  // Normal (in)equality operators.

  bool operator==(const RollingNumber& aOther) const {
    return mIndex == aOther.mIndex;
  }
  bool operator!=(const RollingNumber& aOther) const {
    return !(*this == aOther);
  }

  // Modified comparison operators.

  bool operator<(const RollingNumber& aOther) const {
    const T& a = mIndex;
    const T& b = aOther.mIndex;
    // static_cast needed because of possible integer promotion
    // (e.g., from uint8_t to int, which would make the test useless).
    const bool lessThanOther = static_cast<ValueType>(a - b) > MidWay;
    MOZ_ASSERT((lessThanOther ? (b - a) : (a - b)) <= MaxDiff);
    return lessThanOther;
  }

  bool operator<=(const RollingNumber& aOther) const {
    const T& a = mIndex;
    const T& b = aOther.mIndex;
    const bool lessishThanOther = static_cast<ValueType>(b - a) <= MidWay;
    MOZ_ASSERT((lessishThanOther ? (b - a) : (a - b)) <= MaxDiff);
    return lessishThanOther;
  }

  bool operator>=(const RollingNumber& aOther) const {
    const T& a = mIndex;
    const T& b = aOther.mIndex;
    const bool greaterishThanOther = static_cast<ValueType>(a - b) <= MidWay;
    MOZ_ASSERT((greaterishThanOther ? (a - b) : (b - a)) <= MaxDiff);
    return greaterishThanOther;
  }

  bool operator>(const RollingNumber& aOther) const {
    const T& a = mIndex;
    const T& b = aOther.mIndex;
    const bool greaterThanOther = static_cast<ValueType>(b - a) > MidWay;
    MOZ_ASSERT((greaterThanOther ? (a - b) : (b - a)) <= MaxDiff);
    return greaterThanOther;
  }

 private:
  // MidWay is used to split the type range in two, to decide how two numbers
  // are ordered.
  static const T MidWay = std::numeric_limits<T>::max() / 2;
#ifdef DEBUG
  // MaxDiff is the expected maximum difference between two numbers, either
  // during comparisons, or when adding/subtracting.
  // This is only used during debugging, to highlight algorithmic issues.
  static const T MaxDiff = std::numeric_limits<T>::max() / 4;
#endif
  ValueType mIndex;
};

}  // namespace mozilla

#endif  // mozilla_RollingNumber_h_
