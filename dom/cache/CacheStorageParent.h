/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStorageParent_h
#define mozilla_dom_cache_CacheStorageParent_h

#include "mozilla/dom/cache/PCacheStorageParent.h"
#include "mozilla/dom/cache/PrincipalVerifier.h"
#include "mozilla/dom/cache/Types.h"

namespace mozilla {
namespace dom {
namespace cache {

class ManagerId;

class CacheStorageParent final : public PCacheStorageParent,
                                 public PrincipalVerifier::Listener {
  friend class PCacheStorageParent;

 public:
  CacheStorageParent(PBackgroundParent* aManagingActor, Namespace aNamespace,
                     const mozilla::ipc::PrincipalInfo& aPrincipalInfo);
  virtual ~CacheStorageParent();

 private:
  // PCacheStorageParent methods
  virtual void ActorDestroy(ActorDestroyReason aReason) override;

  PCacheOpParent* AllocPCacheOpParent(const CacheOpArgs& aOpArgs);

  bool DeallocPCacheOpParent(PCacheOpParent* aActor);

  virtual mozilla::ipc::IPCResult RecvPCacheOpConstructor(
      PCacheOpParent* actor, const CacheOpArgs& aOpArgs) override;

  mozilla::ipc::IPCResult RecvTeardown();

  // PrincipalVerifier::Listener methods
  virtual void OnPrincipalVerified(
      nsresult aRv, const SafeRefPtr<ManagerId>& aManagerId) override;

  const Namespace mNamespace;
  RefPtr<PrincipalVerifier> mVerifier;
  nsresult mVerifiedStatus;
  SafeRefPtr<ManagerId> mManagerId;
};

}  // namespace cache
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_cache_CacheStorageParent_h
