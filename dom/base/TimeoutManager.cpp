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
#include "TimeoutBudgetManager.h"
#include "mozilla/net/WebSocketEventService.h"
#include "mozilla/MediaManager.h"

#ifdef MOZ_WEBRTC
#include "IPeerConnection.h"
#endif // MOZ_WEBRTC

using namespace mozilla;
using namespace mozilla::dom;

static LazyLogModule gLog("Timeout");

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
static int32_t gTimeoutThrottlingDelay = 0;
static bool    gAnnotateTrackingChannels = false;

#define DEFAULT_BACKGROUND_BUDGET_REGENERATION_FACTOR 100 // 1ms per 100ms
#define DEFAULT_FOREGROUND_BUDGET_REGENERATION_FACTOR 1 // 1ms per 1ms
#define DEFAULT_BACKGROUND_THROTTLING_MAX_BUDGET 50 // 50ms
#define DEFAULT_FOREGROUND_THROTTLING_MAX_BUDGET -1 // infinite
#define DEFAULT_BUDGET_THROTTLING_MAX_DELAY 15000 // 15s
#define DEFAULT_ENABLE_BUDGET_TIMEOUT_THROTTLING false
static int32_t gBackgroundBudgetRegenerationFactor = 0;
static int32_t gForegroundBudgetRegenerationFactor = 0;
static int32_t gBackgroundThrottlingMaxBudget = 0;
static int32_t gForegroundThrottlingMaxBudget = 0;
static int32_t gBudgetThrottlingMaxDelay = 0;
static bool    gEnableBudgetTimeoutThrottling = false;

// static
const uint32_t TimeoutManager::InvalidFiringId = 0;

namespace
{
double
GetRegenerationFactor(bool aIsBackground)
{
  // Lookup function for "dom.timeout.{background,
  // foreground}_budget_regeneration_rate".

  // Returns the rate of regeneration of the execution budget as a
  // fraction. If the value is 1.0, the amount of time regenerated is
  // equal to time passed. At this rate we regenerate 1ms/ms. If it is
  // 0.01 the amount regenerated is 1% of time passed. At this rate we
  // regenerate 1ms/100ms, etc.
  double denominator =
    std::max(aIsBackground ? gBackgroundBudgetRegenerationFactor
                           : gForegroundBudgetRegenerationFactor,
             1);
  return 1.0 / denominator;
}

TimeDuration
GetMaxBudget(bool aIsBackground)
{
  // Lookup function for "dom.timeout.{background,
  // foreground}_throttling_max_budget".

  // Returns how high a budget can be regenerated before being
  // clamped. If this value is less or equal to zero,
  // TimeDuration::Forever() is implied.
  int32_t maxBudget = aIsBackground ? gBackgroundThrottlingMaxBudget
                                    : gForegroundThrottlingMaxBudget;
  return maxBudget > 0 ? TimeDuration::FromMilliseconds(maxBudget)
                       : TimeDuration::Forever();
}
} // namespace

//

bool
TimeoutManager::IsBackground() const
{
  return !IsActive() && mWindow.IsBackgroundInternal();
}

bool
TimeoutManager::IsActive() const
{
  // A window is considered active if:
  // * It is a chrome window
  // * It is playing audio
  // * If it is using user media
  // * If it is using WebRTC
  // * If it has open WebSockets
  // * If it has active IndexedDB databases
  //
  // Note that a window can be considered active if it is either in the
  // foreground or in the background.

  if (mWindow.IsChromeWindow()) {
    return true;
  }

  // Check if we're playing audio
  if (mWindow.AsInner()->IsPlayingAudio()) {
    return true;
  }

  // Check if there are any active IndexedDB databases
  if (mWindow.AsInner()->HasActiveIndexedDBDatabases()) {
    return true;
  }

  // Check if we have active GetUserMedia
  if (MediaManager::Exists() &&
      MediaManager::Get()->IsWindowStillActive(mWindow.WindowID())) {
    return true;
  }

  bool active = false;
#if 0
  // Check if we have active PeerConnections This doesn't actually
  // work, since we sometimes call IsActive from Resume, which in turn
  // is sometimes called from nsGlobalWindow::LeaveModalState. The
  // problem here is that LeaveModalState can be called with pending
  // exeptions on the js context, and the following call to
  // HasActivePeerConnection is a JS call, which will assert on that
  // exception. Also, calling JS is expensive so we should try to fix
  // this in some other way.
  nsCOMPtr<IPeerConnectionManager> pcManager =
    do_GetService(IPEERCONNECTION_MANAGER_CONTRACTID);

  if (pcManager && NS_SUCCEEDED(pcManager->HasActivePeerConnection(
                     mWindow.WindowID(), &active)) &&
      active) {
    return true;
  }
#endif // MOZ_WEBRTC

  // Check if we have web sockets
  RefPtr<WebSocketEventService> eventService = WebSocketEventService::Get();
  if (eventService &&
      NS_SUCCEEDED(eventService->HasListenerFor(mWindow.WindowID(), &active)) &&
      active) {
    return true;
  }

  return false;
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
  // gBudgetThrottlingMaxDelay to not entirely starve the timeouts.
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
    isBackground ? TimeDuration::FromMilliseconds(gMinBackgroundTimeoutValue)
                 : TimeDuration();
  if (mBudgetThrottleTimeouts && mExecutionBudget < TimeDuration()) {
    // Only throttle if execution budget is less than 0
    double factor = 1.0 / GetRegenerationFactor(mWindow.IsBackgroundInternal());
    return TimeDuration::Min(
      TimeDuration::FromMilliseconds(gBudgetThrottlingMaxDelay),
      TimeDuration::Max(unthrottled, -mExecutionBudget.MultDouble(factor)));
  }
  //
  return unthrottled;
}

nsresult
TimeoutManager::MaybeSchedule(const TimeStamp& aWhen, const TimeStamp& aNow)
{
  MOZ_DIAGNOSTIC_ASSERT(mExecutor);

  // Before we can schedule the executor we need to make sure that we
  // have an updated execution budget.
  UpdateBudget(aNow);
  return mExecutor->MaybeSchedule(aWhen, MinSchedulingDelay());
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

// The number of nested timeouts before we start clamping. HTML5 says 1, WebKit
// uses 5.
#define DOM_CLAMP_TIMEOUT_NESTING_LEVEL 5

TimeDuration
TimeoutManager::CalculateDelay(Timeout* aTimeout) const {
  MOZ_DIAGNOSTIC_ASSERT(aTimeout);
  TimeDuration result = aTimeout->mInterval;

  if (aTimeout->mIsInterval ||
      aTimeout->mNestingLevel >= DOM_CLAMP_TIMEOUT_NESTING_LEVEL) {
    result = TimeDuration::Max(
      result, TimeDuration::FromMilliseconds(gMinClampTimeoutValue));
  }

  if (aTimeout->mIsTracking && mThrottleTrackingTimeouts) {
    result = TimeDuration::Max(
      result, TimeDuration::FromMilliseconds(gMinTrackingTimeoutValue));
  }

  return result;
}

void
TimeoutManager::RecordExecution(Timeout* aRunningTimeout,
                                Timeout* aTimeout)
{
  if (mWindow.IsChromeWindow()) {
    return;
  }

  TimeoutBudgetManager& budgetManager = TimeoutBudgetManager::Get();
  TimeStamp now = TimeStamp::Now();

  if (aRunningTimeout) {
    // If we're running a timeout callback, record any execution until
    // now.
    TimeDuration duration = budgetManager.RecordExecution(
      now, aRunningTimeout, mWindow.IsBackgroundInternal());
    budgetManager.MaybeCollectTelemetry(now);

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

void
TimeoutManager::UpdateBudget(const TimeStamp& aNow, const TimeDuration& aDuration)
{
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
  if (mBudgetThrottleTimeouts) {
    bool isBackground = mWindow.IsBackgroundInternal();
    double factor = GetRegenerationFactor(isBackground);
    TimeDuration regenerated = (aNow - mLastBudgetUpdate).MultDouble(factor);
    // Clamp the budget to the maximum allowed budget.
    mExecutionBudget = TimeDuration::Min(
      GetMaxBudget(isBackground), mExecutionBudget - aDuration + regenerated);
  }
  mLastBudgetUpdate = aNow;
}

#define TRACKING_SEPARATE_TIMEOUT_BUCKETING_STRATEGY 0 // Consider all timeouts coming from tracking scripts as tracking
// These strategies are useful for testing.
#define ALL_NORMAL_TIMEOUT_BUCKETING_STRATEGY        1 // Consider all timeouts as normal
#define ALTERNATE_TIMEOUT_BUCKETING_STRATEGY         2 // Put every other timeout in the list of tracking timeouts
#define RANDOM_TIMEOUT_BUCKETING_STRATEGY            3 // Put timeouts into either the normal or tracking timeouts list randomly
static int32_t gTimeoutBucketingStrategy = 0;

#define DEFAULT_TIMEOUT_THROTTLING_DELAY           -1  // Only positive integers cause us to introduce a delay for
                                                       // timeout throttling.

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

// Only propagate the open window click permission if the setTimeout() is equal
// to or less than this value.
#define DEFAULT_DISABLE_OPEN_CLICK_DELAY 0
int32_t gDisableOpenClickDelay;

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
    mLastBudgetUpdate(TimeStamp::Now()),
    mExecutionBudget(GetMaxBudget(mWindow.IsBackgroundInternal())),
    mThrottleTimeouts(false),
    mThrottleTrackingTimeouts(false),
    mBudgetThrottleTimeouts(false)
{
  MOZ_DIAGNOSTIC_ASSERT(aWindow.IsInnerWindow());

  MOZ_LOG(gLog, LogLevel::Debug,
          ("TimeoutManager %p created, tracking bucketing %s\n",
           this, gAnnotateTrackingChannels ? "enabled" : "disabled"));
}

TimeoutManager::~TimeoutManager()
{
  MOZ_DIAGNOSTIC_ASSERT(mWindow.AsInner()->InnerObjectsFreed());
  MOZ_DIAGNOSTIC_ASSERT(!mThrottleTimeoutsTimer);

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
  Preferences::AddIntVarCache(&gTimeoutThrottlingDelay,
                              "dom.timeout.throttling_delay",
                              DEFAULT_TIMEOUT_THROTTLING_DELAY);

  Preferences::AddBoolVarCache(&gAnnotateTrackingChannels,
                               "privacy.trackingprotection.annotate_channels",
                               false);

  Preferences::AddUintVarCache(&gMaxConsecutiveCallbacksMilliseconds,
                               "dom.timeout.max_consecutive_callbacks_ms",
                               DEFAULT_MAX_CONSECUTIVE_CALLBACKS_MILLISECONDS);

  Preferences::AddIntVarCache(&gDisableOpenClickDelay,
                              "dom.disable_open_click_delay",
                              DEFAULT_DISABLE_OPEN_CLICK_DELAY);
  Preferences::AddIntVarCache(&gBackgroundBudgetRegenerationFactor,
                              "dom.timeout.background_budget_regeneration_rate",
                              DEFAULT_BACKGROUND_BUDGET_REGENERATION_FACTOR);
  Preferences::AddIntVarCache(&gForegroundBudgetRegenerationFactor,
                              "dom.timeout.foreground_budget_regeneration_rate",
                              DEFAULT_FOREGROUND_BUDGET_REGENERATION_FACTOR);
  Preferences::AddIntVarCache(&gBackgroundThrottlingMaxBudget,
                              "dom.timeout.background_throttling_max_budget",
                              DEFAULT_BACKGROUND_THROTTLING_MAX_BUDGET);
  Preferences::AddIntVarCache(&gForegroundThrottlingMaxBudget,
                              "dom.timeout.foreground_throttling_max_budget",
                              DEFAULT_FOREGROUND_THROTTLING_MAX_BUDGET);
  Preferences::AddIntVarCache(&gBudgetThrottlingMaxDelay,
                              "dom.timeout.budget_throttling_max_delay",
                              DEFAULT_BUDGET_THROTTLING_MAX_DELAY);
  Preferences::AddBoolVarCache(&gEnableBudgetTimeoutThrottling,
                               "dom.timeout.enable_budget_timer_throttling",
                               DEFAULT_ENABLE_BUDGET_TIMEOUT_THROTTLING);
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
  timeout->mInterval = TimeDuration::FromMilliseconds(interval);
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
      mWindow.GetPopupControlState() < openAbused) {
    // This timeout is *not* set from another timeout and it's set
    // while popups are enabled. Propagate the state to the timeout if
    // its delay (interval) is equal to or less than what
    // "dom.disable_open_click_delay" is set to (in ms).

    // This is checking |interval|, not realInterval, on purpose,
    // because our lower bound for |realInterval| could be pretty high
    // in some cases.
    if (interval <= gDisableOpenClickDelay) {
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

  MOZ_LOG(gLog,
          LogLevel::Debug,
          ("Set%s(TimeoutManager=%p, timeout=%p, delay=%i, "
           "minimum=%f, throttling=%s, state=%s(%s), realInterval=%f) "
           "returned %stracking timeout ID %u, budget=%d\n",
           aIsInterval ? "Interval" : "Timeout",
           this, timeout.get(), interval,
           (CalculateDelay(timeout) - timeout->mInterval).ToMilliseconds(),
           mThrottleTimeouts
             ? "yes"
             : (mThrottleTimeoutsTimer ? "pending" : "no"),
           IsActive() ? "active" : "inactive",
           mWindow.IsBackgroundInternal() ? "background" : "foreground",
           realInterval.ToMilliseconds(),
           timeout->mIsTracking ? "" : "non-",
           timeout->mTimeoutId,
           int(mExecutionBudget.ToMilliseconds())));

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
    MOZ_ALWAYS_SUCCEEDS(MaybeSchedule(nextTimeout->When()));
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
      if (!timeout || totalTimeLimit.IsZero() || timeout->When() > deadline) {
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
          if (elapsed >= initialTimeLimit) {
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
    MOZ_ALWAYS_SUCCEEDS(MaybeSchedule(nextDeadline, now));
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

      // If we need to reschedule a setInterval() the delay should be
      // calculated based on when its callback started to execute.  So
      // save off the last time before updating our "now" timestamp to
      // account for its callback execution time.
      TimeStamp lastCallbackTime = now;
      now = TimeStamp::Now();

      // If we have a regular interval timer, we re-schedule the
      // timeout, accounting for clock drift.
      bool needsReinsertion = RescheduleTimeout(timeout, lastCallbackTime, now);

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
            // If we ran out of execution budget we need to force a
            // reschedule. By cancelling the executor we will not run
            // immediately, but instead reschedule to the minimum
            // scheduling delay.
            if (mExecutionBudget < TimeDuration()) {
              mExecutor->Cancel();
            }

            MOZ_ALWAYS_SUCCEEDS(MaybeSchedule(timeout->When(), now));
          }
        }
        break;
      }
    }
  }
}

bool
TimeoutManager::RescheduleTimeout(Timeout* aTimeout,
                                  const TimeStamp& aLastCallbackTime,
                                  const TimeStamp& aCurrentNow)
{
  MOZ_DIAGNOSTIC_ASSERT(aLastCallbackTime <= aCurrentNow);

  if (!aTimeout->mIsInterval) {
    return false;
  }

  // Compute time to next timeout for interval timer.
  // Make sure nextInterval is at least CalculateDelay().
  TimeDuration nextInterval = CalculateDelay(aTimeout);

  TimeStamp firingTime = aLastCallbackTime + nextInterval;
  TimeDuration delay = firingTime - aCurrentNow;

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

void
TimeoutManager::ClearAllTimeouts()
{
  bool seenRunningTimeout = false;

  MOZ_LOG(gLog, LogLevel::Debug,
          ("ClearAllTimeouts(TimeoutManager=%p)\n", this));

  if (mThrottleTimeoutsTimer) {
    mThrottleTimeoutsTimer->Cancel();
    mThrottleTimeoutsTimer = nullptr;
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

  RecordExecution(currentTimeout, aTimeout);
  return currentTimeout;
}

void
TimeoutManager::EndRunningTimeout(Timeout* aTimeout)
{
  --gRunningTimeoutDepth;

  RecordExecution(mRunningTimeout, aTimeout);
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

  if (mThrottleTimeoutsTimer) {
    mThrottleTimeoutsTimer->Cancel();
    mThrottleTimeoutsTimer = nullptr;
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
  if (mWindow.AsInner()->IsDocumentLoaded() && !mThrottleTimeouts) {
    MaybeStartThrottleTimeout();
  }

  OrderedTimeoutIterator iter(mNormalTimeouts, mTrackingTimeouts);
  Timeout* nextTimeout = iter.Next();
  if (nextTimeout) {
    MOZ_ALWAYS_SUCCEEDS(MaybeSchedule(nextTimeout->When()));
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
      MOZ_ALWAYS_SUCCEEDS(MaybeSchedule(nextTimeout->When()));
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

class ThrottleTimeoutsCallback final : public nsITimerCallback
{
public:
  explicit ThrottleTimeoutsCallback(nsGlobalWindow* aWindow)
    : mWindow(aWindow)
  {
    MOZ_DIAGNOSTIC_ASSERT(aWindow->IsInnerWindow());
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

private:
  ~ThrottleTimeoutsCallback() {}

private:
  // The strong reference here keeps the Window and hence the TimeoutManager
  // object itself alive.
  RefPtr<nsGlobalWindow> mWindow;
};

NS_IMPL_ISUPPORTS(ThrottleTimeoutsCallback, nsITimerCallback)

NS_IMETHODIMP
ThrottleTimeoutsCallback::Notify(nsITimer* aTimer)
{
  mWindow->AsInner()->TimeoutManager().StartThrottlingTimeouts();
  mWindow = nullptr;
  return NS_OK;
}

}

void
TimeoutManager::StartThrottlingTimeouts()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mThrottleTimeoutsTimer);

  MOZ_LOG(gLog, LogLevel::Debug,
          ("TimeoutManager %p started to throttle tracking timeouts\n", this));

  MOZ_DIAGNOSTIC_ASSERT(!mThrottleTimeouts);
  mThrottleTimeouts = true;
  mThrottleTrackingTimeouts = true;
  mBudgetThrottleTimeouts = gEnableBudgetTimeoutThrottling;
  mThrottleTimeoutsTimer = nullptr;
}

void
TimeoutManager::OnDocumentLoaded()
{
  // The load event may be firing again if we're coming back to the page by
  // navigating through the session history, so we need to ensure to only call
  // this when mThrottleTimeouts hasn't been set yet.
  if (!mThrottleTimeouts) {
    MaybeStartThrottleTimeout();
  }
}

void
TimeoutManager::MaybeStartThrottleTimeout()
{
  if (gTimeoutThrottlingDelay <= 0 ||
      mWindow.AsInner()->InnerObjectsFreed() || mWindow.IsSuspended()) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(!mThrottleTimeouts);

  MOZ_LOG(gLog, LogLevel::Debug,
          ("TimeoutManager %p delaying tracking timeout throttling by %dms\n",
           this, gTimeoutThrottlingDelay));

  mThrottleTimeoutsTimer =
    do_CreateInstance("@mozilla.org/timer;1");
  if (!mThrottleTimeoutsTimer) {
    return;
  }

  nsCOMPtr<nsITimerCallback> callback =
    new ThrottleTimeoutsCallback(&mWindow);

  mThrottleTimeoutsTimer->InitWithCallback(
    callback, gTimeoutThrottlingDelay, nsITimer::TYPE_ONE_SHOT);
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
  RecordExecution(mRunningTimeout, nullptr);
}

void
TimeoutManager::EndSyncOperation()
{
  // If we're running a timeout, restart the measurement from here.
  RecordExecution(nullptr, mRunningTimeout);
}

nsIEventTarget*
TimeoutManager::EventTarget()
{
  return mWindow.EventTargetFor(TaskCategory::Timer);
}
