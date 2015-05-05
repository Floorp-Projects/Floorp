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
  nsRefPtr<nsVolumeService> volumeService = sSingleton.get();
  return volumeService.forget();
}

// static
void
nsVolumeService::Shutdown()
{
  if (!sSingleton) {
    return;
  }
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    sSingleton = nullptr;
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
  : mArrayMonitor("nsVolumeServiceArray"),
    mGotVolumesFromParent(false)
{
  sSingleton = this;

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    // VolumeServiceIOThread and the WakeLock listener should only run in the
    // parent, so we return early.
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
    nsRefPtr<nsVolume> vol = mVolumeArray[volIndex];
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

  nsRefPtr<nsVolume> vol = FindVolumeByName(aVolName);
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
    nsRefPtr<nsVolume> vol = mVolumeArray[volIndex];
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
    nsRefPtr<nsVolume> vol = mVolumeArray[volIndex];
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
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());

  MonitorAutoLock autoLock(mArrayMonitor);

  nsVolume::Array::size_type numVolumes = mVolumeArray.Length();
  nsVolume::Array::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    nsRefPtr<nsVolume> vol = mVolumeArray[volIndex];
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
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // We are the parent. Therefore our volumes are already correct.
    return;
  }
  if (mGotVolumesFromParent) {
    // We've already done this, no need to do it again.
    return;
  }

  for (uint32_t i = 0; i < aVolumes.Length(); i++) {
    const VolumeInfo& volInfo(aVolumes[i]);
    nsRefPtr<nsVolume> vol = new nsVolume(volInfo.name(),
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
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<nsVolume> vol = FindVolumeByMountLockName(aMountLockName);
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
    nsRefPtr<nsVolume> vol = mVolumeArray[volIndex];
    nsString mountLockName;
    vol->GetMountLockName(mountLockName);
    if (mountLockName.Equals(aMountLockName)) {
      return vol.forget();
    }
  }
  return nullptr;
}

already_AddRefed<nsVolume>
nsVolumeService::FindVolumeByName(const nsAString& aName)
{
  mArrayMonitor.AssertCurrentThreadOwns();

  nsVolume::Array::size_type numVolumes = mVolumeArray.Length();
  nsVolume::Array::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    nsRefPtr<nsVolume> vol = mVolumeArray[volIndex];
    if (vol->Name().Equals(aName)) {
      return vol.forget();
    }
  }
  return nullptr;
}

//static
already_AddRefed<nsVolume>
nsVolumeService::CreateOrFindVolumeByName(const nsAString& aName, bool aIsFake /*= false*/)
{
  MonitorAutoLock autoLock(mArrayMonitor);

  nsRefPtr<nsVolume> vol;
  vol = FindVolumeByName(aName);
  if (vol) {
    return vol.forget();
  }
  // Volume not found - add a new one
  vol = new nsVolume(aName);
  vol->SetIsFake(aIsFake);
  mVolumeArray.AppendElement(vol);
  return vol.forget();
}

void
nsVolumeService::UpdateVolume(nsIVolume* aVolume, bool aNotifyObservers)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsString volName;
  aVolume->GetName(volName);
  bool aIsFake;
  aVolume->GetIsFake(&aIsFake);
  nsRefPtr<nsVolume> vol = CreateOrFindVolumeByName(volName, aIsFake);
  if (vol->Equals(aVolume)) {
    // Nothing has really changed. Don't bother telling anybody.
    return;
  }

  if (!vol->IsFake() && aIsFake) {
    // Prevent an incoming fake volume from overriding an existing real volume.
    return;
  }

  vol->Set(aVolume);

  if (!aNotifyObservers) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs = GetObserverService();
  if (!obs) {
    return;
  }
  NS_ConvertUTF8toUTF16 stateStr(vol->StateStr());
  obs->NotifyObservers(vol, NS_VOLUME_STATE_CHANGED, stateStr.get());
}

NS_IMETHODIMP
nsVolumeService::CreateFakeVolume(const nsAString& name, const nsAString& path)
{
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    nsRefPtr<nsVolume> vol = new nsVolume(name, path, nsIVolume::STATE_MOUNTED,
                                          -1    /* mountGeneration */,
                                          true  /* isMediaPresent */,
                                          false /* isSharing */,
                                          false /* isFormatting */,
                                          true  /* isFake */,
                                          false /* isUnmounting */,
                                          false /* isRemovable */,
                                          false /* isHotSwappable */);
    vol->LogState();
    UpdateVolume(vol.get());
    return NS_OK;
  }

  ContentChild::GetSingleton()->SendCreateFakeVolume(nsString(name), nsString(path));
  return NS_OK;
}

NS_IMETHODIMP
nsVolumeService::SetFakeVolumeState(const nsAString& name, int32_t state)
{
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    nsRefPtr<nsVolume> vol;
    {
      MonitorAutoLock autoLock(mArrayMonitor);
      vol = FindVolumeByName(name);
    }
    if (!vol || !vol->IsFake()) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    // UpdateVolume expects the volume passed in to NOT be the
    // same pointer as what CreateOrFindVolumeByName would return,
    // which is why we allocate a temporary volume here.
    nsRefPtr<nsVolume> volume = new nsVolume(name);
    volume->Set(vol);
    volume->SetState(state);
    volume->LogState();
    UpdateVolume(volume.get());
    return NS_OK;
  }

  ContentChild::GetSingleton()->SendSetFakeVolumeState(nsString(name), state);
  return NS_OK;
}

NS_IMETHODIMP
nsVolumeService::RemoveFakeVolume(const nsAString& name)
{
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    SetFakeVolumeState(name, nsIVolume::STATE_NOMEDIA);
    RemoveVolumeByName(name);
    return NS_OK;
  }

  ContentChild::GetSingleton()->SendRemoveFakeVolume(nsString(name));
  return NS_OK;
}

void
nsVolumeService::RemoveVolumeByName(const nsAString& aName)
{
  nsRefPtr<nsVolume> vol;
  {
    MonitorAutoLock autoLock(mArrayMonitor);
    vol = FindVolumeByName(aName);
  }
  if (!vol) {
    return;
  }
  mVolumeArray.RemoveElement(vol);

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
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
  nsRefPtr<nsVolumeService> mVolumeService;
  nsRefPtr<nsVolume>        mVolume;
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
