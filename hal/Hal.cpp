/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

#include "HalImpl.h"
#include "HalLog.h"
#include "HalSandbox.h"
#include "HalWakeLockInternal.h"
#include "nsIDOMWindow.h"
#include "mozilla/dom/Document.h"
#include "nsIDocShell.h"
#include "nsIBrowserChild.h"
#include "nsIWebNavigation.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nsPIDOMWindow.h"
#include "nsJSUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Observer.h"
#include "mozilla/dom/ContentChild.h"
#include "WindowIdentifier.h"

#ifdef XP_WIN
#  include <process.h>
#  define getpid _getpid
#endif

using namespace mozilla::services;
using namespace mozilla::dom;

#define PROXY_IF_SANDBOXED(_call)              \
  do {                                         \
    if (InSandbox()) {                         \
      if (!hal_sandbox::HalChildDestroyed()) { \
        hal_sandbox::_call;                    \
      }                                        \
    } else {                                   \
      hal_impl::_call;                         \
    }                                          \
  } while (0)

#define RETURN_PROXY_IF_SANDBOXED(_call, defValue) \
  do {                                             \
    if (InSandbox()) {                             \
      if (hal_sandbox::HalChildDestroyed()) {      \
        return defValue;                           \
      }                                            \
      return hal_sandbox::_call;                   \
    } else {                                       \
      return hal_impl::_call;                      \
    }                                              \
  } while (0)

namespace mozilla {
namespace hal {

static bool sInitialized = false;

mozilla::LogModule* GetHalLog() {
  static mozilla::LazyLogModule sHalLog("hal");
  return sHalLog;
}

namespace {

void AssertMainThread() { MOZ_ASSERT(NS_IsMainThread()); }

bool InSandbox() { return GeckoProcessType_Content == XRE_GetProcessType(); }

bool WindowIsActive(nsPIDOMWindowInner* aWindow) {
  dom::Document* document = aWindow->GetDoc();
  NS_ENSURE_TRUE(document, false);
  return !document->Hidden();
}

StaticAutoPtr<WindowIdentifier::IDArrayType> gLastIDToVibrate;

static void RecordLastIDToVibrate(const WindowIdentifier& aId) {
  if (!InSandbox()) {
    *gLastIDToVibrate = aId.AsArray();
  }
}

static bool MayCancelVibration(const WindowIdentifier& aId) {
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

  return InSandbox() || (*gLastIDToVibrate == aId.AsArray());
}

}  // namespace

void Vibrate(const nsTArray<uint32_t>& pattern, nsPIDOMWindowInner* window) {
  Vibrate(pattern, WindowIdentifier(window));
}

void Vibrate(const nsTArray<uint32_t>& pattern, const WindowIdentifier& id) {
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

  RecordLastIDToVibrate(id);

  // Don't forward our ID if we are not in the sandbox, because hal_impl
  // doesn't need it, and we don't want it to be tempted to read it.  The
  // empty identifier will assert if it's used.
  PROXY_IF_SANDBOXED(Vibrate(pattern, InSandbox() ? id : WindowIdentifier()));
}

void CancelVibrate(nsPIDOMWindowInner* window) {
  CancelVibrate(WindowIdentifier(window));
}

void CancelVibrate(const WindowIdentifier& id) {
  AssertMainThread();

  if (MayCancelVibration(id)) {
    // Don't forward our ID if we are not in the sandbox, because hal_impl
    // doesn't need it, and we don't want it to be tempted to read it.  The
    // empty identifier will assert if it's used.
    PROXY_IF_SANDBOXED(CancelVibrate(InSandbox() ? id : WindowIdentifier()));
  }
}

template <class InfoType>
class ObserversManager {
 public:
  void AddObserver(Observer<InfoType>* aObserver) {
    mObservers.AddObserver(aObserver);

    if (mObservers.Length() == 1) {
      EnableNotifications();
    }
  }

  void RemoveObserver(Observer<InfoType>* aObserver) {
    bool removed = mObservers.RemoveObserver(aObserver);
    if (!removed) {
      return;
    }

    if (mObservers.Length() == 0) {
      DisableNotifications();
      OnNotificationsDisabled();
    }
  }

  void BroadcastInformation(const InfoType& aInfo) {
    mObservers.Broadcast(aInfo);
  }

 protected:
  ~ObserversManager() { MOZ_ASSERT(mObservers.Length() == 0); }

  virtual void EnableNotifications() = 0;
  virtual void DisableNotifications() = 0;
  virtual void OnNotificationsDisabled() {}

 private:
  mozilla::ObserverList<InfoType> mObservers;
};

template <class InfoType>
class CachingObserversManager : public ObserversManager<InfoType> {
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

  void BroadcastCachedInformation() { this->BroadcastInformation(mInfo); }

 protected:
  virtual void GetCurrentInformationInternal(InfoType*) = 0;

  void OnNotificationsDisabled() override { mHasValidCache = false; }

 private:
  InfoType mInfo;
  bool mHasValidCache;
};

class BatteryObserversManager final
    : public CachingObserversManager<BatteryInformation> {
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

class NetworkObserversManager final
    : public CachingObserversManager<NetworkInformation> {
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

class WakeLockObserversManager final
    : public ObserversManager<WakeLockInformation> {
 protected:
  void EnableNotifications() override {
    PROXY_IF_SANDBOXED(EnableWakeLockNotifications());
  }

  void DisableNotifications() override {
    PROXY_IF_SANDBOXED(DisableWakeLockNotifications());
  }
};

class ScreenConfigurationObserversManager final
    : public CachingObserversManager<ScreenConfiguration> {
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

typedef mozilla::ObserverList<SensorData> SensorObserverList;
StaticAutoPtr<SensorObserverList> sSensorObservers[NUM_SENSOR_TYPE];

static SensorObserverList* GetSensorObservers(SensorType sensor_type) {
  AssertMainThread();
  MOZ_ASSERT(sensor_type < NUM_SENSOR_TYPE);

  if (!sSensorObservers[sensor_type]) {
    sSensorObservers[sensor_type] = new SensorObserverList();
  }

  return sSensorObservers[sensor_type];
}

#define MOZ_IMPL_HAL_OBSERVER(name_)                             \
  StaticAutoPtr<name_##ObserversManager> s##name_##Observers;    \
                                                                 \
  static name_##ObserversManager* name_##Observers() {           \
    AssertMainThread();                                          \
                                                                 \
    if (!s##name_##Observers) {                                  \
      MOZ_ASSERT(sInitialized);                                  \
      s##name_##Observers = new name_##ObserversManager();       \
    }                                                            \
                                                                 \
    return s##name_##Observers;                                  \
  }                                                              \
                                                                 \
  void Register##name_##Observer(name_##Observer* aObserver) {   \
    AssertMainThread();                                          \
    name_##Observers()->AddObserver(aObserver);                  \
  }                                                              \
                                                                 \
  void Unregister##name_##Observer(name_##Observer* aObserver) { \
    AssertMainThread();                                          \
    name_##Observers()->RemoveObserver(aObserver);               \
  }

MOZ_IMPL_HAL_OBSERVER(Battery)

void GetCurrentBatteryInformation(BatteryInformation* aInfo) {
  *aInfo = BatteryObservers()->GetCurrentInformation();
}

void NotifyBatteryChange(const BatteryInformation& aInfo) {
  BatteryObservers()->CacheInformation(aInfo);
  BatteryObservers()->BroadcastCachedInformation();
}

void EnableSensorNotifications(SensorType aSensor) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(EnableSensorNotifications(aSensor));
}

void DisableSensorNotifications(SensorType aSensor) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(DisableSensorNotifications(aSensor));
}

void RegisterSensorObserver(SensorType aSensor, ISensorObserver* aObserver) {
  SensorObserverList* observers = GetSensorObservers(aSensor);

  observers->AddObserver(aObserver);
  if (observers->Length() == 1) {
    EnableSensorNotifications(aSensor);
  }
}

void UnregisterSensorObserver(SensorType aSensor, ISensorObserver* aObserver) {
  SensorObserverList* observers = GetSensorObservers(aSensor);
  if (!observers->RemoveObserver(aObserver) || observers->Length() > 0) {
    return;
  }
  DisableSensorNotifications(aSensor);
}

void NotifySensorChange(const SensorData& aSensorData) {
  SensorObserverList* observers = GetSensorObservers(aSensorData.sensor());

  observers->Broadcast(aSensorData);
}

MOZ_IMPL_HAL_OBSERVER(Network)

void GetCurrentNetworkInformation(NetworkInformation* aInfo) {
  *aInfo = NetworkObservers()->GetCurrentInformation();
}

void NotifyNetworkChange(const NetworkInformation& aInfo) {
  NetworkObservers()->CacheInformation(aInfo);
  NetworkObservers()->BroadcastCachedInformation();
}

MOZ_IMPL_HAL_OBSERVER(WakeLock)

void ModifyWakeLock(const nsAString& aTopic, WakeLockControl aLockAdjust,
                    WakeLockControl aHiddenAdjust,
                    uint64_t aProcessID /* = CONTENT_PROCESS_ID_UNKNOWN */) {
  AssertMainThread();

  if (aProcessID == CONTENT_PROCESS_ID_UNKNOWN) {
    aProcessID = InSandbox() ? ContentChild::GetSingleton()->GetID()
                             : CONTENT_PROCESS_ID_MAIN;
  }

  PROXY_IF_SANDBOXED(
      ModifyWakeLock(aTopic, aLockAdjust, aHiddenAdjust, aProcessID));
}

void GetWakeLockInfo(const nsAString& aTopic,
                     WakeLockInformation* aWakeLockInfo) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(GetWakeLockInfo(aTopic, aWakeLockInfo));
}

void NotifyWakeLockChange(const WakeLockInformation& aInfo) {
  AssertMainThread();
  WakeLockObservers()->BroadcastInformation(aInfo);
}

MOZ_IMPL_HAL_OBSERVER(ScreenConfiguration)

void GetCurrentScreenConfiguration(ScreenConfiguration* aScreenConfiguration) {
  *aScreenConfiguration =
      ScreenConfigurationObservers()->GetCurrentInformation();
}

void NotifyScreenConfigurationChange(
    const ScreenConfiguration& aScreenConfiguration) {
  ScreenConfigurationObservers()->CacheInformation(aScreenConfiguration);
  ScreenConfigurationObservers()->BroadcastCachedInformation();
}

bool LockScreenOrientation(const ScreenOrientation& aOrientation) {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(LockScreenOrientation(aOrientation), false);
}

void UnlockScreenOrientation() {
  AssertMainThread();
  PROXY_IF_SANDBOXED(UnlockScreenOrientation());
}

bool SetProcessPrioritySupported() {
  RETURN_PROXY_IF_SANDBOXED(SetProcessPrioritySupported(), false);
}

void SetProcessPriority(int aPid, ProcessPriority aPriority) {
  // n.b. The sandboxed implementation crashes; SetProcessPriority works only
  // from the main process.
  PROXY_IF_SANDBOXED(SetProcessPriority(aPid, aPriority));
}

// From HalTypes.h.
const char* ProcessPriorityToString(ProcessPriority aPriority) {
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

void Init() {
  MOZ_ASSERT(!sInitialized);

  if (!InSandbox()) {
    gLastIDToVibrate = new WindowIdentifier::IDArrayType();
  }

  WakeLockInit();

  sInitialized = true;
}

void Shutdown() {
  MOZ_ASSERT(sInitialized);

  gLastIDToVibrate = nullptr;

  sBatteryObservers = nullptr;
  sNetworkObservers = nullptr;
  sWakeLockObservers = nullptr;
  sScreenConfigurationObservers = nullptr;

  for (auto& sensorObserver : sSensorObservers) {
    sensorObserver = nullptr;
  }

  sInitialized = false;
}

}  // namespace hal
}  // namespace mozilla
