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
#include "mozilla/dom/TimeRanges.h"

namespace mozilla {
namespace media {

// Number of microseconds per second. 1e6.
static const int64_t USECS_PER_S = 1000000;

// Number of microseconds per millisecond.
static const int64_t USECS_PER_MS = 1000;

// Number of nanoseconds per second. 1e9.
static const int64_t NSECS_PER_S = 1000000000;

struct Microseconds {
  Microseconds()
    : mValue(0)
  {}

  explicit Microseconds(int64_t aValue)
    : mValue(aValue)
  {}

  double ToSeconds() {
    return double(mValue) / USECS_PER_S;
  }

  static Microseconds FromSeconds(double aValue) {
    MOZ_ASSERT(!IsNaN(aValue));

    double val = aValue * USECS_PER_S;
    if (val >= double(INT64_MAX)) {
      return Microseconds(INT64_MAX);
    } else if (val <= double(INT64_MIN)) {
      return Microseconds(INT64_MIN);
    } else {
      return Microseconds(int64_t(val));
    }
  }

  bool operator == (const Microseconds& aOther) const {
    return mValue == aOther.mValue;
  }
  bool operator > (const Microseconds& aOther) const {
    return mValue > aOther.mValue;
  }
  bool operator >= (const Microseconds& aOther) const {
    return mValue >= aOther.mValue;
  }
  bool operator < (const Microseconds& aOther) const {
    return mValue < aOther.mValue;
  }
  bool operator <= (const Microseconds& aOther) const {
    return mValue <= aOther.mValue;
  }

  int64_t mValue;
};

// TimeUnit at present uses a CheckedInt64 as storage.
// INT64_MAX has the special meaning of being +oo.
class TimeUnit final {
public:
  static TimeUnit FromSeconds(double aValue) {
    MOZ_ASSERT(!IsNaN(aValue));

    if (mozilla::IsInfinite<double>(aValue)) {
      return FromInfinity();
    }
    double val = aValue * USECS_PER_S;
    if (val >= double(INT64_MAX)) {
      return FromMicroseconds(INT64_MAX);
    } else if (val <= double(INT64_MIN)) {
      return FromMicroseconds(INT64_MIN);
    } else {
      return FromMicroseconds(int64_t(val));
    }
  }

  static TimeUnit FromMicroseconds(int64_t aValue) {
    return TimeUnit(aValue);
  }

  static TimeUnit FromMicroseconds(Microseconds aValue) {
    return TimeUnit(aValue.mValue);
  }

  static TimeUnit FromNanoseconds(int64_t aValue) {
    return TimeUnit(aValue / 1000);
  }

  static TimeUnit FromInfinity() {
    return TimeUnit(INT64_MAX);
  }

  int64_t ToMicroseconds() const {
    return mValue.value();
  }

  int64_t ToNanoseconds() const {
    return mValue.value() * 1000;
  }

  double ToSeconds() const {
    if (IsInfinite()) {
      return PositiveInfinity<double>();
    }
    return double(mValue.value()) / USECS_PER_S;
  }

  bool IsInfinite() const {
    return mValue.value() == INT64_MAX;
  }

  bool operator == (const TimeUnit& aOther) const {
    MOZ_ASSERT(IsValid() && aOther.IsValid());
    return mValue.value() == aOther.mValue.value();
  }
  bool operator >= (const TimeUnit& aOther) const {
    MOZ_ASSERT(IsValid() && aOther.IsValid());
    return mValue.value() >= aOther.mValue.value();
  }
  bool operator > (const TimeUnit& aOther) const {
    return !(*this <= aOther);
  }
  bool operator <= (const TimeUnit& aOther) const {
    MOZ_ASSERT(IsValid() && aOther.IsValid());
    return mValue.value() <= aOther.mValue.value();
  }
  bool operator < (const TimeUnit& aOther) const {
    return !(*this >= aOther);
  }
  TimeUnit operator + (const TimeUnit& aOther) const {
    if (IsInfinite() || aOther.IsInfinite()) {
      return FromInfinity();
    }
    return TimeUnit(mValue + aOther.mValue);
  }
  TimeUnit operator - (const TimeUnit& aOther) const {
    if (IsInfinite() && !aOther.IsInfinite()) {
      return FromInfinity();
    }
    MOZ_ASSERT(!IsInfinite() && !aOther.IsInfinite());
    return TimeUnit(mValue - aOther.mValue);
  }

  bool IsValid() const
  {
    return mValue.isValid();
  }

  TimeUnit()
    : mValue(CheckedInt64(0))
  {}

  explicit TimeUnit(const Microseconds& aMicroseconds)
    : mValue(aMicroseconds.mValue)
  {}
  TimeUnit& operator = (const Microseconds& aMicroseconds)
  {
    mValue = aMicroseconds.mValue;
    return *this;
  }

  TimeUnit(const TimeUnit&) = default;

  TimeUnit& operator = (const TimeUnit&) = default;

private:
  explicit TimeUnit(CheckedInt64 aMicroseconds)
    : mValue(aMicroseconds)
  {}

  // Our internal representation is in microseconds.
  CheckedInt64 mValue;
};

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
  {}
  MOZ_IMPLICIT TimeIntervals(BaseType&& aOther)
    : BaseType(Move(aOther))
  {}
  explicit TimeIntervals(const BaseType::ElemType& aOther)
    : BaseType(aOther)
  {}
  explicit TimeIntervals(BaseType::ElemType&& aOther)
    : BaseType(Move(aOther))
  {}

  static TimeIntervals Invalid()
  {
    return TimeIntervals(TimeInterval(TimeUnit::FromMicroseconds(INT64_MIN),
                                      TimeUnit::FromMicroseconds(INT64_MIN)));
  }
  bool IsInvalid()
  {
    return Length() == 1 && Start(0).ToMicroseconds() == INT64_MIN &&
      End(0).ToMicroseconds() == INT64_MIN;
  }

  TimeIntervals() = default;

  // Make TimeIntervals interchangeable with dom::TimeRanges.
  explicit TimeIntervals(dom::TimeRanges* aRanges)
  {
    for (uint32_t i = 0; i < aRanges->Length(); i++) {
      ErrorResult rv;
      *this +=
        TimeInterval(TimeUnit::FromSeconds(aRanges->Start(i, rv)),
                     TimeUnit::FromSeconds(aRanges->End(i, rv)));
    }
  }
  TimeIntervals& operator = (dom::TimeRanges* aRanges)
  {
    *this = TimeIntervals(aRanges);
    return *this;
  }

  static TimeIntervals FromTimeRanges(dom::TimeRanges* aRanges)
  {
    return TimeIntervals(aRanges);
  }

  void ToTimeRanges(dom::TimeRanges* aRanges) const
  {
    for (IndexType i = 0; i < Length(); i++) {
      aRanges->Add(Start(i).ToSeconds(), End(i).ToSeconds());
    }
  }
};

} // namespace media
} // namespace mozilla

#endif // TIME_UNITS_H
