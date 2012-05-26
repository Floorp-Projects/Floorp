/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <android/log.h>

#include "AutoMounter.h"
#include "AutoMounterSetting.h"
#include "base/message_loop.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Hal.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "Volume.h"
#include "VolumeManager.h"

using namespace mozilla::hal;

/**************************************************************************
*
* The following "switch" files are available for monitoring usb
* connections:
*
*   /sys/devices/virtual/switch/usb_connected/state
*   /sys/devices/virtual/switch/usb_configuration/state
*
*   Under gingerbread, only the usb_configuration seems to be available.
*   Starting with honeycomb, usb_connected was also added.
*
*   When a cable insertion/removal occurs, then a uevent similar to the
*   following will be generted:
*
*    change@/devices/virtual/switch/usb_configuration
*      ACTION=change
*      DEVPATH=/devices/virtual/switch/usb_configuration
*      SUBSYSTEM=switch
*      SWITCH_NAME=usb_configuration
*      SWITCH_STATE=0
*      SEQNUM=5038
*
*    SWITCH_STATE will be 0 after a removal and 1 after an insertion
*
**************************************************************************/

#define USB_CONFIGURATION_SWITCH_NAME   NS_LITERAL_STRING("usb_configuration")

#define GB_SYS_UMS_ENABLE     "/sys/devices/virtual/usb_composite/usb_mass_storage/enable"
#define GB_SYS_USB_CONFIGURED "/sys/devices/virtual/switch/usb_configuration/state"

#define ICS_SYS_USB_FUNCTIONS "/sys/devices/virtual/android_usb/android0/functions"
#define ICS_SYS_UMS_DIRECTORY "/sys/devices/virtual/android_usb/android0/f_mass_storage"
#define ICS_SYS_USB_STATE     "/sys/devices/virtual/android_usb/android0/state"

#define USE_DEBUG 0

#undef LOG
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AutoMounter" , ## args)
#define ERR(args...)  __android_log_print(ANDROID_LOG_ERROR, "AutoMounter" , ## args)

#if USE_DEBUG
#define DBG(args...)  __android_log_print(ANDROID_LOG_DEBUG, "AutoMounter" , ## args)
#else
#define DBG(args...)
#endif

namespace mozilla {
namespace system {

class AutoMounter;

/**************************************************************************
*
*   Some helper functions for reading/writing files in /sys
*
**************************************************************************/

static bool
ReadSysFile(const char *aFilename, char *aBuf, size_t aBufSize)
{
  int fd = open(aFilename, O_RDONLY);
  if (fd < 0) {
    ERR("Unable to open file '%s' for reading", aFilename);
    return false;
  }
  ScopedClose autoClose(fd);
  ssize_t bytesRead = read(fd, aBuf, aBufSize - 1);
  if (bytesRead < 0) {
    ERR("Unable to read from file '%s'", aFilename);
    return false;
  }
  if (aBuf[bytesRead - 1] == '\n') {
    bytesRead--;
  }
  aBuf[bytesRead] = '\0';
  return true;
}

static bool
ReadSysFile(const char *aFilename, bool *aVal)
{
  char valBuf[20];
  if (!ReadSysFile(aFilename, valBuf, sizeof(valBuf))) {
    return false;
  }
  int intVal;
  if (sscanf(valBuf, "%d", &intVal) != 1) {
    return false;
  }
  *aVal = (intVal != 0);
  return true;
}

/***************************************************************************/

inline const char *SwitchStateStr(const SwitchEvent &aEvent)
{
  return aEvent.status() == SWITCH_STATE_ON ? "plugged" : "unplugged";
}

/***************************************************************************/

static bool
IsUsbCablePluggedIn()
{
#if 0
  // Use this code when bug 745078 gets fixed (or use whatever the
  // appropriate method is)
  return GetCurrentSwitchEvent(SWITCH_USB) == SWITCH_STATE_ON;
#else
  // Until then, just go read the file directly
  if (access(ICS_SYS_USB_STATE, F_OK) == 0) {
    char usbState[20];
    return ReadSysFile(ICS_SYS_USB_STATE,
                       usbState, sizeof(usbState)) &&
           (strcmp(usbState, "CONFIGURED") == 0);
  }
  bool configured;
  return ReadSysFile(GB_SYS_USB_CONFIGURED, &configured) &&
         configured;
#endif
}

/***************************************************************************/

class VolumeManagerStateObserver : public VolumeManager::StateObserver
{
public:
  virtual void Notify(const VolumeManager::StateChangedEvent &aEvent);
};

class AutoMounterResponseCallback : public VolumeResponseCallback
{
public:
  AutoMounterResponseCallback()
    : mErrorCount(0)
  {
  }

protected:
  virtual void ResponseReceived(const VolumeCommand *aCommand);

private:
    const static int kMaxErrorCount = 3; // Max number of errors before we give up

    int   mErrorCount;
};

/***************************************************************************/

class AutoMounter : public RefCounted<AutoMounter>
{
public:
  AutoMounter()
    : mResponseCallback(new AutoMounterResponseCallback),
      mMode(AUTOMOUNTER_DISABLE)
  {
    VolumeManager::RegisterStateObserver(&mVolumeManagerStateObserver);
    DBG("Calling UpdateState from constructor");
    UpdateState();
  }

  ~AutoMounter()
  {
    VolumeManager::UnregisterStateObserver(&mVolumeManagerStateObserver);
  }

  void UpdateState();

  void SetMode(int32_t aMode)
  {
    if ((aMode == AUTOMOUNTER_DISABLE_WHEN_UNPLUGGED) &&
        (mMode == AUTOMOUNTER_DISABLE)) {
      // If it's already disabled, then leave it as disabled.
      // AUTOMOUNTER_DISABLE_WHEN_UNPLUGGED implies "enabled until unplugged"
      aMode = AUTOMOUNTER_DISABLE;
    }
    mMode = aMode;
    DBG("Calling UpdateState due to mode set to %d", mMode);
    UpdateState();
  }

private:

  VolumeManagerStateObserver      mVolumeManagerStateObserver;
  RefPtr<VolumeResponseCallback>  mResponseCallback;
  int32_t                         mMode;
};

static RefPtr<AutoMounter> sAutoMounter;

/***************************************************************************/

void
VolumeManagerStateObserver::Notify(const VolumeManager::StateChangedEvent &)
{
  LOG("VolumeManager state changed event: %s", VolumeManager::StateStr());

  if (!sAutoMounter) {
    return;
  }
  DBG("Calling UpdateState due to VolumeManagerStateObserver");
  sAutoMounter->UpdateState();
}

void
AutoMounterResponseCallback::ResponseReceived(const VolumeCommand *aCommand)
{

  if (WasSuccessful()) {
    DBG("Calling UpdateState due to Volume::OnSuccess");
    mErrorCount = 0;
    sAutoMounter->UpdateState();
    return;
  }
  ERR("Command '%s' failed: %d '%s'",
      aCommand->CmdStr(), ResponseCode(), ResponseStr().get());

  if (++mErrorCount < kMaxErrorCount) {
    DBG("Calling UpdateState due to VolumeResponseCallback::OnError");
    sAutoMounter->UpdateState();
  }
}

/***************************************************************************/

void
AutoMounter::UpdateState()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  // If the following preconditions are met:
  //    - UMS is available (i.e. compiled into the kernel)
  //    - UMS is enabled
  //    - AutoMounter is enabled
  //    - USB cable is plugged in
  //  then we will try to unmount and share
  //  otherwise we will try to unshare and mount.

  if (VolumeManager::State() != VolumeManager::VOLUMES_READY) {
    // The volume manager isn't in a ready state, so there
    // isn't anything else that we can do.
    LOG("UpdateState: VolumeManager not ready yet");
    return;
  }

  if (mResponseCallback->IsPending()) {
    // We only deal with one outstanding volume command at a time,
    // so we need to wait for it to finish.
    return;
  }

  std::vector<RefPtr<Volume> > volumeArray;
  RefPtr<Volume> vol = VolumeManager::FindVolumeByName(NS_LITERAL_CSTRING("sdcard"));
  if (vol != NULL) {
    volumeArray.push_back(vol);
  }
  if (volumeArray.size() == 0) {
    // No volumes of interest, nothing to do
    LOG("UpdateState: No volumes found");
    return;
  }

  bool  umsAvail = false;
  bool  umsEnabled = false;

  if (access(ICS_SYS_USB_FUNCTIONS, F_OK) == 0) {
    umsAvail = (access(ICS_SYS_UMS_DIRECTORY, F_OK) == 0);
    char functionsStr[60];
    umsEnabled = umsAvail &&
                 ReadSysFile(ICS_SYS_USB_FUNCTIONS, functionsStr, sizeof(functionsStr)) &&
                 !!strstr(functionsStr, "mass_storage");
  } else {
    umsAvail = ReadSysFile(GB_SYS_UMS_ENABLE, &umsEnabled);
  }

  bool usbCablePluggedIn = IsUsbCablePluggedIn();
  bool enabled = (mMode == AUTOMOUNTER_ENABLE);

  if (mMode == AUTOMOUNTER_DISABLE_WHEN_UNPLUGGED) {
    enabled = usbCablePluggedIn;
    if (!usbCablePluggedIn) {
      mMode = AUTOMOUNTER_DISABLE;
    }
  }

  bool tryToShare = (umsAvail && umsEnabled && enabled && usbCablePluggedIn);
  LOG("UpdateState: umsAvail:%d umsEnabled:%d mode:%d usbCablePluggedIn:%d tryToShare:%d",
      umsAvail, umsEnabled, mMode, usbCablePluggedIn, tryToShare);

  VolumeManager::VolumeArray::iterator volIter;
  for (volIter = volumeArray.begin(); volIter != volumeArray.end(); volIter++) {
    RefPtr<Volume>  vol = *volIter;
    Volume::STATE   volState = vol->State();

    LOG("UpdateState: Volume %s is %s", vol->NameStr(), vol->StateStr());
    if (tryToShare) {
      // We're going to try to unmount and share the volumes
      switch (volState) {
        case Volume::STATE_MOUNTED: {
          // Volume is mounted, we need to unmount before
          // we can share.
          DBG("UpdateState: Unmounting %s", vol->NameStr());
          vol->StartUnmount(mResponseCallback);
          return;
        }
        case Volume::STATE_IDLE: {
          // Volume is unmounted. We can go ahead and share.
          DBG("UpdateState: Sharing %s", vol->NameStr());
          vol->StartShare(mResponseCallback);
          return;
        }
        default: {
          // Not in a state that we can do anything about.
          break;
        }
      }
    } else {
      // We're going to try and unshare and remount the volumes
      switch (volState) {
        case Volume::STATE_SHARED: {
          // Volume is shared. We can go ahead and unshare.
          DBG("UpdateState: Unsharing %s", vol->NameStr());
          vol->StartUnshare(mResponseCallback);
          return;
        }
        case Volume::STATE_IDLE: {
          // Volume is unmounted, try to mount.

          DBG("UpdateState: Mounting %s", vol->NameStr());
          vol->StartMount(mResponseCallback);
          return;
        }
        default: {
          // Not in a state that we can do anything about.
          break;
        }
      }
    }
  }
}

/***************************************************************************/

static void
InitAutoMounterIOThread()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(!sAutoMounter);

  sAutoMounter = new AutoMounter();
}

static void
ShutdownAutoMounterIOThread()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  sAutoMounter = NULL;
  ShutdownVolumeManager();
}

static void
SetAutoMounterModeIOThread(const int32_t &aMode)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(sAutoMounter);

  sAutoMounter->SetMode(aMode);
}

static void
UsbCableEventIOThread()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  if (!sAutoMounter) {
    return;
  }
  DBG("Calling UpdateState due to USBCableEvent");
  sAutoMounter->UpdateState();
}

/**************************************************************************
*
*   Public API
*
*   Since the AutoMounter runs in IO Thread context, we need to switch
*   to IOThread context before we can do anything.
*
**************************************************************************/

class UsbCableObserver : public SwitchObserver,
                         public RefCounted<UsbCableObserver>
{
public:
  UsbCableObserver()
  {
    RegisterSwitchObserver(SWITCH_USB, this);
  }

  ~UsbCableObserver()
  {
    UnregisterSwitchObserver(SWITCH_USB, this);
  }

  virtual void Notify(const SwitchEvent &aEvent)
  {
    DBG("UsbCable switch device: %d state: %s\n",
        aEvent.device(), SwitchStateStr(aEvent));
    XRE_GetIOMessageLoop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(UsbCableEventIOThread));
  }
};

static RefPtr<UsbCableObserver> sUsbCableObserver;
static RefPtr<AutoMounterSetting> sAutoMounterSetting;

void
InitAutoMounter()
{
  InitVolumeManager();
  sAutoMounterSetting = new AutoMounterSetting();

  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(InitAutoMounterIOThread));

  // Switch Observers need to run on the main thread, so we need to
  // start it here and have it send events to the AutoMounter running
  // on the IO Thread.
  sUsbCableObserver = new UsbCableObserver();
}

void
SetAutoMounterMode(int32_t aMode)
{
  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(SetAutoMounterModeIOThread, aMode));
}

void
ShutdownAutoMounter()
{
  sAutoMounterSetting = NULL;
  sUsbCableObserver = NULL;

  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(ShutdownAutoMounterIOThread));
}

} // system
} // mozilla
