/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimeoutExecutor.h"

#include "mozilla/dom/TimeoutManager.h"
#include "nsComponentManagerUtils.h"
#include "nsIEventTarget.h"
#include "nsString.h"

extern mozilla::LazyLogModule gTimeoutLog;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(TimeoutExecutor, nsIRunnable, nsITimerCallback, nsINamed)

TimeoutExecutor::~TimeoutExecutor() {
  // The TimeoutManager should keep the Executor alive until its destroyed,
  // and then call Shutdown() explicitly.
  MOZ_DIAGNOSTIC_ASSERT(mMode == Mode::Shutdown);
  MOZ_DIAGNOSTIC_ASSERT(!mOwner);
  MOZ_DIAGNOSTIC_ASSERT(!mTimer);
}

nsresult TimeoutExecutor::ScheduleImmediate(const TimeStamp& aDeadline,
                                            const TimeStamp& aNow) {
  MOZ_DIAGNOSTIC_ASSERT(mDeadline.IsNull());
  MOZ_DIAGNOSTIC_ASSERT(mMode == Mode::None);
  MOZ_DIAGNOSTIC_ASSERT(aDeadline <= (aNow + mAllowedEarlyFiringTime));

  nsresult rv;
  if (mIsIdleQueue) {
    RefPtr<TimeoutExecutor> runnable(this);
    MOZ_LOG(gTimeoutLog, LogLevel::Debug, ("Starting IdleDispatch runnable"));
    rv = NS_DispatchToCurrentThreadQueue(runnable.forget(), mMaxIdleDeferMS,
                                         EventQueuePriority::DeferredTimers);
  } else {
    rv = mOwner->EventTarget()->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  mMode = Mode::Immediate;
  mDeadline = aDeadline;

  return NS_OK;
}

nsresult TimeoutExecutor::ScheduleDelayed(const TimeStamp& aDeadline,
                                          const TimeStamp& aNow,
                                          const TimeDuration& aMinDelay) {
  MOZ_DIAGNOSTIC_ASSERT(mDeadline.IsNull());
  MOZ_DIAGNOSTIC_ASSERT(mMode == Mode::None);
  MOZ_DIAGNOSTIC_ASSERT(!aMinDelay.IsZero() ||
                        aDeadline > (aNow + mAllowedEarlyFiringTime));

  nsresult rv = NS_OK;

  if (mIsIdleQueue) {
    // Nothing goes into the idletimeouts list if it wasn't going to
    // fire at that time, so we can always schedule idle-execution of
    // these immediately
    return ScheduleImmediate(aNow, aNow);
  }

  if (!mTimer) {
    mTimer = NS_NewTimer(mOwner->EventTarget());
    NS_ENSURE_TRUE(mTimer, NS_ERROR_OUT_OF_MEMORY);

    uint32_t earlyMicros = 0;
    MOZ_ALWAYS_SUCCEEDS(
        mTimer->GetAllowedEarlyFiringMicroseconds(&earlyMicros));
    mAllowedEarlyFiringTime = TimeDuration::FromMicroseconds(earlyMicros);
    // Re-evaluate if we should have scheduled this immediately
    if (aDeadline <= (aNow + mAllowedEarlyFiringTime)) {
      return ScheduleImmediate(aDeadline, aNow);
    }
  } else {
    // Always call Cancel() in case we are re-using a timer.
    rv = mTimer->Cancel();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Calculate the delay based on the deadline and current time.  If we have
  // a minimum delay set then clamp to that value.
  //
  // Note, we don't actually adjust our mDeadline for the minimum delay, just
  // the nsITimer value.  This is necessary to avoid lots of needless
  // rescheduling if more deadlines come in between now and the minimum delay
  // firing time.
  TimeDuration delay = TimeDuration::Max(aMinDelay, aDeadline - aNow);

  // Note, we cannot use the normal nsITimer init methods that take
  // integer milliseconds.  We need higher precision.  Consider this
  // situation:
  //
  // 1. setTimeout(f, 1);
  // 2. do some work for 500us
  // 3. setTimeout(g, 1);
  //
  // This should fire f() and g() 500us apart.
  //
  // In the past worked because each setTimeout() got its own nsITimer.  The 1ms
  // was preserved and passed through to nsITimer which converted it to a
  // TimeStamp, etc.
  //
  // Now, however, there is only one nsITimer.  We fire f() and then try to
  // schedule a new nsITimer for g().  Its only 500us in the future, though.  We
  // must be able to pass this fractional value to nsITimer in order to get an
  // accurate wakeup time.
  rv = mTimer->InitHighResolutionWithCallback(this, delay,
                                              nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);

  mMode = Mode::Delayed;
  mDeadline = aDeadline;

  return NS_OK;
}

nsresult TimeoutExecutor::Schedule(const TimeStamp& aDeadline,
                                   const TimeDuration& aMinDelay) {
  TimeStamp now(TimeStamp::Now());

  // Schedule an immediate runnable if the desired deadline has passed
  // or is slightly in the future.  This is similar to how nsITimer will
  // fire timers early based on the interval resolution.
  if (aMinDelay.IsZero() && aDeadline <= (now + mAllowedEarlyFiringTime)) {
    return ScheduleImmediate(aDeadline, now);
  }

  return ScheduleDelayed(aDeadline, now, aMinDelay);
}

nsresult TimeoutExecutor::MaybeReschedule(const TimeStamp& aDeadline,
                                          const TimeDuration& aMinDelay) {
  MOZ_DIAGNOSTIC_ASSERT(!mDeadline.IsNull());
  MOZ_DIAGNOSTIC_ASSERT(mMode == Mode::Immediate || mMode == Mode::Delayed);

  if (aDeadline >= mDeadline) {
    return NS_OK;
  }

  if (mMode == Mode::Immediate) {
    // Don't reduce the deadline here as we want to execute the
    // timer we originally scheduled even if its a few microseconds
    // in the future.
    return NS_OK;
  }

  Cancel();
  return Schedule(aDeadline, aMinDelay);
}

void TimeoutExecutor::MaybeExecute() {
  MOZ_DIAGNOSTIC_ASSERT(mMode != Mode::Shutdown && mMode != Mode::None);
  MOZ_DIAGNOSTIC_ASSERT(mOwner);
  MOZ_DIAGNOSTIC_ASSERT(!mDeadline.IsNull());

  TimeStamp deadline(mDeadline);

  // Sometimes nsITimer or canceled timers will fire too early.  If this
  // happens then just cap our deadline to our maximum time in the future
  // and proceed.  If there are no timers ready we will get rescheduled
  // by TimeoutManager.
  TimeStamp now(TimeStamp::Now());
  TimeStamp limit = now + mAllowedEarlyFiringTime;
  if (deadline > limit) {
    deadline = limit;
  }

  Cancel();

  mOwner->RunTimeout(now, deadline, mIsIdleQueue);
}

TimeoutExecutor::TimeoutExecutor(TimeoutManager* aOwner, bool aIsIdleQueue,
                                 uint32_t aMaxIdleDeferMS)
    : mOwner(aOwner),
      mIsIdleQueue(aIsIdleQueue),
      mMaxIdleDeferMS(aMaxIdleDeferMS),
      mMode(Mode::None) {
  MOZ_DIAGNOSTIC_ASSERT(mOwner);
}

void TimeoutExecutor::Shutdown() {
  mOwner = nullptr;

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  mMode = Mode::Shutdown;
  mDeadline = TimeStamp();
}

nsresult TimeoutExecutor::MaybeSchedule(const TimeStamp& aDeadline,
                                        const TimeDuration& aMinDelay) {
  MOZ_DIAGNOSTIC_ASSERT(!aDeadline.IsNull());

  if (mMode == Mode::Shutdown) {
    return NS_OK;
  }

  if (mMode == Mode::Immediate || mMode == Mode::Delayed) {
    return MaybeReschedule(aDeadline, aMinDelay);
  }

  return Schedule(aDeadline, aMinDelay);
}

void TimeoutExecutor::Cancel() {
  if (mTimer) {
    mTimer->Cancel();
  }
  mMode = Mode::None;
  mDeadline = TimeStamp();
}

// MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.  See
// bug 1535398.
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP TimeoutExecutor::Run() {
  // If the executor is canceled and then rescheduled its possible to get
  // spurious executions here.  Ignore these unless our current mode matches.
  MOZ_LOG(gTimeoutLog, LogLevel::Debug,
          ("Running Immediate %stimers", mIsIdleQueue ? "Idle" : ""));
  if (mMode == Mode::Immediate) {
    MaybeExecute();
  }
  return NS_OK;
}

// MOZ_CAN_RUN_SCRIPT_BOUNDARY until nsITimerCallback::Notify is
// MOZ_CAN_RUN_SCRIPT.
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
TimeoutExecutor::Notify(nsITimer* aTimer) {
  // If the executor is canceled and then rescheduled its possible to get
  // spurious executions here.  Ignore these unless our current mode matches.
  if (mMode == Mode::Delayed) {
    MaybeExecute();
  }
  return NS_OK;
}

NS_IMETHODIMP
TimeoutExecutor::GetName(nsACString& aNameOut) {
  aNameOut.AssignLiteral("TimeoutExecutor Runnable");
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
