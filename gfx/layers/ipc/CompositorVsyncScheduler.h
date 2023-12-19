/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorVsyncScheduler_h
#define mozilla_layers_CompositorVsyncScheduler_h

#include <stdint.h>  // for uint64_t

#include "mozilla/Attributes.h"  // for override
#include "mozilla/Monitor.h"     // for Monitor
#include "mozilla/RefPtr.h"      // for RefPtr
#include "mozilla/TimeStamp.h"   // for TimeStamp
#include "mozilla/gfx/Point.h"   // for IntSize
#include "mozilla/layers/SampleTime.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/VsyncDispatcher.h"
#include "mozilla/widget/CompositorWidget.h"
#include "nsISupportsImpl.h"

namespace mozilla {

class CancelableRunnable;
class Runnable;

namespace gfx {
class DrawTarget;
}  // namespace gfx

namespace layers {

class CompositorVsyncSchedulerOwner;

/**
 * Manages the vsync (de)registration and tracking on behalf of the
 * compositor when it need to paint.
 * Turns vsync notifications into scheduled composites.
 **/
class CompositorVsyncScheduler {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorVsyncScheduler)

 public:
  CompositorVsyncScheduler(CompositorVsyncSchedulerOwner* aVsyncSchedulerOwner,
                           widget::CompositorWidget* aWidget);

  /**
   * Notify this class of a vsync. This will trigger a composite if one is
   * needed. This must be called from the vsync dispatch thread.
   */
  void NotifyVsync(const VsyncEvent& aVsync);

  /**
   * Do cleanup. This must be called on the compositor thread.
   */
  void Destroy();

  /**
   * Notify this class that a composition is needed. This will trigger a
   * composition soon (likely at the next vsync). This must be called on the
   * compositor thread.
   */
  void ScheduleComposition(wr::RenderReasons aReasons);

  /**
   * Cancel any composite task that has been scheduled but hasn't run yet.
   *
   * Returns the render reasons of the canceled task if any.
   */
  wr::RenderReasons CancelCurrentCompositeTask();

  /**
   * Check if a composite is pending. This is generally true between a call
   * to ScheduleComposition() and the time the composite happens.
   */
  bool NeedsComposite();

  /**
   * Force a composite to happen right away, without waiting for the next vsync.
   * This must be called on the compositor thread.
   */
  void ForceComposeToTarget(wr::RenderReasons aReasons,
                            gfx::DrawTarget* aTarget,
                            const gfx::IntRect* aRect);

  /**
   * If a composite is pending, force it to trigger right away. This must be
   * called on the compositor thread. Returns true if there was a composite
   * flushed.
   */
  bool FlushPendingComposite();

  /**
   * Return the vsync timestamp of the last or ongoing composite. Must be called
   * on the compositor thread.
   */
  const SampleTime& GetLastComposeTime() const;

  /**
   * Return the vsync timestamp and id of the most recently received
   * vsync event. Must be called on the compositor thread.
   */
  const TimeStamp& GetLastVsyncTime() const;
  const TimeStamp& GetLastVsyncOutputTime() const;
  const VsyncId& GetLastVsyncId() const;

  /**
   * Update LastCompose TimeStamp to current timestamp.
   * The function is typically used when composition is handled outside the
   * CompositorVsyncScheduler.
   */
  void UpdateLastComposeTime();

 private:
  virtual ~CompositorVsyncScheduler();

  // Post a task to run Composite() on the compositor thread, if there isn't
  // such a task already queued. Can be called from any thread.
  void PostCompositeTask(const VsyncEvent& aVsyncEvent,
                         wr::RenderReasons aReasons);

  // Post a task to run DispatchVREvents() on the VR thread, if there isn't
  // such a task already queued. Can be called from any thread.
  void PostVRTask(TimeStamp aTimestamp);

  /**
   * Cancel any VR task that has been scheduled but hasn't run yet.
   */
  void CancelCurrentVRTask();

  // This gets run at vsync time and "does" a composite (which really means
  // update internal state and call the owner to do the composite).
  void Composite(const VsyncEvent& aVsyncEvent, wr::RenderReasons aReasons);

  void ObserveVsync();
  void UnobserveVsync();

  void DispatchVREvents(TimeStamp aVsyncTimestamp);

  class Observer final : public VsyncObserver {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorVsyncScheduler::Observer,
                                          override)

   public:
    explicit Observer(CompositorVsyncScheduler* aOwner);
    void NotifyVsync(const VsyncEvent& aVsync) override;
    void Destroy();

   private:
    virtual ~Observer();

    Mutex mMutex;
    // Hold raw pointer to avoid mutual reference.
    CompositorVsyncScheduler* mOwner;
  };

  CompositorVsyncSchedulerOwner* mVsyncSchedulerOwner;
  SampleTime mLastComposeTime;
  TimeStamp mLastVsyncTime;
  TimeStamp mLastVsyncOutputTime;
  VsyncId mLastVsyncId;

  bool mAsapScheduling;
  bool mIsObservingVsync;
  // Accessed on the compositor thread.
  wr::RenderReasons mRendersDelayedByVsyncReasons;
  TimeStamp mCompositeRequestedAt;
  int32_t mVsyncNotificationsSkipped;
  widget::CompositorWidget* mWidget;
  RefPtr<CompositorVsyncScheduler::Observer> mVsyncObserver;

  mozilla::Monitor mCurrentCompositeTaskMonitor;
  RefPtr<CancelableRunnable> mCurrentCompositeTask
      MOZ_GUARDED_BY(mCurrentCompositeTaskMonitor);
  // Accessed on multiple threads, guarded by mCurrentCompositeTaskMonitor.
  wr::RenderReasons mCurrentCompositeTaskReasons
      MOZ_GUARDED_BY(mCurrentCompositeTaskMonitor);

  mozilla::Monitor mCurrentVRTaskMonitor;
  RefPtr<CancelableRunnable> mCurrentVRTask
      MOZ_GUARDED_BY(mCurrentVRTaskMonitor);
};
}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CompositorVsyncScheduler_h
