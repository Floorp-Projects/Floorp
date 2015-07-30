/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheOpChild_h
#define mozilla_dom_cache_CacheOpChild_h

#include "mozilla/dom/cache/ActorChild.h"
#include "mozilla/dom/cache/PCacheOpChild.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "mozilla/nsRefPtr.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class Promise;

namespace cache {

class CacheOpChild final : public PCacheOpChild
                         , public ActorChild
                         , public TypeUtils
{
  friend class CacheChild;
  friend class CacheStorageChild;

private:
  // This class must be constructed by CacheChild or CacheStorageChild using
  // their ExecuteOp() factory method.
  CacheOpChild(Feature* aFeature, nsIGlobalObject* aGlobal,
               nsISupports* aParent, Promise* aPromise);
  ~CacheOpChild();

  // PCacheOpChild methods
  virtual void
  ActorDestroy(ActorDestroyReason aReason) override;

  virtual bool
  Recv__delete__(const ErrorResult& aRv, const CacheOpResult& aResult) override;

  // ActorChild methods
  virtual void
  StartDestroy() override;

  // TypeUtils methods
  virtual nsIGlobalObject*
  GetGlobalObject() const override;

#ifdef DEBUG
  virtual void
  AssertOwningThread() const override;
#endif

  virtual CachePushStreamChild*
  CreatePushStream(nsIAsyncInputStream* aStream) override;

  // Utility methods
  void
  HandleResponse(const CacheResponseOrVoid& aResponseOrVoid);

  void
  HandleResponseList(const nsTArray<CacheResponse>& aResponseList);

  void
  HandleRequestList(const nsTArray<CacheRequest>& aRequestList);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  // Hold the parent Cache or CacheStorage object alive until this async
  // operation completes.
  nsCOMPtr<nsISupports> mParent;
  nsRefPtr<Promise> mPromise;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheOpChild_h
