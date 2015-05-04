/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TIME_UNITS_H
#define TIME_UNITS_H

#include "VideoUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"

namespace mozilla {
namespace media {

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

class TimeUnit final {
public:
  static TimeUnit FromSeconds(double aValue) {
    MOZ_ASSERT(!IsNaN(aValue));

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

  int64_t ToMicroseconds() const {
    return mValue.value();
  }

  double ToSeconds() const {
    return double(mValue.value()) / USECS_PER_S;
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
    return TimeUnit(mValue + aOther.mValue);
  }
  TimeUnit operator - (const TimeUnit& aOther) const {
    return TimeUnit(mValue - aOther.mValue);
  }

  bool IsValid() const
  {
    return mValue.isValid();
  }

  explicit TimeUnit(const Microseconds& aMicroseconds)
    : mValue(aMicroseconds.mValue)
  {}

  TimeUnit(const TimeUnit&) = default;

  TimeUnit& operator = (const TimeUnit&) = default;

private:
  explicit TimeUnit(CheckedInt64 aMicroseconds)
    : mValue(aMicroseconds)
  {}

  // Our internal representation is in microseconds.
  CheckedInt64 mValue;
};

} // namespace media
} // namespace mozilla

#endif // TIME_UNITS_H
