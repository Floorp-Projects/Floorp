/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheParent_h
#define mozilla_dom_cache_CacheParent_h

#include "mozilla/dom/cache/PCacheParent.h"
#include "mozilla/dom/cache/Types.h"

namespace mozilla {
namespace dom {
namespace cache {

class Manager;

class CacheParent final : public PCacheParent
{
public:
  CacheParent(cache::Manager* aManager, CacheId aCacheId);
  virtual ~CacheParent();

private:
  // PCacheParent methods
  virtual void ActorDestroy(ActorDestroyReason aReason) override;

  virtual PCacheOpParent*
  AllocPCacheOpParent(const CacheOpArgs& aOpArgs) override;

  virtual bool
  DeallocPCacheOpParent(PCacheOpParent* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvPCacheOpConstructor(PCacheOpParent* actor,
                          const CacheOpArgs& aOpArgs) override;

  virtual mozilla::ipc::IPCResult
  RecvTeardown() override;

  RefPtr<cache::Manager> mManager;
  const CacheId mCacheId;
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheParent_h
