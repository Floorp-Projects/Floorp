/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheParent_h
#define mozilla_dom_cache_CacheParent_h

#include "mozilla/dom/SafeRefPtr.h"
#include "mozilla/dom/cache/PCacheParent.h"
#include "mozilla/dom/cache/Types.h"

namespace mozilla::dom::cache {

class Manager;

class CacheParent final : public PCacheParent {
  friend class PCacheParent;

 public:
  CacheParent(SafeRefPtr<cache::Manager> aManager, CacheId aCacheId);

 private:
  virtual ~CacheParent();

  // PCacheParent methods
  virtual void ActorDestroy(ActorDestroyReason aReason) override;

  already_AddRefed<PCacheOpParent> AllocPCacheOpParent(
      const CacheOpArgs& aOpArgs);

  virtual mozilla::ipc::IPCResult RecvPCacheOpConstructor(
      PCacheOpParent* actor, const CacheOpArgs& aOpArgs) override;

  mozilla::ipc::IPCResult RecvTeardown();

  SafeRefPtr<cache::Manager> mManager;
  const CacheId mCacheId;

  NS_INLINE_DECL_REFCOUNTING(CacheParent, override)
};

}  // namespace mozilla::dom::cache

#endif  // mozilla_dom_cache_CacheParent_h
