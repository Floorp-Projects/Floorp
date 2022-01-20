/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CCGCScheduler.h"

#include "mozilla/StaticPrefs_javascript.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsRefreshDriver.h"

/*
 * GC Scheduling from Firefox
 * ==========================
 *
 * See also GC Scheduling from SpiderMonkey's perspective here:
 * https://searchfox.org/mozilla-central/source/js/src/gc/Scheduling.h
 *
 * From Firefox's perspective GCs can start in 5 different ways:
 *
 *  * The JS engine just starts doing a GC for its own reasons (see above).
 *    Firefox finds out about these via a callback in nsJSEnvironment.cpp
 *  * PokeGC()
 *  * PokeFullGC()
 *  * PokeShrinkingGC()
 *  * memory-pressure GCs (via a listener in nsJSEnvironment.cpp).
 *
 * PokeGC
 * ------
 *
 * void CCGCScheduler::PokeGC(JS::GCReason aReason, JSObject* aObj,
 *                            TimeDuration aDelay)
 *
 * PokeGC provides a way for callers to say "Hey, there may be some memory
 * associated with this object (via Zone) you can collect."  PokeGC will:
 *  * add the zone to a set,
 *  * set flags including what kind of GC to run (SetWantMajorGC),
 *  * then creates the mGCRunner with a short delay.
 *
 * The delay can allow other calls to PokeGC to add their zones so they can
 * be collected together.
 *
 * See below for what happens when mGCRunner fires.
 *
 * PokeFullGC
 * ----------
 *
 * void CCGCScheduler::PokeFullGC()
 *
 * PokeFullGC will create a timer that will initiate a "full" (all zones)
 * collection.  This is usually used after a regular collection if a full GC
 * seems like a good idea (to collect inter-zone references).
 *
 * When the timer fires it will:
 *  * set flags (SetWantMajorGC),
 *  * start the mGCRunner with zero delay.
 *
 * See below for when mGCRunner fires.
 *
 * PokeShrinkingGC
 * ---------------
 *
 * void CCGCScheduler::PokeShrinkingGC()
 *
 * PokeShrinkingGC is called when Firefox's user is inactive.
 * Like PokeFullGC, PokeShrinkingGC uses a timer, but the timeout is longer
 * which should prevent the ShrinkingGC from starting if the user only
 * glances away for a brief time.  When the timer fires it will:
 *
 *  * set flags (SetWantMajorGC),
 *  * create the mGCRunner.
 *
 * There is a check if the user is still inactive in GCRunnerFired), if the
 * user has become active the shrinking GC is canceled and either a regular
 * GC (if requested, see mWantAtLeastRegularGC) or no GC is run.
 *
 * When mGCRunner fires
 * --------------------
 *
 * When mGCRunner fires it calls GCRunnerFired.  This starts in the
 * WaitToMajorGC state:
 *
 *  * If this is a parent process it jumps to the next state
 *  * If this is a content process it will ask the parent if now is a good
 *      time to do a GC.  (MayGCNow)
 *  * kill the mGCRunner
 *  * Exit
 *
 * Meanwhile the parent process will queue GC requests so that not too many
 * are running in parallel overwhelming the CPU cores (see
 * IdleSchedulerParent).
 *
 * When the promise from MayGCNow is resolved it will set some
 * state (NoteReadyForMajorGC) and restore the mGCRunner.
 *
 * When the mGCRunner runs a second time (or this is the parent process and
 * which jumped over the above logic.  It will be in the StartMajorGC state.
 * It will initiate the GC for real, usually.  If it's a shrinking GC and the
 * user is now active again it may abort.  See GCRunnerFiredDoGC().
 *
 * The runner will then run the first slice of the garbage collection.
 * Later slices are also run by the runner, the final slice kills the runner
 * from the GC callback in nsJSEnvironment.cpp.
 *
 * There is additional logic in the code to handle concurrent requests of
 * various kinds.
 */

namespace mozilla {

void CCGCScheduler::NoteGCBegin() {
  // Treat all GC as incremental here; non-incremental GC will just appear to
  // be one slice.
  mInIncrementalGC = true;
  mReadyForMajorGC = false;

  // Tell the parent process that we've started a GC (it might not know if
  // we hit a threshold in the JS engine).
  using mozilla::ipc::IdleSchedulerChild;
  IdleSchedulerChild* child = IdleSchedulerChild::GetMainThreadIdleScheduler();
  if (child) {
    child->StartedGC();
  }
}

void CCGCScheduler::NoteGCEnd() {
  mMajorGCReason = JS::GCReason::NO_REASON;

  mInIncrementalGC = false;
  mCCBlockStart = TimeStamp();
  mReadyForMajorGC = false;
  mWantAtLeastRegularGC = false;
  mNeedsFullCC = CCReason::GC_FINISHED;
  mHasRunGC = true;
  mIsCompactingOnUserInactive = false;

  mCleanupsSinceLastGC = 0;
  mCCollectedWaitingForGC = 0;
  mCCollectedZonesWaitingForGC = 0;
  mLikelyShortLivingObjectsNeedingGC = 0;

  using mozilla::ipc::IdleSchedulerChild;
  IdleSchedulerChild* child = IdleSchedulerChild::GetMainThreadIdleScheduler();
  if (child) {
    child->DoneGC();
  }
}

#ifdef MOZ_GECKO_PROFILER
struct CCIntervalMarker {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("CC");
  }
  static void StreamJSONMarkerData(
      baseprofiler::SpliceableJSONWriter& aWriter,
      const mozilla::ProfilerString8View& aReason) {
    if (aReason.Length()) {
      aWriter.StringProperty("reason", aReason);
    }
  }
  static mozilla::MarkerSchema MarkerTypeDisplay() {
    using MS = mozilla::MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable,
              MS::Location::TimelineMemory};
    schema.AddStaticLabelValue(
        "Description",
        "Summary data for the core part of a cycle collection, possibly "
        "encompassing a set of incremental slices. The main thread is not "
        "blocked for the entire major CC interval, only for the individual "
        "slices.");
    schema.AddKeyLabelFormatSearchable("reason", "Reason", MS::Format::String,
                                       MS::Searchable::Searchable);
    return schema;
  }
};
#endif

void CCGCScheduler::NoteCCBegin(CCReason aReason, TimeStamp aWhen) {
#ifdef MOZ_GECKO_PROFILER
  profiler_add_marker(
      "CC", baseprofiler::category::GCCC,
      MarkerOptions(MarkerTiming::IntervalStart(aWhen)), CCIntervalMarker{},
      ProfilerString8View::WrapNullTerminatedString(CCReasonToString(aReason)));
#endif

  mIsCollectingCycles = true;
}

void CCGCScheduler::NoteCCEnd(TimeStamp aWhen) {
#ifdef MOZ_GECKO_PROFILER
  profiler_add_marker("CC", baseprofiler::category::GCCC,
                      MarkerOptions(MarkerTiming::IntervalEnd(aWhen)),
                      CCIntervalMarker{}, nullptr);
#endif

  mIsCollectingCycles = false;
  mLastCCEndTime = aWhen;
  mNeedsFullCC = CCReason::NO_REASON;

  // The GC for this CC has already been requested.
  mNeedsGCAfterCC = false;
}

void CCGCScheduler::NoteWontGC() {
  mReadyForMajorGC = false;
  mMajorGCReason = JS::GCReason::NO_REASON;
  mWantAtLeastRegularGC = false;
  // Don't clear the WantFullGC state, we will do a full GC the next time a
  // GC happens for any other reason.
}

bool CCGCScheduler::GCRunnerFired(TimeStamp aDeadline) {
  MOZ_ASSERT(!mDidShutdown, "GCRunner still alive during shutdown");

  GCRunnerStep step = GetNextGCRunnerAction();
  switch (step.mAction) {
    case GCRunnerAction::None:
      MOZ_CRASH("Unexpected GCRunnerAction");

    case GCRunnerAction::WaitToMajorGC: {
      MOZ_ASSERT(!mHaveAskedParent, "GCRunner alive after asking the parent");
      RefPtr<CCGCScheduler::MayGCPromise> mbPromise =
          CCGCScheduler::MayGCNow(step.mReason);
      if (!mbPromise) {
        // We can GC now.
        break;
      }

      mHaveAskedParent = true;
      KillGCRunner();
      mbPromise->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [this](bool aMayGC) {
            mHaveAskedParent = false;
            if (aMayGC) {
              if (!NoteReadyForMajorGC()) {
                // Another GC started and maybe completed while waiting.
                return;
              }
              // Recreate the GC runner with a 0 delay.  The new runner will
              // continue in idle time.
              KillGCRunner();
              EnsureGCRunner(0);
            } else if (!InIncrementalGC()) {
              // We should kill the GC runner since we're done with it, but
              // only if there's no incremental GC.
              KillGCRunner();
              NoteWontGC();
            }
          },
          [this](mozilla::ipc::ResponseRejectReason r) {
            mHaveAskedParent = false;
            if (!InIncrementalGC()) {
              KillGCRunner();
              NoteWontGC();
            }
          });

      return true;
    }

    case GCRunnerAction::StartMajorGC:
    case GCRunnerAction::GCSlice:
      break;
  }

  return GCRunnerFiredDoGC(aDeadline, step);
}

bool CCGCScheduler::GCRunnerFiredDoGC(TimeStamp aDeadline,
                                      const GCRunnerStep& aStep) {
  // Run a GC slice, possibly the first one of a major GC.
  nsJSContext::IsShrinking is_shrinking = nsJSContext::NonShrinkingGC;
  if (!InIncrementalGC() && aStep.mReason == JS::GCReason::USER_INACTIVE) {
    bool do_gc = mWantAtLeastRegularGC;

    if (!mUserIsActive) {
      if (!nsRefreshDriver::IsRegularRateTimerTicking()) {
        mIsCompactingOnUserInactive = true;
        is_shrinking = nsJSContext::ShrinkingGC;
        do_gc = true;
      } else {
        // Poke again to restart the timer.
        PokeShrinkingGC();
      }
    }

    if (!do_gc) {
      using mozilla::ipc::IdleSchedulerChild;
      IdleSchedulerChild* child =
          IdleSchedulerChild::GetMainThreadIdleScheduler();
      if (child) {
        child->DoneGC();
      }
      NoteWontGC();
      KillGCRunner();
      return true;
    }
  }

  MOZ_ASSERT(mActiveIntersliceGCBudget);
  TimeStamp startTimeStamp = TimeStamp::Now();
  TimeDuration budget = ComputeInterSliceGCBudget(aDeadline, startTimeStamp);
  TimeDuration duration = mGCUnnotifiedTotalTime;
  nsJSContext::GarbageCollectNow(aStep.mReason, nsJSContext::IncrementalGC,
                                 is_shrinking, budget.ToMilliseconds());

  mGCUnnotifiedTotalTime = TimeDuration();
  TimeStamp now = TimeStamp::Now();
  TimeDuration sliceDuration = now - startTimeStamp;
  duration += sliceDuration;
  if (duration.ToSeconds()) {
    TimeDuration idleDuration;
    if (!aDeadline.IsNull()) {
      if (aDeadline < now) {
        // This slice overflowed the idle period.
        idleDuration = aDeadline - startTimeStamp;
      } else {
        // Note, we don't want to use duration here, since it may contain
        // data also from JS engine triggered GC slices.
        idleDuration = sliceDuration;
      }
    }

    uint32_t percent =
        uint32_t(idleDuration.ToSeconds() / duration.ToSeconds() * 100);
    Telemetry::Accumulate(Telemetry::GC_SLICE_DURING_IDLE, percent);
  }

  // If the GC doesn't have any more work to do on the foreground thread (and
  // e.g. is waiting for background sweeping to finish) then return false to
  // make IdleTaskRunner postpone the next call a bit.
  JSContext* cx = dom::danger::GetJSContext();
  return JS::IncrementalGCHasForegroundWork(cx);
}

RefPtr<CCGCScheduler::MayGCPromise> CCGCScheduler::MayGCNow(
    JS::GCReason reason) {
  using namespace mozilla::ipc;

  // We ask the parent if we should GC for GCs that aren't too timely,
  // with the exception of MEM_PRESSURE, in that case we ask the parent
  // because GCing on too many processes at the same time when under
  // memory pressure could be a very bad experience for the user.
  switch (reason) {
    case JS::GCReason::PAGE_HIDE:
    case JS::GCReason::MEM_PRESSURE:
    case JS::GCReason::USER_INACTIVE:
    case JS::GCReason::FULL_GC_TIMER:
    case JS::GCReason::CC_FINISHED: {
      if (XRE_IsContentProcess()) {
        IdleSchedulerChild* child =
            IdleSchedulerChild::GetMainThreadIdleScheduler();
        if (child) {
          return child->MayGCNow();
        }
      }
      // The parent process doesn't ask IdleSchedulerParent if it can GC.
      break;
    }
    default:
      break;
  }

  return MayGCPromise::CreateAndResolve(true, __func__);
}

void CCGCScheduler::RunNextCollectorTimer(JS::GCReason aReason,
                                          mozilla::TimeStamp aDeadline) {
  if (mDidShutdown) {
    return;
  }

  // When we're in an incremental GC, we should always have an sGCRunner, so do
  // not check CC timers. The CC timers won't do anything during a GC.
  MOZ_ASSERT_IF(InIncrementalGC(), mGCRunner);

  RefPtr<IdleTaskRunner> runner;
  if (mGCRunner) {
    SetWantMajorGC(aReason);
    runner = mGCRunner;
  } else if (mCCRunner) {
    runner = mCCRunner;
  }

  if (runner) {
    runner->SetIdleDeadline(aDeadline);
    runner->Run();
  }
}

void CCGCScheduler::PokeShrinkingGC() {
  if (mShrinkingGCTimer || mDidShutdown) {
    return;
  }

  NS_NewTimerWithFuncCallback(
      &mShrinkingGCTimer,
      [](nsITimer* aTimer, void* aClosure) {
        CCGCScheduler* s = static_cast<CCGCScheduler*>(aClosure);
        s->KillShrinkingGCTimer();
        if (!s->mUserIsActive) {
          if (!nsRefreshDriver::IsRegularRateTimerTicking()) {
            s->SetWantMajorGC(JS::GCReason::USER_INACTIVE);
            if (!s->mHaveAskedParent) {
              s->EnsureGCRunner(0);
            }
          } else {
            s->PokeShrinkingGC();
          }
        }
      },
      this, StaticPrefs::javascript_options_compact_on_user_inactive_delay(),
      nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY, "ShrinkingGCTimerFired");
}

void CCGCScheduler::PokeFullGC() {
  if (!mFullGCTimer && !mDidShutdown) {
    NS_NewTimerWithFuncCallback(
        &mFullGCTimer,
        [](nsITimer* aTimer, void* aClosure) {
          CCGCScheduler* s = static_cast<CCGCScheduler*>(aClosure);
          s->KillFullGCTimer();

          // Even if the GC is denied by the parent process, because we've
          // set that we want a full GC we will get one eventually.
          s->SetNeedsFullGC();
          s->SetWantMajorGC(JS::GCReason::FULL_GC_TIMER);
          if (!s->mHaveAskedParent) {
            s->EnsureGCRunner(0);
          }
        },
        this, StaticPrefs::javascript_options_gc_delay_full(),
        nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY, "FullGCTimerFired");
  }
}

void CCGCScheduler::PokeGC(JS::GCReason aReason, JSObject* aObj,
                           TimeDuration aDelay) {
  if (mDidShutdown) {
    return;
  }

  if (aObj) {
    JS::Zone* zone = JS::GetObjectZone(aObj);
    CycleCollectedJSRuntime::Get()->AddZoneWaitingForGC(zone);
  } else if (aReason != JS::GCReason::CC_FINISHED) {
    SetNeedsFullGC();
  }

  if (mGCRunner || mHaveAskedParent) {
    // There's already a GC runner, there or will be, so just return.
    return;
  }

  SetWantMajorGC(aReason);

  if (mCCRunner) {
    // Make sure CC is called regardless of the size of the purple buffer, and
    // GC after it.
    EnsureCCThenGC(CCReason::GC_WAITING);
    return;
  }

  // Wait for javascript.options.gc_delay (or delay_first) then start
  // looking for idle time to run the initial GC slice.
  static bool first = true;
  TimeDuration delay =
      aDelay ? aDelay
             : TimeDuration::FromMilliseconds(
                   first ? StaticPrefs::javascript_options_gc_delay_first()
                         : StaticPrefs::javascript_options_gc_delay());
  first = false;
  EnsureGCRunner(delay);
}

void CCGCScheduler::EnsureGCRunner(TimeDuration aDelay) {
  if (mGCRunner) {
    return;
  }

  // Wait at most the interslice GC delay before forcing a run.
  mGCRunner = IdleTaskRunner::Create(
      [this](TimeStamp aDeadline) { return GCRunnerFired(aDeadline); },
      "CCGCScheduler::EnsureGCRunner", aDelay,
      TimeDuration::FromMilliseconds(
          StaticPrefs::javascript_options_gc_delay_interslice()),
      mActiveIntersliceGCBudget, true, [this] { return mDidShutdown; });
}

// nsJSEnvironmentObserver observes the user-interaction-inactive notifications
// and triggers a shrinking a garbage collection if the user is still inactive
// after NS_SHRINKING_GC_DELAY ms later, if the appropriate pref is set.
void CCGCScheduler::UserIsInactive() {
  mUserIsActive = false;
  if (StaticPrefs::javascript_options_compact_on_user_inactive()) {
    PokeShrinkingGC();
  }
}

void CCGCScheduler::UserIsActive() {
  mUserIsActive = true;
  KillShrinkingGCTimer();
  if (mIsCompactingOnUserInactive) {
    mozilla::dom::AutoJSAPI jsapi;
    jsapi.Init();
    JS::AbortIncrementalGC(jsapi.cx());
  }
  MOZ_ASSERT(!mIsCompactingOnUserInactive);
}

void CCGCScheduler::KillShrinkingGCTimer() {
  if (mShrinkingGCTimer) {
    mShrinkingGCTimer->Cancel();
    NS_RELEASE(mShrinkingGCTimer);
  }
}

void CCGCScheduler::KillFullGCTimer() {
  if (mFullGCTimer) {
    mFullGCTimer->Cancel();
    NS_RELEASE(mFullGCTimer);
  }
}

void CCGCScheduler::KillGCRunner() {
  // If we're in an incremental GC then killing the timer is only okay if
  // we're shutting down.
  MOZ_ASSERT(!(InIncrementalGC() && !mDidShutdown));
  if (mGCRunner) {
    mGCRunner->Cancel();
    mGCRunner = nullptr;
  }
}

void CCGCScheduler::EnsureCCRunner(TimeDuration aDelay, TimeDuration aBudget) {
  MOZ_ASSERT(!mDidShutdown);

  if (!mCCRunner) {
    mCCRunner = IdleTaskRunner::Create(
        CCRunnerFired, "EnsureCCRunner::CCRunnerFired", 0, aDelay, aBudget,
        true, [this] { return mDidShutdown; });
  } else {
    mCCRunner->SetMinimumUsefulBudget(aBudget.ToMilliseconds());
    nsIEventTarget* target = mozilla::GetCurrentEventTarget();
    if (target) {
      mCCRunner->SetTimer(aDelay, target);
    }
  }
}

void CCGCScheduler::MaybePokeCC(TimeStamp aNow, uint32_t aSuspectedCCObjects) {
  if (mCCRunner || mDidShutdown) {
    return;
  }

  CCReason reason = ShouldScheduleCC(aNow, aSuspectedCCObjects);
  if (reason != CCReason::NO_REASON) {
    // We can kill some objects before running forgetSkippable.
    nsCycleCollector_dispatchDeferredDeletion();

    if (!mCCRunner) {
      InitCCRunnerStateMachine(CCRunnerState::ReducePurple, reason);
    }
    EnsureCCRunner(kCCSkippableDelay, kForgetSkippableSliceDuration);
  }
}

void CCGCScheduler::KillCCRunner() {
  UnblockCC();
  DeactivateCCRunner();
  if (mCCRunner) {
    mCCRunner->Cancel();
    mCCRunner = nullptr;
  }
}

void CCGCScheduler::KillAllTimersAndRunners() {
  KillShrinkingGCTimer();
  KillCCRunner();
  KillFullGCTimer();
  KillGCRunner();
}

js::SliceBudget CCGCScheduler::ComputeCCSliceBudget(
    TimeStamp aDeadline, TimeStamp aCCBeginTime, TimeStamp aPrevSliceEndTime,
    TimeStamp aNow, bool* aPreferShorterSlices) const {
  *aPreferShorterSlices =
      aDeadline.IsNull() || (aDeadline - aNow) < kICCSliceBudget;

  TimeDuration baseBudget =
      aDeadline.IsNull() ? kICCSliceBudget : aDeadline - aNow;

  if (aCCBeginTime.IsNull()) {
    // If no CC is in progress, use the standard slice time.
    return js::SliceBudget(js::TimeBudget(baseBudget),
                           kNumCCNodesBetweenTimeChecks);
  }

  // Only run a limited slice if we're within the max running time.
  MOZ_ASSERT(aNow >= aCCBeginTime);
  TimeDuration runningTime = aNow - aCCBeginTime;
  if (runningTime >= kMaxICCDuration) {
    return js::SliceBudget::unlimited();
  }

  const TimeDuration maxSlice =
      TimeDuration::FromMilliseconds(MainThreadIdlePeriod::GetLongIdlePeriod());

  // Try to make up for a delay in running this slice.
  MOZ_ASSERT(aNow >= aPrevSliceEndTime);
  double sliceDelayMultiplier =
      (aNow - aPrevSliceEndTime) / kICCIntersliceDelay;
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
  return js::SliceBudget(js::TimeBudget(std::max(
                             {delaySliceBudget, laterSliceBudget, baseBudget})),
                         kNumCCNodesBetweenTimeChecks);
}

TimeDuration CCGCScheduler::ComputeInterSliceGCBudget(TimeStamp aDeadline,
                                                      TimeStamp aNow) const {
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

CCReason CCGCScheduler::ShouldScheduleCC(TimeStamp aNow,
                                         uint32_t aSuspectedCCObjects) const {
  if (!mHasRunGC) {
    return CCReason::NO_REASON;
  }

  // Don't run consecutive CCs too often.
  if (mCleanupsSinceLastGC && !mLastCCEndTime.IsNull()) {
    if (aNow - mLastCCEndTime < kCCDelay) {
      return CCReason::NO_REASON;
    }
  }

  // If GC hasn't run recently and forget skippable only cycle was run,
  // don't start a new cycle too soon.
  if ((mCleanupsSinceLastGC > kMajorForgetSkippableCalls) &&
      !mLastForgetSkippableCycleEndTime.IsNull()) {
    if (aNow - mLastForgetSkippableCycleEndTime <
        kTimeBetweenForgetSkippableCycles) {
      return CCReason::NO_REASON;
    }
  }

  return IsCCNeeded(aNow, aSuspectedCCObjects);
}

CCRunnerStep CCGCScheduler::AdvanceCCRunner(TimeStamp aDeadline, TimeStamp aNow,
                                            uint32_t aSuspectedCCObjects) {
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

  if (InIncrementalGC()) {
    if (mCCBlockStart.IsNull()) {
      BlockCC(aNow);

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

    if (GetCCBlockedTime(aNow) < kMaxCCLockedoutTime) {
      return {CCRunnerAction::None, Yield};
    }

    // Locked out for too long, so proceed and finish the incremental GC
    // synchronously.
  }

  // For states that aren't just continuations of previous states, check
  // whether a CC is still needed (after doing various things to reduce the
  // purple buffer).
  if (desc.mCanAbortCC &&
      IsCCNeeded(aNow, aSuspectedCCObjects) == CCReason::NO_REASON) {
    // If we don't pass the threshold for wanting to cycle collect, stop now
    // (after possibly doing a final ForgetSkippable).
    mCCRunnerState = CCRunnerState::Canceled;
    NoteForgetSkippableOnlyCycle(aNow);

    // Preserve the previous code's idea of when to check whether a
    // ForgetSkippable should be fired.
    if (desc.mTryFinalForgetSkippable &&
        ShouldForgetSkippable(aSuspectedCCObjects)) {
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

      if (ShouldForgetSkippable(aSuspectedCCObjects)) {
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
      if (aNow >= aDeadline) {
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
      if (aNow >= aDeadline) {
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
    case CCRunnerState::CycleCollecting: {
      CCRunnerStep step{CCRunnerAction::CycleCollect, Yield};
      step.mCCReason = mCCReason;
      mCCReason = CCReason::SLICE;  // Set reason for following slices.
      return step;
    }

    default:
      MOZ_CRASH("Unexpected CCRunner state");
  };
}

GCRunnerStep CCGCScheduler::GetNextGCRunnerAction() const {
  MOZ_ASSERT(mMajorGCReason != JS::GCReason::NO_REASON);

  if (InIncrementalGC()) {
    return {GCRunnerAction::GCSlice, mMajorGCReason};
  }

  if (mReadyForMajorGC) {
    return {GCRunnerAction::StartMajorGC, mMajorGCReason};
  }

  return {GCRunnerAction::WaitToMajorGC, mMajorGCReason};
}

js::SliceBudget CCGCScheduler::ComputeForgetSkippableBudget(
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
