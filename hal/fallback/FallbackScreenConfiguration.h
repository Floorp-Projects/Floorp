/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_fallback_FallbackScreenConfiguration_h
#define mozilla_fallback_FallbackScreenConfiguration_h

#include "Hal.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "nsIScreenManager.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace fallback {

inline void
GetCurrentScreenConfiguration(hal::ScreenConfiguration* aScreenConfiguration)
{
  nsresult rv;
  nsCOMPtr<nsIScreenManager> screenMgr =
    do_GetService("@mozilla.org/gfx/screenmanager;1", &rv);
  if (NS_FAILED(rv)) {
    NS_ERROR("Can't find nsIScreenManager!");
    return;
  }

  int32_t colorDepth, pixelDepth, x, y, w, h;
  dom::ScreenOrientationInternal orientation;
  nsCOMPtr<nsIScreen> screen;

  screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
  screen->GetRect(&x, &y, &w, &h);
  screen->GetColorDepth(&colorDepth);
  screen->GetPixelDepth(&pixelDepth);
  orientation = w >= h
                ? dom::eScreenOrientation_LandscapePrimary
                : dom::eScreenOrientation_PortraitPrimary;

  *aScreenConfiguration = hal::ScreenConfiguration(nsIntRect(x, y, w, h),
						   orientation, 0,
						   colorDepth, pixelDepth);
}

}
}

#endif // mozilla_fallback_FallbackScreenConfiguration_h
