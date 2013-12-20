// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor.h"

namespace base {

void SystemMonitor::ProcessWmPowerBroadcastMessage(int event_id) {
  PowerEvent power_event;
  switch (event_id) {
    case PBT_APMPOWERSTATUSCHANGE:  // The power status changed.
      power_event = POWER_STATE_EVENT;
      break;
    case PBT_APMRESUMEAUTOMATIC:  // Non-user initiated resume from suspend.
    case PBT_APMRESUMESUSPEND:    // User initiated resume from suspend.
      power_event = RESUME_EVENT;
      break;
    case PBT_APMSUSPEND:  // System has been suspended.
      power_event = SUSPEND_EVENT;
      break;
    default:
      return;

    // Other Power Events:
    // PBT_APMBATTERYLOW - removed in Vista.
    // PBT_APMOEMEVENT - removed in Vista.
    // PBT_APMQUERYSUSPEND - removed in Vista.
    // PBT_APMQUERYSUSPENDFAILED - removed in Vista.
    // PBT_APMRESUMECRITICAL - removed in Vista.
    // PBT_POWERSETTINGCHANGE - user changed the power settings.
  }
  ProcessPowerMessage(power_event);
}

// Function to query the system to see if it is currently running on
// battery power.  Returns true if running on battery.
bool SystemMonitor::IsBatteryPower() {
  SYSTEM_POWER_STATUS status;
  if (!GetSystemPowerStatus(&status)) {
    CHROMIUM_LOG(ERROR) << "GetSystemPowerStatus failed: " << GetLastError();
    return false;
  }
  return (status.ACLineStatus == 0);
}



} // namespace base
