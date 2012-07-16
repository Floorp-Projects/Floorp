/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Volume.h"
#include "VolumeCommand.h"
#include "VolumeManager.h"
#include "VolumeManagerLog.h"
#include "nsIVolume.h"
#include "nsXULAppAPI.h"

#include <vold/ResponseCode.h>

namespace mozilla {
namespace system {

Volume::EventObserverList Volume::mEventObserverList;

// We don't get media inserted/removed events at startup. So we
// assume it's present, and we'll be told that it's missing.
Volume::Volume(const nsCSubstring &aName)
  : mMediaPresent(true),
    mState(nsIVolume::STATE_INIT),
    mName(aName)
{
  DBG("Volume %s: created", NameStr());
}

void
Volume::SetMediaPresent(bool aMediaPresent)
{
  // mMediaPresent is slightly redunant to the state, however
  // when media is removed (while Idle), we get the following:
  //    631 Volume sdcard /mnt/sdcard disk removed (179:0)
  //    605 Volume sdcard /mnt/sdcard state changed from 1 (Idle-Unmounted) to 0 (No-Media)
  //
  // And on media insertion, we get:
  //    630 Volume sdcard /mnt/sdcard disk inserted (179:0)
  //    605 Volume sdcard /mnt/sdcard state changed from 0 (No-Media) to 2 (Pending)
  //    605 Volume sdcard /mnt/sdcard state changed from 2 (Pending) to 1 (Idle-Unmounted)
  //
  // On media removal while the media is mounted:
  //    632 Volume sdcard /mnt/sdcard bad removal (179:1)
  //    605 Volume sdcard /mnt/sdcard state changed from 4 (Mounted) to 5 (Unmounting)
  //    605 Volume sdcard /mnt/sdcard state changed from 5 (Unmounting) to 1 (Idle-Unmounted)
  //    631 Volume sdcard /mnt/sdcard disk removed (179:0)
  //    605 Volume sdcard /mnt/sdcard state changed from 1 (Idle-Unmounted) to 0 (No-Media)
  //
  // When sharing with a PC, it goes Mounted -> Idle -> Shared
  // When unsharing with a PC, it goes Shared -> Idle -> Mounted
  //
  // The AutoMounter needs to know whether the media is present or not when
  // processing the Idle state.

  if (mMediaPresent != aMediaPresent) {
    LOG("Volume: %s media %s", NameStr(), aMediaPresent ? "inserted" : "removed");
    mMediaPresent = aMediaPresent;
    // No need to broadcast the change. A state change will be coming right away,
    // and that will serve the purpose.
  }
}

void
Volume::SetState(Volume::STATE aNewState)
{
  if (aNewState == mState) {
    return;
  }
  LOG("Volume %s: changing state from %s to %s (%d observers)",
      NameStr(), StateStr(mState),
      StateStr(aNewState), mEventObserverList.Length());

  if (aNewState == nsIVolume::STATE_NOMEDIA) {
    // Cover the startup case where we don't get insertion/removal events
    mMediaPresent = false;
  }
  mState = aNewState;
  mEventObserverList.Broadcast(this);
}

void
Volume::SetMountPoint(const nsCSubstring &aMountPoint)
{
  if (mMountPoint.Equals(aMountPoint)) {
    return;
  }
  mMountPoint = aMountPoint;
  DBG("Volume %s: Setting mountpoint to '%s'", NameStr(), mMountPoint.get());
}

void
Volume::StartMount(VolumeResponseCallback *aCallback)
{
  StartCommand(new VolumeActionCommand(this, "mount", "", aCallback));
}

void
Volume::StartUnmount(VolumeResponseCallback *aCallback)
{
  StartCommand(new VolumeActionCommand(this, "unmount", "force", aCallback));
}

void
Volume::StartShare(VolumeResponseCallback *aCallback)
{
  StartCommand(new VolumeActionCommand(this, "share", "ums", aCallback));
}

void
Volume::StartUnshare(VolumeResponseCallback *aCallback)
{
  StartCommand(new VolumeActionCommand(this, "unshare", "ums", aCallback));
}

void
Volume::StartCommand(VolumeCommand *aCommand)
{
  VolumeManager::PostCommand(aCommand);
}

//static
void
Volume::RegisterObserver(Volume::EventObserver *aObserver)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  mEventObserverList.AddObserver(aObserver);
  // Send an initial event to the observer (for each volume)
  size_t numVolumes = VolumeManager::NumVolumes();
  for (size_t volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<Volume> vol = VolumeManager::GetVolume(volIndex);
    aObserver->Notify(vol);
  }
}

//static
void
Volume::UnregisterObserver(Volume::EventObserver *aObserver)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  mEventObserverList.RemoveObserver(aObserver);
}

void
Volume::HandleVoldResponse(int aResponseCode, nsCWhitespaceTokenizer &aTokenizer)
{
  // The volume name will have already been parsed, and the tokenizer will point
  // to the token after the volume name
  switch (aResponseCode) {
    case ResponseCode::VolumeListResult: {
      // Each line will look something like:
      //
      //  sdcard /mnt/sdcard 1
      //
      nsDependentCSubstring mntPoint(aTokenizer.nextToken());
      SetMountPoint(mntPoint);
      PRInt32 errCode;
      nsCString state(aTokenizer.nextToken());
      SetState((STATE)state.ToInteger(&errCode));
      break;
    }

    case ResponseCode::VolumeStateChange: {
      // Format of the line looks something like:
      //
      //  Volume sdcard /mnt/sdcard state changed from 7 (Shared-Unmounted) to 1 (Idle-Unmounted)
      //
      // So we parse out the state after the string " to "
      while (aTokenizer.hasMoreTokens()) {
        nsCAutoString token(aTokenizer.nextToken());
        if (token.Equals("to")) {
          PRInt32 errCode;
          token = aTokenizer.nextToken();
          SetState((STATE)token.ToInteger(&errCode));
          break;
        }
      }
      break;
    }

    case ResponseCode::VolumeDiskInserted:
      SetMediaPresent(true);
      break;

    case ResponseCode::VolumeDiskRemoved: // fall-thru
    case ResponseCode::VolumeBadRemoval:
      SetMediaPresent(false);
      break;

    default:
      LOG("Volume: %s unrecognized reponse code (ignored)", NameStr());
      break;
  }
}

} // namespace system
} // namespace mozilla
