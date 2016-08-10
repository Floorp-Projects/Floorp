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

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::ipc::PBackgroundChild;

namespace {

void
AddWorkerHolderToStreamChild(const CacheReadStream& aReadStream,
                             CacheWorkerHolder* aWorkerHolder)
{
  MOZ_ASSERT_IF(!NS_IsMainThread(), aWorkerHolder);
  CacheStreamControlChild* cacheControl =
    static_cast<CacheStreamControlChild*>(aReadStream.controlChild());
  if (cacheControl) {
    cacheControl->SetWorkerHolder(aWorkerHolder);
  }
}

void
AddWorkerHolderToStreamChild(const CacheResponse& aResponse,
                             CacheWorkerHolder* aWorkerHolder)
{
  MOZ_ASSERT_IF(!NS_IsMainThread(), aWorkerHolder);

  if (aResponse.body().type() == CacheReadStreamOrVoid::Tvoid_t) {
    return;
  }

  AddWorkerHolderToStreamChild(aResponse.body().get_CacheReadStream(),
                               aWorkerHolder);
}

void
AddWorkerHolderToStreamChild(const CacheRequest& aRequest,
                             CacheWorkerHolder* aWorkerHolder)
{
  MOZ_ASSERT_IF(!NS_IsMainThread(), aWorkerHolder);

  if (aRequest.body().type() == CacheReadStreamOrVoid::Tvoid_t) {
    return;
  }

  AddWorkerHolderToStreamChild(aRequest.body().get_CacheReadStream(),
                               aWorkerHolder);
}

} // namespace

CacheOpChild::CacheOpChild(CacheWorkerHolder* aWorkerHolder,
                           nsIGlobalObject* aGlobal,
                           nsISupports* aParent, Promise* aPromise)
  : mGlobal(aGlobal)
  , mParent(aParent)
  , mPromise(aPromise)
{
  MOZ_ASSERT(mGlobal);
  MOZ_ASSERT(mParent);
  MOZ_ASSERT(mPromise);

  MOZ_ASSERT_IF(!NS_IsMainThread(), aWorkerHolder);
  SetWorkerHolder(aWorkerHolder);
}

CacheOpChild::~CacheOpChild()
{
  NS_ASSERT_OWNINGTHREAD(CacheOpChild);
  MOZ_ASSERT(!mPromise);
}

void
CacheOpChild::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CacheOpChild);

  // If the actor was terminated for some unknown reason, then indicate the
  // operation is dead.
  if (mPromise) {
    mPromise->MaybeReject(NS_ERROR_FAILURE);
    mPromise = nullptr;
  }

  RemoveWorkerHolder();
}

bool
CacheOpChild::Recv__delete__(const ErrorResult& aRv,
                             const CacheOpResult& aResult)
{
  NS_ASSERT_OWNINGTHREAD(CacheOpChild);

  if (NS_WARN_IF(aRv.Failed())) {
    MOZ_ASSERT(aResult.type() == CacheOpResult::Tvoid_t);
    // TODO: Remove this const_cast (bug 1152078).
    // It is safe for now since this ErrorResult is handed off to us by IPDL
    // and is thrown into the trash afterwards.
    mPromise->MaybeReject(const_cast<ErrorResult&>(aRv));
    mPromise = nullptr;
    return true;
  }

  switch (aResult.type()) {
    case CacheOpResult::TCacheMatchResult:
    {
      HandleResponse(aResult.get_CacheMatchResult().responseOrVoid());
      break;
    }
    case CacheOpResult::TCacheMatchAllResult:
    {
      HandleResponseList(aResult.get_CacheMatchAllResult().responseList());
      break;
    }
    case CacheOpResult::TCachePutAllResult:
    {
      mPromise->MaybeResolveWithUndefined();
      break;
    }
    case CacheOpResult::TCacheDeleteResult:
    {
      mPromise->MaybeResolve(aResult.get_CacheDeleteResult().success());
      break;
    }
    case CacheOpResult::TCacheKeysResult:
    {
      HandleRequestList(aResult.get_CacheKeysResult().requestList());
      break;
    }
    case CacheOpResult::TStorageMatchResult:
    {
      HandleResponse(aResult.get_StorageMatchResult().responseOrVoid());
      break;
    }
    case CacheOpResult::TStorageHasResult:
    {
      mPromise->MaybeResolve(aResult.get_StorageHasResult().success());
      break;
    }
    case CacheOpResult::TStorageOpenResult:
    {
      auto actor = static_cast<CacheChild*>(
        aResult.get_StorageOpenResult().actorChild());
      actor->SetWorkerHolder(GetWorkerHolder());
      RefPtr<Cache> cache = new Cache(mGlobal, actor);
      mPromise->MaybeResolve(cache);
      break;
    }
    case CacheOpResult::TStorageDeleteResult:
    {
      mPromise->MaybeResolve(aResult.get_StorageDeleteResult().success());
      break;
    }
    case CacheOpResult::TStorageKeysResult:
    {
      mPromise->MaybeResolve(aResult.get_StorageKeysResult().keyList());
      break;
    }
    default:
      MOZ_CRASH("Unknown Cache op result type!");
  }

  mPromise = nullptr;

  return true;
}

void
CacheOpChild::StartDestroy()
{
  NS_ASSERT_OWNINGTHREAD(CacheOpChild);

  // Do not cancel on-going operations when WorkerHolder calls this.  Instead,
  // keep the Worker alive until we are done.
}

nsIGlobalObject*
CacheOpChild::GetGlobalObject() const
{
  return mGlobal;
}

#ifdef DEBUG
void
CacheOpChild::AssertOwningThread() const
{
  NS_ASSERT_OWNINGTHREAD(CacheOpChild);
}
#endif

PBackgroundChild*
CacheOpChild::GetIPCManager()
{
  MOZ_CRASH("CacheOpChild does not implement TypeUtils::GetIPCManager()");
}

void
CacheOpChild::HandleResponse(const CacheResponseOrVoid& aResponseOrVoid)
{
  if (aResponseOrVoid.type() == CacheResponseOrVoid::Tvoid_t) {
    mPromise->MaybeResolveWithUndefined();
    return;
  }

  const CacheResponse& cacheResponse = aResponseOrVoid.get_CacheResponse();

  AddWorkerHolderToStreamChild(cacheResponse, GetWorkerHolder());
  RefPtr<Response> response = ToResponse(cacheResponse);

  mPromise->MaybeResolve(response);
}

void
CacheOpChild::HandleResponseList(const nsTArray<CacheResponse>& aResponseList)
{
  AutoTArray<RefPtr<Response>, 256> responses;
  responses.SetCapacity(aResponseList.Length());

  for (uint32_t i = 0; i < aResponseList.Length(); ++i) {
    AddWorkerHolderToStreamChild(aResponseList[i], GetWorkerHolder());
    responses.AppendElement(ToResponse(aResponseList[i]));
  }

  mPromise->MaybeResolve(responses);
}

void
CacheOpChild::HandleRequestList(const nsTArray<CacheRequest>& aRequestList)
{
  AutoTArray<RefPtr<Request>, 256> requests;
  requests.SetCapacity(aRequestList.Length());

  for (uint32_t i = 0; i < aRequestList.Length(); ++i) {
    AddWorkerHolderToStreamChild(aRequestList[i], GetWorkerHolder());
    requests.AppendElement(ToRequest(aRequestList[i]));
  }

  mPromise->MaybeResolve(requests);
}

} // namespace cache
} // namespace dom
} // namespace mozilla
