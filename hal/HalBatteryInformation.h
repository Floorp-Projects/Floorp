/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HalBatteryInformation_h
#define mozilla_HalBatteryInformation_h

#include "mozilla/Observer.h"

namespace mozilla {
namespace hal {
class BatteryInformation;
typedef Observer<hal::BatteryInformation> BatteryObserver;
} // namespace hal
} // namespace mozilla

#endif // mozilla_HalBatteryInformation_h

