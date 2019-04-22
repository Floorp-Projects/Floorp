/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Code to notify things that animate before a refresh, at an appropriate
 * refresh rate.  (Perhaps temporary, until replaced by compositor.)
 *
 * Chrome and each tab have their own RefreshDriver, which in turn
 * hooks into one of a few global timer based on RefreshDriverTimer,
 * defined below.  There are two main global timers -- one for active
 * animations, and one for inactive ones.  These are implemented as
 * subclasses of RefreshDriverTimer; see below for a description of
 * their implementations.  In the future, additional timer types may
 * implement things like blocking on vsync.
 */

#include "nsRefreshDriver.h"

#ifdef XP_WIN
#  include <windows.h>
// mmsystem isn't part of WIN32_LEAN_AND_MEAN, so we have
// to manually include it
#  include <mmsystem.h>
#  include "WinUtils.h"
#endif

#include "mozilla/AnimationEventDispatcher.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/dom/FontTableURIProtocolHandler.h"
#include "nsITimer.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/Document.h"
#include "nsIXULRuntime.h"
#include "jsapi.h"
#include "nsContentUtils.h"
#include "mozilla/PendingAnimationTracker.h"
#include "mozilla/PendingFullscreenEvent.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs.h"
#include "nsViewManager.h"
#include "GeckoProfiler.h"
#include "nsNPAPIPluginInstance.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/RestyleManager.h"
#include "Layers.h"
#include "imgIContainer.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsDocShell.h"
#include "nsISimpleEnumerator.h"
#include "nsJSEnvironment.h"
#include "mozilla/Telemetry.h"
#include "gfxPrefs.h"
#include "BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/layout/VsyncChild.h"
#include "VsyncSource.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsThreadUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/TimelineConsumers.h"
#include "nsAnimationManager.h"
#include "nsDisplayList.h"
#include "nsTransitionManager.h"

#ifdef MOZ_XUL
#  include "nsXULPopupManager.h"
#endif

using namespace mozilla;
using namespace mozilla::widget;
using namespace mozilla::ipc;
using namespace mozilla::dom;
using namespace mozilla::layout;

static mozilla::LazyLogModule sRefreshDriverLog("nsRefreshDriver");
#define LOG(...) \
  MOZ_LOG(sRefreshDriverLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

#define DEFAULT_THROTTLED_FRAME_RATE 1
#define DEFAULT_RECOMPUTE_VISIBILITY_INTERVAL_MS 1000
#define DEFAULT_NOTIFY_INTERSECTION_OBSERVERS_INTERVAL_MS 100
// after 10 minutes, stop firing off inactive timers
#define DEFAULT_INACTIVE_TIMER_DISABLE_SECONDS 600

// The number of seconds spent skipping frames because we are waiting for the
// compositor before logging.
#if defined(MOZ_ASAN)
#  define REFRESH_WAIT_WARNING 5
#elif defined(DEBUG) && !defined(MOZ_VALGRIND)
#  define REFRESH_WAIT_WARNING 5
#elif defined(DEBUG) && defined(MOZ_VALGRIND)
#  define REFRESH_WAIT_WARNING (RUNNING_ON_VALGRIND ? 20 : 5)
#elif defined(MOZ_VALGRIND)
#  define REFRESH_WAIT_WARNING (RUNNING_ON_VALGRIND ? 10 : 1)
#else
#  define REFRESH_WAIT_WARNING 1
#endif

namespace {
// `true` if we are currently in jank-critical mode.
//
// In jank-critical mode, any iteration of the event loop that takes
// more than 16ms to compute will cause an ongoing animation to miss
// frames.
//
// For simplicity, the current implementation assumes that we are in
// jank-critical mode if and only if at least one vsync driver has
// at least one observer.
static uint64_t sActiveVsyncTimers = 0;

// The latest value of process-wide jank levels.
//
// For each i, sJankLevels[i] counts the number of times delivery of
// vsync to the main thread has been delayed by at least 2^i ms. Use
// GetJankLevels to grab a copy of this array.
uint64_t sJankLevels[12];

// The number outstanding nsRefreshDrivers (that have been created but not
// disconnected). When this reaches zero we will call
// nsRefreshDriver::Shutdown.
static uint32_t sRefreshDriverCount = 0;
}  // namespace

namespace mozilla {

/*
 * The base class for all global refresh driver timers.  It takes care
 * of managing the list of refresh drivers attached to them and
 * provides interfaces for querying/setting the rate and actually
 * running a timer 'Tick'.  Subclasses must implement StartTimer(),
 * StopTimer(), and ScheduleNextTick() -- the first two just
 * start/stop whatever timer mechanism is in use, and ScheduleNextTick
 * is called at the start of the Tick() implementation to set a time
 * for the next tick.
 */
class RefreshDriverTimer {
 public:
  RefreshDriverTimer() {}

  NS_INLINE_DECL_REFCOUNTING(RefreshDriverTimer)

  virtual void AddRefreshDriver(nsRefreshDriver* aDriver) {
    LOG("[%p] AddRefreshDriver %p", this, aDriver);

    bool startTimer =
        mContentRefreshDrivers.IsEmpty() && mRootRefreshDrivers.IsEmpty();
    if (IsRootRefreshDriver(aDriver)) {
      NS_ASSERTION(!mRootRefreshDrivers.Contains(aDriver),
                   "Adding a duplicate root refresh driver!");
      mRootRefreshDrivers.AppendElement(aDriver);
    } else {
      NS_ASSERTION(!mContentRefreshDrivers.Contains(aDriver),
                   "Adding a duplicate content refresh driver!");
      mContentRefreshDrivers.AppendElement(aDriver);
    }

    if (startTimer) {
      StartTimer();
    }
  }

  void RemoveRefreshDriver(nsRefreshDriver* aDriver) {
    LOG("[%p] RemoveRefreshDriver %p", this, aDriver);

    if (IsRootRefreshDriver(aDriver)) {
      NS_ASSERTION(mRootRefreshDrivers.Contains(aDriver),
                   "RemoveRefreshDriver for a refresh driver that's not in the "
                   "root refresh list!");
      mRootRefreshDrivers.RemoveElement(aDriver);
    } else {
      nsPresContext* pc = aDriver->GetPresContext();
      nsPresContext* rootContext = pc ? pc->GetRootPresContext() : nullptr;
      // During PresContext shutdown, we can't accurately detect
      // if a root refresh driver exists or not. Therefore, we have to
      // search and find out which list this driver exists in.
      if (!rootContext) {
        if (mRootRefreshDrivers.Contains(aDriver)) {
          mRootRefreshDrivers.RemoveElement(aDriver);
        } else {
          NS_ASSERTION(mContentRefreshDrivers.Contains(aDriver),
                       "RemoveRefreshDriver without a display root for a "
                       "driver that is not in the content refresh list");
          mContentRefreshDrivers.RemoveElement(aDriver);
        }
      } else {
        NS_ASSERTION(mContentRefreshDrivers.Contains(aDriver),
                     "RemoveRefreshDriver for a driver that is not in the "
                     "content refresh list");
        mContentRefreshDrivers.RemoveElement(aDriver);
      }
    }

    bool stopTimer =
        mContentRefreshDrivers.IsEmpty() && mRootRefreshDrivers.IsEmpty();
    if (stopTimer) {
      StopTimer();
    }
  }

  TimeStamp MostRecentRefresh() const { return mLastFireTime; }

  void SwapRefreshDrivers(RefreshDriverTimer* aNewTimer) {
    MOZ_ASSERT(NS_IsMainThread());

    for (nsRefreshDriver* driver : mContentRefreshDrivers) {
      aNewTimer->AddRefreshDriver(driver);
      driver->mActiveTimer = aNewTimer;
    }
    mContentRefreshDrivers.Clear();

    for (nsRefreshDriver* driver : mRootRefreshDrivers) {
      aNewTimer->AddRefreshDriver(driver);
      driver->mActiveTimer = aNewTimer;
    }
    mRootRefreshDrivers.Clear();

    aNewTimer->mLastFireTime = mLastFireTime;

    StopTimer();
  }

  virtual TimeDuration GetTimerRate() = 0;

  TimeStamp GetIdleDeadlineHint(TimeStamp aDefault) {
    MOZ_ASSERT(NS_IsMainThread());

    TimeStamp mostRecentRefresh = MostRecentRefresh();
    TimeDuration refreshRate = GetTimerRate();
    TimeStamp idleEnd = mostRecentRefresh + refreshRate;

    if (idleEnd +
            refreshRate * nsLayoutUtils::QuiescentFramesBeforeIdlePeriod() <
        TimeStamp::Now()) {
      return aDefault;
    }

    idleEnd = idleEnd - TimeDuration::FromMilliseconds(
                            nsLayoutUtils::IdlePeriodDeadlineLimit());
    return idleEnd < aDefault ? idleEnd : aDefault;
  }

  Maybe<TimeStamp> GetNextTickHint() {
    MOZ_ASSERT(NS_IsMainThread());
    TimeStamp nextTick = MostRecentRefresh() + GetTimerRate();
    return nextTick < TimeStamp::Now() ? Nothing() : Some(nextTick);
  }

  // Returns null if the RefreshDriverTimer is attached to several
  // RefreshDrivers. That may happen for example when there are
  // several windows open.
  nsPresContext* GetPresContextForOnlyRefreshDriver() {
    if (mRootRefreshDrivers.Length() == 1 && mContentRefreshDrivers.IsEmpty()) {
      return mRootRefreshDrivers[0]->GetPresContext();
    }
    if (mContentRefreshDrivers.Length() == 1 && mRootRefreshDrivers.IsEmpty()) {
      return mContentRefreshDrivers[0]->GetPresContext();
    }
    return nullptr;
  }

 protected:
  virtual ~RefreshDriverTimer() {
    MOZ_ASSERT(
        mContentRefreshDrivers.Length() == 0,
        "Should have removed all content refresh drivers from here by now!");
    MOZ_ASSERT(
        mRootRefreshDrivers.Length() == 0,
        "Should have removed all root refresh drivers from here by now!");
  }

  virtual void StartTimer() = 0;
  virtual void StopTimer() = 0;
  virtual void ScheduleNextTick(TimeStamp aNowTime) = 0;

  bool IsRootRefreshDriver(nsRefreshDriver* aDriver) {
    nsPresContext* pc = aDriver->GetPresContext();
    nsPresContext* rootContext = pc ? pc->GetRootPresContext() : nullptr;
    if (!rootContext) {
      return false;
    }

    return aDriver == rootContext->RefreshDriver();
  }

  /*
   * Actually runs a tick, poking all the attached RefreshDrivers.
   * Grabs the "now" time via TimeStamp::Now().
   */
  void Tick() {
    TimeStamp now = TimeStamp::Now();
    Tick(VsyncId(), now);
  }

  void TickRefreshDrivers(VsyncId aId, TimeStamp aNow,
                          nsTArray<RefPtr<nsRefreshDriver>>& aDrivers) {
    if (aDrivers.IsEmpty()) {
      return;
    }

    nsTArray<RefPtr<nsRefreshDriver>> drivers(aDrivers);
    for (nsRefreshDriver* driver : drivers) {
      // don't poke this driver if it's in test mode
      if (driver->IsTestControllingRefreshesEnabled()) {
        continue;
      }

      TickDriver(driver, aId, aNow);
    }
  }

  /*
   * Tick the refresh drivers based on the given timestamp.
   */
  void Tick(VsyncId aId, TimeStamp now) {
    ScheduleNextTick(now);

    mLastFireTime = now;

    LOG("[%p] ticking drivers...", this);
    // RD is short for RefreshDriver
    AUTO_PROFILER_TRACING("Paint", "RefreshDriverTick", GRAPHICS);

    TickRefreshDrivers(aId, now, mContentRefreshDrivers);
    TickRefreshDrivers(aId, now, mRootRefreshDrivers);

    LOG("[%p] done.", this);
  }

  static void TickDriver(nsRefreshDriver* driver, VsyncId aId, TimeStamp now) {
    driver->Tick(aId, now);
  }

  TimeStamp mLastFireTime;
  TimeStamp mTargetTime;

  nsTArray<RefPtr<nsRefreshDriver>> mContentRefreshDrivers;
  nsTArray<RefPtr<nsRefreshDriver>> mRootRefreshDrivers;

  // useful callback for nsITimer-based derived classes, here
  // because of c++ protected shenanigans
  static void TimerTick(nsITimer* aTimer, void* aClosure) {
    RefPtr<RefreshDriverTimer> timer =
        static_cast<RefreshDriverTimer*>(aClosure);
    timer->Tick();
  }
};

/*
 * A RefreshDriverTimer that uses a nsITimer as the underlying timer.  Note that
 * this is a ONE_SHOT timer, not a repeating one!  Subclasses are expected to
 * implement ScheduleNextTick and intelligently calculate the next time to tick,
 * and to reset mTimer.  Using a repeating nsITimer gets us into a lot of pain
 * with its attempt at intelligent slack removal and such, so we don't do it.
 */
class SimpleTimerBasedRefreshDriverTimer : public RefreshDriverTimer {
 public:
  /*
   * aRate -- the delay, in milliseconds, requested between timer firings
   */
  explicit SimpleTimerBasedRefreshDriverTimer(double aRate) {
    SetRate(aRate);
    mTimer = NS_NewTimer();
  }

  virtual ~SimpleTimerBasedRefreshDriverTimer() override { StopTimer(); }

  // will take effect at next timer tick
  virtual void SetRate(double aNewRate) {
    mRateMilliseconds = aNewRate;
    mRateDuration = TimeDuration::FromMilliseconds(mRateMilliseconds);
  }

  double GetRate() const { return mRateMilliseconds; }

  TimeDuration GetTimerRate() override { return mRateDuration; }

 protected:
  void StartTimer() override {
    // pretend we just fired, and we schedule the next tick normally
    mLastFireTime = TimeStamp::Now();

    mTargetTime = mLastFireTime + mRateDuration;

    uint32_t delay = static_cast<uint32_t>(mRateMilliseconds);
    mTimer->InitWithNamedFuncCallback(
        TimerTick, this, delay, nsITimer::TYPE_ONE_SHOT,
        "SimpleTimerBasedRefreshDriverTimer::StartTimer");
  }

  void StopTimer() override { mTimer->Cancel(); }

  double mRateMilliseconds;
  TimeDuration mRateDuration;
  RefPtr<nsITimer> mTimer;
};

/*
 * A refresh driver that listens to vsync events and ticks the refresh driver
 * on vsync intervals. We throttle the refresh driver if we get too many
 * vsync events and wait to catch up again.
 */
class VsyncRefreshDriverTimer : public RefreshDriverTimer {
 public:
  VsyncRefreshDriverTimer() : mVsyncChild(nullptr) {
    MOZ_ASSERT(XRE_IsParentProcess());
    MOZ_ASSERT(NS_IsMainThread());
    mVsyncObserver = new RefreshDriverVsyncObserver(this);
    RefPtr<mozilla::gfx::VsyncSource> vsyncSource =
        gfxPlatform::GetPlatform()->GetHardwareVsync();
    MOZ_ALWAYS_TRUE(mVsyncDispatcher =
                        vsyncSource->GetRefreshTimerVsyncDispatcher());
    mVsyncDispatcher->SetParentRefreshTimer(mVsyncObserver);
    mVsyncRate = vsyncSource->GetGlobalDisplay().GetVsyncRate();
  }

  explicit VsyncRefreshDriverTimer(VsyncChild* aVsyncChild)
      : mVsyncChild(aVsyncChild) {
    MOZ_ASSERT(!XRE_IsParentProcess());
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mVsyncChild);
    mVsyncObserver = new RefreshDriverVsyncObserver(this);
    mVsyncChild->SetVsyncObserver(mVsyncObserver);
    mVsyncRate = mVsyncChild->GetVsyncRate();
  }

  TimeDuration GetTimerRate() override {
    if (mVsyncRate != TimeDuration::Forever()) {
      return mVsyncRate;
    }

    if (mVsyncChild) {
      // VsyncChild::VsyncRate() is a simple getter for the cached
      // hardware vsync rate. We depend on that
      // VsyncChild::GetVsyncRate() being called in the constructor
      // will result in a response with the actual vsync rate sooner
      // or later. Until that happens VsyncChild::VsyncRate() returns
      // TimeDuration::Forever() and we have to guess below.
      mVsyncRate = mVsyncChild->VsyncRate();
    }

    // If hardware queries fail / are unsupported, we have to just guess.
    return mVsyncRate != TimeDuration::Forever()
               ? mVsyncRate
               : TimeDuration::FromMilliseconds(1000.0 / 60.0);
  }

 private:
  // Since VsyncObservers are refCounted, but the RefreshDriverTimer are
  // explicitly shutdown. We create an inner class that has the VsyncObserver
  // and is shutdown when the RefreshDriverTimer is deleted.
  class RefreshDriverVsyncObserver final : public VsyncObserver {
   public:
    explicit RefreshDriverVsyncObserver(
        VsyncRefreshDriverTimer* aVsyncRefreshDriverTimer)
        : mVsyncRefreshDriverTimer(aVsyncRefreshDriverTimer),
          mRefreshTickLock("RefreshTickLock"),
          mRecentVsync(TimeStamp::Now()),
          mLastChildTick(TimeStamp::Now()),
          mVsyncRate(TimeDuration::Forever()),
          mProcessedVsync(true) {
      MOZ_ASSERT(NS_IsMainThread());
    }

    class ParentProcessVsyncNotifier final : public Runnable,
                                             public nsIRunnablePriority {
     public:
      ParentProcessVsyncNotifier(RefreshDriverVsyncObserver* aObserver,
                                 VsyncId aId, TimeStamp aVsyncTimestamp)
          : Runnable(
                "VsyncRefreshDriverTimer::RefreshDriverVsyncObserver::"
                "ParentProcessVsyncNotifier"),
            mObserver(aObserver),
            mId(aId),
            mVsyncTimestamp(aVsyncTimestamp) {}

      NS_DECL_ISUPPORTS_INHERITED

      NS_IMETHOD Run() override {
        MOZ_ASSERT(NS_IsMainThread());
        static bool sCacheInitialized = false;
        static bool sHighPriorityPrefValue = false;
        if (!sCacheInitialized) {
          sCacheInitialized = true;
          Preferences::AddBoolVarCache(&sHighPriorityPrefValue,
                                       "vsync.parentProcess.highPriority",
                                       mozilla::BrowserTabsRemoteAutostart());
        }
        sHighPriorityEnabled = sHighPriorityPrefValue;

        mObserver->TickRefreshDriver(mId, mVsyncTimestamp);
        return NS_OK;
      }

      NS_IMETHOD GetPriority(uint32_t* aPriority) override {
        *aPriority = sHighPriorityEnabled
                         ? nsIRunnablePriority::PRIORITY_HIGH
                         : nsIRunnablePriority::PRIORITY_NORMAL;
        return NS_OK;
      }

     private:
      ~ParentProcessVsyncNotifier() {}
      RefPtr<RefreshDriverVsyncObserver> mObserver;
      VsyncId mId;
      TimeStamp mVsyncTimestamp;
      static mozilla::Atomic<bool> sHighPriorityEnabled;
    };

    bool NotifyVsync(const VsyncEvent& aVsync) override {
      // IMPORTANT: All paths through this method MUST hold a strong ref on
      // |this| for the duration of the TickRefreshDriver callback.

      if (!NS_IsMainThread()) {
        MOZ_ASSERT(XRE_IsParentProcess());
        // Compress vsync notifications such that only 1 may run at a time
        // This is so that we don't flood the refresh driver with vsync messages
        // if the main thread is blocked for long periods of time
        {  // scope lock
          MonitorAutoLock lock(mRefreshTickLock);
          mRecentVsync = aVsync.mTime;
          mRecentVsyncId = aVsync.mId;
          if (!mProcessedVsync) {
            return true;
          }
          mProcessedVsync = false;
        }

        nsCOMPtr<nsIRunnable> vsyncEvent =
            new ParentProcessVsyncNotifier(this, aVsync.mId, aVsync.mTime);
        NS_DispatchToMainThread(vsyncEvent);
      } else {
        mRecentVsync = aVsync.mTime;
        mRecentVsyncId = aVsync.mId;
        if (!mBlockUntil.IsNull() && mBlockUntil > aVsync.mTime) {
          if (mProcessedVsync) {
            // Re-post vsync update as a normal priority runnable. This way
            // runnables already in normal priority queue get processed.
            mProcessedVsync = false;
            nsCOMPtr<nsIRunnable> vsyncEvent = NewRunnableMethod<>(
                "RefreshDriverVsyncObserver::NormalPriorityNotify", this,
                &RefreshDriverVsyncObserver::NormalPriorityNotify);
            NS_DispatchToMainThread(vsyncEvent);
          }

          return true;
        }

        if (StaticPrefs::layout_lower_priority_refresh_driver_during_load()) {
          nsPresContext* pctx =
              mVsyncRefreshDriverTimer->GetPresContextForOnlyRefreshDriver();
          if (pctx && pctx->HadContentfulPaint() && pctx->Document() &&
              pctx->Document()->GetReadyStateEnum() <
                  Document::READYSTATE_COMPLETE) {
            nsPIDOMWindowInner* win = pctx->Document()->GetInnerWindow();
            if (win) {
              dom::Performance* perf = win->GetPerformance();
              // Limit slower refresh rate to 5 seconds between the
              // first contentful paint and page load.
              if (perf && perf->Now() < 5000) {
                if (mProcessedVsync) {
                  mProcessedVsync = false;
                  // Handle this case similarly to the code above, but just
                  // use idle queue.
                  TimeDuration rate = mVsyncRefreshDriverTimer->GetTimerRate();
                  uint32_t slowRate =
                      static_cast<uint32_t>(rate.ToMilliseconds() * 4);
                  nsCOMPtr<nsIRunnable> vsyncEvent = NewRunnableMethod<>(
                      "RefreshDriverVsyncObserver::NormalPriorityNotify[IDLE]",
                      this, &RefreshDriverVsyncObserver::NormalPriorityNotify);
                  NS_DispatchToCurrentThreadQueue(vsyncEvent.forget(), slowRate,
                                                  EventQueuePriority::Idle);
                }
                return true;
              }
            }
          }
        }

        RefPtr<RefreshDriverVsyncObserver> kungFuDeathGrip(this);
        TickRefreshDriver(aVsync.mId, aVsync.mTime);
      }

      return true;
    }

    void Shutdown() {
      MOZ_ASSERT(NS_IsMainThread());
      mVsyncRefreshDriverTimer = nullptr;
    }

    void OnTimerStart() {
      if (!XRE_IsParentProcess()) {
        mLastChildTick = TimeStamp::Now();
      }
    }

    void NormalPriorityNotify() {
      if (mLastProcessedTickInChildProcess.IsNull() ||
          mRecentVsync > mLastProcessedTickInChildProcess) {
        // mBlockUntil is for high priority vsync notifications only.
        mBlockUntil = TimeStamp();
        TickRefreshDriver(mRecentVsyncId, mRecentVsync);
      }

      mProcessedVsync = true;
    }

   private:
    ~RefreshDriverVsyncObserver() = default;

    void RecordTelemetryProbes(TimeStamp aVsyncTimestamp) {
      MOZ_ASSERT(NS_IsMainThread());
#ifndef ANDROID /* bug 1142079 */
      if (XRE_IsParentProcess()) {
        TimeDuration vsyncLatency = TimeStamp::Now() - aVsyncTimestamp;
        uint32_t sample = (uint32_t)vsyncLatency.ToMilliseconds();
        Telemetry::Accumulate(
            Telemetry::FX_REFRESH_DRIVER_CHROME_FRAME_DELAY_MS, sample);
        Telemetry::Accumulate(
            Telemetry::FX_REFRESH_DRIVER_SYNC_SCROLL_FRAME_DELAY_MS, sample);
        RecordJank(sample);
      } else if (mVsyncRate != TimeDuration::Forever()) {
        TimeDuration contentDelay =
            (TimeStamp::Now() - mLastChildTick) - mVsyncRate;
        if (contentDelay.ToMilliseconds() < 0) {
          // Vsyncs are noisy and some can come at a rate quicker than
          // the reported hardware rate. In those cases, consider that we have 0
          // delay.
          contentDelay = TimeDuration::FromMilliseconds(0);
        }
        uint32_t sample = (uint32_t)contentDelay.ToMilliseconds();
        Telemetry::Accumulate(
            Telemetry::FX_REFRESH_DRIVER_CONTENT_FRAME_DELAY_MS, sample);
        Telemetry::Accumulate(
            Telemetry::FX_REFRESH_DRIVER_SYNC_SCROLL_FRAME_DELAY_MS, sample);
        RecordJank(sample);
      } else {
        // Request the vsync rate from the parent process. Might be a few vsyncs
        // until the parent responds.
        if (mVsyncRefreshDriverTimer) {
          mVsyncRate = mVsyncRefreshDriverTimer->mVsyncChild->GetVsyncRate();
        }
      }
#endif
    }

    void RecordJank(uint32_t aJankMS) {
      uint32_t duration = 1 /* ms */;
      for (size_t i = 0;
           i < mozilla::ArrayLength(sJankLevels) && duration < aJankMS;
           ++i, duration *= 2) {
        sJankLevels[i]++;
      }
    }

    void TickRefreshDriver(VsyncId aId, TimeStamp aVsyncTimestamp) {
      MOZ_ASSERT(NS_IsMainThread());

      RecordTelemetryProbes(aVsyncTimestamp);
      if (XRE_IsParentProcess()) {
        MonitorAutoLock lock(mRefreshTickLock);
        aVsyncTimestamp = mRecentVsync;
        mProcessedVsync = true;
      } else {
        mLastChildTick = TimeStamp::Now();
        mLastProcessedTickInChildProcess = aVsyncTimestamp;
      }

      // On 32-bit Windows we sometimes get times where TimeStamp::Now() is not
      // monotonic because the underlying system apis produce non-monontonic
      // results. (bug 1306896)
#if !defined(_WIN32)
      // Do not compare timestamps unless they are both canonical or fuzzy
      DebugOnly<TimeStamp> rightnow = TimeStamp::Now();
      MOZ_ASSERT_IF(
          (*&rightnow).UsedCanonicalNow() == aVsyncTimestamp.UsedCanonicalNow(),
          aVsyncTimestamp <= *&rightnow);
#endif

      // We might have a problem that we call ~VsyncRefreshDriverTimer() before
      // the scheduled TickRefreshDriver() runs. Check mVsyncRefreshDriverTimer
      // before use.
      if (mVsyncRefreshDriverTimer) {
        RefPtr<VsyncRefreshDriverTimer> timer = mVsyncRefreshDriverTimer;
        timer->RunRefreshDrivers(aId, aVsyncTimestamp);
        // Note: mVsyncRefreshDriverTimer might be null now.
      }

      if (!XRE_IsParentProcess()) {
        TimeDuration tickDuration = TimeStamp::Now() - mLastChildTick;
        mBlockUntil = aVsyncTimestamp + tickDuration;
      }
    }

    // VsyncRefreshDriverTimer holds this RefreshDriverVsyncObserver and it will
    // be always available before Shutdown(). We can just use the raw pointer
    // here.
    VsyncRefreshDriverTimer* mVsyncRefreshDriverTimer;
    Monitor mRefreshTickLock;
    TimeStamp mRecentVsync;
    VsyncId mRecentVsyncId;
    TimeStamp mLastChildTick;
    TimeStamp mLastProcessedTickInChildProcess;
    TimeStamp mBlockUntil;
    TimeDuration mVsyncRate;
    bool mProcessedVsync;
  };  // RefreshDriverVsyncObserver

  ~VsyncRefreshDriverTimer() override {
    if (XRE_IsParentProcess()) {
      mVsyncDispatcher->SetParentRefreshTimer(nullptr);
      mVsyncDispatcher = nullptr;
    } else {
      // Since the PVsyncChild actors live through the life of the process, just
      // send the unobserveVsync message to disable vsync event. We don't need
      // to handle the cleanup stuff of this actor. PVsyncChild::ActorDestroy()
      // will be called and clean up this actor.
      Unused << mVsyncChild->SendUnobserve();
      mVsyncChild->SetVsyncObserver(nullptr);
      mVsyncChild = nullptr;
    }

    // Detach current vsync timer from this VsyncObserver. The observer will no
    // longer tick this timer.
    mVsyncObserver->Shutdown();
    mVsyncObserver = nullptr;
  }

  void StartTimer() override {
    // Protect updates to `sActiveVsyncTimers`.
    MOZ_ASSERT(NS_IsMainThread());

    mLastFireTime = TimeStamp::Now();

    if (XRE_IsParentProcess()) {
      mVsyncDispatcher->SetParentRefreshTimer(mVsyncObserver);
    } else {
      Unused << mVsyncChild->SendObserve();
      mVsyncObserver->OnTimerStart();
    }

    ++sActiveVsyncTimers;
  }

  void StopTimer() override {
    // Protect updates to `sActiveVsyncTimers`.
    MOZ_ASSERT(NS_IsMainThread());

    if (XRE_IsParentProcess()) {
      mVsyncDispatcher->SetParentRefreshTimer(nullptr);
    } else {
      Unused << mVsyncChild->SendUnobserve();
    }

    MOZ_ASSERT(sActiveVsyncTimers > 0);
    --sActiveVsyncTimers;
  }

  void ScheduleNextTick(TimeStamp aNowTime) override {
    // Do nothing since we just wait for the next vsync from
    // RefreshDriverVsyncObserver.
  }

  void RunRefreshDrivers(VsyncId aId, TimeStamp aTimeStamp) {
    Tick(aId, aTimeStamp);
  }

  RefPtr<RefreshDriverVsyncObserver> mVsyncObserver;
  // Used for parent process.
  RefPtr<RefreshTimerVsyncDispatcher> mVsyncDispatcher;
  // Used for child process.
  // The mVsyncChild will be always available before VsncChild::ActorDestroy().
  // After ActorDestroy(), StartTimer() and StopTimer() calls will be non-op.
  RefPtr<VsyncChild> mVsyncChild;
  TimeDuration mVsyncRate;
};  // VsyncRefreshDriverTimer

NS_IMPL_ISUPPORTS_INHERITED(
    VsyncRefreshDriverTimer::RefreshDriverVsyncObserver::
        ParentProcessVsyncNotifier,
    Runnable, nsIRunnablePriority)

mozilla::Atomic<bool> VsyncRefreshDriverTimer::RefreshDriverVsyncObserver::
    ParentProcessVsyncNotifier::sHighPriorityEnabled(false);

/**
 * Since the content process takes some time to setup
 * the vsync IPC connection, this timer is used
 * during the intial startup process.
 * During initial startup, the refresh drivers
 * are ticked off this timer, and are swapped out once content
 * vsync IPC connection is established.
 */
class StartupRefreshDriverTimer : public SimpleTimerBasedRefreshDriverTimer {
 public:
  explicit StartupRefreshDriverTimer(double aRate)
      : SimpleTimerBasedRefreshDriverTimer(aRate) {}

 protected:
  void ScheduleNextTick(TimeStamp aNowTime) override {
    // Since this is only used for startup, it isn't super critical
    // that we tick at consistent intervals.
    TimeStamp newTarget = aNowTime + mRateDuration;
    uint32_t delay =
        static_cast<uint32_t>((newTarget - aNowTime).ToMilliseconds());
    mTimer->InitWithNamedFuncCallback(
        TimerTick, this, delay, nsITimer::TYPE_ONE_SHOT,
        "StartupRefreshDriverTimer::ScheduleNextTick");
    mTargetTime = newTarget;
  }
};

/*
 * A RefreshDriverTimer for inactive documents.  When a new refresh driver is
 * added, the rate is reset to the base (normally 1s/1fps).  Every time
 * it ticks, a single refresh driver is poked.  Once they have all been poked,
 * the duration between ticks doubles, up to mDisableAfterMilliseconds.  At that
 * point, the timer is quiet and doesn't tick (until something is added to it
 * again).
 *
 * When a timer is removed, there is a possibility of another timer
 * being skipped for one cycle.  We could avoid this by adjusting
 * mNextDriverIndex in RemoveRefreshDriver, but there's little need to
 * add that complexity.  All we want is for inactive drivers to tick
 * at some point, but we don't care too much about how often.
 */
class InactiveRefreshDriverTimer final
    : public SimpleTimerBasedRefreshDriverTimer {
 public:
  explicit InactiveRefreshDriverTimer(double aRate)
      : SimpleTimerBasedRefreshDriverTimer(aRate),
        mNextTickDuration(aRate),
        mDisableAfterMilliseconds(-1.0),
        mNextDriverIndex(0) {}

  InactiveRefreshDriverTimer(double aRate, double aDisableAfterMilliseconds)
      : SimpleTimerBasedRefreshDriverTimer(aRate),
        mNextTickDuration(aRate),
        mDisableAfterMilliseconds(aDisableAfterMilliseconds),
        mNextDriverIndex(0) {}

  void AddRefreshDriver(nsRefreshDriver* aDriver) override {
    RefreshDriverTimer::AddRefreshDriver(aDriver);

    LOG("[%p] inactive timer got new refresh driver %p, resetting rate", this,
        aDriver);

    // reset the timer, and start with the newly added one next time.
    mNextTickDuration = mRateMilliseconds;

    // we don't really have to start with the newly added one, but we may as
    // well not tick the old ones at the fastest rate any more than we need to.
    mNextDriverIndex = GetRefreshDriverCount() - 1;

    StopTimer();
    StartTimer();
  }

  TimeDuration GetTimerRate() override {
    return TimeDuration::FromMilliseconds(mNextTickDuration);
  }

 protected:
  uint32_t GetRefreshDriverCount() {
    return mContentRefreshDrivers.Length() + mRootRefreshDrivers.Length();
  }

  void StartTimer() override {
    mLastFireTime = TimeStamp::Now();

    mTargetTime = mLastFireTime + mRateDuration;

    uint32_t delay = static_cast<uint32_t>(mRateMilliseconds);
    mTimer->InitWithNamedFuncCallback(TimerTickOne, this, delay,
                                      nsITimer::TYPE_ONE_SHOT,
                                      "InactiveRefreshDriverTimer::StartTimer");
  }

  void StopTimer() override { mTimer->Cancel(); }

  void ScheduleNextTick(TimeStamp aNowTime) override {
    if (mDisableAfterMilliseconds > 0.0 &&
        mNextTickDuration > mDisableAfterMilliseconds) {
      // We hit the time after which we should disable
      // inactive window refreshes; don't schedule anything
      // until we get kicked by an AddRefreshDriver call.
      return;
    }

    // double the next tick time if we've already gone through all of them once
    if (mNextDriverIndex >= GetRefreshDriverCount()) {
      mNextTickDuration *= 2.0;
      mNextDriverIndex = 0;
    }

    // this doesn't need to be precise; do a simple schedule
    uint32_t delay = static_cast<uint32_t>(mNextTickDuration);
    mTimer->InitWithNamedFuncCallback(
        TimerTickOne, this, delay, nsITimer::TYPE_ONE_SHOT,
        "InactiveRefreshDriverTimer::ScheduleNextTick");

    LOG("[%p] inactive timer next tick in %f ms [index %d/%d]", this,
        mNextTickDuration, mNextDriverIndex, GetRefreshDriverCount());
  }

  /* Runs just one driver's tick. */
  void TickOne() {
    TimeStamp now = TimeStamp::Now();

    ScheduleNextTick(now);

    mLastFireTime = now;

    nsTArray<RefPtr<nsRefreshDriver>> drivers(mContentRefreshDrivers);
    drivers.AppendElements(mRootRefreshDrivers);
    size_t index = mNextDriverIndex;

    if (index < drivers.Length() &&
        !drivers[index]->IsTestControllingRefreshesEnabled()) {
      TickDriver(drivers[index], VsyncId(), now);
    }

    mNextDriverIndex++;
  }

  static void TimerTickOne(nsITimer* aTimer, void* aClosure) {
    RefPtr<InactiveRefreshDriverTimer> timer =
        static_cast<InactiveRefreshDriverTimer*>(aClosure);
    timer->TickOne();
  }

  double mNextTickDuration;
  double mDisableAfterMilliseconds;
  uint32_t mNextDriverIndex;
};

}  // namespace mozilla

static StaticRefPtr<RefreshDriverTimer> sRegularRateTimer;
static StaticRefPtr<InactiveRefreshDriverTimer> sThrottledRateTimer;

static void CreateContentVsyncRefreshTimer(void*) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!XRE_IsParentProcess());

  // Create the PVsync actor child for vsync-base refresh timer.
  // PBackgroundChild is created synchronously. We will still use software
  // timer before PVsync ready, and change to use hw timer when the connection
  // is done. Please check nsRefreshDriver::PVsyncActorCreated().

  PBackgroundChild* actorChild = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    return;
  }

  layout::PVsyncChild* actor = actorChild->SendPVsyncConstructor();
  if (NS_WARN_IF(!actor)) {
    return;
  }

  layout::VsyncChild* child = static_cast<layout::VsyncChild*>(actor);
  nsRefreshDriver::PVsyncActorCreated(child);
}

static void CreateVsyncRefreshTimer() {
  MOZ_ASSERT(NS_IsMainThread());

  PodArrayZero(sJankLevels);
  // Sometimes, gfxPrefs is not initialized here. Make sure the gfxPrefs is
  // ready.
  gfxPrefs::GetSingleton();

  if (gfxPlatform::IsInLayoutAsapMode()) {
    return;
  }

  if (XRE_IsParentProcess()) {
    // Make sure all vsync systems are ready.
    gfxPlatform::GetPlatform();
    // In parent process, we don't need to use ipc. We can create the
    // VsyncRefreshDriverTimer directly.
    sRegularRateTimer = new VsyncRefreshDriverTimer();
    return;
  }

  // If this process is not created by NUWA, just create the vsync timer here.
  CreateContentVsyncRefreshTimer(nullptr);
}

static uint32_t GetFirstFrameDelay(imgIRequest* req) {
  nsCOMPtr<imgIContainer> container;
  if (NS_FAILED(req->GetImage(getter_AddRefs(container))) || !container) {
    return 0;
  }

  // If this image isn't animated, there isn't a first frame delay.
  int32_t delay = container->GetFirstFrameDelay();
  if (delay < 0) return 0;

  return static_cast<uint32_t>(delay);
}

/* static */
void nsRefreshDriver::Shutdown() {
  // clean up our timers
  sRegularRateTimer = nullptr;
  sThrottledRateTimer = nullptr;
}

/* static */
int32_t nsRefreshDriver::DefaultInterval() {
  return NSToIntRound(1000.0 / gfxPlatform::GetDefaultFrameRate());
}

// Compute the interval to use for the refresh driver timer, in milliseconds.
// outIsDefault indicates that rate was not explicitly set by the user
// so we might choose other, more appropriate rates (e.g. vsync, etc)
// layout.frame_rate=0 indicates "ASAP mode".
// In ASAP mode rendering is iterated as fast as possible (typically for stress
// testing). A target rate of 10k is used internally instead of special-handling
// 0. Backends which block on swap/present/etc should try to not block when
// layout.frame_rate=0 - to comply with "ASAP" as much as possible.
double nsRefreshDriver::GetRegularTimerInterval() const {
  int32_t rate = Preferences::GetInt("layout.frame_rate", -1);
  if (rate < 0) {
    rate = gfxPlatform::GetDefaultFrameRate();
  } else if (rate == 0) {
    rate = 10000;
  }

  return 1000.0 / rate;
}

/* static */
double nsRefreshDriver::GetThrottledTimerInterval() {
  int32_t rate = Preferences::GetInt("layout.throttled_frame_rate", -1);
  if (rate <= 0) {
    rate = DEFAULT_THROTTLED_FRAME_RATE;
  }
  return 1000.0 / rate;
}

/* static */ mozilla::TimeDuration
nsRefreshDriver::GetMinRecomputeVisibilityInterval() {
  int32_t interval =
      Preferences::GetInt("layout.visibility.min-recompute-interval-ms", -1);
  if (interval <= 0) {
    interval = DEFAULT_RECOMPUTE_VISIBILITY_INTERVAL_MS;
  }
  return TimeDuration::FromMilliseconds(interval);
}

RefreshDriverTimer* nsRefreshDriver::ChooseTimer() const {
  if (mThrottled) {
    if (!sThrottledRateTimer)
      sThrottledRateTimer = new InactiveRefreshDriverTimer(
          GetThrottledTimerInterval(),
          DEFAULT_INACTIVE_TIMER_DISABLE_SECONDS * 1000.0);
    return sThrottledRateTimer;
  }

  if (!sRegularRateTimer) {
    double rate = GetRegularTimerInterval();

    // Try to use vsync-base refresh timer first for sRegularRateTimer.
    CreateVsyncRefreshTimer();

    if (!sRegularRateTimer) {
      sRegularRateTimer = new StartupRefreshDriverTimer(rate);
    }
  }
  return sRegularRateTimer;
}

nsRefreshDriver::nsRefreshDriver(nsPresContext* aPresContext)
    : mActiveTimer(nullptr),
      mPresContext(aPresContext),
      mRootRefresh(nullptr),
      mNextTransactionId{0},
      mOutstandingTransactionId{0},
      mCompletedTransaction{0},
      mFreezeCount(0),
      mThrottledFrameRequestInterval(
          TimeDuration::FromMilliseconds(GetThrottledTimerInterval())),
      mMinRecomputeVisibilityInterval(GetMinRecomputeVisibilityInterval()),
      mThrottled(false),
      mNeedToRecomputeVisibility(false),
      mTestControllingRefreshes(false),
      mViewManagerFlushIsPending(false),
      mHasScheduleFlush(false),
      mInRefresh(false),
      mWaitingForTransaction(false),
      mSkippedPaints(false),
      mResizeSuppressed(false),
      mNotifyDOMContentFlushed(false),
      mWarningThreshold(REFRESH_WAIT_WARNING) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPresContext,
             "Need a pres context to tell us to call Disconnect() later "
             "and decrement sRefreshDriverCount.");
  mMostRecentRefresh = TimeStamp::Now();
  mNextThrottledFrameRequestTick = mMostRecentRefresh;
  mNextRecomputeVisibilityTick = mMostRecentRefresh;

  ++sRefreshDriverCount;
}

nsRefreshDriver::~nsRefreshDriver() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(ObserverCount() == mEarlyRunners.Length(),
             "observers, except pending selection scrolls, "
             "should have been unregistered");
  MOZ_ASSERT(!mActiveTimer, "timer should be gone");
  MOZ_ASSERT(!mPresContext,
             "Should have called Disconnect() and decremented "
             "sRefreshDriverCount!");

  if (mRootRefresh) {
    mRootRefresh->RemoveRefreshObserver(this, FlushType::Style);
    mRootRefresh = nullptr;
  }
}

// Method for testing.  See nsIDOMWindowUtils.advanceTimeAndRefresh
// for description.
void nsRefreshDriver::AdvanceTimeAndRefresh(int64_t aMilliseconds) {
  // ensure that we're removed from our driver
  StopTimer();

  if (!mTestControllingRefreshes) {
    mMostRecentRefresh = TimeStamp::Now();

    mTestControllingRefreshes = true;
    if (mWaitingForTransaction) {
      // Disable any refresh driver throttling when entering test mode
      mWaitingForTransaction = false;
      mSkippedPaints = false;
      mWarningThreshold = REFRESH_WAIT_WARNING;
    }
  }

  mMostRecentRefresh += TimeDuration::FromMilliseconds((double)aMilliseconds);

  mozilla::dom::AutoNoJSAPI nojsapi;
  DoTick();
}

void nsRefreshDriver::RestoreNormalRefresh() {
  mTestControllingRefreshes = false;
  EnsureTimerStarted(eAllowTimeToGoBackwards);
  mCompletedTransaction = mOutstandingTransactionId = mNextTransactionId;
}

TimeStamp nsRefreshDriver::MostRecentRefresh() const {
  // In case of stylo traversal, we have already activated the refresh driver in
  // RestyleManager::ProcessPendingRestyles().
  if (!ServoStyleSet::IsInServoTraversal()) {
    const_cast<nsRefreshDriver*>(this)->EnsureTimerStarted();
  }

  return mMostRecentRefresh;
}

void nsRefreshDriver::AddRefreshObserver(nsARefreshObserver* aObserver,
                                         FlushType aFlushType) {
  ObserverArray& array = ArrayFor(aFlushType);
  array.AppendElement(aObserver);
  EnsureTimerStarted();
}

bool nsRefreshDriver::RemoveRefreshObserver(nsARefreshObserver* aObserver,
                                            FlushType aFlushType) {
  ObserverArray& array = ArrayFor(aFlushType);
  return array.RemoveElement(aObserver);
}

void nsRefreshDriver::AddTimerAdjustmentObserver(
    nsATimerAdjustmentObserver* aObserver) {
  MOZ_ASSERT(!mTimerAdjustmentObservers.Contains(aObserver));
  mTimerAdjustmentObservers.AppendElement(aObserver);
}

void nsRefreshDriver::RemoveTimerAdjustmentObserver(
    nsATimerAdjustmentObserver* aObserver) {
  MOZ_ASSERT(mTimerAdjustmentObservers.Contains(aObserver));
  mTimerAdjustmentObservers.RemoveElement(aObserver);
}

void nsRefreshDriver::PostVisualViewportResizeEvent(
    VVPResizeEvent* aResizeEvent) {
  mVisualViewportResizeEvents.AppendElement(aResizeEvent);
  EnsureTimerStarted();
}

void nsRefreshDriver::DispatchVisualViewportResizeEvents() {
  // We're taking a hint from scroll events and only dispatch the current set
  // of queued resize events. If additional events are posted in response to
  // the current events being dispatched, we'll dispatch them on the next tick.
  VisualViewportResizeEventArray events;
  events.SwapElements(mVisualViewportResizeEvents);
  for (auto& event : events) {
    event->Run();
  }
}

void nsRefreshDriver::PostScrollEvent(mozilla::Runnable* aScrollEvent,
                                      bool aDelayed) {
  if (aDelayed) {
    mDelayedScrollEvents.AppendElement(aScrollEvent);
  } else {
    mScrollEvents.AppendElement(aScrollEvent);
    EnsureTimerStarted();
  }
}

void nsRefreshDriver::DispatchScrollEvents() {
  // Scroll events are one-shot, so after running them we can drop them.
  // However, dispatching a scroll event can potentially cause more scroll
  // events to be posted, so we move the initial set into a temporary array
  // first. (Newly posted scroll events will be dispatched on the next tick.)
  ScrollEventArray events;
  events.SwapElements(mScrollEvents);
  for (auto& event : events) {
    event->Run();
  }
}

void nsRefreshDriver::PostVisualViewportScrollEvent(
    VVPScrollEvent* aScrollEvent) {
  mVisualViewportScrollEvents.AppendElement(aScrollEvent);
  EnsureTimerStarted();
}

void nsRefreshDriver::DispatchVisualViewportScrollEvents() {
  // Scroll events are one-shot, so after running them we can drop them.
  // However, dispatching a scroll event can potentially cause more scroll
  // events to be posted, so we move the initial set into a temporary array
  // first. (Newly posted scroll events will be dispatched on the next tick.)
  VisualViewportScrollEventArray events;
  events.SwapElements(mVisualViewportScrollEvents);
  for (auto& event : events) {
    event->Run();
  }
}

void nsRefreshDriver::AddPostRefreshObserver(
    nsAPostRefreshObserver* aObserver) {
  mPostRefreshObservers.AppendElement(aObserver);
}

void nsRefreshDriver::RemovePostRefreshObserver(
    nsAPostRefreshObserver* aObserver) {
  mPostRefreshObservers.RemoveElement(aObserver);
}

bool nsRefreshDriver::AddImageRequest(imgIRequest* aRequest) {
  uint32_t delay = GetFirstFrameDelay(aRequest);
  if (delay == 0) {
    mRequests.PutEntry(aRequest);
  } else {
    ImageStartData* start = mStartTable.LookupForAdd(delay).OrInsert(
        []() { return new ImageStartData(); });
    start->mEntries.PutEntry(aRequest);
  }

  EnsureTimerStarted();

  return true;
}

void nsRefreshDriver::RemoveImageRequest(imgIRequest* aRequest) {
  // Try to remove from both places, just in case, because we can't tell
  // whether RemoveEntry() succeeds.
  mRequests.RemoveEntry(aRequest);
  uint32_t delay = GetFirstFrameDelay(aRequest);
  if (delay != 0) {
    ImageStartData* start = mStartTable.Get(delay);
    if (start) {
      start->mEntries.RemoveEntry(aRequest);
    }
  }
}

void nsRefreshDriver::NotifyDOMContentLoaded() {
  // If the refresh driver is going to tick, we mark the timestamp after
  // everything is flushed in the next tick. If it isn't, mark ourselves as
  // flushed now.
  if (!HasObservers()) {
    GetPresContext()->NotifyDOMContentFlushed();
  } else {
    mNotifyDOMContentFlushed = true;
  }
}

void nsRefreshDriver::RunDelayedEventsSoon() {
  // Place entries for delayed events into their corresponding normal list,
  // and schedule a refresh. When these delayed events run, if their document
  // still has events suppressed then they will be readded to the delayed
  // events list.

  mScrollEvents.AppendElements(mDelayedScrollEvents);
  mDelayedScrollEvents.Clear();

  mResizeEventFlushObservers.AppendElements(mDelayedResizeEventFlushObservers);
  mDelayedResizeEventFlushObservers.Clear();

  EnsureTimerStarted();
}

void nsRefreshDriver::EnsureTimerStarted(EnsureTimerStartedFlags aFlags) {
  // FIXME: Bug 1346065: We should also assert the case where we have
  // STYLO_THREADS=1.
  MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal() || NS_IsMainThread(),
             "EnsureTimerStarted should be called only when we are not "
             "in servo traversal or on the main-thread");

  if (mTestControllingRefreshes) return;

  // will it already fire, and no other changes needed?
  if (mActiveTimer && !(aFlags & eForceAdjustTimer)) return;

  if (IsFrozen() || !mPresContext) {
    // If we don't want to start it now, or we've been disconnected.
    StopTimer();
    return;
  }

  if (mPresContext->Document()->IsBeingUsedAsImage()) {
    // Image documents receive ticks from clients' refresh drivers.
    // XXXdholbert Exclude SVG-in-opentype fonts from this optimization, until
    // they receive refresh-driver ticks from their client docs (bug 1107252).
    nsIURI* uri = mPresContext->Document()->GetDocumentURI();
    if (!uri || !mozilla::dom::IsFontTableURI(uri)) {
      MOZ_ASSERT(!mActiveTimer,
                 "image doc refresh driver should never have its own timer");
      return;
    }
  }

  // We got here because we're either adjusting the time *or* we're
  // starting it for the first time.  Add to the right timer,
  // prehaps removing it from a previously-set one.
  RefreshDriverTimer* newTimer = ChooseTimer();
  if (newTimer != mActiveTimer) {
    if (mActiveTimer) mActiveTimer->RemoveRefreshDriver(this);
    mActiveTimer = newTimer;
    mActiveTimer->AddRefreshDriver(this);
  }

  // When switching from an inactive timer to an active timer, the root
  // refresh driver is skipped due to being set to the content refresh
  // driver's timestamp. In case of EnsureTimerStarted is called from
  // ScheduleViewManagerFlush, we should avoid this behavior to flush
  // a paint in the same tick on the root refresh driver.
  if (aFlags & eNeverAdjustTimer) {
    return;
  }

  // Since the different timers are sampled at different rates, when switching
  // timers, the most recent refresh of the new timer may be *before* the
  // most recent refresh of the old timer. However, the refresh driver time
  // should not go backwards so we clamp the most recent refresh time.
  //
  // The one exception to this is when we are restoring the refresh driver
  // from test control in which case the time is expected to go backwards
  // (see bug 1043078).
  TimeStamp newMostRecentRefresh =
      aFlags & eAllowTimeToGoBackwards
          ? mActiveTimer->MostRecentRefresh()
          : std::max(mActiveTimer->MostRecentRefresh(), mMostRecentRefresh);

  if (mMostRecentRefresh != newMostRecentRefresh) {
    mMostRecentRefresh = newMostRecentRefresh;

    nsTObserverArray<nsATimerAdjustmentObserver*>::EndLimitedIterator iter(
        mTimerAdjustmentObservers);
    while (iter.HasMore()) {
      nsATimerAdjustmentObserver* obs = iter.GetNext();
      obs->NotifyTimerAdjusted(mMostRecentRefresh);
    }
  }
}

void nsRefreshDriver::StopTimer() {
  if (!mActiveTimer) return;

  mActiveTimer->RemoveRefreshDriver(this);
  mActiveTimer = nullptr;
}

uint32_t nsRefreshDriver::ObserverCount() const {
  uint32_t sum = 0;
  for (const ObserverArray& array : mObservers) {
    sum += array.Length();
  }

  // Even while throttled, we need to process layout and style changes.  Style
  // changes can trigger transitions which fire events when they complete, and
  // layout changes can affect media queries on child documents, triggering
  // style changes, etc.
  sum += mAnimationEventFlushObservers.Length();
  sum += mResizeEventFlushObservers.Length();
  sum += mStyleFlushObservers.Length();
  sum += mLayoutFlushObservers.Length();
  sum += mPendingFullscreenEvents.Length();
  sum += mFrameRequestCallbackDocs.Length();
  sum += mThrottledFrameRequestCallbackDocs.Length();
  sum += mViewManagerFlushIsPending;
  sum += mEarlyRunners.Length();
  sum += mTimerAdjustmentObservers.Length();
  return sum;
}

bool nsRefreshDriver::HasObservers() const {
  for (const ObserverArray& array : mObservers) {
    if (!array.IsEmpty()) {
      return true;
    }
  }

  // We should NOT count mTimerAdjustmentObservers here since this method is
  // used to determine whether or not to stop the timer or re-start it and timer
  // adjustment observers should not influence timer starting or stopping.
  return mViewManagerFlushIsPending || !mStyleFlushObservers.IsEmpty() ||
         !mLayoutFlushObservers.IsEmpty() ||
         !mAnimationEventFlushObservers.IsEmpty() ||
         !mResizeEventFlushObservers.IsEmpty() ||
         !mPendingFullscreenEvents.IsEmpty() ||
         !mFrameRequestCallbackDocs.IsEmpty() ||
         !mThrottledFrameRequestCallbackDocs.IsEmpty() ||
         !mEarlyRunners.IsEmpty();
}

bool nsRefreshDriver::HasImageRequests() const {
  for (auto iter = mStartTable.ConstIter(); !iter.Done(); iter.Next()) {
    if (!iter.UserData()->mEntries.IsEmpty()) {
      return true;
    }
  }

  return !mRequests.IsEmpty();
}

nsRefreshDriver::ObserverArray& nsRefreshDriver::ArrayFor(
    FlushType aFlushType) {
  switch (aFlushType) {
    case FlushType::Event:
      return mObservers[0];
    case FlushType::Style:
      return mObservers[1];
    case FlushType::Layout:
      return mObservers[2];
    case FlushType::Display:
      return mObservers[3];
    default:
      MOZ_CRASH("We don't track refresh observers for this flush type");
  }
}

/*
 * nsITimerCallback implementation
 */

void nsRefreshDriver::DoTick() {
  MOZ_ASSERT(!IsFrozen(), "Why are we notified while frozen?");
  MOZ_ASSERT(mPresContext, "Why are we notified after disconnection?");
  MOZ_ASSERT(!nsContentUtils::GetCurrentJSContext(),
             "Shouldn't have a JSContext on the stack");

  if (mTestControllingRefreshes) {
    Tick(VsyncId(), mMostRecentRefresh);
  } else {
    Tick(VsyncId(), TimeStamp::Now());
  }
}

struct DocumentFrameCallbacks {
  explicit DocumentFrameCallbacks(Document* aDocument) : mDocument(aDocument) {}

  RefPtr<Document> mDocument;
  Document::FrameRequestCallbackList mCallbacks;
};

static nsDocShell* GetDocShell(nsPresContext* aPresContext) {
  return static_cast<nsDocShell*>(aPresContext->GetDocShell());
}

static bool HasPendingAnimations(nsIPresShell* aShell) {
  Document* doc = aShell->GetDocument();
  if (!doc) {
    return false;
  }

  PendingAnimationTracker* tracker = doc->GetPendingAnimationTracker();
  return tracker && tracker->HasPendingAnimations();
}

/**
 * Return a list of all the child docShells in a given root docShell that are
 * visible and are recording markers for the profilingTimeline
 */
static void GetProfileTimelineSubDocShells(nsDocShell* aRootDocShell,
                                           nsTArray<nsDocShell*>& aShells) {
  if (!aRootDocShell) {
    return;
  }

  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  if (!timelines || timelines->IsEmpty()) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  nsresult rv = aRootDocShell->GetDocShellEnumerator(
      nsIDocShellTreeItem::typeAll, nsIDocShell::ENUMERATE_BACKWARDS,
      getter_AddRefs(enumerator));

  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIDocShell> curItem;
  bool hasMore = false;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> curSupports;
    enumerator->GetNext(getter_AddRefs(curSupports));
    curItem = do_QueryInterface(curSupports);

    if (!curItem || !curItem->GetRecordProfileTimelineMarkers()) {
      continue;
    }

    nsDocShell* shell = static_cast<nsDocShell*>(curItem.get());
    bool isVisible = false;
    shell->GetVisibility(&isVisible);
    if (!isVisible) {
      continue;
    }

    aShells.AppendElement(shell);
  }
}

static void TakeFrameRequestCallbacksFrom(
    Document* aDocument, nsTArray<DocumentFrameCallbacks>& aTarget) {
  aTarget.AppendElement(aDocument);
  aDocument->TakeFrameRequestCallbacks(aTarget.LastElement().mCallbacks);
}

// https://fullscreen.spec.whatwg.org/#run-the-fullscreen-steps
void nsRefreshDriver::RunFullscreenSteps() {
  // Swap out the current pending events
  nsTArray<UniquePtr<PendingFullscreenEvent>> pendings(
      std::move(mPendingFullscreenEvents));
  for (UniquePtr<PendingFullscreenEvent>& event : pendings) {
    event->Dispatch();
  }
}

void nsRefreshDriver::UpdateIntersectionObservations() {
  AutoTArray<RefPtr<Document>, 32> documents;

  if (mPresContext->Document()->HasIntersectionObservers()) {
    documents.AppendElement(mPresContext->Document());
  }

  mPresContext->Document()->CollectDescendantDocuments(
      documents, [](const Document* document) -> bool {
        return document->HasIntersectionObservers();
      });

  for (uint32_t i = 0; i < documents.Length(); ++i) {
    Document* doc = documents[i];
    doc->UpdateIntersectionObservations();
    doc->ScheduleIntersectionObserverNotification();
  }
}

void nsRefreshDriver::DispatchAnimationEvents() {
  if (!mPresContext) {
    return;
  }

  // Hold all AnimationEventDispatcher in mAnimationEventFlushObservers as
  // a RefPtr<> array since each AnimationEventDispatcher might be destroyed
  // during processing the previous dispatcher.
  AutoTArray<RefPtr<AnimationEventDispatcher>, 16> dispatchers;
  dispatchers.AppendElements(mAnimationEventFlushObservers);
  mAnimationEventFlushObservers.Clear();

  for (auto& dispatcher : dispatchers) {
    dispatcher->DispatchEvents();
  }
}

void nsRefreshDriver::RunFrameRequestCallbacks(TimeStamp aNowTime) {
  // Grab all of our frame request callbacks up front.
  nsTArray<DocumentFrameCallbacks> frameRequestCallbacks(
      mFrameRequestCallbackDocs.Length() +
      mThrottledFrameRequestCallbackDocs.Length());

  // First, grab throttled frame request callbacks.
  {
    nsTArray<Document*> docsToRemove;

    // We always tick throttled frame requests if the entire refresh driver is
    // throttled, because in that situation throttled frame requests tick at the
    // same frequency as non-throttled frame requests.
    bool tickThrottledFrameRequests = mThrottled;

    if (!tickThrottledFrameRequests &&
        aNowTime >= mNextThrottledFrameRequestTick) {
      mNextThrottledFrameRequestTick =
          aNowTime + mThrottledFrameRequestInterval;
      tickThrottledFrameRequests = true;
    }

    for (Document* doc : mThrottledFrameRequestCallbackDocs) {
      if (tickThrottledFrameRequests) {
        // We're ticking throttled documents, so grab this document's requests.
        // We don't bother appending to docsToRemove because we're going to
        // clear mThrottledFrameRequestCallbackDocs anyway.
        TakeFrameRequestCallbacksFrom(doc, frameRequestCallbacks);
      } else if (!doc->ShouldThrottleFrameRequests()) {
        // This document is no longer throttled, so grab its requests even
        // though we're not ticking throttled frame requests right now. If
        // this is the first unthrottled document with frame requests, we'll
        // enter high precision mode the next time the callback is scheduled.
        TakeFrameRequestCallbacksFrom(doc, frameRequestCallbacks);
        docsToRemove.AppendElement(doc);
      }
    }

    // Remove all the documents we're ticking from
    // mThrottledFrameRequestCallbackDocs so they can be readded as needed.
    if (tickThrottledFrameRequests) {
      mThrottledFrameRequestCallbackDocs.Clear();
    } else {
      // XXX(seth): We're using this approach to avoid concurrent modification
      // of mThrottledFrameRequestCallbackDocs. docsToRemove usually has either
      // zero elements or a very small number, so this should be OK in practice.
      for (Document* doc : docsToRemove) {
        mThrottledFrameRequestCallbackDocs.RemoveElement(doc);
      }
    }
  }

  // Now grab unthrottled frame request callbacks.
  for (Document* doc : mFrameRequestCallbackDocs) {
    TakeFrameRequestCallbacksFrom(doc, frameRequestCallbacks);
  }

  // Reset mFrameRequestCallbackDocs so they can be readded as needed.
  mFrameRequestCallbackDocs.Clear();

  if (!frameRequestCallbacks.IsEmpty()) {
    AUTO_PROFILER_TRACING_DOCSHELL("Paint", "Scripts", GRAPHICS,
                                   GetDocShell(mPresContext));
    for (const DocumentFrameCallbacks& docCallbacks : frameRequestCallbacks) {
      // XXXbz Bug 863140: GetInnerWindow can return the outer
      // window in some cases.
      nsPIDOMWindowInner* innerWindow =
          docCallbacks.mDocument->GetInnerWindow();
      DOMHighResTimeStamp timeStamp = 0;
      if (innerWindow) {
        mozilla::dom::Performance* perf = innerWindow->GetPerformance();
        if (perf) {
          timeStamp = perf->GetDOMTiming()->TimeStampToDOMHighRes(aNowTime);
        }
        // else window is partially torn down already
      }
      for (auto& callback : docCallbacks.mCallbacks) {
        callback->Call(timeStamp);
      }
    }
  }
}

struct RunnableWithDelay {
  nsCOMPtr<nsIRunnable> mRunnable;
  uint32_t mDelay;
};

static AutoTArray<RunnableWithDelay, 8>* sPendingIdleRunnables = nullptr;

void nsRefreshDriver::DispatchIdleRunnableAfterTick(nsIRunnable* aRunnable,
                                                    uint32_t aDelay) {
  if (!sPendingIdleRunnables) {
    sPendingIdleRunnables = new AutoTArray<RunnableWithDelay, 8>();
  }

  RunnableWithDelay rwd = {aRunnable, aDelay};
  sPendingIdleRunnables->AppendElement(rwd);
}

void nsRefreshDriver::CancelIdleRunnable(nsIRunnable* aRunnable) {
  if (!sPendingIdleRunnables) {
    return;
  }

  for (uint32_t i = 0; i < sPendingIdleRunnables->Length(); ++i) {
    if ((*sPendingIdleRunnables)[i].mRunnable == aRunnable) {
      sPendingIdleRunnables->RemoveElementAt(i);
      break;
    }
  }

  if (sPendingIdleRunnables->IsEmpty()) {
    delete sPendingIdleRunnables;
    sPendingIdleRunnables = nullptr;
  }
}

void nsRefreshDriver::Tick(VsyncId aId, TimeStamp aNowTime) {
  MOZ_ASSERT(!nsContentUtils::GetCurrentJSContext(),
             "Shouldn't have a JSContext on the stack");

  if (nsNPAPIPluginInstance::InPluginCallUnsafeForReentry()) {
    NS_ERROR("Refresh driver should not run during plugin call!");
    // Try to survive this by just ignoring the refresh tick.
    return;
  }

  AUTO_PROFILER_LABEL("nsRefreshDriver::Tick", LAYOUT);

  // We're either frozen or we were disconnected (likely in the middle
  // of a tick iteration).  Just do nothing here, since our
  // prescontext went away.
  if (IsFrozen() || !mPresContext) {
    return;
  }

  // We can have a race condition where the vsync timestamp
  // is before the most recent refresh due to a forced refresh.
  // The underlying assumption is that the refresh driver tick can only
  // go forward in time, not backwards. To prevent the refresh
  // driver from going back in time, just skip this tick and
  // wait until the next tick.
  if ((aNowTime <= mMostRecentRefresh) && !mTestControllingRefreshes) {
    return;
  }

  if (IsWaitingForPaint(aNowTime)) {
    // We're currently suspended waiting for earlier Tick's to
    // be completed (on the Compositor). Mark that we missed the paint
    // and keep waiting.
    PROFILER_ADD_MARKER("nsRefreshDriver::Tick waiting for paint", LAYOUT);
    return;
  }

  TimeStamp previousRefresh = mMostRecentRefresh;
  mMostRecentRefresh = aNowTime;

  if (mRootRefresh) {
    mRootRefresh->RemoveRefreshObserver(this, FlushType::Style);
    mRootRefresh = nullptr;
  }
  mSkippedPaints = false;
  mWarningThreshold = 1;

  nsCOMPtr<nsIPresShell> presShell = mPresContext->GetPresShell();
  if (!presShell ||
      (!HasObservers() && !HasImageRequests() &&
       mVisualViewportResizeEvents.IsEmpty() && mScrollEvents.IsEmpty() &&
       mVisualViewportScrollEvents.IsEmpty())) {
    // Things are being destroyed, or we no longer have any observers.
    // We don't want to stop the timer when observers are initially
    // removed, because sometimes observers can be added and removed
    // often depending on what other things are going on and in that
    // situation we don't want to thrash our timer.  So instead we
    // wait until we get a Notify() call when we have no observers
    // before stopping the timer.
    // On top level content pages keep the timer running initially so that we
    // paint the page soon enough.
    if (presShell && !mThrottled && !mTestControllingRefreshes &&
        XRE_IsContentProcess() &&
        mPresContext->Document()->IsTopLevelContentDocument() &&
        !gfxPlatform::IsInLayoutAsapMode() &&
        !mPresContext->HadContentfulPaint() &&
        mPresContext->Document()->GetReadyStateEnum() <
            Document::READYSTATE_COMPLETE) {
      if (mInitialTimerRunningLimit.IsNull()) {
        mInitialTimerRunningLimit =
            TimeStamp::Now() + TimeDuration::FromSeconds(4.0f);
        // Don't let the timer to run forever, so limit to 4s for now.
      } else if (mInitialTimerRunningLimit < TimeStamp::Now()) {
        StopTimer();
      }
    } else {
      StopTimer();
    }
    return;
  }

  mResizeSuppressed = false;

  AutoRestore<bool> restoreInRefresh(mInRefresh);
  mInRefresh = true;

  AutoRestore<TimeStamp> restoreTickStart(mTickStart);
  mTickStart = TimeStamp::Now();
  mTickVsyncId = aId;
  mTickVsyncTime = aNowTime;

  gfxPlatform::GetPlatform()->SchedulePaintIfDeviceReset();

  // We want to process any pending APZ metrics ahead of their positions
  // in the queue. This will prevent us from spending precious time
  // painting a stale displayport.
  if (gfxPrefs::APZPeekMessages()) {
    nsLayoutUtils::UpdateDisplayPortMarginsFromPendingMessages();
  }

  AutoTArray<nsCOMPtr<nsIRunnable>, 16> earlyRunners;
  earlyRunners.SwapElements(mEarlyRunners);
  for (auto& runner : earlyRunners) {
    runner->Run();
  }

  // Resize events should be fired before layout flushes or
  // calling animation frame callbacks.
  AutoTArray<nsIPresShell*, 16> observers;
  observers.AppendElements(mResizeEventFlushObservers);
  for (nsIPresShell* shell : Reversed(observers)) {
    if (!mPresContext || !mPresContext->GetPresShell()) {
      StopTimer();
      return;
    }
    // Make sure to not process observers which might have been removed
    // during previous iterations.
    if (!mResizeEventFlushObservers.RemoveElement(shell)) {
      continue;
    }
    shell->FireResizeEvent();
  }
  DispatchVisualViewportResizeEvents();

  /*
   * The timer holds a reference to |this| while calling |Notify|.
   * However, implementations of |WillRefresh| are permitted to destroy
   * the pres context, which will cause our |mPresContext| to become
   * null.  If this happens, we must stop notifying observers.
   */
  for (uint32_t i = 0; i < ArrayLength(mObservers); ++i) {
    ObserverArray::EndLimitedIterator etor(mObservers[i]);
    while (etor.HasMore()) {
      RefPtr<nsARefreshObserver> obs = etor.GetNext();
      obs->WillRefresh(aNowTime);

      if (!mPresContext || !mPresContext->GetPresShell()) {
        StopTimer();
        return;
      }
    }

    if (i == 1) {
      // This is the FlushType::Style case.

      DispatchScrollEvents();
      DispatchVisualViewportScrollEvents();
      DispatchAnimationEvents();
      RunFullscreenSteps();
      RunFrameRequestCallbacks(aNowTime);

      if (mPresContext && mPresContext->GetPresShell()) {
        AutoTArray<nsIPresShell*, 16> observers;
        observers.AppendElements(mStyleFlushObservers);
        for (uint32_t j = observers.Length();
             j && mPresContext && mPresContext->GetPresShell(); --j) {
          // Make sure to not process observers which might have been removed
          // during previous iterations.
          nsIPresShell* shell = observers[j - 1];
          if (!mStyleFlushObservers.RemoveElement(shell)) continue;

          nsCOMPtr<nsIPresShell> shellKungFuDeathGrip(shell);
          shell->mObservingStyleFlushes = false;
          shell->FlushPendingNotifications(
              ChangesToFlush(FlushType::Style, false));
          // Inform the FontFaceSet that we ticked, so that it can resolve its
          // ready promise if it needs to (though it might still be waiting on
          // a layout flush).
          shell->NotifyFontFaceSetOnRefresh();
          mNeedToRecomputeVisibility = true;
        }
      }
    } else if (i == 2) {
      // This is the FlushType::Layout case.
      AutoTArray<nsIPresShell*, 16> observers;
      observers.AppendElements(mLayoutFlushObservers);
      for (uint32_t j = observers.Length();
           j && mPresContext && mPresContext->GetPresShell(); --j) {
        // Make sure to not process observers which might have been removed
        // during previous iterations.
        nsIPresShell* shell = observers[j - 1];
        if (!mLayoutFlushObservers.RemoveElement(shell)) continue;

        nsCOMPtr<nsIPresShell> shellKungFuDeathGrip(shell);
        shell->mObservingLayoutFlushes = false;
        shell->mWasLastReflowInterrupted = false;
        FlushType flushType = HasPendingAnimations(shell)
                                  ? FlushType::Layout
                                  : FlushType::InterruptibleLayout;
        shell->FlushPendingNotifications(ChangesToFlush(flushType, false));
        // Inform the FontFaceSet that we ticked, so that it can resolve its
        // ready promise if it needs to.
        shell->NotifyFontFaceSetOnRefresh();
        mNeedToRecomputeVisibility = true;
      }
    }

    // The pres context may be destroyed during we do the flushing.
    if (!mPresContext || !mPresContext->GetPresShell()) {
      StopTimer();
      return;
    }
  }

  // Recompute approximate frame visibility if it's necessary and enough time
  // has passed since the last time we did it.
  if (mNeedToRecomputeVisibility && !mThrottled &&
      aNowTime >= mNextRecomputeVisibilityTick &&
      !presShell->IsPaintingSuppressed()) {
    mNextRecomputeVisibilityTick = aNowTime + mMinRecomputeVisibilityInterval;
    mNeedToRecomputeVisibility = false;

    presShell->ScheduleApproximateFrameVisibilityUpdateNow();
  }

#ifdef MOZ_XUL
  // Update any popups that may need to be moved or hidden due to their
  // anchor changing.
  if (nsXULPopupManager* pm = nsXULPopupManager::GetInstance()) {
    pm->UpdatePopupPositions(this);
  }
#endif

  UpdateIntersectionObservations();

  /*
   * Perform notification to imgIRequests subscribed to listen
   * for refresh events.
   */

  for (auto iter = mStartTable.Iter(); !iter.Done(); iter.Next()) {
    const uint32_t& delay = iter.Key();
    ImageStartData* data = iter.UserData();

    if (data->mStartTime) {
      TimeStamp& start = *data->mStartTime;
      TimeDuration prev = previousRefresh - start;
      TimeDuration curr = aNowTime - start;
      uint32_t prevMultiple = uint32_t(prev.ToMilliseconds()) / delay;

      // We want to trigger images' refresh if we've just crossed over a
      // multiple of the first image's start time. If so, set the animation
      // start time to the nearest multiple of the delay and move all the
      // images in this table to the main requests table.
      if (prevMultiple != uint32_t(curr.ToMilliseconds()) / delay) {
        mozilla::TimeStamp desired =
            start + TimeDuration::FromMilliseconds(prevMultiple * delay);
        BeginRefreshingImages(data->mEntries, desired);
      }
    } else {
      // This is the very first time we've drawn images with this time delay.
      // Set the animation start time to "now" and move all the images in this
      // table to the main requests table.
      mozilla::TimeStamp desired = aNowTime;
      BeginRefreshingImages(data->mEntries, desired);
      data->mStartTime.emplace(aNowTime);
    }
  }

  if (mRequests.Count()) {
    // RequestRefresh may run scripts, so it's not safe to directly call it
    // while using a hashtable enumerator to enumerate mRequests in case
    // script modifies the hashtable. Instead, we build a (local) array of
    // images to refresh, and then we refresh each image in that array.
    nsCOMArray<imgIContainer> imagesToRefresh(mRequests.Count());

    for (auto iter = mRequests.Iter(); !iter.Done(); iter.Next()) {
      nsISupportsHashKey* entry = iter.Get();
      auto req = static_cast<imgIRequest*>(entry->GetKey());
      MOZ_ASSERT(req, "Unable to retrieve the image request");
      nsCOMPtr<imgIContainer> image;
      if (NS_SUCCEEDED(req->GetImage(getter_AddRefs(image)))) {
        imagesToRefresh.AppendElement(image.forget());
      }
    }

    for (uint32_t i = 0; i < imagesToRefresh.Length(); i++) {
      imagesToRefresh[i]->RequestRefresh(aNowTime);
    }
  }

  bool dispatchRunnablesAfterTick = false;
  if (mViewManagerFlushIsPending) {
    RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();

    nsTArray<nsDocShell*> profilingDocShells;
    GetProfileTimelineSubDocShells(GetDocShell(mPresContext),
                                   profilingDocShells);
    for (nsDocShell* docShell : profilingDocShells) {
      // For the sake of the profile timeline's simplicity, this is flagged as
      // paint even if it includes creating display lists
      MOZ_ASSERT(timelines);
      MOZ_ASSERT(timelines->HasConsumer(docShell));
      timelines->AddMarkerForDocShell(docShell, "Paint",
                                      MarkerTracingType::START);
    }

#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      printf_stderr("Starting ProcessPendingUpdates\n");
    }
#endif

    mViewManagerFlushIsPending = false;
    RefPtr<nsViewManager> vm = mPresContext->GetPresShell()->GetViewManager();
    {
      PaintTelemetry::AutoRecordPaint record;
      vm->ProcessPendingUpdates();
    }

#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      printf_stderr("Ending ProcessPendingUpdates\n");
    }
#endif

    for (nsDocShell* docShell : profilingDocShells) {
      MOZ_ASSERT(timelines);
      MOZ_ASSERT(timelines->HasConsumer(docShell));
      timelines->AddMarkerForDocShell(docShell, "Paint",
                                      MarkerTracingType::END);
    }

    dispatchRunnablesAfterTick = true;
    mHasScheduleFlush = false;
  }

#ifndef ANDROID /* bug 1142079 */
  mozilla::Telemetry::AccumulateTimeDelta(
      mozilla::Telemetry::REFRESH_DRIVER_TICK, mTickStart);
#endif

  if (mNotifyDOMContentFlushed) {
    mNotifyDOMContentFlushed = false;
    mPresContext->NotifyDOMContentFlushed();
  }

  nsTObserverArray<nsAPostRefreshObserver*>::ForwardIterator iter(
      mPostRefreshObservers);
  while (iter.HasMore()) {
    nsAPostRefreshObserver* observer = iter.GetNext();
    observer->DidRefresh();
  }

  NS_ASSERTION(mInRefresh, "Still in refresh");

  if (mPresContext->IsRoot() && XRE_IsContentProcess() &&
      gfxPrefs::AlwaysPaint()) {
    ScheduleViewManagerFlush();
  }

  if (dispatchRunnablesAfterTick && sPendingIdleRunnables) {
    AutoTArray<RunnableWithDelay, 8>* runnables = sPendingIdleRunnables;
    sPendingIdleRunnables = nullptr;
    for (RunnableWithDelay& runnableWithDelay : *runnables) {
      NS_DispatchToCurrentThreadQueue(runnableWithDelay.mRunnable.forget(),
                                      runnableWithDelay.mDelay,
                                      EventQueuePriority::Idle);
    }
    delete runnables;
  }
}

void nsRefreshDriver::BeginRefreshingImages(RequestTable& aEntries,
                                            mozilla::TimeStamp aDesired) {
  for (auto iter = aEntries.Iter(); !iter.Done(); iter.Next()) {
    auto req = static_cast<imgIRequest*>(iter.Get()->GetKey());
    MOZ_ASSERT(req, "Unable to retrieve the image request");

    mRequests.PutEntry(req);

    nsCOMPtr<imgIContainer> image;
    if (NS_SUCCEEDED(req->GetImage(getter_AddRefs(image)))) {
      image->SetAnimationStartTime(aDesired);
    }
  }
  aEntries.Clear();
}

void nsRefreshDriver::Freeze() {
  StopTimer();
  mFreezeCount++;
}

void nsRefreshDriver::Thaw() {
  NS_ASSERTION(mFreezeCount > 0, "Thaw() called on an unfrozen refresh driver");

  if (mFreezeCount > 0) {
    mFreezeCount--;
  }

  if (mFreezeCount == 0) {
    if (HasObservers() || HasImageRequests()) {
      // FIXME: This isn't quite right, since our EnsureTimerStarted call
      // updates our mMostRecentRefresh, but the DoRefresh call won't run
      // and notify our observers until we get back to the event loop.
      // Thus MostRecentRefresh() will lie between now and the DoRefresh.
      RefPtr<nsRunnableMethod<nsRefreshDriver>> event = NewRunnableMethod(
          "nsRefreshDriver::DoRefresh", this, &nsRefreshDriver::DoRefresh);
      nsPresContext* pc = GetPresContext();
      if (pc) {
        pc->Document()->Dispatch(TaskCategory::Other, event.forget());
        EnsureTimerStarted();
      } else {
        NS_ERROR("Thawing while document is being destroyed");
      }
    }
  }
}

void nsRefreshDriver::FinishedWaitingForTransaction() {
  mWaitingForTransaction = false;
  mSkippedPaints = false;
  mWarningThreshold = 1;
}

mozilla::layers::TransactionId nsRefreshDriver::GetTransactionId(
    bool aThrottle) {
  mOutstandingTransactionId = mOutstandingTransactionId.Next();
  mNextTransactionId = mNextTransactionId.Next();

  if (aThrottle && mOutstandingTransactionId - mCompletedTransaction >= 2 &&
      !mWaitingForTransaction && !mTestControllingRefreshes) {
    mWaitingForTransaction = true;
    mSkippedPaints = false;
    mWarningThreshold = 1;
  }

  return mNextTransactionId;
}

mozilla::layers::TransactionId nsRefreshDriver::LastTransactionId() const {
  return mNextTransactionId;
}

void nsRefreshDriver::RevokeTransactionId(
    mozilla::layers::TransactionId aTransactionId) {
  MOZ_ASSERT(aTransactionId == mNextTransactionId);
  if (mOutstandingTransactionId - mCompletedTransaction == 2 &&
      mWaitingForTransaction) {
    MOZ_ASSERT(!mSkippedPaints,
               "How did we skip a paint when we're in the middle of one?");
    FinishedWaitingForTransaction();
  }

  // Notify the pres context so that it can deliver MozAfterPaint for this
  // id if any caller was expecting it.
  nsPresContext* pc = GetPresContext();
  if (pc) {
    pc->NotifyRevokingDidPaint(aTransactionId);
  }
  // Revert the outstanding transaction since we're no longer waiting on it to
  // be completed, but don't revert mNextTransactionId since we can't use the id
  // again.
  mOutstandingTransactionId = mOutstandingTransactionId.Prev();
}

void nsRefreshDriver::ClearPendingTransactions() {
  mCompletedTransaction = mOutstandingTransactionId = mNextTransactionId;
  mWaitingForTransaction = false;
}

void nsRefreshDriver::ResetInitialTransactionId(
    mozilla::layers::TransactionId aTransactionId) {
  mCompletedTransaction = mOutstandingTransactionId = mNextTransactionId =
      aTransactionId;
}

mozilla::TimeStamp nsRefreshDriver::GetTransactionStart() { return mTickStart; }

VsyncId nsRefreshDriver::GetVsyncId() { return mTickVsyncId; }

mozilla::TimeStamp nsRefreshDriver::GetVsyncStart() { return mTickVsyncTime; }

void nsRefreshDriver::NotifyTransactionCompleted(
    mozilla::layers::TransactionId aTransactionId) {
  if (aTransactionId > mCompletedTransaction) {
    if (mOutstandingTransactionId - mCompletedTransaction > 1 &&
        mWaitingForTransaction) {
      mCompletedTransaction = aTransactionId;
      FinishedWaitingForTransaction();
    } else {
      mCompletedTransaction = aTransactionId;
    }
  }

  // If completed transaction id get ahead of outstanding id, reset to distance
  // id.
  if (mCompletedTransaction > mOutstandingTransactionId) {
    mOutstandingTransactionId = mCompletedTransaction;
  }
}

void nsRefreshDriver::WillRefresh(mozilla::TimeStamp aTime) {
  mRootRefresh->RemoveRefreshObserver(this, FlushType::Style);
  mRootRefresh = nullptr;
  if (mSkippedPaints) {
    DoRefresh();
  }
}

bool nsRefreshDriver::IsWaitingForPaint(mozilla::TimeStamp aTime) {
  if (mTestControllingRefreshes) {
    return false;
  }

  if (mWaitingForTransaction) {
    if (mSkippedPaints &&
        aTime > (mMostRecentRefresh +
                 TimeDuration::FromMilliseconds(mWarningThreshold * 1000))) {
      // XXX - Bug 1303369 - too many false positives.
      // gfxCriticalNote << "Refresh driver waiting for the compositor for "
      //                << (aTime - mMostRecentRefresh).ToSeconds()
      //                << " seconds.";
      mWarningThreshold *= 2;
    }

    mSkippedPaints = true;
    return true;
  }

  // Try find the 'root' refresh driver for the current window and check
  // if that is waiting for a paint.
  nsPresContext* pc = GetPresContext();
  nsPresContext* rootContext = pc ? pc->GetRootPresContext() : nullptr;
  if (rootContext) {
    nsRefreshDriver* rootRefresh = rootContext->RefreshDriver();
    if (rootRefresh && rootRefresh != this) {
      if (rootRefresh->IsWaitingForPaint(aTime)) {
        if (mRootRefresh != rootRefresh) {
          if (mRootRefresh) {
            mRootRefresh->RemoveRefreshObserver(this, FlushType::Style);
          }
          rootRefresh->AddRefreshObserver(this, FlushType::Style);
          mRootRefresh = rootRefresh;
        }
        mSkippedPaints = true;
        return true;
      }
    }
  }
  return false;
}

void nsRefreshDriver::SetThrottled(bool aThrottled) {
  if (aThrottled != mThrottled) {
    mThrottled = aThrottled;
    if (mActiveTimer) {
      // We want to switch our timer type here, so just stop and
      // restart the timer.
      EnsureTimerStarted(eForceAdjustTimer);
    }
  }
}

/*static*/
void nsRefreshDriver::PVsyncActorCreated(VsyncChild* aVsyncChild) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!XRE_IsParentProcess());
  RefPtr<RefreshDriverTimer> vsyncRefreshDriverTimer =
      new VsyncRefreshDriverTimer(aVsyncChild);

  // If we are using software timer, swap current timer to
  // VsyncRefreshDriverTimer.
  if (sRegularRateTimer) {
    sRegularRateTimer->SwapRefreshDrivers(vsyncRefreshDriverTimer);
  }
  sRegularRateTimer = vsyncRefreshDriverTimer.forget();
}

void nsRefreshDriver::DoRefresh() {
  // Don't do a refresh unless we're in a state where we should be refreshing.
  if (!IsFrozen() && mPresContext && mActiveTimer) {
    DoTick();
  }
}

#ifdef DEBUG
bool nsRefreshDriver::IsRefreshObserver(nsARefreshObserver* aObserver,
                                        FlushType aFlushType) {
  ObserverArray& array = ArrayFor(aFlushType);
  return array.Contains(aObserver);
}
#endif

void nsRefreshDriver::ScheduleViewManagerFlush() {
  NS_ASSERTION(mPresContext->IsRoot(),
               "Should only schedule view manager flush on root prescontexts");
  mViewManagerFlushIsPending = true;
  mHasScheduleFlush = true;
  EnsureTimerStarted(eNeverAdjustTimer);
}

void nsRefreshDriver::ScheduleFrameRequestCallbacks(Document* aDocument) {
  NS_ASSERTION(mFrameRequestCallbackDocs.IndexOf(aDocument) ==
                       mFrameRequestCallbackDocs.NoIndex &&
                   mThrottledFrameRequestCallbackDocs.IndexOf(aDocument) ==
                       mThrottledFrameRequestCallbackDocs.NoIndex,
               "Don't schedule the same document multiple times");
  if (aDocument->ShouldThrottleFrameRequests()) {
    mThrottledFrameRequestCallbackDocs.AppendElement(aDocument);
  } else {
    mFrameRequestCallbackDocs.AppendElement(aDocument);
  }

  // make sure that the timer is running
  EnsureTimerStarted();
}

void nsRefreshDriver::RevokeFrameRequestCallbacks(Document* aDocument) {
  mFrameRequestCallbackDocs.RemoveElement(aDocument);
  mThrottledFrameRequestCallbackDocs.RemoveElement(aDocument);
  // No need to worry about restarting our timer in slack mode if it's already
  // running; that will happen automatically when it fires.
}

void nsRefreshDriver::ScheduleFullscreenEvent(
    UniquePtr<PendingFullscreenEvent> aEvent) {
  mPendingFullscreenEvents.AppendElement(std::move(aEvent));
  // make sure that the timer is running
  EnsureTimerStarted();
}

void nsRefreshDriver::CancelPendingFullscreenEvents(Document* aDocument) {
  for (auto i : Reversed(IntegerRange(mPendingFullscreenEvents.Length()))) {
    if (mPendingFullscreenEvents[i]->Document() == aDocument) {
      mPendingFullscreenEvents.RemoveElementAt(i);
    }
  }
}

void nsRefreshDriver::CancelPendingAnimationEvents(
    AnimationEventDispatcher* aDispatcher) {
  MOZ_ASSERT(aDispatcher);
  aDispatcher->ClearEventQueue();
  mAnimationEventFlushObservers.RemoveElement(aDispatcher);
}

/* static */
TimeStamp nsRefreshDriver::GetIdleDeadlineHint(TimeStamp aDefault) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDefault.IsNull());

  if (!sRegularRateTimer) {
    return aDefault;
  }

  // For computing idleness of refresh drivers we only care about
  // sRegularRateTimer, since we consider refresh drivers attached to
  // sThrottledRateTimer to be inactive. This implies that tasks
  // resulting from a tick on the sRegularRateTimer counts as being
  // busy but tasks resulting from a tick on sThrottledRateTimer
  // counts as being idle.
  return sRegularRateTimer->GetIdleDeadlineHint(aDefault);
}

/* static */
Maybe<TimeStamp> nsRefreshDriver::GetNextTickHint() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sRegularRateTimer) {
    return Nothing();
  }
  return sRegularRateTimer->GetNextTickHint();
}

void nsRefreshDriver::Disconnect() {
  MOZ_ASSERT(NS_IsMainThread());

  StopTimer();

  if (mPresContext) {
    mPresContext = nullptr;
    if (--sRefreshDriverCount == 0) {
      Shutdown();
    }
  }
}

/* static */
bool nsRefreshDriver::IsJankCritical() {
  MOZ_ASSERT(NS_IsMainThread());
  return sActiveVsyncTimers > 0;
}

/* static */
bool nsRefreshDriver::GetJankLevels(Vector<uint64_t>& aJank) {
  aJank.clear();
  return aJank.append(sJankLevels, ArrayLength(sJankLevels));
}

#undef LOG
