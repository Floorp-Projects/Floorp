/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_Overscroll_h
#define mozilla_layers_Overscroll_h

#include "AsyncPanZoomAnimation.h"
#include "AsyncPanZoomController.h"
#include "mozilla/TimeStamp.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

// Animation used by GenericOverscrollEffect.
class OverscrollAnimation : public AsyncPanZoomAnimation {
 public:
  OverscrollAnimation(AsyncPanZoomController& aApzc,
                      const ParentLayerPoint& aVelocity)
      : mApzc(aApzc) {
    mApzc.mX.StartOverscrollAnimation(aVelocity.x);
    mApzc.mY.StartOverscrollAnimation(aVelocity.y);
  }
  virtual ~OverscrollAnimation() {
    mApzc.mX.EndOverscrollAnimation();
    mApzc.mY.EndOverscrollAnimation();
  }

  virtual bool DoSample(FrameMetrics& aFrameMetrics,
                        const TimeDuration& aDelta) override {
    // Can't inline these variables due to short-circuit evaluation.
    bool continueX = mApzc.mX.SampleOverscrollAnimation(aDelta);
    bool continueY = mApzc.mY.SampleOverscrollAnimation(aDelta);
    if (!continueX && !continueY) {
      // If we got into overscroll from a fling, that fling did not request a
      // fling snap to avoid a resulting scrollTo from cancelling the overscroll
      // animation too early. We do still want to request a fling snap, though,
      // in case the end of the axis at which we're overscrolled is not a valid
      // snap point, so we request one now. If there are no snap points, this
      // will do nothing. If there are snap points, we'll get a scrollTo that
      // snaps us back to the nearest valid snap point. The scroll snapping is
      // done in a deferred task, otherwise the state change to NOTHING caused
      // by the overscroll animation ending would clobber a possible state
      // change to SMOOTH_SCROLL in ScrollSnap().
      mDeferredTasks.AppendElement(
          NewRunnableMethod("layers::AsyncPanZoomController::ScrollSnap",
                            &mApzc, &AsyncPanZoomController::ScrollSnap));
      return false;
    }
    return true;
  }

  virtual bool WantsRepaints() override { return false; }

 private:
  AsyncPanZoomController& mApzc;
};

// Base class for different overscroll effects;
class OverscrollEffectBase {
 public:
  virtual ~OverscrollEffectBase() = default;
  virtual void ConsumeOverscroll(ParentLayerPoint& aOverscroll,
                                 bool aShouldOverscrollX,
                                 bool aShouldOverscrollY) = 0;
  virtual void HandleFlingOverscroll(const ParentLayerPoint& aVelocity) = 0;
};

// A generic overscroll effect, implemented by AsyncPanZoomController itself.
class GenericOverscrollEffect : public OverscrollEffectBase {
 public:
  explicit GenericOverscrollEffect(AsyncPanZoomController& aApzc)
      : mApzc(aApzc) {}

  void ConsumeOverscroll(ParentLayerPoint& aOverscroll, bool aShouldOverscrollX,
                         bool aShouldOverscrollY) override {
    if (aShouldOverscrollX) {
      mApzc.mX.OverscrollBy(aOverscroll.x);
      aOverscroll.x = 0;
    }

    if (aShouldOverscrollY) {
      mApzc.mY.OverscrollBy(aOverscroll.y);
      aOverscroll.y = 0;
    }

    if (aShouldOverscrollX || aShouldOverscrollY) {
      mApzc.ScheduleComposite();
    }
  }

  void HandleFlingOverscroll(const ParentLayerPoint& aVelocity) override {
    mApzc.StartOverscrollAnimation(aVelocity);
  }

 private:
  AsyncPanZoomController& mApzc;
};

// A widget-specific overscroll effect, implemented by the widget via
// GeckoContentController.
class WidgetOverscrollEffect : public OverscrollEffectBase {
 public:
  explicit WidgetOverscrollEffect(AsyncPanZoomController& aApzc)
      : mApzc(aApzc) {}

  void ConsumeOverscroll(ParentLayerPoint& aOverscroll, bool aShouldOverscrollX,
                         bool aShouldOverscrollY) override {
    RefPtr<GeckoContentController> controller =
        mApzc.GetGeckoContentController();
    if (controller && (aShouldOverscrollX || aShouldOverscrollY)) {
      controller->UpdateOverscrollOffset(aOverscroll.x, aOverscroll.y,
                                         mApzc.IsRootContent());
      aOverscroll = ParentLayerPoint();
    }
  }

  void HandleFlingOverscroll(const ParentLayerPoint& aVelocity) override {
    RefPtr<GeckoContentController> controller =
        mApzc.GetGeckoContentController();
    if (controller) {
      controller->UpdateOverscrollVelocity(aVelocity.x, aVelocity.y,
                                           mApzc.IsRootContent());
    }
  }

 private:
  AsyncPanZoomController& mApzc;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_Overscroll_h
