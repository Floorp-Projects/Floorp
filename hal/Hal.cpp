/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalImpl.h"
#include "HalSandbox.h"
#include "mozilla/Util.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/Observer.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "mozilla/Services.h"
#include "nsIWebNavigation.h"
#include "nsITabChild.h"
#include "nsIDocShell.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "WindowIdentifier.h"
#include "mozilla/dom/ScreenOrientation.h"

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#endif

using namespace mozilla::services;

#define PROXY_IF_SANDBOXED(_call)                 \
  do {                                            \
    if (InSandbox()) {                            \
      hal_sandbox::_call;                         \
    } else {                                      \
      hal_impl::_call;                            \
    }                                             \
  } while (0)

#define RETURN_PROXY_IF_SANDBOXED(_call)          \
  do {                                            \
    if (InSandbox()) {                            \
      return hal_sandbox::_call;                  \
    } else {                                      \
      return hal_impl::_call;                     \
    }                                             \
  } while (0)

namespace mozilla {
namespace hal {

PRLogModuleInfo *sHalLog = PR_LOG_DEFINE("hal");

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

bool
WindowIsActive(nsIDOMWindow *window)
{
  NS_ENSURE_TRUE(window, false);

  nsCOMPtr<nsIDOMDocument> doc;
  window->GetDocument(getter_AddRefs(doc));
  NS_ENSURE_TRUE(doc, false);

  bool hidden = true;
  doc->GetMozHidden(&hidden);
  return !hidden;
}

StaticAutoPtr<WindowIdentifier::IDArrayType> gLastIDToVibrate;

void InitLastIDToVibrate()
{
  gLastIDToVibrate = new WindowIdentifier::IDArrayType();
  ClearOnShutdown(&gLastIDToVibrate);
}

} // anonymous namespace

void
Vibrate(const nsTArray<uint32_t>& pattern, nsIDOMWindow* window)
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
    HAL_LOG(("Vibrate: Window is inactive, dropping vibrate."));
    return;
  }

  if (InSandbox()) {
    hal_sandbox::Vibrate(pattern, id);
  }
  else {
    if (!gLastIDToVibrate)
      InitLastIDToVibrate();
    *gLastIDToVibrate = id.AsArray();

    HAL_LOG(("Vibrate: Forwarding to hal_impl."));

    // hal_impl doesn't need |id|. Send it an empty id, which will
    // assert if it's used.
    hal_impl::Vibrate(pattern, WindowIdentifier());
  }
}

void
CancelVibrate(nsIDOMWindow* window)
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

  if (InSandbox()) {
    hal_sandbox::CancelVibrate(id);
  }
  else if (*gLastIDToVibrate == id.AsArray()) {
    // Don't forward our ID to hal_impl. It doesn't need it, and we
    // don't want it to be tempted to read it.  The empty identifier
    // will assert if it's used.
    HAL_LOG(("CancelVibrate: Forwarding to hal_impl."));
    hal_impl::CancelVibrate(WindowIdentifier());
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
    // If mObservers is null, that means there are no observers.
    // In addition, if RemoveObserver() returns false, that means we didn't
    // find the observer.
    // In both cases, that is a logical error we want to make sure the developer
    // notices.

    MOZ_ASSERT(mObservers);

#ifndef DEBUG
    if (!mObservers) {
      return;
    }
#endif

    DebugOnly<bool> removed = mObservers->RemoveObserver(aObserver);
    MOZ_ASSERT(removed);

    if (mObservers->Length() == 0) {
      DisableNotifications();

      OnNotificationsDisabled();

      delete mObservers;
      mObservers = 0;
    }
  }

  void BroadcastInformation(const InfoType& aInfo) {
    // It is possible for mObservers to be NULL here on some platforms,
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

  virtual void OnNotificationsDisabled() {
    mHasValidCache = false;
  }

private:
  InfoType                mInfo;
  bool                    mHasValidCache;
};

class BatteryObserversManager : public CachingObserversManager<BatteryInformation>
{
protected:
  void EnableNotifications() {
    PROXY_IF_SANDBOXED(EnableBatteryNotifications());
  }

  void DisableNotifications() {
    PROXY_IF_SANDBOXED(DisableBatteryNotifications());
  }

  void GetCurrentInformationInternal(BatteryInformation* aInfo) {
    PROXY_IF_SANDBOXED(GetCurrentBatteryInformation(aInfo));
  }
};

static BatteryObserversManager sBatteryObservers;

class NetworkObserversManager : public CachingObserversManager<NetworkInformation>
{
protected:
  void EnableNotifications() {
    PROXY_IF_SANDBOXED(EnableNetworkNotifications());
  }

  void DisableNotifications() {
    PROXY_IF_SANDBOXED(DisableNetworkNotifications());
  }

  void GetCurrentInformationInternal(NetworkInformation* aInfo) {
    PROXY_IF_SANDBOXED(GetCurrentNetworkInformation(aInfo));
  }
};

static NetworkObserversManager sNetworkObservers;

class WakeLockObserversManager : public ObserversManager<WakeLockInformation>
{
protected:
  void EnableNotifications() {
    PROXY_IF_SANDBOXED(EnableWakeLockNotifications());
  }

  void DisableNotifications() {
    PROXY_IF_SANDBOXED(DisableWakeLockNotifications());
  }
};

static WakeLockObserversManager sWakeLockObservers;

class ScreenConfigurationObserversManager : public CachingObserversManager<ScreenConfiguration>
{
protected:
  void EnableNotifications() {
    PROXY_IF_SANDBOXED(EnableScreenConfigurationNotifications());
  }

  void DisableNotifications() {
    PROXY_IF_SANDBOXED(DisableScreenConfigurationNotifications());
  }

  void GetCurrentInformationInternal(ScreenConfiguration* aInfo) {
    PROXY_IF_SANDBOXED(GetCurrentScreenConfiguration(aInfo));
  }
};

static ScreenConfigurationObserversManager sScreenConfigurationObservers;

void
RegisterBatteryObserver(BatteryObserver* aObserver)
{
  AssertMainThread();
  sBatteryObservers.AddObserver(aObserver);
}

void
UnregisterBatteryObserver(BatteryObserver* aObserver)
{
  AssertMainThread();
  sBatteryObservers.RemoveObserver(aObserver);
}

void
GetCurrentBatteryInformation(BatteryInformation* aInfo)
{
  AssertMainThread();
  *aInfo = sBatteryObservers.GetCurrentInformation();
}

void
NotifyBatteryChange(const BatteryInformation& aInfo)
{
  AssertMainThread();
  sBatteryObservers.CacheInformation(aInfo);
  sBatteryObservers.BroadcastCachedInformation();
}

bool GetScreenEnabled()
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetScreenEnabled());
}

void SetScreenEnabled(bool enabled)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetScreenEnabled(enabled));
}

bool GetCpuSleepAllowed()
{
  // Generally for interfaces that are accessible by normal web content
  // we should cache the result and be notified on state changes, like
  // what the battery API does. But since this is only used by
  // privileged interface, the synchronous getter is OK here.
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetCpuSleepAllowed());
}

void SetCpuSleepAllowed(bool allowed)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetCpuSleepAllowed(allowed));
}

double GetScreenBrightness()
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetScreenBrightness());
}

void SetScreenBrightness(double brightness)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetScreenBrightness(clamped(brightness, 0.0, 1.0)));
}

bool SetLight(LightType light, const hal::LightConfiguration& aConfig)
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(SetLight(light, aConfig));
}

bool GetLight(LightType light, hal::LightConfiguration* aConfig)
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetLight(light, aConfig));
}


void 
AdjustSystemClock(int32_t aDeltaMilliseconds)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(AdjustSystemClock(aDeltaMilliseconds));
}

void 
SetTimezone(const nsCString& aTimezoneSpec)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetTimezone(aTimezoneSpec));
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
static SensorObserverList* gSensorObservers = NULL;

static SensorObserverList &
GetSensorObservers(SensorType sensor_type) {
  MOZ_ASSERT(sensor_type < NUM_SENSOR_TYPE);
  
  if(gSensorObservers == NULL)
    gSensorObservers = new SensorObserverList[NUM_SENSOR_TYPE];
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
  SensorObserverList &observers = GetSensorObservers(aSensor);

  AssertMainThread();
  
  observers.RemoveObserver(aObserver);
  if (observers.Length() > 0) {
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
  sNetworkObservers.AddObserver(aObserver);
}

void
UnregisterNetworkObserver(NetworkObserver* aObserver)
{
  AssertMainThread();
  sNetworkObservers.RemoveObserver(aObserver);
}

void
GetCurrentNetworkInformation(NetworkInformation* aInfo)
{
  AssertMainThread();
  *aInfo = sNetworkObservers.GetCurrentInformation();
}

void
NotifyNetworkChange(const NetworkInformation& aInfo)
{
  sNetworkObservers.CacheInformation(aInfo);
  sNetworkObservers.BroadcastCachedInformation();
}

void Reboot()
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(Reboot());
}

void PowerOff()
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(PowerOff());
}

void
RegisterWakeLockObserver(WakeLockObserver* aObserver)
{
  AssertMainThread();
  sWakeLockObservers.AddObserver(aObserver);
}

void
UnregisterWakeLockObserver(WakeLockObserver* aObserver)
{
  AssertMainThread();
  sWakeLockObservers.RemoveObserver(aObserver);
}

void
ModifyWakeLock(const nsAString &aTopic,
               hal::WakeLockControl aLockAdjust,
               hal::WakeLockControl aHiddenAdjust)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(ModifyWakeLock(aTopic, aLockAdjust, aHiddenAdjust));
}

void
GetWakeLockInfo(const nsAString &aTopic, WakeLockInformation *aWakeLockInfo)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(GetWakeLockInfo(aTopic, aWakeLockInfo));
}

void
NotifyWakeLockChange(const WakeLockInformation& aInfo)
{
  AssertMainThread();
  sWakeLockObservers.BroadcastInformation(aInfo);
}

void
RegisterScreenConfigurationObserver(ScreenConfigurationObserver* aObserver)
{
  AssertMainThread();
  sScreenConfigurationObservers.AddObserver(aObserver);
}

void
UnregisterScreenConfigurationObserver(ScreenConfigurationObserver* aObserver)
{
  AssertMainThread();
  sScreenConfigurationObservers.RemoveObserver(aObserver);
}

void
GetCurrentScreenConfiguration(ScreenConfiguration* aScreenConfiguration)
{
  AssertMainThread();
  *aScreenConfiguration = sScreenConfigurationObservers.GetCurrentInformation();
}

void
NotifyScreenConfigurationChange(const ScreenConfiguration& aScreenConfiguration)
{
  sScreenConfigurationObservers.CacheInformation(aScreenConfiguration);
  sScreenConfigurationObservers.BroadcastCachedInformation();
}

bool
LockScreenOrientation(const dom::ScreenOrientation& aOrientation)
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(LockScreenOrientation(aOrientation));
}

void
UnlockScreenOrientation()
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(UnlockScreenOrientation());
}

void
EnableSwitchNotifications(hal::SwitchDevice aDevice) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(EnableSwitchNotifications(aDevice));
}

void
DisableSwitchNotifications(hal::SwitchDevice aDevice) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(DisableSwitchNotifications(aDevice));
}

hal::SwitchState GetCurrentSwitchState(hal::SwitchDevice aDevice)
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetCurrentSwitchState(aDevice));
}

typedef mozilla::ObserverList<SwitchEvent> SwitchObserverList;

static SwitchObserverList *sSwitchObserverLists = NULL;

static SwitchObserverList&
GetSwitchObserverList(hal::SwitchDevice aDevice) {
  MOZ_ASSERT(0 <= aDevice && aDevice < NUM_SWITCH_DEVICE); 
  if (sSwitchObserverLists == NULL) {
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
  sSwitchObserverLists = NULL;
}

void
RegisterSwitchObserver(hal::SwitchDevice aDevice, hal::SwitchObserver *aObserver)
{
  AssertMainThread();
  SwitchObserverList& observer = GetSwitchObserverList(aDevice);
  observer.AddObserver(aObserver);
  if (observer.Length() == 1) {
    EnableSwitchNotifications(aDevice);
  }
}

void
UnregisterSwitchObserver(hal::SwitchDevice aDevice, hal::SwitchObserver *aObserver)
{
  AssertMainThread();
  SwitchObserverList& observer = GetSwitchObserverList(aDevice);
  observer.RemoveObserver(aObserver);
  if (observer.Length() == 0) {
    DisableSwitchNotifications(aDevice);
    ReleaseObserversIfNeeded();
  }
}

void
NotifySwitchChange(const hal::SwitchEvent& aEvent)
{
  // When callback this notification, main thread may call unregister function
  // first. We should check if this pointer is valid.
  if (!sSwitchObserverLists)
    return;

  SwitchObserverList& observer = GetSwitchObserverList(aEvent.device());
  observer.Broadcast(aEvent);
}

static AlarmObserver* sAlarmObserver;

bool
RegisterTheOneAlarmObserver(AlarmObserver* aObserver)
{
  MOZ_ASSERT(!InSandbox());
  MOZ_ASSERT(!sAlarmObserver);

  sAlarmObserver = aObserver;
  RETURN_PROXY_IF_SANDBOXED(EnableAlarm());
}

void
UnregisterTheOneAlarmObserver()
{
  if (sAlarmObserver) {
    sAlarmObserver = NULL;
    PROXY_IF_SANDBOXED(DisableAlarm());
  }
}

void
NotifyAlarmFired()
{
  if (sAlarmObserver) {
    sAlarmObserver->Notify(void_t());
  }
}

bool
SetAlarm(PRInt32 aSeconds, PRInt32 aNanoseconds)
{
  // It's pointless to program an alarm nothing is going to observe ...
  MOZ_ASSERT(sAlarmObserver);
  RETURN_PROXY_IF_SANDBOXED(SetAlarm(aSeconds, aNanoseconds));
}

void
SetProcessPriority(int aPid, ProcessPriority aPriority)
{
  if (InSandbox()) {
    hal_sandbox::SetProcessPriority(aPid, aPriority);
  }
  else {
    hal_impl::SetProcessPriority(aPid, aPriority);
  }
}

} // namespace hal
} // namespace mozilla
