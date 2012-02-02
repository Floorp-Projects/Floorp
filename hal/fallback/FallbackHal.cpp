/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* ***** BEGIN LICENSE BLOCK *****
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *   Jim Straus <jstraus@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "Hal.h"
#include "mozilla/dom/battery/Constants.h"
#include "mozilla/dom/network/Constants.h"

using mozilla::hal::WindowIdentifier;

namespace mozilla {
namespace hal_impl {

void
Vibrate(const nsTArray<uint32>& pattern, const hal::WindowIdentifier &)
{}

void
CancelVibrate(const hal::WindowIdentifier &)
{}

void
EnableBatteryNotifications()
{}

void
DisableBatteryNotifications()
{}

void
GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo)
{
  aBatteryInfo->level() = dom::battery::kDefaultLevel;
  aBatteryInfo->charging() = dom::battery::kDefaultCharging;
  aBatteryInfo->remainingTime() = dom::battery::kDefaultRemainingTime;
}

bool
GetScreenEnabled()
{
  return true;
}

void
SetScreenEnabled(bool enabled)
{}

double
GetScreenBrightness()
{
  return 1;
}

void
SetScreenBrightness(double brightness)
{}

void
EnableNetworkNotifications()
{}

void
DisableNetworkNotifications()
{}

void
GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo)
{
  aNetworkInfo->bandwidth() = dom::network::kDefaultBandwidth;
  aNetworkInfo->canBeMetered() = dom::network::kDefaultCanBeMetered;
}

void
Reboot()
{}

void
PowerOff()
{}

} // hal_impl
} // namespace mozilla
