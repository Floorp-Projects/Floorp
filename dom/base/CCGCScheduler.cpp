/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CCGCScheduler.h"

#include "mozilla/StaticPrefs_javascript.h"
#include "mozilla/CycleCollectedJSRuntime.h"

namespace mozilla {

// These definitions must match those in nsJSEnvironment.cpp
TimeStamp CCGCScheduler::Now() { return TimeStamp::Now(); }

uint32_t CCGCScheduler::SuspectedCCObjects() {
  return nsCycleCollector_suspectedCount();
}

void CCGCScheduler::FullGCTimerFired(nsITimer* aTimer) {
  KillFullGCTimer();

  RefPtr<CCGCScheduler::MayGCPromise> mbPromise =
      CCGCScheduler::MayGCNow(JS::GCReason::FULL_GC_TIMER);
  if (mbPromise) {
    mbPromise->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [](bool aIgnored) {
          nsJSContext::GarbageCollectNow(JS::GCReason::FULL_GC_TIMER,
                                         nsJSContext::IncrementalGC);
        },
        [](mozilla::ipc::ResponseRejectReason r) {
          // do nothing
        });
  }
}

// nsJSEnvironmentObserver observes the user-interaction-inactive notifications
// and triggers a shrinking a garbage collection if the user is still inactive
// after NS_SHRINKING_GC_DELAY ms later, if the appropriate pref is set.

void CCGCScheduler::ShrinkingGCTimerFired(nsITimer* aTimer) {
  KillShrinkingGCTimer();

  RefPtr<MayGCPromise> mbPromise = MayGCNow(JS::GCReason::USER_INACTIVE);
  if (mbPromise) {
    mbPromise->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [](bool aIgnored) {
          if (!sUserIsActive) {
            sIsCompactingOnUserInactive = true;
            nsJSContext::GarbageCollectNow(JS::GCReason::USER_INACTIVE,
                                           nsJSContext::IncrementalGC,
                                           nsJSContext::ShrinkingGC);
          } else {
            using mozilla::ipc::IdleSchedulerChild;
            IdleSchedulerChild* child =
                IdleSchedulerChild::GetMainThreadIdleScheduler();
            if (child) {
              child->DoneGC();
            }
          }
        },
        [](mozilla::ipc::ResponseRejectReason r) {});
  }
}

bool CCGCScheduler::GCRunnerFired(TimeStamp aDeadline) {
  MOZ_ASSERT(!mDidShutdown, "GCRunner still alive during shutdown");

  GCRunnerStep step = GetNextGCRunnerAction(aDeadline);
  switch (step.mAction) {
    case GCRunnerAction::None:
      MOZ_CRASH("Unexpected GCRunnerAction");

    case GCRunnerAction::WaitToMajorGC: {
      RefPtr<CCGCScheduler::MayGCPromise> mbPromise =
          CCGCScheduler::MayGCNow(step.mReason);
      if (!mbPromise || mbPromise->IsResolved()) {
        // Only use the promise if it's not resolved yet, otherwise fall through
        // and begin the GC in the current idle time with our current deadline.
        break;
      }

      KillGCRunner();
      mbPromise->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [this](bool aIgnored) {
            if (!NoteReadyForMajorGC()) {
              return;  // Another GC completed while waiting.
            }
            // If a new runner was started, recreate it with a 0 delay. The new
            // runner will continue in idle time.
            KillGCRunner();
            EnsureGCRunner(0);
          },
          [](mozilla::ipc::ResponseRejectReason r) {});

      return true;
    }

    case GCRunnerAction::StartMajorGC:
    case GCRunnerAction::GCSlice:
      break;
  }

  // Run a GC slice, possibly the first one of a major GC.

  MOZ_ASSERT(mActiveIntersliceGCBudget);
  TimeStamp startTimeStamp = TimeStamp::Now();
  TimeDuration budget = ComputeInterSliceGCBudget(aDeadline, startTimeStamp);
  TimeDuration duration = mGCUnnotifiedTotalTime;
  nsJSContext::GarbageCollectNow(step.mReason, nsJSContext::IncrementalGC,
                                 nsJSContext::NonShrinkingGC,
                                 budget.ToMilliseconds());

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
      // TODO: but it should ask.
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
        static_cast<CCGCScheduler*>(aClosure)->ShrinkingGCTimerFired(aTimer);
      },
      this, StaticPrefs::javascript_options_compact_on_user_inactive_delay(),
      nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY, "ShrinkingGCTimerFired");
}

void CCGCScheduler::PokeFullGC() {
  if (!mFullGCTimer && !mDidShutdown) {
    NS_NewTimerWithFuncCallback(
        &mFullGCTimer,
        [](nsITimer* aTimer, void* aClosure) {
          static_cast<CCGCScheduler*>(aClosure)->FullGCTimerFired(aTimer);
        },
        this, StaticPrefs::javascript_options_gc_delay_full(),
        nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY, "FullGCTimerFired");
  }
}

void CCGCScheduler::PokeGC(JS::GCReason aReason, JSObject* aObj,
                           uint32_t aDelay) {
  if (mDidShutdown) {
    return;
  }

  if (aObj) {
    JS::Zone* zone = JS::GetObjectZone(aObj);
    CycleCollectedJSRuntime::Get()->AddZoneWaitingForGC(zone);
  } else if (aReason != JS::GCReason::CC_FINISHED) {
    SetNeedsFullGC();
  }

  if (mGCRunner) {
    // There's already a runner for GC'ing, just return
    return;
  }

  SetWantMajorGC(aReason);

  if (mCCRunner) {
    // Make sure CC is called regardless of the size of the purple buffer, and
    // GC after it.
    EnsureCCThenGC();
    return;
  }

  // Wait for javascript.options.gc_delay (or delay_first) then start
  // looking for idle time to run the initial GC slice.
  static bool first = true;
  uint32_t delay =
      aDelay ? aDelay
             : (first ? StaticPrefs::javascript_options_gc_delay_first()
                      : StaticPrefs::javascript_options_gc_delay());
  first = false;
  EnsureGCRunner(delay);
}

void CCGCScheduler::EnsureGCRunner(uint32_t aDelay) {
  if (mGCRunner) {
    return;
  }

  // Wait at most the interslice GC delay before forcing a run.
  mGCRunner = IdleTaskRunner::Create(
      [this](TimeStamp aDeadline) { return GCRunnerFired(aDeadline); },
      "CCGCScheduler::EnsureGCRunner", aDelay,
      StaticPrefs::javascript_options_gc_delay_interslice(),
      int64_t(mActiveIntersliceGCBudget.ToMilliseconds()), true,
      [this] { return mDidShutdown; });
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
  if (mGCRunner) {
    mGCRunner->Cancel();
    mGCRunner = nullptr;
  }
}

void CCGCScheduler::EnsureCCRunner(TimeDuration aDelay, TimeDuration aBudget) {
  MOZ_ASSERT(!mDidShutdown);

  if (!mCCRunner) {
    mCCRunner = IdleTaskRunner::Create(
        CCRunnerFired, "EnsureCCRunner::CCRunnerFired", 0,
        aDelay.ToMilliseconds(), aBudget.ToMilliseconds(), true,
        [this] { return mDidShutdown; });
  } else {
    mCCRunner->SetMinimumUsefulBudget(aBudget.ToMilliseconds());
    nsIEventTarget* target = mozilla::GetCurrentEventTarget();
    if (target) {
      mCCRunner->SetTimer(aDelay.ToMilliseconds(), target);
    }
  }
}

void CCGCScheduler::MaybePokeCC() {
  if (mCCRunner || mDidShutdown) {
    return;
  }

  if (ShouldScheduleCC()) {
    // We can kill some objects before running forgetSkippable.
    nsCycleCollector_dispatchDeferredDeletion();

    if (!mCCRunner) {
      InitCCRunnerStateMachine(CCRunnerState::ReducePurple);
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

}  // namespace mozilla
