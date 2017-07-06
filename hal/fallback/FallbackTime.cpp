/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

namespace mozilla {
namespace hal_impl {

void
AdjustSystemClock(int64_t aDeltaMilliseconds)
{}

void
SetTimezone(const nsCString& aTimezoneSpec)
{}

nsCString
GetTimezone()
{
  return EmptyCString();
}

int32_t
GetTimezoneOffset()
{
  return 0;
}

void
EnableSystemClockChangeNotifications()
{
}

void
DisableSystemClockChangeNotifications()
{
}

void
EnableSystemTimezoneChangeNotifications()
{
}

void
DisableSystemTimezoneChangeNotifications()
{
}

} // namespace hal_impl
} // namespace mozilla
