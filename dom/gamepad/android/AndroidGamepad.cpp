/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// TODO: Bug 680289, implement gamepad haptics for Android.
// TODO: Bug 1523355, implement gamepad lighindicator and touch for
// Android.

#include "mozilla/java/AndroidGamepadManagerNatives.h"
#include "mozilla/java/GeckoAppShellWrappers.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

class AndroidGamepadManager final
    : public java::AndroidGamepadManager::Natives<AndroidGamepadManager> {
  AndroidGamepadManager() = delete;

 public:
  static int32_t NativeAddGamepad() {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    MOZ_RELEASE_ASSERT(service);

    const uint32_t gamepadId = service->AddGamepad(
        "android", GamepadMappingType::Standard, GamepadHand::_empty,
        kStandardGamepadButtons, kStandardGamepadAxes, 0, 0, 0);

    MOZ_RELEASE_ASSERT(gamepadId <= INT32_MAX);

    return static_cast<int32_t>(gamepadId);
  }

  static void NativeRemoveGamepad(int32_t aGamepadId) {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    service->RemoveGamepad(aGamepadId);
  }

  static void OnButtonChange(int32_t aGamepadId, int32_t aButton, bool aPressed,
                             float aValue) {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    service->NewButtonEvent(aGamepadId, aButton, aPressed, aValue);
  }

  static void OnAxisChange(int32_t aGamepadId, jni::BooleanArray::Param aValid,
                           jni::FloatArray::Param aValues) {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    const auto& valid = aValid->GetElements();
    const auto& values = aValues->GetElements();
    MOZ_ASSERT(valid.Length() == values.Length());

    for (size_t i = 0; i < values.Length(); i++) {
      if (valid[i]) {
        service->NewAxisMoveEvent(aGamepadId, i, values[i]);
      }
    }
  }
};

void StartGamepadMonitoring() {
  AndroidGamepadManager::Init();
  java::AndroidGamepadManager::Start(
      java::GeckoAppShell::GetApplicationContext());
}

void StopGamepadMonitoring() {
  java::AndroidGamepadManager::Stop(
      java::GeckoAppShell::GetApplicationContext());
}

void SetGamepadLightIndicatorColor(uint32_t aControllerIdx,
                                   uint32_t aLightColorIndex, uint8_t aRed,
                                   uint8_t aGreen, uint8_t aBlue) {
  NS_WARNING("Android doesn't support gamepad light indicator.");
}

}  // namespace dom
}  // namespace mozilla
