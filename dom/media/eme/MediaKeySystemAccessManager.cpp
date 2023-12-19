/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaKeySystemAccessManager.h"

#include "DecoderDoctorDiagnostics.h"
#include "MediaKeySystemAccessPermissionRequest.h"
#include "VideoUtils.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/KeySystemNames.h"
#include "mozilla/DetailedPromise.h"
#include "mozilla/EMEUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/Unused.h"
#ifdef XP_WIN
#  include "mozilla/WindowsVersion.h"
#endif
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsTHashMap.h"
#include "nsIObserverService.h"
#include "nsIScriptError.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"

namespace mozilla::dom {

MediaKeySystemAccessManager::PendingRequest::PendingRequest(
    DetailedPromise* aPromise, const nsAString& aKeySystem,
    const Sequence<MediaKeySystemConfiguration>& aConfigs)
    : MediaKeySystemAccessRequest(aKeySystem, aConfigs), mPromise(aPromise) {
  MOZ_COUNT_CTOR(MediaKeySystemAccessManager::PendingRequest);
}

MediaKeySystemAccessManager::PendingRequest::~PendingRequest() {
  MOZ_COUNT_DTOR(MediaKeySystemAccessManager::PendingRequest);
}

void MediaKeySystemAccessManager::PendingRequest::CancelTimer() {
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

void MediaKeySystemAccessManager::PendingRequest::
    RejectPromiseWithInvalidAccessError(const nsACString& aReason) {
  if (mPromise) {
    mPromise->MaybeRejectWithInvalidAccessError(aReason);
  }
}

void MediaKeySystemAccessManager::PendingRequest::
    RejectPromiseWithNotSupportedError(const nsACString& aReason) {
  if (mPromise) {
    mPromise->MaybeRejectWithNotSupportedError(aReason);
  }
}

void MediaKeySystemAccessManager::PendingRequest::RejectPromiseWithTypeError(
    const nsACString& aReason) {
  if (mPromise) {
    mPromise->MaybeRejectWithTypeError(aReason);
  }
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaKeySystemAccessManager)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaKeySystemAccessManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaKeySystemAccessManager)

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaKeySystemAccessManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaKeySystemAccessManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  for (size_t i = 0; i < tmp->mPendingInstallRequests.Length(); i++) {
    tmp->mPendingInstallRequests[i]->CancelTimer();
    tmp->mPendingInstallRequests[i]->RejectPromiseWithInvalidAccessError(
        nsLiteralCString(
            "Promise still outstanding at MediaKeySystemAccessManager GC"));
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingInstallRequests[i]->mPromise)
  }
  tmp->mPendingInstallRequests.Clear();
  for (size_t i = 0; i < tmp->mPendingAppApprovalRequests.Length(); i++) {
    tmp->mPendingAppApprovalRequests[i]->RejectPromiseWithInvalidAccessError(
        nsLiteralCString(
            "Promise still outstanding at MediaKeySystemAccessManager GC"));
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingAppApprovalRequests[i]->mPromise)
  }
  tmp->mPendingAppApprovalRequests.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MediaKeySystemAccessManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  for (size_t i = 0; i < tmp->mPendingInstallRequests.Length(); i++) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingInstallRequests[i]->mPromise)
  }
  for (size_t i = 0; i < tmp->mPendingAppApprovalRequests.Length(); i++) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingAppApprovalRequests[i]->mPromise)
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define MKSAM_LOG_DEBUG(msg, ...) \
  EME_LOG("MediaKeySystemAccessManager::%s " msg, __func__, ##__VA_ARGS__)

MediaKeySystemAccessManager::MediaKeySystemAccessManager(
    nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
}

MediaKeySystemAccessManager::~MediaKeySystemAccessManager() {
  MOZ_ASSERT(NS_IsMainThread());
  Shutdown();
}

void MediaKeySystemAccessManager::Request(
    DetailedPromise* aPromise, const nsAString& aKeySystem,
    const Sequence<MediaKeySystemConfiguration>& aConfigs) {
  MOZ_ASSERT(NS_IsMainThread());
  CheckDoesWindowSupportProtectedMedia(
      MakeUnique<PendingRequest>(aPromise, aKeySystem, aConfigs));
}

void MediaKeySystemAccessManager::CheckDoesWindowSupportProtectedMedia(
    UniquePtr<PendingRequest> aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);
  MKSAM_LOG_DEBUG("aRequest->mKeySystem=%s",
                  NS_ConvertUTF16toUTF8(aRequest->mKeySystem).get());

  // In Windows OS, some Firefox windows that host content cannot support
  // protected content, so check the status of support for this window.
  // On other platforms windows should always support protected media.
#ifdef XP_WIN
  RefPtr<BrowserChild> browser(BrowserChild::GetFrom(mWindow));
  if (!browser) {
    if (!XRE_IsParentProcess() || XRE_IsE10sParentProcess()) {
      // In this case, there is no browser because the Navigator object has
      // been disconnected from its window. Thus, reject the promise.
      aRequest->mPromise->MaybeRejectWithTypeError(
          "Browsing context is no longer available");
    } else {
      // In this case, there is no browser because e10s is off. Proceed with
      // the request with support since this scenario is always supported.
      MKSAM_LOG_DEBUG("Allowing protected media on Windows with e10s off.");

      OnDoesWindowSupportProtectedMedia(true, std::move(aRequest));
    }

    return;
  }

  RefPtr<MediaKeySystemAccessManager> self(this);

  MKSAM_LOG_DEBUG(
      "Checking with browser if this window supports protected media.");
  browser->DoesWindowSupportProtectedMedia()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self, request = std::move(aRequest)](
          const BrowserChild::IsWindowSupportingProtectedMediaPromise::
              ResolveOrRejectValue& value) mutable {
        if (value.IsResolve()) {
          self->OnDoesWindowSupportProtectedMedia(value.ResolveValue(),
                                                  std::move(request));
        } else {
          EME_LOG(
              "MediaKeySystemAccessManager::DoesWindowSupportProtectedMedia-"
              "ResolveOrRejectLambda Failed to make IPC call to "
              "IsWindowSupportingProtectedMedia: "
              "reason=%d",
              static_cast<int>(value.RejectValue()));
          // Treat as failure.
          self->OnDoesWindowSupportProtectedMedia(false, std::move(request));
        }
      });

#else
  // Non-Windows OS windows always support protected media.
  MKSAM_LOG_DEBUG(
      "Allowing protected media because all non-Windows OS windows support "
      "protected media.");
  OnDoesWindowSupportProtectedMedia(true, std::move(aRequest));
#endif
}

void MediaKeySystemAccessManager::OnDoesWindowSupportProtectedMedia(
    bool aIsSupportedInWindow, UniquePtr<PendingRequest> aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);
  MKSAM_LOG_DEBUG("aIsSupportedInWindow=%s aRequest->mKeySystem=%s",
                  aIsSupportedInWindow ? "true" : "false",
                  NS_ConvertUTF16toUTF8(aRequest->mKeySystem).get());

  if (!aIsSupportedInWindow) {
    aRequest->RejectPromiseWithNotSupportedError(
        "EME is not supported in this window"_ns);
    return;
  }

  RequestMediaKeySystemAccess(std::move(aRequest));
}

void MediaKeySystemAccessManager::CheckDoesAppAllowProtectedMedia(
    UniquePtr<PendingRequest> aRequest) {
  // At time of writing, only GeckoView is expected to leverage the need to
  // approve EME requests from the application. However, this functionality
  // can be tested on all platforms by manipulating the
  // media.eme.require-app-approval + test prefs associated with
  // MediaKeySystemPermissionRequest.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);
  MKSAM_LOG_DEBUG("aRequest->mKeySystem=%s",
                  NS_ConvertUTF16toUTF8(aRequest->mKeySystem).get());

  if (!StaticPrefs::media_eme_require_app_approval()) {
    MKSAM_LOG_DEBUG(
        "media.eme.require-app-approval is false, allowing request.");
    // We don't require app approval as the pref is not set. Treat as app
    // approving by passing true to the handler.
    OnDoesAppAllowProtectedMedia(true, std::move(aRequest));
    return;
  }

  if (mAppAllowsProtectedMediaPromiseRequest.Exists()) {
    // We already have a pending permission request, we don't need to fire
    // another. Just wait for the existing permission request to be handled
    // and the result from that will be used to handle the current request.
    MKSAM_LOG_DEBUG(
        "mAppAllowsProtectedMediaPromiseRequest already exists. aRequest "
        "addded to queue and will be handled when exising permission request "
        "is serviced.");
    mPendingAppApprovalRequests.AppendElement(std::move(aRequest));
    return;
  }

  RefPtr<MediaKeySystemAccessPermissionRequest> appApprovalRequest =
      MediaKeySystemAccessPermissionRequest::Create(mWindow);
  if (!appApprovalRequest) {
    MKSAM_LOG_DEBUG(
        "Failed to create app approval request! Blocking eme request as "
        "fallback.");
    aRequest->RejectPromiseWithInvalidAccessError(nsLiteralCString(
        "Failed to create approval request to send to app embedding Gecko."));
    return;
  }

  // If we're not using testing prefs (which take precedence over cached
  // values) and have a cached value, handle based on the cached value.
  if (appApprovalRequest->CheckPromptPrefs() ==
          MediaKeySystemAccessPermissionRequest::PromptResult::Pending &&
      mAppAllowsProtectedMedia) {
    MKSAM_LOG_DEBUG(
        "Short circuiting based on mAppAllowsProtectedMedia cached value");
    OnDoesAppAllowProtectedMedia(*mAppAllowsProtectedMedia,
                                 std::move(aRequest));
    return;
  }

  // Store the approval request, it will be handled when we get a response
  // from the app.
  mPendingAppApprovalRequests.AppendElement(std::move(aRequest));

  RefPtr<MediaKeySystemAccessPermissionRequest::RequestPromise> p =
      appApprovalRequest->GetPromise();
  p->Then(
       GetCurrentSerialEventTarget(), __func__,
       // Allow callback
       [this,
        self = RefPtr<MediaKeySystemAccessManager>(this)](bool aRequestResult) {
         MOZ_ASSERT(NS_IsMainThread());
         MOZ_ASSERT(aRequestResult, "Result should be true on allow callback!");
         mAppAllowsProtectedMediaPromiseRequest.Complete();
         // Cache result.
         mAppAllowsProtectedMedia = Some(aRequestResult);
         // For each pending request, handle it based on the app's response.
         for (UniquePtr<PendingRequest>& approvalRequest :
              mPendingAppApprovalRequests) {
           OnDoesAppAllowProtectedMedia(*mAppAllowsProtectedMedia,
                                        std::move(approvalRequest));
         }
         self->mPendingAppApprovalRequests.Clear();
       },
       // Cancel callback
       [this,
        self = RefPtr<MediaKeySystemAccessManager>(this)](bool aRequestResult) {
         MOZ_ASSERT(NS_IsMainThread());
         MOZ_ASSERT(!aRequestResult,
                    "Result should be false on cancel callback!");
         mAppAllowsProtectedMediaPromiseRequest.Complete();
         // Cache result.
         mAppAllowsProtectedMedia = Some(aRequestResult);
         // For each pending request, handle it based on the app's response.
         for (UniquePtr<PendingRequest>& approvalRequest :
              mPendingAppApprovalRequests) {
           OnDoesAppAllowProtectedMedia(*mAppAllowsProtectedMedia,
                                        std::move(approvalRequest));
         }
         self->mPendingAppApprovalRequests.Clear();
       })
      ->Track(mAppAllowsProtectedMediaPromiseRequest);

  // Prefs not causing short circuit, no cached value, go ahead and request
  // permission.
  MKSAM_LOG_DEBUG("Dispatching async request for app approval");
  if (NS_FAILED(appApprovalRequest->Start())) {
    // This shouldn't happen unless we're shutting down or similar edge cases.
    // If this is regularly being hit then something is wrong and should be
    // investigated.
    MKSAM_LOG_DEBUG(
        "Failed to start app approval request! Eme approval will be left in "
        "limbo!");
  }
}

void MediaKeySystemAccessManager::OnDoesAppAllowProtectedMedia(
    bool aIsAllowed, UniquePtr<PendingRequest> aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);
  MKSAM_LOG_DEBUG("aIsAllowed=%s aRequest->mKeySystem=%s",
                  aIsAllowed ? "true" : "false",
                  NS_ConvertUTF16toUTF8(aRequest->mKeySystem).get());
  if (!aIsAllowed) {
    aRequest->RejectPromiseWithNotSupportedError(
        nsLiteralCString("The application embedding this user agent has "
                         "blocked MediaKeySystemAccess"));
    return;
  }

  ProvideAccess(std::move(aRequest));
}

void MediaKeySystemAccessManager::RequestMediaKeySystemAccess(
    UniquePtr<PendingRequest> aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);
  MKSAM_LOG_DEBUG("aIsSupportedInWindow=%s",
                  NS_ConvertUTF16toUTF8(aRequest->mKeySystem).get());

  // 1. If keySystem is the empty string, return a promise rejected with a newly
  // created TypeError.
  if (aRequest->mKeySystem.IsEmpty()) {
    aRequest->mPromise->MaybeRejectWithTypeError("Key system string is empty");
    // Don't notify DecoderDoctor, as there's nothing we or the user can
    // do to fix this situation; the site is using the API wrong.
    return;
  }
  // 2. If supportedConfigurations is empty, return a promise rejected with a
  // newly created TypeError.
  if (aRequest->mConfigs.IsEmpty()) {
    aRequest->mPromise->MaybeRejectWithTypeError(
        "Candidate MediaKeySystemConfigs is empty");
    // Don't notify DecoderDoctor, as there's nothing we or the user can
    // do to fix this situation; the site is using the API wrong.
    return;
  }

  // 3. Let document be the calling context's Document.
  // 4. Let origin be the origin of document.
  // 5. Let promise be a new promise.
  // 6. Run the following steps in parallel:

  DecoderDoctorDiagnostics diagnostics;

  //   1. If keySystem is not one of the Key Systems supported by the user
  //   agent, reject promise with a NotSupportedError. String comparison is
  //   case-sensitive.
  if (!IsWidevineKeySystem(aRequest->mKeySystem) &&
#ifdef MOZ_WMF_CDM
      !IsPlayReadyKeySystemAndSupported(aRequest->mKeySystem) &&
      !IsWidevineExperimentKeySystemAndSupported(aRequest->mKeySystem) &&
#endif
      !IsClearkeyKeySystem(aRequest->mKeySystem)) {
    // Not to inform user, because nothing to do if the keySystem is not
    // supported.
    aRequest->RejectPromiseWithNotSupportedError(
        "Key system is unsupported"_ns);
    diagnostics.StoreMediaKeySystemAccess(
        mWindow->GetExtantDoc(), aRequest->mKeySystem, false, __func__);
    return;
  }

  if (!StaticPrefs::media_eme_enabled() &&
      !IsClearkeyKeySystem(aRequest->mKeySystem)) {
    // EME disabled by user, send notification to chrome so UI can inform user.
    // Clearkey is allowed even when EME is disabled because we want the pref
    // "media.eme.enabled" only taking effect on proprietary DRMs.
    // We don't show the notification if the pref is locked.
    if (!Preferences::IsLocked("media.eme.enabled")) {
      MediaKeySystemAccess::NotifyObservers(mWindow, aRequest->mKeySystem,
                                            MediaKeySystemStatus::Api_disabled);
    }
    aRequest->RejectPromiseWithNotSupportedError("EME has been preffed off"_ns);
    diagnostics.StoreMediaKeySystemAccess(
        mWindow->GetExtantDoc(), aRequest->mKeySystem, false, __func__);
    return;
  }

  nsAutoCString message;
  MediaKeySystemStatus status =
      MediaKeySystemAccess::GetKeySystemStatus(*aRequest, message);

  nsPrintfCString msg(
      "MediaKeySystemAccess::GetKeySystemStatus(%s) "
      "result=%s msg='%s'",
      NS_ConvertUTF16toUTF8(aRequest->mKeySystem).get(),
      nsCString(MediaKeySystemStatusValues::GetString(status)).get(),
      message.get());
  LogToBrowserConsole(NS_ConvertUTF8toUTF16(msg));
  EME_LOG("%s", msg.get());

  // We may need to install Widevine CDM to continue.
  if (status == MediaKeySystemStatus::Cdm_not_installed &&
      (IsWidevineKeySystem(aRequest->mKeySystem)
#ifdef MOZ_WMF_CDM
       || IsWidevineExperimentKeySystemAndSupported(aRequest->mKeySystem)
#endif
           )) {
    // These are cases which could be resolved by downloading a new(er) CDM.
    // When we send the status to chrome, chrome's GMPProvider will attempt to
    // download or update the CDM. In AwaitInstall() we add listeners to wait
    // for the update to complete, and we'll call this function again with
    // aType==Subsequent once the download has completed and the GMPService
    // has had a new plugin added. AwaitInstall() sets a timer to fail if the
    // update/download takes too long or fails.

    if (aRequest->mRequestType != PendingRequest::RequestType::Initial) {
      MOZ_ASSERT(aRequest->mRequestType ==
                 PendingRequest::RequestType::Subsequent);
      // CDM is not installed, but this is a subsequent request. We've waited,
      // but can't service this request! Give up. Chrome will still be showing a
      // "I can't play, updating" notification.
      aRequest->RejectPromiseWithNotSupportedError(
          "Timed out while waiting for a CDM update"_ns);
      diagnostics.StoreMediaKeySystemAccess(
          mWindow->GetExtantDoc(), aRequest->mKeySystem, false, __func__);
      return;
    }

    nsString keySystem = aRequest->mKeySystem;
#ifdef MOZ_WMF_CDM
    // If cdm-not-install is for HWDRM, that means we want to install Widevine
    // L1, which requires using hardware key system name for GMP to look up the
    // plugin.
    if (CheckIfHarewareDRMConfigExists(aRequest->mConfigs)) {
      keySystem = NS_ConvertUTF8toUTF16(kWidevineExperimentKeySystemName);
    }
#endif
    if (AwaitInstall(std::move(aRequest))) {
      // Notify chrome that we're going to wait for the CDM to download/update.
      EME_LOG("Await %s for installation",
              NS_ConvertUTF16toUTF8(keySystem).get());
      MediaKeySystemAccess::NotifyObservers(mWindow, keySystem, status);
    } else {
      // Failed to await the install. Log failure and give up trying to service
      // this request.
      EME_LOG("Failed to await %s for installation",
              NS_ConvertUTF16toUTF8(keySystem).get());
      diagnostics.StoreMediaKeySystemAccess(mWindow->GetExtantDoc(), keySystem,
                                            false, __func__);
    }
    return;
  }
  if (status != MediaKeySystemStatus::Available) {
    // Failed due to user disabling something, send a notification to
    // chrome, so we can show some UI to explain how the user can rectify
    // the situation.
    EME_LOG("Notify CDM failure for %s and reject the promise",
            NS_ConvertUTF16toUTF8(aRequest->mKeySystem).get());
    MediaKeySystemAccess::NotifyObservers(mWindow, aRequest->mKeySystem,
                                          status);
    aRequest->RejectPromiseWithNotSupportedError(message);
    return;
  }

  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();
  nsTHashMap<nsCharPtrHashKey, bool> warnings;
  std::function<void(const char*)> deprecationWarningLogFn =
      [&](const char* aMsgName) {
        EME_LOG(
            "MediaKeySystemAccessManager::DeprecationWarningLambda Logging "
            "deprecation warning '%s' to WebConsole.",
            aMsgName);
        warnings.InsertOrUpdate(aMsgName, true);
        AutoTArray<nsString, 1> params;
        nsString& uri = *params.AppendElement();
        if (doc) {
          Unused << doc->GetDocumentURI(uri);
        }
        nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "Media"_ns,
                                        doc, nsContentUtils::eDOM_PROPERTIES,
                                        aMsgName, params);
      };

  bool isPrivateBrowsing =
      mWindow->GetExtantDoc() &&
      mWindow->GetExtantDoc()->NodePrincipal()->GetPrivateBrowsingId() > 0;
  //   2. Let implementation be the implementation of keySystem.
  //   3. For each value in supportedConfigurations:
  //     1. Let candidate configuration be the value.
  //     2. Let supported configuration be the result of executing the Get
  //     Supported Configuration algorithm on implementation, candidate
  //     configuration, and origin.
  //     3. If supported configuration is not NotSupported, run the following
  //     steps:
  //       1. Let access be a new MediaKeySystemAccess object, and initialize it
  //       as follows:
  //         1. Set the keySystem attribute to keySystem.
  //         2. Let the configuration value be supported configuration.
  //         3. Let the cdm implementation value be implementation.
  //      2. Resolve promise with access and abort the parallel steps of this
  //      algorithm.
  MediaKeySystemConfiguration config;
  if (MediaKeySystemAccess::GetSupportedConfig(
          aRequest->mKeySystem, aRequest->mConfigs, config, &diagnostics,
          isPrivateBrowsing, deprecationWarningLogFn)) {
    aRequest->mSupportedConfig = Some(config);
    // The app gets the final say on if we provide access or not.
    CheckDoesAppAllowProtectedMedia(std::move(aRequest));
    return;
  }
  // 4. Reject promise with a NotSupportedError.

  // Not to inform user, because nothing to do if the corresponding keySystem
  // configuration is not supported.
  aRequest->RejectPromiseWithNotSupportedError(
      "Key system configuration is not supported"_ns);
  diagnostics.StoreMediaKeySystemAccess(mWindow->GetExtantDoc(),
                                        aRequest->mKeySystem, false, __func__);
}

void MediaKeySystemAccessManager::ProvideAccess(
    UniquePtr<PendingRequest> aRequest) {
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(
      aRequest->mSupportedConfig,
      "The request needs a supported config if we're going to provide access!");
  MKSAM_LOG_DEBUG("aRequest->mKeySystem=%s",
                  NS_ConvertUTF16toUTF8(aRequest->mKeySystem).get());

  DecoderDoctorDiagnostics diagnostics;

  RefPtr<MediaKeySystemAccess> access(new MediaKeySystemAccess(
      mWindow, aRequest->mKeySystem, aRequest->mSupportedConfig.ref()));
  aRequest->mPromise->MaybeResolve(access);
  diagnostics.StoreMediaKeySystemAccess(mWindow->GetExtantDoc(),
                                        aRequest->mKeySystem, true, __func__);
}

bool MediaKeySystemAccessManager::AwaitInstall(
    UniquePtr<PendingRequest> aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);
  MKSAM_LOG_DEBUG("aRequest->mKeySystem=%s",
                  NS_ConvertUTF16toUTF8(aRequest->mKeySystem).get());

  if (!EnsureObserversAdded()) {
    NS_WARNING("Failed to add pref observer");
    aRequest->RejectPromiseWithNotSupportedError(nsLiteralCString(
        "Failed trying to setup CDM update: failed adding observers"));
    return false;
  }

  nsCOMPtr<nsITimer> timer;
  NS_NewTimerWithObserver(getter_AddRefs(timer), this, 60 * 1000,
                          nsITimer::TYPE_ONE_SHOT);
  if (!timer) {
    NS_WARNING("Failed to create timer to await CDM install.");
    aRequest->RejectPromiseWithNotSupportedError(nsLiteralCString(
        "Failed trying to setup CDM update: failed timer creation"));
    return false;
  }

  MOZ_DIAGNOSTIC_ASSERT(
      aRequest->mTimer == nullptr,
      "Timer should not already be set on a request we're about to await");
  aRequest->mTimer = timer;

  mPendingInstallRequests.AppendElement(std::move(aRequest));
  return true;
}

void MediaKeySystemAccessManager::RetryRequest(
    UniquePtr<PendingRequest> aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);
  MKSAM_LOG_DEBUG("aRequest->mKeySystem=%s",
                  NS_ConvertUTF16toUTF8(aRequest->mKeySystem).get());
  // Cancel and null timer if it exists.
  aRequest->CancelTimer();
  // Indicate that this is a request that's being retried.
  aRequest->mRequestType = PendingRequest::RequestType::Subsequent;
  RequestMediaKeySystemAccess(std::move(aRequest));
}

nsresult MediaKeySystemAccessManager::Observe(nsISupports* aSubject,
                                              const char* aTopic,
                                              const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MKSAM_LOG_DEBUG("%s", aTopic);

  if (!strcmp(aTopic, "gmp-changed")) {
    // Filter out the requests where the CDM's install-status is no longer
    // "unavailable". This will be the CDMs which have downloaded since the
    // initial request.
    // Note: We don't have a way to communicate from chrome that the CDM has
    // failed to download, so we'll just let the timeout fail us in that case.
    nsTArray<UniquePtr<PendingRequest>> requests;
    for (size_t i = mPendingInstallRequests.Length(); i-- > 0;) {
      nsAutoCString message;
      MediaKeySystemStatus status = MediaKeySystemAccess::GetKeySystemStatus(
          *mPendingInstallRequests[i], message);
      if (status == MediaKeySystemStatus::Cdm_not_installed) {
        // Not yet installed, don't retry. Keep waiting until timeout.
        continue;
      }
      // Status has changed, retry request.
      requests.AppendElement(std::move(mPendingInstallRequests[i]));
      mPendingInstallRequests.RemoveElementAt(i);
    }
    // Retry all pending requests, but this time fail if the CDM is not
    // installed.
    for (size_t i = requests.Length(); i-- > 0;) {
      RetryRequest(std::move(requests[i]));
    }
  } else if (!strcmp(aTopic, "timer-callback")) {
    // Find the timer that expired and re-run the request for it.
    nsCOMPtr<nsITimer> timer(do_QueryInterface(aSubject));
    for (size_t i = 0; i < mPendingInstallRequests.Length(); i++) {
      if (mPendingInstallRequests[i]->mTimer == timer) {
        EME_LOG("MediaKeySystemAccessManager::AwaitInstall resuming request");
        UniquePtr<PendingRequest> request =
            std::move(mPendingInstallRequests[i]);
        mPendingInstallRequests.RemoveElementAt(i);
        RetryRequest(std::move(request));
        break;
      }
    }
  }
  return NS_OK;
}

nsresult MediaKeySystemAccessManager::GetName(nsACString& aName) {
  aName.AssignLiteral("MediaKeySystemAccessManager");
  return NS_OK;
}

bool MediaKeySystemAccessManager::EnsureObserversAdded() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mAddedObservers) {
    return true;
  }

  nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obsService)) {
    return false;
  }
  mAddedObservers =
      NS_SUCCEEDED(obsService->AddObserver(this, "gmp-changed", false));
  return mAddedObservers;
}

void MediaKeySystemAccessManager::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  MKSAM_LOG_DEBUG("");
  for (const UniquePtr<PendingRequest>& installRequest :
       mPendingInstallRequests) {
    // Cancel all requests; we're shutting down.
    installRequest->CancelTimer();
    installRequest->RejectPromiseWithInvalidAccessError(nsLiteralCString(
        "Promise still outstanding at MediaKeySystemAccessManager shutdown"));
  }
  mPendingInstallRequests.Clear();
  for (const UniquePtr<PendingRequest>& approvalRequest :
       mPendingAppApprovalRequests) {
    approvalRequest->RejectPromiseWithInvalidAccessError(nsLiteralCString(
        "Promise still outstanding at MediaKeySystemAccessManager shutdown"));
  }
  mPendingAppApprovalRequests.Clear();
  mAppAllowsProtectedMediaPromiseRequest.DisconnectIfExists();
  if (mAddedObservers) {
    nsCOMPtr<nsIObserverService> obsService =
        mozilla::services::GetObserverService();
    if (obsService) {
      obsService->RemoveObserver(this, "gmp-changed");
      mAddedObservers = false;
    }
  }
}

}  // namespace mozilla::dom

#undef MKSAM_LOG_DEBUG
