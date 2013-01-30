/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVolume.h"

#include "base/message_loop.h"
#include "nsIPowerManagerService.h"
#include "nsISupportsUtils.h"
#include "nsIVolume.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsVolumeStat.h"
#include "nsXULAppAPI.h"
#include "Volume.h"

#define VOLUME_MANAGER_LOG_TAG  "nsVolume"
#include "VolumeManagerLog.h"

namespace mozilla {
namespace system {

const char *
NS_VolumeStateStr(int32_t aState)
{
  switch (aState) {
    case nsIVolume::STATE_INIT:       return "Init";
    case nsIVolume::STATE_NOMEDIA:    return "NoMedia";
    case nsIVolume::STATE_IDLE:       return "Idle";
    case nsIVolume::STATE_PENDING:    return "Pending";
    case nsIVolume::STATE_CHECKING:   return "Checking";
    case nsIVolume::STATE_MOUNTED:    return "Mounted";
    case nsIVolume::STATE_UNMOUNTING: return "Unmounting";
    case nsIVolume::STATE_FORMATTING: return "Formatting";
    case nsIVolume::STATE_SHARED:     return "Shared";
    case nsIVolume::STATE_SHAREDMNT:  return "Shared-Mounted";
  }
  return "???";
}

// While nsVolumes can only be used on the main thread, in the
// UpdateVolumeRunnable constructor (which is called from IOThread) we
// allocate an nsVolume which is then passed to MainThread. Since we
// have a situation where we allocate on one thread and free on another
// we use a thread safe AddRef implementation.
NS_IMPL_THREADSAFE_ISUPPORTS1(nsVolume, nsIVolume)

nsVolume::nsVolume(const Volume* aVolume)
  : mName(NS_ConvertUTF8toUTF16(aVolume->Name())),
    mMountPoint(NS_ConvertUTF8toUTF16(aVolume->MountPoint())),
    mState(aVolume->State()),
    mMountGeneration(aVolume->MountGeneration()),
    mMountLocked(aVolume->IsMountLocked())
{
}

NS_IMETHODIMP nsVolume::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP nsVolume::GetMountGeneration(int32_t* aMountGeneration)
{
  *aMountGeneration = mMountGeneration;
  return NS_OK;
}

NS_IMETHODIMP nsVolume::GetMountLockName(nsAString& aMountLockName)
{
  aMountLockName = NS_LITERAL_STRING("volume-") + Name();
  aMountLockName.AppendPrintf("-%d", mMountGeneration);

  return NS_OK;
}

NS_IMETHODIMP nsVolume::GetMountPoint(nsAString& aMountPoint)
{
  aMountPoint = mMountPoint;
  return NS_OK;
}

NS_IMETHODIMP nsVolume::GetState(int32_t* aState)
{
  *aState = mState;
  return NS_OK;
}

NS_IMETHODIMP nsVolume::GetStats(nsIVolumeStat **aResult)
{
  if (mState != STATE_MOUNTED) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_IF_ADDREF(*aResult = new nsVolumeStat(mMountPoint));
  return NS_OK;
}

void
nsVolume::LogState() const
{
  if (mState == nsIVolume::STATE_MOUNTED) {
    LOG("nsVolume: %s state %s @ '%s' gen %d locked %d",
        NameStr().get(), StateStr(), MountPointStr().get(),
        MountGeneration(), (int)IsMountLocked());
    return;
  }

  LOG("nsVolume: %s state %s", NameStr().get(), StateStr());
}

void nsVolume::Set(const nsVolume* aVolume)
{
  MOZ_ASSERT(NS_IsMainThread());

  mName = aVolume->mName;
  mMountPoint = aVolume->mMountPoint;
  mState = aVolume->mState;

  if (mState != nsIVolume::STATE_MOUNTED) {
    // Since we're not in the mounted state, we need to
    // forgot whatever mount generation we may have had.
    mMountGeneration = -1;
    return;
  }
  if (mMountGeneration == aVolume->mMountGeneration) {
    // No change in mount generation, nothing else to do
    return;
  }

  mMountGeneration = aVolume->mMountGeneration;

  // Notify the Volume on IOThread whether the volume is locked or not.
  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  if (!pmService) {
    return;
  }
  nsString mountLockName;
  GetMountLockName(mountLockName);
  nsString mountLockState;
  pmService->GetWakeLockState(mountLockName, mountLockState);
  UpdateMountLock(mountLockState);
}

void
nsVolume::UpdateMountLock(const nsAString& aMountLockState)
{
  MOZ_ASSERT(NS_IsMainThread());

  // There are 3 states, unlocked, locked-background, and locked-foreground
  // I figured it was easier to use negtive logic and compare for unlocked.
  UpdateMountLock(!aMountLockState.EqualsLiteral("unlocked"));
}

void
nsVolume::UpdateMountLock(bool aMountLocked)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aMountLocked == mMountLocked) {
    return;
  }
  // The locked/unlocked state changed. Tell IOThread about it.
  mMountLocked = aMountLocked;
  LogState();
  XRE_GetIOMessageLoop()->PostTask(
     FROM_HERE,
     NewRunnableFunction(Volume::UpdateMountLock,
                         NS_LossyConvertUTF16toASCII(Name()),
                         MountGeneration(), aMountLocked));
}

} // system
} // mozilla
