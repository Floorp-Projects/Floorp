/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoader.h"

#include <algorithm>
#include <type_traits>

#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocShell.h"
#include "nsIHttpChannel.h"
#include "nsIInputStreamPump.h"
#include "nsIIOService.h"
#include "nsIOService.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamLoader.h"
#include "nsIStreamListenerTee.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIURI.h"
#include "nsIXPConnect.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CompilationAndEvaluation.h"
#include "js/SourceText.h"
#include "nsError.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsDocShellCID.h"
#include "nsNetUtil.h"
#include "nsIPipe.h"
#include "nsIOutputStream.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsStreamUtils.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "xpcpublic.h"

#include "mozilla/Assertions.h"
#include "mozilla/LoadContext.h"
#include "mozilla/Maybe.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/dom/cache/Cache.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/ChannelInfo.h"
#include "mozilla/dom/ClientChannelHelper.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/nsCSPService.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SRILogHelper.h"
#include "mozilla/dom/ServiceWorkerBinding.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/UniquePtr.h"
#include "Principal.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

#define MAX_CONCURRENT_SCRIPTS 1000

using mozilla::dom::cache::Cache;
using mozilla::dom::cache::CacheStorage;
using mozilla::ipc::PrincipalInfo;

namespace mozilla {
namespace dom {

namespace {

nsIURI* GetBaseURI(bool aIsMainScript, WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);
  nsIURI* baseURI;
  WorkerPrivate* parentWorker = aWorkerPrivate->GetParent();
  if (aIsMainScript) {
    if (parentWorker) {
      baseURI = parentWorker->GetBaseURI();
      NS_ASSERTION(baseURI, "Should have been set already!");
    } else {
      // May be null.
      baseURI = aWorkerPrivate->GetBaseURI();
    }
  } else {
    baseURI = aWorkerPrivate->GetBaseURI();
    NS_ASSERTION(baseURI, "Should have been set already!");
  }

  return baseURI;
}

nsresult ConstructURI(const nsAString& aScriptURL, nsIURI* baseURI,
                      Document* parentDoc, bool aDefaultURIEncoding,
                      nsIURI** aResult) {
  nsresult rv;
  if (aDefaultURIEncoding) {
    rv = NS_NewURI(aResult, aScriptURL, nullptr, baseURI);
  } else {
    rv = nsContentUtils::NewURIWithDocumentCharset(aResult, aScriptURL,
                                                   parentDoc, baseURI);
  }

  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  return NS_OK;
}

nsresult ChannelFromScriptURL(
    nsIPrincipal* principal, Document* parentDoc, WorkerPrivate* aWorkerPrivate,
    nsILoadGroup* loadGroup, nsIIOService* ios,
    nsIScriptSecurityManager* secMan, nsIURI* aScriptURL,
    const Maybe<ClientInfo>& aClientInfo,
    const Maybe<ServiceWorkerDescriptor>& aController, bool aIsMainScript,
    WorkerScriptType aWorkerScriptType,
    nsContentPolicyType aMainScriptContentPolicyType, nsLoadFlags aLoadFlags,
    nsICookieJarSettings* aCookieJarSettings, nsIReferrerInfo* aReferrerInfo,
    nsIChannel** aChannel) {
  AssertIsOnMainThread();

  nsresult rv;
  nsCOMPtr<nsIURI> uri = aScriptURL;

  // If we have the document, use it. Unfortunately, for dedicated workers
  // 'parentDoc' ends up being the parent document, which is not the document
  // that we want to use. So make sure to avoid using 'parentDoc' in that
  // situation.
  if (parentDoc && parentDoc->NodePrincipal() != principal) {
    parentDoc = nullptr;
  }

  uint32_t secFlags = aIsMainScript
                          ? nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED
                          : nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS;

  bool inheritAttrs = nsContentUtils::ChannelShouldInheritPrincipal(
      principal, uri, true /* aInheritForAboutBlank */,
      false /* aForceInherit */);

  bool isData = uri->SchemeIs("data");
  bool isURIUniqueOrigin =
      StaticPrefs::security_data_uri_unique_opaque_origin() && isData;
  if (inheritAttrs && !isURIUniqueOrigin) {
    secFlags |= nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }

  if (aWorkerScriptType == DebuggerScript) {
    // A DebuggerScript needs to be a local resource like chrome: or resource:
    bool isUIResource = false;
    rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_IS_UI_RESOURCE,
                             &isUIResource);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isUIResource) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    secFlags |= nsILoadInfo::SEC_ALLOW_CHROME;
  }

  // Note: this is for backwards compatibility and goes against spec.
  // We should find a better solution.
  if (aIsMainScript && isData) {
    secFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL;
  }

  nsContentPolicyType contentPolicyType =
      aIsMainScript ? aMainScriptContentPolicyType
                    : nsIContentPolicy::TYPE_INTERNAL_WORKER_IMPORT_SCRIPTS;

  // The main service worker script should never be loaded over the network
  // in this path.  It should always be offlined by ServiceWorkerScriptCache.
  // We assert here since this error should also be caught by the runtime
  // check in CacheScriptLoader.
  //
  // Note, if we ever allow service worker scripts to be loaded from network
  // here we need to configure the channel properly.  For example, it must
  // not allow redirects.
  MOZ_DIAGNOSTIC_ASSERT(contentPolicyType !=
                        nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER);

  nsCOMPtr<nsIChannel> channel;
  if (parentDoc) {
    rv = NS_NewChannel(getter_AddRefs(channel), uri, parentDoc, secFlags,
                       contentPolicyType,
                       nullptr,  // aPerformanceStorage
                       loadGroup,
                       nullptr,  // aCallbacks
                       aLoadFlags, ios);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SECURITY_ERR);
  } else {
    // We must have a loadGroup with a load context for the principal to
    // traverse the channel correctly.
    MOZ_ASSERT(loadGroup);
    MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(loadGroup, principal));

    RefPtr<PerformanceStorage> performanceStorage;
    nsCOMPtr<nsICSPEventListener> cspEventListener;
    if (aWorkerPrivate && !aIsMainScript) {
      performanceStorage = aWorkerPrivate->GetPerformanceStorage();
      cspEventListener = aWorkerPrivate->CSPEventListener();
    }

    if (aClientInfo.isSome()) {
      rv = NS_NewChannel(getter_AddRefs(channel), uri, principal,
                         aClientInfo.ref(), aController, secFlags,
                         contentPolicyType, aCookieJarSettings,
                         performanceStorage, loadGroup, nullptr,  // aCallbacks
                         aLoadFlags, ios);
    } else {
      rv = NS_NewChannel(getter_AddRefs(channel), uri, principal, secFlags,
                         contentPolicyType, aCookieJarSettings,
                         performanceStorage, loadGroup, nullptr,  // aCallbacks
                         aLoadFlags, ios);
    }

    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SECURITY_ERR);

    if (cspEventListener) {
      nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
      rv = loadInfo->SetCspEventListener(cspEventListener);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (aReferrerInfo) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
    if (httpChannel) {
      rv = httpChannel->SetReferrerInfo(aReferrerInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  channel.forget(aChannel);
  return rv;
}

struct ScriptLoadInfo {
  ScriptLoadInfo() {
    MOZ_ASSERT(mScriptIsUTF8 == false, "set by member initializer");
    MOZ_ASSERT(mScriptLength == 0, "set by member initializer");
    mScript.mUTF16 = nullptr;
  }

  ~ScriptLoadInfo() {
    if (void* data = mScriptIsUTF8 ? static_cast<void*>(mScript.mUTF8)
                                   : static_cast<void*>(mScript.mUTF16)) {
      js_free(data);
    }
  }

  nsString mURL;

  // This full URL string is populated only if this object is used in a
  // ServiceWorker.
  nsString mFullURL;

  // This promise is set only when the script is for a ServiceWorker but
  // it's not in the cache yet. The promise is resolved when the full body is
  // stored into the cache.  mCachePromise will be set to nullptr after
  // resolution.
  RefPtr<Promise> mCachePromise;

  // The reader stream the cache entry should be filled from, for those cases
  // when we're going to have an mCachePromise.
  nsCOMPtr<nsIInputStream> mCacheReadStream;

  nsCOMPtr<nsIChannel> mChannel;
  Maybe<ClientInfo> mReservedClientInfo;
  nsresult mLoadResult = NS_ERROR_NOT_INITIALIZED;

  // If |mScriptIsUTF8|, then |mUTF8| is active, otherwise |mUTF16| is active.
  union {
    char16_t* mUTF16;
    Utf8Unit* mUTF8;
  } mScript;
  size_t mScriptLength = 0;  // in code units
  bool mScriptIsUTF8 = false;

  bool ScriptTextIsNull() const {
    return mScriptIsUTF8 ? mScript.mUTF8 == nullptr : mScript.mUTF16 == nullptr;
  }

  void InitUTF8Script() {
    MOZ_ASSERT(ScriptTextIsNull());
    MOZ_ASSERT(mScriptLength == 0);

    mScriptIsUTF8 = true;
    mScript.mUTF8 = nullptr;
    mScriptLength = 0;
  }

  void InitUTF16Script() {
    MOZ_ASSERT(ScriptTextIsNull());
    MOZ_ASSERT(mScriptLength == 0);

    mScriptIsUTF8 = false;
    mScript.mUTF16 = nullptr;
    mScriptLength = 0;
  }

  bool mLoadingFinished = false;
  bool mExecutionScheduled = false;
  bool mExecutionResult = false;

  enum CacheStatus {
    // By default a normal script is just loaded from the network. But for
    // ServiceWorkers, we have to check if the cache contains the script and
    // load it from the cache.
    Uncached,

    WritingToCache,

    ReadingFromCache,

    // This script has been loaded from the ServiceWorker cache.
    Cached,

    // This script must be stored in the ServiceWorker cache.
    ToBeCached,

    // Something went wrong or the worker went away.
    Cancel
  };

  CacheStatus mCacheStatus = Uncached;

  nsLoadFlags mLoadFlags = nsIRequest::LOAD_NORMAL;

  Maybe<bool> mMutedErrorFlag;

  bool Finished() const {
    return mLoadingFinished && !mCachePromise && !mChannel;
  }
};

class ScriptLoaderRunnable;

class ScriptExecutorRunnable final : public MainThreadWorkerSyncRunnable {
  ScriptLoaderRunnable& mScriptLoader;
  bool mIsWorkerScript;
  uint32_t mFirstIndex;
  uint32_t mLastIndex;

 public:
  ScriptExecutorRunnable(ScriptLoaderRunnable& aScriptLoader,
                         nsIEventTarget* aSyncLoopTarget, bool aIsWorkerScript,
                         uint32_t aFirstIndex, uint32_t aLastIndex);

 private:
  ~ScriptExecutorRunnable() = default;

  virtual bool IsDebuggerRunnable() const override;

  virtual bool PreRun(WorkerPrivate* aWorkerPrivate) override;

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override;

  virtual void PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                       bool aRunResult) override;

  nsresult Cancel() override;

  void ShutdownScriptLoader(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                            bool aResult, bool aMutedError);

  void LogExceptionToConsole(JSContext* aCx, WorkerPrivate* WorkerPrivate);
};

class CacheScriptLoader;

class CacheCreator final : public PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  explicit CacheCreator(WorkerPrivate* aWorkerPrivate)
      : mCacheName(aWorkerPrivate->ServiceWorkerCacheName()),
        mOriginAttributes(aWorkerPrivate->GetOriginAttributes()) {
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
    AssertIsOnMainThread();
  }

  void AddLoader(CacheScriptLoader* aLoader) {
    AssertIsOnMainThread();
    MOZ_ASSERT(!mCacheStorage);
    mLoaders.AppendElement(aLoader);
  }

  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override;

  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override;

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
  nsTArray<RefPtr<CacheScriptLoader>> mLoaders;

  nsString mCacheName;
  OriginAttributes mOriginAttributes;
};

NS_IMPL_ISUPPORTS0(CacheCreator)

class CacheScriptLoader final : public PromiseNativeHandler,
                                public nsIStreamLoaderObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER

  CacheScriptLoader(WorkerPrivate* aWorkerPrivate, ScriptLoadInfo& aLoadInfo,
                    uint32_t aIndex, bool aIsWorkerScript,
                    ScriptLoaderRunnable* aRunnable)
      : mLoadInfo(aLoadInfo),
        mIndex(aIndex),
        mRunnable(aRunnable),
        mIsWorkerScript(aIsWorkerScript),
        mFailed(false),
        mState(aWorkerPrivate->GetServiceWorkerDescriptor().State()) {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
    mMainThreadEventTarget = aWorkerPrivate->MainThreadEventTarget();
    MOZ_ASSERT(mMainThreadEventTarget);
    mBaseURI = GetBaseURI(mIsWorkerScript, aWorkerPrivate);
    AssertIsOnMainThread();
  }

  void Fail(nsresult aRv);

  void Load(Cache* aCache);

  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override;

  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override;

 private:
  ~CacheScriptLoader() { AssertIsOnMainThread(); }

  ScriptLoadInfo& mLoadInfo;
  uint32_t mIndex;
  RefPtr<ScriptLoaderRunnable> mRunnable;
  bool mIsWorkerScript;
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

NS_IMPL_ISUPPORTS(CacheScriptLoader, nsIStreamLoaderObserver)

class CachePromiseHandler final : public PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  CachePromiseHandler(ScriptLoaderRunnable* aRunnable,
                      ScriptLoadInfo& aLoadInfo, uint32_t aIndex)
      : mRunnable(aRunnable), mLoadInfo(aLoadInfo), mIndex(aIndex) {
    AssertIsOnMainThread();
    MOZ_ASSERT(mRunnable);
  }

  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override;

  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override;

 private:
  ~CachePromiseHandler() { AssertIsOnMainThread(); }

  RefPtr<ScriptLoaderRunnable> mRunnable;
  ScriptLoadInfo& mLoadInfo;
  uint32_t mIndex;
};

NS_IMPL_ISUPPORTS0(CachePromiseHandler)

class LoaderListener final : public nsIStreamLoaderObserver,
                             public nsIRequestObserver {
 public:
  NS_DECL_ISUPPORTS

  LoaderListener(ScriptLoaderRunnable* aRunnable, uint32_t aIndex)
      : mRunnable(aRunnable), mIndex(aIndex) {
    MOZ_ASSERT(mRunnable);
  }

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                   nsresult aStatus, uint32_t aStringLen,
                   const uint8_t* aString) override;

  NS_IMETHOD
  OnStartRequest(nsIRequest* aRequest) override;

  NS_IMETHOD
  OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) override {
    // Nothing to do here!
    return NS_OK;
  }

 private:
  ~LoaderListener() = default;

  RefPtr<ScriptLoaderRunnable> mRunnable;
  uint32_t mIndex;
};

NS_IMPL_ISUPPORTS(LoaderListener, nsIStreamLoaderObserver, nsIRequestObserver)

class ScriptLoaderRunnable final : public nsIRunnable, public nsINamed {
  friend class ScriptExecutorRunnable;
  friend class CachePromiseHandler;
  friend class CacheScriptLoader;
  friend class LoaderListener;

  WorkerPrivate* mWorkerPrivate;
  UniquePtr<SerializedStackHolder> mOriginStack;
  nsString mOriginStackJSON;
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;
  nsTArray<ScriptLoadInfo> mLoadInfos;
  RefPtr<CacheCreator> mCacheCreator;
  Maybe<ClientInfo> mClientInfo;
  Maybe<ServiceWorkerDescriptor> mController;
  bool mIsMainScript;
  WorkerScriptType mWorkerScriptType;
  bool mCanceledMainThread;
  ErrorResult& mRv;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ScriptLoaderRunnable(WorkerPrivate* aWorkerPrivate,
                       UniquePtr<SerializedStackHolder> aOriginStack,
                       nsIEventTarget* aSyncLoopTarget,
                       nsTArray<ScriptLoadInfo>& aLoadInfos,
                       const Maybe<ClientInfo>& aClientInfo,
                       const Maybe<ServiceWorkerDescriptor>& aController,
                       bool aIsMainScript, WorkerScriptType aWorkerScriptType,
                       ErrorResult& aRv)
      : mWorkerPrivate(aWorkerPrivate),
        mOriginStack(std::move(aOriginStack)),
        mSyncLoopTarget(aSyncLoopTarget),
        mClientInfo(aClientInfo),
        mController(aController),
        mIsMainScript(aIsMainScript),
        mWorkerScriptType(aWorkerScriptType),
        mCanceledMainThread(false),
        mRv(aRv) {
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aSyncLoopTarget);
    MOZ_ASSERT_IF(aIsMainScript, aLoadInfos.Length() == 1);

    mLoadInfos.SwapElements(aLoadInfos);
  }

  void CancelMainThreadWithBindingAborted() {
    CancelMainThread(NS_BINDING_ABORTED);
  }

 private:
  ~ScriptLoaderRunnable() = default;

  NS_IMETHOD
  Run() override {
    AssertIsOnMainThread();

    nsresult rv = RunInternal();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      CancelMainThread(rv);
    }

    return NS_OK;
  }

  NS_IMETHOD
  GetName(nsACString& aName) override {
    aName.AssignLiteral("ScriptLoaderRunnable");
    return NS_OK;
  }

  void LoadingFinished(uint32_t aIndex, nsresult aRv) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aIndex < mLoadInfos.Length());
    ScriptLoadInfo& loadInfo = mLoadInfos[aIndex];

    loadInfo.mLoadResult = aRv;

    MOZ_ASSERT(!loadInfo.mLoadingFinished);
    loadInfo.mLoadingFinished = true;

    if (IsMainWorkerScript() && NS_SUCCEEDED(aRv)) {
      MOZ_DIAGNOSTIC_ASSERT(mWorkerPrivate->PrincipalURIMatchesScriptURL());
    }

    MaybeExecuteFinishedScripts(aIndex);
  }

  void MaybeExecuteFinishedScripts(uint32_t aIndex) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aIndex < mLoadInfos.Length());
    ScriptLoadInfo& loadInfo = mLoadInfos[aIndex];

    // We execute the last step if we don't have a pending operation with the
    // cache and the loading is completed.
    if (loadInfo.Finished()) {
      ExecuteFinishedScripts();
    }
  }

  nsresult OnStreamComplete(nsIStreamLoader* aLoader, uint32_t aIndex,
                            nsresult aStatus, uint32_t aStringLen,
                            const uint8_t* aString) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aIndex < mLoadInfos.Length());

    nsresult rv = OnStreamCompleteInternal(aLoader, aStatus, aStringLen,
                                           aString, mLoadInfos[aIndex]);
    LoadingFinished(aIndex, rv);
    return NS_OK;
  }

  nsresult OnStartRequest(nsIRequest* aRequest, uint32_t aIndex) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aIndex < mLoadInfos.Length());

    // If one load info cancels or hits an error, it can race with the start
    // callback coming from another load info.
    if (mCanceledMainThread || !mCacheCreator) {
      aRequest->Cancel(NS_ERROR_FAILURE);
      return NS_ERROR_FAILURE;
    }

    ScriptLoadInfo& loadInfo = mLoadInfos[aIndex];

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

    // Checking the MIME type is only required for ServiceWorkers'
    // importScripts, per step 10 of
    // https://w3c.github.io/ServiceWorker/#importscripts
    //
    // "Extract a MIME type from the response’s header list. If this MIME type
    // (ignoring parameters) is not a JavaScript MIME type, return a network
    // error."
    if (mWorkerPrivate->IsServiceWorker()) {
      nsAutoCString mimeType;
      channel->GetContentType(mimeType);

      if (!nsContentUtils::IsJavascriptMIMEType(
              NS_ConvertUTF8toUTF16(mimeType))) {
        const nsCString& scope =
            mWorkerPrivate->GetServiceWorkerRegistrationDescriptor().Scope();

        ServiceWorkerManager::LocalizeAndReportToAllClients(
            scope, "ServiceWorkerRegisterMimeTypeError2",
            nsTArray<nsString>{NS_ConvertUTF8toUTF16(scope),
                               NS_ConvertUTF8toUTF16(mimeType), loadInfo.mURL});

        channel->Cancel(NS_ERROR_DOM_NETWORK_ERR);
        return NS_ERROR_DOM_NETWORK_ERR;
      }
    }

    // Note that importScripts() can redirect.  In theory the main
    // script could also encounter an internal redirect, but currently
    // the assert does not allow that.
    MOZ_ASSERT_IF(mIsMainScript, channel == loadInfo.mChannel);
    loadInfo.mChannel = channel;

    // We synthesize the result code, but its never exposed to content.
    RefPtr<mozilla::dom::InternalResponse> ir =
        new mozilla::dom::InternalResponse(200, NS_LITERAL_CSTRING("OK"));
    ir->SetBody(loadInfo.mCacheReadStream, InternalResponse::UNKNOWN_BODY_SIZE);

    // Drop our reference to the stream now that we've passed it along, so it
    // doesn't hang around once the cache is done with it and keep data alive.
    loadInfo.mCacheReadStream = nullptr;

    // Set the channel info of the channel on the response so that it's
    // saved in the cache.
    ir->InitChannelInfo(channel);

    // Save the principal of the channel since its URI encodes the script URI
    // rather than the ServiceWorkerRegistrationInfo URI.
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    NS_ASSERTION(ssm, "Should never be null!");

    nsCOMPtr<nsIPrincipal> channelPrincipal;
    nsresult rv = ssm->GetChannelResultPrincipal(
        channel, getter_AddRefs(channelPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      channel->Cancel(rv);
      return rv;
    }

    UniquePtr<PrincipalInfo> principalInfo(new PrincipalInfo());
    rv = PrincipalToPrincipalInfo(channelPrincipal, principalInfo.get());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      channel->Cancel(rv);
      return rv;
    }

    ir->SetPrincipalInfo(std::move(principalInfo));
    ir->Headers()->FillResponseHeaders(loadInfo.mChannel);

    RefPtr<mozilla::dom::Response> response =
        new mozilla::dom::Response(mCacheCreator->Global(), ir, nullptr);

    mozilla::dom::RequestOrUSVString request;

    MOZ_ASSERT(!loadInfo.mFullURL.IsEmpty());
    request.SetAsUSVString().ShareOrDependUpon(loadInfo.mFullURL);

    // This JSContext will not end up executing JS code because here there are
    // no ReadableStreams involved.
    AutoJSAPI jsapi;
    jsapi.Init();

    ErrorResult error;
    RefPtr<Promise> cachePromise =
        mCacheCreator->Cache_()->Put(jsapi.cx(), request, *response, error);
    error.WouldReportJSException();
    if (NS_WARN_IF(error.Failed())) {
      nsresult rv = error.StealNSResult();
      channel->Cancel(rv);
      return rv;
    }

    RefPtr<CachePromiseHandler> promiseHandler =
        new CachePromiseHandler(this, loadInfo, aIndex);
    cachePromise->AppendNativeHandler(promiseHandler);

    loadInfo.mCachePromise.swap(cachePromise);
    loadInfo.mCacheStatus = ScriptLoadInfo::WritingToCache;

    return NS_OK;
  }

  bool IsMainWorkerScript() const {
    return mIsMainScript && mWorkerScriptType == WorkerScript;
  }

  bool IsDebuggerScript() const { return mWorkerScriptType == DebuggerScript; }

  void CancelMainThread(nsresult aCancelResult) {
    AssertIsOnMainThread();

    if (mCanceledMainThread) {
      return;
    }

    mCanceledMainThread = true;

    if (mCacheCreator) {
      MOZ_ASSERT(mWorkerPrivate->IsServiceWorker());
      DeleteCache();
    }

    // Cancel all the channels that were already opened.
    for (uint32_t index = 0; index < mLoadInfos.Length(); index++) {
      ScriptLoadInfo& loadInfo = mLoadInfos[index];

      // If promise or channel is non-null, their failures will lead to
      // LoadingFinished being called.
      bool callLoadingFinished = true;

      if (loadInfo.mCachePromise) {
        MOZ_ASSERT(mWorkerPrivate->IsServiceWorker());
        loadInfo.mCachePromise->MaybeReject(aCancelResult);
        loadInfo.mCachePromise = nullptr;
        callLoadingFinished = false;
      }

      if (loadInfo.mChannel) {
        if (NS_SUCCEEDED(loadInfo.mChannel->Cancel(aCancelResult))) {
          callLoadingFinished = false;
        } else {
          NS_WARNING("Failed to cancel channel!");
        }
      }

      if (callLoadingFinished && !loadInfo.Finished()) {
        LoadingFinished(index, aCancelResult);
      }
    }

    ExecuteFinishedScripts();
  }

  void DeleteCache() {
    AssertIsOnMainThread();

    if (!mCacheCreator) {
      return;
    }

    mCacheCreator->DeleteCache();
    mCacheCreator = nullptr;
  }

  nsresult RunInternal() {
    AssertIsOnMainThread();

    if (IsMainWorkerScript()) {
      mWorkerPrivate->SetLoadingWorkerScript(true);
    }

    // Convert the origin stack to JSON (which must be done on the main
    // thread) explicitly, so that we can use the stack to notify the net
    // monitor about every script we load.
    if (mOriginStack) {
      ConvertSerializedStackToJSON(std::move(mOriginStack), mOriginStackJSON);
    }

    if (!mWorkerPrivate->IsServiceWorker() || IsDebuggerScript()) {
      for (uint32_t index = 0, len = mLoadInfos.Length(); index < len;
           ++index) {
        nsresult rv = LoadScript(index);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          LoadingFinished(index, rv);
          return rv;
        }
      }

      return NS_OK;
    }

    MOZ_ASSERT(!mCacheCreator);
    mCacheCreator = new CacheCreator(mWorkerPrivate);

    for (uint32_t index = 0, len = mLoadInfos.Length(); index < len; ++index) {
      RefPtr<CacheScriptLoader> loader = new CacheScriptLoader(
          mWorkerPrivate, mLoadInfos[index], index, IsMainWorkerScript(), this);
      mCacheCreator->AddLoader(loader);
    }

    // The worker may have a null principal on first load, but in that case its
    // parent definitely will have one.
    nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
    if (!principal) {
      WorkerPrivate* parentWorker = mWorkerPrivate->GetParent();
      MOZ_ASSERT(parentWorker, "Must have a parent!");
      principal = parentWorker->GetPrincipal();
    }

    nsresult rv = mCacheCreator->Load(principal);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  nsresult LoadScript(uint32_t aIndex) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aIndex < mLoadInfos.Length());
    MOZ_ASSERT_IF(IsMainWorkerScript(), mWorkerScriptType != DebuggerScript);

    WorkerPrivate* parentWorker = mWorkerPrivate->GetParent();

    // For JavaScript debugging, the devtools server must run on the same
    // thread as the debuggee, indicating the worker uses content principal.
    // However, in Bug 863246, web content will no longer be able to load
    // resource:// URIs by default, so we need system principal to load
    // debugger scripts.
    nsIPrincipal* principal = (mWorkerScriptType == DebuggerScript)
                                  ? nsContentUtils::GetSystemPrincipal()
                                  : mWorkerPrivate->GetPrincipal();

    nsCOMPtr<nsILoadGroup> loadGroup = mWorkerPrivate->GetLoadGroup();
    MOZ_DIAGNOSTIC_ASSERT(principal);

    NS_ENSURE_TRUE(NS_LoadGroupMatchesPrincipal(loadGroup, principal),
                   NS_ERROR_FAILURE);

    // Figure out our base URI.
    nsCOMPtr<nsIURI> baseURI = GetBaseURI(mIsMainScript, mWorkerPrivate);

    // May be null.
    nsCOMPtr<Document> parentDoc = mWorkerPrivate->GetDocument();

    nsCOMPtr<nsIChannel> channel;
    if (IsMainWorkerScript()) {
      // May be null.
      channel = mWorkerPrivate->ForgetWorkerChannel();
    }

    nsCOMPtr<nsIIOService> ios(do_GetIOService());

    nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
    NS_ASSERTION(secMan, "This should never be null!");

    ScriptLoadInfo& loadInfo = mLoadInfos[aIndex];
    nsresult& rv = loadInfo.mLoadResult;

    nsLoadFlags loadFlags = loadInfo.mLoadFlags;

    // Get the top-level worker.
    WorkerPrivate* topWorkerPrivate = mWorkerPrivate;
    WorkerPrivate* parent = topWorkerPrivate->GetParent();
    while (parent) {
      topWorkerPrivate = parent;
      parent = topWorkerPrivate->GetParent();
    }

    // If the top-level worker is a dedicated worker and has a window, and the
    // window has a docshell, the caching behavior of this worker should match
    // that of that docshell.
    if (topWorkerPrivate->IsDedicatedWorker()) {
      nsCOMPtr<nsPIDOMWindowInner> window = topWorkerPrivate->GetWindow();
      if (window) {
        nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
        if (docShell) {
          nsresult rv = docShell->GetDefaultLoadFlags(&loadFlags);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }
    }

    if (!channel) {
      // Only top level workers' main script use the document charset for the
      // script uri encoding. Otherwise, default encoding (UTF-8) is applied.
      bool useDefaultEncoding = !(!parentWorker && IsMainWorkerScript());
      nsCOMPtr<nsIURI> url;
      rv = ConstructURI(loadInfo.mURL, baseURI, parentDoc, useDefaultEncoding,
                        getter_AddRefs(url));
      if (NS_FAILED(rv)) {
        return rv;
      }

      nsCOMPtr<nsIReferrerInfo> referrerInfo =
          ReferrerInfo::CreateForFetch(principal, nullptr);
      if (parentWorker && !IsMainWorkerScript()) {
        referrerInfo =
            static_cast<ReferrerInfo*>(referrerInfo.get())
                ->CloneWithNewPolicy(parentWorker->GetReferrerPolicy());
      }

      rv = ChannelFromScriptURL(principal, parentDoc, mWorkerPrivate, loadGroup,
                                ios, secMan, url, mClientInfo, mController,
                                IsMainWorkerScript(), mWorkerScriptType,
                                mWorkerPrivate->ContentPolicyType(), loadFlags,
                                mWorkerPrivate->CookieJarSettings(),
                                referrerInfo, getter_AddRefs(channel));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    // Associate any originating stack with the channel.
    if (!mOriginStackJSON.IsEmpty()) {
      NotifyNetworkMonitorAlternateStack(channel, mOriginStackJSON);
    }

    // We need to know which index we're on in OnStreamComplete so we know
    // where to put the result.
    RefPtr<LoaderListener> listener = new LoaderListener(this, aIndex);

    // We don't care about progress so just use the simple stream loader for
    // OnStreamComplete notification only.
    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), listener);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (IsMainWorkerScript()) {
      MOZ_DIAGNOSTIC_ASSERT(loadInfo.mReservedClientInfo.isSome());
      rv = AddClientChannelHelper(
          channel, std::move(loadInfo.mReservedClientInfo), Maybe<ClientInfo>(),
          mWorkerPrivate->HybridEventTarget());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (loadInfo.mCacheStatus != ScriptLoadInfo::ToBeCached) {
      rv = channel->AsyncOpen(loader);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      nsCOMPtr<nsIOutputStream> writer;

      // In case we return early.
      loadInfo.mCacheStatus = ScriptLoadInfo::Cancel;

      rv = NS_NewPipe(
          getter_AddRefs(loadInfo.mCacheReadStream), getter_AddRefs(writer), 0,
          UINT32_MAX,    // unlimited size to avoid writer WOULD_BLOCK case
          true, false);  // non-blocking reader, blocking writer
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsCOMPtr<nsIStreamListenerTee> tee =
          do_CreateInstance(NS_STREAMLISTENERTEE_CONTRACTID);
      rv = tee->Init(loader, writer, listener);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsresult rv = channel->AsyncOpen(tee);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    loadInfo.mChannel.swap(channel);

    return NS_OK;
  }

  nsresult OnStreamCompleteInternal(nsIStreamLoader* aLoader, nsresult aStatus,
                                    uint32_t aStringLen, const uint8_t* aString,
                                    ScriptLoadInfo& aLoadInfo) {
    AssertIsOnMainThread();

    if (!aLoadInfo.mChannel) {
      return NS_BINDING_ABORTED;
    }

    aLoadInfo.mChannel = nullptr;

    if (NS_FAILED(aStatus)) {
      return aStatus;
    }

    NS_ASSERTION(aString, "This should never be null!");

    nsCOMPtr<nsIRequest> request;
    nsresult rv = aLoader->GetRequest(getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    MOZ_ASSERT(channel);

    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    NS_ASSERTION(ssm, "Should never be null!");

    nsCOMPtr<nsIPrincipal> channelPrincipal;
    rv = ssm->GetChannelResultPrincipal(channel,
                                        getter_AddRefs(channelPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
    if (!principal) {
      WorkerPrivate* parentWorker = mWorkerPrivate->GetParent();
      MOZ_ASSERT(parentWorker, "Must have a parent!");
      principal = parentWorker->GetPrincipal();
    }

#ifdef DEBUG
    if (IsMainWorkerScript()) {
      nsCOMPtr<nsIPrincipal> loadingPrincipal =
          mWorkerPrivate->GetLoadingPrincipal();
      // if we are not in a ServiceWorker, and the principal is not null, then
      // the loading principal must subsume the worker principal if it is not a
      // nullPrincipal (sandbox).
      MOZ_ASSERT(!loadingPrincipal || loadingPrincipal->GetIsNullPrincipal() ||
                 principal->GetIsNullPrincipal() ||
                 loadingPrincipal->Subsumes(principal));
    }
#endif

    // We don't mute the main worker script becase we've already done
    // same-origin checks on them so we should be able to see their errors.
    // Note that for data: url, where we allow it through the same-origin check
    // but then give it a different origin.
    aLoadInfo.mMutedErrorFlag.emplace(!IsMainWorkerScript() &&
                                      !principal->Subsumes(channelPrincipal));

    // Make sure we're not seeing the result of a 404 or something by checking
    // the 'requestSucceeded' attribute on the http channel.
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
    nsAutoCString tCspHeaderValue, tCspROHeaderValue, tRPHeaderCValue;

    if (httpChannel) {
      bool requestSucceeded;
      rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!requestSucceeded) {
        return NS_ERROR_NOT_AVAILABLE;
      }

      Unused << httpChannel->GetResponseHeader(
          NS_LITERAL_CSTRING("content-security-policy"), tCspHeaderValue);

      Unused << httpChannel->GetResponseHeader(
          NS_LITERAL_CSTRING("content-security-policy-report-only"),
          tCspROHeaderValue);

      Unused << httpChannel->GetResponseHeader(
          NS_LITERAL_CSTRING("referrer-policy"), tRPHeaderCValue);
    }

    // May be null.
    Document* parentDoc = mWorkerPrivate->GetDocument();

    // Use the regular ScriptLoader for this grunt work! Should be just fine
    // because we're running on the main thread.
    // Worker scripts are always decoded as UTF-8 per spec. Passing null for a
    // channel and UTF-8 for the hint will always interpret |aString| as UTF-8.
    if (StaticPrefs::dom_worker_script_loader_utf8_parsing_enabled()) {
      aLoadInfo.InitUTF8Script();
      rv = ScriptLoader::ConvertToUTF8(
          nullptr, aString, aStringLen, NS_LITERAL_STRING("UTF-8"), parentDoc,
          aLoadInfo.mScript.mUTF8, aLoadInfo.mScriptLength);
    } else {
      aLoadInfo.InitUTF16Script();
      rv = ScriptLoader::ConvertToUTF16(
          nullptr, aString, aStringLen, NS_LITERAL_STRING("UTF-8"), parentDoc,
          aLoadInfo.mScript.mUTF16, aLoadInfo.mScriptLength);
    }
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (aLoadInfo.ScriptTextIsNull()) {
      if (aLoadInfo.mScriptLength != 0) {
        return NS_ERROR_FAILURE;
      }

      nsContentUtils::ReportToConsole(
          nsIScriptError::warningFlag, NS_LITERAL_CSTRING("DOM"), parentDoc,
          nsContentUtils::eDOM_PROPERTIES, "EmptyWorkerSourceWarning");
    }

    // Figure out what we actually loaded.
    nsCOMPtr<nsIURI> finalURI;
    rv = NS_GetFinalChannelURI(channel, getter_AddRefs(finalURI));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString filename;
    rv = finalURI->GetSpec(filename);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!filename.IsEmpty()) {
      // This will help callers figure out what their script url resolved to in
      // case of errors.
      aLoadInfo.mURL.Assign(NS_ConvertUTF8toUTF16(filename));
    }

    // Update the principal of the worker and its base URI if we just loaded the
    // worker's primary script.
    if (IsMainWorkerScript()) {
      // Take care of the base URI first.
      mWorkerPrivate->SetBaseURI(finalURI);

      // Store the channel info if needed.
      mWorkerPrivate->InitChannelInfo(channel);

      // Our final channel principal should match the loading principal
      // in terms of the origin.  This used to be an assert, but it seems
      // there are some rare cases where this check can fail in practice.
      // Perhaps some browser script setting nsIChannel.owner, etc.
      NS_ENSURE_TRUE(mWorkerPrivate->FinalChannelPrincipalIsValid(channel),
                     NS_ERROR_FAILURE);

      // However, we must still override the principal since the nsIPrincipal
      // URL may be different due to same-origin redirects.  Unfortunately this
      // URL must exactly match the final worker script URL in order to
      // properly set the referrer header on fetch/xhr requests.  If bug 1340694
      // is ever fixed this can be removed.
      rv = mWorkerPrivate->SetPrincipalsAndCSPFromChannel(channel);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIContentSecurityPolicy> csp = mWorkerPrivate->GetCSP();
      // We did inherit CSP in bug 1223647. If we do not already have a CSP, we
      // should get it from the HTTP headers on the worker script.
      if (StaticPrefs::security_csp_enable()) {
        if (!csp) {
          rv = mWorkerPrivate->SetCSPFromHeaderValues(tCspHeaderValue,
                                                      tCspROHeaderValue);
          NS_ENSURE_SUCCESS(rv, rv);
        } else {
          csp->EnsureEventTarget(mWorkerPrivate->MainThreadEventTarget());
        }
      }

      mWorkerPrivate->UpdateReferrerInfoFromHeader(tRPHeaderCValue);

      WorkerPrivate* parent = mWorkerPrivate->GetParent();
      if (parent) {
        // XHR Params Allowed
        mWorkerPrivate->SetXHRParamsAllowed(parent->XHRParamsAllowed());
      }

      nsCOMPtr<nsILoadInfo> chanLoadInfo = channel->LoadInfo();
      if (chanLoadInfo) {
        mController = chanLoadInfo->GetController();
      }

      // If we are loading a blob URL we must inherit the controller
      // from the parent.  This is a bit odd as the blob URL may have
      // been created in a different context with a different controller.
      // For now, though, this is what the spec says.  See:
      //
      // https://github.com/w3c/ServiceWorker/issues/1261
      //
      if (IsBlobURI(mWorkerPrivate->GetBaseURI())) {
        MOZ_DIAGNOSTIC_ASSERT(mController.isNothing());
        mController = mWorkerPrivate->GetParentController();
      }
    }

    return NS_OK;
  }

  void DataReceivedFromCache(uint32_t aIndex, const uint8_t* aString,
                             uint32_t aStringLen,
                             const mozilla::dom::ChannelInfo& aChannelInfo,
                             UniquePtr<PrincipalInfo> aPrincipalInfo,
                             const nsACString& aCSPHeaderValue,
                             const nsACString& aCSPReportOnlyHeaderValue,
                             const nsACString& aReferrerPolicyHeaderValue) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aIndex < mLoadInfos.Length());
    ScriptLoadInfo& loadInfo = mLoadInfos[aIndex];
    MOZ_ASSERT(loadInfo.mCacheStatus == ScriptLoadInfo::Cached);

    nsCOMPtr<nsIPrincipal> responsePrincipal =
        PrincipalInfoToPrincipal(*aPrincipalInfo);
    MOZ_DIAGNOSTIC_ASSERT(responsePrincipal);

    nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
    if (!principal) {
      WorkerPrivate* parentWorker = mWorkerPrivate->GetParent();
      MOZ_ASSERT(parentWorker, "Must have a parent!");
      principal = parentWorker->GetPrincipal();
    }

    loadInfo.mMutedErrorFlag.emplace(!principal->Subsumes(responsePrincipal));

    // May be null.
    Document* parentDoc = mWorkerPrivate->GetDocument();

    MOZ_ASSERT(loadInfo.ScriptTextIsNull());

    nsresult rv;
    if (StaticPrefs::dom_worker_script_loader_utf8_parsing_enabled()) {
      loadInfo.InitUTF8Script();
      rv = ScriptLoader::ConvertToUTF8(
          nullptr, aString, aStringLen, NS_LITERAL_STRING("UTF-8"), parentDoc,
          loadInfo.mScript.mUTF8, loadInfo.mScriptLength);
    } else {
      loadInfo.InitUTF16Script();
      rv = ScriptLoader::ConvertToUTF16(
          nullptr, aString, aStringLen, NS_LITERAL_STRING("UTF-8"), parentDoc,
          loadInfo.mScript.mUTF16, loadInfo.mScriptLength);
    }
    if (NS_SUCCEEDED(rv) && IsMainWorkerScript()) {
      nsCOMPtr<nsIURI> finalURI;
      rv = NS_NewURI(getter_AddRefs(finalURI), loadInfo.mFullURL);
      if (NS_SUCCEEDED(rv)) {
        mWorkerPrivate->SetBaseURI(finalURI);
      }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
      MOZ_DIAGNOSTIC_ASSERT(principal);

      bool equal = false;
      MOZ_ALWAYS_SUCCEEDS(responsePrincipal->Equals(principal, &equal));
      MOZ_DIAGNOSTIC_ASSERT(equal);

      nsCOMPtr<nsIContentSecurityPolicy> csp;
      if (parentDoc) {
        csp = parentDoc->GetCsp();
      }
      MOZ_DIAGNOSTIC_ASSERT(!csp);
#endif

      mWorkerPrivate->InitChannelInfo(aChannelInfo);

      nsILoadGroup* loadGroup = mWorkerPrivate->GetLoadGroup();
      MOZ_DIAGNOSTIC_ASSERT(loadGroup);

      // Override the principal on the WorkerPrivate.  This is only necessary
      // in order to get a principal with exactly the correct URL.  The fetch
      // referrer logic depends on the WorkerPrivate principal having a URL
      // that matches the worker script URL.  If bug 1340694 is ever fixed
      // this can be removed.
      // XXX: force the storagePrincipal to be equal to the response one. This
      // is OK for now because we don't want to expose storagePrincipal
      // functionality in ServiceWorkers yet.
      rv = mWorkerPrivate->SetPrincipalsAndCSPOnMainThread(
          responsePrincipal, responsePrincipal, loadGroup, nullptr);
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

      rv = mWorkerPrivate->SetCSPFromHeaderValues(aCSPHeaderValue,
                                                  aCSPReportOnlyHeaderValue);
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

      mWorkerPrivate->UpdateReferrerInfoFromHeader(aReferrerPolicyHeaderValue);
    }

    if (NS_SUCCEEDED(rv)) {
      DataReceived();
    }

    LoadingFinished(aIndex, rv);
  }

  void DataReceived() {
    if (IsMainWorkerScript()) {
      WorkerPrivate* parent = mWorkerPrivate->GetParent();

      if (parent) {
        // XHR Params Allowed
        mWorkerPrivate->SetXHRParamsAllowed(parent->XHRParamsAllowed());

        // Set Eval and ContentSecurityPolicy
        mWorkerPrivate->SetCSP(parent->GetCSP());
        mWorkerPrivate->SetEvalAllowed(parent->IsEvalAllowed());
      }
    }
  }

  void ExecuteFinishedScripts() {
    AssertIsOnMainThread();

    if (IsMainWorkerScript()) {
      mWorkerPrivate->WorkerScriptLoaded();
    }

    uint32_t firstIndex = UINT32_MAX;
    uint32_t lastIndex = UINT32_MAX;

    // Find firstIndex based on whether mExecutionScheduled is unset.
    for (uint32_t index = 0; index < mLoadInfos.Length(); index++) {
      if (!mLoadInfos[index].mExecutionScheduled) {
        firstIndex = index;
        break;
      }
    }

    // Find lastIndex based on whether mChannel is set, and update
    // mExecutionScheduled on the ones we're about to schedule.
    if (firstIndex != UINT32_MAX) {
      for (uint32_t index = firstIndex; index < mLoadInfos.Length(); index++) {
        ScriptLoadInfo& loadInfo = mLoadInfos[index];

        if (!loadInfo.Finished()) {
          break;
        }

        // We can execute this one.
        loadInfo.mExecutionScheduled = true;

        lastIndex = index;
      }
    }

    // This is the last index, we can unused things before the exection of the
    // script and the stopping of the sync loop.
    if (lastIndex == mLoadInfos.Length() - 1) {
      mCacheCreator = nullptr;
    }

    if (firstIndex != UINT32_MAX && lastIndex != UINT32_MAX) {
      RefPtr<ScriptExecutorRunnable> runnable = new ScriptExecutorRunnable(
          *this, mSyncLoopTarget, IsMainWorkerScript(), firstIndex, lastIndex);
      if (!runnable->Dispatch()) {
        MOZ_ASSERT(false, "This should never fail!");
      }
    }
  }
};

NS_IMPL_ISUPPORTS(ScriptLoaderRunnable, nsIRunnable, nsINamed)

NS_IMETHODIMP
LoaderListener::OnStreamComplete(nsIStreamLoader* aLoader,
                                 nsISupports* aContext, nsresult aStatus,
                                 uint32_t aStringLen, const uint8_t* aString) {
  return mRunnable->OnStreamComplete(aLoader, mIndex, aStatus, aStringLen,
                                     aString);
}

NS_IMETHODIMP
LoaderListener::OnStartRequest(nsIRequest* aRequest) {
  return mRunnable->OnStartRequest(aRequest, mIndex);
}

void CachePromiseHandler::ResolvedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue) {
  AssertIsOnMainThread();
  // May already have been canceled by CacheScriptLoader::Fail from
  // CancelMainThread.
  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::WritingToCache ||
             mLoadInfo.mCacheStatus == ScriptLoadInfo::Cancel);
  MOZ_ASSERT_IF(mLoadInfo.mCacheStatus == ScriptLoadInfo::Cancel,
                !mLoadInfo.mCachePromise);

  if (mLoadInfo.mCachePromise) {
    mLoadInfo.mCacheStatus = ScriptLoadInfo::Cached;
    mLoadInfo.mCachePromise = nullptr;
    mRunnable->MaybeExecuteFinishedScripts(mIndex);
  }
}

void CachePromiseHandler::RejectedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue) {
  AssertIsOnMainThread();
  // May already have been canceled by CacheScriptLoader::Fail from
  // CancelMainThread.
  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::WritingToCache ||
             mLoadInfo.mCacheStatus == ScriptLoadInfo::Cancel);
  mLoadInfo.mCacheStatus = ScriptLoadInfo::Cancel;

  mLoadInfo.mCachePromise = nullptr;

  // This will delete the cache object and will call LoadingFinished() with an
  // error for each ongoing operation.
  mRunnable->DeleteCache();
}

nsresult CacheCreator::CreateCacheStorage(nsIPrincipal* aPrincipal) {
  AssertIsOnMainThread();
  MOZ_ASSERT(!mCacheStorage);
  MOZ_ASSERT(aPrincipal);

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc, "This should never be null!");

  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> sandbox(cx);
  nsresult rv = xpc->CreateSandbox(cx, aPrincipal, sandbox.address());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The JSContext is not in a realm, so CreateSandbox returned an unwrapped
  // global.
  MOZ_ASSERT(JS_IsGlobalObject(sandbox));

  mSandboxGlobalObject = xpc::NativeGlobal(sandbox);
  if (NS_WARN_IF(!mSandboxGlobalObject)) {
    return NS_ERROR_FAILURE;
  }

  // If we're in private browsing mode, don't even try to create the
  // CacheStorage.  Instead, just fail immediately to terminate the
  // ServiceWorker load.
  if (NS_WARN_IF(mOriginAttributes.mPrivateBrowsingId > 0)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Create a CacheStorage bypassing its trusted origin checks.  The
  // ServiceWorker has already performed its own checks before getting
  // to this point.
  ErrorResult error;
  mCacheStorage = CacheStorage::CreateOnMainThread(
      mozilla::dom::cache::CHROME_ONLY_NAMESPACE, mSandboxGlobalObject,
      aPrincipal, true /* force trusted origin */, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  return NS_OK;
}

nsresult CacheCreator::Load(nsIPrincipal* aPrincipal) {
  AssertIsOnMainThread();
  MOZ_ASSERT(!mLoaders.IsEmpty());

  nsresult rv = CreateCacheStorage(aPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  ErrorResult error;
  MOZ_ASSERT(!mCacheName.IsEmpty());
  RefPtr<Promise> promise = mCacheStorage->Open(mCacheName, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  promise->AppendNativeHandler(this);
  return NS_OK;
}

void CacheCreator::FailLoaders(nsresult aRv) {
  AssertIsOnMainThread();

  // Fail() can call LoadingFinished() which may call ExecuteFinishedScripts()
  // which sets mCacheCreator to null, so hold a ref.
  RefPtr<CacheCreator> kungfuDeathGrip = this;

  for (uint32_t i = 0, len = mLoaders.Length(); i < len; ++i) {
    mLoaders[i]->Fail(aRv);
  }

  mLoaders.Clear();
}

void CacheCreator::RejectedCallback(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue) {
  AssertIsOnMainThread();
  FailLoaders(NS_ERROR_FAILURE);
}

void CacheCreator::ResolvedCallback(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue) {
  AssertIsOnMainThread();

  if (!aValue.isObject()) {
    FailLoaders(NS_ERROR_FAILURE);
    return;
  }

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  Cache* cache = nullptr;
  nsresult rv = UNWRAP_OBJECT(Cache, &obj, cache);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailLoaders(NS_ERROR_FAILURE);
    return;
  }

  mCache = cache;
  MOZ_DIAGNOSTIC_ASSERT(mCache);

  // If the worker is canceled, CancelMainThread() will have cleared the
  // loaders via DeleteCache().
  for (uint32_t i = 0, len = mLoaders.Length(); i < len; ++i) {
    MOZ_DIAGNOSTIC_ASSERT(mLoaders[i]);
    mLoaders[i]->Load(cache);
  }
}

void CacheCreator::DeleteCache() {
  AssertIsOnMainThread();

  // This is called when the load is canceled which can occur before
  // mCacheStorage is initialized.
  if (mCacheStorage) {
    // It's safe to do this while Cache::Match() and Cache::Put() calls are
    // running.
    RefPtr<Promise> promise = mCacheStorage->Delete(mCacheName, IgnoreErrors());

    // We don't care to know the result of the promise object.
  }

  // Always call this here to ensure the loaders array is cleared.
  FailLoaders(NS_ERROR_FAILURE);
}

void CacheScriptLoader::Fail(nsresult aRv) {
  AssertIsOnMainThread();
  MOZ_ASSERT(NS_FAILED(aRv));

  if (mFailed) {
    return;
  }

  mFailed = true;

  if (mPump) {
    MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::ReadingFromCache);
    mPump->Cancel(aRv);
    mPump = nullptr;
  }

  mLoadInfo.mCacheStatus = ScriptLoadInfo::Cancel;

  // Stop if the load was aborted on the main thread.
  // Can't use Finished() because mCachePromise may still be true.
  if (mLoadInfo.mLoadingFinished) {
    MOZ_ASSERT(!mLoadInfo.mChannel);
    MOZ_ASSERT_IF(mLoadInfo.mCachePromise,
                  mLoadInfo.mCacheStatus == ScriptLoadInfo::WritingToCache ||
                      mLoadInfo.mCacheStatus == ScriptLoadInfo::Cancel);
    return;
  }

  mRunnable->LoadingFinished(mIndex, aRv);
}

void CacheScriptLoader::Load(Cache* aCache) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aCache);

  nsCOMPtr<nsIURI> uri;
  nsresult rv =
      NS_NewURI(getter_AddRefs(uri), mLoadInfo.mURL, nullptr, mBaseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  MOZ_ASSERT(mLoadInfo.mFullURL.IsEmpty());
  CopyUTF8toUTF16(spec, mLoadInfo.mFullURL);

  mozilla::dom::RequestOrUSVString request;
  request.SetAsUSVString().ShareOrDependUpon(mLoadInfo.mFullURL);

  mozilla::dom::CacheQueryOptions params;

  // This JSContext will not end up executing JS code because here there are
  // no ReadableStreams involved.
  AutoJSAPI jsapi;
  jsapi.Init();

  ErrorResult error;
  RefPtr<Promise> promise = aCache->Match(jsapi.cx(), request, params, error);
  if (NS_WARN_IF(error.Failed())) {
    Fail(error.StealNSResult());
    return;
  }

  promise->AppendNativeHandler(this);
}

void CacheScriptLoader::RejectedCallback(JSContext* aCx,
                                         JS::Handle<JS::Value> aValue) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::Uncached);
  Fail(NS_ERROR_FAILURE);
}

void CacheScriptLoader::ResolvedCallback(JSContext* aCx,
                                         JS::Handle<JS::Value> aValue) {
  AssertIsOnMainThread();
  // If we have already called 'Fail', we should not proceed.
  if (mFailed) {
    return;
  }

  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::Uncached);

  nsresult rv;

  // The ServiceWorkerScriptCache will store data for any scripts it
  // it knows about.  This is always at least the top level script.
  // Depending on if a previous version of the service worker has
  // been installed or not it may also know about importScripts().  We
  // must handle loading and offlining new importScripts() here, however.
  if (aValue.isUndefined()) {
    // If this is the main script or we're not loading a new service worker
    // then this is an error.  This can happen for internal reasons, like
    // storage was probably wiped without removing the service worker
    // registration.  It can also happen for exposed reasons like the
    // service worker script calling importScripts() after install.
    if (NS_WARN_IF(mIsWorkerScript ||
                   (mState != ServiceWorkerState::Parsed &&
                    mState != ServiceWorkerState::Installing))) {
      Fail(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }

    mLoadInfo.mCacheStatus = ScriptLoadInfo::ToBeCached;
    rv = mRunnable->LoadScript(mIndex);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      Fail(rv);
    }
    return;
  }

  MOZ_ASSERT(aValue.isObject());

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  mozilla::dom::Response* response = nullptr;
  rv = UNWRAP_OBJECT(Response, &obj, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  InternalHeaders* headers = response->GetInternalHeaders();

  headers->Get(NS_LITERAL_CSTRING("content-security-policy"), mCSPHeaderValue,
               IgnoreErrors());
  headers->Get(NS_LITERAL_CSTRING("content-security-policy-report-only"),
               mCSPReportOnlyHeaderValue, IgnoreErrors());
  headers->Get(NS_LITERAL_CSTRING("referrer-policy"),
               mReferrerPolicyHeaderValue, IgnoreErrors());

  nsCOMPtr<nsIInputStream> inputStream;
  response->GetBody(getter_AddRefs(inputStream));
  mChannelInfo = response->GetChannelInfo();
  const UniquePtr<PrincipalInfo>& pInfo = response->GetPrincipalInfo();
  if (pInfo) {
    mPrincipalInfo = mozilla::MakeUnique<PrincipalInfo>(*pInfo);
  }

  if (!inputStream) {
    mLoadInfo.mCacheStatus = ScriptLoadInfo::Cached;
    mRunnable->DataReceivedFromCache(
        mIndex, (uint8_t*)"", 0, mChannelInfo, std::move(mPrincipalInfo),
        mCSPHeaderValue, mCSPReportOnlyHeaderValue, mReferrerPolicyHeaderValue);
    return;
  }

  MOZ_ASSERT(!mPump);
  rv = NS_NewInputStreamPump(getter_AddRefs(mPump), inputStream.forget(),
                             0,     /* default segsize */
                             0,     /* default segcount */
                             false, /* default closeWhenDone */
                             mMainThreadEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  rv = mPump->AsyncRead(loader, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPump = nullptr;
    Fail(rv);
    return;
  }

  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(mPump);
  if (rr) {
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    rv = rr->RetargetDeliveryTo(sts);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch the nsIInputStreamPump to a IO thread.");
    }
  }

  mLoadInfo.mCacheStatus = ScriptLoadInfo::ReadingFromCache;
}

NS_IMETHODIMP
CacheScriptLoader::OnStreamComplete(nsIStreamLoader* aLoader,
                                    nsISupports* aContext, nsresult aStatus,
                                    uint32_t aStringLen,
                                    const uint8_t* aString) {
  AssertIsOnMainThread();

  mPump = nullptr;

  if (NS_FAILED(aStatus)) {
    MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::ReadingFromCache ||
               mLoadInfo.mCacheStatus == ScriptLoadInfo::Cancel);
    Fail(aStatus);
    return NS_OK;
  }

  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::ReadingFromCache);
  mLoadInfo.mCacheStatus = ScriptLoadInfo::Cached;

  MOZ_ASSERT(mPrincipalInfo);
  mRunnable->DataReceivedFromCache(
      mIndex, aString, aStringLen, mChannelInfo, std::move(mPrincipalInfo),
      mCSPHeaderValue, mCSPReportOnlyHeaderValue, mReferrerPolicyHeaderValue);
  return NS_OK;
}

class ChannelGetterRunnable final : public WorkerMainThreadRunnable {
  const nsAString& mScriptURL;
  const ClientInfo mClientInfo;
  WorkerLoadInfo& mLoadInfo;
  nsresult mResult;

 public:
  ChannelGetterRunnable(WorkerPrivate* aParentWorker,
                        const nsAString& aScriptURL, WorkerLoadInfo& aLoadInfo)
      : WorkerMainThreadRunnable(
            aParentWorker, NS_LITERAL_CSTRING("ScriptLoader :: ChannelGetter")),
        mScriptURL(aScriptURL)
        // ClientInfo should always be present since this should not be called
        // if parent's status is greater than Running.
        ,
        mClientInfo(aParentWorker->GetClientInfo().ref()),
        mLoadInfo(aLoadInfo),
        mResult(NS_ERROR_FAILURE) {
    MOZ_ASSERT(aParentWorker);
    aParentWorker->AssertIsOnWorkerThread();
  }

  virtual bool MainThreadRun() override {
    AssertIsOnMainThread();

    // Initialize the WorkerLoadInfo principal to our triggering principal
    // before doing anything else.  Normally we do this in the WorkerPrivate
    // Constructor, but we can't do so off the main thread when creating
    // a nested worker.  So do it here instead.
    mLoadInfo.mLoadingPrincipal = mWorkerPrivate->GetPrincipal();
    MOZ_DIAGNOSTIC_ASSERT(mLoadInfo.mLoadingPrincipal);

    mLoadInfo.mPrincipal = mLoadInfo.mLoadingPrincipal;

    // Figure out our base URI.
    nsCOMPtr<nsIURI> baseURI = mWorkerPrivate->GetBaseURI();
    MOZ_ASSERT(baseURI);

    // May be null.
    nsCOMPtr<Document> parentDoc = mWorkerPrivate->GetDocument();

    mLoadInfo.mLoadGroup = mWorkerPrivate->GetLoadGroup();
    mLoadInfo.mCookieJarSettings = mWorkerPrivate->CookieJarSettings();

    // Nested workers use default uri encoding.
    nsCOMPtr<nsIURI> url;
    mResult =
        ConstructURI(mScriptURL, baseURI, parentDoc, true, getter_AddRefs(url));
    NS_ENSURE_SUCCESS(mResult, true);

    Maybe<ClientInfo> clientInfo;
    clientInfo.emplace(mClientInfo);

    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIReferrerInfo> referrerInfo =
        ReferrerInfo::CreateForFetch(mLoadInfo.mLoadingPrincipal, nullptr);
    mLoadInfo.mReferrerInfo =
        static_cast<ReferrerInfo*>(referrerInfo.get())
            ->CloneWithNewPolicy(mWorkerPrivate->GetReferrerPolicy());

    mResult = workerinternals::ChannelFromScriptURLMainThread(
        mLoadInfo.mLoadingPrincipal, parentDoc, mLoadInfo.mLoadGroup, url,
        clientInfo,
        // Nested workers are always dedicated.
        nsIContentPolicy::TYPE_INTERNAL_WORKER, mLoadInfo.mCookieJarSettings,
        mLoadInfo.mReferrerInfo, getter_AddRefs(channel));
    NS_ENSURE_SUCCESS(mResult, true);

    mResult = mLoadInfo.SetPrincipalsAndCSPFromChannel(channel);
    NS_ENSURE_SUCCESS(mResult, true);

    mLoadInfo.mChannel = std::move(channel);
    return true;
  }

  nsresult GetResult() const { return mResult; }

 private:
  virtual ~ChannelGetterRunnable() = default;
};

ScriptExecutorRunnable::ScriptExecutorRunnable(
    ScriptLoaderRunnable& aScriptLoader, nsIEventTarget* aSyncLoopTarget,
    bool aIsWorkerScript, uint32_t aFirstIndex, uint32_t aLastIndex)
    : MainThreadWorkerSyncRunnable(aScriptLoader.mWorkerPrivate,
                                   aSyncLoopTarget),
      mScriptLoader(aScriptLoader),
      mIsWorkerScript(aIsWorkerScript),
      mFirstIndex(aFirstIndex),
      mLastIndex(aLastIndex) {
  MOZ_ASSERT(aFirstIndex <= aLastIndex);
  MOZ_ASSERT(aLastIndex < aScriptLoader.mLoadInfos.Length());
}

bool ScriptExecutorRunnable::IsDebuggerRunnable() const {
  // ScriptExecutorRunnable is used to execute both worker and debugger scripts.
  // In the latter case, the runnable needs to be dispatched to the debugger
  // queue.
  return mScriptLoader.mWorkerScriptType == DebuggerScript;
}

bool ScriptExecutorRunnable::PreRun(WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();

  if (!mIsWorkerScript) {
    return true;
  }

  if (!aWorkerPrivate->GetJSContext()) {
    return false;
  }

  MOZ_ASSERT(mFirstIndex == 0);
  MOZ_ASSERT(!mScriptLoader.mRv.Failed());

  // Move the CSP from the workerLoadInfo in the corresponding Client
  // where the CSP code expects it!
  aWorkerPrivate->StoreCSPOnClient();

  AutoJSAPI jsapi;
  jsapi.Init();

  WorkerGlobalScope* globalScope =
      aWorkerPrivate->GetOrCreateGlobalScope(jsapi.cx());
  if (NS_WARN_IF(!globalScope)) {
    NS_WARNING("Failed to make global!");
    // There's no way to report the exception on jsapi right now, because there
    // is no way to even enter a compartment on this thread anymore.  Just clear
    // the exception.  We'll report some sort of error to our caller in
    // ShutdownScriptLoader, but it will get squelched for the same reason we're
    // squelching here: all the error reporting machinery relies on being able
    // to enter a compartment to report the error.
    jsapi.ClearException();
    return false;
  }

  return true;
}

template <typename Unit>
static bool EvaluateScriptData(JSContext* aCx,
                               const JS::CompileOptions& aOptions,
                               Unit*& aScriptData, size_t aScriptLength) {
  static_assert(std::is_same<Unit, char16_t>::value ||
                    std::is_same<Unit, Utf8Unit>::value,
                "inferred units must be UTF-8 or UTF-16");

  // Transfer script data to a local variable.
  Unit* script = nullptr;
  std::swap(script, aScriptData);

  // Transfer the local to appropriate |SourceText|.
  JS::SourceText<Unit> srcBuf;
  if (!srcBuf.init(aCx, script, aScriptLength,
                   JS::SourceOwnership::TakeOwnership)) {
    return false;
  }

  JS::Rooted<JS::Value> unused(aCx);
  return Evaluate(aCx, aOptions, srcBuf, &unused);
}

bool ScriptExecutorRunnable::WorkerRun(JSContext* aCx,
                                       WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();

  nsTArray<ScriptLoadInfo>& loadInfos = mScriptLoader.mLoadInfos;

  // Don't run if something else has already failed.
  for (uint32_t index = 0; index < mFirstIndex; index++) {
    ScriptLoadInfo& loadInfo = loadInfos.ElementAt(index);

    NS_ASSERTION(!loadInfo.mChannel, "Should no longer have a channel!");
    NS_ASSERTION(loadInfo.mExecutionScheduled, "Should be scheduled!");

    if (!loadInfo.mExecutionResult) {
      return true;
    }
  }

  // If nothing else has failed, our ErrorResult better not be a failure either.
  MOZ_ASSERT(!mScriptLoader.mRv.Failed(), "Who failed it and why?");

  // Slightly icky action at a distance, but there's no better place to stash
  // this value, really.
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  MOZ_ASSERT(global);

  for (uint32_t index = mFirstIndex; index <= mLastIndex; index++) {
    ScriptLoadInfo& loadInfo = loadInfos.ElementAt(index);

    NS_ASSERTION(!loadInfo.mChannel, "Should no longer have a channel!");
    NS_ASSERTION(loadInfo.mExecutionScheduled, "Should be scheduled!");
    NS_ASSERTION(!loadInfo.mExecutionResult, "Should not have executed yet!");

    MOZ_ASSERT(!mScriptLoader.mRv.Failed(), "Who failed it and why?");
    mScriptLoader.mRv.MightThrowJSException();
    if (NS_FAILED(loadInfo.mLoadResult)) {
      workerinternals::ReportLoadError(mScriptLoader.mRv, loadInfo.mLoadResult,
                                       loadInfo.mURL);
      return true;
    }

    // If this is a top level script that succeeded, then mark the
    // Client execution ready and possible controlled by a service worker.
    if (mIsWorkerScript) {
      if (mScriptLoader.mController.isSome()) {
        aWorkerPrivate->Control(mScriptLoader.mController.ref());
      }
      aWorkerPrivate->ExecutionReady();
    }

    NS_ConvertUTF16toUTF8 filename(loadInfo.mURL);

    JS::CompileOptions options(aCx);
    options.setFileAndLine(filename.get(), 1).setNoScriptRval(true);

    MOZ_ASSERT(loadInfo.mMutedErrorFlag.isSome());
    options.setMutedErrors(loadInfo.mMutedErrorFlag.valueOr(true));

    // Our ErrorResult still shouldn't be a failure.
    MOZ_ASSERT(!mScriptLoader.mRv.Failed(), "Who failed it and why?");

    // Transfer script length to a local variable, encoding-agnostically.
    size_t scriptLength = 0;
    std::swap(scriptLength, loadInfo.mScriptLength);

    // This transfers script data out of the active arm of |loadInfo.mScript|.
    bool successfullyEvaluated =
        loadInfo.mScriptIsUTF8
            ? EvaluateScriptData(aCx, options, loadInfo.mScript.mUTF8,
                                 scriptLength)
            : EvaluateScriptData(aCx, options, loadInfo.mScript.mUTF16,
                                 scriptLength);
    MOZ_ASSERT(loadInfo.ScriptTextIsNull());
    if (!successfullyEvaluated) {
      mScriptLoader.mRv.StealExceptionFromJSContext(aCx);
      return true;
    }

    loadInfo.mExecutionResult = true;
  }

  return true;
}

void ScriptExecutorRunnable::PostRun(JSContext* aCx,
                                     WorkerPrivate* aWorkerPrivate,
                                     bool aRunResult) {
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(!JS_IsExceptionPending(aCx), "Who left an exception on there?");

  nsTArray<ScriptLoadInfo>& loadInfos = mScriptLoader.mLoadInfos;

  if (mLastIndex == loadInfos.Length() - 1) {
    // All done. If anything failed then return false.
    bool result = true;
    bool mutedError = false;
    for (uint32_t index = 0; index < loadInfos.Length(); index++) {
      if (!loadInfos[index].mExecutionResult) {
        mutedError = loadInfos[index].mMutedErrorFlag.valueOr(true);
        result = false;
        break;
      }
    }

    // The only way we can get here with "result" false but without
    // mScriptLoader.mRv being a failure is if we're loading the main worker
    // script and GetOrCreateGlobalScope() fails.  In that case we would have
    // returned false from WorkerRun, so assert that.
    MOZ_ASSERT_IF(!result && !mScriptLoader.mRv.Failed(), !aRunResult);
    ShutdownScriptLoader(aCx, aWorkerPrivate, result, mutedError);
  }
}

nsresult ScriptExecutorRunnable::Cancel() {
  if (mLastIndex == mScriptLoader.mLoadInfos.Length() - 1) {
    ShutdownScriptLoader(mWorkerPrivate->GetJSContext(), mWorkerPrivate, false,
                         false);
  }
  return MainThreadWorkerSyncRunnable::Cancel();
}

void ScriptExecutorRunnable::ShutdownScriptLoader(JSContext* aCx,
                                                  WorkerPrivate* aWorkerPrivate,
                                                  bool aResult,
                                                  bool aMutedError) {
  aWorkerPrivate->AssertIsOnWorkerThread();

  MOZ_ASSERT(mLastIndex == mScriptLoader.mLoadInfos.Length() - 1);

  if (mIsWorkerScript) {
    aWorkerPrivate->SetLoadingWorkerScript(false);
  }

  if (!aResult) {
    // At this point there are two possibilities:
    //
    // 1) mScriptLoader.mRv.Failed().  In that case we just want to leave it
    //    as-is, except if it has a JS exception and we need to mute JS
    //    exceptions.  In that case, we log the exception without firing any
    //    events and then replace it on the ErrorResult with a NetworkError,
    //    per spec.
    //
    // 2) mScriptLoader.mRv succeeded.  As far as I can tell, this can only
    //    happen when loading the main worker script and
    //    GetOrCreateGlobalScope() fails or if ScriptExecutorRunnable::Cancel
    //    got called.  Does it matter what we throw in this case?  I'm not
    //    sure...
    if (mScriptLoader.mRv.Failed()) {
      if (aMutedError && mScriptLoader.mRv.IsJSException()) {
        LogExceptionToConsole(aCx, aWorkerPrivate);
        mScriptLoader.mRv.Throw(NS_ERROR_DOM_NETWORK_ERR);
      }
    } else {
      mScriptLoader.mRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    }
  }

  aWorkerPrivate->StopSyncLoop(mSyncLoopTarget, aResult);
}

void ScriptExecutorRunnable::LogExceptionToConsole(
    JSContext* aCx, WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();

  MOZ_ASSERT(mScriptLoader.mRv.IsJSException());

  JS::Rooted<JS::Value> exn(aCx);
  if (!ToJSValue(aCx, std::move(mScriptLoader.mRv), &exn)) {
    return;
  }

  // Now the exception state should all be in exn.
  MOZ_ASSERT(!JS_IsExceptionPending(aCx));
  MOZ_ASSERT(!mScriptLoader.mRv.Failed());

  js::ErrorReport report(aCx);
  if (!report.init(aCx, exn, js::ErrorReport::WithSideEffects)) {
    JS_ClearPendingException(aCx);
    return;
  }

  RefPtr<xpc::ErrorReport> xpcReport = new xpc::ErrorReport();
  xpcReport->Init(report.report(), report.toStringResult().c_str(),
                  aWorkerPrivate->IsChromeWorker(), aWorkerPrivate->WindowID());

  RefPtr<AsyncErrorReporter> r = new AsyncErrorReporter(xpcReport);
  NS_DispatchToMainThread(r);
}

void LoadAllScripts(WorkerPrivate* aWorkerPrivate,
                    UniquePtr<SerializedStackHolder> aOriginStack,
                    nsTArray<ScriptLoadInfo>& aLoadInfos, bool aIsMainScript,
                    WorkerScriptType aWorkerScriptType, ErrorResult& aRv) {
  aWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(!aLoadInfos.IsEmpty(), "Bad arguments!");

  AutoSyncLoopHolder syncLoop(aWorkerPrivate, Canceling);
  nsCOMPtr<nsIEventTarget> syncLoopTarget = syncLoop.GetEventTarget();
  if (!syncLoopTarget) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  Maybe<ClientInfo> clientInfo;
  Maybe<ServiceWorkerDescriptor> controller;
  if (!aIsMainScript) {
    clientInfo = aWorkerPrivate->GetClientInfo();
    controller = aWorkerPrivate->GetController();
  }

  RefPtr<ScriptLoaderRunnable> loader = new ScriptLoaderRunnable(
      aWorkerPrivate, std::move(aOriginStack), syncLoopTarget, aLoadInfos,
      clientInfo, controller, aIsMainScript, aWorkerScriptType, aRv);

  NS_ASSERTION(aLoadInfos.IsEmpty(), "Should have swapped!");

  RefPtr<StrongWorkerRef> workerRef =
      StrongWorkerRef::Create(aWorkerPrivate, "ScriptLoader", [loader]() {
        NS_DispatchToMainThread(NewRunnableMethod(
            "ScriptLoader::CancelMainThreadWithBindingAborted", loader,
            &ScriptLoaderRunnable::CancelMainThreadWithBindingAborted));
      });

  if (NS_WARN_IF(!workerRef)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (NS_FAILED(NS_DispatchToMainThread(loader))) {
    NS_ERROR("Failed to dispatch!");
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  syncLoop.Run();
}

} /* anonymous namespace */

namespace workerinternals {

nsresult ChannelFromScriptURLMainThread(
    nsIPrincipal* aPrincipal, Document* aParentDoc, nsILoadGroup* aLoadGroup,
    nsIURI* aScriptURL, const Maybe<ClientInfo>& aClientInfo,
    nsContentPolicyType aMainScriptContentPolicyType,
    nsICookieJarSettings* aCookieJarSettings, nsIReferrerInfo* aReferrerInfo,
    nsIChannel** aChannel) {
  AssertIsOnMainThread();

  nsCOMPtr<nsIIOService> ios(do_GetIOService());

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  NS_ASSERTION(secMan, "This should never be null!");

  return ChannelFromScriptURL(
      aPrincipal, aParentDoc, nullptr, aLoadGroup, ios, secMan, aScriptURL,
      aClientInfo, Maybe<ServiceWorkerDescriptor>(), true, WorkerScript,
      aMainScriptContentPolicyType, nsIRequest::LOAD_NORMAL, aCookieJarSettings,
      aReferrerInfo, aChannel);
}

nsresult ChannelFromScriptURLWorkerThread(JSContext* aCx,
                                          WorkerPrivate* aParent,
                                          const nsAString& aScriptURL,
                                          WorkerLoadInfo& aLoadInfo) {
  aParent->AssertIsOnWorkerThread();

  RefPtr<ChannelGetterRunnable> getter =
      new ChannelGetterRunnable(aParent, aScriptURL, aLoadInfo);

  ErrorResult rv;
  getter->Dispatch(Canceling, rv);
  if (rv.Failed()) {
    NS_ERROR("Failed to dispatch!");
    return rv.StealNSResult();
  }

  return getter->GetResult();
}

void ReportLoadError(ErrorResult& aRv, nsresult aLoadResult,
                     const nsAString& aScriptURL) {
  MOZ_ASSERT(!aRv.Failed());

  nsPrintfCString err("Failed to load worker script at \"%s\"",
                      NS_ConvertUTF16toUTF8(aScriptURL).get());

  switch (aLoadResult) {
    case NS_ERROR_FILE_NOT_FOUND:
    case NS_ERROR_NOT_AVAILABLE:
      aRv.ThrowNetworkError(err);
      break;

    case NS_ERROR_MALFORMED_URI:
    case NS_ERROR_DOM_SYNTAX_ERR:
      aRv.ThrowSyntaxError(err);
      break;

    case NS_BINDING_ABORTED:
      // Note: we used to pretend like we didn't set an exception for
      // NS_BINDING_ABORTED, but then ShutdownScriptLoader did it anyway.  The
      // other callsite, in WorkerPrivate::Constructor, never passed in
      // NS_BINDING_ABORTED.  So just throw it directly here.  Consumers will
      // deal as needed.  But note that we do NOT want to use one of the
      // Throw*Error() methods on ErrorResult for this case, because that will
      // make it impossible for consumers to realize that our error was
      // NS_BINDING_ABORTED.
      aRv.Throw(aLoadResult);
      return;

    case NS_ERROR_DOM_BAD_URI:
      // This is actually a security error.
    case NS_ERROR_DOM_SECURITY_ERR:
      aRv.ThrowSecurityError(err);
      break;

    default:
      // For lack of anything better, go ahead and throw a NetworkError here.
      // We don't want to throw a JS exception, because for toplevel script
      // loads that would get squelched.
      aRv.ThrowNetworkError(nsPrintfCString(
          "Failed to load worker script at %s (nsresult = 0x%" PRIx32 ")",
          NS_ConvertUTF16toUTF8(aScriptURL).get(),
          static_cast<uint32_t>(aLoadResult)));
      return;
  }
}

void LoadMainScript(WorkerPrivate* aWorkerPrivate,
                    UniquePtr<SerializedStackHolder> aOriginStack,
                    const nsAString& aScriptURL,
                    WorkerScriptType aWorkerScriptType, ErrorResult& aRv) {
  nsTArray<ScriptLoadInfo> loadInfos;

  ScriptLoadInfo* info = loadInfos.AppendElement();
  info->mURL = aScriptURL;
  info->mLoadFlags = aWorkerPrivate->GetLoadFlags();

  // We are loading the main script, so the worker's Client must be
  // reserved.
  info->mReservedClientInfo = aWorkerPrivate->GetClientInfo();

  LoadAllScripts(aWorkerPrivate, std::move(aOriginStack), loadInfos, true,
                 aWorkerScriptType, aRv);
}

void Load(WorkerPrivate* aWorkerPrivate,
          UniquePtr<SerializedStackHolder> aOriginStack,
          const nsTArray<nsString>& aScriptURLs,
          WorkerScriptType aWorkerScriptType, ErrorResult& aRv) {
  const uint32_t urlCount = aScriptURLs.Length();

  if (!urlCount) {
    return;
  }

  if (urlCount > MAX_CONCURRENT_SCRIPTS) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  nsTArray<ScriptLoadInfo> loadInfos;
  loadInfos.SetLength(urlCount);

  for (uint32_t index = 0; index < urlCount; index++) {
    loadInfos[index].mURL = aScriptURLs[index];
    loadInfos[index].mLoadFlags = aWorkerPrivate->GetLoadFlags();
  }

  LoadAllScripts(aWorkerPrivate, std::move(aOriginStack), loadInfos, false,
                 aWorkerScriptType, aRv);
}

}  // namespace workerinternals

}  // namespace dom
}  // namespace mozilla
