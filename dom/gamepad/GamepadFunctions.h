/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GamepadFunctions_h_
#define mozilla_dom_GamepadFunctions_h_

#include "mozilla/dom/GamepadBinding.h"

namespace mozilla {
namespace dom {
namespace GamepadFunctions {

// Functions for building and transmitting IPDL messages through the HAL
// sandbox. Used by platform specific Gamepad implementations

// Add a gamepad to the list of known gamepads, and return its index.
uint32_t AddGamepad(const char* aID, GamepadMappingType aMapping,
                    uint32_t aNumButtons, uint32_t aNumAxes);
// Remove the gamepad at |aIndex| from the list of known gamepads.
void RemoveGamepad(uint32_t aIndex);

// Update the state of |aButton| for the gamepad at |aIndex| for all
// windows that are listening and visible, and fire one of
// a gamepadbutton{up,down} event at them as well.
// aPressed is used for digital buttons, aValue is for analog buttons.
void NewButtonEvent(uint32_t aIndex, uint32_t aButton, bool aPressed,
                    double aValue);
// When only a digital button is available the value will be synthesized.
void NewButtonEvent(uint32_t aIndex, uint32_t aButton, bool aPressed);

// Update the state of |aAxis| for the gamepad at |aIndex| for all
// windows that are listening and visible, and fire a gamepadaxismove
// event at them as well.
void NewAxisMoveEvent(uint32_t aIndex, uint32_t aAxis, double aValue);

// When shutting down the platform communications for gamepad, also reset the
// indexes.
void ResetGamepadIndexes();

} // namespace GamepadFunctions
} // namespace dom
} // namespace mozilla

#endif
