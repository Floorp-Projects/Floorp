/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_Cache_h
#define mozilla_dom_cache_Cache_h

#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/cache/Types.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class OwningRequestOrUSVString;
class Promise;
struct CacheQueryOptions;
class RequestOrUSVString;
class Response;
template<typename T> class Optional;
template<typename T> class Sequence;

namespace cache {

class CacheChild;
class PCacheRequest;
class PCacheResponse;
class PCacheResponseOrVoid;

class Cache final : public PromiseNativeHandler
                  , public nsWrapperCache
                  , public TypeUtils
{
public:
  Cache(nsIGlobalObject* aGlobal, CacheChild* aActor);

  // webidl interface methods
  already_AddRefed<Promise>
  Match(const RequestOrUSVString& aRequest, const CacheQueryOptions& aOptions,
        ErrorResult& aRv);
  already_AddRefed<Promise>
  MatchAll(const Optional<RequestOrUSVString>& aRequest,
           const CacheQueryOptions& aOptions, ErrorResult& aRv);
  already_AddRefed<Promise>
  Add(const RequestOrUSVString& aRequest, ErrorResult& aRv);
  already_AddRefed<Promise>
  AddAll(const Sequence<OwningRequestOrUSVString>& aRequests,
         ErrorResult& aRv);
  already_AddRefed<Promise>
  Put(const RequestOrUSVString& aRequest, Response& aResponse,
      ErrorResult& aRv);
  already_AddRefed<Promise>
  Delete(const RequestOrUSVString& aRequest, const CacheQueryOptions& aOptions,
         ErrorResult& aRv);
  already_AddRefed<Promise>
  Keys(const Optional<RequestOrUSVString>& aRequest,
       const CacheQueryOptions& aParams, ErrorResult& aRv);

  // binding methods
  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);

  nsISupports* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aContext, JS::Handle<JSObject*> aGivenProto) override;

  // Called when CacheChild actor is being destroyed
  void DestroyInternal(CacheChild* aActor);

  // methods forwarded from CacheChild
  void RecvMatchResponse(RequestId aRequestId, nsresult aRv,
                         const PCacheResponseOrVoid& aResponse);
  void RecvMatchAllResponse(RequestId aRequestId, nsresult aRv,
                            const nsTArray<PCacheResponse>& aResponses);
  void RecvAddAllResponse(RequestId aRequestId, nsresult aRv);
  void RecvPutResponse(RequestId aRequestId, nsresult aRv);

  void RecvDeleteResponse(RequestId aRequestId, nsresult aRv,
                          bool aSuccess);
  void RecvKeysResponse(RequestId aRequestId, nsresult aRv,
                        const nsTArray<PCacheRequest>& aRequests);

  // TypeUtils methods
  virtual nsIGlobalObject*
  GetGlobalObject() const override;

#ifdef DEBUG
  virtual void AssertOwningThread() const override;
#endif

  virtual CachePushStreamChild*
  CreatePushStream(nsIAsyncInputStream* aStream) override;

  // PromiseNativeHandler methods
  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

private:
  ~Cache();

  // Called when we're destroyed or CCed.
  void DisconnectFromActor();

  // TODO: Replace with actor-per-request model during refactor (bug 1110485)
  RequestId AddRequestPromise(Promise* aPromise, ErrorResult& aRv);
  already_AddRefed<Promise> RemoveRequestPromise(RequestId aRequestId);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  CacheChild* mActor;
  nsTArray<nsRefPtr<Promise>> mRequestPromises;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Cache)
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Cache_h
