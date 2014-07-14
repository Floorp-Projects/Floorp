/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoderStateMachineScheduler.h"
#include "SharedThreadPool.h"
#include "mozilla/Preferences.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsITimer.h"
#include "nsComponentManagerUtils.h"
#include "VideoUtils.h"

namespace {
class TimerEvent : public nsITimerCallback, public nsRunnable {
  typedef mozilla::MediaDecoderStateMachineScheduler Scheduler;
  NS_DECL_ISUPPORTS_INHERITED
public:
  TimerEvent(Scheduler* aScheduler, int aTimerId)
    : mScheduler(aScheduler), mTimerId(aTimerId) {}

  NS_IMETHOD Run() MOZ_OVERRIDE {
    return mScheduler->TimeoutExpired(mTimerId);
  }

  NS_IMETHOD Notify(nsITimer* aTimer) MOZ_OVERRIDE {
    return mScheduler->TimeoutExpired(mTimerId);
  }
private:
  ~TimerEvent() {}
  Scheduler* const mScheduler;
  const int mTimerId;
};

NS_IMPL_ISUPPORTS_INHERITED(TimerEvent, nsRunnable, nsITimerCallback);
} // anonymous namespace

static already_AddRefed<nsIEventTarget>
CreateStateMachineThread()
{
  using mozilla::SharedThreadPool;
  using mozilla::RefPtr;
  RefPtr<SharedThreadPool> threadPool(
      SharedThreadPool::Get(NS_LITERAL_CSTRING("Media State Machine"), 1));
  nsCOMPtr<nsIEventTarget> rv = threadPool.get();
  return rv.forget();
}

namespace mozilla {

MediaDecoderStateMachineScheduler::MediaDecoderStateMachineScheduler(
    ReentrantMonitor& aMonitor,
    nsresult (*aTimeoutCallback)(void*),
    void* aClosure, bool aRealTime)
  : mTimeoutCallback(aTimeoutCallback)
  , mClosure(aClosure)
  // Only enable realtime mode when "media.realtime_decoder.enabled" is true.
  , mRealTime(aRealTime &&
              Preferences::GetBool("media.realtime_decoder.enabled", false))
  , mMonitor(aMonitor)
  , mEventTarget(CreateStateMachineThread())
  , mTimer(do_CreateInstance("@mozilla.org/timer;1"))
  , mTimerId(0)
  , mState(SCHEDULER_STATE_NONE)
  , mInRunningStateMachine(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(MediaDecoderStateMachineScheduler);
}

MediaDecoderStateMachineScheduler::~MediaDecoderStateMachineScheduler()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_DTOR(MediaDecoderStateMachineScheduler);
}

nsresult
MediaDecoderStateMachineScheduler::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(mEventTarget, NS_ERROR_FAILURE);
  nsresult rv = mTimer->SetTarget(mEventTarget);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
MediaDecoderStateMachineScheduler::Schedule(int64_t aUsecs)
{
  mMonitor.AssertCurrentThreadIn();

  switch(mState) {
  case SCHEDULER_STATE_SHUTDOWN:
    return NS_ERROR_FAILURE;
  case SCHEDULER_STATE_FROZEN:
    mState = SCHEDULER_STATE_FROZEN_WITH_PENDING_TASK;
  case SCHEDULER_STATE_FROZEN_WITH_PENDING_TASK:
    return NS_OK;
  case SCHEDULER_STATE_NONE:
    break;
  }

  aUsecs = std::max<int64_t>(aUsecs, 0);

  TimeStamp timeout = TimeStamp::Now() +
    TimeDuration::FromMilliseconds(static_cast<double>(aUsecs) / USECS_PER_MS);

  if (!mTimeout.IsNull() && timeout >= mTimeout) {
    // We've already scheduled a timer set to expire at or before this time,
    // or have an event dispatched to run the state machine.
    return NS_OK;
  }

  uint32_t ms = static_cast<uint32_t>((aUsecs / USECS_PER_MS) & 0xFFFFFFFF);
  if (IsRealTime() && ms > 40) {
    ms = 40;
  }

  // Don't cancel the timer here for this function will be called from
  // different threads.

  nsresult rv = NS_ERROR_FAILURE;
  nsRefPtr<TimerEvent> event = new TimerEvent(this, mTimerId+1);

  if (ms == 0) {
    // Dispatch a runnable to the state machine thread when delay is 0.
    // It will has less latency than dispatching a runnable to the state
    // machine thread which will then schedule a zero-delay timer.
    rv = mEventTarget->Dispatch(event, NS_DISPATCH_NORMAL);
  } else if (OnStateMachineThread()) {
    rv = mTimer->InitWithCallback(event, ms, nsITimer::TYPE_ONE_SHOT);
  } else {
    MOZ_ASSERT(false, "non-zero delay timer should be only "
                      "scheduled in state machine thread");
  }

  if (NS_SUCCEEDED(rv)) {
    mTimeout = timeout;
    ++mTimerId;
  } else {
    NS_WARNING("Failed to schedule state machine");
  }

  return rv;
}

nsresult
MediaDecoderStateMachineScheduler::TimeoutExpired(int aTimerId)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  MOZ_ASSERT(OnStateMachineThread());
  MOZ_ASSERT(!mInRunningStateMachine,
             "State machine cycles must run in sequence!");

  mInRunningStateMachine = true;
  // Only run state machine cycles when id matches.
  nsresult rv = NS_OK;
  if (mTimerId == aTimerId) {
    ResetTimer();
    rv = mTimeoutCallback(mClosure);
  }
  mInRunningStateMachine = false;

  return rv;
}

void
MediaDecoderStateMachineScheduler::ScheduleAndShutdown()
{
  mMonitor.AssertCurrentThreadIn();
  if (IsFrozen()) {
    ThawScheduling();
  }
  // Schedule next cycle to handle SHUTDOWN in state machine thread.
  Schedule();
  // This must be set after calling Schedule()
  // which does nothing in shutdown state.
  mState = SCHEDULER_STATE_SHUTDOWN;
}

bool
MediaDecoderStateMachineScheduler::OnStateMachineThread() const
{
  bool rv = false;
  mEventTarget->IsOnCurrentThread(&rv);
  return rv;
}

bool
MediaDecoderStateMachineScheduler::IsScheduled() const
{
  mMonitor.AssertCurrentThreadIn();
  return !mTimeout.IsNull();
}

void
MediaDecoderStateMachineScheduler::ResetTimer()
{
  mMonitor.AssertCurrentThreadIn();
  mTimer->Cancel();
  mTimeout = TimeStamp();
}

void MediaDecoderStateMachineScheduler::FreezeScheduling()
{
  mMonitor.AssertCurrentThreadIn();
  if (mState == SCHEDULER_STATE_SHUTDOWN) {
    return;
  }
  MOZ_ASSERT(mState == SCHEDULER_STATE_NONE);
  mState = !IsScheduled() ? SCHEDULER_STATE_FROZEN :
                            SCHEDULER_STATE_FROZEN_WITH_PENDING_TASK;
  // Nullify pending timer task if any.
  ++mTimerId;
  mTimeout = TimeStamp();
}

void MediaDecoderStateMachineScheduler::ThawScheduling()
{
  mMonitor.AssertCurrentThreadIn();
  if (mState == SCHEDULER_STATE_SHUTDOWN) {
    return;
  }
  // We should be in frozen state and no pending timer task.
  MOZ_ASSERT(IsFrozen() && !IsScheduled());
  bool pendingTask = mState == SCHEDULER_STATE_FROZEN_WITH_PENDING_TASK;
  mState = SCHEDULER_STATE_NONE;
  if (pendingTask) {
    Schedule();
  }
}

} // namespace mozilla
