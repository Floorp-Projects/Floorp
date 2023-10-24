/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerLoadContext.h"
#include "mozilla/dom/workerinternals/ScriptLoader.h"
#include "CacheLoadHandler.h"  // CacheCreator

namespace mozilla {
namespace dom {

WorkerLoadContext::WorkerLoadContext(
    Kind aKind, const Maybe<ClientInfo>& aClientInfo,
    workerinternals::loader::WorkerScriptLoader* aScriptLoader,
    bool aOnlyExistingCachedResourcesAllowed)
    : JS::loader::LoadContextBase(JS::loader::ContextKind::Worker),
      mKind(aKind),
      mClientInfo(aClientInfo),
      mScriptLoader(aScriptLoader),
      mOnlyExistingCachedResourcesAllowed(
          aOnlyExistingCachedResourcesAllowed){};

ThreadSafeRequestHandle::ThreadSafeRequestHandle(
    JS::loader::ScriptLoadRequest* aRequest, nsISerialEventTarget* aSyncTarget)
    : mRequest(aRequest), mOwningEventTarget(aSyncTarget) {}

already_AddRefed<JS::loader::ScriptLoadRequest>
ThreadSafeRequestHandle::ReleaseRequest() {
  RefPtr<JS::loader::ScriptLoadRequest> request;
  mRequest.swap(request);
  mRunnable = nullptr;
  return request.forget();
}

nsresult ThreadSafeRequestHandle::OnStreamComplete(nsresult aStatus) {
  return mRunnable->OnStreamComplete(this, aStatus);
}

void ThreadSafeRequestHandle::LoadingFinished(nsresult aRv) {
  mRunnable->LoadingFinished(this, aRv);
}

void ThreadSafeRequestHandle::MaybeExecuteFinishedScripts() {
  mRunnable->MaybeExecuteFinishedScripts(this);
}

bool ThreadSafeRequestHandle::IsCancelled() { return mRunnable->IsCancelled(); }

nsresult ThreadSafeRequestHandle::GetCancelResult() {
  return mRunnable->GetCancelResult();
}

workerinternals::loader::CacheCreator*
ThreadSafeRequestHandle::GetCacheCreator() {
  AssertIsOnMainThread();
  return mRunnable->GetCacheCreator();
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
