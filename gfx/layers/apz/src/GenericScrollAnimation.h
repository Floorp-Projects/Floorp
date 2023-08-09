/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_GenericScrollAnimation_h_
#define mozilla_layers_GenericScrollAnimation_h_

#include "AsyncPanZoomAnimation.h"

namespace mozilla {

class ScrollAnimationPhysics;

namespace layers {

class AsyncPanZoomController;

class GenericScrollAnimation : public AsyncPanZoomAnimation {
 public:
  GenericScrollAnimation(AsyncPanZoomController& aApzc,
                         const nsPoint& aInitialPosition, ScrollOrigin aOrigin);

  bool DoSample(FrameMetrics& aFrameMetrics,
                const TimeDuration& aDelta) override;

  bool HandleScrollOffsetUpdate(const Maybe<CSSPoint>& aRelativeDelta) override;

  void UpdateDelta(TimeStamp aTime, const nsPoint& aDelta,
                   const nsSize& aCurrentVelocity);
  void UpdateDestination(TimeStamp aTime, const nsPoint& aDestination,
                         const nsSize& aCurrentVelocity);

  CSSPoint GetDestination() const {
    return CSSPoint::FromAppUnits(mFinalDestination);
  }

 private:
  void Update(TimeStamp aTime, const nsSize& aCurrentVelocity);

 protected:
  AsyncPanZoomController& mApzc;
  UniquePtr<ScrollAnimationPhysics> mAnimationPhysics;
  nsPoint mFinalDestination;
  // If a direction is forced to overscroll, it means it's axis in that
  // direction is locked, and scroll in that direction is treated as overscroll
  // of an equal amount, which, for example, may then bubble up a scroll action
  // to its parent, or may behave as whatever an overscroll occurence requires
  // to behave
  Maybe<ScrollDirection> mDirectionForcedToOverscroll;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_GenericScrollAnimation_h_
