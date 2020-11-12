/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_SmoothScrollAnimation_h_
#define mozilla_layers_SmoothScrollAnimation_h_

#include "GenericScrollAnimation.h"
#include "mozilla/ScrollOrigin.h"
#include "mozilla/layers/KeyboardScrollAction.h"

namespace mozilla {
namespace layers {

class AsyncPanZoomController;

class SmoothScrollAnimation : public GenericScrollAnimation {
 public:
  SmoothScrollAnimation(AsyncPanZoomController& aApzc,
                        const nsPoint& aInitialPosition,
                        ScrollOrigin aScrollOrigin);

  SmoothScrollAnimation* AsSmoothScrollAnimation() override;
  ScrollOrigin GetScrollOrigin() const;
  static ScrollOrigin GetScrollOriginForAction(
      KeyboardScrollAction::KeyboardScrollActionType aAction);

 private:
  ScrollOrigin mOrigin;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_SmoothScrollAnimation_h_
