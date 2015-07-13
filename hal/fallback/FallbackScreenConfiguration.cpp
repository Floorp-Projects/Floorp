/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "nsIScreenManager.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace hal_impl {

void
EnableScreenConfigurationNotifications()
{
}

void
DisableScreenConfigurationNotifications()
{
}

void
GetCurrentScreenConfiguration(hal::ScreenConfiguration* aScreenConfiguration)
{
  nsresult rv;
  nsCOMPtr<nsIScreenManager> screenMgr =
    do_GetService("@mozilla.org/gfx/screenmanager;1", &rv);
  if (NS_FAILED(rv)) {
    NS_ERROR("Can't find nsIScreenManager!");
    return;
  }

  nsIntRect rect;
  int32_t colorDepth, pixelDepth;
  dom::ScreenOrientation orientation;
  nsCOMPtr<nsIScreen> screen;

  screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
  screen->GetRect(&rect.x, &rect.y, &rect.width, &rect.height);
  screen->GetColorDepth(&colorDepth);
  screen->GetPixelDepth(&pixelDepth);
  orientation = rect.width >= rect.height
                ? dom::eScreenOrientation_LandscapePrimary
                : dom::eScreenOrientation_PortraitPrimary;

  *aScreenConfiguration =
      hal::ScreenConfiguration(rect, orientation, colorDepth, pixelDepth);
}

bool
LockScreenOrientation(const dom::ScreenOrientation& aOrientation)
{
  return false;
}

void
UnlockScreenOrientation()
{
}

} // namespace hal_impl
} // namespace mozilla
