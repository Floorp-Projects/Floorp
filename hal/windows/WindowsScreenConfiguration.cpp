/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "mozilla/widget/ScreenManager.h"
#include "nsIWindowsUIUtils.h"
#include "WinUtils.h"

#include <windows.h>

namespace mozilla {
namespace hal_impl {

static decltype(SetDisplayAutoRotationPreferences)*
    sSetDisplayAutoRotationPreferences = nullptr;

RefPtr<GenericNonExclusivePromise> LockScreenOrientation(
    const hal::ScreenOrientation& aOrientation) {
  AR_STATE state;
  if (!widget::WinUtils::GetAutoRotationState(&state)) {
    return GenericNonExclusivePromise::CreateAndReject(
        NS_ERROR_DOM_NOT_SUPPORTED_ERR, __func__);
  }

  if (state & (AR_DISABLED | AR_REMOTESESSION | AR_MULTIMON | AR_NOSENSOR |
               AR_NOT_SUPPORTED | AR_LAPTOP | AR_DOCKED)) {
    return GenericNonExclusivePromise::CreateAndReject(
        NS_ERROR_DOM_NOT_SUPPORTED_ERR, __func__);
  }

  if (!sSetDisplayAutoRotationPreferences) {
    HMODULE user32dll = GetModuleHandleW(L"user32.dll");
    if (user32dll) {
      sSetDisplayAutoRotationPreferences =
          (decltype(SetDisplayAutoRotationPreferences)*)GetProcAddress(
              user32dll, "SetDisplayAutoRotationPreferences");
    }
    if (!sSetDisplayAutoRotationPreferences) {
      return GenericNonExclusivePromise::CreateAndReject(
          NS_ERROR_DOM_NOT_SUPPORTED_ERR, __func__);
    }
  }

  ORIENTATION_PREFERENCE orientation = ORIENTATION_PREFERENCE_NONE;

  if (aOrientation == hal::ScreenOrientation::Default) {
    // Actually, current screen is single and tablet mode according to
    // GetAutoRotationState. So get primary screen data for natural orientation.
    RefPtr<widget::Screen> screen =
        widget::ScreenManager::GetSingleton().GetPrimaryScreen();
    hal::ScreenOrientation defaultOrientation =
        screen->GetDefaultOrientationType();
    if (defaultOrientation == hal::ScreenOrientation::LandscapePrimary) {
      orientation = ORIENTATION_PREFERENCE_LANDSCAPE;
    } else {
      orientation = ORIENTATION_PREFERENCE_PORTRAIT;
    }
  } else {
    if (aOrientation & hal::ScreenOrientation::LandscapePrimary) {
      orientation |= ORIENTATION_PREFERENCE_LANDSCAPE;
    }
    if (aOrientation & hal::ScreenOrientation::LandscapeSecondary) {
      orientation |= ORIENTATION_PREFERENCE_LANDSCAPE_FLIPPED;
    }
    if (aOrientation & hal::ScreenOrientation::PortraitPrimary) {
      orientation |= ORIENTATION_PREFERENCE_PORTRAIT;
    }
    if (aOrientation & hal::ScreenOrientation::PortraitSecondary) {
      orientation |= ORIENTATION_PREFERENCE_PORTRAIT_FLIPPED;
    }
  }

  if (!sSetDisplayAutoRotationPreferences(orientation)) {
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_DOM_ABORT_ERR,
                                                       __func__);
  }

  return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
}

void UnlockScreenOrientation() {
  if (!sSetDisplayAutoRotationPreferences) {
    return;
  }
  // This does nothing if the device doesn't support orientation lock
  sSetDisplayAutoRotationPreferences(ORIENTATION_PREFERENCE_NONE);
}

}  // namespace hal_impl
}  // namespace mozilla
