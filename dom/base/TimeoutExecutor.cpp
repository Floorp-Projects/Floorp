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
  MOZ_DIAGNOSTIC_ASSERT(aDeadline <= aNow);

  nsresult rv =
    mOwner->EventTarget()->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  mMode = Mode::Immediate;
  mDeadline = aDeadline;

  return NS_OK;
}

nsresult
TimeoutExecutor::ScheduleDelayed(const TimeStamp& aDeadline,
                                 const TimeStamp& aNow)
{
  MOZ_DIAGNOSTIC_ASSERT(mDeadline.IsNull());
  MOZ_DIAGNOSTIC_ASSERT(mMode == Mode::None);
  MOZ_DIAGNOSTIC_ASSERT(aDeadline > aNow);

  nsresult rv = NS_OK;

  if (!mTimer) {
    mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Always call Cancel() in case we are re-using a timer.  Otherwise
  // the subsequent SetTarget() may fail.
  rv = mTimer->Cancel();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mTimer->SetTarget(mOwner->EventTarget());
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert the precise delay to integral milliseconds for nsITimer.  We
  // favor rounding down here.  If we fire early we will simply be rescheduled
  // for an immediate runnable or a 0-ms timer.  This ends up giving us the
  // most accurate firing time at the cost of a few more runnables.  This cost
  // is only incurred when the browser is idle, though.  When the busy main
  // thread is busy there will be a delay and we won't actually be early.
  // TODO: In the future we could pass a precision argument in and round
  //       up here for low-precision background timers.  We don't really care
  //       if those timers fire late.
  TimeDuration delay(aDeadline - aNow - TimeDuration::FromMilliseconds(0.1));
  rv = mTimer->InitWithCallback(this, delay.ToMilliseconds(),
                                nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);

  mMode = Mode::Delayed;
  mDeadline = aDeadline;

  return NS_OK;
}

nsresult
TimeoutExecutor::Schedule(const TimeStamp& aDeadline)
{
  TimeStamp now(TimeStamp::Now());

  // Schedule an immediate runnable if the desired deadline has passed
  // or is slightly in the future.  This is similar to how nsITimer will
  // fire timers early based on the interval resolution.
  if (aDeadline <= now) {
    return ScheduleImmediate(aDeadline, now);
  }

  return ScheduleDelayed(aDeadline, now);
}

nsresult
TimeoutExecutor::MaybeReschedule(const TimeStamp& aDeadline)
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
  return Schedule(aDeadline);
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
  if (deadline > now) {
    deadline = now;
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
TimeoutExecutor::MaybeSchedule(const TimeStamp& aDeadline)
{
  MOZ_DIAGNOSTIC_ASSERT(!aDeadline.IsNull());

  if (mMode == Mode::Shutdown) {
    return NS_OK;
  }

  if (mMode == Mode::Immediate || mMode == Mode::Delayed) {
    return MaybeReschedule(aDeadline);
  }

  return Schedule(aDeadline);
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

NS_IMETHODIMP
TimeoutExecutor::SetName(const char* aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace dom
} // namespace mozilla
