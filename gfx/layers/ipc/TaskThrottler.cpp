/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "TaskThrottler.h"

namespace mozilla {
namespace layers {

TaskThrottler::TaskThrottler(const TimeStamp& aTimeStamp)
  : mOutstanding(false)
  , mQueuedTask(nullptr)
  , mStartTime(aTimeStamp)
{ }

void
TaskThrottler::PostTask(const tracked_objects::Location& aLocation,
                        CancelableTask* aTask, const TimeStamp& aTimeStamp)
{
  aTask->SetBirthPlace(aLocation);

  if (mOutstanding) {
    if (mQueuedTask) {
      mQueuedTask->Cancel();
    }
    mQueuedTask = aTask;
  } else {
    mStartTime = aTimeStamp;
    aTask->Run();
    delete aTask;
    mOutstanding = true;
  }
}

void
TaskThrottler::TaskComplete(const TimeStamp& aTimeStamp)
{
  if (!mOutstanding) {
    return;
  }

  // Remove the oldest sample we have if adding a new sample takes us over our
  // desired number of samples.
  if (mMaxDurations > 0) {
      if (mDurations.Length() >= mMaxDurations) {
          mDurations.RemoveElementAt(0);
      }
      mDurations.AppendElement(aTimeStamp - mStartTime);
  }

  if (mQueuedTask) {
    mStartTime = aTimeStamp;
    mQueuedTask->Run();
    mQueuedTask = nullptr;
  } else {
    mOutstanding = false;
  }
}

TimeDuration
TaskThrottler::AverageDuration()
{
  if (!mDurations.Length()) {
    return TimeDuration();
  }

  TimeDuration durationSum;
  for (uint32_t i = 0; i < mDurations.Length(); i++) {
    durationSum += mDurations[i];
  }

  return durationSum / mDurations.Length();
}

TimeDuration
TaskThrottler::TimeSinceLastRequest(const TimeStamp& aTimeStamp)
{
  return aTimeStamp - mStartTime;
}

}
}
