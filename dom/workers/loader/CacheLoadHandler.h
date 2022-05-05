/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_CacheLoadHandler_h__
#define mozilla_dom_workers_CacheLoadHandler_h__

#include "nsIContentPolicy.h"
#include "nsIInputStreamPump.h"
#include "nsIStreamLoader.h"
#include "nsStringFwd.h"
#include "nsStreamUtils.h"

#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/ChannelInfo.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/cache/Cache.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/WorkerCommon.h"

#include "mozilla/dom/ScriptLoadInfo.h"
#include "mozilla/dom/workerinternals/ScriptLoader.h"

using mozilla::dom::cache::Cache;
using mozilla::dom::cache::CacheStorage;
using mozilla::ipc::PrincipalInfo;

namespace mozilla {
namespace dom {

namespace workerinternals::loader {

class CacheLoadHandler final : public PromiseNativeHandler,
                               public nsIStreamLoaderObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER

  CacheLoadHandler(WorkerPrivate* aWorkerPrivate, ScriptLoadInfo& aLoadInfo,
                   bool aIsWorkerScript, WorkerScriptLoader* aLoader);

  void Fail(nsresult aRv);

  void Load(Cache* aCache);

  virtual void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                ErrorResult& aRv) override;

  virtual void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                ErrorResult& aRv) override;

 private:
  ~CacheLoadHandler() { AssertIsOnMainThread(); }

  nsresult DataReceivedFromCache(const uint8_t* aString, uint32_t aStringLen,
                                 const mozilla::dom::ChannelInfo& aChannelInfo,
                                 UniquePtr<PrincipalInfo> aPrincipalInfo,
                                 const nsACString& aCSPHeaderValue,
                                 const nsACString& aCSPReportOnlyHeaderValue,
                                 const nsACString& aReferrerPolicyHeaderValue);
  void DataReceived();

  ScriptLoadInfo& mLoadInfo;
  const RefPtr<WorkerScriptLoader> mLoader;
  WorkerPrivate* const mWorkerPrivate;
  const bool mIsWorkerScript;
  bool mFailed;
  const ServiceWorkerState mState;
  nsCOMPtr<nsIInputStreamPump> mPump;
  nsCOMPtr<nsIURI> mBaseURI;
  mozilla::dom::ChannelInfo mChannelInfo;
  UniquePtr<PrincipalInfo> mPrincipalInfo;
  nsCString mCSPHeaderValue;
  nsCString mCSPReportOnlyHeaderValue;
  nsCString mReferrerPolicyHeaderValue;
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;
};

class CacheCreator final : public PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  explicit CacheCreator(WorkerPrivate* aWorkerPrivate);

  void AddLoader(MovingNotNull<RefPtr<CacheLoadHandler>> aLoader) {
    AssertIsOnMainThread();
    MOZ_ASSERT(!mCacheStorage);
    mLoaders.AppendElement(std::move(aLoader));
  }

  virtual void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                ErrorResult& aRv) override;

  virtual void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                ErrorResult& aRv) override;

  // Try to load from cache with aPrincipal used for cache access.
  nsresult Load(nsIPrincipal* aPrincipal);

  Cache* Cache_() const {
    AssertIsOnMainThread();
    MOZ_ASSERT(mCache);
    return mCache;
  }

  nsIGlobalObject* Global() const {
    AssertIsOnMainThread();
    MOZ_ASSERT(mSandboxGlobalObject);
    return mSandboxGlobalObject;
  }

  void DeleteCache();

 private:
  ~CacheCreator() = default;

  nsresult CreateCacheStorage(nsIPrincipal* aPrincipal);

  void FailLoaders(nsresult aRv);

  RefPtr<Cache> mCache;
  RefPtr<CacheStorage> mCacheStorage;
  nsCOMPtr<nsIGlobalObject> mSandboxGlobalObject;
  nsTArray<NotNull<RefPtr<CacheLoadHandler>>> mLoaders;

  nsString mCacheName;
  OriginAttributes mOriginAttributes;
};

class CachePromiseHandler final : public PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  CachePromiseHandler(WorkerScriptLoader* aLoader, ScriptLoadInfo& aLoadInfo)
      : mLoader(aLoader), mLoadInfo(aLoadInfo) {
    AssertIsOnMainThread();
    MOZ_ASSERT(mLoader);
  }

  virtual void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                ErrorResult& aRv) override;

  virtual void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                ErrorResult& aRv) override;

 private:
  ~CachePromiseHandler() { AssertIsOnMainThread(); }

  RefPtr<WorkerScriptLoader> mLoader;
  ScriptLoadInfo& mLoadInfo;
};

}  // namespace workerinternals::loader
}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_workers_CacheLoadHandler_h__ */
