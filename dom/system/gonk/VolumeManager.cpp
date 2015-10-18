/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VolumeManager.h"

#include "Volume.h"
#include "VolumeCommand.h"
#include "VolumeManagerLog.h"
#include "VolumeServiceTest.h"

#include "nsWhitespaceTokenizer.h"
#include "nsXULAppAPI.h"

#include "base/message_loop.h"
#include "mozilla/Scoped.h"
#include "mozilla/StaticPtr.h"

#include <android/log.h>
#include <cutils/sockets.h>
#include <fcntl.h>
#include <sys/socket.h>

namespace mozilla {
namespace system {

static StaticRefPtr<VolumeManager> sVolumeManager;

VolumeManager::STATE VolumeManager::mState = VolumeManager::UNINITIALIZED;
VolumeManager::StateObserverList VolumeManager::mStateObserverList;

/***************************************************************************/

VolumeManager::VolumeManager()
  : LineWatcher('\0', kRcvBufSize),
    mSocket(-1),
    mCommandPending(false)
{
  DBG("VolumeManager constructor called");
}

VolumeManager::~VolumeManager()
{
}

//static
void
VolumeManager::Dump(const char* aLabel)
{
  if (!sVolumeManager) {
    LOG("%s: sVolumeManager == null", aLabel);
    return;
  }

  VolumeArray::size_type  numVolumes = NumVolumes();
  VolumeArray::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<Volume> vol = GetVolume(volIndex);
    vol->Dump(aLabel);
  }
}

//static
size_t
VolumeManager::NumVolumes()
{
  if (!sVolumeManager) {
    return 0;
  }
  return sVolumeManager->mVolumeArray.Length();
}

//static
already_AddRefed<Volume>
VolumeManager::GetVolume(size_t aIndex)
{
  MOZ_ASSERT(aIndex < NumVolumes());
  RefPtr<Volume> vol = sVolumeManager->mVolumeArray[aIndex];
  return vol.forget();
}

//static
VolumeManager::STATE
VolumeManager::State()
{
  return mState;
}

//static
const char *
VolumeManager::StateStr(VolumeManager::STATE aState)
{
  switch (aState) {
    case UNINITIALIZED: return "Uninitialized";
    case STARTING:      return "Starting";
    case VOLUMES_READY: return "Volumes Ready";
  }
  return "???";
}


//static
void
VolumeManager::SetState(STATE aNewState)
{
  if (mState != aNewState) {
    LOG("changing state from '%s' to '%s'",
        StateStr(mState), StateStr(aNewState));
    mState = aNewState;
    mStateObserverList.Broadcast(StateChangedEvent());
  }
}

//static
void
VolumeManager::RegisterStateObserver(StateObserver* aObserver)
{
  mStateObserverList.AddObserver(aObserver);
}

//static
void VolumeManager::UnregisterStateObserver(StateObserver* aObserver)
{
  mStateObserverList.RemoveObserver(aObserver);
}

//static
already_AddRefed<Volume>
VolumeManager::FindVolumeByName(const nsCSubstring& aName)
{
  if (!sVolumeManager) {
    return nullptr;
  }
  VolumeArray::size_type  numVolumes = NumVolumes();
  VolumeArray::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<Volume> vol = GetVolume(volIndex);
    if (vol->Name().Equals(aName)) {
      return vol.forget();
    }
  }
  return nullptr;
}

//static
already_AddRefed<Volume>
VolumeManager::FindAddVolumeByName(const nsCSubstring& aName)
{
  RefPtr<Volume> vol = FindVolumeByName(aName);
  if (vol) {
    return vol.forget();
  }
  // No volume found, create and add a new one.
  vol = new Volume(aName);
  sVolumeManager->mVolumeArray.AppendElement(vol);
  return vol.forget();
}

//static
bool
VolumeManager::RemoveVolumeByName(const nsCSubstring& aName)
{
  if (!sVolumeManager) {
    return false;
  }
  VolumeArray::size_type  numVolumes = NumVolumes();
  VolumeArray::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<Volume> vol = GetVolume(volIndex);
    if (vol->Name().Equals(aName)) {
      sVolumeManager->mVolumeArray.RemoveElementAt(volIndex);
      return true;
    }
  }
  // No volume found. Return false to indicate this.
  return false;
}


//static
void VolumeManager::InitConfig()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  // This function uses /system/etc/volume.cfg to add additional volumes
  // to the Volume Manager.
  //
  // This is useful on devices like the Nexus 4, which have no physical sd card
  // or dedicated partition.
  //
  // The format of the volume.cfg file is as follows:
  // create volume-name mount-point
  // configure volume-name preference preference-value
  // Blank lines and lines starting with the hash character "#" will be ignored.

  ScopedCloseFile fp;
  int n = 0;
  char line[255];
  const char *filename = "/system/etc/volume.cfg";
  if (!(fp = fopen(filename, "r"))) {
    LOG("Unable to open volume configuration file '%s' - ignoring", filename);
    return;
  }
  while(fgets(line, sizeof(line), fp)) {
    n++;

    if (line[0] == '#')
      continue;

    nsCString commandline(line);
    nsCWhitespaceTokenizer tokenizer(commandline);
    if (!tokenizer.hasMoreTokens()) {
      // Blank line - ignore
      continue;
    }

    nsCString command(tokenizer.nextToken());
    if (command.EqualsLiteral("create")) {
      if (!tokenizer.hasMoreTokens()) {
        ERR("No vol_name in %s line %d",  filename, n);
        continue;
      }
      nsCString volName(tokenizer.nextToken());
      if (!tokenizer.hasMoreTokens()) {
        ERR("No mount point for volume '%s'. %s line %d",
             volName.get(), filename, n);
        continue;
      }
      nsCString mountPoint(tokenizer.nextToken());
      RefPtr<Volume> vol = FindAddVolumeByName(volName);
      vol->SetFakeVolume(mountPoint);
      continue;
    }
    if (command.EqualsLiteral("configure")) {
      if (!tokenizer.hasMoreTokens()) {
        ERR("No vol_name in %s line %d", filename, n);
        continue;
      }
      nsCString volName(tokenizer.nextToken());
      if (!tokenizer.hasMoreTokens()) {
        ERR("No configuration name specified for volume '%s'. %s line %d",
             volName.get(), filename, n);
        continue;
      }
      nsCString configName(tokenizer.nextToken());
      if (!tokenizer.hasMoreTokens()) {
        ERR("No value for configuration name '%s'. %s line %d",
            configName.get(), filename, n);
        continue;
      }
      nsCString configValue(tokenizer.nextToken());
      RefPtr<Volume> vol = FindVolumeByName(volName);
      if (vol) {
        vol->SetConfig(configName, configValue);
      } else {
        ERR("Invalid volume name '%s'.", volName.get());
      }
      continue;
    }
    if (command.EqualsLiteral("ignore")) {
      // This command is useful to remove volumes which are being tracked by
      // vold, but for which we have no interest.
      if (!tokenizer.hasMoreTokens()) {
        ERR("No vol_name in %s line %d", filename, n);
        continue;
      }
      nsCString volName(tokenizer.nextToken());
      RemoveVolumeByName(volName);
      continue;
    }
    ERR("Unrecognized command: '%s'", command.get());
  }
}

void
VolumeManager::DefaultConfig()
{

  VolumeManager::VolumeArray::size_type numVolumes = VolumeManager::NumVolumes();
  if (numVolumes == 0) {
    return;
  }
  if (numVolumes == 1) {
    // This is to cover early shipping phones like the Buri,
    // which had no internal storage, and only external sdcard.
    //
    // Phones line the nexus-4 which only have an internal
    // storage area will need to have a volume.cfg file with
    // removable set to false.
    RefPtr<Volume> vol = VolumeManager::GetVolume(0);
    vol->SetIsRemovable(true);
    vol->SetIsHotSwappable(true);
    return;
  }
  VolumeManager::VolumeArray::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<Volume> vol = VolumeManager::GetVolume(volIndex);
    if (!vol->Name().EqualsLiteral("sdcard")) {
      vol->SetIsRemovable(true);
      vol->SetIsHotSwappable(true);
    }
  }
}

class VolumeListCallback : public VolumeResponseCallback
{
  virtual void ResponseReceived(const VolumeCommand* aCommand)
  {
    switch (ResponseCode()) {
      case ::ResponseCode::VolumeListResult: {
        // Each line will look something like:
        //
        //  sdcard /mnt/sdcard 1
        //
        // So for each volume that we get back, we update any volumes that
        // we have of the same name, or add new ones if they don't exist.
        nsCWhitespaceTokenizer tokenizer(ResponseStr());
        nsDependentCSubstring volName(tokenizer.nextToken());
        RefPtr<Volume> vol = VolumeManager::FindAddVolumeByName(volName);
        vol->HandleVoldResponse(ResponseCode(), tokenizer);
        break;
      }

      case ::ResponseCode::CommandOkay: {
        // We've received the list of volumes. Now read the Volume.cfg
        // file to perform customizations, and then tell everybody
        // that we're ready for business.
        VolumeManager::DefaultConfig();
        VolumeManager::InitConfig();
        VolumeManager::Dump("READY");
        VolumeManager::SetState(VolumeManager::VOLUMES_READY);
        break;
      }
    }
  }
};

bool
VolumeManager::OpenSocket()
{
  SetState(STARTING);
  if ((mSocket.rwget() = socket_local_client("vold",
                                             ANDROID_SOCKET_NAMESPACE_RESERVED,
                                             SOCK_STREAM)) < 0) {
      ERR("Error connecting to vold: (%s) - will retry", strerror(errno));
      return false;
  }
  // add FD_CLOEXEC flag
  int flags = fcntl(mSocket.get(), F_GETFD);
  if (flags == -1) {
      return false;
  }
  flags |= FD_CLOEXEC;
  if (fcntl(mSocket.get(), F_SETFD, flags) == -1) {
    return false;
  }
  // set non-blocking
  if (fcntl(mSocket.get(), F_SETFL, O_NONBLOCK) == -1) {
    return false;
  }
  if (!MessageLoopForIO::current()->
      WatchFileDescriptor(mSocket.get(),
                          true,
                          MessageLoopForIO::WATCH_READ,
                          &mReadWatcher,
                          this)) {
      return false;
  }

  LOG("Connected to vold");
  PostCommand(new VolumeListCommand(new VolumeListCallback));
  return true;
}

//static
void
VolumeManager::PostCommand(VolumeCommand* aCommand)
{
  if (!sVolumeManager) {
    ERR("VolumeManager not initialized. Dropping command '%s'", aCommand->Data());
    return;
  }
  aCommand->SetPending(true);

  DBG("Sending command '%s'", aCommand->Data());
  // vold can only process one command at a time, so add our command
  // to the end of the command queue.
  sVolumeManager->mCommands.push(aCommand);
  if (!sVolumeManager->mCommandPending) {
    // There aren't any commands currently being processed, so go
    // ahead and kick this one off.
    sVolumeManager->mCommandPending = true;
    sVolumeManager->WriteCommandData();
  }
}

/***************************************************************************
* The WriteCommandData initiates sending of a command to vold. Since
* we're running on the IOThread and not allowed to block, WriteCommandData
* will write as much data as it can, and if not all of the data can be
* written then it will setup a file descriptor watcher and
* OnFileCanWriteWithoutBlocking will call WriteCommandData to write out
* more of the command data.
*/
void
VolumeManager::WriteCommandData()
{
  if (mCommands.size() == 0) {
    return;
  }

  VolumeCommand* cmd = mCommands.front();
  if (cmd->BytesRemaining() == 0) {
    // All bytes have been written. We're waiting for a response.
    return;
  }
  // There are more bytes left to write. Try to write them all.
  ssize_t bytesWritten = write(mSocket.get(), cmd->Data(), cmd->BytesRemaining());
  if (bytesWritten < 0) {
    ERR("Failed to write %d bytes to vold socket", cmd->BytesRemaining());
    Restart();
    return;
  }
  DBG("Wrote %d bytes (of %d)", bytesWritten, cmd->BytesRemaining());
  cmd->ConsumeBytes(bytesWritten);
  if (cmd->BytesRemaining() == 0) {
    return;
  }
  // We were unable to write all of the command bytes. Setup a watcher
  // so we'll get called again when we can write without blocking.
  if (!MessageLoopForIO::current()->
      WatchFileDescriptor(mSocket.get(),
                          false, // one-shot
                          MessageLoopForIO::WATCH_WRITE,
                          &mWriteWatcher,
                          this)) {
    ERR("Failed to setup write watcher for vold socket");
    Restart();
  }
}

void
VolumeManager::OnLineRead(int aFd, nsDependentCSubstring& aMessage)
{
  MOZ_ASSERT(aFd == mSocket.get());
  char* endPtr;
  int responseCode = strtol(aMessage.Data(), &endPtr, 10);
  if (*endPtr == ' ') {
    endPtr++;
  }

  // Now fish out the rest of the line after the response code
  nsDependentCString  responseLine(endPtr, aMessage.Length() - (endPtr - aMessage.Data()));
  DBG("Rcvd: %d '%s'", responseCode, responseLine.Data());

  if (responseCode >= ::ResponseCode::UnsolicitedInformational) {
    // These are unsolicited broadcasts. We intercept these and process
    // them ourselves
    HandleBroadcast(responseCode, responseLine);
  } else {
    // Everything else is considered to be part of the command response.
    if (mCommands.size() > 0) {
      VolumeCommand* cmd = mCommands.front();
      cmd->HandleResponse(responseCode, responseLine);
      if (responseCode >= ::ResponseCode::CommandOkay) {
        // That's a terminating response. We can remove the command.
        mCommands.pop();
        mCommandPending = false;
        // Start the next command, if there is one.
        WriteCommandData();
      }
    } else {
      ERR("Response with no command");
    }
  }
}

void
VolumeManager::OnFileCanWriteWithoutBlocking(int aFd)
{
  MOZ_ASSERT(aFd == mSocket.get());
  WriteCommandData();
}

void
VolumeManager::HandleBroadcast(int aResponseCode, nsCString& aResponseLine)
{
  // Format of the line is something like:
  //
  //  Volume sdcard /mnt/sdcard state changed from 7 (Shared-Unmounted) to 1 (Idle-Unmounted)
  //
  // So we parse out the volume name and the state after the string " to "
  nsCWhitespaceTokenizer  tokenizer(aResponseLine);
  tokenizer.nextToken();  // The word "Volume"
  nsDependentCSubstring volName(tokenizer.nextToken());

  RefPtr<Volume> vol = FindVolumeByName(volName);
  if (!vol) {
    return;
  }
  vol->HandleVoldResponse(aResponseCode, tokenizer);
}

void
VolumeManager::Restart()
{
  mReadWatcher.StopWatchingFileDescriptor();
  mWriteWatcher.StopWatchingFileDescriptor();

  while (!mCommands.empty()) {
    mCommands.pop();
  }
  mCommandPending = false;
  mSocket.dispose();
  Start();
}

//static
void
VolumeManager::Start()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  if (!sVolumeManager) {
    return;
  }
  SetState(STARTING);
  if (!sVolumeManager->OpenSocket()) {
    // Socket open failed, try again in a second.
    MessageLoopForIO::current()->
      PostDelayedTask(FROM_HERE,
                      NewRunnableFunction(VolumeManager::Start),
                      1000);
  }
}

void
VolumeManager::OnError()
{
  Restart();
}

/***************************************************************************/

static void
InitVolumeManagerIOThread()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(!sVolumeManager);

  sVolumeManager = new VolumeManager();
  VolumeManager::Start();

  InitVolumeServiceTestIOThread();
}

static void
ShutdownVolumeManagerIOThread()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  sVolumeManager = nullptr;
}

/**************************************************************************
*
*   Public API
*
*   Since the VolumeManager runs in IO Thread context, we need to switch
*   to IOThread context before we can do anything.
*
**************************************************************************/

void
InitVolumeManager()
{
  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(InitVolumeManagerIOThread));
}

void
ShutdownVolumeManager()
{
  ShutdownVolumeServiceTest();

  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(ShutdownVolumeManagerIOThread));
}

} // system
} // mozilla
