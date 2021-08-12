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
#include "mozilla/Assertions.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/DisplayPortUtils.h"
#include "mozilla/InputTaskManager.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/FontTableURIProtocolHandler.h"
#include "nsITimer.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsIXULRuntime.h"
#include "jsapi.h"
#include "nsContentUtils.h"
#include "mozilla/PendingAnimationTracker.h"
#include "mozilla/PendingFullscreenEvent.h"
#include "mozilla/dom/PerformanceMainThread.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_page_load.h"
#include "nsViewManager.h"
#include "GeckoProfiler.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/CallbackDebuggerNotification.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/VsyncChild.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/TaskController.h"
#include "Layers.h"
#include "imgIContainer.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsDocShell.h"
#include "nsISimpleEnumerator.h"
#include "nsJSEnvironment.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Telemetry.h"

#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "VsyncSource.h"
#include "mozilla/VsyncDispatcher.h"
#include "mozilla/Unused.h"
#include "mozilla/TimelineConsumers.h"
#include "nsAnimationManager.h"
#include "nsDisplayList.h"
#include "nsDOMNavigationTiming.h"
#include "nsTransitionManager.h"

#if defined(MOZ_WIDGET_ANDROID)
#  include "VRManagerChild.h"
#endif  // defined(MOZ_WIDGET_ANDROID)

#ifdef MOZ_XUL
#  include "nsXULPopupManager.h"
#endif

#include <numeric>

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

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(nsRefreshDriver::TickReasons);

namespace {
// The number outstanding nsRefreshDrivers (that have been created but not
// disconnected). When this reaches zero we will call
// nsRefreshDriver::Shutdown.
static uint32_t sRefreshDriverCount = 0;

// RAII-helper for recording elapsed duration for refresh tick phases.
class AutoRecordPhase {
 public:
  explicit AutoRecordPhase(double* aResultMs)
      : mTotalMs(aResultMs), mStartTime(TimeStamp::Now()) {
    MOZ_ASSERT(mTotalMs);
  }
  ~AutoRecordPhase() {
    *mTotalMs = (TimeStamp::Now() - mStartTime).ToMilliseconds();
  }

 private:
  double* mTotalMs;
  mozilla::TimeStamp mStartTime;
};

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
  RefreshDriverTimer() = default;

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
  VsyncId MostRecentRefreshVsyncId() const { return mLastFireId; }

  virtual TimeDuration GetTimerRate() = 0;

  TimeStamp GetIdleDeadlineHint(TimeStamp aDefault) {
    MOZ_ASSERT(NS_IsMainThread());

    TimeStamp mostRecentRefresh = MostRecentRefresh();
    TimeDuration refreshPeriod = GetTimerRate();
    TimeStamp idleEnd = mostRecentRefresh + refreshPeriod;

    // If we haven't painted for some time, then guess that we won't paint
    // again for a while, so the refresh driver is not a good way to predict
    // idle time.
    if (idleEnd +
            refreshPeriod *
                StaticPrefs::layout_idle_period_required_quiescent_frames() <
        TimeStamp::Now()) {
      return aDefault;
    }

    // End the predicted idle time a little early, the amount controlled by a
    // pref, to prevent overrunning the idle time and delaying a frame.
    idleEnd = idleEnd - TimeDuration::FromMilliseconds(
                            StaticPrefs::layout_idle_period_time_limit());
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

    for (nsRefreshDriver* driver : aDrivers.Clone()) {
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
    mLastFireId = aId;

    LOG("[%p] ticking drivers...", this);

    TickRefreshDrivers(aId, now, mContentRefreshDrivers);
    TickRefreshDrivers(aId, now, mRootRefreshDrivers);

    LOG("[%p] done.", this);
  }

  static void TickDriver(nsRefreshDriver* driver, VsyncId aId, TimeStamp now) {
    driver->Tick(aId, now);
  }

  TimeStamp mLastFireTime;
  VsyncId mLastFireId;
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
    mLastFireId = VsyncId();

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
  VsyncRefreshDriverTimer()
      : mVsyncDispatcher(nullptr),
        mVsyncChild(nullptr),
        mVsyncRate(TimeDuration::Forever()) {
    MOZ_ASSERT(XRE_IsParentProcess());
    MOZ_ASSERT(NS_IsMainThread());
    mVsyncSource = gfxPlatform::GetPlatform()->GetHardwareVsync();
    mVsyncObserver = new RefreshDriverVsyncObserver(this);
    MOZ_ALWAYS_TRUE(mVsyncDispatcher =
                        mVsyncSource->GetRefreshTimerVsyncDispatcher());
  }

  // Constructor for when we have a local vsync source. As it is local, we do
  // not have to worry about it being re-inited by gfxPlatform on frame rate
  // change on the global source.
  explicit VsyncRefreshDriverTimer(const RefPtr<gfx::VsyncSource>& aVsyncSource)
      : mVsyncSource(aVsyncSource),
        mVsyncDispatcher(nullptr),
        mVsyncChild(nullptr),
        mVsyncRate(TimeDuration::Forever()) {
    MOZ_ASSERT(XRE_IsParentProcess());
    MOZ_ASSERT(NS_IsMainThread());
    mVsyncObserver = new RefreshDriverVsyncObserver(this);
    MOZ_ALWAYS_TRUE(mVsyncDispatcher =
                        aVsyncSource->GetRefreshTimerVsyncDispatcher());
  }

  explicit VsyncRefreshDriverTimer(const RefPtr<VsyncChild>& aVsyncChild)
      : mVsyncSource(nullptr),
        mVsyncDispatcher(nullptr),
        mVsyncChild(aVsyncChild),
        mVsyncRate(TimeDuration::Forever()) {
    MOZ_ASSERT(XRE_IsContentProcess());
    MOZ_ASSERT(NS_IsMainThread());
    mVsyncObserver = new RefreshDriverVsyncObserver(this);
  }

  TimeDuration GetTimerRate() override {
    if (mVsyncSource) {
      mVsyncRate = mVsyncSource->GetGlobalDisplay().GetVsyncRate();
    } else if (mVsyncChild) {
      mVsyncRate = mVsyncChild->GetVsyncRate();
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
          mParentProcessRefreshTickLock("RefreshTickLock"),
          mPendingParentProcessVsync(false),
          mRecentVsync(TimeStamp::Now()),
          mLastTick(TimeStamp::Now()),
          mVsyncRate(TimeDuration::Forever()),
          mProcessedVsync(true) {
      MOZ_ASSERT(NS_IsMainThread());
    }

    class ParentProcessVsyncNotifier final : public Runnable,
                                             public nsIRunnablePriority {
     public:
      explicit ParentProcessVsyncNotifier(RefreshDriverVsyncObserver* aObserver)
          : Runnable(
                "VsyncRefreshDriverTimer::RefreshDriverVsyncObserver::"
                "ParentProcessVsyncNotifier"),
            mObserver(aObserver) {}

      NS_DECL_ISUPPORTS_INHERITED

      NS_IMETHOD Run() override {
        MOZ_ASSERT(NS_IsMainThread());
        sVsyncPriorityEnabled = mozilla::BrowserTabsRemoteAutostart();

        mObserver->NotifyParentProcessVsync();
        return NS_OK;
      }

      NS_IMETHOD GetPriority(uint32_t* aPriority) override {
        *aPriority = sVsyncPriorityEnabled
                         ? nsIRunnablePriority::PRIORITY_VSYNC
                         : nsIRunnablePriority::PRIORITY_NORMAL;
        return NS_OK;
      }

     private:
      ~ParentProcessVsyncNotifier() = default;
      RefPtr<RefreshDriverVsyncObserver> mObserver;
      static mozilla::Atomic<bool> sVsyncPriorityEnabled;
    };

    bool NotifyVsync(const VsyncEvent& aVsync) override {
      // Compress vsync notifications such that only 1 may run at a time
      // This is so that we don't flood the refresh driver with vsync messages
      // if the main thread is blocked for long periods of time
      {  // scope lock
        MonitorAutoLock lock(mParentProcessRefreshTickLock);
        mRecentParentProcessVsync = aVsync;
        if (mPendingParentProcessVsync) {
          return true;
        }
        mPendingParentProcessVsync = true;
      }

      if (XRE_IsContentProcess()) {
        NotifyParentProcessVsync();
        return true;
      }

      nsCOMPtr<nsIRunnable> vsyncEvent = new ParentProcessVsyncNotifier(this);
      NS_DispatchToMainThread(vsyncEvent);
      return true;
    }

    void NotifyParentProcessVsync() {
      // IMPORTANT: All paths through this method MUST hold a strong ref on
      // |this| for the duration of the TickRefreshDriver callback.
      MOZ_ASSERT(NS_IsMainThread());

      // This clears the input handling start time.
      InputTaskManager::Get()->SetInputHandlingStartTime(TimeStamp());

      VsyncEvent aVsync;
      {
        MonitorAutoLock lock(mParentProcessRefreshTickLock);
        aVsync = mRecentParentProcessVsync;
        mPendingParentProcessVsync = false;
      }

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

        return;
      }

      if (StaticPrefs::layout_lower_priority_refresh_driver_during_load() &&
          mVsyncRefreshDriverTimer) {
        nsPresContext* pctx =
            mVsyncRefreshDriverTimer->GetPresContextForOnlyRefreshDriver();
        if (pctx && pctx->HadContentfulPaint() && pctx->Document() &&
            pctx->Document()->GetReadyStateEnum() <
                Document::READYSTATE_COMPLETE) {
          nsPIDOMWindowInner* win = pctx->Document()->GetInnerWindow();
          uint32_t frameRateMultiplier = pctx->GetNextFrameRateMultiplier();
          if (!frameRateMultiplier) {
            pctx->DidUseFrameRateMultiplier();
          }
          if (win && frameRateMultiplier) {
            dom::Performance* perf = win->GetPerformance();
            // Limit slower refresh rate to 5 seconds between the
            // first contentful paint and page load.
            if (perf && perf->Now() <
                            StaticPrefs::page_load_deprioritization_period()) {
              if (mProcessedVsync) {
                mProcessedVsync = false;
                // Handle this case similarly to the code above, but just
                // use idle queue.
                TimeDuration rate = mVsyncRefreshDriverTimer->GetTimerRate();
                uint32_t slowRate = static_cast<uint32_t>(
                    rate.ToMilliseconds() * frameRateMultiplier);
                pctx->DidUseFrameRateMultiplier();
                nsCOMPtr<nsIRunnable> vsyncEvent = NewRunnableMethod<>(
                    "RefreshDriverVsyncObserver::NormalPriorityNotify[IDLE]",
                    this, &RefreshDriverVsyncObserver::NormalPriorityNotify);
                NS_DispatchToCurrentThreadQueue(vsyncEvent.forget(), slowRate,
                                                EventQueuePriority::Idle);
              }
              return;
            }
          }
        }
      }

      RefPtr<RefreshDriverVsyncObserver> kungFuDeathGrip(this);
      TickRefreshDriver(aVsync.mId, aVsync.mTime);
    }

    void Shutdown() {
      MOZ_ASSERT(NS_IsMainThread());
      mVsyncRefreshDriverTimer = nullptr;
    }

    void OnTimerStart() { mLastTick = TimeStamp::Now(); }

    void NormalPriorityNotify() {
      if (mLastProcessedTick.IsNull() || mRecentVsync > mLastProcessedTick) {
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
      } else if (mVsyncRate != TimeDuration::Forever()) {
        TimeDuration contentDelay = (TimeStamp::Now() - mLastTick) - mVsyncRate;
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
      } else {
        // Request the vsync rate from the parent process. Might be a few vsyncs
        // until the parent responds.
        if (mVsyncRefreshDriverTimer) {
          mVsyncRate = mVsyncRefreshDriverTimer->mVsyncChild->GetVsyncRate();
        }
      }
#endif
    }

    void TickRefreshDriver(VsyncId aId, TimeStamp aVsyncTimestamp) {
      MOZ_ASSERT(NS_IsMainThread());

      RecordTelemetryProbes(aVsyncTimestamp);
      mLastTick = TimeStamp::Now();
      mLastProcessedTick = aVsyncTimestamp;

      // On 32-bit Windows we sometimes get times where TimeStamp::Now() is not
      // monotonic because the underlying system apis produce non-monontonic
      // results. (bug 1306896)
#if !defined(_WIN32)
      MOZ_ASSERT(aVsyncTimestamp <= TimeStamp::Now());
#endif

      // Let also non-RefreshDriver code to run at least for awhile if we have
      // a mVsyncRefreshDriverTimer. Note, if nothing else is running,
      // RefreshDriver will still run as fast as possible, some ticks will
      // just be triggered from a normal priority runnable.
      TimeDuration timeForOutsideTick = TimeDuration::FromMilliseconds(0.0f);

      // We might have a problem that we call ~VsyncRefreshDriverTimer() before
      // the scheduled TickRefreshDriver() runs. Check mVsyncRefreshDriverTimer
      // before use.
      if (mVsyncRefreshDriverTimer) {
        timeForOutsideTick = TimeDuration::FromMilliseconds(
            mVsyncRefreshDriverTimer->GetTimerRate().ToMilliseconds() / 100.0f);
        RefPtr<VsyncRefreshDriverTimer> timer = mVsyncRefreshDriverTimer;
        timer->RunRefreshDrivers(aId, aVsyncTimestamp);
        // Note: mVsyncRefreshDriverTimer might be null now.
      }

      TimeDuration tickDuration = TimeStamp::Now() - mLastTick;
      mBlockUntil = aVsyncTimestamp + tickDuration + timeForOutsideTick;
    }

    // VsyncRefreshDriverTimer holds this RefreshDriverVsyncObserver and it will
    // be always available before Shutdown(). We can just use the raw pointer
    // here.
    VsyncRefreshDriverTimer* mVsyncRefreshDriverTimer;

    Monitor mParentProcessRefreshTickLock;
    VsyncEvent mRecentParentProcessVsync;
    bool mPendingParentProcessVsync;

    TimeStamp mRecentVsync;
    VsyncId mRecentVsyncId;
    TimeStamp mLastTick;
    TimeStamp mLastProcessedTick;
    TimeStamp mBlockUntil;
    TimeDuration mVsyncRate;
    bool mProcessedVsync;
  };  // RefreshDriverVsyncObserver

  ~VsyncRefreshDriverTimer() override {
    if (mVsyncDispatcher) {
      mVsyncDispatcher->RemoveChildRefreshTimer(mVsyncObserver);
      mVsyncDispatcher = nullptr;
    } else if (mVsyncChild) {
      mVsyncChild->RemoveChildRefreshTimer(mVsyncObserver);
      mVsyncChild = nullptr;
    }

    // Detach current vsync timer from this VsyncObserver. The observer will no
    // longer tick this timer.
    mVsyncObserver->Shutdown();
    mVsyncObserver = nullptr;
  }

  void StartTimer() override {
    MOZ_ASSERT(NS_IsMainThread());

    mLastFireTime = TimeStamp::Now();
    mLastFireId = VsyncId();

    if (mVsyncDispatcher) {
      mVsyncDispatcher->AddChildRefreshTimer(mVsyncObserver);
    } else if (mVsyncChild) {
      mVsyncChild->AddChildRefreshTimer(mVsyncObserver);
      mVsyncObserver->OnTimerStart();
    }
  }

  void StopTimer() override {
    MOZ_ASSERT(NS_IsMainThread());

    if (mVsyncDispatcher) {
      mVsyncDispatcher->RemoveChildRefreshTimer(mVsyncObserver);
    } else if (mVsyncChild) {
      mVsyncChild->RemoveChildRefreshTimer(mVsyncObserver);
    }
  }

  void ScheduleNextTick(TimeStamp aNowTime) override {
    // Do nothing since we just wait for the next vsync from
    // RefreshDriverVsyncObserver.
  }

  void RunRefreshDrivers(VsyncId aId, TimeStamp aTimeStamp) {
    Tick(aId, aTimeStamp);
    for (auto& driver : mContentRefreshDrivers) {
      driver->FinishedVsyncTick();
    }
    for (auto& driver : mRootRefreshDrivers) {
      driver->FinishedVsyncTick();
    }
  }

  // When using local vsync source, we keep a strong ref to it here to ensure
  // that the weak ref in the vsync dispatcher does not end up dangling.
  // As this is a local vsync source, it is not affected by gfxPlatform vsync
  // source reinit.
  RefPtr<gfx::VsyncSource> mVsyncSource;
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
    ParentProcessVsyncNotifier::sVsyncPriorityEnabled(false);

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
    mLastFireId = VsyncId();

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
    mLastFireId = VsyncId();

    nsTArray<RefPtr<nsRefreshDriver>> drivers(mContentRefreshDrivers.Clone());
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
static nsTArray<RefreshDriverTimer*>* sRegularRateTimerList;
static StaticRefPtr<InactiveRefreshDriverTimer> sThrottledRateTimer;

void nsRefreshDriver::CreateVsyncRefreshTimer() {
  MOZ_ASSERT(NS_IsMainThread());

  if (gfxPlatform::IsInLayoutAsapMode()) {
    return;
  }

  if (!mOwnTimer) {
    // If available, we fetch the widget-specific vsync source.
    nsPresContext* pc = GetPresContext();
    nsIWidget* widget = pc->GetRootWidget();
    if (widget) {
      if (RefPtr<gfx::VsyncSource> localVsyncSource =
              widget->GetVsyncSource()) {
        mOwnTimer = new VsyncRefreshDriverTimer(localVsyncSource);
        sRegularRateTimerList->AppendElement(mOwnTimer.get());
        return;
      }
      if (BrowserChild* browserChild = widget->GetOwningBrowserChild()) {
        if (RefPtr<VsyncChild> localVsyncSource =
                browserChild->GetVsyncChild()) {
          mOwnTimer = new VsyncRefreshDriverTimer(localVsyncSource);
          sRegularRateTimerList->AppendElement(mOwnTimer.get());
          return;
        }
      }
    }
  }
  if (!sRegularRateTimer) {
    if (XRE_IsParentProcess()) {
      // Make sure all vsync systems are ready.
      gfxPlatform::GetPlatform();
      // In parent process, we can create the VsyncRefreshDriverTimer directly.
      sRegularRateTimer = new VsyncRefreshDriverTimer();
    } else {
      PBackgroundChild* actorChild =
          BackgroundChild::GetOrCreateForCurrentThread();
      if (NS_WARN_IF(!actorChild)) {
        return;
      }

      dom::PVsyncChild* actor = actorChild->SendPVsyncConstructor();
      if (NS_WARN_IF(!actor)) {
        return;
      }

      dom::VsyncChild* child = static_cast<dom::VsyncChild*>(actor);

      RefPtr<RefreshDriverTimer> vsyncRefreshDriverTimer =
          new VsyncRefreshDriverTimer(child);

      sRegularRateTimer = std::move(vsyncRefreshDriverTimer);
    }
  }
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
  MOZ_ASSERT(NS_IsMainThread());
  // clean up our timers
  sRegularRateTimer = nullptr;
  delete sRegularRateTimerList;
  sRegularRateTimerList = nullptr;
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

RefreshDriverTimer* nsRefreshDriver::ChooseTimer() {
  if (mThrottled) {
    if (!sThrottledRateTimer)
      sThrottledRateTimer = new InactiveRefreshDriverTimer(
          GetThrottledTimerInterval(),
          DEFAULT_INACTIVE_TIMER_DISABLE_SECONDS * 1000.0);
    return sThrottledRateTimer;
  }

  if (!mOwnTimer) {
    CreateVsyncRefreshTimer();
  }

  if (mOwnTimer) {
    return mOwnTimer.get();
  }

  if (!sRegularRateTimer) {
    double rate = GetRegularTimerInterval();
    sRegularRateTimer = new StartupRefreshDriverTimer(rate);
  }

  return sRegularRateTimer;
}

static nsDocShell* GetDocShell(nsPresContext* aPresContext) {
  return static_cast<nsDocShell*>(aPresContext->GetDocShell());
}

nsRefreshDriver::nsRefreshDriver(nsPresContext* aPresContext)
    : mActiveTimer(nullptr),
      mOwnTimer(nullptr),
      mPresContext(aPresContext),
      mRootRefresh(nullptr),
      mNextTransactionId{0},
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
      mNeedToUpdateIntersectionObservations(false),
      mInNormalTick(false),
      mAttemptedExtraTickSinceLastVsync(false),
      mHasExceededAfterLoadTickPeriod(false),
      mWarningThreshold(REFRESH_WAIT_WARNING) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPresContext,
             "Need a pres context to tell us to call Disconnect() later "
             "and decrement sRefreshDriverCount.");
  mMostRecentRefresh = TimeStamp::Now();
  mNextThrottledFrameRequestTick = mMostRecentRefresh;
  mNextRecomputeVisibilityTick = mMostRecentRefresh;

  if (!sRegularRateTimerList) {
    sRegularRateTimerList = new nsTArray<RefreshDriverTimer*>();
  }
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
  if (mOwnTimer && sRegularRateTimerList) {
    sRegularRateTimerList->RemoveElement(mOwnTimer.get());
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
  mPendingTransactions.Clear();
}

TimeStamp nsRefreshDriver::MostRecentRefresh(bool aEnsureTimerStarted) const {
  // In case of stylo traversal, we have already activated the refresh driver in
  // RestyleManager::ProcessPendingRestyles().
  if (aEnsureTimerStarted && !ServoStyleSet::IsInServoTraversal()) {
    const_cast<nsRefreshDriver*>(this)->EnsureTimerStarted();
  }

  return mMostRecentRefresh;
}

void nsRefreshDriver::AddRefreshObserver(nsARefreshObserver* aObserver,
                                         FlushType aFlushType,
                                         const char* aObserverDescription) {
  ObserverArray& array = ArrayFor(aFlushType);
  array.AppendElement(ObserverData{
      aObserver, aObserverDescription, TimeStamp::Now(),
      mPresContext
          ? MarkerInnerWindowIdFromDocShell(mPresContext->GetDocShell())
          : MarkerInnerWindowId::NoId(),
      profiler_capture_backtrace(), aFlushType});
  EnsureTimerStarted();
}

bool nsRefreshDriver::RemoveRefreshObserver(nsARefreshObserver* aObserver,
                                            FlushType aFlushType) {
  ObserverArray& array = ArrayFor(aFlushType);
  auto index = array.IndexOf(aObserver);
  if (index == ObserverArray::array_type::NoIndex) {
    return false;
  }

  if (profiler_can_accept_markers()) {
    auto& data = array.ElementAt(index);
    nsPrintfCString str("%s [%s]", data.mDescription,
                        kFlushTypeNames[aFlushType]);
    PROFILER_MARKER_TEXT(
        "RefreshObserver", GRAPHICS,
        MarkerOptions(MarkerStack::TakeBacktrace(std::move(data.mCause)),
                      MarkerTiming::IntervalUntilNowFrom(data.mRegisterTime),
                      std::move(data.mInnerWindowId)),
        str);
  }

  array.RemoveElementAt(index);
  return true;
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
  VisualViewportResizeEventArray events =
      std::move(mVisualViewportResizeEvents);
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
  ScrollEventArray events = std::move(mScrollEvents);
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
  VisualViewportScrollEventArray events =
      std::move(mVisualViewportScrollEvents);
  for (auto& event : events) {
    event->Run();
  }
}

void nsRefreshDriver::AddPostRefreshObserver(
    nsAPostRefreshObserver* aObserver) {
  MOZ_DIAGNOSTIC_ASSERT(!mPostRefreshObservers.Contains(aObserver));
  mPostRefreshObservers.AppendElement(aObserver);
}

void nsRefreshDriver::RemovePostRefreshObserver(
    nsAPostRefreshObserver* aObserver) {
  bool removed = mPostRefreshObservers.RemoveElement(aObserver);
  MOZ_DIAGNOSTIC_ASSERT(removed);
  Unused << removed;
}

bool nsRefreshDriver::AddImageRequest(imgIRequest* aRequest) {
  uint32_t delay = GetFirstFrameDelay(aRequest);
  if (delay == 0) {
    mRequests.Insert(aRequest);
  } else {
    auto* const start = mStartTable.GetOrInsertNew(delay);
    start->mEntries.Insert(aRequest);
  }

  EnsureTimerStarted();

  if (profiler_can_accept_markers()) {
    nsCOMPtr<nsIURI> uri;
    aRequest->GetURI(getter_AddRefs(uri));
    nsAutoCString uristr;
    uri->GetAsciiSpec(uristr);

    PROFILER_MARKER_TEXT("Image Animation", GRAPHICS,
                         MarkerOptions(MarkerTiming::IntervalStart(),
                                       MarkerInnerWindowIdFromDocShell(
                                           GetDocShell(mPresContext))),
                         uristr);
  }

  return true;
}

void nsRefreshDriver::RemoveImageRequest(imgIRequest* aRequest) {
  // Try to remove from both places, just in case.
  bool removed = mRequests.EnsureRemoved(aRequest);
  uint32_t delay = GetFirstFrameDelay(aRequest);
  if (delay != 0) {
    ImageStartData* start = mStartTable.Get(delay);
    if (start) {
      removed = removed | start->mEntries.EnsureRemoved(aRequest);
    }
  }

  if (removed && profiler_can_accept_markers()) {
    nsCOMPtr<nsIURI> uri;
    aRequest->GetURI(getter_AddRefs(uri));
    nsAutoCString uristr;
    uri->GetAsciiSpec(uristr);

    PROFILER_MARKER_TEXT("Image Animation", GRAPHICS,
                         MarkerOptions(MarkerTiming::IntervalEnd(),
                                       MarkerInnerWindowIdFromDocShell(
                                           GetDocShell(mPresContext))),
                         uristr);
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

void nsRefreshDriver::RegisterCompositionPayload(
    const mozilla::layers::CompositionPayload& aPayload) {
  mCompositionPayloads.AppendElement(aPayload);
}

void nsRefreshDriver::AddForceNotifyContentfulPaintPresContext(
    nsPresContext* aPresContext) {
  mForceNotifyContentfulPaintPresContexts.AppendElement(aPresContext);
}

void nsRefreshDriver::FlushForceNotifyContentfulPaintPresContext() {
  while (!mForceNotifyContentfulPaintPresContexts.IsEmpty()) {
    WeakPtr<nsPresContext> presContext =
        mForceNotifyContentfulPaintPresContexts.PopLastElement();
    if (presContext) {
      presContext->NotifyContentfulPaint();
    }
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

bool nsRefreshDriver::CanDoCatchUpTick() {
  if (mTestControllingRefreshes || !mActiveTimer) {
    return false;
  }

  // If we've already ticked for the current timer refresh (or more recently
  // than that), then we don't need to do any catching up.
  if (mMostRecentRefresh >= mActiveTimer->MostRecentRefresh()) {
    return false;
  }

  if (mTickVsyncTime.IsNull()) {
    // Don't try to run a catch-up tick before there has been at least one
    // normal tick. The catch-up tick could affect negatively to the page load
    // performance.
    return false;
  }

  return true;
}

bool nsRefreshDriver::CanDoExtraTick() {
  // Only allow one extra tick per normal vsync tick.
  if (mAttemptedExtraTickSinceLastVsync) {
    return false;
  }

  // If we don't have a timer, or we didn't tick on the timer's
  // refresh then we can't do an 'extra' tick (but we may still
  // do a catch up tick).
  if (!mActiveTimer ||
      mActiveTimer->MostRecentRefresh() != mMostRecentRefresh) {
    return false;
  }

  // Grab the current timestamp before checking the tick hint to be sure
  // sure that it's equal or smaller than the value used within checking
  // the tick hint.
  TimeStamp now = TimeStamp::Now();
  Maybe<TimeStamp> nextTick = mActiveTimer->GetNextTickHint();
  int32_t minimumRequiredTime = StaticPrefs::layout_extra_tick_minimum_ms();
  // If there's less than 4 milliseconds until the next tick, it's probably
  // not worth trying to catch up.
  if (minimumRequiredTime < 0 || !nextTick ||
      (*nextTick - now) < TimeDuration::FromMilliseconds(minimumRequiredTime)) {
    return false;
  }

  return true;
}

void nsRefreshDriver::EnsureTimerStarted(EnsureTimerStartedFlags aFlags) {
  // FIXME: Bug 1346065: We should also assert the case where we have
  // STYLO_THREADS=1.
  MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal() || NS_IsMainThread(),
             "EnsureTimerStarted should be called only when we are not "
             "in servo traversal or on the main-thread");

  if (mTestControllingRefreshes) return;

  if (!mRefreshTimerStartedCause) {
    mRefreshTimerStartedCause = profiler_capture_backtrace();
  }

  // will it already fire, and no other changes needed?
  if (mActiveTimer && !(aFlags & eForceAdjustTimer)) {
    // If we're being called from within a user input handler, and we think
    // there's time to rush an extra tick immediately, then schedule a runnable
    // to run the extra tick.
    if (mUserInputProcessingCount && CanDoExtraTick()) {
      RefPtr<nsRefreshDriver> self = this;
      NS_DispatchToCurrentThreadQueue(
          NS_NewRunnableFunction(
              "RefreshDriver::EnsureTimerStarted::extra",
              [self]() -> void {
                // Re-check if we can still do an extra tick, in case anything
                // changed while the runnable was pending.
                if (self->CanDoExtraTick()) {
                  PROFILER_MARKER_UNTYPED("ExtraRefreshDriverTick", GRAPHICS);
                  LOG("[%p] Doing extra tick for user input", self.get());
                  self->mAttemptedExtraTickSinceLastVsync = true;
                  self->Tick(self->mActiveTimer->MostRecentRefreshVsyncId(),
                             self->mActiveTimer->MostRecentRefresh(),
                             IsExtraTick::Yes);
                }
              }),
          EventQueuePriority::Vsync);
    }
    return;
  }

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

    // If the timer has ticked since we last ticked, consider doing a 'catch-up'
    // tick immediately.
    if (CanDoCatchUpTick()) {
      RefPtr<nsRefreshDriver> self = this;
      NS_DispatchToCurrentThreadQueue(
          NS_NewRunnableFunction(
              "RefreshDriver::EnsureTimerStarted::catch-up",
              [self]() -> void {
                // Re-check if we can still do a catch-up, in case anything
                // changed while the runnable was pending.
                if (self->CanDoCatchUpTick()) {
                  LOG("[%p] Doing catch up tick", self.get());
                  self->Tick(self->mActiveTimer->MostRecentRefreshVsyncId(),
                             self->mActiveTimer->MostRecentRefresh());
                }
              }),
          EventQueuePriority::Vsync);
    }
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
  // most recent refresh of the old timer.
  // If we are restoring the refresh driver from test control, the time is
  // expected to go backwards (see bug 1043078), otherwise we just keep the most
  // recent tick of this driver (which may be older than the most recent tick of
  // the timer).
  if (!(aFlags & eAllowTimeToGoBackwards)) {
    return;
  }

  if (mMostRecentRefresh != mActiveTimer->MostRecentRefresh()) {
    mMostRecentRefresh = mActiveTimer->MostRecentRefresh();

    for (nsATimerAdjustmentObserver* obs :
         mTimerAdjustmentObservers.EndLimitedRange()) {
      obs->NotifyTimerAdjusted(mMostRecentRefresh);
    }
  }
}

void nsRefreshDriver::StopTimer() {
  if (!mActiveTimer) return;

  mActiveTimer->RemoveRefreshDriver(this);
  mActiveTimer = nullptr;
  mRefreshTimerStartedCause = nullptr;
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

void nsRefreshDriver::AppendObserverDescriptionsToString(
    nsACString& aStr) const {
  for (const ObserverArray& array : mObservers) {
    for (const auto& observer : array.EndLimitedRange()) {
      aStr.AppendPrintf("%s [%s], ", observer.mDescription,
                        kFlushTypeNames[observer.mFlushType]);
    }
  }
  if (mViewManagerFlushIsPending) {
    aStr.AppendLiteral("View manager flush pending, ");
  }
  if (!mAnimationEventFlushObservers.IsEmpty()) {
    aStr.AppendPrintf("%zux Animation event flush observer, ",
                      mAnimationEventFlushObservers.Length());
  }
  if (!mResizeEventFlushObservers.IsEmpty()) {
    aStr.AppendPrintf("%zux Resize event flush observer, ",
                      mResizeEventFlushObservers.Length());
  }
  if (!mStyleFlushObservers.IsEmpty()) {
    aStr.AppendPrintf("%zux Style flush observer, ",
                      mStyleFlushObservers.Length());
  }
  if (!mLayoutFlushObservers.IsEmpty()) {
    aStr.AppendPrintf("%zux Layout flush observer, ",
                      mLayoutFlushObservers.Length());
  }
  if (!mPendingFullscreenEvents.IsEmpty()) {
    aStr.AppendPrintf("%zux Pending fullscreen event, ",
                      mPendingFullscreenEvents.Length());
  }
  if (!mFrameRequestCallbackDocs.IsEmpty()) {
    aStr.AppendPrintf("%zux Frame request callback doc, ",
                      mFrameRequestCallbackDocs.Length());
  }
  if (!mThrottledFrameRequestCallbackDocs.IsEmpty()) {
    aStr.AppendPrintf("%zux Throttled frame request callback doc, ",
                      mThrottledFrameRequestCallbackDocs.Length());
  }
  if (!mEarlyRunners.IsEmpty()) {
    aStr.AppendPrintf("%zux Early runner, ", mEarlyRunners.Length());
  }
  // Remove last ", "
  aStr.Truncate(aStr.Length() - 2);
}

bool nsRefreshDriver::HasImageRequests() const {
  for (const auto& data : mStartTable.Values()) {
    if (!data->mEntries.IsEmpty()) {
      return true;
    }
  }

  return !mRequests.IsEmpty();
}

auto nsRefreshDriver::GetReasonsToTick() const -> TickReasons {
  TickReasons reasons = TickReasons::eNone;
  if (HasObservers()) {
    reasons |= TickReasons::eHasObservers;
  }
  if (HasImageRequests()) {
    reasons |= TickReasons::eHasImageRequests;
  }
  if (mNeedToUpdateIntersectionObservations) {
    reasons |= TickReasons::eNeedsToUpdateIntersectionObservations;
  }
  if (!mVisualViewportResizeEvents.IsEmpty()) {
    reasons |= TickReasons::eHasVisualViewportResizeEvents;
  }
  if (!mScrollEvents.IsEmpty()) {
    reasons |= TickReasons::eHasScrollEvents;
  }
  if (!mVisualViewportScrollEvents.IsEmpty()) {
    reasons |= TickReasons::eHasVisualViewportScrollEvents;
  }
  return reasons;
}

void nsRefreshDriver::AppendTickReasonsToString(TickReasons aReasons,
                                                nsACString& aStr) const {
  if (aReasons == TickReasons::eNone) {
    aStr.AppendLiteral(" <none>");
    return;
  }

  if (aReasons & TickReasons::eHasObservers) {
    aStr.AppendLiteral(" HasObservers (");
    AppendObserverDescriptionsToString(aStr);
    aStr.AppendLiteral(")");
  }
  if (aReasons & TickReasons::eHasImageRequests) {
    aStr.AppendLiteral(" HasImageAnimations");
  }
  if (aReasons & TickReasons::eNeedsToUpdateIntersectionObservations) {
    aStr.AppendLiteral(" NeedsToUpdateIntersectionObservations");
  }
  if (aReasons & TickReasons::eHasVisualViewportResizeEvents) {
    aStr.AppendLiteral(" HasVisualViewportResizeEvents");
  }
  if (aReasons & TickReasons::eHasScrollEvents) {
    aStr.AppendLiteral(" HasScrollEvents");
  }
  if (aReasons & TickReasons::eHasVisualViewportScrollEvents) {
    aStr.AppendLiteral(" HasVisualViewportScrollEvents");
  }
}

bool nsRefreshDriver::
    ShouldKeepTimerRunningWhileWaitingForFirstContentfulPaint() {
  // On top level content pages keep the timer running initially so that we
  // paint the page soon enough.
  if (mThrottled || mTestControllingRefreshes || !XRE_IsContentProcess() ||
      !mPresContext->Document()->IsTopLevelContentDocument() ||
      gfxPlatform::IsInLayoutAsapMode() || mPresContext->HadContentfulPaint() ||
      mPresContext->Document()->GetReadyStateEnum() ==
          Document::READYSTATE_COMPLETE) {
    return false;
  }
  if (mBeforeFirstContentfulPaintTimerRunningLimit.IsNull()) {
    // Don't let the timer to run forever, so limit to 4s for now.
    mBeforeFirstContentfulPaintTimerRunningLimit =
        TimeStamp::Now() + TimeDuration::FromSeconds(4.0f);
  }

  return TimeStamp::Now() <= mBeforeFirstContentfulPaintTimerRunningLimit;
}

bool nsRefreshDriver::ShouldKeepTimerRunningAfterPageLoad() {
  if (mHasExceededAfterLoadTickPeriod ||
      !StaticPrefs::layout_keep_ticking_after_load_ms() || mThrottled ||
      mTestControllingRefreshes || !XRE_IsContentProcess() ||
      !mPresContext->Document()->IsTopLevelContentDocument() ||
      gfxPlatform::IsInLayoutAsapMode()) {
    // Make the next check faster.
    mHasExceededAfterLoadTickPeriod = true;
    return false;
  }

  nsPIDOMWindowInner* innerWindow = mPresContext->Document()->GetInnerWindow();
  if (innerWindow) {
    if (PerformanceMainThread* perf = static_cast<PerformanceMainThread*>(
            innerWindow->GetPerformance())) {
      nsDOMNavigationTiming* timing = perf->GetDOMTiming();
      if (timing) {
        TimeStamp loadend = timing->LoadEventEnd();
        if (loadend) {
          // Keep ticking after the page load for some time.
          bool retval =
              (loadend +
               TimeDuration::FromMilliseconds(
                   StaticPrefs::layout_keep_ticking_after_load_ms())) >
              TimeStamp::Now();
          if (!retval) {
            mHasExceededAfterLoadTickPeriod = true;
          }
          return retval;
        }
      }
    }
  }

  return false;
}

nsRefreshDriver::ObserverArray& nsRefreshDriver::ArrayFor(
    FlushType aFlushType) {
  switch (aFlushType) {
    case FlushType::Event:
      return mObservers[0];
    case FlushType::Style:
    case FlushType::Frames:
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
  nsTArray<Document::FrameRequest> mCallbacks;
};

static bool HasPendingAnimations(PresShell* aPresShell) {
  Document* doc = aPresShell->GetDocument();
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

  RefPtr<BrowsingContext> bc = aRootDocShell->GetBrowsingContext();
  if (!bc) {
    return;
  }

  bc->PostOrderWalk([&](BrowsingContext* aContext) {
    if (!aContext->IsActive()) {
      return;
    }

    nsDocShell* shell = nsDocShell::Cast(aContext->GetDocShell());
    if (!shell || !shell->GetRecordProfileTimelineMarkers()) {
      // This process isn't painting OOP iframes so we ignore
      // docshells that are OOP.
      return;
    }

    aShells.AppendElement(shell);
  });
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

void nsRefreshDriver::UpdateIntersectionObservations(TimeStamp aNowTime) {
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
    doc->UpdateIntersectionObservations(aNowTime);
    doc->ScheduleIntersectionObserverNotification();
  }

  mNeedToUpdateIntersectionObservations = false;
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
    AUTO_PROFILER_TRACING_MARKER_DOCSHELL("Paint",
                                          "requestAnimationFrame callbacks",
                                          GRAPHICS, GetDocShell(mPresContext));
    for (const DocumentFrameCallbacks& docCallbacks : frameRequestCallbacks) {
      TimeStamp startTime = TimeStamp::Now();

      // XXXbz Bug 863140: GetInnerWindow can return the outer
      // window in some cases.
      nsPIDOMWindowInner* innerWindow =
          docCallbacks.mDocument->GetInnerWindow();
      DOMHighResTimeStamp timeStamp = 0;
      if (innerWindow) {
        if (Performance* perf = innerWindow->GetPerformance()) {
          timeStamp = perf->TimeStampToDOMHighResForRendering(aNowTime);
        }
        // else window is partially torn down already
      }
      for (auto& callback : docCallbacks.mCallbacks) {
        if (docCallbacks.mDocument->IsCanceledFrameRequestCallback(
                callback.mHandle)) {
          continue;
        }

        nsCOMPtr<nsIGlobalObject> global(innerWindow ? innerWindow->AsGlobal()
                                                     : nullptr);
        CallbackDebuggerNotificationGuard guard(
            global, DebuggerNotificationType::RequestAnimationFrameCallback);

        // MOZ_KnownLive is OK, because the stack array frameRequestCallbacks
        // keeps callback alive and the mCallback strong reference can't be
        // mutated by the call.
        LogFrameRequestCallback::Run run(callback.mCallback);
        MOZ_KnownLive(callback.mCallback)->Call(timeStamp);
      }

      if (docCallbacks.mDocument->GetReadyStateEnum() ==
          Document::READYSTATE_COMPLETE) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::PERF_REQUEST_ANIMATION_CALLBACK_NON_PAGELOAD_MS,
            startTime, TimeStamp::Now());
      } else {
        Telemetry::AccumulateTimeDelta(
            Telemetry::PERF_REQUEST_ANIMATION_CALLBACK_PAGELOAD_MS, startTime,
            TimeStamp::Now());
      }
    }
  }
}

static AutoTArray<RefPtr<Task>, 8>* sPendingIdleTasks = nullptr;

void nsRefreshDriver::DispatchIdleTaskAfterTickUnlessExists(Task* aTask) {
  if (!sPendingIdleTasks) {
    sPendingIdleTasks = new AutoTArray<RefPtr<Task>, 8>();
  } else {
    if (sPendingIdleTasks->Contains(aTask)) {
      return;
    }
  }

  sPendingIdleTasks->AppendElement(aTask);
}

void nsRefreshDriver::CancelIdleTask(Task* aTask) {
  if (!sPendingIdleTasks) {
    return;
  }

  sPendingIdleTasks->RemoveElement(aTask);

  if (sPendingIdleTasks->IsEmpty()) {
    delete sPendingIdleTasks;
    sPendingIdleTasks = nullptr;
  }
}

static CallState ReduceAnimations(Document& aDocument) {
  if (nsPresContext* pc = aDocument.GetPresContext()) {
    if (pc->EffectCompositor()->NeedsReducing()) {
      pc->EffectCompositor()->ReduceAnimations();
    }
  }
  aDocument.EnumerateSubDocuments(ReduceAnimations);
  return CallState::Continue;
}

void nsRefreshDriver::Tick(VsyncId aId, TimeStamp aNowTime,
                           IsExtraTick aIsExtraTick /* = No */) {
  MOZ_ASSERT(!nsContentUtils::GetCurrentJSContext(),
             "Shouldn't have a JSContext on the stack");

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
  // If this is an 'extra' tick, then we expect it to be using the same
  // vsync id and timestamp as the original tick, so also allow those.
  if ((aNowTime <= mMostRecentRefresh) && !mTestControllingRefreshes &&
      aIsExtraTick == IsExtraTick::No) {
    return;
  }
  auto cleanupInExtraTick = MakeScopeExit([&] { mInNormalTick = false; });
  mInNormalTick = aIsExtraTick != IsExtraTick::Yes;

  bool isPresentingInVR = false;
#if defined(MOZ_WIDGET_ANDROID)
  isPresentingInVR = gfx::VRManagerChild::IsPresenting();
#endif  // defined(MOZ_WIDGET_ANDROID)

  if (!isPresentingInVR && IsWaitingForPaint(aNowTime)) {
    // In immersive VR mode, we do not get notifications when frames are
    // presented, so we do not wait for the compositor in that mode.

    // We're currently suspended waiting for earlier Tick's to
    // be completed (on the Compositor). Mark that we missed the paint
    // and keep waiting.
    PROFILER_MARKER_UNTYPED(
        "RefreshDriverTick waiting for paint", GRAPHICS,
        MarkerInnerWindowIdFromDocShell(GetDocShell(mPresContext)));
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

  RefPtr<PresShell> presShell = mPresContext->GetPresShell();
  if (!presShell) {
    StopTimer();
    return;
  }

  TickReasons tickReasons = GetReasonsToTick();
  if (tickReasons == TickReasons::eNone) {
    // We no longer have any observers.
    // Discard composition payloads because there is no paint.
    mCompositionPayloads.Clear();

    // We don't want to stop the timer when observers are initially
    // removed, because sometimes observers can be added and removed
    // often depending on what other things are going on and in that
    // situation we don't want to thrash our timer.  So instead we
    // wait until we get a Notify() call when we have no observers
    // before stopping the timer.
    // On top level content pages keep the timer running initially so that we
    // paint the page soon enough.
    if (ShouldKeepTimerRunningWhileWaitingForFirstContentfulPaint()) {
      PROFILER_MARKER(
          "RefreshDriverTick waiting for first contentful paint", GRAPHICS,
          MarkerInnerWindowIdFromDocShell(GetDocShell(mPresContext)), Tracing,
          "Paint");
    } else if (ShouldKeepTimerRunningAfterPageLoad()) {
      PROFILER_MARKER(
          "RefreshDriverTick after page load", GRAPHICS,
          MarkerInnerWindowIdFromDocShell(GetDocShell(mPresContext)), Tracing,
          "Paint");
    } else {
      StopTimer();
    }
    return;
  }

  AUTO_PROFILER_LABEL("nsRefreshDriver::Tick", LAYOUT);

  nsAutoCString profilerStr;
  if (profiler_can_accept_markers()) {
    profilerStr.AppendLiteral("Tick reasons:");
    AppendTickReasonsToString(tickReasons, profilerStr);
  }
  AUTO_PROFILER_MARKER_TEXT(
      "RefreshDriverTick", GRAPHICS,
      MarkerOptions(
          MarkerStack::TakeBacktrace(std::move(mRefreshTimerStartedCause)),
          MarkerInnerWindowIdFromDocShell(GetDocShell(mPresContext))),
      profilerStr);

  mResizeSuppressed = false;

  bool oldInRefresh = mInRefresh;
  auto restoreInRefresh = MakeScopeExit([&] { mInRefresh = oldInRefresh; });
  mInRefresh = true;

  AutoRestore<TimeStamp> restoreTickStart(mTickStart);
  mTickStart = TimeStamp::Now();
  mTickVsyncId = aId;
  mTickVsyncTime = aNowTime;

  gfxPlatform::GetPlatform()->SchedulePaintIfDeviceReset();

  // We want to process any pending APZ metrics ahead of their positions
  // in the queue. This will prevent us from spending precious time
  // painting a stale displayport.
  if (StaticPrefs::apz_peek_messages_enabled()) {
    DisplayPortUtils::UpdateDisplayPortMarginsFromPendingMessages();
  }

  FlushForceNotifyContentfulPaintPresContext();

  AutoTArray<nsCOMPtr<nsIRunnable>, 16> earlyRunners = std::move(mEarlyRunners);
  for (auto& runner : earlyRunners) {
    runner->Run();
  }

  // Resize events should be fired before layout flushes or
  // calling animation frame callbacks.
  AutoTArray<RefPtr<PresShell>, 16> observers;
  observers.AppendElements(mResizeEventFlushObservers);
  for (RefPtr<PresShell>& presShell : Reversed(observers)) {
    if (!mPresContext || !mPresContext->GetPresShell()) {
      StopTimer();
      return;
    }
    // Make sure to not process observers which might have been removed
    // during previous iterations.
    if (!mResizeEventFlushObservers.RemoveElement(presShell)) {
      continue;
    }
    // MOZ_KnownLive because 'observers' is guaranteed to
    // keep it alive.
    //
    // Fixing https://bugzilla.mozilla.org/show_bug.cgi?id=1620312 on its own
    // won't help here, because 'observers' is non-const and we have the
    // Reversed() going on too...
    MOZ_KnownLive(presShell)->FireResizeEvent();
  }
  DispatchVisualViewportResizeEvents();

  double phaseMetrics[MOZ_ARRAY_LENGTH(mObservers)] = {
      0.0,
  };

  /*
   * The timer holds a reference to |this| while calling |Notify|.
   * However, implementations of |WillRefresh| are permitted to destroy
   * the pres context, which will cause our |mPresContext| to become
   * null.  If this happens, we must stop notifying observers.
   */
  for (uint32_t i = 0; i < ArrayLength(mObservers); ++i) {
    AutoRecordPhase phaseRecord(&phaseMetrics[i]);

    for (RefPtr<nsARefreshObserver> obs : mObservers[i].EndLimitedRange()) {
      obs->WillRefresh(aNowTime);

      if (!mPresContext || !mPresContext->GetPresShell()) {
        StopTimer();
        return;
      }
    }

    // Any animation timelines updated above may cause animations to queue
    // Promise resolution microtasks. We shouldn't run these, however, until we
    // have fully updated the animation state.
    //
    // As per the "update animations and send events" procedure[1], we should
    // remove replaced animations and then run these microtasks before
    // dispatching the corresponding animation events.
    //
    // [1]
    // https://drafts.csswg.org/web-animations-1/#update-animations-and-send-events
    if (i == 1) {
      nsAutoMicroTask mt;
      ReduceAnimations(*mPresContext->Document());
    }

    // Check if running the microtask checkpoint caused the pres context to
    // be destroyed.
    if (i == 1 && (!mPresContext || !mPresContext->GetPresShell())) {
      StopTimer();
      return;
    }

    if (i == 1) {
      // This is the FlushType::Style case.

      DispatchScrollEvents();
      DispatchVisualViewportScrollEvents();
      DispatchAnimationEvents();
      RunFullscreenSteps();
      RunFrameRequestCallbacks(aNowTime);

      if (mPresContext && mPresContext->GetPresShell()) {
        AutoTArray<PresShell*, 16> observers;
        observers.AppendElements(mStyleFlushObservers);
        for (uint32_t j = observers.Length();
             j && mPresContext && mPresContext->GetPresShell(); --j) {
          // Make sure to not process observers which might have been removed
          // during previous iterations.
          PresShell* rawPresShell = observers[j - 1];
          if (!mStyleFlushObservers.RemoveElement(rawPresShell)) {
            continue;
          }

          LogPresShellObserver::Run run(rawPresShell, this);

          RefPtr<PresShell> presShell = rawPresShell;
          presShell->mObservingStyleFlushes = false;
          presShell->FlushPendingNotifications(
              ChangesToFlush(FlushType::Style, false));
          // Inform the FontFaceSet that we ticked, so that it can resolve its
          // ready promise if it needs to (though it might still be waiting on
          // a layout flush).
          presShell->NotifyFontFaceSetOnRefresh();
          mNeedToRecomputeVisibility = true;

          // Record the telemetry for events that occurred between ticks.
          presShell->PingPerTickTelemetry(FlushType::Style);
        }
      }
    } else if (i == 2) {
      // This is the FlushType::Layout case.
      AutoTArray<PresShell*, 16> observers;
      observers.AppendElements(mLayoutFlushObservers);
      for (uint32_t j = observers.Length();
           j && mPresContext && mPresContext->GetPresShell(); --j) {
        // Make sure to not process observers which might have been removed
        // during previous iterations.
        PresShell* rawPresShell = observers[j - 1];
        if (!mLayoutFlushObservers.RemoveElement(rawPresShell)) {
          continue;
        }

        LogPresShellObserver::Run run(rawPresShell, this);

        RefPtr<PresShell> presShell = rawPresShell;
        presShell->mObservingLayoutFlushes = false;
        presShell->mWasLastReflowInterrupted = false;
        FlushType flushType = HasPendingAnimations(presShell)
                                  ? FlushType::Layout
                                  : FlushType::InterruptibleLayout;
        presShell->FlushPendingNotifications(ChangesToFlush(flushType, false));
        // Inform the FontFaceSet that we ticked, so that it can resolve its
        // ready promise if it needs to.
        presShell->NotifyFontFaceSetOnRefresh();
        mNeedToRecomputeVisibility = true;

        // Record the telemetry for events that occurred between ticks.
        presShell->PingPerTickTelemetry(FlushType::Layout);
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

  UpdateIntersectionObservations(aNowTime);

  /*
   * Perform notification to imgIRequests subscribed to listen
   * for refresh events.
   */

  for (const auto& entry : mStartTable) {
    const uint32_t& delay = entry.GetKey();
    ImageStartData* data = entry.GetWeak();

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

    for (nsISupports* entry : mRequests) {
      auto* req = static_cast<imgIRequest*>(entry);
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

  double phasePaint = 0.0;
  bool dispatchTasksAfterTick = false;
  if (mViewManagerFlushIsPending) {
    AutoRecordPhase paintRecord(&phasePaint);
    nsCString transactionId;
    if (profiler_can_accept_markers()) {
      transactionId.AppendLiteral("Transaction ID: ");
      transactionId.AppendInt((uint64_t)mNextTransactionId);
    }
    AUTO_PROFILER_MARKER_TEXT(
        "ViewManagerFlush", GRAPHICS,
        MarkerOptions(
            MarkerInnerWindowIdFromDocShell(GetDocShell(mPresContext)),
            MarkerStack::TakeBacktrace(std::move(mViewManagerFlushCause))),
        transactionId);

    // Forward our composition payloads to the layer manager.
    if (!mCompositionPayloads.IsEmpty()) {
      nsIWidget* widget = mPresContext->GetRootWidget();
      WindowRenderer* renderer = widget ? widget->GetWindowRenderer() : nullptr;
      if (renderer && renderer->AsLayerManager()) {
        renderer->AsLayerManager()->RegisterPayloads(mCompositionPayloads);
      }
      mCompositionPayloads.Clear();
    }

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
    const bool skipPaint = isPresentingInVR;
    // Skip the paint in immersive VR mode because whatever we paint here will
    // not end up on the screen. The screen is displaying WebGL content from a
    // single canvas in that mode.
    if (!skipPaint) {
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

    dispatchTasksAfterTick = true;
    mHasScheduleFlush = false;
  } else {
    // No paint happened, discard composition payloads.
    mCompositionPayloads.Clear();
  }

  double totalMs = (TimeStamp::Now() - mTickStart).ToMilliseconds();

#ifndef ANDROID /* bug 1142079 */
  mozilla::Telemetry::Accumulate(mozilla::Telemetry::REFRESH_DRIVER_TICK,
                                 static_cast<uint32_t>(totalMs));
#endif

  // Bug 1568107: If the totalMs is greater than 1/60th second (ie. 1000/60 ms)
  // then record, via telemetry, the percentage of time spent in each
  // sub-system.
  if (totalMs > 1000.0 / 60.0) {
    auto record = [=](const nsCString& aKey, double aDurationMs) -> void {
      MOZ_ASSERT(aDurationMs <= totalMs);
      auto phasePercent = static_cast<uint32_t>(aDurationMs * 100.0 / totalMs);
      Telemetry::Accumulate(Telemetry::REFRESH_DRIVER_TICK_PHASE_WEIGHT, aKey,
                            phasePercent);
    };

    record("Event"_ns, phaseMetrics[0]);
    record("Style"_ns, phaseMetrics[1]);
    record("Reflow"_ns, phaseMetrics[2]);
    record("Display"_ns, phaseMetrics[3]);
    record("Paint"_ns, phasePaint);

    // Explicitly record the time unaccounted for.
    double other = totalMs -
                   std::accumulate(phaseMetrics, ArrayEnd(phaseMetrics), 0.0) -
                   phasePaint;
    record("Other"_ns, other);
  }

  if (mNotifyDOMContentFlushed) {
    mNotifyDOMContentFlushed = false;
    mPresContext->NotifyDOMContentFlushed();
  }

  for (nsAPostRefreshObserver* observer :
       mPostRefreshObservers.ForwardRange()) {
    observer->DidRefresh();
  }

  NS_ASSERTION(mInRefresh, "Still in refresh");

  if (mPresContext->IsRoot() && XRE_IsContentProcess() &&
      StaticPrefs::gfx_content_always_paint()) {
    ScheduleViewManagerFlush();
  }

  if (dispatchTasksAfterTick && sPendingIdleTasks) {
    AutoTArray<RefPtr<Task>, 8>* tasks = sPendingIdleTasks;
    sPendingIdleTasks = nullptr;
    for (RefPtr<Task>& taskWithDelay : *tasks) {
      TaskController::Get()->AddTask(taskWithDelay.forget());
    }
    delete tasks;
  }
}

void nsRefreshDriver::BeginRefreshingImages(RequestTable& aEntries,
                                            mozilla::TimeStamp aDesired) {
  for (const auto& key : aEntries) {
    auto* req = static_cast<imgIRequest*>(key);
    MOZ_ASSERT(req, "Unable to retrieve the image request");

    mRequests.Insert(req);

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
  mNextTransactionId = mNextTransactionId.Next();
  LOG("[%p] Allocating transaction id %" PRIu64, this, mNextTransactionId.mId);

  // If this a paint from within a normal tick, and the caller hasn't explicitly
  // asked for it to skip being throttled, then record this transaction as
  // pending and maybe disable painting until some transactions are processed.
  if (aThrottle && mInNormalTick) {
    mPendingTransactions.AppendElement(mNextTransactionId);
    if (TooManyPendingTransactions() && !mWaitingForTransaction &&
        !mTestControllingRefreshes) {
      LOG("[%p] Hit max pending transaction limit, entering wait mode", this);
      mWaitingForTransaction = true;
      mSkippedPaints = false;
      mWarningThreshold = 1;
    }
  }

  return mNextTransactionId;
}

mozilla::layers::TransactionId nsRefreshDriver::LastTransactionId() const {
  return mNextTransactionId;
}

void nsRefreshDriver::RevokeTransactionId(
    mozilla::layers::TransactionId aTransactionId) {
  MOZ_ASSERT(aTransactionId == mNextTransactionId);
  LOG("[%p] Revoking transaction id %" PRIu64, this, aTransactionId.mId);
  if (AtPendingTransactionLimit() &&
      mPendingTransactions.Contains(aTransactionId) && mWaitingForTransaction) {
    LOG("[%p] No longer over pending transaction limit, leaving wait state",
        this);
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
  // Remove aTransactionId from the set of outstanding transactions since we're
  // no longer waiting on it to be completed, but don't revert
  // mNextTransactionId since we can't use the id again.
  mPendingTransactions.RemoveElement(aTransactionId);
}

void nsRefreshDriver::ClearPendingTransactions() {
  LOG("[%p] ClearPendingTransactions", this);
  mPendingTransactions.Clear();
  mWaitingForTransaction = false;
}

void nsRefreshDriver::ResetInitialTransactionId(
    mozilla::layers::TransactionId aTransactionId) {
  mNextTransactionId = aTransactionId;
}

mozilla::TimeStamp nsRefreshDriver::GetTransactionStart() { return mTickStart; }

VsyncId nsRefreshDriver::GetVsyncId() { return mTickVsyncId; }

mozilla::TimeStamp nsRefreshDriver::GetVsyncStart() { return mTickVsyncTime; }

void nsRefreshDriver::NotifyTransactionCompleted(
    mozilla::layers::TransactionId aTransactionId) {
  LOG("[%p] Completed transaction id %" PRIu64, this, aTransactionId.mId);
  mPendingTransactions.RemoveElement(aTransactionId);
  if (mWaitingForTransaction && !TooManyPendingTransactions()) {
    LOG("[%p] No longer over pending transaction limit, leaving wait state",
        this);
    FinishedWaitingForTransaction();
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

    LOG("[%p] Over max pending transaction limit when trying to paint, "
        "skipping",
        this);
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
          rootRefresh->AddRefreshObserver(this, FlushType::Style,
                                          "Waiting for paint");
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

nsPresContext* nsRefreshDriver::GetPresContext() const { return mPresContext; }

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
  if (!mViewManagerFlushCause) {
    mViewManagerFlushCause = profiler_capture_backtrace();
  }
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

  // For computing idleness of refresh drivers we only care about
  // sRegularRateTimerList, since we consider refresh drivers attached to
  // sThrottledRateTimer to be inactive. This implies that tasks
  // resulting from a tick on the sRegularRateTimer counts as being
  // busy but tasks resulting from a tick on sThrottledRateTimer
  // counts as being idle.
  if (sRegularRateTimer) {
    return sRegularRateTimer->GetIdleDeadlineHint(aDefault);
  }

  // The following calculation is only used on platform using per-BrowserChild
  // Vsync. This is hard to properly map on static calls such as this -
  // optimally we'd only want to query the timers that are relevant for the
  // caller, not all in this process. Further more, in this scenario we often
  // hit cases where timers would return their fallback value that is aDefault,
  // giving us a much higher value than intended.
  // For now we use a somewhat simplistic approach that in many situations
  // gives us similar behaviour to what we would get using sRegularRateTimer:
  // use the highest result that is still lower than the aDefault fallback.
  TimeStamp hint = TimeStamp();
  if (sRegularRateTimerList) {
    for (RefreshDriverTimer* timer : *sRegularRateTimerList) {
      TimeStamp newHint = timer->GetIdleDeadlineHint(aDefault);
      if (newHint < aDefault && (hint.IsNull() || newHint > hint)) {
        hint = newHint;
      }
    }
  }

  return hint.IsNull() ? aDefault : hint;
}

/* static */
Maybe<TimeStamp> nsRefreshDriver::GetNextTickHint() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sRegularRateTimer) {
    return sRegularRateTimer->GetNextTickHint();
  }

  Maybe<TimeStamp> hint = Nothing();
  if (sRegularRateTimerList) {
    for (RefreshDriverTimer* timer : *sRegularRateTimerList) {
      if (Maybe<TimeStamp> newHint = timer->GetNextTickHint()) {
        if (!hint || newHint.value() < hint.value()) {
          hint = newHint;
        }
      }
    }
  }
  return hint;
}

void nsRefreshDriver::Disconnect() {
  MOZ_ASSERT(NS_IsMainThread());

  StopTimer();

  mEarlyRunners.Clear();

  if (mPresContext) {
    mPresContext = nullptr;
    if (--sRefreshDriverCount == 0) {
      Shutdown();
    }
  }
}

#undef LOG
