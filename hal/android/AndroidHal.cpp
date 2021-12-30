/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalImpl.h"
#include "WindowIdentifier.h"
#include "AndroidBridge.h"
#include "mozilla/dom/network/Constants.h"
#include "mozilla/java/GeckoAppShellWrappers.h"
#include "mozilla/java/GeckoRuntimeWrappers.h"
#include "mozilla/widget/ScreenManager.h"
#include "nsPIDOMWindow.h"

using namespace mozilla::dom;
using namespace mozilla::hal;

namespace java = mozilla::java;

namespace mozilla {
namespace hal_impl {

void Vibrate(const nsTArray<uint32_t>& pattern, WindowIdentifier&&) {
  // Ignore the WindowIdentifier parameter; it's here only because hal::Vibrate,
  // hal_sandbox::Vibrate, and hal_impl::Vibrate all must have the same
  // signature.

  // Strangely enough, the Android Java API seems to treat vibrate([0]) as a
  // nop.  But we want to treat vibrate([0]) like CancelVibrate!  (Note that we
  // also need to treat vibrate([]) as a call to CancelVibrate.)
  bool allZero = true;
  for (uint32_t i = 0; i < pattern.Length(); i++) {
    if (pattern[i] != 0) {
      allZero = false;
      break;
    }
  }

  if (allZero) {
    hal_impl::CancelVibrate(WindowIdentifier());
    return;
  }

  AndroidBridge* b = AndroidBridge::Bridge();
  if (!b) {
    return;
  }

  b->Vibrate(pattern);
}

void CancelVibrate(WindowIdentifier&&) {
  // Ignore WindowIdentifier parameter.

  java::GeckoAppShell::CancelVibrate();
}

void EnableBatteryNotifications() {
  java::GeckoAppShell::EnableBatteryNotifications();
}

void DisableBatteryNotifications() {
  java::GeckoAppShell::DisableBatteryNotifications();
}

void GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo) {
  AndroidBridge::Bridge()->GetCurrentBatteryInformation(aBatteryInfo);
}

void EnableNetworkNotifications() {
  java::GeckoAppShell::EnableNetworkNotifications();
}

void DisableNetworkNotifications() {
  java::GeckoAppShell::DisableNetworkNotifications();
}

void GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo) {
  AndroidBridge::Bridge()->GetCurrentNetworkInformation(aNetworkInfo);
}

void EnableScreenConfigurationNotifications() {
  java::GeckoAppShell::EnableScreenOrientationNotifications();
}

void DisableScreenConfigurationNotifications() {
  java::GeckoAppShell::DisableScreenOrientationNotifications();
}

void GetCurrentScreenConfiguration(ScreenConfiguration* aScreenConfiguration) {
  AndroidBridge* bridge = AndroidBridge::Bridge();
  if (!bridge) {
    return;
  }

  RefPtr<widget::Screen> screen =
      widget::ScreenManager::GetSingleton().GetPrimaryScreen();
  *aScreenConfiguration = screen->ToScreenConfiguration();
  aScreenConfiguration->orientation() =
      static_cast<hal::ScreenOrientation>(bridge->GetScreenOrientation());
  aScreenConfiguration->angle() = bridge->GetScreenAngle();
}

RefPtr<MozPromise<bool, bool, false>> LockScreenOrientation(
    const hal::ScreenOrientation& aOrientation) {
  switch (aOrientation) {
    // The Android backend only supports these orientations.
    case eScreenOrientation_PortraitPrimary:
    case eScreenOrientation_PortraitSecondary:
    case eScreenOrientation_PortraitPrimary |
        eScreenOrientation_PortraitSecondary:
    case eScreenOrientation_LandscapePrimary:
    case eScreenOrientation_LandscapeSecondary:
    case eScreenOrientation_LandscapePrimary |
        eScreenOrientation_LandscapeSecondary:
    case eScreenOrientation_Default: {
      java::GeckoRuntime::LocalRef runtime = java::GeckoRuntime::GetInstance();
      if (runtime != NULL) {
        auto result = runtime->LockScreenOrientation(aOrientation);
        auto geckoResult = java::GeckoResult::LocalRef(std::move(result));
        return geckoResult
                   ? MozPromise<bool, bool, false>::FromGeckoResult(geckoResult)
                   : MozPromise<bool, bool, false>::CreateAndReject(false,
                                                                    __func__);
      } else {
        return MozPromise<bool, bool, false>::CreateAndReject(false, __func__);
      }
    }
    default:
      return nullptr;
  }
}

void UnlockScreenOrientation() {
  java::GeckoRuntime::LocalRef runtime = java::GeckoRuntime::GetInstance();
  if (runtime != NULL) {
    runtime->UnlockScreenOrientation();
  }
}

}  // namespace hal_impl
}  // namespace mozilla
