/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_battery_Types_h
#define mozilla_dom_battery_Types_h

namespace mozilla {
namespace hal {
class BatteryInformation;
} // namespace hal

template <class T>
class Observer;

typedef Observer<hal::BatteryInformation> BatteryObserver;

} // namespace mozilla

#endif // mozilla_dom_battery_Types_h

