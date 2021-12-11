/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorVsyncScheduler.h"

#include <stdio.h>        // for fprintf, stdout
#include <stdint.h>       // for uint64_t
#include "base/task.h"    // for CancelableTask, etc
#include "base/thread.h"  // for Thread
#include "gfxPlatform.h"  // for gfxPlatform
#ifdef MOZ_WIDGET_GTK
#  include "gfxPlatformGtk.h"  // for gfxPlatform
#endif
#include "mozilla/AutoRestore.h"  // for AutoRestore
#include "mozilla/DebugOnly.h"    // for DebugOnly
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/gfx/2D.h"     // for DrawTarget
#include "mozilla/gfx/Point.h"  // for IntSize
#include "mozilla/gfx/Rect.h"   // for IntSize
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorVsyncSchedulerOwner.h"
#include "mozilla/mozalloc.h"  // for operator new, etc
#include "nsCOMPtr.h"          // for already_AddRefed
#include "nsDebug.h"           // for NS_ASSERTION, etc
#include "nsISupportsImpl.h"   // for MOZ_COUNT_CTOR, etc
#include "nsIWidget.h"         // for nsIWidget
#include "nsThreadUtils.h"     // for NS_IsMainThread
#include "mozilla/Telemetry.h"
#include "mozilla/VsyncDispatcher.h"
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
#  include "VsyncSource.h"
#endif
#include "mozilla/widget/CompositorWidget.h"
#include "VRManager.h"

namespace mozilla {

namespace layers {

using namespace mozilla::gfx;

CompositorVsyncScheduler::Observer::Observer(CompositorVsyncScheduler* aOwner)
    : mMutex("CompositorVsyncScheduler.Observer.Mutex"), mOwner(aOwner) {}

CompositorVsyncScheduler::Observer::~Observer() { MOZ_ASSERT(!mOwner); }

bool CompositorVsyncScheduler::Observer::NotifyVsync(const VsyncEvent& aVsync) {
  MutexAutoLock lock(mMutex);
  if (!mOwner) {
    return false;
  }
  return mOwner->NotifyVsync(aVsync);
}

void CompositorVsyncScheduler::Observer::Destroy() {
  MutexAutoLock lock(mMutex);
  mOwner = nullptr;
}

CompositorVsyncScheduler::CompositorVsyncScheduler(
    CompositorVsyncSchedulerOwner* aVsyncSchedulerOwner,
    widget::CompositorWidget* aWidget)
    : mVsyncSchedulerOwner(aVsyncSchedulerOwner),
      mLastComposeTime(SampleTime::FromNow()),
      mLastVsyncTime(TimeStamp::Now()),
      mLastVsyncOutputTime(TimeStamp::Now()),
      mIsObservingVsync(false),
      mRendersDelayedByVsyncReasons(wr::RenderReasons::NONE),
      mVsyncNotificationsSkipped(0),
      mWidget(aWidget),
      mCurrentCompositeTaskMonitor("CurrentCompositeTaskMonitor"),
      mCurrentCompositeTask(nullptr),
      mCurrentCompositeTaskReasons(wr::RenderReasons::NONE),
      mCurrentVRTaskMonitor("CurrentVRTaskMonitor"),
      mCurrentVRTask(nullptr) {
  mVsyncObserver = new Observer(this);

  // mAsapScheduling is set on the main thread during init,
  // but is only accessed after on the compositor thread.
  mAsapScheduling =
      StaticPrefs::layers_offmainthreadcomposition_frame_rate() == 0 ||
      gfxPlatform::IsInLayoutAsapMode();
}

CompositorVsyncScheduler::~CompositorVsyncScheduler() {
  MOZ_ASSERT(!mIsObservingVsync);
  MOZ_ASSERT(!mVsyncObserver);
  // The CompositorVsyncDispatcher is cleaned up before this in the
  // nsBaseWidget, which stops vsync listeners
  mVsyncSchedulerOwner = nullptr;
}

void CompositorVsyncScheduler::Destroy() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  if (!mVsyncObserver) {
    // Destroy was already called on this object.
    return;
  }
  UnobserveVsync();
  mVsyncObserver->Destroy();
  mVsyncObserver = nullptr;

  mCompositeRequestedAt = TimeStamp();
  CancelCurrentCompositeTask();
  CancelCurrentVRTask();
}

void CompositorVsyncScheduler::PostCompositeTask(const VsyncEvent& aVsyncEvent,
                                                 wr::RenderReasons aReasons) {
  MonitorAutoLock lock(mCurrentCompositeTaskMonitor);
  mCurrentCompositeTaskReasons = mCurrentCompositeTaskReasons | aReasons;
  if (mCurrentCompositeTask == nullptr && CompositorThread()) {
    RefPtr<CancelableRunnable> task =
        NewCancelableRunnableMethod<VsyncEvent, wr::RenderReasons>(
            "layers::CompositorVsyncScheduler::Composite", this,
            &CompositorVsyncScheduler::Composite, aVsyncEvent, aReasons);
    mCurrentCompositeTask = task;
    CompositorThread()->Dispatch(task.forget());
  }
}

void CompositorVsyncScheduler::PostVRTask(TimeStamp aTimestamp) {
  MonitorAutoLock lockVR(mCurrentVRTaskMonitor);
  if (mCurrentVRTask == nullptr && CompositorThread()) {
    RefPtr<CancelableRunnable> task = NewCancelableRunnableMethod<TimeStamp>(
        "layers::CompositorVsyncScheduler::DispatchVREvents", this,
        &CompositorVsyncScheduler::DispatchVREvents, aTimestamp);
    mCurrentVRTask = task;
    CompositorThread()->Dispatch(task.forget());
  }
}

void CompositorVsyncScheduler::ScheduleComposition(wr::RenderReasons aReasons) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!mVsyncObserver) {
    // Destroy was already called on this object.
    return;
  }

  // Make a synthetic vsync event for the calls to PostCompositeTask below.
  TimeStamp vsyncTime = TimeStamp::Now();
  TimeStamp outputTime = vsyncTime + mVsyncSchedulerOwner->GetVsyncInterval();
  VsyncEvent vsyncEvent(VsyncId(), vsyncTime, outputTime);

  if (mAsapScheduling) {
    // Used only for performance testing purposes, and when recording/replaying
    // to ensure that graphics are up to date.
    PostCompositeTask(vsyncEvent, aReasons);
  } else {
    if (!mCompositeRequestedAt) {
      mCompositeRequestedAt = TimeStamp::Now();
    }
    if (!mIsObservingVsync && mCompositeRequestedAt) {
      ObserveVsync();
      // Starting to observe vsync is an async operation that goes
      // through the main thread of the UI process. It's possible that
      // we're blocking there waiting on a composite, so schedule an initial
      // one now to get things started.
      PostCompositeTask(vsyncEvent, aReasons);
    } else {
      mRendersDelayedByVsyncReasons = aReasons;
    }
  }
}

bool CompositorVsyncScheduler::NotifyVsync(const VsyncEvent& aVsync) {
  // Called from the vsync dispatch thread. When in the GPU Process, that's
  // the same as the compositor thread.
#ifdef DEBUG
#  ifdef MOZ_WAYLAND
  // On Wayland, we dispatch vsync from the main thread, without a GPU process.
  // To allow this, we skip the following asserts if we're currently utilizing
  // the Wayland backend. The IsParentProcess guard is needed to ensure that
  // we don't accidentally attempt to initialize the gfxPlatform in the GPU
  // process on X11.
  if (!XRE_IsParentProcess() ||
      !gfxPlatformGtk::GetPlatform()->IsWaylandDisplay())
#  endif  // MOZ_WAYLAND
  {
    MOZ_ASSERT_IF(XRE_IsParentProcess(),
                  !CompositorThreadHolder::IsInCompositorThread());
    MOZ_ASSERT(!NS_IsMainThread());
  }

  MOZ_ASSERT_IF(XRE_GetProcessType() == GeckoProcessType_GPU,
                CompositorThreadHolder::IsInCompositorThread());
#endif  // DEBUG

#if defined(MOZ_WIDGET_ANDROID)
  gfx::VRManager* vm = gfx::VRManager::Get();
  if (!vm->IsPresenting()) {
    PostCompositeTask(aVsync, wr::RenderReasons::VSYNC);
  }
#else
  PostCompositeTask(aVsync, wr::RenderReasons::VSYNC);
#endif  // defined(MOZ_WIDGET_ANDROID)

  PostVRTask(aVsync.mTime);
  return true;
}

void CompositorVsyncScheduler::CancelCurrentVRTask() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread() ||
             NS_IsMainThread());
  MonitorAutoLock lock(mCurrentVRTaskMonitor);
  if (mCurrentVRTask) {
    mCurrentVRTask->Cancel();
    mCurrentVRTask = nullptr;
  }
}

wr::RenderReasons CompositorVsyncScheduler::CancelCurrentCompositeTask() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread() ||
             NS_IsMainThread());
  MonitorAutoLock lock(mCurrentCompositeTaskMonitor);
  wr::RenderReasons canceledTaskRenderReasons = mCurrentCompositeTaskReasons;
  mCurrentCompositeTaskReasons = wr::RenderReasons::NONE;
  if (mCurrentCompositeTask) {
    mCurrentCompositeTask->Cancel();
    mCurrentCompositeTask = nullptr;
  }

  return canceledTaskRenderReasons;
}

void CompositorVsyncScheduler::Composite(const VsyncEvent& aVsyncEvent,
                                         wr::RenderReasons aReasons) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(mVsyncSchedulerOwner);

  {  // scope lock
    MonitorAutoLock lock(mCurrentCompositeTaskMonitor);
    aReasons =
        aReasons | mCurrentCompositeTaskReasons | mRendersDelayedByVsyncReasons;
    mCurrentCompositeTaskReasons = wr::RenderReasons::NONE;
    mRendersDelayedByVsyncReasons = wr::RenderReasons::NONE;
    mCurrentCompositeTask = nullptr;
  }

  mLastVsyncTime = aVsyncEvent.mTime;
  mLastVsyncOutputTime = aVsyncEvent.mOutputTime;
  mLastVsyncId = aVsyncEvent.mId;

  if (!mAsapScheduling) {
    // Some early exit conditions if we're not in ASAP mode
    if (aVsyncEvent.mTime < mLastComposeTime.Time()) {
      // We can sometimes get vsync timestamps that are in the past
      // compared to the last compose with force composites.
      // In those cases, wait until the next vsync;
      return;
    }

    if (mVsyncSchedulerOwner->IsPendingComposite()) {
      // If previous composite is still on going, finish it and wait for the
      // next vsync.
      mVsyncSchedulerOwner->FinishPendingComposite();
      return;
    }
  }

  if (mCompositeRequestedAt || mAsapScheduling) {
    mCompositeRequestedAt = TimeStamp();
    mLastComposeTime = SampleTime::FromVsync(aVsyncEvent.mTime);

    // Tell the owner to do a composite
    mVsyncSchedulerOwner->CompositeToTarget(aVsyncEvent.mId, aReasons, nullptr,
                                            nullptr);

    mVsyncNotificationsSkipped = 0;

    TimeDuration compositeFrameTotal = TimeStamp::Now() - aVsyncEvent.mTime;
    mozilla::Telemetry::Accumulate(
        mozilla::Telemetry::COMPOSITE_FRAME_ROUNDTRIP_TIME,
        compositeFrameTotal.ToMilliseconds());
  } else if (mVsyncNotificationsSkipped++ >
             StaticPrefs::gfx_vsync_compositor_unobserve_count_AtStartup()) {
    UnobserveVsync();
  }
}

void CompositorVsyncScheduler::ForceComposeToTarget(wr::RenderReasons aReasons,
                                                    gfx::DrawTarget* aTarget,
                                                    const IntRect* aRect) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  /**
   * bug 1138502 - There are cases such as during long-running window resizing
   * events where we receive many force-composites. We also continue to get
   * vsync notifications. Because the force-composites trigger compositing and
   * clear the mCompositeRequestedAt timestamp, the vsync notifications will not
   * need to do anything and so will increment the mVsyncNotificationsSkipped
   * counter to indicate the vsync was ignored. If this happens enough times, we
   * will disable listening for vsync entirely. On the next force-composite we
   * will enable listening for vsync again, and continued force-composites and
   * vsyncs will cause oscillation between observing vsync and not. On some
   * platforms, enabling/disabling vsync is not free and this oscillating
   * behavior causes a performance hit. In order to avoid this problem, we reset
   * the mVsyncNotificationsSkipped counter to keep vsync enabled.
   */
  mVsyncNotificationsSkipped = 0;

  mLastComposeTime = SampleTime::FromNow();
  MOZ_ASSERT(mVsyncSchedulerOwner);
  mVsyncSchedulerOwner->CompositeToTarget(VsyncId(), aReasons, aTarget, aRect);
}

bool CompositorVsyncScheduler::NeedsComposite() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return (bool)mCompositeRequestedAt;
}

bool CompositorVsyncScheduler::FlushPendingComposite() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (mCompositeRequestedAt) {
    wr::RenderReasons reasons = CancelCurrentCompositeTask();
    ForceComposeToTarget(reasons, nullptr, nullptr);
    return true;
  }
  return false;
}

void CompositorVsyncScheduler::ObserveVsync() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mWidget->ObserveVsync(mVsyncObserver);
  mIsObservingVsync = true;
}

void CompositorVsyncScheduler::UnobserveVsync() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mWidget->ObserveVsync(nullptr);
  mIsObservingVsync = false;
}

void CompositorVsyncScheduler::DispatchVREvents(TimeStamp aVsyncTimestamp) {
  {
    MonitorAutoLock lock(mCurrentVRTaskMonitor);
    mCurrentVRTask = nullptr;
  }
  // This only allows to be called by CompositorVsyncScheduler::PostVRTask()
  // When the process is going to shutdown, the runnable has chance to be
  // executed by other threads, we only want it to be run in the compositor
  // thread.
  if (!CompositorThreadHolder::IsInCompositorThread()) {
    return;
  }

  VRManager* vm = VRManager::Get();
  vm->NotifyVsync(aVsyncTimestamp);
}

const SampleTime& CompositorVsyncScheduler::GetLastComposeTime() const {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mLastComposeTime;
}

const TimeStamp& CompositorVsyncScheduler::GetLastVsyncTime() const {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mLastVsyncTime;
}

const TimeStamp& CompositorVsyncScheduler::GetLastVsyncOutputTime() const {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mLastVsyncOutputTime;
}

const VsyncId& CompositorVsyncScheduler::GetLastVsyncId() const {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mLastVsyncId;
}

void CompositorVsyncScheduler::UpdateLastComposeTime() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mLastComposeTime = SampleTime::FromNow();
}

}  // namespace layers
}  // namespace mozilla
