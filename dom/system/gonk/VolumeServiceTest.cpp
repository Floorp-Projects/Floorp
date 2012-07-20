/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VolumeServiceTest.h"

#include "base/message_loop.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsIVolume.h"
#include "nsIVolumeService.h"
#include "nsIVolumeStat.h"
#include "nsXULAppAPI.h"

#include "mozilla/Services.h"

#define VOLUME_MANAGER_LOG_TAG  "VolumeServiceTest"
#include "VolumeManagerLog.h"

using namespace mozilla::services;

namespace mozilla {
namespace system {

#define TEST_NSVOLUME_OBSERVER  0

#if TEST_NSVOLUME_OBSERVER

/***************************************************************************
* A test class to verify that the Observer stuff is working properly.
*/
class VolumeTestObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  VolumeTestObserver()
  {
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    if (!obs) {
      return;
    }
    obs->AddObserver(this, NS_VOLUME_STATE_CHANGED, false);
  }
  ~VolumeTestObserver()
  {
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    if (!obs) {
      return;
    }
    obs->RemoveObserver(this, NS_VOLUME_STATE_CHANGED);
  }

  void LogVolume(nsIVolume *vol)
  {
    nsString volName;
    nsString mountPoint;
    PRInt32 volState;

    vol->GetName(volName);
    vol->GetMountPoint(mountPoint);
    vol->GetState(&volState);

    LOG("  Volume: %s MountPoint: %s State: %s",
        NS_LossyConvertUTF16toASCII(volName).get(),
        NS_LossyConvertUTF16toASCII(mountPoint).get(),
        NS_VolumeStateStr(volState));

    nsCOMPtr<nsIVolumeStat> stat;
    nsresult rv = vol->GetStats(getter_AddRefs(stat));
    if (NS_SUCCEEDED(rv)) {
      PRInt64 totalBytes;
      PRInt64 freeBytes;

      stat->GetTotalBytes(&totalBytes);
      stat->GetFreeBytes(&freeBytes);

      LOG("  Total Space: %llu Mb  Free Bytes: %llu Mb",
          totalBytes / (1024LL * 1024LL), freeBytes / (1024LL * 1024LL));
    }
    else {
      LOG("  Unable to retrieve stats");
    }
  }
};
static nsCOMPtr<VolumeTestObserver>  sTestObserver;

NS_IMPL_ISUPPORTS1(VolumeTestObserver, nsIObserver)

NS_IMETHODIMP
VolumeTestObserver::Observe(nsISupports *aSubject,
                            const char *aTopic,
                            const PRUnichar *aData)
{
  LOG("TestObserver: topic: %s", aTopic);

  if (strcmp(aTopic, NS_VOLUME_STATE_CHANGED) != 0) {
    return NS_OK;
  }
  nsCOMPtr<nsIVolume> vol = do_QueryInterface(aSubject);
  if (vol) {
    LogVolume(vol);
  }

  // Since this observe method was called then we know that the service
  // has been initialized so we can do the VolumeService tests.

  nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
  if (!vs) {
    ERR("do_GetService('%s') failed", NS_VOLUMESERVICE_CONTRACTID);
    return NS_ERROR_FAILURE;
  }

  nsresult rv = vs->GetVolumeByName(NS_LITERAL_STRING("sdcard"), getter_AddRefs(vol));
  if (NS_SUCCEEDED(rv)) {
    LOG("GetVolumeByName( 'sdcard' ) succeeded (expected)");
    LogVolume(vol);
  } else {
    ERR("GetVolumeByName( 'sdcard' ) failed (unexpected)");
  }

  rv = vs->GetVolumeByName(NS_LITERAL_STRING("foo"), getter_AddRefs(vol));
  if (NS_SUCCEEDED(rv)) {
    ERR("GetVolumeByName( 'foo' ) succeeded (unexpected)");
  } else {
    LOG("GetVolumeByName( 'foo' ) failed (expected)");
  }

  rv = vs->GetVolumeByPath(NS_LITERAL_STRING("/mnt/sdcard"), getter_AddRefs(vol));
  if (NS_SUCCEEDED(rv)) {
    LOG("GetVolumeByPath( '/mnt/sdcard' ) succeeded (expected)");
    LogVolume(vol);
  } else {
    ERR("GetVolumeByPath( '/mnt/sdcard' ) failed (unexpected");
  }

  rv = vs->GetVolumeByPath(NS_LITERAL_STRING("/mnt/sdcard/foo"), getter_AddRefs(vol));
  if (NS_SUCCEEDED(rv)) {
    LOG("GetVolumeByPath( '/mnt/sdcard/foo' ) succeeded (expected)");
    LogVolume(vol);
  } else {
    LOG("GetVolumeByPath( '/mnt/sdcard/foo' ) failed (unexpected)");
  }

  rv = vs->GetVolumeByPath(NS_LITERAL_STRING("/mnt/sdcardfoo"), getter_AddRefs(vol));
  if (NS_SUCCEEDED(rv)) {
    ERR("GetVolumeByPath( '/mnt/sdcardfoo' ) succeeded (unexpected)");
  } else {
    LOG("GetVolumeByPath( '/mnt/sdcardfoo' ) failed (expected)");
  }

  return NS_OK;
}

class InitVolumeServiceTestIO : public nsRunnable
{
public:
  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    DBG("InitVolumeServiceTest called");
    nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
    if (!vs) {
      ERR("do_GetService('%s') failed", NS_VOLUMESERVICE_CONTRACTID);
      return NS_ERROR_FAILURE;
    }
    sTestObserver = new VolumeTestObserver();

    return NS_OK;
  }
};
#endif // TEST_NSVOLUME_OBSERVER

void
InitVolumeServiceTestIOThread()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

#if TEST_NSVOLUME_OBSERVER
  // Now that the volume manager is initialized we can go
  // ahead and do our test (on main thread).
  NS_DispatchToMainThread(new InitVolumeServiceTestIO());
#endif
}

void
ShutdownVolumeServiceTest()
{
#if TEST_NSVOLUME_OBSERVER
  DBG("ShutdownVolumeServiceTestIOThread called");
  sTestObserver = NULL;
#endif
}

} // system
} // mozilla
