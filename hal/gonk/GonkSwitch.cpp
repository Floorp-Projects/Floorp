/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <sysutils/NetlinkEvent.h>

#include "base/message_loop.h"

#include "Hal.h"
#include "mozilla/Monitor.h"
#include "nsXULAppAPI.h"
#include "UeventPoller.h"

using namespace mozilla::hal;

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GonkSwitch" , ## args) 

namespace mozilla {
namespace hal_impl {

struct {const char* name; SwitchDevice device; } kSwitchNameMap[] = {
  { "h2w", SWITCH_HEADPHONES },
  { NULL, SWITCH_DEVICE_UNKNOWN },
};

static SwitchDevice
NameToDevice(const char* name) {
  for (int i = 0; kSwitchNameMap[i].device != SWITCH_DEVICE_UNKNOWN; i++) {
    if (strcmp(name, kSwitchNameMap[i].name) == 0) {
      return kSwitchNameMap[i].device;
    }
  }
  return SWITCH_DEVICE_UNKNOWN;
}

class SwitchEventRunnable : public nsRunnable
{
public:
  SwitchEventRunnable(SwitchEvent& event) : mEvent(event) {}

  NS_IMETHOD Run() {
    NotifySwitchChange(mEvent);
    return NS_OK;
  }
private:
  SwitchEvent mEvent;
};

class SwitchEventObserver : public IUeventObserver
{
public:
  SwitchEventObserver() : mEnableNum(0) {
   InternalInit();
  }
  ~SwitchEventObserver() {}

  int GetEnableCount() {
    return mEnableNum;
  }

  void EnableSwitch(SwitchDevice aDevice) {
    mEventInfo[aDevice].mEnable = true;
    mEnableNum++;
  }

  void DisableSwitch(SwitchDevice aDevice) {
    mEventInfo[aDevice].mEnable = false;
    mEnableNum--;
  }

  void Notify(const NetlinkEvent& event) {
    const char* name;
    const char* state;
   
    SwitchDevice device = ProcessEvent(event, &name, &state);
    if (device == SWITCH_DEVICE_UNKNOWN) { 
      return; 
    } 

    EventInfo& info = mEventInfo[device]; 
    info.mEvent.status() = atoi(state) == 0 ? SWITCH_STATE_OFF : SWITCH_STATE_ON; 
    if (info.mEnable) { 
      NS_DispatchToMainThread(new SwitchEventRunnable(info.mEvent)); 
    } 
  }

  SwitchState GetCurrentInformation(SwitchDevice aDevice) {
    return mEventInfo[aDevice].mEvent.status();
  }

private:
  class EventInfo {
  public:
    EventInfo() : mEnable(false) {}
    SwitchEvent mEvent;
    bool mEnable;
  };

  EventInfo mEventInfo[NUM_SWITCH_DEVICE];
  size_t mEnableNum;

  void InternalInit() {
    for (int i = 0; i < NUM_SWITCH_DEVICE; i++) {
      mEventInfo[i].mEvent.device() = kSwitchNameMap[i].device;
      mEventInfo[i].mEvent.status() = SWITCH_STATE_UNKNOWN;
    }
  }

  bool GetEventInfo(const NetlinkEvent& event, const char** name, const char** state) {
    //working around the android code not being const-correct
    NetlinkEvent *e = const_cast<NetlinkEvent*>(&event);
    const char* subsystem = e->getSubsystem();
 
    if (!subsystem || strcmp(subsystem, "switch")) {
      return false;
    }

    *name = e->findParam("SWITCH_NAME");
    *state = e->findParam("SWITCH_STATE");

    if (!*name || !*state) {
      return false;
    }
    return true;
  }

  SwitchDevice ProcessEvent(const NetlinkEvent& event, const char** name, const char** state) {
    bool rv = GetEventInfo(event, name, state);
    NS_ENSURE_TRUE(rv, SWITCH_DEVICE_UNKNOWN);
    return NameToDevice(*name);
  }
};

SwitchEventObserver* sSwitchObserver;

static void
InitializeResourceIfNeed()
{
  if (!sSwitchObserver) {
    sSwitchObserver = new SwitchEventObserver();
    RegisterUeventListener(sSwitchObserver);
  }
}

static void
ReleaseResourceIfNeed()
{
  if (sSwitchObserver->GetEnableCount() == 0) {
    UnregisterUeventListener(sSwitchObserver);
    delete sSwitchObserver;
    sSwitchObserver = NULL;
  }
}

static void
EnableSwitchNotificationsIOThread(SwitchDevice aDevice, Monitor *aMonitor)
{
  InitializeResourceIfNeed();
  sSwitchObserver->EnableSwitch(aDevice);
  {
    MonitorAutoLock lock(*aMonitor);
    lock.Notify();
  }
}

void
EnableSwitchNotifications(SwitchDevice aDevice)
{
  Monitor monitor("EnableSwitch.monitor");
  {
    MonitorAutoLock lock(monitor);
    XRE_GetIOMessageLoop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(EnableSwitchNotificationsIOThread, aDevice, &monitor));
    lock.Wait();
  }
}

static void
DisableSwitchNotificationsIOThread(SwitchDevice aDevice)
{
  MOZ_ASSERT(sSwitchObserver->GetEnableCount());
  sSwitchObserver->DisableSwitch(aDevice);
  ReleaseResourceIfNeed();
}

void
DisableSwitchNotifications(SwitchDevice aDevice)
{
  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(DisableSwitchNotificationsIOThread, aDevice));
}

SwitchState
GetCurrentSwitchState(SwitchDevice aDevice)
{
  MOZ_ASSERT(sSwitchObserver && sSwitchObserver->GetEnableCount());
  return sSwitchObserver->GetCurrentInformation(aDevice);
}

} // hal_impl
} //mozilla
