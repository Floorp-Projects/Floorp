/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GAMEPAD_DOM_GAMEPADSTATELAYOUT_H_
#define GAMEPAD_DOM_GAMEPADSTATELAYOUT_H_

#include "mozilla/dom/Gamepad.h"
#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/dom/GamepadHandle.h"
#include "mozilla/dom/GamepadLightIndicatorBinding.h"
#include "mozilla/dom/GamepadPoseState.h"
#include "mozilla/dom/GamepadTouchState.h"
#include "mozilla/Array.h"
#include <bitset>
#include <inttypes.h>

namespace mozilla::dom {

// Define the shared memory and reasonable maximums for gamepads
constexpr size_t kMaxGamepadIdLength = 32;
constexpr size_t kMaxGamepads = 8;

// These 2 values from from the w3 spec:
// https://dvcs.w3.org/hg/gamepad/raw-file/default/gamepad.html#remapping
constexpr size_t kMaxButtonsPerGamepad = kStandardGamepadButtons;
constexpr size_t kMaxAxesPerGamepad = kStandardGamepadAxes;

constexpr size_t kMaxHapticsPerGamepad = 0;  // We don't support haptics yet
constexpr size_t kMaxLightsPerGamepad = 2;
constexpr size_t kMaxNumMultiTouches = 2;

// CAUTION: You must update ValidateGamepadSystemState() if you change
// any of these structures

struct GamepadProperties {
  Array<char, kMaxGamepadIdLength + 1> id{};
  GamepadMappingType mapping{};
  GamepadHand hand{};
  uint32_t numAxes{};
  uint32_t numButtons{};
  uint32_t numHaptics{};
  uint32_t numLights{};
  uint32_t numTouches{};
};

// It is important that the default values for these members matches the
// default values for the JS objects. When a new gamepad is added, its state
// will be compared to a default GamepadValues object to determine what events
// are needed.
struct GamepadValues {
  Array<double, kMaxAxesPerGamepad> axes{};
  Array<double, kMaxButtonsPerGamepad> buttonValues{};
  Array<GamepadLightIndicatorType, kMaxLightsPerGamepad> lights{};
  Array<GamepadTouchState, kMaxNumMultiTouches> touches{};
  GamepadPoseState pose{};
  std::bitset<kMaxButtonsPerGamepad> buttonPressedBits;
  std::bitset<kMaxButtonsPerGamepad> buttonTouchedBits;
};

struct GamepadSlot {
  GamepadHandle handle{};
  GamepadProperties props{};
  GamepadValues values{};
};

struct GamepadSystemState {
  Array<GamepadSlot, kMaxGamepads> gamepadSlots{};
  uint64_t changeId{};
  uint32_t testCommandId{};
  bool testCommandTrigger{};
};

static void ValidateGamepadSystemState(GamepadSystemState* p) {
  for (auto& slot : p->gamepadSlots) {
    // Check that id is a null-terminated string
    bool hasNull = false;
    for (char c : slot.props.id) {
      if (c == 0) {
        hasNull = true;
        break;
      }
    }
    MOZ_RELEASE_ASSERT(hasNull);

    MOZ_RELEASE_ASSERT(slot.props.mapping < GamepadMappingType::EndGuard_);
    MOZ_RELEASE_ASSERT(slot.props.hand < GamepadHand::EndGuard_);
    MOZ_RELEASE_ASSERT(slot.props.numAxes <= kMaxAxesPerGamepad);
    MOZ_RELEASE_ASSERT(slot.props.numButtons <= kMaxButtonsPerGamepad);
    MOZ_RELEASE_ASSERT(slot.props.numHaptics <= kMaxHapticsPerGamepad);
    MOZ_RELEASE_ASSERT(slot.props.numLights <= kMaxLightsPerGamepad);
    MOZ_RELEASE_ASSERT(slot.props.numTouches <= kMaxNumMultiTouches);
  }
}

}  // namespace mozilla::dom

#endif  // GAMEPAD_DOM_GAMEPADSTATELAYOUT_H_
