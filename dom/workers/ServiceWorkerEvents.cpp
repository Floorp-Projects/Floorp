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
public:
  explicit CancelChannelRunnable(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel)
    : mChannel(aChannel)
  {
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsresult rv = mChannel->Cancel();
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

    mChannel->SynthesizeStatus(mInternalResponse->GetStatus(), mInternalResponse->GetStatusText());

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
  RequestMode mRequestMode;
public:
  NS_DECL_ISUPPORTS

  RespondWithHandler(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                     nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker,
                     RequestMode aRequestMode)
    : mInterceptedChannel(aChannel)
    , mServiceWorker(aServiceWorker)
    , mRequestMode(aRequestMode)
  {
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void CancelRequest();
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
    event = new CancelChannelRunnable(data->mInterceptedChannel);
  }
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(event)));
}

class MOZ_STACK_CLASS AutoCancel
{
  nsRefPtr<RespondWithHandler> mOwner;

public:
  explicit AutoCancel(RespondWithHandler* aOwner)
    : mOwner(aOwner)
  {
  }

  ~AutoCancel()
  {
    if (mOwner) {
      mOwner->CancelRequest();
    }
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
    return;
  }

  // Section 4.2, step 2.2 "If either response's type is "opaque" and request's
  // mode is not "no-cors" or response's type is error, return a network error."
  if (((response->Type() == ResponseType::Opaque) && (mRequestMode != RequestMode::No_cors)) ||
      response->Type() == ResponseType::Error) {
    return;
  }

  if (NS_WARN_IF(response->BodyUsed())) {
    return;
  }

  nsRefPtr<InternalResponse> ir = response->GetInternalResponse();
  if (NS_WARN_IF(!ir)) {
    return;
  }

  nsAutoPtr<RespondWithClosure> closure(
      new RespondWithClosure(mInterceptedChannel, ir, worker->GetChannelInfo()));
  nsCOMPtr<nsIInputStream> body;
  ir->GetInternalBody(getter_AddRefs(body));
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
  CancelRequest();
}

void
RespondWithHandler::CancelRequest()
{
  nsCOMPtr<nsIRunnable> runnable = new CancelChannelRunnable(mInterceptedChannel);
  NS_DispatchToMainThread(runnable);
}

} // anonymous namespace

void
FetchEvent::RespondWith(const ResponseOrPromise& aArg, ErrorResult& aRv)
{
  if (mWaitToRespond) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsRefPtr<Promise> promise;

  if (aArg.IsResponse()) {
    nsRefPtr<Response> res = &aArg.GetAsResponse();
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->AssertIsOnWorkerThread();
    promise = Promise::Create(worker->GlobalScope(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
    promise->MaybeResolve(res);
  } else if (aArg.IsPromise()) {
    promise = &aArg.GetAsPromise();
  }
  mWaitToRespond = true;
  nsRefPtr<RespondWithHandler> handler =
    new RespondWithHandler(mChannel, mServiceWorker, mRequest->Mode());
  promise->AppendNativeHandler(handler);
}

already_AddRefed<ServiceWorkerClient>
FetchEvent::GetClient()
{
  if (!mClient) {
    if (!mClientInfo) {
      return nullptr;
    }

    mClient = new ServiceWorkerClient(GetParentObject(), *mClientInfo);
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
ExtendableEvent::WaitUntil(Promise& aPromise)
{
  MOZ_ASSERT(!NS_IsMainThread());

  // Only first caller counts.
  if (EventPhase() == AT_TARGET && !mPromise) {
    mPromise = &aPromise;
  }
}

NS_IMPL_ADDREF_INHERITED(ExtendableEvent, Event)
NS_IMPL_RELEASE_INHERITED(ExtendableEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ExtendableEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ExtendableEvent, Event, mPromise)

#ifndef MOZ_SIMPLEPUSH

PushMessageData::PushMessageData(const nsAString& aData)
  : mData(aData)
{
}

PushMessageData::~PushMessageData()
{
}

NS_IMPL_ISUPPORTS0(PushMessageData);


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

NS_INTERFACE_MAP_BEGIN(PushEvent)
NS_INTERFACE_MAP_END_INHERITING(ExtendableEvent)

NS_IMPL_ADDREF_INHERITED(PushEvent, ExtendableEvent)
NS_IMPL_RELEASE_INHERITED(PushEvent, ExtendableEvent)

#endif /* ! MOZ_SIMPLEPUSH */

END_WORKERS_NAMESPACE
