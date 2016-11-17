/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalLog.h"
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
#include "mozilla/Unused.h"
#include "nsAutoPtr.h"
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
  HAL_LOG("Vibrate: Sending to parent process.");

  AutoTArray<uint32_t, 8> p(pattern);

  WindowIdentifier newID(id);
  newID.AppendProcessID();
  Hal()->SendVibrate(p, newID.AsArray(), TabChild::GetFrom(newID.GetWindow()));
}

void
CancelVibrate(const WindowIdentifier &id)
{
  HAL_LOG("CancelVibrate: Sending to parent process.");

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
LockScreenOrientation(const dom::ScreenOrientationInternal& aOrientation)
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
  Unused << aDevice;
  Unused << aState;
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
SetProcessPriority(int aPid, ProcessPriority aPriority, uint32_t aLRU)
{
  NS_RUNTIMEABORT("Only the main process may set processes' priorities.");
}

void
SetCurrentThreadPriority(ThreadPriority aThreadPriority)
{
  NS_RUNTIMEABORT("Setting current thread priority cannot be called from sandboxed contexts.");
}

void
SetThreadPriority(PlatformThreadId aThreadId,
                  ThreadPriority aThreadPriority)
{
  NS_RUNTIMEABORT("Setting thread priority cannot be called from sandboxed contexts.");
}

void
FactoryReset(FactoryResetReason& aReason)
{
  if (aReason == FactoryResetReason::Normal) {
    Hal()->SendFactoryReset(NS_LITERAL_STRING("normal"));
  } else if (aReason == FactoryResetReason::Wipe) {
    Hal()->SendFactoryReset(NS_LITERAL_STRING("wipe"));
  } else if (aReason == FactoryResetReason::Root) {
    Hal()->SendFactoryReset(NS_LITERAL_STRING("root"));
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

bool IsHeadphoneEventFromInputDev()
{
  NS_RUNTIMEABORT("IsHeadphoneEventFromInputDev() cannot be called from sandboxed contexts.");
  return false;
}

nsresult StartSystemService(const char* aSvcName, const char* aArgs)
{
  NS_RUNTIMEABORT("System services cannot be controlled from sandboxed contexts.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

void StopSystemService(const char* aSvcName)
{
  NS_RUNTIMEABORT("System services cannot be controlled from sandboxed contexts.");
}

bool SystemServiceIsRunning(const char* aSvcName)
{
  NS_RUNTIMEABORT("System services cannot be controlled from sandboxed contexts.");
  return false;
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
  ActorDestroy(ActorDestroyReason aWhy) override
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

  virtual mozilla::ipc::IPCResult
  RecvVibrate(InfallibleTArray<unsigned int>&& pattern,
              InfallibleTArray<uint64_t>&& id,
              PBrowserParent *browserParent) override
  {
    // We give all content vibration permission.
    //    TabParent *tabParent = TabParent::GetFrom(browserParent);
    /* xxxkhuey wtf
    nsCOMPtr<nsIDOMWindow> window =
      do_QueryInterface(tabParent->GetBrowserDOMWindow());
    */
    WindowIdentifier newID(id, nullptr);
    hal::Vibrate(pattern, newID);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvCancelVibrate(InfallibleTArray<uint64_t> &&id,
                    PBrowserParent *browserParent) override
  {
    //TabParent *tabParent = TabParent::GetFrom(browserParent);
    /* XXXkhuey wtf
    nsCOMPtr<nsIDOMWindow> window =
      tabParent->GetBrowserDOMWindow();
    */
    WindowIdentifier newID(id, nullptr);
    hal::CancelVibrate(newID);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvEnableBatteryNotifications() override {
    // We give all content battery-status permission.
    hal::RegisterBatteryObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvDisableBatteryNotifications() override {
    hal::UnregisterBatteryObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvGetCurrentBatteryInformation(BatteryInformation* aBatteryInfo) override {
    // We give all content battery-status permission.
    hal::GetCurrentBatteryInformation(aBatteryInfo);
    return IPC_OK();
  }

  void Notify(const BatteryInformation& aBatteryInfo) override {
    Unused << SendNotifyBatteryChange(aBatteryInfo);
  }

  virtual mozilla::ipc::IPCResult
  RecvEnableNetworkNotifications() override {
    // We give all content access to this network-status information.
    hal::RegisterNetworkObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvDisableNetworkNotifications() override {
    hal::UnregisterNetworkObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvGetCurrentNetworkInformation(NetworkInformation* aNetworkInfo) override {
    hal::GetCurrentNetworkInformation(aNetworkInfo);
    return IPC_OK();
  }

  void Notify(const NetworkInformation& aNetworkInfo) override {
    Unused << SendNotifyNetworkChange(aNetworkInfo);
  }

  virtual mozilla::ipc::IPCResult
  RecvEnableScreenConfigurationNotifications() override {
    // Screen configuration is used to implement CSS and DOM
    // properties, so all content already has access to this.
    hal::RegisterScreenConfigurationObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvDisableScreenConfigurationNotifications() override {
    hal::UnregisterScreenConfigurationObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvGetCurrentScreenConfiguration(ScreenConfiguration* aScreenConfiguration) override {
    hal::GetCurrentScreenConfiguration(aScreenConfiguration);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvLockScreenOrientation(const dom::ScreenOrientationInternal& aOrientation, bool* aAllowed) override
  {
    // FIXME/bug 777980: unprivileged content may only lock
    // orientation while fullscreen.  We should check whether the
    // request comes from an actor in a process that might be
    // fullscreen.  We don't have that information currently.
    *aAllowed = hal::LockScreenOrientation(aOrientation);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvUnlockScreenOrientation() override
  {
    hal::UnlockScreenOrientation();
    return IPC_OK();
  }

  void Notify(const ScreenConfiguration& aScreenConfiguration) override {
    Unused << SendNotifyScreenConfigurationChange(aScreenConfiguration);
  }

  virtual mozilla::ipc::IPCResult
  RecvGetScreenEnabled(bool* aEnabled) override
  {
    *aEnabled = hal::GetScreenEnabled();
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvSetScreenEnabled(const bool& aEnabled) override
  {
    hal::SetScreenEnabled(aEnabled);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvGetKeyLightEnabled(bool* aEnabled) override
  {
    *aEnabled = hal::GetKeyLightEnabled();
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvSetKeyLightEnabled(const bool& aEnabled) override
  {
    hal::SetKeyLightEnabled(aEnabled);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvGetCpuSleepAllowed(bool* aAllowed) override
  {
    *aAllowed = hal::GetCpuSleepAllowed();
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvSetCpuSleepAllowed(const bool& aAllowed) override
  {
    hal::SetCpuSleepAllowed(aAllowed);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvGetScreenBrightness(double* aBrightness) override
  {
    *aBrightness = hal::GetScreenBrightness();
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvSetScreenBrightness(const double& aBrightness) override
  {
    hal::SetScreenBrightness(aBrightness);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvAdjustSystemClock(const int64_t &aDeltaMilliseconds) override
  {
    hal::AdjustSystemClock(aDeltaMilliseconds);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvSetTimezone(const nsCString& aTimezoneSpec) override
  {
    hal::SetTimezone(aTimezoneSpec);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvGetTimezone(nsCString *aTimezoneSpec) override
  {
    *aTimezoneSpec = hal::GetTimezone();
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvGetTimezoneOffset(int32_t *aTimezoneOffset) override
  {
    *aTimezoneOffset = hal::GetTimezoneOffset();
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvEnableSystemClockChangeNotifications() override
  {
    hal::RegisterSystemClockChangeObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvDisableSystemClockChangeNotifications() override
  {
    hal::UnregisterSystemClockChangeObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvEnableSystemTimezoneChangeNotifications() override
  {
    hal::RegisterSystemTimezoneChangeObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvDisableSystemTimezoneChangeNotifications() override
  {
    hal::UnregisterSystemTimezoneChangeObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvEnableSensorNotifications(const SensorType &aSensor) override {
    // We currently allow any content to register device-sensor
    // listeners.
    hal::RegisterSensorObserver(aSensor, this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvDisableSensorNotifications(const SensorType &aSensor) override {
    hal::UnregisterSensorObserver(aSensor, this);
    return IPC_OK();
  }

  void Notify(const SensorData& aSensorData) override {
    Unused << SendNotifySensorChange(aSensorData);
  }

  virtual mozilla::ipc::IPCResult
  RecvModifyWakeLock(const nsString& aTopic,
                     const WakeLockControl& aLockAdjust,
                     const WakeLockControl& aHiddenAdjust,
                     const uint64_t& aProcessID) override
  {
    MOZ_ASSERT(aProcessID != CONTENT_PROCESS_ID_UNKNOWN);

    // We allow arbitrary content to use wake locks.
    hal::ModifyWakeLock(aTopic, aLockAdjust, aHiddenAdjust, aProcessID);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvEnableWakeLockNotifications() override
  {
    // We allow arbitrary content to use wake locks.
    hal::RegisterWakeLockObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvDisableWakeLockNotifications() override
  {
    hal::UnregisterWakeLockObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvGetWakeLockInfo(const nsString &aTopic, WakeLockInformation *aWakeLockInfo) override
  {
    hal::GetWakeLockInfo(aTopic, aWakeLockInfo);
    return IPC_OK();
  }

  void Notify(const WakeLockInformation& aWakeLockInfo) override
  {
    Unused << SendNotifyWakeLockChange(aWakeLockInfo);
  }

  virtual mozilla::ipc::IPCResult
  RecvEnableSwitchNotifications(const SwitchDevice& aDevice) override
  {
    // Content has no reason to listen to switch events currently.
    hal::RegisterSwitchObserver(aDevice, this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvDisableSwitchNotifications(const SwitchDevice& aDevice) override
  {
    hal::UnregisterSwitchObserver(aDevice, this);
    return IPC_OK();
  }

  void Notify(const SwitchEvent& aSwitchEvent) override
  {
    Unused << SendNotifySwitchChange(aSwitchEvent);
  }

  virtual mozilla::ipc::IPCResult
  RecvGetCurrentSwitchState(const SwitchDevice& aDevice, hal::SwitchState *aState) override
  {
    // Content has no reason to listen to switch events currently.
    *aState = hal::GetCurrentSwitchState(aDevice);
    return IPC_OK();
  }

  void Notify(const int64_t& aClockDeltaMS) override
  {
    Unused << SendNotifySystemClockChange(aClockDeltaMS);
  }

  void Notify(const SystemTimezoneChangeInformation& aSystemTimezoneChangeInfo) override
  {
    Unused << SendNotifySystemTimezoneChange(aSystemTimezoneChangeInfo);
  }

  virtual mozilla::ipc::IPCResult
  RecvFactoryReset(const nsString& aReason) override
  {
    FactoryResetReason reason = FactoryResetReason::Normal;
    if (aReason.EqualsLiteral("normal")) {
      reason = FactoryResetReason::Normal;
    } else if (aReason.EqualsLiteral("wipe")) {
      reason = FactoryResetReason::Wipe;
    } else if (aReason.EqualsLiteral("root")) {
      reason = FactoryResetReason::Root;
    } else {
      // Invalid factory reset reason. That should never happen.
      return IPC_FAIL_NO_REASON(this);
    }

    hal::FactoryReset(reason);
    return IPC_OK();
  }
};

class HalChild : public PHalChild {
public:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override
  {
    sHalChildDestroyed = true;
  }

  virtual mozilla::ipc::IPCResult
  RecvNotifyBatteryChange(const BatteryInformation& aBatteryInfo) override {
    hal::NotifyBatteryChange(aBatteryInfo);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvNotifySensorChange(const hal::SensorData &aSensorData) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyNetworkChange(const NetworkInformation& aNetworkInfo) override {
    hal::NotifyNetworkChange(aNetworkInfo);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvNotifyWakeLockChange(const WakeLockInformation& aWakeLockInfo) override {
    hal::NotifyWakeLockChange(aWakeLockInfo);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvNotifyScreenConfigurationChange(const ScreenConfiguration& aScreenConfiguration) override {
    hal::NotifyScreenConfigurationChange(aScreenConfiguration);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvNotifySwitchChange(const mozilla::hal::SwitchEvent& aEvent) override {
    hal::NotifySwitchChange(aEvent);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvNotifySystemClockChange(const int64_t& aClockDeltaMS) override {
    hal::NotifySystemClockChange(aClockDeltaMS);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult
  RecvNotifySystemTimezoneChange(
    const SystemTimezoneChangeInformation& aSystemTimezoneChangeInfo) override {
    hal::NotifySystemTimezoneChange(aSystemTimezoneChangeInfo);
    return IPC_OK();
  }
};

mozilla::ipc::IPCResult
HalChild::RecvNotifySensorChange(const hal::SensorData &aSensorData) {
  hal::NotifySensorChange(aSensorData);

  return IPC_OK();
}

PHalChild* CreateHalChild() {
  return new HalChild();
}

PHalParent* CreateHalParent() {
  return new HalParent();
}

} // namespace hal_sandbox
} // namespace mozilla
