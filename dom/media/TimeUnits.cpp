/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdint>
#include <cmath>
#include <inttypes.h>
#include <limits>
#include <type_traits>

#include "TimeUnits.h"
#include "Intervals.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "nsDebug.h"
#include "nsPrintfCString.h"
#include "nsStringFwd.h"

namespace mozilla::media {
class TimeIntervals;
}  // namespace mozilla::media

namespace mozilla {

namespace media {

TimeUnit TimeUnit::FromSeconds(double aValue, int64_t aBase) {
  MOZ_ASSERT(!std::isnan(aValue));
  MOZ_ASSERT(aBase > 0);

  if (std::isinf(aValue)) {
    return aValue > 0 ? FromInfinity() : FromNegativeInfinity();
  }
  // Warn that a particular value won't be able to be roundtrip at the same
  // base -- we can keep this for some time until we're confident this is
  // stable.
  double inBase = aValue * static_cast<double>(aBase);
  if (std::abs(inBase) >
      static_cast<double>(std::numeric_limits<int64_t>::max())) {
    NS_WARNING(
        nsPrintfCString("Warning: base %" PRId64
                        " is too high to represent %lfs, returning Infinity.",
                        aBase, aValue)
            .get());
    if (inBase > 0) {
      return TimeUnit::FromInfinity();
    }
    return TimeUnit::FromNegativeInfinity();
  }

  // inBase can large enough that it doesn't map to an exact integer, warn in
  // this case. This happens if aBase is large, and so the loss of precision is
  // likely small.
  if (inBase > std::pow(2, std::numeric_limits<double>::digits) - 1) {
    NS_WARNING(nsPrintfCString("Warning: base %" PRId64
                               " is too high to represent %lfs accurately.",
                               aBase, aValue)
                   .get());
  }
  return TimeUnit(static_cast<int64_t>(std::round(inBase)), aBase);
}

TimeUnit TimeUnit::FromInfinity() { return TimeUnit(INT64_MAX); }

TimeUnit TimeUnit::FromNegativeInfinity() { return TimeUnit(INT64_MIN); }

TimeUnit TimeUnit::FromTimeDuration(const TimeDuration& aDuration) {
  // This could be made to choose the base
  return TimeUnit(AssertedCast<int64_t>(aDuration.ToMicroseconds()),
                  USECS_PER_S);
}

TimeUnit TimeUnit::Invalid() {
  TimeUnit ret;
  ret.mTicks = CheckedInt64(INT64_MAX);
  // Force an overflow to render the CheckedInt invalid.
  ret.mTicks += 1;
  return ret;
}

int64_t TimeUnit::ToMilliseconds() const { return ToCommonUnit(MSECS_PER_S); }

int64_t TimeUnit::ToMicroseconds() const { return ToCommonUnit(USECS_PER_S); }

int64_t TimeUnit::ToNanoseconds() const { return ToCommonUnit(NSECS_PER_S); }

int64_t TimeUnit::ToTicksAtRate(int64_t aRate) const {
  // Common case
  if (aRate == mBase) {
    return mTicks.value();
  }
  // Approximation
  return mTicks.value() * aRate / mBase;
}

double TimeUnit::ToSeconds() const {
  if (IsPosInf()) {
    return PositiveInfinity<double>();
  }
  if (IsNegInf()) {
    return NegativeInfinity<double>();
  }
  return static_cast<double>(mTicks.value()) / static_cast<double>(mBase);
}

nsCString TimeUnit::ToString() const {
  nsCString dump;
  if (mTicks.isValid()) {
    dump += nsPrintfCString("{%" PRId64 ",%" PRId64 "}", mTicks.value(), mBase);
  } else {
    dump += nsLiteralCString("{invalid}"_ns);
  }
  return dump;
}

TimeDuration TimeUnit::ToTimeDuration() const {
  return TimeDuration::FromSeconds(ToSeconds());
}

bool TimeUnit::IsInfinite() const { return IsPosInf() || IsNegInf(); }

bool TimeUnit::IsPositive() const { return mTicks.value() > 0; }

bool TimeUnit::IsPositiveOrZero() const { return mTicks.value() >= 0; }

bool TimeUnit::IsZero() const { return mTicks.value() == 0; }

bool TimeUnit::IsNegative() const { return mTicks.value() < 0; }

// Returns true if the fractions are equal when converted to the smallest
// base.
bool TimeUnit::EqualsAtLowestResolution(const TimeUnit& aOther) const {
  MOZ_ASSERT(IsValid() && aOther.IsValid());
  if (aOther.mBase == mBase) {
    return mTicks == aOther.mTicks;
  }
  if (mBase > aOther.mBase) {
    TimeUnit thisInBase = ToBase(aOther.mBase);
    return thisInBase.mTicks == aOther.mTicks;
  }
  TimeUnit otherInBase = aOther.ToBase(mBase);
  return otherInBase.mTicks == mTicks;
}

// Strict equality -- the fractions must be exactly equal
bool TimeUnit::operator==(const TimeUnit& aOther) const {
  MOZ_ASSERT(IsValid() && aOther.IsValid());
  if (aOther.mBase == mBase) {
    return mTicks == aOther.mTicks;
  }
  // debatable mathematically
  if ((IsPosInf() && aOther.IsPosInf()) || (IsNegInf() && aOther.IsNegInf())) {
    return true;
  }
  if ((IsPosInf() && !aOther.IsPosInf()) ||
      (IsNegInf() && !aOther.IsNegInf())) {
    return false;
  }
  CheckedInt<int64_t> lhs = mTicks * aOther.mBase;
  CheckedInt<int64_t> rhs = aOther.mTicks * mBase;
  if (lhs.isValid() && rhs.isValid()) {
    return lhs == rhs;
  }
  // Reduce the fractions and try again
  const TimeUnit a = Reduced();
  const TimeUnit b = aOther.Reduced();
  lhs = a.mTicks * b.mBase;
  rhs = b.mTicks * a.mBase;

  if (lhs.isValid() && rhs.isValid()) {
    return lhs.value() == rhs.value();
  }
  // last ditch, convert the reduced fractions to doubles
  double lhsFloating =
      static_cast<double>(a.mTicks.value()) * static_cast<double>(a.mBase);
  double rhsFloating =
      static_cast<double>(b.mTicks.value()) * static_cast<double>(b.mBase);

  return lhsFloating == rhsFloating;
}
bool TimeUnit::operator!=(const TimeUnit& aOther) const {
  MOZ_ASSERT(IsValid() && aOther.IsValid());
  return !(aOther == *this);
}
bool TimeUnit::operator>=(const TimeUnit& aOther) const {
  MOZ_ASSERT(IsValid() && aOther.IsValid());
  if (aOther.mBase == mBase) {
    return mTicks.value() >= aOther.mTicks.value();
  }
  if ((!IsPosInf() && aOther.IsPosInf()) ||
      (IsNegInf() && !aOther.IsNegInf())) {
    return false;
  }
  if ((IsPosInf() && !aOther.IsPosInf()) ||
      (!IsNegInf() && aOther.IsNegInf())) {
    return true;
  }
  CheckedInt<int64_t> lhs = mTicks * aOther.mBase;
  CheckedInt<int64_t> rhs = aOther.mTicks * mBase;
  if (lhs.isValid() && rhs.isValid()) {
    return lhs.value() >= rhs.value();
  }
  // Reduce the fractions and try again
  const TimeUnit a = Reduced();
  const TimeUnit b = aOther.Reduced();
  lhs = a.mTicks * b.mBase;
  rhs = b.mTicks * a.mBase;

  if (lhs.isValid() && rhs.isValid()) {
    return lhs.value() >= rhs.value();
  }
  // last ditch, convert the reduced fractions to doubles
  return ToSeconds() >= aOther.ToSeconds();
}
bool TimeUnit::operator>(const TimeUnit& aOther) const {
  return !(*this <= aOther);
}
bool TimeUnit::operator<=(const TimeUnit& aOther) const {
  MOZ_ASSERT(IsValid() && aOther.IsValid());
  if (aOther.mBase == mBase) {
    return mTicks.value() <= aOther.mTicks.value();
  }
  if ((!IsPosInf() && aOther.IsPosInf()) ||
      (IsNegInf() && !aOther.IsNegInf())) {
    return true;
  }
  if ((IsPosInf() && !aOther.IsPosInf()) ||
      (!IsNegInf() && aOther.IsNegInf())) {
    return false;
  }
  CheckedInt<int64_t> lhs = mTicks * aOther.mBase;
  CheckedInt<int64_t> rhs = aOther.mTicks * mBase;
  if (lhs.isValid() && rhs.isValid()) {
    return lhs.value() <= rhs.value();
  }
  // Reduce the fractions and try again
  const TimeUnit a = Reduced();
  const TimeUnit b = aOther.Reduced();
  lhs = a.mTicks * b.mBase;
  rhs = b.mTicks * a.mBase;
  if (lhs.isValid() && rhs.isValid()) {
    return lhs.value() <= rhs.value();
  }
  // last ditch, convert the reduced fractions to doubles
  return ToSeconds() <= aOther.ToSeconds();
}
bool TimeUnit::operator<(const TimeUnit& aOther) const {
  return !(*this >= aOther);
}

TimeUnit TimeUnit::operator%(const TimeUnit& aOther) const {
  MOZ_ASSERT(IsValid() && aOther.IsValid());
  if (aOther.mBase == mBase) {
    return TimeUnit(mTicks % aOther.mTicks, mBase);
  }
  // This path can be made better if need be.
  double a = ToSeconds();
  double b = aOther.ToSeconds();
  return TimeUnit::FromSeconds(fmod(a, b), mBase);
}

TimeUnit TimeUnit::operator+(const TimeUnit& aOther) const {
  if (IsInfinite() || aOther.IsInfinite()) {
    // When adding at least one infinite value, the result is either
    // +/-Inf, or NaN. So do the calculation in floating point for
    // simplicity.
    double result = ToSeconds() + aOther.ToSeconds();
    return std::isnan(result) ? TimeUnit::Invalid() : FromSeconds(result);
  }
  if (aOther.mBase == mBase) {
    return TimeUnit(mTicks + aOther.mTicks, mBase);
  }
  if (aOther.IsZero()) {
    return *this;
  }
  if (IsZero()) {
    return aOther;
  }

  double error;
  TimeUnit inBase = aOther.ToBase(mBase, error);
  if (error == 0.0) {
    return *this + inBase;
  }

  // Last ditch: not exact
  double a = ToSeconds();
  double b = aOther.ToSeconds();
  return TimeUnit::FromSeconds(a + b, mBase);
}

TimeUnit TimeUnit::operator-(const TimeUnit& aOther) const {
  if (IsInfinite() || aOther.IsInfinite()) {
    // When subtracting at least one infinite value, the result is either
    // +/-Inf, or NaN. So do the calculation in floating point for
    // simplicity.
    double result = ToSeconds() - aOther.ToSeconds();
    return std::isnan(result) ? TimeUnit::Invalid() : FromSeconds(result);
  }
  if (aOther.mBase == mBase) {
    return TimeUnit(mTicks - aOther.mTicks, mBase);
  }
  if (aOther.IsZero()) {
    return *this;
  }

  if (IsZero()) {
    return TimeUnit(-aOther.mTicks, aOther.mBase);
  }

  double error = 0.0;
  TimeUnit inBase = aOther.ToBase(mBase, error);
  if (error == 0) {
    return *this - inBase;
  }

  // Last ditch: not exact
  double a = ToSeconds();
  double b = aOther.ToSeconds();
  return TimeUnit::FromSeconds(a - b, mBase);
}
TimeUnit& TimeUnit::operator+=(const TimeUnit& aOther) {
  if (aOther.mBase == mBase) {
    mTicks += aOther.mTicks;
    return *this;
  }
  *this = *this + aOther;
  return *this;
}
TimeUnit& TimeUnit::operator-=(const TimeUnit& aOther) {
  if (aOther.mBase == mBase) {
    mTicks -= aOther.mTicks;
    return *this;
  }
  *this = *this - aOther;
  return *this;
}

TimeUnit TimeUnit::MultDouble(double aVal) const {
  double multiplied = AssertedCast<double>(mTicks.value()) * aVal;
  // Check is the result of the multiplication can be represented exactly as
  // an integer, in a double.
  if (multiplied > std::pow(2, std::numeric_limits<double>::digits) - 1) {
    printf_stderr("TimeUnit tick count after multiplication %" PRId64
                  " * %lf is too"
                  " high for the result to be exact",
                  mTicks.value(), aVal);
    MOZ_CRASH();
  }
  // static_cast is ok, the magnitude of the number has been checked just above.
  return TimeUnit(static_cast<int64_t>(multiplied), mBase);
}

bool TimeUnit::IsValid() const { return mTicks.isValid(); }

bool TimeUnit::IsPosInf() const {
  return mTicks.isValid() && mTicks.value() == INT64_MAX;
}
bool TimeUnit::IsNegInf() const {
  return mTicks.isValid() && mTicks.value() == INT64_MIN;
}

int64_t TimeUnit::ToCommonUnit(int64_t aRatio) const {
  CheckedInt<int64_t> rv = mTicks;
  // Avoid the risk overflowing in common cases, e.g. converting a TimeUnit
  // with a base of 1e9 back to nanoseconds.
  if (mBase == aRatio) {
    return rv.value();
  }
  // Avoid overflowing in other common cases, e.g. converting a TimeUnit with
  // a base of 1e9 to microseconds: the denominator is divisible by the target
  // unit so we can reorder the computation and keep the number within int64_t
  // range.
  if (aRatio < mBase && (mBase % aRatio) == 0) {
    int64_t exactDivisor = mBase / aRatio;
    rv /= exactDivisor;
    return rv.value();
  }
  rv *= aRatio;
  rv /= mBase;
  if (rv.isValid()) {
    return rv.value();
  }
  // Last ditch, perform the computation in floating point.
  double ratioFloating = AssertedCast<double>(aRatio);
  double baseFloating = AssertedCast<double>(mBase);
  double ticksFloating = static_cast<double>(mTicks.value());
  double approx = ticksFloating * (ratioFloating / baseFloating);
  // Clamp to a valid range. If this is clamped it's outside any usable time
  // value even in nanoseconds (thousands of years).
  if (approx > static_cast<double>(std::numeric_limits<int64_t>::max())) {
    return std::numeric_limits<int64_t>::max();
  }
  if (approx < static_cast<double>(std::numeric_limits<int64_t>::lowest())) {
    return std::numeric_limits<int64_t>::lowest();
  }
  return static_cast<int64_t>(approx);
}

// Reduce a TimeUnit to the smallest possible ticks and base. This is useful
// to comparison with big time values that can otherwise overflow.
TimeUnit TimeUnit::Reduced() const {
  int64_t gcd = GCD(mTicks.value(), mBase);
  return TimeUnit(mTicks.value() / gcd, mBase / gcd);
}

double RoundToMicrosecondResolution(double aSeconds) {
  return std::round(aSeconds * USECS_PER_S) / USECS_PER_S;
}

TimeRanges TimeRanges::ToMicrosecondResolution() const {
  TimeRanges output;

  for (const auto& interval : mIntervals) {
    TimeRange reducedPrecision{RoundToMicrosecondResolution(interval.mStart),
                               RoundToMicrosecondResolution(interval.mEnd),
                               RoundToMicrosecondResolution(interval.mFuzz)};
    output += reducedPrecision;
  }
  return output;
}

};  // namespace media

}  // namespace mozilla
