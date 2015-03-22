/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStorageChild_h
#define mozilla_dom_cache_CacheStorageChild_h

#include "mozilla/dom/cache/ActorChild.h"
#include "mozilla/dom/cache/Types.h"
#include "mozilla/dom/cache/PCacheStorageChild.h"

namespace mozilla {
namespace dom {
namespace cache {

class CacheStorage;
class PCacheChild;
class Feature;

class CacheStorageChild final : public PCacheStorageChild
                              , public ActorChild
{
public:
  CacheStorageChild(CacheStorage* aListener, Feature* aFeature);
  ~CacheStorageChild();

  // Must be called by the associated CacheStorage listener in its
  // ActorDestroy() method.  Also, CacheStorage must Send__delete__() the
  // actor in its destructor to trigger ActorDestroy() if it has not been
  // called yet.
  void ClearListener();

  // ActorChild methods

  // Synchronously call ActorDestroy on our CacheStorage listener and then start
  // the actor destruction asynchronously from the parent-side.
  virtual void StartDestroy() override;

private:
  // PCacheStorageChild methods
  virtual void ActorDestroy(ActorDestroyReason aReason) override;

  virtual bool RecvMatchResponse(const RequestId& aRequestId,
                                 const nsresult& aRv,
                                 const PCacheResponseOrVoid& response) override;
  virtual bool RecvHasResponse(const cache::RequestId& aRequestId,
                               const nsresult& aRv,
                               const bool& aSuccess) override;
  virtual bool RecvOpenResponse(const cache::RequestId& aRequestId,
                                const nsresult& aRv,
                                PCacheChild* aActor) override;
  virtual bool RecvDeleteResponse(const cache::RequestId& aRequestId,
                                  const nsresult& aRv,
                                  const bool& aResult) override;
  virtual bool RecvKeysResponse(const cache::RequestId& aRequestId,
                                const nsresult& aRv,
                                nsTArray<nsString>&& aKeys) override;

  // Use a weak ref so actor does not hold DOM object alive past content use.
  // The CacheStorage object must call ClearListener() to null this before its
  // destroyed.
  CacheStorage* MOZ_NON_OWNING_REF mListener;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStorageChild_h
