/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheOpParent_h
#define mozilla_dom_cache_CacheOpParent_h

#include "mozilla/dom/cache/Manager.h"
#include "mozilla/dom/cache/PCacheOpParent.h"
#include "mozilla/dom/cache/PrincipalVerifier.h"
#include "nsTArray.h"

namespace mozilla {
namespace ipc {
class PBackgroundParent;
} // namespace ipc
namespace dom {
namespace cache {

class CacheOpParent final : public PCacheOpParent
                          , public PrincipalVerifier::Listener
                          , public Manager::Listener
{
  // to allow use of convenience overrides
  using Manager::Listener::OnOpComplete;

public:
  CacheOpParent(mozilla::ipc::PBackgroundParent* aIpcManager, CacheId aCacheId,
                const CacheOpArgs& aOpArgs);
  CacheOpParent(mozilla::ipc::PBackgroundParent* aIpcManager,
                Namespace aNamespace, const CacheOpArgs& aOpArgs);
  ~CacheOpParent();

  void
  Execute(ManagerId* aManagerId);

  void
  Execute(cache::Manager* aManager);

  void
  WaitForVerification(PrincipalVerifier* aVerifier);

private:
  // PCacheOpParent methods
  virtual void
  ActorDestroy(ActorDestroyReason aReason) override;

  // PrincipalVerifier::Listener methods
  virtual void
  OnPrincipalVerified(nsresult aRv, ManagerId* aManagerId) override;

  // Manager::Listener methods
  virtual void
  OnOpComplete(ErrorResult&& aRv, const CacheOpResult& aResult,
               CacheId aOpenedCacheId,
               const nsTArray<SavedResponse>& aSavedResponseList,
               const nsTArray<SavedRequest>& aSavedRequestList,
               StreamList* aStreamList) override;

  // utility methods
  already_AddRefed<nsIInputStream>
  DeserializeCacheStream(const CacheReadStreamOrVoid& aStreamOrVoid);

  mozilla::ipc::PBackgroundParent* mIpcManager;
  const CacheId mCacheId;
  const Namespace mNamespace;
  const CacheOpArgs mOpArgs;
  RefPtr<cache::Manager> mManager;
  RefPtr<PrincipalVerifier> mVerifier;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheOpParent_h
