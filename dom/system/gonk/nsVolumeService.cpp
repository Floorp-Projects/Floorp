/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVolumeService.h"

#include "Volume.h"
#include "VolumeManager.h"
#include "VolumeServiceIOThread.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDependentSubstring.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPowerManagerService.h"
#include "nsISupportsUtils.h"
#include "nsIVolume.h"
#include "nsIVolumeService.h"
#include "nsLocalFile.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsVolumeMountLock.h"
#include "nsXULAppAPI.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Services.h"

#define VOLUME_MANAGER_LOG_TAG  "nsVolumeService"
#include "VolumeManagerLog.h"

#include <stdlib.h>

using namespace mozilla::dom;
using namespace mozilla::services;

namespace mozilla {
namespace system {

NS_IMPL_THREADSAFE_ISUPPORTS2(nsVolumeService,
                              nsIVolumeService,
                              nsIDOMMozWakeLockListener)

StaticRefPtr<nsVolumeService> nsVolumeService::sSingleton;

// static
already_AddRefed<nsVolumeService>
nsVolumeService::GetSingleton()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton) {
    sSingleton = new nsVolumeService();
  }
  NS_ADDREF(sSingleton.get());
  return sSingleton.get();
}

// static
void
nsVolumeService::Shutdown()
{
  if (!sSingleton || (XRE_GetProcessType() != GeckoProcessType_Default)) {
    return;
  }

  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  if (pmService) {
    pmService->RemoveWakeLockListener(sSingleton.get());
  }

  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(ShutdownVolumeServiceIOThread));

  sSingleton = nullptr;
}

nsVolumeService::nsVolumeService()
{
  sSingleton = this;

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    // We don't support the nsIVolumeService in the child processes,
    // but we get constructed due to the way we're registered with
    // nsLayoutModule.cpp. So we exit early to reduce our memory
    // impact, and so that we don't start unnecessary IOThread stuff.
    return;
  }

  // Startup the IOThread side of things. The actual volume changes
  // are captured by the IOThread and forwarded to main thread.
  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(InitVolumeServiceIOThread, this));

  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  if (!pmService) {
    return;
  }
  pmService->AddWakeLockListener(this);
}

nsVolumeService::~nsVolumeService()
{
}

// Callback for nsIDOMMozWakeLockListener
NS_IMETHODIMP nsVolumeService::Callback(const nsAString& aTopic, const nsAString& aState)
{
  CheckMountLock(aTopic, aState);
  return NS_OK;
}

NS_IMETHODIMP nsVolumeService::BroadcastVolume(const nsAString& aVolName)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  nsRefPtr<nsVolume> vol = FindVolumeByName(aVolName);
  if (!vol) {
    ERR("BroadcastVolume: Unable to locate volume '%s'",
        NS_LossyConvertUTF16toASCII(aVolName).get());
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIObserverService> obs = GetObserverService();
  NS_ENSURE_TRUE(obs, NS_NOINTERFACE);

  DBG("nsVolumeService::BroadcastVolume for '%s'", vol->NameStr());
  nsString stateStr(NS_ConvertUTF8toUTF16(vol->StateStr()));
  obs->NotifyObservers(vol, NS_VOLUME_STATE_CHANGED, stateStr.get());
  return NS_OK;
}

NS_IMETHODIMP nsVolumeService::GetVolumeByName(const nsAString& aVolName, nsIVolume **aResult)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  nsRefPtr<nsVolume> vol = FindVolumeByName(aVolName);
  if (!vol) {
    ERR("GetVolumeByName: Unable to locate volume '%s'",
        NS_LossyConvertUTF16toASCII(aVolName).get());
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ADDREF(*aResult = vol);
  return NS_OK;
}

NS_IMETHODIMP nsVolumeService::GetVolumeByPath(const nsAString& aPath, nsIVolume **aResult)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  nsCString utf8Path = NS_ConvertUTF16toUTF8(aPath);
  char realPathBuf[PATH_MAX];

  if (!realpath(utf8Path.get(), realPathBuf)) {
    ERR("GetVolumeByPath: realpath on '%s' failed: %d", utf8Path.get(), errno);
    return NSRESULT_FOR_ERRNO();
  }

  // The volume mount point is always a directory. Something like /mnt/sdcard
  // Once we have a full qualified pathname with symlinks removed (which is
  // what realpath does), we basically check if aPath starts with the mount
  // point, but we don't want to have /mnt/sdcard match /mnt/sdcardfoo but we
  // do want it to match /mnt/sdcard/foo
  // So we add a trailing slash to the mount point and the pathname passed in
  // prior to doing the comparison.

  strlcat(realPathBuf, "/", sizeof(realPathBuf));

  nsVolume::Array::size_type  numVolumes = mVolumeArray.Length();
  nsVolume::Array::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    nsRefPtr<nsVolume> vol = mVolumeArray[volIndex];
    nsAutoCString volMountPointSlash = NS_ConvertUTF16toUTF8(vol->MountPoint());
    volMountPointSlash.Append(NS_LITERAL_CSTRING("/"));
    nsDependentCSubstring testStr(realPathBuf, volMountPointSlash.Length());
    if (volMountPointSlash.Equals(testStr)) {
      NS_ADDREF(*aResult = vol);
      return NS_OK;
    }
  }

  // In order to support queries by DeviceStorage and the updater, we will fabricate
  // a volume from the pathname, so that the caller can determine the volume size
  nsRefPtr<nsVolume> vol = new nsVolume(NS_LITERAL_STRING("fake"),
                                        aPath, nsIVolume::STATE_MOUNTED,
                                        -1 /*generation*/);
  NS_ADDREF(*aResult = vol);
  return NS_OK;
}

NS_IMETHODIMP nsVolumeService::CreateMountLock(const nsAString& aVolumeName, nsIVolumeMountLock **aResult)
{
  nsRefPtr<nsVolumeMountLock> mountLock = nsVolumeMountLock::Create(aVolumeName);
  if (!mountLock) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  NS_ADDREF(*aResult = mountLock);
  return NS_OK;
}

void nsVolumeService::CheckMountLock(const nsAString& aMountLockName,
                                     const nsAString& aMountLockState)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());

  nsVolume::Array::size_type  numVolumes = mVolumeArray.Length();
  nsVolume::Array::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    nsRefPtr<nsVolume> vol = mVolumeArray[volIndex];
    nsString mountLockName;
    vol->GetMountLockName(mountLockName);
    if (mountLockName.Equals(aMountLockName)) {
      vol->UpdateMountLock(aMountLockState);
      return;
    }
  }
}

already_AddRefed<nsVolume> nsVolumeService::FindVolumeByName(const nsAString& aName)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());

  nsVolume::Array::size_type  numVolumes = mVolumeArray.Length();
  nsVolume::Array::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    nsRefPtr<nsVolume> vol = mVolumeArray[volIndex];
    if (vol->Name().Equals(aName)) {
      return vol.forget();
    }
  }
  return NULL;
}

//static
already_AddRefed<nsVolume> nsVolumeService::FindAddVolumeByName(const nsAString& aName)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<nsVolume> vol;
  vol = FindVolumeByName(aName);
  if (vol) {
    return vol.forget();
  }
  // Volume not found - add a new one
  vol = new nsVolume(aName);
  mVolumeArray.AppendElement(vol);
  return vol.forget();
}

void nsVolumeService::UpdateVolume(const nsVolume* aVolume)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<nsVolume> vol = FindAddVolumeByName(aVolume->Name());
  if (vol->Equals(aVolume)) {
    // Nothing has really changed. Don't bother telling anybody.
    return;
  }
  vol->Set(aVolume);
  nsCOMPtr<nsIObserverService> obs = GetObserverService();
  if (!obs) {
    return;
  }
  nsString stateStr(NS_ConvertUTF8toUTF16(vol->StateStr()));
  obs->NotifyObservers(vol, NS_VOLUME_STATE_CHANGED, stateStr.get());
}

/***************************************************************************
* The UpdateVolumeRunnable creates an nsVolume and updates the main thread
* data structure while running on the main thread.
*/
class UpdateVolumeRunnable : public nsRunnable
{
public:
  UpdateVolumeRunnable(nsVolumeService* aVolumeService, const Volume* aVolume)
    : mVolumeService(aVolumeService),
      mVolume(new nsVolume(aVolume))
  {
    MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    DBG("UpdateVolumeRunnable::Run '%s' state %s gen %d locked %d",
        mVolume->NameStr(), mVolume->StateStr(), mVolume->MountGeneration(),
        (int)mVolume->IsMountLocked());

    mVolumeService->UpdateVolume(mVolume);
    mVolumeService = NULL;
    mVolume = NULL;
    return NS_OK;
  }

private:
  nsRefPtr<nsVolumeService> mVolumeService;
  nsRefPtr<nsVolume>        mVolume;
};

void nsVolumeService::UpdateVolumeIOThread(const Volume* aVolume)
{
  DBG("UpdateVolumeIOThread: Volume '%s' state %s mount '%s' gen %d locked %d",
      aVolume->NameStr(), aVolume->StateStr(), aVolume->MountPoint().get(),
      aVolume->MountGeneration(), (int)aVolume->IsMountLocked());
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  NS_DispatchToMainThread(new UpdateVolumeRunnable(this, aVolume));
}

} // namespace system
} // namespace mozilla
