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

#if DEBUG_VOLUME_OBSERVER
void
VolumeObserverList::Broadcast(Volume* const& aVolume)
{
  uint32_t size = mObservers.Length();
  for (uint32_t i = 0; i < size; ++i) {
    LOG("VolumeObserverList::Broadcast to [%u] %p volume '%s'",
        i, mObservers[i], aVolume->NameStr());
    mObservers[i]->Notify(aVolume);
  }
}
#endif

VolumeObserverList Volume::sEventObserverList;

// We have a feature where volumes can be locked when mounted. This
// is used to prevent a volume from being shared with the PC while
// it is actively being used (say for storing an update image)
//
// We use WakeLocks (a poor choice of name, but it does what we want)
// from the PowerManagerService to determine when we're locked.
// In particular we'll create a wakelock called volume-NAME-GENERATION
// (where NAME is the volume name, and GENERATION is its generation
// number), and if this wakelock is locked, then we'll prevent a volume
// from being shared.
//
// Implementation Details:
//
// Since the AutoMounter can only control when something gets mounted
// and not when it gets unmounted (for example: a user pulls the SDCard)
// and because Volume and nsVolume data structures are maintained on
// separate threads, we have the potential for some race conditions.
// We eliminate the race conditions by introducing the concept of a
// generation number. Every time a volume transitions to the Mounted
// state, it gets assigned a new generation number. Whenever the state
// of a Volume changes, we send the updated state and current generation
// number to the main thread where it gets updated in the nsVolume.
//
// Since WakeLocks can only be queried from the main-thread, the
// nsVolumeService looks for WakeLock status changes, and forwards
// the results to the IOThread.
//
// If the Volume (IOThread) recieves a volume update where the generation
// number mismatches, then the update is simply ignored.
//
// When a Volume (IOThread) initially becomes mounted, we assume it to
// be locked until we get our first update from nsVolume (MainThread).
static int32_t sMountGeneration = 0;

static uint32_t sNextId = 1;

// We don't get media inserted/removed events at startup. So we
// assume it's present, and we'll be told that it's missing.
Volume::Volume(const nsCSubstring& aName)
  : mMediaPresent(true),
    mState(nsIVolume::STATE_INIT),
    mName(aName),
    mMountGeneration(-1),
    mMountLocked(true),  // Needs to agree with nsVolume::nsVolume
    mSharingEnabled(false),
    mFormatRequested(false),
    mMountRequested(false),
    mUnmountRequested(false),
    mCanBeShared(true),
    mIsSharing(false),
    mIsFormatting(false),
    mIsUnmounting(false),
    mId(sNextId++)
{
  DBG("Volume %s: created", NameStr());
}

void
Volume::Dump(const char* aLabel) const
{
  LOG("%s: Volume: %s (%d) is %s and %s @ %s gen %d locked %d",
      aLabel,
      NameStr(),
      Id(),
      StateStr(),
      MediaPresent() ? "inserted" : "missing",
      MountPoint().get(),
      MountGeneration(),
      (int)IsMountLocked());
  LOG("%s:   Sharing %s Mounting %s Formating %s Unmounting %s",
      aLabel,
      CanBeShared() ? (IsSharingEnabled() ? (IsSharing() ? "en-y" : "en-n")
                                          : "dis")
                    : "x",
      IsMountRequested() ? "req" : "n",
      IsFormatRequested() ? (IsFormatting() ? "req-y" : "req-n")
                          : (IsFormatting() ? "y" : "n"),
      IsUnmountRequested() ? (IsUnmounting() ? "req-y" : "req-n")
                           : (IsUnmounting() ? "y" : "n"));
}

void Volume::SetFakeVolume(const nsACString& aMountPoint)
{
  this->mMountLocked = false;
  this->mCanBeShared = false;
  this->mMountPoint = aMountPoint;
  SetState(nsIVolume::STATE_MOUNTED);
}

void
Volume::SetIsSharing(bool aIsSharing)
{
  if (aIsSharing == mIsSharing) {
    return;
  }
  mIsSharing = aIsSharing;
  LOG("Volume %s: IsSharing set to %d state %s",
      NameStr(), (int)mIsSharing, StateStr(mState));
  sEventObserverList.Broadcast(this);
}

void
Volume::SetIsFormatting(bool aIsFormatting)
{
  if (aIsFormatting == mIsFormatting) {
    return;
  }
  mIsFormatting = aIsFormatting;
  LOG("Volume %s: IsFormatting set to %d state %s",
      NameStr(), (int)mIsFormatting, StateStr(mState));
  if (mIsFormatting) {
    sEventObserverList.Broadcast(this);
  }
}

void
Volume::SetIsUnmounting(bool aIsUnmounting)
{
  if (aIsUnmounting == mIsUnmounting) {
    return;
  }
  mIsUnmounting = aIsUnmounting;
  LOG("Volume %s: IsUnmounting set to %d state %s",
      NameStr(), (int)mIsUnmounting, StateStr(mState));
  sEventObserverList.Broadcast(this);
}

void
Volume::SetMediaPresent(bool aMediaPresent)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

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

  if (mMediaPresent == aMediaPresent) {
    return;
  }

  LOG("Volume: %s media %s", NameStr(), aMediaPresent ? "inserted" : "removed");
  mMediaPresent = aMediaPresent;
  sEventObserverList.Broadcast(this);
}

void
Volume::SetSharingEnabled(bool aSharingEnabled)
{
  mSharingEnabled = aSharingEnabled;

  LOG("SetSharingMode for volume %s to %d canBeShared = %d",
      NameStr(), (int)mSharingEnabled, (int)mCanBeShared);
  sEventObserverList.Broadcast(this);
}

void
Volume::SetFormatRequested(bool aFormatRequested)
{
  mFormatRequested = aFormatRequested;

  LOG("SetFormatRequested for volume %s to %d CanBeFormatted = %d",
      NameStr(), (int)mFormatRequested, (int)CanBeFormatted());
}

void
Volume::SetMountRequested(bool aMountRequested)
{
  mMountRequested = aMountRequested;

  LOG("SetMountRequested for volume %s to %d CanBeMounted = %d",
      NameStr(), (int)mMountRequested, (int)CanBeMounted());
}

void
Volume::SetUnmountRequested(bool aUnmountRequested)
{
  mUnmountRequested = aUnmountRequested;

  LOG("SetUnmountRequested for volume %s to %d CanBeMounted = %d",
      NameStr(), (int)mUnmountRequested, (int)CanBeMounted());
}

void
Volume::SetState(Volume::STATE aNewState)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  if (aNewState == mState) {
    return;
  }
  if (aNewState == nsIVolume::STATE_MOUNTED) {
    mMountGeneration = ++sMountGeneration;
    LOG("Volume %s (%u): changing state from %s to %s @ '%s' (%d observers) "
        "mountGeneration = %d, locked = %d",
        NameStr(), mId, StateStr(mState),
        StateStr(aNewState), mMountPoint.get(), sEventObserverList.Length(),
        mMountGeneration, (int)mMountLocked);
  } else {
    LOG("Volume %s (%u): changing state from %s to %s (%d observers)",
        NameStr(), mId, StateStr(mState),
        StateStr(aNewState), sEventObserverList.Length());
  }

  switch (aNewState) {
     case nsIVolume::STATE_NOMEDIA:
       // Cover the startup case where we don't get insertion/removal events
       mMediaPresent = false;
       mIsSharing = false;
       mUnmountRequested = false;
       mMountRequested = false;
       mIsUnmounting = false;
       break;

     case nsIVolume::STATE_MOUNTED:
       mMountRequested = false;
       mIsFormatting = false;
       mIsSharing = false;
       mIsUnmounting = false;
       break;
     case nsIVolume::STATE_FORMATTING:
       mFormatRequested = false;
       mIsFormatting = true;
       mIsSharing = false;
       mIsUnmounting = false;
       break;

     case nsIVolume::STATE_SHARED:
     case nsIVolume::STATE_SHAREDMNT:
       // Covers startup cases. Normally, mIsSharing would be set to true
       // when we issue the command to initiate the sharing process, but
       // it's conceivable that a volume could already be in a shared state
       // when b2g starts.
       mIsSharing = true;
       mIsUnmounting = false;
       mIsFormatting = false;
       break;

     case nsIVolume::STATE_UNMOUNTING:
       mIsUnmounting = true;
       mIsFormatting = false;
       mIsSharing = false;
       break;

     case nsIVolume::STATE_IDLE: // Fall through
     case nsIVolume::STATE_CHECKMNT: // Fall through
     default:
       break;
  }
  mState = aNewState;
  sEventObserverList.Broadcast(this);
}

void
Volume::SetMountPoint(const nsCSubstring& aMountPoint)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  if (mMountPoint.Equals(aMountPoint)) {
    return;
  }
  mMountPoint = aMountPoint;
  DBG("Volume %s: Setting mountpoint to '%s'", NameStr(), mMountPoint.get());
}

void
Volume::StartMount(VolumeResponseCallback* aCallback)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  StartCommand(new VolumeActionCommand(this, "mount", "", aCallback));
}

void
Volume::StartUnmount(VolumeResponseCallback* aCallback)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  StartCommand(new VolumeActionCommand(this, "unmount", "force", aCallback));
}

void
Volume::StartFormat(VolumeResponseCallback* aCallback)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  StartCommand(new VolumeActionCommand(this, "format", "", aCallback));
}

void
Volume::StartShare(VolumeResponseCallback* aCallback)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  StartCommand(new VolumeActionCommand(this, "share", "ums", aCallback));
}

void
Volume::StartUnshare(VolumeResponseCallback* aCallback)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  StartCommand(new VolumeActionCommand(this, "unshare", "ums", aCallback));
}

void
Volume::StartCommand(VolumeCommand* aCommand)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  VolumeManager::PostCommand(aCommand);
}

//static
void
Volume::RegisterVolumeObserver(Volume::EventObserver* aObserver, const char* aName)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  sEventObserverList.AddObserver(aObserver);

  DBG("Added Volume Observer '%s' @%p, length = %u",
      aName, aObserver, sEventObserverList.Length());

  // Send an initial event to the observer (for each volume)
  size_t numVolumes = VolumeManager::NumVolumes();
  for (size_t volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<Volume> vol = VolumeManager::GetVolume(volIndex);
    aObserver->Notify(vol);
  }
}

//static
void
Volume::UnregisterVolumeObserver(Volume::EventObserver* aObserver, const char* aName)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  sEventObserverList.RemoveObserver(aObserver);

  DBG("Removed Volume Observer '%s' @%p, length = %u",
      aName, aObserver, sEventObserverList.Length());
}

//static
void
Volume::UpdateMountLock(const nsACString& aVolumeName,
                        const int32_t& aMountGeneration,
                        const bool& aMountLocked)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  RefPtr<Volume> vol = VolumeManager::FindVolumeByName(aVolumeName);
  if (!vol || (vol->mMountGeneration != aMountGeneration)) {
    return;
  }
  if (vol->mMountLocked != aMountLocked) {
    vol->mMountLocked = aMountLocked;
    DBG("Volume::UpdateMountLock for '%s' to %d\n", vol->NameStr(), (int)aMountLocked);
    sEventObserverList.Broadcast(vol);
  }
}

void
Volume::HandleVoldResponse(int aResponseCode, nsCWhitespaceTokenizer& aTokenizer)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  // The volume name will have already been parsed, and the tokenizer will point
  // to the token after the volume name
  switch (aResponseCode) {
    case ::ResponseCode::VolumeListResult: {
      // Each line will look something like:
      //
      //  sdcard /mnt/sdcard 1
      //
      nsDependentCSubstring mntPoint(aTokenizer.nextToken());
      SetMountPoint(mntPoint);
      nsresult errCode;
      nsCString state(aTokenizer.nextToken());
      if (state.EqualsLiteral("X")) {
        // Special state for creating fake volumes which can't be shared.
        mCanBeShared = false;
        SetState(nsIVolume::STATE_MOUNTED);
      } else {
        SetState((STATE)state.ToInteger(&errCode));
      }
      break;
    }

    case ::ResponseCode::VolumeStateChange: {
      // Format of the line looks something like:
      //
      //  Volume sdcard /mnt/sdcard state changed from 7 (Shared-Unmounted) to 1 (Idle-Unmounted)
      //
      // So we parse out the state after the string " to "
      while (aTokenizer.hasMoreTokens()) {
        nsAutoCString token(aTokenizer.nextToken());
        if (token.EqualsLiteral("to")) {
          nsresult errCode;
          token = aTokenizer.nextToken();
          STATE newState = (STATE)(token.ToInteger(&errCode));
          if (newState == nsIVolume::STATE_MOUNTED) {
            // We set the state to STATE_CHECKMNT here, and the once the
            // AutoMounter detects that the volume is actually accessible
            // then the AutoMounter will set the volume as STATE_MOUNTED.
            SetState(nsIVolume::STATE_CHECKMNT);
          } else {
            SetState(newState);
          }
          break;
        }
      }
      break;
    }

    case ::ResponseCode::VolumeDiskInserted:
      SetMediaPresent(true);
      break;

    case ::ResponseCode::VolumeDiskRemoved: // fall-thru
    case ::ResponseCode::VolumeBadRemoval:
      SetMediaPresent(false);
      break;

    default:
      LOG("Volume: %s unrecognized reponse code (ignored)", NameStr());
      break;
  }
}

} // namespace system
} // namespace mozilla
