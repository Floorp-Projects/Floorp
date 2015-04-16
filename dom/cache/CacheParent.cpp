/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheParent.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/cache/AutoUtils.h"
#include "mozilla/dom/cache/CachePushStreamParent.h"
#include "mozilla/dom/cache/CacheStreamControlParent.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/dom/cache/SavedTypes.h"
#include "mozilla/dom/cache/StreamList.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/PFileDescriptorSetParent.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::dom::ErrNum;
using mozilla::ipc::FileDescriptorSetParent;
using mozilla::ipc::PFileDescriptorSetParent;

// Declared in ActorUtils.h
void
DeallocPCacheParent(PCacheParent* aActor)
{
  delete aActor;
}

CacheParent::CacheParent(cache::Manager* aManager, CacheId aCacheId)
  : mManager(aManager)
  , mCacheId(aCacheId)
{
  MOZ_COUNT_CTOR(cache::CacheParent);
  MOZ_ASSERT(mManager);
  mManager->AddRefCacheId(mCacheId);
}

CacheParent::~CacheParent()
{
  MOZ_COUNT_DTOR(cache::CacheParent);
  MOZ_ASSERT(!mManager);
  MOZ_ASSERT(mFetchPutList.IsEmpty());
}

void
CacheParent::ActorDestroy(ActorDestroyReason aReason)
{
  MOZ_ASSERT(mManager);
  for (uint32_t i = 0; i < mFetchPutList.Length(); ++i) {
    mFetchPutList[i]->ClearListener();
  }
  mFetchPutList.Clear();
  mManager->RemoveListener(this);
  mManager->ReleaseCacheId(mCacheId);
  mManager = nullptr;
}

PCachePushStreamParent*
CacheParent::AllocPCachePushStreamParent()
{
  return CachePushStreamParent::Create();
}

bool
CacheParent::DeallocPCachePushStreamParent(PCachePushStreamParent* aActor)
{
  delete aActor;
  return true;
}

bool
CacheParent::RecvTeardown()
{
  if (!Send__delete__(this)) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("Cache failed to send delete.");
  }
  return true;
}

bool
CacheParent::RecvMatch(const RequestId& aRequestId, const PCacheRequest& aRequest,
                       const PCacheQueryParams& aParams)
{
  MOZ_ASSERT(mManager);
  mManager->CacheMatch(this, aRequestId, mCacheId, aRequest,
                       aParams);
  return true;
}

bool
CacheParent::RecvMatchAll(const RequestId& aRequestId,
                          const PCacheRequestOrVoid& aRequest,
                          const PCacheQueryParams& aParams)
{
  MOZ_ASSERT(mManager);
  mManager->CacheMatchAll(this, aRequestId, mCacheId, aRequest, aParams);
  return true;
}

bool
CacheParent::RecvAddAll(const RequestId& aRequestId,
                        nsTArray<PCacheRequest>&& aRequests)
{
  nsAutoTArray<nsCOMPtr<nsIInputStream>, 256> requestStreams;
  requestStreams.SetCapacity(aRequests.Length());

  for (uint32_t i = 0; i < aRequests.Length(); ++i) {
    requestStreams.AppendElement(DeserializeCacheStream(aRequests[i].body()));
  }

  nsRefPtr<FetchPut> fetchPut;
  nsresult rv = FetchPut::Create(this, mManager, aRequestId, mCacheId,
                                 aRequests, requestStreams,
                                 getter_AddRefs(fetchPut));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_ASSERT(rv != NS_ERROR_TYPE_ERR);
    ErrorResult error;
    error.Throw(rv);
    if (!SendAddAllResponse(aRequestId, error)) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("Cache failed to send AddAll response.");
    }
    return true;
  }

  mFetchPutList.AppendElement(fetchPut.forget());

  return true;
}

bool
CacheParent::RecvPut(const RequestId& aRequestId,
                     const CacheRequestResponse& aPut)
{
  MOZ_ASSERT(mManager);

  nsAutoTArray<CacheRequestResponse, 1> putList;
  putList.AppendElement(aPut);

  nsAutoTArray<nsCOMPtr<nsIInputStream>, 1> requestStreamList;
  nsAutoTArray<nsCOMPtr<nsIInputStream>, 1> responseStreamList;

  requestStreamList.AppendElement(
    DeserializeCacheStream(aPut.request().body()));
  responseStreamList.AppendElement(
    DeserializeCacheStream(aPut.response().body()));


  mManager->CachePutAll(this, aRequestId, mCacheId, putList, requestStreamList,
                        responseStreamList);

  return true;
}

bool
CacheParent::RecvDelete(const RequestId& aRequestId,
                        const PCacheRequest& aRequest,
                        const PCacheQueryParams& aParams)
{
  MOZ_ASSERT(mManager);
  mManager->CacheDelete(this, aRequestId, mCacheId, aRequest, aParams);
  return true;
}

bool
CacheParent::RecvKeys(const RequestId& aRequestId,
                      const PCacheRequestOrVoid& aRequest,
                      const PCacheQueryParams& aParams)
{
  MOZ_ASSERT(mManager);
  mManager->CacheKeys(this, aRequestId, mCacheId, aRequest, aParams);
  return true;
}

void
CacheParent::OnCacheMatch(RequestId aRequestId, nsresult aRv,
                          const SavedResponse* aSavedResponse,
                          StreamList* aStreamList)
{
  AutoParentResponseOrVoid response(Manager());

  // no match
  if (NS_FAILED(aRv) || !aSavedResponse || !aStreamList) {
    if (!SendMatchResponse(aRequestId, aRv, response.SendAsResponseOrVoid())) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("Cache failed to send Match response.");
    }
    return;
  }

  if (aSavedResponse) {
    response.Add(*aSavedResponse, aStreamList);
  }

  if (!SendMatchResponse(aRequestId, aRv, response.SendAsResponseOrVoid())) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("Cache failed to send Match response.");
  }
}

void
CacheParent::OnCacheMatchAll(RequestId aRequestId, nsresult aRv,
                             const nsTArray<SavedResponse>& aSavedResponses,
                             StreamList* aStreamList)
{
  AutoParentResponseList responses(Manager(), aSavedResponses.Length());

  for (uint32_t i = 0; i < aSavedResponses.Length(); ++i) {
    responses.Add(aSavedResponses[i], aStreamList);
  }

  if (!SendMatchAllResponse(aRequestId, aRv, responses.SendAsResponseList())) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("Cache failed to send MatchAll response.");
  }
}

void
CacheParent::OnCachePutAll(RequestId aRequestId, nsresult aRv)
{
  if (!SendPutResponse(aRequestId, aRv)) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("Cache failed to send Put response.");
  }
}

void
CacheParent::OnCacheDelete(RequestId aRequestId, nsresult aRv, bool aSuccess)
{
  if (!SendDeleteResponse(aRequestId, aRv, aSuccess)) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("Cache failed to send Delete response.");
  }
}

void
CacheParent::OnCacheKeys(RequestId aRequestId, nsresult aRv,
                         const nsTArray<SavedRequest>& aSavedRequests,
                         StreamList* aStreamList)
{
  AutoParentRequestList requests(Manager(), aSavedRequests.Length());

  for (uint32_t i = 0; i < aSavedRequests.Length(); ++i) {
    requests.Add(aSavedRequests[i], aStreamList);
  }

  if (!SendKeysResponse(aRequestId, aRv, requests.SendAsRequestList())) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("Cache failed to send Keys response.");
  }
}

void
CacheParent::OnFetchPut(FetchPut* aFetchPut, RequestId aRequestId, const ErrorResult& aRv)
{
  aFetchPut->ClearListener();
  mFetchPutList.RemoveElement(aFetchPut);
  if (!SendAddAllResponse(aRequestId, aRv)) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("Cache failed to send AddAll response.");
  }
}

already_AddRefed<nsIInputStream>
CacheParent::DeserializeCacheStream(const PCacheReadStreamOrVoid& aStreamOrVoid)
{
  if (aStreamOrVoid.type() == PCacheReadStreamOrVoid::Tvoid_t) {
    return nullptr;
  }

  nsCOMPtr<nsIInputStream> stream;
  const PCacheReadStream& readStream = aStreamOrVoid.get_PCacheReadStream();

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
} // namesapce mozilla
