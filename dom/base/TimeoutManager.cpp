/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimeoutManager.h"
#include "nsGlobalWindow.h"
#include "mozilla/ThrottledEventQueue.h"
#include "mozilla/TimeStamp.h"
#include "nsITimeoutHandler.h"
#include "mozilla/dom/TabGroup.h"
#include "OrderedTimeoutIterator.h"

using namespace mozilla;
using namespace mozilla::dom;

static int32_t              gRunningTimeoutDepth       = 0;

// The default shortest interval/timeout we permit
#define DEFAULT_MIN_TIMEOUT_VALUE 4 // 4ms
#define DEFAULT_MIN_BACKGROUND_TIMEOUT_VALUE 1000 // 1000ms
static int32_t gMinTimeoutValue;
static int32_t gMinBackgroundTimeoutValue;
int32_t
TimeoutManager::DOMMinTimeoutValue() const {
  // First apply any back pressure delay that might be in effect.
  int32_t value = std::max(mBackPressureDelayMS, 0);
  // Don't use the background timeout value when there are audio contexts
  // present, so that background audio can keep running smoothly. (bug 1181073)
  bool isBackground = !mWindow.AsInner()->HasAudioContexts() &&
    mWindow.IsBackgroundInternal();
  return
    std::max(isBackground ? gMinBackgroundTimeoutValue : gMinTimeoutValue, value);
}

#define TRACKING_SEPARATE_TIMEOUT_BUCKETING_STRATEGY 0 // Consider all timeouts coming from tracking scripts as tracking
// These strategies are useful for testing.
#define ALL_NORMAL_TIMEOUT_BUCKETING_STRATEGY        1 // Consider all timeouts as normal
#define ALTERNATE_TIMEOUT_BUCKETING_STRATEGY         2 // Put every other timeout in the list of tracking timeouts
#define RANDOM_TIMEOUT_BUCKETING_STRATEGY            3 // Put timeouts into either the normal or tracking timeouts list randomly
static int32_t gTimeoutBucketingStrategy = 0;

// The number of nested timeouts before we start clamping. HTML5 says 1, WebKit
// uses 5.
#define DOM_CLAMP_TIMEOUT_NESTING_LEVEL 5

// The longest interval (as PRIntervalTime) we permit, or that our
// timer code can handle, really. See DELAY_INTERVAL_LIMIT in
// nsTimerImpl.h for details.
#define DOM_MAX_TIMEOUT_VALUE    DELAY_INTERVAL_LIMIT

uint32_t TimeoutManager::sNestingLevel = 0;

namespace {

// The number of queued runnables within the TabGroup ThrottledEventQueue
// at which to begin applying back pressure to the window.
const uint32_t kThrottledEventQueueBackPressure = 5000;

// The amount of delay to apply to timers when back pressure is triggered.
// As the length of the ThrottledEventQueue grows delay is increased.  The
// delay is scaled such that every kThrottledEventQueueBackPressure runnables
// in the queue equates to an additional kBackPressureDelayMS.
const double kBackPressureDelayMS = 500;

// This defines a limit for how much the delay must drop before we actually
// reduce back pressure throttle amount.  This makes the throttle delay
// a bit "sticky" once we enter back pressure.
const double kBackPressureDelayReductionThresholdMS = 400;

// The minimum delay we can reduce back pressure to before we just floor
// the value back to zero.  This allows us to ensure that we can exit
// back pressure event if there are always a small number of runnables
// queued up.
const double kBackPressureDelayMinimumMS = 100;

// Convert a ThrottledEventQueue length to a timer delay in milliseconds.
// This will return a value between 0 and INT32_MAX.
int32_t
CalculateNewBackPressureDelayMS(uint32_t aBacklogDepth)
{
  double multiplier = static_cast<double>(aBacklogDepth) /
                      static_cast<double>(kThrottledEventQueueBackPressure);
  double value = kBackPressureDelayMS * multiplier;
  // Avoid overflow
  if (value > INT32_MAX) {
    value = INT32_MAX;
  }

  // Once we get close to an empty queue just floor the delay back to zero.
  // We want to ensure we don't get stuck in a condition where there is a
  // small amount of delay remaining due to an active, but reasonable, queue.
  else if (value < kBackPressureDelayMinimumMS) {
    value = 0;
  }
  return static_cast<int32_t>(value);
}

} // anonymous namespace

TimeoutManager::TimeoutManager(nsGlobalWindow& aWindow)
  : mWindow(aWindow),
    mTimeoutIdCounter(1),
    mTimeoutFiringDepth(0),
    mRunningTimeout(nullptr),
    mIdleCallbackTimeoutCounter(1),
    mBackPressureDelayMS(0)
{
  MOZ_DIAGNOSTIC_ASSERT(aWindow.IsInnerWindow());
}

/* static */
void
TimeoutManager::Initialize()
{
  Preferences::AddIntVarCache(&gMinTimeoutValue,
                              "dom.min_timeout_value",
                              DEFAULT_MIN_TIMEOUT_VALUE);
  Preferences::AddIntVarCache(&gMinBackgroundTimeoutValue,
                              "dom.min_background_timeout_value",
                              DEFAULT_MIN_BACKGROUND_TIMEOUT_VALUE);
  Preferences::AddIntVarCache(&gTimeoutBucketingStrategy,
                              "dom.timeout_bucketing_strategy",
                              TRACKING_SEPARATE_TIMEOUT_BUCKETING_STRATEGY);
}

uint32_t
TimeoutManager::GetTimeoutId(Timeout::Reason aReason)
{
  switch (aReason) {
    case Timeout::Reason::eIdleCallbackTimeout:
      return ++mIdleCallbackTimeoutCounter;
    case Timeout::Reason::eTimeoutOrInterval:
    default:
      return ++mTimeoutIdCounter;
  }
}

nsresult
TimeoutManager::SetTimeout(nsITimeoutHandler* aHandler,
                           int32_t interval, bool aIsInterval,
                           Timeout::Reason aReason, int32_t* aReturn)
{
  // If we don't have a document (we could have been unloaded since
  // the call to setTimeout was made), do nothing.
  nsCOMPtr<nsIDocument> doc = mWindow.GetExtantDoc();
  if (!doc) {
    return NS_OK;
  }

  // Disallow negative intervals.  If aIsInterval also disallow 0,
  // because we use that as a "don't repeat" flag.
  interval = std::max(aIsInterval ? 1 : 0, interval);

  // Make sure we don't proceed with an interval larger than our timer
  // code can handle. (Note: we already forced |interval| to be non-negative,
  // so the uint32_t cast (to avoid compiler warnings) is ok.)
  uint32_t maxTimeoutMs = PR_IntervalToMilliseconds(DOM_MAX_TIMEOUT_VALUE);
  if (static_cast<uint32_t>(interval) > maxTimeoutMs) {
    interval = maxTimeoutMs;
  }

  RefPtr<Timeout> timeout = new Timeout();
  timeout->mIsInterval = aIsInterval;
  timeout->mInterval = interval;
  timeout->mScriptHandler = aHandler;
  timeout->mReason = aReason;

  // Now clamp the actual interval we will use for the timer based on
  uint32_t nestingLevel = sNestingLevel + 1;
  uint32_t realInterval = interval;
  if (aIsInterval || nestingLevel >= DOM_CLAMP_TIMEOUT_NESTING_LEVEL ||
      mBackPressureDelayMS > 0 || mWindow.IsBackgroundInternal()) {
    // Don't allow timeouts less than DOMMinTimeoutValue() from
    // now...
    realInterval = std::max(realInterval, uint32_t(DOMMinTimeoutValue()));
  }

  TimeDuration delta = TimeDuration::FromMilliseconds(realInterval);

  if (mWindow.IsFrozen()) {
    // If we are frozen simply set timeout->mTimeRemaining to be the
    // "time remaining" in the timeout (i.e., the interval itself).  This
    // will be used to create a new mWhen time when the window is thawed.
    // The end effect is that time does not appear to pass for frozen windows.
    timeout->mTimeRemaining = delta;
  } else {
    // Since we are not frozen we must set a precise mWhen target wakeup
    // time.  Even if we are suspended we want to use this target time so
    // that it appears time passes while suspended.
    timeout->mWhen = TimeStamp::Now() + delta;
  }

  // If we're not suspended, then set the timer.
  if (!mWindow.IsSuspended()) {
    MOZ_ASSERT(!timeout->mWhen.IsNull());

    nsresult rv;
    timeout->mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    RefPtr<Timeout> copy = timeout;

    rv = timeout->InitTimer(mWindow.EventTargetFor(TaskCategory::Timer),
                            realInterval);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // The timeout is now also held in the timer's closure.
    Unused << copy.forget();
  }

  timeout->mWindow = &mWindow;

  if (!aIsInterval) {
    timeout->mNestingLevel = nestingLevel;
  }

  // No popups from timeouts by default
  timeout->mPopupState = openAbused;

  if (gRunningTimeoutDepth == 0 &&
      mWindow.GetPopupControlState() < openAbused) {
    // This timeout is *not* set from another timeout and it's set
    // while popups are enabled. Propagate the state to the timeout if
    // its delay (interval) is equal to or less than what
    // "dom.disable_open_click_delay" is set to (in ms).

    int32_t delay =
      Preferences::GetInt("dom.disable_open_click_delay");

    // This is checking |interval|, not realInterval, on purpose,
    // because our lower bound for |realInterval| could be pretty high
    // in some cases.
    if (interval <= delay) {
      timeout->mPopupState = mWindow.GetPopupControlState();
    }
  }

  bool isTracking = false;
  switch (gTimeoutBucketingStrategy) {
  default:
  case TRACKING_SEPARATE_TIMEOUT_BUCKETING_STRATEGY: {
    const char* filename = nullptr;
    uint32_t dummyLine = 0, dummyColumn = 0;
    aHandler->GetLocation(&filename, &dummyLine, &dummyColumn);
    isTracking = doc->IsScriptTracking(nsDependentCString(filename));
    break;
  }
  case ALL_NORMAL_TIMEOUT_BUCKETING_STRATEGY:
    // isTracking is already false!
    break;
  case ALTERNATE_TIMEOUT_BUCKETING_STRATEGY:
    isTracking = (mTimeoutIdCounter % 2) == 0;
    break;
  case RANDOM_TIMEOUT_BUCKETING_STRATEGY:
    isTracking = (rand() % 2) == 0;
    break;
  }

  Timeouts::SortBy sort(mWindow.IsFrozen() ? Timeouts::SortBy::TimeRemaining
                                           : Timeouts::SortBy::TimeWhen);
  if (isTracking) {
    mTrackingTimeouts.Insert(timeout, sort);
  } else {
    mNormalTimeouts.Insert(timeout, sort);
  }

  timeout->mTimeoutId = GetTimeoutId(aReason);
  *aReturn = timeout->mTimeoutId;

  return NS_OK;
}

void
TimeoutManager::ClearTimeout(int32_t aTimerId, Timeout::Reason aReason)
{
  uint32_t timerId = (uint32_t)aTimerId;

  ForEachUnorderedTimeoutAbortable([&](Timeout* aTimeout) {
    if (aTimeout->mTimeoutId == timerId && aTimeout->mReason == aReason) {
      if (aTimeout->mRunning) {
        /* We're running from inside the aTimeout. Mark this
           aTimeout for deferred deletion by the code in
           RunTimeout() */
        aTimeout->mIsInterval = false;
      }
      else {
        /* Delete the aTimeout from the pending aTimeout list */
        aTimeout->remove();

        if (aTimeout->mTimer) {
          aTimeout->mTimer->Cancel();
          aTimeout->mTimer = nullptr;
          aTimeout->Release();
        }
        aTimeout->Release();
      }
      return true; // abort!
    }
    return false;
  });
}

void
TimeoutManager::RunTimeout(Timeout* aTimeout)
{
  if (mWindow.IsSuspended()) {
    return;
  }

  NS_ASSERTION(!mWindow.IsFrozen(), "Timeout running on a window in the bfcache!");

  Timeout* last_expired_normal_timeout = nullptr;
  Timeout* last_expired_tracking_timeout = nullptr;
  bool     last_expired_timeout_is_normal = false;
  Timeout* last_normal_insertion_point = nullptr;
  Timeout* last_tracking_insertion_point = nullptr;
  uint32_t firingDepth = mTimeoutFiringDepth + 1;

  // Make sure that the window and the script context don't go away as
  // a result of running timeouts
  nsCOMPtr<nsIScriptGlobalObject> windowKungFuDeathGrip(&mWindow);
  // Silence the static analysis error about windowKungFuDeathGrip.  Accessing
  // members of mWindow here is safe, because the lifetime of TimeoutManager is
  // the same as the lifetime of the containing nsGlobalWindow.
  Unused << windowKungFuDeathGrip;

  // A native timer has gone off. See which of our timeouts need
  // servicing
  TimeStamp now = TimeStamp::Now();
  TimeStamp deadline;

  if (aTimeout && aTimeout->mWhen > now) {
    // The OS timer fired early (which can happen due to the timers
    // having lower precision than TimeStamp does).  Set |deadline| to
    // be the time when the OS timer *should* have fired so that any
    // timers that *should* have fired before aTimeout *will* be fired
    // now.

    deadline = aTimeout->mWhen;
  } else {
    deadline = now;
  }

  // The timeout list is kept in deadline order. Discover the latest timeout
  // whose deadline has expired. On some platforms, native timeout events fire
  // "early", but we handled that above by setting deadline to aTimeout->mWhen
  // if the timer fired early.  So we can stop walking if we get to timeouts
  // whose mWhen is greater than deadline, since once that happens we know
  // nothing past that point is expired.
  {
    // Use a nested scope in order to make sure the strong references held by
    // the iterator are freed after the loop.
    OrderedTimeoutIterator expiredIter(mNormalTimeouts,
                                       mTrackingTimeouts,
                                       nullptr,
                                       nullptr);
    while (true) {
      Timeout* timeout = expiredIter.Next();
      if (!timeout || timeout->mWhen > deadline) {
        break;
      }

      if (timeout->mFiringDepth == 0) {
        // Mark any timeouts that are on the list to be fired with the
        // firing depth so that we can reentrantly run timeouts
        timeout->mFiringDepth = firingDepth;
        last_expired_timeout_is_normal = expiredIter.PickedNormalIter();
        if (last_expired_timeout_is_normal) {
          last_expired_normal_timeout = timeout;
        } else {
          last_expired_tracking_timeout = timeout;
        }

        // Run available timers until we see our target timer.  After
        // that, however, stop coalescing timers so we can yield the
        // main thread.  Further timers that are ready will get picked
        // up by their own nsITimer runnables when they execute.
        //
        // For chrome windows, however, we do coalesce all timers and
        // do not yield the main thread.  This is partly because we
        // trust chrome windows not to misbehave and partly because a
        // number of browser chrome tests have races that depend on this
        // coalescing.
        if (timeout == aTimeout && !mWindow.IsChromeWindow()) {
          break;
        }
      }

      expiredIter.UpdateIterator();
    }
  }

  // Maybe the timeout that the event was fired for has been deleted
  // and there are no others timeouts with deadlines that make them
  // eligible for execution yet. Go away.
  if (!last_expired_normal_timeout && !last_expired_tracking_timeout) {
    return;
  }

  // Insert a dummy timeout into the list of timeouts between the
  // portion of the list that we are about to process now and those
  // timeouts that will be processed in a future call to
  // win_run_timeout(). This dummy timeout serves as the head of the
  // list for any timeouts inserted as a result of running a timeout.
  RefPtr<Timeout> dummy_normal_timeout = new Timeout();
  dummy_normal_timeout->mFiringDepth = firingDepth;
  dummy_normal_timeout->mWhen = now;
  if (last_expired_timeout_is_normal) {
    last_expired_normal_timeout->setNext(dummy_normal_timeout);
  }

  RefPtr<Timeout> dummy_tracking_timeout = new Timeout();
  dummy_tracking_timeout->mFiringDepth = firingDepth;
  dummy_tracking_timeout->mWhen = now;
  if (!last_expired_timeout_is_normal) {
    last_expired_tracking_timeout->setNext(dummy_tracking_timeout);
  }

  RefPtr<Timeout> timeoutExtraRef1(dummy_normal_timeout);
  RefPtr<Timeout> timeoutExtraRef2(dummy_tracking_timeout);

  // Now we need to search the normal and tracking timer list at the same
  // time to run the timers in the scheduled order.

  last_normal_insertion_point = mNormalTimeouts.InsertionPoint();
  if (last_expired_timeout_is_normal) {
    // If we ever start setting insertion point to a non-dummy timeout, the logic
    // in ResetTimersForThrottleReduction will need to change.
    mNormalTimeouts.SetInsertionPoint(dummy_normal_timeout);
  }

  last_tracking_insertion_point = mTrackingTimeouts.InsertionPoint();
  if (!last_expired_timeout_is_normal) {
    // If we ever start setting mTrackingTimeoutInsertionPoint to a non-dummy timeout,
    // the logic in ResetTimersForThrottleReduction will need to change.
    mTrackingTimeouts.SetInsertionPoint(dummy_tracking_timeout);
  }

  // We stop iterating each list when we go past the last expired timeout from
  // that list that we have observed above.  That timeout will either be the
  // dummy timeout for the list that the last expired timeout came from, or it
  // will be the next item after the last timeout we looked at (or nullptr if
  // we have exhausted the entire list while looking for the last expired
  // timeout).
  {
    // Use a nested scope in order to make sure the strong references held by
    // the iterator are freed after the loop.
    OrderedTimeoutIterator runIter(mNormalTimeouts,
                                   mTrackingTimeouts,
                                   last_expired_normal_timeout ?
                                     last_expired_normal_timeout->getNext() :
                                     nullptr,
                                   last_expired_tracking_timeout ?
                                     last_expired_tracking_timeout->getNext() :
                                     nullptr);
    while (!mWindow.IsFrozen()) {
      Timeout* timeout = runIter.Next();
      MOZ_ASSERT(timeout != dummy_normal_timeout &&
                 timeout != dummy_tracking_timeout,
                 "We should have stopped iterating before getting to the dummy timeout");
      if (!timeout) {
        // We have run out of timeouts!
        break;
      }
      runIter.UpdateIterator();

      if (timeout->mFiringDepth != firingDepth) {
        // We skip the timeout since it's on the list to run at another
        // depth.
        continue;
      }

      if (mWindow.IsSuspended()) {
        // Some timer did suspend us. Make sure the
        // rest of the timers get executed later.
        timeout->mFiringDepth = 0;
        continue;
      }

      // The timeout is on the list to run at this depth, go ahead and
      // process it.

      // Get the script context (a strong ref to prevent it going away)
      // for this timeout and ensure the script language is enabled.
      nsCOMPtr<nsIScriptContext> scx = mWindow.GetContextInternal();

      if (!scx) {
        // No context means this window was closed or never properly
        // initialized for this language.
        continue;
      }

      // This timeout is good to run
      bool timeout_was_cleared = mWindow.RunTimeoutHandler(timeout, scx);

      if (timeout_was_cleared) {
        // Make sure the iterator isn't holding any Timeout objects alive.
        runIter.Clear();

        // The running timeout's window was cleared, this means that
        // ClearAllTimeouts() was called from a *nested* call, possibly
        // through a timeout that fired while a modal (to this window)
        // dialog was open or through other non-obvious paths.
        // Note that if the last expired timeout corresponding to each list
        // is null, then we should expect a refcount of two, since the
        // dummy timeout for this queue was never injected into it, and the
        // corresponding timeoutExtraRef variable hasn't been cleared yet.
        if (last_expired_timeout_is_normal) {
          MOZ_ASSERT(dummy_normal_timeout->HasRefCnt(1), "dummy_normal_timeout may leak");
          MOZ_ASSERT(dummy_tracking_timeout->HasRefCnt(2), "dummy_tracking_timeout may leak");
          Unused << timeoutExtraRef1.forget().take();
        } else {
          MOZ_ASSERT(dummy_normal_timeout->HasRefCnt(2), "dummy_normal_timeout may leak");
          MOZ_ASSERT(dummy_tracking_timeout->HasRefCnt(1), "dummy_tracking_timeout may leak");
          Unused << timeoutExtraRef2.forget().take();
        }

        mNormalTimeouts.SetInsertionPoint(last_normal_insertion_point);
        mTrackingTimeouts.SetInsertionPoint(last_tracking_insertion_point);

        return;
      }

      // If we have a regular interval timer, we re-schedule the
      // timeout, accounting for clock drift.
      bool needsReinsertion = RescheduleTimeout(timeout, now, !aTimeout);

      // Running a timeout can cause another timeout to be deleted, so
      // we need to reset the pointer to the following timeout.
      runIter.UpdateIterator();

      timeout->remove();

      if (needsReinsertion) {
        // Insert interval timeout onto the corresponding list sorted in
        // deadline order. AddRefs timeout.
        if (runIter.PickedTrackingIter()) {
          mTrackingTimeouts.Insert(timeout,
                                   mWindow.IsFrozen() ? Timeouts::SortBy::TimeRemaining
                                                      : Timeouts::SortBy::TimeWhen);
        } else {
          mNormalTimeouts.Insert(timeout,
                                 mWindow.IsFrozen() ? Timeouts::SortBy::TimeRemaining
                                                    : Timeouts::SortBy::TimeWhen);
        }
      }

      // Release the timeout struct since it's possibly out of the list
      timeout->Release();
    }
  }

  // Take the dummy timeout off the head of the list
  if (dummy_normal_timeout->isInList()) {
    dummy_normal_timeout->remove();
  }
  timeoutExtraRef1 = nullptr;
  MOZ_ASSERT(dummy_normal_timeout->HasRefCnt(1), "dummy_normal_timeout may leak");
  if (dummy_tracking_timeout->isInList()) {
    dummy_tracking_timeout->remove();
  }
  timeoutExtraRef2 = nullptr;
  MOZ_ASSERT(dummy_tracking_timeout->HasRefCnt(1), "dummy_tracking_timeout may leak");

  mNormalTimeouts.SetInsertionPoint(last_normal_insertion_point);
  mTrackingTimeouts.SetInsertionPoint(last_tracking_insertion_point);

  MaybeApplyBackPressure();
}

void
TimeoutManager::MaybeApplyBackPressure()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we are already in back pressure then we don't need to apply back
  // pressure again.  We also shouldn't need to apply back pressure while
  // the window is suspended.
  if (mBackPressureDelayMS > 0 || mWindow.IsSuspended()) {
    return;
  }

  RefPtr<ThrottledEventQueue> queue =
    do_QueryObject(mWindow.TabGroup()->EventTargetFor(TaskCategory::Timer));
  if (!queue) {
    return;
  }

  // Only begin back pressure if the window has greatly fallen behind the main
  // thread.  This is a somewhat arbitrary threshold chosen such that it should
  // rarely fire under normaly circumstances.  Its low enough, though,
  // that we should have time to slow new runnables from being added before an
  // OOM occurs.
  if (queue->Length() < kThrottledEventQueueBackPressure) {
    return;
  }

  // First attempt to dispatch a runnable to update our back pressure state.  We
  // do this first in order to verify we can dispatch successfully before
  // entering the back pressure state.
  nsCOMPtr<nsIRunnable> r =
    NewNonOwningRunnableMethod<StorensRefPtrPassByPtr<nsGlobalWindow>>(this,
      &TimeoutManager::CancelOrUpdateBackPressure, &mWindow);
  nsresult rv = queue->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS_VOID(rv);

  // Since the callback was scheduled successfully we can now persist the
  // backpressure value.
  mBackPressureDelayMS = CalculateNewBackPressureDelayMS(queue->Length());
}

void
TimeoutManager::CancelOrUpdateBackPressure(nsGlobalWindow* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow == &mWindow);
  MOZ_ASSERT(mBackPressureDelayMS > 0);

  // First, re-calculate the back pressure delay.
  RefPtr<ThrottledEventQueue> queue =
    do_QueryObject(mWindow.TabGroup()->EventTargetFor(TaskCategory::Timer));
  int32_t newBackPressureDelayMS =
    CalculateNewBackPressureDelayMS(queue ? queue->Length() : 0);

  // If the delay has increased, then simply apply it.  Increasing the delay
  // does not risk re-ordering timers with similar parameters.  We want to
  // extra careful not to re-order sequential calls to setTimeout(func, 0),
  // for example.
  if (newBackPressureDelayMS > mBackPressureDelayMS) {
    mBackPressureDelayMS = newBackPressureDelayMS;
  }

  // If the delay has decreased, though, we only apply the new value if it has
  // reduced significantly.  This hysteresis avoids thrashing the back pressure
  // value back and forth rapidly.  This is important because reducing the
  // backpressure delay requires calling ResetTimerForThrottleReduction() which
  // can be quite expensive.  We only want to call that method if the back log
  // is really clearing.
  else if (newBackPressureDelayMS == 0 ||
           (newBackPressureDelayMS <=
           (mBackPressureDelayMS - kBackPressureDelayReductionThresholdMS))) {
    int32_t oldBackPressureDelayMS = mBackPressureDelayMS;
    mBackPressureDelayMS = newBackPressureDelayMS;

    // If the back pressure delay has gone down we must reset any existing
    // timers to use the new value.  Otherwise we run the risk of executing
    // timer callbacks out-of-order.
    ResetTimersForThrottleReduction(oldBackPressureDelayMS);
  }

  // If all of the back pressure delay has been removed then we no longer need
  // to check back pressure updates.  We can simply return without scheduling
  // another update runnable.
  if (!mBackPressureDelayMS) {
    return;
  }

  // Otherwise, if there is a back pressure delay still in effect we need
  // queue a runnable to check if it can be reduced in the future.  Note
  // that this runnable is dispatched to the ThrottledEventQueue.  This
  // means we will not check for a new value until the current back log
  // has been processed.  The next update will only keep back pressure if
  // more runnables continue to be dispatched to the queue.
  nsCOMPtr<nsIRunnable> r =
    NewNonOwningRunnableMethod<StorensRefPtrPassByPtr<nsGlobalWindow>>(this,
      &TimeoutManager::CancelOrUpdateBackPressure, &mWindow);
  MOZ_ALWAYS_SUCCEEDS(queue->Dispatch(r.forget(), NS_DISPATCH_NORMAL));
}

bool
TimeoutManager::RescheduleTimeout(Timeout* aTimeout, const TimeStamp& now,
                                  bool aRunningPendingTimeouts)
{
  if (!aTimeout->mIsInterval) {
    if (aTimeout->mTimer) {
      // The timeout still has an OS timer, and it's not an interval,
      // that means that the OS timer could still fire; cancel the OS
      // timer and release its reference to the timeout.
      aTimeout->mTimer->Cancel();
      aTimeout->mTimer = nullptr;
      aTimeout->Release();
    }
    return false;
  }

  // Compute time to next timeout for interval timer.
  // Make sure nextInterval is at least DOMMinTimeoutValue().
  TimeDuration nextInterval =
    TimeDuration::FromMilliseconds(std::max(aTimeout->mInterval,
                                          uint32_t(DOMMinTimeoutValue())));

  // If we're running pending timeouts, set the next interval to be
  // relative to "now", and not to when the timeout that was pending
  // should have fired.
  TimeStamp firingTime;
  if (aRunningPendingTimeouts) {
    firingTime = now + nextInterval;
  } else {
    firingTime = aTimeout->mWhen + nextInterval;
  }

  TimeStamp currentNow = TimeStamp::Now();
  TimeDuration delay = firingTime - currentNow;

  // And make sure delay is nonnegative; that might happen if the timer
  // thread is firing our timers somewhat early or if they're taking a long
  // time to run the callback.
  if (delay < TimeDuration(0)) {
    delay = TimeDuration(0);
  }

  if (!aTimeout->mTimer) {
    if (mWindow.IsFrozen()) {
      // If we are frozen simply set timeout->mTimeRemaining to be the
      // "time remaining" in the timeout (i.e., the interval itself).  This
      // will be used to create a new mWhen time when the window is thawed.
      // The end effect is that time does not appear to pass for frozen windows.
      aTimeout->mTimeRemaining = delay;
    } else if (mWindow.IsSuspended()) {
    // Since we are not frozen we must set a precise mWhen target wakeup
    // time.  Even if we are suspended we want to use this target time so
    // that it appears time passes while suspended.
      aTimeout->mWhen = currentNow + delay;
    } else {
      MOZ_ASSERT_UNREACHABLE("Window should be frozen or suspended.");
    }
    return true;
  }

  aTimeout->mWhen = currentNow + delay;

  // Reschedule the OS timer. Don't bother returning any error codes if
  // this fails since the callers of this method don't care about them.
  nsresult rv = aTimeout->InitTimer(mWindow.EventTargetFor(TaskCategory::Timer),
                                    delay.ToMilliseconds());

  if (NS_FAILED(rv)) {
    NS_ERROR("Error initializing timer for DOM timeout!");

    // We failed to initialize the new OS timer, this timer does
    // us no good here so we just cancel it (just in case) and
    // null out the pointer to the OS timer, this will release the
    // OS timer. As we continue executing the code below we'll end
    // up deleting the timeout since it's not an interval timeout
    // any more (since timeout->mTimer == nullptr).
    aTimeout->mTimer->Cancel();
    aTimeout->mTimer = nullptr;

    // Now that the OS timer no longer has a reference to the
    // timeout we need to drop that reference.
    aTimeout->Release();

    return false;
  }

  return true;
}

nsresult
TimeoutManager::ResetTimersForThrottleReduction()
{
  return ResetTimersForThrottleReduction(gMinBackgroundTimeoutValue);
}

nsresult
TimeoutManager::ResetTimersForThrottleReduction(int32_t aPreviousThrottleDelayMS)
{
  MOZ_ASSERT(aPreviousThrottleDelayMS > 0);

  if (mWindow.IsFrozen() || mWindow.IsSuspended()) {
    return NS_OK;
  }

  auto minTimeout = DOMMinTimeoutValue();
  Timeouts::SortBy sortBy = mWindow.IsFrozen() ? Timeouts::SortBy::TimeRemaining
                                               : Timeouts::SortBy::TimeWhen;

  nsCOMPtr<nsIEventTarget> queue = mWindow.EventTargetFor(TaskCategory::Timer);
  nsresult rv = mNormalTimeouts.ResetTimersForThrottleReduction(aPreviousThrottleDelayMS,
                                                                minTimeout,
                                                                sortBy,
                                                                queue);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mTrackingTimeouts.ResetTimersForThrottleReduction(aPreviousThrottleDelayMS,
                                                         minTimeout,
                                                         sortBy,
                                                         queue);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
TimeoutManager::Timeouts::ResetTimersForThrottleReduction(int32_t aPreviousThrottleDelayMS,
                                                          int32_t aMinTimeoutValueMS,
                                                          SortBy aSortBy,
                                                          nsIEventTarget* aQueue)
{
  TimeStamp now = TimeStamp::Now();

  // If insertion point is non-null, we're in the middle of firing timers and
  // the timers we're planning to fire all come before insertion point;
  // insertion point itself is a dummy timeout with an mWhen that may be
  // semi-bogus.  In that case, we don't need to do anything with insertion
  // point or anything before it, so should start at the timer after insertion
  // point, if there is one.
  // Otherwise, start at the beginning of the list.
  for (Timeout* timeout = InsertionPoint() ?
         InsertionPoint()->getNext() : GetFirst();
       timeout; ) {
    // It's important that this check be <= so that we guarantee that
    // taking std::max with |now| won't make a quantity equal to
    // timeout->mWhen below.
    if (timeout->mWhen <= now) {
      timeout = timeout->getNext();
      continue;
    }

    if (timeout->mWhen - now >
        TimeDuration::FromMilliseconds(aPreviousThrottleDelayMS)) {
      // No need to loop further.  Timeouts are sorted in mWhen order
      // and the ones after this point were all set up for at least
      // gMinBackgroundTimeoutValue ms and hence were not clamped.
      break;
    }

    // We reduced our throttled delay. Re-init the timer appropriately.
    // Compute the interval the timer should have had if it had not been set in a
    // background window
    TimeDuration interval =
      TimeDuration::FromMilliseconds(std::max(timeout->mInterval,
                                            uint32_t(aMinTimeoutValueMS)));
    uint32_t oldIntervalMillisecs = 0;
    timeout->mTimer->GetDelay(&oldIntervalMillisecs);
    TimeDuration oldInterval = TimeDuration::FromMilliseconds(oldIntervalMillisecs);
    if (oldInterval > interval) {
      // unclamp
      TimeStamp firingTime =
        std::max(timeout->mWhen - oldInterval + interval, now);

      NS_ASSERTION(firingTime < timeout->mWhen,
                   "Our firing time should strictly decrease!");

      TimeDuration delay = firingTime - now;
      timeout->mWhen = firingTime;

      // Since we reset mWhen we need to move |timeout| to the right
      // place in the list so that it remains sorted by mWhen.

      // Get the pointer to the next timeout now, before we move the
      // current timeout in the list.
      Timeout* nextTimeout = timeout->getNext();

      // It is safe to remove and re-insert because mWhen is now
      // strictly smaller than it used to be, so we know we'll insert
      // |timeout| before nextTimeout.
      NS_ASSERTION(!nextTimeout ||
                   timeout->mWhen < nextTimeout->mWhen, "How did that happen?");
      timeout->remove();
      // Insert() will addref |timeout| and reset mFiringDepth.  Make sure to
      // undo that after calling it.
      uint32_t firingDepth = timeout->mFiringDepth;
      Insert(timeout, aSortBy);
      timeout->mFiringDepth = firingDepth;
      timeout->Release();

      nsresult rv = timeout->InitTimer(aQueue, delay.ToMilliseconds());

      if (NS_FAILED(rv)) {
        NS_WARNING("Error resetting non background timer for DOM timeout!");
        return rv;
      }

      timeout = nextTimeout;
    } else {
      timeout = timeout->getNext();
    }
  }

  return NS_OK;
}

void
TimeoutManager::ClearAllTimeouts()
{
  bool seenRunningTimeout = false;

  ForEachUnorderedTimeout([&](Timeout* aTimeout) {
    /* If RunTimeout() is higher up on the stack for this
       window, e.g. as a result of document.write from a timeout,
       then we need to reset the list insertion point for
       newly-created timeouts in case the user adds a timeout,
       before we pop the stack back to RunTimeout. */
    if (mRunningTimeout == aTimeout) {
      seenRunningTimeout = true;
    }

    if (aTimeout->mTimer) {
      aTimeout->mTimer->Cancel();
      aTimeout->mTimer = nullptr;

      // Drop the count since the timer isn't going to hold on
      // anymore.
      aTimeout->Release();
    }

    // Set timeout->mCleared to true to indicate that the timeout was
    // cleared and taken out of the list of timeouts
    aTimeout->mCleared = true;

    // Drop the count since we're removing it from the list.
    aTimeout->Release();
  });

  if (seenRunningTimeout) {
    mNormalTimeouts.SetInsertionPoint(nullptr);
    mTrackingTimeouts.SetInsertionPoint(nullptr);
  }

  // Clear out our list
  mNormalTimeouts.Clear();
  mTrackingTimeouts.Clear();
}

void
TimeoutManager::Timeouts::Insert(Timeout* aTimeout, SortBy aSortBy)
{
  // Start at mLastTimeout and go backwards.  Don't go further than insertion
  // point, though.  This optimizes for the common case of insertion at the end.
  Timeout* prevSibling;
  for (prevSibling = GetLast();
       prevSibling && prevSibling != InsertionPoint() &&
         // This condition needs to match the one in SetTimeoutOrInterval that
         // determines whether to set mWhen or mTimeRemaining.
         (aSortBy == SortBy::TimeRemaining ?
          prevSibling->mTimeRemaining > aTimeout->mTimeRemaining :
          prevSibling->mWhen > aTimeout->mWhen);
       prevSibling = prevSibling->getPrevious()) {
    /* Do nothing; just searching */
  }

  // Now link in aTimeout after prevSibling.
  if (prevSibling) {
    prevSibling->setNext(aTimeout);
  } else {
    InsertFront(aTimeout);
  }

  aTimeout->mFiringDepth = 0;

  // Increment the timeout's reference count since it's now held on to
  // by the list
  aTimeout->AddRef();
}

Timeout*
TimeoutManager::BeginRunningTimeout(Timeout* aTimeout)
{
  Timeout* currentTimeout = mRunningTimeout;
  mRunningTimeout = aTimeout;

  ++gRunningTimeoutDepth;
  ++mTimeoutFiringDepth;

  return currentTimeout;
}

void
TimeoutManager::EndRunningTimeout(Timeout* aTimeout)
{
  --mTimeoutFiringDepth;
  --gRunningTimeoutDepth;

  mRunningTimeout = aTimeout;
}

void
TimeoutManager::UnmarkGrayTimers()
{
  ForEachUnorderedTimeout([](Timeout* aTimeout) {
    if (aTimeout->mScriptHandler) {
      aTimeout->mScriptHandler->MarkForCC();
    }
  });
}

void
TimeoutManager::Suspend()
{
  ForEachUnorderedTimeout([](Timeout* aTimeout) {
    // Leave the timers with the current time remaining.  This will
    // cause the timers to potentially fire when the window is
    // Resume()'d.  Time effectively passes while suspended.

    // Drop the XPCOM timer; we'll reschedule when restoring the state.
    if (aTimeout->mTimer) {
      aTimeout->mTimer->Cancel();
      aTimeout->mTimer = nullptr;

      // Drop the reference that the timer's closure had on this timeout, we'll
      // add it back in Resume().
      aTimeout->Release();
    }
  });
}

void
TimeoutManager::Resume()
{
  TimeStamp now = TimeStamp::Now();
  DebugOnly<bool> _seenDummyTimeout = false;

  ForEachUnorderedTimeout([&](Timeout* aTimeout) {
    // There's a chance we're being called with RunTimeout on the stack in which
    // case we have a dummy timeout in the list that *must not* be resumed. It
    // can be identified by a null mWindow.
    if (!aTimeout->mWindow) {
      NS_ASSERTION(!_seenDummyTimeout, "More than one dummy timeout?!");
      _seenDummyTimeout = true;
      return;
    }

    MOZ_ASSERT(!aTimeout->mTimer);

    // The timeout mWhen is set to the absolute time when the timer should
    // fire.  Recalculate the delay from now until that deadline.  If the
    // the deadline has already passed or falls within our minimum delay
    // deadline, then clamp the resulting value to the minimum delay.  The
    // mWhen will remain at its absolute time, but we won'aTimeout fire the OS
    // timer until our calculated delay has passed.
    int32_t remaining = 0;
    if (aTimeout->mWhen > now) {
      remaining = static_cast<int32_t>((aTimeout->mWhen - now).ToMilliseconds());
    }
    uint32_t delay = std::max(remaining, DOMMinTimeoutValue());

    aTimeout->mTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (!aTimeout->mTimer) {
      aTimeout->remove();
      return;
    }

    nsresult rv = aTimeout->InitTimer(mWindow.EventTargetFor(TaskCategory::Timer),
                                      delay);
    if (NS_FAILED(rv)) {
      aTimeout->mTimer = nullptr;
      aTimeout->remove();
      return;
    }

    // Add a reference for the new timer's closure.
    aTimeout->AddRef();
  });
}

void
TimeoutManager::Freeze()
{
  TimeStamp now = TimeStamp::Now();
  ForEachUnorderedTimeout([&](Timeout* aTimeout) {
    // Save the current remaining time for this timeout.  We will
    // re-apply it when the window is Thaw()'d.  This effectively
    // shifts timers to the right as if time does not pass while
    // the window is frozen.
    if (aTimeout->mWhen > now) {
      aTimeout->mTimeRemaining = aTimeout->mWhen - now;
    } else {
      aTimeout->mTimeRemaining = TimeDuration(0);
    }

    // Since we are suspended there should be no OS timer set for
    // this timeout entry.
    MOZ_ASSERT(!aTimeout->mTimer);
  });
}

void
TimeoutManager::Thaw()
{
  TimeStamp now = TimeStamp::Now();
  DebugOnly<bool> _seenDummyTimeout = false;

  ForEachUnorderedTimeout([&](Timeout* aTimeout) {
    // There's a chance we're being called with RunTimeout on the stack in which
    // case we have a dummy timeout in the list that *must not* be resumed. It
    // can be identified by a null mWindow.
    if (!aTimeout->mWindow) {
      NS_ASSERTION(!_seenDummyTimeout, "More than one dummy timeout?!");
      _seenDummyTimeout = true;
      return;
    }

    // Set mWhen back to the time when the timer is supposed to fire.
    aTimeout->mWhen = now + aTimeout->mTimeRemaining;

    MOZ_ASSERT(!aTimeout->mTimer);
  });
}

bool
TimeoutManager::IsTimeoutTracking(uint32_t aTimeoutId)
{
  return mTrackingTimeouts.ForEachAbortable([&](Timeout* aTimeout) {
      return aTimeout->mTimeoutId == aTimeoutId;
    });
}
