/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/hal_sandbox/PHalChild.h"
#include "mozilla/hal_sandbox/PHalParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/battery/Types.h"
#include "mozilla/dom/network/Types.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/Observer.h"
#include "mozilla/unused.h"
#include "WindowIdentifier.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

namespace mozilla {
namespace hal_sandbox {

static PHalChild* sHal;
static PHalChild*
Hal()
{
  if (!sHal) {
    sHal = ContentChild::GetSingleton()->SendPHalConstructor();
  }
  return sHal;
}

void
Vibrate(const nsTArray<uint32>& pattern, const WindowIdentifier &id)
{
  HAL_LOG(("Vibrate: Sending to parent process."));

  AutoInfallibleTArray<uint32, 8> p(pattern);

  WindowIdentifier newID(id);
  newID.AppendProcessID();
  Hal()->SendVibrate(p, newID.AsArray(), GetTabChildFrom(newID.GetWindow()));
}

void
CancelVibrate(const WindowIdentifier &id)
{
  HAL_LOG(("CancelVibrate: Sending to parent process."));

  WindowIdentifier newID(id);
  newID.AppendProcessID();
  Hal()->SendCancelVibrate(newID.AsArray(), GetTabChildFrom(newID.GetWindow()));
}

void
EnableBatteryNotifications()
{
  Hal()->SendEnableBatteryNotifications();
}

void
DisableBatteryNotifications()
{
  Hal()->SendDisableBatteryNotifications();
}

void
GetCurrentBatteryInformation(BatteryInformation* aBatteryInfo)
{
  Hal()->SendGetCurrentBatteryInformation(aBatteryInfo);
}

void
EnableNetworkNotifications()
{
  Hal()->SendEnableNetworkNotifications();
}

void
DisableNetworkNotifications()
{
  Hal()->SendDisableNetworkNotifications();
}

void
GetCurrentNetworkInformation(NetworkInformation* aNetworkInfo)
{
  Hal()->SendGetCurrentNetworkInformation(aNetworkInfo);
}

void
EnableScreenOrientationNotifications()
{
  Hal()->SendEnableScreenOrientationNotifications();
}

void
DisableScreenOrientationNotifications()
{
  Hal()->SendDisableScreenOrientationNotifications();
}

void
GetCurrentScreenOrientation(ScreenOrientation* aScreenOrientation)
{
  Hal()->SendGetCurrentScreenOrientation(aScreenOrientation);
}

bool
LockScreenOrientation(const dom::ScreenOrientation& aOrientation)
{
  bool allowed;
  Hal()->SendLockScreenOrientation(aOrientation, &allowed);
  return allowed;
}

void
UnlockScreenOrientation()
{
  Hal()->SendUnlockScreenOrientation();
}

bool
GetScreenEnabled()
{
  bool enabled = false;
  Hal()->SendGetScreenEnabled(&enabled);
  return enabled;
}

void
SetScreenEnabled(bool enabled)
{
  Hal()->SendSetScreenEnabled(enabled);
}

bool
GetCpuSleepAllowed()
{
  bool allowed = true;
  Hal()->SendGetCpuSleepAllowed(&allowed);
  return allowed;
}

void
SetCpuSleepAllowed(bool allowed)
{
  Hal()->SendSetCpuSleepAllowed(allowed);
}

double
GetScreenBrightness()
{
  double brightness = 0;
  Hal()->SendGetScreenBrightness(&brightness);
  return brightness;
}

void
SetScreenBrightness(double brightness)
{
  Hal()->SendSetScreenBrightness(brightness);
}

bool
SetLight(hal::LightType light, const hal::LightConfiguration& aConfig)
{
  bool status;
  Hal()->SendSetLight(light, aConfig, &status);
  return status;
}

bool
GetLight(hal::LightType light, hal::LightConfiguration* aConfig)
{
  bool status;
  Hal()->SendGetLight(light, aConfig, &status);
  return status;
}

void 
AdjustSystemClock(int32_t aDeltaMilliseconds)
{
  Hal()->SendAdjustSystemClock(aDeltaMilliseconds);
}

void
SetTimezone(const nsCString& aTimezoneSpec)
{
  Hal()->SendSetTimezone(nsCString(aTimezoneSpec));
} 

void
Reboot()
{
  Hal()->SendReboot();
}

void
PowerOff()
{
  Hal()->SendPowerOff();
}

void
EnableSensorNotifications(SensorType aSensor) {
  Hal()->SendEnableSensorNotifications(aSensor);
}

void
DisableSensorNotifications(SensorType aSensor) {
  Hal()->SendDisableSensorNotifications(aSensor);
}

void
EnableWakeLockNotifications()
{
  Hal()->SendEnableWakeLockNotifications();
}

void
DisableWakeLockNotifications()
{
  Hal()->SendDisableWakeLockNotifications();
}

void
ModifyWakeLock(const nsAString &aTopic, WakeLockControl aLockAdjust, WakeLockControl aHiddenAdjust)
{
  Hal()->SendModifyWakeLock(nsString(aTopic), aLockAdjust, aHiddenAdjust);
}

void
GetWakeLockInfo(const nsAString &aTopic, WakeLockInformation *aWakeLockInfo)
{
  Hal()->SendGetWakeLockInfo(nsString(aTopic), aWakeLockInfo);
}

void
EnableSwitchNotifications(SwitchDevice aDevice)
{
  Hal()->SendEnableSwitchNotifications(aDevice);
}

void
DisableSwitchNotifications(SwitchDevice aDevice)
{
  Hal()->SendDisableSwitchNotifications(aDevice);
}

SwitchState
GetCurrentSwitchState(SwitchDevice aDevice)
{
  SwitchState state;
  Hal()->SendGetCurrentSwitchState(aDevice, &state);
  return state;
}

class HalParent : public PHalParent
                , public BatteryObserver
                , public NetworkObserver
                , public ISensorObserver
                , public WakeLockObserver
                , public ScreenOrientationObserver
                , public SwitchObserver
{
public:
  NS_OVERRIDE virtual bool
  RecvVibrate(const InfallibleTArray<unsigned int>& pattern,
              const InfallibleTArray<uint64> &id,
              PBrowserParent *browserParent)
  {
    // Check whether browserParent is active.  We should have already
    // checked that the corresponding window is active, but this check
    // isn't redundant.  A window may be inactive in an active
    // browser.  And a window is not notified synchronously when it's
    // deactivated, so the window may think it's active when the tab
    // is actually inactive.
    TabParent *tabParent = static_cast<TabParent*>(browserParent);
    if (!tabParent->Active()) {
      HAL_LOG(("RecvVibrate: Tab is not active. Cancelling."));
      return true;
    }

    // Forward to hal::, not hal_impl::, because we might be a
    // subprocess of another sandboxed process.  The hal:: entry point
    // will do the right thing.
    nsCOMPtr<nsIDOMWindow> window =
      do_QueryInterface(tabParent->GetBrowserDOMWindow());
    WindowIdentifier newID(id, window);
    hal::Vibrate(pattern, newID);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvCancelVibrate(const InfallibleTArray<uint64> &id,
                    PBrowserParent *browserParent)
  {
    TabParent *tabParent = static_cast<TabParent*>(browserParent);
    nsCOMPtr<nsIDOMWindow> window =
      do_QueryInterface(tabParent->GetBrowserDOMWindow());
    WindowIdentifier newID(id, window);
    hal::CancelVibrate(newID);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvEnableBatteryNotifications() {
    hal::RegisterBatteryObserver(this);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvDisableBatteryNotifications() {
    hal::UnregisterBatteryObserver(this);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvGetCurrentBatteryInformation(BatteryInformation* aBatteryInfo) {
    hal::GetCurrentBatteryInformation(aBatteryInfo);
    return true;
  }

  void Notify(const BatteryInformation& aBatteryInfo) {
    unused << SendNotifyBatteryChange(aBatteryInfo);
  }

  NS_OVERRIDE virtual bool
  RecvEnableNetworkNotifications() {
    hal::RegisterNetworkObserver(this);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvDisableNetworkNotifications() {
    hal::UnregisterNetworkObserver(this);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvGetCurrentNetworkInformation(NetworkInformation* aNetworkInfo) {
    hal::GetCurrentNetworkInformation(aNetworkInfo);
    return true;
  }

  void Notify(const NetworkInformation& aNetworkInfo) {
    unused << SendNotifyNetworkChange(aNetworkInfo);
  }

  NS_OVERRIDE virtual bool
  RecvEnableScreenOrientationNotifications() {
    hal::RegisterScreenOrientationObserver(this);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvDisableScreenOrientationNotifications() {
    hal::UnregisterScreenOrientationObserver(this);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvGetCurrentScreenOrientation(ScreenOrientation* aScreenOrientation) {
    hal::GetCurrentScreenOrientation(aScreenOrientation);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvLockScreenOrientation(const dom::ScreenOrientation& aOrientation, bool* aAllowed)
  {
    *aAllowed = hal::LockScreenOrientation(aOrientation);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvUnlockScreenOrientation()
  {
    hal::UnlockScreenOrientation();
    return true;
  }

  void Notify(const ScreenOrientationWrapper& aScreenOrientation) {
    unused << SendNotifyScreenOrientationChange(aScreenOrientation.orientation);
  }

  NS_OVERRIDE virtual bool
  RecvGetScreenEnabled(bool *enabled)
  {
    *enabled = hal::GetScreenEnabled();
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvSetScreenEnabled(const bool &enabled)
  {
    hal::SetScreenEnabled(enabled);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvGetCpuSleepAllowed(bool *allowed)
  {
    *allowed = hal::GetCpuSleepAllowed();
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvSetCpuSleepAllowed(const bool &allowed)
  {
    hal::SetCpuSleepAllowed(allowed);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvGetScreenBrightness(double *brightness)
  {
    *brightness = hal::GetScreenBrightness();
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvSetScreenBrightness(const double &brightness)
  {
    hal::SetScreenBrightness(brightness);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvSetLight(const LightType& aLight,  const hal::LightConfiguration& aConfig, bool *status)
  {
    *status = hal::SetLight(aLight, aConfig);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvGetLight(const LightType& aLight, LightConfiguration* aConfig, bool* status)
  {
    *status = hal::GetLight(aLight, aConfig);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvAdjustSystemClock(const int32_t &aDeltaMilliseconds)
  {
    hal::AdjustSystemClock(aDeltaMilliseconds);
    return true;
  }

  NS_OVERRIDE virtual bool 
  RecvSetTimezone(const nsCString& aTimezoneSpec)
  {
    hal::SetTimezone(aTimezoneSpec);
    return true;  
  }

  NS_OVERRIDE virtual bool
  RecvReboot()
  {
    hal::Reboot();
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvPowerOff()
  {
    hal::PowerOff();
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvEnableSensorNotifications(const SensorType &aSensor) {
    hal::RegisterSensorObserver(aSensor, this);
    return true;
  }
   
  NS_OVERRIDE virtual bool
  RecvDisableSensorNotifications(const SensorType &aSensor) {
    hal::UnregisterSensorObserver(aSensor, this);
    return true;
  }
  
  void Notify(const SensorData& aSensorData) {
    unused << SendNotifySensorChange(aSensorData);
  }

  NS_OVERRIDE virtual bool
  RecvModifyWakeLock(const nsString &aTopic,
                     const WakeLockControl &aLockAdjust,
                     const WakeLockControl &aHiddenAdjust)
  {
    hal::ModifyWakeLock(aTopic, aLockAdjust, aHiddenAdjust);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvEnableWakeLockNotifications()
  {
    hal::RegisterWakeLockObserver(this);
    return true;
  }
   
  NS_OVERRIDE virtual bool
  RecvDisableWakeLockNotifications()
  {
    hal::UnregisterWakeLockObserver(this);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvGetWakeLockInfo(const nsString &aTopic, WakeLockInformation *aWakeLockInfo)
  {
    hal::GetWakeLockInfo(aTopic, aWakeLockInfo);
    return true;
  }
  
  void Notify(const WakeLockInformation& aWakeLockInfo)
  {
    unused << SendNotifyWakeLockChange(aWakeLockInfo);
  }

  NS_OVERRIDE virtual bool
  RecvEnableSwitchNotifications(const SwitchDevice& aDevice) 
  {
    hal::RegisterSwitchObserver(aDevice, this);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvDisableSwitchNotifications(const SwitchDevice& aDevice) 
  {
    hal::UnregisterSwitchObserver(aDevice, this);
    return true;
  }

  void Notify(const SwitchEvent& aSwitchEvent)
  {
    unused << SendNotifySwitchChange(aSwitchEvent);
  }

  NS_OVERRIDE virtual bool
  RecvGetCurrentSwitchState(const SwitchDevice& aDevice, hal::SwitchState *aState)
  {
    *aState = hal::GetCurrentSwitchState(aDevice);
    return true;
  }
};

class HalChild : public PHalChild {
public:
  NS_OVERRIDE virtual bool
  RecvNotifyBatteryChange(const BatteryInformation& aBatteryInfo) {
    hal::NotifyBatteryChange(aBatteryInfo);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvNotifySensorChange(const hal::SensorData &aSensorData);

  NS_OVERRIDE virtual bool
  RecvNotifyNetworkChange(const NetworkInformation& aNetworkInfo) {
    hal::NotifyNetworkChange(aNetworkInfo);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvNotifyWakeLockChange(const WakeLockInformation& aWakeLockInfo) {
    hal::NotifyWakeLockChange(aWakeLockInfo);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvNotifyScreenOrientationChange(const ScreenOrientation& aScreenOrientation) {
    hal::NotifyScreenOrientationChange(aScreenOrientation);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvNotifySwitchChange(const mozilla::hal::SwitchEvent& aEvent) {
    hal::NotifySwitchChange(aEvent);
    return true;
  }
};

bool
HalChild::RecvNotifySensorChange(const hal::SensorData &aSensorData) {
  hal::NotifySensorChange(aSensorData);
  
  return true;
}

PHalChild* CreateHalChild() {
  return new HalChild();
}

PHalParent* CreateHalParent() {
  return new HalParent();
}

} // namespace hal_sandbox
} // namespace mozilla
