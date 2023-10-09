/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TIME_UNITS_H
#define TIME_UNITS_H

#include <limits>
#include <type_traits>

#include "Intervals.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "nsPrintfCString.h"

namespace mozilla::media {
class TimeIntervals;
}  // namespace mozilla::media
// CopyChooser specialization for nsTArray
template <>
struct nsTArray_RelocationStrategy<mozilla::media::TimeIntervals> {
  using Type =
      nsTArray_RelocateUsingMoveConstructor<mozilla::media::TimeIntervals>;
};

namespace mozilla {

// Number of milliseconds per second. 1e3.
static const int64_t MSECS_PER_S = 1000;

// Number of microseconds per second. 1e6.
static const int64_t USECS_PER_S = 1000000;

// Number of nanoseconds per second. 1e9.
static const int64_t NSECS_PER_S = 1000000000;

namespace media {

#ifndef PROCESS_DECODE_LOG
#  define PROCESS_DECODE_LOG(sample)                                   \
    MOZ_LOG(sPDMLog, mozilla::LogLevel::Verbose,                       \
            ("ProcessDecode: mDuration=%" PRIu64 "µs ; mTime=%" PRIu64 \
             "µs ; mTimecode=%" PRIu64 "µs",                           \
             (sample)->mDuration.ToMicroseconds(),                     \
             (sample)->mTime.ToMicroseconds(),                         \
             (sample)->mTimecode.ToMicroseconds()))
#endif  // PROCESS_DECODE_LOG

// TimeUnit is a class that represents a time value, that can be negative or
// positive.
//
// Internally, it works by storing a numerator (the tick numbers), that uses
// checked arithmetics, and a denominator (the base), that is a regular integer
// on which arithmetics is never performed, and is only set at construction, or
// replaced.
//
// Dividing the tick count by the base always yields a value in seconds,
// but it's very useful to have a base that is dependant on the context: it can
// be the sample-rate of an audio stream, the time base of an mp4, that's often
// 90000 because it's divisible by 24, 25 and 30.
//
// Keeping time like this allows performing calculations on time values with
// maximum precision, without having to have to care about rounding errors or
// precision loss.
//
// If not specified, the base is 1e6, representing microseconds, for historical
// reasons. Users can gradually move to more precise representations when
// needed.
//
// INT64_MAX has the special meaning of being +∞, and INT64_MIN means -∞. Any
// other value corresponds to a valid time.
//
// If an overflow or other problem occurs, the underlying CheckedInt<int64_t> is
// invalid and a crash is triggered.
class TimeUnit final {
 public:
  constexpr TimeUnit(CheckedInt64 aTicks, int64_t aBase)
      : mTicks(aTicks), mBase(aBase) {
    MOZ_RELEASE_ASSERT(mBase > 0);
  }

  explicit constexpr TimeUnit(CheckedInt64 aTicks)
      : mTicks(aTicks), mBase(USECS_PER_S) {}

  // Return the maximum number of ticks that a TimeUnit can contain.
  static constexpr int64_t MaxTicks() {
    return std::numeric_limits<int64_t>::max() - 1;
  }

  // This is only precise up to a point, which is aValue * aBase <= 2^53 - 1
  static TimeUnit FromSeconds(double aValue, int64_t aBase = USECS_PER_S);
  static constexpr TimeUnit FromMicroseconds(int64_t aValue) {
    return TimeUnit(aValue, USECS_PER_S);
  }
  static TimeUnit FromHns(int64_t aValue, int64_t aBase) {
    // Truncating here would mean a loss of precision.
    return TimeUnit::FromNanoseconds(aValue * 100).ToBase<RoundPolicy>(aBase);
  }
  static constexpr TimeUnit FromNanoseconds(int64_t aValue) {
    return TimeUnit(aValue, NSECS_PER_S);
  }
  static TimeUnit FromInfinity();
  static TimeUnit FromNegativeInfinity();
  static TimeUnit FromTimeDuration(const TimeDuration& aDuration);
  static constexpr TimeUnit Zero(int64_t aBase = USECS_PER_S) {
    return TimeUnit(0, aBase);
  }
  static constexpr TimeUnit Zero(const TimeUnit& aOther) {
    return TimeUnit(0, aOther.mBase);
  }
  static TimeUnit Invalid();
  int64_t ToMilliseconds() const;
  int64_t ToMicroseconds() const;
  int64_t ToNanoseconds() const;
  int64_t ToTicksAtRate(int64_t aRate) const;
  double ToSeconds() const;
  nsCString ToString() const;
  TimeDuration ToTimeDuration() const;
  bool IsInfinite() const;
  bool IsPositive() const;
  bool IsPositiveOrZero() const;
  bool IsZero() const;
  bool IsNegative() const;

  // Returns true if the fractions are equal when converted to the smallest
  // base.
  bool EqualsAtLowestResolution(const TimeUnit& aOther) const;
  // Strict equality -- the fractions must be exactly equal
  bool operator==(const TimeUnit& aOther) const;
  bool operator!=(const TimeUnit& aOther) const;
  bool operator>=(const TimeUnit& aOther) const;
  bool operator>(const TimeUnit& aOther) const;
  bool operator<=(const TimeUnit& aOther) const;
  bool operator<(const TimeUnit& aOther) const;
  TimeUnit operator%(const TimeUnit& aOther) const;
  TimeUnit operator+(const TimeUnit& aOther) const;
  TimeUnit operator-(const TimeUnit& aOther) const;
  TimeUnit& operator+=(const TimeUnit& aOther);
  TimeUnit& operator-=(const TimeUnit& aOther);
  template <typename T>
  TimeUnit operator*(T aVal) const {
    // See bug 853398 for the reason to block double multiplier.
    // If required, use MultDouble below and with caution.
    static_assert(std::is_integral_v<T>, "Must be an integral type");
    return TimeUnit(mTicks * aVal, mBase);
  }
  TimeUnit MultDouble(double aVal) const;
  friend TimeUnit operator/(const TimeUnit& aUnit, int64_t aVal) {
    MOZ_DIAGNOSTIC_ASSERT(0 <= aVal && aVal <= UINT32_MAX);
    return TimeUnit(aUnit.mTicks / aVal, aUnit.mBase);
  }
  friend TimeUnit operator%(const TimeUnit& aUnit, int64_t aVal) {
    MOZ_DIAGNOSTIC_ASSERT(0 <= aVal && aVal <= UINT32_MAX);
    return TimeUnit(aUnit.mTicks % aVal, aUnit.mBase);
  }

  struct TruncatePolicy {
    template <typename T>
    static T policy(T& aValue) {
      return static_cast<T>(aValue);
    }
  };

  struct RoundPolicy {
    template <typename T>
    static T policy(T& aValue) {
      return std::round(aValue);
    }
  };

  struct CeilingPolicy {
    template <typename T>
    static T policy(T& aValue) {
      return std::ceil(aValue);
    }
  };

  template <class RoundingPolicy = TruncatePolicy>
  TimeUnit ToBase(int64_t aTargetBase) const {
    double dummy = 0.0;
    return ToBase<RoundingPolicy>(aTargetBase, dummy);
  }

  template <class RoundingPolicy = TruncatePolicy>
  TimeUnit ToBase(const TimeUnit& aTimeUnit) const {
    double dummy = 0.0;
    return ToBase<RoundingPolicy>(aTimeUnit, dummy);
  }

  // Allow returning the same value, in a base that matches another TimeUnit.
  template <class RoundingPolicy = TruncatePolicy>
  TimeUnit ToBase(const TimeUnit& aTimeUnit, double& aOutError) const {
    int64_t targetBase = aTimeUnit.mBase;
    return ToBase<RoundingPolicy>(targetBase, aOutError);
  }

  template <class RoundingPolicy = TruncatePolicy>
  TimeUnit ToBase(int64_t aTargetBase, double& aOutError) const {
    aOutError = 0.0;
    CheckedInt<int64_t> ticks = mTicks * aTargetBase;
    if (ticks.isValid()) {
      imaxdiv_t rv = imaxdiv(ticks.value(), mBase);
      if (!rv.rem) {
        return TimeUnit(rv.quot, aTargetBase);
      }
    }
    double approx = static_cast<double>(mTicks.value()) *
                    static_cast<double>(aTargetBase) /
                    static_cast<double>(mBase);
    double integer;
    aOutError = modf(approx, &integer);
    return TimeUnit(AssertedCast<int64_t>(RoundingPolicy::policy(approx)),
                    aTargetBase);
  }

  bool IsValid() const;

  constexpr TimeUnit() = default;

  TimeUnit(const TimeUnit&) = default;

  TimeUnit& operator=(const TimeUnit&) = default;

  bool IsPosInf() const;
  bool IsNegInf() const;

  // Allow serializing a TimeUnit via IPC
  friend IPC::ParamTraits<mozilla::media::TimeUnit>;

#ifndef VISIBLE_TIMEUNIT_INTERNALS
 private:
#endif
  int64_t ToCommonUnit(int64_t aRatio) const;
  // Reduce a TimeUnit to the smallest possible ticks and base. This is useful
  // to comparison with big time values that can otherwise overflow.
  TimeUnit Reduced() const;

  CheckedInt64 mTicks{0};
  // Default base is microseconds.
  int64_t mBase{USECS_PER_S};
};

using NullableTimeUnit = Maybe<TimeUnit>;

using TimeInterval = Interval<TimeUnit>;

// A set of intervals, containing TimeUnit.
class TimeIntervals : public IntervalSet<TimeUnit> {
 public:
  using BaseType = IntervalSet<TimeUnit>;
  using InnerType = TimeUnit;

  // We can't use inherited constructors yet. So we have to duplicate all the
  // constructors found in IntervalSet base class.
  // all this could be later replaced with:
  // using IntervalSet<TimeUnit>::IntervalSet;

  // MOZ_IMPLICIT as we want to enable initialization in the form:
  // TimeIntervals i = ... like we would do with IntervalSet<T> i = ...
  MOZ_IMPLICIT TimeIntervals(const BaseType& aOther) : BaseType(aOther) {}
  MOZ_IMPLICIT TimeIntervals(BaseType&& aOther) : BaseType(std::move(aOther)) {}
  explicit TimeIntervals(const BaseType::ElemType& aOther) : BaseType(aOther) {}
  explicit TimeIntervals(BaseType::ElemType&& aOther)
      : BaseType(std::move(aOther)) {}

  static TimeIntervals Invalid() {
    return TimeIntervals(TimeInterval(TimeUnit::FromNegativeInfinity(),
                                      TimeUnit::FromNegativeInfinity()));
  }
  bool IsInvalid() const {
    return Length() == 1 && Start(0).IsNegInf() && End(0).IsNegInf();
  }

  // Returns the same interval, with a given base resolution.
  TimeIntervals ToBase(const TimeUnit& aBase) const {
    TimeIntervals output;
    for (const auto& interval : mIntervals) {
      TimeInterval convertedInterval{interval.mStart.ToBase(aBase),
                                     interval.mEnd.ToBase(aBase),
                                     interval.mFuzz.ToBase(aBase)};
      output += convertedInterval;
    }
    return output;
  }

  // Returns the same interval, with a microsecond resolution. This is used to
  // compare TimeUnits internal to demuxers (that use a base from the container)
  // to floating point numbers in seconds from content.
  TimeIntervals ToMicrosecondResolution() const {
    TimeIntervals output;

    for (const auto& interval : mIntervals) {
      TimeInterval reducedPrecision{interval.mStart.ToBase(USECS_PER_S),
                                    interval.mEnd.ToBase(USECS_PER_S),
                                    interval.mFuzz.ToBase(USECS_PER_S)};
      output += reducedPrecision;
    }
    return output;
  }

  nsCString ToString() const {
    nsCString dump;
    for (const auto& interval : mIntervals) {
      dump += nsPrintfCString("[%s],", interval.ToString().get());
    }
    return dump;
  }

  TimeIntervals() = default;
};

using TimeRange = Interval<double>;

// A set of intervals, containing doubles that are seconds.
class TimeRanges : public IntervalSet<double> {
 public:
  using BaseType = IntervalSet<double>;
  using InnerType = double;
  using nld = std::numeric_limits<double>;

  // We can't use inherited constructors yet. So we have to duplicate all the
  // constructors found in IntervalSet base class.
  // all this could be later replaced with:
  // using IntervalSet<TimeUnit>::IntervalSet;

  // MOZ_IMPLICIT as we want to enable initialization in the form:
  // TimeIntervals i = ... like we would do with IntervalSet<T> i = ...
  MOZ_IMPLICIT TimeRanges(const BaseType& aOther) : BaseType(aOther) {}
  MOZ_IMPLICIT TimeRanges(BaseType&& aOther) : BaseType(std::move(aOther)) {}
  explicit TimeRanges(const BaseType::ElemType& aOther) : BaseType(aOther) {}
  explicit TimeRanges(BaseType::ElemType&& aOther)
      : BaseType(std::move(aOther)) {}

  static TimeRanges Invalid() {
    return TimeRanges(TimeRange(-nld::infinity(), nld::infinity()));
  }
  bool IsInvalid() const {
    return Length() == 1 && Start(0) == -nld::infinity() &&
           End(0) == nld::infinity();
  }
  // Convert from TimeUnit-based intervals to second-based TimeRanges.
  explicit TimeRanges(const TimeIntervals& aIntervals) {
    for (const auto& interval : aIntervals) {
      Add(TimeRange(interval.mStart.ToSeconds(), interval.mEnd.ToSeconds()));
    }
  }

  TimeRanges ToMicrosecondResolution() const;

  TimeRanges() = default;
};

}  // namespace media
}  // namespace mozilla

#endif  // TIME_UNITS_H
