/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_battery_Constants_h__
#define mozilla_dom_battery_Constants_h__

/**
 * A set of constants that might need to be used by battery backends.
 * It's not part of BatteryManager.h to prevent those backends to include it.
 */
namespace mozilla {
namespace dom {
namespace battery {

  static const double kDefaultLevel         = 1.0;
  static const bool   kDefaultCharging      = true;
  static const double kDefaultRemainingTime = 0;
  static const double kUnknownRemainingTime = -1;

} // namespace battery
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_battery_Constants_h__
