/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalImpl.h"
#include "HalSandbox.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/Observer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindow.h"
#include "mozilla/Services.h"
#include "nsIWebNavigation.h"
#include "nsITabChild.h"
#include "nsIDocShell.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "WindowIdentifier.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"

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

PRLogModuleInfo *
GetHalLog()
{
  static PRLogModuleInfo *sHalLog;
  if (!sHalLog) {
    sHalLog = PR_NewLogModule("hal");
  }
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
WindowIsActive(nsIDOMWindow* aWindow)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(window, false);

  nsIDocument* document = window->GetDoc();
  NS_ENSURE_TRUE(document, false);

  return !document->Hidden();
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
      NS_WARNING("RemoveObserver() called for unregistered observer");
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
  RETURN_PROXY_IF_SANDBOXED(GetScreenEnabled(), false);
}

void SetScreenEnabled(bool aEnabled)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetScreenEnabled(aEnabled));
}

bool GetKeyLightEnabled()
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetKeyLightEnabled(), false);
}

void SetKeyLightEnabled(bool aEnabled)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetKeyLightEnabled(aEnabled));
}

bool GetCpuSleepAllowed()
{
  // Generally for interfaces that are accessible by normal web content
  // we should cache the result and be notified on state changes, like
  // what the battery API does. But since this is only used by
  // privileged interface, the synchronous getter is OK here.
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetCpuSleepAllowed(), true);
}

void SetCpuSleepAllowed(bool aAllowed)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetCpuSleepAllowed(aAllowed));
}

double GetScreenBrightness()
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetScreenBrightness(), 0);
}

void SetScreenBrightness(double aBrightness)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetScreenBrightness(clamped(aBrightness, 0.0, 1.0)));
}

bool SetLight(LightType light, const LightConfiguration& aConfig)
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(SetLight(light, aConfig), false);
}

bool GetLight(LightType light, LightConfiguration* aConfig)
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetLight(light, aConfig), false);
}

class SystemClockChangeObserversManager : public ObserversManager<int64_t>
{
protected:
  void EnableNotifications() {
    PROXY_IF_SANDBOXED(EnableSystemClockChangeNotifications());
  }

  void DisableNotifications() {
    PROXY_IF_SANDBOXED(DisableSystemClockChangeNotifications());
  }
};

static SystemClockChangeObserversManager sSystemClockChangeObservers;

void
RegisterSystemClockChangeObserver(SystemClockChangeObserver* aObserver)
{
  AssertMainThread();
  sSystemClockChangeObservers.AddObserver(aObserver);
}

void
UnregisterSystemClockChangeObserver(SystemClockChangeObserver* aObserver)
{
  AssertMainThread();
  sSystemClockChangeObservers.RemoveObserver(aObserver);
}

void
NotifySystemClockChange(const int64_t& aClockDeltaMS)
{
  sSystemClockChangeObservers.BroadcastInformation(aClockDeltaMS);
}

class SystemTimezoneChangeObserversManager : public ObserversManager<SystemTimezoneChangeInformation>
{
protected:
  void EnableNotifications() {
    PROXY_IF_SANDBOXED(EnableSystemTimezoneChangeNotifications());
  }

  void DisableNotifications() {
    PROXY_IF_SANDBOXED(DisableSystemTimezoneChangeNotifications());
  }
};

static SystemTimezoneChangeObserversManager sSystemTimezoneChangeObservers;

void
RegisterSystemTimezoneChangeObserver(SystemTimezoneChangeObserver* aObserver)
{
  AssertMainThread();
  sSystemTimezoneChangeObservers.AddObserver(aObserver);
}

void
UnregisterSystemTimezoneChangeObserver(SystemTimezoneChangeObserver* aObserver)
{
  AssertMainThread();
  sSystemTimezoneChangeObservers.RemoveObserver(aObserver);
}

void
NotifySystemTimezoneChange(const SystemTimezoneChangeInformation& aSystemTimezoneChangeInfo)
{
  sSystemTimezoneChangeObservers.BroadcastInformation(aSystemTimezoneChangeInfo);
}

void 
AdjustSystemClock(int64_t aDeltaMilliseconds)
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

int32_t
GetTimezoneOffset()
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetTimezoneOffset(), 0);
}

nsCString
GetTimezone()
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetTimezone(), nsCString(""));
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
  AssertMainProcess();
  AssertMainThread();
  PROXY_IF_SANDBOXED(Reboot());
}

void PowerOff()
{
  AssertMainProcess();
  AssertMainThread();
  PROXY_IF_SANDBOXED(PowerOff());
}

void StartForceQuitWatchdog(ShutdownMode aMode, int32_t aTimeoutSecs)
{
  AssertMainProcess();
  AssertMainThread();
  PROXY_IF_SANDBOXED(StartForceQuitWatchdog(aMode, aTimeoutSecs));
}

void StartMonitoringGamepadStatus()
{
  PROXY_IF_SANDBOXED(StartMonitoringGamepadStatus());
}

void StopMonitoringGamepadStatus()
{
  PROXY_IF_SANDBOXED(StopMonitoringGamepadStatus());
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

SwitchState GetCurrentSwitchState(SwitchDevice aDevice)
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetCurrentSwitchState(aDevice), SWITCH_STATE_UNKNOWN);
}

void NotifySwitchStateFromInputDevice(SwitchDevice aDevice, SwitchState aState)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(NotifySwitchStateFromInputDevice(aDevice, aState));
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

static AlarmObserver* sAlarmObserver;

bool
RegisterTheOneAlarmObserver(AlarmObserver* aObserver)
{
  MOZ_ASSERT(!InSandbox());
  MOZ_ASSERT(!sAlarmObserver);

  sAlarmObserver = aObserver;
  RETURN_PROXY_IF_SANDBOXED(EnableAlarm(), false);
}

void
UnregisterTheOneAlarmObserver()
{
  if (sAlarmObserver) {
    sAlarmObserver = nullptr;
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
SetAlarm(int32_t aSeconds, int32_t aNanoseconds)
{
  // It's pointless to program an alarm nothing is going to observe ...
  MOZ_ASSERT(sAlarmObserver);
  RETURN_PROXY_IF_SANDBOXED(SetAlarm(aSeconds, aNanoseconds), false);
}

void
SetProcessPriority(int aPid,
                   ProcessPriority aPriority,
                   ProcessCPUPriority aCPUPriority,
                   uint32_t aBackgroundLRU)
{
  // n.b. The sandboxed implementation crashes; SetProcessPriority works only
  // from the main process.
  MOZ_ASSERT(aBackgroundLRU == 0 || aPriority == PROCESS_PRIORITY_BACKGROUND);
  PROXY_IF_SANDBOXED(SetProcessPriority(aPid, aPriority, aCPUPriority,
                                        aBackgroundLRU));
}

void
SetCurrentThreadPriority(hal::ThreadPriority aThreadPriority)
{
  PROXY_IF_SANDBOXED(SetCurrentThreadPriority(aThreadPriority));
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
  case PROCESS_PRIORITY_BACKGROUND_HOMESCREEN:
    return "BACKGROUND_HOMESCREEN";
  case PROCESS_PRIORITY_BACKGROUND:
    return "BACKGROUND";
  case PROCESS_PRIORITY_UNKNOWN:
    return "UNKNOWN";
  default:
    MOZ_ASSERT(false);
    return "???";
  }
}

const char *
ThreadPriorityToString(ThreadPriority aPriority)
{
  switch (aPriority) {
    case THREAD_PRIORITY_COMPOSITOR:
      return "COMPOSITOR";
    default:
      MOZ_ASSERT(false);
      return "???";
  }
}

// From HalTypes.h.
const char*
ProcessPriorityToString(ProcessPriority aPriority,
                        ProcessCPUPriority aCPUPriority)
{
  // Sorry this is ugly.  At least it's all in one place.
  //
  // We intentionally fall through if aCPUPriority is invalid; we won't hit any
  // of the if statements further down, so it's OK.

  switch (aPriority) {
  case PROCESS_PRIORITY_MASTER:
    if (aCPUPriority == PROCESS_CPU_PRIORITY_NORMAL) {
      return "MASTER:CPU_NORMAL";
    }
    if (aCPUPriority == PROCESS_CPU_PRIORITY_LOW) {
      return "MASTER:CPU_LOW";
    }
  case PROCESS_PRIORITY_PREALLOC:
    if (aCPUPriority == PROCESS_CPU_PRIORITY_NORMAL) {
      return "PREALLOC:CPU_NORMAL";
    }
    if (aCPUPriority == PROCESS_CPU_PRIORITY_LOW) {
      return "PREALLOC:CPU_LOW";
    }
  case PROCESS_PRIORITY_FOREGROUND_HIGH:
    if (aCPUPriority == PROCESS_CPU_PRIORITY_NORMAL) {
      return "FOREGROUND_HIGH:CPU_NORMAL";
    }
    if (aCPUPriority == PROCESS_CPU_PRIORITY_LOW) {
      return "FOREGROUND_HIGH:CPU_LOW";
    }
  case PROCESS_PRIORITY_FOREGROUND:
    if (aCPUPriority == PROCESS_CPU_PRIORITY_NORMAL) {
      return "FOREGROUND:CPU_NORMAL";
    }
    if (aCPUPriority == PROCESS_CPU_PRIORITY_LOW) {
      return "FOREGROUND:CPU_LOW";
    }
  case PROCESS_PRIORITY_FOREGROUND_KEYBOARD:
    if (aCPUPriority == PROCESS_CPU_PRIORITY_NORMAL) {
      return "FOREGROUND_KEYBOARD:CPU_NORMAL";
    }
    if (aCPUPriority == PROCESS_CPU_PRIORITY_LOW) {
      return "FOREGROUND_KEYBOARD:CPU_LOW";
    }
  case PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE:
    if (aCPUPriority == PROCESS_CPU_PRIORITY_NORMAL) {
      return "BACKGROUND_PERCEIVABLE:CPU_NORMAL";
    }
    if (aCPUPriority == PROCESS_CPU_PRIORITY_LOW) {
      return "BACKGROUND_PERCEIVABLE:CPU_LOW";
    }
  case PROCESS_PRIORITY_BACKGROUND_HOMESCREEN:
    if (aCPUPriority == PROCESS_CPU_PRIORITY_NORMAL) {
      return "BACKGROUND_HOMESCREEN:CPU_NORMAL";
    }
    if (aCPUPriority == PROCESS_CPU_PRIORITY_LOW) {
      return "BACKGROUND_HOMESCREEN:CPU_LOW";
    }
  case PROCESS_PRIORITY_BACKGROUND:
    if (aCPUPriority == PROCESS_CPU_PRIORITY_NORMAL) {
      return "BACKGROUND:CPU_NORMAL";
    }
    if (aCPUPriority == PROCESS_CPU_PRIORITY_LOW) {
      return "BACKGROUND:CPU_LOW";
    }
  case PROCESS_PRIORITY_UNKNOWN:
    if (aCPUPriority == PROCESS_CPU_PRIORITY_NORMAL) {
      return "UNKNOWN:CPU_NORMAL";
    }
    if (aCPUPriority == PROCESS_CPU_PRIORITY_LOW) {
      return "UNKNOWN:CPU_LOW";
    }
  default:
    // Fall through.  (|default| is here to silence warnings.)
    break;
  }

  MOZ_ASSERT(false);
  return "???";
}

static StaticAutoPtr<ObserverList<FMRadioOperationInformation> > sFMRadioObservers;

static void
InitializeFMRadioObserver()
{
  if (!sFMRadioObservers) {
    sFMRadioObservers = new ObserverList<FMRadioOperationInformation>;
    ClearOnShutdown(&sFMRadioObservers);
  }
}

void
RegisterFMRadioObserver(FMRadioObserver* aFMRadioObserver) {
  AssertMainThread();
  InitializeFMRadioObserver();
  sFMRadioObservers->AddObserver(aFMRadioObserver);
}

void
UnregisterFMRadioObserver(FMRadioObserver* aFMRadioObserver) {
  AssertMainThread();
  InitializeFMRadioObserver();
  sFMRadioObservers->RemoveObserver(aFMRadioObserver);
}

void
NotifyFMRadioStatus(const FMRadioOperationInformation& aFMRadioState) {
  InitializeFMRadioObserver();
  sFMRadioObservers->Broadcast(aFMRadioState);
}

void
EnableFMRadio(const FMRadioSettings& aInfo) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(EnableFMRadio(aInfo));
}

void
DisableFMRadio() {
  AssertMainThread();
  PROXY_IF_SANDBOXED(DisableFMRadio());
}

void
FMRadioSeek(const FMRadioSeekDirection& aDirection) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(FMRadioSeek(aDirection));
}

void
GetFMRadioSettings(FMRadioSettings* aInfo) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(GetFMRadioSettings(aInfo));
}

void
SetFMRadioFrequency(const uint32_t aFrequency) {
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetFMRadioFrequency(aFrequency));
}

uint32_t
GetFMRadioFrequency() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetFMRadioFrequency(), 0);
}

bool
IsFMRadioOn() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(IsFMRadioOn(), false);
}

uint32_t
GetFMRadioSignalStrength() {
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetFMRadioSignalStrength(), 0);
}

void
CancelFMRadioSeek() {
  AssertMainThread();
  PROXY_IF_SANDBOXED(CancelFMRadioSeek());
}

FMRadioSettings
GetFMBandSettings(FMRadioCountry aCountry) {
  FMRadioSettings settings;

  switch (aCountry) {
    case FM_RADIO_COUNTRY_US:
    case FM_RADIO_COUNTRY_EU:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 87800;
      settings.spaceType() = 200;
      settings.preEmphasis() = 75;
      break;
    case FM_RADIO_COUNTRY_JP_STANDARD:
      settings.upperLimit() = 76000;
      settings.lowerLimit() = 90000;
      settings.spaceType() = 100;
      settings.preEmphasis() = 50;
      break;
    case FM_RADIO_COUNTRY_CY:
    case FM_RADIO_COUNTRY_DE:
    case FM_RADIO_COUNTRY_DK:
    case FM_RADIO_COUNTRY_ES:
    case FM_RADIO_COUNTRY_FI:
    case FM_RADIO_COUNTRY_FR:
    case FM_RADIO_COUNTRY_HU:
    case FM_RADIO_COUNTRY_IR:
    case FM_RADIO_COUNTRY_IT:
    case FM_RADIO_COUNTRY_KW:
    case FM_RADIO_COUNTRY_LT:
    case FM_RADIO_COUNTRY_ML:
    case FM_RADIO_COUNTRY_NO:
    case FM_RADIO_COUNTRY_OM:
    case FM_RADIO_COUNTRY_PG:
    case FM_RADIO_COUNTRY_NL:
    case FM_RADIO_COUNTRY_CZ:
    case FM_RADIO_COUNTRY_UK:
    case FM_RADIO_COUNTRY_RW:
    case FM_RADIO_COUNTRY_SN:
    case FM_RADIO_COUNTRY_SI:
    case FM_RADIO_COUNTRY_ZA:
    case FM_RADIO_COUNTRY_SE:
    case FM_RADIO_COUNTRY_CH:
    case FM_RADIO_COUNTRY_TW:
    case FM_RADIO_COUNTRY_UA:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 87500;
      settings.spaceType() = 100;
      settings.preEmphasis() = 50;
      break;
    case FM_RADIO_COUNTRY_VA:
    case FM_RADIO_COUNTRY_MA:
    case FM_RADIO_COUNTRY_TR:
      settings.upperLimit() = 10800;
      settings.lowerLimit() = 87500;
      settings.spaceType() = 100;
      settings.preEmphasis() = 75;
      break;
    case FM_RADIO_COUNTRY_AU:
    case FM_RADIO_COUNTRY_BD:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 87500;
      settings.spaceType() = 200;
      settings.preEmphasis() = 75;
      break;
    case FM_RADIO_COUNTRY_AW:
    case FM_RADIO_COUNTRY_BS:
    case FM_RADIO_COUNTRY_CO:
    case FM_RADIO_COUNTRY_KR:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 88000;
      settings.spaceType() = 200;
      settings.preEmphasis() = 75;
      break;
    case FM_RADIO_COUNTRY_EC:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 88000;
      settings.spaceType() = 200;
      settings.preEmphasis() = 0;
      break;
    case FM_RADIO_COUNTRY_GM:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 88000;
      settings.spaceType() = 0;
      settings.preEmphasis() = 75;
      break;
    case FM_RADIO_COUNTRY_QA:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 88000;
      settings.spaceType() = 200;
      settings.preEmphasis() = 50;
      break;
    case FM_RADIO_COUNTRY_SG:
      settings.upperLimit() = 108000;
      settings.lowerLimit() = 88000;
      settings.spaceType() = 200;
      settings.preEmphasis() = 50;
      break;
    case FM_RADIO_COUNTRY_IN:
      settings.upperLimit() = 100000;
      settings.lowerLimit() = 108000;
      settings.spaceType() = 100;
      settings.preEmphasis() = 50;
      break;
    case FM_RADIO_COUNTRY_NZ:
      settings.upperLimit() = 100000;
      settings.lowerLimit() = 88000;
      settings.spaceType() = 50;
      settings.preEmphasis() = 50;
      break;
    case FM_RADIO_COUNTRY_USER_DEFINED:
      break;
    default:
      MOZ_ASSERT(0);
      break;
    };
    return settings;
}

void FactoryReset(mozilla::dom::FactoryResetReason& aReason)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(FactoryReset(aReason));
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

uint32_t
GetTotalSystemMemory()
{
  return hal_impl::GetTotalSystemMemory();
}

uint32_t
GetTotalSystemMemoryLevel()
{
  return hal_impl::GetTotalSystemMemoryLevel();
}

} // namespace hal
} // namespace mozilla
