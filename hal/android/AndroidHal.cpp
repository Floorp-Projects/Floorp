/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalImpl.h"
#include "WindowIdentifier.h"
#include "AndroidBridge.h"
#include "mozilla/dom/network/Constants.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "nsIScreenManager.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::dom;
using namespace mozilla::hal;

namespace mozilla {
namespace hal_impl {

void
Vibrate(const nsTArray<uint32_t> &pattern, const WindowIdentifier &)
{
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

void
CancelVibrate(const WindowIdentifier &)
{
  // Ignore WindowIdentifier parameter.

  AndroidBridge* b = AndroidBridge::Bridge();
  if (b)
    b->CancelVibrate();
}

void
EnableBatteryNotifications()
{
  AndroidBridge* bridge = AndroidBridge::Bridge();
  if (!bridge) {
    return;
  }

  bridge->EnableBatteryNotifications();
}

void
DisableBatteryNotifications()
{
  AndroidBridge* bridge = AndroidBridge::Bridge();
  if (!bridge) {
    return;
  }

  bridge->DisableBatteryNotifications();
}

void
GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo)
{
  AndroidBridge* bridge = AndroidBridge::Bridge();
  if (!bridge) {
    return;
  }

  bridge->GetCurrentBatteryInformation(aBatteryInfo);
}

void
EnableNetworkNotifications()
{
  AndroidBridge* bridge = AndroidBridge::Bridge();
  if (!bridge) {
    return;
  }

  bridge->EnableNetworkNotifications();
}

void
DisableNetworkNotifications()
{
  AndroidBridge* bridge = AndroidBridge::Bridge();
  if (!bridge) {
    return;
  }

  bridge->DisableNetworkNotifications();
}

void
GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo)
{
  AndroidBridge* bridge = AndroidBridge::Bridge();
  if (!bridge) {
    return;
  }

  bridge->GetCurrentNetworkInformation(aNetworkInfo);
}

void
EnableScreenConfigurationNotifications()
{
  AndroidBridge* bridge = AndroidBridge::Bridge();
  if (!bridge) {
    return;
  }

  bridge->EnableScreenOrientationNotifications();
}

void
DisableScreenConfigurationNotifications()
{
  AndroidBridge* bridge = AndroidBridge::Bridge();
  if (!bridge) {
    return;
  }

  bridge->DisableScreenOrientationNotifications();
}

void
GetCurrentScreenConfiguration(ScreenConfiguration* aScreenConfiguration)
{
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

  nsIntRect rect;
  int32_t colorDepth, pixelDepth;
  ScreenOrientation orientation;
  nsCOMPtr<nsIScreen> screen;

  screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
  screen->GetRect(&rect.x, &rect.y, &rect.width, &rect.height);
  screen->GetColorDepth(&colorDepth);
  screen->GetPixelDepth(&pixelDepth);
  orientation = static_cast<ScreenOrientation>(bridge->GetScreenOrientation());

  *aScreenConfiguration =
    hal::ScreenConfiguration(rect, orientation, colorDepth, pixelDepth);
}

bool
LockScreenOrientation(const ScreenOrientation& aOrientation)
{
  AndroidBridge* bridge = AndroidBridge::Bridge();
  if (!bridge) {
    return false;
  }

  switch (aOrientation) {
    // The Android backend only supports these orientations.
    case eScreenOrientation_PortraitPrimary:
    case eScreenOrientation_PortraitSecondary:
    case eScreenOrientation_PortraitPrimary | eScreenOrientation_PortraitSecondary:
    case eScreenOrientation_LandscapePrimary:
    case eScreenOrientation_LandscapeSecondary:
    case eScreenOrientation_LandscapePrimary | eScreenOrientation_LandscapeSecondary:
    case eScreenOrientation_Default:
      bridge->LockScreenOrientation(aOrientation);
      return true;
    default:
      return false;
  }
}

void
UnlockScreenOrientation()
{
  AndroidBridge* bridge = AndroidBridge::Bridge();
  if (!bridge) {
    return;
  }

  bridge->UnlockScreenOrientation();
}

} // hal_impl
} // mozilla

