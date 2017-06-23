/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_timeoutbudgetmanager_h
#define mozilla_dom_timeoutbudgetmanager_h

#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace dom {

class Timeout;

class TimeoutBudgetManager
{
public:
  static TimeoutBudgetManager& Get();
  void StartRecording(const TimeStamp& aNow);
  void StopRecording();
  void RecordExecution(const TimeStamp& aNow,
                       const Timeout* aTimeout,
                       bool aIsBackground);
  void MaybeCollectTelemetry(const TimeStamp& aNow);
private:
  TimeoutBudgetManager() : mLastCollection(TimeStamp::Now()) {}
  struct TelemetryData
  {
    TimeDuration mForegroundTracking;
    TimeDuration mForegroundNonTracking;
    TimeDuration mBackgroundTracking;
    TimeDuration mBackgroundNonTracking;
  };

  void Accumulate(Telemetry::HistogramID aId, const TimeDuration& aSample);

  TelemetryData mTelemetryData;
  TimeStamp mStart;
  TimeStamp mLastCollection;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_timeoutbudgetmanager_h
