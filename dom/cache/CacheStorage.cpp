/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStorage.h"

#include "mozilla/unused.h"
#include "mozilla/dom/CacheStorageBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/cache/AutoUtils.h"
#include "mozilla/dom/cache/Cache.h"
#include "mozilla/dom/cache/CacheChild.h"
#include "mozilla/dom/cache/CacheStorageChild.h"
#include "mozilla/dom/cache/Feature.h"
#include "mozilla/dom/cache/PCacheChild.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsIGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::unused;
using mozilla::ErrorResult;
using mozilla::dom::workers::WorkerPrivate;
using mozilla::ipc::BackgroundChild;
using mozilla::ipc::PBackgroundChild;
using mozilla::ipc::IProtocol;
using mozilla::ipc::PrincipalInfo;
using mozilla::ipc::PrincipalToPrincipalInfo;

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozilla::dom::cache::CacheStorage);
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozilla::dom::cache::CacheStorage);
NS_IMPL_CYCLE_COLLECTION_CLASS(mozilla::dom::cache::CacheStorage)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(mozilla::dom::cache::CacheStorage)
  tmp->DisconnectFromActor();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal, mRequestPromises)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(mozilla::dom::cache::CacheStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal, mRequestPromises)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(mozilla::dom::cache::CacheStorage)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CacheStorage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIIPCBackgroundChildCreateCallback)
  NS_INTERFACE_MAP_ENTRY(nsIIPCBackgroundChildCreateCallback)
NS_INTERFACE_MAP_END

// static
already_AddRefed<CacheStorage>
CacheStorage::CreateOnMainThread(Namespace aNamespace, nsIGlobalObject* aGlobal,
                                 nsIPrincipal* aPrincipal, ErrorResult& aRv)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(NS_IsMainThread());

  bool nullPrincipal;
  nsresult rv = aPrincipal->GetIsNullPrincipal(&nullPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  if (nullPrincipal) {
    NS_WARNING("CacheStorage not supported on null principal.");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // An unknown appId means that this principal was created for the codebase
  // without all the security information from the end document or worker.
  // We require exact knowledge of this information before allowing the
  // caller to touch the disk using the Cache API.
  bool unknownAppId = false;
  aPrincipal->GetUnknownAppId(&unknownAppId);
  if (unknownAppId) {
    NS_WARNING("CacheStorage not supported on principal with unknown appId.");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  PrincipalInfo principalInfo;
  rv = PrincipalToPrincipalInfo(aPrincipal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsRefPtr<CacheStorage> ref = new CacheStorage(aNamespace, aGlobal,
                                                principalInfo, nullptr);
  return ref.forget();
}

// static
already_AddRefed<CacheStorage>
CacheStorage::CreateOnWorker(Namespace aNamespace, nsIGlobalObject* aGlobal,
                             WorkerPrivate* aWorkerPrivate, ErrorResult& aRv)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  nsRefPtr<Feature> feature = Feature::Create(aWorkerPrivate);
  if (!feature) {
    NS_WARNING("Worker thread is shutting down.");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  const PrincipalInfo& principalInfo = aWorkerPrivate->GetPrincipalInfo();
  if (principalInfo.type() == PrincipalInfo::TNullPrincipalInfo) {
    NS_WARNING("CacheStorage not supported on null principal.");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (principalInfo.type() == PrincipalInfo::TContentPrincipalInfo &&
      principalInfo.get_ContentPrincipalInfo().appId() ==
      nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    NS_WARNING("CacheStorage not supported on principal with unknown appId.");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<CacheStorage> ref = new CacheStorage(aNamespace, aGlobal,
                                                principalInfo, feature);
  return ref.forget();
}

CacheStorage::CacheStorage(Namespace aNamespace, nsIGlobalObject* aGlobal,
                           const PrincipalInfo& aPrincipalInfo, Feature* aFeature)
  : mNamespace(aNamespace)
  , mGlobal(aGlobal)
  , mPrincipalInfo(MakeUnique<PrincipalInfo>(aPrincipalInfo))
  , mFeature(aFeature)
  , mActor(nullptr)
  , mFailedActor(false)
{
  MOZ_ASSERT(mGlobal);

  // If the PBackground actor is already initialized then we can
  // immediately use it
  PBackgroundChild* actor = BackgroundChild::GetForCurrentThread();
  if (actor) {
    ActorCreated(actor);
    return;
  }

  // Otherwise we must begin the PBackground initialization process and
  // wait for the async ActorCreated() callback.
  MOZ_ASSERT(NS_IsMainThread());
  bool ok = BackgroundChild::GetOrCreateForCurrentThread(this);
  if (!ok) {
    ActorFailed();
  }
}

already_AddRefed<Promise>
CacheStorage::Match(const RequestOrUSVString& aRequest,
                    const CacheQueryOptions& aOptions, ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  if (mFailedActor) {
    promise->MaybeReject(NS_ERROR_UNEXPECTED);
    return promise.forget();
  }

  RequestId requestId = AddRequestPromise(promise, aRv);

  Entry entry;
  entry.mRequestId = requestId;
  entry.mOp = OP_MATCH;
  entry.mOptions = aOptions;
  entry.mRequest = ToInternalRequest(aRequest, IgnoreBody, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  mPendingRequests.AppendElement(entry);

  MaybeRunPendingRequests();

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Has(const nsAString& aKey, ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  if (mFailedActor) {
    promise->MaybeReject(NS_ERROR_UNEXPECTED);
    return promise.forget();
  }

  RequestId requestId = AddRequestPromise(promise, aRv);

  Entry* entry = mPendingRequests.AppendElement();
  entry->mRequestId = requestId;
  entry->mOp = OP_HAS;
  entry->mKey = aKey;

  MaybeRunPendingRequests();

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Open(const nsAString& aKey, ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  if (mFailedActor) {
    promise->MaybeReject(NS_ERROR_UNEXPECTED);
    return promise.forget();
  }

  RequestId requestId = AddRequestPromise(promise, aRv);

  Entry* entry = mPendingRequests.AppendElement();
  entry->mRequestId = requestId;
  entry->mOp = OP_OPEN;
  entry->mKey = aKey;

  MaybeRunPendingRequests();

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Delete(const nsAString& aKey, ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  if (mFailedActor) {
    promise->MaybeReject(NS_ERROR_UNEXPECTED);
    return promise.forget();
  }

  RequestId requestId = AddRequestPromise(promise, aRv);

  Entry* entry = mPendingRequests.AppendElement();
  entry->mRequestId = requestId;
  entry->mOp = OP_DELETE;
  entry->mKey = aKey;

  MaybeRunPendingRequests();

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Keys(ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  if (mFailedActor) {
    promise->MaybeReject(NS_ERROR_UNEXPECTED);
    return promise.forget();
  }

  RequestId requestId = AddRequestPromise(promise, aRv);

  Entry* entry = mPendingRequests.AppendElement();
  entry->mRequestId = requestId;
  entry->mOp = OP_KEYS;

  MaybeRunPendingRequests();

  return promise.forget();
}

// static
bool
CacheStorage::PrefEnabled(JSContext* aCx, JSObject* aObj)
{
  return Cache::PrefEnabled(aCx, aObj);
}

nsISupports*
CacheStorage::GetParentObject() const
{
  return mGlobal;
}

JSObject*
CacheStorage::WrapObject(JSContext* aContext, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::CacheStorageBinding::Wrap(aContext, this, aGivenProto);
}

void
CacheStorage::ActorCreated(PBackgroundChild* aActor)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);
  MOZ_ASSERT(aActor);

  if (NS_WARN_IF(mFeature && mFeature->Notified())) {
    ActorFailed();
    return;
  }

  // Feature ownership is passed to the CacheStorageChild actor and any actors
  // it may create.  The Feature will keep the worker thread alive until the
  // actors can gracefully shutdown.
  CacheStorageChild* newActor = new CacheStorageChild(this, mFeature);
  PCacheStorageChild* constructedActor =
    aActor->SendPCacheStorageConstructor(newActor, mNamespace, *mPrincipalInfo);

  if (NS_WARN_IF(!constructedActor)) {
    ActorFailed();
    return;
  }

  mFeature = nullptr;

  MOZ_ASSERT(constructedActor == newActor);
  mActor = newActor;

  MaybeRunPendingRequests();
  MOZ_ASSERT(mPendingRequests.IsEmpty());
}

void
CacheStorage::ActorFailed()
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);
  MOZ_ASSERT(!mFailedActor);

  mFailedActor = true;
  mFeature = nullptr;

  for (uint32_t i = 0; i < mPendingRequests.Length(); ++i) {
    RequestId requestId = mPendingRequests[i].mRequestId;
    nsRefPtr<Promise> promise = RemoveRequestPromise(requestId);
    promise->MaybeReject(NS_ERROR_UNEXPECTED);
  }
  mPendingRequests.Clear();
}

void
CacheStorage::DestroyInternal(CacheStorageChild* aActor)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mActor == aActor);
  mActor->ClearListener();
  mActor = nullptr;

  // Note that we will never get an actor again in case another request is
  // made before this object is destructed.
  ActorFailed();
}

void
CacheStorage::RecvMatchResponse(RequestId aRequestId, nsresult aRv,
                                const PCacheResponseOrVoid& aResponse)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

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

  // If cache name was specified in the request options and the cache does
  // not exist, then an error code will already have been set.  If we
  // still do not have a response, then we just resolve undefined like a
  // normal Cache::Match.
  if (!response) {
    promise->MaybeResolve(JS::UndefinedHandleValue);
    return;
  }

  promise->MaybeResolve(response);
}

void
CacheStorage::RecvHasResponse(RequestId aRequestId, nsresult aRv, bool aSuccess)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);

  if (NS_FAILED(aRv)) {
    promise->MaybeReject(aRv);
    return;

  }

  promise->MaybeResolve(aSuccess);
}

void
CacheStorage::RecvOpenResponse(RequestId aRequestId, nsresult aRv,
                               CacheChild* aActor)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  // Unlike most of our async callback Recv*() methods, this one gets back
  // an actor.  We need to make sure to clean it up in case of error.

  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);

  if (NS_FAILED(aRv)) {
    if (aActor) {
      // We cannot use the CacheChild::StartDestroy() method because there
      // is no Cache object associated with the actor yet.  Instead, just
      // send the underlying Teardown message.
      unused << aActor->SendTeardown();
    }
    promise->MaybeReject(aRv);
    return;
  }

  if (!aActor) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  nsRefPtr<Cache> cache = new Cache(mGlobal, aActor);
  promise->MaybeResolve(cache);
}

void
CacheStorage::RecvDeleteResponse(RequestId aRequestId, nsresult aRv,
                                 bool aSuccess)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);

  if (NS_FAILED(aRv)) {
    promise->MaybeReject(aRv);
    return;
  }

  promise->MaybeResolve(aSuccess);
}

void
CacheStorage::RecvKeysResponse(RequestId aRequestId, nsresult aRv,
                               const nsTArray<nsString>& aKeys)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);

  if (NS_FAILED(aRv)) {
    promise->MaybeReject(aRv);
    return;
  }

  promise->MaybeResolve(aKeys);
}

nsIGlobalObject*
CacheStorage::GetGlobalObject() const
{
  return mGlobal;
}

#ifdef DEBUG
void
CacheStorage::AssertOwningThread() const
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);
}
#endif

CachePushStreamChild*
CacheStorage::CreatePushStream(nsIAsyncInputStream* aStream)
{
  // This is true because CacheStorage always uses IgnoreBody for requests.
  MOZ_CRASH("CacheStorage should never create a push stream.");
}

void
CacheStorage::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  // Do nothing.  The Promise will automatically drop the ref to us after
  // calling the callback.  This is what we want as we only registered in order
  // to be held alive via the Promise handle.
}

void
CacheStorage::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  // Do nothing.  The Promise will automatically drop the ref to us after
  // calling the callback.  This is what we want as we only registered in order
  // to be held alive via the Promise handle.
}

CacheStorage::~CacheStorage()
{
  DisconnectFromActor();
}

void
CacheStorage::DisconnectFromActor()
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (mActor) {
    mActor->StartDestroy();
    // DestroyInternal() is called synchronously by StartDestroy().  So we
    // should have already cleared the mActor.
    MOZ_ASSERT(!mActor);
  }
}

void
CacheStorage::MaybeRunPendingRequests()
{
  if (!mActor) {
    return;
  }

  for (uint32_t i = 0; i < mPendingRequests.Length(); ++i) {
    // Note, the entry can be modified below due to Request/Response body
    // being marked used.
    Entry& entry = mPendingRequests[i];
    RequestId requestId = entry.mRequestId;
    switch(entry.mOp) {
      case OP_MATCH:
      {
        AutoChildRequest request(this);
        ErrorResult rv;
        request.Add(entry.mRequest, IgnoreBody, PassThroughReferrer,
                    IgnoreInvalidScheme, rv);
        if (NS_WARN_IF(rv.Failed())) {
          nsRefPtr<Promise> promise = RemoveRequestPromise(requestId);
          promise->MaybeReject(rv);
          break;
        }

        PCacheQueryParams params;
        ToPCacheQueryParams(params, entry.mOptions);

        unused << mActor->SendMatch(requestId, request.SendAsRequest(), params);
        break;
      }
      case OP_HAS:
        unused << mActor->SendHas(requestId, entry.mKey);
        break;
      case OP_OPEN:
        unused << mActor->SendOpen(requestId, entry.mKey);
        break;
      case OP_DELETE:
        unused << mActor->SendDelete(requestId, entry.mKey);
        break;
      case OP_KEYS:
        unused << mActor->SendKeys(requestId);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown pending CacheStorage op.");
    }
  }
  mPendingRequests.Clear();
}

RequestId
CacheStorage::AddRequestPromise(Promise* aPromise, ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(!mRequestPromises.Contains(aPromise));

  // Register ourself as a promise handler so that the promise will hold us
  // alive.  This allows the client code to drop the ref to the CacheStorage
  // object and just keep their promise.  This is fairly common in promise
  // chaining code.
  aPromise->AppendNativeHandler(this);

  mRequestPromises.AppendElement(aPromise);

  // (Ab)use the promise pointer as our request ID.  This is a fast, thread-safe
  // way to get a unique ID for the promise to be resolved later.
  return reinterpret_cast<RequestId>(aPromise);
}

already_AddRefed<Promise>
CacheStorage::RemoveRequestPromise(RequestId aRequestId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);
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
