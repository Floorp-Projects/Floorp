/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheOpParent.h"

#include "mozilla/unused.h"
#include "mozilla/dom/cache/AutoUtils.h"
#include "mozilla/dom/cache/CachePushStreamParent.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/dom/cache/SavedTypes.h"
#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/InputStreamUtils.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::ipc::FileDescriptorSetParent;
using mozilla::ipc::PBackgroundParent;

CacheOpParent::CacheOpParent(PBackgroundParent* aIpcManager, CacheId aCacheId,
                             const CacheOpArgs& aOpArgs)
  : mIpcManager(aIpcManager)
  , mCacheId(aCacheId)
  , mNamespace(INVALID_NAMESPACE)
  , mOpArgs(aOpArgs)
{
  MOZ_ASSERT(mIpcManager);
}

CacheOpParent::CacheOpParent(PBackgroundParent* aIpcManager,
                             Namespace aNamespace, const CacheOpArgs& aOpArgs)
  : mIpcManager(aIpcManager)
  , mCacheId(INVALID_CACHE_ID)
  , mNamespace(aNamespace)
  , mOpArgs(aOpArgs)
{
  MOZ_ASSERT(mIpcManager);
}

CacheOpParent::~CacheOpParent()
{
  NS_ASSERT_OWNINGTHREAD(CacheOpParent);
}

void
CacheOpParent::Execute(ManagerId* aManagerId)
{
  NS_ASSERT_OWNINGTHREAD(CacheOpParent);
  MOZ_ASSERT(!mManager);
  MOZ_ASSERT(!mVerifier);

  nsRefPtr<Manager> manager;
  nsresult rv = Manager::GetOrCreate(aManagerId, getter_AddRefs(manager));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    unused << Send__delete__(this, ErrorResult(rv), void_t());
    return;
  }

  Execute(manager);
}

void
CacheOpParent::Execute(Manager* aManager)
{
  NS_ASSERT_OWNINGTHREAD(CacheOpParent);
  MOZ_ASSERT(!mManager);
  MOZ_ASSERT(!mVerifier);

  mManager = aManager;

  // Handle add/addAll op with a FetchPut object
  if (mOpArgs.type() == CacheOpArgs::TCacheAddAllArgs) {
    MOZ_ASSERT(mCacheId != INVALID_CACHE_ID);

    const CacheAddAllArgs& args = mOpArgs.get_CacheAddAllArgs();
    const nsTArray<CacheRequest>& list = args.requestList();

    nsAutoTArray<nsCOMPtr<nsIInputStream>, 256> requestStreamList;
    for (uint32_t i = 0; i < list.Length(); ++i) {
      requestStreamList.AppendElement(DeserializeCacheStream(list[i].body()));
    }

    nsRefPtr<FetchPut> fetchPut;
    nsresult rv = FetchPut::Create(this, mManager, mCacheId, list,
                                   requestStreamList, getter_AddRefs(fetchPut));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      OnOpComplete(ErrorResult(rv), CacheAddAllResult());
      return;
    }

    mFetchPutList.AppendElement(fetchPut.forget());
    return;
  }

  // Handle put op
  if (mOpArgs.type() == CacheOpArgs::TCachePutAllArgs) {
    MOZ_ASSERT(mCacheId != INVALID_CACHE_ID);

    const CachePutAllArgs& args = mOpArgs.get_CachePutAllArgs();
    const nsTArray<CacheRequestResponse>& list = args.requestResponseList();

    nsAutoTArray<nsCOMPtr<nsIInputStream>, 256> requestStreamList;
    nsAutoTArray<nsCOMPtr<nsIInputStream>, 256> responseStreamList;

    for (uint32_t i = 0; i < list.Length(); ++i) {
      requestStreamList.AppendElement(
        DeserializeCacheStream(list[i].request().body()));
      responseStreamList.AppendElement(
        DeserializeCacheStream(list[i].response().body()));
    }

    mManager->ExecutePutAll(this, mCacheId, args.requestResponseList(),
                            requestStreamList, responseStreamList);
    return;
  }

  // Handle all other cache ops
  if (mCacheId != INVALID_CACHE_ID) {
    MOZ_ASSERT(mNamespace == INVALID_NAMESPACE);
    mManager->ExecuteCacheOp(this, mCacheId, mOpArgs);
    return;
  }

  // Handle all storage ops
  MOZ_ASSERT(mNamespace != INVALID_NAMESPACE);
  mManager->ExecuteStorageOp(this, mNamespace, mOpArgs);
}

void
CacheOpParent::WaitForVerification(PrincipalVerifier* aVerifier)
{
  NS_ASSERT_OWNINGTHREAD(CacheOpParent);
  MOZ_ASSERT(!mManager);
  MOZ_ASSERT(!mVerifier);

  mVerifier = aVerifier;
  mVerifier->AddListener(this);
}

void
CacheOpParent::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CacheOpParent);

  if (mVerifier) {
    mVerifier->RemoveListener(this);
    mVerifier = nullptr;
  }

  for (uint32_t i = 0; i < mFetchPutList.Length(); ++i) {
    mFetchPutList[i]->ClearListener();
  }
  mFetchPutList.Clear();

  if (mManager) {
    mManager->RemoveListener(this);
    mManager = nullptr;
  }

  mIpcManager = nullptr;
}

void
CacheOpParent::OnPrincipalVerified(nsresult aRv, ManagerId* aManagerId)
{
  NS_ASSERT_OWNINGTHREAD(CacheOpParent);

  mVerifier->RemoveListener(this);
  mVerifier = nullptr;

  if (NS_WARN_IF(NS_FAILED(aRv))) {
    unused << Send__delete__(this, ErrorResult(aRv), void_t());
    return;
  }

  Execute(aManagerId);
}

void
CacheOpParent::OnOpComplete(ErrorResult&& aRv, const CacheOpResult& aResult,
                            CacheId aOpenedCacheId,
                            const nsTArray<SavedResponse>& aSavedResponseList,
                            const nsTArray<SavedRequest>& aSavedRequestList,
                            StreamList* aStreamList)
{
  NS_ASSERT_OWNINGTHREAD(CacheOpParent);
  MOZ_ASSERT(mIpcManager);
  MOZ_ASSERT(mManager);

  // Never send an op-specific result if we have an error.  Instead, send
  // void_t() to ensure that we don't leak actors on the child side.
  if (aRv.Failed()) {
    unused << Send__delete__(this, aRv, void_t());
    aRv.ClearMessage(); // This may contain a TypeError.
    return;
  }

  // The result must contain the appropriate type at this point.  It may
  // or may not contain the additional result data yet.  For types that
  // do not need special processing, it should already be set.  If the
  // result requires actor-specific operations, then we do that below.
  // If the type and data types don't match, then we will trigger an
  // assertion in AutoParentOpResult::Add().
  AutoParentOpResult result(mIpcManager, aResult);

  if (aOpenedCacheId != INVALID_CACHE_ID) {
    result.Add(aOpenedCacheId, mManager);
  }

  for (uint32_t i = 0; i < aSavedResponseList.Length(); ++i) {
    result.Add(aSavedResponseList[i], aStreamList);
  }

  for (uint32_t i = 0; i < aSavedRequestList.Length(); ++i) {
    result.Add(aSavedRequestList[i], aStreamList);
  }

  unused << Send__delete__(this, aRv, result.SendAsOpResult());
}

void
CacheOpParent::OnFetchPut(FetchPut* aFetchPut, ErrorResult&& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheOpParent);
  MOZ_ASSERT(aFetchPut);

  aFetchPut->ClearListener();
  MOZ_ALWAYS_TRUE(mFetchPutList.RemoveElement(aFetchPut));

  OnOpComplete(Move(aRv), CacheAddAllResult());
}

already_AddRefed<nsIInputStream>
CacheOpParent::DeserializeCacheStream(const CacheReadStreamOrVoid& aStreamOrVoid)
{
  if (aStreamOrVoid.type() == CacheReadStreamOrVoid::Tvoid_t) {
    return nullptr;
  }

  nsCOMPtr<nsIInputStream> stream;
  const CacheReadStream& readStream = aStreamOrVoid.get_CacheReadStream();

  // Option 1: A push stream actor was sent for nsPipe data
  if (readStream.pushStreamParent()) {
    MOZ_ASSERT(!readStream.controlParent());
    CachePushStreamParent* pushStream =
      static_cast<CachePushStreamParent*>(readStream.pushStreamParent());
    stream = pushStream->TakeReader();
    MOZ_ASSERT(stream);
    return stream.forget();
  }

  // Option 2: One of our own ReadStreams was passed back to us with a stream
  //           control actor.
  stream = ReadStream::Create(readStream);
  if (stream) {
    return stream.forget();
  }

  // Option 3: A stream was serialized using normal methods.
  nsAutoTArray<FileDescriptor, 4> fds;
  if (readStream.fds().type() ==
      OptionalFileDescriptorSet::TPFileDescriptorSetChild) {

    FileDescriptorSetParent* fdSetActor =
      static_cast<FileDescriptorSetParent*>(readStream.fds().get_PFileDescriptorSetParent());
    MOZ_ASSERT(fdSetActor);

    fdSetActor->ForgetFileDescriptors(fds);
    MOZ_ASSERT(!fds.IsEmpty());

    if (!fdSetActor->Send__delete__(fdSetActor)) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("Cache failed to delete fd set actor.");
    }
  }

  return DeserializeInputStream(readStream.params(), fds);
}

} // namespace cache
} // namespace dom
} // namespace mozilla
