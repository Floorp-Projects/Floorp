/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaKeySystemAccessManager.h"
#include "DecoderDoctorDiagnostics.h"
#include "MediaPrefs.h"
#include "mozilla/EMEUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/DetailedPromise.h"
#ifdef XP_WIN
#include "mozilla/WindowsVersion.h"
#endif
#ifdef XP_MACOSX
#include "nsCocoaFeatures.h"
#endif
#include "nsPrintfCString.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaKeySystemAccessManager)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaKeySystemAccessManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaKeySystemAccessManager)

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaKeySystemAccessManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaKeySystemAccessManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  for (size_t i = 0; i < tmp->mRequests.Length(); i++) {
    tmp->mRequests[i].RejectPromise(NS_LITERAL_CSTRING("Promise still outstanding at MediaKeySystemAccessManager GC"));
    tmp->mRequests[i].CancelTimer();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mRequests[i].mPromise)
  }
  tmp->mRequests.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MediaKeySystemAccessManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  for (size_t i = 0; i < tmp->mRequests.Length(); i++) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRequests[i].mPromise)
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

MediaKeySystemAccessManager::MediaKeySystemAccessManager(nsPIDOMWindowInner* aWindow)
  : mWindow(aWindow)
  , mAddedObservers(false)
{
}

MediaKeySystemAccessManager::~MediaKeySystemAccessManager()
{
  Shutdown();
}

void
MediaKeySystemAccessManager::Request(DetailedPromise* aPromise,
                                     const nsAString& aKeySystem,
                                     const Sequence<MediaKeySystemConfiguration>& aConfigs)
{
  if (aKeySystem.IsEmpty() || aConfigs.IsEmpty()) {
    aPromise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR,
                          NS_LITERAL_CSTRING("Invalid keysystem type or invalid options sequence"));
    return;
  }
  Request(aPromise, aKeySystem, aConfigs, RequestType::Initial);
}

void
MediaKeySystemAccessManager::Request(DetailedPromise* aPromise,
                                     const nsAString& aKeySystem,
                                     const Sequence<MediaKeySystemConfiguration>& aConfigs,
                                     RequestType aType)
{
  EME_LOG("MediaKeySystemAccessManager::Request %s", NS_ConvertUTF16toUTF8(aKeySystem).get());

  if (aKeySystem.IsEmpty()) {
    aPromise->MaybeReject(NS_ERROR_DOM_TYPE_ERR,
                          NS_LITERAL_CSTRING("Key system string is empty"));
    // Don't notify DecoderDoctor, as there's nothing we or the user can
    // do to fix this situation; the site is using the API wrong.
    return;
  }
  if (aConfigs.IsEmpty()) {
    aPromise->MaybeReject(NS_ERROR_DOM_TYPE_ERR,
                          NS_LITERAL_CSTRING("Candidate MediaKeySystemConfigs is empty"));
    // Don't notify DecoderDoctor, as there's nothing we or the user can
    // do to fix this situation; the site is using the API wrong.
    return;
  }

  DecoderDoctorDiagnostics diagnostics;

  // Parse keysystem, split it out into keySystem prefix, and version suffix.
  nsAutoString keySystem;
  int32_t minCdmVersion = NO_CDM_VERSION;
  if (!ParseKeySystem(aKeySystem, keySystem, minCdmVersion)) {
    // Not to inform user, because nothing to do if the keySystem is not
    // supported.
    aPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                          NS_LITERAL_CSTRING("Key system string is invalid,"
                                             " or key system is unsupported"));
    diagnostics.StoreMediaKeySystemAccess(mWindow->GetExtantDoc(),
                                          aKeySystem, false, __func__);
    return;
  }

  if (!MediaPrefs::EMEEnabled() && !IsClearkeyKeySystem(aKeySystem)) {
    // EME disabled by user, send notification to chrome so UI can inform user.
    // Clearkey is allowed even when EME is disabled because we want the pref
    // "media.eme.enabled" only taking effect on proprietary DRMs.
    MediaKeySystemAccess::NotifyObservers(mWindow,
                                          aKeySystem,
                                          MediaKeySystemStatus::Api_disabled);
    aPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                          NS_LITERAL_CSTRING("EME has been preffed off"));
    diagnostics.StoreMediaKeySystemAccess(mWindow->GetExtantDoc(),
                                          aKeySystem, false, __func__);
    return;
  }

  nsAutoCString message;
  nsAutoCString cdmVersion;
  MediaKeySystemStatus status =
    MediaKeySystemAccess::GetKeySystemStatus(keySystem, minCdmVersion, message, cdmVersion);

  nsPrintfCString msg("MediaKeySystemAccess::GetKeySystemStatus(%s, minVer=%d) "
                      "result=%s version='%s' msg='%s'",
                      NS_ConvertUTF16toUTF8(keySystem).get(),
                      minCdmVersion,
                      MediaKeySystemStatusValues::strings[(size_t)status].value,
                      cdmVersion.get(),
                      message.get());
  LogToBrowserConsole(NS_ConvertUTF8toUTF16(msg));

  if ((status == MediaKeySystemStatus::Cdm_not_installed ||
       status == MediaKeySystemStatus::Cdm_insufficient_version) &&
      (IsPrimetimeKeySystem(keySystem) || IsWidevineKeySystem(keySystem))) {
    // These are cases which could be resolved by downloading a new(er) CDM.
    // When we send the status to chrome, chrome's GMPProvider will attempt to
    // download or update the CDM. In AwaitInstall() we add listeners to wait
    // for the update to complete, and we'll call this function again with
    // aType==Subsequent once the download has completed and the GMPService
    // has had a new plugin added. AwaitInstall() sets a timer to fail if the
    // update/download takes too long or fails.
    if (aType == RequestType::Initial &&
        AwaitInstall(aPromise, aKeySystem, aConfigs)) {
      // Notify chrome that we're going to wait for the CDM to download/update.
      // Note: If we're re-trying, we don't re-send the notification,
      // as chrome is already displaying the "we can't play, updating"
      // notification.
      MediaKeySystemAccess::NotifyObservers(mWindow, keySystem, status);
    } else {
      // We waited or can't wait for an update and we still can't service
      // the request. Give up. Chrome will still be showing a "I can't play,
      // updating" notification.
      aPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                            NS_LITERAL_CSTRING("Gave up while waiting for a CDM update"));
    }
    diagnostics.StoreMediaKeySystemAccess(mWindow->GetExtantDoc(),
                                          aKeySystem, false, __func__);
    return;
  }
  if (status != MediaKeySystemStatus::Available) {
    if (status != MediaKeySystemStatus::Error) {
      // Failed due to user disabling something, send a notification to
      // chrome, so we can show some UI to explain how the user can rectify
      // the situation.
      MediaKeySystemAccess::NotifyObservers(mWindow, keySystem, status);
      aPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR, message);
      return;
    }
    aPromise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                          NS_LITERAL_CSTRING("GetKeySystemAccess failed"));
    diagnostics.StoreMediaKeySystemAccess(mWindow->GetExtantDoc(),
                                          aKeySystem, false, __func__);
    return;
  }

  MediaKeySystemConfiguration config;
  if (MediaKeySystemAccess::GetSupportedConfig(keySystem, aConfigs, config, &diagnostics)) {
    RefPtr<MediaKeySystemAccess> access(
      new MediaKeySystemAccess(mWindow, keySystem, NS_ConvertUTF8toUTF16(cdmVersion), config));
    aPromise->MaybeResolve(access);
    diagnostics.StoreMediaKeySystemAccess(mWindow->GetExtantDoc(),
                                          aKeySystem, true, __func__);
    return;
  }
  // Not to inform user, because nothing to do if the corresponding keySystem
  // configuration is not supported.
  aPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                        NS_LITERAL_CSTRING("Key system configuration is not supported"));
  diagnostics.StoreMediaKeySystemAccess(mWindow->GetExtantDoc(),
                                        aKeySystem, false, __func__);
}

MediaKeySystemAccessManager::PendingRequest::PendingRequest(DetailedPromise* aPromise,
                                                            const nsAString& aKeySystem,
                                                            const Sequence<MediaKeySystemConfiguration>& aConfigs,
                                                            nsITimer* aTimer)
  : mPromise(aPromise)
  , mKeySystem(aKeySystem)
  , mConfigs(aConfigs)
  , mTimer(aTimer)
{
  MOZ_COUNT_CTOR(MediaKeySystemAccessManager::PendingRequest);
}

MediaKeySystemAccessManager::PendingRequest::PendingRequest(const PendingRequest& aOther)
  : mPromise(aOther.mPromise)
  , mKeySystem(aOther.mKeySystem)
  , mConfigs(aOther.mConfigs)
  , mTimer(aOther.mTimer)
{
  MOZ_COUNT_CTOR(MediaKeySystemAccessManager::PendingRequest);
}

MediaKeySystemAccessManager::PendingRequest::~PendingRequest()
{
  MOZ_COUNT_DTOR(MediaKeySystemAccessManager::PendingRequest);
}

void
MediaKeySystemAccessManager::PendingRequest::CancelTimer()
{
  if (mTimer) {
    mTimer->Cancel();
  }
}

void
MediaKeySystemAccessManager::PendingRequest::RejectPromise(const nsCString& aReason)
{
  if (mPromise) {
    mPromise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR, aReason);
  }
}

bool
MediaKeySystemAccessManager::AwaitInstall(DetailedPromise* aPromise,
                                          const nsAString& aKeySystem,
                                          const Sequence<MediaKeySystemConfiguration>& aConfigs)
{
  EME_LOG("MediaKeySystemAccessManager::AwaitInstall %s", NS_ConvertUTF16toUTF8(aKeySystem).get());

  if (!EnsureObserversAdded()) {
    NS_WARNING("Failed to add pref observer");
    return false;
  }

  nsCOMPtr<nsITimer> timer(do_CreateInstance("@mozilla.org/timer;1"));
  if (!timer || NS_FAILED(timer->Init(this, 60 * 1000, nsITimer::TYPE_ONE_SHOT))) {
    NS_WARNING("Failed to create timer to await CDM install.");
    return false;
  }

  mRequests.AppendElement(PendingRequest(aPromise, aKeySystem, aConfigs, timer));
  return true;
}

void
MediaKeySystemAccessManager::RetryRequest(PendingRequest& aRequest)
{
  aRequest.CancelTimer();
  Request(aRequest.mPromise, aRequest.mKeySystem, aRequest.mConfigs, RequestType::Subsequent);
}

nsresult
MediaKeySystemAccessManager::Observe(nsISupports* aSubject,
                                     const char* aTopic,
                                     const char16_t* aData)
{
  EME_LOG("MediaKeySystemAccessManager::Observe %s", aTopic);

  if (!strcmp(aTopic, "gmp-changed")) {
    // Filter out the requests where the CDM's install-status is no longer
    // "unavailable". This will be the CDMs which have downloaded since the
    // initial request.
    // Note: We don't have a way to communicate from chrome that the CDM has
    // failed to download, so we'll just let the timeout fail us in that case.
    nsTArray<PendingRequest> requests;
    for (size_t i = mRequests.Length(); i > 0; i--) {
      const size_t index = i - i;
      PendingRequest& request = mRequests[index];
      nsAutoCString message;
      nsAutoCString cdmVersion;
      MediaKeySystemStatus status =
        MediaKeySystemAccess::GetKeySystemStatus(request.mKeySystem,
                                                 NO_CDM_VERSION,
                                                 message,
                                                 cdmVersion);
      if (status == MediaKeySystemStatus::Cdm_not_installed) {
        // Not yet installed, don't retry. Keep waiting until timeout.
        continue;
      }
      // Status has changed, retry request.
      requests.AppendElement(Move(request));
      mRequests.RemoveElementAt(index);
    }
    // Retry all pending requests, but this time fail if the CDM is not installed.
    for (PendingRequest& request : requests) {
      RetryRequest(request);
    }
  } else if (!strcmp(aTopic, "timer-callback")) {
    // Find the timer that expired and re-run the request for it.
    nsCOMPtr<nsITimer> timer(do_QueryInterface(aSubject));
    for (size_t i = 0; i < mRequests.Length(); i++) {
      if (mRequests[i].mTimer == timer) {
        EME_LOG("MediaKeySystemAccessManager::AwaitInstall resuming request");
        PendingRequest request = mRequests[i];
        mRequests.RemoveElementAt(i);
        RetryRequest(request);
        break;
      }
    }
  }
  return NS_OK;
}

bool
MediaKeySystemAccessManager::EnsureObserversAdded()
{
  if (mAddedObservers) {
    return true;
  }

  nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obsService)) {
    return false;
  }
  mAddedObservers = NS_SUCCEEDED(obsService->AddObserver(this, "gmp-changed", false));
  return mAddedObservers;
}

void
MediaKeySystemAccessManager::Shutdown()
{
  EME_LOG("MediaKeySystemAccessManager::Shutdown");
  nsTArray<PendingRequest> requests(Move(mRequests));
  for (PendingRequest& request : requests) {
    // Cancel all requests; we're shutting down.
    request.CancelTimer();
    request.RejectPromise(NS_LITERAL_CSTRING("Promise still outstanding at MediaKeySystemAccessManager shutdown"));
  }
  if (mAddedObservers) {
    nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
    if (obsService) {
      obsService->RemoveObserver(this, "gmp-changed");
      mAddedObservers = false;
    }
  }
}

} // namespace dom
} // namespace mozilla
