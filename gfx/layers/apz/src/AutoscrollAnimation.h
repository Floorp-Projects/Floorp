/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AutocrollAnimation_h_
#define mozilla_layers_AutocrollAnimation_h_

#include "AsyncPanZoomAnimation.h"

namespace mozilla {
namespace layers {

class AsyncPanZoomController;

class AutoscrollAnimation : public AsyncPanZoomAnimation
{
public:
  AutoscrollAnimation(AsyncPanZoomController& aApzc,
                      const ScreenPoint& aAnchorLocation);

  bool DoSample(FrameMetrics& aFrameMetrics, const TimeDuration& aDelta) override;

  void Cancel(CancelAnimationFlags aFlags) override;
private:
  AsyncPanZoomController& mApzc;
  ScreenPoint mAnchorLocation;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_AutoscrollAnimation_h_
