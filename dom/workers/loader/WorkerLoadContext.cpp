/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerLoadContext.h"
#include "CacheLoadHandler.h"  // CacheCreator

namespace mozilla {
namespace dom {

WorkerLoadContext::WorkerLoadContext(Kind aKind,
                                     const Maybe<ClientInfo>& aClientInfo)
    : JS::loader::LoadContextBase(JS::loader::ContextKind::Worker),
      mKind(aKind),
      mClientInfo(aClientInfo){};

void WorkerLoadContext::SetCacheCreator(
    RefPtr<workerinternals::loader::CacheCreator> aCacheCreator) {
  AssertIsOnMainThread();
  mCacheCreator =
      new nsMainThreadPtrHolder<workerinternals::loader::CacheCreator>(
          "WorkerLoadContext::mCacheCreator", aCacheCreator);
}

void WorkerLoadContext::ClearCacheCreator() {
  AssertIsOnMainThread();
  mCacheCreator = nullptr;
}

RefPtr<workerinternals::loader::CacheCreator>
WorkerLoadContext::GetCacheCreator() {
  AssertIsOnMainThread();
  return mCacheCreator.get();
}

ThreadSafeRequestHandle::ThreadSafeRequestHandle(
    JS::loader::ScriptLoadRequest* aRequest, nsISerialEventTarget* aSyncTarget)
    : mRequest(aRequest), mOwningEventTarget(aSyncTarget) {}

already_AddRefed<JS::loader::ScriptLoadRequest>
ThreadSafeRequestHandle::ReleaseRequest() {
  return mRequest.forget();
}

ThreadSafeRequestHandle::~ThreadSafeRequestHandle() {
  // Normally we only touch mStrongRef on the owning thread.  This is safe,
  // however, because when we do use mStrongRef on the owning thread we are
  // always holding a strong ref to the ThreadsafeHandle via the owning
  // runnable.  So we cannot run the ThreadsafeHandle destructor simultaneously.
  if (!mRequest || mOwningEventTarget->IsOnCurrentThread()) {
    return;
  }

  // Dispatch in NS_ProxyRelease is guaranteed to succeed here because we block
  // shutdown until all Contexts have been destroyed. Therefore it is ok to have
  // MOZ_ALWAYS_SUCCEED here.
  MOZ_ALWAYS_SUCCEEDS(NS_ProxyRelease("ThreadSafeRequestHandle::mRequest",
                                      mOwningEventTarget, mRequest.forget()));
}

}  // namespace dom
}  // namespace mozilla
