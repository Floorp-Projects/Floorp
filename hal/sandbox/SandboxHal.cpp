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
Vibrate(const nsTArray<uint32_t>& pattern, const WindowIdentifier &id)
{
  HAL_LOG(("Vibrate: Sending to parent process."));

  AutoInfallibleTArray<uint32_t, 8> p(pattern);

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
EnableScreenConfigurationNotifications()
{
  Hal()->SendEnableScreenConfigurationNotifications();
}

void
DisableScreenConfigurationNotifications()
{
  Hal()->SendDisableScreenConfigurationNotifications();
}

void
GetCurrentScreenConfiguration(ScreenConfiguration* aScreenConfiguration)
{
  Hal()->SendGetCurrentScreenConfiguration(aScreenConfiguration);
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

bool
EnableAlarm()
{
  NS_RUNTIMEABORT("Alarms can't be programmed from sandboxed contexts.  Yet.");
  return false;
}

void
DisableAlarm()
{
  NS_RUNTIMEABORT("Alarms can't be programmed from sandboxed contexts.  Yet.");
}

bool
SetAlarm(long aSeconds, long aNanoseconds)
{
  NS_RUNTIMEABORT("Alarms can't be programmed from sandboxed contexts.  Yet.");
  return false;
}

class HalParent : public PHalParent
                , public BatteryObserver
                , public NetworkObserver
                , public ISensorObserver
                , public WakeLockObserver
                , public ScreenConfigurationObserver
                , public SwitchObserver
{
public:
  virtual bool
  RecvVibrate(const InfallibleTArray<unsigned int>& pattern,
              const InfallibleTArray<uint64_t> &id,
              PBrowserParent *browserParent) MOZ_OVERRIDE
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

  virtual bool
  RecvCancelVibrate(const InfallibleTArray<uint64_t> &id,
                    PBrowserParent *browserParent) MOZ_OVERRIDE
  {
    TabParent *tabParent = static_cast<TabParent*>(browserParent);
    nsCOMPtr<nsIDOMWindow> window =
      do_QueryInterface(tabParent->GetBrowserDOMWindow());
    WindowIdentifier newID(id, window);
    hal::CancelVibrate(newID);
    return true;
  }

  virtual bool
  RecvEnableBatteryNotifications() MOZ_OVERRIDE {
    hal::RegisterBatteryObserver(this);
    return true;
  }

  virtual bool
  RecvDisableBatteryNotifications() MOZ_OVERRIDE {
    hal::UnregisterBatteryObserver(this);
    return true;
  }

  virtual bool
  RecvGetCurrentBatteryInformation(BatteryInformation* aBatteryInfo) MOZ_OVERRIDE {
    hal::GetCurrentBatteryInformation(aBatteryInfo);
    return true;
  }

  void Notify(const BatteryInformation& aBatteryInfo) MOZ_OVERRIDE {
    unused << SendNotifyBatteryChange(aBatteryInfo);
  }

  virtual bool
  RecvEnableNetworkNotifications() MOZ_OVERRIDE {
    hal::RegisterNetworkObserver(this);
    return true;
  }

  virtual bool
  RecvDisableNetworkNotifications() MOZ_OVERRIDE {
    hal::UnregisterNetworkObserver(this);
    return true;
  }

  virtual bool
  RecvGetCurrentNetworkInformation(NetworkInformation* aNetworkInfo) MOZ_OVERRIDE {
    hal::GetCurrentNetworkInformation(aNetworkInfo);
    return true;
  }

  void Notify(const NetworkInformation& aNetworkInfo) {
    unused << SendNotifyNetworkChange(aNetworkInfo);
  }

  virtual bool
  RecvEnableScreenConfigurationNotifications() MOZ_OVERRIDE {
    hal::RegisterScreenConfigurationObserver(this);
    return true;
  }

  virtual bool
  RecvDisableScreenConfigurationNotifications() MOZ_OVERRIDE {
    hal::UnregisterScreenConfigurationObserver(this);
    return true;
  }

  virtual bool
  RecvGetCurrentScreenConfiguration(ScreenConfiguration* aScreenConfiguration) MOZ_OVERRIDE {
    hal::GetCurrentScreenConfiguration(aScreenConfiguration);
    return true;
  }

  virtual bool
  RecvLockScreenOrientation(const dom::ScreenOrientation& aOrientation, bool* aAllowed) MOZ_OVERRIDE
  {
    *aAllowed = hal::LockScreenOrientation(aOrientation);
    return true;
  }

  virtual bool
  RecvUnlockScreenOrientation() MOZ_OVERRIDE
  {
    hal::UnlockScreenOrientation();
    return true;
  }

  void Notify(const ScreenConfiguration& aScreenConfiguration) {
    unused << SendNotifyScreenConfigurationChange(aScreenConfiguration);
  }

  virtual bool
  RecvGetScreenEnabled(bool *enabled) MOZ_OVERRIDE
  {
    *enabled = hal::GetScreenEnabled();
    return true;
  }

  virtual bool
  RecvSetScreenEnabled(const bool &enabled) MOZ_OVERRIDE
  {
    hal::SetScreenEnabled(enabled);
    return true;
  }

  virtual bool
  RecvGetCpuSleepAllowed(bool *allowed) MOZ_OVERRIDE
  {
    *allowed = hal::GetCpuSleepAllowed();
    return true;
  }

  virtual bool
  RecvSetCpuSleepAllowed(const bool &allowed) MOZ_OVERRIDE
  {
    hal::SetCpuSleepAllowed(allowed);
    return true;
  }

  virtual bool
  RecvGetScreenBrightness(double *brightness) MOZ_OVERRIDE
  {
    *brightness = hal::GetScreenBrightness();
    return true;
  }

  virtual bool
  RecvSetScreenBrightness(const double &brightness) MOZ_OVERRIDE
  {
    hal::SetScreenBrightness(brightness);
    return true;
  }

  virtual bool
  RecvSetLight(const LightType& aLight,  const hal::LightConfiguration& aConfig, bool *status) MOZ_OVERRIDE
  {
    *status = hal::SetLight(aLight, aConfig);
    return true;
  }

  virtual bool
  RecvGetLight(const LightType& aLight, LightConfiguration* aConfig, bool* status) MOZ_OVERRIDE
  {
    *status = hal::GetLight(aLight, aConfig);
    return true;
  }

  virtual bool
  RecvAdjustSystemClock(const int32_t &aDeltaMilliseconds) MOZ_OVERRIDE
  {
    hal::AdjustSystemClock(aDeltaMilliseconds);
    return true;
  }

  virtual bool 
  RecvSetTimezone(const nsCString& aTimezoneSpec) MOZ_OVERRIDE
  {
    hal::SetTimezone(aTimezoneSpec);
    return true;  
  }

  virtual bool
  RecvReboot() MOZ_OVERRIDE
  {
    hal::Reboot();
    return true;
  }

  virtual bool
  RecvPowerOff() MOZ_OVERRIDE
  {
    hal::PowerOff();
    return true;
  }

  virtual bool
  RecvEnableSensorNotifications(const SensorType &aSensor) MOZ_OVERRIDE {
    hal::RegisterSensorObserver(aSensor, this);
    return true;
  }
   
  virtual bool
  RecvDisableSensorNotifications(const SensorType &aSensor) MOZ_OVERRIDE {
    hal::UnregisterSensorObserver(aSensor, this);
    return true;
  }
  
  void Notify(const SensorData& aSensorData) {
    unused << SendNotifySensorChange(aSensorData);
  }

  virtual bool
  RecvModifyWakeLock(const nsString &aTopic,
                     const WakeLockControl &aLockAdjust,
                     const WakeLockControl &aHiddenAdjust) MOZ_OVERRIDE
  {
    hal::ModifyWakeLock(aTopic, aLockAdjust, aHiddenAdjust);
    return true;
  }

  virtual bool
  RecvEnableWakeLockNotifications() MOZ_OVERRIDE
  {
    hal::RegisterWakeLockObserver(this);
    return true;
  }
   
  virtual bool
  RecvDisableWakeLockNotifications() MOZ_OVERRIDE
  {
    hal::UnregisterWakeLockObserver(this);
    return true;
  }

  virtual bool
  RecvGetWakeLockInfo(const nsString &aTopic, WakeLockInformation *aWakeLockInfo) MOZ_OVERRIDE
  {
    hal::GetWakeLockInfo(aTopic, aWakeLockInfo);
    return true;
  }
  
  void Notify(const WakeLockInformation& aWakeLockInfo)
  {
    unused << SendNotifyWakeLockChange(aWakeLockInfo);
  }

  virtual bool
  RecvEnableSwitchNotifications(const SwitchDevice& aDevice) MOZ_OVERRIDE
  {
    hal::RegisterSwitchObserver(aDevice, this);
    return true;
  }

  virtual bool
  RecvDisableSwitchNotifications(const SwitchDevice& aDevice) MOZ_OVERRIDE
  {
    hal::UnregisterSwitchObserver(aDevice, this);
    return true;
  }

  void Notify(const SwitchEvent& aSwitchEvent)
  {
    unused << SendNotifySwitchChange(aSwitchEvent);
  }

  virtual bool
  RecvGetCurrentSwitchState(const SwitchDevice& aDevice, hal::SwitchState *aState) MOZ_OVERRIDE
  {
    *aState = hal::GetCurrentSwitchState(aDevice);
    return true;
  }
};

class HalChild : public PHalChild {
public:
  virtual bool
  RecvNotifyBatteryChange(const BatteryInformation& aBatteryInfo) MOZ_OVERRIDE {
    hal::NotifyBatteryChange(aBatteryInfo);
    return true;
  }

  virtual bool
  RecvNotifySensorChange(const hal::SensorData &aSensorData) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyNetworkChange(const NetworkInformation& aNetworkInfo) MOZ_OVERRIDE {
    hal::NotifyNetworkChange(aNetworkInfo);
    return true;
  }

  virtual bool
  RecvNotifyWakeLockChange(const WakeLockInformation& aWakeLockInfo) MOZ_OVERRIDE {
    hal::NotifyWakeLockChange(aWakeLockInfo);
    return true;
  }

  virtual bool
  RecvNotifyScreenConfigurationChange(const ScreenConfiguration& aScreenConfiguration) MOZ_OVERRIDE {
    hal::NotifyScreenConfigurationChange(aScreenConfiguration);
    return true;
  }

  virtual bool
  RecvNotifySwitchChange(const mozilla::hal::SwitchEvent& aEvent) MOZ_OVERRIDE {
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
