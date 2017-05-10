/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoader.h"

#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocShell.h"
#include "nsIDOMDocument.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIInputStreamPump.h"
#include "nsIIOService.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamLoader.h"
#include "nsIStreamListenerTee.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIURI.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsError.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsDocShellCID.h"
#include "nsISupportsPrimitives.h"
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
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/dom/cache/Cache.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/ChannelInfo.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/nsCSPService.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SRILogHelper.h"
#include "mozilla/UniquePtr.h"
#include "Principal.h"
#include "WorkerHolder.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

#define MAX_CONCURRENT_SCRIPTS 1000

USING_WORKERS_NAMESPACE

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::dom::cache::Cache;
using mozilla::dom::cache::CacheStorage;
using mozilla::ipc::PrincipalInfo;

namespace {

nsIURI*
GetBaseURI(bool aIsMainScript, WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(aWorkerPrivate);
  nsIURI* baseURI;
  WorkerPrivate* parentWorker = aWorkerPrivate->GetParent();
  if (aIsMainScript) {
    if (parentWorker) {
      baseURI = parentWorker->GetBaseURI();
      NS_ASSERTION(baseURI, "Should have been set already!");
    }
    else {
      // May be null.
      baseURI = aWorkerPrivate->GetBaseURI();
    }
  }
  else {
    baseURI = aWorkerPrivate->GetBaseURI();
    NS_ASSERTION(baseURI, "Should have been set already!");
  }

  return baseURI;
}

nsresult
ChannelFromScriptURL(nsIPrincipal* principal,
                     nsIURI* baseURI,
                     nsIDocument* parentDoc,
                     nsILoadGroup* loadGroup,
                     nsIIOService* ios,
                     nsIScriptSecurityManager* secMan,
                     const nsAString& aScriptURL,
                     bool aIsMainScript,
                     WorkerScriptType aWorkerScriptType,
                     nsContentPolicyType aMainScriptContentPolicyType,
                     nsLoadFlags aLoadFlags,
                     bool aDefaultURIEncoding,
                     nsIChannel** aChannel)
{
  AssertIsOnMainThread();

  nsresult rv;
  nsCOMPtr<nsIURI> uri;

  if (aDefaultURIEncoding) {
    rv = NS_NewURI(getter_AddRefs(uri), aScriptURL, nullptr, baseURI);
  } else {
    rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri),
                                                   aScriptURL, parentDoc,
                                                   baseURI);
  }

  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  // If we have the document, use it. Unfortunately, for dedicated workers
  // 'parentDoc' ends up being the parent document, which is not the document
  // that we want to use. So make sure to avoid using 'parentDoc' in that
  // situation.
  if (parentDoc && parentDoc->NodePrincipal() != principal) {
    parentDoc = nullptr;
  }

  aLoadFlags |= nsIChannel::LOAD_CLASSIFY_URI;
  uint32_t secFlags = aIsMainScript ? nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED
                                    : nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS;

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
  bool isData = false;
  if (aIsMainScript && NS_SUCCEEDED(uri->SchemeIs("data", &isData)) && isData) {
    secFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL;
  }

  nsContentPolicyType contentPolicyType =
    aIsMainScript ? aMainScriptContentPolicyType
                  : nsIContentPolicy::TYPE_INTERNAL_WORKER_IMPORT_SCRIPTS;

  nsCOMPtr<nsIChannel> channel;
  // If we have the document, use it. Unfortunately, for dedicated workers
  // 'parentDoc' ends up being the parent document, which is not the document
  // that we want to use. So make sure to avoid using 'parentDoc' in that
  // situation.
  if (parentDoc && parentDoc->NodePrincipal() == principal) {
    rv = NS_NewChannel(getter_AddRefs(channel),
                       uri,
                       parentDoc,
                       secFlags,
                       contentPolicyType,
                       loadGroup,
                       nullptr, // aCallbacks
                       aLoadFlags,
                       ios);
  } else {
    // We must have a loadGroup with a load context for the principal to
    // traverse the channel correctly.
    MOZ_ASSERT(loadGroup);
    MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(loadGroup, principal));

    rv = NS_NewChannel(getter_AddRefs(channel),
                       uri,
                       principal,
                       secFlags,
                       contentPolicyType,
                       loadGroup,
                       nullptr, // aCallbacks
                       aLoadFlags,
                       ios);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel)) {
    mozilla::net::ReferrerPolicy referrerPolicy = parentDoc ?
      parentDoc->GetReferrerPolicy() : mozilla::net::RP_Unset;
    rv = nsContentUtils::SetFetchReferrerURIWithPolicy(principal, parentDoc,
                                                       httpChannel, referrerPolicy);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  channel.forget(aChannel);
  return rv;
}

struct ScriptLoadInfo
{
  ScriptLoadInfo()
  : mScriptTextBuf(nullptr)
  , mScriptTextLength(0)
  , mLoadResult(NS_ERROR_NOT_INITIALIZED)
  , mLoadingFinished(false)
  , mExecutionScheduled(false)
  , mExecutionResult(false)
  , mCacheStatus(Uncached)
  , mLoadFlags(nsIRequest::LOAD_NORMAL)
  { }

  ~ScriptLoadInfo()
  {
    if (mScriptTextBuf) {
      js_free(mScriptTextBuf);
    }
  }

  bool
  ReadyToExecute()
  {
    return !mChannel && NS_SUCCEEDED(mLoadResult) && !mExecutionScheduled;
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
  char16_t* mScriptTextBuf;
  size_t mScriptTextLength;

  nsresult mLoadResult;
  bool mLoadingFinished;
  bool mExecutionScheduled;
  bool mExecutionResult;

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

  CacheStatus mCacheStatus;

  nsLoadFlags mLoadFlags;

  Maybe<bool> mMutedErrorFlag;

  bool Finished() const
  {
    return mLoadingFinished && !mCachePromise && !mChannel;
  }
};

class ScriptLoaderRunnable;

class ScriptExecutorRunnable final : public MainThreadWorkerSyncRunnable
{
  ScriptLoaderRunnable& mScriptLoader;
  bool mIsWorkerScript;
  uint32_t mFirstIndex;
  uint32_t mLastIndex;

public:
  ScriptExecutorRunnable(ScriptLoaderRunnable& aScriptLoader,
                         nsIEventTarget* aSyncLoopTarget,
                         bool aIsWorkerScript,
                         uint32_t aFirstIndex,
                         uint32_t aLastIndex);

private:
  ~ScriptExecutorRunnable()
  { }

  virtual bool
  IsDebuggerRunnable() const override;

  virtual bool
  PreRun(WorkerPrivate* aWorkerPrivate) override;

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;

  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
          override;

  nsresult
  Cancel() override;

  void
  ShutdownScriptLoader(JSContext* aCx,
                       WorkerPrivate* aWorkerPrivate,
                       bool aResult,
                       bool aMutedError);

  void LogExceptionToConsole(JSContext* aCx,
                             WorkerPrivate* WorkerPrivate);
};

class CacheScriptLoader;

class CacheCreator final : public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS

  explicit CacheCreator(WorkerPrivate* aWorkerPrivate)
    : mCacheName(aWorkerPrivate->ServiceWorkerCacheName())
    , mOriginAttributes(aWorkerPrivate->GetOriginAttributes())
  {
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
    MOZ_ASSERT(aWorkerPrivate->LoadScriptAsPartOfLoadingServiceWorkerScript());
    AssertIsOnMainThread();
  }

  void
  AddLoader(CacheScriptLoader* aLoader)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(!mCacheStorage);
    mLoaders.AppendElement(aLoader);
  }

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  // Try to load from cache with aPrincipal used for cache access.
  nsresult
  Load(nsIPrincipal* aPrincipal);

  Cache*
  Cache_() const
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mCache);
    return mCache;
  }

  nsIGlobalObject*
  Global() const
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mSandboxGlobalObject);
    return mSandboxGlobalObject;
  }

  void
  DeleteCache();

private:
  ~CacheCreator()
  {
  }

  nsresult
  CreateCacheStorage(nsIPrincipal* aPrincipal);

  void
  FailLoaders(nsresult aRv);

  RefPtr<Cache> mCache;
  RefPtr<CacheStorage> mCacheStorage;
  nsCOMPtr<nsIGlobalObject> mSandboxGlobalObject;
  nsTArray<RefPtr<CacheScriptLoader>> mLoaders;

  nsString mCacheName;
  OriginAttributes mOriginAttributes;
};

NS_IMPL_ISUPPORTS0(CacheCreator)

class CacheScriptLoader final : public PromiseNativeHandler
                              , public nsIStreamLoaderObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER

  CacheScriptLoader(WorkerPrivate* aWorkerPrivate, ScriptLoadInfo& aLoadInfo,
                    uint32_t aIndex, bool aIsWorkerScript,
                    ScriptLoaderRunnable* aRunnable)
    : mLoadInfo(aLoadInfo)
    , mIndex(aIndex)
    , mRunnable(aRunnable)
    , mIsWorkerScript(aIsWorkerScript)
    , mFailed(false)
  {
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
    mBaseURI = GetBaseURI(mIsWorkerScript, aWorkerPrivate);
    AssertIsOnMainThread();
  }

  void
  Fail(nsresult aRv);

  void
  Load(Cache* aCache);

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

private:
  ~CacheScriptLoader()
  {
    AssertIsOnMainThread();
  }

  ScriptLoadInfo& mLoadInfo;
  uint32_t mIndex;
  RefPtr<ScriptLoaderRunnable> mRunnable;
  bool mIsWorkerScript;
  bool mFailed;
  nsCOMPtr<nsIInputStreamPump> mPump;
  nsCOMPtr<nsIURI> mBaseURI;
  mozilla::dom::ChannelInfo mChannelInfo;
  UniquePtr<PrincipalInfo> mPrincipalInfo;
  nsCString mCSPHeaderValue;
  nsCString mCSPReportOnlyHeaderValue;
  nsCString mReferrerPolicyHeaderValue;
};

NS_IMPL_ISUPPORTS(CacheScriptLoader, nsIStreamLoaderObserver)

class CachePromiseHandler final : public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS

  CachePromiseHandler(ScriptLoaderRunnable* aRunnable,
                      ScriptLoadInfo& aLoadInfo,
                      uint32_t aIndex)
    : mRunnable(aRunnable)
    , mLoadInfo(aLoadInfo)
    , mIndex(aIndex)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mRunnable);
  }

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

private:
  ~CachePromiseHandler()
  {
    AssertIsOnMainThread();
  }

  RefPtr<ScriptLoaderRunnable> mRunnable;
  ScriptLoadInfo& mLoadInfo;
  uint32_t mIndex;
};

NS_IMPL_ISUPPORTS0(CachePromiseHandler)

class LoaderListener final : public nsIStreamLoaderObserver
                           , public nsIRequestObserver
{
public:
  NS_DECL_ISUPPORTS

  LoaderListener(ScriptLoaderRunnable* aRunnable, uint32_t aIndex)
    : mRunnable(aRunnable)
    , mIndex(aIndex)
  {
    MOZ_ASSERT(mRunnable);
  }

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                   nsresult aStatus, uint32_t aStringLen,
                   const uint8_t* aString) override;

  NS_IMETHOD
  OnStartRequest(nsIRequest* aRequest, nsISupports* aContext) override;

  NS_IMETHOD
  OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                nsresult aStatusCode) override
  {
    // Nothing to do here!
    return NS_OK;
  }

private:
  ~LoaderListener() {}

  RefPtr<ScriptLoaderRunnable> mRunnable;
  uint32_t mIndex;
};

NS_IMPL_ISUPPORTS(LoaderListener, nsIStreamLoaderObserver, nsIRequestObserver)

class ScriptLoaderHolder;

class ScriptLoaderRunnable final : public nsIRunnable,
                                   public nsINamed
{
  friend class ScriptExecutorRunnable;
  friend class ScriptLoaderHolder;
  friend class CachePromiseHandler;
  friend class CacheScriptLoader;
  friend class LoaderListener;

  WorkerPrivate* mWorkerPrivate;
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;
  nsTArray<ScriptLoadInfo> mLoadInfos;
  RefPtr<CacheCreator> mCacheCreator;
  bool mIsMainScript;
  WorkerScriptType mWorkerScriptType;
  bool mCanceled;
  bool mCanceledMainThread;
  ErrorResult& mRv;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ScriptLoaderRunnable(WorkerPrivate* aWorkerPrivate,
                       nsIEventTarget* aSyncLoopTarget,
                       nsTArray<ScriptLoadInfo>& aLoadInfos,
                       bool aIsMainScript,
                       WorkerScriptType aWorkerScriptType,
                       ErrorResult& aRv)
  : mWorkerPrivate(aWorkerPrivate), mSyncLoopTarget(aSyncLoopTarget),
    mIsMainScript(aIsMainScript), mWorkerScriptType(aWorkerScriptType),
    mCanceled(false), mCanceledMainThread(false), mRv(aRv)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aSyncLoopTarget);
    MOZ_ASSERT_IF(aIsMainScript, aLoadInfos.Length() == 1);

    mLoadInfos.SwapElements(aLoadInfos);
  }

private:
  ~ScriptLoaderRunnable()
  { }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

    nsresult rv = RunInternal();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      CancelMainThread(rv);
    }

    return NS_OK;
  }

  NS_IMETHOD
  GetName(nsACString& aName) override
  {
    aName.AssignASCII("ScriptLoaderRunnable");
    return NS_OK;
  }

  NS_IMETHOD
  SetName(const char* aName) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  void
  LoadingFinished(uint32_t aIndex, nsresult aRv)
  {
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

  void
  MaybeExecuteFinishedScripts(uint32_t aIndex)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aIndex < mLoadInfos.Length());
    ScriptLoadInfo& loadInfo = mLoadInfos[aIndex];

    // We execute the last step if we don't have a pending operation with the
    // cache and the loading is completed.
    if (loadInfo.Finished()) {
      ExecuteFinishedScripts();
    }
  }

  nsresult
  OnStreamComplete(nsIStreamLoader* aLoader, uint32_t aIndex,
                   nsresult aStatus, uint32_t aStringLen,
                   const uint8_t* aString)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aIndex < mLoadInfos.Length());

    nsresult rv = OnStreamCompleteInternal(aLoader, aStatus, aStringLen,
                                           aString, mLoadInfos[aIndex]);
    LoadingFinished(aIndex, rv);
    return NS_OK;
  }

  nsresult
  OnStartRequest(nsIRequest* aRequest, uint32_t aIndex)
  {
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
    MOZ_ASSERT(channel == loadInfo.mChannel);

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
    nsresult rv = ssm->GetChannelResultPrincipal(channel, getter_AddRefs(channelPrincipal));
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

    ir->SetPrincipalInfo(Move(principalInfo));
    ir->Headers()->FillResponseHeaders(loadInfo.mChannel);

    RefPtr<mozilla::dom::Response> response =
      new mozilla::dom::Response(mCacheCreator->Global(), ir);

    mozilla::dom::RequestOrUSVString request;

    MOZ_ASSERT(!loadInfo.mFullURL.IsEmpty());
    request.SetAsUSVString().Rebind(loadInfo.mFullURL.Data(),
                                    loadInfo.mFullURL.Length());

    ErrorResult error;
    RefPtr<Promise> cachePromise =
      mCacheCreator->Cache_()->Put(request, *response, error);
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

  bool
  Notify(Status aStatus)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    if (aStatus >= Terminating && !mCanceled) {
      mCanceled = true;

      MOZ_ALWAYS_SUCCEEDS(
        NS_DispatchToMainThread(NewRunnableMethod(this,
                                                  &ScriptLoaderRunnable::CancelMainThreadWithBindingAborted)));
    }

    return true;
  }

  bool
  IsMainWorkerScript() const
  {
    return mIsMainScript && mWorkerScriptType == WorkerScript;
  }

  void
  CancelMainThreadWithBindingAborted()
  {
    CancelMainThread(NS_BINDING_ABORTED);
  }

  void
  CancelMainThread(nsresult aCancelResult)
  {
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

  void
  DeleteCache()
  {
    AssertIsOnMainThread();

    if (!mCacheCreator) {
      return;
    }

    mCacheCreator->DeleteCache();
    mCacheCreator = nullptr;
  }

  nsresult
  RunInternal()
  {
    AssertIsOnMainThread();

    if (IsMainWorkerScript() && mWorkerPrivate->IsServiceWorker()) {
      mWorkerPrivate->SetLoadingWorkerScript(true);
    }

    if (!mWorkerPrivate->IsServiceWorker() ||
        !mWorkerPrivate->LoadScriptAsPartOfLoadingServiceWorkerScript()) {
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
      RefPtr<CacheScriptLoader> loader =
        new CacheScriptLoader(mWorkerPrivate, mLoadInfos[index], index,
                              IsMainWorkerScript(), this);
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

  nsresult
  LoadScript(uint32_t aIndex)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aIndex < mLoadInfos.Length());

    WorkerPrivate* parentWorker = mWorkerPrivate->GetParent();

    nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
    nsCOMPtr<nsILoadGroup> loadGroup = mWorkerPrivate->GetLoadGroup();
    MOZ_DIAGNOSTIC_ASSERT(principal);
    MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(loadGroup, principal));

    // Figure out our base URI.
    nsCOMPtr<nsIURI> baseURI = GetBaseURI(mIsMainScript, mWorkerPrivate);

    // May be null.
    nsCOMPtr<nsIDocument> parentDoc = mWorkerPrivate->GetDocument();

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
      rv = ChannelFromScriptURL(principal, baseURI, parentDoc, loadGroup, ios,
                                secMan, loadInfo.mURL, IsMainWorkerScript(),
                                mWorkerScriptType,
                                mWorkerPrivate->ContentPolicyType(), loadFlags,
                                useDefaultEncoding,
                                getter_AddRefs(channel));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
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

    if (loadInfo.mCacheStatus != ScriptLoadInfo::ToBeCached) {
      rv = channel->AsyncOpen2(loader);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      nsCOMPtr<nsIOutputStream> writer;

      // In case we return early.
      loadInfo.mCacheStatus = ScriptLoadInfo::Cancel;

      rv = NS_NewPipe(getter_AddRefs(loadInfo.mCacheReadStream),
                      getter_AddRefs(writer), 0,
                      UINT32_MAX, // unlimited size to avoid writer WOULD_BLOCK case
                      true, false); // non-blocking reader, blocking writer
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsCOMPtr<nsIStreamListenerTee> tee =
        do_CreateInstance(NS_STREAMLISTENERTEE_CONTRACTID);
      rv = tee->Init(loader, writer, listener);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsresult rv = channel->AsyncOpen2(tee);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    loadInfo.mChannel.swap(channel);

    return NS_OK;
  }

  nsresult
  OnStreamCompleteInternal(nsIStreamLoader* aLoader, nsresult aStatus,
                           uint32_t aStringLen, const uint8_t* aString,
                           ScriptLoadInfo& aLoadInfo)
  {
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
    rv = ssm->GetChannelResultPrincipal(channel, getter_AddRefs(channelPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
    if (!principal) {
      WorkerPrivate* parentWorker = mWorkerPrivate->GetParent();
      MOZ_ASSERT(parentWorker, "Must have a parent!");
      principal = parentWorker->GetPrincipal();
    }

    // We don't mute the main worker script becase we've already done
    // same-origin checks on them so we should be able to see their errors.
    // Note that for data: url, where we allow it through the same-origin check
    // but then give it a different origin.
    aLoadInfo.mMutedErrorFlag.emplace(IsMainWorkerScript()
                                        ? false
                                        : !principal->Subsumes(channelPrincipal));

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
        NS_LITERAL_CSTRING("content-security-policy"),
        tCspHeaderValue);

      Unused << httpChannel->GetResponseHeader(
        NS_LITERAL_CSTRING("content-security-policy-report-only"),
        tCspROHeaderValue);

      Unused << httpChannel->GetResponseHeader(
        NS_LITERAL_CSTRING("referrer-policy"),
        tRPHeaderCValue);
    }

    // May be null.
    nsIDocument* parentDoc = mWorkerPrivate->GetDocument();

    // Use the regular ScriptLoader for this grunt work! Should be just fine
    // because we're running on the main thread.
    // Unlike <script> tags, Worker scripts are always decoded as UTF-8,
    // per spec. So we explicitly pass in the charset hint.
    rv = ScriptLoader::ConvertToUTF16(aLoadInfo.mChannel, aString, aStringLen,
                                      NS_LITERAL_STRING("UTF-8"), parentDoc,
                                      aLoadInfo.mScriptTextBuf,
                                      aLoadInfo.mScriptTextLength);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (!aLoadInfo.mScriptTextLength && !aLoadInfo.mScriptTextBuf) {
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      NS_LITERAL_CSTRING("DOM"), parentDoc,
                                      nsContentUtils::eDOM_PROPERTIES,
                                      "EmptyWorkerSourceWarning");
    } else if (!aLoadInfo.mScriptTextBuf) {
      return NS_ERROR_FAILURE;
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

    nsCOMPtr<nsILoadInfo> chanLoadInfo = channel->GetLoadInfo();
    if (chanLoadInfo && chanLoadInfo->GetEnforceSRI()) {
      // importScripts() and the Worker constructor do not support integrity metadata
      //  (or any fetch options). Until then, we can just block.
      //  If we ever have those data in the future, we'll have to the check to
      //  by using the SRICheck module
      MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
            ("Scriptloader::Load, SRI required but not supported in workers"));
      nsCOMPtr<nsIContentSecurityPolicy> wcsp;
      chanLoadInfo->LoadingPrincipal()->GetCsp(getter_AddRefs(wcsp));
      MOZ_ASSERT(wcsp, "We sould have a CSP for the worker here");
      if (wcsp) {
        wcsp->LogViolationDetails(
            nsIContentSecurityPolicy::VIOLATION_TYPE_REQUIRE_SRI_FOR_SCRIPT,
            aLoadInfo.mURL, EmptyString(), 0, EmptyString(), EmptyString());
      }
      return NS_ERROR_SRI_CORRUPT;
    }

    // Update the principal of the worker and its base URI if we just loaded the
    // worker's primary script.
    if (IsMainWorkerScript()) {
      // Take care of the base URI first.
      mWorkerPrivate->SetBaseURI(finalURI);

      // Store the channel info if needed.
      mWorkerPrivate->InitChannelInfo(channel);

      // Our final channel principal should match the original principal
      // in terms of the origin.
      MOZ_DIAGNOSTIC_ASSERT(mWorkerPrivate->FinalChannelPrincipalIsValid(channel));

      // However, we must still override the principal since the nsIPrincipal
      // URL may be different due to same-origin redirects.  Unfortunately this
      // URL must exactly match the final worker script URL in order to
      // properly set the referrer header on fetch/xhr requests.  If bug 1340694
      // is ever fixed this can be removed.
      rv = mWorkerPrivate->SetPrincipalFromChannel(channel);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIContentSecurityPolicy> csp = mWorkerPrivate->GetCSP();
      // We did inherit CSP in bug 1223647. If we do not already have a CSP, we
      // should get it from the HTTP headers on the worker script.
      if (CSPService::sCSPEnabled) {
        if (!csp) {
          rv = mWorkerPrivate->SetCSPFromHeaderValues(tCspHeaderValue,
                                                      tCspROHeaderValue);
          NS_ENSURE_SUCCESS(rv, rv);
        } else {
          csp->EnsureEventTarget(mWorkerPrivate->MainThreadEventTarget());
        }
      }

      mWorkerPrivate->SetReferrerPolicyFromHeaderValue(tRPHeaderCValue);

      WorkerPrivate* parent = mWorkerPrivate->GetParent();
      if (parent) {
        // XHR Params Allowed
        mWorkerPrivate->SetXHRParamsAllowed(parent->XHRParamsAllowed());
      }
    }

    return NS_OK;
  }

  void
  DataReceivedFromCache(uint32_t aIndex, const uint8_t* aString,
                        uint32_t aStringLen,
                        const mozilla::dom::ChannelInfo& aChannelInfo,
                        UniquePtr<PrincipalInfo> aPrincipalInfo,
                        const nsACString& aCSPHeaderValue,
                        const nsACString& aCSPReportOnlyHeaderValue,
                        const nsACString& aReferrerPolicyHeaderValue)
  {
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
    nsIDocument* parentDoc = mWorkerPrivate->GetDocument();

    MOZ_ASSERT(!loadInfo.mScriptTextBuf);

    nsresult rv =
      ScriptLoader::ConvertToUTF16(nullptr, aString, aStringLen,
                                   NS_LITERAL_STRING("UTF-8"), parentDoc,
                                   loadInfo.mScriptTextBuf,
                                   loadInfo.mScriptTextLength);
    if (NS_SUCCEEDED(rv) && IsMainWorkerScript()) {
      nsCOMPtr<nsIURI> finalURI;
      rv = NS_NewURI(getter_AddRefs(finalURI), loadInfo.mFullURL, nullptr, nullptr);
      if (NS_SUCCEEDED(rv)) {
        mWorkerPrivate->SetBaseURI(finalURI);
      }

#if defined(DEBUG) || !defined(RELEASE_OR_BETA)
      nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
      MOZ_DIAGNOSTIC_ASSERT(principal);

      bool equal = false;
      MOZ_ALWAYS_SUCCEEDS(responsePrincipal->Equals(principal, &equal));
      MOZ_DIAGNOSTIC_ASSERT(equal);

      nsCOMPtr<nsIContentSecurityPolicy> csp;
      MOZ_ALWAYS_SUCCEEDS(responsePrincipal->GetCsp(getter_AddRefs(csp)));
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
      rv = mWorkerPrivate->SetPrincipalOnMainThread(responsePrincipal, loadGroup);
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

      rv = mWorkerPrivate->SetCSPFromHeaderValues(aCSPHeaderValue,
                                                  aCSPReportOnlyHeaderValue);
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

      mWorkerPrivate->SetReferrerPolicyFromHeaderValue(aReferrerPolicyHeaderValue);
    }

    if (NS_SUCCEEDED(rv)) {
      DataReceived();
    }

    LoadingFinished(aIndex, rv);
  }

  void
  DataReceived()
  {
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

  void
  ExecuteFinishedScripts()
  {
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
      RefPtr<ScriptExecutorRunnable> runnable =
        new ScriptExecutorRunnable(*this, mSyncLoopTarget, IsMainWorkerScript(),
                                   firstIndex, lastIndex);
      if (!runnable->Dispatch()) {
        MOZ_ASSERT(false, "This should never fail!");
      }
    }
  }
};

NS_IMPL_ISUPPORTS(ScriptLoaderRunnable, nsIRunnable, nsINamed)

class MOZ_STACK_CLASS ScriptLoaderHolder final : public WorkerHolder
{
  // Raw pointer because this holder object follows the mRunnable life-time.
  ScriptLoaderRunnable* mRunnable;

public:
  explicit ScriptLoaderHolder(ScriptLoaderRunnable* aRunnable)
    : mRunnable(aRunnable)
  {
    MOZ_ASSERT(aRunnable);
  }

  virtual bool
  Notify(Status aStatus) override
  {
    mRunnable->Notify(aStatus);
    return true;
  }
};

NS_IMETHODIMP
LoaderListener::OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                                 nsresult aStatus, uint32_t aStringLen,
                                 const uint8_t* aString)
{
  return mRunnable->OnStreamComplete(aLoader, mIndex, aStatus, aStringLen, aString);
}

NS_IMETHODIMP
LoaderListener::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  return mRunnable->OnStartRequest(aRequest, mIndex);
}

void
CachePromiseHandler::ResolvedCallback(JSContext* aCx,
                                      JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();
  // May already have been canceled by CacheScriptLoader::Fail from
  // CancelMainThread.
  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::WritingToCache ||
             mLoadInfo.mCacheStatus == ScriptLoadInfo::Cancel);
  MOZ_ASSERT_IF(mLoadInfo.mCacheStatus == ScriptLoadInfo::Cancel, !mLoadInfo.mCachePromise);

  if (mLoadInfo.mCachePromise) {
    mLoadInfo.mCacheStatus = ScriptLoadInfo::Cached;
    mLoadInfo.mCachePromise = nullptr;
    mRunnable->MaybeExecuteFinishedScripts(mIndex);
  }
}

void
CachePromiseHandler::RejectedCallback(JSContext* aCx,
                                      JS::Handle<JS::Value> aValue)
{
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

nsresult
CacheCreator::CreateCacheStorage(nsIPrincipal* aPrincipal)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!mCacheStorage);
  MOZ_ASSERT(aPrincipal);

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc, "This should never be null!");

  mozilla::AutoSafeJSContext cx;
  JS::Rooted<JSObject*> sandbox(cx);
  nsresult rv = xpc->CreateSandbox(cx, aPrincipal, sandbox.address());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

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
  mCacheStorage =
    CacheStorage::CreateOnMainThread(mozilla::dom::cache::CHROME_ONLY_NAMESPACE,
                                     mSandboxGlobalObject,
                                     aPrincipal,
                                     false, /* privateBrowsing can't be true here */
                                     true /* force trusted origin */,
                                     error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  return NS_OK;
}

nsresult
CacheCreator::Load(nsIPrincipal* aPrincipal)
{
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

void
CacheCreator::FailLoaders(nsresult aRv)
{
  AssertIsOnMainThread();

  // Fail() can call LoadingFinished() which may call ExecuteFinishedScripts()
  // which sets mCacheCreator to null, so hold a ref.
  RefPtr<CacheCreator> kungfuDeathGrip = this;

  for (uint32_t i = 0, len = mLoaders.Length(); i < len; ++i) {
    mLoaders[i]->Fail(aRv);
  }

  mLoaders.Clear();
}

void
CacheCreator::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();
  FailLoaders(NS_ERROR_FAILURE);
}

void
CacheCreator::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();

  if (!aValue.isObject()) {
    FailLoaders(NS_ERROR_FAILURE);
    return;
  }

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  Cache* cache = nullptr;
  nsresult rv = UNWRAP_OBJECT(Cache, obj, cache);
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

void
CacheCreator::DeleteCache()
{
  AssertIsOnMainThread();

  // This is called when the load is canceled which can occur before
  // mCacheStorage is initialized.
  if (mCacheStorage) {
    // It's safe to do this while Cache::Match() and Cache::Put() calls are
    // running.
    IgnoredErrorResult rv;
    RefPtr<Promise> promise = mCacheStorage->Delete(mCacheName, rv);

    // We don't care to know the result of the promise object.
  }

  // Always call this here to ensure the loaders array is cleared.
  FailLoaders(NS_ERROR_FAILURE);
}

void
CacheScriptLoader::Fail(nsresult aRv)
{
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

void
CacheScriptLoader::Load(Cache* aCache)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aCache);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), mLoadInfo.mURL, nullptr,
                          mBaseURI);
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
  request.SetAsUSVString().Rebind(mLoadInfo.mFullURL.Data(),
                                  mLoadInfo.mFullURL.Length());

  mozilla::dom::CacheQueryOptions params;

  ErrorResult error;
  RefPtr<Promise> promise = aCache->Match(request, params, error);
  if (NS_WARN_IF(error.Failed())) {
    Fail(error.StealNSResult());
    return;
  }

  promise->AppendNativeHandler(this);
}

void
CacheScriptLoader::RejectedCallback(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::Uncached);
  Fail(NS_ERROR_FAILURE);
}

void
CacheScriptLoader::ResolvedCallback(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();
  // If we have already called 'Fail', we should not proceed.
  if (mFailed) {
    return;
  }

  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::Uncached);

  nsresult rv;

  if (aValue.isUndefined()) {
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
  rv = UNWRAP_OBJECT(Response, obj, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  InternalHeaders* headers = response->GetInternalHeaders();

  IgnoredErrorResult ignored;
  headers->Get(NS_LITERAL_CSTRING("content-security-policy"),
               mCSPHeaderValue, ignored);
  headers->Get(NS_LITERAL_CSTRING("content-security-policy-report-only"),
               mCSPReportOnlyHeaderValue, ignored);
  headers->Get(NS_LITERAL_CSTRING("referrer-policy"),
               mReferrerPolicyHeaderValue, ignored);

  nsCOMPtr<nsIInputStream> inputStream;
  response->GetBody(getter_AddRefs(inputStream));
  mChannelInfo = response->GetChannelInfo();
  const UniquePtr<PrincipalInfo>& pInfo = response->GetPrincipalInfo();
  if (pInfo) {
    mPrincipalInfo = mozilla::MakeUnique<PrincipalInfo>(*pInfo);
  }

  if (!inputStream) {
    mLoadInfo.mCacheStatus = ScriptLoadInfo::Cached;
    mRunnable->DataReceivedFromCache(mIndex, (uint8_t*)"", 0, mChannelInfo,
                                     Move(mPrincipalInfo), mCSPHeaderValue,
                                     mCSPReportOnlyHeaderValue,
                                     mReferrerPolicyHeaderValue);
    return;
  }

  MOZ_ASSERT(!mPump);
  rv = NS_NewInputStreamPump(getter_AddRefs(mPump), inputStream);
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
CacheScriptLoader::OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                                    nsresult aStatus, uint32_t aStringLen,
                                    const uint8_t* aString)
{
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
  mRunnable->DataReceivedFromCache(mIndex, aString, aStringLen, mChannelInfo,
                                   Move(mPrincipalInfo), mCSPHeaderValue,
                                   mCSPReportOnlyHeaderValue,
                                   mReferrerPolicyHeaderValue);
  return NS_OK;
}

class ChannelGetterRunnable final : public WorkerMainThreadRunnable
{
  const nsAString& mScriptURL;
  WorkerLoadInfo& mLoadInfo;
  nsresult mResult;

public:
  ChannelGetterRunnable(WorkerPrivate* aParentWorker,
                        const nsAString& aScriptURL,
                        WorkerLoadInfo& aLoadInfo)
    : WorkerMainThreadRunnable(aParentWorker,
                               NS_LITERAL_CSTRING("ScriptLoader :: ChannelGetter"))
    , mScriptURL(aScriptURL)
    , mLoadInfo(aLoadInfo)
    , mResult(NS_ERROR_FAILURE)
  {
    MOZ_ASSERT(aParentWorker);
    aParentWorker->AssertIsOnWorkerThread();
  }

  virtual bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    // Initialize the WorkerLoadInfo principal to our triggering principal
    // before doing anything else.  Normally we do this in the WorkerPrivate
    // Constructor, but we can't do so off the main thread when creating
    // a nested worker.  So do it here instead.
    mLoadInfo.mPrincipal = mWorkerPrivate->GetPrincipal();
    MOZ_ASSERT(mLoadInfo.mPrincipal);

    // Figure out our base URI.
    nsCOMPtr<nsIURI> baseURI = mWorkerPrivate->GetBaseURI();
    MOZ_ASSERT(baseURI);

    // May be null.
    nsCOMPtr<nsIDocument> parentDoc = mWorkerPrivate->GetDocument();

    mLoadInfo.mLoadGroup = mWorkerPrivate->GetLoadGroup();

    nsCOMPtr<nsIChannel> channel;
    mResult =
      scriptloader::ChannelFromScriptURLMainThread(mLoadInfo.mPrincipal,
                                                   baseURI, parentDoc,
                                                   mLoadInfo.mLoadGroup,
                                                   mScriptURL,
                                                   // Nested workers are always dedicated.
                                                   nsIContentPolicy::TYPE_INTERNAL_WORKER,
                                                   // Nested workers use default uri encoding.
                                                   true,
                                                   getter_AddRefs(channel));
    NS_ENSURE_SUCCESS(mResult, true);

    mResult = mLoadInfo.SetPrincipalFromChannel(channel);
    NS_ENSURE_SUCCESS(mResult, true);

    mLoadInfo.mChannel = channel.forget();
    return true;
  }

  nsresult
  GetResult() const
  {
    return mResult;
  }

private:
  virtual ~ChannelGetterRunnable()
  { }
};

ScriptExecutorRunnable::ScriptExecutorRunnable(
                                            ScriptLoaderRunnable& aScriptLoader,
                                            nsIEventTarget* aSyncLoopTarget,
                                            bool aIsWorkerScript,
                                            uint32_t aFirstIndex,
                                            uint32_t aLastIndex)
: MainThreadWorkerSyncRunnable(aScriptLoader.mWorkerPrivate, aSyncLoopTarget),
  mScriptLoader(aScriptLoader), mIsWorkerScript(aIsWorkerScript),
  mFirstIndex(aFirstIndex), mLastIndex(aLastIndex)
{
  MOZ_ASSERT(aFirstIndex <= aLastIndex);
  MOZ_ASSERT(aLastIndex < aScriptLoader.mLoadInfos.Length());
}

bool
ScriptExecutorRunnable::IsDebuggerRunnable() const
{
  // ScriptExecutorRunnable is used to execute both worker and debugger scripts.
  // In the latter case, the runnable needs to be dispatched to the debugger
  // queue.
  return mScriptLoader.mWorkerScriptType == DebuggerScript;
}

bool
ScriptExecutorRunnable::PreRun(WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnWorkerThread();

  if (!mIsWorkerScript) {
    return true;
  }

  if (!aWorkerPrivate->GetJSContext()) {
    return false;
  }

  MOZ_ASSERT(mFirstIndex == 0);
  MOZ_ASSERT(!mScriptLoader.mRv.Failed());

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

bool
ScriptExecutorRunnable::WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
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
      scriptloader::ReportLoadError(mScriptLoader.mRv,
                                    loadInfo.mLoadResult, loadInfo.mURL);
      // Top level scripts only!
      if (mIsWorkerScript) {
        aWorkerPrivate->MaybeDispatchLoadFailedRunnable();
      }
      return true;
    }

    NS_ConvertUTF16toUTF8 filename(loadInfo.mURL);

    JS::CompileOptions options(aCx);
    options.setFileAndLine(filename.get(), 1)
           .setNoScriptRval(true);

    if (mScriptLoader.mWorkerScriptType == DebuggerScript) {
      options.setVersion(JSVERSION_LATEST);
    }

    MOZ_ASSERT(loadInfo.mMutedErrorFlag.isSome());
    options.setMutedErrors(loadInfo.mMutedErrorFlag.valueOr(true));

    JS::SourceBufferHolder srcBuf(loadInfo.mScriptTextBuf,
                                  loadInfo.mScriptTextLength,
                                  JS::SourceBufferHolder::GiveOwnership);
    loadInfo.mScriptTextBuf = nullptr;
    loadInfo.mScriptTextLength = 0;

    // Our ErrorResult still shouldn't be a failure.
    MOZ_ASSERT(!mScriptLoader.mRv.Failed(), "Who failed it and why?");
    JS::Rooted<JS::Value> unused(aCx);
    if (!JS::Evaluate(aCx, options, srcBuf, &unused)) {
      mScriptLoader.mRv.StealExceptionFromJSContext(aCx);
      return true;
    }

    loadInfo.mExecutionResult = true;
  }

  return true;
}

void
ScriptExecutorRunnable::PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                                bool aRunResult)
{
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
    MOZ_ASSERT_IF(!result && !mScriptLoader.mRv.Failed(),
                  !aRunResult);
    ShutdownScriptLoader(aCx, aWorkerPrivate, result, mutedError);
  }
}

nsresult
ScriptExecutorRunnable::Cancel()
{
  if (mLastIndex == mScriptLoader.mLoadInfos.Length() - 1) {
    ShutdownScriptLoader(mWorkerPrivate->GetJSContext(), mWorkerPrivate,
                         false, false);
  }
  return MainThreadWorkerSyncRunnable::Cancel();
}

void
ScriptExecutorRunnable::ShutdownScriptLoader(JSContext* aCx,
                                             WorkerPrivate* aWorkerPrivate,
                                             bool aResult,
                                             bool aMutedError)
{
  aWorkerPrivate->AssertIsOnWorkerThread();

  MOZ_ASSERT(mLastIndex == mScriptLoader.mLoadInfos.Length() - 1);

  if (mIsWorkerScript && aWorkerPrivate->IsServiceWorker()) {
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
        mScriptLoader.mRv.ThrowWithCustomCleanup(NS_ERROR_DOM_NETWORK_ERR);
      }
    } else {
      mScriptLoader.mRv.ThrowWithCustomCleanup(NS_ERROR_DOM_INVALID_STATE_ERR);
    }
  }

  aWorkerPrivate->StopSyncLoop(mSyncLoopTarget, aResult);
}

void
ScriptExecutorRunnable::LogExceptionToConsole(JSContext* aCx,
                                              WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnWorkerThread();

  MOZ_ASSERT(mScriptLoader.mRv.IsJSException());

  JS::Rooted<JS::Value> exn(aCx);
  if (!ToJSValue(aCx, mScriptLoader.mRv, &exn)) {
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

void
LoadAllScripts(WorkerPrivate* aWorkerPrivate,
               nsTArray<ScriptLoadInfo>& aLoadInfos, bool aIsMainScript,
               WorkerScriptType aWorkerScriptType, ErrorResult& aRv)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(!aLoadInfos.IsEmpty(), "Bad arguments!");

  AutoSyncLoopHolder syncLoop(aWorkerPrivate, Terminating);
  nsCOMPtr<nsIEventTarget> syncLoopTarget = syncLoop.GetEventTarget();
  if (!syncLoopTarget) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  RefPtr<ScriptLoaderRunnable> loader =
    new ScriptLoaderRunnable(aWorkerPrivate, syncLoopTarget, aLoadInfos,
                             aIsMainScript, aWorkerScriptType, aRv);

  NS_ASSERTION(aLoadInfos.IsEmpty(), "Should have swapped!");

  ScriptLoaderHolder workerHolder(loader);

  if (NS_WARN_IF(!workerHolder.HoldWorker(aWorkerPrivate, Terminating))) {
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

BEGIN_WORKERS_NAMESPACE

namespace scriptloader {

nsresult
ChannelFromScriptURLMainThread(nsIPrincipal* aPrincipal,
                               nsIURI* aBaseURI,
                               nsIDocument* aParentDoc,
                               nsILoadGroup* aLoadGroup,
                               const nsAString& aScriptURL,
                               nsContentPolicyType aMainScriptContentPolicyType,
                               bool aDefaultURIEncoding,
                               nsIChannel** aChannel)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIIOService> ios(do_GetIOService());

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  NS_ASSERTION(secMan, "This should never be null!");

  return ChannelFromScriptURL(aPrincipal, aBaseURI, aParentDoc, aLoadGroup,
                              ios, secMan, aScriptURL, true, WorkerScript,
                              aMainScriptContentPolicyType,
                              nsIRequest::LOAD_NORMAL, aDefaultURIEncoding,
                              aChannel);
}

nsresult
ChannelFromScriptURLWorkerThread(JSContext* aCx,
                                 WorkerPrivate* aParent,
                                 const nsAString& aScriptURL,
                                 WorkerLoadInfo& aLoadInfo)
{
  aParent->AssertIsOnWorkerThread();

  RefPtr<ChannelGetterRunnable> getter =
    new ChannelGetterRunnable(aParent, aScriptURL, aLoadInfo);

  ErrorResult rv;
  getter->Dispatch(Terminating, rv);
  if (rv.Failed()) {
    NS_ERROR("Failed to dispatch!");
    return rv.StealNSResult();
  }

  return getter->GetResult();
}

void ReportLoadError(ErrorResult& aRv, nsresult aLoadResult,
                     const nsAString& aScriptURL)
{
  MOZ_ASSERT(!aRv.Failed());

  switch (aLoadResult) {
    case NS_ERROR_FILE_NOT_FOUND:
    case NS_ERROR_NOT_AVAILABLE:
      aLoadResult = NS_ERROR_DOM_NETWORK_ERR;
      break;

    case NS_ERROR_MALFORMED_URI:
      aLoadResult = NS_ERROR_DOM_SYNTAX_ERR;
      break;

    case NS_BINDING_ABORTED:
      // Note: we used to pretend like we didn't set an exception for
      // NS_BINDING_ABORTED, but then ShutdownScriptLoader did it anyway.  The
      // other callsite, in WorkerPrivate::Constructor, never passed in
      // NS_BINDING_ABORTED.  So just throw it directly here.  Consumers will
      // deal as needed.  But note that we do NOT want to ThrowDOMException()
      // for this case, because that will make it impossible for consumers to
      // realize that our error was NS_BINDING_ABORTED.
      aRv.Throw(aLoadResult);
      return;

    case NS_ERROR_DOM_SECURITY_ERR:
    case NS_ERROR_DOM_SYNTAX_ERR:
      break;

    case NS_ERROR_DOM_BAD_URI:
      // This is actually a security error.
      aLoadResult = NS_ERROR_DOM_SECURITY_ERR;
      break;

    default:
      // For lack of anything better, go ahead and throw a NetworkError here.
      // We don't want to throw a JS exception, because for toplevel script
      // loads that would get squelched.
      aRv.ThrowDOMException(NS_ERROR_DOM_NETWORK_ERR,
        nsPrintfCString("Failed to load worker script at %s (nsresult = 0x%" PRIx32 ")",
                        NS_ConvertUTF16toUTF8(aScriptURL).get(),
                        static_cast<uint32_t>(aLoadResult)));
      return;
  }

  aRv.ThrowDOMException(aLoadResult,
                        NS_LITERAL_CSTRING("Failed to load worker script at \"") +
                        NS_ConvertUTF16toUTF8(aScriptURL) +
                        NS_LITERAL_CSTRING("\""));
}

void
LoadMainScript(WorkerPrivate* aWorkerPrivate,
               const nsAString& aScriptURL,
               WorkerScriptType aWorkerScriptType,
               ErrorResult& aRv)
{
  nsTArray<ScriptLoadInfo> loadInfos;

  ScriptLoadInfo* info = loadInfos.AppendElement();
  info->mURL = aScriptURL;
  info->mLoadFlags = aWorkerPrivate->GetLoadFlags();

  LoadAllScripts(aWorkerPrivate, loadInfos, true, aWorkerScriptType, aRv);
}

void
Load(WorkerPrivate* aWorkerPrivate,
     const nsTArray<nsString>& aScriptURLs, WorkerScriptType aWorkerScriptType,
     ErrorResult& aRv)
{
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

  LoadAllScripts(aWorkerPrivate, loadInfos, false, aWorkerScriptType, aRv);
}

} // namespace scriptloader

END_WORKERS_NAMESPACE
