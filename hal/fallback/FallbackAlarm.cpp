/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

namespace mozilla {
namespace hal_impl {

bool
EnableAlarm()
{
  return false;
}

void
DisableAlarm()
{
}

bool
SetAlarm(long aSeconds, long aNanoseconds)
{
  return false;
}

} // hal_impl
} // namespace mozilla
