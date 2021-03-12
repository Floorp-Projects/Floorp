/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/SliceBudget.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/MainThreadIdlePeriod.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "nsCycleCollector.h"
#include "nsJSEnvironment.h"

namespace mozilla {

static const TimeDuration kOneMinute = TimeDuration::FromSeconds(60.0f);

// The amount of time we wait between a request to CC (after GC ran)
// and doing the actual CC.
static const TimeDuration kCCDelay = TimeDuration::FromSeconds(6);

static const TimeDuration kCCSkippableDelay =
    TimeDuration::FromMilliseconds(250);

// In case the cycle collector isn't run at all, we don't want forget skippables
// to run too often. So limit the forget skippable cycle to start at earliest 2
// seconds after the end of the previous cycle.
static const TimeDuration kTimeBetweenForgetSkippableCycles =
    TimeDuration::FromSeconds(2);

// ForgetSkippable is usually fast, so we can use small budgets.
// This isn't a real budget but a hint to IdleTaskRunner whether there
// is enough time to call ForgetSkippable.
static const TimeDuration kForgetSkippableSliceDuration =
    TimeDuration::FromMilliseconds(2);

// Maximum amount of time that should elapse between incremental CC slices
static const TimeDuration kICCIntersliceDelay =
    TimeDuration::FromMilliseconds(64);

// Time budget for an incremental CC slice when using timer to run it.
static const TimeDuration kICCSliceBudget = TimeDuration::FromMilliseconds(3);
// Minimum budget for an incremental CC slice when using idle time to run it.
static const TimeDuration kIdleICCSliceBudget =
    TimeDuration::FromMilliseconds(2);

// Maximum total duration for an ICC
static const TimeDuration kMaxICCDuration = TimeDuration::FromSeconds(2);

// Force a CC after this long if there's more than NS_CC_FORCED_PURPLE_LIMIT
// objects in the purple buffer.
static const TimeDuration kCCForced = kOneMinute * 2;
static const uint32_t kCCForcedPurpleLimit = 10;

// Don't allow an incremental GC to lock out the CC for too long.
static const TimeDuration kMaxCCLockedoutTime = TimeDuration::FromSeconds(30);

// Trigger a CC if the purple buffer exceeds this size when we check it.
static const uint32_t kCCPurpleLimit = 200;

enum class CCRunnerAction {
  None,
  ForgetSkippable,
  CleanupContentUnbinder,
  CleanupDeferred,
  CycleCollect,
  StopRunning
};

enum CCRunnerYield { Continue, Yield };

enum CCRunnerForgetSkippableRemoveChildless {
  KeepChildless = false,
  RemoveChildless = true
};

struct CCRunnerStep {
  // The action to scheduler is instructing the caller to perform.
  CCRunnerAction mAction;

  // Whether to stop processing actions for this invocation of the timer
  // callback.
  CCRunnerYield mYield;

  // If the action is ForgetSkippable, then whether to remove childless nodes
  // or not. (ForgetSkippable is the only action requiring a parameter; if
  // that changes, this will become a union.)
  CCRunnerForgetSkippableRemoveChildless mRemoveChildless;
};

class CCGCScheduler {
 public:
  // Mockable functions to interface with the code being scheduled.

  // Current time. In real usage, this will just return TimeStamp::Now(), but
  // tests can reimplement it to return a value controlled by the test.
  static inline TimeStamp Now();

  // Number of entries in the purple buffer (those objects whose ref counts
  // have been decremented since the previous CC, roughly), and are therefore
  // "suspected" of being members of cyclic garbage.
  static inline uint32_t SuspectedCCObjects();

  // Parameter setting

  void SetActiveIntersliceGCBudget(TimeDuration aDuration) {
    mActiveIntersliceGCBudget = aDuration;
  }

  // State retrieval

  TimeDuration GetCCBlockedTime(TimeStamp aNow) const {
    MOZ_ASSERT(mInIncrementalGC);
    MOZ_ASSERT(!mCCBlockStart.IsNull());
    return aNow - mCCBlockStart;
  }

  bool InIncrementalGC() const { return mInIncrementalGC; }

  TimeStamp GetLastCCEndTime() const { return mLastCCEndTime; }

  bool IsEarlyForgetSkippable(uint32_t aN = kMajorForgetSkippableCalls) const {
    return mCleanupsSinceLastGC < aN;
  }

  bool NeedsFullGC() const { return mNeedsFullGC; }

  // State modification

  void SetNeedsFullGC(bool aNeedGC = true) { mNeedsFullGC = aNeedGC; }

  // Ensure that the current runner does a cycle collection, and trigger a GC
  // after it finishes.
  void EnsureCCThenGC() {
    MOZ_ASSERT(mCCRunnerState != CCRunnerState::Inactive);
    mNeedsFullCC = true;
    mNeedsGCAfterCC = true;
  }

  void NoteGCBegin() {
    // Treat all GC as incremental here; non-incremental GC will just appear to
    // be one slice.
    mInIncrementalGC = true;
  }

  void NoteGCEnd() {
    mInIncrementalGC = false;
    mCCBlockStart = TimeStamp();
    mInIncrementalGC = false;
    mNeedsFullCC = true;
    mHasRunGC = true;

    mCleanupsSinceLastGC = 0;
    mCCollectedWaitingForGC = 0;
    mCCollectedZonesWaitingForGC = 0;
    mLikelyShortLivingObjectsNeedingGC = 0;
  }

  // When we decide to do a cycle collection but we're in the middle of an
  // incremental GC, the CC is "locked out" until the GC completes -- unless
  // the wait is too long, and we decide to finish the incremental GC early.
  void BlockCC(TimeStamp aNow) {
    MOZ_ASSERT(mInIncrementalGC);
    MOZ_ASSERT(mCCBlockStart.IsNull());
    mCCBlockStart = aNow;
  }

  void UnblockCC() { mCCBlockStart = TimeStamp(); }

  // Returns the number of purple buffer items that were processed and removed.
  uint32_t NoteForgetSkippableComplete(
      TimeStamp aNow, uint32_t aSuspectedBeforeForgetSkippable) {
    mLastForgetSkippableEndTime = aNow;
    uint32_t suspected = SuspectedCCObjects();
    mPreviousSuspectedCount = suspected;
    mCleanupsSinceLastGC++;
    return aSuspectedBeforeForgetSkippable - suspected;
  }

  // After collecting cycles, record the results that are used in scheduling
  // decisions.
  void NoteCycleCollected(const CycleCollectorResults& aResults) {
    mCCollectedWaitingForGC += aResults.mFreedGCed;
    mCCollectedZonesWaitingForGC += aResults.mFreedJSZones;
  }

  // This is invoked when the whole process of collection is done -- i.e., CC
  // preparation (eg ForgetSkippables), the CC itself, and the optional
  // followup GC. There really ought to be a separate name for the overall CC
  // as opposed to the actual cycle collection portion.
  void NoteCCEnd(TimeStamp aWhen) {
    mLastCCEndTime = aWhen;
    mNeedsFullCC = false;

    // The GC for this CC has already been requested.
    mNeedsGCAfterCC = false;
  }

  // The CC was abandoned without running a slice, so we only did forget
  // skippables. Prevent running another cycle soon.
  void NoteForgetSkippableOnlyCycle() {
    mLastForgetSkippableCycleEndTime = Now();
  }

  void Shutdown() { mDidShutdown = true; }

  // Scheduling

  // Return a budget along with a boolean saying whether to prefer to run short
  // slices and stop rather than continuing to the next phase of cycle
  // collection.
  inline js::SliceBudget ComputeCCSliceBudget(TimeStamp aDeadline,
                                              TimeStamp aCCBeginTime,
                                              TimeStamp aPrevSliceEndTime,
                                              bool* aPreferShorterSlices) const;

  inline TimeDuration ComputeInterSliceGCBudget(TimeStamp aDeadline,
                                                TimeStamp aNow) const;

  bool ShouldForgetSkippable() const {
    // Only do a forget skippable if there are more than a few new objects
    // or we're doing the initial forget skippables.
    return ((mPreviousSuspectedCount + 100) <= SuspectedCCObjects()) ||
           mCleanupsSinceLastGC < kMajorForgetSkippableCalls;
  }

  // There is reason to suspect that there may be a significant amount of
  // garbage to cycle collect: either we just finished a GC, or the purple
  // buffer is getting really big, or it's getting somewhat big and it has been
  // too long since the last CC.
  bool IsCCNeeded(TimeStamp aNow = Now()) const {
    if (mNeedsFullCC) {
      return true;
    }
    uint32_t suspected = SuspectedCCObjects();
    return suspected > kCCPurpleLimit ||
           (suspected > kCCForcedPurpleLimit && mLastCCEndTime &&
            aNow - mLastCCEndTime > kCCForced);
  }

  inline bool ShouldScheduleCC() const;

  // If we collected a substantial amount of cycles, poke the GC since more
  // objects might be unreachable now.
  bool NeedsGCAfterCC() const {
    return mCCollectedWaitingForGC > 250 || mCCollectedZonesWaitingForGC > 0 ||
           mLikelyShortLivingObjectsNeedingGC > 2500 || mNeedsGCAfterCC;
  }

  bool IsLastEarlyCCTimer(int32_t aCurrentFireCount) const {
    int32_t numEarlyTimerFires =
        std::max(int32_t(mCCDelay / kCCSkippableDelay) - 2, 1);

    return aCurrentFireCount >= numEarlyTimerFires;
  }

  enum class CCRunnerState {
    Inactive,
    ReducePurple,
    CleanupChildless,
    CleanupContentUnbinder,
    CleanupDeferred,
    StartCycleCollection,
    CycleCollecting,
    Canceled,
    NumStates
  };

  void InitCCRunnerStateMachine(CCRunnerState initialState) {
    // The state machine should always have been deactivated after the previous
    // collection, however far that collection may have gone.
    MOZ_ASSERT(mCCRunnerState == CCRunnerState::Inactive,
               "DeactivateCCRunner should have been called");
    mCCRunnerState = initialState;

    // Currently, there are only two entry points to the non-Inactive part of
    // the state machine.
    if (initialState == CCRunnerState::ReducePurple) {
      mCCDelay = kCCDelay;
      mCCRunnerEarlyFireCount = 0;
    } else if (initialState == CCRunnerState::CycleCollecting) {
      // Nothing needed.
    } else {
      MOZ_CRASH("Invalid initial state");
    }
  }

  void DeactivateCCRunner() { mCCRunnerState = CCRunnerState::Inactive; }

  inline CCRunnerStep GetNextCCRunnerAction(TimeStamp aDeadline);

  // aStartTimeStamp : when the ForgetSkippable timer fired. This may be some
  // time ago, if an incremental GC needed to be finished.
  js::SliceBudget ComputeForgetSkippableBudget(TimeStamp aStartTimeStamp,
                                               TimeStamp aDeadline);

 private:
  // State

  // An incremental GC is in progress, which blocks the CC from running for its
  // duration (or until it goes too long and is finished synchronously.)
  bool mInIncrementalGC = false;

  // When the CC started actually waiting for the GC to finish. This will be
  // set to non-null at a later time than mCCLockedOut.
  TimeStamp mCCBlockStart;

  bool mDidShutdown = false;

  TimeStamp mLastForgetSkippableEndTime;
  uint32_t mForgetSkippableCounter = 0;
  TimeStamp mForgetSkippableFrequencyStartTime;
  TimeStamp mLastCCEndTime;
  TimeStamp mLastForgetSkippableCycleEndTime;

  CCRunnerState mCCRunnerState = CCRunnerState::Inactive;
  int32_t mCCRunnerEarlyFireCount = 0;
  TimeDuration mCCDelay = kCCDelay;

  // Prevent the very first CC from running before we have GC'd and set the
  // gray bits.
  bool mHasRunGC = false;

  bool mNeedsFullCC = false;
  bool mNeedsFullGC = true;
  bool mNeedsGCAfterCC = false;
  uint32_t mPreviousSuspectedCount = 0;

  uint32_t mCleanupsSinceLastGC = UINT32_MAX;

 public:
  uint32_t mCCollectedWaitingForGC = 0;
  uint32_t mCCollectedZonesWaitingForGC = 0;
  uint32_t mLikelyShortLivingObjectsNeedingGC = 0;

  // Configuration parameters

  TimeDuration mActiveIntersliceGCBudget = TimeDuration::FromMilliseconds(5);
};

js::SliceBudget CCGCScheduler::ComputeCCSliceBudget(
    TimeStamp aDeadline, TimeStamp aCCBeginTime, TimeStamp aPrevSliceEndTime,
    bool* aPreferShorterSlices) const {
  TimeStamp now = Now();

  *aPreferShorterSlices =
      aDeadline.IsNull() || (aDeadline - now) < kICCSliceBudget;

  TimeDuration baseBudget =
      aDeadline.IsNull() ? kICCSliceBudget : aDeadline - now;

  if (aCCBeginTime.IsNull()) {
    // If no CC is in progress, use the standard slice time.
    return js::SliceBudget(baseBudget);
  }

  // Only run a limited slice if we're within the max running time.
  MOZ_ASSERT(now >= aCCBeginTime);
  TimeDuration runningTime = now - aCCBeginTime;
  if (runningTime >= kMaxICCDuration) {
    return js::SliceBudget::unlimited();
  }

  const TimeDuration maxSlice =
      TimeDuration::FromMilliseconds(MainThreadIdlePeriod::GetLongIdlePeriod());

  // Try to make up for a delay in running this slice.
  MOZ_ASSERT(now >= aPrevSliceEndTime);
  double sliceDelayMultiplier = (now - aPrevSliceEndTime) / kICCIntersliceDelay;
  TimeDuration delaySliceBudget =
      std::min(baseBudget.MultDouble(sliceDelayMultiplier), maxSlice);

  // Increase slice budgets up to |maxSlice| as we approach
  // half way through the ICC, to avoid large sync CCs.
  double percentToHalfDone =
      std::min(2.0 * (runningTime / kMaxICCDuration), 1.0);
  TimeDuration laterSliceBudget = maxSlice.MultDouble(percentToHalfDone);

  // Note: We may have already overshot the deadline, in which case
  // baseBudget will be negative and we will end up returning
  // laterSliceBudget.
  return js::SliceBudget(
      std::max({delaySliceBudget, laterSliceBudget, baseBudget}));
}

inline TimeDuration CCGCScheduler::ComputeInterSliceGCBudget(
    TimeStamp aDeadline, TimeStamp aNow) const {
  // We use longer budgets when the CC has been locked out but the CC has
  // tried to run since that means we may have a significant amount of
  // garbage to collect and it's better to GC in several longer slices than
  // in a very long one.
  TimeDuration budget =
      aDeadline.IsNull() ? mActiveIntersliceGCBudget * 2 : aDeadline - aNow;
  if (!mCCBlockStart) {
    return budget;
  }

  TimeDuration blockedTime = aNow - mCCBlockStart;
  TimeDuration maxSliceGCBudget = mActiveIntersliceGCBudget * 10;
  double percentOfBlockedTime =
      std::min(blockedTime / kMaxCCLockedoutTime, 1.0);
  return std::max(budget, maxSliceGCBudget.MultDouble(percentOfBlockedTime));
}

bool CCGCScheduler::ShouldScheduleCC() const {
  if (!mHasRunGC) {
    return false;
  }

  TimeStamp now = Now();

  // Don't run consecutive CCs too often.
  if (mCleanupsSinceLastGC && !mLastCCEndTime.IsNull()) {
    if (now - mLastCCEndTime < kCCDelay) {
      return false;
    }
  }

  // If GC hasn't run recently and forget skippable only cycle was run,
  // don't start a new cycle too soon.
  if ((mCleanupsSinceLastGC > kMajorForgetSkippableCalls) &&
      !mLastForgetSkippableCycleEndTime.IsNull()) {
    if (now - mLastForgetSkippableCycleEndTime <
        kTimeBetweenForgetSkippableCycles) {
      return false;
    }
  }

  return IsCCNeeded(now);
}

CCRunnerStep CCGCScheduler::GetNextCCRunnerAction(TimeStamp aDeadline) {
  struct StateDescriptor {
    // When in this state, should we first check to see if we still have
    // enough reason to CC?
    bool mCanAbortCC;

    // If we do decide to abort the CC, should we still try to forget
    // skippables one more time?
    bool mTryFinalForgetSkippable;
  };

  // The state descriptors for Inactive and Canceled will never actually be
  // used. We will never call this function while Inactive, and Canceled is
  // handled specially at the beginning.
  constexpr StateDescriptor stateDescriptors[] = {
      {false, false},  /* CCRunnerState::Inactive */
      {false, false},  /* CCRunnerState::ReducePurple */
      {true, true},    /* CCRunnerState::CleanupChildless */
      {true, false},   /* CCRunnerState::CleanupContentUnbinder */
      {false, false},  /* CCRunnerState::CleanupDeferred */
      {false, false},  /* CCRunnerState::StartCycleCollection */
      {false, false},  /* CCRunnerState::CycleCollecting */
      {false, false}}; /* CCRunnerState::Canceled */
  static_assert(
      ArrayLength(stateDescriptors) == size_t(CCRunnerState::NumStates),
      "need one state descriptor per state");
  const StateDescriptor& desc = stateDescriptors[int(mCCRunnerState)];

  // Make sure we initialized the state machine.
  MOZ_ASSERT(mCCRunnerState != CCRunnerState::Inactive);

  if (mDidShutdown) {
    return {CCRunnerAction::StopRunning, Yield};
  }

  if (mCCRunnerState == CCRunnerState::Canceled) {
    // When we cancel a cycle, there may have been a final ForgetSkippable.
    return {CCRunnerAction::StopRunning, Yield};
  }

  TimeStamp now = Now();

  if (InIncrementalGC()) {
    if (mCCBlockStart.IsNull()) {
      BlockCC(now);

      // If we have reached the CycleCollecting state, then ignore CC timer
      // fires while incremental GC is running. (Running ICC during an IGC
      // would cause us to synchronously finish the GC, which is bad.)
      //
      // If we have not yet started cycle collecting, then reset our state so
      // that we run forgetSkippable often enough before CC. Because of reduced
      // mCCDelay, forgetSkippable will be called just a few times.
      //
      // The kMaxCCLockedoutTime limit guarantees that we end up calling
      // forgetSkippable and CycleCollectNow eventually.

      if (mCCRunnerState != CCRunnerState::CycleCollecting) {
        mCCRunnerState = CCRunnerState::ReducePurple;
        mCCRunnerEarlyFireCount = 0;
        mCCDelay = kCCDelay / int64_t(3);
      }
      return {CCRunnerAction::None, Yield};
    }

    if (GetCCBlockedTime(now) < kMaxCCLockedoutTime) {
      return {CCRunnerAction::None, Yield};
    }

    // Locked out for too long, so proceed and finish the incremental GC
    // synchronously.
  }

  // For states that aren't just continuations of previous states, check
  // whether a CC is still needed (after doing various things to reduce the
  // purple buffer).
  if (desc.mCanAbortCC && !IsCCNeeded(now)) {
    // If we don't pass the threshold for wanting to cycle collect, stop now
    // (after possibly doing a final ForgetSkippable).
    mCCRunnerState = CCRunnerState::Canceled;
    NoteForgetSkippableOnlyCycle();

    // Preserve the previous code's idea of when to check whether a
    // ForgetSkippable should be fired.
    if (desc.mTryFinalForgetSkippable && ShouldForgetSkippable()) {
      // The Canceled state will make us StopRunning after this action is
      // performed (see conditional at top of function).
      return {CCRunnerAction::ForgetSkippable, Yield, KeepChildless};
    }

    return {CCRunnerAction::StopRunning, Yield};
  }

  switch (mCCRunnerState) {
      // ReducePurple: a GC ran (or we otherwise decided to try CC'ing). Wait
      // for some amount of time (kCCDelay, or less if incremental GC blocked
      // this CC) while firing regular ForgetSkippable actions before continuing
      // on.
    case CCRunnerState::ReducePurple:
      ++mCCRunnerEarlyFireCount;
      if (IsLastEarlyCCTimer(mCCRunnerEarlyFireCount)) {
        mCCRunnerState = CCRunnerState::CleanupChildless;
      }

      if (ShouldForgetSkippable()) {
        return {CCRunnerAction::ForgetSkippable, Yield, KeepChildless};
      }

      if (aDeadline.IsNull()) {
        return {CCRunnerAction::None, Yield};
      }

      // If we're called during idle time, try to find some work to do by
      // advancing to the next state, effectively bypassing some possible forget
      // skippable calls.
      mCCRunnerState = CCRunnerState::CleanupChildless;

      // Continue on to CleanupChildless, but only after checking IsCCNeeded
      // again.
      return {CCRunnerAction::None, Continue};

      // CleanupChildless: do a stronger ForgetSkippable that removes nodes with
      // no children in the cycle collector graph. This state is split into 3
      // parts; the other Cleanup* actions will happen within the same callback
      // (unless the ForgetSkippable shrinks the purple buffer enough for the CC
      // to be skipped entirely.)
    case CCRunnerState::CleanupChildless:
      mCCRunnerState = CCRunnerState::CleanupContentUnbinder;
      return {CCRunnerAction::ForgetSkippable, Yield, RemoveChildless};

      // CleanupContentUnbinder: continuing cleanup, clear out the content
      // unbinder.
    case CCRunnerState::CleanupContentUnbinder:
      if (aDeadline.IsNull()) {
        // Non-idle (waiting) callbacks skip the rest of the cleanup, but still
        // wait for another fire before the actual CC.
        mCCRunnerState = CCRunnerState::StartCycleCollection;
        return {CCRunnerAction::None, Yield};
      }

      // Running in an idle callback.

      // The deadline passed, so go straight to CC in the next slice.
      if (now >= aDeadline) {
        mCCRunnerState = CCRunnerState::StartCycleCollection;
        return {CCRunnerAction::None, Yield};
      }

      mCCRunnerState = CCRunnerState::CleanupDeferred;
      return {CCRunnerAction::CleanupContentUnbinder, Continue};

      // CleanupDeferred: continuing cleanup, do deferred deletion.
    case CCRunnerState::CleanupDeferred:
      MOZ_ASSERT(!aDeadline.IsNull(),
                 "Should only be in CleanupDeferred state when idle");

      // Our efforts to avoid a CC have failed. Let the timer fire once more
      // to trigger a CC.
      mCCRunnerState = CCRunnerState::StartCycleCollection;
      if (now >= aDeadline) {
        // The deadline passed, go straight to CC in the next slice.
        return {CCRunnerAction::None, Yield};
      }

      return {CCRunnerAction::CleanupDeferred, Yield};

      // StartCycleCollection: start actually doing cycle collection slices.
    case CCRunnerState::StartCycleCollection:
      // We are in the final timer fire and still meet the conditions for
      // triggering a CC. Let RunCycleCollectorSlice finish the current IGC if
      // any, because that will allow us to include the GC time in the CC pause.
      mCCRunnerState = CCRunnerState::CycleCollecting;
      [[fallthrough]];

      // CycleCollecting: continue running slices until done.
    case CCRunnerState::CycleCollecting:
      return {CCRunnerAction::CycleCollect, Yield};

    default:
      MOZ_CRASH("Unexpected CCRunner state");
  };
}

inline js::SliceBudget CCGCScheduler::ComputeForgetSkippableBudget(
    TimeStamp aStartTimeStamp, TimeStamp aDeadline) {
  if (mForgetSkippableFrequencyStartTime.IsNull()) {
    mForgetSkippableFrequencyStartTime = aStartTimeStamp;
  } else if (aStartTimeStamp - mForgetSkippableFrequencyStartTime >
             kOneMinute) {
    TimeStamp startPlusMinute = mForgetSkippableFrequencyStartTime + kOneMinute;

    // If we had forget skippables only at the beginning of the interval, we
    // still want to use the whole time, minute or more, for frequency
    // calculation. mLastForgetSkippableEndTime is needed if forget skippable
    // takes enough time to push the interval to be over a minute.
    TimeStamp endPoint = std::max(startPlusMinute, mLastForgetSkippableEndTime);

    // Duration in minutes.
    double duration =
        (endPoint - mForgetSkippableFrequencyStartTime).ToSeconds() / 60;
    uint32_t frequencyPerMinute = uint32_t(mForgetSkippableCounter / duration);
    Telemetry::Accumulate(Telemetry::FORGET_SKIPPABLE_FREQUENCY,
                          frequencyPerMinute);
    mForgetSkippableCounter = 0;
    mForgetSkippableFrequencyStartTime = aStartTimeStamp;
  }
  ++mForgetSkippableCounter;

  TimeDuration budgetTime =
      aDeadline ? (aDeadline - aStartTimeStamp) : kForgetSkippableSliceDuration;
  return js::SliceBudget(budgetTime);
}

}  // namespace mozilla
