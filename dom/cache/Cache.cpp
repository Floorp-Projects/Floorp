/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/Cache.h"

#include "mozilla/dom/Headers.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/cache/AutoUtils.h"
#include "mozilla/dom/cache/CacheChild.h"
#include "mozilla/dom/cache/CachePushStreamChild.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Preferences.h"
#include "mozilla/unused.h"
#include "nsIGlobalObject.h"
#include "nsNetUtil.h"

namespace {

using mozilla::ErrorResult;
using mozilla::dom::MSG_INVALID_REQUEST_METHOD;
using mozilla::dom::OwningRequestOrUSVString;
using mozilla::dom::Request;
using mozilla::dom::RequestOrUSVString;

static bool
IsValidPutRequestMethod(const Request& aRequest, ErrorResult& aRv)
{
  nsAutoCString method;
  aRequest.GetMethod(method);
  bool valid = method.LowerCaseEqualsLiteral("get");
  if (!valid) {
    NS_ConvertASCIItoUTF16 label(method);
    aRv.ThrowTypeError(MSG_INVALID_REQUEST_METHOD, &label);
  }
  return valid;
}

static bool
IsValidPutRequestMethod(const RequestOrUSVString& aRequest,
                        ErrorResult& aRv)
{
  // If the provided request is a string URL, then it will default to
  // a valid http method automatically.
  if (!aRequest.IsRequest()) {
    return true;
  }
  return IsValidPutRequestMethod(aRequest.GetAsRequest(), aRv);
}

static bool
IsValidPutRequestMethod(const OwningRequestOrUSVString& aRequest,
                        ErrorResult& aRv)
{
  if (!aRequest.IsRequest()) {
    return true;
  }
  return IsValidPutRequestMethod(*aRequest.GetAsRequest().get(), aRv);
}

} // anonymous namespace

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::ErrorResult;
using mozilla::unused;
using mozilla::dom::workers::GetCurrentThreadWorkerPrivate;
using mozilla::dom::workers::WorkerPrivate;

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozilla::dom::cache::Cache);
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozilla::dom::cache::Cache);
NS_IMPL_CYCLE_COLLECTION_CLASS(mozilla::dom::cache::Cache)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(mozilla::dom::cache::Cache)
  tmp->DisconnectFromActor();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal, mRequestPromises)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(mozilla::dom::cache::Cache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal, mRequestPromises)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(mozilla::dom::cache::Cache)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Cache)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Cache::Cache(nsIGlobalObject* aGlobal, CacheChild* aActor)
  : mGlobal(aGlobal)
  , mActor(aActor)
{
  MOZ_ASSERT(mGlobal);
  MOZ_ASSERT(mActor);
  mActor->SetListener(this);
}

already_AddRefed<Promise>
Cache::Match(const RequestOrUSVString& aRequest,
             const CacheQueryOptions& aOptions, ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  nsRefPtr<InternalRequest> ir = ToInternalRequest(aRequest, IgnoreBody, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  AutoChildRequest request(this);

  request.Add(ir, IgnoreBody, PassThroughReferrer, IgnoreInvalidScheme, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  PCacheQueryParams params;
  ToPCacheQueryParams(params, aOptions);

  RequestId requestId = AddRequestPromise(promise, aRv);

  unused << mActor->SendMatch(requestId, request.SendAsRequest(), params);

  return promise.forget();
}

already_AddRefed<Promise>
Cache::MatchAll(const Optional<RequestOrUSVString>& aRequest,
                const CacheQueryOptions& aOptions, ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  AutoChildRequest request(this);

  if (aRequest.WasPassed()) {
    nsRefPtr<InternalRequest> ir = ToInternalRequest(aRequest.Value(),
                                                     IgnoreBody, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    request.Add(ir, IgnoreBody, PassThroughReferrer, IgnoreInvalidScheme, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  PCacheQueryParams params;
  ToPCacheQueryParams(params, aOptions);

  RequestId requestId = AddRequestPromise(promise, aRv);

  unused << mActor->SendMatchAll(requestId, request.SendAsRequestOrVoid(),
                                 params);

  return promise.forget();
}

already_AddRefed<Promise>
Cache::Add(const RequestOrUSVString& aRequest, ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  if (!IsValidPutRequestMethod(aRequest, aRv)) {
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  nsRefPtr<InternalRequest> ir = ToInternalRequest(aRequest, ReadBody, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  AutoChildRequestList requests(this, 1);
  requests.Add(ir, ReadBody, ExpandReferrer, NetworkErrorOnInvalidScheme, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RequestId requestId = AddRequestPromise(promise, aRv);

  unused << mActor->SendAddAll(requestId, requests.SendAsRequestList());

  return promise.forget();
}

already_AddRefed<Promise>
Cache::AddAll(const Sequence<OwningRequestOrUSVString>& aRequests,
              ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  // If there is no work to do, then resolve immediately
  if (aRequests.IsEmpty()) {
    promise->MaybeResolve(JS::UndefinedHandleValue);
    return promise.forget();
  }

  AutoChildRequestList requests(this, aRequests.Length());

  for (uint32_t i = 0; i < aRequests.Length(); ++i) {
    if (!IsValidPutRequestMethod(aRequests[i], aRv)) {
      return nullptr;
    }

    nsRefPtr<InternalRequest> ir = ToInternalRequest(aRequests[i], ReadBody,
                                                     aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    requests.Add(ir, ReadBody, ExpandReferrer, NetworkErrorOnInvalidScheme,
                 aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  RequestId requestId = AddRequestPromise(promise, aRv);

  unused << mActor->SendAddAll(requestId, requests.SendAsRequestList());

  return promise.forget();
}

already_AddRefed<Promise>
Cache::Put(const RequestOrUSVString& aRequest, Response& aResponse,
           ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  if (!IsValidPutRequestMethod(aRequest, aRv)) {
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  nsRefPtr<InternalRequest> ir = ToInternalRequest(aRequest, ReadBody, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  AutoChildRequestResponse put(this);
  put.Add(ir, ReadBody, PassThroughReferrer, TypeErrorOnInvalidScheme, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  put.Add(aResponse, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RequestId requestId = AddRequestPromise(promise, aRv);

  unused << mActor->SendPut(requestId, put.SendAsRequestResponse());

  return promise.forget();
}

already_AddRefed<Promise>
Cache::Delete(const RequestOrUSVString& aRequest,
              const CacheQueryOptions& aOptions, ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  nsRefPtr<InternalRequest> ir = ToInternalRequest(aRequest, IgnoreBody, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  AutoChildRequest request(this);
  request.Add(ir, IgnoreBody, PassThroughReferrer, IgnoreInvalidScheme, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  PCacheQueryParams params;
  ToPCacheQueryParams(params, aOptions);

  RequestId requestId = AddRequestPromise(promise, aRv);

  unused << mActor->SendDelete(requestId, request.SendAsRequest(), params);

  return promise.forget();
}

already_AddRefed<Promise>
Cache::Keys(const Optional<RequestOrUSVString>& aRequest,
            const CacheQueryOptions& aOptions, ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  AutoChildRequest request(this);

  if (aRequest.WasPassed()) {
    nsRefPtr<InternalRequest> ir = ToInternalRequest(aRequest.Value(),
                                                     IgnoreBody, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    request.Add(ir, IgnoreBody, PassThroughReferrer, IgnoreInvalidScheme, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  PCacheQueryParams params;
  ToPCacheQueryParams(params, aOptions);

  RequestId requestId = AddRequestPromise(promise, aRv);

  unused << mActor->SendKeys(requestId, request.SendAsRequestOrVoid(), params);

  return promise.forget();
}

// static
bool
Cache::PrefEnabled(JSContext* aCx, JSObject* aObj)
{
  using mozilla::dom::workers::WorkerPrivate;
  using mozilla::dom::workers::GetWorkerPrivateFromContext;

  // If we're on the main thread, then check the pref directly.
  if (NS_IsMainThread()) {
    bool enabled = false;
    Preferences::GetBool("dom.caches.enabled", &enabled);
    return enabled;
  }

  // Otherwise check the pref via the work private helper
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->DOMCachesEnabled();
}

nsISupports*
Cache::GetParentObject() const
{
  return mGlobal;
}

JSObject*
Cache::WrapObject(JSContext* aContext, JS::Handle<JSObject*> aGivenProto)
{
  return CacheBinding::Wrap(aContext, this, aGivenProto);
}

void
Cache::DestroyInternal(CacheChild* aActor)
{
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mActor == aActor);
  mActor->ClearListener();
  mActor = nullptr;
}

void
Cache::RecvMatchResponse(RequestId aRequestId, nsresult aRv,
                         const PCacheResponseOrVoid& aResponse)
{
  // Convert the response immediately if its present.  This ensures that
  // any stream actors are cleaned up, even if we error out below.
  nsRefPtr<Response> response;
  if (aResponse.type() == PCacheResponseOrVoid::TPCacheResponse) {
    response = ToResponse(aResponse);
  }

  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);

  if (NS_FAILED(aRv)) {
    promise->MaybeReject(aRv);
    return;
  }

  if (!response) {
    promise->MaybeResolve(JS::UndefinedHandleValue);
    return;
  }

  promise->MaybeResolve(response);
}

void
Cache::RecvMatchAllResponse(RequestId aRequestId, nsresult aRv,
                            const nsTArray<PCacheResponse>& aResponses)
{
  // Convert responses immediately.  This ensures that any stream actors are
  // cleaned up, even if we error out below.
  nsAutoTArray<nsRefPtr<Response>, 256> responses;
  responses.SetCapacity(aResponses.Length());

  for (uint32_t i = 0; i < aResponses.Length(); ++i) {
    nsRefPtr<Response> response = ToResponse(aResponses[i]);
    responses.AppendElement(response.forget());
  }

  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);

  if (NS_FAILED(aRv)) {
    promise->MaybeReject(aRv);
    return;
  }

  promise->MaybeResolve(responses);
}

void
Cache::RecvAddAllResponse(RequestId aRequestId,
                          const mozilla::ErrorResult& aError)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);

  if (aError.Failed()) {
    // TODO: Remove this const_cast (bug 1152078).
    // It is safe for now since this ErrorResult is handed off to us by IPDL
    // and is thrown into the trash afterwards.
    promise->MaybeReject(const_cast<ErrorResult&>(aError));
    return;
  }

  promise->MaybeResolve(JS::UndefinedHandleValue);
}

void
Cache::RecvPutResponse(RequestId aRequestId, nsresult aRv)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);

  if (NS_FAILED(aRv)) {
    promise->MaybeReject(aRv);
    return;
  }

  promise->MaybeResolve(JS::UndefinedHandleValue);
}

void
Cache::RecvDeleteResponse(RequestId aRequestId, nsresult aRv, bool aSuccess)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);

  if (NS_FAILED(aRv)) {
    promise->MaybeReject(aRv);
    return;
  }

  promise->MaybeResolve(aSuccess);
}

void
Cache::RecvKeysResponse(RequestId aRequestId, nsresult aRv,
                        const nsTArray<PCacheRequest>& aRequests)
{
  // Convert requests immediately.  This ensures that any stream actors are
  // cleaned up, even if we error out below.
  nsAutoTArray<nsRefPtr<Request>, 256> requests;
  requests.SetCapacity(aRequests.Length());

  for (uint32_t i = 0; i < aRequests.Length(); ++i) {
    nsRefPtr<Request> request = ToRequest(aRequests[i]);
    requests.AppendElement(request.forget());
  }

  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);

  if (NS_FAILED(aRv)) {
    promise->MaybeReject(aRv);
    return;
  }

  promise->MaybeResolve(requests);
}

nsIGlobalObject*
Cache::GetGlobalObject() const
{
  return mGlobal;
}

#ifdef DEBUG
void
Cache::AssertOwningThread() const
{
  NS_ASSERT_OWNINGTHREAD(Cache);
}
#endif

CachePushStreamChild*
Cache::CreatePushStream(nsIAsyncInputStream* aStream)
{
  NS_ASSERT_OWNINGTHREAD(Cache);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(aStream);
  auto actor = mActor->SendPCachePushStreamConstructor(
    new CachePushStreamChild(mActor->GetFeature(), aStream));
  MOZ_ASSERT(actor);
  return static_cast<CachePushStreamChild*>(actor);
}

void
Cache::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  // Do nothing.  The Promise will automatically drop the ref to us after
  // calling the callback.  This is what we want as we only registered in order
  // to be held alive via the Promise handle.
}

void
Cache::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  // Do nothing.  The Promise will automatically drop the ref to us after
  // calling the callback.  This is what we want as we only registered in order
  // to be held alive via the Promise handle.
}

Cache::~Cache()
{
  DisconnectFromActor();
}

void
Cache::DisconnectFromActor()
{
  if (mActor) {
    mActor->StartDestroy();
    // DestroyInternal() is called synchronously by StartDestroy().  So we
    // should have already cleared the mActor.
    MOZ_ASSERT(!mActor);
  }
}

RequestId
Cache::AddRequestPromise(Promise* aPromise, ErrorResult& aRv)
{
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(!mRequestPromises.Contains(aPromise));

  // Register ourself as a promise handler so that the promise will hold us
  // alive.  This allows the client code to drop the ref to the Cache
  // object and just keep their promise.  This is fairly common in promise
  // chaining code.
  aPromise->AppendNativeHandler(this);

  mRequestPromises.AppendElement(aPromise);

  // (Ab)use the promise pointer as our request ID.  This is a fast, thread-safe
  // way to get a unique ID for the promise to be resolved later.
  return reinterpret_cast<RequestId>(aPromise);
}

already_AddRefed<Promise>
Cache::RemoveRequestPromise(RequestId aRequestId)
{
  MOZ_ASSERT(aRequestId != INVALID_REQUEST_ID);

  for (uint32_t i = 0; i < mRequestPromises.Length(); ++i) {
    nsRefPtr<Promise>& promise = mRequestPromises.ElementAt(i);
    // To be safe, only cast promise pointers to our integer RequestId
    // type and never cast an integer to a pointer.
    if (aRequestId == reinterpret_cast<RequestId>(promise.get())) {
      nsRefPtr<Promise> ref;
      ref.swap(promise);
      mRequestPromises.RemoveElementAt(i);
      return ref.forget();
    }
  }
  MOZ_ASSERT_UNREACHABLE("Received response without a matching promise!");
  return nullptr;
}

} // namespace cache
} // namespace dom
} // namespace mozilla
