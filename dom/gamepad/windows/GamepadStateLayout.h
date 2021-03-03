/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GAMEPAD_DOM_GAMEPADSTATELAYOUT_H_
#define GAMEPAD_DOM_GAMEPADSTATELAYOUT_H_

#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/dom/GamepadHandle.h"
#include "mozilla/dom/GamepadLightIndicatorBinding.h"
#include "mozilla/dom/GamepadPoseState.h"
#include "mozilla/dom/GamepadTouchState.h"
#include "mozilla/Array.h"
#include <bitset>
#include <inttypes.h>

namespace mozilla::dom {

// Placeholder for the actual shared state in a later changelist
struct GamepadSystemState {
  uint32_t placeholder;
};

}  // namespace mozilla::dom

#endif  // GAMEPAD_DOM_GAMEPADSTATELAYOUT_H_
