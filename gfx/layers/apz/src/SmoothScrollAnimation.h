/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_SmoothScrollAnimation_h_
#define mozilla_layers_SmoothScrollAnimation_h_

#include "GenericScrollAnimation.h"
#include "ScrollPositionUpdate.h"
#include "mozilla/ScrollOrigin.h"
#include "mozilla/layers/KeyboardScrollAction.h"

namespace mozilla {
namespace layers {

class AsyncPanZoomController;

class SmoothScrollAnimation : public GenericScrollAnimation {
 public:
  SmoothScrollAnimation(AsyncPanZoomController& aApzc,
                        const nsPoint& aInitialPosition, ScrollOrigin aOrigin);

  void UpdateDestinationAndSnapTargets(
      TimeStamp aTime, const nsPoint& aDestination,
      const nsSize& aCurrentVelocity, ScrollSnapTargetIds&& aSnapTargetIds,
      ScrollTriggeredByScript aTriggeredByScript);

  SmoothScrollAnimation* AsSmoothScrollAnimation() override;
  bool WasTriggeredByScript() const override {
    return mTriggeredByScript == ScrollTriggeredByScript::Yes;
  }
  ScrollSnapTargetIds TakeSnapTargetIds() { return std::move(mSnapTargetIds); }
  ScrollOrigin GetScrollOrigin() const;
  static ScrollOrigin GetScrollOriginForAction(
      KeyboardScrollAction::KeyboardScrollActionType aAction);

 private:
  ScrollOrigin mOrigin;
  ScrollSnapTargetIds mSnapTargetIds;
  ScrollTriggeredByScript mTriggeredByScript;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_SmoothScrollAnimation_h_
