/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchEventOpChild.h"

#include <utility>

#include "MainThreadUtils.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIChannel.h"
#include "nsIConsoleReportCollector.h"
#include "nsIContentPolicy.h"
#include "nsIInputStream.h"
#include "nsILoadInfo.h"
#include "nsINetworkInterceptController.h"
#include "nsIObserverService.h"
#include "nsIScriptError.h"
#include "nsISupportsImpl.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

#include "ServiceWorkerPrivate.h"
#include "mozilla/Assertions.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/PRemoteWorkerControllerChild.h"
#include "mozilla/dom/ServiceWorkerRegistrationInfo.h"
#include "mozilla/net/NeckoChannelParams.h"

namespace mozilla {
namespace dom {

namespace {

bool CSPPermitsResponse(nsILoadInfo* aLoadInfo, InternalResponse* aResponse,
                        const nsACString& aWorkerScriptSpec) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aLoadInfo);

  nsCString url = aResponse->GetUnfilteredURL();
  if (url.IsEmpty()) {
    // Synthetic response.
    url = aWorkerScriptSpec;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), url, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  int16_t decision = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(uri, aLoadInfo, EmptyCString(), &decision);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return decision == nsIContentPolicy::ACCEPT;
}

void AsyncLog(nsIInterceptedChannel* aChannel, const nsACString& aScriptSpec,
              uint32_t aLineNumber, uint32_t aColumnNumber,
              const nsACString& aMessageName, nsTArray<nsString>&& aParams) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsIConsoleReportCollector> reporter =
      aChannel->GetConsoleReportCollector();

  if (reporter) {
    // NOTE: is appears that `const nsTArray<nsString>&` is required for
    // nsIConsoleReportCollector::AddConsoleReport to resolve to the correct
    // overload.
    const nsTArray<nsString> params = std::move(aParams);

    reporter->AddConsoleReport(
        nsIScriptError::errorFlag,
        NS_LITERAL_CSTRING("Service Worker Interception"),
        nsContentUtils::eDOM_PROPERTIES, aScriptSpec, aLineNumber,
        aColumnNumber, aMessageName, params);
  }
}

class SynthesizeResponseWatcher final : public nsIInterceptedBodyCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  SynthesizeResponseWatcher(
      const nsMainThreadPtrHandle<nsIInterceptedChannel>& aInterceptedChannel,
      const nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
      const bool aIsNonSubresourceRequest,
      FetchEventRespondWithClosure&& aClosure, nsAString&& aRequestURL)
      : mInterceptedChannel(aInterceptedChannel),
        mRegistration(aRegistration),
        mIsNonSubresourceRequest(aIsNonSubresourceRequest),
        mClosure(std::move(aClosure)),
        mRequestURL(std::move(aRequestURL)) {
    AssertIsOnMainThread();
    MOZ_ASSERT(mInterceptedChannel);
    MOZ_ASSERT(mRegistration);
  }

  NS_IMETHOD
  BodyComplete(nsresult aRv) override {
    AssertIsOnMainThread();
    MOZ_ASSERT(mInterceptedChannel);

    if (NS_WARN_IF(NS_FAILED(aRv))) {
      AsyncLog(mInterceptedChannel, mClosure.respondWithScriptSpec(),
               mClosure.respondWithLineNumber(),
               mClosure.respondWithColumnNumber(),
               NS_LITERAL_CSTRING("InterceptionFailedWithURL"), {mRequestURL});

      CancelInterception(NS_ERROR_INTERCEPTION_FAILED);

      return NS_OK;
    }

    nsresult rv = mInterceptedChannel->FinishSynthesizedResponse();

    if (NS_WARN_IF(NS_FAILED(rv))) {
      CancelInterception(rv);
    }

    mInterceptedChannel = nullptr;

    return NS_OK;
  }

  // See FetchEventOpChild::MaybeScheduleRegistrationUpdate() for comments.
  void CancelInterception(nsresult aStatus) {
    AssertIsOnMainThread();
    MOZ_ASSERT(mInterceptedChannel);
    MOZ_ASSERT(mRegistration);

    mInterceptedChannel->CancelInterception(aStatus);

    if (mIsNonSubresourceRequest) {
      mRegistration->MaybeScheduleUpdate();
    } else {
      mRegistration->MaybeScheduleTimeCheckAndUpdate();
    }

    mInterceptedChannel = nullptr;
    mRegistration = nullptr;
  }

 private:
  ~SynthesizeResponseWatcher() {
    if (NS_WARN_IF(mInterceptedChannel)) {
      CancelInterception(NS_ERROR_DOM_ABORT_ERR);
    }
  }

  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;
  const bool mIsNonSubresourceRequest;
  const FetchEventRespondWithClosure mClosure;
  const nsString mRequestURL;
};

NS_IMPL_ISUPPORTS(SynthesizeResponseWatcher, nsIInterceptedBodyCallback)

}  // anonymous namespace

/* static */ RefPtr<GenericPromise> FetchEventOpChild::Create(
    PRemoteWorkerControllerChild* aManager,
    ServiceWorkerFetchEventOpArgs&& aArgs,
    nsCOMPtr<nsIInterceptedChannel> aInterceptedChannel,
    RefPtr<ServiceWorkerRegistrationInfo> aRegistration,
    RefPtr<KeepAliveToken>&& aKeepAliveToken) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aInterceptedChannel);
  MOZ_ASSERT(aKeepAliveToken);

  FetchEventOpChild* actor = new FetchEventOpChild(
      std::move(aArgs), std::move(aInterceptedChannel),
      std::move(aRegistration), std::move(aKeepAliveToken));
  Unused << aManager->SendPFetchEventOpConstructor(actor, actor->mArgs);
  return actor->mPromiseHolder.Ensure(__func__);
}

FetchEventOpChild::~FetchEventOpChild() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mInterceptedChannelHandled);
  MOZ_DIAGNOSTIC_ASSERT(mPromiseHolder.IsEmpty());
}

FetchEventOpChild::FetchEventOpChild(
    ServiceWorkerFetchEventOpArgs&& aArgs,
    nsCOMPtr<nsIInterceptedChannel>&& aInterceptedChannel,
    RefPtr<ServiceWorkerRegistrationInfo>&& aRegistration,
    RefPtr<KeepAliveToken>&& aKeepAliveToken)
    : mArgs(std::move(aArgs)),
      mInterceptedChannel(std::move(aInterceptedChannel)),
      mRegistration(std::move(aRegistration)),
      mKeepAliveToken(std::move(aKeepAliveToken)) {}

mozilla::ipc::IPCResult FetchEventOpChild::RecvAsyncLog(
    const nsCString& aScriptSpec, const uint32_t& aLineNumber,
    const uint32_t& aColumnNumber, const nsCString& aMessageName,
    nsTArray<nsString>&& aParams) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mInterceptedChannel);

  AsyncLog(mInterceptedChannel, aScriptSpec, aLineNumber, aColumnNumber,
           aMessageName, std::move(aParams));

  return IPC_OK();
}

mozilla::ipc::IPCResult FetchEventOpChild::RecvRespondWith(
    IPCFetchEventRespondWithResult&& aResult) {
  AssertIsOnMainThread();

  switch (aResult.type()) {
    case IPCFetchEventRespondWithResult::TIPCSynthesizeResponseArgs:
      SynthesizeResponse(std::move(aResult.get_IPCSynthesizeResponseArgs()));
      break;
    case IPCFetchEventRespondWithResult::TResetInterceptionArgs:
      ResetInterception();
      break;
    case IPCFetchEventRespondWithResult::TCancelInterceptionArgs:
      CancelInterception(aResult.get_CancelInterceptionArgs().status());
      break;
    default:
      MOZ_CRASH("Unknown IPCFetchEventRespondWithResult type!");
      break;
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult FetchEventOpChild::Recv__delete__(
    const ServiceWorkerFetchEventOpResult& aResult) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mRegistration);

  if (NS_WARN_IF(!mInterceptedChannelHandled)) {
    MOZ_ASSERT(NS_FAILED(aResult.rv()));
    NS_WARNING(
        "Failed to handle intercepted network request; canceling "
        "interception!");

    CancelInterception(aResult.rv());
  }

  mPromiseHolder.ResolveIfExists(true, __func__);

  /**
   * This corresponds to the "Fire Functional Event" algorithm's step 9:
   *
   * "If the time difference in seconds calculated by the current time minus
   * registration's last update check time is greater than 84600, invoke Soft
   * Update algorithm with registration."
   *
   * TODO: this is probably being called later than it should be; it should be
   * called ASAP after dispatching the FetchEvent.
   */
  mRegistration->MaybeScheduleTimeCheckAndUpdate();

  return IPC_OK();
}

void FetchEventOpChild::ActorDestroy(ActorDestroyReason) {
  AssertIsOnMainThread();

  // If `Recv__delete__` was called, it would have resolved the promise already.
  mPromiseHolder.RejectIfExists(NS_ERROR_DOM_ABORT_ERR, __func__);

  if (NS_WARN_IF(!mInterceptedChannelHandled)) {
    Unused << Recv__delete__(NS_ERROR_DOM_ABORT_ERR);
  }
}

nsresult FetchEventOpChild::StartSynthesizedResponse(
    IPCSynthesizeResponseArgs&& aArgs) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mInterceptedChannel);
  MOZ_ASSERT(!mInterceptedChannelHandled);
  MOZ_ASSERT(mRegistration);

  /**
   * TODO: moving the IPCInternalResponse won't do anything right now because
   * there isn't a prefect-forwarding or rvalue-ref-parameter overload of
   * `InternalResponse::FromIPC().`
   */
  RefPtr<InternalResponse> response =
      InternalResponse::FromIPC(aArgs.internalResponse());
  if (NS_WARN_IF(!response)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIChannel> underlyingChannel;
  nsresult rv =
      mInterceptedChannel->GetChannel(getter_AddRefs(underlyingChannel));
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(!underlyingChannel)) {
    return NS_FAILED(rv) ? rv : NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = underlyingChannel->LoadInfo();
  if (!CSPPermitsResponse(loadInfo, response, mArgs.workerScriptSpec())) {
    return NS_ERROR_CONTENT_BLOCKED;
  }

  MOZ_ASSERT(response->GetChannelInfo().IsInitialized());
  ChannelInfo channelInfo = response->GetChannelInfo();
  rv = mInterceptedChannel->SetChannelInfo(&channelInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_INTERCEPTION_FAILED;
  }

  rv = mInterceptedChannel->SynthesizeStatus(
      response->GetUnfilteredStatus(), response->GetUnfilteredStatusText());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoTArray<InternalHeaders::Entry, 5> entries;
  response->UnfilteredHeaders()->GetEntries(entries);
  for (auto& entry : entries) {
    mInterceptedChannel->SynthesizeHeader(entry.mName, entry.mValue);
  }

  auto castLoadInfo = static_cast<mozilla::net::LoadInfo*>(loadInfo.get());
  castLoadInfo->SynthesizeServiceWorkerTainting(response->GetTainting());

  // Get the preferred alternative data type of the outer channel
  nsAutoCString preferredAltDataType(EmptyCString());
  nsCOMPtr<nsICacheInfoChannel> outerChannel =
      do_QueryInterface(underlyingChannel);
  if (outerChannel &&
      !outerChannel->PreferredAlternativeDataTypes().IsEmpty()) {
    preferredAltDataType.Assign(
        outerChannel->PreferredAlternativeDataTypes()[0].type());
  }

  nsCOMPtr<nsIInputStream> body;
  if (preferredAltDataType.Equals(response->GetAlternativeDataType())) {
    body = response->TakeAlternativeBody();
  }
  if (!body) {
    response->GetUnfilteredBody(getter_AddRefs(body));
  } else {
    Telemetry::ScalarAdd(Telemetry::ScalarID::SW_ALTERNATIVE_BODY_USED_COUNT,
                         1);
  }

  // Propagate the URL to the content if the request mode is not "navigate".
  // Note that, we only reflect the final URL if the response.redirected is
  // false. We propagate all the URLs if the response.redirected is true.
  const IPCInternalRequest& request = mArgs.internalRequest();
  nsAutoCString responseURL;
  if (request.requestMode() != RequestMode::Navigate) {
    responseURL = response->GetUnfilteredURL();

    // Similar to how we apply the request fragment to redirects automatically
    // we also want to apply it automatically when propagating the response
    // URL from a service worker interception.  Currently response.url strips
    // the fragment, so this will never conflict with an existing fragment
    // on the response.  In the future we will have to check for a response
    // fragment and avoid overriding in that case.
    if (!request.fragment().IsEmpty() && !responseURL.IsEmpty()) {
      MOZ_ASSERT(!responseURL.Contains('#'));
      responseURL.AppendLiteral("#");
      responseURL.Append(request.fragment());
    }
  }

  nsMainThreadPtrHandle<nsIInterceptedChannel> interceptedChannel(
      new nsMainThreadPtrHolder<nsIInterceptedChannel>(
          "nsIInterceptedChannel", mInterceptedChannel, false));

  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> registration(
      new nsMainThreadPtrHolder<ServiceWorkerRegistrationInfo>(
          "ServiceWorkerRegistrationInfo", mRegistration, false));

  nsCString requestURL = request.urlList().LastElement();
  if (!request.fragment().IsEmpty()) {
    requestURL.AppendLiteral("#");
    requestURL.Append(request.fragment());
  }

  RefPtr<SynthesizeResponseWatcher> watcher = new SynthesizeResponseWatcher(
      interceptedChannel, registration, mArgs.isNonSubresourceRequest(),
      std::move(aArgs.closure()), NS_ConvertUTF8toUTF16(responseURL));

  rv = mInterceptedChannel->StartSynthesizedResponse(
      body, watcher, nullptr /* TODO */, responseURL, response->IsRedirected());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(underlyingChannel,
                                "service-worker-synthesized-response", nullptr);
  }

  return rv;
}

void FetchEventOpChild::SynthesizeResponse(IPCSynthesizeResponseArgs&& aArgs) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mInterceptedChannel);
  MOZ_ASSERT(!mInterceptedChannelHandled);

  nsresult rv = StartSynthesizedResponse(std::move(aArgs));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    NS_WARNING("Failed to synthesize response!");

    mInterceptedChannel->CancelInterception(rv);
  }

  mInterceptedChannelHandled = true;

  MaybeScheduleRegistrationUpdate();
}

void FetchEventOpChild::ResetInterception() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mInterceptedChannel);
  MOZ_ASSERT(!mInterceptedChannelHandled);

  nsresult rv = mInterceptedChannel->ResetInterception();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    NS_WARNING("Failed to resume intercepted network request!");

    mInterceptedChannel->CancelInterception(rv);
  }

  mInterceptedChannelHandled = true;

  MaybeScheduleRegistrationUpdate();
}

void FetchEventOpChild::CancelInterception(nsresult aStatus) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mInterceptedChannel);
  MOZ_ASSERT(!mInterceptedChannelHandled);
  MOZ_ASSERT(NS_FAILED(aStatus));

  mInterceptedChannel->CancelInterception(aStatus);
  mInterceptedChannelHandled = true;

  MaybeScheduleRegistrationUpdate();
}

/**
 * This corresponds to the "Handle Fetch" algorithm's steps 20.3, 21.2, and
 * 22.2:
 *
 * "If request is a non-subresource request, or request is a subresource
 * request and the time difference in seconds calculated by the current time
 * minus registration's last update check time is greater than 86400, invoke
 * Soft Update algorithm with registration."
 */
void FetchEventOpChild::MaybeScheduleRegistrationUpdate() const {
  AssertIsOnMainThread();
  MOZ_ASSERT(mRegistration);
  MOZ_ASSERT(mInterceptedChannelHandled);

  if (mArgs.isNonSubresourceRequest()) {
    mRegistration->MaybeScheduleUpdate();
  } else {
    mRegistration->MaybeScheduleTimeCheckAndUpdate();
  }
}

}  // namespace dom
}  // namespace mozilla
