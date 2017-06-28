/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_KeyboardScrollAnimation_h_
#define mozilla_layers_KeyboardScrollAnimation_h_

#include "GenericScrollAnimation.h"
#include "mozilla/layers/KeyboardMap.h"

namespace mozilla {
namespace layers {

class AsyncPanZoomController;

class KeyboardScrollAnimation
  : public GenericScrollAnimation
{
public:
  KeyboardScrollAnimation(AsyncPanZoomController& aApzc,
                          const nsPoint& aInitialPosition,
                          KeyboardScrollAction::KeyboardScrollActionType aType);

  KeyboardScrollAnimation* AsKeyboardScrollAnimation() override {
    return this;
  }
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_KeyboardScrollAnimation_h_
