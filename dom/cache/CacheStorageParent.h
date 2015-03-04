/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStorageParent_h
#define mozilla_dom_cache_CacheStorageParent_h

#include "mozilla/dom/cache/CacheInitData.h"
#include "mozilla/dom/cache/PCacheStorageParent.h"
#include "mozilla/dom/cache/Manager.h"
#include "mozilla/dom/cache/PrincipalVerifier.h"
#include "mozilla/dom/cache/Types.h"

template <class T> class nsRefPtr;

namespace mozilla {
namespace dom {
namespace cache {

class CacheStreamControlParent;
class ManagerId;

class CacheStorageParent MOZ_FINAL : public PCacheStorageParent
                                   , public PrincipalVerifier::Listener
                                   , public Manager::Listener
{
public:
  CacheStorageParent(PBackgroundParent* aManagingActor, Namespace aNamespace,
                     const mozilla::ipc::PrincipalInfo& aPrincipalInfo);
  virtual ~CacheStorageParent();

private:
  // PCacheStorageParent methods
  virtual void ActorDestroy(ActorDestroyReason aReason) MOZ_OVERRIDE;
  virtual bool RecvTeardown() MOZ_OVERRIDE;
  virtual bool RecvMatch(const RequestId& aRequestId,
                         const PCacheRequest& aRequest,
                         const PCacheQueryParams& aParams) MOZ_OVERRIDE;
  virtual bool RecvHas(const RequestId& aRequestId,
                       const nsString& aKey) MOZ_OVERRIDE;
  virtual bool RecvOpen(const RequestId& aRequestId,
                        const nsString& aKey) MOZ_OVERRIDE;
  virtual bool RecvDelete(const RequestId& aRequestId,
                          const nsString& aKey) MOZ_OVERRIDE;
  virtual bool RecvKeys(const RequestId& aRequestId) MOZ_OVERRIDE;

  // PrincipalVerifier::Listener methods
  virtual void OnPrincipalVerified(nsresult aRv,
                                   ManagerId* aManagerId) MOZ_OVERRIDE;

  // Manager::Listener methods
  virtual void OnStorageMatch(RequestId aRequestId, nsresult aRv,
                              const SavedResponse* aResponse,
                              StreamList* aStreamList) MOZ_OVERRIDE;
  virtual void OnStorageHas(RequestId aRequestId, nsresult aRv,
                            bool aCacheFound) MOZ_OVERRIDE;
  virtual void OnStorageOpen(RequestId aRequestId, nsresult aRv,
                             CacheId aCacheId) MOZ_OVERRIDE;
  virtual void OnStorageDelete(RequestId aRequestId, nsresult aRv,
                               bool aCacheDeleted) MOZ_OVERRIDE;
  virtual void OnStorageKeys(RequestId aRequestId, nsresult aRv,
                             const nsTArray<nsString>& aKeys) MOZ_OVERRIDE;

  CacheStreamControlParent*
  SerializeReadStream(CacheStreamControlParent *aStreamControl, const nsID& aId,
                      StreamList* aStreamList,
                      PCacheReadStream* aReadStreamOut);

  void RetryPendingRequests();

  nsresult RequestManager(RequestId aRequestId, cache::Manager** aManagerOut);
  void ReleaseManager(RequestId aRequestId);

  const Namespace mNamespace;
  nsRefPtr<PrincipalVerifier> mVerifier;
  nsresult mVerifiedStatus;
  nsRefPtr<ManagerId> mManagerId;
  nsRefPtr<cache::Manager> mManager;

  enum Op
  {
    OP_MATCH,
    OP_HAS,
    OP_OPEN,
    OP_DELETE,
    OP_KEYS
  };

  struct Entry
  {
    Op mOp;
    RequestId mRequestId;
    nsString mKey;
    PCacheRequest mRequest;
    PCacheQueryParams mParams;
  };

  nsTArray<Entry> mPendingRequests;
  nsTArray<RequestId> mActiveRequests;
};

} // namesapce cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStorageParent_h
