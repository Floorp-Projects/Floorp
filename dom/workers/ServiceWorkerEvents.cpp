/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerEvents.h"
#include "ServiceWorkerClient.h"

#include "nsIHttpChannelInternal.h"
#include "nsINetworkInterceptController.h"
#include "nsIOutputStream.h"
#include "nsContentUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsNetCID.h"
#include "nsSerializationHelper.h"
#include "nsQueryObject.h"

#include "mozilla/Preferences.h"
#include "mozilla/dom/FetchEventBinding.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/workers/bindings/ServiceWorker.h"

#include "WorkerPrivate.h"

using namespace mozilla::dom;

BEGIN_WORKERS_NAMESPACE

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

class CancelChannelRunnable final : public nsRunnable
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;
  const nsresult mStatus;
public:
  CancelChannelRunnable(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                        nsresult aStatus)
    : mChannel(aChannel)
    , mStatus(aStatus)
  {
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsresult rv = mChannel->Cancel(mStatus);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }
};

class FinishResponse final : public nsRunnable
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;
  nsRefPtr<InternalResponse> mInternalResponse;
  ChannelInfo mWorkerChannelInfo;
public:
  FinishResponse(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                 InternalResponse* aInternalResponse,
                 const ChannelInfo& aWorkerChannelInfo)
    : mChannel(aChannel)
    , mInternalResponse(aInternalResponse)
    , mWorkerChannelInfo(aWorkerChannelInfo)
  {
  }

  NS_IMETHOD
  Run()
  {
    AssertIsOnMainThread();

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
};

class RespondWithHandler final : public PromiseNativeHandler
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;
  const RequestMode mRequestMode;
  const bool mIsClientRequest;
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
  nsRefPtr<InternalResponse> mInternalResponse;
  ChannelInfo mWorkerChannelInfo;

  RespondWithClosure(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                     InternalResponse* aInternalResponse,
                     const ChannelInfo& aWorkerChannelInfo)
    : mInterceptedChannel(aChannel)
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
  //    * request is a client request and response's type is neither "basic"
  //      nor "default".
  //    * request is not a navigation request and response's type is
  //      "opaqueredirect".

  if (response->Type() == ResponseType::Error) {
    autoCancel.SetCancelStatus(NS_ERROR_INTERCEPTED_ERROR_RESPONSE);
    return;
  }

  if (response->Type() == ResponseType::Opaque && mRequestMode != RequestMode::No_cors) {
    autoCancel.SetCancelStatus(NS_ERROR_BAD_OPAQUE_INTERCEPTION_REQUEST_MODE);
    return;
  }

  // TODO: remove this case as its no longer in the spec (bug 1184967)
  if (mIsClientRequest && response->Type() != ResponseType::Basic &&
      response->Type() != ResponseType::Default &&
      response->Type() != ResponseType::Opaqueredirect) {
    autoCancel.SetCancelStatus(NS_ERROR_CLIENT_REQUEST_OPAQUE_INTERCEPTION);
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
      new RespondWithClosure(mInterceptedChannel, ir, worker->GetChannelInfo()));
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

PushMessageData::PushMessageData(const nsAString& aData)
  : mData(aData)
{
}

PushMessageData::~PushMessageData()
{
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(PushMessageData);

NS_IMPL_CYCLE_COLLECTING_ADDREF(PushMessageData)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushMessageData)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushMessageData)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

void
PushMessageData::Json(JSContext* cx, JS::MutableHandle<JSObject*> aRetval)
{
  //todo bug 1149195.  Don't be lazy.
   NS_ABORT();
}

void
PushMessageData::Text(nsAString& aData)
{
  aData = mData;
}

void
PushMessageData::ArrayBuffer(JSContext* cx, JS::MutableHandle<JSObject*> aRetval)
{
  //todo bug 1149195.  Don't be lazy.
   NS_ABORT();
}

mozilla::dom::Blob*
PushMessageData::Blob()
{
  //todo bug 1149195.  Don't be lazy.
  NS_ABORT();
  return nullptr;
}

PushEvent::PushEvent(EventTarget* aOwner)
  : ExtendableEvent(aOwner)
{
}

#endif /* ! MOZ_SIMPLEPUSH */

END_WORKERS_NAMESPACE
