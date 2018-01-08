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
enum class CallerType : uint32_t;

namespace cache {

class AutoChildOpArgs;
class CacheChild;

class Cache final : public nsISupports
                  , public nsWrapperCache
                  , public TypeUtils
{
public:
  Cache(nsIGlobalObject* aGlobal, CacheChild* aActor, Namespace aNamespace);

  // webidl interface methods
  already_AddRefed<Promise>
  Match(JSContext* aCx, const RequestOrUSVString& aRequest,
        const CacheQueryOptions& aOptions, ErrorResult& aRv);
  already_AddRefed<Promise>
  MatchAll(JSContext* aCx, const Optional<RequestOrUSVString>& aRequest,
           const CacheQueryOptions& aOptions, ErrorResult& aRv);
  already_AddRefed<Promise>
  Add(JSContext* aContext, const RequestOrUSVString& aRequest,
      CallerType aCallerType, ErrorResult& aRv);
  already_AddRefed<Promise>
  AddAll(JSContext* aContext,
         const Sequence<OwningRequestOrUSVString>& aRequests,
         CallerType aCallerType, ErrorResult& aRv);
  already_AddRefed<Promise>
  Put(JSContext* aCx, const RequestOrUSVString& aRequest, Response& aResponse,
      ErrorResult& aRv);
  already_AddRefed<Promise>
  Delete(JSContext* aCx, const RequestOrUSVString& aRequest,
         const CacheQueryOptions& aOptions, ErrorResult& aRv);
  already_AddRefed<Promise>
  Keys(JSContext* aCx, const Optional<RequestOrUSVString>& aRequest,
       const CacheQueryOptions& aParams, ErrorResult& aRv);

  // binding methods
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

  virtual mozilla::ipc::PBackgroundChild*
  GetIPCManager() override;

private:
  class FetchHandler;

  ~Cache();

  // Called when we're destroyed or CCed.
  void DisconnectFromActor();

  already_AddRefed<Promise>
  ExecuteOp(AutoChildOpArgs& aOpArgs, ErrorResult& aRv);

  already_AddRefed<Promise>
  AddAll(const GlobalObject& aGlobal, nsTArray<RefPtr<Request>>&& aRequestList,
         CallerType aCallerType, ErrorResult& aRv);

  already_AddRefed<Promise>
  PutAll(JSContext* aCx, const nsTArray<RefPtr<Request>>& aRequestList,
         const nsTArray<RefPtr<Response>>& aResponseList,
         ErrorResult& aRv);

  OpenMode
  GetOpenMode() const;

  nsCOMPtr<nsIGlobalObject> mGlobal;
  CacheChild* mActor;
  const Namespace mNamespace;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Cache)
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Cache_h
