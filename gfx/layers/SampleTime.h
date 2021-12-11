/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_SampleTime_h
#define mozilla_layers_SampleTime_h

#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace layers {

/**
 * When compositing frames, there is usually a "sample time" associated
 * with the frame, which may be derived from vsync or controlled by a test.
 * This class encapsulates that and tracks where the sample time comes from,
 * as the desired behaviour may vary based on the time source.
 */
class SampleTime {
 public:
  enum TimeType {
    // Uninitialized sample time.
    eNull,
    // Time comes from a vsync tick, possibly adjusted by a vsync interval.
    eVsync,
    // Time comes from a "now" timestamp
    eNow,
    // Time is set by a test
    eTest,
  };

  SampleTime();
  static SampleTime FromVsync(const TimeStamp& aTime);
  static SampleTime FromNow();
  static SampleTime FromTest(const TimeStamp& aTime);

  bool IsNull() const;
  TimeType Type() const;
  const TimeStamp& Time() const;

  // These operators just compare the timestamps
  bool operator==(const SampleTime& aOther) const;
  bool operator!=(const SampleTime& aOther) const;
  bool operator<(const SampleTime& aOther) const;
  bool operator<=(const SampleTime& aOther) const;
  bool operator>(const SampleTime& aOther) const;
  bool operator>=(const SampleTime& aOther) const;
  // These return a new SampleTime with the same type as this one, but the
  // time adjusted by the provided TimeDuration
  SampleTime operator+(const TimeDuration& aDuration) const;
  SampleTime operator-(const TimeDuration& aDuration) const;
  // This just produces the time difference between the two SampleTimes,
  // ignoring the type.
  TimeDuration operator-(const SampleTime& aOther) const;

 private:
  // Private constructor; use one of the static FromXXX methods instead.
  SampleTime(TimeType aType, const TimeStamp& aTime);

  TimeType mType;
  TimeStamp mTime;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_SampleTime_h
