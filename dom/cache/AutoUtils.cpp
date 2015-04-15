/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/AutoUtils.h"

#include "mozilla/unused.h"
#include "mozilla/dom/cache/CachePushStreamChild.h"
#include "mozilla/dom/cache/CacheStreamControlParent.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/dom/cache/SavedTypes.h"
#include "mozilla/dom/cache/StreamList.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/PBackgroundParent.h"

namespace {

using mozilla::unused;
using mozilla::dom::cache::CachePushStreamChild;
using mozilla::dom::cache::PCacheReadStream;
using mozilla::dom::cache::PCacheReadStreamOrVoid;
using mozilla::ipc::FileDescriptor;
using mozilla::ipc::FileDescriptorSetChild;
using mozilla::ipc::FileDescriptorSetParent;
using mozilla::ipc::OptionalFileDescriptorSet;

enum CleanupAction
{
  Forget,
  Delete
};

void
CleanupChildFds(PCacheReadStream& aReadStream, CleanupAction aAction)
{
  if (aReadStream.fds().type() !=
      OptionalFileDescriptorSet::TPFileDescriptorSetChild) {
    return;
  }

  nsAutoTArray<FileDescriptor, 4> fds;

  FileDescriptorSetChild* fdSetActor =
    static_cast<FileDescriptorSetChild*>(aReadStream.fds().get_PFileDescriptorSetChild());
  MOZ_ASSERT(fdSetActor);

  if (aAction == Delete) {
    unused << fdSetActor->Send__delete__(fdSetActor);
  }

  // FileDescriptorSet doesn't clear its fds in its ActorDestroy, so we
  // unconditionally forget them here.  The fds themselves are auto-closed in
  // ~FileDescriptor since they originated in this process.
  fdSetActor->ForgetFileDescriptors(fds);
}

void
CleanupChildPushStream(PCacheReadStream& aReadStream, CleanupAction aAction)
{
  if (!aReadStream.pushStreamChild()) {
    return;
  }

  auto pushStream =
    static_cast<CachePushStreamChild*>(aReadStream.pushStreamChild());

  if (aAction == Delete) {
    pushStream->StartDestroy();
    return;
  }

  // If we send the stream, then we need to start it before forgetting about it.
  pushStream->Start();
}

void
CleanupChild(PCacheReadStream& aReadStream, CleanupAction aAction)
{
  CleanupChildFds(aReadStream, aAction);
  CleanupChildPushStream(aReadStream, aAction);
}

void
CleanupChild(PCacheReadStreamOrVoid& aReadStreamOrVoid, CleanupAction aAction)
{
  if (aReadStreamOrVoid.type() == PCacheReadStreamOrVoid::Tvoid_t) {
    return;
  }

  CleanupChild(aReadStreamOrVoid.get_PCacheReadStream(), aAction);
}

void
CleanupParentFds(PCacheReadStream& aReadStream, CleanupAction aAction)
{
  if (aReadStream.fds().type() !=
      OptionalFileDescriptorSet::TPFileDescriptorSetParent) {
    return;
  }

  nsAutoTArray<FileDescriptor, 4> fds;

  FileDescriptorSetParent* fdSetActor =
    static_cast<FileDescriptorSetParent*>(aReadStream.fds().get_PFileDescriptorSetParent());
  MOZ_ASSERT(fdSetActor);

  if (aAction == Delete) {
    unused << fdSetActor->Send__delete__(fdSetActor);
  }

  // FileDescriptorSet doesn't clear its fds in its ActorDestroy, so we
  // unconditionally forget them here.  The fds themselves are auto-closed in
  // ~FileDescriptor since they originated in this process.
  fdSetActor->ForgetFileDescriptors(fds);
}

void
CleanupParentFds(PCacheReadStreamOrVoid& aReadStreamOrVoid, CleanupAction aAction)
{
  if (aReadStreamOrVoid.type() == PCacheReadStreamOrVoid::Tvoid_t) {
    return;
  }

  CleanupParentFds(aReadStreamOrVoid.get_PCacheReadStream(), aAction);
}

} // anonymous namespace

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::ipc::PBackgroundParent;

AutoChildBase::AutoChildBase(TypeUtils* aTypeUtils)
  : mTypeUtils(aTypeUtils)
  , mSent(false)
{
  MOZ_ASSERT(mTypeUtils);
}

AutoChildBase::~AutoChildBase()
{
}

// --------------------------------------------

AutoChildRequest::AutoChildRequest(TypeUtils* aTypeUtils)
  : AutoChildBase(aTypeUtils)
{
  mRequestOrVoid = void_t();
}

AutoChildRequest::~AutoChildRequest()
{
  if (mRequestOrVoid.type() != PCacheRequestOrVoid::TPCacheRequest) {
    return;
  }

  CleanupAction action = mSent ? Forget : Delete;
  CleanupChild(mRequestOrVoid.get_PCacheRequest().body(), action);
}

void
AutoChildRequest::Add(InternalRequest* aRequest, BodyAction aBodyAction,
                      ReferrerAction aReferrerAction, SchemeAction aSchemeAction,
                      ErrorResult& aRv)
{
  MOZ_ASSERT(!mSent);
  MOZ_ASSERT(mRequestOrVoid.type() == PCacheRequestOrVoid::Tvoid_t);
  mRequestOrVoid = PCacheRequest();
  mTypeUtils->ToPCacheRequest(mRequestOrVoid.get_PCacheRequest(), aRequest,
                              aBodyAction, aReferrerAction, aSchemeAction, aRv);
}

const PCacheRequest&
AutoChildRequest::SendAsRequest()
{
  MOZ_ASSERT(mRequestOrVoid.type() == PCacheRequestOrVoid::TPCacheRequest);
  return mRequestOrVoid.get_PCacheRequest();
}

const PCacheRequestOrVoid&
AutoChildRequest::SendAsRequestOrVoid()
{
  return mRequestOrVoid;
}

// --------------------------------------------

AutoChildRequestList::AutoChildRequestList(TypeUtils* aTypeUtils,
                                           uint32_t aCapacity)
  : AutoChildBase(aTypeUtils)
{
  mRequestList.SetCapacity(aCapacity);
}

AutoChildRequestList::~AutoChildRequestList()
{
  CleanupAction action = mSent ? Forget : Delete;
  for (uint32_t i = 0; i < mRequestList.Length(); ++i) {
    CleanupChild(mRequestList[i].body(), action);
  }
}

void
AutoChildRequestList::Add(InternalRequest* aRequest, BodyAction aBodyAction,
                          ReferrerAction aReferrerAction,
                          SchemeAction aSchemeAction, ErrorResult& aRv)
{
  MOZ_ASSERT(!mSent);

  // The FileDescriptorSetChild asserts in its destructor that all fds have
  // been removed.  The copy constructor, however, simply duplicates the
  // fds without removing any.  This means each temporary and copy must be
  // explicitly cleaned up.
  //
  // Avoid a lot of this hassle by making sure we only create one here.  On
  // error we remove it.

  PCacheRequest* request = mRequestList.AppendElement();
  mTypeUtils->ToPCacheRequest(*request, aRequest, aBodyAction, aReferrerAction,
                              aSchemeAction, aRv);
  if (aRv.Failed()) {
    mRequestList.RemoveElementAt(mRequestList.Length() - 1);
  }
}

const nsTArray<PCacheRequest>&
AutoChildRequestList::SendAsRequestList()
{
  MOZ_ASSERT(!mSent);
  mSent = true;
  return mRequestList;
}

// --------------------------------------------

AutoChildRequestResponse::AutoChildRequestResponse(TypeUtils* aTypeUtils)
  : AutoChildBase(aTypeUtils)
{
  // Default IPC-generated constructor does not initialize these correctly
  // and we check them later when cleaning up.
  mRequestResponse.request().body() = void_t();
  mRequestResponse.response().body() = void_t();
}

AutoChildRequestResponse::~AutoChildRequestResponse()
{
  CleanupAction action = mSent ? Forget : Delete;
  CleanupChild(mRequestResponse.request().body(), action);
  CleanupChild(mRequestResponse.response().body(), action);
}

void
AutoChildRequestResponse::Add(InternalRequest* aRequest, BodyAction aBodyAction,
                              ReferrerAction aReferrerAction,
                              SchemeAction aSchemeAction, ErrorResult& aRv)
{
  MOZ_ASSERT(!mSent);
  mTypeUtils->ToPCacheRequest(mRequestResponse.request(), aRequest, aBodyAction,
                              aReferrerAction, aSchemeAction, aRv);
}

void
AutoChildRequestResponse::Add(Response& aResponse, ErrorResult& aRv)
{
  MOZ_ASSERT(!mSent);
  mTypeUtils->ToPCacheResponse(mRequestResponse.response(), aResponse, aRv);
}

const CacheRequestResponse&
AutoChildRequestResponse::SendAsRequestResponse()
{
  MOZ_ASSERT(!mSent);
  mSent = true;
  return mRequestResponse;
}

// --------------------------------------------

AutoParentBase::AutoParentBase(PBackgroundParent* aManager)
  : mManager(aManager)
  , mStreamControl(nullptr)
  , mSent(false)
{
  MOZ_ASSERT(mManager);
}

AutoParentBase::~AutoParentBase()
{
  if (!mSent && mStreamControl) {
    unused << PCacheStreamControlParent::Send__delete__(mStreamControl);
  }
}

void
AutoParentBase::SerializeReadStream(const nsID& aId, StreamList* aStreamList,
                                    PCacheReadStream* aReadStreamOut)
{
  MOZ_ASSERT(aStreamList);
  MOZ_ASSERT(aReadStreamOut);
  MOZ_ASSERT(!mSent);

  nsCOMPtr<nsIInputStream> stream = aStreamList->Extract(aId);
  MOZ_ASSERT(stream);

  if (!mStreamControl) {
    mStreamControl = static_cast<CacheStreamControlParent*>(
      mManager->SendPCacheStreamControlConstructor(new CacheStreamControlParent()));

    // If this failed, then the child process is gone.  Warn and allow actor
    // cleanup to proceed as normal.
    if (!mStreamControl) {
      NS_WARNING("Cache failed to create stream control actor.");
      return;
    }
  }

  aStreamList->SetStreamControl(mStreamControl);

  nsRefPtr<ReadStream> readStream = ReadStream::Create(mStreamControl,
                                                       aId, stream);
  readStream->Serialize(aReadStreamOut);
}

// --------------------------------------------

AutoParentRequestList::AutoParentRequestList(PBackgroundParent* aManager,
                                             uint32_t aCapacity)
  : AutoParentBase(aManager)
{
  mRequestList.SetCapacity(aCapacity);
}

AutoParentRequestList::~AutoParentRequestList()
{
  CleanupAction action = mSent ? Forget : Delete;
  for (uint32_t i = 0; i < mRequestList.Length(); ++i) {
    CleanupParentFds(mRequestList[i].body(), action);
  }
}

void
AutoParentRequestList::Add(const SavedRequest& aSavedRequest,
                           StreamList* aStreamList)
{
  MOZ_ASSERT(!mSent);

  mRequestList.AppendElement(aSavedRequest.mValue);
  PCacheRequest& request = mRequestList.LastElement();

  if (!aSavedRequest.mHasBodyId) {
    request.body() = void_t();
    return;
  }

  request.body() = PCacheReadStream();
  SerializeReadStream(aSavedRequest.mBodyId, aStreamList,
                      &request.body().get_PCacheReadStream());
}

const nsTArray<PCacheRequest>&
AutoParentRequestList::SendAsRequestList()
{
  MOZ_ASSERT(!mSent);
  mSent = true;
  return mRequestList;
}

// --------------------------------------------

AutoParentResponseList::AutoParentResponseList(PBackgroundParent* aManager,
                                               uint32_t aCapacity)
  : AutoParentBase(aManager)
{
  mResponseList.SetCapacity(aCapacity);
}

AutoParentResponseList::~AutoParentResponseList()
{
  CleanupAction action = mSent ? Forget : Delete;
  for (uint32_t i = 0; i < mResponseList.Length(); ++i) {
    CleanupParentFds(mResponseList[i].body(), action);
  }
}

void
AutoParentResponseList::Add(const SavedResponse& aSavedResponse,
                            StreamList* aStreamList)
{
  MOZ_ASSERT(!mSent);

  mResponseList.AppendElement(aSavedResponse.mValue);
  PCacheResponse& response = mResponseList.LastElement();

  if (!aSavedResponse.mHasBodyId) {
    response.body() = void_t();
    return;
  }

  response.body() = PCacheReadStream();
  SerializeReadStream(aSavedResponse.mBodyId, aStreamList,
                      &response.body().get_PCacheReadStream());
}

const nsTArray<PCacheResponse>&
AutoParentResponseList::SendAsResponseList()
{
  MOZ_ASSERT(!mSent);
  mSent = true;
  return mResponseList;
}

// --------------------------------------------

AutoParentResponseOrVoid::AutoParentResponseOrVoid(ipc::PBackgroundParent* aManager)
  : AutoParentBase(aManager)
{
  mResponseOrVoid = void_t();
}

AutoParentResponseOrVoid::~AutoParentResponseOrVoid()
{
  if (mResponseOrVoid.type() != PCacheResponseOrVoid::TPCacheResponse) {
    return;
  }

  CleanupAction action = mSent ? Forget : Delete;
  CleanupParentFds(mResponseOrVoid.get_PCacheResponse().body(), action);
}

void
AutoParentResponseOrVoid::Add(const SavedResponse& aSavedResponse,
                              StreamList* aStreamList)
{
  MOZ_ASSERT(!mSent);

  mResponseOrVoid = aSavedResponse.mValue;
  PCacheResponse& response = mResponseOrVoid.get_PCacheResponse();

  if (!aSavedResponse.mHasBodyId) {
    response.body() = void_t();
    return;
  }

  response.body() = PCacheReadStream();
  SerializeReadStream(aSavedResponse.mBodyId, aStreamList,
                      &response.body().get_PCacheReadStream());
}

const PCacheResponseOrVoid&
AutoParentResponseOrVoid::SendAsResponseOrVoid()
{
  MOZ_ASSERT(!mSent);
  mSent = true;
  return mResponseOrVoid;
}

} // namespace cache
} // namespace dom
} // namespace mozilla
