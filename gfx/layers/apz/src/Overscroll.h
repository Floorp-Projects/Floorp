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
                      const ParentLayerPoint& aVelocity,
                      SideBits aOverscrollSideBits)
      : mApzc(aApzc), mOverscrollSideBits(aOverscrollSideBits) {
    MOZ_ASSERT(
        (mOverscrollSideBits & SideBits::eTopBottom) != SideBits::eTopBottom &&
            (mOverscrollSideBits & SideBits::eLeftRight) !=
                SideBits::eLeftRight,
        "Don't allow overscrolling on both sides at the same time");
    if ((aOverscrollSideBits & SideBits::eLeftRight) != SideBits::eNone) {
      mApzc.mX.StartOverscrollAnimation(aVelocity.x);
    }
    if ((aOverscrollSideBits & SideBits::eTopBottom) != SideBits::eNone) {
      mApzc.mY.StartOverscrollAnimation(aVelocity.y);
    }
  }
  virtual ~OverscrollAnimation() {
    mApzc.mX.EndOverscrollAnimation();
    mApzc.mY.EndOverscrollAnimation();
  }

  virtual bool DoSample(FrameMetrics& aFrameMetrics,
                        const TimeDuration& aDelta) override {
    // Can't inline these variables due to short-circuit evaluation.
    bool continueX = mApzc.mX.IsOverscrollAnimationAlive() &&
                     mApzc.mX.SampleOverscrollAnimation(
                         aDelta, mOverscrollSideBits & SideBits::eLeftRight);
    bool continueY = mApzc.mY.IsOverscrollAnimationAlive() &&
                     mApzc.mY.SampleOverscrollAnimation(
                         aDelta, mOverscrollSideBits & SideBits::eTopBottom);
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
      mDeferredTasks.AppendElement(NewRunnableMethod<ScrollSnapFlags>(
          "layers::AsyncPanZoomController::ScrollSnap", &mApzc,
          &AsyncPanZoomController::ScrollSnap,
          ScrollSnapFlags::IntendedDirection |
              ScrollSnapFlags::IntendedEndPosition));
      return false;
    }
    return true;
  }

  virtual bool WantsRepaints() override { return false; }

  // Tell the overscroll animation about the pan momentum event. For each axis,
  // the overscroll animation may start, stop, or continue managing that axis in
  // response to the pan momentum event
  void HandlePanMomentum(const ParentLayerPoint& aDisplacement) {
    float xOverscroll = mApzc.mX.GetOverscroll();
    if ((xOverscroll > 0 && aDisplacement.x > 0) ||
        (xOverscroll < 0 && aDisplacement.x < 0)) {
      if (!mApzc.mX.IsOverscrollAnimationRunning()) {
        // Start a new overscroll animation on this axis, if there is no
        // overscroll animation running and if the pan momentum displacement
        // the pan momentum displacement is the same direction of the current
        // overscroll.
        mApzc.mX.StartOverscrollAnimation(mApzc.mX.GetVelocity());
        mOverscrollSideBits |=
            xOverscroll > 0 ? SideBits::eRight : SideBits::eLeft;
      }
    } else if ((xOverscroll > 0 && aDisplacement.x < 0) ||
               (xOverscroll < 0 && aDisplacement.x > 0)) {
      // Otherwise, stop the animation in the direction so that it won't clobber
      // subsequent pan momentum scrolling.
      mApzc.mX.EndOverscrollAnimation();
    }

    // Same as above but for Y axis.
    float yOverscroll = mApzc.mY.GetOverscroll();
    if ((yOverscroll > 0 && aDisplacement.y > 0) ||
        (yOverscroll < 0 && aDisplacement.y < 0)) {
      if (!mApzc.mY.IsOverscrollAnimationRunning()) {
        mApzc.mY.StartOverscrollAnimation(mApzc.mY.GetVelocity());
        mOverscrollSideBits |=
            yOverscroll > 0 ? SideBits::eBottom : SideBits::eTop;
      }
    } else if ((yOverscroll > 0 && aDisplacement.y < 0) ||
               (yOverscroll < 0 && aDisplacement.y > 0)) {
      mApzc.mY.EndOverscrollAnimation();
    }
  }

  ScrollDirections GetDirections() const {
    ScrollDirections directions;
    if (mApzc.mX.IsOverscrollAnimationRunning()) {
      directions += ScrollDirection::eHorizontal;
    }
    if (mApzc.mY.IsOverscrollAnimationRunning()) {
      directions += ScrollDirection::eVertical;
    }
    return directions;
  };

  OverscrollAnimation* AsOverscrollAnimation() override { return this; }

  bool IsManagingXAxis() const {
    return mApzc.mX.IsOverscrollAnimationRunning();
  }
  bool IsManagingYAxis() const {
    return mApzc.mY.IsOverscrollAnimationRunning();
  }

 private:
  AsyncPanZoomController& mApzc;
  SideBits mOverscrollSideBits;
};

// Base class for different overscroll effects;
class OverscrollEffectBase {
 public:
  virtual ~OverscrollEffectBase() = default;

  // Try to increase the amount of overscroll by |aOverscroll|. Limited to
  // directions contained in |aOverscrollableDirections|. Components of
  // |aOverscroll| in directions that are successfully consumed are dropped.
  virtual void ConsumeOverscroll(
      ParentLayerPoint& aOverscroll,
      ScrollDirections aOverscrollableDirections) = 0;

  // Relieve overscroll. Depending on the implementation, the relief may
  // be immediate, or gradual (e.g. after an animation) but this starts
  // the process. |aVelocity| is the current velocity of the APZC, and
  // |aOverscrollSideBits| contains the side(s) at which the APZC is
  // overscrolled.
  virtual void RelieveOverscroll(const ParentLayerPoint& aVelocity,
                                 SideBits aOverscrollSideBits) = 0;

  virtual bool IsOverscrolled() const = 0;

  // Similarly to RelieveOverscroll(), but has immediate effect
  // (no animation).
  virtual void ClearOverscroll() = 0;
};

// A generic overscroll effect, implemented by AsyncPanZoomController itself.
class GenericOverscrollEffect : public OverscrollEffectBase {
 public:
  explicit GenericOverscrollEffect(AsyncPanZoomController& aApzc)
      : mApzc(aApzc) {}

  void ConsumeOverscroll(ParentLayerPoint& aOverscroll,
                         ScrollDirections aOverscrollableDirections) override {
    if (mApzc.mScrollMetadata.PrefersReducedMotion()) {
      return;
    }

    if (aOverscrollableDirections.contains(ScrollDirection::eHorizontal)) {
      mApzc.mX.OverscrollBy(aOverscroll.x);
      aOverscroll.x = 0;
    }

    if (aOverscrollableDirections.contains(ScrollDirection::eVertical)) {
      mApzc.mY.OverscrollBy(aOverscroll.y);
      aOverscroll.y = 0;
    }

    if (!aOverscrollableDirections.isEmpty()) {
      mApzc.ScheduleComposite();
    }
  }

  void RelieveOverscroll(const ParentLayerPoint& aVelocity,
                         SideBits aOverscrollSideBits) override {
    if (mApzc.mScrollMetadata.PrefersReducedMotion()) {
      return;
    }

    mApzc.StartOverscrollAnimation(aVelocity, aOverscrollSideBits);
  }

  bool IsOverscrolled() const override {
    return mApzc.IsPhysicallyOverscrolled();
  }

  void ClearOverscroll() override { mApzc.ClearPhysicalOverscroll(); }

 private:
  AsyncPanZoomController& mApzc;
};

// A widget-specific overscroll effect, implemented by the widget via
// GeckoContentController.
class WidgetOverscrollEffect : public OverscrollEffectBase {
 public:
  explicit WidgetOverscrollEffect(AsyncPanZoomController& aApzc)
      : mApzc(aApzc), mIsOverscrolled(false) {}

  void ConsumeOverscroll(ParentLayerPoint& aOverscroll,
                         ScrollDirections aOverscrollableDirections) override {
    RefPtr<GeckoContentController> controller =
        mApzc.GetGeckoContentController();
    if (controller && !aOverscrollableDirections.isEmpty()) {
      mIsOverscrolled = true;
      controller->UpdateOverscrollOffset(mApzc.GetGuid(), aOverscroll.x,
                                         aOverscroll.y, mApzc.IsRootContent());
      aOverscroll = ParentLayerPoint();
    }
  }

  void RelieveOverscroll(const ParentLayerPoint& aVelocity,
                         SideBits aOverscrollSideBits) override {
    RefPtr<GeckoContentController> controller =
        mApzc.GetGeckoContentController();
    // From APZC's point of view, consider it to no longer be overscrolled
    // as soon as RelieveOverscroll() is called. The widget may use a
    // delay or animation until the relieving of the overscroll is complete,
    // but we don't have any insight into that.
    mIsOverscrolled = false;
    if (controller) {
      controller->UpdateOverscrollVelocity(mApzc.GetGuid(), aVelocity.x,
                                           aVelocity.y, mApzc.IsRootContent());
    }
  }

  bool IsOverscrolled() const override { return mIsOverscrolled; }

  void ClearOverscroll() override {
    RelieveOverscroll(ParentLayerPoint(), SideBits() /* ignored */);
  }

 private:
  AsyncPanZoomController& mApzc;
  bool mIsOverscrolled;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_Overscroll_h
