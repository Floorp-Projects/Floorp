/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_FlingOverScrollerAnimation_h_
#define mozilla_layers_FlingOverScrollerAnimation_h_

#include "AsyncPanZoomAnimation.h"
#include "OverScroller.h"

namespace mozilla {
namespace layers {

class AsyncPanZoomController;

class FlingOverScrollerAnimation: public AsyncPanZoomAnimation {
public:
  FlingOverScrollerAnimation(AsyncPanZoomController& aApzc,
                 widget::sdk::OverScroller::GlobalRef aOverScroller,
                 const RefPtr<const OverscrollHandoffChain>& aOverscrollHandoffChain,
                 const RefPtr<const AsyncPanZoomController>& aScrolledApzc);
  virtual bool DoSample(FrameMetrics& aFrameMetrics,
                        const TimeDuration& aDelta) override;
private:
  // Returns true if value is on or outside of axis bounds.
  bool CheckBounds(Axis& aAxis, float aValue, float* aClamped);

  AsyncPanZoomController& mApzc;
  widget::sdk::OverScroller::GlobalRef mOverScroller;
  RefPtr<const OverscrollHandoffChain> mOverscrollHandoffChain;
  RefPtr<const AsyncPanZoomController> mScrolledApzc;
  bool mSentBounceX;
  bool mSentBounceY;
  ParentLayerPoint mStartOffset;
  ParentLayerPoint mPreviousOffset;
  // Unit vector in the direction of the fling.
  ParentLayerPoint mFlingDirection;
  int32_t mOverScrollCount;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_FlingOverScrollerAnimation_h_
