/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheParent_h
#define mozilla_dom_cache_CacheParent_h

#include "mozilla/dom/cache/FetchPut.h"
#include "mozilla/dom/cache/Manager.h"
#include "mozilla/dom/cache/PCacheParent.h"
#include "mozilla/dom/cache/Types.h"

struct nsID;
template <class T> class nsRefPtr;

namespace mozilla {
namespace dom {
namespace cache {

struct SavedResponse;

class CacheParent final : public PCacheParent
                        , public Manager::Listener
                        , public FetchPut::Listener
{
public:
  CacheParent(cache::Manager* aManager, CacheId aCacheId);
  virtual ~CacheParent();

private:
  // PCacheParent method
  virtual void ActorDestroy(ActorDestroyReason aReason) override;
  virtual PCachePushStreamParent* AllocPCachePushStreamParent() override;
  virtual bool DeallocPCachePushStreamParent(PCachePushStreamParent* aActor) override;
  virtual bool RecvTeardown() override;
  virtual bool
  RecvMatch(const RequestId& aRequestId, const PCacheRequest& aRequest,
            const PCacheQueryParams& aParams) override;
  virtual bool
  RecvMatchAll(const RequestId& aRequestId, const PCacheRequestOrVoid& aRequest,
               const PCacheQueryParams& aParams) override;
  virtual bool
  RecvAddAll(const RequestId& aRequestId,
             nsTArray<PCacheRequest>&& aRequests) override;
  virtual bool
  RecvPut(const RequestId& aRequestId,
          const CacheRequestResponse& aPut) override;
  virtual bool
  RecvDelete(const RequestId& aRequestId, const PCacheRequest& aRequest,
             const PCacheQueryParams& aParams) override;
  virtual bool
  RecvKeys(const RequestId& aRequestId, const PCacheRequestOrVoid& aRequest,
           const PCacheQueryParams& aParams) override;

  // Manager::Listener methods
  virtual void OnCacheMatch(RequestId aRequestId, nsresult aRv,
                            const SavedResponse* aSavedResponse,
                            StreamList* aStreamList) override;
  virtual void OnCacheMatchAll(RequestId aRequestId, nsresult aRv,
                               const nsTArray<SavedResponse>& aSavedResponses,
                               StreamList* aStreamList) override;
  virtual void OnCachePutAll(RequestId aRequestId, nsresult aRv) override;
  virtual void OnCacheDelete(RequestId aRequestId, nsresult aRv,
                             bool aSuccess) override;
  virtual void OnCacheKeys(RequestId aRequestId, nsresult aRv,
                           const nsTArray<SavedRequest>& aSavedRequests,
                           StreamList* aStreamList) override;

  // FetchPut::Listener methods
  virtual void OnFetchPut(FetchPut* aFetchPut, RequestId aRequestId,
                          nsresult aRv) override;

  already_AddRefed<nsIInputStream>
  DeserializeCacheStream(const PCacheReadStreamOrVoid& aStreamOrVoid);

  nsRefPtr<cache::Manager> mManager;
  const CacheId mCacheId;
  nsTArray<nsRefPtr<FetchPut>> mFetchPutList;
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheParent_h
