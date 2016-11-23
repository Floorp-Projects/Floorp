/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVolumeService.h"

#include "Volume.h"
#include "VolumeManager.h"
#include "VolumeServiceIOThread.h"

#include "nsCOMPtr.h"
#include "nsDependentSubstring.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIMutableArray.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPowerManagerService.h"
#include "nsISupportsPrimitives.h"
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
#include "base/task.h"

#undef VOLUME_MANAGER_LOG_TAG
#define VOLUME_MANAGER_LOG_TAG  "nsVolumeService"
#include "VolumeManagerLog.h"

#include <stdlib.h>

using namespace mozilla::dom;
using namespace mozilla::services;

namespace mozilla {
namespace system {

NS_IMPL_ISUPPORTS(nsVolumeService,
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
  RefPtr<nsVolumeService> volumeService = sSingleton.get();
  return volumeService.forget();
}

// static
void
nsVolumeService::Shutdown()
{
  if (!sSingleton) {
    return;
  }
  if (!XRE_IsParentProcess()) {
    sSingleton = nullptr;
    return;
  }

  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  if (pmService) {
    pmService->RemoveWakeLockListener(sSingleton.get());
  }

  XRE_GetIOMessageLoop()->PostTask(
      NewRunnableFunction(ShutdownVolumeServiceIOThread));

  sSingleton = nullptr;
}

nsVolumeService::nsVolumeService()
  : mArrayMonitor("nsVolumeServiceArray"),
    mGotVolumesFromParent(false)
{
  sSingleton = this;

  if (!XRE_IsParentProcess()) {
    // VolumeServiceIOThread and the WakeLock listener should only run in the
    // parent, so we return early.
    return;
  }

  // Startup the IOThread side of things. The actual volume changes
  // are captured by the IOThread and forwarded to main thread.
  XRE_GetIOMessageLoop()->PostTask(
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
NS_IMETHODIMP
nsVolumeService::Callback(const nsAString& aTopic, const nsAString& aState)
{
  CheckMountLock(aTopic, aState);
  return NS_OK;
}

void nsVolumeService::DumpNoLock(const char* aLabel)
{
  mArrayMonitor.AssertCurrentThreadOwns();

  nsVolume::Array::size_type numVolumes = mVolumeArray.Length();

  if (numVolumes == 0) {
    LOG("%s: No Volumes!", aLabel);
    return;
  }
  nsVolume::Array::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<nsVolume> vol = mVolumeArray[volIndex];
    vol->Dump(aLabel);
  }
}

NS_IMETHODIMP
nsVolumeService::Dump(const nsAString& aLabel)
{
  MonitorAutoLock autoLock(mArrayMonitor);
  DumpNoLock(NS_LossyConvertUTF16toASCII(aLabel).get());
  return NS_OK;
}

NS_IMETHODIMP nsVolumeService::GetVolumeByName(const nsAString& aVolName, nsIVolume **aResult)
{
  MonitorAutoLock autoLock(mArrayMonitor);

  RefPtr<nsVolume> vol = FindVolumeByName(aVolName);
  if (!vol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  vol.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsVolumeService::GetVolumeByPath(const nsAString& aPath, nsIVolume **aResult)
{
  NS_ConvertUTF16toUTF8 utf8Path(aPath);
  char realPathBuf[PATH_MAX];

  while (realpath(utf8Path.get(), realPathBuf) < 0) {
    if (errno != ENOENT) {
      ERR("GetVolumeByPath: realpath on '%s' failed: %d", utf8Path.get(), errno);
      return NSRESULT_FOR_ERRNO();
    }
    // The pathname we were passed doesn't exist, so we try stripping off trailing
    // components until we get a successful call to realpath, or until we run out
    // of components (if we finally get to /something then we also stop).
    int32_t slashIndex = utf8Path.RFindChar('/');
    if ((slashIndex == kNotFound) || (slashIndex == 0)) {
      errno = ENOENT;
      ERR("GetVolumeByPath: realpath on '%s' failed.", utf8Path.get());
      return NSRESULT_FOR_ERRNO();
    }
    utf8Path.Assign(Substring(utf8Path, 0, slashIndex));
  }

  // The volume mount point is always a directory. Something like /mnt/sdcard
  // Once we have a full qualified pathname with symlinks removed (which is
  // what realpath does), we basically check if aPath starts with the mount
  // point, but we don't want to have /mnt/sdcard match /mnt/sdcardfoo but we
  // do want it to match /mnt/sdcard/foo
  // So we add a trailing slash to the mount point and the pathname passed in
  // prior to doing the comparison.

  strlcat(realPathBuf, "/", sizeof(realPathBuf));

  MonitorAutoLock autoLock(mArrayMonitor);

  nsVolume::Array::size_type numVolumes = mVolumeArray.Length();
  nsVolume::Array::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<nsVolume> vol = mVolumeArray[volIndex];
    NS_ConvertUTF16toUTF8 volMountPointSlash(vol->MountPoint());
    volMountPointSlash.Append('/');
    nsDependentCSubstring testStr(realPathBuf, volMountPointSlash.Length());
    if (volMountPointSlash.Equals(testStr)) {
      vol.forget(aResult);
      return NS_OK;
    }
  }
  return NS_ERROR_FILE_NOT_FOUND;
}

NS_IMETHODIMP
nsVolumeService::CreateOrGetVolumeByPath(const nsAString& aPath, nsIVolume** aResult)
{
  nsresult rv = GetVolumeByPath(aPath, aResult);
  if (rv == NS_OK) {
    return NS_OK;
  }

  // In order to support queries by the updater, we will fabricate a volume
  // from the pathname, so that the caller can determine the volume size.
  nsCOMPtr<nsIVolume> vol = new nsVolume(NS_LITERAL_STRING("fake"),
                                         aPath, nsIVolume::STATE_MOUNTED,
                                         -1    /* generation */,
                                         true  /* isMediaPresent*/,
                                         false /* isSharing */,
                                         false /* isFormatting */,
                                         true  /* isFake */,
                                         false /* isUnmounting */,
                                         false /* isRemovable */,
                                         false /* isHotSwappable*/);
  vol.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsVolumeService::GetVolumeNames(nsIArray** aVolNames)
{
  NS_ENSURE_ARG_POINTER(aVolNames);
  MonitorAutoLock autoLock(mArrayMonitor);

  *aVolNames = nullptr;

  nsresult rv;
  nsCOMPtr<nsIMutableArray> volNames =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsVolume::Array::size_type numVolumes = mVolumeArray.Length();
  nsVolume::Array::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<nsVolume> vol = mVolumeArray[volIndex];
    nsCOMPtr<nsISupportsString> isupportsString =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = isupportsString->SetData(vol->Name());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = volNames->AppendElement(isupportsString, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  volNames.forget(aVolNames);
  return NS_OK;
}

void
nsVolumeService::GetVolumesForIPC(nsTArray<VolumeInfo>* aResult)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  MonitorAutoLock autoLock(mArrayMonitor);

  nsVolume::Array::size_type numVolumes = mVolumeArray.Length();
  nsVolume::Array::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<nsVolume> vol = mVolumeArray[volIndex];
    VolumeInfo* volInfo = aResult->AppendElement();

    volInfo->name()             = vol->mName;
    volInfo->mountPoint()       = vol->mMountPoint;
    volInfo->volState()         = vol->mState;
    volInfo->mountGeneration()  = vol->mMountGeneration;
    volInfo->isMediaPresent()   = vol->mIsMediaPresent;
    volInfo->isSharing()        = vol->mIsSharing;
    volInfo->isFormatting()     = vol->mIsFormatting;
    volInfo->isFake()           = vol->mIsFake;
    volInfo->isUnmounting()     = vol->mIsUnmounting;
    volInfo->isRemovable()      = vol->mIsRemovable;
    volInfo->isHotSwappable()   = vol->mIsHotSwappable;
  }
}

void
nsVolumeService::RecvVolumesFromParent(const nsTArray<VolumeInfo>& aVolumes)
{
  if (XRE_IsParentProcess()) {
    // We are the parent. Therefore our volumes are already correct.
    return;
  }
  if (mGotVolumesFromParent) {
    // We've already done this, no need to do it again.
    return;
  }

  for (uint32_t i = 0; i < aVolumes.Length(); i++) {
    const VolumeInfo& volInfo(aVolumes[i]);
    RefPtr<nsVolume> vol = new nsVolume(volInfo.name(),
                                          volInfo.mountPoint(),
                                          volInfo.volState(),
                                          volInfo.mountGeneration(),
                                          volInfo.isMediaPresent(),
                                          volInfo.isSharing(),
                                          volInfo.isFormatting(),
                                          volInfo.isFake(),
                                          volInfo.isUnmounting(),
                                          volInfo.isRemovable(),
                                          volInfo.isHotSwappable());
    UpdateVolume(vol, false);
  }
}

NS_IMETHODIMP
nsVolumeService::CreateMountLock(const nsAString& aVolumeName, nsIVolumeMountLock **aResult)
{
  nsCOMPtr<nsIVolumeMountLock> mountLock = nsVolumeMountLock::Create(aVolumeName);
  if (!mountLock) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mountLock.forget(aResult);
  return NS_OK;
}

void
nsVolumeService::CheckMountLock(const nsAString& aMountLockName,
                                const nsAString& aMountLockState)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<nsVolume> vol = FindVolumeByMountLockName(aMountLockName);
  if (vol) {
    vol->UpdateMountLock(aMountLockState);
  }
}

already_AddRefed<nsVolume>
nsVolumeService::FindVolumeByMountLockName(const nsAString& aMountLockName)
{
  MonitorAutoLock autoLock(mArrayMonitor);

  nsVolume::Array::size_type numVolumes = mVolumeArray.Length();
  nsVolume::Array::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<nsVolume> vol = mVolumeArray[volIndex];
    nsString mountLockName;
    vol->GetMountLockName(mountLockName);
    if (mountLockName.Equals(aMountLockName)) {
      return vol.forget();
    }
  }
  return nullptr;
}

already_AddRefed<nsVolume>
nsVolumeService::FindVolumeByName(const nsAString& aName, nsVolume::Array::index_type* aIndex)
{
  mArrayMonitor.AssertCurrentThreadOwns();

  nsVolume::Array::size_type numVolumes = mVolumeArray.Length();
  nsVolume::Array::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<nsVolume> vol = mVolumeArray[volIndex];
    if (vol->Name().Equals(aName)) {
      if (aIndex) {
        *aIndex = volIndex;
      }
      return vol.forget();
    }
  }
  return nullptr;
}

void
nsVolumeService::UpdateVolume(nsVolume* aVolume, bool aNotifyObservers)
{
  MOZ_ASSERT(NS_IsMainThread());

  {
    MonitorAutoLock autoLock(mArrayMonitor);
    nsVolume::Array::index_type volIndex;
    RefPtr<nsVolume> vol = FindVolumeByName(aVolume->Name(), &volIndex);
    if (!vol) {
      mVolumeArray.AppendElement(aVolume);
    } else if (vol->Equals(aVolume) || (!vol->IsFake() && aVolume->IsFake())) {
      // Ignore if nothing changed or if a fake tries to override a real volume.
      return;
    } else {
      mVolumeArray.ReplaceElementAt(volIndex, aVolume);
    }
    aVolume->UpdateMountLock(vol);
  }

  if (!aNotifyObservers) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs = GetObserverService();
  if (!obs) {
    return;
  }
  NS_ConvertUTF8toUTF16 stateStr(aVolume->StateStr());
  obs->NotifyObservers(aVolume, NS_VOLUME_STATE_CHANGED, stateStr.get());
}

void
nsVolumeService::RemoveVolumeByName(const nsAString& aName)
{
  {
    MonitorAutoLock autoLock(mArrayMonitor);
    nsVolume::Array::index_type volIndex;
    RefPtr<nsVolume> vol = FindVolumeByName(aName, &volIndex);
    if (!vol) {
      return;
    }
    mVolumeArray.RemoveElementAt(volIndex);
  }

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    if (!obs) {
      return;
    }
    obs->NotifyObservers(nullptr, NS_VOLUME_REMOVED, nsString(aName).get());
  }
}

/***************************************************************************
* The UpdateVolumeRunnable creates an nsVolume and updates the main thread
* data structure while running on the main thread.
*/
class UpdateVolumeRunnable : public Runnable
{
public:
  UpdateVolumeRunnable(nsVolumeService* aVolumeService, const Volume* aVolume)
    : mVolumeService(aVolumeService),
      mVolume(new nsVolume(aVolume))
  {
    MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    DBG("UpdateVolumeRunnable::Run '%s' state %s gen %d locked %d "
        "media %d sharing %d formatting %d unmounting %d removable %d hotswappable %d",
        mVolume->NameStr().get(), mVolume->StateStr(),
        mVolume->MountGeneration(), (int)mVolume->IsMountLocked(),
        (int)mVolume->IsMediaPresent(), mVolume->IsSharing(),
        mVolume->IsFormatting(), mVolume->IsUnmounting(),
        (int)mVolume->IsRemovable(), (int)mVolume->IsHotSwappable());

    mVolumeService->UpdateVolume(mVolume);
    mVolumeService = nullptr;
    mVolume = nullptr;
    return NS_OK;
  }

private:
  RefPtr<nsVolumeService> mVolumeService;
  RefPtr<nsVolume>        mVolume;
};

void
nsVolumeService::UpdateVolumeIOThread(const Volume* aVolume)
{
  DBG("UpdateVolumeIOThread: Volume '%s' state %s mount '%s' gen %d locked %d "
      "media %d sharing %d formatting %d unmounting %d removable %d hotswappable %d",
      aVolume->NameStr(), aVolume->StateStr(), aVolume->MountPoint().get(),
      aVolume->MountGeneration(), (int)aVolume->IsMountLocked(),
      (int)aVolume->MediaPresent(), (int)aVolume->IsSharing(),
      (int)aVolume->IsFormatting(), (int)aVolume->IsUnmounting(),
      (int)aVolume->IsRemovable(), (int)aVolume->IsHotSwappable());
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  NS_DispatchToMainThread(new UpdateVolumeRunnable(this, aVolume));
}

} // namespace system
} // namespace mozilla
