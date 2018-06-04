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
#include "mozilla/fallback/FallbackScreenConfiguration.h"
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
  fallback::GetCurrentScreenConfiguration(aScreenConfiguration);
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

bool
EnableAlarm()
{
  MOZ_CRASH("Alarms can't be programmed from sandboxed contexts.  Yet.");
}

void
DisableAlarm()
{
  MOZ_CRASH("Alarms can't be programmed from sandboxed contexts.  Yet.");
}

bool
SetAlarm(int32_t aSeconds, int32_t aNanoseconds)
{
  MOZ_CRASH("Alarms can't be programmed from sandboxed contexts.  Yet.");
}

void
SetProcessPriority(int aPid, ProcessPriority aPriority)
{
  MOZ_CRASH("Only the main process may set processes' priorities.");
}

bool
SetProcessPrioritySupported()
{
  MOZ_CRASH("Only the main process may call SetProcessPrioritySupported().");
}

void
StartDiskSpaceWatcher()
{
  MOZ_CRASH("StartDiskSpaceWatcher() can't be called from sandboxed contexts.");
}

void
StopDiskSpaceWatcher()
{
  MOZ_CRASH("StopDiskSpaceWatcher() can't be called from sandboxed contexts.");
}

class HalParent : public PHalParent
                , public BatteryObserver
                , public NetworkObserver
                , public ISensorObserver
                , public WakeLockObserver
                , public ScreenConfigurationObserver
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
