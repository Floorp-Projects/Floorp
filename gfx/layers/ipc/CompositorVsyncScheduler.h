/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorVsyncScheduler_h
#define mozilla_layers_CompositorVsyncScheduler_h

#include <stdint.h>                     // for uint64_t

#include "mozilla/Attributes.h"         // for override
#include "mozilla/Monitor.h"            // for Monitor
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/VsyncDispatcher.h"
#include "mozilla/widget/CompositorWidget.h"
#include "nsISupportsImpl.h"


namespace mozilla {

class CancelableRunnable;
class Runnable;

namespace gfx {
class DrawTarget;
} // namespace gfx

namespace layers {

class CompositorVsyncSchedulerOwner;

/**
 * Manages the vsync (de)registration and tracking on behalf of the
 * compositor when it need to paint.
 * Turns vsync notifications into scheduled composites.
 **/
class CompositorVsyncScheduler
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorVsyncScheduler)

public:
  explicit CompositorVsyncScheduler(CompositorVsyncSchedulerOwner* aVsyncSchedulerOwner,
                                    widget::CompositorWidget* aWidget);

  /**
   * Notify this class of a vsync. This will trigger a composite if one is
   * needed. This must be called from the vsync dispatch thread.
   */
  bool NotifyVsync(TimeStamp aVsyncTimestamp);

  /**
   * Do cleanup. This must be called on the compositor thread.
   */
  void Destroy();

  /**
   * Notify this class that a composition is needed. This will trigger a
   * composition soon (likely at the next vsync). This must be called on the
   * compositor thread.
   */
  void ScheduleComposition();

  /**
   * Cancel any composite task that has been scheduled but hasn't run yet.
   */
  void CancelCurrentCompositeTask();

  /**
   * Check if a composite is pending. This is generally true between a call
   * to ScheduleComposition() and the time the composite happens.
   */
  bool NeedsComposite();

  /**
   * Force a composite to happen right away, without waiting for the next vsync.
   * This must be called on the compositor thread.
   */
  void ForceComposeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect);

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
  const TimeStamp& GetLastComposeTime() const;

  /**
   * Update LastCompose TimeStamp to current timestamp.
   * The function is typically used when composition is handled outside the CompositorVsyncScheduler.
   */
  void UpdateLastComposeTime();

private:
  virtual ~CompositorVsyncScheduler();

  // Schedule a task to run on the compositor thread.
  void ScheduleTask(already_AddRefed<CancelableRunnable>);

  // Post a task to run Composite() on the compositor thread, if there isn't
  // such a task already queued. Can be called from any thread.
  void PostCompositeTask(TimeStamp aCompositeTimestamp);

  // Post a task to run DispatchVREvents() on the VR thread, if there isn't
  // such a task already queued. Can be called from any thread.
  void PostVRTask(TimeStamp aTimestamp);

  // This gets run at vsync time and "does" a composite (which really means
  // update internal state and call the owner to do the composite).
  void Composite(TimeStamp aVsyncTimestamp);

  void ObserveVsync();
  void UnobserveVsync();

  void DispatchVREvents(TimeStamp aVsyncTimestamp);

  class Observer final : public VsyncObserver
  {
  public:
    explicit Observer(CompositorVsyncScheduler* aOwner);
    virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) override;
    void Destroy();
  private:
    virtual ~Observer();

    Mutex mMutex;
    // Hold raw pointer to avoid mutual reference.
    CompositorVsyncScheduler* mOwner;
  };

  CompositorVsyncSchedulerOwner* mVsyncSchedulerOwner;
  TimeStamp mLastCompose;

  bool mAsapScheduling;
  bool mIsObservingVsync;
  TimeStamp mCompositeRequestedAt;
  int32_t mVsyncNotificationsSkipped;
  widget::CompositorWidget* mWidget;
  RefPtr<CompositorVsyncScheduler::Observer> mVsyncObserver;

  mozilla::Monitor mCurrentCompositeTaskMonitor;
  RefPtr<CancelableRunnable> mCurrentCompositeTask;

  mozilla::Monitor mCurrentVRTaskMonitor;
  RefPtr<Runnable> mCurrentVRTask;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_CompositorVsyncScheduler_h
