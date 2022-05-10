/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GamepadMonitoring_h_
#define mozilla_dom_GamepadMonitoring_h_
#include "mozilla/dom/GamepadHandle.h"

namespace mozilla::dom {

// These two functions are implemented in the platform specific service files
// (linux/LinuxGamepad.cpp, cocoa/CocoaGamepad.cpp, etc)
void StartGamepadMonitoring();
void StopGamepadMonitoring();
void SetGamepadLightIndicatorColor(const Tainted<GamepadHandle>& aGamepadHandle,
                                   const Tainted<uint32_t>& aLightColorIndex,
                                   const uint8_t& aRed, const uint8_t& aGreen,
                                   const uint8_t& aBlue);

}  // namespace mozilla::dom

#endif
