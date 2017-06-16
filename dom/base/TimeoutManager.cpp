/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimeoutManager.h"
#include "nsGlobalWindow.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ThrottledEventQueue.h"
#include "mozilla/TimeStamp.h"
#include "nsITimeoutHandler.h"
#include "mozilla/dom/TabGroup.h"
#include "OrderedTimeoutIterator.h"
#include "TimeoutExecutor.h"

using namespace mozilla;
using namespace mozilla::dom;

static LazyLogModule gLog("Timeout");

// Time between sampling timeout execution time.
const uint32_t kTelemetryPeriodMS = 1000;

class TimeoutTelemetry
{
public:
  static TimeoutTelemetry& Get();
  TimeoutTelemetry() : mLastCollection(TimeStamp::Now()) {}

  void StartRecording(TimeStamp aNow);
  void StopRecording();
  void RecordExecution(TimeStamp aNow, Timeout* aTimeout, bool aIsBackground);
  void MaybeCollectTelemetry(TimeStamp aNow);
private:
  struct TelemetryData
  {
    TimeDuration mForegroundTracking;
    TimeDuration mForegroundNonTracking;
    TimeDuration mBackgroundTracking;
    TimeDuration mBackgroundNonTracking;
  };

  void Accumulate(Telemetry::HistogramID aId, TimeDuration aSample);

  TelemetryData mTelemetryData;
  TimeStamp mStart;
  TimeStamp mLastCollection;
};

static TimeoutTelemetry gTimeoutTelemetry;

/* static */ TimeoutTelemetry&
TimeoutTelemetry::Get()
{
  return gTimeoutTelemetry;
}

void
TimeoutTelemetry::StartRecording(TimeStamp aNow)
{
  mStart = aNow;
}

void
TimeoutTelemetry::StopRecording()
{
  mStart = TimeStamp();
}

void
TimeoutTelemetry::RecordExecution(TimeStamp aNow,
                                  Timeout* aTimeout,
                                  bool aIsBackground)
{
  if (!mStart) {
    // If we've started a sync operation mStart might be null, in
    // which case we should not record this piece of execution.
    return;
  }

  TimeDuration duration = aNow - mStart;

  if (aIsBackground) {
    if (aTimeout->mIsTracking) {
      mTelemetryData.mBackgroundTracking += duration;
    } else {
      mTelemetryData.mBackgroundNonTracking += duration;
    }
  } else {
    if (aTimeout->mIsTracking) {
      mTelemetryData.mForegroundTracking += duration;
    } else {
      mTelemetryData.mForegroundNonTracking += duration;
    }
  }
}

void
TimeoutTelemetry::Accumulate(Telemetry::HistogramID aId, TimeDuration aSample)
{
  uint32_t sample = std::round(aSample.ToMilliseconds());
  if (sample) {
    Telemetry::Accumulate(aId, sample);
  }
}

void
TimeoutTelemetry::MaybeCollectTelemetry(TimeStamp aNow)
{
  if ((aNow - mLastCollection).ToMilliseconds() < kTelemetryPeriodMS) {
    return;
  }

  Accumulate(Telemetry::TIMEOUT_EXECUTION_FG_TRACKING_MS,
             mTelemetryData.mForegroundTracking);
  Accumulate(Telemetry::TIMEOUT_EXECUTION_FG_MS,
             mTelemetryData.mForegroundNonTracking);
  Accumulate(Telemetry::TIMEOUT_EXECUTION_BG_TRACKING_MS,
             mTelemetryData.mBackgroundTracking);
  Accumulate(Telemetry::TIMEOUT_EXECUTION_BG_MS,
             mTelemetryData.mBackgroundNonTracking);

  mTelemetryData = TelemetryData();
  mLastCollection = aNow;
}

static int32_t              gRunningTimeoutDepth       = 0;

// The default shortest interval/timeout we permit
#define DEFAULT_MIN_CLAMP_TIMEOUT_VALUE 4 // 4ms
#define DEFAULT_MIN_BACKGROUND_TIMEOUT_VALUE 1000 // 1000ms
#define DEFAULT_MIN_TRACKING_TIMEOUT_VALUE 4 // 4ms
#define DEFAULT_MIN_TRACKING_BACKGROUND_TIMEOUT_VALUE 1000 // 1000ms
static int32_t gMinClampTimeoutValue = 0;
static int32_t gMinBackgroundTimeoutValue = 0;
static int32_t gMinTrackingTimeoutValue = 0;
static int32_t gMinTrackingBackgroundTimeoutValue = 0;
static int32_t gTrackingTimeoutThrottlingDelay = 0;
static bool    gAnnotateTrackingChannels = false;

// static
const uint32_t TimeoutManager::InvalidFiringId = 0;

bool
TimeoutManager::IsBackground() const
{
  // Don't use the background timeout value when the tab is playing audio.
  // Until bug 1336484 we only used to do this for pages that use Web Audio.
  // The original behavior was implemented in bug 11811073.
  return !mWindow.AsInner()->IsPlayingAudio() && mWindow.IsBackgroundInternal();
}

uint32_t
TimeoutManager::CreateFiringId()
{
  uint32_t id = mNextFiringId;
  mNextFiringId += 1;
  if (mNextFiringId == InvalidFiringId) {
    mNextFiringId += 1;
  }

  mFiringIdStack.AppendElement(id);

  return id;
}

void
TimeoutManager::DestroyFiringId(uint32_t aFiringId)
{
  MOZ_DIAGNOSTIC_ASSERT(!mFiringIdStack.IsEmpty());
  MOZ_DIAGNOSTIC_ASSERT(mFiringIdStack.LastElement() == aFiringId);
  mFiringIdStack.RemoveElementAt(mFiringIdStack.Length() - 1);
}

bool
TimeoutManager::IsValidFiringId(uint32_t aFiringId) const
{
  return !IsInvalidFiringId(aFiringId);
}

TimeDuration
TimeoutManager::MinSchedulingDelay() const
{
  if (IsBackground()) {
    return TimeDuration::FromMilliseconds(gMinBackgroundTimeoutValue);
  }
  return TimeDuration();
}

bool
TimeoutManager::IsInvalidFiringId(uint32_t aFiringId) const
{
  // Check the most common ways to invalidate a firing id first.
  // These should be quite fast.
  if (aFiringId == InvalidFiringId ||
      mFiringIdStack.IsEmpty()) {
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
    Swap(low, high);
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

int32_t
TimeoutManager::DOMMinTimeoutValue(bool aIsTracking) const {
  bool throttleTracking = aIsTracking && mThrottleTrackingTimeouts;
  auto minValue = throttleTracking ? gMinTrackingTimeoutValue
                                   : gMinClampTimeoutValue;
  return minValue;
}

#define TRACKING_SEPARATE_TIMEOUT_BUCKETING_STRATEGY 0 // Consider all timeouts coming from tracking scripts as tracking
// These strategies are useful for testing.
#define ALL_NORMAL_TIMEOUT_BUCKETING_STRATEGY        1 // Consider all timeouts as normal
#define ALTERNATE_TIMEOUT_BUCKETING_STRATEGY         2 // Put every other timeout in the list of tracking timeouts
#define RANDOM_TIMEOUT_BUCKETING_STRATEGY            3 // Put timeouts into either the normal or tracking timeouts list randomly
static int32_t gTimeoutBucketingStrategy = 0;

#define DEFAULT_TRACKING_TIMEOUT_THROTTLING_DELAY  -1  // Only positive integers cause us to introduce a delay for tracking
                                                       // timeout throttling.

// The number of nested timeouts before we start clamping. HTML5 says 1, WebKit
// uses 5.
#define DOM_CLAMP_TIMEOUT_NESTING_LEVEL 5

// The longest interval (as PRIntervalTime) we permit, or that our
// timer code can handle, really. See DELAY_INTERVAL_LIMIT in
// nsTimerImpl.h for details.
#define DOM_MAX_TIMEOUT_VALUE    DELAY_INTERVAL_LIMIT

uint32_t TimeoutManager::sNestingLevel = 0;

namespace {

// The maximum number of milliseconds to allow consecutive timer callbacks
// to run in a single event loop runnable.
#define DEFAULT_MAX_CONSECUTIVE_CALLBACKS_MILLISECONDS 4
uint32_t gMaxConsecutiveCallbacksMilliseconds;

} // anonymous namespace

TimeoutManager::TimeoutManager(nsGlobalWindow& aWindow)
  : mWindow(aWindow),
    mExecutor(new TimeoutExecutor(this)),
    mNormalTimeouts(*this),
    mTrackingTimeouts(*this),
    mTimeoutIdCounter(1),
    mNextFiringId(InvalidFiringId + 1),
    mRunningTimeout(nullptr),
    mIdleCallbackTimeoutCounter(1),
    mThrottleTrackingTimeouts(false)
{
  MOZ_DIAGNOSTIC_ASSERT(aWindow.IsInnerWindow());

  MOZ_LOG(gLog, LogLevel::Debug,
          ("TimeoutManager %p created, tracking bucketing %s\n",
           this, gAnnotateTrackingChannels ? "enabled" : "disabled"));
}

TimeoutManager::~TimeoutManager()
{
  MOZ_DIAGNOSTIC_ASSERT(mWindow.AsInner()->InnerObjectsFreed());
  MOZ_DIAGNOSTIC_ASSERT(!mThrottleTrackingTimeoutsTimer);

  mExecutor->Shutdown();

  MOZ_LOG(gLog, LogLevel::Debug,
          ("TimeoutManager %p destroyed\n", this));
}

/* static */
void
TimeoutManager::Initialize()
{
  Preferences::AddIntVarCache(&gMinClampTimeoutValue,
                              "dom.min_timeout_value",
                              DEFAULT_MIN_CLAMP_TIMEOUT_VALUE);
  Preferences::AddIntVarCache(&gMinBackgroundTimeoutValue,
                              "dom.min_background_timeout_value",
                              DEFAULT_MIN_BACKGROUND_TIMEOUT_VALUE);
  Preferences::AddIntVarCache(&gMinTrackingTimeoutValue,
                              "dom.min_tracking_timeout_value",
                              DEFAULT_MIN_TRACKING_TIMEOUT_VALUE);
  Preferences::AddIntVarCache(&gMinTrackingBackgroundTimeoutValue,
                              "dom.min_tracking_background_timeout_value",
                              DEFAULT_MIN_TRACKING_BACKGROUND_TIMEOUT_VALUE);
  Preferences::AddIntVarCache(&gTimeoutBucketingStrategy,
                              "dom.timeout_bucketing_strategy",
                              TRACKING_SEPARATE_TIMEOUT_BUCKETING_STRATEGY);
  Preferences::AddIntVarCache(&gTrackingTimeoutThrottlingDelay,
                              "dom.timeout.tracking_throttling_delay",
                              DEFAULT_TRACKING_TIMEOUT_THROTTLING_DELAY);
  Preferences::AddBoolVarCache(&gAnnotateTrackingChannels,
                               "privacy.trackingprotection.annotate_channels",
                               false);

  Preferences::AddUintVarCache(&gMaxConsecutiveCallbacksMilliseconds,
                               "dom.timeout.max_consecutive_callbacks_ms",
                               DEFAULT_MAX_CONSECUTIVE_CALLBACKS_MILLISECONDS);
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

bool
TimeoutManager::IsRunningTimeout() const
{
  return mRunningTimeout;
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
  timeout->mWindow = &mWindow;
  timeout->mIsInterval = aIsInterval;
  timeout->mInterval = interval;
  timeout->mScriptHandler = aHandler;
  timeout->mReason = aReason;

  // No popups from timeouts by default
  timeout->mPopupState = openAbused;

  switch (gTimeoutBucketingStrategy) {
  default:
  case TRACKING_SEPARATE_TIMEOUT_BUCKETING_STRATEGY: {
    const char* filename = nullptr;
    uint32_t dummyLine = 0, dummyColumn = 0;
    aHandler->GetLocation(&filename, &dummyLine, &dummyColumn);
    timeout->mIsTracking = doc->IsScriptTracking(nsDependentCString(filename));

    MOZ_LOG(gLog, LogLevel::Debug,
            ("Classified timeout %p set from %s as %stracking\n",
             timeout.get(), filename, timeout->mIsTracking ? "" : "non-"));
    break;
  }
  case ALL_NORMAL_TIMEOUT_BUCKETING_STRATEGY:
    // timeout->mIsTracking is already false!
    MOZ_DIAGNOSTIC_ASSERT(!timeout->mIsTracking);

    MOZ_LOG(gLog, LogLevel::Debug,
            ("Classified timeout %p unconditionally as normal\n",
             timeout.get()));
    break;
  case ALTERNATE_TIMEOUT_BUCKETING_STRATEGY:
    timeout->mIsTracking = (mTimeoutIdCounter % 2) == 0;

    MOZ_LOG(gLog, LogLevel::Debug,
            ("Classified timeout %p as %stracking (alternating mode)\n",
             timeout.get(), timeout->mIsTracking ? "" : "non-"));
    break;
  case RANDOM_TIMEOUT_BUCKETING_STRATEGY:
    timeout->mIsTracking = (rand() % 2) == 0;

    MOZ_LOG(gLog, LogLevel::Debug,
            ("Classified timeout %p as %stracking (random mode)\n",
             timeout.get(), timeout->mIsTracking ? "" : "non-"));
    break;
  }

  uint32_t nestingLevel = sNestingLevel + 1;
  if (!aIsInterval) {
    timeout->mNestingLevel = nestingLevel;
  }

  // Now clamp the actual interval we will use for the timer based on
  uint32_t realInterval = interval;
  if (aIsInterval || nestingLevel >= DOM_CLAMP_TIMEOUT_NESTING_LEVEL ||
      timeout->mIsTracking) {
    // Don't allow timeouts less than DOMMinTimeoutValue() from
    // now...
    realInterval = std::max(realInterval,
                            uint32_t(DOMMinTimeoutValue(timeout->mIsTracking)));
  }

  TimeDuration delta = TimeDuration::FromMilliseconds(realInterval);
  timeout->SetWhenOrTimeRemaining(TimeStamp::Now(), delta);

  // If we're not suspended, then set the timer.
  if (!mWindow.IsSuspended()) {
    nsresult rv = mExecutor->MaybeSchedule(timeout->When(),
                                           MinSchedulingDelay());
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

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

  Timeouts::SortBy sort(mWindow.IsFrozen() ? Timeouts::SortBy::TimeRemaining
                                           : Timeouts::SortBy::TimeWhen);
  if (timeout->mIsTracking) {
    mTrackingTimeouts.Insert(timeout, sort);
  } else {
    mNormalTimeouts.Insert(timeout, sort);
  }

  timeout->mTimeoutId = GetTimeoutId(aReason);
  *aReturn = timeout->mTimeoutId;

  MOZ_LOG(gLog, LogLevel::Debug,
          ("Set%s(TimeoutManager=%p, timeout=%p, delay=%i, "
           "minimum=%i, throttling=%s, background=%d, realInterval=%i) "
           "returned %stracking timeout ID %u\n",
           aIsInterval ? "Interval" : "Timeout",
           this, timeout.get(), interval,
           DOMMinTimeoutValue(timeout->mIsTracking),
           mThrottleTrackingTimeouts ? "yes"
                                     : (mThrottleTrackingTimeoutsTimer ?
                                          "pending" : "no"),
           int(IsBackground()), realInterval,
           timeout->mIsTracking ? "" : "non-",
           timeout->mTimeoutId));

  return NS_OK;
}

void
TimeoutManager::ClearTimeout(int32_t aTimerId, Timeout::Reason aReason)
{
  uint32_t timerId = (uint32_t)aTimerId;

  bool firstTimeout = true;
  bool deferredDeletion = false;

  ForEachUnorderedTimeoutAbortable([&](Timeout* aTimeout) {
    MOZ_LOG(gLog, LogLevel::Debug,
            ("Clear%s(TimeoutManager=%p, timeout=%p, aTimerId=%u, ID=%u, tracking=%d)\n", aTimeout->mIsInterval ? "Interval" : "Timeout",
             this, aTimeout, timerId, aTimeout->mTimeoutId,
             int(aTimeout->mIsTracking)));

    if (aTimeout->mTimeoutId == timerId && aTimeout->mReason == aReason) {
      if (aTimeout->mRunning) {
        /* We're running from inside the aTimeout. Mark this
           aTimeout for deferred deletion by the code in
           RunTimeout() */
        aTimeout->mIsInterval = false;
        deferredDeletion = true;
      }
      else {
        /* Delete the aTimeout from the pending aTimeout list */
        aTimeout->remove();
      }
      return true; // abort!
    }

    firstTimeout = false;

    return false;
  });

  // We don't need to reschedule the executor if any of the following are true:
  //  * If the we weren't cancelling the first timeout, then the executor's
  //    state doesn't need to change.  It will only reflect the next soonest
  //    Timeout.
  //  * If we did cancel the first Timeout, but its currently running, then
  //    RunTimeout() will handle rescheduling the executor.
  //  * If the window has become suspended then we should not start executing
  //    Timeouts.
  if (!firstTimeout || deferredDeletion || mWindow.IsSuspended()) {
    return;
  }

  // Stop the executor and restart it at the next soonest deadline.
  mExecutor->Cancel();

  OrderedTimeoutIterator iter(mNormalTimeouts, mTrackingTimeouts);
  Timeout* nextTimeout = iter.Next();
  if (nextTimeout) {
    MOZ_ALWAYS_SUCCEEDS(mExecutor->MaybeSchedule(nextTimeout->When(),
                                                 MinSchedulingDelay()));
  }
}

void
TimeoutManager::RunTimeout(const TimeStamp& aNow, const TimeStamp& aTargetDeadline)
{
  MOZ_DIAGNOSTIC_ASSERT(!aNow.IsNull());
  MOZ_DIAGNOSTIC_ASSERT(!aTargetDeadline.IsNull());

  MOZ_ASSERT_IF(mWindow.IsFrozen(), mWindow.IsSuspended());
  if (mWindow.IsSuspended()) {
    return;
  }

  // Limit the overall time spent in RunTimeout() to reduce jank.
  uint32_t totalTimeLimitMS = std::max(1u, gMaxConsecutiveCallbacksMilliseconds);
  const TimeDuration totalTimeLimit = TimeDuration::FromMilliseconds(totalTimeLimitMS);

  // Allow up to 25% of our total time budget to be used figuring out which
  // timers need to run.  This is the initial loop in this method.
  const TimeDuration initalTimeLimit =
    TimeDuration::FromMilliseconds(totalTimeLimit.ToMilliseconds() / 4);

  // Ammortize overhead from from calling TimeStamp::Now() in the initial
  // loop, though, by only checking for an elapsed limit every N timeouts.
  const uint32_t kNumTimersPerInitialElapsedCheck = 100;

  // Start measuring elapsed time immediately.  We won't potentially expire
  // the time budget until at least one Timeout has run, though.
  TimeStamp now(aNow);
  TimeStamp start = now;

  uint32_t firingId = CreateFiringId();
  auto guard = MakeScopeExit([&] {
    DestroyFiringId(firingId);
  });

  // Make sure that the window and the script context don't go away as
  // a result of running timeouts
  nsCOMPtr<nsIScriptGlobalObject> windowKungFuDeathGrip(&mWindow);
  // Silence the static analysis error about windowKungFuDeathGrip.  Accessing
  // members of mWindow here is safe, because the lifetime of TimeoutManager is
  // the same as the lifetime of the containing nsGlobalWindow.
  Unused << windowKungFuDeathGrip;

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
  {
    // Use a nested scope in order to make sure the strong references held by
    // the iterator are freed after the loop.
    OrderedTimeoutIterator expiredIter(mNormalTimeouts, mTrackingTimeouts);

    while (true) {
      Timeout* timeout = expiredIter.Next();
      if (!timeout || timeout->When() > deadline) {
        if (timeout) {
          nextDeadline = timeout->When();
        }
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
          if (elapsed >= initalTimeLimit) {
            nextDeadline = timeout->When();
            break;
          }
        }
      }

      expiredIter.UpdateIterator();
    }
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
    MOZ_ALWAYS_SUCCEEDS(mExecutor->MaybeSchedule(nextDeadline,
                                                 MinSchedulingDelay()));
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
    // Use a nested scope in order to make sure the strong references held by
    // the iterator are freed after the loop.
    OrderedTimeoutIterator runIter(mNormalTimeouts, mTrackingTimeouts);
    while (true) {
      RefPtr<Timeout> timeout = runIter.Next();
      if (!timeout) {
        // We have run out of timeouts!
        break;
      }
      runIter.UpdateIterator();

      // We should only execute callbacks for the set of expired Timeout
      // objects we computed above.
      if (timeout->mFiringId != firingId) {
        // If the FiringId does not match, but is still valid, then this is
        // a TImeout for another RunTimeout() on the call stack.  Just
        // skip it.
        if (IsValidFiringId(timeout->mFiringId)) {
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

      // This timeout is good to run
      bool timeout_was_cleared = mWindow.RunTimeoutHandler(timeout, scx);
      MOZ_LOG(gLog, LogLevel::Debug,
              ("Run%s(TimeoutManager=%p, timeout=%p, tracking=%d) returned %d\n", timeout->mIsInterval ? "Interval" : "Timeout",
               this, timeout.get(),
               int(timeout->mIsTracking),
               !!timeout_was_cleared));

      if (timeout_was_cleared) {
        // Make sure the iterator isn't holding any Timeout objects alive.
        runIter.Clear();

        // Since ClearAllTimeouts() was called the lists should be empty.
        MOZ_DIAGNOSTIC_ASSERT(!HasTimeouts());

        return;
      }

      now = TimeStamp::Now();

      // If we have a regular interval timer, we re-schedule the
      // timeout, accounting for clock drift.
      bool needsReinsertion = RescheduleTimeout(timeout, now);

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

      // Check to see if we have run out of time to execute timeout handlers.
      // If we've exceeded our time budget then terminate the loop immediately.
      TimeDuration elapsed = now - start;
      if (elapsed >= totalTimeLimit) {
        // We ran out of time.  Make sure to schedule the executor to
        // run immediately for the next timer, if it exists.  Its possible,
        // however, that the last timeout handler suspended the window.  If
        // that happened then we must skip this step.
        if (!mWindow.IsSuspended()) {
          RefPtr<Timeout> timeout = runIter.Next();
          if (timeout) {
            MOZ_ALWAYS_SUCCEEDS(mExecutor->MaybeSchedule(timeout->When(),
                                                         MinSchedulingDelay()));
          }
        }
        break;
      }
    }
  }
}

bool
TimeoutManager::RescheduleTimeout(Timeout* aTimeout, const TimeStamp& now)
{
  if (!aTimeout->mIsInterval) {
    return false;
  }

  // Compute time to next timeout for interval timer.
  // Make sure nextInterval is at least DOMMinTimeoutValue().
  TimeDuration nextInterval =
    TimeDuration::FromMilliseconds(
        std::max(aTimeout->mInterval,
                 uint32_t(DOMMinTimeoutValue(aTimeout->mIsTracking))));

  TimeStamp firingTime = now + nextInterval;

  TimeStamp currentNow = TimeStamp::Now();
  TimeDuration delay = firingTime - currentNow;

  // And make sure delay is nonnegative; that might happen if the timer
  // thread is firing our timers somewhat early or if they're taking a long
  // time to run the callback.
  if (delay < TimeDuration(0)) {
    delay = TimeDuration(0);
  }

  aTimeout->SetWhenOrTimeRemaining(currentNow, delay);

  if (mWindow.IsSuspended()) {
    return true;
  }

  nsresult rv = mExecutor->MaybeSchedule(aTimeout->When(),
                                         MinSchedulingDelay());
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

void
TimeoutManager::ClearAllTimeouts()
{
  bool seenRunningTimeout = false;

  MOZ_LOG(gLog, LogLevel::Debug,
          ("ClearAllTimeouts(TimeoutManager=%p)\n", this));

  if (mThrottleTrackingTimeoutsTimer) {
    mThrottleTrackingTimeoutsTimer->Cancel();
    mThrottleTrackingTimeoutsTimer = nullptr;
  }

  mExecutor->Cancel();

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

  // Clear out our list
  mNormalTimeouts.Clear();
  mTrackingTimeouts.Clear();
}

void
TimeoutManager::Timeouts::Insert(Timeout* aTimeout, SortBy aSortBy)
{

  // Start at mLastTimeout and go backwards.  Stop if we see a Timeout with a
  // valid FiringId since those timers are currently being processed by
  // RunTimeout.  This optimizes for the common case of insertion at the end.
  Timeout* prevSibling;
  for (prevSibling = GetLast();
       prevSibling &&
         // This condition needs to match the one in SetTimeoutOrInterval that
         // determines whether to set When() or TimeRemaining().
         (aSortBy == SortBy::TimeRemaining ?
          prevSibling->TimeRemaining() > aTimeout->TimeRemaining() :
          prevSibling->When() > aTimeout->When()) &&
         // Check the firing ID last since it will evaluate true in the vast
         // majority of cases.
         mManager.IsInvalidFiringId(prevSibling->mFiringId);
       prevSibling = prevSibling->getPrevious()) {
    /* Do nothing; just searching */
  }

  // Now link in aTimeout after prevSibling.
  if (prevSibling) {
    prevSibling->setNext(aTimeout);
  } else {
    InsertFront(aTimeout);
  }

  aTimeout->mFiringId = InvalidFiringId;
}

Timeout*
TimeoutManager::BeginRunningTimeout(Timeout* aTimeout)
{
  Timeout* currentTimeout = mRunningTimeout;
  mRunningTimeout = aTimeout;

  ++gRunningTimeoutDepth;

  if (!mWindow.IsChromeWindow()) {
    TimeStamp now = TimeStamp::Now();
    if (currentTimeout) {
      // If we're already running a timeout and start running another
      // one, record the fragment duration already collected.
      TimeoutTelemetry::Get().RecordExecution(
        now, currentTimeout, IsBackground());
    }

    TimeoutTelemetry::Get().MaybeCollectTelemetry(now);
    TimeoutTelemetry::Get().StartRecording(now);
  }

  return currentTimeout;
}

void
TimeoutManager::EndRunningTimeout(Timeout* aTimeout)
{
  --gRunningTimeoutDepth;

  if (!mWindow.IsChromeWindow()) {
    TimeStamp now = TimeStamp::Now();
    TimeoutTelemetry::Get().RecordExecution(now, mRunningTimeout, IsBackground());

    if (aTimeout) {
      // If we were running a nested timeout, restart the measurement
      // from here.
      TimeoutTelemetry::Get().StartRecording(now);
    }
  }

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
  MOZ_LOG(gLog, LogLevel::Debug,
          ("Suspend(TimeoutManager=%p)\n", this));

  if (mThrottleTrackingTimeoutsTimer) {
    mThrottleTrackingTimeoutsTimer->Cancel();
    mThrottleTrackingTimeoutsTimer = nullptr;
  }

  mExecutor->Cancel();
}

void
TimeoutManager::Resume()
{
  MOZ_LOG(gLog, LogLevel::Debug,
          ("Resume(TimeoutManager=%p)\n", this));

  // When Suspend() has been called after IsDocumentLoaded(), but the
  // throttle tracking timer never managed to fire, start the timer
  // again.
  if (mWindow.AsInner()->IsDocumentLoaded() && !mThrottleTrackingTimeouts) {
    MaybeStartThrottleTrackingTimout();
  }

  OrderedTimeoutIterator iter(mNormalTimeouts, mTrackingTimeouts);
  Timeout* nextTimeout = iter.Next();
  if (nextTimeout) {
    MOZ_ALWAYS_SUCCEEDS(mExecutor->MaybeSchedule(nextTimeout->When(),
                                                 MinSchedulingDelay()));
  }
}

void
TimeoutManager::Freeze()
{
  MOZ_LOG(gLog, LogLevel::Debug,
          ("Freeze(TimeoutManager=%p)\n", this));

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

void
TimeoutManager::Thaw()
{
  MOZ_LOG(gLog, LogLevel::Debug,
          ("Thaw(TimeoutManager=%p)\n", this));

  TimeStamp now = TimeStamp::Now();

  ForEachUnorderedTimeout([&](Timeout* aTimeout) {
    // Set When() back to the time when the timer is supposed to fire.
    aTimeout->SetWhenOrTimeRemaining(now, aTimeout->TimeRemaining());
    MOZ_DIAGNOSTIC_ASSERT(!aTimeout->When().IsNull());
  });
}

void
TimeoutManager::UpdateBackgroundState()
{
  // When the window moves to the background or foreground we should
  // reschedule the TimeoutExecutor in case the MinSchedulingDelay()
  // changed.  Only do this if the window is not suspended and we
  // actually have a timeout.
  if (!mWindow.IsSuspended()) {
    OrderedTimeoutIterator iter(mNormalTimeouts, mTrackingTimeouts);
    Timeout* nextTimeout = iter.Next();
    if (nextTimeout) {
      mExecutor->Cancel();
      MOZ_ALWAYS_SUCCEEDS(mExecutor->MaybeSchedule(nextTimeout->When(),
                                                   MinSchedulingDelay()));
    }
  }
}

bool
TimeoutManager::IsTimeoutTracking(uint32_t aTimeoutId)
{
  return mTrackingTimeouts.ForEachAbortable([&](Timeout* aTimeout) {
      return aTimeout->mTimeoutId == aTimeoutId;
    });
}

namespace {

class ThrottleTrackingTimeoutsCallback final : public nsITimerCallback
{
public:
  explicit ThrottleTrackingTimeoutsCallback(nsGlobalWindow* aWindow)
    : mWindow(aWindow)
  {
    MOZ_DIAGNOSTIC_ASSERT(aWindow->IsInnerWindow());
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

private:
  ~ThrottleTrackingTimeoutsCallback() {}

private:
  // The strong reference here keeps the Window and hence the TimeoutManager
  // object itself alive.
  RefPtr<nsGlobalWindow> mWindow;
};

NS_IMPL_ISUPPORTS(ThrottleTrackingTimeoutsCallback, nsITimerCallback)

NS_IMETHODIMP
ThrottleTrackingTimeoutsCallback::Notify(nsITimer* aTimer)
{
  mWindow->AsInner()->TimeoutManager().StartThrottlingTrackingTimeouts();
  mWindow = nullptr;
  return NS_OK;
}

}

void
TimeoutManager::StartThrottlingTrackingTimeouts()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mThrottleTrackingTimeoutsTimer);

  MOZ_LOG(gLog, LogLevel::Debug,
          ("TimeoutManager %p started to throttle tracking timeouts\n", this));

  MOZ_DIAGNOSTIC_ASSERT(!mThrottleTrackingTimeouts);
  mThrottleTrackingTimeouts = true;
  mThrottleTrackingTimeoutsTimer = nullptr;
}

void
TimeoutManager::OnDocumentLoaded()
{
  // The load event may be firing again if we're coming back to the page by
  // navigating through the session history, so we need to ensure to only call
  // this when mThrottleTrackingTimeouts hasn't been set yet.
  if (!mThrottleTrackingTimeouts) {
    MaybeStartThrottleTrackingTimout();
  }
}

void
TimeoutManager::MaybeStartThrottleTrackingTimout()
{
  if (gTrackingTimeoutThrottlingDelay <= 0 ||
      mWindow.AsInner()->InnerObjectsFreed() || mWindow.IsSuspended()) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(!mThrottleTrackingTimeouts);

  MOZ_LOG(gLog, LogLevel::Debug,
          ("TimeoutManager %p delaying tracking timeout throttling by %dms\n",
           this, gTrackingTimeoutThrottlingDelay));

  mThrottleTrackingTimeoutsTimer =
    do_CreateInstance("@mozilla.org/timer;1");
  if (!mThrottleTrackingTimeoutsTimer) {
    return;
  }

  nsCOMPtr<nsITimerCallback> callback =
    new ThrottleTrackingTimeoutsCallback(&mWindow);

  mThrottleTrackingTimeoutsTimer->InitWithCallback(callback,
                                                   gTrackingTimeoutThrottlingDelay,
                                                   nsITimer::TYPE_ONE_SHOT);
}

void
TimeoutManager::BeginSyncOperation()
{
  // If we're beginning a sync operation, the currently running
  // timeout will be put on hold. To not get into an inconsistent
  // state, where the currently running timeout appears to take time
  // equivalent to the period of us spinning up a new event loop,
  // record what we have and stop recording until we reach
  // EndSyncOperation.
  if (!mWindow.IsChromeWindow()) {
    if (mRunningTimeout) {
      TimeoutTelemetry::Get().RecordExecution(
        TimeStamp::Now(), mRunningTimeout, IsBackground());
    }
    TimeoutTelemetry::Get().StopRecording();
  }
}

void
TimeoutManager::EndSyncOperation()
{
  // If we're running a timeout, restart the measurement from here.
  if (!mWindow.IsChromeWindow() && mRunningTimeout) {
    TimeoutTelemetry::Get().StartRecording(TimeStamp::Now());
  }
}

nsIEventTarget*
TimeoutManager::EventTarget()
{
  return mWindow.EventTargetFor(TaskCategory::Timer);
}
