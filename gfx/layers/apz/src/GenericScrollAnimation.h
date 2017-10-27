/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_GenericScrollAnimation_h_
#define mozilla_layers_GenericScrollAnimation_h_

#include "AsyncPanZoomAnimation.h"

namespace mozilla {

struct ScrollAnimationBezierPhysicsSettings;
class ScrollAnimationPhysics;

namespace layers {

class AsyncPanZoomController;

class GenericScrollAnimation
  : public AsyncPanZoomAnimation
{
public:
  GenericScrollAnimation(AsyncPanZoomController& aApzc,
                         const nsPoint& aInitialPosition,
                         const ScrollAnimationBezierPhysicsSettings& aSettings);

  bool DoSample(FrameMetrics& aFrameMetrics, const TimeDuration& aDelta) override;

  void UpdateDelta(TimeStamp aTime, nsPoint aDelta, const nsSize& aCurrentVelocity);
  void UpdateDestination(TimeStamp aTime, nsPoint aDestination, const nsSize& aCurrentVelocity);

  CSSPoint GetDestination() const {
    return CSSPoint::FromAppUnits(mFinalDestination);
  }

private:
  void Update(TimeStamp aTime, const nsSize& aCurrentVelocity);

protected:
  AsyncPanZoomController& mApzc;
  UniquePtr<ScrollAnimationPhysics> mAnimationPhysics;
  nsPoint mFinalDestination;
  bool mForceVerticalOverscroll;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_GenericScrollAnimation_h_
