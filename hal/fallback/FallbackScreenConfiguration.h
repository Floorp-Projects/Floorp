/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_fallback_FallbackScreenConfiguration_h
#define mozilla_fallback_FallbackScreenConfiguration_h

#include "Hal.h"
#include "mozilla/widget/ScreenManager.h"

namespace mozilla {
namespace fallback {

inline void GetCurrentScreenConfiguration(
    hal::ScreenConfiguration* aScreenConfiguration) {
  RefPtr<widget::Screen> screen =
      widget::ScreenManager::GetSingleton().GetPrimaryScreen();

  *aScreenConfiguration = screen->ToScreenConfiguration();
  aScreenConfiguration->orientation() =
      aScreenConfiguration->rect().Width() >=
              aScreenConfiguration->rect().Height()
          ? hal::eScreenOrientation_LandscapePrimary
          : hal::eScreenOrientation_PortraitPrimary;
}

}  // namespace fallback
}  // namespace mozilla

#endif  // mozilla_fallback_FallbackScreenConfiguration_h
