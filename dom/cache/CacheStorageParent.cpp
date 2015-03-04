/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStorageParent.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/cache/AutoUtils.h"
#include "mozilla/dom/cache/CacheParent.h"
#include "mozilla/dom/cache/CacheStreamControlParent.h"
#include "mozilla/dom/cache/Manager.h"
#include "mozilla/dom/cache/ManagerId.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/dom/cache/SavedTypes.h"
#include "mozilla/dom/cache/StreamList.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/PFileDescriptorSetParent.h"
#include "mozilla/DebugOnly.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::ipc::PBackgroundParent;
using mozilla::ipc::PFileDescriptorSetParent;
using mozilla::ipc::PrincipalInfo;

// declared in ActorUtils.h
PCacheStorageParent*
AllocPCacheStorageParent(PBackgroundParent* aManagingActor,
                         Namespace aNamespace,
                         const mozilla::ipc::PrincipalInfo& aPrincipalInfo)
{
  return new CacheStorageParent(aManagingActor, aNamespace, aPrincipalInfo);
}

// declared in ActorUtils.h
void
DeallocPCacheStorageParent(PCacheStorageParent* aActor)
{
  delete aActor;
}

CacheStorageParent::CacheStorageParent(PBackgroundParent* aManagingActor,
                                       Namespace aNamespace,
                                       const PrincipalInfo& aPrincipalInfo)
  : mNamespace(aNamespace)
  , mVerifiedStatus(NS_OK)
{
  MOZ_COUNT_CTOR(cache::CacheStorageParent);
  MOZ_ASSERT(aManagingActor);

  // Start the async principal verification process immediately.
  mVerifier = PrincipalVerifier::CreateAndDispatch(this, aManagingActor,
                                                   aPrincipalInfo);
  MOZ_ASSERT(mVerifier);
}

CacheStorageParent::~CacheStorageParent()
{
  MOZ_COUNT_DTOR(cache::CacheStorageParent);
  MOZ_ASSERT(!mVerifier);
  MOZ_ASSERT(!mManager);
}

void
CacheStorageParent::ActorDestroy(ActorDestroyReason aReason)
{
  if (mVerifier) {
    mVerifier->ClearListener();
    mVerifier = nullptr;
  }

  if (mManager) {
    MOZ_ASSERT(!mActiveRequests.IsEmpty());
    mManager->RemoveListener(this);
    mManager = nullptr;
  }
}

bool
CacheStorageParent::RecvTeardown()
{
  if (!Send__delete__(this)) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("CacheStorage failed to delete actor.");
  }
  return true;
}

bool
CacheStorageParent::RecvMatch(const RequestId& aRequestId,
                              const PCacheRequest& aRequest,
                              const PCacheQueryParams& aParams)
{
  if (NS_WARN_IF(NS_FAILED(mVerifiedStatus))) {
    if (!SendMatchResponse(aRequestId, mVerifiedStatus, void_t())) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("CacheStorage failed to send Match response.");
    }
    return true;
  }

  // queue requests if we are still waiting for principal verification
  if (!mManagerId) {
    Entry* entry = mPendingRequests.AppendElement();
    entry->mOp = OP_MATCH;
    entry->mRequestId = aRequestId;
    entry->mRequest = aRequest;
    entry->mParams = aParams;
    return true;
  }

  nsRefPtr<cache::Manager> manager;
  nsresult rv = RequestManager(aRequestId, getter_AddRefs(manager));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (!SendMatchResponse(aRequestId, rv, void_t())) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("CacheStorage failed to send Match response.");
    }
    return true;
  }

  manager->StorageMatch(this, aRequestId, mNamespace, aRequest,
                        aParams);

  return true;
}

bool
CacheStorageParent::RecvHas(const RequestId& aRequestId, const nsString& aKey)
{
  if (NS_WARN_IF(NS_FAILED(mVerifiedStatus))) {
    if (!SendHasResponse(aRequestId, mVerifiedStatus, false)) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("CacheStorage failed to send Has response.");
    }
    return true;
  }

  // queue requests if we are still waiting for principal verification
  if (!mManagerId) {
    Entry* entry = mPendingRequests.AppendElement();
    entry->mOp = OP_HAS;
    entry->mRequestId = aRequestId;
    entry->mKey = aKey;
    return true;
  }

  nsRefPtr<cache::Manager> manager;
  nsresult rv = RequestManager(aRequestId, getter_AddRefs(manager));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (!SendHasResponse(aRequestId, rv, false)) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("CacheStorage failed to send Has response.");
    }
    return true;
  }

  manager->StorageHas(this, aRequestId, mNamespace, aKey);

  return true;
}

bool
CacheStorageParent::RecvOpen(const RequestId& aRequestId, const nsString& aKey)
{
  if (NS_WARN_IF(NS_FAILED(mVerifiedStatus))) {
    if (!SendOpenResponse(aRequestId, mVerifiedStatus, nullptr)) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("CacheStorage failed to send Open response.");
    }
    return true;
  }

  // queue requests if we are still waiting for principal verification
  if (!mManagerId) {
    Entry* entry = mPendingRequests.AppendElement();
    entry->mOp = OP_OPEN;
    entry->mRequestId = aRequestId;
    entry->mKey = aKey;
    return true;
  }

  nsRefPtr<cache::Manager> manager;
  nsresult rv = RequestManager(aRequestId, getter_AddRefs(manager));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (!SendOpenResponse(aRequestId, rv, nullptr)) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("CacheStorage failed to send Open response.");
    }
    return true;
  }

  manager->StorageOpen(this, aRequestId, mNamespace, aKey);

  return true;
}

bool
CacheStorageParent::RecvDelete(const RequestId& aRequestId,
                               const nsString& aKey)
{
  if (NS_WARN_IF(NS_FAILED(mVerifiedStatus))) {
    if (!SendDeleteResponse(aRequestId, mVerifiedStatus, false)) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("CacheStorage failed to send Delete response.");
    }
    return true;
  }

  // queue requests if we are still waiting for principal verification
  if (!mManagerId) {
    Entry* entry = mPendingRequests.AppendElement();
    entry->mOp = OP_DELETE;
    entry->mRequestId = aRequestId;
    entry->mKey = aKey;
    return true;
  }

  nsRefPtr<cache::Manager> manager;
  nsresult rv = RequestManager(aRequestId, getter_AddRefs(manager));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (!SendDeleteResponse(aRequestId, rv, false)) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("CacheStorage failed to send Delete response.");
    }
    return true;
  }

  manager->StorageDelete(this, aRequestId, mNamespace, aKey);

  return true;
}

bool
CacheStorageParent::RecvKeys(const RequestId& aRequestId)
{
  if (NS_WARN_IF(NS_FAILED(mVerifiedStatus))) {
    if (!SendKeysResponse(aRequestId, mVerifiedStatus, nsTArray<nsString>())) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("CacheStorage failed to send Keys response.");
    }
  }

  // queue requests if we are still waiting for principal verification
  if (!mManagerId) {
    Entry* entry = mPendingRequests.AppendElement();
    entry->mOp = OP_DELETE;
    entry->mRequestId = aRequestId;
    return true;
  }

  nsRefPtr<cache::Manager> manager;
  nsresult rv = RequestManager(aRequestId, getter_AddRefs(manager));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (!SendKeysResponse(aRequestId, rv, nsTArray<nsString>())) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("CacheStorage failed to send Keys response.");
    }
    return true;
  }

  manager->StorageKeys(this, aRequestId, mNamespace);

  return true;
}

void
CacheStorageParent::OnPrincipalVerified(nsresult aRv, ManagerId* aManagerId)
{
  MOZ_ASSERT(mVerifier);
  MOZ_ASSERT(!mManagerId);
  MOZ_ASSERT(!mManager);
  MOZ_ASSERT(NS_SUCCEEDED(mVerifiedStatus));

  if (NS_WARN_IF(NS_FAILED(aRv))) {
    mVerifiedStatus = aRv;
  }

  mManagerId = aManagerId;
  mVerifier->ClearListener();
  mVerifier = nullptr;

  RetryPendingRequests();
}

void
CacheStorageParent::OnStorageMatch(RequestId aRequestId, nsresult aRv,
                                   const SavedResponse* aSavedResponse,
                                   StreamList* aStreamList)
{
  PCacheResponseOrVoid responseOrVoid;

  ReleaseManager(aRequestId);

  AutoParentResponseOrVoid response(Manager());

  // no match
  if (NS_FAILED(aRv) || !aSavedResponse) {
    if (!SendMatchResponse(aRequestId, aRv, response.SendAsResponseOrVoid())) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("CacheStorage failed to send Match response.");
    }
    return;
  }

  if (aSavedResponse) {
    response.Add(*aSavedResponse, aStreamList);
  }

  if (!SendMatchResponse(aRequestId, aRv, response.SendAsResponseOrVoid())) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("CacheStorage failed to send Match response.");
  }
}

void
CacheStorageParent::OnStorageHas(RequestId aRequestId, nsresult aRv,
                                 bool aCacheFound)
{
  ReleaseManager(aRequestId);
  if (!SendHasResponse(aRequestId, aRv, aCacheFound)) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("CacheStorage failed to send Has response.");
  }
}

void
CacheStorageParent::OnStorageOpen(RequestId aRequestId, nsresult aRv,
                                  CacheId aCacheId)
{
  if (NS_FAILED(aRv)) {
    ReleaseManager(aRequestId);
    if (!SendOpenResponse(aRequestId, aRv, nullptr)) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("CacheStorage failed to send Open response.");
    }
    return;
  }

  MOZ_ASSERT(mManager);
  CacheParent* actor = new CacheParent(mManager, aCacheId);

  ReleaseManager(aRequestId);

  PCacheParent* base = Manager()->SendPCacheConstructor(actor);
  actor = static_cast<CacheParent*>(base);
  if (!SendOpenResponse(aRequestId, aRv, actor)) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("CacheStorage failed to send Open response.");
  }
}

void
CacheStorageParent::OnStorageDelete(RequestId aRequestId, nsresult aRv,
                                    bool aCacheDeleted)
{
  ReleaseManager(aRequestId);
  if (!SendDeleteResponse(aRequestId, aRv, aCacheDeleted)) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("CacheStorage failed to send Delete response.");
  }
}

void
CacheStorageParent::OnStorageKeys(RequestId aRequestId, nsresult aRv,
                                  const nsTArray<nsString>& aKeys)
{
  ReleaseManager(aRequestId);
  if (!SendKeysResponse(aRequestId, aRv, aKeys)) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("CacheStorage failed to send Keys response.");
  }
}

void
CacheStorageParent::RetryPendingRequests()
{
  MOZ_ASSERT(mManagerId || NS_FAILED(mVerifiedStatus));
  for (uint32_t i = 0; i < mPendingRequests.Length(); ++i) {
    const Entry& entry = mPendingRequests[i];
    switch(entry.mOp) {
      case OP_MATCH:
        RecvMatch(entry.mRequestId, entry.mRequest, entry.mParams);
        break;
      case OP_HAS:
        RecvHas(entry.mRequestId, entry.mKey);
        break;
      case OP_OPEN:
        RecvOpen(entry.mRequestId, entry.mKey);
        break;
      case OP_DELETE:
        RecvDelete(entry.mRequestId, entry.mKey);
        break;
      case OP_KEYS:
        RecvKeys(entry.mRequestId);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Pending request within unknown op");
    }
  }
  mPendingRequests.Clear();
  mPendingRequests.Compact();
}

nsresult
CacheStorageParent::RequestManager(RequestId aRequestId,
                                   cache::Manager** aManagerOut)
{
  MOZ_ASSERT(!mActiveRequests.Contains(aRequestId));
  nsRefPtr<cache::Manager> ref = mManager;
  if (!ref) {
    MOZ_ASSERT(mActiveRequests.IsEmpty());
    nsresult rv = cache::Manager::GetOrCreate(mManagerId, getter_AddRefs(ref));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    mManager = ref;
  }
  mActiveRequests.AppendElement(aRequestId);
  ref.forget(aManagerOut);
  return NS_OK;
}

void
CacheStorageParent::ReleaseManager(RequestId aRequestId)
{
  // Note that if the child process dies we also clean up the mManager in
  // ActorDestroy().  There is no race with this method, however, because
  // ActorDestroy removes this object from the Manager's listener list.
  // Therefore ReleaseManager() should never be called after ActorDestroy()
  // runs.
  MOZ_ASSERT(mManager);
  MOZ_ASSERT(!mActiveRequests.IsEmpty());

  MOZ_ALWAYS_TRUE(mActiveRequests.RemoveElement(aRequestId));

  if (mActiveRequests.IsEmpty()) {
    mManager->RemoveListener(this);
    mManager = nullptr;
  }
}

} // namespace cache
} // namespace dom
} // namespace mozilla
