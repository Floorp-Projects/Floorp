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

class CacheDBConnection;
class CacheStreamControlParent;
struct SavedResponse;
struct StreamHolder;

class CacheParent MOZ_FINAL : public PCacheParent
                            , public Manager::Listener
                            , public FetchPut::Listener
{
public:
  CacheParent(cache::Manager* aManager, CacheId aCacheId);
  virtual ~CacheParent();

private:
  // PCacheParent method
  virtual void ActorDestroy(ActorDestroyReason aReason) MOZ_OVERRIDE;
  virtual bool RecvTeardown() MOZ_OVERRIDE;
  virtual bool
  RecvMatch(const RequestId& aRequestId, const PCacheRequest& aRequest,
            const PCacheQueryParams& aParams) MOZ_OVERRIDE;
  virtual bool
  RecvMatchAll(const RequestId& aRequestId, const PCacheRequestOrVoid& aRequest,
               const PCacheQueryParams& aParams) MOZ_OVERRIDE;
  virtual bool
  RecvAddAll(const RequestId& aRequestId,
             nsTArray<PCacheRequest>&& aRequests) MOZ_OVERRIDE;
  virtual bool
  RecvPut(const RequestId& aRequestId,
          const CacheRequestResponse& aPut) MOZ_OVERRIDE;
  virtual bool
  RecvDelete(const RequestId& aRequestId, const PCacheRequest& aRequest,
             const PCacheQueryParams& aParams) MOZ_OVERRIDE;
  virtual bool
  RecvKeys(const RequestId& aRequestId, const PCacheRequestOrVoid& aRequest,
           const PCacheQueryParams& aParams) MOZ_OVERRIDE;

  // Manager::Listener methods
  virtual void OnCacheMatch(RequestId aRequestId, nsresult aRv,
                            const SavedResponse* aSavedResponse,
                            StreamList* aStreamList) MOZ_OVERRIDE;
  virtual void OnCacheMatchAll(RequestId aRequestId, nsresult aRv,
                               const nsTArray<SavedResponse>& aSavedResponses,
                               StreamList* aStreamList) MOZ_OVERRIDE;
  virtual void OnCachePutAll(RequestId aRequestId, nsresult aRv) MOZ_OVERRIDE;
  virtual void OnCacheDelete(RequestId aRequestId, nsresult aRv,
                             bool aSuccess) MOZ_OVERRIDE;
  virtual void OnCacheKeys(RequestId aRequestId, nsresult aRv,
                           const nsTArray<SavedRequest>& aSavedRequests,
                           StreamList* aStreamList) MOZ_OVERRIDE;

  // FetchPut::Listener methods
  virtual void OnFetchPut(FetchPut* aFetchPut, RequestId aRequestId,
                          nsresult aRv) MOZ_OVERRIDE;

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
