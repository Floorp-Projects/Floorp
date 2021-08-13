/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/SliceBudget.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/IdleTaskRunner.h"
#include "mozilla/MainThreadIdlePeriod.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/ipc/IdleSchedulerChild.h"
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

// How many cycle collected nodes to traverse between time checks.
static const int64_t kNumCCNodesBetweenTimeChecks = 1000;

// Actions performed by the GCRunner state machine.
enum class GCRunnerAction {
  WaitToMajorGC,  // We want to start a new major GC
  StartMajorGC,   // The parent says we may begin our major GC
  GCSlice,        // Run a single slice of a major GC
  None
};

struct GCRunnerStep {
  GCRunnerAction mAction;
  JS::GCReason mReason;
};

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
  static bool CCRunnerFired(TimeStamp aDeadline);

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

  // Requests
  void PokeGC(JS::GCReason aReason, JSObject* aObj, TimeDuration aDelay = 0);
  void PokeShrinkingGC();
  void PokeFullGC();
  void MaybePokeCC(TimeStamp aNow, uint32_t aSuspectedCCObjects);

  void UserIsInactive();
  void UserIsActive();

  void KillShrinkingGCTimer();
  void KillFullGCTimer();
  void KillGCRunner();
  void KillCCRunner();
  void KillAllTimersAndRunners();

  /*
   * aDelay is the delay before the first time the idle task runner runs.
   * Then it runs every
   * StaticPrefs::javascript_options_gc_delay_interslice()
   */
  void EnsureGCRunner(TimeDuration aDelay);

  void EnsureCCRunner(TimeDuration aDelay, TimeDuration aBudget);

  // State modification

  void SetNeedsFullGC(bool aNeedGC = true) { mNeedsFullGC = aNeedGC; }

  void SetWantMajorGC(JS::GCReason aReason) {
    MOZ_ASSERT(aReason != JS::GCReason::NO_REASON);

    if (mMajorGCReason != JS::GCReason::NO_REASON &&
        mMajorGCReason != JS::GCReason::USER_INACTIVE &&
        aReason != JS::GCReason::USER_INACTIVE) {
      mWantAtLeastRegularGC = true;
    }
    mMajorGCReason = aReason;

    // Force full GCs when called from reftests so that we collect dead zones
    // that have not been scheduled for collection.
    if (aReason == JS::GCReason::DOM_WINDOW_UTILS) {
      SetNeedsFullGC();
    }
  }

  // Ensure that the current runner does a cycle collection, and trigger a GC
  // after it finishes.
  void EnsureCCThenGC() {
    MOZ_ASSERT(mCCRunnerState != CCRunnerState::Inactive);
    mNeedsFullCC = true;
    mNeedsGCAfterCC = true;
  }

  // Returns false if we started and finished a major GC while waiting for a
  // response.
  [[nodiscard]] bool NoteReadyForMajorGC() {
    if (mMajorGCReason == JS::GCReason::NO_REASON || InIncrementalGC()) {
      return false;
    }
    mReadyForMajorGC = true;
    return true;
  }

  void NoteGCBegin();
  void NoteGCEnd();
  // A timer fired, but then decided not to run a GC.
  void NoteWontGC();

  void NoteGCSliceEnd(TimeDuration aSliceDuration) {
    if (mMajorGCReason == JS::GCReason::NO_REASON) {
      // Internally-triggered GCs do not wait for the parent's permission to
      // proceed. This flag won't be checked during an incremental GC anyway,
      // but it better reflects reality.
      mReadyForMajorGC = true;
    }

    // Subsequent slices should be INTER_SLICE_GC unless they are triggered by
    // something else that provides its own reason.
    mMajorGCReason = JS::GCReason::INTER_SLICE_GC;

    mGCUnnotifiedTotalTime += aSliceDuration;
  }

  bool GCRunnerFired(TimeStamp aDeadline);
  bool GCRunnerFiredDoGC(TimeStamp aDeadline, const GCRunnerStep& aStep);

  using MayGCPromise =
      MozPromise<bool, mozilla::ipc::ResponseRejectReason, true>;

  // Returns null if we shouldn't GC now (eg a GC is already running).
  static RefPtr<MayGCPromise> MayGCNow(JS::GCReason reason);

  // Check all of the various collector timers/runners and see if they are
  // waiting to fire. This does not check the Full GC Timer, as that's a
  // more expensive collection we run on a long timer.
  void RunNextCollectorTimer(JS::GCReason aReason,
                             mozilla::TimeStamp aDeadline);

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
  uint32_t NoteForgetSkippableComplete(TimeStamp aNow,
                                       uint32_t aSuspectedBeforeForgetSkippable,
                                       uint32_t aSuspectedCCObjects) {
    mLastForgetSkippableEndTime = aNow;
    mPreviousSuspectedCount = aSuspectedCCObjects;
    mCleanupsSinceLastGC++;
    return aSuspectedBeforeForgetSkippable - aSuspectedCCObjects;
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
  void NoteForgetSkippableOnlyCycle(TimeStamp aNow) {
    mLastForgetSkippableCycleEndTime = aNow;
  }

  void Shutdown() {
    mDidShutdown = true;
    KillAllTimersAndRunners();
  }

  // Scheduling

  // Return a budget along with a boolean saying whether to prefer to run short
  // slices and stop rather than continuing to the next phase of cycle
  // collection.
  js::SliceBudget ComputeCCSliceBudget(TimeStamp aDeadline,
                                       TimeStamp aCCBeginTime,
                                       TimeStamp aPrevSliceEndTime,
                                       TimeStamp aNow,
                                       bool* aPreferShorterSlices) const;

  TimeDuration ComputeInterSliceGCBudget(TimeStamp aDeadline,
                                         TimeStamp aNow) const;

  bool ShouldForgetSkippable(uint32_t aSuspectedCCObjects) const {
    // Only do a forget skippable if there are more than a few new objects
    // or we're doing the initial forget skippables.
    return ((mPreviousSuspectedCount + 100) <= aSuspectedCCObjects) ||
           mCleanupsSinceLastGC < kMajorForgetSkippableCalls;
  }

  // There is reason to suspect that there may be a significant amount of
  // garbage to cycle collect: either we just finished a GC, or the purple
  // buffer is getting really big, or it's getting somewhat big and it has been
  // too long since the last CC.
  bool IsCCNeeded(TimeStamp aNow, uint32_t aSuspectedCCObjects) const {
    if (mNeedsFullCC) {
      return true;
    }
    return aSuspectedCCObjects > kCCPurpleLimit ||
           (aSuspectedCCObjects > kCCForcedPurpleLimit && mLastCCEndTime &&
            aNow - mLastCCEndTime > kCCForced);
  }

  bool ShouldScheduleCC(TimeStamp aNow, uint32_t aSuspectedCCObjects) const;

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
    if (mCCRunner) {
      return;
    }

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

  GCRunnerStep GetNextGCRunnerAction() const;

  CCRunnerStep AdvanceCCRunner(TimeStamp aDeadline, TimeStamp aNow,
                               uint32_t aSuspectedCCObjects);

  // aStartTimeStamp : when the ForgetSkippable timer fired. This may be some
  // time ago, if an incremental GC needed to be finished.
  js::SliceBudget ComputeForgetSkippableBudget(TimeStamp aStartTimeStamp,
                                               TimeStamp aDeadline);

 private:
  // State

  // An incremental GC is in progress, which blocks the CC from running for its
  // duration (or until it goes too long and is finished synchronously.)
  bool mInIncrementalGC = false;

  // The parent process is ready for us to do a major GC.
  bool mReadyForMajorGC = false;

  // When a shrinking GC has been requested but we back-out, if this is true
  // we run a non-shrinking GC.
  bool mWantAtLeastRegularGC = false;

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

  TimeDuration mGCUnnotifiedTotalTime;

  RefPtr<IdleTaskRunner> mGCRunner;
  RefPtr<IdleTaskRunner> mCCRunner;
  nsITimer* mShrinkingGCTimer = nullptr;
  nsITimer* mFullGCTimer = nullptr;

  JS::GCReason mMajorGCReason = JS::GCReason::NO_REASON;

  bool mIsCompactingOnUserInactive = false;
  bool mUserIsActive = true;

 public:
  uint32_t mCCollectedWaitingForGC = 0;
  uint32_t mCCollectedZonesWaitingForGC = 0;
  uint32_t mLikelyShortLivingObjectsNeedingGC = 0;

  // Configuration parameters

  TimeDuration mActiveIntersliceGCBudget = TimeDuration::FromMilliseconds(5);
};

}  // namespace mozilla
