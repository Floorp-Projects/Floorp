/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTimer.h"

#include <math.h>

#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SharedThreadPool.h"

namespace mozilla {

NS_IMPL_ADDREF(MediaTimer)
NS_IMPL_RELEASE_WITH_DESTROY(MediaTimer, DispatchDestroy())

MediaTimer::MediaTimer()
  : mMonitor("MediaTimer Monitor")
  , mTimer(do_CreateInstance("@mozilla.org/timer;1"))
  , mCreationTimeStamp(TimeStamp::Now())
  , mUpdateScheduled(false)
{
  TIMER_LOG("MediaTimer::MediaTimer");

  // Use the SharedThreadPool to create an nsIThreadPool with a maximum of one
  // thread, which is equivalent to an nsIThread for our purposes.
  RefPtr<SharedThreadPool> threadPool(
    SharedThreadPool::Get(NS_LITERAL_CSTRING("MediaTimer"), 1));
  mThread = threadPool.get();
  mTimer->SetTarget(mThread);
}

void
MediaTimer::DispatchDestroy()
{
  nsCOMPtr<nsIRunnable> task = NS_NewNonOwningRunnableMethod(this, &MediaTimer::Destroy);
  // Hold a strong reference to the thread so that it doesn't get deleted in
  // Destroy(), which may run completely before the stack if Dispatch() begins
  // to unwind.
  nsCOMPtr<nsIEventTarget> thread = mThread;
  nsresult rv = thread->Dispatch(task, NS_DISPATCH_NORMAL);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  (void) rv;
}

void
MediaTimer::Destroy()
{
  MOZ_ASSERT(OnMediaTimerThread());
  TIMER_LOG("MediaTimer::Destroy");

  // Reject any outstanding entries. There's no need to acquire the monitor
  // here, because we're on the timer thread and all other references to us
  // must be gone.
  while (!mEntries.empty()) {
    mEntries.top().mPromise->Reject(false, __func__);
    mEntries.pop();
  }

  // Cancel the timer if necessary.
  CancelTimerIfArmed();

  delete this;
}

bool
MediaTimer::OnMediaTimerThread()
{
  bool rv = false;
  mThread->IsOnCurrentThread(&rv);
  return rv;
}

nsRefPtr<MediaTimerPromise>
MediaTimer::WaitUntil(const TimeStamp& aTimeStamp, const char* aCallSite)
{
  MonitorAutoLock mon(mMonitor);
  TIMER_LOG("MediaTimer::WaitUntil %lld", RelativeMicroseconds(aTimeStamp));
  Entry e(aTimeStamp, aCallSite);
  nsRefPtr<MediaTimerPromise> p = e.mPromise.get();
  mEntries.push(e);
  ScheduleUpdate();
  return p;
}

void
MediaTimer::ScheduleUpdate()
{
  mMonitor.AssertCurrentThreadOwns();
  if (mUpdateScheduled) {
    return;
  }
  mUpdateScheduled = true;

  nsCOMPtr<nsIRunnable> task = NS_NewRunnableMethod(this, &MediaTimer::Update);
  nsresult rv = mThread->Dispatch(task, NS_DISPATCH_NORMAL);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  (void) rv;
}

void
MediaTimer::Update()
{
  MonitorAutoLock mon(mMonitor);
  UpdateLocked();
}

void
MediaTimer::UpdateLocked()
{
  MOZ_ASSERT(OnMediaTimerThread());
  mMonitor.AssertCurrentThreadOwns();
  mUpdateScheduled = false;

  TIMER_LOG("MediaTimer::UpdateLocked");

  // Resolve all the promises whose time is up.
  TimeStamp now = TimeStamp::Now();
  while (!mEntries.empty() && mEntries.top().mTimeStamp <= now) {
    mEntries.top().mPromise->Resolve(true, __func__);
    DebugOnly<TimeStamp> poppedTimeStamp = mEntries.top().mTimeStamp;
    mEntries.pop();
    MOZ_ASSERT_IF(!mEntries.empty(), *&poppedTimeStamp <= mEntries.top().mTimeStamp);
  }

  // If we've got no more entries, cancel any pending timer and bail out.
  if (mEntries.empty()) {
    CancelTimerIfArmed();
    return;
  }

  // We've got more entries - (re)arm the timer for the soonest one.
  if (!TimerIsArmed() || mEntries.top().mTimeStamp < mCurrentTimerTarget) {
    CancelTimerIfArmed();
    ArmTimer(mEntries.top().mTimeStamp, now);
  }
}

/*
 * We use a callback function, rather than a callback method, to ensure that
 * the nsITimer does not artifically keep the refcount of the MediaTimer above
 * zero. When the MediaTimer is destroyed, it safely cancels the nsITimer so that
 * we never fire against a dangling closure.
 */

/* static */ void
MediaTimer::TimerCallback(nsITimer* aTimer, void* aClosure)
{
  static_cast<MediaTimer*>(aClosure)->TimerFired();
}

void
MediaTimer::TimerFired()
{
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(OnMediaTimerThread());
  mCurrentTimerTarget = TimeStamp();
  UpdateLocked();
}

void
MediaTimer::ArmTimer(const TimeStamp& aTarget, const TimeStamp& aNow)
{
  MOZ_DIAGNOSTIC_ASSERT(!TimerIsArmed());
  MOZ_DIAGNOSTIC_ASSERT(aTarget > aNow);

  // XPCOM timer resolution is in milliseconds. It's important to never resolve
  // a timer when mTarget might compare < now (even if very close), so round up.
  unsigned long delay = std::ceil((aTarget - aNow).ToMilliseconds());
  TIMER_LOG("MediaTimer::ArmTimer delay=%lu", delay);
  mCurrentTimerTarget = aTarget;
  nsresult rv = mTimer->InitWithNamedFuncCallback(&TimerCallback, this, delay,
                                                  nsITimer::TYPE_ONE_SHOT,
                                                  "MediaTimer::TimerCallback");
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  (void) rv;
}

} // namespace mozilla
