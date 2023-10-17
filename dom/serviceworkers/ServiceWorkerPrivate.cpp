/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerPrivate.h"

#include <utility>

#include "MainThreadUtils.h"
#include "ServiceWorkerCloneData.h"
#include "ServiceWorkerManager.h"
#include "ServiceWorkerRegistrationInfo.h"
#include "ServiceWorkerUtils.h"
#include "js/ErrorReport.h"
#include "mozIThirdPartyUtil.h"
#include "mozilla/Assertions.h"
#include "mozilla/CycleCollectedJSContext.h"  // for MicroTaskRunnable
#include "mozilla/ErrorResult.h"
#include "mozilla/JSObjectHolder.h"
#include "mozilla/Maybe.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/RemoteLazyInputStreamStorage.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/FetchEventOpChild.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/ReferrerInfo.h"
#include "mozilla/dom/RemoteType.h"
#include "mozilla/dom/RemoteWorkerControllerChild.h"
#include "mozilla/dom/RemoteWorkerManager.h"  // RemoteWorkerManager::GetRemoteType
#include "mozilla/dom/ServiceWorkerBinding.h"
#include "mozilla/extensions/WebExtensionPolicy.h"  // WebExtensionPolicy
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/CookieJarSettings.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsICacheInfoChannel.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsINetworkInterceptController.h"
#include "nsINamed.h"
#include "nsIObserverService.h"
#include "nsIRedirectHistoryEntry.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupportsImpl.h"
#include "nsIURI.h"
#include "nsIUploadChannel2.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsRFPService.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsThreadUtils.h"

#include "mozilla/dom/Client.h"
#include "mozilla/dom/FetchUtil.h"
#include "mozilla/dom/IndexedDatabaseManager.h"
#include "mozilla/dom/NotificationEvent.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/PushEventBinding.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/WorkerDebugger.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "nsIReferrerInfo.h"

extern mozilla::LazyLogModule sWorkerTelemetryLog;

#ifdef LOG
#  undef LOG
#endif
#define LOG(_args) MOZ_LOG(sWorkerTelemetryLog, LogLevel::Debug, _args);

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla::dom {

uint32_t ServiceWorkerPrivate::sRunningServiceWorkers = 0;
uint32_t ServiceWorkerPrivate::sRunningServiceWorkersFetch = 0;
uint32_t ServiceWorkerPrivate::sRunningServiceWorkersMax = 0;
uint32_t ServiceWorkerPrivate::sRunningServiceWorkersFetchMax = 0;

// Tracks the "dom.serviceWorkers.disable_open_click_delay" preference. Modified
// on main thread, read on worker threads.
// It is updated every time a "notificationclick" event is dispatched. While
// this is done without synchronization, at the worst, the thread will just get
// an older value within which a popup is allowed to be displayed, which will
// still be a valid value since it was set prior to dispatching the runnable.
Atomic<uint32_t> gDOMDisableOpenClickDelay(0);

/**
 * KeepAliveToken
 */
KeepAliveToken::KeepAliveToken(ServiceWorkerPrivate* aPrivate)
    : mPrivate(aPrivate) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrivate);
  mPrivate->AddToken();
}

KeepAliveToken::~KeepAliveToken() {
  MOZ_ASSERT(NS_IsMainThread());
  mPrivate->ReleaseToken();
}

NS_IMPL_ISUPPORTS0(KeepAliveToken)

/**
 * RAIIActorPtrHolder
 */
ServiceWorkerPrivate::RAIIActorPtrHolder::RAIIActorPtrHolder(
    already_AddRefed<RemoteWorkerControllerChild> aActor)
    : mActor(aActor) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mActor->Manager());
}

ServiceWorkerPrivate::RAIIActorPtrHolder::~RAIIActorPtrHolder() {
  AssertIsOnMainThread();

  mDestructorPromiseHolder.ResolveIfExists(true, __func__);

  mActor->MaybeSendDelete();
}

RemoteWorkerControllerChild*
ServiceWorkerPrivate::RAIIActorPtrHolder::operator->() const {
  AssertIsOnMainThread();

  return get();
}

RemoteWorkerControllerChild* ServiceWorkerPrivate::RAIIActorPtrHolder::get()
    const {
  AssertIsOnMainThread();

  return mActor.get();
}

RefPtr<GenericPromise>
ServiceWorkerPrivate::RAIIActorPtrHolder::OnDestructor() {
  AssertIsOnMainThread();

  return mDestructorPromiseHolder.Ensure(__func__);
}

/**
 *  PendingFunctionEvent
 */
ServiceWorkerPrivate::PendingFunctionalEvent::PendingFunctionalEvent(
    ServiceWorkerPrivate* aOwner,
    RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration)
    : mOwner(aOwner), mRegistration(std::move(aRegistration)) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOwner);
  MOZ_ASSERT(mOwner->mInfo);
  MOZ_ASSERT(mOwner->mInfo->State() == ServiceWorkerState::Activating);
  MOZ_ASSERT(mRegistration);
}

ServiceWorkerPrivate::PendingFunctionalEvent::~PendingFunctionalEvent() {
  AssertIsOnMainThread();
}

ServiceWorkerPrivate::PendingPushEvent::PendingPushEvent(
    ServiceWorkerPrivate* aOwner,
    RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
    ServiceWorkerPushEventOpArgs&& aArgs)
    : PendingFunctionalEvent(aOwner, std::move(aRegistration)),
      mArgs(std::move(aArgs)) {
  AssertIsOnMainThread();
}

nsresult ServiceWorkerPrivate::PendingPushEvent::Send() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOwner);
  MOZ_ASSERT(mOwner->mInfo);

  return mOwner->SendPushEventInternal(std::move(mRegistration),
                                       std::move(mArgs));
}

ServiceWorkerPrivate::PendingFetchEvent::PendingFetchEvent(
    ServiceWorkerPrivate* aOwner,
    RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
    ParentToParentServiceWorkerFetchEventOpArgs&& aArgs,
    nsCOMPtr<nsIInterceptedChannel>&& aChannel,
    RefPtr<FetchServicePromises>&& aPreloadResponseReadyPromises)
    : PendingFunctionalEvent(aOwner, std::move(aRegistration)),
      mArgs(std::move(aArgs)),
      mChannel(std::move(aChannel)),
      mPreloadResponseReadyPromises(std::move(aPreloadResponseReadyPromises)) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mChannel);
}

nsresult ServiceWorkerPrivate::PendingFetchEvent::Send() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOwner);
  MOZ_ASSERT(mOwner->mInfo);

  return mOwner->SendFetchEventInternal(
      std::move(mRegistration), std::move(mArgs), std::move(mChannel),
      std::move(mPreloadResponseReadyPromises));
}

ServiceWorkerPrivate::PendingFetchEvent::~PendingFetchEvent() {
  AssertIsOnMainThread();

  if (NS_WARN_IF(mChannel)) {
    mChannel->CancelInterception(NS_ERROR_INTERCEPTION_FAILED);
  }
}

namespace {

class HeaderFiller final : public nsIHttpHeaderVisitor {
 public:
  NS_DECL_ISUPPORTS

  explicit HeaderFiller(HeadersGuardEnum aGuard)
      : mInternalHeaders(new InternalHeaders(aGuard)) {
    MOZ_ASSERT(mInternalHeaders);
  }

  NS_IMETHOD
  VisitHeader(const nsACString& aHeader, const nsACString& aValue) override {
    ErrorResult result;
    mInternalHeaders->Append(aHeader, aValue, result);

    if (NS_WARN_IF(result.Failed())) {
      return result.StealNSResult();
    }

    return NS_OK;
  }

  RefPtr<InternalHeaders> Extract() {
    return RefPtr<InternalHeaders>(std::move(mInternalHeaders));
  }

 private:
  ~HeaderFiller() = default;

  RefPtr<InternalHeaders> mInternalHeaders;
};

NS_IMPL_ISUPPORTS(HeaderFiller, nsIHttpHeaderVisitor)

Result<IPCInternalRequest, nsresult> GetIPCInternalRequest(
    nsIInterceptedChannel* aChannel) {
  AssertIsOnMainThread();

  nsCOMPtr<nsIURI> uri;
  MOZ_TRY(aChannel->GetSecureUpgradedChannelURI(getter_AddRefs(uri)));

  nsCOMPtr<nsIURI> uriNoFragment;
  MOZ_TRY(NS_GetURIWithoutRef(uri, getter_AddRefs(uriNoFragment)));

  nsCOMPtr<nsIChannel> underlyingChannel;
  MOZ_TRY(aChannel->GetChannel(getter_AddRefs(underlyingChannel)));

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(underlyingChannel);
  MOZ_ASSERT(httpChannel, "How come we don't have an HTTP channel?");

  nsCOMPtr<nsIHttpChannelInternal> internalChannel =
      do_QueryInterface(httpChannel);
  NS_ENSURE_TRUE(internalChannel, Err(NS_ERROR_NOT_AVAILABLE));

  nsCOMPtr<nsICacheInfoChannel> cacheInfoChannel =
      do_QueryInterface(underlyingChannel);

  nsAutoCString spec;
  MOZ_TRY(uriNoFragment->GetSpec(spec));

  nsAutoCString fragment;
  MOZ_TRY(uri->GetRef(fragment));

  nsAutoCString method;
  MOZ_TRY(httpChannel->GetRequestMethod(method));

  // This is safe due to static_asserts in ServiceWorkerManager.cpp
  uint32_t cacheModeInt;
  MOZ_ALWAYS_SUCCEEDS(internalChannel->GetFetchCacheMode(&cacheModeInt));
  RequestCache cacheMode = static_cast<RequestCache>(cacheModeInt);

  RequestMode requestMode =
      InternalRequest::MapChannelToRequestMode(underlyingChannel);

  // This is safe due to static_asserts in ServiceWorkerManager.cpp
  uint32_t redirectMode;
  MOZ_ALWAYS_SUCCEEDS(internalChannel->GetRedirectMode(&redirectMode));
  RequestRedirect requestRedirect = static_cast<RequestRedirect>(redirectMode);

  RequestCredentials requestCredentials =
      InternalRequest::MapChannelToRequestCredentials(underlyingChannel);

  nsAutoString referrer;
  ReferrerPolicy referrerPolicy = ReferrerPolicy::_empty;
  ReferrerPolicy environmentReferrerPolicy = ReferrerPolicy::_empty;

  nsCOMPtr<nsIReferrerInfo> referrerInfo = httpChannel->GetReferrerInfo();
  if (referrerInfo) {
    referrerPolicy = referrerInfo->ReferrerPolicy();
    Unused << referrerInfo->GetComputedReferrerSpec(referrer);
  }

  uint32_t loadFlags;
  MOZ_TRY(underlyingChannel->GetLoadFlags(&loadFlags));

  nsCOMPtr<nsILoadInfo> loadInfo = underlyingChannel->LoadInfo();
  nsContentPolicyType contentPolicyType = loadInfo->InternalContentPolicyType();

  nsAutoString integrity;
  MOZ_TRY(internalChannel->GetIntegrityMetadata(integrity));

  RefPtr<HeaderFiller> headerFiller =
      MakeRefPtr<HeaderFiller>(HeadersGuardEnum::Request);
  MOZ_TRY(httpChannel->VisitNonDefaultRequestHeaders(headerFiller));

  RefPtr<InternalHeaders> internalHeaders = headerFiller->Extract();

  ErrorResult result;
  internalHeaders->SetGuard(HeadersGuardEnum::Immutable, result);
  if (NS_WARN_IF(result.Failed())) {
    return Err(result.StealNSResult());
  }

  nsTArray<HeadersEntry> ipcHeaders;
  HeadersGuardEnum ipcHeadersGuard;
  internalHeaders->ToIPC(ipcHeaders, ipcHeadersGuard);

  nsAutoCString alternativeDataType;
  if (cacheInfoChannel &&
      !cacheInfoChannel->PreferredAlternativeDataTypes().IsEmpty()) {
    // TODO: the internal request probably needs all the preferred types.
    alternativeDataType.Assign(
        cacheInfoChannel->PreferredAlternativeDataTypes()[0].type());
  }

  Maybe<PrincipalInfo> principalInfo;
  Maybe<PrincipalInfo> interceptionPrincipalInfo;
  if (loadInfo->TriggeringPrincipal()) {
    principalInfo.emplace();
    interceptionPrincipalInfo.emplace();
    MOZ_ALWAYS_SUCCEEDS(PrincipalToPrincipalInfo(
        loadInfo->TriggeringPrincipal(), principalInfo.ptr()));
    MOZ_ALWAYS_SUCCEEDS(PrincipalToPrincipalInfo(
        loadInfo->TriggeringPrincipal(), interceptionPrincipalInfo.ptr()));
  }

  nsTArray<RedirectHistoryEntryInfo> redirectChain;
  for (const nsCOMPtr<nsIRedirectHistoryEntry>& redirectEntry :
       loadInfo->RedirectChain()) {
    RedirectHistoryEntryInfo* entry = redirectChain.AppendElement();
    MOZ_ALWAYS_SUCCEEDS(RHEntryToRHEntryInfo(redirectEntry, entry));
  }

  bool isThirdPartyChannel;
  // ThirdPartyUtil* thirdPartyUtil = ThirdPartyUtil::GetInstance();
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
      do_GetService(THIRDPARTYUTIL_CONTRACTID);
  if (thirdPartyUtil) {
    nsCOMPtr<nsIURI> uri;
    MOZ_TRY(underlyingChannel->GetURI(getter_AddRefs(uri)));
    MOZ_TRY(thirdPartyUtil->IsThirdPartyChannel(underlyingChannel, uri,
                                                &isThirdPartyChannel));
  }

  nsILoadInfo::CrossOriginEmbedderPolicy embedderPolicy =
      loadInfo->GetLoadingEmbedderPolicy();

  // Note: all the arguments are copied rather than moved, which would be more
  // efficient, because there's no move-friendly constructor generated.
  return IPCInternalRequest(
      method, {spec}, ipcHeadersGuard, ipcHeaders, Nothing(), -1,
      alternativeDataType, contentPolicyType, referrer, referrerPolicy,
      environmentReferrerPolicy, requestMode, requestCredentials, cacheMode,
      requestRedirect, integrity, fragment, principalInfo,
      interceptionPrincipalInfo, contentPolicyType, redirectChain,
      isThirdPartyChannel, embedderPolicy);
}

nsresult MaybeStoreStreamForBackgroundThread(nsIInterceptedChannel* aChannel,
                                             IPCInternalRequest& aIPCRequest) {
  nsCOMPtr<nsIChannel> channel;
  MOZ_ALWAYS_SUCCEEDS(aChannel->GetChannel(getter_AddRefs(channel)));

  Maybe<BodyStreamVariant> body;
  nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(channel);

  if (uploadChannel) {
    nsCOMPtr<nsIInputStream> uploadStream;
    MOZ_TRY(uploadChannel->CloneUploadStream(&aIPCRequest.bodySize(),
                                             getter_AddRefs(uploadStream)));

    if (uploadStream) {
      Maybe<BodyStreamVariant>& body = aIPCRequest.body();
      body.emplace(ParentToParentStream());

      MOZ_TRY(
          nsID::GenerateUUIDInPlace(body->get_ParentToParentStream().uuid()));

      auto storageOrErr = RemoteLazyInputStreamStorage::Get();
      if (NS_WARN_IF(storageOrErr.isErr())) {
        return storageOrErr.unwrapErr();
      }

      auto storage = storageOrErr.unwrap();
      storage->AddStream(uploadStream, body->get_ParentToParentStream().uuid());
    }
  }

  return NS_OK;
}

}  // anonymous namespace

/**
 * ServiceWorkerPrivate
 */
ServiceWorkerPrivate::ServiceWorkerPrivate(ServiceWorkerInfo* aInfo)
    : mInfo(aInfo), mDebuggerCount(0), mTokenCount(0) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aInfo);
  MOZ_ASSERT(!mControllerChild);

  mIdleWorkerTimer = NS_NewTimer();
  MOZ_ASSERT(mIdleWorkerTimer);

  // Assert in all debug builds as well as non-debug Nightly and Dev Edition.
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(Initialize()));
#else
  MOZ_ALWAYS_SUCCEEDS(Initialize());
#endif
}

ServiceWorkerPrivate::~ServiceWorkerPrivate() {
  MOZ_ASSERT(!mTokenCount);
  MOZ_ASSERT(!mInfo);
  MOZ_ASSERT(!mControllerChild);
  MOZ_ASSERT(mIdlePromiseHolder.IsEmpty());

  mIdleWorkerTimer->Cancel();
}

nsresult ServiceWorkerPrivate::Initialize() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mInfo);

  nsCOMPtr<nsIPrincipal> principal = mInfo->Principal();

  nsCOMPtr<nsIURI> uri;
  auto* basePrin = BasePrincipal::Cast(principal);
  nsresult rv = basePrin->GetURI(getter_AddRefs(uri));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!uri)) {
    return NS_ERROR_FAILURE;
  }

  URIParams baseScriptURL;
  SerializeURI(uri, baseScriptURL);

  nsString id;
  rv = mInfo->GetId(id);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  PrincipalInfo principalInfo;
  rv = PrincipalToPrincipalInfo(principal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

  if (NS_WARN_IF(!swm)) {
    return NS_ERROR_DOM_ABORT_ERR;
  }

  RefPtr<ServiceWorkerRegistrationInfo> regInfo =
      swm->GetRegistration(principal, mInfo->Scope());

  if (NS_WARN_IF(!regInfo)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      net::CookieJarSettings::Create(principal);
  MOZ_ASSERT(cookieJarSettings);

  // We can populate the partitionKey and the fingerprinting protection
  // overrides using the originAttribute of the principal. If it has
  // partitionKey set, It's a foreign partitioned principal and it implies that
  // it's a third-party service worker. So, the cookieJarSettings can directly
  // use the partitionKey from it. For first-party case, we can populate the
  // partitionKey from the principal URI.
  Maybe<uint64_t> overriddenFingerprintingSettingsArg;
  Maybe<RFPTarget> overriddenFingerprintingSettings;
  if (!principal->OriginAttributesRef().mPartitionKey.IsEmpty()) {
    net::CookieJarSettings::Cast(cookieJarSettings)
        ->SetPartitionKey(principal->OriginAttributesRef().mPartitionKey);

    // The service worker is for a third-party context, we get first-party
    // domain from the partitionKey and the third-party domain from the
    // principal of the service worker. Then, we can get the fingerprinting
    // protection overrides using them.
    nsAutoString scheme;
    nsAutoString pkBaseDomain;
    int32_t unused;

    if (OriginAttributes::ParsePartitionKey(
            principal->OriginAttributesRef().mPartitionKey, scheme,
            pkBaseDomain, unused)) {
      nsCOMPtr<nsIURI> firstPartyURI;
      rv = NS_NewURI(getter_AddRefs(firstPartyURI),
                     scheme + u"://"_ns + pkBaseDomain);
      if (NS_SUCCEEDED(rv)) {
        overriddenFingerprintingSettings =
            nsRFPService::GetOverriddenFingerprintingSettingsForURI(
                firstPartyURI, uri);
        if (overriddenFingerprintingSettings.isSome()) {
          overriddenFingerprintingSettingsArg.emplace(
              uint64_t(overriddenFingerprintingSettings.ref()));
        }
      }
    }
  } else if (!principal->OriginAttributesRef().mFirstPartyDomain.IsEmpty()) {
    // Using the first party domain to know the context of the service worker.
    // We will run into here if FirstPartyIsolation is enabled. In this case,
    // the PartitionKey won't get populated.
    nsCOMPtr<nsIURI> firstPartyURI;
    // Because the service worker is only available in secure contexts, so we
    // don't need to consider http and only use https as scheme to create
    // the first-party URI
    rv = NS_NewURI(
        getter_AddRefs(firstPartyURI),
        u"https://"_ns + principal->OriginAttributesRef().mFirstPartyDomain);
    if (NS_SUCCEEDED(rv)) {
      // If the first party domain is not a third-party domain, the service
      // worker is running in first-party context.
      bool isThirdParty;
      rv = principal->IsThirdPartyURI(firstPartyURI, &isThirdParty);
      NS_ENSURE_SUCCESS(rv, rv);

      overriddenFingerprintingSettings =
          isThirdParty
              ? nsRFPService::GetOverriddenFingerprintingSettingsForURI(
                    firstPartyURI, uri)
              : nsRFPService::GetOverriddenFingerprintingSettingsForURI(
                    uri, nullptr);

      if (overriddenFingerprintingSettings.isSome()) {
        overriddenFingerprintingSettingsArg.emplace(
            uint64_t(overriddenFingerprintingSettings.ref()));
      }
    }
  } else {
    net::CookieJarSettings::Cast(cookieJarSettings)->SetPartitionKey(uri);

    // The service worker is for a first-party context, we can use the uri of
    // the service worker as the first-party domain to get the fingerprinting
    // protection overrides.
    overriddenFingerprintingSettings =
        nsRFPService::GetOverriddenFingerprintingSettingsForURI(uri, nullptr);

    if (overriddenFingerprintingSettings.isSome()) {
      overriddenFingerprintingSettingsArg.emplace(
          uint64_t(overriddenFingerprintingSettings.ref()));
    }
  }

  net::CookieJarSettingsArgs cjsData;
  net::CookieJarSettings::Cast(cookieJarSettings)->Serialize(cjsData);

  nsCOMPtr<nsIPrincipal> partitionedPrincipal;
  rv = StoragePrincipalHelper::CreatePartitionedPrincipalForServiceWorker(
      principal, cookieJarSettings, getter_AddRefs(partitionedPrincipal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  PrincipalInfo partitionedPrincipalInfo;
  rv =
      PrincipalToPrincipalInfo(partitionedPrincipal, &partitionedPrincipalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  StorageAccess storageAccess =
      StorageAllowedForServiceWorker(principal, cookieJarSettings);

  ServiceWorkerData serviceWorkerData;
  serviceWorkerData.cacheName() = mInfo->CacheName();
  serviceWorkerData.loadFlags() = static_cast<uint32_t>(
      mInfo->GetImportsLoadFlags() | nsIChannel::LOAD_BYPASS_SERVICE_WORKER);
  serviceWorkerData.id() = std::move(id);

  nsAutoCString domain;
  rv = uri->GetHost(domain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  auto remoteType = RemoteWorkerManager::GetRemoteType(
      principal, WorkerKind::WorkerKindService);
  if (NS_WARN_IF(remoteType.isErr())) {
    return remoteType.unwrapErr();
  }

  // Determine if the service worker is registered under a third-party context
  // by checking if it's running under a partitioned principal.
  bool isThirdPartyContextToTopWindow =
      !principal->OriginAttributesRef().mPartitionKey.IsEmpty();

  mRemoteWorkerData = RemoteWorkerData(
      NS_ConvertUTF8toUTF16(mInfo->ScriptSpec()), baseScriptURL, baseScriptURL,
      /* name */ VoidString(),
      /* workerType */ WorkerType::Classic,
      /* credentials */ RequestCredentials::Omit,
      /* loading principal */ principalInfo, principalInfo,
      partitionedPrincipalInfo,
      /* useRegularPrincipal */ true,

      // ServiceWorkers run as first-party, no storage-access permission needed.
      /* usingStorageAccess */ false,

      cjsData, domain,
      /* isSecureContext */ true,
      /* clientInfo*/ Nothing(),

      // The RemoteWorkerData CTOR doesn't allow to set the referrerInfo via
      // already_AddRefed<>. Let's set it to null.
      /* referrerInfo */ nullptr,

      storageAccess, isThirdPartyContextToTopWindow,
      nsContentUtils::ShouldResistFingerprinting_dangerous(
          principal,
          "Service Workers exist outside a Document or Channel; as a property "
          "of the domain (and origin attributes). We don't have a "
          "CookieJarSettings to perform the nested check, but we can rely on"
          "the FPI/dFPI partition key check. The WorkerPrivate's "
          "ShouldResistFingerprinting function for the ServiceWorker depends "
          "on this boolean and will also consider an explicit RFPTarget.",
          RFPTarget::IsAlwaysEnabledForPrecompute),
      overriddenFingerprintingSettingsArg,
      // Origin trials are associated to a window, so it doesn't make sense on
      // service workers.
      OriginTrials(), std::move(serviceWorkerData), regInfo->AgentClusterId(),
      remoteType.unwrap());

  mRemoteWorkerData.referrerInfo() = MakeAndAddRef<ReferrerInfo>();

  // This fills in the rest of mRemoteWorkerData.serviceWorkerData().
  RefreshRemoteWorkerData(regInfo);

  return NS_OK;
}

nsresult ServiceWorkerPrivate::CheckScriptEvaluation(
    RefPtr<LifeCycleEventCallback> aCallback) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);

  RefPtr<ServiceWorkerPrivate> self = this;

  /**
   * We need to capture the actor associated with the current Service Worker so
   * we can terminate it if script evaluation failed.
   */
  nsresult rv = SpawnWorkerIfNeeded();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aCallback->SetResult(false);
    aCallback->Run();

    return rv;
  }

  MOZ_ASSERT(mControllerChild);

  RefPtr<RAIIActorPtrHolder> holder = mControllerChild;

  return ExecServiceWorkerOp(
      ServiceWorkerCheckScriptEvaluationOpArgs(),
      [self = std::move(self), holder = std::move(holder),
       callback = aCallback](ServiceWorkerOpResult&& aResult) mutable {
        if (aResult.type() == ServiceWorkerOpResult::
                                  TServiceWorkerCheckScriptEvaluationOpResult) {
          auto& result =
              aResult.get_ServiceWorkerCheckScriptEvaluationOpResult();

          if (result.workerScriptExecutedSuccessfully()) {
            self->SetHandlesFetch(result.fetchHandlerWasAdded());
            if (self->mHandlesFetch == Unknown) {
              self->mHandlesFetch =
                  result.fetchHandlerWasAdded() ? Enabled : Disabled;
              // Update telemetry for # of running SW - the already-running SW
              // handles fetch
              if (self->mHandlesFetch == Enabled) {
                self->UpdateRunning(0, 1);
              }
            }

            callback->SetResult(result.workerScriptExecutedSuccessfully());
            callback->Run();
            return;
          }
        }

        /**
         * If script evaluation failed, first terminate the Service Worker
         * before invoking the callback.
         */
        MOZ_ASSERT_IF(aResult.type() == ServiceWorkerOpResult::Tnsresult,
                      NS_FAILED(aResult.get_nsresult()));

        // If a termination operation was already issued using `holder`...
        if (self->mControllerChild != holder) {
          holder->OnDestructor()->Then(
              GetCurrentSerialEventTarget(), __func__,
              [callback = std::move(callback)](
                  const GenericPromise::ResolveOrRejectValue&) {
                callback->SetResult(false);
                callback->Run();
              });

          return;
        }

        RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
        MOZ_ASSERT(swm);

        auto shutdownStateId = swm->MaybeInitServiceWorkerShutdownProgress();

        RefPtr<GenericNonExclusivePromise> promise =
            self->ShutdownInternal(shutdownStateId);

        swm->BlockShutdownOn(promise, shutdownStateId);

        promise->Then(
            GetCurrentSerialEventTarget(), __func__,
            [callback = std::move(callback)](
                const GenericNonExclusivePromise::ResolveOrRejectValue&) {
              callback->SetResult(false);
              callback->Run();
            });
      },
      [callback = aCallback] {
        callback->SetResult(false);
        callback->Run();
      });
}

nsresult ServiceWorkerPrivate::SendMessageEvent(
    RefPtr<ServiceWorkerCloneData>&& aData,
    const ClientInfoAndState& aClientInfoAndState) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aData);

  auto scopeExit = MakeScopeExit([&] { Shutdown(); });

  PBackgroundChild* bgChild = BackgroundChild::GetForCurrentThread();

  if (NS_WARN_IF(!bgChild)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  ServiceWorkerMessageEventOpArgs args;
  args.clientInfoAndState() = aClientInfoAndState;
  if (!aData->BuildClonedMessageData(args.clonedData())) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  scopeExit.release();

  return ExecServiceWorkerOp(
      std::move(args), [](ServiceWorkerOpResult&& aResult) {
        MOZ_ASSERT(aResult.type() == ServiceWorkerOpResult::Tnsresult);
      });
}

nsresult ServiceWorkerPrivate::SendLifeCycleEvent(
    const nsAString& aEventType, RefPtr<LifeCycleEventCallback> aCallback) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aCallback);

  return ExecServiceWorkerOp(
      ServiceWorkerLifeCycleEventOpArgs(nsString(aEventType)),
      [callback = aCallback](ServiceWorkerOpResult&& aResult) {
        MOZ_ASSERT(aResult.type() == ServiceWorkerOpResult::Tnsresult);

        callback->SetResult(NS_SUCCEEDED(aResult.get_nsresult()));
        callback->Run();
      },
      [callback = aCallback] {
        callback->SetResult(false);
        callback->Run();
      });
}

nsresult ServiceWorkerPrivate::SendPushEvent(
    const nsAString& aMessageId, const Maybe<nsTArray<uint8_t>>& aData,
    RefPtr<ServiceWorkerRegistrationInfo> aRegistration) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mInfo);
  MOZ_ASSERT(aRegistration);

  ServiceWorkerPushEventOpArgs args;
  args.messageId() = nsString(aMessageId);

  if (aData) {
    args.data() = aData.ref();
  } else {
    args.data() = void_t();
  }

  if (mInfo->State() == ServiceWorkerState::Activating) {
    UniquePtr<PendingFunctionalEvent> pendingEvent =
        MakeUnique<PendingPushEvent>(this, std::move(aRegistration),
                                     std::move(args));

    mPendingFunctionalEvents.AppendElement(std::move(pendingEvent));

    return NS_OK;
  }

  MOZ_ASSERT(mInfo->State() == ServiceWorkerState::Activated);

  return SendPushEventInternal(std::move(aRegistration), std::move(args));
}

nsresult ServiceWorkerPrivate::SendPushEventInternal(
    RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
    ServiceWorkerPushEventOpArgs&& aArgs) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aRegistration);

  return ExecServiceWorkerOp(
      std::move(aArgs),
      [registration = aRegistration](ServiceWorkerOpResult&& aResult) {
        MOZ_ASSERT(aResult.type() == ServiceWorkerOpResult::Tnsresult);

        registration->MaybeScheduleTimeCheckAndUpdate();
      },
      [registration = aRegistration]() {
        registration->MaybeScheduleTimeCheckAndUpdate();
      });
}

nsresult ServiceWorkerPrivate::SendPushSubscriptionChangeEvent() {
  AssertIsOnMainThread();

  return ExecServiceWorkerOp(
      ServiceWorkerPushSubscriptionChangeEventOpArgs(),
      [](ServiceWorkerOpResult&& aResult) {
        MOZ_ASSERT(aResult.type() == ServiceWorkerOpResult::Tnsresult);
      });
}

nsresult ServiceWorkerPrivate::SendNotificationEvent(
    const nsAString& aEventName, const nsAString& aID, const nsAString& aTitle,
    const nsAString& aDir, const nsAString& aLang, const nsAString& aBody,
    const nsAString& aTag, const nsAString& aIcon, const nsAString& aData,
    const nsAString& aBehavior, const nsAString& aScope) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aEventName.EqualsLiteral(NOTIFICATION_CLICK_EVENT_NAME)) {
    gDOMDisableOpenClickDelay =
        Preferences::GetInt("dom.serviceWorkers.disable_open_click_delay");
  } else if (!aEventName.EqualsLiteral(NOTIFICATION_CLOSE_EVENT_NAME)) {
    MOZ_ASSERT_UNREACHABLE("Invalid notification event name");
    return NS_ERROR_FAILURE;
  }

  ServiceWorkerNotificationEventOpArgs args;
  args.eventName() = nsString(aEventName);
  args.id() = nsString(aID);
  args.title() = nsString(aTitle);
  args.dir() = nsString(aDir);
  args.lang() = nsString(aLang);
  args.body() = nsString(aBody);
  args.tag() = nsString(aTag);
  args.icon() = nsString(aIcon);
  args.data() = nsString(aData);
  args.behavior() = nsString(aBehavior);
  args.scope() = nsString(aScope);
  args.disableOpenClickDelay() = gDOMDisableOpenClickDelay;

  return ExecServiceWorkerOp(
      std::move(args), [](ServiceWorkerOpResult&& aResult) {
        MOZ_ASSERT(aResult.type() == ServiceWorkerOpResult::Tnsresult);
      });
}

nsresult ServiceWorkerPrivate::SendFetchEvent(
    nsCOMPtr<nsIInterceptedChannel> aChannel, nsILoadGroup* aLoadGroup,
    const nsAString& aClientId, const nsAString& aResultingClientId) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aChannel);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (NS_WARN_IF(!mInfo || !swm)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIChannel> channel;
  nsresult rv = aChannel->GetChannel(getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);
  bool isNonSubresourceRequest =
      nsContentUtils::IsNonSubresourceRequest(channel);

  RefPtr<ServiceWorkerRegistrationInfo> registration;
  if (isNonSubresourceRequest) {
    registration = swm->GetRegistration(mInfo->Principal(), mInfo->Scope());
  } else {
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();

    // We'll check for a null registration below rather than an error code here.
    Unused << swm->GetClientRegistration(loadInfo->GetClientInfo().ref(),
                                         getter_AddRefs(registration));
  }

  // Its possible the registration is removed between starting the interception
  // and actually dispatching the fetch event.  In these cases we simply
  // want to restart the original network request.  Since this is a normal
  // condition we handle the reset here instead of returning an error which
  // would in turn trigger a console report.
  if (!registration) {
    nsresult rv = aChannel->ResetInterception(false);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to resume intercepted network request");
      aChannel->CancelInterception(rv);
    }
    return NS_OK;
  }

  // Handle Fetch algorithm - step 16. If the service worker didn't register
  // any fetch event handlers, then abort the interception and maybe trigger
  // the soft update algorithm.
  if (!mInfo->HandlesFetch()) {
    nsresult rv = aChannel->ResetInterception(false);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to resume intercepted network request");
      aChannel->CancelInterception(rv);
    }

    // Trigger soft updates if necessary.
    registration->MaybeScheduleTimeCheckAndUpdate();

    return NS_OK;
  }

  auto scopeExit = MakeScopeExit([&] {
    aChannel->CancelInterception(NS_ERROR_INTERCEPTION_FAILED);
    Shutdown();
  });

  IPCInternalRequest request;
  MOZ_TRY_VAR(request, GetIPCInternalRequest(aChannel));

  scopeExit.release();

  bool preloadNavigation = isNonSubresourceRequest &&
                           request.method().LowerCaseEqualsASCII("get") &&
                           registration->GetNavigationPreloadState().enabled();

  RefPtr<FetchServicePromises> preloadResponsePromises;
  if (preloadNavigation) {
    preloadResponsePromises = SetupNavigationPreload(aChannel, registration);
  }

  ParentToParentServiceWorkerFetchEventOpArgs args(
      ServiceWorkerFetchEventOpArgsCommon(
          mInfo->ScriptSpec(), request, nsString(aClientId),
          nsString(aResultingClientId), isNonSubresourceRequest,
          preloadNavigation, mInfo->TestingInjectCancellation()),
      Nothing(), Nothing(), Nothing());

  if (mInfo->State() == ServiceWorkerState::Activating) {
    UniquePtr<PendingFunctionalEvent> pendingEvent =
        MakeUnique<PendingFetchEvent>(this, std::move(registration),
                                      std::move(args), std::move(aChannel),
                                      std::move(preloadResponsePromises));

    mPendingFunctionalEvents.AppendElement(std::move(pendingEvent));

    return NS_OK;
  }

  MOZ_ASSERT(mInfo->State() == ServiceWorkerState::Activated);

  return SendFetchEventInternal(std::move(registration), std::move(args),
                                std::move(aChannel),
                                std::move(preloadResponsePromises));
}

nsresult ServiceWorkerPrivate::SendFetchEventInternal(
    RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
    ParentToParentServiceWorkerFetchEventOpArgs&& aArgs,
    nsCOMPtr<nsIInterceptedChannel>&& aChannel,
    RefPtr<FetchServicePromises>&& aPreloadResponseReadyPromises) {
  AssertIsOnMainThread();

  auto scopeExit = MakeScopeExit([&] { Shutdown(); });

  if (NS_WARN_IF(!mInfo)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  MOZ_TRY(SpawnWorkerIfNeeded());
  MOZ_TRY(MaybeStoreStreamForBackgroundThread(
      aChannel, aArgs.common().internalRequest()));

  scopeExit.release();

  MOZ_ASSERT(mControllerChild);

  RefPtr<RAIIActorPtrHolder> holder = mControllerChild;

  FetchEventOpChild::SendFetchEvent(
      mControllerChild->get(), std::move(aArgs), std::move(aChannel),
      std::move(aRegistration), std::move(aPreloadResponseReadyPromises),
      CreateEventKeepAliveToken())
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [holder = std::move(holder)](
                 const GenericPromise::ResolveOrRejectValue& aResult) {
               Unused << NS_WARN_IF(aResult.IsReject());
             });

  return NS_OK;
}

Result<RefPtr<ServiceWorkerPrivate::PromiseExtensionWorkerHasListener>,
       nsresult>
ServiceWorkerPrivate::WakeForExtensionAPIEvent(
    const nsAString& aExtensionAPINamespace,
    const nsAString& aExtensionAPIEventName) {
  AssertIsOnMainThread();

  ServiceWorkerExtensionAPIEventOpArgs args;
  args.apiNamespace() = nsString(aExtensionAPINamespace);
  args.apiEventName() = nsString(aExtensionAPIEventName);

  auto promise =
      MakeRefPtr<PromiseExtensionWorkerHasListener::Private>(__func__);

  nsresult rv = ExecServiceWorkerOp(
      std::move(args),
      [promise](ServiceWorkerOpResult&& aResult) {
        MOZ_ASSERT(
            aResult.type() ==
            ServiceWorkerOpResult::TServiceWorkerExtensionAPIEventOpResult);
        auto& result = aResult.get_ServiceWorkerExtensionAPIEventOpResult();
        promise->Resolve(result.extensionAPIEventListenerWasAdded(), __func__);
      },
      [promise]() { promise->Reject(NS_ERROR_FAILURE, __func__); });

  if (NS_FAILED(rv)) {
    promise->Reject(rv, __func__);
  }

  RefPtr<PromiseExtensionWorkerHasListener> outPromise(promise);
  return outPromise;
}

nsresult ServiceWorkerPrivate::SpawnWorkerIfNeeded() {
  AssertIsOnMainThread();

  if (mControllerChild) {
    RenewKeepAliveToken();
    return NS_OK;
  }

  if (!mInfo) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  mServiceWorkerLaunchTimeStart = TimeStamp::Now();

  PBackgroundChild* bgChild = BackgroundChild::GetForCurrentThread();

  if (NS_WARN_IF(!bgChild)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  // If the worker principal is an extension principal, then we should not spawn
  // a worker if there is no WebExtensionPolicy associated to that principal
  // or if the WebExtensionPolicy is not active.
  auto* principal = mInfo->Principal();
  if (principal->SchemeIs("moz-extension")) {
    auto* addonPolicy = BasePrincipal::Cast(principal)->AddonPolicy();
    if (!addonPolicy || !addonPolicy->Active()) {
      NS_WARNING(
          "Trying to wake up a service worker for a disabled webextension.");
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

  if (NS_WARN_IF(!swm)) {
    return NS_ERROR_DOM_ABORT_ERR;
  }

  RefPtr<ServiceWorkerRegistrationInfo> regInfo =
      swm->GetRegistration(principal, mInfo->Scope());

  if (NS_WARN_IF(!regInfo)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  RefreshRemoteWorkerData(regInfo);

  RefPtr<RemoteWorkerControllerChild> controllerChild =
      new RemoteWorkerControllerChild(this);

  if (NS_WARN_IF(!bgChild->SendPRemoteWorkerControllerConstructor(
          controllerChild, mRemoteWorkerData))) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  /**
   * Manually `AddRef()` because `DeallocPRemoteWorkerControllerChild()`
   * calls `Release()` and the `AllocPRemoteWorkerControllerChild()` function
   * is not called.
   */
  // NOLINTNEXTLINE(readability-redundant-smartptr-get)
  controllerChild.get()->AddRef();

  mControllerChild = new RAIIActorPtrHolder(controllerChild.forget());

  // Update Running count here because we may Terminate before we get
  // CreationSucceeded().  We'll update if it handles Fetch if that changes
  // (
  UpdateRunning(1, mHandlesFetch == Enabled ? 1 : 0);

  return NS_OK;
}

void ServiceWorkerPrivate::TerminateWorker() {
  MOZ_ASSERT(NS_IsMainThread());
  mIdleWorkerTimer->Cancel();
  mIdleKeepAliveToken = nullptr;
  Shutdown();
}

void ServiceWorkerPrivate::NoteDeadServiceWorkerInfo() {
  MOZ_ASSERT(NS_IsMainThread());

  TerminateWorker();
  mInfo = nullptr;
}

void ServiceWorkerPrivate::UpdateState(ServiceWorkerState aState) {
  AssertIsOnMainThread();

  if (!mControllerChild) {
    return;
  }

  nsresult rv = ExecServiceWorkerOp(
      ServiceWorkerUpdateStateOpArgs(aState),
      [](ServiceWorkerOpResult&& aResult) {
        MOZ_ASSERT(aResult.type() == ServiceWorkerOpResult::Tnsresult);
      });

  if (NS_WARN_IF(NS_FAILED(rv))) {
    Shutdown();
    return;
  }

  if (aState != ServiceWorkerState::Activated) {
    return;
  }

  for (auto& event : mPendingFunctionalEvents) {
    Unused << NS_WARN_IF(NS_FAILED(event->Send()));
  }

  mPendingFunctionalEvents.Clear();
}

nsresult ServiceWorkerPrivate::GetDebugger(nsIWorkerDebugger** aResult) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aResult);

  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult ServiceWorkerPrivate::AttachDebugger() {
  MOZ_ASSERT(NS_IsMainThread());

  // When the first debugger attaches to a worker, we spawn a worker if needed,
  // and cancel the idle timeout. The idle timeout should not be reset until
  // the last debugger detached from the worker.
  if (!mDebuggerCount) {
    nsresult rv = SpawnWorkerIfNeeded();
    NS_ENSURE_SUCCESS(rv, rv);

    /**
     * Renewing the idle KeepAliveToken for spawning workers happens
     * asynchronously, rather than synchronously.
     * The asynchronous renewal is because the actual spawning of workers occurs
     * in a content process, so we will only renew once notified that the worker
     * has been successfully created
     *
     * This means that the DevTools way of starting up a worker by calling
     * `AttachDebugger` immediately followed by `DetachDebugger` will spawn and
     * immediately terminate a worker (because `mTokenCount` is possibly 0
     * due to the idle KeepAliveToken being created asynchronously). So, just
     * renew the KeepAliveToken right now.
     */
    RenewKeepAliveToken();
    mIdleWorkerTimer->Cancel();
  }

  ++mDebuggerCount;

  return NS_OK;
}

nsresult ServiceWorkerPrivate::DetachDebugger() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDebuggerCount) {
    return NS_ERROR_UNEXPECTED;
  }

  --mDebuggerCount;

  // When the last debugger detaches from a worker, we either reset the idle
  // timeout, or terminate the worker if there are no more active tokens.
  if (!mDebuggerCount) {
    if (mTokenCount) {
      ResetIdleTimeout();
    } else {
      TerminateWorker();
    }
  }

  return NS_OK;
}

bool ServiceWorkerPrivate::IsIdle() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mTokenCount == 0 || (mTokenCount == 1 && mIdleKeepAliveToken);
}

RefPtr<GenericPromise> ServiceWorkerPrivate::GetIdlePromise() {
#ifdef DEBUG
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsIdle());
  MOZ_ASSERT(!mIdlePromiseObtained, "Idle promise may only be obtained once!");
  mIdlePromiseObtained = true;
#endif

  return mIdlePromiseHolder.Ensure(__func__);
}

namespace {

class ServiceWorkerPrivateTimerCallback final : public nsITimerCallback,
                                                public nsINamed {
 public:
  using Method = void (ServiceWorkerPrivate::*)(nsITimer*);

  ServiceWorkerPrivateTimerCallback(ServiceWorkerPrivate* aServiceWorkerPrivate,
                                    Method aMethod)
      : mServiceWorkerPrivate(aServiceWorkerPrivate), mMethod(aMethod) {}

  NS_IMETHOD
  Notify(nsITimer* aTimer) override {
    (mServiceWorkerPrivate->*mMethod)(aTimer);
    mServiceWorkerPrivate = nullptr;
    return NS_OK;
  }

  NS_IMETHOD
  GetName(nsACString& aName) override {
    aName.AssignLiteral("ServiceWorkerPrivateTimerCallback");
    return NS_OK;
  }

 private:
  ~ServiceWorkerPrivateTimerCallback() = default;

  RefPtr<ServiceWorkerPrivate> mServiceWorkerPrivate;
  Method mMethod;

  NS_DECL_THREADSAFE_ISUPPORTS
};

NS_IMPL_ISUPPORTS(ServiceWorkerPrivateTimerCallback, nsITimerCallback,
                  nsINamed);

}  // anonymous namespace

void ServiceWorkerPrivate::NoteIdleWorkerCallback(nsITimer* aTimer) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(aTimer == mIdleWorkerTimer, "Invalid timer!");

  // Release ServiceWorkerPrivate's token, since the grace period has ended.
  mIdleKeepAliveToken = nullptr;

  if (mControllerChild) {
    // If we still have a living worker at this point it means that either there
    // are pending waitUntil promises or the worker is doing some long-running
    // computation. Wait a bit more until we forcibly terminate the worker.
    uint32_t timeout =
        Preferences::GetInt("dom.serviceWorkers.idle_extended_timeout");
    nsCOMPtr<nsITimerCallback> cb = new ServiceWorkerPrivateTimerCallback(
        this, &ServiceWorkerPrivate::TerminateWorkerCallback);
    DebugOnly<nsresult> rv = mIdleWorkerTimer->InitWithCallback(
        cb, timeout, nsITimer::TYPE_ONE_SHOT);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

void ServiceWorkerPrivate::TerminateWorkerCallback(nsITimer* aTimer) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(aTimer == this->mIdleWorkerTimer, "Invalid timer!");

  // mInfo must be non-null at this point because NoteDeadServiceWorkerInfo
  // which zeroes it calls TerminateWorker which cancels our timer which will
  // ensure we don't get invoked even if the nsTimerEvent is in the event queue.
  ServiceWorkerManager::LocalizeAndReportToAllClients(
      mInfo->Scope(), "ServiceWorkerGraceTimeoutTermination",
      nsTArray<nsString>{NS_ConvertUTF8toUTF16(mInfo->Scope())});

  TerminateWorker();
}

void ServiceWorkerPrivate::RenewKeepAliveToken() {
  // We should have an active worker if we're renewing the keep alive token.
  MOZ_ASSERT(mControllerChild);

  // If there is at least one debugger attached to the worker, the idle worker
  // timeout was canceled when the first debugger attached to the worker. It
  // should not be reset until the last debugger detaches from the worker.
  if (!mDebuggerCount) {
    ResetIdleTimeout();
  }

  if (!mIdleKeepAliveToken) {
    mIdleKeepAliveToken = new KeepAliveToken(this);
  }
}

void ServiceWorkerPrivate::ResetIdleTimeout() {
  uint32_t timeout = Preferences::GetInt("dom.serviceWorkers.idle_timeout");
  nsCOMPtr<nsITimerCallback> cb = new ServiceWorkerPrivateTimerCallback(
      this, &ServiceWorkerPrivate::NoteIdleWorkerCallback);
  DebugOnly<nsresult> rv =
      mIdleWorkerTimer->InitWithCallback(cb, timeout, nsITimer::TYPE_ONE_SHOT);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void ServiceWorkerPrivate::AddToken() {
  MOZ_ASSERT(NS_IsMainThread());
  ++mTokenCount;
}

void ServiceWorkerPrivate::ReleaseToken() {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(mTokenCount > 0);
  --mTokenCount;

  if (IsIdle()) {
    mIdlePromiseHolder.ResolveIfExists(true, __func__);

    if (!mTokenCount) {
      TerminateWorker();
    }

    // mInfo can be nullptr here if NoteDeadServiceWorkerInfo() is called while
    // the KeepAliveToken is being proxy released as a runnable.
    else if (mInfo) {
      RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
      if (swm) {
        swm->WorkerIsIdle(mInfo);
      }
    }
  }
}

already_AddRefed<KeepAliveToken>
ServiceWorkerPrivate::CreateEventKeepAliveToken() {
  MOZ_ASSERT(NS_IsMainThread());

  // When the WorkerPrivate is in a separate process, we first hold a normal
  // KeepAliveToken. Then, after we're notified that the worker is alive, we
  // create the idle KeepAliveToken.
  MOZ_ASSERT(mIdleKeepAliveToken || mControllerChild);

  RefPtr<KeepAliveToken> ref = new KeepAliveToken(this);
  return ref.forget();
}

void ServiceWorkerPrivate::SetHandlesFetch(bool aValue) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!mInfo)) {
    return;
  }

  mInfo->SetHandlesFetch(aValue);
}

RefPtr<GenericPromise> ServiceWorkerPrivate::SetSkipWaitingFlag() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mInfo);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

  if (!swm) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  RefPtr<ServiceWorkerRegistrationInfo> regInfo =
      swm->GetRegistration(mInfo->Principal(), mInfo->Scope());

  if (!regInfo) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  mInfo->SetSkipWaitingFlag();

  RefPtr<GenericPromise::Private> promise =
      new GenericPromise::Private(__func__);

  regInfo->TryToActivateAsync([promise] { promise->Resolve(true, __func__); });

  return promise;
}

/* static */
void ServiceWorkerPrivate::UpdateRunning(int32_t aDelta, int32_t aFetchDelta) {
  // Record values for time we were running at the current values
  RefPtr<ServiceWorkerManager> manager(ServiceWorkerManager::GetInstance());
  manager->RecordTelemetry(sRunningServiceWorkers, sRunningServiceWorkersFetch);

  MOZ_ASSERT(((int64_t)sRunningServiceWorkers) + aDelta >= 0);
  sRunningServiceWorkers += aDelta;
  if (sRunningServiceWorkers > sRunningServiceWorkersMax) {
    sRunningServiceWorkersMax = sRunningServiceWorkers;
    LOG(("ServiceWorker max now %d", sRunningServiceWorkersMax));
    Telemetry::ScalarSet(Telemetry::ScalarID::SERVICEWORKER_RUNNING_MAX,
                         u"All"_ns, sRunningServiceWorkersMax);
  }
  MOZ_ASSERT(((int64_t)sRunningServiceWorkersFetch) + aFetchDelta >= 0);
  sRunningServiceWorkersFetch += aFetchDelta;
  if (sRunningServiceWorkersFetch > sRunningServiceWorkersFetchMax) {
    sRunningServiceWorkersFetchMax = sRunningServiceWorkersFetch;
    LOG(("ServiceWorker Fetch max now %d", sRunningServiceWorkersFetchMax));
    Telemetry::ScalarSet(Telemetry::ScalarID::SERVICEWORKER_RUNNING_MAX,
                         u"Fetch"_ns, sRunningServiceWorkersFetchMax);
  }
  LOG(("ServiceWorkers running now %d/%d", sRunningServiceWorkers,
       sRunningServiceWorkersFetch));
}

void ServiceWorkerPrivate::CreationFailed() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mControllerChild);

  if (mRemoteWorkerData.remoteType().Find(SERVICEWORKER_REMOTE_TYPE) !=
      kNotFound) {
    Telemetry::AccumulateTimeDelta(
        Telemetry::SERVICE_WORKER_ISOLATED_LAUNCH_TIME,
        mServiceWorkerLaunchTimeStart);
  } else {
    Telemetry::AccumulateTimeDelta(Telemetry::SERVICE_WORKER_LAUNCH_TIME_2,
                                   mServiceWorkerLaunchTimeStart);
  }

  Shutdown();
}

void ServiceWorkerPrivate::CreationSucceeded() {
  AssertIsOnMainThread();
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInfo);
  MOZ_ASSERT(mControllerChild);

  if (mRemoteWorkerData.remoteType().Find(SERVICEWORKER_REMOTE_TYPE) !=
      kNotFound) {
    Telemetry::AccumulateTimeDelta(
        Telemetry::SERVICE_WORKER_ISOLATED_LAUNCH_TIME,
        mServiceWorkerLaunchTimeStart);
  } else {
    Telemetry::AccumulateTimeDelta(Telemetry::SERVICE_WORKER_LAUNCH_TIME_2,
                                   mServiceWorkerLaunchTimeStart);
  }

  RenewKeepAliveToken();

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  nsCOMPtr<nsIPrincipal> principal = mInfo->Principal();
  RefPtr<ServiceWorkerRegistrationInfo> regInfo =
      swm->GetRegistration(principal, mInfo->Scope());
  if (regInfo) {
    // If it's already set, we're done and the running count is already set
    if (mHandlesFetch == Unknown) {
      if (regInfo->GetActive()) {
        mHandlesFetch =
            regInfo->GetActive()->HandlesFetch() ? Enabled : Disabled;
        if (mHandlesFetch == Enabled) {
          UpdateRunning(0, 1);
        }
      }
      // else we're likely still in Evaluating state, and don't know if it
      // handles fetch.  If so, defer updating the counter for Fetch until we
      // finish evaluation.  We already updated the Running count for All in
      // SpawnWorkerIfNeeded().
    }
  }
}

void ServiceWorkerPrivate::ErrorReceived(const ErrorValue& aError) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mInfo);
  MOZ_ASSERT(mControllerChild);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);

  ServiceWorkerInfo* info = mInfo;

  swm->HandleError(nullptr, info->Principal(), info->Scope(),
                   NS_ConvertUTF8toUTF16(info->ScriptSpec()), u""_ns, u""_ns,
                   u""_ns, 0, 0, nsIScriptError::errorFlag, JSEXN_ERR);
}

void ServiceWorkerPrivate::Terminated() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mInfo);
  MOZ_ASSERT(mControllerChild);

  Shutdown();
}

void ServiceWorkerPrivate::RefreshRemoteWorkerData(
    const RefPtr<ServiceWorkerRegistrationInfo>& aRegistration) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mInfo);

  ServiceWorkerData& serviceWorkerData =
      mRemoteWorkerData.serviceWorkerData().get_ServiceWorkerData();
  serviceWorkerData.descriptor() = mInfo->Descriptor().ToIPC();
  serviceWorkerData.registrationDescriptor() =
      aRegistration->Descriptor().ToIPC();
}

RefPtr<FetchServicePromises> ServiceWorkerPrivate::SetupNavigationPreload(
    nsCOMPtr<nsIInterceptedChannel>& aChannel,
    const RefPtr<ServiceWorkerRegistrationInfo>& aRegistration) {
  MOZ_ASSERT(XRE_IsParentProcess());
  AssertIsOnMainThread();

  // create IPC request from the intercepted channel.
  auto result = GetIPCInternalRequest(aChannel);
  if (result.isErr()) {
    return nullptr;
  }
  IPCInternalRequest ipcRequest = result.unwrap();

  // Step 1. Clone the request for preload
  // Create the InternalResponse from the created IPCRequest.
  SafeRefPtr<InternalRequest> preloadRequest =
      MakeSafeRefPtr<InternalRequest>(ipcRequest);
  // Copy the request body from uploadChannel
  nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(aChannel);
  if (uploadChannel) {
    nsCOMPtr<nsIInputStream> uploadStream;
    nsresult rv = uploadChannel->CloneUploadStream(
        &ipcRequest.bodySize(), getter_AddRefs(uploadStream));
    // Fail to get the request's body, stop navigation preload by returning
    // nullptr.
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return FetchService::NetworkErrorResponse(rv);
    }
    preloadRequest->SetBody(uploadStream, ipcRequest.bodySize());
  }

  // Set SkipServiceWorker for the navigation preload request
  preloadRequest->SetSkipServiceWorker();

  // Step 2. Append Service-Worker-Navigation-Preload header with
  //         registration->GetNavigationPreloadState().headerValue() on
  //         request's header list.
  IgnoredErrorResult err;
  auto headersGuard = preloadRequest->Headers()->Guard();
  preloadRequest->Headers()->SetGuard(HeadersGuardEnum::None, err);
  preloadRequest->Headers()->Append(
      "Service-Worker-Navigation-Preload"_ns,
      aRegistration->GetNavigationPreloadState().headerValue(), err);
  preloadRequest->Headers()->SetGuard(headersGuard, err);

  // Step 3. Perform fetch through FetchService with the cloned request
  if (!err.Failed()) {
    nsCOMPtr<nsIChannel> underlyingChannel;
    MOZ_ALWAYS_SUCCEEDS(
        aChannel->GetChannel(getter_AddRefs(underlyingChannel)));
    RefPtr<FetchService> fetchService = FetchService::GetInstance();
    return fetchService->Fetch(AsVariant(FetchService::NavigationPreloadArgs{
        std::move(preloadRequest), underlyingChannel}));
  }
  return FetchService::NetworkErrorResponse(NS_ERROR_UNEXPECTED);
}

void ServiceWorkerPrivate::Shutdown() {
  AssertIsOnMainThread();

  if (mControllerChild) {
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

    MOZ_ASSERT(swm,
               "All Service Workers should start shutting down before the "
               "ServiceWorkerManager does!");

    auto shutdownStateId = swm->MaybeInitServiceWorkerShutdownProgress();

    RefPtr<GenericNonExclusivePromise> promise =
        ShutdownInternal(shutdownStateId);
    swm->BlockShutdownOn(promise, shutdownStateId);
  }

  MOZ_ASSERT(!mControllerChild);
}

RefPtr<GenericNonExclusivePromise> ServiceWorkerPrivate::ShutdownInternal(
    uint32_t aShutdownStateId) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mControllerChild);

  mPendingFunctionalEvents.Clear();

  mControllerChild->get()->RevokeObserver(this);

  if (StaticPrefs::dom_serviceWorkers_testing_enabled()) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->NotifyObservers(nullptr, "service-worker-shutdown", nullptr);
    }
  }

  RefPtr<GenericNonExclusivePromise::Private> promise =
      new GenericNonExclusivePromise::Private(__func__);

  Unused << ExecServiceWorkerOp(
      ServiceWorkerTerminateWorkerOpArgs(aShutdownStateId),
      [promise](ServiceWorkerOpResult&& aResult) {
        MOZ_ASSERT(aResult.type() == ServiceWorkerOpResult::Tnsresult);
        promise->Resolve(true, __func__);
      },
      [promise]() { promise->Reject(NS_ERROR_DOM_ABORT_ERR, __func__); });

  /**
   * After dispatching a termination operation, no new operations should
   * be routed through this actor anymore.
   */
  mControllerChild = nullptr;

  // Update here, since Evaluation failures directly call ShutdownInternal
  UpdateRunning(-1, mHandlesFetch == Enabled ? -1 : 0);

  return promise;
}

nsresult ServiceWorkerPrivate::ExecServiceWorkerOp(
    ServiceWorkerOpArgs&& aArgs,
    std::function<void(ServiceWorkerOpResult&&)>&& aSuccessCallback,
    std::function<void()>&& aFailureCallback) {
  AssertIsOnMainThread();
  MOZ_ASSERT(
      aArgs.type() !=
          ServiceWorkerOpArgs::TParentToChildServiceWorkerFetchEventOpArgs,
      "FetchEvent operations should be sent through FetchEventOp(Proxy) "
      "actors!");
  MOZ_ASSERT(aSuccessCallback);

  nsresult rv = SpawnWorkerIfNeeded();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aFailureCallback();
    return rv;
  }

  MOZ_ASSERT(mControllerChild);

  RefPtr<ServiceWorkerPrivate> self = this;
  RefPtr<RAIIActorPtrHolder> holder = mControllerChild;
  RefPtr<KeepAliveToken> token =
      aArgs.type() == ServiceWorkerOpArgs::TServiceWorkerTerminateWorkerOpArgs
          ? nullptr
          : CreateEventKeepAliveToken();

  /**
   * NOTE: moving `aArgs` won't do anything until IPDL `SendMethod()` methods
   * can accept rvalue references rather than just const references.
   */
  mControllerChild->get()->SendExecServiceWorkerOp(aArgs)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = std::move(self), holder = std::move(holder),
       token = std::move(token), onSuccess = std::move(aSuccessCallback),
       onFailure = std::move(aFailureCallback)](
          PRemoteWorkerControllerChild::ExecServiceWorkerOpPromise::
              ResolveOrRejectValue&& aResult) {
        if (NS_WARN_IF(aResult.IsReject())) {
          onFailure();
          return;
        }

        onSuccess(std::move(aResult.ResolveValue()));
      });

  return NS_OK;
}

}  // namespace mozilla::dom
