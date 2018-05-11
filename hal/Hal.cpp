/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

#include "HalImpl.h"
#include "HalLog.h"
#include "HalSandbox.h"
#include "nsIDOMWindow.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsITabChild.h"
#include "nsIWebNavigation.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nsPIDOMWindow.h"
#include "nsJSUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Observer.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "WindowIdentifier.h"

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#endif

using namespace mozilla::services;
using namespace mozilla::dom;

#define PROXY_IF_SANDBOXED(_call)                 \
  do {                                            \
    if (InSandbox()) {                            \
      if (!hal_sandbox::HalChildDestroyed()) {    \
        hal_sandbox::_call;                       \
      }                                           \
    } else {                                      \
      hal_impl::_call;                            \
    }                                             \
  } while (0)

#define RETURN_PROXY_IF_SANDBOXED(_call, defValue)\
  do {                                            \
    if (InSandbox()) {                            \
      if (hal_sandbox::HalChildDestroyed()) {     \
        return defValue;                          \
      }                                           \
      return hal_sandbox::_call;                  \
    } else {                                      \
      return hal_impl::_call;                     \
    }                                             \
  } while (0)

namespace mozilla {
namespace hal {

mozilla::LogModule *
GetHalLog()
{
  static mozilla::LazyLogModule sHalLog("hal");
  return sHalLog;
}

namespace {

void
AssertMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
}

bool
InSandbox()
{
  return GeckoProcessType_Content == XRE_GetProcessType();
}

void
AssertMainProcess()
{
  MOZ_ASSERT(GeckoProcessType_Default == XRE_GetProcessType());
}

bool
WindowIsActive(nsPIDOMWindowInner* aWindow)
{
  nsIDocument* document = aWindow->GetDoc();
  NS_ENSURE_TRUE(document, false);

  return !document->Hidden();
}

StaticAutoPtr<WindowIdentifier::IDArrayType> gLastIDToVibrate;

void InitLastIDToVibrate()
{
  gLastIDToVibrate = new WindowIdentifier::IDArrayType();
  ClearOnShutdown(&gLastIDToVibrate);
}

} // namespace

void
Vibrate(const nsTArray<uint32_t>& pattern, nsPIDOMWindowInner* window)
{
  Vibrate(pattern, WindowIdentifier(window));
}

void
Vibrate(const nsTArray<uint32_t>& pattern, const WindowIdentifier &id)
{
  AssertMainThread();

  // Only active windows may start vibrations.  If |id| hasn't gone
  // through the IPC layer -- that is, if our caller is the outside
  // world, not hal_proxy -- check whether the window is active.  If
  // |id| has gone through IPC, don't check the window's visibility;
  // only the window corresponding to the bottommost process has its
  // visibility state set correctly.
  if (!id.HasTraveledThroughIPC() && !WindowIsActive(id.GetWindow())) {
    HAL_LOG("Vibrate: Window is inactive, dropping vibrate.");
    return;
  }

  if (!InSandbox()) {
    if (!gLastIDToVibrate) {
      InitLastIDToVibrate();
    }
    *gLastIDToVibrate = id.AsArray();
  }

  // Don't forward our ID if we are not in the sandbox, because hal_impl
  // doesn't need it, and we don't want it to be tempted to read it.  The
  // empty identifier will assert if it's used.
  PROXY_IF_SANDBOXED(Vibrate(pattern, InSandbox() ? id : WindowIdentifier()));
}

void
CancelVibrate(nsPIDOMWindowInner* window)
{
  CancelVibrate(WindowIdentifier(window));
}

void
CancelVibrate(const WindowIdentifier &id)
{
  AssertMainThread();

  // Although only active windows may start vibrations, a window may
  // cancel its own vibration even if it's no longer active.
  //
  // After a window is marked as inactive, it sends a CancelVibrate
  // request.  We want this request to cancel a playing vibration
  // started by that window, so we certainly don't want to reject the
  // cancellation request because the window is now inactive.
  //
  // But it could be the case that, after this window became inactive,
  // some other window came along and started a vibration.  We don't
  // want this window's cancellation request to cancel that window's
  // actively-playing vibration!
  //
  // To solve this problem, we keep track of the id of the last window
  // to start a vibration, and only accepts cancellation requests from
  // the same window.  All other cancellation requests are ignored.

  if (InSandbox() || (gLastIDToVibrate && *gLastIDToVibrate == id.AsArray())) {
    // Don't forward our ID if we are not in the sandbox, because hal_impl
    // doesn't need it, and we don't want it to be tempted to read it.  The
    // empty identifier will assert if it's used.
    PROXY_IF_SANDBOXED(CancelVibrate(InSandbox() ? id : WindowIdentifier()));
  }
}

template <class InfoType>
class ObserversManager
{
public:
  void AddObserver(Observer<InfoType>* aObserver) {
    if (!mObservers) {
      mObservers = new mozilla::ObserverList<InfoType>();
    }

    mObservers->AddObserver(aObserver);

    if (mObservers->Length() == 1) {
      EnableNotifications();
    }
  }

  void RemoveObserver(Observer<InfoType>* aObserver) {
    bool removed = mObservers && mObservers->RemoveObserver(aObserver);
    if (!removed) {
      return;
    }

    if (mObservers->Length() == 0) {
      DisableNotifications();

      OnNotificationsDisabled();

      delete mObservers;
      mObservers = nullptr;
    }
  }

  void BroadcastInformation(const InfoType& aInfo) {
    // It is possible for mObservers to be nullptr here on some platforms,
    // because a call to BroadcastInformation gets queued up asynchronously
    // while RemoveObserver is running (and before the notifications are
    // disabled). The queued call can then get run after mObservers has
    // been nulled out. See bug 757025.
    if (!mObservers) {
      return;
    }
    mObservers->Broadcast(aInfo);
  }

protected:
  virtual void EnableNotifications() = 0;
  virtual void DisableNotifications() = 0;
  virtual void OnNotificationsDisabled() {}

private:
  mozilla::ObserverList<InfoType>* mObservers;
};

template <class InfoType>
class CachingObserversManager : public ObserversManager<InfoType>
{
public:
  InfoType GetCurrentInformation() {
    if (mHasValidCache) {
      return mInfo;
    }

    GetCurrentInformationInternal(&mInfo);
    mHasValidCache = true;
    return mInfo;
  }

  void CacheInformation(const InfoType& aInfo) {
    mHasValidCache = true;
    mInfo = aInfo;
  }

  void BroadcastCachedInformation() {
    this->BroadcastInformation(mInfo);
  }

protected:
  virtual void GetCurrentInformationInternal(InfoType*) = 0;

  void OnNotificationsDisabled() override {
    mHasValidCache = false;
  }

private:
  InfoType                mInfo;
  bool                    mHasValidCache;
};

class BatteryObserversManager : public CachingObserversManager<BatteryInformation>
{
protected:
  void EnableNotifications() override {
    PROXY_IF_SANDBOXED(EnableBatteryNotifications());
  }

  void DisableNotifications() override {
    PROXY_IF_SANDBOXED(DisableBatteryNotifications());
  }

  void GetCurrentInformationInternal(BatteryInformation* aInfo) override {
    PROXY_IF_SANDBOXED(GetCurrentBatteryInformation(aInfo));
  }
};

static BatteryObserversManager&
BatteryObservers()
{
  static BatteryObserversManager sBatteryObservers;
  AssertMainThread();
  return sBatteryObservers;
}

class NetworkObserversManager : public CachingObserversManager<NetworkInformation>
{
protected:
  void EnableNotifications() override {
    PROXY_IF_SANDBOXED(EnableNetworkNotifications());
  }

  void DisableNotifications() override {
    PROXY_IF_SANDBOXED(DisableNetworkNotifications());
  }

  void GetCurrentInformationInternal(NetworkInformation* aInfo) override {
    PROXY_IF_SANDBOXED(GetCurrentNetworkInformation(aInfo));
  }
};

static NetworkObserversManager&
NetworkObservers()
{
  static NetworkObserversManager sNetworkObservers;
  AssertMainThread();
  return sNetworkObservers;
}

class WakeLockObserversManager : public ObserversManager<WakeLockInformation>
{
protected:
  void EnableNotifications() override {
    PROXY_IF_SANDBOXED(EnableWakeLockNotifications());
  }

  void DisableNotifications() override {
    PROXY_IF_SANDBOXED(DisableWakeLockNotifications());
  }
};

static WakeLockObserversManager&
WakeLockObservers()
{
  static WakeLockObserversManager sWakeLockObservers;
  AssertMainThread();
  return sWakeLockObservers;
}

class ScreenConfigurationObserversManager : public CachingObserversManager<ScreenConfiguration>
{
protected:
  void EnableNotifications() override {
    PROXY_IF_SANDBOXED(EnableScreenConfigurationNotifications());
  }

  void DisableNotifications() override {
    PROXY_IF_SANDBOXED(DisableScreenConfigurationNotifications());
  }

  void GetCurrentInformationInternal(ScreenConfiguration* aInfo) override {
    PROXY_IF_SANDBOXED(GetCurrentScreenConfiguration(aInfo));
  }
};

static ScreenConfigurationObserversManager&
ScreenConfigurationObservers()
{
  AssertMainThread();
  static ScreenConfigurationObserversManager sScreenConfigurationObservers;
  return sScreenConfigurationObservers;
}

void
RegisterBatteryObserver(BatteryObserver* aObserver)
{
  AssertMainThread();
  BatteryObservers().AddObserver(aObserver);
}

void
UnregisterBatteryObserver(BatteryObserver* aObserver)
{
  AssertMainThread();
  BatteryObservers().RemoveObserver(aObserver);
}

void
GetCurrentBatteryInformation(BatteryInformation* aInfo)
{
  AssertMainThread();
  *aInfo = BatteryObservers().GetCurrentInformation();
}

void
NotifyBatteryChange(const BatteryInformation& aInfo)
{
  AssertMainThread();
  BatteryObservers().CacheInformation(aInfo);
  BatteryObservers().BroadcastCachedInformation();
}

void
EnableSensorNotifications(SensorType aSensor) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(EnableSensorNotifications(aSensor));
}

void
DisableSensorNotifications(SensorType aSensor) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(DisableSensorNotifications(aSensor));
}

typedef mozilla::ObserverList<SensorData> SensorObserverList;
static SensorObserverList* gSensorObservers = nullptr;

static SensorObserverList &
GetSensorObservers(SensorType sensor_type) {
  MOZ_ASSERT(sensor_type < NUM_SENSOR_TYPE);

  if(!gSensorObservers) {
    gSensorObservers = new SensorObserverList[NUM_SENSOR_TYPE];
  }
  return gSensorObservers[sensor_type];
}

void
RegisterSensorObserver(SensorType aSensor, ISensorObserver *aObserver) {
  SensorObserverList &observers = GetSensorObservers(aSensor);

  AssertMainThread();

  observers.AddObserver(aObserver);
  if(observers.Length() == 1) {
    EnableSensorNotifications(aSensor);
  }
}

void
UnregisterSensorObserver(SensorType aSensor, ISensorObserver *aObserver) {
  AssertMainThread();

  if (!gSensorObservers) {
    return;
  }

  SensorObserverList &observers = GetSensorObservers(aSensor);
  if (!observers.RemoveObserver(aObserver) || observers.Length() > 0) {
    return;
  }
  DisableSensorNotifications(aSensor);

  // Destroy sSensorObservers only if all observer lists are empty.
  for (int i = 0; i < NUM_SENSOR_TYPE; i++) {
    if (gSensorObservers[i].Length() > 0) {
      return;
    }
  }
  delete [] gSensorObservers;
  gSensorObservers = nullptr;
}

void
NotifySensorChange(const SensorData &aSensorData) {
  SensorObserverList &observers = GetSensorObservers(aSensorData.sensor());

  AssertMainThread();

  observers.Broadcast(aSensorData);
}

void
RegisterNetworkObserver(NetworkObserver* aObserver)
{
  AssertMainThread();
  NetworkObservers().AddObserver(aObserver);
}

void
UnregisterNetworkObserver(NetworkObserver* aObserver)
{
  AssertMainThread();
  NetworkObservers().RemoveObserver(aObserver);
}

void
GetCurrentNetworkInformation(NetworkInformation* aInfo)
{
  AssertMainThread();
  *aInfo = NetworkObservers().GetCurrentInformation();
}

void
NotifyNetworkChange(const NetworkInformation& aInfo)
{
  NetworkObservers().CacheInformation(aInfo);
  NetworkObservers().BroadcastCachedInformation();
}

void
RegisterWakeLockObserver(WakeLockObserver* aObserver)
{
  AssertMainThread();
  WakeLockObservers().AddObserver(aObserver);
}

void
UnregisterWakeLockObserver(WakeLockObserver* aObserver)
{
  AssertMainThread();
  WakeLockObservers().RemoveObserver(aObserver);
}

void
ModifyWakeLock(const nsAString& aTopic,
               WakeLockControl aLockAdjust,
               WakeLockControl aHiddenAdjust,
               uint64_t aProcessID /* = CONTENT_PROCESS_ID_UNKNOWN */)
{
  AssertMainThread();

  if (aProcessID == CONTENT_PROCESS_ID_UNKNOWN) {
    aProcessID = InSandbox() ? ContentChild::GetSingleton()->GetID() :
                               CONTENT_PROCESS_ID_MAIN;
  }

  PROXY_IF_SANDBOXED(ModifyWakeLock(aTopic, aLockAdjust,
                                    aHiddenAdjust, aProcessID));
}

void
GetWakeLockInfo(const nsAString& aTopic, WakeLockInformation* aWakeLockInfo)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(GetWakeLockInfo(aTopic, aWakeLockInfo));
}

void
NotifyWakeLockChange(const WakeLockInformation& aInfo)
{
  AssertMainThread();
  WakeLockObservers().BroadcastInformation(aInfo);
}

void
RegisterScreenConfigurationObserver(ScreenConfigurationObserver* aObserver)
{
  AssertMainThread();
  ScreenConfigurationObservers().AddObserver(aObserver);
}

void
UnregisterScreenConfigurationObserver(ScreenConfigurationObserver* aObserver)
{
  AssertMainThread();
  ScreenConfigurationObservers().RemoveObserver(aObserver);
}

void
GetCurrentScreenConfiguration(ScreenConfiguration* aScreenConfiguration)
{
  AssertMainThread();
  *aScreenConfiguration = ScreenConfigurationObservers().GetCurrentInformation();
}

void
NotifyScreenConfigurationChange(const ScreenConfiguration& aScreenConfiguration)
{
  ScreenConfigurationObservers().CacheInformation(aScreenConfiguration);
  ScreenConfigurationObservers().BroadcastCachedInformation();
}

bool
LockScreenOrientation(const dom::ScreenOrientationInternal& aOrientation)
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(LockScreenOrientation(aOrientation), false);
}

void
UnlockScreenOrientation()
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(UnlockScreenOrientation());
}

void
EnableSwitchNotifications(SwitchDevice aDevice) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(EnableSwitchNotifications(aDevice));
}

void
DisableSwitchNotifications(SwitchDevice aDevice) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(DisableSwitchNotifications(aDevice));
}

typedef mozilla::ObserverList<SwitchEvent> SwitchObserverList;

static SwitchObserverList *sSwitchObserverLists = nullptr;

static SwitchObserverList&
GetSwitchObserverList(SwitchDevice aDevice) {
  MOZ_ASSERT(0 <= aDevice && aDevice < NUM_SWITCH_DEVICE);
  if (sSwitchObserverLists == nullptr) {
    sSwitchObserverLists = new SwitchObserverList[NUM_SWITCH_DEVICE];
  }
  return sSwitchObserverLists[aDevice];
}

static void
ReleaseObserversIfNeeded() {
  for (int i = 0; i < NUM_SWITCH_DEVICE; i++) {
    if (sSwitchObserverLists[i].Length() != 0)
      return;
  }

  //The length of every list is 0, no observer in the list.
  delete [] sSwitchObserverLists;
  sSwitchObserverLists = nullptr;
}

void
RegisterSwitchObserver(SwitchDevice aDevice, SwitchObserver *aObserver)
{
  AssertMainThread();
  SwitchObserverList& observer = GetSwitchObserverList(aDevice);
  observer.AddObserver(aObserver);
  if (observer.Length() == 1) {
    EnableSwitchNotifications(aDevice);
  }
}

void
UnregisterSwitchObserver(SwitchDevice aDevice, SwitchObserver *aObserver)
{
  AssertMainThread();

  if (!sSwitchObserverLists) {
    return;
  }

  SwitchObserverList& observer = GetSwitchObserverList(aDevice);
  if (!observer.RemoveObserver(aObserver) || observer.Length() > 0) {
    return;
  }

  DisableSwitchNotifications(aDevice);
  ReleaseObserversIfNeeded();
}

void
NotifySwitchChange(const SwitchEvent& aEvent)
{
  // When callback this notification, main thread may call unregister function
  // first. We should check if this pointer is valid.
  if (!sSwitchObserverLists)
    return;

  SwitchObserverList& observer = GetSwitchObserverList(aEvent.device());
  observer.Broadcast(aEvent);
}

bool
SetProcessPrioritySupported()
{
  RETURN_PROXY_IF_SANDBOXED(SetProcessPrioritySupported(), false);
}

void
SetProcessPriority(int aPid, ProcessPriority aPriority)
{
  // n.b. The sandboxed implementation crashes; SetProcessPriority works only
  // from the main process.
  PROXY_IF_SANDBOXED(SetProcessPriority(aPid, aPriority));
}

// From HalTypes.h.
const char*
ProcessPriorityToString(ProcessPriority aPriority)
{
  switch (aPriority) {
  case PROCESS_PRIORITY_MASTER:
    return "MASTER";
  case PROCESS_PRIORITY_PREALLOC:
    return "PREALLOC";
  case PROCESS_PRIORITY_FOREGROUND_HIGH:
    return "FOREGROUND_HIGH";
  case PROCESS_PRIORITY_FOREGROUND:
    return "FOREGROUND";
  case PROCESS_PRIORITY_FOREGROUND_KEYBOARD:
    return "FOREGROUND_KEYBOARD";
  case PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE:
    return "BACKGROUND_PERCEIVABLE";
  case PROCESS_PRIORITY_BACKGROUND:
    return "BACKGROUND";
  case PROCESS_PRIORITY_UNKNOWN:
    return "UNKNOWN";
  default:
    MOZ_ASSERT(false);
    return "???";
  }
}

void
StartDiskSpaceWatcher()
{
  AssertMainProcess();
  AssertMainThread();
  PROXY_IF_SANDBOXED(StartDiskSpaceWatcher());
}

void
StopDiskSpaceWatcher()
{
  AssertMainProcess();
  AssertMainThread();
  PROXY_IF_SANDBOXED(StopDiskSpaceWatcher());
}

} // namespace hal
} // namespace mozilla
