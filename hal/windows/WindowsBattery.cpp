/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Makoto Kato <m_kato@ga2.so-net.ne.jp> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "Hal.h"
#include "HalImpl.h"
#include "nsITimer.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/battery/Constants.h"

#include <windows.h>

using namespace mozilla::dom::battery;

namespace mozilla {
namespace hal_impl {

static nsCOMPtr<nsITimer> sUpdateTimer;

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
/* Power Event API is Vista or later */
typedef HPOWERNOTIFY (WINAPI *REGISTERPOWERSETTINGNOTIFICATION) (HANDLE, LPCGUID, DWORD);
typedef BOOL (WINAPI *UNREGISTERPOWERSETTINGNOTIFICATION) (HPOWERNOTIFY);
static REGISTERPOWERSETTINGNOTIFICATION sRegisterPowerSettingNotification = nsnull;
static UNREGISTERPOWERSETTINGNOTIFICATION sUnregisterPowerSettingNotification = nsnull;
static HPOWERNOTIFY sPowerHandle = nsnull;
static HPOWERNOTIFY sCapacityHandle = nsnull;
static HWND sHWnd = nsnull;
#endif


static bool
IsVistaOrLater()
{
  OSVERSIONINFO info;

  ZeroMemory(&info, sizeof(OSVERSIONINFO));
  info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&info);

  return info.dwMajorVersion >= 6;
}

static void
UpdateHandler(nsITimer* aTimer, void* aClosure) {
  NS_ASSERTION(!IsVistaOrLater(),
               "We shouldn't call this function for Vista or later version!");

  static hal::BatteryInformation sLastInfo;
  hal::BatteryInformation currentInfo;

  hal_impl::GetCurrentBatteryInformation(&currentInfo);
  if (sLastInfo.level() != currentInfo.level() ||
      sLastInfo.charging() != currentInfo.charging() ||
      sLastInfo.remainingTime() != currentInfo.remainingTime()) {
    hal::NotifyBatteryChange(currentInfo);
    sLastInfo = currentInfo;
  }
}

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
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
#endif

void
EnableBatteryNotifications()
{
#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
  if (IsVistaOrLater()) {
    // RegisterPowerSettingNotification is from Vista or later.
    // Use this API if available.
    HMODULE hUser32 = GetModuleHandleW(L"USER32.DLL");
    if (!sRegisterPowerSettingNotification)
      sRegisterPowerSettingNotification = (REGISTERPOWERSETTINGNOTIFICATION)
        GetProcAddress(hUser32, "RegisterPowerSettingNotification");
    if (!sUnregisterPowerSettingNotification)
      sUnregisterPowerSettingNotification = (UNREGISTERPOWERSETTINGNOTIFICATION)
        GetProcAddress(hUser32, "UnregisterPowerSettingNotification");

    if (!sRegisterPowerSettingNotification ||
        !sUnregisterPowerSettingNotification) {
      NS_ASSERTION(false, "Canot find PowerSettingNotification functions.");
      return;
    }

    // Create custom window to watch battery event
    // If we can get Gecko's window handle, this is unnecessary.

    if (sHWnd == nsnull) {
      WNDCLASSW wc;
      HMODULE hSelf = GetModuleHandle(nsnull);

      if (!GetClassInfoW(hSelf, L"MozillaBatteryClass", &wc)) {
        ZeroMemory(&wc, sizeof(WNDCLASSW));
        wc.hInstance = hSelf;
        wc.lpfnWndProc = BatteryWindowProc;
        wc.lpszClassName = L"MozillaBatteryClass";
        RegisterClassW(&wc);
      }

      sHWnd = CreateWindowW(L"MozillaBatteryClass", L"Battery Watcher",
                            0, 0, 0, 0, 0,
                            nsnull, nsnull, hSelf, nsnull);
    }

    if (sHWnd == nsnull) {
      return;
    }

    sPowerHandle =
      sRegisterPowerSettingNotification(sHWnd,
                                        &GUID_ACDC_POWER_SOURCE,
                                        DEVICE_NOTIFY_WINDOW_HANDLE);
    sCapacityHandle =
      sRegisterPowerSettingNotification(sHWnd,
                                        &GUID_BATTERY_PERCENTAGE_REMAINING,
                                        DEVICE_NOTIFY_WINDOW_HANDLE);
  } else
#endif
  {
    // for Windows 2000 and Windwos XP.  If we remove Windows XP support,
    // we should remove timer-based power notification
    sUpdateTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (sUpdateTimer) {
      sUpdateTimer->InitWithFuncCallback(UpdateHandler,
                                         nsnull,
                                         Preferences::GetInt("dom.battery.timer",
                                                             30000 /* 30s */),
                                         nsITimer::TYPE_REPEATING_SLACK);
    } 
  }
}

void
DisableBatteryNotifications()
{
#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
  if (IsVistaOrLater()) {
    if (sPowerHandle) {
      sUnregisterPowerSettingNotification(sPowerHandle);
      sPowerHandle = nsnull;
    }

    if (sCapacityHandle) {
      sUnregisterPowerSettingNotification(sCapacityHandle);
      sCapacityHandle = nsnull;
    }

    if (sHWnd) {
      DestroyWindow(sHWnd);
      sHWnd = nsnull;
    }
  } else
#endif
  {
    if (sUpdateTimer) {
      sUpdateTimer->Cancel();
      sUpdateTimer = nsnull;
    }
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
