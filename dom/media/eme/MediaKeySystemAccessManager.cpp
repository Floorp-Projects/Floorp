/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaKeySystemAccessManager.h"
#include "mozilla/Preferences.h"
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

MediaKeySystemAccessManager::MediaKeySystemAccessManager(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
  , mAddedObservers(false)
#ifdef XP_WIN
  , mTrialCreator(new GMPVideoDecoderTrialCreator())
#endif
{
}

MediaKeySystemAccessManager::~MediaKeySystemAccessManager()
{
  Shutdown();
}

void
MediaKeySystemAccessManager::Request(DetailedPromise* aPromise,
                                     const nsAString& aKeySystem,
                                     const Optional<Sequence<MediaKeySystemOptions>>& aOptions)
{
  if (aKeySystem.IsEmpty() || (aOptions.WasPassed() && aOptions.Value().IsEmpty())) {
    aPromise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR,
                          NS_LITERAL_CSTRING("Invalid keysystem type or invalid options sequence"));
    return;
  }
  Sequence<MediaKeySystemOptions> optionsNotPassed;
  const auto& options = aOptions.WasPassed() ? aOptions.Value() : optionsNotPassed;
  Request(aPromise, aKeySystem, options, RequestType::Initial);
}

static bool
ShouldTrialCreateGMP(const nsAString& aKeySystem)
{
  // Trial create where the CDM has a decoder;
  // * ClearKey and Primetime on Windows Vista and later.
  // * Primetime on MacOSX Lion and later.
  return
#ifdef XP_WIN
    IsVistaOrLater();
#elif defined(XP_MACOSX)
    aKeySystem.EqualsLiteral("com.adobe.primetime") &&
    nsCocoaFeatures::OnLionOrLater();
#else
    false;
#endif
}

void
MediaKeySystemAccessManager::Request(DetailedPromise* aPromise,
                                     const nsAString& aKeySystem,
                                     const Sequence<MediaKeySystemOptions>& aOptions,
                                     RequestType aType)
{
  EME_LOG("MediaKeySystemAccessManager::Request %s", NS_ConvertUTF16toUTF8(aKeySystem).get());
  if (!Preferences::GetBool("media.eme.enabled", false)) {
    // EME disabled by user, send notification to chrome so UI can
    // inform user.
    MediaKeySystemAccess::NotifyObservers(mWindow,
                                          aKeySystem,
                                          MediaKeySystemStatus::Api_disabled);
    aPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                          NS_LITERAL_CSTRING("EME has been preffed off"));
    return;
  }

  // Parse keysystem, split it out into keySystem prefix, and version suffix.
  nsAutoString keySystem;
  int32_t minCdmVersion = NO_CDM_VERSION;
  if (!ParseKeySystem(aKeySystem,
                      keySystem,
                      minCdmVersion)) {
    // Invalid keySystem string, or unsupported keySystem. Send notification
    // to chrome to show a failure notice.
    MediaKeySystemAccess::NotifyObservers(mWindow, aKeySystem, MediaKeySystemStatus::Cdm_not_supported);
    aPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                          NS_LITERAL_CSTRING("Key system string is invalid, or key system is unsupported"));
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
      keySystem.EqualsLiteral("com.adobe.primetime")) {
    // These are cases which could be resolved by downloading a new(er) CDM.
    // When we send the status to chrome, chrome's GMPProvider will attempt to
    // download or update the CDM. In AwaitInstall() we add listeners to wait
    // for the update to complete, and we'll call this function again with
    // aType==Subsequent once the download has completed and the GMPService
    // has had a new plugin added. AwaitInstall() sets a timer to fail if the
    // update/download takes too long or fails.
    if (aType == RequestType::Initial &&
        AwaitInstall(aPromise, aKeySystem, aOptions)) {
      // Notify chrome that we're going to wait for the CDM to download/update.
      // Note: If we're re-trying, we don't re-send the notificaiton,
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
    return;
  }

  if (aOptions.IsEmpty() ||
      MediaKeySystemAccess::IsSupported(keySystem, aOptions)) {
    nsRefPtr<MediaKeySystemAccess> access(
      new MediaKeySystemAccess(mWindow, keySystem, NS_ConvertUTF8toUTF16(cdmVersion)));
   if (ShouldTrialCreateGMP(keySystem)) {
      // Ensure we have tried creating a GMPVideoDecoder for this
      // keySystem, and that we can use it to decode. This ensures that we only
      // report that we support this keySystem when the CDM us usable (i.e.
      // report that we support this keySystem when the CDM us usable.
      mTrialCreator->MaybeAwaitTrialCreate(keySystem, access, aPromise, mWindow);
      return;
    }
    aPromise->MaybeResolve(access);
    return;
  }

  aPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                        NS_LITERAL_CSTRING("Key system is not supported"));
}

MediaKeySystemAccessManager::PendingRequest::PendingRequest(DetailedPromise* aPromise,
                                                            const nsAString& aKeySystem,
                                                            const Sequence<MediaKeySystemOptions>& aOptions,
                                                            nsITimer* aTimer)
  : mPromise(aPromise)
  , mKeySystem(aKeySystem)
  , mOptions(aOptions)
  , mTimer(aTimer)
{
  MOZ_COUNT_CTOR(MediaKeySystemAccessManager::PendingRequest);
}

MediaKeySystemAccessManager::PendingRequest::PendingRequest(const PendingRequest& aOther)
  : mPromise(aOther.mPromise)
  , mKeySystem(aOther.mKeySystem)
  , mOptions(aOther.mOptions)
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
                                          const Sequence<MediaKeySystemOptions>& aOptions)
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

  mRequests.AppendElement(PendingRequest(aPromise, aKeySystem, aOptions, timer));
  return true;
}

void
MediaKeySystemAccessManager::RetryRequest(PendingRequest& aRequest)
{
  aRequest.CancelTimer();
  Request(aRequest.mPromise, aRequest.mKeySystem, aRequest.mOptions, RequestType::Subsequent);
}

nsresult
MediaKeySystemAccessManager::Observe(nsISupports* aSubject,
                                     const char* aTopic,
                                     const char16_t* aData)
{
  EME_LOG("MediaKeySystemAccessManager::Observe %s", aTopic);

  if (!strcmp(aTopic, "gmp-path-added")) {
    nsTArray<PendingRequest> requests(Move(mRequests));
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
  mAddedObservers = NS_SUCCEEDED(obsService->AddObserver(this, "gmp-path-added", false));
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
      obsService->RemoveObserver(this, "gmp-path-added");
      mAddedObservers = false;
    }
  }
}

} // namespace dom
} // namespace mozilla
