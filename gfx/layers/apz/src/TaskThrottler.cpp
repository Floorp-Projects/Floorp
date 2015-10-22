/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TaskThrottler.h"

#include "mozilla/layers/APZThreadUtils.h"

#define TASK_LOG(...)
// #define TASK_LOG(...) printf_stderr("TASK: " __VA_ARGS__)

namespace mozilla {
namespace layers {

TaskThrottler::TaskThrottler(const TimeStamp& aTimeStamp, const TimeDuration& aMaxWait)
  : mMonitor("TaskThrottler")
  , mOutstanding(false)
  , mQueuedTask(nullptr)
  , mStartTime(aTimeStamp)
  , mMaxWait(aMaxWait)
  , mMean(1)
  , mTimeoutTask(nullptr)
{
}

TaskThrottler::~TaskThrottler()
{
  // The timeout task holds a strong reference to the TaskThrottler, so if the
  // TaskThrottler is being destroyed, there's no need to cancel the task.
}

void
TaskThrottler::PostTask(const tracked_objects::Location& aLocation,
                        UniquePtr<CancelableTask> aTask, const TimeStamp& aTimeStamp)
{
  MonitorAutoLock lock(mMonitor);

  TASK_LOG("%p got a task posted; mOutstanding=%d\n", this, mOutstanding);
  aTask->SetBirthPlace(aLocation);

  if (mOutstanding) {
    CancelPendingTask(lock);
    if (TimeSinceLastRequest(aTimeStamp, lock) < mMaxWait) {
      mQueuedTask = Move(aTask);
      TASK_LOG("%p queued task %p\n", this, mQueuedTask.get());
      // Make sure the queued task is sent after mMaxWait time elapses,
      // even if we don't get a TaskComplete() until then.
      TimeDuration timeout = mMaxWait - TimeSinceLastRequest(aTimeStamp, lock);
      mTimeoutTask = NewRunnableMethod(this, &TaskThrottler::OnTimeout);
      APZThreadUtils::RunDelayedTaskOnCurrentThread(mTimeoutTask, timeout);
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
TaskThrottler::OnTimeout()
{
  MonitorAutoLock lock(mMonitor);
  if (mQueuedTask) {
    RunQueuedTask(TimeStamp::Now(), lock);
  }
  // The message loop will delete the posted timeout task. Make sure we don't
  // keep a dangling pointer to it.
  mTimeoutTask = nullptr;
}

void
TaskThrottler::TaskComplete(const TimeStamp& aTimeStamp)
{
  MonitorAutoLock lock(mMonitor);

  if (!mOutstanding) {
    return;
  }

  mMean.insert(aTimeStamp - mStartTime);

  if (mQueuedTask) {
    RunQueuedTask(aTimeStamp, lock);
    CancelTimeoutTask(lock);
  } else {
    mOutstanding = false;
  }
}

TimeDuration
TaskThrottler::AverageDuration()
{
  MonitorAutoLock lock(mMonitor);

  return mMean.empty() ? TimeDuration() : mMean.mean();
}

void
TaskThrottler::RunQueuedTask(const TimeStamp& aTimeStamp,
                             const MonitorAutoLock& aProofOfLock)
{
  TASK_LOG("%p running task %p\n", this, mQueuedTask.get());
  mStartTime = aTimeStamp;
  mQueuedTask->Run();
  mQueuedTask = nullptr;
}

void
TaskThrottler::CancelPendingTask()
{
  MonitorAutoLock lock(mMonitor);
  CancelPendingTask(lock);
}

void
TaskThrottler::CancelPendingTask(const MonitorAutoLock& aProofOfLock)
{
  if (mQueuedTask) {
    TASK_LOG("%p cancelling task %p\n", this, mQueuedTask.get());
    mQueuedTask->Cancel();
    mQueuedTask = nullptr;
    CancelTimeoutTask(aProofOfLock);
  }
}

void
TaskThrottler::CancelTimeoutTask(const MonitorAutoLock& aProofOfLock)
{
  if (mTimeoutTask) {
    mTimeoutTask->Cancel();
    mTimeoutTask = nullptr;  // the MessageLoop will destroy it
  }
}

TimeDuration
TaskThrottler::TimeSinceLastRequest(const TimeStamp& aTimeStamp)
{
  MonitorAutoLock lock(mMonitor);
  return TimeSinceLastRequest(aTimeStamp, lock);
}

TimeDuration
TaskThrottler::TimeSinceLastRequest(const TimeStamp& aTimeStamp,
                                    const MonitorAutoLock& aProofOfLock)
{
  return aTimeStamp - mStartTime;
}

void
TaskThrottler::ClearHistory()
{
  MonitorAutoLock lock(mMonitor);

  mMean.clear();
}

void
TaskThrottler::SetMaxDurations(uint32_t aMaxDurations)
{
  MonitorAutoLock lock(mMonitor);

  if (aMaxDurations != mMean.maxValues()) {
    mMean = RollingMean<TimeDuration, TimeDuration>(aMaxDurations);
  }
}

} // namespace layers
} // namespace mozilla
