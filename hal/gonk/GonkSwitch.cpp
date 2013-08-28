/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/log.h>
#include <fcntl.h>
#include <sysutils/NetlinkEvent.h>

#include "base/message_loop.h"

#include "Hal.h"
#include "mozilla/FileUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Monitor.h"
#include "nsPrintfCString.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"
#include "UeventPoller.h"

using namespace mozilla::hal;

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GonkSwitch" , ## args) 

#define SWITCH_HEADSET_DEVPATH "/devices/virtual/switch/h2w"
#define SWITCH_USB_DEVPATH_GB  "/devices/virtual/switch/usb_configuration"
#define SWITCH_USB_DEVPATH_ICS "/devices/virtual/android_usb/android0"

namespace mozilla {
namespace hal_impl {
/**
 * The uevent for a usb on GB insertion looks like:
 *
 *  change@/devices/virtual/switch/usb_configuration
 *    ACTION=change
 *    DEVPATH=/devices/virtual/switch/usb_configuration
 *    SUBSYSTEM=switch
 *    SWITCH_NAME=usb_configuration
 *    SWITCH_STATE=0
 *    SEQNUM=5038
 */
class SwitchHandler : public RefCounted<SwitchHandler>
{
public:
  SwitchHandler(const char* aDevPath, SwitchDevice aDevice)
    : mDevPath(aDevPath),
      mState(SWITCH_STATE_UNKNOWN),
      mDevice(aDevice)
  {
    GetInitialState();
  }

  virtual ~SwitchHandler()
  {
  }

  bool CheckEvent(NetlinkEvent* aEvent)
  {
    if (strcmp(GetSubsystem(), aEvent->getSubsystem()) ||
        strcmp(mDevPath, aEvent->findParam("DEVPATH"))) {
        return false;
    }
    
    mState = ConvertState(GetStateString(aEvent));
    return mState != SWITCH_STATE_UNKNOWN;
  }

  SwitchState GetState()
  { 
    return mState;
  }

  SwitchDevice GetType()
  {
    return mDevice;
  }
protected:
  virtual const char* GetSubsystem()
  {
    return "switch";
  }

  virtual const char* GetStateString(NetlinkEvent* aEvent)
  {
    return aEvent->findParam("SWITCH_STATE");
  }

  void GetInitialState()
  {
    nsPrintfCString statePath("/sys%s/state", mDevPath);
    int fd = open(statePath.get(), O_RDONLY);
    if (fd <= 0) {
      return;
    }

    ScopedClose autoClose(fd);
    char state[16];
    ssize_t bytesRead = read(fd, state, sizeof(state));
    if (bytesRead < 0) {
      LOG("Read data from %s fails", statePath.get());
      return;
    }

    if (state[bytesRead - 1] == '\n') {
      bytesRead--;
    }
    
    state[bytesRead] = '\0';
    mState = ConvertState(state);
  }

  virtual SwitchState ConvertState(const char* aState)
  {
    MOZ_ASSERT(aState);
    return aState[0] == '0' ? SWITCH_STATE_OFF : SWITCH_STATE_ON;
  }

  const char* mDevPath;
  SwitchState mState;
  SwitchDevice mDevice;
};

/**
 * The uevent delivered for the USB configuration under ICS looks like,
 *
 *  change@/devices/virtual/android_usb/android0
 *    ACTION=change
 *    DEVPATH=/devices/virtual/android_usb/android0
 *    SUBSYSTEM=android_usb
 *    USB_STATE=CONFIGURED
 *    SEQNUM=1802
 */
class SwitchHandlerUsbIcs: public SwitchHandler
{
public:
  SwitchHandlerUsbIcs(const char* aDevPath) : SwitchHandler(aDevPath, SWITCH_USB)
  {
    SwitchHandler::GetInitialState();
  }

  virtual ~SwitchHandlerUsbIcs() { }

protected:
  virtual const char* GetSubsystem()
  {
    return "android_usb";
  }

  virtual const char* GetStateString(NetlinkEvent* aEvent)
  {
    return aEvent->findParam("USB_STATE");
  }

  SwitchState ConvertState(const char* aState)
  {
    MOZ_ASSERT(aState);
    return strcmp(aState, "CONFIGURED") == 0 ? SWITCH_STATE_ON : SWITCH_STATE_OFF;
  }
};

/**
 * The uevent delivered for the headset under ICS looks like,
 *
 * change@/devices/virtual/switch/h2w
 *   ACTION=change
 *   DEVPATH=/devices/virtual/switch/h2w
 *   SUBSYSTEM=switch
 *   SWITCH_NAME=h2w
 *   SWITCH_STATE=2 // Headset with no mic
 *   SEQNUM=2581
 * On Otoro, SWITCH_NAME could be Headset/No Device when plug/unplug.
 * change@/devices/virtual/switch/h2w
 *   ACTION=change
 *   DEVPATH=/devices/virtual/switch/h2w
 *   SUBSYSTEM=switch
 *   SWITCH_NAME=Headset
 *   SWITCH_STATE=1 // Headset with mic
 *   SEQNUM=1602
 */
class SwitchHandlerHeadphone: public SwitchHandler
{
public:
  SwitchHandlerHeadphone(const char* aDevPath) :
    SwitchHandler(aDevPath, SWITCH_HEADPHONES)
  {
    SwitchHandler::GetInitialState();
  }

  virtual ~SwitchHandlerHeadphone() { }

protected:
  SwitchState ConvertState(const char* aState)
  {
    MOZ_ASSERT(aState);

    return aState[0] == '0' ? SWITCH_STATE_OFF :
      (aState[0] == '1' ? SWITCH_STATE_HEADSET : SWITCH_STATE_HEADPHONE);
  }
};


typedef nsTArray<RefPtr<SwitchHandler> > SwitchHandlerArray;

class SwitchEventRunnable : public nsRunnable
{
public:
  SwitchEventRunnable(SwitchEvent& aEvent) : mEvent(aEvent)
  {
  }

  NS_IMETHOD Run()
  {
    NotifySwitchChange(mEvent);
    return NS_OK;
  }
private:
  SwitchEvent mEvent;
};

class SwitchEventObserver : public IUeventObserver,
                            public RefCounted<SwitchEventObserver>
{
public:
  SwitchEventObserver() : mEnableCount(0)
  {
    Init();
  }

  ~SwitchEventObserver()
  {
    mHandler.Clear();
  }

  int GetEnableCount()
  {
    return mEnableCount;
  }

  void EnableSwitch(SwitchDevice aDevice)
  {
    mEventInfo[aDevice].mEnabled = true;
    mEnableCount++;
  }

  void DisableSwitch(SwitchDevice aDevice)
  {
    mEventInfo[aDevice].mEnabled = false;
    mEnableCount--;
  }

  void Notify(const NetlinkEvent& aEvent)
  {
    SwitchState currState;
    
    SwitchDevice device = GetEventInfo(aEvent, currState);
    if (device == SWITCH_DEVICE_UNKNOWN) {
      return; 
    }

    EventInfo& info = mEventInfo[device];
    if (currState == info.mEvent.status()) {
      return;
    }

    info.mEvent.status() = currState;

    if (info.mEnabled) {
      NS_DispatchToMainThread(new SwitchEventRunnable(info.mEvent));
    }
  }

  SwitchState GetCurrentInformation(SwitchDevice aDevice)
  {
    return mEventInfo[aDevice].mEvent.status();
  }

  void NotifyAnEvent(SwitchDevice aDevice)
  {
    EventInfo& info = mEventInfo[aDevice];
    if (info.mEvent.status() != SWITCH_STATE_UNKNOWN) {
      NS_DispatchToMainThread(new SwitchEventRunnable(info.mEvent));
    }
  }
private:
  class EventInfo
  {
  public:
    EventInfo() : mEnabled(false)
    {
      mEvent.status() = SWITCH_STATE_UNKNOWN;
      mEvent.device() = SWITCH_DEVICE_UNKNOWN;
    }
    SwitchEvent mEvent;
    bool mEnabled;
  };

  EventInfo mEventInfo[NUM_SWITCH_DEVICE];
  size_t mEnableCount;
  SwitchHandlerArray mHandler;

  void Init()
  {
    mHandler.AppendElement(new SwitchHandlerHeadphone(SWITCH_HEADSET_DEVPATH));
    mHandler.AppendElement(new SwitchHandler(SWITCH_USB_DEVPATH_GB, SWITCH_USB));
    mHandler.AppendElement(new SwitchHandlerUsbIcs(SWITCH_USB_DEVPATH_ICS));

    SwitchHandlerArray::index_type handlerIndex;
    SwitchHandlerArray::size_type numHandlers = mHandler.Length();

    for (handlerIndex = 0; handlerIndex < numHandlers; handlerIndex++) {
      SwitchState state = mHandler[handlerIndex]->GetState();
      if (state == SWITCH_STATE_UNKNOWN) {
        continue;
      }

      SwitchDevice device = mHandler[handlerIndex]->GetType();
      mEventInfo[device].mEvent.device() = device;
      mEventInfo[device].mEvent.status() = state;
    }
  }

  SwitchDevice GetEventInfo(const NetlinkEvent& aEvent, SwitchState& aState)
  {
    //working around the android code not being const-correct
    NetlinkEvent *e = const_cast<NetlinkEvent*>(&aEvent);
    
    for (size_t i = 0; i < mHandler.Length(); i++) {
      if (mHandler[i]->CheckEvent(e)) {
        aState = mHandler[i]->GetState();
        return mHandler[i]->GetType();
      }
    }
    return SWITCH_DEVICE_UNKNOWN;
  }
};

static RefPtr<SwitchEventObserver> sSwitchObserver;

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
    sSwitchObserver = nullptr;
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

  // Notify the latest state if IO thread has the information. 
  if (sSwitchObserver->GetEnableCount() > 1) {
    sSwitchObserver->NotifyAnEvent(aDevice);
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
