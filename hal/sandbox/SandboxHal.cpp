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
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/HalWakeLock.h"
#include "mozilla/Observer.h"
#include "mozilla/Unused.h"
#include "WindowIdentifier.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

namespace mozilla {
namespace hal_sandbox {

static bool sHalChildDestroyed = false;

bool HalChildDestroyed() { return sHalChildDestroyed; }

static PHalChild* sHal;
static PHalChild* Hal() {
  if (!sHal) {
    sHal = ContentChild::GetSingleton()->SendPHalConstructor();
  }
  return sHal;
}

void Vibrate(const nsTArray<uint32_t>& pattern, WindowIdentifier&& id) {
  HAL_LOG("Vibrate: Sending to parent process.");

  WindowIdentifier newID(std::move(id));
  newID.AppendProcessID();
  if (BrowserChild* bc = BrowserChild::GetFrom(newID.GetWindow())) {
    Hal()->SendVibrate(pattern, newID.AsArray(), WrapNotNull(bc));
  }
}

void CancelVibrate(WindowIdentifier&& id) {
  HAL_LOG("CancelVibrate: Sending to parent process.");

  WindowIdentifier newID(std::move(id));
  newID.AppendProcessID();
  if (BrowserChild* bc = BrowserChild::GetFrom(newID.GetWindow())) {
    Hal()->SendCancelVibrate(newID.AsArray(), WrapNotNull(bc));
  }
}

void EnableBatteryNotifications() { Hal()->SendEnableBatteryNotifications(); }

void DisableBatteryNotifications() { Hal()->SendDisableBatteryNotifications(); }

void GetCurrentBatteryInformation(BatteryInformation* aBatteryInfo) {
  Hal()->SendGetCurrentBatteryInformation(aBatteryInfo);
}

void EnableNetworkNotifications() { Hal()->SendEnableNetworkNotifications(); }

void DisableNetworkNotifications() { Hal()->SendDisableNetworkNotifications(); }

void GetCurrentNetworkInformation(NetworkInformation* aNetworkInfo) {
  Hal()->SendGetCurrentNetworkInformation(aNetworkInfo);
}

RefPtr<GenericNonExclusivePromise> LockScreenOrientation(
    const hal::ScreenOrientation& aOrientation) {
  return Hal()
      ->SendLockScreenOrientation(aOrientation)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [](const mozilla::MozPromise<nsresult, ipc::ResponseRejectReason,
                                          true>::ResolveOrRejectValue& aValue) {
               if (aValue.IsResolve()) {
                 if (NS_SUCCEEDED(aValue.ResolveValue())) {
                   return GenericNonExclusivePromise::CreateAndResolve(
                       true, __func__);
                 }
                 return GenericNonExclusivePromise::CreateAndReject(
                     aValue.ResolveValue(), __func__);
               }
               return GenericNonExclusivePromise::CreateAndReject(
                   NS_ERROR_FAILURE, __func__);
             });
}

void UnlockScreenOrientation() { Hal()->SendUnlockScreenOrientation(); }

void EnableSensorNotifications(SensorType aSensor) {
  Hal()->SendEnableSensorNotifications(aSensor);
}

void DisableSensorNotifications(SensorType aSensor) {
  Hal()->SendDisableSensorNotifications(aSensor);
}

void EnableWakeLockNotifications() { Hal()->SendEnableWakeLockNotifications(); }

void DisableWakeLockNotifications() {
  Hal()->SendDisableWakeLockNotifications();
}

void ModifyWakeLock(const nsAString& aTopic, WakeLockControl aLockAdjust,
                    WakeLockControl aHiddenAdjust) {
  Hal()->SendModifyWakeLock(aTopic, aLockAdjust, aHiddenAdjust);
}

void GetWakeLockInfo(const nsAString& aTopic,
                     WakeLockInformation* aWakeLockInfo) {
  Hal()->SendGetWakeLockInfo(aTopic, aWakeLockInfo);
}

bool EnableAlarm() {
  MOZ_CRASH("Alarms can't be programmed from sandboxed contexts.  Yet.");
}

void DisableAlarm() {
  MOZ_CRASH("Alarms can't be programmed from sandboxed contexts.  Yet.");
}

bool SetAlarm(int32_t aSeconds, int32_t aNanoseconds) {
  MOZ_CRASH("Alarms can't be programmed from sandboxed contexts.  Yet.");
}

void SetProcessPriority(int aPid, ProcessPriority aPriority) {
  MOZ_CRASH("Only the main process may set processes' priorities.");
}

class HalParent : public PHalParent,
                  public BatteryObserver,
                  public NetworkObserver,
                  public ISensorObserver,
                  public WakeLockObserver {
 public:
  virtual void ActorDestroy(ActorDestroyReason aWhy) override {
    // NB: you *must* unconditionally unregister your observer here,
    // if it *may* be registered below.
    hal::UnregisterBatteryObserver(this);
    hal::UnregisterNetworkObserver(this);
    for (auto sensor : MakeEnumeratedRange(NUM_SENSOR_TYPE)) {
      hal::UnregisterSensorObserver(sensor, this);
    }
    hal::UnregisterWakeLockObserver(this);
  }

  virtual mozilla::ipc::IPCResult RecvVibrate(
      nsTArray<unsigned int>&& pattern, nsTArray<uint64_t>&& id,
      NotNull<PBrowserParent*> browserParent) override {
    // We give all content vibration permission.
    //    BrowserParent *browserParent = BrowserParent::GetFrom(browserParent);
    /* xxxkhuey wtf
    nsCOMPtr<nsIDOMWindow> window =
      do_QueryInterface(browserParent->GetBrowserDOMWindow());
    */
    hal::Vibrate(pattern, WindowIdentifier(std::move(id), nullptr));
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvCancelVibrate(
      nsTArray<uint64_t>&& id,
      NotNull<PBrowserParent*> browserParent) override {
    // BrowserParent *browserParent = BrowserParent::GetFrom(browserParent);
    /* XXXkhuey wtf
    nsCOMPtr<nsIDOMWindow> window =
      browserParent->GetBrowserDOMWindow();
    */
    hal::CancelVibrate(WindowIdentifier(std::move(id), nullptr));
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvEnableBatteryNotifications() override {
    // We give all content battery-status permission.
    hal::RegisterBatteryObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvDisableBatteryNotifications() override {
    hal::UnregisterBatteryObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvGetCurrentBatteryInformation(
      BatteryInformation* aBatteryInfo) override {
    // We give all content battery-status permission.
    hal::GetCurrentBatteryInformation(aBatteryInfo);
    return IPC_OK();
  }

  void Notify(const BatteryInformation& aBatteryInfo) override {
    Unused << SendNotifyBatteryChange(aBatteryInfo);
  }

  virtual mozilla::ipc::IPCResult RecvEnableNetworkNotifications() override {
    // We give all content access to this network-status information.
    hal::RegisterNetworkObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvDisableNetworkNotifications() override {
    hal::UnregisterNetworkObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvGetCurrentNetworkInformation(
      NetworkInformation* aNetworkInfo) override {
    hal::GetCurrentNetworkInformation(aNetworkInfo);
    return IPC_OK();
  }

  void Notify(const NetworkInformation& aNetworkInfo) override {
    Unused << SendNotifyNetworkChange(aNetworkInfo);
  }

  virtual mozilla::ipc::IPCResult RecvLockScreenOrientation(
      const ScreenOrientation& aOrientation,
      LockScreenOrientationResolver&& aResolve) override {
    // FIXME/bug 777980: unprivileged content may only lock
    // orientation while fullscreen.  We should check whether the
    // request comes from an actor in a process that might be
    // fullscreen.  We don't have that information currently.

    hal::LockScreenOrientation(aOrientation)
        ->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [aResolve](const GenericNonExclusivePromise::ResolveOrRejectValue&
                           aValue) {
              if (aValue.IsResolve()) {
                MOZ_ASSERT(aValue.ResolveValue());
                aResolve(NS_OK);
                return;
              }
              aResolve(aValue.RejectValue());
            });
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvUnlockScreenOrientation() override {
    hal::UnlockScreenOrientation();
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvEnableSensorNotifications(
      const SensorType& aSensor) override {
    // We currently allow any content to register device-sensor
    // listeners.
    hal::RegisterSensorObserver(aSensor, this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvDisableSensorNotifications(
      const SensorType& aSensor) override {
    hal::UnregisterSensorObserver(aSensor, this);
    return IPC_OK();
  }

  void Notify(const SensorData& aSensorData) override {
    Unused << SendNotifySensorChange(aSensorData);
  }

  virtual mozilla::ipc::IPCResult RecvModifyWakeLock(
      const nsAString& aTopic, const WakeLockControl& aLockAdjust,
      const WakeLockControl& aHiddenAdjust) override {
    // We allow arbitrary content to use wake locks.
    uint64_t id = static_cast<ContentParent*>(Manager())->ChildID();
    hal_impl::ModifyWakeLockWithChildID(aTopic, aLockAdjust, aHiddenAdjust, id);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvEnableWakeLockNotifications() override {
    // We allow arbitrary content to use wake locks.
    hal::RegisterWakeLockObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvDisableWakeLockNotifications() override {
    hal::UnregisterWakeLockObserver(this);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvGetWakeLockInfo(
      const nsAString& aTopic, WakeLockInformation* aWakeLockInfo) override {
    hal::GetWakeLockInfo(aTopic, aWakeLockInfo);
    return IPC_OK();
  }

  void Notify(const WakeLockInformation& aWakeLockInfo) override {
    Unused << SendNotifyWakeLockChange(aWakeLockInfo);
  }
};

class HalChild : public PHalChild {
 public:
  virtual void ActorDestroy(ActorDestroyReason aWhy) override {
    sHalChildDestroyed = true;
  }

  virtual mozilla::ipc::IPCResult RecvNotifyBatteryChange(
      const BatteryInformation& aBatteryInfo) override {
    hal::NotifyBatteryChange(aBatteryInfo);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvNotifySensorChange(
      const hal::SensorData& aSensorData) override;

  virtual mozilla::ipc::IPCResult RecvNotifyNetworkChange(
      const NetworkInformation& aNetworkInfo) override {
    hal::NotifyNetworkChange(aNetworkInfo);
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvNotifyWakeLockChange(
      const WakeLockInformation& aWakeLockInfo) override {
    hal::NotifyWakeLockChange(aWakeLockInfo);
    return IPC_OK();
  }
};

mozilla::ipc::IPCResult HalChild::RecvNotifySensorChange(
    const hal::SensorData& aSensorData) {
  hal::NotifySensorChange(aSensorData);

  return IPC_OK();
}

PHalChild* CreateHalChild() { return new HalChild(); }

PHalParent* CreateHalParent() { return new HalParent(); }

}  // namespace hal_sandbox
}  // namespace mozilla
