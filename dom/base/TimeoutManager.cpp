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
#define DEFAULT_MIN_TIMEOUT_VALUE 4 // 4ms
#define DEFAULT_MIN_BACKGROUND_TIMEOUT_VALUE 1000 // 1000ms
#define DEFAULT_MIN_TRACKING_TIMEOUT_VALUE 4 // 4ms
#define DEFAULT_MIN_TRACKING_BACKGROUND_TIMEOUT_VALUE 1000 // 1000ms
static int32_t gMinTimeoutValue = 0;
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
TimeoutManager::IsInvalidFiringId(uint32_t aFiringId) const
{
  // Check the most common ways to invalidate a firing id first.
  // These should be quite fast.
  if (aFiringId == InvalidFiringId ||
      mFiringIdStack.IsEmpty() ||
      (mFiringIdStack.Length() == 1 && mFiringIdStack[0] != aFiringId)) {
    return true;
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
  // First apply any back pressure delay that might be in effect.
  int32_t value = std::max(mBackPressureDelayMS, 0);
  // Don't use the background timeout value when the tab is playing audio.
  // Until bug 1336484 we only used to do this for pages that use Web Audio.
  // The original behavior was implemented in bug 11811073.
  bool isBackground = IsBackground();
  bool throttleTracking = aIsTracking && mThrottleTrackingTimeouts;
  auto minValue = throttleTracking ? (isBackground ? gMinTrackingBackgroundTimeoutValue
                                                   : gMinTrackingTimeoutValue)
                                   : (isBackground ? gMinBackgroundTimeoutValue
                                                   : gMinTimeoutValue);
  return std::max(minValue, value);
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

// The number of queued runnables within the TabGroup ThrottledEventQueue
// at which to begin applying back pressure to the window.
#define DEFAULT_THROTTLED_EVENT_QUEUE_BACK_PRESSURE 5000
static uint32_t gThrottledEventQueueBackPressure;

// The amount of delay to apply to timers when back pressure is triggered.
// As the length of the ThrottledEventQueue grows delay is increased.  The
// delay is scaled such that every kThrottledEventQueueBackPressure runnables
// in the queue equates to an additional kBackPressureDelayMS.
#define DEFAULT_BACK_PRESSURE_DELAY_MS 250
static uint32_t gBackPressureDelayMS;

// This defines a limit for how much the delay must drop before we actually
// reduce back pressure throttle amount.  This makes the throttle delay
// a bit "sticky" once we enter back pressure.
#define DEFAULT_BACK_PRESSURE_DELAY_REDUCTION_THRESHOLD_MS 1000
static uint32_t gBackPressureDelayReductionThresholdMS;

// The minimum delay we can reduce back pressure to before we just floor
// the value back to zero.  This allows us to ensure that we can exit
// back pressure event if there are always a small number of runnables
// queued up.
#define DEFAULT_BACK_PRESSURE_DELAY_MINIMUM_MS 100
static uint32_t gBackPressureDelayMinimumMS;

// Convert a ThrottledEventQueue length to a timer delay in milliseconds.
// This will return a value between 0 and INT32_MAX.
int32_t
CalculateNewBackPressureDelayMS(uint32_t aBacklogDepth)
{
  double multiplier = static_cast<double>(aBacklogDepth) /
                      static_cast<double>(gThrottledEventQueueBackPressure);
  double value = static_cast<double>(gBackPressureDelayMS) * multiplier;
  // Avoid overflow
  if (value > INT32_MAX) {
    value = INT32_MAX;
  }

  // Once we get close to an empty queue just floor the delay back to zero.
  // We want to ensure we don't get stuck in a condition where there is a
  // small amount of delay remaining due to an active, but reasonable, queue.
  else if (value < static_cast<double>(gBackPressureDelayMinimumMS)) {
    value = 0;
  }
  return static_cast<int32_t>(value);
}

} // anonymous namespace

TimeoutManager::TimeoutManager(nsGlobalWindow& aWindow)
  : mWindow(aWindow),
    mExecutor(new TimeoutExecutor(this)),
    mTimeoutIdCounter(1),
    mNextFiringId(InvalidFiringId + 1),
    mRunningTimeout(nullptr),
    mIdleCallbackTimeoutCounter(1),
    mBackPressureDelayMS(0),
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
  Preferences::AddIntVarCache(&gMinTimeoutValue,
                              "dom.min_timeout_value",
                              DEFAULT_MIN_TIMEOUT_VALUE);
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

  Preferences::AddUintVarCache(&gThrottledEventQueueBackPressure,
                               "dom.timeout.throttled_event_queue_back_pressure",
                               DEFAULT_THROTTLED_EVENT_QUEUE_BACK_PRESSURE);
  Preferences::AddUintVarCache(&gBackPressureDelayMS,
                               "dom.timeout.back_pressure_delay_ms",
                               DEFAULT_BACK_PRESSURE_DELAY_MS);
  Preferences::AddUintVarCache(&gBackPressureDelayReductionThresholdMS,
                               "dom.timeout.back_pressure_delay_reduction_threshold_ms",
                               DEFAULT_BACK_PRESSURE_DELAY_REDUCTION_THRESHOLD_MS);
  Preferences::AddUintVarCache(&gBackPressureDelayMinimumMS,
                               "dom.timeout.back_pressure_delay_minimum_ms",
                               DEFAULT_BACK_PRESSURE_DELAY_MINIMUM_MS);

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
  timeout->mIsInterval = aIsInterval;
  timeout->mInterval = interval;
  timeout->mScriptHandler = aHandler;
  timeout->mReason = aReason;

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

  // Now clamp the actual interval we will use for the timer based on
  uint32_t nestingLevel = sNestingLevel + 1;
  uint32_t realInterval = interval;
  if (aIsInterval || nestingLevel >= DOM_CLAMP_TIMEOUT_NESTING_LEVEL ||
      mBackPressureDelayMS > 0 || mWindow.IsBackgroundInternal() ||
      timeout->mIsTracking) {
    // Don't allow timeouts less than DOMMinTimeoutValue() from
    // now...
    realInterval = std::max(realInterval,
                            uint32_t(DOMMinTimeoutValue(timeout->mIsTracking)));
  }

  timeout->mWindow = &mWindow;

  TimeDuration delta = TimeDuration::FromMilliseconds(realInterval);
  timeout->SetWhenOrTimeRemaining(TimeStamp::Now(), delta);

  // If we're not suspended, then set the timer.
  if (!mWindow.IsSuspended()) {
    MOZ_ASSERT(!timeout->When().IsNull());

    nsresult rv = mExecutor->MaybeSchedule(timeout->When());
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

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
           int(mWindow.IsBackgroundInternal()), realInterval,
           timeout->mIsTracking ? "" : "non-",
           timeout->mTimeoutId));

  return NS_OK;
}

void
TimeoutManager::ClearTimeout(int32_t aTimerId, Timeout::Reason aReason)
{
  uint32_t timerId = (uint32_t)aTimerId;

  bool firstTimeout = true;

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

  if (!firstTimeout) {
    return;
  }

  // If the first timeout was cancelled we need to stop the executor and
  // restart at the next soonest deadline.
  mExecutor->Cancel();

  OrderedTimeoutIterator iter(mNormalTimeouts,
                              mTrackingTimeouts,
                              nullptr,
                              nullptr);
  Timeout* nextTimeout = iter.Next();
  if (nextTimeout) {
    MOZ_ALWAYS_SUCCEEDS(mExecutor->MaybeSchedule(nextTimeout->When()));
  }
}

void
TimeoutManager::RunTimeout(const TimeStamp& aNow, const TimeStamp& aTargetDeadline)
{
  MOZ_DIAGNOSTIC_ASSERT(!aNow.IsNull());
  MOZ_DIAGNOSTIC_ASSERT(!aTargetDeadline.IsNull());

  if (mWindow.IsSuspended()) {
    return;
  }

  NS_ASSERTION(!mWindow.IsFrozen(), "Timeout running on a window in the bfcache!");

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

  Timeout* last_expired_normal_timeout = nullptr;
  Timeout* last_expired_tracking_timeout = nullptr;
  bool     last_expired_timeout_is_normal = false;
  Timeout* last_normal_insertion_point = nullptr;
  Timeout* last_tracking_insertion_point = nullptr;

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

  // The timeout list is kept in deadline order. Discover the latest timeout
  // whose deadline has expired. On some platforms, native timeout events fire
  // "early", but we handled that above by setting deadline to aTargetDeadline
  // if the timer fired early.  So we can stop walking if we get to timeouts
  // whose When() is greater than deadline, since once that happens we know
  // nothing past that point is expired.
  {
    // Use a nested scope in order to make sure the strong references held by
    // the iterator are freed after the loop.
    OrderedTimeoutIterator expiredIter(mNormalTimeouts,
                                       mTrackingTimeouts,
                                       nullptr,
                                       nullptr);

    uint32_t numTimersToRun = 0;

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
        last_expired_timeout_is_normal = expiredIter.PickedNormalIter();
        if (last_expired_timeout_is_normal) {
          last_expired_normal_timeout = timeout;
        } else {
          last_expired_tracking_timeout = timeout;
        }

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
    MOZ_ALWAYS_SUCCEEDS(mExecutor->MaybeSchedule(nextDeadline));
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
  dummy_normal_timeout->mFiringId = firingId;
  dummy_normal_timeout->SetDummyWhen(now);
  if (last_expired_timeout_is_normal) {
    last_expired_normal_timeout->setNext(dummy_normal_timeout);
  }

  RefPtr<Timeout> dummy_tracking_timeout = new Timeout();
  dummy_tracking_timeout->mFiringId = firingId;
  dummy_tracking_timeout->SetDummyWhen(now);
  if (!last_expired_timeout_is_normal) {
    last_expired_tracking_timeout->setNext(dummy_tracking_timeout);
  }

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
    while (true) {
      RefPtr<Timeout> timeout = runIter.Next();
      MOZ_ASSERT(timeout != dummy_normal_timeout &&
                 timeout != dummy_tracking_timeout,
                 "We should have stopped iterating before getting to the dummy timeout");
      if (!timeout) {
        // We have run out of timeouts!
        break;
      }
      runIter.UpdateIterator();

      if (timeout->mFiringId != firingId) {
        // We skip the timeout since it's on the list to run at another
        // depth.
        continue;
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

        mNormalTimeouts.SetInsertionPoint(last_normal_insertion_point);
        mTrackingTimeouts.SetInsertionPoint(last_tracking_insertion_point);

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
        // run immediately for the next timer, if it exists.
        RefPtr<Timeout> timeout = runIter.Next();
        if (timeout) {
          MOZ_ALWAYS_SUCCEEDS(mExecutor->MaybeSchedule(timeout->When()));
        }
        break;
      }
    }
  }

  // Take the dummy timeout off the head of the list
  if (dummy_normal_timeout->isInList()) {
    dummy_normal_timeout->remove();
  }
  if (dummy_tracking_timeout->isInList()) {
    dummy_tracking_timeout->remove();
  }

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
  if (queue->Length() < gThrottledEventQueueBackPressure) {
    return;
  }

  // First attempt to dispatch a runnable to update our back pressure state.  We
  // do this first in order to verify we can dispatch successfully before
  // entering the back pressure state.
  nsCOMPtr<nsIRunnable> r =
    NewNonOwningRunnableMethod<StoreRefPtrPassByPtr<nsGlobalWindow>>(this,
      &TimeoutManager::CancelOrUpdateBackPressure, &mWindow);
  nsresult rv = queue->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS_VOID(rv);

  // Since the callback was scheduled successfully we can now persist the
  // backpressure value.
  mBackPressureDelayMS = CalculateNewBackPressureDelayMS(queue->Length());

  MOZ_LOG(gLog, LogLevel::Debug,
          ("Applying %dms of back pressure to TimeoutManager %p "
           "because of a queue length of %u\n",
           mBackPressureDelayMS, this,
           queue->Length()));
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
  auto queueLength = queue ? queue->Length() : 0;
  int32_t newBackPressureDelayMS = CalculateNewBackPressureDelayMS(queueLength);

  MOZ_LOG(gLog, LogLevel::Debug,
          ("Updating back pressure from %d to %dms for TimeoutManager %p "
           "because of a queue length of %u\n",
           mBackPressureDelayMS, newBackPressureDelayMS,
           this, queueLength));

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
           (static_cast<uint32_t>(mBackPressureDelayMS) >
           (newBackPressureDelayMS + gBackPressureDelayReductionThresholdMS))) {
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
    NewNonOwningRunnableMethod<StoreRefPtrPassByPtr<nsGlobalWindow>>(this,
      &TimeoutManager::CancelOrUpdateBackPressure, &mWindow);
  MOZ_ALWAYS_SUCCEEDS(queue->Dispatch(r.forget(), NS_DISPATCH_NORMAL));
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

  nsresult rv = mExecutor->MaybeSchedule(aTimeout->When());
  NS_ENSURE_SUCCESS(rv, false);

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

  Timeouts::SortBy sortBy = mWindow.IsFrozen() ? Timeouts::SortBy::TimeRemaining
                                               : Timeouts::SortBy::TimeWhen;

  nsresult rv = mNormalTimeouts.ResetTimersForThrottleReduction(aPreviousThrottleDelayMS,
                                                                *this,
                                                                sortBy);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mTrackingTimeouts.ResetTimersForThrottleReduction(aPreviousThrottleDelayMS,
                                                         *this,
                                                         sortBy);
  NS_ENSURE_SUCCESS(rv, rv);

  OrderedTimeoutIterator iter(mNormalTimeouts,
                              mTrackingTimeouts,
                              nullptr,
                              nullptr);
  Timeout* firstTimeout = iter.Next();
  if (firstTimeout) {
    rv = mExecutor->MaybeSchedule(firstTimeout->When());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
TimeoutManager::Timeouts::ResetTimersForThrottleReduction(int32_t aPreviousThrottleDelayMS,
                                                          const TimeoutManager& aTimeoutManager,
                                                          SortBy aSortBy)
{
  TimeStamp now = TimeStamp::Now();

  // If insertion point is non-null, we're in the middle of firing timers and
  // the timers we're planning to fire all come before insertion point;
  // insertion point itself is a dummy timeout with an When() that may be
  // semi-bogus.  In that case, we don't need to do anything with insertion
  // point or anything before it, so should start at the timer after insertion
  // point, if there is one.
  // Otherwise, start at the beginning of the list.
  for (RefPtr<Timeout> timeout = InsertionPoint() ?
         InsertionPoint()->getNext() : GetFirst();
       timeout; ) {
    // It's important that this check be <= so that we guarantee that
    // taking std::max with |now| won't make a quantity equal to
    // timeout->When() below.
    if (timeout->When() <= now) {
      timeout = timeout->getNext();
      continue;
    }

    if (timeout->When() - now >
        TimeDuration::FromMilliseconds(aPreviousThrottleDelayMS)) {
      // No need to loop further.  Timeouts are sorted in When() order
      // and the ones after this point were all set up for at least
      // gMinBackgroundTimeoutValue ms and hence were not clamped.
      break;
    }

    // We reduced our throttled delay. Re-init the timer appropriately.
    // Compute the interval the timer should have had if it had not been set in a
    // background window
    TimeDuration interval =
      TimeDuration::FromMilliseconds(
          std::max(timeout->mInterval,
                   uint32_t(aTimeoutManager.
                                DOMMinTimeoutValue(timeout->mIsTracking))));
    const TimeDuration& oldInterval = timeout->ScheduledDelay();
    if (oldInterval > interval) {
      // unclamp
      TimeStamp firingTime =
        std::max(timeout->When() - oldInterval + interval, now);

      NS_ASSERTION(firingTime < timeout->When(),
                   "Our firing time should strictly decrease!");

      TimeDuration delay = firingTime - now;
      timeout->SetWhenOrTimeRemaining(now, delay);
      MOZ_DIAGNOSTIC_ASSERT(timeout->When() == firingTime);

      // Since we reset When() we need to move |timeout| to the right
      // place in the list so that it remains sorted by When().

      // Get the pointer to the next timeout now, before we move the
      // current timeout in the list.
      Timeout* nextTimeout = timeout->getNext();

      // Since we are only reducing intervals in this method we can
      // make an optimization here.  If the reduction does not cause us
      // to fall before our previous timeout then we do not have to
      // remove and re-insert the current timeout.  This is important
      // because re-insertion makes this algorithm O(n^2).  Since we
      // will typically be shifting a lot of timers at once this
      // optimization saves us a lot of work.
      Timeout* prevTimeout = timeout->getPrevious();
      if (prevTimeout && prevTimeout->When() > timeout->When()) {
        // It is safe to remove and re-insert because When() is now
        // strictly smaller than it used to be, so we know we'll insert
        // |timeout| before nextTimeout.
        NS_ASSERTION(!nextTimeout ||
                     timeout->When() < nextTimeout->When(), "How did that happen?");
        timeout->remove();
        // Insert() will reset mFiringId. Make sure to undo that.
        uint32_t firingId = timeout->mFiringId;
        Insert(timeout, aSortBy);
        timeout->mFiringId = firingId;
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
         // determines whether to set When() or TimeRemaining().
         (aSortBy == SortBy::TimeRemaining ?
          prevSibling->TimeRemaining() > aTimeout->TimeRemaining() :
          prevSibling->When() > aTimeout->When());
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

  TimeStamp now = TimeStamp::Now();
  DebugOnly<bool> _seenDummyTimeout = false;

  TimeStamp nextWakeUp;

  ForEachUnorderedTimeout([&](Timeout* aTimeout) {
    // There's a chance we're being called with RunTimeout on the stack in which
    // case we have a dummy timeout in the list that *must not* be resumed. It
    // can be identified by a null mWindow.
    if (!aTimeout->mWindow) {
      NS_ASSERTION(!_seenDummyTimeout, "More than one dummy timeout?!");
      _seenDummyTimeout = true;
      return;
    }

    // The timeout When() is set to the absolute time when the timer should
    // fire.  Recalculate the delay from now until that deadline.  If the
    // the deadline has already passed or falls within our minimum delay
    // deadline, then clamp the resulting value to the minimum delay.
    int32_t remaining = 0;
    if (aTimeout->When() > now) {
      remaining = static_cast<int32_t>((aTimeout->When() - now).ToMilliseconds());
    }
    uint32_t delay = std::max(remaining, DOMMinTimeoutValue(aTimeout->mIsTracking));
    aTimeout->SetWhenOrTimeRemaining(now, TimeDuration::FromMilliseconds(delay));

    if (nextWakeUp.IsNull() || aTimeout->When() < nextWakeUp) {
      nextWakeUp = aTimeout->When();
    }
  });

  if (!nextWakeUp.IsNull()) {
    MOZ_ALWAYS_SUCCEEDS(mExecutor->MaybeSchedule(nextWakeUp));
  }
}

void
TimeoutManager::Freeze()
{
  MOZ_LOG(gLog, LogLevel::Debug,
          ("Freeze(TimeoutManager=%p)\n", this));

  DebugOnly<bool> _seenDummyTimeout = false;

  TimeStamp now = TimeStamp::Now();
  ForEachUnorderedTimeout([&](Timeout* aTimeout) {
    if (!aTimeout->mWindow) {
      NS_ASSERTION(!_seenDummyTimeout, "More than one dummy timeout?!");
      _seenDummyTimeout = true;
      return;
    }

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

    // Set When() back to the time when the timer is supposed to fire.
    aTimeout->SetWhenOrTimeRemaining(now, aTimeout->TimeRemaining());
    MOZ_DIAGNOSTIC_ASSERT(!aTimeout->When().IsNull());
  });
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
