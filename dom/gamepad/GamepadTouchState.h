/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_gamepad_GamepadTouchState_h_
#define mozilla_dom_gamepad_GamepadTouchState_h_

namespace mozilla::dom {

struct GamepadTouchState {
  uint32_t touchId;
  uint32_t surfaceId;
  float position[2];
  uint32_t surfaceDimensions[2];
  bool isSurfaceDimensionsValid;

  GamepadTouchState()
      : touchId(0),
        surfaceId(0),
        position{0, 0},
        surfaceDimensions{0, 0},
        isSurfaceDimensionsValid(false) {}

  bool operator==(const GamepadTouchState& aTouch) const {
    return touchId == aTouch.touchId && surfaceId == aTouch.surfaceId &&
           position[0] == aTouch.position[0] &&
           position[1] == aTouch.position[1] &&
           surfaceDimensions[0] == aTouch.surfaceDimensions[0] &&
           surfaceDimensions[1] == aTouch.surfaceDimensions[1] &&
           isSurfaceDimensionsValid == aTouch.isSurfaceDimensionsValid;
  }

  bool operator!=(const GamepadTouchState& aTouch) const {
    return !(*this == aTouch);
  }
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_gamepad_GamepadTouchState_h_
