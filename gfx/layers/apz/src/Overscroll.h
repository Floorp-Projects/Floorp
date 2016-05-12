/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_Overscroll_h
#define mozilla_layers_Overscroll_h

#include "AsyncPanZoomAnimation.h"
#include "AsyncPanZoomController.h"
#include "FrameMetrics.h"
#include "mozilla/TimeStamp.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

class OverscrollAnimation: public AsyncPanZoomAnimation {
public:
  explicit OverscrollAnimation(AsyncPanZoomController& aApzc, const ParentLayerPoint& aVelocity)
    : mApzc(aApzc)
  {
    mApzc.mX.StartOverscrollAnimation(aVelocity.x);
    mApzc.mY.StartOverscrollAnimation(aVelocity.y);
  }
  ~OverscrollAnimation()
  {
    mApzc.mX.EndOverscrollAnimation();
    mApzc.mY.EndOverscrollAnimation();
  }

  virtual bool DoSample(FrameMetrics& aFrameMetrics,
                        const TimeDuration& aDelta) override
  {
    // Can't inline these variables due to short-circuit evaluation.
    bool continueX = mApzc.mX.SampleOverscrollAnimation(aDelta);
    bool continueY = mApzc.mY.SampleOverscrollAnimation(aDelta);
    if (!continueX && !continueY) {
      // If we got into overscroll from a fling, that fling did not request a
      // fling snap to avoid a resulting scrollTo from cancelling the overscroll
      // animation too early. We do still want to request a fling snap, though,
      // in case the end of the axis at which we're overscrolled is not a valid
      // snap point, so we request one now. If there are no snap points, this will
      // do nothing. If there are snap points, we'll get a scrollTo that snaps us
      // back to the nearest valid snap point.
      // The scroll snapping is done in a deferred task, otherwise the state
      // change to NOTHING caused by the overscroll animation ending would
      // clobber a possible state change to SMOOTH_SCROLL in ScrollSnap().
      mDeferredTasks.AppendElement(NewRunnableMethod(&mApzc, &AsyncPanZoomController::ScrollSnap));
      return false;
    }
    return true;
  }

  virtual bool WantsRepaints() override
  {
    return false;
  }

private:
  AsyncPanZoomController& mApzc;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_Overscroll_h
