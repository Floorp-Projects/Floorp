/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidAPZ.h"

#include "AndroidFlingPhysics.h"
#include "AndroidVelocityTracker.h"
#include "AsyncPanZoomController.h"
#include "GenericFlingAnimation.h"
#include "OverscrollHandoffState.h"

namespace mozilla {
namespace layers {

AsyncPanZoomAnimation* AndroidSpecificState::CreateFlingAnimation(
    AsyncPanZoomController& aApzc, const FlingHandoffState& aHandoffState,
    float aPLPPI) {
  return new GenericFlingAnimation<AndroidFlingPhysics>(aApzc, aHandoffState,
                                                        aPLPPI);
}

UniquePtr<VelocityTracker> AndroidSpecificState::CreateVelocityTracker(
    Axis* aAxis) {
  return MakeUnique<AndroidVelocityTracker>();
}

/* static */
void AndroidSpecificState::InitializeGlobalState() {
  AndroidFlingPhysics::InitializeGlobalState();
}

}  // namespace layers
}  // namespace mozilla
