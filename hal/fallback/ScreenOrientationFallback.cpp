/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "nsIScreenManager.h"

namespace mozilla {
namespace hal_impl {

void
EnableScreenOrientationNotifications()
{
}

void
DisableScreenOrientationNotifications()
{
}

void
GetCurrentScreenOrientation(dom::ScreenOrientation* aScreenOrientation)
{
  nsresult result;
  nsCOMPtr<nsIScreenManager> screenMgr =
    do_GetService("@mozilla.org/gfx/screenmanager;1", &result);
  if (NS_FAILED(result)) {
    NS_ERROR("Can't find nsIScreenManager!");
    return;
  }

  PRInt32 screenLeft, screenTop, screenWidth, screenHeight;
  nsCOMPtr<nsIScreen> screen;

  screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
  screen->GetRect(&screenLeft, &screenTop, &screenWidth, &screenHeight);

  *aScreenOrientation = screenWidth >= screenHeight
                          ? dom::eScreenOrientation_LandscapePrimary
                          : dom::eScreenOrientation_PortraitPrimary;
}

} // hal_impl
} // mozilla
