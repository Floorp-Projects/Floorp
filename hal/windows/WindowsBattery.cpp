/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalImpl.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/battery/Constants.h"

#include <windows.h>

using namespace mozilla::dom::battery;

namespace mozilla {
namespace hal_impl {

static HPOWERNOTIFY sPowerHandle = nullptr;
static HPOWERNOTIFY sCapacityHandle = nullptr;
static HWND sHWnd = nullptr;

static
LRESULT CALLBACK
BatteryWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg != WM_POWERBROADCAST || wParam != PBT_POWERSETTINGCHANGE) {
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }

  hal::BatteryInformation currentInfo;

  // Since we need update remainingTime, we cannot use LPARAM.
  hal_impl::GetCurrentBatteryInformation(&currentInfo);

  hal::NotifyBatteryChange(currentInfo);
  return TRUE;
}

void
EnableBatteryNotifications()
{
  // Create custom window to watch battery event
  // If we can get Gecko's window handle, this is unnecessary.

  if (sHWnd == nullptr) {
    WNDCLASSW wc;
    HMODULE hSelf = GetModuleHandle(nullptr);

    if (!GetClassInfoW(hSelf, L"MozillaBatteryClass", &wc)) {
      ZeroMemory(&wc, sizeof(WNDCLASSW));
      wc.hInstance = hSelf;
      wc.lpfnWndProc = BatteryWindowProc;
      wc.lpszClassName = L"MozillaBatteryClass";
      RegisterClassW(&wc);
    }

    sHWnd = CreateWindowW(L"MozillaBatteryClass", L"Battery Watcher",
                          0, 0, 0, 0, 0,
                          nullptr, nullptr, hSelf, nullptr);
  }

  if (sHWnd == nullptr) {
    return;
  }

  sPowerHandle =
    RegisterPowerSettingNotification(sHWnd,
                                     &GUID_ACDC_POWER_SOURCE,
                                     DEVICE_NOTIFY_WINDOW_HANDLE);
  sCapacityHandle =
    RegisterPowerSettingNotification(sHWnd,
                                     &GUID_BATTERY_PERCENTAGE_REMAINING,
                                     DEVICE_NOTIFY_WINDOW_HANDLE);
}

void
DisableBatteryNotifications()
{
  if (sPowerHandle) {
    UnregisterPowerSettingNotification(sPowerHandle);
    sPowerHandle = nullptr;
  }

  if (sCapacityHandle) {
    UnregisterPowerSettingNotification(sCapacityHandle);
    sCapacityHandle = nullptr;
  }

  if (sHWnd) {
    DestroyWindow(sHWnd);
    sHWnd = nullptr;
  }
}

void
GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo)
{
  SYSTEM_POWER_STATUS status;
  if (!GetSystemPowerStatus(&status)) {
    aBatteryInfo->level() = kDefaultLevel;
    aBatteryInfo->charging() = kDefaultCharging;
    aBatteryInfo->remainingTime() = kDefaultRemainingTime;
    return;
  }

  aBatteryInfo->level() =
    status.BatteryLifePercent == 255 ? kDefaultLevel
                                     : ((double)status.BatteryLifePercent) / 100.0;
  aBatteryInfo->charging() = (status.ACLineStatus != 0);
  if (status.ACLineStatus != 0) {
    if (aBatteryInfo->level() == 1.0) {
      // GetSystemPowerStatus API may returns -1 for BatteryFullLifeTime.
      // So, if battery is 100%, set kDefaultRemainingTime at force.
      aBatteryInfo->remainingTime() = kDefaultRemainingTime;
    } else {
      aBatteryInfo->remainingTime() =
        status.BatteryFullLifeTime == (DWORD)-1 ? kUnknownRemainingTime
                                                : status.BatteryFullLifeTime;
    }
  } else {
    aBatteryInfo->remainingTime() =
      status.BatteryLifeTime == (DWORD)-1 ? kUnknownRemainingTime
                                          : status.BatteryLifeTime;
  }
}

} // hal_impl
} // mozilla
