/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/AutoUtils.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/cache/CacheParent.h"
#include "mozilla/dom/cache/CacheStreamControlParent.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/dom/cache/SavedTypes.h"
#include "mozilla/dom/cache/StreamList.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "nsCRT.h"
#include "nsHttp.h"

using mozilla::Maybe;
using mozilla::Unused;
using mozilla::dom::cache::CacheReadStream;
using mozilla::ipc::AutoIPCStream;
using mozilla::ipc::PBackgroundParent;

namespace {

enum CleanupAction { Forget, Delete };

void CleanupChild(CacheReadStream& aReadStream, CleanupAction aAction) {
  // fds cleaned up by mStreamCleanupList
  // PChildToParentStream actors cleaned up by mStreamCleanupList
}

void CleanupChild(Maybe<CacheReadStream>& aMaybeReadStream,
                  CleanupAction aAction) {
  if (aMaybeReadStream.isNothing()) {
    return;
  }

  CleanupChild(aMaybeReadStream.ref(), aAction);
}

}  // namespace

namespace mozilla {
namespace dom {
namespace cache {

// --------------------------------------------

AutoChildOpArgs::AutoChildOpArgs(TypeUtils* aTypeUtils,
                                 const CacheOpArgs& aOpArgs,
                                 uint32_t aEntryCount)
    : mTypeUtils(aTypeUtils), mOpArgs(aOpArgs), mSent(false) {
  MOZ_DIAGNOSTIC_ASSERT(mTypeUtils);
  MOZ_RELEASE_ASSERT(aEntryCount != 0);
  // We are using AutoIPCStream objects to cleanup target IPCStream
  // structures embedded in our CacheOpArgs.  These IPCStream structs
  // must not move once we attach our AutoIPCStream to them.  Therefore,
  // its important that any arrays containing streams are pre-sized for
  // the number of entries we have in order to avoid realloc moving
  // things around on us.
  if (mOpArgs.type() == CacheOpArgs::TCachePutAllArgs) {
    CachePutAllArgs& args = mOpArgs.get_CachePutAllArgs();
    args.requestResponseList().SetCapacity(aEntryCount);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(aEntryCount == 1);
  }
}

AutoChildOpArgs::~AutoChildOpArgs() {
  CleanupAction action = mSent ? Forget : Delete;

  switch (mOpArgs.type()) {
    case CacheOpArgs::TCacheMatchArgs: {
      CacheMatchArgs& args = mOpArgs.get_CacheMatchArgs();
      CleanupChild(args.request().body(), action);
      break;
    }
    case CacheOpArgs::TCacheMatchAllArgs: {
      CacheMatchAllArgs& args = mOpArgs.get_CacheMatchAllArgs();
      if (args.maybeRequest().isNothing()) {
        break;
      }
      CleanupChild(args.maybeRequest().ref().body(), action);
      break;
    }
    case CacheOpArgs::TCachePutAllArgs: {
      CachePutAllArgs& args = mOpArgs.get_CachePutAllArgs();
      auto& list = args.requestResponseList();
      for (uint32_t i = 0; i < list.Length(); ++i) {
        CleanupChild(list[i].request().body(), action);
        CleanupChild(list[i].response().body(), action);
      }
      break;
    }
    case CacheOpArgs::TCacheDeleteArgs: {
      CacheDeleteArgs& args = mOpArgs.get_CacheDeleteArgs();
      CleanupChild(args.request().body(), action);
      break;
    }
    case CacheOpArgs::TCacheKeysArgs: {
      CacheKeysArgs& args = mOpArgs.get_CacheKeysArgs();
      if (args.maybeRequest().isNothing()) {
        break;
      }
      CleanupChild(args.maybeRequest().ref().body(), action);
      break;
    }
    case CacheOpArgs::TStorageMatchArgs: {
      StorageMatchArgs& args = mOpArgs.get_StorageMatchArgs();
      CleanupChild(args.request().body(), action);
      break;
    }
    default:
      // Other types do not need cleanup
      break;
  }

  mStreamCleanupList.Clear();
}

void AutoChildOpArgs::Add(InternalRequest* aRequest, BodyAction aBodyAction,
                          SchemeAction aSchemeAction, ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(!mSent);

  switch (mOpArgs.type()) {
    case CacheOpArgs::TCacheMatchArgs: {
      CacheMatchArgs& args = mOpArgs.get_CacheMatchArgs();
      mTypeUtils->ToCacheRequest(args.request(), aRequest, aBodyAction,
                                 aSchemeAction, mStreamCleanupList, aRv);
      break;
    }
    case CacheOpArgs::TCacheMatchAllArgs: {
      CacheMatchAllArgs& args = mOpArgs.get_CacheMatchAllArgs();
      MOZ_DIAGNOSTIC_ASSERT(args.maybeRequest().isNothing());
      args.maybeRequest().emplace(CacheRequest());
      mTypeUtils->ToCacheRequest(args.maybeRequest().ref(), aRequest,
                                 aBodyAction, aSchemeAction, mStreamCleanupList,
                                 aRv);
      break;
    }
    case CacheOpArgs::TCacheDeleteArgs: {
      CacheDeleteArgs& args = mOpArgs.get_CacheDeleteArgs();
      mTypeUtils->ToCacheRequest(args.request(), aRequest, aBodyAction,
                                 aSchemeAction, mStreamCleanupList, aRv);
      break;
    }
    case CacheOpArgs::TCacheKeysArgs: {
      CacheKeysArgs& args = mOpArgs.get_CacheKeysArgs();
      MOZ_DIAGNOSTIC_ASSERT(args.maybeRequest().isNothing());
      args.maybeRequest().emplace(CacheRequest());
      mTypeUtils->ToCacheRequest(args.maybeRequest().ref(), aRequest,
                                 aBodyAction, aSchemeAction, mStreamCleanupList,
                                 aRv);
      break;
    }
    case CacheOpArgs::TStorageMatchArgs: {
      StorageMatchArgs& args = mOpArgs.get_StorageMatchArgs();
      mTypeUtils->ToCacheRequest(args.request(), aRequest, aBodyAction,
                                 aSchemeAction, mStreamCleanupList, aRv);
      break;
    }
    default:
      MOZ_CRASH("Cache args type cannot send a Request!");
  }
}

namespace {

bool MatchInPutList(InternalRequest* aRequest,
                    const nsTArray<CacheRequestResponse>& aPutList) {
  MOZ_DIAGNOSTIC_ASSERT(aRequest);

  // This method implements the SW spec QueryCache algorithm against an
  // in memory array of Request/Response objects.  This essentially the
  // same algorithm that is implemented in DBSchema.cpp.  Unfortunately
  // we cannot unify them because when operating against the real database
  // we don't want to load all request/response objects into memory.

  // Note, we can skip the check for a invalid request method because
  // Cache should only call into here with a GET or HEAD.
#ifdef DEBUG
  nsAutoCString method;
  aRequest->GetMethod(method);
  MOZ_ASSERT(method.LowerCaseEqualsLiteral("get") ||
             method.LowerCaseEqualsLiteral("head"));
#endif

  RefPtr<InternalHeaders> requestHeaders = aRequest->Headers();

  for (uint32_t i = 0; i < aPutList.Length(); ++i) {
    const CacheRequest& cachedRequest = aPutList[i].request();
    const CacheResponse& cachedResponse = aPutList[i].response();

    nsAutoCString url;
    aRequest->GetURL(url);

    nsAutoCString requestUrl(cachedRequest.urlWithoutQuery());
    requestUrl.Append(cachedRequest.urlQuery());

    // If the URLs don't match, then just skip to the next entry.
    if (url != requestUrl) {
      continue;
    }

    RefPtr<InternalHeaders> cachedRequestHeaders =
        TypeUtils::ToInternalHeaders(cachedRequest.headers());

    RefPtr<InternalHeaders> cachedResponseHeaders =
        TypeUtils::ToInternalHeaders(cachedResponse.headers());

    nsCString varyHeaders;
    ErrorResult rv;
    cachedResponseHeaders->Get(NS_LITERAL_CSTRING("vary"), varyHeaders, rv);
    MOZ_ALWAYS_TRUE(!rv.Failed());

    // Assume the vary headers match until we find a conflict
    bool varyHeadersMatch = true;

    char* rawBuffer = varyHeaders.BeginWriting();
    char* token = nsCRT::strtok(rawBuffer, NS_HTTP_HEADER_SEPS, &rawBuffer);
    for (; token;
         token = nsCRT::strtok(rawBuffer, NS_HTTP_HEADER_SEPS, &rawBuffer)) {
      nsDependentCString header(token);
      MOZ_DIAGNOSTIC_ASSERT(!header.EqualsLiteral("*"),
                            "We should have already caught this in "
                            "TypeUtils::ToPCacheResponseWithoutBody()");

      ErrorResult headerRv;
      nsAutoCString value;
      requestHeaders->Get(header, value, headerRv);
      if (NS_WARN_IF(headerRv.Failed())) {
        headerRv.SuppressException();
        MOZ_DIAGNOSTIC_ASSERT(value.IsEmpty());
      }

      nsAutoCString cachedValue;
      cachedRequestHeaders->Get(header, cachedValue, headerRv);
      if (NS_WARN_IF(headerRv.Failed())) {
        headerRv.SuppressException();
        MOZ_DIAGNOSTIC_ASSERT(cachedValue.IsEmpty());
      }

      if (value != cachedValue) {
        varyHeadersMatch = false;
        break;
      }
    }

    // URL was equal and all vary headers match!
    if (varyHeadersMatch) {
      return true;
    }
  }

  return false;
}

}  // namespace

void AutoChildOpArgs::Add(JSContext* aCx, InternalRequest* aRequest,
                          BodyAction aBodyAction, SchemeAction aSchemeAction,
                          Response& aResponse, ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(!mSent);

  switch (mOpArgs.type()) {
    case CacheOpArgs::TCachePutAllArgs: {
      CachePutAllArgs& args = mOpArgs.get_CachePutAllArgs();

      // Throw an error if a request/response pair would mask another
      // request/response pair in the same PutAll operation.  This is
      // step 2.3.2.3 from the "Batch Cache Operations" spec algorithm.
      if (MatchInPutList(aRequest, args.requestResponseList())) {
        aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
        return;
      }

      // Ensure that we don't realloc the array since this can result
      // in our AutoIPCStream objects to reference the wrong memory
      // location.  This should never happen and is a UAF if it does.
      // Therefore make this a release assertion.
      MOZ_RELEASE_ASSERT(args.requestResponseList().Length() <
                         args.requestResponseList().Capacity());

      // The FileDescriptorSetChild asserts in its destructor that all fds have
      // been removed.  The copy constructor, however, simply duplicates the
      // fds without removing any.  This means each temporary and copy must be
      // explicitly cleaned up.
      //
      // Avoid a lot of this hassle by making sure we only create one here.  On
      // error we remove it.
      CacheRequestResponse& pair = *args.requestResponseList().AppendElement();
      pair.request().body() = Nothing();
      pair.response().body() = Nothing();

      mTypeUtils->ToCacheRequest(pair.request(), aRequest, aBodyAction,
                                 aSchemeAction, mStreamCleanupList, aRv);
      if (!aRv.Failed()) {
        mTypeUtils->ToCacheResponse(aCx, pair.response(), aResponse,
                                    mStreamCleanupList, aRv);
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

const CacheOpArgs& AutoChildOpArgs::SendAsOpArgs() {
  MOZ_DIAGNOSTIC_ASSERT(!mSent);
  mSent = true;
  for (UniquePtr<AutoIPCStream>& autoStream : mStreamCleanupList) {
    autoStream->TakeOptionalValue();
  }
  return mOpArgs;
}

// --------------------------------------------

AutoParentOpResult::AutoParentOpResult(
    mozilla::ipc::PBackgroundParent* aManager, const CacheOpResult& aOpResult,
    uint32_t aEntryCount)
    : mManager(aManager),
      mOpResult(aOpResult),
      mStreamControl(nullptr),
      mSent(false) {
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  MOZ_RELEASE_ASSERT(aEntryCount != 0);
  // We are using AutoIPCStream objects to cleanup target IPCStream
  // structures embedded in our CacheOpArgs.  These IPCStream structs
  // must not move once we attach our AutoIPCStream to them.  Therefore,
  // its important that any arrays containing streams are pre-sized for
  // the number of entries we have in order to avoid realloc moving
  // things around on us.
  if (mOpResult.type() == CacheOpResult::TCacheMatchAllResult) {
    CacheMatchAllResult& result = mOpResult.get_CacheMatchAllResult();
    result.responseList().SetCapacity(aEntryCount);
  } else if (mOpResult.type() == CacheOpResult::TCacheKeysResult) {
    CacheKeysResult& result = mOpResult.get_CacheKeysResult();
    result.requestList().SetCapacity(aEntryCount);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(aEntryCount == 1);
  }
}

AutoParentOpResult::~AutoParentOpResult() {
  CleanupAction action = mSent ? Forget : Delete;

  switch (mOpResult.type()) {
    case CacheOpResult::TStorageOpenResult: {
      StorageOpenResult& result = mOpResult.get_StorageOpenResult();
      if (action == Forget || result.actorParent() == nullptr) {
        break;
      }
      Unused << PCacheParent::Send__delete__(result.actorParent());
      break;
    }
    default:
      // other types do not need additional clean up
      break;
  }

  if (action == Delete && mStreamControl) {
    Unused << PCacheStreamControlParent::Send__delete__(mStreamControl);
  }

  mStreamCleanupList.Clear();
}

void AutoParentOpResult::Add(CacheId aOpenedCacheId, Manager* aManager) {
  MOZ_DIAGNOSTIC_ASSERT(mOpResult.type() == CacheOpResult::TStorageOpenResult);
  MOZ_DIAGNOSTIC_ASSERT(mOpResult.get_StorageOpenResult().actorParent() ==
                        nullptr);
  mOpResult.get_StorageOpenResult().actorParent() =
      mManager->SendPCacheConstructor(
          new CacheParent(aManager, aOpenedCacheId));
}

void AutoParentOpResult::Add(const SavedResponse& aSavedResponse,
                             StreamList* aStreamList) {
  MOZ_DIAGNOSTIC_ASSERT(!mSent);

  switch (mOpResult.type()) {
    case CacheOpResult::TCacheMatchResult: {
      CacheMatchResult& result = mOpResult.get_CacheMatchResult();
      MOZ_DIAGNOSTIC_ASSERT(result.maybeResponse().isNothing());
      result.maybeResponse().emplace(aSavedResponse.mValue);
      SerializeResponseBody(aSavedResponse, aStreamList,
                            &result.maybeResponse().ref());
      break;
    }
    case CacheOpResult::TCacheMatchAllResult: {
      CacheMatchAllResult& result = mOpResult.get_CacheMatchAllResult();
      // Ensure that we don't realloc the array since this can result
      // in our AutoIPCStream objects to reference the wrong memory
      // location.  This should never happen and is a UAF if it does.
      // Therefore make this a release assertion.
      MOZ_RELEASE_ASSERT(result.responseList().Length() <
                         result.responseList().Capacity());
      result.responseList().AppendElement(aSavedResponse.mValue);
      SerializeResponseBody(aSavedResponse, aStreamList,
                            &result.responseList().LastElement());
      break;
    }
    case CacheOpResult::TStorageMatchResult: {
      StorageMatchResult& result = mOpResult.get_StorageMatchResult();
      MOZ_DIAGNOSTIC_ASSERT(result.maybeResponse().isNothing());
      result.maybeResponse().emplace(aSavedResponse.mValue);
      SerializeResponseBody(aSavedResponse, aStreamList,
                            &result.maybeResponse().ref());
      break;
    }
    default:
      MOZ_CRASH("Cache result type cannot handle returning a Response!");
  }
}

void AutoParentOpResult::Add(const SavedRequest& aSavedRequest,
                             StreamList* aStreamList) {
  MOZ_DIAGNOSTIC_ASSERT(!mSent);

  switch (mOpResult.type()) {
    case CacheOpResult::TCacheKeysResult: {
      CacheKeysResult& result = mOpResult.get_CacheKeysResult();
      // Ensure that we don't realloc the array since this can result
      // in our AutoIPCStream objects to reference the wrong memory
      // location.  This should never happen and is a UAF if it does.
      // Therefore make this a release assertion.
      MOZ_RELEASE_ASSERT(result.requestList().Length() <
                         result.requestList().Capacity());
      result.requestList().AppendElement(aSavedRequest.mValue);
      CacheRequest& request = result.requestList().LastElement();

      if (!aSavedRequest.mHasBodyId) {
        request.body() = Nothing();
        break;
      }

      request.body().emplace(CacheReadStream());
      SerializeReadStream(aSavedRequest.mBodyId, aStreamList,
                          &request.body().ref());
      break;
    }
    default:
      MOZ_CRASH("Cache result type cannot handle returning a Request!");
  }
}

const CacheOpResult& AutoParentOpResult::SendAsOpResult() {
  MOZ_DIAGNOSTIC_ASSERT(!mSent);
  mSent = true;
  for (UniquePtr<AutoIPCStream>& autoStream : mStreamCleanupList) {
    autoStream->TakeOptionalValue();
  }
  return mOpResult;
}

void AutoParentOpResult::SerializeResponseBody(
    const SavedResponse& aSavedResponse, StreamList* aStreamList,
    CacheResponse* aResponseOut) {
  MOZ_DIAGNOSTIC_ASSERT(aResponseOut);

  if (!aSavedResponse.mHasBodyId) {
    aResponseOut->body() = Nothing();
    return;
  }

  aResponseOut->body().emplace(CacheReadStream());
  SerializeReadStream(aSavedResponse.mBodyId, aStreamList,
                      &aResponseOut->body().ref());
}

void AutoParentOpResult::SerializeReadStream(const nsID& aId,
                                             StreamList* aStreamList,
                                             CacheReadStream* aReadStreamOut) {
  MOZ_DIAGNOSTIC_ASSERT(aStreamList);
  MOZ_DIAGNOSTIC_ASSERT(aReadStreamOut);
  MOZ_DIAGNOSTIC_ASSERT(!mSent);

  nsCOMPtr<nsIInputStream> stream = aStreamList->Extract(aId);

  if (!mStreamControl) {
    mStreamControl = static_cast<CacheStreamControlParent*>(
        mManager->SendPCacheStreamControlConstructor(
            new CacheStreamControlParent()));

    // If this failed, then the child process is gone.  Warn and allow actor
    // cleanup to proceed as normal.
    if (!mStreamControl) {
      NS_WARNING("Cache failed to create stream control actor.");
      return;
    }
  }

  aStreamList->SetStreamControl(mStreamControl);

  RefPtr<ReadStream> readStream =
      ReadStream::Create(mStreamControl, aId, stream);
  ErrorResult rv;
  readStream->Serialize(aReadStreamOut, mStreamCleanupList, rv);
  MOZ_DIAGNOSTIC_ASSERT(!rv.Failed());
}

}  // namespace cache
}  // namespace dom
}  // namespace mozilla
