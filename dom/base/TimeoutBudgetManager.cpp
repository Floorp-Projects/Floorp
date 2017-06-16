/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimeoutBudgetManager.h"

namespace mozilla {
namespace dom {

// Time between sampling timeout execution time.
const uint32_t kTelemetryPeriodMS = 1000;

static TimeoutBudgetManager gTimeoutBudgetManager;

/* static */ TimeoutBudgetManager&
TimeoutBudgetManager::Get()
{
  return gTimeoutBudgetManager;
}

void
TimeoutBudgetManager::StartRecording(const TimeStamp& aNow)
{
  mStart = aNow;
}

void
TimeoutBudgetManager::StopRecording()
{
  mStart = TimeStamp();
}

TimeDuration
TimeoutBudgetManager::RecordExecution(const TimeStamp& aNow,
                                      bool aIsTracking,
                                      bool aIsBackground)
{
  if (!mStart) {
    // If we've started a sync operation mStart might be null, in
    // which case we should not record this piece of execution.
    return TimeDuration();
  }

  TimeDuration duration = aNow - mStart;

  if (aIsBackground) {
    if (aIsTracking) {
      mTelemetryData.mBackgroundTracking += duration;
    } else {
      mTelemetryData.mBackgroundNonTracking += duration;
    }
  } else {
    if (aIsTracking) {
      mTelemetryData.mForegroundTracking += duration;
    } else {
      mTelemetryData.mForegroundNonTracking += duration;
    }
  }

  return duration;
}

void
TimeoutBudgetManager::Accumulate(Telemetry::HistogramID aId,
                                 const TimeDuration& aSample)
{
  uint32_t sample = std::round(aSample.ToMilliseconds());
  if (sample) {
    Telemetry::Accumulate(aId, sample);
  }
}

void
TimeoutBudgetManager::MaybeCollectTelemetry(const TimeStamp& aNow)
{
  if ((aNow - mLastCollection).ToMilliseconds() < kTelemetryPeriodMS) {
    return;
  }

  Accumulate(Telemetry::TIMEOUT_EXECUTION_FG_TRACKING_MS,
             mTelemetryData.mForegroundTracking);
  Accumulate(Telemetry::TIMEOUT_EXECUTION_FG_MS,
             mTelemetryData.mForegroundNonTracking);
  Accumulate(Telemetry::TIMEOUT_EXECUTION_BG_TRACKING_MS,
             mTelemetryData.mBackgroundTracking);
  Accumulate(Telemetry::TIMEOUT_EXECUTION_BG_MS,
             mTelemetryData.mBackgroundNonTracking);

  mTelemetryData = TelemetryData();
  mLastCollection = aNow;
}

} // namespace dom
} // namespace mozilla
