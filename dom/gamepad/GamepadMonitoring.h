/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GamepadMonitoring_h_
#define mozilla_dom_GamepadMonitoring_h_

namespace mozilla {
namespace dom {
// Functions for platform specific gamepad monitoring.

void MaybeStopGamepadMonitoring();

// These two functions are implemented in the platform specific service files
// (linux/LinuxGamepad.cpp, cocoa/CocoaGamepad.cpp, etc)
void StartGamepadMonitoring();
void StopGamepadMonitoring();

} // namespace dom
} // namespace mozilla

#endif
