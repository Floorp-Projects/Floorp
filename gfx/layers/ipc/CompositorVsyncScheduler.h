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


class MessageLoop;
class nsIWidget;

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

  bool NotifyVsync(TimeStamp aVsyncTimestamp);
  void SetNeedsComposite();
  void OnForceComposeToTarget();

  void ScheduleTask(already_AddRefed<CancelableRunnable>, int);
  void ResumeComposition();
  void ComposeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect = nullptr);
  void PostCompositeTask(TimeStamp aCompositeTimestamp);
  void PostVRTask(TimeStamp aTimestamp);
  void Destroy();
  void ScheduleComposition();
  void CancelCurrentCompositeTask();
  bool NeedsComposite();
  void Composite(TimeStamp aVsyncTimestamp);
  void ForceComposeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect);

  const TimeStamp& GetLastComposeTime()
  {
    return mLastCompose;
  }

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  const TimeStamp& GetExpectedComposeStartTime()
  {
    return mExpectedComposeStartTime;
  }
#endif

private:
  virtual ~CompositorVsyncScheduler();

  void NotifyCompositeTaskExecuted();
  void ObserveVsync();
  void UnobserveVsync();
  void DispatchTouchEvents(TimeStamp aVsyncTimestamp);
  void DispatchVREvents(TimeStamp aVsyncTimestamp);
  void CancelCurrentSetNeedsCompositeTask();

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

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeStamp mExpectedComposeStartTime;
#endif

  bool mAsapScheduling;
  bool mIsObservingVsync;
  uint32_t mNeedsComposite;
  int32_t mVsyncNotificationsSkipped;
  widget::CompositorWidget* mWidget;
  RefPtr<CompositorVsyncScheduler::Observer> mVsyncObserver;

  mozilla::Monitor mCurrentCompositeTaskMonitor;
  RefPtr<CancelableRunnable> mCurrentCompositeTask;

  mozilla::Monitor mSetNeedsCompositeMonitor;
  RefPtr<CancelableRunnable> mSetNeedsCompositeTask;

  mozilla::Monitor mCurrentVRListenerTaskMonitor;
  RefPtr<Runnable> mCurrentVRListenerTask;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_CompositorVsyncScheduler_h
