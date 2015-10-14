/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerEvents.h"
#include "ServiceWorkerClient.h"
#include "ServiceWorkerManager.h"

#include "nsIHttpChannelInternal.h"
#include "nsINetworkInterceptController.h"
#include "nsIOutputStream.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsSerializationHelper.h"
#include "nsQueryObject.h"

#include "mozilla/Preferences.h"
#include "mozilla/dom/FetchEventBinding.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/workers/bindings/ServiceWorker.h"

#ifndef MOZ_SIMPLEPUSH
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"

#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/FetchUtil.h"
#include "mozilla/dom/TypedArray.h"
#endif

#include "WorkerPrivate.h"

using namespace mozilla::dom;

BEGIN_WORKERS_NAMESPACE

CancelChannelRunnable::CancelChannelRunnable(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                                             nsresult aStatus)
  : mChannel(aChannel)
  , mStatus(aStatus)
{
}

NS_IMETHODIMP
CancelChannelRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = mChannel->Cancel(mStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

FetchEvent::FetchEvent(EventTarget* aOwner)
: Event(aOwner, nullptr, nullptr)
, mIsReload(false)
, mWaitToRespond(false)
{
}

FetchEvent::~FetchEvent()
{
}

void
FetchEvent::PostInit(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                     nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker,
                     nsAutoPtr<ServiceWorkerClientInfo>& aClientInfo)
{
  mChannel = aChannel;
  mServiceWorker = aServiceWorker;
  mClientInfo = aClientInfo;
}

/*static*/ already_AddRefed<FetchEvent>
FetchEvent::Constructor(const GlobalObject& aGlobal,
                        const nsAString& aType,
                        const FetchEventInit& aOptions,
                        ErrorResult& aRv)
{
  nsRefPtr<EventTarget> owner = do_QueryObject(aGlobal.GetAsSupports());
  MOZ_ASSERT(owner);
  nsRefPtr<FetchEvent> e = new FetchEvent(owner);
  bool trusted = e->Init(owner);
  e->InitEvent(aType, aOptions.mBubbles, aOptions.mCancelable);
  e->SetTrusted(trusted);
  e->mRequest = aOptions.mRequest.WasPassed() ?
      &aOptions.mRequest.Value() : nullptr;
  e->mIsReload = aOptions.mIsReload.WasPassed() ?
      aOptions.mIsReload.Value() : false;
  e->mClient = aOptions.mClient.WasPassed() ?
      &aOptions.mClient.Value() : nullptr;
  return e.forget();
}

namespace {

class FinishResponse final : public nsRunnable
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;
  nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;
  nsRefPtr<InternalResponse> mInternalResponse;
  ChannelInfo mWorkerChannelInfo;

public:
  FinishResponse(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                 nsMainThreadPtrHandle<ServiceWorker> aServiceWorker,
                 InternalResponse* aInternalResponse,
                 const ChannelInfo& aWorkerChannelInfo)
    : mChannel(aChannel)
    , mServiceWorker(aServiceWorker)
    , mInternalResponse(aInternalResponse)
    , mWorkerChannelInfo(aWorkerChannelInfo)
  {
  }

  NS_IMETHOD
  Run()
  {
    AssertIsOnMainThread();

    if (!CSPPermitsResponse()) {
      mChannel->Cancel(NS_ERROR_CONTENT_BLOCKED);
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
    nsresult rv = mChannel->SetChannelInfo(&channelInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mChannel->SynthesizeStatus(mInternalResponse->GetUnfilteredStatus(),
                               mInternalResponse->GetUnfilteredStatusText());

    nsAutoTArray<InternalHeaders::Entry, 5> entries;
    mInternalResponse->UnfilteredHeaders()->GetEntries(entries);
    for (uint32_t i = 0; i < entries.Length(); ++i) {
       mChannel->SynthesizeHeader(entries[i].mName, entries[i].mValue);
    }

    rv = mChannel->FinishSynthesizedResponse();
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to finish synthesized response");
    return rv;
  }

  bool CSPPermitsResponse()
  {
    AssertIsOnMainThread();

    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    nsAutoCString url;
    mInternalResponse->GetUnfilteredUrl(url);
    if (url.IsEmpty()) {
      // Synthetic response. The buck stops at the worker script.
      url = mServiceWorker->Info()->ScriptSpec();
    }
    rv = NS_NewURI(getter_AddRefs(uri), url, nullptr, nullptr);
    NS_ENSURE_SUCCESS(rv, false);

    nsCOMPtr<nsIChannel> underlyingChannel;
    rv = mChannel->GetChannel(getter_AddRefs(underlyingChannel));
    NS_ENSURE_SUCCESS(rv, false);
    NS_ENSURE_TRUE(underlyingChannel, false);
    nsCOMPtr<nsILoadInfo> loadInfo = underlyingChannel->GetLoadInfo();

    int16_t decision = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(loadInfo->InternalContentPolicyType(), uri, loadInfo->LoadingPrincipal(),
                                   loadInfo->LoadingNode(), EmptyCString(), nullptr, &decision);
    NS_ENSURE_SUCCESS(rv, false);
    return decision == nsIContentPolicy::ACCEPT;
  }
};

class RespondWithHandler final : public PromiseNativeHandler
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;
  const RequestMode mRequestMode;
  const DebugOnly<bool> mIsClientRequest;
  const bool mIsNavigationRequest;
public:
  NS_DECL_ISUPPORTS

  RespondWithHandler(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                     nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker,
                     RequestMode aRequestMode, bool aIsClientRequest,
                     bool aIsNavigationRequest)
    : mInterceptedChannel(aChannel)
    , mServiceWorker(aServiceWorker)
    , mRequestMode(aRequestMode)
    , mIsClientRequest(aIsClientRequest)
    , mIsNavigationRequest(aIsNavigationRequest)
  {
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void CancelRequest(nsresult aStatus);
private:
  ~RespondWithHandler() {}
};

struct RespondWithClosure
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;
  nsRefPtr<InternalResponse> mInternalResponse;
  ChannelInfo mWorkerChannelInfo;

  RespondWithClosure(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                     nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker,
                     InternalResponse* aInternalResponse,
                     const ChannelInfo& aWorkerChannelInfo)
    : mInterceptedChannel(aChannel)
    , mServiceWorker(aServiceWorker)
    , mInternalResponse(aInternalResponse)
    , mWorkerChannelInfo(aWorkerChannelInfo)
  {
  }
};

void RespondWithCopyComplete(void* aClosure, nsresult aStatus)
{
  nsAutoPtr<RespondWithClosure> data(static_cast<RespondWithClosure*>(aClosure));
  nsCOMPtr<nsIRunnable> event;
  if (NS_SUCCEEDED(aStatus)) {
    event = new FinishResponse(data->mInterceptedChannel,
                               data->mServiceWorker,
                               data->mInternalResponse,
                               data->mWorkerChannelInfo);
  } else {
    event = new CancelChannelRunnable(data->mInterceptedChannel,
                                      NS_ERROR_INTERCEPTION_FAILED);
  }
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(event)));
}

class MOZ_STACK_CLASS AutoCancel
{
  nsRefPtr<RespondWithHandler> mOwner;
  nsresult mStatus;

public:
  explicit AutoCancel(RespondWithHandler* aOwner)
    : mOwner(aOwner)
    , mStatus(NS_ERROR_INTERCEPTION_FAILED)
  {
  }

  ~AutoCancel()
  {
    if (mOwner) {
      mOwner->CancelRequest(mStatus);
    }
  }

  void SetCancelStatus(nsresult aStatus)
  {
    MOZ_ASSERT(NS_FAILED(aStatus));
    mStatus = aStatus;
  }

  void Reset()
  {
    mOwner = nullptr;
  }
};

NS_IMPL_ISUPPORTS0(RespondWithHandler)

void
RespondWithHandler::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AutoCancel autoCancel(this);

  if (!aValue.isObject()) {
    NS_WARNING("FetchEvent::RespondWith was passed a promise resolved to a non-Object value");
    return;
  }

  nsRefPtr<Response> response;
  nsresult rv = UNWRAP_OBJECT(Response, &aValue.toObject(), response);
  if (NS_FAILED(rv)) {
    return;
  }

  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  // Allow opaque response interception to be disabled until we can ensure the
  // security implications are not a complete disaster.
  if (response->Type() == ResponseType::Opaque &&
      !worker->OpaqueInterceptionEnabled()) {
    autoCancel.SetCancelStatus(NS_ERROR_OPAQUE_INTERCEPTION_DISABLED);
    return;
  }

  // Section "HTTP Fetch", step 2.2:
  //  If one of the following conditions is true, return a network error:
  //    * response's type is "error".
  //    * request's mode is not "no-cors" and response's type is "opaque".
  //    * request is not a navigation request and response's type is
  //      "opaqueredirect".

  if (response->Type() == ResponseType::Error) {
    autoCancel.SetCancelStatus(NS_ERROR_INTERCEPTED_ERROR_RESPONSE);
    return;
  }

  MOZ_ASSERT_IF(mIsClientRequest, mRequestMode == RequestMode::Same_origin);

  if (response->Type() == ResponseType::Opaque && mRequestMode != RequestMode::No_cors) {
    autoCancel.SetCancelStatus(NS_ERROR_BAD_OPAQUE_INTERCEPTION_REQUEST_MODE);
    return;
  }

  if (!mIsNavigationRequest && response->Type() == ResponseType::Opaqueredirect) {
    autoCancel.SetCancelStatus(NS_ERROR_BAD_OPAQUE_REDIRECT_INTERCEPTION);
    return;
  }

  if (NS_WARN_IF(response->BodyUsed())) {
    autoCancel.SetCancelStatus(NS_ERROR_INTERCEPTED_USED_RESPONSE);
    return;
  }

  nsRefPtr<InternalResponse> ir = response->GetInternalResponse();
  if (NS_WARN_IF(!ir)) {
    return;
  }

  nsAutoPtr<RespondWithClosure> closure(
      new RespondWithClosure(mInterceptedChannel, mServiceWorker, ir, worker->GetChannelInfo()));
  nsCOMPtr<nsIInputStream> body;
  ir->GetUnfilteredBody(getter_AddRefs(body));
  // Errors and redirects may not have a body.
  if (body) {
    response->SetBodyUsed();

    nsCOMPtr<nsIOutputStream> responseBody;
    rv = mInterceptedChannel->GetResponseBody(getter_AddRefs(responseBody));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsCOMPtr<nsIEventTarget> stsThread = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_WARN_IF(!stsThread)) {
      return;
    }

    // XXXnsm, Fix for Bug 1141332 means that if we decide to make this
    // streaming at some point, we'll need a different solution to that bug.
    rv = NS_AsyncCopy(body, responseBody, stsThread, NS_ASYNCCOPY_VIA_READSEGMENTS, 4096,
                      RespondWithCopyComplete, closure.forget());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  } else {
    RespondWithCopyComplete(closure.forget(), NS_OK);
  }

  MOZ_ASSERT(!closure);
  autoCancel.Reset();
}

void
RespondWithHandler::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  CancelRequest(NS_ERROR_INTERCEPTION_FAILED);
}

void
RespondWithHandler::CancelRequest(nsresult aStatus)
{
  nsCOMPtr<nsIRunnable> runnable =
    new CancelChannelRunnable(mInterceptedChannel, aStatus);
  NS_DispatchToMainThread(runnable);
}

} // namespace

void
FetchEvent::RespondWith(Promise& aArg, ErrorResult& aRv)
{
  if (mWaitToRespond) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsRefPtr<InternalRequest> ir = mRequest->GetInternalRequest();
  StopImmediatePropagation();
  mWaitToRespond = true;
  nsRefPtr<RespondWithHandler> handler =
    new RespondWithHandler(mChannel, mServiceWorker, mRequest->Mode(),
                           ir->IsClientRequest(), ir->IsNavigationRequest());
  aArg.AppendNativeHandler(handler);
}

already_AddRefed<ServiceWorkerClient>
FetchEvent::GetClient()
{
  if (!mClient) {
    if (!mClientInfo) {
      return nullptr;
    }

    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    nsRefPtr<nsIGlobalObject> global = worker->GlobalScope();

    mClient = new ServiceWorkerClient(global, *mClientInfo);
  }
  nsRefPtr<ServiceWorkerClient> client = mClient;
  return client.forget();
}

NS_IMPL_ADDREF_INHERITED(FetchEvent, Event)
NS_IMPL_RELEASE_INHERITED(FetchEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FetchEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_CYCLE_COLLECTION_INHERITED(FetchEvent, Event, mRequest, mClient)

ExtendableEvent::ExtendableEvent(EventTarget* aOwner)
  : Event(aOwner, nullptr, nullptr)
{
}

void
ExtendableEvent::WaitUntil(Promise& aPromise, ErrorResult& aRv)
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (EventPhase() == nsIDOMEvent::NONE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  mPromises.AppendElement(&aPromise);
}

already_AddRefed<Promise>
ExtendableEvent::GetPromise()
{
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  GlobalObject global(worker->GetJSContext(), worker->GlobalScope()->GetGlobalJSObject());

  ErrorResult result;
  nsRefPtr<Promise> p = Promise::All(global, Move(mPromises), result);
  if (NS_WARN_IF(result.Failed())) {
    return nullptr;
  }

  return p.forget();
}

NS_IMPL_ADDREF_INHERITED(ExtendableEvent, Event)
NS_IMPL_RELEASE_INHERITED(ExtendableEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ExtendableEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ExtendableEvent, Event, mPromises)

#ifndef MOZ_SIMPLEPUSH

namespace {
nsresult
ExtractBytesFromArrayBufferView(const ArrayBufferView& aView, nsTArray<uint8_t>& aBytes)
{
  MOZ_ASSERT(aBytes.IsEmpty());
  aView.ComputeLengthAndData();
  aBytes.InsertElementsAt(0, aView.Data(), aView.Length());
  return NS_OK;
}

nsresult
ExtractBytesFromArrayBuffer(const ArrayBuffer& aBuffer, nsTArray<uint8_t>& aBytes)
{
  MOZ_ASSERT(aBytes.IsEmpty());
  aBuffer.ComputeLengthAndData();
  aBytes.InsertElementsAt(0, aBuffer.Data(), aBuffer.Length());
  return NS_OK;
}

nsresult
ExtractBytesFromUSVString(const nsAString& aStr, nsTArray<uint8_t>& aBytes)
{
  MOZ_ASSERT(aBytes.IsEmpty());
  nsCOMPtr<nsIUnicodeEncoder> encoder = EncodingUtils::EncoderForEncoding("UTF-8");
  if (!encoder) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int32_t srcLen = aStr.Length();
  int32_t destBufferLen;
  nsresult rv = encoder->GetMaxLength(aStr.BeginReading(), srcLen, &destBufferLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aBytes.SetLength(destBufferLen);

  char* destBuffer = reinterpret_cast<char*>(aBytes.Elements());
  int32_t outLen = destBufferLen;
  rv = encoder->Convert(aStr.BeginReading(), &srcLen, destBuffer, &outLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(outLen <= destBufferLen);
  aBytes.SetLength(outLen);

  return NS_OK;
}

nsresult
ExtractBytesFromData(const OwningArrayBufferViewOrArrayBufferOrUSVString& aDataInit, nsTArray<uint8_t>& aBytes)
{
  if (aDataInit.IsArrayBufferView()) {
    const ArrayBufferView& view = aDataInit.GetAsArrayBufferView();
    return ExtractBytesFromArrayBufferView(view, aBytes);
  } else if (aDataInit.IsArrayBuffer()) {
    const ArrayBuffer& buffer = aDataInit.GetAsArrayBuffer();
    return ExtractBytesFromArrayBuffer(buffer, aBytes);
  } else if (aDataInit.IsUSVString()) {
    return ExtractBytesFromUSVString(aDataInit.GetAsUSVString(), aBytes);
  }
  NS_NOTREACHED("Unexpected push message data");
  return NS_ERROR_FAILURE;
}
}

PushMessageData::PushMessageData(nsISupports* aOwner,
                                 const nsTArray<uint8_t>& aBytes)
  : mOwner(aOwner), mBytes(aBytes) {}

PushMessageData::~PushMessageData()
{
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PushMessageData, mOwner)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PushMessageData)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushMessageData)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushMessageData)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

void
PushMessageData::Json(JSContext* cx, JS::MutableHandle<JS::Value> aRetval,
                      ErrorResult& aRv)
{
  if (NS_FAILED(EnsureDecodedText())) {
    aRv.Throw(NS_ERROR_DOM_UNKNOWN_ERR);
    return;
  }
  FetchUtil::ConsumeJson(cx, aRetval, mDecodedText, aRv);
}

void
PushMessageData::Text(nsAString& aData)
{
  if (NS_SUCCEEDED(EnsureDecodedText())) {
    aData = mDecodedText;
  }
}

void
PushMessageData::ArrayBuffer(JSContext* cx,
                             JS::MutableHandle<JSObject*> aRetval,
                             ErrorResult& aRv)
{
  uint8_t* data = GetContentsCopy();
  if (data) {
    FetchUtil::ConsumeArrayBuffer(cx, aRetval, mBytes.Length(), data, aRv);
  }
}

already_AddRefed<mozilla::dom::Blob>
PushMessageData::Blob(ErrorResult& aRv)
{
  uint8_t* data = GetContentsCopy();
  if (data) {
    nsRefPtr<mozilla::dom::Blob> blob = FetchUtil::ConsumeBlob(
      mOwner, EmptyString(), mBytes.Length(), data, aRv);
    if (blob) {
      return blob.forget();
    }
  }
  return nullptr;
}

NS_METHOD
PushMessageData::EnsureDecodedText()
{
  if (mBytes.IsEmpty() || !mDecodedText.IsEmpty()) {
    return NS_OK;
  }
  nsresult rv = FetchUtil::ConsumeText(
    mBytes.Length(),
    reinterpret_cast<uint8_t*>(mBytes.Elements()),
    mDecodedText
  );
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mDecodedText.Truncate();
    return rv;
  }
  return NS_OK;
}

uint8_t*
PushMessageData::GetContentsCopy()
{
  uint32_t length = mBytes.Length();
  void* data = malloc(length);
  if (!data) {
    return nullptr;
  }
  memcpy(data, mBytes.Elements(), length);
  return reinterpret_cast<uint8_t*>(data);
}

PushEvent::PushEvent(EventTarget* aOwner)
  : ExtendableEvent(aOwner)
{
}

already_AddRefed<PushEvent>
PushEvent::Constructor(mozilla::dom::EventTarget* aOwner,
                       const nsAString& aType,
                       const PushEventInit& aOptions,
                       ErrorResult& aRv)
{
  nsRefPtr<PushEvent> e = new PushEvent(aOwner);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aOptions.mBubbles, aOptions.mCancelable);
  e->SetTrusted(trusted);
  if(aOptions.mData.WasPassed()){
    nsTArray<uint8_t> bytes;
    nsresult rv = ExtractBytesFromData(aOptions.mData.Value(), bytes);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
    e->mData = new PushMessageData(aOwner, bytes);
  }
  return e.forget();
}

NS_IMPL_ADDREF_INHERITED(PushEvent, ExtendableEvent)
NS_IMPL_RELEASE_INHERITED(PushEvent, ExtendableEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PushEvent)
NS_INTERFACE_MAP_END_INHERITING(ExtendableEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED(PushEvent, ExtendableEvent, mData)

#endif /* ! MOZ_SIMPLEPUSH */

END_WORKERS_NAMESPACE
