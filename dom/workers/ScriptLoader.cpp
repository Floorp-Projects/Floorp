/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoader.h"

#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIHttpChannel.h"
#include "nsIInputStreamPump.h"
#include "nsIIOService.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamLoader.h"
#include "nsIStreamListenerTee.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIURI.h"

#include "jsapi.h"
#include "nsError.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsDocShellCID.h"
#include "nsISupportsPrimitives.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsString.h"
#include "nsStreamUtils.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "xpcpublic.h"

#include "mozilla/Assertions.h"
#include "mozilla/LoadContext.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/cache/Cache.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Response.h"
#include "Principal.h"
#include "WorkerFeature.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

#define MAX_CONCURRENT_SCRIPTS 1000

USING_WORKERS_NAMESPACE

using mozilla::dom::cache::Cache;
using mozilla::dom::cache::CacheStorage;
using mozilla::dom::Promise;
using mozilla::dom::PromiseNativeHandler;
using mozilla::dom::workers::exceptions::ThrowDOMExceptionForNSResult;

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
                     nsIChannel** aChannel)
{
  AssertIsOnMainThread();

  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri),
                                                 aScriptURL, parentDoc,
                                                 baseURI);
  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  // If we're part of a document then check the content load policy.
  if (parentDoc) {
    int16_t shouldLoad = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_SCRIPT, uri,
                                   principal, parentDoc,
                                   NS_LITERAL_CSTRING("text/javascript"),
                                   nullptr, &shouldLoad,
                                   nsContentUtils::GetContentPolicy(),
                                   secMan);
    if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
      if (NS_FAILED(rv) || shouldLoad != nsIContentPolicy::REJECT_TYPE) {
        return rv = NS_ERROR_CONTENT_BLOCKED;
      }
      return rv = NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
    }
  }

  if (aWorkerScriptType == DebuggerScript) {
    bool isChrome = false;
    NS_ENSURE_SUCCESS(uri->SchemeIs("chrome", &isChrome),
                      NS_ERROR_DOM_SECURITY_ERR);

    bool isResource = false;
    NS_ENSURE_SUCCESS(uri->SchemeIs("resource", &isResource),
                      NS_ERROR_DOM_SECURITY_ERR);

    if (!isChrome && !isResource) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
  } else if (aIsMainScript) {
    // If this script loader is being used to make a new worker then we need
    // to do a same-origin check. Otherwise we need to clear the load with the
    // security manager.
    nsCString scheme;
    rv = uri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    // We pass true as the 3rd argument to checkMayLoad here.
    // This allows workers in sandboxed documents to load data URLs
    // (and other URLs that inherit their principal from their
    // creator.)
    rv = principal->CheckMayLoad(uri, false, true);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SECURITY_ERR);
  }
  else {
    rv = secMan->CheckLoadURIWithPrincipal(principal, uri, 0);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SECURITY_ERR);
  }

  uint32_t flags = nsIRequest::LOAD_NORMAL | nsIChannel::LOAD_CLASSIFY_URI;

  nsCOMPtr<nsIChannel> channel;
  // If we have the document, use it
  if (parentDoc) {
    rv = NS_NewChannel(getter_AddRefs(channel),
                       uri,
                       parentDoc,
                       nsILoadInfo::SEC_NORMAL,
                       nsIContentPolicy::TYPE_SCRIPT,
                       loadGroup,
                       nullptr, // aCallbacks
                       flags,
                       ios);
  } else {
    // We must have a loadGroup with a load context for the principal to
    // traverse the channel correctly.
    MOZ_ASSERT(loadGroup);
    MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(loadGroup, principal));

    rv = NS_NewChannel(getter_AddRefs(channel),
                       uri,
                       principal,
                       nsILoadInfo::SEC_NORMAL,
                       nsIContentPolicy::TYPE_SCRIPT,
                       loadGroup,
                       nullptr, // aCallbacks
                       flags,
                       ios);
  }

  NS_ENSURE_SUCCESS(rv, rv);

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
  nsRefPtr<Promise> mCachePromise;

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
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;

  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
          override;

  NS_DECL_NSICANCELABLERUNNABLE

  void
  ShutdownScriptLoader(JSContext* aCx,
                       WorkerPrivate* aWorkerPrivate,
                       bool aResult);
};

class CacheScriptLoader;

class CacheCreator final : public PromiseNativeHandler
{
public:
  explicit CacheCreator(WorkerPrivate* aWorkerPrivate)
    : mCacheName(aWorkerPrivate->ServiceWorkerCacheName())
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

  nsRefPtr<Cache> mCache;
  nsRefPtr<CacheStorage> mCacheStorage;
  nsCOMPtr<nsIGlobalObject> mSandboxGlobalObject;
  nsTArray<nsRefPtr<CacheScriptLoader>> mLoaders;

  nsString mCacheName;
};

class CacheScriptLoader final : public PromiseNativeHandler
                                  , public nsIStreamLoaderObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
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
  nsRefPtr<ScriptLoaderRunnable> mRunnable;
  bool mIsWorkerScript;
  bool mFailed;
  nsCOMPtr<nsIInputStreamPump> mPump;
  nsCOMPtr<nsIURI> mBaseURI;
};

NS_IMPL_ISUPPORTS(CacheScriptLoader, nsIStreamLoaderObserver)

class CachePromiseHandler final : public PromiseNativeHandler
{
public:
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

  nsRefPtr<ScriptLoaderRunnable> mRunnable;
  ScriptLoadInfo& mLoadInfo;
  uint32_t mIndex;
};

class ScriptLoaderRunnable final : public WorkerFeature,
                                   public nsIRunnable,
                                   public nsIStreamLoaderObserver
{
  friend class ScriptExecutorRunnable;
  friend class CachePromiseHandler;
  friend class CacheScriptLoader;

  WorkerPrivate* mWorkerPrivate;
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;
  nsTArray<ScriptLoadInfo> mLoadInfos;
  nsRefPtr<CacheCreator> mCacheCreator;
  bool mIsMainScript;
  WorkerScriptType mWorkerScriptType;
  bool mCanceled;
  bool mCanceledMainThread;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ScriptLoaderRunnable(WorkerPrivate* aWorkerPrivate,
                       nsIEventTarget* aSyncLoopTarget,
                       nsTArray<ScriptLoadInfo>& aLoadInfos,
                       bool aIsMainScript,
                       WorkerScriptType aWorkerScriptType)
  : mWorkerPrivate(aWorkerPrivate), mSyncLoopTarget(aSyncLoopTarget),
    mIsMainScript(aIsMainScript), mWorkerScriptType(aWorkerScriptType),
    mCanceled(false), mCanceledMainThread(false)
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

    if (NS_FAILED(RunInternal())) {
      CancelMainThread();
    }

    return NS_OK;
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

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                   nsresult aStatus, uint32_t aStringLen,
                   const uint8_t* aString) override
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsISupportsPRUint32> indexSupports(do_QueryInterface(aContext));
    NS_ASSERTION(indexSupports, "This should never fail!");

    uint32_t index = UINT32_MAX;
    if (NS_FAILED(indexSupports->GetData(&index)) ||
        index >= mLoadInfos.Length()) {
      NS_ERROR("Bad index!");
    }

    ScriptLoadInfo& loadInfo = mLoadInfos[index];

    nsresult rv = OnStreamCompleteInternal(aLoader, aContext, aStatus,
                                           aStringLen, aString, loadInfo);
    LoadingFinished(index, rv);
    return NS_OK;
  }

  virtual bool
  Notify(JSContext* aCx, Status aStatus) override
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    if (aStatus >= Terminating && !mCanceled) {
      mCanceled = true;

      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableMethod(this, &ScriptLoaderRunnable::CancelMainThread);
      NS_ASSERTION(runnable, "This should never fail!");

      if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
        JS_ReportError(aCx, "Failed to cancel script loader!");
        return false;
      }
    }

    return true;
  }

  bool
  IsMainWorkerScript() const
  {
    return mIsMainScript && mWorkerScriptType == WorkerScript;
  }

  void
  CancelMainThread()
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
        loadInfo.mCachePromise->MaybeReject(NS_BINDING_ABORTED);
        loadInfo.mCachePromise = nullptr;
        callLoadingFinished = false;
      }

      if (loadInfo.mChannel) {
        if (NS_SUCCEEDED(loadInfo.mChannel->Cancel(NS_BINDING_ABORTED))) {
          callLoadingFinished = false;
        } else {
          NS_WARNING("Failed to cancel channel!");
        }
      }

      if (callLoadingFinished && !loadInfo.Finished()) {
        LoadingFinished(index, NS_BINDING_ABORTED);
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
          return rv;
        }
      }

      return NS_OK;
    }

    MOZ_ASSERT(!mCacheCreator);
    mCacheCreator = new CacheCreator(mWorkerPrivate);

    for (uint32_t index = 0, len = mLoadInfos.Length(); index < len; ++index) {
      nsRefPtr<CacheScriptLoader> loader =
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

    // Figure out which principal to use.
    nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
    nsCOMPtr<nsILoadGroup> loadGroup = mWorkerPrivate->GetLoadGroup();
    if (!principal) {
      NS_ASSERTION(parentWorker, "Must have a principal!");
      NS_ASSERTION(mIsMainScript, "Must have a principal for importScripts!");

      principal = parentWorker->GetPrincipal();
      loadGroup = parentWorker->GetLoadGroup();
    }
    NS_ASSERTION(principal, "This should never be null here!");
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

    if (!channel) {
      rv = ChannelFromScriptURL(principal, baseURI, parentDoc, loadGroup, ios,
                                secMan, loadInfo.mURL, IsMainWorkerScript(),
                                mWorkerScriptType, getter_AddRefs(channel));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    // We need to know which index we're on in OnStreamComplete so we know
    // where to put the result.
    nsCOMPtr<nsISupportsPRUint32> indexSupports =
      do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = indexSupports->SetData(aIndex);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // We don't care about progress so just use the simple stream loader for
    // OnStreamComplete notification only.
    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (loadInfo.mCacheStatus != ScriptLoadInfo::ToBeCached) {
      rv = channel->AsyncOpen(loader, indexSupports);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      nsCOMPtr<nsIInputStream> reader;
      nsCOMPtr<nsIOutputStream> writer;

      // In case we return early.
      loadInfo.mCacheStatus = ScriptLoadInfo::Cancel;

      rv = NS_NewPipe(getter_AddRefs(reader), getter_AddRefs(writer), 0,
                      UINT32_MAX, // unlimited size to avoid writer WOULD_BLOCK case
                      true, false); // non-blocking reader, blocking writer
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // We synthesize the result code, but its never exposed to content.
      nsRefPtr<InternalResponse> ir =
        new InternalResponse(200, NS_LITERAL_CSTRING("OK"));
      ir->SetBody(reader);

      nsCOMPtr<nsIStreamListenerTee> tee =
        do_CreateInstance(NS_STREAMLISTENERTEE_CONTRACTID);
      rv = tee->Init(loader, writer, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = channel->AsyncOpen(tee, indexSupports);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsRefPtr<Response> response = new Response(mCacheCreator->Global(), ir);

      RequestOrUSVString request;

      MOZ_ASSERT(!loadInfo.mFullURL.IsEmpty());
      request.SetAsUSVString().Rebind(loadInfo.mFullURL.Data(),
                                      loadInfo.mFullURL.Length());

      ErrorResult error;
      nsRefPtr<Promise> cachePromise =
        mCacheCreator->Cache_()->Put(request, *response, error);
      if (NS_WARN_IF(error.Failed())) {
        nsresult rv = error.StealNSResult();
        channel->Cancel(rv);
        return rv;
      }

      nsRefPtr<CachePromiseHandler> promiseHandler =
        new CachePromiseHandler(this, loadInfo, aIndex);
      cachePromise->AppendNativeHandler(promiseHandler);

      loadInfo.mCachePromise.swap(cachePromise);
      loadInfo.mCacheStatus = ScriptLoadInfo::WritingToCache;
    }

    loadInfo.mChannel.swap(channel);

    return NS_OK;
  }

  nsresult
  OnStreamCompleteInternal(nsIStreamLoader* aLoader, nsISupports* aContext,
                           nsresult aStatus, uint32_t aStringLen,
                           const uint8_t* aString, ScriptLoadInfo& aLoadInfo)
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

    // Make sure we're not seeing the result of a 404 or something by checking
    // the 'requestSucceeded' attribute on the http channel.
    nsCOMPtr<nsIRequest> request;
    nsresult rv = aLoader->GetRequest(getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
    if (httpChannel) {
      bool requestSucceeded;
      rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!requestSucceeded) {
        return NS_ERROR_NOT_AVAILABLE;
      }
    }

    // May be null.
    nsIDocument* parentDoc = mWorkerPrivate->GetDocument();

    // Use the regular nsScriptLoader for this grunt work! Should be just fine
    // because we're running on the main thread.
    // Unlike <script> tags, Worker scripts are always decoded as UTF-8,
    // per spec. So we explicitly pass in the charset hint.
    rv = nsScriptLoader::ConvertToUTF16(aLoadInfo.mChannel, aString, aStringLen,
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

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    NS_ASSERTION(channel, "This should never fail!");

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

      // Now to figure out which principal to give this worker.
      WorkerPrivate* parent = mWorkerPrivate->GetParent();

      NS_ASSERTION(mWorkerPrivate->GetPrincipal() || parent,
                   "Must have one of these!");

      nsCOMPtr<nsIPrincipal> loadPrincipal = mWorkerPrivate->GetPrincipal() ?
                                             mWorkerPrivate->GetPrincipal() :
                                             parent->GetPrincipal();

      nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
      NS_ASSERTION(ssm, "Should never be null!");

      nsCOMPtr<nsIPrincipal> channelPrincipal;
      rv = ssm->GetChannelResultPrincipal(channel, getter_AddRefs(channelPrincipal));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsILoadGroup> channelLoadGroup;
      rv = channel->GetLoadGroup(getter_AddRefs(channelLoadGroup));
      NS_ENSURE_SUCCESS(rv, rv);
      MOZ_ASSERT(channelLoadGroup);

      // See if this is a resource URI. Since JSMs usually come from resource://
      // URIs we're currently considering all URIs with the URI_IS_UI_RESOURCE
      // flag as valid for creating privileged workers.
      if (!nsContentUtils::IsSystemPrincipal(channelPrincipal)) {
        bool isResource;
        rv = NS_URIChainHasFlags(finalURI,
                                 nsIProtocolHandler::URI_IS_UI_RESOURCE,
                                 &isResource);
        NS_ENSURE_SUCCESS(rv, rv);

        if (isResource) {
          rv = ssm->GetSystemPrincipal(getter_AddRefs(channelPrincipal));
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }

      // If the load principal is the system principal then the channel
      // principal must also be the system principal (we do not allow chrome
      // code to create workers with non-chrome scripts). Otherwise this channel
      // principal must be same origin with the load principal (we check again
      // here in case redirects changed the location of the script).
      if (nsContentUtils::IsSystemPrincipal(loadPrincipal)) {
        if (!nsContentUtils::IsSystemPrincipal(channelPrincipal)) {
          return NS_ERROR_DOM_BAD_URI;
        }
      }
      else  {
        nsCString scheme;
        rv = finalURI->GetScheme(scheme);
        NS_ENSURE_SUCCESS(rv, rv);

        // We exempt data urls and other URI's that inherit their
        // principal again.
        if (NS_FAILED(loadPrincipal->CheckMayLoad(finalURI, false, true))) {
          return NS_ERROR_DOM_BAD_URI;
        }
      }

      // The principal can change, but it should still match the original
      // load group's appId and browser element flag.
      MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(channelLoadGroup, channelPrincipal));

      mWorkerPrivate->SetPrincipal(channelPrincipal, channelLoadGroup);
    }

    DataReceived();
    return NS_OK;
  }

  void
  DataReceivedFromCache(uint32_t aIndex, const uint8_t* aString,
                        uint32_t aStringLen)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aIndex < mLoadInfos.Length());
    ScriptLoadInfo& loadInfo = mLoadInfos[aIndex];
    MOZ_ASSERT(loadInfo.mCacheStatus == ScriptLoadInfo::Cached);

    // May be null.
    nsIDocument* parentDoc = mWorkerPrivate->GetDocument();

    MOZ_ASSERT(!loadInfo.mScriptTextBuf);

    nsresult rv =
      nsScriptLoader::ConvertToUTF16(nullptr, aString, aStringLen,
                                     NS_LITERAL_STRING("UTF-8"), parentDoc,
                                     loadInfo.mScriptTextBuf,
                                     loadInfo.mScriptTextLength);
    if (NS_SUCCEEDED(rv) && IsMainWorkerScript()) {
      nsCOMPtr<nsIURI> finalURI;
      rv = NS_NewURI(getter_AddRefs(finalURI), loadInfo.mFullURL, nullptr, nullptr);
      if (NS_SUCCEEDED(rv)) {
        mWorkerPrivate->SetBaseURI(finalURI);
      }

      nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
      MOZ_ASSERT(principal);
      nsILoadGroup* loadGroup = mWorkerPrivate->GetLoadGroup();
      MOZ_ASSERT(loadGroup);
      // Needed to initialize the principal info. This is fine because
      // the cache principal cannot change, unlike the channel principal.
      mWorkerPrivate->SetPrincipal(principal, loadGroup);
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
      nsRefPtr<ScriptExecutorRunnable> runnable =
        new ScriptExecutorRunnable(*this, mSyncLoopTarget, IsMainWorkerScript(),
                                   firstIndex, lastIndex);
      if (!runnable->Dispatch(nullptr)) {
        MOZ_ASSERT(false, "This should never fail!");
      }
    }
  }
};

NS_IMPL_ISUPPORTS(ScriptLoaderRunnable, nsIRunnable, nsIStreamLoaderObserver)

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

  AutoSafeJSContext cx;
  nsCOMPtr<nsIXPConnectJSObjectHolder> sandbox;
  nsresult rv = xpc->CreateSandbox(cx, aPrincipal, getter_AddRefs(sandbox));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mSandboxGlobalObject = xpc::NativeGlobal(sandbox->GetJSObject());
  if (NS_WARN_IF(!mSandboxGlobalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult error;
  mCacheStorage =
    CacheStorage::CreateOnMainThread(cache::CHROME_ONLY_NAMESPACE,
                                     mSandboxGlobalObject,
                                     aPrincipal, error);
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
  nsRefPtr<Promise> promise = mCacheStorage->Open(mCacheName, error);
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
  nsRefPtr<CacheCreator> kungfuDeathGrip = this;

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
  MOZ_ASSERT(aValue.isObject());

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  Cache* cache = nullptr;
  nsresult rv = UNWRAP_OBJECT(Cache, obj, cache);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));

  mCache = cache;
  MOZ_ASSERT(mCache);

  // If the worker is canceled, CancelMainThread() will have cleared the
  // loaders.
  for (uint32_t i = 0, len = mLoaders.Length(); i < len; ++i) {
    mLoaders[i]->Load(cache);
  }
}

void
CacheCreator::DeleteCache()
{
  AssertIsOnMainThread();

  ErrorResult rv;

  // It's safe to do this while Cache::Match() and Cache::Put() calls are
  // running.
  nsRefPtr<Promise> promise = mCacheStorage->Delete(mCacheName, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return;
  }

  // We don't care to know the result of the promise object.
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

  RequestOrUSVString request;
  request.SetAsUSVString().Rebind(mLoadInfo.mFullURL.Data(),
                                  mLoadInfo.mFullURL.Length());

  CacheQueryOptions params;

  ErrorResult error;
  nsRefPtr<Promise> promise = aCache->Match(request, params, error);
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

  if (aValue.isUndefined()) {
    mLoadInfo.mCacheStatus = ScriptLoadInfo::ToBeCached;
    mRunnable->LoadScript(mIndex);
    return;
  }

  MOZ_ASSERT(aValue.isObject());

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  Response* response = nullptr;
  nsresult rv = UNWRAP_OBJECT(Response, obj, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  response->GetBody(getter_AddRefs(inputStream));

  if (!inputStream) {
    mLoadInfo.mCacheStatus = ScriptLoadInfo::Cached;
    mRunnable->DataReceivedFromCache(mIndex, (uint8_t*)"", 0);
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
  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::ReadingFromCache);

  mPump = nullptr;

  if (NS_FAILED(aStatus)) {
    Fail(aStatus);
    return NS_OK;
  }

  mLoadInfo.mCacheStatus = ScriptLoadInfo::Cached;

  mRunnable->DataReceivedFromCache(mIndex, aString, aStringLen);
  return NS_OK;
}

 class ChannelGetterRunnable final : public nsRunnable
{
  WorkerPrivate* mParentWorker;
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;
  const nsAString& mScriptURL;
  nsIChannel** mChannel;
  nsresult mResult;

public:
  ChannelGetterRunnable(WorkerPrivate* aParentWorker,
                        nsIEventTarget* aSyncLoopTarget,
                        const nsAString& aScriptURL,
                        nsIChannel** aChannel)
  : mParentWorker(aParentWorker), mSyncLoopTarget(aSyncLoopTarget),
    mScriptURL(aScriptURL), mChannel(aChannel), mResult(NS_ERROR_FAILURE)
  {
    MOZ_ASSERT(mParentWorker);
    aParentWorker->AssertIsOnWorkerThread();
    MOZ_ASSERT(aSyncLoopTarget);
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

    nsIPrincipal* principal = mParentWorker->GetPrincipal();
    MOZ_ASSERT(principal);

    // Figure out our base URI.
    nsCOMPtr<nsIURI> baseURI = mParentWorker->GetBaseURI();
    MOZ_ASSERT(baseURI);

    // May be null.
    nsCOMPtr<nsIDocument> parentDoc = mParentWorker->GetDocument();

    nsCOMPtr<nsILoadGroup> loadGroup = mParentWorker->GetLoadGroup();

    nsCOMPtr<nsIChannel> channel;
    mResult =
      scriptloader::ChannelFromScriptURLMainThread(principal, baseURI,
                                                   parentDoc, loadGroup,
                                                   mScriptURL,
                                                   getter_AddRefs(channel));
    if (NS_SUCCEEDED(mResult)) {
      channel.forget(mChannel);
    }

    nsRefPtr<MainThreadStopSyncLoopRunnable> runnable =
      new MainThreadStopSyncLoopRunnable(mParentWorker,
                                         mSyncLoopTarget.forget(), true);
    if (!runnable->Dispatch(nullptr)) {
      NS_ERROR("This should never fail!");
    }

    return NS_OK;
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
ScriptExecutorRunnable::WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
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

  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  NS_ASSERTION(global, "Must have a global by now!");

  // Determine whether we want to be discarding source on this global to save
  // memory. It would make more sense to do this when we create the global, but
  // the information behind UsesSystemPrincipal() et al isn't finalized until
  // the call to SetPrincipal during the first script load. After that, however,
  // it never changes. So we can just idempotently set the bits here.
  //
  // Note that we read a pref that is cached on the main thread. This is benignly
  // racey.
  if (xpc::ShouldDiscardSystemSource()) {
    bool discard = aWorkerPrivate->UsesSystemPrincipal() ||
                   aWorkerPrivate->IsInPrivilegedApp();
    JS::CompartmentOptionsRef(global).setDiscardSource(discard);
  }

  // Similar to the above.
  if (xpc::ExtraWarningsForSystemJS() && aWorkerPrivate->UsesSystemPrincipal()) {
      JS::CompartmentOptionsRef(global).extraWarningsOverride().set(true);
  }

  for (uint32_t index = mFirstIndex; index <= mLastIndex; index++) {
    ScriptLoadInfo& loadInfo = loadInfos.ElementAt(index);

    NS_ASSERTION(!loadInfo.mChannel, "Should no longer have a channel!");
    NS_ASSERTION(loadInfo.mExecutionScheduled, "Should be scheduled!");
    NS_ASSERTION(!loadInfo.mExecutionResult, "Should not have executed yet!");

    if (NS_FAILED(loadInfo.mLoadResult)) {
      scriptloader::ReportLoadError(aCx, loadInfo.mURL, loadInfo.mLoadResult,
                                    false);
      return true;
    }

    NS_ConvertUTF16toUTF8 filename(loadInfo.mURL);

    JS::CompileOptions options(aCx);
    options.setFileAndLine(filename.get(), 1)
           .setNoScriptRval(true);

    if (mScriptLoader.mWorkerScriptType == DebuggerScript) {
      options.setVersion(JSVERSION_LATEST);
    }

    JS::SourceBufferHolder srcBuf(loadInfo.mScriptTextBuf,
                                  loadInfo.mScriptTextLength,
                                  JS::SourceBufferHolder::GiveOwnership);
    loadInfo.mScriptTextBuf = nullptr;
    loadInfo.mScriptTextLength = 0;

    JS::Rooted<JS::Value> unused(aCx);
    if (!JS::Evaluate(aCx, options, srcBuf, &unused)) {
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
  nsTArray<ScriptLoadInfo>& loadInfos = mScriptLoader.mLoadInfos;

  if (mLastIndex == loadInfos.Length() - 1) {
    // All done. If anything failed then return false.
    bool result = true;
    for (uint32_t index = 0; index < loadInfos.Length(); index++) {
      if (!loadInfos[index].mExecutionResult) {
        result = false;
        break;
      }
    }

    ShutdownScriptLoader(aCx, aWorkerPrivate, result);
  }
}

NS_IMETHODIMP
ScriptExecutorRunnable::Cancel()
{
  if (mLastIndex == mScriptLoader.mLoadInfos.Length() - 1) {
    ShutdownScriptLoader(mWorkerPrivate->GetJSContext(), mWorkerPrivate, false);
  }
  return MainThreadWorkerSyncRunnable::Cancel();
}

void
ScriptExecutorRunnable::ShutdownScriptLoader(JSContext* aCx,
                                             WorkerPrivate* aWorkerPrivate,
                                             bool aResult)
{
  MOZ_ASSERT(mLastIndex == mScriptLoader.mLoadInfos.Length() - 1);

  if (mIsWorkerScript && aWorkerPrivate->IsServiceWorker()) {
    aWorkerPrivate->SetLoadingWorkerScript(false);
  }

  aWorkerPrivate->RemoveFeature(aCx, &mScriptLoader);
  aWorkerPrivate->StopSyncLoop(mSyncLoopTarget, aResult);
}

bool
LoadAllScripts(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               nsTArray<ScriptLoadInfo>& aLoadInfos, bool aIsMainScript,
               WorkerScriptType aWorkerScriptType)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(!aLoadInfos.IsEmpty(), "Bad arguments!");

  AutoSyncLoopHolder syncLoop(aWorkerPrivate);

  nsRefPtr<ScriptLoaderRunnable> loader =
    new ScriptLoaderRunnable(aWorkerPrivate, syncLoop.EventTarget(),
                             aLoadInfos, aIsMainScript, aWorkerScriptType);

  NS_ASSERTION(aLoadInfos.IsEmpty(), "Should have swapped!");

  if (!aWorkerPrivate->AddFeature(aCx, loader)) {
    return false;
  }

  if (NS_FAILED(NS_DispatchToMainThread(loader))) {
    NS_ERROR("Failed to dispatch!");

    aWorkerPrivate->RemoveFeature(aCx, loader);
    return false;
  }

  return syncLoop.Run();
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
                               nsIChannel** aChannel)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIIOService> ios(do_GetIOService());

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  NS_ASSERTION(secMan, "This should never be null!");

  return ChannelFromScriptURL(aPrincipal, aBaseURI, aParentDoc, aLoadGroup,
                              ios, secMan, aScriptURL, true, WorkerScript, aChannel);
}

nsresult
ChannelFromScriptURLWorkerThread(JSContext* aCx,
                                 WorkerPrivate* aParent,
                                 const nsAString& aScriptURL,
                                 nsIChannel** aChannel)
{
  aParent->AssertIsOnWorkerThread();

  AutoSyncLoopHolder syncLoop(aParent);

  nsRefPtr<ChannelGetterRunnable> getter =
    new ChannelGetterRunnable(aParent, syncLoop.EventTarget(), aScriptURL,
                              aChannel);

  if (NS_FAILED(NS_DispatchToMainThread(getter))) {
    NS_ERROR("Failed to dispatch!");
    return NS_ERROR_FAILURE;
  }

  if (!syncLoop.Run()) {
    return NS_ERROR_FAILURE;
  }

  return getter->GetResult();
}

void ReportLoadError(JSContext* aCx, const nsAString& aURL,
                     nsresult aLoadResult, bool aIsMainThread)
{
  NS_LossyConvertUTF16toASCII url(aURL);

  switch (aLoadResult) {
    case NS_BINDING_ABORTED:
      // Canceled, don't set an exception.
      break;

    case NS_ERROR_MALFORMED_URI:
      JS_ReportError(aCx, "Malformed script URI: %s", url.get());
      break;

    case NS_ERROR_FILE_NOT_FOUND:
    case NS_ERROR_NOT_AVAILABLE:
      JS_ReportError(aCx, "Script file not found: %s", url.get());
      break;

    case NS_ERROR_DOM_SECURITY_ERR:
    case NS_ERROR_DOM_SYNTAX_ERR:
      Throw(aCx, aLoadResult);
      break;

    default:
      JS_ReportError(aCx, "Failed to load script (nsresult = 0x%x)", aLoadResult);
  }
}

bool
LoadMainScript(JSContext* aCx, const nsAString& aScriptURL,
               WorkerScriptType aWorkerScriptType)
{
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  NS_ASSERTION(worker, "This should never be null!");

  nsTArray<ScriptLoadInfo> loadInfos;

  ScriptLoadInfo* info = loadInfos.AppendElement();
  info->mURL = aScriptURL;

  return LoadAllScripts(aCx, worker, loadInfos, true, aWorkerScriptType);
}

void
Load(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
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
  }

  if (!LoadAllScripts(aCx, aWorkerPrivate, loadInfos, false, aWorkerScriptType)) {
    // LoadAllScripts can fail if we're shutting down.
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

} // namespace scriptloader

END_WORKERS_NAMESPACE
