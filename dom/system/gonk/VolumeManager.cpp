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

#include <android/log.h>
#include <cutils/sockets.h>
#include <fcntl.h>
#include <sys/socket.h>

namespace mozilla {
namespace system {

static RefPtr<VolumeManager> sVolumeManager;

/***************************************************************************/

VolumeManager::VolumeManager()
  : mState(VolumeManager::STARTING),
    mSocket(-1),
    mCommandPending(false),
    mRcvIdx(0)
{
  DBG("VolumeManager constructor called");
}

VolumeManager::~VolumeManager()
{
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
TemporaryRef<Volume>
VolumeManager::GetVolume(size_t aIndex)
{
  MOZ_ASSERT(aIndex < NumVolumes());
  return sVolumeManager->mVolumeArray[aIndex];
}

//static
VolumeManager::STATE
VolumeManager::State()
{
  if (!sVolumeManager) {
    return VolumeManager::UNINITIALIZED;
  }
  return sVolumeManager->mState;
}

//static
const char *
VolumeManager::StateStr()
{
  switch (State()) {
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
  if (!sVolumeManager) {
    return;
  }
  if (sVolumeManager->mState != aNewState) {
    sVolumeManager->mState = aNewState;
    sVolumeManager->mStateObserverList.Broadcast(StateChangedEvent());
  }
}

//static
void
VolumeManager::RegisterStateObserver(StateObserver *aObserver)
{
  sVolumeManager->mStateObserverList.AddObserver(aObserver);
}

//static
void VolumeManager::UnregisterStateObserver(StateObserver *aObserver)
{
  sVolumeManager->mStateObserverList.RemoveObserver(aObserver);
}

//static
TemporaryRef<Volume>
VolumeManager::FindVolumeByName(const nsCSubstring &aName)
{
  if (!sVolumeManager) {
    return NULL;
  }
  VolumeArray::size_type  numVolumes = NumVolumes();
  VolumeArray::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<Volume> vol = GetVolume(volIndex);
    if (vol->Name().Equals(aName)) {
      return vol;
    }
  }
  return NULL;
}

//static
TemporaryRef<Volume>
VolumeManager::FindAddVolumeByName(const nsCSubstring &aName)
{
  RefPtr<Volume> vol = FindVolumeByName(aName);
  if (vol) {
    return vol;
  }
  // No volume found, create and add a new one.
  vol = new Volume(aName);
  sVolumeManager->mVolumeArray.AppendElement(vol);
  return vol;
}

class VolumeListCallback : public VolumeResponseCallback
{
  virtual void ResponseReceived(const VolumeCommand *aCommand)
  {
    switch (ResponseCode()) {
      case ResponseCode::VolumeListResult: {
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

      case ResponseCode::CommandOkay: {
        // We've received the list of volumes. Tell anybody who
        // is listening that we're open for business.
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
VolumeManager::PostCommand(VolumeCommand *aCommand)
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

  VolumeCommand *cmd = mCommands.front();
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
  DBG("Wrote %ld bytes (of %d)", bytesWritten, cmd->BytesRemaining());
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
VolumeManager::OnFileCanReadWithoutBlocking(int aFd)
{
  MOZ_ASSERT(aFd == mSocket.get());
  while (true) {
    ssize_t bytesRemaining = read(aFd, &mRcvBuf[mRcvIdx], sizeof(mRcvBuf) - mRcvIdx);
    if (bytesRemaining < 0) {
      if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
        return;
      }
      if (errno == EINTR) {
        continue;
      }
      ERR("Unknown read error: %d (%s) - restarting", errno, strerror(errno));
      Restart();
      return;
    }
    if (bytesRemaining == 0) {
      // This means that vold probably crashed
      ERR("Vold appears to have crashed - restarting");
      Restart();
      return;
    }
    // We got some data. Each line is terminated by a null character
    DBG("Read %ld bytes", bytesRemaining);
    while (bytesRemaining > 0) {
      bytesRemaining--;
      if (mRcvBuf[mRcvIdx] == '\0') {
        // We found a line terminator. Each line is formatted as an
        // integer response code followed by the rest of the line.
        // Fish out the response code.
        char *endPtr;
        int responseCode = strtol(mRcvBuf, &endPtr, 10);
        if (*endPtr == ' ') {
          endPtr++;
        }

        // Now fish out the rest of the line after the response code
        nsDependentCString  responseLine(endPtr, &mRcvBuf[mRcvIdx] - endPtr);
        DBG("Rcvd: %d '%s'", responseCode, responseLine.Data());

        if (responseCode >= ResponseCode::UnsolicitedInformational) {
          // These are unsolicited broadcasts. We intercept these and process
          // them ourselves
          HandleBroadcast(responseCode, responseLine);
        } else {
          // Everything else is considered to be part of the command response.
          if (mCommands.size() > 0) {
            VolumeCommand *cmd = mCommands.front();
            cmd->HandleResponse(responseCode, responseLine);
            if (responseCode >= ResponseCode::CommandOkay) {
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
        if (bytesRemaining > 0) {
          // There is data in the receive buffer beyond the current line.
          // Shift it down to the beginning.
          memmove(&mRcvBuf[0], &mRcvBuf[mRcvIdx + 1], bytesRemaining);
        }
        mRcvIdx = 0;
      } else {
        mRcvIdx++;
      }
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
VolumeManager::HandleBroadcast(int aResponseCode, nsCString &aResponseLine)
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
  mRcvIdx = 0;
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
  if (!sVolumeManager->OpenSocket()) {
    // Socket open failed, try again in a second.
    MessageLoopForIO::current()->
      PostDelayedTask(FROM_HERE,
                      NewRunnableFunction(VolumeManager::Start),
                      1000);
  }
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

  sVolumeManager = NULL;
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
