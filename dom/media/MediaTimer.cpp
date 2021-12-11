/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTimer.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/Unused.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"
#include <math.h>

namespace mozilla {

MediaTimer::MediaTimer(bool aFuzzy)
    : mMonitor("MediaTimer Monitor"),
      mCreationTimeStamp(TimeStamp::Now()),
      mUpdateScheduled(false),
      mFuzzy(aFuzzy) {
  TIMER_LOG("MediaTimer::MediaTimer");

  // Use the SharedThreadPool to create an nsIThreadPool with a maximum of one
  // thread, which is equivalent to an nsIThread for our purposes.
  RefPtr<SharedThreadPool> threadPool(
      SharedThreadPool::Get("MediaTimer"_ns, 1));
  mThread = threadPool.get();
  mTimer = NS_NewTimer(mThread);
}

void MediaTimer::DispatchDestroy() {
  // Hold a strong reference to the thread so that it doesn't get deleted in
  // Destroy(), which may run completely before the stack if Dispatch() begins
  // to unwind.
  nsCOMPtr<nsIEventTarget> thread = mThread;
  nsresult rv =
      thread->Dispatch(NewNonOwningRunnableMethod("MediaTimer::Destroy", this,
                                                  &MediaTimer::Destroy),
                       NS_DISPATCH_NORMAL);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
  (void)rv;
}

void MediaTimer::Destroy() {
  MOZ_ASSERT(OnMediaTimerThread());
  TIMER_LOG("MediaTimer::Destroy");

  // Reject any outstanding entries.
  {
    MonitorAutoLock lock(mMonitor);
    Reject();
  }

  // Cancel the timer if necessary.
  CancelTimerIfArmed();

  delete this;
}

bool MediaTimer::OnMediaTimerThread() {
  bool rv = false;
  mThread->IsOnCurrentThread(&rv);
  return rv;
}

RefPtr<MediaTimerPromise> MediaTimer::WaitFor(const TimeDuration& aDuration,
                                              const char* aCallSite) {
  return WaitUntil(TimeStamp::Now() + aDuration, aCallSite);
}

RefPtr<MediaTimerPromise> MediaTimer::WaitUntil(const TimeStamp& aTimeStamp,
                                                const char* aCallSite) {
  MonitorAutoLock mon(mMonitor);
  TIMER_LOG("MediaTimer::WaitUntil %" PRId64, RelativeMicroseconds(aTimeStamp));
  Entry e(aTimeStamp, aCallSite);
  RefPtr<MediaTimerPromise> p = e.mPromise.get();
  mEntries.push(e);
  ScheduleUpdate();
  return p;
}

void MediaTimer::Cancel() {
  MonitorAutoLock mon(mMonitor);
  TIMER_LOG("MediaTimer::Cancel");
  Reject();
}

void MediaTimer::ScheduleUpdate() {
  mMonitor.AssertCurrentThreadOwns();
  if (mUpdateScheduled) {
    return;
  }
  mUpdateScheduled = true;

  nsresult rv = mThread->Dispatch(
      NewRunnableMethod("MediaTimer::Update", this, &MediaTimer::Update),
      NS_DISPATCH_NORMAL);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
  (void)rv;
}

void MediaTimer::Update() {
  MonitorAutoLock mon(mMonitor);
  UpdateLocked();
}

bool MediaTimer::IsExpired(const TimeStamp& aTarget, const TimeStamp& aNow) {
  MOZ_ASSERT(OnMediaTimerThread());
  mMonitor.AssertCurrentThreadOwns();
  // Treat this timer as expired in fuzzy mode even if it is fired
  // slightly (< 1ms) before the schedule target. So we don't need to schedule a
  // timer with very small timeout again when the client doesn't need a high-res
  // timer.
  TimeStamp t = mFuzzy ? aTarget - TimeDuration::FromMilliseconds(1) : aTarget;
  return t <= aNow;
}

void MediaTimer::UpdateLocked() {
  MOZ_ASSERT(OnMediaTimerThread());
  mMonitor.AssertCurrentThreadOwns();
  mUpdateScheduled = false;

  TIMER_LOG("MediaTimer::UpdateLocked");

  // Resolve all the promises whose time is up.
  TimeStamp now = TimeStamp::Now();
  while (!mEntries.empty() && IsExpired(mEntries.top().mTimeStamp, now)) {
    mEntries.top().mPromise->Resolve(true, __func__);
    DebugOnly<TimeStamp> poppedTimeStamp = mEntries.top().mTimeStamp;
    mEntries.pop();
    MOZ_ASSERT_IF(!mEntries.empty(),
                  *&poppedTimeStamp <= mEntries.top().mTimeStamp);
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

void MediaTimer::Reject() {
  mMonitor.AssertCurrentThreadOwns();
  while (!mEntries.empty()) {
    mEntries.top().mPromise->Reject(false, __func__);
    mEntries.pop();
  }
}

/*
 * We use a callback function, rather than a callback method, to ensure that
 * the nsITimer does not artifically keep the refcount of the MediaTimer above
 * zero. When the MediaTimer is destroyed, it safely cancels the nsITimer so
 * that we never fire against a dangling closure.
 */

/* static */
void MediaTimer::TimerCallback(nsITimer* aTimer, void* aClosure) {
  static_cast<MediaTimer*>(aClosure)->TimerFired();
}

void MediaTimer::TimerFired() {
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(OnMediaTimerThread());
  mCurrentTimerTarget = TimeStamp();
  UpdateLocked();
}

void MediaTimer::ArmTimer(const TimeStamp& aTarget, const TimeStamp& aNow) {
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
  Unused << rv;
  (void)rv;
}

}  // namespace mozilla
