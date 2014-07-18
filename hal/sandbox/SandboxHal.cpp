/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "mozilla/AppProcessChecker.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
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

static bool sHalChildDestroyed = false;

bool
HalChildDestroyed()
{
  return sHalChildDestroyed;
}

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
  Hal()->SendVibrate(p, newID.AsArray(), TabChild::GetFrom(newID.GetWindow()));
}

void
CancelVibrate(const WindowIdentifier &id)
{
  HAL_LOG(("CancelVibrate: Sending to parent process."));

  WindowIdentifier newID(id);
  newID.AppendProcessID();
  Hal()->SendCancelVibrate(newID.AsArray(), TabChild::GetFrom(newID.GetWindow()));
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
SetScreenEnabled(bool aEnabled)
{
  Hal()->SendSetScreenEnabled(aEnabled);
}

bool
GetKeyLightEnabled()
{
  bool enabled = false;
  Hal()->SendGetKeyLightEnabled(&enabled);
  return enabled;
}

void
SetKeyLightEnabled(bool aEnabled)
{
  Hal()->SendSetKeyLightEnabled(aEnabled);
}

bool
GetCpuSleepAllowed()
{
  bool allowed = true;
  Hal()->SendGetCpuSleepAllowed(&allowed);
  return allowed;
}

void
SetCpuSleepAllowed(bool aAllowed)
{
  Hal()->SendSetCpuSleepAllowed(aAllowed);
}

double
GetScreenBrightness()
{
  double brightness = 0;
  Hal()->SendGetScreenBrightness(&brightness);
  return brightness;
}

void
SetScreenBrightness(double aBrightness)
{
  Hal()->SendSetScreenBrightness(aBrightness);
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
AdjustSystemClock(int64_t aDeltaMilliseconds)
{
  Hal()->SendAdjustSystemClock(aDeltaMilliseconds);
}

void
SetTimezone(const nsCString& aTimezoneSpec)
{
  Hal()->SendSetTimezone(nsCString(aTimezoneSpec));
} 

nsCString
GetTimezone()
{
  nsCString timezone;
  Hal()->SendGetTimezone(&timezone);
  return timezone;
}

int32_t
GetTimezoneOffset()
{
  int32_t timezoneOffset;
  Hal()->SendGetTimezoneOffset(&timezoneOffset);
  return timezoneOffset;
}

void
EnableSystemClockChangeNotifications()
{
  Hal()->SendEnableSystemClockChangeNotifications();
}

void
DisableSystemClockChangeNotifications()
{
  Hal()->SendDisableSystemClockChangeNotifications();
}

void
EnableSystemTimezoneChangeNotifications()
{
  Hal()->SendEnableSystemTimezoneChangeNotifications();
}

void
DisableSystemTimezoneChangeNotifications()
{
  Hal()->SendDisableSystemTimezoneChangeNotifications();
}

void
Reboot()
{
  NS_RUNTIMEABORT("Reboot() can't be called from sandboxed contexts.");
}

void
PowerOff()
{
  NS_RUNTIMEABORT("PowerOff() can't be called from sandboxed contexts.");
}

void
StartForceQuitWatchdog(ShutdownMode aMode, int32_t aTimeoutSecs)
{
  NS_RUNTIMEABORT("StartForceQuitWatchdog() can't be called from sandboxed contexts.");
}

void
EnableSensorNotifications(SensorType aSensor) {
  Hal()->SendEnableSensorNotifications(aSensor);
}

void
DisableSensorNotifications(SensorType aSensor) {
  Hal()->SendDisableSensorNotifications(aSensor);
}

//TODO: bug 852944 - IPC implementations of these
void StartMonitoringGamepadStatus()
{}

void StopMonitoringGamepadStatus()
{}

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
ModifyWakeLock(const nsAString &aTopic,
               WakeLockControl aLockAdjust,
               WakeLockControl aHiddenAdjust,
               uint64_t aProcessID)
{
  MOZ_ASSERT(aProcessID != CONTENT_PROCESS_ID_UNKNOWN);
  Hal()->SendModifyWakeLock(nsString(aTopic), aLockAdjust, aHiddenAdjust, aProcessID);
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

void
NotifySwitchStateFromInputDevice(SwitchDevice aDevice, SwitchState aState)
{
  unused << aDevice;
  unused << aState;
  NS_RUNTIMEABORT("Only the main process may notify switch state change.");
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
SetAlarm(int32_t aSeconds, int32_t aNanoseconds)
{
  NS_RUNTIMEABORT("Alarms can't be programmed from sandboxed contexts.  Yet.");
  return false;
}

void
SetProcessPriority(int aPid,
                   ProcessPriority aPriority,
                   ProcessCPUPriority aCPUPriority,
                   uint32_t aBackgroundLRU)
{
  NS_RUNTIMEABORT("Only the main process may set processes' priorities.");
}

void
SetCurrentThreadPriority(ThreadPriority aThreadPriority)
{
  NS_RUNTIMEABORT("Setting thread priority cannot be called from sandboxed contexts.");
}

void
EnableFMRadio(const hal::FMRadioSettings& aSettings)
{
  NS_RUNTIMEABORT("FM radio cannot be called from sandboxed contexts.");
}

void
DisableFMRadio()
{
  NS_RUNTIMEABORT("FM radio cannot be called from sandboxed contexts.");
}

void
FMRadioSeek(const hal::FMRadioSeekDirection& aDirection)
{
  NS_RUNTIMEABORT("FM radio cannot be called from sandboxed contexts.");
}

void
GetFMRadioSettings(FMRadioSettings* aSettings)
{
  NS_RUNTIMEABORT("FM radio cannot be called from sandboxed contexts.");
}

void
SetFMRadioFrequency(const uint32_t aFrequency)
{
  NS_RUNTIMEABORT("FM radio cannot be called from sandboxed contexts.");
}

uint32_t
GetFMRadioFrequency()
{
  NS_RUNTIMEABORT("FM radio cannot be called from sandboxed contexts.");
  return 0;
}

bool
IsFMRadioOn()
{
  NS_RUNTIMEABORT("FM radio cannot be called from sandboxed contexts.");
  return false;
}

uint32_t
GetFMRadioSignalStrength()
{
  NS_RUNTIMEABORT("FM radio cannot be called from sandboxed contexts.");
  return 0;
}

void
CancelFMRadioSeek()
{
  NS_RUNTIMEABORT("FM radio cannot be called from sandboxed contexts.");
}

void
FactoryReset(FactoryResetReason& aReason)
{
  if (aReason == FactoryResetReason::Normal) {
    Hal()->SendFactoryReset(NS_LITERAL_STRING("normal"));
  } else if (aReason == FactoryResetReason::Wipe) {
    Hal()->SendFactoryReset(NS_LITERAL_STRING("wipe"));
  }
}

void
StartDiskSpaceWatcher()
{
  NS_RUNTIMEABORT("StartDiskSpaceWatcher() can't be called from sandboxed contexts.");
}

void
StopDiskSpaceWatcher()
{
  NS_RUNTIMEABORT("StopDiskSpaceWatcher() can't be called from sandboxed contexts.");
}

class HalParent : public PHalParent
                , public BatteryObserver
                , public NetworkObserver
                , public ISensorObserver
                , public WakeLockObserver
                , public ScreenConfigurationObserver
                , public SwitchObserver
                , public SystemClockChangeObserver
                , public SystemTimezoneChangeObserver
{
public:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE
  {
    // NB: you *must* unconditionally unregister your observer here,
    // if it *may* be registered below.
    hal::UnregisterBatteryObserver(this);
    hal::UnregisterNetworkObserver(this);
    hal::UnregisterScreenConfigurationObserver(this);
    for (int32_t sensor = SENSOR_UNKNOWN + 1;
         sensor < NUM_SENSOR_TYPE; ++sensor) {
      hal::UnregisterSensorObserver(SensorType(sensor), this);
    }
    hal::UnregisterWakeLockObserver(this);
    hal::UnregisterSystemClockChangeObserver(this);
    hal::UnregisterSystemTimezoneChangeObserver(this);
    for (int32_t switchDevice = SWITCH_DEVICE_UNKNOWN + 1;
         switchDevice < NUM_SWITCH_DEVICE; ++switchDevice) {
      hal::UnregisterSwitchObserver(SwitchDevice(switchDevice), this);
    }
  }

  virtual bool
  RecvVibrate(const InfallibleTArray<unsigned int>& pattern,
              const InfallibleTArray<uint64_t> &id,
              PBrowserParent *browserParent) MOZ_OVERRIDE
  {
    // We give all content vibration permission.
    TabParent *tabParent = static_cast<TabParent*>(browserParent);
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
    // We give all content battery-status permission.
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
    // We give all content battery-status permission.
    hal::GetCurrentBatteryInformation(aBatteryInfo);
    return true;
  }

  void Notify(const BatteryInformation& aBatteryInfo) MOZ_OVERRIDE {
    unused << SendNotifyBatteryChange(aBatteryInfo);
  }

  virtual bool
  RecvEnableNetworkNotifications() MOZ_OVERRIDE {
    // We give all content access to this network-status information.
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
    // Screen configuration is used to implement CSS and DOM
    // properties, so all content already has access to this.
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
    // FIXME/bug 777980: unprivileged content may only lock
    // orientation while fullscreen.  We should check whether the
    // request comes from an actor in a process that might be
    // fullscreen.  We don't have that information currently.
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
  RecvGetScreenEnabled(bool* aEnabled) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "power")) {
      return false;
    }
    *aEnabled = hal::GetScreenEnabled();
    return true;
  }

  virtual bool
  RecvSetScreenEnabled(const bool& aEnabled) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "power")) {
      return false;
    }
    hal::SetScreenEnabled(aEnabled);
    return true;
  }

  virtual bool
  RecvGetKeyLightEnabled(bool* aEnabled) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "power")) {
      return false;
    }
    *aEnabled = hal::GetKeyLightEnabled();
    return true;
  }

  virtual bool
  RecvSetKeyLightEnabled(const bool& aEnabled) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "power")) {
      return false;
    }
    hal::SetKeyLightEnabled(aEnabled);
    return true;
  }

  virtual bool
  RecvGetCpuSleepAllowed(bool* aAllowed) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "power")) {
      return false;
    }
    *aAllowed = hal::GetCpuSleepAllowed();
    return true;
  }

  virtual bool
  RecvSetCpuSleepAllowed(const bool& aAllowed) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "power")) {
      return false;
    }
    hal::SetCpuSleepAllowed(aAllowed);
    return true;
  }

  virtual bool
  RecvGetScreenBrightness(double* aBrightness) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "power")) {
      return false;
    }
    *aBrightness = hal::GetScreenBrightness();
    return true;
  }

  virtual bool
  RecvSetScreenBrightness(const double& aBrightness) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "power")) {
      return false;
    }
    hal::SetScreenBrightness(aBrightness);
    return true;
  }

  virtual bool
  RecvSetLight(const LightType& aLight,  const hal::LightConfiguration& aConfig, bool *status) MOZ_OVERRIDE
  {
    // XXX currently, the hardware key light and screen backlight are
    // controlled as a unit.  Those are set through the power API, and
    // there's no other way to poke lights currently, so we require
    // "power" privileges here.
    if (!AssertAppProcessPermission(this, "power")) {
      return false;
    }
    *status = hal::SetLight(aLight, aConfig);
    return true;
  }

  virtual bool
  RecvGetLight(const LightType& aLight, LightConfiguration* aConfig, bool* status) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "power")) {
      return false;
    }
    *status = hal::GetLight(aLight, aConfig);
    return true;
  }

  virtual bool
  RecvAdjustSystemClock(const int64_t &aDeltaMilliseconds) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "time")) {
      return false;
    }
    hal::AdjustSystemClock(aDeltaMilliseconds);
    return true;
  }

  virtual bool 
  RecvSetTimezone(const nsCString& aTimezoneSpec) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "time")) {
      return false;
    }
    hal::SetTimezone(aTimezoneSpec);
    return true;  
  }

  virtual bool
  RecvGetTimezone(nsCString *aTimezoneSpec) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "time")) {
      return false;
    }
    *aTimezoneSpec = hal::GetTimezone();
    return true;
  }

  virtual bool
  RecvGetTimezoneOffset(int32_t *aTimezoneOffset) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "time")) {
      return false;
    }
    *aTimezoneOffset = hal::GetTimezoneOffset();
    return true;
  }

  virtual bool
  RecvEnableSystemClockChangeNotifications() MOZ_OVERRIDE
  {
    hal::RegisterSystemClockChangeObserver(this);
    return true;
  }

  virtual bool
  RecvDisableSystemClockChangeNotifications() MOZ_OVERRIDE
  {
    hal::UnregisterSystemClockChangeObserver(this);
    return true;
  }

  virtual bool
  RecvEnableSystemTimezoneChangeNotifications() MOZ_OVERRIDE
  {
    hal::RegisterSystemTimezoneChangeObserver(this);
    return true;
  }

  virtual bool
  RecvDisableSystemTimezoneChangeNotifications() MOZ_OVERRIDE
  {
    hal::UnregisterSystemTimezoneChangeObserver(this);
    return true;
  }

  virtual bool
  RecvEnableSensorNotifications(const SensorType &aSensor) MOZ_OVERRIDE {
    // We currently allow any content to register device-sensor
    // listeners.
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
  RecvModifyWakeLock(const nsString& aTopic,
                     const WakeLockControl& aLockAdjust,
                     const WakeLockControl& aHiddenAdjust,
                     const uint64_t& aProcessID) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aProcessID != CONTENT_PROCESS_ID_UNKNOWN);

    // We allow arbitrary content to use wake locks.
    hal::ModifyWakeLock(aTopic, aLockAdjust, aHiddenAdjust, aProcessID);
    return true;
  }

  virtual bool
  RecvEnableWakeLockNotifications() MOZ_OVERRIDE
  {
    // We allow arbitrary content to use wake locks.
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
    // Content has no reason to listen to switch events currently.
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
    // Content has no reason to listen to switch events currently.
    *aState = hal::GetCurrentSwitchState(aDevice);
    return true;
  }

  void Notify(const int64_t& aClockDeltaMS)
  {
    unused << SendNotifySystemClockChange(aClockDeltaMS);
  }

  void Notify(const SystemTimezoneChangeInformation& aSystemTimezoneChangeInfo)
  {
    unused << SendNotifySystemTimezoneChange(aSystemTimezoneChangeInfo);
  }

  virtual bool
  RecvFactoryReset(const nsString& aReason) MOZ_OVERRIDE
  {
    if (!AssertAppProcessPermission(this, "power")) {
      return false;
    }

    FactoryResetReason reason = FactoryResetReason::Normal;
    if (aReason.EqualsLiteral("normal")) {
      reason = FactoryResetReason::Normal;
    } else if (aReason.EqualsLiteral("wipe")) {
      reason = FactoryResetReason::Wipe;
    } else {
      // Invalid factory reset reason. That should never happen.
      return false;
    }

    hal::FactoryReset(reason);
    return true;
  }

  virtual mozilla::ipc::IProtocol*
  CloneProtocol(Channel* aChannel,
                mozilla::ipc::ProtocolCloneContext* aCtx) MOZ_OVERRIDE
  {
    ContentParent* contentParent = aCtx->GetContentParent();
    nsAutoPtr<PHalParent> actor(contentParent->AllocPHalParent());
    if (!actor || !contentParent->RecvPHalConstructor(actor)) {
      return nullptr;
    }
    return actor.forget();
  }
};

class HalChild : public PHalChild {
public:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE
  {
    sHalChildDestroyed = true;
  }

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

  virtual bool
  RecvNotifySystemClockChange(const int64_t& aClockDeltaMS) {
    hal::NotifySystemClockChange(aClockDeltaMS);
    return true;
  }

  virtual bool
  RecvNotifySystemTimezoneChange(
    const SystemTimezoneChangeInformation& aSystemTimezoneChangeInfo) {
    hal::NotifySystemTimezoneChange(aSystemTimezoneChangeInfo);
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
