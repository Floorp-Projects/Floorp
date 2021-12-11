/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_SmoothMsdScrollAnimation_h_
#define mozilla_layers_SmoothMsdScrollAnimation_h_

#include "AsyncPanZoomAnimation.h"
#include "mozilla/layers/AxisPhysicsMSDModel.h"

namespace mozilla {
namespace layers {

class AsyncPanZoomController;

class SmoothMsdScrollAnimation : public AsyncPanZoomAnimation {
 public:
  SmoothMsdScrollAnimation(AsyncPanZoomController& aApzc,
                           const CSSPoint& aInitialPosition,
                           const CSSPoint& aInitialVelocity,
                           const CSSPoint& aDestination, double aSpringConstant,
                           double aDampingRatio);

  /**
   * Advances a smooth scroll simulation based on the time passed in |aDelta|.
   * This should be called whenever sampling the content transform for this
   * frame. Returns true if the smooth scroll should be advanced by one frame,
   * or false if the smooth scroll has ended.
   */
  bool DoSample(FrameMetrics& aFrameMetrics,
                const TimeDuration& aDelta) override;

  void SetDestination(const CSSPoint& aNewDestination);
  CSSPoint GetDestination() const;
  SmoothMsdScrollAnimation* AsSmoothMsdScrollAnimation() override;

 private:
  AsyncPanZoomController& mApzc;
  AxisPhysicsMSDModel mXAxisModel;
  AxisPhysicsMSDModel mYAxisModel;
};

}  // namespace layers
}  // namespace mozilla

#endif
