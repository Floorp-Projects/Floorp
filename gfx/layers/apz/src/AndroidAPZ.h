/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AndroidAPZ_h_
#define mozilla_layers_AndroidAPZ_h_

#include "AsyncPanZoomAnimation.h"
#include "AsyncPanZoomController.h"

namespace mozilla {
namespace layers {

class AndroidSpecificState : public PlatformSpecificStateBase {
 public:
  virtual AndroidSpecificState* AsAndroidSpecificState() override {
    return this;
  }

  virtual AsyncPanZoomAnimation* CreateFlingAnimation(
      AsyncPanZoomController& aApzc, const FlingHandoffState& aHandoffState,
      float aPLPPI) override;
  virtual UniquePtr<VelocityTracker> CreateVelocityTracker(
      Axis* aAxis) override;

  static void InitializeGlobalState();
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_AndroidAPZ_h_
