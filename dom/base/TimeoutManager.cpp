/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimeoutManager.h"
#include "nsGlobalWindowInner.h"
#include "mozilla/Logging.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ThrottledEventQueue.h"
#include "mozilla/TimeStamp.h"
#include "nsINamed.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/PopupBlocker.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/TimeoutHandler.h"
#include "TimeoutExecutor.h"
#include "TimeoutBudgetManager.h"
#include "mozilla/net/WebSocketEventService.h"
#include "mozilla/MediaManager.h"

using namespace mozilla;
using namespace mozilla::dom;

LazyLogModule gTimeoutLog("Timeout");

static int32_t gRunningTimeoutDepth = 0;

// static
const uint32_t TimeoutManager::InvalidFiringId = 0;

namespace {
double GetRegenerationFactor(bool aIsBackground) {
  // Lookup function for "dom.timeout.{background,
  // foreground}_budget_regeneration_rate".

  // Returns the rate of regeneration of the execution budget as a
  // fraction. If the value is 1.0, the amount of time regenerated is
  // equal to time passed. At this rate we regenerate 1ms/ms. If it is
  // 0.01 the amount regenerated is 1% of time passed. At this rate we
  // regenerate 1ms/100ms, etc.
  double denominator = std::max(
      aIsBackground
          ? StaticPrefs::dom_timeout_background_budget_regeneration_rate()
          : StaticPrefs::dom_timeout_foreground_budget_regeneration_rate(),
      1);
  return 1.0 / denominator;
}

TimeDuration GetMaxBudget(bool aIsBackground) {
  // Lookup function for "dom.timeout.{background,
  // foreground}_throttling_max_budget".

  // Returns how high a budget can be regenerated before being
  // clamped. If this value is less or equal to zero,
  // TimeDuration::Forever() is implied.
  int32_t maxBudget =
      aIsBackground
          ? StaticPrefs::dom_timeout_background_throttling_max_budget()
          : StaticPrefs::dom_timeout_foreground_throttling_max_budget();
  return maxBudget > 0 ? TimeDuration::FromMilliseconds(maxBudget)
                       : TimeDuration::Forever();
}

TimeDuration GetMinBudget(bool aIsBackground) {
  // The minimum budget is computed by looking up the maximum allowed
  // delay and computing how long time it would take to regenerate
  // that budget using the regeneration factor. This number is
  // expected to be negative.
  return TimeDuration::FromMilliseconds(
      -StaticPrefs::dom_timeout_budget_throttling_max_delay() /
      std::max(
          aIsBackground
              ? StaticPrefs::dom_timeout_background_budget_regeneration_rate()
              : StaticPrefs::dom_timeout_foreground_budget_regeneration_rate(),
          1));
}
}  // namespace

//

bool TimeoutManager::IsBackground() const {
  return !IsActive() && mWindow.IsBackgroundInternal();
}

bool TimeoutManager::IsActive() const {
  // A window is considered active if:
  // * It is a chrome window
  // * It is playing audio
  //
  // Note that a window can be considered active if it is either in the
  // foreground or in the background.

  if (mWindow.IsChromeWindow()) {
    return true;
  }

  // Check if we're playing audio
  if (mWindow.IsPlayingAudio()) {
    return true;
  }

  return false;
}

void TimeoutManager::SetLoading(bool value) {
  // When moving from loading to non-loading, we may need to
  // reschedule any existing timeouts from the idle timeout queue
  // to the normal queue.
  MOZ_LOG(gTimeoutLog, LogLevel::Debug, ("%p: SetLoading(%d)", this, value));
  if (mIsLoading && !value) {
    MoveIdleToActive();
  }
  // We don't immediately move existing timeouts to the idle queue if we
  // move to loading.  When they would have fired, we'll see we're loading
  // and move them then.
  mIsLoading = value;
}

void TimeoutManager::MoveIdleToActive() {
  uint32_t num = 0;
  TimeStamp when;
  TimeStamp now;
  // Ensure we maintain the ordering of timeouts, so timeouts
  // never fire before a timeout set for an earlier time, or
  // before a timeout for the same time already submitted.
  // See https://html.spec.whatwg.org/#dom-settimeout #16 and #17
  while (RefPtr<Timeout> timeout = mIdleTimeouts.GetLast()) {
    if (num == 0) {
      when = timeout->When();
    }
    timeout->remove();
    mTimeouts.InsertFront(timeout);
    if (profiler_thread_is_being_profiled_for_markers()) {
      if (num == 0) {
        now = TimeStamp::Now();
      }
      TimeDuration elapsed = now - timeout->SubmitTime();
      TimeDuration target = timeout->When() - timeout->SubmitTime();
      TimeDuration delta = now - timeout->When();
      nsPrintfCString marker(
          "Releasing deferred setTimeout() for %dms (original target time was "
          "%dms (%dms delta))",
          int(elapsed.ToMilliseconds()), int(target.ToMilliseconds()),
          int(delta.ToMilliseconds()));
      // don't have end before start...
      PROFILER_MARKER_TEXT(
          "setTimeout deferred release", DOM,
          MarkerOptions(
              MarkerTiming::Interval(
                  delta.ToMilliseconds() >= 0 ? timeout->When() : now, now),
              MarkerInnerWindowId(mWindow.WindowID())),
          marker);
    }
    num++;
  }
  if (num > 0) {
    MOZ_ALWAYS_SUCCEEDS(MaybeSchedule(when));
    mIdleExecutor->Cancel();
  }
  MOZ_LOG(gTimeoutLog, LogLevel::Debug,
          ("%p: Moved %d timeouts from Idle to active", this, num));
}

uint32_t TimeoutManager::CreateFiringId() {
  uint32_t id = mNextFiringId;
  mNextFiringId += 1;
  if (mNextFiringId == InvalidFiringId) {
    mNextFiringId += 1;
  }

  mFiringIdStack.AppendElement(id);

  return id;
}

void TimeoutManager::DestroyFiringId(uint32_t aFiringId) {
  MOZ_DIAGNOSTIC_ASSERT(!mFiringIdStack.IsEmpty());
  MOZ_DIAGNOSTIC_ASSERT(mFiringIdStack.LastElement() == aFiringId);
  mFiringIdStack.RemoveLastElement();
}

bool TimeoutManager::IsValidFiringId(uint32_t aFiringId) const {
  return !IsInvalidFiringId(aFiringId);
}

TimeDuration TimeoutManager::MinSchedulingDelay() const {
  if (IsActive()) {
    return TimeDuration();
  }

  bool isBackground = mWindow.IsBackgroundInternal();

  // If a window isn't active as defined by TimeoutManager::IsActive()
  // and we're throttling timeouts using an execution budget, we
  // should adjust the minimum scheduling delay if we have used up all
  // of our execution budget. Note that a window can be active or
  // inactive regardless of wether it is in the foreground or in the
  // background. Throttling using a budget depends largely on the
  // regeneration factor, which can be specified separately for
  // foreground and background windows.
  //
  // The value that we compute is the time in the future when we again
  // have a positive execution budget. We do this by taking the
  // execution budget into account, which if it positive implies that
  // we have time left to execute, and if it is negative implies that
  // we should throttle it until the budget again is positive. The
  // factor used is the rate of budget regeneration.
  //
  // We clamp the delay to be less than or equal to
  // "dom.timeout.budget_throttling_max_delay" to not entirely starve
  // the timeouts.
  //
  // Consider these examples assuming we should throttle using
  // budgets:
  //
  // mExecutionBudget is 20ms
  // factor is 1, which is 1 ms/ms
  // delay is 0ms
  // then we will compute the minimum delay:
  // max(0, - 20 * 1) = 0
  //
  // mExecutionBudget is -50ms
  // factor is 0.1, which is 1 ms/10ms
  // delay is 1000ms
  // then we will compute the minimum delay:
  // max(1000, - (- 50) * 1/0.1) = max(1000, 500) = 1000
  //
  // mExecutionBudget is -15ms
  // factor is 0.01, which is 1 ms/100ms
  // delay is 1000ms
  // then we will compute the minimum delay:
  // max(1000, - (- 15) * 1/0.01) = max(1000, 1500) = 1500
  TimeDuration unthrottled =
      isBackground ? TimeDuration::FromMilliseconds(
                         StaticPrefs::dom_min_background_timeout_value())
                   : TimeDuration();
  bool budgetThrottlingEnabled = BudgetThrottlingEnabled(isBackground);
  if (budgetThrottlingEnabled && mExecutionBudget < TimeDuration()) {
    // Only throttle if execution budget is less than 0
    double factor = 1.0 / GetRegenerationFactor(mWindow.IsBackgroundInternal());
    return TimeDuration::Max(unthrottled, -mExecutionBudget.MultDouble(factor));
  }
  if (!budgetThrottlingEnabled && isBackground) {
    return TimeDuration::FromMilliseconds(
        StaticPrefs::
            dom_min_background_timeout_value_without_budget_throttling());
  }

  return unthrottled;
}

nsresult TimeoutManager::MaybeSchedule(const TimeStamp& aWhen,
                                       const TimeStamp& aNow) {
  MOZ_DIAGNOSTIC_ASSERT(mExecutor);

  // Before we can schedule the executor we need to make sure that we
  // have an updated execution budget.
  UpdateBudget(aNow);
  return mExecutor->MaybeSchedule(aWhen, MinSchedulingDelay());
}

bool TimeoutManager::IsInvalidFiringId(uint32_t aFiringId) const {
  // Check the most common ways to invalidate a firing id first.
  // These should be quite fast.
  if (aFiringId == InvalidFiringId || mFiringIdStack.IsEmpty()) {
    return true;
  }

  if (mFiringIdStack.Length() == 1) {
    return mFiringIdStack[0] != aFiringId;
  }

  // Next do a range check on the first and last items in the stack
  // of active firing ids.  This is a bit slower.
  uint32_t low = mFiringIdStack[0];
  uint32_t high = mFiringIdStack.LastElement();
  MOZ_DIAGNOSTIC_ASSERT(low != high);
  if (low > high) {
    // If the first element is bigger than the last element in the
    // stack, that means mNextFiringId wrapped around to zero at
    // some point.
    std::swap(low, high);
  }
  MOZ_DIAGNOSTIC_ASSERT(low < high);

  if (aFiringId < low || aFiringId > high) {
    return true;
  }

  // Finally, fall back to verifying the firing id is not anywhere
  // in the stack.  This could be slow for a large stack, but that
  // should be rare.  It can only happen with deeply nested event
  // loop spinning.  For example, a page that does a lot of timers
  // and a lot of sync XHRs within those timers could be slow here.
  return !mFiringIdStack.Contains(aFiringId);
}

TimeDuration TimeoutManager::CalculateDelay(Timeout* aTimeout) const {
  MOZ_DIAGNOSTIC_ASSERT(aTimeout);
  TimeDuration result = aTimeout->mInterval;

  if (aTimeout->mNestingLevel >=
      StaticPrefs::dom_clamp_timeout_nesting_level_AtStartup()) {
    uint32_t minTimeoutValue = StaticPrefs::dom_min_timeout_value();
    result = TimeDuration::Max(result,
                               TimeDuration::FromMilliseconds(minTimeoutValue));
  }

  return result;
}

void TimeoutManager::RecordExecution(Timeout* aRunningTimeout,
                                     Timeout* aTimeout) {
  TimeoutBudgetManager& budgetManager = TimeoutBudgetManager::Get();
  TimeStamp now = TimeStamp::Now();

  if (aRunningTimeout) {
    // If we're running a timeout callback, record any execution until
    // now.
    TimeDuration duration = budgetManager.RecordExecution(now, aRunningTimeout);

    UpdateBudget(now, duration);
  }

  if (aTimeout) {
    // If we're starting a new timeout callback, start recording.
    budgetManager.StartRecording(now);
  } else {
    // Else stop by clearing the start timestamp.
    budgetManager.StopRecording();
  }
}

void TimeoutManager::UpdateBudget(const TimeStamp& aNow,
                                  const TimeDuration& aDuration) {
  if (mWindow.IsChromeWindow()) {
    return;
  }

  // The budget is adjusted by increasing it with the time since the
  // last budget update factored with the regeneration rate. If a
  // runnable has executed, subtract that duration from the
  // budget. The budget updated without consideration of wether the
  // window is active or not. If throttling is enabled and the window
  // is active and then becomes inactive, an overdrawn budget will
  // still be counted against the minimum delay.
  bool isBackground = mWindow.IsBackgroundInternal();
  if (BudgetThrottlingEnabled(isBackground)) {
    double factor = GetRegenerationFactor(isBackground);
    TimeDuration regenerated = (aNow - mLastBudgetUpdate).MultDouble(factor);
    // Clamp the budget to the range of minimum and maximum allowed budget.
    mExecutionBudget = TimeDuration::Max(
        GetMinBudget(isBackground),
        TimeDuration::Min(GetMaxBudget(isBackground),
                          mExecutionBudget - aDuration + regenerated));
  } else {
    // If budget throttling isn't enabled, reset the execution budget
    // to the max budget specified in preferences. Always doing this
    // will catch the case of BudgetThrottlingEnabled going from
    // returning true to returning false. This prevent us from looping
    // in RunTimeout, due to totalTimeLimit being set to zero and no
    // timeouts being executed, even though budget throttling isn't
    // active at the moment.
    mExecutionBudget = GetMaxBudget(isBackground);
  }

  mLastBudgetUpdate = aNow;
}

// The longest interval (as PRIntervalTime) we permit, or that our
// timer code can handle, really. See DELAY_INTERVAL_LIMIT in
// nsTimerImpl.h for details.
#define DOM_MAX_TIMEOUT_VALUE DELAY_INTERVAL_LIMIT

uint32_t TimeoutManager::sNestingLevel = 0;

TimeoutManager::TimeoutManager(nsGlobalWindowInner& aWindow,
                               uint32_t aMaxIdleDeferMS)
    : mWindow(aWindow),
      mExecutor(new TimeoutExecutor(this, false, 0)),
      mIdleExecutor(new TimeoutExecutor(this, true, aMaxIdleDeferMS)),
      mTimeouts(*this),
      mTimeoutIdCounter(1),
      mNextFiringId(InvalidFiringId + 1),
#ifdef DEBUG
      mFiringIndex(0),
      mLastFiringIndex(-1),
#endif
      mRunningTimeout(nullptr),
      mIdleTimeouts(*this),
      mIdleCallbackTimeoutCounter(1),
      mLastBudgetUpdate(TimeStamp::Now()),
      mExecutionBudget(GetMaxBudget(mWindow.IsBackgroundInternal())),
      mThrottleTimeouts(false),
      mThrottleTrackingTimeouts(false),
      mBudgetThrottleTimeouts(false),
      mIsLoading(false) {
  MOZ_LOG(gTimeoutLog, LogLevel::Debug,
          ("TimeoutManager %p created, tracking bucketing %s\n", this,
           StaticPrefs::privacy_trackingprotection_annotate_channels()
               ? "enabled"
               : "disabled"));
}

TimeoutManager::~TimeoutManager() {
  MOZ_DIAGNOSTIC_ASSERT(mWindow.IsDying());
  MOZ_DIAGNOSTIC_ASSERT(!mThrottleTimeoutsTimer);

  mExecutor->Shutdown();
  mIdleExecutor->Shutdown();

  MOZ_LOG(gTimeoutLog, LogLevel::Debug,
          ("TimeoutManager %p destroyed\n", this));
}

uint32_t TimeoutManager::GetTimeoutId(Timeout::Reason aReason) {
  switch (aReason) {
    case Timeout::Reason::eIdleCallbackTimeout:
      return ++mIdleCallbackTimeoutCounter;
    case Timeout::Reason::eTimeoutOrInterval:
      return ++mTimeoutIdCounter;
    case Timeout::Reason::eDelayedWebTaskTimeout:
    default:
      return std::numeric_limits<uint32_t>::max();  // no cancellation support
  }
}

bool TimeoutManager::IsRunningTimeout() const { return mRunningTimeout; }

nsresult TimeoutManager::SetTimeout(TimeoutHandler* aHandler, int32_t interval,
                                    bool aIsInterval, Timeout::Reason aReason,
                                    int32_t* aReturn) {
  // If we don't have a document (we could have been unloaded since
  // the call to setTimeout was made), do nothing.
  nsCOMPtr<Document> doc = mWindow.GetExtantDoc();
  if (!doc || mWindow.IsDying()) {
    return NS_OK;
  }

  // Disallow negative intervals.
  interval = std::max(0, interval);

  // Make sure we don't proceed with an interval larger than our timer
  // code can handle. (Note: we already forced |interval| to be non-negative,
  // so the uint32_t cast (to avoid compiler warnings) is ok.)
  uint32_t maxTimeoutMs = PR_IntervalToMilliseconds(DOM_MAX_TIMEOUT_VALUE);
  if (static_cast<uint32_t>(interval) > maxTimeoutMs) {
    interval = maxTimeoutMs;
  }

  RefPtr<Timeout> timeout = new Timeout();
#ifdef DEBUG
  timeout->mFiringIndex = -1;
#endif
  timeout->mWindow = &mWindow;
  timeout->mIsInterval = aIsInterval;
  timeout->mInterval = TimeDuration::FromMilliseconds(interval);
  timeout->mScriptHandler = aHandler;
  timeout->mReason = aReason;

  // No popups from timeouts by default
  timeout->mPopupState = PopupBlocker::openAbused;

  // XXX: Does eIdleCallbackTimeout need clamping?
  if (aReason == Timeout::Reason::eTimeoutOrInterval ||
      aReason == Timeout::Reason::eIdleCallbackTimeout) {
    timeout->mNestingLevel =
        sNestingLevel < StaticPrefs::dom_clamp_timeout_nesting_level_AtStartup()
            ? sNestingLevel + 1
            : sNestingLevel;
  }

  // Now clamp the actual interval we will use for the timer based on
  TimeDuration realInterval = CalculateDelay(timeout);
  TimeStamp now = TimeStamp::Now();
  timeout->SetWhenOrTimeRemaining(now, realInterval);

  // If we're not suspended, then set the timer.
  if (!mWindow.IsSuspended()) {
    nsresult rv = MaybeSchedule(timeout->When(), now);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (gRunningTimeoutDepth == 0 &&
      PopupBlocker::GetPopupControlState() < PopupBlocker::openBlocked) {
    // This timeout is *not* set from another timeout and it's set
    // while popups are enabled. Propagate the state to the timeout if
    // its delay (interval) is equal to or less than what
    // "dom.disable_open_click_delay" is set to (in ms).

    // This is checking |interval|, not realInterval, on purpose,
    // because our lower bound for |realInterval| could be pretty high
    // in some cases.
    if (interval <= StaticPrefs::dom_disable_open_click_delay()) {
      timeout->mPopupState = PopupBlocker::GetPopupControlState();
    }
  }

  Timeouts::SortBy sort(mWindow.IsFrozen() ? Timeouts::SortBy::TimeRemaining
                                           : Timeouts::SortBy::TimeWhen);

  timeout->mTimeoutId = GetTimeoutId(aReason);
  mTimeouts.Insert(timeout, sort);

  *aReturn = timeout->mTimeoutId;

  MOZ_LOG(
      gTimeoutLog, LogLevel::Debug,
      ("Set%s(TimeoutManager=%p, timeout=%p, delay=%i, "
       "minimum=%f, throttling=%s, state=%s(%s), realInterval=%f) "
       "returned timeout ID %u, budget=%d\n",
       aIsInterval ? "Interval" : "Timeout", this, timeout.get(), interval,
       (CalculateDelay(timeout) - timeout->mInterval).ToMilliseconds(),
       mThrottleTimeouts ? "yes" : (mThrottleTimeoutsTimer ? "pending" : "no"),
       IsActive() ? "active" : "inactive",
       mWindow.IsBackgroundInternal() ? "background" : "foreground",
       realInterval.ToMilliseconds(), timeout->mTimeoutId,
       int(mExecutionBudget.ToMilliseconds())));

  return NS_OK;
}

// Make sure we clear it no matter which list it's in
void TimeoutManager::ClearTimeout(int32_t aTimerId, Timeout::Reason aReason) {
  if (ClearTimeoutInternal(aTimerId, aReason, false) ||
      mIdleTimeouts.IsEmpty()) {
    return;  // no need to check the other list if we cleared the timeout
  }
  ClearTimeoutInternal(aTimerId, aReason, true);
}

bool TimeoutManager::ClearTimeoutInternal(int32_t aTimerId,
                                          Timeout::Reason aReason,
                                          bool aIsIdle) {
  MOZ_ASSERT(aReason == Timeout::Reason::eTimeoutOrInterval ||
                 aReason == Timeout::Reason::eIdleCallbackTimeout,
             "This timeout reason doesn't support cancellation.");

  uint32_t timerId = (uint32_t)aTimerId;
  Timeouts& timeouts = aIsIdle ? mIdleTimeouts : mTimeouts;
  RefPtr<TimeoutExecutor>& executor = aIsIdle ? mIdleExecutor : mExecutor;
  bool deferredDeletion = false;

  Timeout* timeout = timeouts.GetTimeout(timerId, aReason);
  if (!timeout) {
    return false;
  }
  bool firstTimeout = timeout == timeouts.GetFirst();

  MOZ_LOG(gTimeoutLog, LogLevel::Debug,
          ("%s(TimeoutManager=%p, timeout=%p, ID=%u)\n",
           timeout->mReason == Timeout::Reason::eIdleCallbackTimeout
               ? "CancelIdleCallback"
           : timeout->mIsInterval ? "ClearInterval"
                                  : "ClearTimeout",
           this, timeout, timeout->mTimeoutId));

  if (timeout->mRunning) {
    /* We're running from inside the timeout. Mark this
       timeout for deferred deletion by the code in
       RunTimeout() */
    timeout->mIsInterval = false;
    deferredDeletion = true;
  } else {
    /* Delete the aTimeout from the pending aTimeout list */
    timeout->remove();
  }

  // We don't need to reschedule the executor if any of the following are true:
  //  * If the we weren't cancelling the first timeout, then the executor's
  //    state doesn't need to change.  It will only reflect the next soonest
  //    Timeout.
  //  * If we did cancel the first Timeout, but its currently running, then
  //    RunTimeout() will handle rescheduling the executor.
  //  * If the window has become suspended then we should not start executing
  //    Timeouts.
  if (!firstTimeout || deferredDeletion || mWindow.IsSuspended()) {
    return true;
  }

  // Stop the executor and restart it at the next soonest deadline.
  executor->Cancel();

  Timeout* nextTimeout = timeouts.GetFirst();
  if (nextTimeout) {
    if (aIsIdle) {
      MOZ_ALWAYS_SUCCEEDS(
          executor->MaybeSchedule(nextTimeout->When(), TimeDuration(0)));
    } else {
      MOZ_ALWAYS_SUCCEEDS(MaybeSchedule(nextTimeout->When()));
    }
  }
  return true;
}

void TimeoutManager::RunTimeout(const TimeStamp& aNow,
                                const TimeStamp& aTargetDeadline,
                                bool aProcessIdle) {
  MOZ_DIAGNOSTIC_ASSERT(!aNow.IsNull());
  MOZ_DIAGNOSTIC_ASSERT(!aTargetDeadline.IsNull());

  MOZ_ASSERT_IF(mWindow.IsFrozen(), mWindow.IsSuspended());
  if (mWindow.IsSuspended()) {
    return;
  }

  Timeouts& timeouts(aProcessIdle ? mIdleTimeouts : mTimeouts);

  // Limit the overall time spent in RunTimeout() to reduce jank.
  uint32_t totalTimeLimitMS =
      std::max(1u, StaticPrefs::dom_timeout_max_consecutive_callbacks_ms());
  const TimeDuration totalTimeLimit =
      TimeDuration::Min(TimeDuration::FromMilliseconds(totalTimeLimitMS),
                        TimeDuration::Max(TimeDuration(), mExecutionBudget));

  // Allow up to 25% of our total time budget to be used figuring out which
  // timers need to run.  This is the initial loop in this method.
  const TimeDuration initialTimeLimit =
      TimeDuration::FromMilliseconds(totalTimeLimit.ToMilliseconds() / 4);

  // Ammortize overhead from from calling TimeStamp::Now() in the initial
  // loop, though, by only checking for an elapsed limit every N timeouts.
  const uint32_t kNumTimersPerInitialElapsedCheck = 100;

  // Start measuring elapsed time immediately.  We won't potentially expire
  // the time budget until at least one Timeout has run, though.
  TimeStamp now(aNow);
  TimeStamp start = now;

  uint32_t firingId = CreateFiringId();
  auto guard = MakeScopeExit([&] { DestroyFiringId(firingId); });

  // Make sure that the window and the script context don't go away as
  // a result of running timeouts
  RefPtr<nsGlobalWindowInner> window(&mWindow);
  // Accessing members of mWindow here is safe, because the lifetime of
  // TimeoutManager is the same as the lifetime of the containing
  // nsGlobalWindow.

  // A native timer has gone off. See which of our timeouts need
  // servicing
  TimeStamp deadline;

  if (aTargetDeadline > now) {
    // The OS timer fired early (which can happen due to the timers
    // having lower precision than TimeStamp does).  Set |deadline| to
    // be the time when the OS timer *should* have fired so that any
    // timers that *should* have fired *will* be fired now.

    deadline = aTargetDeadline;
  } else {
    deadline = now;
  }

  TimeStamp nextDeadline;
  uint32_t numTimersToRun = 0;

  // The timeout list is kept in deadline order. Discover the latest timeout
  // whose deadline has expired. On some platforms, native timeout events fire
  // "early", but we handled that above by setting deadline to aTargetDeadline
  // if the timer fired early.  So we can stop walking if we get to timeouts
  // whose When() is greater than deadline, since once that happens we know
  // nothing past that point is expired.

  for (Timeout* timeout = timeouts.GetFirst(); timeout != nullptr;
       timeout = timeout->getNext()) {
    if (totalTimeLimit.IsZero() || timeout->When() > deadline) {
      nextDeadline = timeout->When();
      break;
    }

    if (IsInvalidFiringId(timeout->mFiringId)) {
      // Mark any timeouts that are on the list to be fired with the
      // firing depth so that we can reentrantly run timeouts
      timeout->mFiringId = firingId;

      numTimersToRun += 1;

      // Run only a limited number of timers based on the configured maximum.
      if (numTimersToRun % kNumTimersPerInitialElapsedCheck == 0) {
        now = TimeStamp::Now();
        TimeDuration elapsed(now - start);
        if (elapsed >= initialTimeLimit) {
          nextDeadline = timeout->When();
          break;
        }
      }
    }
  }
  if (aProcessIdle) {
    MOZ_LOG(
        gTimeoutLog, LogLevel::Debug,
        ("Running %u deferred timeouts on idle (TimeoutManager=%p), "
         "nextDeadline = %gms from now",
         numTimersToRun, this,
         nextDeadline.IsNull() ? 0.0 : (nextDeadline - now).ToMilliseconds()));
  }

  now = TimeStamp::Now();

  // Wherever we stopped in the timer list, schedule the executor to
  // run for the next unexpired deadline.  Note, this *must* be done
  // before we start executing any content script handlers.  If one
  // of them spins the event loop the executor must already be scheduled
  // in order for timeouts to fire properly.
  if (!nextDeadline.IsNull()) {
    // Note, we verified the window is not suspended at the top of
    // method and the window should not have been suspended while
    // executing the loop above since it doesn't call out to js.
    MOZ_DIAGNOSTIC_ASSERT(!mWindow.IsSuspended());
    if (aProcessIdle) {
      // We don't want to update timing budget for idle queue firings, and
      // all timeouts in the IdleTimeouts list have hit their deadlines,
      // and so should run as soon as possible.
      MOZ_ALWAYS_SUCCEEDS(
          mIdleExecutor->MaybeSchedule(nextDeadline, TimeDuration()));
    } else {
      MOZ_ALWAYS_SUCCEEDS(MaybeSchedule(nextDeadline, now));
    }
  }

  // Maybe the timeout that the event was fired for has been deleted
  // and there are no others timeouts with deadlines that make them
  // eligible for execution yet. Go away.
  if (!numTimersToRun) {
    return;
  }

  // Now we need to search the normal and tracking timer list at the same
  // time to run the timers in the scheduled order.

  // We stop iterating each list when we go past the last expired timeout from
  // that list that we have observed above.  That timeout will either be the
  // next item after the last timeout we looked at or nullptr if we have
  // exhausted the entire list while looking for the last expired timeout.
  {
    // Use a nested scope in order to make sure the strong references held while
    // iterating are freed after the loop.

    // The next timeout to run. This is used to advance the loop, but
    // we cannot set it until we've run the current timeout, since
    // running the current timeout might remove the immediate next
    // timeout.
    RefPtr<Timeout> next;

    for (RefPtr<Timeout> timeout = timeouts.GetFirst(); timeout != nullptr;
         timeout = next) {
      next = timeout->getNext();
      // We should only execute callbacks for the set of expired Timeout
      // objects we computed above.
      if (timeout->mFiringId != firingId) {
        // If the FiringId does not match, but is still valid, then this is
        // a Timeout for another RunTimeout() on the call stack (such as in
        // the case of nested event loops, for alert() or more likely XHR).
        // Just skip it.
        if (IsValidFiringId(timeout->mFiringId)) {
          MOZ_LOG(gTimeoutLog, LogLevel::Debug,
                  ("Skipping Run%s(TimeoutManager=%p, timeout=%p) since "
                   "firingId %d is valid (processing firingId %d)"
#ifdef DEBUG
                   " - FiringIndex %" PRId64 " (mLastFiringIndex %" PRId64 ")"
#endif
                   ,
                   timeout->mIsInterval ? "Interval" : "Timeout", this,
                   timeout.get(), timeout->mFiringId, firingId
#ifdef DEBUG
                   ,
                   timeout->mFiringIndex, mFiringIndex
#endif
                   ));
#ifdef DEBUG
          // The old FiringIndex assumed no recursion; recursion can cause
          // other timers to get fired "in the middle" of a sequence we've
          // already assigned firingindexes to.  Since we're not going to
          // run this timeout now, remove any FiringIndex that was already
          // set.

          // Since all timers that have FiringIndexes set *must* be ready
          // to run and have valid FiringIds, all of them will be 'skipped'
          // and reset if we recurse - we don't have to look through the
          // list past where we'll stop on the first InvalidFiringId.
          timeout->mFiringIndex = -1;
#endif
          continue;
        }

        // If, however, the FiringId is invalid then we have reached Timeout
        // objects beyond the list we calculated above.  This can happen
        // if the Timeout just beyond our last expired Timeout is cancelled
        // by one of the callbacks we've just executed.  In this case we
        // should just stop iterating.  We're done.
        else {
          break;
        }
      }

      MOZ_ASSERT_IF(mWindow.IsFrozen(), mWindow.IsSuspended());
      if (mWindow.IsSuspended()) {
        break;
      }

      // The timeout is on the list to run at this depth, go ahead and
      // process it.

      if (mIsLoading && !aProcessIdle) {
        // Any timeouts that would fire during a load will be deferred
        // until the load event occurs, but if there's an idle time,
        // they'll be run before the load event.
        timeout->remove();
        // MOZ_RELEASE_ASSERT(timeout->When() <= (TimeStamp::Now()));
        mIdleTimeouts.InsertBack(timeout);
        if (MOZ_LOG_TEST(gTimeoutLog, LogLevel::Debug)) {
          uint32_t num = 0;
          for (Timeout* t = mIdleTimeouts.GetFirst(); t != nullptr;
               t = t->getNext()) {
            num++;
          }
          MOZ_LOG(
              gTimeoutLog, LogLevel::Debug,
              ("Deferring Run%s(TimeoutManager=%p, timeout=%p (%gms in the "
               "past)) (%u deferred)",
               timeout->mIsInterval ? "Interval" : "Timeout", this,
               timeout.get(), (now - timeout->When()).ToMilliseconds(), num));
        }
        MOZ_ALWAYS_SUCCEEDS(mIdleExecutor->MaybeSchedule(now, TimeDuration()));
      } else {
        // Record the first time we try to fire a timeout, and ensure that
        // all actual firings occur in that order.  This ensures that we
        // retain compliance with the spec language
        // (https://html.spec.whatwg.org/#dom-settimeout) specifically items
        // 15 ("If method context is a Window object, wait until the Document
        // associated with method context has been fully active for a further
        // timeout milliseconds (not necessarily consecutively)") and item 16
        // ("Wait until any invocations of this algorithm that had the same
        // method context, that started before this one, and whose timeout is
        // equal to or less than this one's, have completed.").
#ifdef DEBUG
        if (timeout->mFiringIndex == -1) {
          timeout->mFiringIndex = mFiringIndex++;
        }
#endif

        // Get the script context (a strong ref to prevent it going away)
        // for this timeout and ensure the script language is enabled.
        nsCOMPtr<nsIScriptContext> scx = mWindow.GetContextInternal();

        if (!scx) {
          // No context means this window was closed or never properly
          // initialized for this language.  This timer will never fire
          // so just remove it.
          timeout->remove();
          continue;
        }

#ifdef DEBUG
        if (timeout->mFiringIndex <= mLastFiringIndex) {
          MOZ_LOG(gTimeoutLog, LogLevel::Debug,
                  ("Incorrect firing index for Run%s(TimeoutManager=%p, "
                   "timeout=%p) with "
                   "firingId %d - FiringIndex %" PRId64
                   " (mLastFiringIndex %" PRId64 ")",
                   timeout->mIsInterval ? "Interval" : "Timeout", this,
                   timeout.get(), timeout->mFiringId, timeout->mFiringIndex,
                   mFiringIndex));
        }
        MOZ_ASSERT(timeout->mFiringIndex > mLastFiringIndex);
        mLastFiringIndex = timeout->mFiringIndex;
#endif
        // This timeout is good to run.
        bool timeout_was_cleared = window->RunTimeoutHandler(timeout, scx);
        MOZ_LOG(gTimeoutLog, LogLevel::Debug,
                ("Run%s(TimeoutManager=%p, timeout=%p) returned %d\n",
                 timeout->mIsInterval ? "Interval" : "Timeout", this,
                 timeout.get(), !!timeout_was_cleared));

        if (timeout_was_cleared) {
          // Make sure we're not holding any Timeout objects alive.
          next = nullptr;

          // Since ClearAllTimeouts() was called the lists should be empty.
          MOZ_DIAGNOSTIC_ASSERT(!HasTimeouts());

          return;
        }

        // If we need to reschedule a setInterval() the delay should be
        // calculated based on when its callback started to execute.  So
        // save off the last time before updating our "now" timestamp to
        // account for its callback execution time.
        TimeStamp lastCallbackTime = now;
        now = TimeStamp::Now();

        // If we have a regular interval timer, we re-schedule the
        // timeout, accounting for clock drift.
        bool needsReinsertion =
            RescheduleTimeout(timeout, lastCallbackTime, now);

        // Running a timeout can cause another timeout to be deleted, so
        // we need to reset the pointer to the following timeout.
        next = timeout->getNext();

        timeout->remove();

        if (needsReinsertion) {
          // Insert interval timeout onto the corresponding list sorted in
          // deadline order. AddRefs timeout.
          // Always re-insert into the normal time queue!
          mTimeouts.Insert(timeout, mWindow.IsFrozen()
                                        ? Timeouts::SortBy::TimeRemaining
                                        : Timeouts::SortBy::TimeWhen);
        }
      }
      // Check to see if we have run out of time to execute timeout handlers.
      // If we've exceeded our time budget then terminate the loop immediately.
      TimeDuration elapsed = now - start;
      if (elapsed >= totalTimeLimit) {
        // We ran out of time.  Make sure to schedule the executor to
        // run immediately for the next timer, if it exists.  Its possible,
        // however, that the last timeout handler suspended the window.  If
        // that happened then we must skip this step.
        if (!mWindow.IsSuspended()) {
          if (next) {
            if (aProcessIdle) {
              // We don't want to update timing budget for idle queue firings,
              // and all timeouts in the IdleTimeouts list have hit their
              // deadlines, and so should run as soon as possible.

              // Shouldn't need cancelling since it never waits
              MOZ_ALWAYS_SUCCEEDS(
                  mIdleExecutor->MaybeSchedule(next->When(), TimeDuration()));
            } else {
              // If we ran out of execution budget we need to force a
              // reschedule. By cancelling the executor we will not run
              // immediately, but instead reschedule to the minimum
              // scheduling delay.
              if (mExecutionBudget < TimeDuration()) {
                mExecutor->Cancel();
              }

              MOZ_ALWAYS_SUCCEEDS(MaybeSchedule(next->When(), now));
            }
          }
        }
        break;
      }
    }
  }
}

bool TimeoutManager::RescheduleTimeout(Timeout* aTimeout,
                                       const TimeStamp& aLastCallbackTime,
                                       const TimeStamp& aCurrentNow) {
  MOZ_DIAGNOSTIC_ASSERT(aLastCallbackTime <= aCurrentNow);

  if (!aTimeout->mIsInterval) {
    return false;
  }

  // Automatically increase the nesting level when a setInterval()
  // is rescheduled just as if it was using a chained setTimeout().
  if (aTimeout->mNestingLevel <
      StaticPrefs::dom_clamp_timeout_nesting_level_AtStartup()) {
    aTimeout->mNestingLevel += 1;
  }

  // Compute time to next timeout for interval timer.
  // Make sure nextInterval is at least CalculateDelay().
  TimeDuration nextInterval = CalculateDelay(aTimeout);

  TimeStamp firingTime = aLastCallbackTime + nextInterval;
  TimeDuration delay = firingTime - aCurrentNow;

#ifdef DEBUG
  aTimeout->mFiringIndex = -1;
#endif
  // And make sure delay is nonnegative; that might happen if the timer
  // thread is firing our timers somewhat early or if they're taking a long
  // time to run the callback.
  if (delay < TimeDuration(0)) {
    delay = TimeDuration(0);
  }

  aTimeout->SetWhenOrTimeRemaining(aCurrentNow, delay);

  if (mWindow.IsSuspended()) {
    return true;
  }

  nsresult rv = MaybeSchedule(aTimeout->When(), aCurrentNow);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

void TimeoutManager::ClearAllTimeouts() {
  bool seenRunningTimeout = false;

  MOZ_LOG(gTimeoutLog, LogLevel::Debug,
          ("ClearAllTimeouts(TimeoutManager=%p)\n", this));

  if (mThrottleTimeoutsTimer) {
    mThrottleTimeoutsTimer->Cancel();
    mThrottleTimeoutsTimer = nullptr;
  }

  mExecutor->Cancel();
  mIdleExecutor->Cancel();

  ForEachUnorderedTimeout([&](Timeout* aTimeout) {
    /* If RunTimeout() is higher up on the stack for this
       window, e.g. as a result of document.write from a timeout,
       then we need to reset the list insertion point for
       newly-created timeouts in case the user adds a timeout,
       before we pop the stack back to RunTimeout. */
    if (mRunningTimeout == aTimeout) {
      seenRunningTimeout = true;
    }

    // Set timeout->mCleared to true to indicate that the timeout was
    // cleared and taken out of the list of timeouts
    aTimeout->mCleared = true;
  });

  // Clear out our lists
  mTimeouts.Clear();
  mIdleTimeouts.Clear();
}

void TimeoutManager::Timeouts::Insert(Timeout* aTimeout, SortBy aSortBy) {
  // Start at mLastTimeout and go backwards.  Stop if we see a Timeout with a
  // valid FiringId since those timers are currently being processed by
  // RunTimeout.  This optimizes for the common case of insertion at the end.
  Timeout* prevSibling;
  for (prevSibling = GetLast();
       prevSibling &&
       // This condition needs to match the one in SetTimeoutOrInterval that
       // determines whether to set When() or TimeRemaining().
       (aSortBy == SortBy::TimeRemaining
            ? prevSibling->TimeRemaining() > aTimeout->TimeRemaining()
            : prevSibling->When() > aTimeout->When()) &&
       // Check the firing ID last since it will evaluate true in the vast
       // majority of cases.
       mManager.IsInvalidFiringId(prevSibling->mFiringId);
       prevSibling = prevSibling->getPrevious()) {
    /* Do nothing; just searching */
  }

  // Now link in aTimeout after prevSibling.
  if (prevSibling) {
    aTimeout->SetTimeoutContainer(mTimeouts);
    prevSibling->setNext(aTimeout);
  } else {
    InsertFront(aTimeout);
  }

  aTimeout->mFiringId = InvalidFiringId;
}

Timeout* TimeoutManager::BeginRunningTimeout(Timeout* aTimeout) {
  Timeout* currentTimeout = mRunningTimeout;
  mRunningTimeout = aTimeout;
  ++gRunningTimeoutDepth;

  RecordExecution(currentTimeout, aTimeout);
  return currentTimeout;
}

void TimeoutManager::EndRunningTimeout(Timeout* aTimeout) {
  --gRunningTimeoutDepth;

  RecordExecution(mRunningTimeout, aTimeout);
  mRunningTimeout = aTimeout;
}

void TimeoutManager::UnmarkGrayTimers() {
  ForEachUnorderedTimeout([](Timeout* aTimeout) {
    if (aTimeout->mScriptHandler) {
      aTimeout->mScriptHandler->MarkForCC();
    }
  });
}

void TimeoutManager::Suspend() {
  MOZ_LOG(gTimeoutLog, LogLevel::Debug, ("Suspend(TimeoutManager=%p)\n", this));

  if (mThrottleTimeoutsTimer) {
    mThrottleTimeoutsTimer->Cancel();
    mThrottleTimeoutsTimer = nullptr;
  }

  mExecutor->Cancel();
  mIdleExecutor->Cancel();
}

void TimeoutManager::Resume() {
  MOZ_LOG(gTimeoutLog, LogLevel::Debug, ("Resume(TimeoutManager=%p)\n", this));

  // When Suspend() has been called after IsDocumentLoaded(), but the
  // throttle tracking timer never managed to fire, start the timer
  // again.
  if (mWindow.IsDocumentLoaded() && !mThrottleTimeouts) {
    MaybeStartThrottleTimeout();
  }

  Timeout* nextTimeout = mTimeouts.GetFirst();
  if (nextTimeout) {
    MOZ_ALWAYS_SUCCEEDS(MaybeSchedule(nextTimeout->When()));
  }
  nextTimeout = mIdleTimeouts.GetFirst();
  if (nextTimeout) {
    MOZ_ALWAYS_SUCCEEDS(
        mIdleExecutor->MaybeSchedule(nextTimeout->When(), TimeDuration()));
  }
}

void TimeoutManager::Freeze() {
  MOZ_LOG(gTimeoutLog, LogLevel::Debug, ("Freeze(TimeoutManager=%p)\n", this));

  // When freezing, preemptively move timeouts from the idle timeout queue to
  // the normal queue. This way they get scheduled automatically when we thaw.
  // We don't need to cancel the idle executor here, since that is done in
  // Suspend.
  size_t num = 0;
  while (RefPtr<Timeout> timeout = mIdleTimeouts.GetLast()) {
    num++;
    timeout->remove();
    mTimeouts.InsertFront(timeout);
  }

  MOZ_LOG(gTimeoutLog, LogLevel::Debug,
          ("%p: Moved %zu (frozen) timeouts from Idle to active", this, num));

  TimeStamp now = TimeStamp::Now();
  ForEachUnorderedTimeout([&](Timeout* aTimeout) {
    // Save the current remaining time for this timeout.  We will
    // re-apply it when the window is Thaw()'d.  This effectively
    // shifts timers to the right as if time does not pass while
    // the window is frozen.
    TimeDuration delta(0);
    if (aTimeout->When() > now) {
      delta = aTimeout->When() - now;
    }
    aTimeout->SetWhenOrTimeRemaining(now, delta);
    MOZ_DIAGNOSTIC_ASSERT(aTimeout->TimeRemaining() == delta);
  });
}

void TimeoutManager::Thaw() {
  MOZ_LOG(gTimeoutLog, LogLevel::Debug, ("Thaw(TimeoutManager=%p)\n", this));

  TimeStamp now = TimeStamp::Now();

  ForEachUnorderedTimeout([&](Timeout* aTimeout) {
    // Set When() back to the time when the timer is supposed to fire.
    aTimeout->SetWhenOrTimeRemaining(now, aTimeout->TimeRemaining());
    MOZ_DIAGNOSTIC_ASSERT(!aTimeout->When().IsNull());
  });
}

void TimeoutManager::UpdateBackgroundState() {
  mExecutionBudget = GetMaxBudget(mWindow.IsBackgroundInternal());

  // When the window moves to the background or foreground we should
  // reschedule the TimeoutExecutor in case the MinSchedulingDelay()
  // changed.  Only do this if the window is not suspended and we
  // actually have a timeout.
  if (!mWindow.IsSuspended()) {
    Timeout* nextTimeout = mTimeouts.GetFirst();
    if (nextTimeout) {
      mExecutor->Cancel();
      MOZ_ALWAYS_SUCCEEDS(MaybeSchedule(nextTimeout->When()));
    }
    // the Idle queue should all be past their firing time, so there we just
    // need to restart the queue

    // XXX May not be needed if we don't stop the idle queue, as
    // MinSchedulingDelay isn't relevant here
    nextTimeout = mIdleTimeouts.GetFirst();
    if (nextTimeout) {
      mIdleExecutor->Cancel();
      MOZ_ALWAYS_SUCCEEDS(
          mIdleExecutor->MaybeSchedule(nextTimeout->When(), TimeDuration()));
    }
  }
}

namespace {

class ThrottleTimeoutsCallback final : public nsITimerCallback,
                                       public nsINamed {
 public:
  explicit ThrottleTimeoutsCallback(nsGlobalWindowInner* aWindow)
      : mWindow(aWindow) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  NS_IMETHOD GetName(nsACString& aName) override {
    aName.AssignLiteral("ThrottleTimeoutsCallback");
    return NS_OK;
  }

 private:
  ~ThrottleTimeoutsCallback() = default;

 private:
  // The strong reference here keeps the Window and hence the TimeoutManager
  // object itself alive.
  RefPtr<nsGlobalWindowInner> mWindow;
};

NS_IMPL_ISUPPORTS(ThrottleTimeoutsCallback, nsITimerCallback, nsINamed)

NS_IMETHODIMP
ThrottleTimeoutsCallback::Notify(nsITimer* aTimer) {
  mWindow->TimeoutManager().StartThrottlingTimeouts();
  mWindow = nullptr;
  return NS_OK;
}

}  // namespace

bool TimeoutManager::BudgetThrottlingEnabled(bool aIsBackground) const {
  // A window can be throttled using budget if
  // * It isn't active
  // * If it isn't using WebRTC
  // * If it hasn't got open WebSockets
  // * If it hasn't got active IndexedDB databases

  // Note that we allow both foreground and background to be
  // considered for budget throttling. What determines if they are if
  // budget throttling is enabled is the max budget.
  if ((aIsBackground
           ? StaticPrefs::dom_timeout_background_throttling_max_budget()
           : StaticPrefs::dom_timeout_foreground_throttling_max_budget()) < 0) {
    return false;
  }

  if (!mBudgetThrottleTimeouts || IsActive()) {
    return false;
  }

  // Check if there are any active IndexedDB databases
  if (mWindow.HasActiveIndexedDBDatabases()) {
    return false;
  }

  // Check if we have active PeerConnection
  if (mWindow.HasActivePeerConnections()) {
    return false;
  }

  if (mWindow.HasOpenWebSockets()) {
    return false;
  }

  return true;
}

void TimeoutManager::StartThrottlingTimeouts() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mThrottleTimeoutsTimer);

  MOZ_LOG(gTimeoutLog, LogLevel::Debug,
          ("TimeoutManager %p started to throttle tracking timeouts\n", this));

  MOZ_DIAGNOSTIC_ASSERT(!mThrottleTimeouts);
  mThrottleTimeouts = true;
  mThrottleTrackingTimeouts = true;
  mBudgetThrottleTimeouts =
      StaticPrefs::dom_timeout_enable_budget_timer_throttling();
  mThrottleTimeoutsTimer = nullptr;
}

void TimeoutManager::OnDocumentLoaded() {
  // The load event may be firing again if we're coming back to the page by
  // navigating through the session history, so we need to ensure to only call
  // this when mThrottleTimeouts hasn't been set yet.
  if (!mThrottleTimeouts) {
    MaybeStartThrottleTimeout();
  }
}

void TimeoutManager::MaybeStartThrottleTimeout() {
  if (StaticPrefs::dom_timeout_throttling_delay() <= 0 || mWindow.IsDying() ||
      mWindow.IsSuspended()) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(!mThrottleTimeouts);

  MOZ_LOG(gTimeoutLog, LogLevel::Debug,
          ("TimeoutManager %p delaying tracking timeout throttling by %dms\n",
           this, StaticPrefs::dom_timeout_throttling_delay()));

  nsCOMPtr<nsITimerCallback> callback = new ThrottleTimeoutsCallback(&mWindow);

  NS_NewTimerWithCallback(getter_AddRefs(mThrottleTimeoutsTimer), callback,
                          StaticPrefs::dom_timeout_throttling_delay(),
                          nsITimer::TYPE_ONE_SHOT, EventTarget());
}

void TimeoutManager::BeginSyncOperation() {
  // If we're beginning a sync operation, the currently running
  // timeout will be put on hold. To not get into an inconsistent
  // state, where the currently running timeout appears to take time
  // equivalent to the period of us spinning up a new event loop,
  // record what we have and stop recording until we reach
  // EndSyncOperation.
  RecordExecution(mRunningTimeout, nullptr);
}

void TimeoutManager::EndSyncOperation() {
  // If we're running a timeout, restart the measurement from here.
  RecordExecution(nullptr, mRunningTimeout);
}

nsIEventTarget* TimeoutManager::EventTarget() {
  return mWindow.GetBrowsingContextGroup()->GetTimerEventQueue();
}
