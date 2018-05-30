/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TIME_UNITS_H
#define TIME_UNITS_H

#include "Intervals.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace media {
class TimeIntervals;
} // namespace media
} // namespace mozilla
// CopyChooser specialization for nsTArray
template<>
struct nsTArray_CopyChooser<mozilla::media::TimeIntervals>
{
  typedef nsTArray_CopyWithConstructors<mozilla::media::TimeIntervals> Type;
};

namespace mozilla {

// Number of microseconds per second. 1e6.
static const int64_t USECS_PER_S = 1000000;

// Number of microseconds per millisecond.
static const int64_t USECS_PER_MS = 1000;

namespace media {

// Number of nanoseconds per second. 1e9.
static const int64_t NSECS_PER_S = 1000000000;

// TimeUnit at present uses a CheckedInt64 as storage.
// INT64_MAX has the special meaning of being +oo.
class TimeUnit final
{
public:
  static TimeUnit FromSeconds(double aValue)
  {
    MOZ_ASSERT(!IsNaN(aValue));

    if (mozilla::IsInfinite<double>(aValue)) {
      return FromInfinity();
    }
    // Due to internal double representation, this
    // operation is not commutative, do not attempt to simplify.
    double halfUsec = .0000005;
    double val =
      (aValue <= 0 ? aValue - halfUsec : aValue + halfUsec) * USECS_PER_S;
    if (val >= double(INT64_MAX)) {
      return FromMicroseconds(INT64_MAX);
    } else if (val <= double(INT64_MIN)) {
      return FromMicroseconds(INT64_MIN);
    } else {
      return FromMicroseconds(int64_t(val));
    }
  }

  static constexpr TimeUnit FromMicroseconds(int64_t aValue)
  {
    return TimeUnit(aValue);
  }

  static constexpr TimeUnit FromNanoseconds(int64_t aValue)
  {
    return TimeUnit(aValue / 1000);
  }

  static constexpr TimeUnit FromInfinity() { return TimeUnit(INT64_MAX); }

  static TimeUnit FromTimeDuration(const TimeDuration& aDuration)
  {
    return FromSeconds(aDuration.ToSeconds());
  }

  static constexpr TimeUnit Zero() { return TimeUnit(0); }

  static TimeUnit Invalid()
  {
    TimeUnit ret;
    ret.mValue = CheckedInt64(INT64_MAX);
    // Force an overflow to render the CheckedInt invalid.
    ret.mValue += 1;
    return ret;
  }

  int64_t ToMicroseconds() const { return mValue.value(); }

  int64_t ToNanoseconds() const { return mValue.value() * 1000; }

  double ToSeconds() const
  {
    if (IsInfinite()) {
      return PositiveInfinity<double>();
    }
    return double(mValue.value()) / USECS_PER_S;
  }

  TimeDuration ToTimeDuration() const
  {
    return TimeDuration::FromMicroseconds(mValue.value());
  }

  bool IsInfinite() const { return mValue.value() == INT64_MAX; }

  bool IsPositive() const { return mValue.value() > 0; }

  bool IsNegative() const { return mValue.value() < 0; }

  bool operator==(const TimeUnit& aOther) const
  {
    MOZ_ASSERT(IsValid() && aOther.IsValid());
    return mValue.value() == aOther.mValue.value();
  }
  bool operator!=(const TimeUnit& aOther) const
  {
    MOZ_ASSERT(IsValid() && aOther.IsValid());
    return mValue.value() != aOther.mValue.value();
  }
  bool operator>=(const TimeUnit& aOther) const
  {
    MOZ_ASSERT(IsValid() && aOther.IsValid());
    return mValue.value() >= aOther.mValue.value();
  }
  bool operator>(const TimeUnit& aOther) const { return !(*this <= aOther); }
  bool operator<=(const TimeUnit& aOther) const
  {
    MOZ_ASSERT(IsValid() && aOther.IsValid());
    return mValue.value() <= aOther.mValue.value();
  }
  bool operator<(const TimeUnit& aOther) const { return !(*this >= aOther); }
  TimeUnit operator+(const TimeUnit& aOther) const
  {
    if (IsInfinite() || aOther.IsInfinite()) {
      return FromInfinity();
    }
    return TimeUnit(mValue + aOther.mValue);
  }
  TimeUnit operator-(const TimeUnit& aOther) const
  {
    if (IsInfinite() && !aOther.IsInfinite()) {
      return FromInfinity();
    }
    MOZ_ASSERT(!IsInfinite() && !aOther.IsInfinite());
    return TimeUnit(mValue - aOther.mValue);
  }
  TimeUnit& operator+=(const TimeUnit& aOther)
  {
    *this = *this + aOther;
    return *this;
  }
  TimeUnit& operator-=(const TimeUnit& aOther)
  {
    *this = *this - aOther;
    return *this;
  }

  template<typename T>
  TimeUnit operator*(T aVal) const
  {
    // See bug 853398 for the reason to block double multiplier.
    // If required, use MultDouble below and with caution.
    static_assert(mozilla::IsIntegral<T>::value, "Must be an integral type");
    return TimeUnit(mValue * aVal);
  }
  TimeUnit MultDouble(double aVal) const
  {
    return TimeUnit::FromSeconds(ToSeconds() * aVal);
  }
  friend TimeUnit operator/(const TimeUnit& aUnit, int aVal)
  {
    return TimeUnit(aUnit.mValue / aVal);
  }
  friend TimeUnit operator%(const TimeUnit& aUnit, int aVal)
  {
    return TimeUnit(aUnit.mValue % aVal);
  }

  bool IsValid() const { return mValue.isValid(); }

  constexpr TimeUnit()
    : mValue(CheckedInt64(0))
  {
  }

  TimeUnit(const TimeUnit&) = default;

  TimeUnit& operator=(const TimeUnit&) = default;

private:
  explicit constexpr TimeUnit(CheckedInt64 aMicroseconds)
    : mValue(aMicroseconds)
  {
  }

  // Our internal representation is in microseconds.
  CheckedInt64 mValue;
};

typedef Maybe<TimeUnit> NullableTimeUnit;

typedef Interval<TimeUnit> TimeInterval;

class TimeIntervals : public IntervalSet<TimeUnit>
{
public:
  typedef IntervalSet<TimeUnit> BaseType;

  // We can't use inherited constructors yet. So we have to duplicate all the
  // constructors found in IntervalSet base class.
  // all this could be later replaced with:
  // using IntervalSet<TimeUnit>::IntervalSet;

  // MOZ_IMPLICIT as we want to enable initialization in the form:
  // TimeIntervals i = ... like we would do with IntervalSet<T> i = ...
  MOZ_IMPLICIT TimeIntervals(const BaseType& aOther)
    : BaseType(aOther)
  {
  }
  MOZ_IMPLICIT TimeIntervals(BaseType&& aOther)
    : BaseType(std::move(aOther))
  {
  }
  explicit TimeIntervals(const BaseType::ElemType& aOther)
    : BaseType(aOther)
  {
  }
  explicit TimeIntervals(BaseType::ElemType&& aOther)
    : BaseType(std::move(aOther))
  {
  }

  static TimeIntervals Invalid()
  {
    return TimeIntervals(TimeInterval(TimeUnit::FromMicroseconds(INT64_MIN),
                                      TimeUnit::FromMicroseconds(INT64_MIN)));
  }
  bool IsInvalid() const
  {
    return Length() == 1 && Start(0).ToMicroseconds() == INT64_MIN &&
           End(0).ToMicroseconds() == INT64_MIN;
  }

  TimeIntervals() = default;
};

} // namespace media
} // namespace mozilla

#endif // TIME_UNITS_H
