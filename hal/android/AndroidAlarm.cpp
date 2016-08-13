/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

#include "GeneratedJNINatives.h"
#include "GeneratedJNIWrappers.h"

using namespace mozilla::hal;

namespace mozilla {

class AlarmReceiver : public java::AlarmReceiver::Natives<AlarmReceiver>
{
private:
    AlarmReceiver();

public:
    static void NotifyAlarmFired() {
        hal::NotifyAlarmFired();
    }
};

namespace hal_impl {

bool
EnableAlarm()
{
    AlarmReceiver::Init();
    return true;
}

void
DisableAlarm()
{
    java::GeckoAppShell::DisableAlarm();
}

bool
SetAlarm(int32_t aSeconds, int32_t aNanoseconds)
{
    return java::GeckoAppShell::SetAlarm(aSeconds, aNanoseconds);
}

} // hal_impl
} // mozilla
