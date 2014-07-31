/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TaskThrottler.h"

namespace mozilla {
namespace layers {

TaskThrottler::TaskThrottler(const TimeStamp& aTimeStamp, const TimeDuration& aMaxWait)
  : mOutstanding(false)
  , mQueuedTask(nullptr)
  , mStartTime(aTimeStamp)
  , mMaxWait(aMaxWait)
  , mMean(1)
{ }

void
TaskThrottler::PostTask(const tracked_objects::Location& aLocation,
                        UniquePtr<CancelableTask> aTask, const TimeStamp& aTimeStamp)
{
  aTask->SetBirthPlace(aLocation);

  if (mOutstanding) {
    if (mQueuedTask) {
      mQueuedTask->Cancel();
      mQueuedTask = nullptr;
    }
    if (TimeSinceLastRequest(aTimeStamp) < mMaxWait) {
      mQueuedTask = Move(aTask);
      return;
    }
    // we've been waiting for more than the max-wait limit, so just fall through
    // and send the new task already.
  }

  mStartTime = aTimeStamp;
  aTask->Run();
  mOutstanding = true;
}

void
TaskThrottler::TaskComplete(const TimeStamp& aTimeStamp)
{
  if (!mOutstanding) {
    return;
  }

  mMean.insert(aTimeStamp - mStartTime);

  if (mQueuedTask) {
    mStartTime = aTimeStamp;
    mQueuedTask->Run();
    mQueuedTask = nullptr;
  } else {
    mOutstanding = false;
  }
}

TimeDuration
TaskThrottler::TimeSinceLastRequest(const TimeStamp& aTimeStamp)
{
  return aTimeStamp - mStartTime;
}

}
}
