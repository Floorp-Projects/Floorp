/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerPrivateImpl.h"

#include <utility>

#include "MainThreadUtils.h"
#include "js/ErrorReport.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsICacheInfoChannel.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIInputStream.h"
#include "nsILoadInfo.h"
#include "nsINetworkInterceptController.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsIURI.h"
#include "nsIUploadChannel2.h"
#include "nsPermissionManager.h"
#include "nsThreadUtils.h"

#include "ServiceWorkerManager.h"
#include "ServiceWorkerRegistrationInfo.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Maybe.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/FetchEventOpChild.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/ReferrerInfo.h"
#include "mozilla/dom/RemoteWorkerControllerChild.h"
#include "mozilla/dom/ServiceWorkerBinding.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/IPCStreamUtils.h"

namespace mozilla {

using namespace ipc;

namespace dom {

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

nsresult GetIPCInternalRequest(nsIInterceptedChannel* aChannel,
                               IPCInternalRequest* aOutRequest,
                               UniquePtr<AutoIPCStream>& aAutoStream) {
  AssertIsOnMainThread();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetSecureUpgradedChannelURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uriNoFragment;
  rv = NS_GetURIWithoutRef(uri, getter_AddRefs(uriNoFragment));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> underlyingChannel;
  rv = aChannel->GetChannel(getter_AddRefs(underlyingChannel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(underlyingChannel);
  MOZ_ASSERT(httpChannel, "How come we don't have an HTTP channel?");

  nsCOMPtr<nsIHttpChannelInternal> internalChannel =
      do_QueryInterface(httpChannel);
  NS_ENSURE_TRUE(internalChannel, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsICacheInfoChannel> cacheInfoChannel =
      do_QueryInterface(underlyingChannel);

  nsAutoCString spec;
  rv = uriNoFragment->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString fragment;
  rv = uri->GetRef(fragment);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString method;
  rv = httpChannel->GetRequestMethod(method);
  NS_ENSURE_SUCCESS(rv, rv);

  // This is safe due to static_asserts in ServiceWorkerManager.cpp
  uint32_t cacheModeInt;
  rv = internalChannel->GetFetchCacheMode(&cacheModeInt);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  RequestCache cacheMode = static_cast<RequestCache>(cacheModeInt);

  RequestMode requestMode =
      InternalRequest::MapChannelToRequestMode(underlyingChannel);

  // This is safe due to static_asserts in ServiceWorkerManager.cpp
  uint32_t redirectMode;
  rv = internalChannel->GetRedirectMode(&redirectMode);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  RequestRedirect requestRedirect = static_cast<RequestRedirect>(redirectMode);

  RequestCredentials requestCredentials =
      InternalRequest::MapChannelToRequestCredentials(underlyingChannel);

  nsCString referrer = EmptyCString();
  uint32_t referrerPolicyInt = 0;

  nsCOMPtr<nsIReferrerInfo> referrerInfo = httpChannel->GetReferrerInfo();
  if (referrerInfo) {
    referrerPolicyInt = referrerInfo->GetReferrerPolicy();
    nsCOMPtr<nsIURI> computedReferrer = referrerInfo->GetComputedReferrer();
    if (computedReferrer) {
      rv = computedReferrer->GetSpec(referrer);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  ReferrerPolicy referrerPolicy;
  switch (referrerPolicyInt) {
    case nsIHttpChannel::REFERRER_POLICY_UNSET:
      referrerPolicy = ReferrerPolicy::_empty;
      break;
    case nsIHttpChannel::REFERRER_POLICY_NO_REFERRER:
      referrerPolicy = ReferrerPolicy::No_referrer;
      break;
    case nsIHttpChannel::REFERRER_POLICY_ORIGIN:
      referrerPolicy = ReferrerPolicy::Origin;
      break;
    case nsIHttpChannel::REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE:
      referrerPolicy = ReferrerPolicy::No_referrer_when_downgrade;
      break;
    case nsIHttpChannel::REFERRER_POLICY_ORIGIN_WHEN_XORIGIN:
      referrerPolicy = ReferrerPolicy::Origin_when_cross_origin;
      break;
    case nsIHttpChannel::REFERRER_POLICY_UNSAFE_URL:
      referrerPolicy = ReferrerPolicy::Unsafe_url;
      break;
    case nsIHttpChannel::REFERRER_POLICY_SAME_ORIGIN:
      referrerPolicy = ReferrerPolicy::Same_origin;
      break;
    case nsIHttpChannel::REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN:
      referrerPolicy = ReferrerPolicy::Strict_origin_when_cross_origin;
      break;
    case nsIHttpChannel::REFERRER_POLICY_STRICT_ORIGIN:
      referrerPolicy = ReferrerPolicy::Strict_origin;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid Referrer Policy enum value?");
      break;
  }

  uint32_t loadFlags;
  rv = underlyingChannel->GetLoadFlags(&loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoadInfo> loadInfo;
  rv = underlyingChannel->GetLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STATE(loadInfo);

  nsContentPolicyType contentPolicyType = loadInfo->InternalContentPolicyType();

  nsAutoString integrity;
  rv = internalChannel->GetIntegrityMetadata(integrity);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<HeaderFiller> headerFiller =
      MakeRefPtr<HeaderFiller>(HeadersGuardEnum::Request);
  rv = httpChannel->VisitNonDefaultRequestHeaders(headerFiller);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<InternalHeaders> internalHeaders = headerFiller->Extract();

  ErrorResult result;
  internalHeaders->SetGuard(HeadersGuardEnum::Immutable, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(httpChannel);
  nsCOMPtr<nsIInputStream> uploadStream;
  int64_t uploadStreamContentLength = -1;
  if (uploadChannel) {
    rv = uploadChannel->CloneUploadStream(&uploadStreamContentLength,
                                          getter_AddRefs(uploadStream));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  RefPtr<InternalRequest> internalRequest = new InternalRequest(
      spec, fragment, method, internalHeaders.forget(), cacheMode, requestMode,
      requestRedirect, requestCredentials, NS_ConvertUTF8toUTF16(referrer),
      referrerPolicy, contentPolicyType, integrity);
  internalRequest->SetBody(uploadStream, uploadStreamContentLength);
  internalRequest->SetCreatedByFetchEvent();

  nsAutoCString alternativeDataType;
  if (cacheInfoChannel &&
      !cacheInfoChannel->PreferredAlternativeDataTypes().IsEmpty()) {
    // TODO: the internal request probably needs all the preferred types.
    alternativeDataType.Assign(
        cacheInfoChannel->PreferredAlternativeDataTypes()[0].type());
    internalRequest->SetPreferredAlternativeDataType(alternativeDataType);
  }

  PBackgroundChild* bgChild = BackgroundChild::GetForCurrentThread();

  if (NS_WARN_IF(!bgChild)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  internalRequest->ToIPC(aOutRequest, bgChild, aAutoStream);

  return NS_OK;
}

}  // anonymous namespace

ServiceWorkerPrivateImpl::RAIIActorPtrHolder::RAIIActorPtrHolder(
    already_AddRefed<RemoteWorkerControllerChild> aActor)
    : mActor(aActor) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mActor->Manager());
}

ServiceWorkerPrivateImpl::RAIIActorPtrHolder::~RAIIActorPtrHolder() {
  AssertIsOnMainThread();

  mDestructorPromiseHolder.ResolveIfExists(true, __func__);

  mActor->MaybeSendDelete();
}

RemoteWorkerControllerChild* ServiceWorkerPrivateImpl::RAIIActorPtrHolder::
operator->() const {
  AssertIsOnMainThread();

  return get();
}

RemoteWorkerControllerChild* ServiceWorkerPrivateImpl::RAIIActorPtrHolder::get()
    const {
  AssertIsOnMainThread();

  return mActor.get();
}

RefPtr<GenericPromise>
ServiceWorkerPrivateImpl::RAIIActorPtrHolder::OnDestructor() {
  AssertIsOnMainThread();

  return mDestructorPromiseHolder.Ensure(__func__);
}

ServiceWorkerPrivateImpl::ServiceWorkerPrivateImpl(
    RefPtr<ServiceWorkerPrivate> aOuter)
    : mOuter(std::move(aOuter)) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(WorkerIsDead());
}

nsresult ServiceWorkerPrivateImpl::Initialize() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(mOuter->mInfo);

  nsCOMPtr<nsIPrincipal> principal = mOuter->mInfo->Principal();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = principal->GetURI(getter_AddRefs(uri));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!uri)) {
    return NS_ERROR_FAILURE;
  }

  URIParams baseScriptURL;
  SerializeURI(uri, baseScriptURL);

  nsString id;
  rv = mOuter->mInfo->GetId(id);

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
      swm->GetRegistration(principal, mOuter->mInfo->Scope());

  if (NS_WARN_IF(!regInfo)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCOMPtr<nsICookieSettings> cookieSettings =
      mozilla::net::CookieSettings::Create();
  MOZ_ASSERT(cookieSettings);

  StorageAccess storageAccess =
      StorageAllowedForServiceWorker(principal, cookieSettings);

  ServiceWorkerData serviceWorkerData;
  serviceWorkerData.cacheName() = mOuter->mInfo->CacheName();
  serviceWorkerData.loadFlags() =
      static_cast<uint32_t>(mOuter->mInfo->GetImportsLoadFlags() |
                            nsIChannel::LOAD_BYPASS_SERVICE_WORKER);
  serviceWorkerData.id() = std::move(id);

  mRemoteWorkerData.originalScriptURL() =
      NS_ConvertUTF8toUTF16(mOuter->mInfo->ScriptSpec());
  mRemoteWorkerData.baseScriptURL() = baseScriptURL;
  mRemoteWorkerData.resolvedScriptURL() = baseScriptURL;
  mRemoteWorkerData.name() = VoidString();

  mRemoteWorkerData.loadingPrincipalInfo() = principalInfo;
  mRemoteWorkerData.principalInfo() = principalInfo;
  // storagePrincipalInfo for ServiceWorkers is equal to principalInfo because,
  // at the moment, ServiceWorkers are not exposed in partitioned contexts.
  mRemoteWorkerData.storagePrincipalInfo() = principalInfo;

  rv = uri->GetHost(mRemoteWorkerData.domain());
  NS_ENSURE_SUCCESS(rv, rv);
  mRemoteWorkerData.isSecureContext() = true;
  mRemoteWorkerData.referrerInfo() = MakeAndAddRef<ReferrerInfo>();
  mRemoteWorkerData.storageAccess() = storageAccess;
  mRemoteWorkerData.serviceWorkerData() = std::move(serviceWorkerData);

  // This fills in the rest of mRemoteWorkerData.serviceWorkerData().
  rv = RefreshRemoteWorkerData(regInfo);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

RefPtr<GenericPromise> ServiceWorkerPrivateImpl::SetSkipWaitingFlag() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(mOuter->mInfo);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

  if (!swm) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  RefPtr<ServiceWorkerRegistrationInfo> regInfo =
      swm->GetRegistration(mOuter->mInfo->Principal(), mOuter->mInfo->Scope());

  if (!regInfo) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  mOuter->mInfo->SetSkipWaitingFlag();

  RefPtr<GenericPromise::Private> promise =
      new GenericPromise::Private(__func__);

  regInfo->TryToActivateAsync([promise] { promise->Resolve(true, __func__); });

  return promise;
}

nsresult ServiceWorkerPrivateImpl::RefreshRemoteWorkerData(
    const RefPtr<ServiceWorkerRegistrationInfo>& aRegistration) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(mOuter->mInfo);

  nsTArray<KeyAndPermissions> permissions;
  nsCOMPtr<nsIPermissionManager> permManager = services::GetPermissionManager();
  nsTArray<nsCString> keys =
      nsPermissionManager::GetAllKeysForPrincipal(mOuter->mInfo->Principal());
  MOZ_ASSERT(!keys.IsEmpty());

  for (auto& key : keys) {
    nsTArray<IPC::Permission> perms;
    nsresult rv = permManager->GetPermissionsWithKey(key, perms);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    KeyAndPermissions kp;
    kp.key() = nsCString(key);
    kp.permissions() = std::move(perms);

    permissions.AppendElement(std::move(kp));
  }

  MOZ_ASSERT(!permissions.IsEmpty());

  ServiceWorkerData& serviceWorkerData =
      mRemoteWorkerData.serviceWorkerData().get_ServiceWorkerData();
  serviceWorkerData.permissionsByKey() = std::move(permissions);
  serviceWorkerData.descriptor() = mOuter->mInfo->Descriptor().ToIPC();
  serviceWorkerData.registrationDescriptor() =
      aRegistration->Descriptor().ToIPC();

  return NS_OK;
}

nsresult ServiceWorkerPrivateImpl::SpawnWorkerIfNeeded() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(mOuter->mInfo);

  if (mControllerChild) {
    return NS_OK;
  }

  PBackgroundChild* bgChild = BackgroundChild::GetForCurrentThread();

  if (NS_WARN_IF(!bgChild)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

  if (NS_WARN_IF(!swm)) {
    return NS_ERROR_DOM_ABORT_ERR;
  }

  RefPtr<ServiceWorkerRegistrationInfo> regInfo =
      swm->GetRegistration(mOuter->mInfo->Principal(), mOuter->mInfo->Scope());

  if (NS_WARN_IF(!regInfo)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsresult rv = RefreshRemoteWorkerData(regInfo);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<RemoteWorkerControllerChild> controllerChild =
      new RemoteWorkerControllerChild(this);

  if (NS_WARN_IF(!bgChild->SendPRemoteWorkerControllerConstructor(
          controllerChild, mRemoteWorkerData))) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  /**
   * Manutally `AddRef()` because `DeallocPRemoteWorkerControllerChild()`
   * calls `Release()` and the `AllocPRemoteWorkerControllerChild()` function
   * is not called.
   */
  // NOLINTNEXTLINE(readability-redundant-smartptr-get)
  controllerChild.get()->AddRef();

  mControllerChild = new RAIIActorPtrHolder(controllerChild.forget());

  return NS_OK;
}

ServiceWorkerPrivateImpl::~ServiceWorkerPrivateImpl() {
  AssertIsOnMainThread();
  MOZ_ASSERT(!mOuter);
  MOZ_ASSERT(WorkerIsDead());
}

nsresult ServiceWorkerPrivateImpl::SendMessageEvent(
    RefPtr<ServiceWorkerCloneData>&& aData,
    const ClientInfoAndState& aClientInfoAndState) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(aData);

  auto scopeExit = MakeScopeExit([&] { Shutdown(); });

  PBackgroundChild* bgChild = BackgroundChild::GetForCurrentThread();

  if (NS_WARN_IF(!bgChild)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  ServiceWorkerMessageEventOpArgs args;
  args.clientInfoAndState() = aClientInfoAndState;
  if (!aData->BuildClonedMessageDataForBackgroundChild(bgChild,
                                                       args.clonedData())) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  scopeExit.release();

  return ExecServiceWorkerOp(
      std::move(args), [](ServiceWorkerOpResult&& aResult) {
        MOZ_ASSERT(aResult.type() == ServiceWorkerOpResult::Tnsresult);
      });
}

nsresult ServiceWorkerPrivateImpl::CheckScriptEvaluation(
    RefPtr<LifeCycleEventCallback> aCallback) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(aCallback);

  RefPtr<ServiceWorkerPrivateImpl> self = this;

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
            if (self->mOuter) {
              self->mOuter->SetHandlesFetch(result.fetchHandlerWasAdded());
            }

            Unused << NS_WARN_IF(!self->mOuter);

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
              GetCurrentThreadSerialEventTarget(), __func__,
              [callback = std::move(callback)](
                  const GenericPromise::ResolveOrRejectValue&) {
                callback->SetResult(false);
                callback->Run();
              });

          return;
        }

        RefPtr<GenericNonExclusivePromise> promise = self->ShutdownInternal();

        RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
        MOZ_ASSERT(swm);

        swm->BlockShutdownOn(promise);

        promise->Then(
            GetCurrentThreadSerialEventTarget(), __func__,
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

nsresult ServiceWorkerPrivateImpl::SendLifeCycleEvent(
    const nsAString& aEventName, RefPtr<LifeCycleEventCallback> aCallback) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(aCallback);

  return ExecServiceWorkerOp(
      ServiceWorkerLifeCycleEventOpArgs(nsString(aEventName)),
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

nsresult ServiceWorkerPrivateImpl::SendPushEvent(
    RefPtr<ServiceWorkerRegistrationInfo> aRegistration,
    const nsAString& aMessageId, const Maybe<nsTArray<uint8_t>>& aData) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(aRegistration);

  ServiceWorkerPushEventOpArgs args;
  args.messageId() = nsString(aMessageId);

  if (aData) {
    args.data() = aData.ref();
  } else {
    args.data() = void_t();
  }

  if (mOuter->mInfo->State() == ServiceWorkerState::Activating) {
    UniquePtr<PendingFunctionalEvent> pendingEvent =
        MakeUnique<PendingPushEvent>(this, std::move(aRegistration),
                                     std::move(args));

    mPendingFunctionalEvents.AppendElement(std::move(pendingEvent));

    return NS_OK;
  }

  MOZ_ASSERT(mOuter->mInfo->State() == ServiceWorkerState::Activated);

  return SendPushEventInternal(std::move(aRegistration), std::move(args));
}

nsresult ServiceWorkerPrivateImpl::SendPushEventInternal(
    RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
    ServiceWorkerPushEventOpArgs&& aArgs) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
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

nsresult ServiceWorkerPrivateImpl::SendPushSubscriptionChangeEvent() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);

  return ExecServiceWorkerOp(
      ServiceWorkerPushSubscriptionChangeEventOpArgs(),
      [](ServiceWorkerOpResult&& aResult) {
        MOZ_ASSERT(aResult.type() == ServiceWorkerOpResult::Tnsresult);
      });
}

nsresult ServiceWorkerPrivateImpl::SendNotificationEvent(
    const nsAString& aEventName, const nsAString& aID, const nsAString& aTitle,
    const nsAString& aDir, const nsAString& aLang, const nsAString& aBody,
    const nsAString& aTag, const nsAString& aIcon, const nsAString& aData,
    const nsAString& aBehavior, const nsAString& aScope,
    uint32_t aDisableOpenClickDelay) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);

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
  args.disableOpenClickDelay() = aDisableOpenClickDelay;

  return ExecServiceWorkerOp(
      std::move(args), [](ServiceWorkerOpResult&& aResult) {
        MOZ_ASSERT(aResult.type() == ServiceWorkerOpResult::Tnsresult);
      });
}

ServiceWorkerPrivateImpl::PendingFunctionalEvent::PendingFunctionalEvent(
    ServiceWorkerPrivateImpl* aOwner,
    RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration)
    : mOwner(aOwner), mRegistration(std::move(aRegistration)) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOwner);
  MOZ_ASSERT(mOwner->mOuter);
  MOZ_ASSERT(mOwner->mOuter->mInfo);
  MOZ_ASSERT(mOwner->mOuter->mInfo->State() == ServiceWorkerState::Activating);
  MOZ_ASSERT(mRegistration);
}

ServiceWorkerPrivateImpl::PendingFunctionalEvent::~PendingFunctionalEvent() {
  AssertIsOnMainThread();
}

ServiceWorkerPrivateImpl::PendingPushEvent::PendingPushEvent(
    ServiceWorkerPrivateImpl* aOwner,
    RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
    ServiceWorkerPushEventOpArgs&& aArgs)
    : PendingFunctionalEvent(aOwner, std::move(aRegistration)),
      mArgs(std::move(aArgs)) {
  AssertIsOnMainThread();
}

nsresult ServiceWorkerPrivateImpl::PendingPushEvent::Send() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOwner->mOuter);
  MOZ_ASSERT(mOwner->mOuter->mInfo);

  return mOwner->SendPushEventInternal(std::move(mRegistration),
                                       std::move(mArgs));
}

ServiceWorkerPrivateImpl::PendingFetchEvent::PendingFetchEvent(
    ServiceWorkerPrivateImpl* aOwner,
    RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
    ServiceWorkerFetchEventOpArgs&& aArgs,
    nsCOMPtr<nsIInterceptedChannel>&& aChannel,
    UniquePtr<AutoIPCStream>&& aAutoStream)
    : PendingFunctionalEvent(aOwner, std::move(aRegistration)),
      mArgs(std::move(aArgs)),
      mChannel(std::move(aChannel)),
      mAutoStream(std::move(aAutoStream)) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mChannel);
  MOZ_ASSERT(mAutoStream);
}

nsresult ServiceWorkerPrivateImpl::PendingFetchEvent::Send() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOwner->mOuter);
  MOZ_ASSERT(mOwner->mOuter->mInfo);

  return mOwner->SendFetchEventInternal(std::move(mRegistration),
                                        std::move(mArgs), std::move(mChannel),
                                        std::move(mAutoStream));
}

ServiceWorkerPrivateImpl::PendingFetchEvent::~PendingFetchEvent() {
  AssertIsOnMainThread();

  if (NS_WARN_IF(mChannel)) {
    mChannel->CancelInterception(NS_ERROR_INTERCEPTION_FAILED);
  }
}

nsresult ServiceWorkerPrivateImpl::SendFetchEvent(
    RefPtr<ServiceWorkerRegistrationInfo> aRegistration,
    nsCOMPtr<nsIInterceptedChannel> aChannel, const nsAString& aClientId,
    const nsAString& aResultingClientId, bool aIsReload) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(aRegistration);
  MOZ_ASSERT(aChannel);

  auto scopeExit = MakeScopeExit([&] {
    aChannel->CancelInterception(NS_ERROR_INTERCEPTION_FAILED);
    Shutdown();
  });

  nsCOMPtr<nsIChannel> channel;
  nsresult rv = aChannel->GetChannel(getter_AddRefs(channel));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  IPCInternalRequest internalRequest;
  UniquePtr<AutoIPCStream> autoStream = MakeUnique<AutoIPCStream>();
  rv = GetIPCInternalRequest(aChannel, &internalRequest, autoStream);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  scopeExit.release();

  MOZ_ASSERT(mOuter->mInfo);

  ServiceWorkerFetchEventOpArgs args(
      mOuter->mInfo->ScriptSpec(), internalRequest, nsString(aClientId),
      nsString(aResultingClientId), aIsReload,
      nsContentUtils::IsNonSubresourceRequest(channel));

  if (mOuter->mInfo->State() == ServiceWorkerState::Activating) {
    UniquePtr<PendingFunctionalEvent> pendingEvent =
        MakeUnique<PendingFetchEvent>(this, std::move(aRegistration),
                                      std::move(args), std::move(aChannel),
                                      std::move(autoStream));

    mPendingFunctionalEvents.AppendElement(std::move(pendingEvent));

    return NS_OK;
  }

  MOZ_ASSERT(mOuter->mInfo->State() == ServiceWorkerState::Activated);

  return SendFetchEventInternal(std::move(aRegistration), std::move(args),
                                std::move(aChannel), std::move(autoStream));
}

nsresult ServiceWorkerPrivateImpl::SendFetchEventInternal(
    RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
    ServiceWorkerFetchEventOpArgs&& aArgs,
    nsCOMPtr<nsIInterceptedChannel>&& aChannel,
    UniquePtr<AutoIPCStream>&& aAutoStream) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);

  auto scopeExit = MakeScopeExit([&] { Shutdown(); });

  if (NS_WARN_IF(!mOuter->mInfo)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsresult rv = SpawnWorkerIfNeeded();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  scopeExit.release();

  MOZ_ASSERT(mControllerChild);

  RefPtr<RAIIActorPtrHolder> holder = mControllerChild;

  FetchEventOpChild::Create(mControllerChild->get(), std::move(aArgs),
                            std::move(aChannel), std::move(aRegistration),
                            mOuter->CreateEventKeepAliveToken())
      ->Then(GetCurrentThreadSerialEventTarget(), __func__,
             [holder = std::move(holder)](
                 const GenericPromise::ResolveOrRejectValue& aResult) {
               Unused << NS_WARN_IF(aResult.IsReject());
             });

  aAutoStream->TakeOptionalValue();

  return NS_OK;
}

void ServiceWorkerPrivateImpl::TerminateWorker() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);

  mOuter->mIdleWorkerTimer->Cancel();
  mOuter->mIdleKeepAliveToken = nullptr;

  Shutdown();
}

void ServiceWorkerPrivateImpl::Shutdown() {
  AssertIsOnMainThread();

  if (!WorkerIsDead()) {
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

    MOZ_ASSERT(swm,
               "All Service Workers should start shutting down before the "
               "ServiceWorkerManager does!");

    RefPtr<GenericNonExclusivePromise> promise = ShutdownInternal();
    swm->BlockShutdownOn(promise);
  }

  MOZ_ASSERT(WorkerIsDead());
}

RefPtr<GenericNonExclusivePromise>
ServiceWorkerPrivateImpl::ShutdownInternal() {
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
      ServiceWorkerTerminateWorkerOpArgs(),
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

  return promise;
}

void ServiceWorkerPrivateImpl::UpdateState(ServiceWorkerState aState) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);

  if (WorkerIsDead()) {
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

void ServiceWorkerPrivateImpl::NoteDeadOuter() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);

  Shutdown();
  mOuter = nullptr;
}

void ServiceWorkerPrivateImpl::CreationFailed() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(mControllerChild);

  Shutdown();
}

void ServiceWorkerPrivateImpl::CreationSucceeded() {
  AssertIsOnMainThread();
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(mControllerChild);

  mOuter->RenewKeepAliveToken(ServiceWorkerPrivate::WakeUpReason::Unknown);
}

void ServiceWorkerPrivateImpl::ErrorReceived(const ErrorValue& aError) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(mOuter->mInfo);
  MOZ_ASSERT(mControllerChild);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);

  ServiceWorkerInfo* info = mOuter->mInfo;

  swm->HandleError(nullptr, info->Principal(), info->Scope(),
                   NS_ConvertUTF8toUTF16(info->ScriptSpec()), EmptyString(),
                   EmptyString(), EmptyString(), 0, 0, JSREPORT_ERROR,
                   JSEXN_ERR);
}

void ServiceWorkerPrivateImpl::Terminated() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(mControllerChild);

  Shutdown();
}

bool ServiceWorkerPrivateImpl::WorkerIsDead() const {
  AssertIsOnMainThread();

  return !mControllerChild;
}

nsresult ServiceWorkerPrivateImpl::ExecServiceWorkerOp(
    ServiceWorkerOpArgs&& aArgs,
    std::function<void(ServiceWorkerOpResult&&)>&& aSuccessCallback,
    std::function<void()>&& aFailureCallback) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mOuter);
  MOZ_ASSERT(
      aArgs.type() != ServiceWorkerOpArgs::TServiceWorkerFetchEventOpArgs,
      "FetchEvent operations should be sent through FetchEventOp(Proxy) "
      "actors!");
  MOZ_ASSERT(aSuccessCallback);

  nsresult rv = SpawnWorkerIfNeeded();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aFailureCallback();
    return rv;
  }

  MOZ_ASSERT(mControllerChild);

  RefPtr<ServiceWorkerPrivateImpl> self = this;
  RefPtr<RAIIActorPtrHolder> holder = mControllerChild;
  RefPtr<KeepAliveToken> token =
      aArgs.type() == ServiceWorkerOpArgs::TServiceWorkerTerminateWorkerOpArgs
          ? nullptr
          : mOuter->CreateEventKeepAliveToken();

  /**
   * NOTE: moving `aArgs` won't do anything until IPDL `SendMethod()` methods
   * can accept rvalue references rather than just const references.
   */
  mControllerChild->get()->SendExecServiceWorkerOp(aArgs)->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
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

}  // namespace dom
}  // namespace mozilla
