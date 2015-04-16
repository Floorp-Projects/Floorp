/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/AutoUtils.h"

#include "mozilla/unused.h"
#include "mozilla/dom/cache/CacheParent.h"
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
using mozilla::dom::cache::CacheReadStream;
using mozilla::dom::cache::CacheReadStreamOrVoid;
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
CleanupChildFds(CacheReadStream& aReadStream, CleanupAction aAction)
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
CleanupChildPushStream(CacheReadStream& aReadStream, CleanupAction aAction)
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
CleanupChild(CacheReadStream& aReadStream, CleanupAction aAction)
{
  CleanupChildFds(aReadStream, aAction);
  CleanupChildPushStream(aReadStream, aAction);
}

void
CleanupChild(CacheReadStreamOrVoid& aReadStreamOrVoid, CleanupAction aAction)
{
  if (aReadStreamOrVoid.type() == CacheReadStreamOrVoid::Tvoid_t) {
    return;
  }

  CleanupChild(aReadStreamOrVoid.get_CacheReadStream(), aAction);
}

void
CleanupParentFds(CacheReadStream& aReadStream, CleanupAction aAction)
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
CleanupParentFds(CacheReadStreamOrVoid& aReadStreamOrVoid, CleanupAction aAction)
{
  if (aReadStreamOrVoid.type() == CacheReadStreamOrVoid::Tvoid_t) {
    return;
  }

  CleanupParentFds(aReadStreamOrVoid.get_CacheReadStream(), aAction);
}

} // anonymous namespace

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::ipc::PBackgroundParent;

// --------------------------------------------

AutoChildOpArgs::AutoChildOpArgs(TypeUtils* aTypeUtils,
                                 const CacheOpArgs& aOpArgs)
  : mTypeUtils(aTypeUtils)
  , mOpArgs(aOpArgs)
  , mSent(false)
{
  MOZ_ASSERT(mTypeUtils);
}

AutoChildOpArgs::~AutoChildOpArgs()
{
  CleanupAction action = mSent ? Forget : Delete;

  switch(mOpArgs.type()) {
    case CacheOpArgs::TCacheMatchArgs:
    {
      CacheMatchArgs& args = mOpArgs.get_CacheMatchArgs();
      CleanupChild(args.request().body(), action);
      break;
    }
    case CacheOpArgs::TCacheMatchAllArgs:
    {
      CacheMatchAllArgs& args = mOpArgs.get_CacheMatchAllArgs();
      if (args.requestOrVoid().type() == CacheRequestOrVoid::Tvoid_t) {
        break;
      }
      CleanupChild(args.requestOrVoid().get_CacheRequest().body(), action);
      break;
    }
    case CacheOpArgs::TCacheAddAllArgs:
    {
      CacheAddAllArgs& args = mOpArgs.get_CacheAddAllArgs();
      auto& list = args.requestList();
      for (uint32_t i = 0; i < list.Length(); ++i) {
        CleanupChild(list[i].body(), action);
      }
      break;
    }
    case CacheOpArgs::TCachePutAllArgs:
    {
      CachePutAllArgs& args = mOpArgs.get_CachePutAllArgs();
      auto& list = args.requestResponseList();
      for (uint32_t i = 0; i < list.Length(); ++i) {
        CleanupChild(list[i].request().body(), action);
        CleanupChild(list[i].response().body(), action);
      }
      break;
    }
    case CacheOpArgs::TCacheDeleteArgs:
    {
      CacheDeleteArgs& args = mOpArgs.get_CacheDeleteArgs();
      CleanupChild(args.request().body(), action);
      break;
    }
    case CacheOpArgs::TCacheKeysArgs:
    {
      CacheKeysArgs& args = mOpArgs.get_CacheKeysArgs();
      if (args.requestOrVoid().type() == CacheRequestOrVoid::Tvoid_t) {
        break;
      }
      CleanupChild(args.requestOrVoid().get_CacheRequest().body(), action);
      break;
    }
    case CacheOpArgs::TStorageMatchArgs:
    {
      StorageMatchArgs& args = mOpArgs.get_StorageMatchArgs();
      CleanupChild(args.request().body(), action);
      break;
    }
    default:
      // Other types do not need cleanup
      break;
  }
}

void
AutoChildOpArgs::Add(InternalRequest* aRequest, BodyAction aBodyAction,
                     ReferrerAction aReferrerAction, SchemeAction aSchemeAction,
                     ErrorResult& aRv)
{
  MOZ_ASSERT(!mSent);

  switch(mOpArgs.type()) {
    case CacheOpArgs::TCacheMatchArgs:
    {
      CacheMatchArgs& args = mOpArgs.get_CacheMatchArgs();
      mTypeUtils->ToCacheRequest(args.request(), aRequest, aBodyAction,
                                  aReferrerAction, aSchemeAction, aRv);
      break;
    }
    case CacheOpArgs::TCacheMatchAllArgs:
    {
      CacheMatchAllArgs& args = mOpArgs.get_CacheMatchAllArgs();
      MOZ_ASSERT(args.requestOrVoid().type() == CacheRequestOrVoid::Tvoid_t);
      args.requestOrVoid() = CacheRequest();
      mTypeUtils->ToCacheRequest(args.requestOrVoid().get_CacheRequest(),
                                  aRequest, aBodyAction, aReferrerAction,
                                  aSchemeAction, aRv);
      break;
    }
    case CacheOpArgs::TCacheAddAllArgs:
    {
      CacheAddAllArgs& args = mOpArgs.get_CacheAddAllArgs();

      // The FileDescriptorSetChild asserts in its destructor that all fds have
      // been removed.  The copy constructor, however, simply duplicates the
      // fds without removing any.  This means each temporary and copy must be
      // explicitly cleaned up.
      //
      // Avoid a lot of this hassle by making sure we only create one here.  On
      // error we remove it.
      CacheRequest& request = *args.requestList().AppendElement();

      mTypeUtils->ToCacheRequest(request, aRequest, aBodyAction,
                                  aReferrerAction, aSchemeAction, aRv);
      if (aRv.Failed()) {
        args.requestList().RemoveElementAt(args.requestList().Length() - 1);
      }
      break;
    }
    case CacheOpArgs::TCacheDeleteArgs:
    {
      CacheDeleteArgs& args = mOpArgs.get_CacheDeleteArgs();
      mTypeUtils->ToCacheRequest(args.request(), aRequest, aBodyAction,
                                  aReferrerAction, aSchemeAction, aRv);
      break;
    }
    case CacheOpArgs::TCacheKeysArgs:
    {
      CacheKeysArgs& args = mOpArgs.get_CacheKeysArgs();
      MOZ_ASSERT(args.requestOrVoid().type() == CacheRequestOrVoid::Tvoid_t);
      args.requestOrVoid() = CacheRequest();
      mTypeUtils->ToCacheRequest(args.requestOrVoid().get_CacheRequest(),
                                  aRequest, aBodyAction, aReferrerAction,
                                  aSchemeAction, aRv);
      break;
    }
    case CacheOpArgs::TStorageMatchArgs:
    {
      StorageMatchArgs& args = mOpArgs.get_StorageMatchArgs();
      mTypeUtils->ToCacheRequest(args.request(), aRequest, aBodyAction,
                                  aReferrerAction, aSchemeAction, aRv);
      break;
    }
    default:
      MOZ_CRASH("Cache args type cannot send a Request!");
  }
}

void
AutoChildOpArgs::Add(InternalRequest* aRequest, BodyAction aBodyAction,
                     ReferrerAction aReferrerAction, SchemeAction aSchemeAction,
                     Response& aResponse, ErrorResult& aRv)
{
  MOZ_ASSERT(!mSent);

  switch(mOpArgs.type()) {
    case CacheOpArgs::TCachePutAllArgs:
    {
      CachePutAllArgs& args = mOpArgs.get_CachePutAllArgs();

      // The FileDescriptorSetChild asserts in its destructor that all fds have
      // been removed.  The copy constructor, however, simply duplicates the
      // fds without removing any.  This means each temporary and copy must be
      // explicitly cleaned up.
      //
      // Avoid a lot of this hassle by making sure we only create one here.  On
      // error we remove it.
      CacheRequestResponse& pair = *args.requestResponseList().AppendElement();
      pair.request().body() = void_t();
      pair.response().body() = void_t();

      mTypeUtils->ToCacheRequest(pair.request(), aRequest, aBodyAction,
                                  aReferrerAction, aSchemeAction, aRv);
      if (!aRv.Failed()) {
        mTypeUtils->ToCacheResponse(pair.response(), aResponse, aRv);
      }

      if (aRv.Failed()) {
        CleanupChild(pair.request().body(), Delete);
        args.requestResponseList().RemoveElementAt(
          args.requestResponseList().Length() - 1);
      }

      break;
    }
    default:
      MOZ_CRASH("Cache args type cannot send a Request/Response pair!");
  }
}

const CacheOpArgs&
AutoChildOpArgs::SendAsOpArgs()
{
  MOZ_ASSERT(!mSent);
  mSent = true;
  return mOpArgs;
}

// --------------------------------------------

AutoParentOpResult::AutoParentOpResult(mozilla::ipc::PBackgroundParent* aManager,
                                       const CacheOpResult& aOpResult)
  : mManager(aManager)
  , mOpResult(aOpResult)
  , mStreamControl(nullptr)
  , mSent(false)
{
  MOZ_ASSERT(mManager);
}

AutoParentOpResult::~AutoParentOpResult()
{
  CleanupAction action = mSent ? Forget : Delete;

  switch (mOpResult.type()) {
    case CacheOpResult::TCacheMatchResult:
    {
      CacheMatchResult& result = mOpResult.get_CacheMatchResult();
      if (result.responseOrVoid().type() == CacheResponseOrVoid::Tvoid_t) {
        break;
      }
      CleanupParentFds(result.responseOrVoid().get_CacheResponse().body(),
                       action);
      break;
    }
    case CacheOpResult::TCacheMatchAllResult:
    {
      CacheMatchAllResult& result = mOpResult.get_CacheMatchAllResult();
      for (uint32_t i = 0; i < result.responseList().Length(); ++i) {
        CleanupParentFds(result.responseList()[i].body(), action);
      }
      break;
    }
    case CacheOpResult::TCacheKeysResult:
    {
      CacheKeysResult& result = mOpResult.get_CacheKeysResult();
      for (uint32_t i = 0; i < result.requestList().Length(); ++i) {
        CleanupParentFds(result.requestList()[i].body(), action);
      }
      break;
    }
    case CacheOpResult::TStorageMatchResult:
    {
      StorageMatchResult& result = mOpResult.get_StorageMatchResult();
      if (result.responseOrVoid().type() == CacheResponseOrVoid::Tvoid_t) {
        break;
      }
      CleanupParentFds(result.responseOrVoid().get_CacheResponse().body(),
                       action);
      break;
    }
    case CacheOpResult::TStorageOpenResult:
    {
      StorageOpenResult& result = mOpResult.get_StorageOpenResult();
      if (action == Forget || result.actorParent() == nullptr) {
        break;
      }
      unused << PCacheParent::Send__delete__(result.actorParent());
    }
    default:
      // other types do not need clean up
      break;
  }

  if (action == Delete && mStreamControl) {
    unused << PCacheStreamControlParent::Send__delete__(mStreamControl);
  }
}

void
AutoParentOpResult::Add(CacheId aOpenedCacheId, Manager* aManager)
{
  MOZ_ASSERT(mOpResult.type() == CacheOpResult::TStorageOpenResult);
  MOZ_ASSERT(mOpResult.get_StorageOpenResult().actorParent() == nullptr);
  mOpResult.get_StorageOpenResult().actorParent() =
    mManager->SendPCacheConstructor(new CacheParent(aManager, aOpenedCacheId));
}

void
AutoParentOpResult::Add(const SavedResponse& aSavedResponse,
                        StreamList* aStreamList)
{
  MOZ_ASSERT(!mSent);

  switch (mOpResult.type()) {
    case CacheOpResult::TCacheMatchResult:
    {
      CacheMatchResult& result = mOpResult.get_CacheMatchResult();
      MOZ_ASSERT(result.responseOrVoid().type() == CacheResponseOrVoid::Tvoid_t);
      result.responseOrVoid() = aSavedResponse.mValue;
      SerializeResponseBody(aSavedResponse, aStreamList,
                            &result.responseOrVoid().get_CacheResponse());
      break;
    }
    case CacheOpResult::TCacheMatchAllResult:
    {
      CacheMatchAllResult& result = mOpResult.get_CacheMatchAllResult();
      result.responseList().AppendElement(aSavedResponse.mValue);
      SerializeResponseBody(aSavedResponse, aStreamList,
                            &result.responseList().LastElement());
      break;
    }
    case CacheOpResult::TStorageMatchResult:
    {
      StorageMatchResult& result = mOpResult.get_StorageMatchResult();
      MOZ_ASSERT(result.responseOrVoid().type() == CacheResponseOrVoid::Tvoid_t);
      result.responseOrVoid() = aSavedResponse.mValue;
      SerializeResponseBody(aSavedResponse, aStreamList,
                            &result.responseOrVoid().get_CacheResponse());
      break;
    }
    default:
      MOZ_CRASH("Cache result type cannot handle returning a Response!");
  }
}

void
AutoParentOpResult::Add(const SavedRequest& aSavedRequest,
                        StreamList* aStreamList)
{
  MOZ_ASSERT(!mSent);

  switch (mOpResult.type()) {
    case CacheOpResult::TCacheKeysResult:
    {
      CacheKeysResult& result = mOpResult.get_CacheKeysResult();
      result.requestList().AppendElement(aSavedRequest.mValue);
      CacheRequest& request = result.requestList().LastElement();

      if (!aSavedRequest.mHasBodyId) {
        request.body() = void_t();
        break;
      }

      request.body() = CacheReadStream();
      SerializeReadStream(aSavedRequest.mBodyId, aStreamList,
                          &request.body().get_CacheReadStream());
      break;
    }
    default:
      MOZ_CRASH("Cache result type cannot handle returning a Request!");
  }
}

const CacheOpResult&
AutoParentOpResult::SendAsOpResult()
{
  MOZ_ASSERT(!mSent);
  mSent = true;
  return mOpResult;
}

void
AutoParentOpResult::SerializeResponseBody(const SavedResponse& aSavedResponse,
                                          StreamList* aStreamList,
                                          CacheResponse* aResponseOut)
{
  MOZ_ASSERT(aResponseOut);

  if (!aSavedResponse.mHasBodyId) {
    aResponseOut->body() = void_t();
    return;
  }

  aResponseOut->body() = CacheReadStream();
  SerializeReadStream(aSavedResponse.mBodyId, aStreamList,
                      &aResponseOut->body().get_CacheReadStream());
}

void
AutoParentOpResult::SerializeReadStream(const nsID& aId, StreamList* aStreamList,
                                        CacheReadStream* aReadStreamOut)
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

} // namespace cache
} // namespace dom
} // namespace mozilla
