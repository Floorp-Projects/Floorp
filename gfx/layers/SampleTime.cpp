/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SampleTime.h"

namespace mozilla {
namespace layers {

SampleTime::SampleTime() : mType(eNull) {}

/*static*/
SampleTime SampleTime::FromVsync(const TimeStamp& aTime) {
  MOZ_ASSERT(!aTime.IsNull());
  return SampleTime(eVsync, aTime);
}

/*static*/
SampleTime SampleTime::FromNow() { return SampleTime(eNow, TimeStamp::Now()); }

/*static*/
SampleTime SampleTime::FromTest(const TimeStamp& aTime) {
  MOZ_ASSERT(!aTime.IsNull());
  return SampleTime(eTest, aTime);
}

bool SampleTime::IsNull() const { return mType == eNull; }

SampleTime::TimeType SampleTime::Type() const { return mType; }

const TimeStamp& SampleTime::Time() const { return mTime; }

bool SampleTime::operator==(const SampleTime& aOther) const {
  return mTime == aOther.mTime;
}

bool SampleTime::operator!=(const SampleTime& aOther) const {
  return !(*this == aOther);
}

bool SampleTime::operator<(const SampleTime& aOther) const {
  return mTime < aOther.mTime;
}

bool SampleTime::operator<=(const SampleTime& aOther) const {
  return mTime <= aOther.mTime;
}

bool SampleTime::operator>(const SampleTime& aOther) const {
  return mTime > aOther.mTime;
}

bool SampleTime::operator>=(const SampleTime& aOther) const {
  return mTime >= aOther.mTime;
}

SampleTime SampleTime::operator+(const TimeDuration& aDuration) const {
  return SampleTime(mType, mTime + aDuration);
}

SampleTime SampleTime::operator-(const TimeDuration& aDuration) const {
  return SampleTime(mType, mTime - aDuration);
}

TimeDuration SampleTime::operator-(const SampleTime& aOther) const {
  return mTime - aOther.mTime;
}

SampleTime::SampleTime(TimeType aType, const TimeStamp& aTime)
    : mType(aType), mTime(aTime) {}

}  // namespace layers
}  // namespace mozilla
