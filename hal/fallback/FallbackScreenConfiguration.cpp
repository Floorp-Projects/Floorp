/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FallbackScreenConfiguration.h"

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
  fallback::GetCurrentScreenConfiguration(aScreenConfiguration);
}

bool
LockScreenOrientation(const dom::ScreenOrientationInternal& aOrientation)
{
  return false;
}

void
UnlockScreenOrientation()
{
}

} // namespace hal_impl
} // namespace mozilla
