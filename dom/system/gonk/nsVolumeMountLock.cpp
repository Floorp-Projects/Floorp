/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVolumeMountLock.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/Services.h"

#include "nsIObserverService.h"
#include "nsIPowerManagerService.h"
#include "nsIVolume.h"
#include "nsIVolumeService.h"
#include "nsString.h"
#include "nsXULAppAPI.h"

#define VOLUME_MANAGER_LOG_TAG  "nsVolumeMountLock"
#include "VolumeManagerLog.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/power/PowerManagerService.h"

using namespace mozilla::dom;
using namespace mozilla::services;

namespace mozilla {
namespace system {

NS_IMPL_ISUPPORTS(nsVolumeMountLock, nsIVolumeMountLock,
                  nsIObserver, nsISupportsWeakReference)

// static
already_AddRefed<nsVolumeMountLock>
nsVolumeMountLock::Create(const nsAString& aVolumeName)
{
  DBG("nsVolumeMountLock::Create called");

  nsRefPtr<nsVolumeMountLock> mountLock = new nsVolumeMountLock(aVolumeName);
  nsresult rv = mountLock->Init();
  NS_ENSURE_SUCCESS(rv, nullptr);

  return mountLock.forget();
}

nsVolumeMountLock::nsVolumeMountLock(const nsAString& aVolumeName)
  : mVolumeName(aVolumeName),
    mVolumeGeneration(-1),
    mUnlocked(false)
{
}

//virtual
nsVolumeMountLock::~nsVolumeMountLock()
{
  Unlock();
}

nsresult nsVolumeMountLock::Init()
{
  LOG("nsVolumeMountLock created for '%s'",
      NS_LossyConvertUTF16toASCII(mVolumeName).get());

  // Add ourselves as an Observer. It's important that we use a weak
  // reference here. If we used a strong reference, then that reference
  // would prevent this object from being destructed.
  nsCOMPtr<nsIObserverService> obs = GetObserverService();
  obs->AddObserver(this, NS_VOLUME_STATE_CHANGED, true /*weak*/);

  // Request the sdcard info, so we know the state/generation without having
  // to wait for a state change.
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    ContentChild::GetSingleton()->SendBroadcastVolume(mVolumeName);
    return NS_OK;
  }
  nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
  NS_ENSURE_TRUE(vs, NS_ERROR_FAILURE);

  vs->BroadcastVolume(mVolumeName);

  return NS_OK;
}

/* void unlock (); */
NS_IMETHODIMP nsVolumeMountLock::Unlock()
{
  LOG("nsVolumeMountLock released for '%s'",
      NS_LossyConvertUTF16toASCII(mVolumeName).get());

  mUnlocked = true;
  mWakeLock = nullptr;

  // While we don't really need to remove weak observers, we do so anyways
  // since it will reduce the number of times Observe gets called.
  nsCOMPtr<nsIObserverService> obs = GetObserverService();
  obs->RemoveObserver(this, NS_VOLUME_STATE_CHANGED);
  return NS_OK;
}

NS_IMETHODIMP nsVolumeMountLock::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
  if (strcmp(aTopic, NS_VOLUME_STATE_CHANGED) != 0) {
    return NS_OK;
  }
  if (mUnlocked) {
    // We're not locked anymore, so we don't need to look at the notifications.
    return NS_OK;
  }

  nsCOMPtr<nsIVolume> vol = do_QueryInterface(aSubject);
  if (!vol) {
    return NS_OK;
  }
  nsString volName;
  vol->GetName(volName);
  if (!volName.Equals(mVolumeName)) {
    return NS_OK;
  }
  int32_t state;
  nsresult rv = vol->GetState(&state);
  NS_ENSURE_SUCCESS(rv, rv);

  if (state != nsIVolume::STATE_MOUNTED) {
    mWakeLock = nullptr;
    mVolumeGeneration = -1;
    return NS_OK;
  }

  int32_t   mountGeneration;
  rv = vol->GetMountGeneration(&mountGeneration);
  NS_ENSURE_SUCCESS(rv, rv);

  DBG("nsVolumeMountLock::Observe mountGeneration = %d mVolumeGeneration = %d",
       mountGeneration, mVolumeGeneration);

  if (mVolumeGeneration == mountGeneration) {
    return NS_OK;
  }

  // The generation changed, which means that any wakelock we may have
  // been holding is now invalid. Grab a new wakelock for the new generation
  // number.

  mWakeLock = nullptr;
  mVolumeGeneration = mountGeneration;

  nsRefPtr<power::PowerManagerService> pmService =
    power::PowerManagerService::GetInstance();
  NS_ENSURE_TRUE(pmService, NS_ERROR_FAILURE);

  nsString mountLockName;
  vol->GetMountLockName(mountLockName);

  ErrorResult err;
  mWakeLock = pmService->NewWakeLock(mountLockName, nullptr, err);
  if (err.Failed()) {
    return err.ErrorCode();
  }

  LOG("nsVolumeMountLock acquired for '%s' gen %d",
      NS_LossyConvertUTF16toASCII(mVolumeName).get(), mVolumeGeneration);
  return NS_OK;
}

} // namespace system
} // namespace mozilla
