/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerEvents.h"

#include <utility>

#include "ServiceWorker.h"
#include "ServiceWorkerManager.h"
#include "js/Conversions.h"
#include "js/Exception.h"  // JS::ExceptionStack, JS::StealPendingExceptionStack
#include "js/TypeDecls.h"
#include "mozilla/Encoding.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BodyUtil.h"
#include "mozilla/dom/Client.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/FetchEventBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/PushEventBinding.h"
#include "mozilla/dom/PushMessageDataBinding.h"
#include "mozilla/dom/PushUtil.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ServiceWorkerOp.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/Telemetry.h"
#include "nsComponentManagerUtils.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsIConsoleReportCollector.h"
#include "nsINetworkInterceptController.h"
#include "nsIScriptError.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "nsSerializationHelper.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "xpcpublic.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace {

void AsyncLog(nsIInterceptedChannel* aInterceptedChannel,
              const nsACString& aRespondWithScriptSpec,
              uint32_t aRespondWithLineNumber,
              uint32_t aRespondWithColumnNumber, const nsACString& aMessageName,
              const nsTArray<nsString>& aParams) {
  MOZ_ASSERT(aInterceptedChannel);
  nsCOMPtr<nsIConsoleReportCollector> reporter =
      aInterceptedChannel->GetConsoleReportCollector();
  if (reporter) {
    reporter->AddConsoleReport(nsIScriptError::errorFlag,
                               "Service Worker Interception"_ns,
                               nsContentUtils::eDOM_PROPERTIES,
                               aRespondWithScriptSpec, aRespondWithLineNumber,
                               aRespondWithColumnNumber, aMessageName, aParams);
  }
}

template <typename... Params>
void AsyncLog(nsIInterceptedChannel* aInterceptedChannel,
              const nsACString& aRespondWithScriptSpec,
              uint32_t aRespondWithLineNumber,
              uint32_t aRespondWithColumnNumber,
              // We have to list one explicit string so that calls with an
              // nsTArray of params won't end up in here.
              const nsACString& aMessageName, const nsAString& aFirstParam,
              Params&&... aParams) {
  nsTArray<nsString> paramsList(sizeof...(Params) + 1);
  StringArrayAppender::Append(paramsList, sizeof...(Params) + 1, aFirstParam,
                              std::forward<Params>(aParams)...);
  AsyncLog(aInterceptedChannel, aRespondWithScriptSpec, aRespondWithLineNumber,
           aRespondWithColumnNumber, aMessageName, paramsList);
}

}  // anonymous namespace

namespace mozilla::dom {

CancelChannelRunnable::CancelChannelRunnable(
    nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
    nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
    nsresult aStatus)
    : Runnable("dom::CancelChannelRunnable"),
      mChannel(aChannel),
      mRegistration(aRegistration),
      mStatus(aStatus) {}

NS_IMETHODIMP
CancelChannelRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  mChannel->CancelInterception(mStatus);
  mRegistration->MaybeScheduleUpdate();
  return NS_OK;
}

FetchEvent::FetchEvent(EventTarget* aOwner)
    : ExtendableEvent(aOwner),
      mPreventDefaultLineNumber(0),
      mPreventDefaultColumnNumber(1),
      mWaitToRespond(false) {}

FetchEvent::~FetchEvent() = default;

void FetchEvent::PostInit(
    nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
    nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
    const nsACString& aScriptSpec) {
  mChannel = aChannel;
  mRegistration = aRegistration;
  mScriptSpec.Assign(aScriptSpec);
}

void FetchEvent::PostInit(const nsACString& aScriptSpec,
                          RefPtr<FetchEventOp> aRespondWithHandler) {
  MOZ_ASSERT(aRespondWithHandler);

  mScriptSpec.Assign(aScriptSpec);
  mRespondWithHandler = std::move(aRespondWithHandler);
}

/*static*/
already_AddRefed<FetchEvent> FetchEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const FetchEventInit& aOptions) {
  RefPtr<EventTarget> owner = do_QueryObject(aGlobal.GetAsSupports());
  MOZ_ASSERT(owner);
  RefPtr<FetchEvent> e = new FetchEvent(owner);
  bool trusted = e->Init(owner);
  e->InitEvent(aType, aOptions.mBubbles, aOptions.mCancelable);
  e->SetTrusted(trusted);
  e->SetComposed(aOptions.mComposed);
  e->mRequest = aOptions.mRequest;
  e->mClientId = aOptions.mClientId;
  e->mResultingClientId = aOptions.mResultingClientId;
  RefPtr<nsIGlobalObject> global = do_QueryObject(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);
  ErrorResult rv;
  e->mHandled = Promise::Create(global, rv);
  if (rv.Failed()) {
    rv.SuppressException();
    return nullptr;
  }
  e->mPreloadResponse = Promise::Create(global, rv);
  if (rv.Failed()) {
    rv.SuppressException();
    return nullptr;
  }
  return e.forget();
}

namespace {

struct RespondWithClosure {
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;
  const nsString mRequestURL;
  const nsCString mRespondWithScriptSpec;
  const uint32_t mRespondWithLineNumber;
  const uint32_t mRespondWithColumnNumber;

  RespondWithClosure(
      nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
      nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
      const nsAString& aRequestURL, const nsACString& aRespondWithScriptSpec,
      uint32_t aRespondWithLineNumber, uint32_t aRespondWithColumnNumber)
      : mInterceptedChannel(aChannel),
        mRegistration(aRegistration),
        mRequestURL(aRequestURL),
        mRespondWithScriptSpec(aRespondWithScriptSpec),
        mRespondWithLineNumber(aRespondWithLineNumber),
        mRespondWithColumnNumber(aRespondWithColumnNumber) {}
};

class FinishResponse final : public Runnable {
  nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;

 public:
  explicit FinishResponse(
      nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel)
      : Runnable("dom::FinishResponse"), mChannel(aChannel) {}

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    nsresult rv = mChannel->FinishSynthesizedResponse();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mChannel->CancelInterception(NS_ERROR_INTERCEPTION_FAILED);
      return NS_OK;
    }

    return rv;
  }
};

class BodyCopyHandle final : public nsIInterceptedBodyCallback {
  UniquePtr<RespondWithClosure> mClosure;

  ~BodyCopyHandle() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit BodyCopyHandle(UniquePtr<RespondWithClosure>&& aClosure)
      : mClosure(std::move(aClosure)) {}

  NS_IMETHOD
  BodyComplete(nsresult aRv) override {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIRunnable> event;
    if (NS_WARN_IF(NS_FAILED(aRv))) {
      ::AsyncLog(
          mClosure->mInterceptedChannel, mClosure->mRespondWithScriptSpec,
          mClosure->mRespondWithLineNumber, mClosure->mRespondWithColumnNumber,
          "InterceptionFailedWithURL"_ns, mClosure->mRequestURL);
      event = new CancelChannelRunnable(mClosure->mInterceptedChannel,
                                        mClosure->mRegistration,
                                        NS_ERROR_INTERCEPTION_FAILED);
    } else {
      event = new FinishResponse(mClosure->mInterceptedChannel);
    }

    mClosure.reset();

    event->Run();

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(BodyCopyHandle, nsIInterceptedBodyCallback)

class StartResponse final : public Runnable {
  nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;
  SafeRefPtr<InternalResponse> mInternalResponse;
  ChannelInfo mWorkerChannelInfo;
  const nsCString mScriptSpec;
  const nsCString mResponseURLSpec;
  UniquePtr<RespondWithClosure> mClosure;

 public:
  StartResponse(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                SafeRefPtr<InternalResponse> aInternalResponse,
                const ChannelInfo& aWorkerChannelInfo,
                const nsACString& aScriptSpec,
                const nsACString& aResponseURLSpec,
                UniquePtr<RespondWithClosure>&& aClosure)
      : Runnable("dom::StartResponse"),
        mChannel(aChannel),
        mInternalResponse(std::move(aInternalResponse)),
        mWorkerChannelInfo(aWorkerChannelInfo),
        mScriptSpec(aScriptSpec),
        mResponseURLSpec(aResponseURLSpec),
        mClosure(std::move(aClosure)) {}

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIChannel> underlyingChannel;
    nsresult rv = mChannel->GetChannel(getter_AddRefs(underlyingChannel));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(underlyingChannel, NS_ERROR_UNEXPECTED);
    nsCOMPtr<nsILoadInfo> loadInfo = underlyingChannel->LoadInfo();

    if (!CSPPermitsResponse(loadInfo)) {
      mChannel->CancelInterception(NS_ERROR_CONTENT_BLOCKED);
      return NS_OK;
    }

    ChannelInfo channelInfo;
    if (mInternalResponse->GetChannelInfo().IsInitialized()) {
      channelInfo = mInternalResponse->GetChannelInfo();
    } else {
      // We are dealing with a synthesized response here, so fall back to the
      // channel info for the worker script.
      channelInfo = mWorkerChannelInfo;
    }
    rv = mChannel->SetChannelInfo(&channelInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mChannel->CancelInterception(NS_ERROR_INTERCEPTION_FAILED);
      return NS_OK;
    }

    rv = mChannel->SynthesizeStatus(
        mInternalResponse->GetUnfilteredStatus(),
        mInternalResponse->GetUnfilteredStatusText());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mChannel->CancelInterception(NS_ERROR_INTERCEPTION_FAILED);
      return NS_OK;
    }

    AutoTArray<InternalHeaders::Entry, 5> entries;
    mInternalResponse->UnfilteredHeaders()->GetEntries(entries);
    for (uint32_t i = 0; i < entries.Length(); ++i) {
      mChannel->SynthesizeHeader(entries[i].mName, entries[i].mValue);
    }

    auto castLoadInfo = static_cast<mozilla::net::LoadInfo*>(loadInfo.get());
    castLoadInfo->SynthesizeServiceWorkerTainting(
        mInternalResponse->GetTainting());

    // Get the preferred alternative data type of outter channel
    nsAutoCString preferredAltDataType(""_ns);
    nsCOMPtr<nsICacheInfoChannel> outerChannel =
        do_QueryInterface(underlyingChannel);
    if (outerChannel &&
        !outerChannel->PreferredAlternativeDataTypes().IsEmpty()) {
      // TODO: handle multiple types properly.
      preferredAltDataType.Assign(
          outerChannel->PreferredAlternativeDataTypes()[0].type());
    }

    // Get the alternative data type saved in the InternalResponse
    nsAutoCString altDataType;
    nsCOMPtr<nsICacheInfoChannel> cacheInfoChannel =
        mInternalResponse->TakeCacheInfoChannel().get();
    if (cacheInfoChannel) {
      cacheInfoChannel->GetAlternativeDataType(altDataType);
    }

    nsCOMPtr<nsIInputStream> body;
    if (preferredAltDataType.Equals(altDataType)) {
      body = mInternalResponse->TakeAlternativeBody();
    }
    if (!body) {
      mInternalResponse->GetUnfilteredBody(getter_AddRefs(body));
    } else {
      Telemetry::ScalarAdd(Telemetry::ScalarID::SW_ALTERNATIVE_BODY_USED_COUNT,
                           1);
    }

    RefPtr<BodyCopyHandle> copyHandle;
    copyHandle = new BodyCopyHandle(std::move(mClosure));

    rv = mChannel->StartSynthesizedResponse(body, copyHandle, cacheInfoChannel,
                                            mResponseURLSpec,
                                            mInternalResponse->IsRedirected());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mChannel->CancelInterception(NS_ERROR_INTERCEPTION_FAILED);
      return NS_OK;
    }

    nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
    if (obsService) {
      obsService->NotifyObservers(
          underlyingChannel, "service-worker-synthesized-response", nullptr);
    }

    return rv;
  }

  bool CSPPermitsResponse(nsILoadInfo* aLoadInfo) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aLoadInfo);
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    nsCString url = mInternalResponse->GetUnfilteredURL();
    if (url.IsEmpty()) {
      // Synthetic response. The buck stops at the worker script.
      url = mScriptSpec;
    }
    rv = NS_NewURI(getter_AddRefs(uri), url);
    NS_ENSURE_SUCCESS(rv, false);
    int16_t decision = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(uri, aLoadInfo, &decision);
    NS_ENSURE_SUCCESS(rv, false);
    return decision == nsIContentPolicy::ACCEPT;
  }
};

class RespondWithHandler final : public PromiseNativeHandler {
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;
  const RequestMode mRequestMode;
  const RequestRedirect mRequestRedirectMode;
#ifdef DEBUG
  const bool mIsClientRequest;
#endif
  const nsCString mScriptSpec;
  const nsString mRequestURL;
  const nsCString mRequestFragment;
  const nsCString mRespondWithScriptSpec;
  const uint32_t mRespondWithLineNumber;
  const uint32_t mRespondWithColumnNumber;
  bool mRequestWasHandled;

 public:
  NS_DECL_ISUPPORTS

  RespondWithHandler(
      nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
      nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
      RequestMode aRequestMode, bool aIsClientRequest,
      RequestRedirect aRedirectMode, const nsACString& aScriptSpec,
      const nsAString& aRequestURL, const nsACString& aRequestFragment,
      const nsACString& aRespondWithScriptSpec, uint32_t aRespondWithLineNumber,
      uint32_t aRespondWithColumnNumber)
      : mInterceptedChannel(aChannel),
        mRegistration(aRegistration),
        mRequestMode(aRequestMode),
        mRequestRedirectMode(aRedirectMode)
#ifdef DEBUG
        ,
        mIsClientRequest(aIsClientRequest)
#endif
        ,
        mScriptSpec(aScriptSpec),
        mRequestURL(aRequestURL),
        mRequestFragment(aRequestFragment),
        mRespondWithScriptSpec(aRespondWithScriptSpec),
        mRespondWithLineNumber(aRespondWithLineNumber),
        mRespondWithColumnNumber(aRespondWithColumnNumber),
        mRequestWasHandled(false) {
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override;

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override;

  void CancelRequest(nsresult aStatus);

  void AsyncLog(const nsACString& aMessageName,
                const nsTArray<nsString>& aParams) {
    ::AsyncLog(mInterceptedChannel, mRespondWithScriptSpec,
               mRespondWithLineNumber, mRespondWithColumnNumber, aMessageName,
               aParams);
  }

  void AsyncLog(const nsACString& aSourceSpec, uint32_t aLine, uint32_t aColumn,
                const nsACString& aMessageName,
                const nsTArray<nsString>& aParams) {
    ::AsyncLog(mInterceptedChannel, aSourceSpec, aLine, aColumn, aMessageName,
               aParams);
  }

 private:
  ~RespondWithHandler() {
    if (!mRequestWasHandled) {
      ::AsyncLog(mInterceptedChannel, mRespondWithScriptSpec,
                 mRespondWithLineNumber, mRespondWithColumnNumber,
                 "InterceptionFailedWithURL"_ns, mRequestURL);
      CancelRequest(NS_ERROR_INTERCEPTION_FAILED);
    }
  }
};

class MOZ_STACK_CLASS AutoCancel {
  RefPtr<RespondWithHandler> mOwner;
  nsCString mSourceSpec;
  uint32_t mLine;
  uint32_t mColumn;
  nsCString mMessageName;
  nsTArray<nsString> mParams;

 public:
  AutoCancel(RespondWithHandler* aOwner, const nsString& aRequestURL)
      : mOwner(aOwner),
        mLine(0),
        mColumn(0),
        mMessageName("InterceptionFailedWithURL"_ns) {
    mParams.AppendElement(aRequestURL);
  }

  ~AutoCancel() {
    if (mOwner) {
      if (mSourceSpec.IsEmpty()) {
        mOwner->AsyncLog(mMessageName, mParams);
      } else {
        mOwner->AsyncLog(mSourceSpec, mLine, mColumn, mMessageName, mParams);
      }
      mOwner->CancelRequest(NS_ERROR_INTERCEPTION_FAILED);
    }
  }

  // This function steals the error message from a ErrorResult.
  void SetCancelErrorResult(JSContext* aCx, ErrorResult& aRv) {
    MOZ_DIAGNOSTIC_ASSERT(aRv.Failed());
    MOZ_DIAGNOSTIC_ASSERT(!JS_IsExceptionPending(aCx));

    // Storing the error as exception in the JSContext.
    if (!aRv.MaybeSetPendingException(aCx)) {
      return;
    }

    MOZ_ASSERT(!aRv.Failed());

    // Let's take the pending exception.
    JS::ExceptionStack exnStack(aCx);
    if (!JS::StealPendingExceptionStack(aCx, &exnStack)) {
      return;
    }

    // Converting the exception in a JS::ErrorReportBuilder.
    JS::ErrorReportBuilder report(aCx);
    if (!report.init(aCx, exnStack, JS::ErrorReportBuilder::WithSideEffects)) {
      JS_ClearPendingException(aCx);
      return;
    }

    MOZ_ASSERT(mOwner);
    MOZ_ASSERT(mMessageName.EqualsLiteral("InterceptionFailedWithURL"));
    MOZ_ASSERT(mParams.Length() == 1);

    // Let's store the error message here.
    mMessageName.Assign(report.toStringResult().c_str());
    mParams.Clear();
  }

  template <typename... Params>
  void SetCancelMessage(const nsACString& aMessageName, Params&&... aParams) {
    MOZ_ASSERT(mOwner);
    MOZ_ASSERT(mMessageName.EqualsLiteral("InterceptionFailedWithURL"));
    MOZ_ASSERT(mParams.Length() == 1);
    mMessageName = aMessageName;
    mParams.Clear();
    StringArrayAppender::Append(mParams, sizeof...(Params),
                                std::forward<Params>(aParams)...);
  }

  template <typename... Params>
  void SetCancelMessageAndLocation(const nsACString& aSourceSpec,
                                   uint32_t aLine, uint32_t aColumn,
                                   const nsACString& aMessageName,
                                   Params&&... aParams) {
    MOZ_ASSERT(mOwner);
    MOZ_ASSERT(mMessageName.EqualsLiteral("InterceptionFailedWithURL"));
    MOZ_ASSERT(mParams.Length() == 1);

    mSourceSpec = aSourceSpec;
    mLine = aLine;
    mColumn = aColumn;

    mMessageName = aMessageName;
    mParams.Clear();
    StringArrayAppender::Append(mParams, sizeof...(Params),
                                std::forward<Params>(aParams)...);
  }

  void Reset() { mOwner = nullptr; }
};

NS_IMPL_ISUPPORTS0(RespondWithHandler)

void RespondWithHandler::ResolvedCallback(JSContext* aCx,
                                          JS::Handle<JS::Value> aValue,
                                          ErrorResult& aRv) {
  AutoCancel autoCancel(this, mRequestURL);

  if (!aValue.isObject()) {
    NS_WARNING(
        "FetchEvent::RespondWith was passed a promise resolved to a non-Object "
        "value");

    nsCString sourceSpec;
    uint32_t line = 0;
    uint32_t column = 0;
    nsString valueString;
    nsContentUtils::ExtractErrorValues(aCx, aValue, sourceSpec, &line, &column,
                                       valueString);

    autoCancel.SetCancelMessageAndLocation(sourceSpec, line, column,
                                           "InterceptedNonResponseWithURL"_ns,
                                           mRequestURL, valueString);
    return;
  }

  RefPtr<Response> response;
  nsresult rv = UNWRAP_OBJECT(Response, &aValue.toObject(), response);
  if (NS_FAILED(rv)) {
    nsCString sourceSpec;
    uint32_t line = 0;
    uint32_t column = 0;
    nsString valueString;
    nsContentUtils::ExtractErrorValues(aCx, aValue, sourceSpec, &line, &column,
                                       valueString);

    autoCancel.SetCancelMessageAndLocation(sourceSpec, line, column,
                                           "InterceptedNonResponseWithURL"_ns,
                                           mRequestURL, valueString);
    return;
  }

  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  // Section "HTTP Fetch", step 3.3:
  //  If one of the following conditions is true, return a network error:
  //    * response's type is "error".
  //    * request's mode is not "no-cors" and response's type is "opaque".
  //    * request's redirect mode is not "manual" and response's type is
  //      "opaqueredirect".
  //    * request's redirect mode is not "follow" and response's url list
  //      has more than one item.

  if (response->Type() == ResponseType::Error) {
    autoCancel.SetCancelMessage("InterceptedErrorResponseWithURL"_ns,
                                mRequestURL);
    return;
  }

  MOZ_ASSERT_IF(mIsClientRequest, mRequestMode == RequestMode::Same_origin ||
                                      mRequestMode == RequestMode::Navigate);

  if (response->Type() == ResponseType::Opaque &&
      mRequestMode != RequestMode::No_cors) {
    NS_ConvertASCIItoUTF16 modeString(
        RequestModeValues::GetString(mRequestMode));

    autoCancel.SetCancelMessage("BadOpaqueInterceptionRequestModeWithURL"_ns,
                                mRequestURL, modeString);
    return;
  }

  if (mRequestRedirectMode != RequestRedirect::Manual &&
      response->Type() == ResponseType::Opaqueredirect) {
    autoCancel.SetCancelMessage("BadOpaqueRedirectInterceptionWithURL"_ns,
                                mRequestURL);
    return;
  }

  if (mRequestRedirectMode != RequestRedirect::Follow &&
      response->Redirected()) {
    autoCancel.SetCancelMessage("BadRedirectModeInterceptionWithURL"_ns,
                                mRequestURL);
    return;
  }

  if (NS_WARN_IF(response->BodyUsed())) {
    autoCancel.SetCancelMessage("InterceptedUsedResponseWithURL"_ns,
                                mRequestURL);
    return;
  }

  SafeRefPtr<InternalResponse> ir = response->GetInternalResponse();
  if (NS_WARN_IF(!ir)) {
    return;
  }

  // An extra safety check to make sure our invariant that opaque and cors
  // responses always have a URL does not break.
  if (NS_WARN_IF((response->Type() == ResponseType::Opaque ||
                  response->Type() == ResponseType::Cors) &&
                 ir->GetUnfilteredURL().IsEmpty())) {
    MOZ_DIAGNOSTIC_ASSERT(false, "Cors or opaque Response without a URL");
    return;
  }

  if (mRequestMode == RequestMode::Same_origin &&
      response->Type() == ResponseType::Cors) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::SW_CORS_RES_FOR_SO_REQ_COUNT, 1);

    // XXXtt: Will have a pref to enable the quirk response in bug 1419684.
    // The variadic template provided by StringArrayAppender requires exactly
    // an nsString.
    NS_ConvertUTF8toUTF16 responseURL(ir->GetUnfilteredURL());
    autoCancel.SetCancelMessage("CorsResponseForSameOriginRequest"_ns,
                                mRequestURL, responseURL);
    return;
  }

  // Propagate the URL to the content if the request mode is not "navigate".
  // Note that, we only reflect the final URL if the response.redirected is
  // false. We propagate all the URLs if the response.redirected is true.
  nsCString responseURL;
  if (mRequestMode != RequestMode::Navigate) {
    responseURL = ir->GetUnfilteredURL();

    // Similar to how we apply the request fragment to redirects automatically
    // we also want to apply it automatically when propagating the response
    // URL from a service worker interception.  Currently response.url strips
    // the fragment, so this will never conflict with an existing fragment
    // on the response.  In the future we will have to check for a response
    // fragment and avoid overriding in that case.
    if (!mRequestFragment.IsEmpty() && !responseURL.IsEmpty()) {
      MOZ_ASSERT(!responseURL.Contains('#'));
      responseURL.Append("#"_ns);
      responseURL.Append(mRequestFragment);
    }
  }

  UniquePtr<RespondWithClosure> closure(new RespondWithClosure(
      mInterceptedChannel, mRegistration, mRequestURL, mRespondWithScriptSpec,
      mRespondWithLineNumber, mRespondWithColumnNumber));

  nsCOMPtr<nsIRunnable> startRunnable = new StartResponse(
      mInterceptedChannel, ir.clonePtr(), worker->GetChannelInfo(), mScriptSpec,
      responseURL, std::move(closure));

  nsCOMPtr<nsIInputStream> body;
  ir->GetUnfilteredBody(getter_AddRefs(body));
  // Errors and redirects may not have a body.
  if (body) {
    ErrorResult error;
    response->SetBodyUsed(aCx, error);
    error.WouldReportJSException();
    if (NS_WARN_IF(error.Failed())) {
      autoCancel.SetCancelErrorResult(aCx, error);
      return;
    }
  }

  MOZ_ALWAYS_SUCCEEDS(worker->DispatchToMainThread(startRunnable.forget()));

  MOZ_ASSERT(!closure);
  autoCancel.Reset();
  mRequestWasHandled = true;
}

void RespondWithHandler::RejectedCallback(JSContext* aCx,
                                          JS::Handle<JS::Value> aValue,
                                          ErrorResult& aRv) {
  nsCString sourceSpec = mRespondWithScriptSpec;
  uint32_t line = mRespondWithLineNumber;
  uint32_t column = mRespondWithColumnNumber;
  nsString valueString;

  nsContentUtils::ExtractErrorValues(aCx, aValue, sourceSpec, &line, &column,
                                     valueString);

  ::AsyncLog(mInterceptedChannel, sourceSpec, line, column,
             "InterceptionRejectedResponseWithURL"_ns, mRequestURL,
             valueString);

  CancelRequest(NS_ERROR_INTERCEPTION_FAILED);
}

void RespondWithHandler::CancelRequest(nsresult aStatus) {
  nsCOMPtr<nsIRunnable> runnable =
      new CancelChannelRunnable(mInterceptedChannel, mRegistration, aStatus);
  // Note, this may run off the worker thread during worker termination.
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  if (worker) {
    MOZ_ALWAYS_SUCCEEDS(worker->DispatchToMainThread(runnable.forget()));
  } else {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable.forget()));
  }
  mRequestWasHandled = true;
}

}  // namespace

void FetchEvent::RespondWith(JSContext* aCx, Promise& aArg, ErrorResult& aRv) {
  if (!GetDispatchFlag() || mWaitToRespond) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Record where respondWith() was called in the script so we can include the
  // information in any error reporting.  We should be guaranteed not to get
  // a file:// string here because service workers require http/https.
  nsCString spec;
  uint32_t line = 0;
  uint32_t column = 1;
  nsJSUtils::GetCallingLocation(aCx, spec, &line, &column);

  SafeRefPtr<InternalRequest> ir = mRequest->GetInternalRequest();

  nsAutoCString requestURL;
  ir->GetURL(requestURL);

  StopImmediatePropagation();
  mWaitToRespond = true;

  if (mChannel) {
    RefPtr<RespondWithHandler> handler = new RespondWithHandler(
        mChannel, mRegistration, mRequest->Mode(), ir->IsClientRequest(),
        mRequest->Redirect(), mScriptSpec, NS_ConvertUTF8toUTF16(requestURL),
        ir->GetFragment(), spec, line, column);

    aArg.AppendNativeHandler(handler);
    // mRespondWithHandler can be nullptr for self-dispatched FetchEvent.
  } else if (mRespondWithHandler) {
    mRespondWithHandler->RespondWithCalledAt(spec, line, column);
    aArg.AppendNativeHandler(mRespondWithHandler);
    mRespondWithHandler = nullptr;
  }

  if (!WaitOnPromise(aArg)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void FetchEvent::PreventDefault(JSContext* aCx, CallerType aCallerType) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aCallerType != CallerType::System,
             "Since when do we support system-principal service workers?");

  if (mPreventDefaultScriptSpec.IsEmpty()) {
    // Note when the FetchEvent might have been canceled by script, but don't
    // actually log the location until we are sure it matters.  This is
    // determined in ServiceWorkerPrivate.cpp.  We only remember the first
    // call to preventDefault() as its the most likely to have actually canceled
    // the event.
    nsJSUtils::GetCallingLocation(aCx, mPreventDefaultScriptSpec,
                                  &mPreventDefaultLineNumber,
                                  &mPreventDefaultColumnNumber);
  }

  Event::PreventDefault(aCx, aCallerType);
}

void FetchEvent::ReportCanceled() {
  MOZ_ASSERT(!mPreventDefaultScriptSpec.IsEmpty());

  SafeRefPtr<InternalRequest> ir = mRequest->GetInternalRequest();
  nsAutoCString url;
  ir->GetURL(url);

  // The variadic template provided by StringArrayAppender requires exactly
  // an nsString.
  NS_ConvertUTF8toUTF16 requestURL(url);
  // nsString requestURL;
  // CopyUTF8toUTF16(url, requestURL);

  if (mChannel) {
    ::AsyncLog(mChannel.get(), mPreventDefaultScriptSpec,
               mPreventDefaultLineNumber, mPreventDefaultColumnNumber,
               "InterceptionCanceledWithURL"_ns, requestURL);
    // mRespondWithHandler could be nullptr for self-dispatched FetchEvent.
  } else if (mRespondWithHandler) {
    mRespondWithHandler->ReportCanceled(mPreventDefaultScriptSpec,
                                        mPreventDefaultLineNumber,
                                        mPreventDefaultColumnNumber);
    mRespondWithHandler = nullptr;
  }
}

namespace {

class WaitUntilHandler final : public PromiseNativeHandler {
  WorkerPrivate* mWorkerPrivate;
  const nsCString mScope;
  nsString mSourceSpec;
  uint32_t mLine;
  uint32_t mColumn;
  nsString mRejectValue;

  ~WaitUntilHandler() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  WaitUntilHandler(WorkerPrivate* aWorkerPrivate, JSContext* aCx)
      : mWorkerPrivate(aWorkerPrivate),
        mScope(mWorkerPrivate->ServiceWorkerScope()),
        mLine(0),
        mColumn(1) {
    mWorkerPrivate->AssertIsOnWorkerThread();

    // Save the location of the waitUntil() call itself as a fallback
    // in case the rejection value does not contain any location info.
    nsJSUtils::GetCallingLocation(aCx, mSourceSpec, &mLine, &mColumn);
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValu,
                        ErrorResult& aRve) override {
    // do nothing, we are only here to report errors
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    mWorkerPrivate->AssertIsOnWorkerThread();

    nsString spec;
    uint32_t line = 0;
    uint32_t column = 0;
    nsContentUtils::ExtractErrorValues(aCx, aValue, spec, &line, &column,
                                       mRejectValue);

    // only use the extracted location if we found one
    if (!spec.IsEmpty()) {
      mSourceSpec = spec;
      mLine = line;
      mColumn = column;
    }

    MOZ_ALWAYS_SUCCEEDS(mWorkerPrivate->DispatchToMainThread(
        NewRunnableMethod("WaitUntilHandler::ReportOnMainThread", this,
                          &WaitUntilHandler::ReportOnMainThread)));
  }

  void ReportOnMainThread() {
    MOZ_ASSERT(NS_IsMainThread());
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      // browser shutdown
      return;
    }

    // TODO: Make the error message a localized string. (bug 1222720)
    nsString message;
    message.AppendLiteral(
        "Service worker event waitUntil() was passed a "
        "promise that rejected with '");
    message.Append(mRejectValue);
    message.AppendLiteral("'.");

    // Note, there is a corner case where this won't report to the window
    // that triggered the error.  Consider a navigation fetch event that
    // rejects waitUntil() without holding respondWith() open.  In this case
    // there is no controlling document yet, the window did call .register()
    // because there is no documeny yet, and the navigation is no longer
    // being intercepted.

    swm->ReportToAllClients(mScope, message, mSourceSpec, u""_ns, mLine,
                            mColumn, nsIScriptError::errorFlag);
  }
};

NS_IMPL_ISUPPORTS0(WaitUntilHandler)

}  // anonymous namespace

ExtendableEvent::ExtensionsHandler::~ExtensionsHandler() {
  MOZ_ASSERT(!mExtendableEvent);
}

bool ExtendableEvent::ExtensionsHandler::GetDispatchFlag() const {
  // mExtendableEvent should set itself as nullptr in its destructor, and we
  // can't be dispatching an event that doesn't exist, so this should work for
  // as long as it's not needed to determine whether the event is still alive,
  // which seems unlikely.
  if (!mExtendableEvent) {
    return false;
  }

  return mExtendableEvent->GetDispatchFlag();
}

void ExtendableEvent::ExtensionsHandler::SetExtendableEvent(
    const ExtendableEvent* const aExtendableEvent) {
  mExtendableEvent = aExtendableEvent;
}

NS_IMPL_ADDREF_INHERITED(FetchEvent, ExtendableEvent)
NS_IMPL_RELEASE_INHERITED(FetchEvent, ExtendableEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FetchEvent)
NS_INTERFACE_MAP_END_INHERITING(ExtendableEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED(FetchEvent, ExtendableEvent, mRequest,
                                   mHandled, mPreloadResponse)

ExtendableEvent::ExtendableEvent(EventTarget* aOwner)
    : Event(aOwner, nullptr, nullptr) {}

bool ExtendableEvent::WaitOnPromise(Promise& aPromise) {
  if (!mExtensionsHandler) {
    return false;
  }
  return mExtensionsHandler->WaitOnPromise(aPromise);
}

void ExtendableEvent::SetKeepAliveHandler(
    ExtensionsHandler* aExtensionsHandler) {
  MOZ_ASSERT(!mExtensionsHandler);
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();
  mExtensionsHandler = aExtensionsHandler;
  mExtensionsHandler->SetExtendableEvent(this);
}

void ExtendableEvent::WaitUntil(JSContext* aCx, Promise& aPromise,
                                ErrorResult& aRv) {
  MOZ_ASSERT(!NS_IsMainThread());

  if (!WaitOnPromise(aPromise)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Append our handler to each waitUntil promise separately so we
  // can record the location in script where waitUntil was called.
  RefPtr<WaitUntilHandler> handler =
      new WaitUntilHandler(GetCurrentThreadWorkerPrivate(), aCx);
  aPromise.AppendNativeHandler(handler);
}

NS_IMPL_ADDREF_INHERITED(ExtendableEvent, Event)
NS_IMPL_RELEASE_INHERITED(ExtendableEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtendableEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

namespace {
nsresult ExtractBytesFromUSVString(const nsAString& aStr,
                                   nsTArray<uint8_t>& aBytes) {
  MOZ_ASSERT(aBytes.IsEmpty());
  auto encoder = UTF_8_ENCODING->NewEncoder();
  CheckedInt<size_t> needed =
      encoder->MaxBufferLengthFromUTF16WithoutReplacement(aStr.Length());
  if (NS_WARN_IF(!needed.isValid() ||
                 !aBytes.SetLength(needed.value(), fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  uint32_t result;
  size_t read;
  size_t written;
  // Do not use structured binding lest deal with [-Werror=unused-variable]
  std::tie(result, read, written) =
      encoder->EncodeFromUTF16WithoutReplacement(aStr, aBytes, true);
  MOZ_ASSERT(result == kInputEmpty);
  MOZ_ASSERT(read == aStr.Length());
  aBytes.TruncateLength(written);
  return NS_OK;
}

nsresult ExtractBytesFromData(
    const OwningArrayBufferViewOrArrayBufferOrUSVString& aDataInit,
    nsTArray<uint8_t>& aBytes) {
  MOZ_ASSERT(aBytes.IsEmpty());
  Maybe<bool> result = AppendTypedArrayDataTo(aDataInit, aBytes);
  if (result.isSome()) {
    return NS_WARN_IF(!result.value()) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
  }
  if (aDataInit.IsUSVString()) {
    return ExtractBytesFromUSVString(aDataInit.GetAsUSVString(), aBytes);
  }
  MOZ_ASSERT_UNREACHABLE("Unexpected push message data");
  return NS_ERROR_FAILURE;
}
}  // namespace

PushMessageData::PushMessageData(nsIGlobalObject* aOwner,
                                 nsTArray<uint8_t>&& aBytes)
    : mOwner(aOwner), mBytes(std::move(aBytes)) {}

PushMessageData::~PushMessageData() = default;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PushMessageData, mOwner)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PushMessageData)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushMessageData)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushMessageData)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* PushMessageData::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::PushMessageData_Binding::Wrap(aCx, this, aGivenProto);
}

void PushMessageData::Json(JSContext* cx, JS::MutableHandle<JS::Value> aRetval,
                           ErrorResult& aRv) {
  if (NS_FAILED(EnsureDecodedText())) {
    aRv.Throw(NS_ERROR_DOM_UNKNOWN_ERR);
    return;
  }
  BodyUtil::ConsumeJson(cx, aRetval, mDecodedText, aRv);
}

void PushMessageData::Text(nsAString& aData) {
  if (NS_SUCCEEDED(EnsureDecodedText())) {
    aData = mDecodedText;
  }
}

void PushMessageData::ArrayBuffer(JSContext* cx,
                                  JS::MutableHandle<JSObject*> aRetval,
                                  ErrorResult& aRv) {
  uint8_t* data = GetContentsCopy();
  if (data) {
    UniquePtr<uint8_t[], JS::FreePolicy> dataPtr(data);
    BodyUtil::ConsumeArrayBuffer(cx, aRetval, mBytes.Length(),
                                 std::move(dataPtr), aRv);
  }
}

already_AddRefed<mozilla::dom::Blob> PushMessageData::Blob(ErrorResult& aRv) {
  uint8_t* data = GetContentsCopy();
  if (data) {
    RefPtr<mozilla::dom::Blob> blob =
        BodyUtil::ConsumeBlob(mOwner, u""_ns, mBytes.Length(), data, aRv);
    if (blob) {
      return blob.forget();
    }
  }
  return nullptr;
}

nsresult PushMessageData::EnsureDecodedText() {
  if (mBytes.IsEmpty() || !mDecodedText.IsEmpty()) {
    return NS_OK;
  }
  nsresult rv = BodyUtil::ConsumeText(
      mBytes.Length(), reinterpret_cast<uint8_t*>(mBytes.Elements()),
      mDecodedText);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mDecodedText.Truncate();
    return rv;
  }
  return NS_OK;
}

uint8_t* PushMessageData::GetContentsCopy() {
  uint32_t length = mBytes.Length();
  void* data = malloc(length);
  if (!data) {
    return nullptr;
  }
  memcpy(data, mBytes.Elements(), length);
  return reinterpret_cast<uint8_t*>(data);
}

PushEvent::PushEvent(EventTarget* aOwner) : ExtendableEvent(aOwner) {}

already_AddRefed<PushEvent> PushEvent::Constructor(
    mozilla::dom::EventTarget* aOwner, const nsAString& aType,
    const PushEventInit& aOptions, ErrorResult& aRv) {
  RefPtr<PushEvent> e = new PushEvent(aOwner);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aOptions.mBubbles, aOptions.mCancelable);
  e->SetTrusted(trusted);
  e->SetComposed(aOptions.mComposed);
  if (aOptions.mData.WasPassed()) {
    nsTArray<uint8_t> bytes;
    nsresult rv = ExtractBytesFromData(aOptions.mData.Value(), bytes);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
    e->mData = new PushMessageData(aOwner->GetOwnerGlobal(), std::move(bytes));
  }
  return e.forget();
}

NS_IMPL_ADDREF_INHERITED(PushEvent, ExtendableEvent)
NS_IMPL_RELEASE_INHERITED(PushEvent, ExtendableEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushEvent)
NS_INTERFACE_MAP_END_INHERITING(ExtendableEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED(PushEvent, ExtendableEvent, mData)

JSObject* PushEvent::WrapObjectInternal(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::PushEvent_Binding::Wrap(aCx, this, aGivenProto);
}

ExtendableMessageEvent::ExtendableMessageEvent(EventTarget* aOwner)
    : ExtendableEvent(aOwner), mData(JS::UndefinedValue()) {
  mozilla::HoldJSObjects(this);
}

ExtendableMessageEvent::~ExtendableMessageEvent() { DropJSObjects(this); }

void ExtendableMessageEvent::GetData(JSContext* aCx,
                                     JS::MutableHandle<JS::Value> aData,
                                     ErrorResult& aRv) {
  aData.set(mData);
  if (!JS_WrapValue(aCx, aData)) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

void ExtendableMessageEvent::GetSource(
    Nullable<OwningClientOrServiceWorkerOrMessagePort>& aValue) const {
  if (mClient) {
    aValue.SetValue().SetAsClient() = mClient;
  } else if (mServiceWorker) {
    aValue.SetValue().SetAsServiceWorker() = mServiceWorker;
  } else if (mMessagePort) {
    aValue.SetValue().SetAsMessagePort() = mMessagePort;
  } else {
    // nullptr source is possible for manually constructed event
    aValue.SetNull();
  }
}

/* static */
already_AddRefed<ExtendableMessageEvent> ExtendableMessageEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const ExtendableMessageEventInit& aOptions) {
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(t, aType, aOptions);
}

/* static */
already_AddRefed<ExtendableMessageEvent> ExtendableMessageEvent::Constructor(
    mozilla::dom::EventTarget* aEventTarget, const nsAString& aType,
    const ExtendableMessageEventInit& aOptions) {
  RefPtr<ExtendableMessageEvent> event =
      new ExtendableMessageEvent(aEventTarget);

  event->InitEvent(aType, aOptions.mBubbles, aOptions.mCancelable);
  bool trusted = event->Init(aEventTarget);
  event->SetTrusted(trusted);

  event->mData = aOptions.mData;
  event->mOrigin = aOptions.mOrigin;
  event->mLastEventId = aOptions.mLastEventId;

  if (!aOptions.mSource.IsNull()) {
    if (aOptions.mSource.Value().IsClient()) {
      event->mClient = aOptions.mSource.Value().GetAsClient();
    } else if (aOptions.mSource.Value().IsServiceWorker()) {
      event->mServiceWorker = aOptions.mSource.Value().GetAsServiceWorker();
    } else if (aOptions.mSource.Value().IsMessagePort()) {
      event->mMessagePort = aOptions.mSource.Value().GetAsMessagePort();
    }
  }

  event->mPorts.AppendElements(aOptions.mPorts);
  return event.forget();
}

void ExtendableMessageEvent::GetPorts(nsTArray<RefPtr<MessagePort>>& aPorts) {
  aPorts = mPorts.Clone();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(ExtendableMessageEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ExtendableMessageEvent, Event)
  tmp->mData.setUndefined();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mClient)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mServiceWorker)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPorts)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ExtendableMessageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mClient)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServiceWorker)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPorts)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ExtendableMessageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtendableMessageEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(ExtendableMessageEvent, Event)
NS_IMPL_RELEASE_INHERITED(ExtendableMessageEvent, Event)

}  // namespace mozilla::dom
