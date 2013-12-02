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
#include "AutoMounter.h"
#include "VolumeManager.h"

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
NS_IMPL_ISUPPORTS1(nsVolume, nsIVolume)

nsVolume::nsVolume(const Volume* aVolume)
  : mName(NS_ConvertUTF8toUTF16(aVolume->Name())),
    mMountPoint(NS_ConvertUTF8toUTF16(aVolume->MountPoint())),
    mState(aVolume->State()),
    mMountGeneration(aVolume->MountGeneration()),
    mMountLocked(aVolume->IsMountLocked()),
    mIsFake(false),
    mIsMediaPresent(aVolume->MediaPresent()),
    mIsSharing(aVolume->IsSharing()),
    mIsFormatting(aVolume->IsFormatting())
{
}

bool nsVolume::Equals(nsIVolume* aVolume)
{
  nsString volName;
  aVolume->GetName(volName);
  if (!mName.Equals(volName)) {
    return false;
  }

  nsString volMountPoint;
  aVolume->GetMountPoint(volMountPoint);
  if (!mMountPoint.Equals(volMountPoint)) {
    return false;
  }

  int32_t volState;
  aVolume->GetState(&volState);
  if (mState != volState){
    return false;
  }

  int32_t volMountGeneration;
  aVolume->GetMountGeneration(&volMountGeneration);
  if (mMountGeneration != volMountGeneration) {
    return false;
  }

  bool volIsMountLocked;
  aVolume->GetIsMountLocked(&volIsMountLocked);
  if (mMountLocked != volIsMountLocked) {
    return false;
  }

  bool isFake;
  aVolume->GetIsFake(&isFake);
  if (mIsFake != isFake) {
    return false;
  }

  bool isSharing;
  aVolume->GetIsSharing(&isSharing);
  if (mIsSharing != isSharing) {
    return false;
  }

  bool isFormatting;
  aVolume->GetIsFormatting(&isFormatting);
  if (mIsFormatting != isFormatting) {
    return false;
  }

  return true;
}

NS_IMETHODIMP nsVolume::GetIsMediaPresent(bool *aIsMediaPresent)
{
  *aIsMediaPresent = mIsMediaPresent;
  return NS_OK;
}

NS_IMETHODIMP nsVolume::GetIsMountLocked(bool *aIsMountLocked)
{
  *aIsMountLocked = mMountLocked;
  return NS_OK;
}

NS_IMETHODIMP nsVolume::GetIsSharing(bool *aIsSharing)
{
  *aIsSharing = mIsSharing;
  return NS_OK;
}

NS_IMETHODIMP nsVolume::GetIsFormatting(bool *aIsFormatting)
{
  *aIsFormatting = mIsFormatting;
  return NS_OK;
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

NS_IMETHODIMP nsVolume::GetIsFake(bool *aIsFake)
{
  *aIsFake = mIsFake;
  return NS_OK;
}

NS_IMETHODIMP nsVolume::Format()
{
  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(FormatVolumeIOThread, NameStr()));

  return NS_OK;
}

/* static */
void nsVolume::FormatVolumeIOThread(const nsCString& aVolume)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  if (VolumeManager::State() != VolumeManager::VOLUMES_READY) {
    return;
  }

  AutoMounterFormatVolume(aVolume);
}

void
nsVolume::LogState() const
{
  if (mState == nsIVolume::STATE_MOUNTED) {
    LOG("nsVolume: %s state %s @ '%s' gen %d locked %d fake %d "
        "media %d sharing %d formatting %d",
        NameStr().get(), StateStr(), MountPointStr().get(),
        MountGeneration(), (int)IsMountLocked(), (int)IsFake(),
        (int)IsMediaPresent(), (int)IsSharing(),
        (int)IsFormatting());
    return;
  }

  LOG("nsVolume: %s state %s", NameStr().get(), StateStr());
}

void nsVolume::Set(nsIVolume* aVolume)
{
  MOZ_ASSERT(NS_IsMainThread());

  aVolume->GetName(mName);
  aVolume->GetMountPoint(mMountPoint);
  aVolume->GetState(&mState);
  aVolume->GetIsFake(&mIsFake);
  aVolume->GetIsMediaPresent(&mIsMediaPresent);
  aVolume->GetIsSharing(&mIsSharing);
  aVolume->GetIsFormatting(&mIsFormatting);

  int32_t volMountGeneration;
  aVolume->GetMountGeneration(&volMountGeneration);

  if (mState != nsIVolume::STATE_MOUNTED) {
    // Since we're not in the mounted state, we need to
    // forgot whatever mount generation we may have had.
    mMountGeneration = -1;
    return;
  }
  if (mMountGeneration == volMountGeneration) {
    // No change in mount generation, nothing else to do
    return;
  }

  mMountGeneration = volMountGeneration;

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    // Child processes just track the state, not maintain it.
    aVolume->GetIsMountLocked(&mMountLocked);
    return;
  }

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
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());

  // There are 3 states, unlocked, locked-background, and locked-foreground
  // I figured it was easier to use negtive logic and compare for unlocked.
  UpdateMountLock(!aMountLockState.EqualsLiteral("unlocked"));
}

void
nsVolume::UpdateMountLock(bool aMountLocked)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
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

void
nsVolume::SetIsFake(bool aIsFake)
{
  mIsFake = aIsFake;
  if (mIsFake) {
    // The media is always present for fake volumes.
    mIsMediaPresent = true;
    MOZ_ASSERT(!mIsSharing);
  }
}

void
nsVolume::SetState(int32_t aState)
{
  static int32_t sMountGeneration = 0;

  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsFake());

  if (aState == mState) {
    return;
  }

  if (aState == nsIVolume::STATE_MOUNTED) {
    mMountGeneration = ++sMountGeneration;
  }

  mState = aState;
}

} // system
} // mozilla
