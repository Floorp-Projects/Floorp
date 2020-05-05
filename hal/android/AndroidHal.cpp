/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalImpl.h"
#include "WindowIdentifier.h"
#include "AndroidBridge.h"
#include "mozilla/dom/network/Constants.h"
#include "nsIScreenManager.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"

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

  nsresult rv;
  nsCOMPtr<nsIScreenManager> screenMgr =
      do_GetService("@mozilla.org/gfx/screenmanager;1", &rv);
  if (NS_FAILED(rv)) {
    NS_ERROR("Can't find nsIScreenManager!");
    return;
  }

  int32_t colorDepth, pixelDepth;
  int16_t angle;
  hal::ScreenOrientation orientation;
  nsCOMPtr<nsIScreen> screen;

  int32_t rectX, rectY, rectWidth, rectHeight;

  screenMgr->GetPrimaryScreen(getter_AddRefs(screen));

  screen->GetRect(&rectX, &rectY, &rectWidth, &rectHeight);
  screen->GetColorDepth(&colorDepth);
  screen->GetPixelDepth(&pixelDepth);
  orientation =
      static_cast<hal::ScreenOrientation>(bridge->GetScreenOrientation());
  angle = bridge->GetScreenAngle();

  *aScreenConfiguration =
      hal::ScreenConfiguration(nsIntRect(rectX, rectY, rectWidth, rectHeight),
                               orientation, angle, colorDepth, pixelDepth);
}

bool LockScreenOrientation(const hal::ScreenOrientation& aOrientation) {
  // Force the default orientation to be portrait-primary.
  hal::ScreenOrientation orientation =
      aOrientation == eScreenOrientation_Default
          ? eScreenOrientation_PortraitPrimary
          : aOrientation;

  switch (orientation) {
    // The Android backend only supports these orientations.
    case eScreenOrientation_PortraitPrimary:
    case eScreenOrientation_PortraitSecondary:
    case eScreenOrientation_PortraitPrimary |
        eScreenOrientation_PortraitSecondary:
    case eScreenOrientation_LandscapePrimary:
    case eScreenOrientation_LandscapeSecondary:
    case eScreenOrientation_LandscapePrimary |
        eScreenOrientation_LandscapeSecondary:
    case eScreenOrientation_Default:
      java::GeckoAppShell::LockScreenOrientation(orientation);
      return true;
    default:
      return false;
  }
}

void UnlockScreenOrientation() {
  java::GeckoAppShell::UnlockScreenOrientation();
}

}  // namespace hal_impl
}  // namespace mozilla
