/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorVsyncScheduler.h"

#include <stdio.h>                      // for fprintf, stdout
#include <stdint.h>                     // for uint64_t
#include "base/task.h"                  // for CancelableTask, etc
#include "base/thread.h"                // for Thread
#include "gfxPlatform.h"                // for gfxPlatform
#ifdef MOZ_WIDGET_GTK
#include "gfxPlatformGtk.h"             // for gfxPlatform
#endif
#include "gfxPrefs.h"                   // for gfxPrefs
#include "mozilla/AutoRestore.h"        // for AutoRestore
#include "mozilla/DebugOnly.h"          // for DebugOnly
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Rect.h"           // for IntSize
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorVsyncSchedulerOwner.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsIWidget.h"                  // for nsIWidget
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "mozilla/Telemetry.h"
#include "mozilla/VsyncDispatcher.h"
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
#include "VsyncSource.h"
#endif
#include "mozilla/widget/CompositorWidget.h"
#include "VRManager.h"
#include "VRThread.h"

namespace mozilla {

namespace layers {

using namespace mozilla::gfx;
using namespace std;

CompositorVsyncScheduler::Observer::Observer(CompositorVsyncScheduler* aOwner)
  : mMutex("CompositorVsyncScheduler.Observer.Mutex")
  , mOwner(aOwner)
{
}

CompositorVsyncScheduler::Observer::~Observer()
{
  MOZ_ASSERT(!mOwner);
}

bool
CompositorVsyncScheduler::Observer::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  MutexAutoLock lock(mMutex);
  if (!mOwner) {
    return false;
  }
  return mOwner->NotifyVsync(aVsyncTimestamp);
}

void
CompositorVsyncScheduler::Observer::Destroy()
{
  MutexAutoLock lock(mMutex);
  mOwner = nullptr;
}

CompositorVsyncScheduler::CompositorVsyncScheduler(CompositorVsyncSchedulerOwner* aVsyncSchedulerOwner,
                                                   widget::CompositorWidget* aWidget)
  : mVsyncSchedulerOwner(aVsyncSchedulerOwner)
  , mLastCompose(TimeStamp::Now())
  , mIsObservingVsync(false)
  , mNeedsComposite(0)
  , mVsyncNotificationsSkipped(0)
  , mWidget(aWidget)
  , mCurrentCompositeTaskMonitor("CurrentCompositeTaskMonitor")
  , mCurrentCompositeTask(nullptr)
  , mCurrentVRListenerTaskMonitor("CurrentVRTaskMonitor")
  , mCurrentVRListenerTask(nullptr)
{
  mVsyncObserver = new Observer(this);

  // mAsapScheduling is set on the main thread during init,
  // but is only accessed after on the compositor thread.
  mAsapScheduling = gfxPrefs::LayersCompositionFrameRate() == 0 ||
                    gfxPlatform::IsInLayoutAsapMode();
}

CompositorVsyncScheduler::~CompositorVsyncScheduler()
{
  MOZ_ASSERT(!mIsObservingVsync);
  MOZ_ASSERT(!mVsyncObserver);
  // The CompositorVsyncDispatcher is cleaned up before this in the nsBaseWidget, which stops vsync listeners
  mVsyncSchedulerOwner = nullptr;
}

void
CompositorVsyncScheduler::Destroy()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  if (!mVsyncObserver) {
    // Destroy was already called on this object.
    return;
  }
  UnobserveVsync();
  mVsyncObserver->Destroy();
  mVsyncObserver = nullptr;

  mNeedsComposite = 0;
  CancelCurrentCompositeTask();
}

void
CompositorVsyncScheduler::PostCompositeTask(TimeStamp aCompositeTimestamp)
{
  MonitorAutoLock lock(mCurrentCompositeTaskMonitor);
  if (mCurrentCompositeTask == nullptr && CompositorThreadHolder::Loop()) {
    RefPtr<CancelableRunnable> task = NewCancelableRunnableMethod<TimeStamp>(
      "layers::CompositorVsyncScheduler::Composite",
      this,
      &CompositorVsyncScheduler::Composite,
      aCompositeTimestamp);
    mCurrentCompositeTask = task;
    ScheduleTask(task.forget());
  }
}

void
CompositorVsyncScheduler::PostVRTask(TimeStamp aTimestamp)
{
  MonitorAutoLock lockVR(mCurrentVRListenerTaskMonitor);
  if (mCurrentVRListenerTask == nullptr && VRListenerThreadHolder::Loop()) {
    RefPtr<Runnable> task = NewRunnableMethod<TimeStamp>(
      "layers::CompositorVsyncScheduler::DispatchVREvents",
      this,
      &CompositorVsyncScheduler::DispatchVREvents,
      aTimestamp);
    mCurrentVRListenerTask = task;
    VRListenerThreadHolder::Loop()->PostDelayedTask(task.forget(), 0);
  }
}

void
CompositorVsyncScheduler::ScheduleComposition()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!mVsyncObserver) {
    // Destroy was already called on this object.
    return;
  }

  if (mAsapScheduling) {
    // Used only for performance testing purposes
    PostCompositeTask(TimeStamp::Now());
#ifdef MOZ_WIDGET_ANDROID
  } else if (mNeedsComposite >= 2 && mIsObservingVsync) {
    // uh-oh, we already requested a composite at least twice so far, and a
    // composite hasn't happened yet. It is possible that the vsync observation
    // is blocked on the main thread, so let's just composite ASAP and not
    // wait for the vsync. Note that this should only ever happen on Fennec
    // because there content runs in the same process as the compositor, and so
    // content can actually block the main thread in this process.
    PostCompositeTask(TimeStamp::Now());
#endif
  } else {
    mNeedsComposite++;
    if (!mIsObservingVsync && mNeedsComposite) {
      ObserveVsync();
      // Starting to observe vsync is an async operation that goes
      // through the main thread of the UI process. It's possible that
      // we're blocking there waiting on a composite, so schedule an initial
      // one now to get things started.
      PostCompositeTask(TimeStamp::Now());
    }
  }
}

bool
CompositorVsyncScheduler::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  // Called from the vsync dispatch thread. When in the GPU Process, that's
  // the same as the compositor thread.
  MOZ_ASSERT_IF(XRE_IsParentProcess(), !CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT_IF(XRE_GetProcessType() == GeckoProcessType_GPU, CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(!NS_IsMainThread());
  PostCompositeTask(aVsyncTimestamp);
  PostVRTask(aVsyncTimestamp);
  return true;
}

void
CompositorVsyncScheduler::CancelCurrentCompositeTask()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread() || NS_IsMainThread());
  MonitorAutoLock lock(mCurrentCompositeTaskMonitor);
  if (mCurrentCompositeTask) {
    mCurrentCompositeTask->Cancel();
    mCurrentCompositeTask = nullptr;
  }
}

void
CompositorVsyncScheduler::Composite(TimeStamp aVsyncTimestamp)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(mVsyncSchedulerOwner);

  { // scope lock
    MonitorAutoLock lock(mCurrentCompositeTaskMonitor);
    mCurrentCompositeTask = nullptr;
  }

  if (!mAsapScheduling) {
    // Some early exit conditions if we're not in ASAP mode
    if (aVsyncTimestamp < mLastCompose) {
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

  if (mNeedsComposite || mAsapScheduling) {
    mNeedsComposite = 0;
    mLastCompose = aVsyncTimestamp;

    // Tell the owner to do a composite
    mVsyncSchedulerOwner->CompositeToTarget(nullptr, nullptr);

    mVsyncNotificationsSkipped = 0;

    TimeDuration compositeFrameTotal = TimeStamp::Now() - aVsyncTimestamp;
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::COMPOSITE_FRAME_ROUNDTRIP_TIME,
                                   compositeFrameTotal.ToMilliseconds());
  } else if (mVsyncNotificationsSkipped++ > gfxPrefs::CompositorUnobserveCount()) {
    UnobserveVsync();
  }
}

void
CompositorVsyncScheduler::ForceComposeToTarget(gfx::DrawTarget* aTarget, const IntRect* aRect)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  /**
   * bug 1138502 - There are cases such as during long-running window resizing
   * events where we receive many force-composites. We also continue to get
   * vsync notifications. Because the force-composites trigger compositing and
   * clear the mNeedsComposite counter, the vsync notifications will not need
   * to do anything and so will increment the mVsyncNotificationsSkipped counter
   * to indicate the vsync was ignored. If this happens enough times, we will
   * disable listening for vsync entirely. On the next force-composite we will
   * enable listening for vsync again, and continued force-composites and vsyncs
   * will cause oscillation between observing vsync and not.
   * On some platforms, enabling/disabling vsync is not free and this
   * oscillating behavior causes a performance hit. In order to avoid this
   * problem, we reset the mVsyncNotificationsSkipped counter to keep vsync
   * enabled.
   */
  mVsyncNotificationsSkipped = 0;

  mLastCompose = TimeStamp::Now();
  MOZ_ASSERT(mVsyncSchedulerOwner);
  mVsyncSchedulerOwner->CompositeToTarget(aTarget, aRect);
}

bool
CompositorVsyncScheduler::NeedsComposite()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mNeedsComposite;
}

bool
CompositorVsyncScheduler::FlushPendingComposite()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (mNeedsComposite) {
    CancelCurrentCompositeTask();
    ForceComposeToTarget(nullptr, nullptr);
    return true;
  }
  return false;
}

void
CompositorVsyncScheduler::ObserveVsync()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mWidget->ObserveVsync(mVsyncObserver);
  mIsObservingVsync = true;
}

void
CompositorVsyncScheduler::UnobserveVsync()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mWidget->ObserveVsync(nullptr);
  mIsObservingVsync = false;
}

void
CompositorVsyncScheduler::DispatchVREvents(TimeStamp aVsyncTimestamp)
{
  {
    MonitorAutoLock lock(mCurrentVRListenerTaskMonitor);
    mCurrentVRListenerTask = nullptr;
  }
  // This only allows to be called by CompositorVsyncScheduler::PostVRTask()
  // When the process is going to shutdown, the runnable has chance to be executed
  // by other threads, we only want it to be run at VRListenerThread.
  if (!VRListenerThreadHolder::IsInVRListenerThread()) {
    return;
  }

  VRManager* vm = VRManager::Get();
  vm->NotifyVsync(aVsyncTimestamp);
}

void
CompositorVsyncScheduler::ScheduleTask(already_AddRefed<CancelableRunnable> aTask)
{
  MOZ_ASSERT(CompositorThreadHolder::Loop());
  CompositorThreadHolder::Loop()->PostDelayedTask(std::move(aTask), 0);
}

const TimeStamp&
CompositorVsyncScheduler::GetLastComposeTime() const
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mLastCompose;
}

} // namespace layers
} // namespace mozilla
