/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheOpChild.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/cache/Cache.h"
#include "mozilla/dom/cache/CacheChild.h"
#include "mozilla/dom/cache/CacheStreamControlChild.h"
#include "mozilla/dom/cache/CacheWorkerRef.h"

namespace mozilla {
namespace dom {
// XXX Move this to ToJSValue.h
template <typename T>
MOZ_MUST_USE bool ToJSValue(JSContext* aCx, const SafeRefPtr<T>& aArgument,
                            JS::MutableHandle<JS::Value> aValue) {
  return ToJSValue(aCx, *aArgument.unsafeGetRawPtr(), aValue);
}

namespace cache {

using mozilla::ipc::PBackgroundChild;

namespace {

void AddWorkerRefToStreamChild(const CacheReadStream& aReadStream,
                               const SafeRefPtr<CacheWorkerRef>& aWorkerRef) {
  MOZ_ASSERT_IF(!NS_IsMainThread(), aWorkerRef);
  CacheStreamControlChild* cacheControl =
      static_cast<CacheStreamControlChild*>(aReadStream.controlChild());
  if (cacheControl) {
    cacheControl->SetWorkerRef(aWorkerRef.clonePtr());
  }
}

void AddWorkerRefToStreamChild(const CacheResponse& aResponse,
                               const SafeRefPtr<CacheWorkerRef>& aWorkerRef) {
  MOZ_ASSERT_IF(!NS_IsMainThread(), aWorkerRef);

  if (aResponse.body().isNothing()) {
    return;
  }

  AddWorkerRefToStreamChild(aResponse.body().ref(), aWorkerRef);
}

void AddWorkerRefToStreamChild(const CacheRequest& aRequest,
                               const SafeRefPtr<CacheWorkerRef>& aWorkerRef) {
  MOZ_ASSERT_IF(!NS_IsMainThread(), aWorkerRef);

  if (aRequest.body().isNothing()) {
    return;
  }

  AddWorkerRefToStreamChild(aRequest.body().ref(), aWorkerRef);
}

}  // namespace

CacheOpChild::CacheOpChild(SafeRefPtr<CacheWorkerRef> aWorkerRef,
                           nsIGlobalObject* aGlobal, nsISupports* aParent,
                           Promise* aPromise)
    : mGlobal(aGlobal), mParent(aParent), mPromise(aPromise) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mParent);
  MOZ_DIAGNOSTIC_ASSERT(mPromise);

  MOZ_ASSERT_IF(!NS_IsMainThread(), aWorkerRef);

  SetWorkerRef(CacheWorkerRef::PreferBehavior(
      std::move(aWorkerRef), CacheWorkerRef::eStrongWorkerRef));
}

CacheOpChild::~CacheOpChild() {
  NS_ASSERT_OWNINGTHREAD(CacheOpChild);
  MOZ_DIAGNOSTIC_ASSERT(!mPromise);
}

void CacheOpChild::ActorDestroy(ActorDestroyReason aReason) {
  NS_ASSERT_OWNINGTHREAD(CacheOpChild);

  // If the actor was terminated for some unknown reason, then indicate the
  // operation is dead.
  if (mPromise) {
    mPromise->MaybeReject(NS_ERROR_FAILURE);
    mPromise = nullptr;
  }

  RemoveWorkerRef();
}

mozilla::ipc::IPCResult CacheOpChild::Recv__delete__(
    ErrorResult&& aRv, const CacheOpResult& aResult) {
  NS_ASSERT_OWNINGTHREAD(CacheOpChild);

  if (NS_WARN_IF(aRv.Failed())) {
    MOZ_DIAGNOSTIC_ASSERT(aResult.type() == CacheOpResult::Tvoid_t);
    mPromise->MaybeReject(std::move(aRv));
    mPromise = nullptr;
    return IPC_OK();
  }

  switch (aResult.type()) {
    case CacheOpResult::TCacheMatchResult: {
      HandleResponse(aResult.get_CacheMatchResult().maybeResponse());
      break;
    }
    case CacheOpResult::TCacheMatchAllResult: {
      HandleResponseList(aResult.get_CacheMatchAllResult().responseList());
      break;
    }
    case CacheOpResult::TCachePutAllResult: {
      mPromise->MaybeResolveWithUndefined();
      break;
    }
    case CacheOpResult::TCacheDeleteResult: {
      mPromise->MaybeResolve(aResult.get_CacheDeleteResult().success());
      break;
    }
    case CacheOpResult::TCacheKeysResult: {
      HandleRequestList(aResult.get_CacheKeysResult().requestList());
      break;
    }
    case CacheOpResult::TStorageMatchResult: {
      HandleResponse(aResult.get_StorageMatchResult().maybeResponse());
      break;
    }
    case CacheOpResult::TStorageHasResult: {
      mPromise->MaybeResolve(aResult.get_StorageHasResult().success());
      break;
    }
    case CacheOpResult::TStorageOpenResult: {
      auto result = aResult.get_StorageOpenResult();
      auto actor = static_cast<CacheChild*>(result.actorChild());

      // If we have a success status then we should have an actor.  Gracefully
      // reject instead of crashing, though, if we get a nullptr here.
      MOZ_DIAGNOSTIC_ASSERT(actor);
      if (!actor) {
        mPromise->MaybeRejectWithTypeError(
            "CacheStorage.open() failed to access the storage system.");
        break;
      }

      actor->SetWorkerRef(CacheWorkerRef::PreferBehavior(
          GetWorkerRefPtr().clonePtr(), CacheWorkerRef::eIPCWorkerRef));
      RefPtr<Cache> cache = new Cache(mGlobal, actor, result.ns());
      mPromise->MaybeResolve(cache);
      break;
    }
    case CacheOpResult::TStorageDeleteResult: {
      mPromise->MaybeResolve(aResult.get_StorageDeleteResult().success());
      break;
    }
    case CacheOpResult::TStorageKeysResult: {
      mPromise->MaybeResolve(aResult.get_StorageKeysResult().keyList());
      break;
    }
    default:
      MOZ_CRASH("Unknown Cache op result type!");
  }

  mPromise = nullptr;

  return IPC_OK();
}

void CacheOpChild::StartDestroy() {
  NS_ASSERT_OWNINGTHREAD(CacheOpChild);

  // Do not cancel on-going operations when WorkerRef calls this.  Instead,
  // keep the Worker alive until we are done.
}

nsIGlobalObject* CacheOpChild::GetGlobalObject() const { return mGlobal; }

#ifdef DEBUG
void CacheOpChild::AssertOwningThread() const {
  NS_ASSERT_OWNINGTHREAD(CacheOpChild);
}
#endif

PBackgroundChild* CacheOpChild::GetIPCManager() {
  MOZ_CRASH("CacheOpChild does not implement TypeUtils::GetIPCManager()");
}

void CacheOpChild::HandleResponse(const Maybe<CacheResponse>& aMaybeResponse) {
  if (aMaybeResponse.isNothing()) {
    mPromise->MaybeResolveWithUndefined();
    return;
  }

  const CacheResponse& cacheResponse = aMaybeResponse.ref();

  AddWorkerRefToStreamChild(cacheResponse, GetWorkerRefPtr());
  RefPtr<Response> response = ToResponse(cacheResponse);

  mPromise->MaybeResolve(response);
}

void CacheOpChild::HandleResponseList(
    const nsTArray<CacheResponse>& aResponseList) {
  AutoTArray<RefPtr<Response>, 256> responses;
  responses.SetCapacity(aResponseList.Length());

  for (uint32_t i = 0; i < aResponseList.Length(); ++i) {
    AddWorkerRefToStreamChild(aResponseList[i], GetWorkerRefPtr());
    responses.AppendElement(ToResponse(aResponseList[i]));
  }

  mPromise->MaybeResolve(responses);
}

void CacheOpChild::HandleRequestList(
    const nsTArray<CacheRequest>& aRequestList) {
  AutoTArray<SafeRefPtr<Request>, 256> requests;
  requests.SetCapacity(aRequestList.Length());

  for (uint32_t i = 0; i < aRequestList.Length(); ++i) {
    AddWorkerRefToStreamChild(aRequestList[i], GetWorkerRefPtr());
    requests.AppendElement(ToRequest(aRequestList[i]));
  }

  mPromise->MaybeResolve(requests);
}

}  // namespace cache
}  // namespace dom
}  // namespace mozilla
