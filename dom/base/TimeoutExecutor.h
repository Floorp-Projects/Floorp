/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_timeoutexecutor_h
#define mozilla_dom_timeoutexecutor_h

#include "nsIRunnable.h"
#include "nsITimer.h"
#include "nsINamed.h"

namespace mozilla {
namespace dom {

class TimeoutExecutor final : public nsIRunnable
                            , public nsITimerCallback
                            , public nsINamed
{
  TimeoutManager* mOwner;
  nsCOMPtr<nsITimer> mTimer;
  TimeStamp mDeadline;

  // Limits how far we allow timers to fire into the future from their
  // deadline.  Starts off at zero, but is then adjusted when we start
  // using nsITimer.  The nsITimer implementation may sometimes fire
  // early and we should allow that to minimize additional wakeups.
  TimeDuration mAllowedEarlyFiringTime;

  // The TimeoutExecutor is repeatedly scheduled by the TimeoutManager
  // to fire for the next soonest Timeout.  Since the executor is re-used
  // it needs to handle switching between a few states.
  enum class Mode
  {
    // None indicates the executor is idle.  It may be scheduled or shutdown.
    None,
    // Immediate means the executor is scheduled to run a Timeout with a
    // deadline that has already expired.
    Immediate,
    // Delayed means the executor is scheduled to run a Timeout with a
    // deadline in the future.
    Delayed,
    // Shutdown means the TimeoutManager has been destroyed.  Once this
    // state is reached the executor cannot be scheduled again.  If the
    // executor is already dispatched as a runnable or held by a timer then
    // we may still get a Run()/Notify() call which will be ignored.
    Shutdown
  };

  Mode mMode;

  ~TimeoutExecutor();

  nsresult
  ScheduleImmediate(const TimeStamp& aDeadline, const TimeStamp& aNow);

  nsresult
  ScheduleDelayed(const TimeStamp& aDeadline, const TimeStamp& aNow,
                  const TimeDuration& aMinDelay);

  nsresult
  Schedule(const TimeStamp& aDeadline, const TimeDuration& aMinDelay);

  nsresult
  MaybeReschedule(const TimeStamp& aDeadline, const TimeDuration& aMinDelay);

  void
  MaybeExecute();

public:
  explicit TimeoutExecutor(TimeoutManager* aOwner);

  void
  Shutdown();

  nsresult
  MaybeSchedule(const TimeStamp& aDeadline, const TimeDuration& aMinDelay);

  void
  Cancel();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_timeoutexecutor_h
