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
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupportsUtils.h"
#include "nsIVolume.h"
#include "nsIVolumeService.h"
#include "nsLocalFile.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/Services.h"

#define VOLUME_MANAGER_LOG_TAG  "nsVolumeService"
#include "VolumeManagerLog.h"

#include <stdlib.h>

using namespace mozilla::services;

namespace mozilla {
namespace system {

NS_IMPL_ISUPPORTS1(nsVolumeService, nsIVolumeService)

nsVolumeService::nsVolumeService()
{
  // Startup the IOThread side of things. The actual volume changes
  // are captured by the IOThread and forwarded to main thread.
  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(InitVolumeServiceIOThread));
}

nsVolumeService::~nsVolumeService()
{
  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(ShutdownVolumeServiceIOThread));
}

/* nsIVolume getVolumeByName (in DOMString volName); */
NS_IMETHODIMP nsVolumeService::GetVolumeByName(const nsAString &aVolName, nsIVolume **aResult)
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

/* nsIVolume getVolumeByPath (in DOMString path); */
NS_IMETHODIMP nsVolumeService::GetVolumeByPath(const nsAString &aPath, nsIVolume **aResult)
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
  aPath, nsIVolume::STATE_MOUNTED);
  NS_ADDREF(*aResult = vol);
  return NS_OK;
}

already_AddRefed<nsVolume> nsVolumeService::FindVolumeByName(const nsAString &aName)
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

already_AddRefed<nsVolume> nsVolumeService::FindAddVolumeByName(const nsAString &aName)
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

void nsVolumeService::UpdateVolume(const nsVolume *aVolume)
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
  if (aVolume->State() == nsIVolume::STATE_MOUNTED) {
    LOG("UpdateVolume: '%s' state %s @ '%s'",
        aVolume->NameStr(), aVolume->StateStr(), aVolume->MountPointStr());
  } else {
    LOG("UpdateVolume: '%s' state %s",
        aVolume->NameStr(), aVolume->StateStr());
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
  UpdateVolumeRunnable(const Volume *aVolume)
    : mVolume(new nsVolume(aVolume))
  {
    MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    DBG("UpdateVolumeRunnable::Run '%s' state %s",
        mVolume->NameStr(), mVolume->StateStr());

    nsCOMPtr<nsIVolumeService> ivs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
    if (!ivs) {
      return NS_OK;
    }
    nsCOMPtr<nsVolumeService> vs(do_QueryInterface(ivs));
    if (!vs) {
      return NS_OK;
    }
    vs->UpdateVolume(mVolume);
    mVolume = NULL;
    return NS_OK;
  }

private:
  nsRefPtr<nsVolume>  mVolume;
};

//static
void nsVolumeService::UpdateVolumeIOThread(const Volume *aVolume)
{
  DBG("UpdateVolumeIOThread: Volume '%s' state %s mount '%s'",
      aVolume->NameStr(), aVolume->StateStr(), aVolume->MountPoint().get());
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  NS_DispatchToMainThread(new UpdateVolumeRunnable(aVolume));
}

} // system
} // mozilla
