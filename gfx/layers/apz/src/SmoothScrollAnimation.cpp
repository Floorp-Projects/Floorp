/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmoothScrollAnimation.h"
#include "ScrollAnimationBezierPhysics.h"
#include "mozilla/layers/APZPublicUtils.h"

namespace mozilla {
namespace layers {

SmoothScrollAnimation::SmoothScrollAnimation(AsyncPanZoomController& aApzc,
                                             const nsPoint& aInitialPosition,
                                             ScrollOrigin aOrigin)
    : GenericScrollAnimation(
          aApzc, aInitialPosition,
          apz::ComputeBezierAnimationSettingsForOrigin(aOrigin)),
      mOrigin(aOrigin) {}

SmoothScrollAnimation* SmoothScrollAnimation::AsSmoothScrollAnimation() {
  return this;
}

ScrollOrigin SmoothScrollAnimation::GetScrollOrigin() const { return mOrigin; }

ScrollOrigin SmoothScrollAnimation::GetScrollOriginForAction(
    KeyboardScrollAction::KeyboardScrollActionType aAction) {
  switch (aAction) {
    case KeyboardScrollAction::eScrollCharacter:
    case KeyboardScrollAction::eScrollLine: {
      return ScrollOrigin::Lines;
    }
    case KeyboardScrollAction::eScrollPage:
      return ScrollOrigin::Pages;
    case KeyboardScrollAction::eScrollComplete:
      return ScrollOrigin::Other;
    default:
      MOZ_ASSERT(false, "Unknown keyboard scroll action type");
      return ScrollOrigin::Other;
  }
}

}  // namespace layers
}  // namespace mozilla
