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
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(mozilla::dom::cache::CacheStorage,
                                      mGlobal);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CacheStorage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIIPCBackgroundChildCreateCallback)
NS_INTERFACE_MAP_END

// We cannot reference IPC types in a webidl binding implementation header.  So
// define this in the .cpp and use heap storage in the mPendingRequests list.
struct CacheStorage::Entry final
{
  nsRefPtr<Promise> mPromise;
  CacheOpArgs mArgs;
  // We cannot add the requests until after the actor is present.  So store
  // the request data separately for now.
  nsRefPtr<InternalRequest> mRequest;
};

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
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
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
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
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
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  if (principalInfo.type() == PrincipalInfo::TContentPrincipalInfo &&
      principalInfo.get_ContentPrincipalInfo().appId() ==
      nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    NS_WARNING("CacheStorage not supported on principal with unknown appId.");
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
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

  if (mFailedActor) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<InternalRequest> request = ToInternalRequest(aRequest, IgnoreBody,
                                                        aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  CacheQueryParams params;
  ToCacheQueryParams(params, aOptions);

  nsAutoPtr<Entry> entry(new Entry());
  entry->mPromise = promise;
  entry->mArgs = StorageMatchArgs(CacheRequest(), params);
  entry->mRequest = request;

  mPendingRequests.AppendElement(entry.forget());
  MaybeRunPendingRequests();

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Has(const nsAString& aKey, ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (mFailedActor) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  nsAutoPtr<Entry> entry(new Entry());
  entry->mPromise = promise;
  entry->mArgs = StorageHasArgs(nsString(aKey));

  mPendingRequests.AppendElement(entry.forget());
  MaybeRunPendingRequests();

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Open(const nsAString& aKey, ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (mFailedActor) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  nsAutoPtr<Entry> entry(new Entry());
  entry->mPromise = promise;
  entry->mArgs = StorageOpenArgs(nsString(aKey));

  mPendingRequests.AppendElement(entry.forget());
  MaybeRunPendingRequests();

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Delete(const nsAString& aKey, ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (mFailedActor) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  nsAutoPtr<Entry> entry(new Entry());
  entry->mPromise = promise;
  entry->mArgs = StorageDeleteArgs(nsString(aKey));

  mPendingRequests.AppendElement(entry.forget());
  MaybeRunPendingRequests();

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Keys(ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (mFailedActor) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  nsAutoPtr<Entry> entry(new Entry());
  entry->mPromise = promise;
  entry->mArgs = StorageKeysArgs();

  mPendingRequests.AppendElement(entry.forget());
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
    nsAutoPtr<Entry> entry(mPendingRequests[i].forget());
    entry->mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
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

CacheStorage::~CacheStorage()
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
    ErrorResult rv;
    nsAutoPtr<Entry> entry(mPendingRequests[i].forget());
    AutoChildOpArgs args(this, entry->mArgs);
    if (entry->mRequest) {
      args.Add(entry->mRequest, IgnoreBody, IgnoreInvalidScheme, rv);
    }
    if (rv.Failed()) {
      entry->mPromise->MaybeReject(rv);
      continue;
    }
    mActor->ExecuteOp(mGlobal, entry->mPromise, args.SendAsOpArgs());
  }
  mPendingRequests.Clear();
}

} // namespace cache
} // namespace dom
} // namespace mozilla
