/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerEvents.h"
#include "ServiceWorkerClient.h"

#include "nsINetworkInterceptController.h"
#include "nsIOutputStream.h"
#include "nsContentUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsNetCID.h"

#include "mozilla/dom/FetchEventBinding.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/workers/bindings/ServiceWorker.h"

using namespace mozilla::dom;

BEGIN_WORKERS_NAMESPACE

FetchEvent::FetchEvent(EventTarget* aOwner)
: Event(aOwner, nullptr, nullptr)
, mWindowId(0)
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
                     uint64_t aWindowId)
{
  mChannel = aChannel;
  mServiceWorker = aServiceWorker;
  mWindowId = aWindowId;
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

class CancelChannelRunnable MOZ_FINAL : public nsRunnable
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

class FinishResponse MOZ_FINAL : public nsRunnable
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;
public:
  explicit FinishResponse(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel)
    : mChannel(aChannel)
  {
  }

  NS_IMETHOD
      Run()
  {
    AssertIsOnMainThread();
    nsresult rv = mChannel->FinishSynthesizedResponse();
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to finish synthesized response");
    return rv;
  }
};

class RespondWithHandler MOZ_FINAL : public PromiseNativeHandler
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  nsMainThreadPtrHandle<ServiceWorker> mServiceWorker;
public:
  RespondWithHandler(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                     nsMainThreadPtrHandle<ServiceWorker>& aServiceWorker)
    : mInterceptedChannel(aChannel)
    , mServiceWorker(aServiceWorker)
  {
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) MOZ_OVERRIDE;

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) MOZ_OVERRIDE;

  void CancelRequest();
};

struct RespondWithClosure
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;

  explicit RespondWithClosure(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel)
    : mInterceptedChannel(aChannel)
  {
  }
};

void RespondWithCopyComplete(void* aClosure, nsresult aStatus)
{
  nsAutoPtr<RespondWithClosure> data(static_cast<RespondWithClosure*>(aClosure));
  nsCOMPtr<nsIRunnable> event;
  if (NS_SUCCEEDED(aStatus)) {
    event = new FinishResponse(data->mInterceptedChannel);
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

void
RespondWithHandler::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AutoCancel autoCancel(this);

  if (!aValue.isObject()) {
    return;
  }

  nsRefPtr<Response> response;
  nsresult rv = UNWRAP_OBJECT(Response, &aValue.toObject(), response);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIInputStream> body;
  response->GetBody(getter_AddRefs(body));
  if (NS_WARN_IF(!body) || NS_WARN_IF(response->BodyUsed())) {
    return;
  }
  response->SetBodyUsed();

  nsCOMPtr<nsIOutputStream> responseBody;
  rv = mInterceptedChannel->GetResponseBody(getter_AddRefs(responseBody));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsAutoPtr<RespondWithClosure> closure(new RespondWithClosure(mInterceptedChannel));

  nsCOMPtr<nsIEventTarget> stsThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(!stsThread)) {
    return;
  }
  rv = NS_AsyncCopy(body, responseBody, stsThread, NS_ASYNCCOPY_VIA_READSEGMENTS, 4096,
                    RespondWithCopyComplete, closure.forget());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

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
FetchEvent::RespondWith(Promise& aPromise, ErrorResult& aRv)
{
  if (mWaitToRespond) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  mWaitToRespond = true;
  nsRefPtr<RespondWithHandler> handler = new RespondWithHandler(mChannel, mServiceWorker);
  aPromise.AppendNativeHandler(handler);
}

already_AddRefed<ServiceWorkerClient>
FetchEvent::Client()
{
  if (!mClient) {
    mClient = new ServiceWorkerClient(GetParentObject(), mWindowId);
  }
  nsRefPtr<ServiceWorkerClient> client = mClient;
  return client.forget();
}

already_AddRefed<Promise>
FetchEvent::ForwardTo(const nsAString& aUrl)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  MOZ_ASSERT(global);
  ErrorResult result;
  nsRefPtr<Promise> promise = Promise::Create(global, result);
  if (NS_WARN_IF(result.Failed())) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
  return promise.forget();
}

already_AddRefed<Promise>
FetchEvent::Default()
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  MOZ_ASSERT(global);
  ErrorResult result;
  nsRefPtr<Promise> promise = Promise::Create(global, result);
  if (result.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
  return promise.forget();
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

InstallEvent::InstallEvent(EventTarget* aOwner)
  : ExtendableEvent(aOwner)
  , mActivateImmediately(false)
{
}

NS_IMPL_ADDREF_INHERITED(InstallEvent, ExtendableEvent)
NS_IMPL_RELEASE_INHERITED(InstallEvent, ExtendableEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(InstallEvent)
NS_INTERFACE_MAP_END_INHERITING(ExtendableEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED(InstallEvent, ExtendableEvent, mActiveWorker)

END_WORKERS_NAMESPACE
