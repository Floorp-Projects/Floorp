/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimeoutExecutor.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(TimeoutExecutor, nsIRunnable, nsITimerCallback, nsINamed)

TimeoutExecutor::~TimeoutExecutor()
{
  // The TimeoutManager should keep the Executor alive until its destroyed,
  // and then call Shutdown() explicitly.
  MOZ_DIAGNOSTIC_ASSERT(mMode == Mode::Shutdown);
  MOZ_DIAGNOSTIC_ASSERT(!mOwner);
  MOZ_DIAGNOSTIC_ASSERT(!mTimer);
}

nsresult
TimeoutExecutor::ScheduleImmediate(const TimeStamp& aDeadline,
                                   const TimeStamp& aNow)
{
  MOZ_DIAGNOSTIC_ASSERT(mDeadline.IsNull());
  MOZ_DIAGNOSTIC_ASSERT(mMode == Mode::None);
  MOZ_DIAGNOSTIC_ASSERT(aDeadline <= (aNow + mAllowedEarlyFiringTime));

  nsresult rv =
    mOwner->EventTarget()->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  mMode = Mode::Immediate;
  mDeadline = aDeadline;

  return NS_OK;
}

nsresult
TimeoutExecutor::ScheduleDelayed(const TimeStamp& aDeadline,
                                 const TimeStamp& aNow,
                                 const TimeDuration& aMinDelay)
{
  MOZ_DIAGNOSTIC_ASSERT(mDeadline.IsNull());
  MOZ_DIAGNOSTIC_ASSERT(mMode == Mode::None);
  MOZ_DIAGNOSTIC_ASSERT(!aMinDelay.IsZero() ||
                        aDeadline > (aNow + mAllowedEarlyFiringTime));

  nsresult rv = NS_OK;

  if (!mTimer) {
    mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t earlyMicros = 0;
    MOZ_ALWAYS_SUCCEEDS(mTimer->GetAllowedEarlyFiringMicroseconds(&earlyMicros));
    mAllowedEarlyFiringTime = TimeDuration::FromMicroseconds(earlyMicros);
  }

  // Always call Cancel() in case we are re-using a timer.  Otherwise
  // the subsequent SetTarget() may fail.
  rv = mTimer->Cancel();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mTimer->SetTarget(mOwner->EventTarget());
  NS_ENSURE_SUCCESS(rv, rv);

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

nsresult
TimeoutExecutor::Schedule(const TimeStamp& aDeadline,
                          const TimeDuration& aMinDelay)
{
  TimeStamp now(TimeStamp::Now());

  // Schedule an immediate runnable if the desired deadline has passed
  // or is slightly in the future.  This is similar to how nsITimer will
  // fire timers early based on the interval resolution.
  if (aMinDelay.IsZero() && aDeadline <= (now + mAllowedEarlyFiringTime)) {
    return ScheduleImmediate(aDeadline, now);
  }

  return ScheduleDelayed(aDeadline, now, aMinDelay);
}

nsresult
TimeoutExecutor::MaybeReschedule(const TimeStamp& aDeadline,
                                 const TimeDuration& aMinDelay)
{
  MOZ_DIAGNOSTIC_ASSERT(!mDeadline.IsNull());
  MOZ_DIAGNOSTIC_ASSERT(mMode == Mode::Immediate ||
                        mMode == Mode::Delayed);

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

void
TimeoutExecutor::MaybeExecute()
{
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

  mOwner->RunTimeout(now, deadline);
}

TimeoutExecutor::TimeoutExecutor(TimeoutManager* aOwner)
  : mOwner(aOwner)
  , mMode(Mode::None)
{
  MOZ_DIAGNOSTIC_ASSERT(mOwner);
}

void
TimeoutExecutor::Shutdown()
{
  mOwner = nullptr;

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  mMode = Mode::Shutdown;
  mDeadline = TimeStamp();
}

nsresult
TimeoutExecutor::MaybeSchedule(const TimeStamp& aDeadline,
                               const TimeDuration& aMinDelay)
{
  MOZ_DIAGNOSTIC_ASSERT(!aDeadline.IsNull());

  if (mMode == Mode::Shutdown) {
    return NS_OK;
  }

  if (mMode == Mode::Immediate || mMode == Mode::Delayed) {
    return MaybeReschedule(aDeadline, aMinDelay);
  }

  return Schedule(aDeadline, aMinDelay);
}

void
TimeoutExecutor::Cancel()
{
  if (mTimer) {
    mTimer->Cancel();
  }
  mMode = Mode::None;
  mDeadline = TimeStamp();
}

NS_IMETHODIMP
TimeoutExecutor::Run()
{
  // If the executor is canceled and then rescheduled its possible to get
  // spurious executions here.  Ignore these unless our current mode matches.
  if (mMode == Mode::Immediate) {
    MaybeExecute();
  }
  return NS_OK;
}

NS_IMETHODIMP
TimeoutExecutor::Notify(nsITimer* aTimer)
{
  // If the executor is canceled and then rescheduled its possible to get
  // spurious executions here.  Ignore these unless our current mode matches.
  if (mMode == Mode::Delayed) {
    MaybeExecute();
  }
  return NS_OK;
}

NS_IMETHODIMP
TimeoutExecutor::GetName(nsACString& aNameOut)
{
  aNameOut.AssignLiteral("TimeoutExecutor Runnable");
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
