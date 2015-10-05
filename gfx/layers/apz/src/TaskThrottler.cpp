/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TaskThrottler.h"

#include "mozilla/layers/APZThreadUtils.h"  // for NewTimerCallback
#include "nsComponentManagerUtils.h"        // for do_CreateInstance
#include "nsITimer.h"

#define TASK_LOG(...)
// #define TASK_LOG(...) printf_stderr("TASK: " __VA_ARGS__)

namespace mozilla {
namespace layers {

TaskThrottler::TaskThrottler(const TimeStamp& aTimeStamp, const TimeDuration& aMaxWait)
  : mOutstanding(false)
  , mQueuedTask(nullptr)
  , mStartTime(aTimeStamp)
  , mMaxWait(aMaxWait)
  , mMean(1)
  , mTimer(do_CreateInstance(NS_TIMER_CONTRACTID))
{
  // The TaskThrottler must be created on the main thread (or some nsITimer-
  // compatible thread) for the nsITimer to work properly. In particular,
  // creating it on the Compositor thread doesn't work.
  MOZ_ASSERT(NS_IsMainThread());
}

TaskThrottler::~TaskThrottler()
{
  mTimer->Cancel();
}

void
TaskThrottler::PostTask(const tracked_objects::Location& aLocation,
                        UniquePtr<CancelableTask> aTask, const TimeStamp& aTimeStamp)
{
  TASK_LOG("%p got a task posted; mOutstanding=%d\n", this, mOutstanding);
  aTask->SetBirthPlace(aLocation);

  if (mOutstanding) {
    CancelPendingTask();
    if (TimeSinceLastRequest(aTimeStamp) < mMaxWait) {
      mQueuedTask = Move(aTask);
      TASK_LOG("%p queued task %p\n", this, mQueuedTask.get());
      // Make sure the queued task is sent after mMaxWait time elapses,
      // even if we don't get a TaskComplete() until then.
      TimeDuration timeout = mMaxWait - TimeSinceLastRequest(aTimeStamp);
      TimeStamp timeoutTime = mStartTime + mMaxWait;
      nsRefPtr<TaskThrottler> refPtrThis = this;
      mTimer->InitWithCallback(NewTimerCallback(
          [refPtrThis, timeoutTime]()
          {
            if (refPtrThis->mQueuedTask) {
              refPtrThis->RunQueuedTask(timeoutTime);
            }
          }),
          timeout.ToMilliseconds(), nsITimer::TYPE_ONE_SHOT);
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
    RunQueuedTask(aTimeStamp);
    mTimer->Cancel();
  } else {
    mOutstanding = false;
  }
}

TimeDuration
TaskThrottler::AverageDuration()
{
  return mMean.empty() ? TimeDuration() : mMean.mean();
}

void
TaskThrottler::RunQueuedTask(const TimeStamp& aTimeStamp)
{
  TASK_LOG("%p running task %p\n", this, mQueuedTask.get());
  mStartTime = aTimeStamp;
  mQueuedTask->Run();
  mQueuedTask = nullptr;

}

void
TaskThrottler::CancelPendingTask()
{
  if (mQueuedTask) {
    TASK_LOG("%p cancelling task %p\n", this, mQueuedTask.get());
    mQueuedTask->Cancel();
    mQueuedTask = nullptr;
    mTimer->Cancel();
  }
}

TimeDuration
TaskThrottler::TimeSinceLastRequest(const TimeStamp& aTimeStamp)
{
  return aTimeStamp - mStartTime;
}

void
TaskThrottler::ClearHistory()
{
  mMean.clear();
}

void
TaskThrottler::SetMaxDurations(uint32_t aMaxDurations)
{
  if (aMaxDurations != mMean.maxValues()) {
    mMean = RollingMean<TimeDuration, TimeDuration>(aMaxDurations);
  }
}

} // namespace layers
} // namespace mozilla
