/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_Cache_h
#define mozilla_dom_cache_Cache_h

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

class AutoChildOpArgs;
class CacheChild;

class Cache final : public nsISupports
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
  Add(JSContext* aContext, const RequestOrUSVString& aRequest,
      ErrorResult& aRv);
  already_AddRefed<Promise>
  AddAll(JSContext* aContext,
         const Sequence<OwningRequestOrUSVString>& aRequests, ErrorResult& aRv);
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

  // TypeUtils methods
  virtual nsIGlobalObject*
  GetGlobalObject() const override;

#ifdef DEBUG
  virtual void AssertOwningThread() const override;
#endif

  virtual CachePushStreamChild*
  CreatePushStream(nsIAsyncInputStream* aStream) override;

private:
  class FetchHandler;

  ~Cache();

  // Called when we're destroyed or CCed.
  void DisconnectFromActor();

  already_AddRefed<Promise>
  ExecuteOp(AutoChildOpArgs& aOpArgs, ErrorResult& aRv);

  already_AddRefed<Promise>
  AddAll(const GlobalObject& aGlobal, nsTArray<nsRefPtr<Request>>&& aRequestList,
         ErrorResult& aRv);

  already_AddRefed<Promise>
  PutAll(const nsTArray<nsRefPtr<Request>>& aRequestList,
         const nsTArray<nsRefPtr<Response>>& aResponseList,
         ErrorResult& aRv);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  CacheChild* mActor;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Cache)
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Cache_h
