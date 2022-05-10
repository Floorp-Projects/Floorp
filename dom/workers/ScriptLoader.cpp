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
#include "nsICookieJarSettings.h"
#include "nsIDocShell.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIIOService.h"
#include "nsIOService.h"
#include "nsIPrincipal.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamListenerTee.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIURI.h"
#include "nsIXPConnect.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CompilationAndEvaluation.h"
#include "js/Exception.h"
#include "js/SourceText.h"
#include "nsError.h"
#include "nsComponentManagerUtils.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsDocShellCID.h"
#include "nsJSEnvironment.h"
#include "nsNetUtil.h"
#include "nsIPipe.h"
#include "nsIOutputStream.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "xpcpublic.h"

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/ArrayAlgorithm.h"
#include "mozilla/Assertions.h"
#include "mozilla/LoadContext.h"
#include "mozilla/Maybe.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/dom/ClientChannelHelper.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/nsCSPService.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SerializedStackHolder.h"
#include "mozilla/dom/workerinternals/CacheLoadHandler.h"
#include "mozilla/dom/workerinternals/NetworkLoadHandler.h"
#include "mozilla/dom/workerinternals/ScriptResponseHeaderProcessor.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/UniquePtr.h"
#include "Principal.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

#define MAX_CONCURRENT_SCRIPTS 1000

using mozilla::ipc::PrincipalInfo;

namespace mozilla::dom::workerinternals {
namespace {

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

  uint32_t secFlags =
      aIsMainScript ? nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED
                    : nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT;

  bool inheritAttrs = nsContentUtils::ChannelShouldInheritPrincipal(
      principal, uri, true /* aInheritForAboutBlank */,
      false /* aForceInherit */);

  bool isData = uri->SchemeIs("data");
  if (inheritAttrs && !isData) {
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
    secFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL;
  }

  nsContentPolicyType contentPolicyType =
      aIsMainScript ? aMainScriptContentPolicyType
                    : nsIContentPolicy::TYPE_INTERNAL_WORKER_IMPORT_SCRIPTS;

  // The main service worker script should never be loaded over the network
  // in this path.  It should always be offlined by ServiceWorkerScriptCache.
  // We assert here since this error should also be caught by the runtime
  // check in CacheLoadHandler.
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

void LoadAllScripts(WorkerPrivate* aWorkerPrivate,
                    UniquePtr<SerializedStackHolder> aOriginStack,
                    const nsTArray<nsString>& aScriptURLs, bool aIsMainScript,
                    WorkerScriptType aWorkerScriptType, ErrorResult& aRv) {
  aWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(!aScriptURLs.IsEmpty(), "Bad arguments!");

  AutoSyncLoopHolder syncLoop(aWorkerPrivate, Canceling);
  nsCOMPtr<nsIEventTarget> syncLoopTarget = syncLoop.GetEventTarget();
  if (!syncLoopTarget) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  Maybe<ClientInfo> clientInfo;
  Maybe<ServiceWorkerDescriptor> controller;
  nsIGlobalObject* global =
      aWorkerScriptType == WorkerScript
          ? static_cast<nsIGlobalObject*>(aWorkerPrivate->GlobalScope())
          : aWorkerPrivate->DebuggerGlobalScope();

  clientInfo = global->GetClientInfo();
  controller = global->GetController();

  nsTArray<ScriptLoadInfo> aLoadInfos =
      TransformIntoNewArray(aScriptURLs, [](const auto& scriptURL) {
        ScriptLoadInfo res;
        res.mURL = scriptURL;
        return res;
      });

  RefPtr<loader::WorkerScriptLoader> loader = new loader::WorkerScriptLoader(
      aWorkerPrivate, std::move(aOriginStack), syncLoopTarget,
      std::move(aLoadInfos), clientInfo, controller, aIsMainScript,
      aWorkerScriptType, aRv);

  NS_ASSERTION(aLoadInfos.IsEmpty(), "Should have swapped!");

  RefPtr<StrongWorkerRef> workerRef =
      StrongWorkerRef::Create(aWorkerPrivate, "ScriptLoader", [loader]() {
        NS_DispatchToMainThread(NewRunnableMethod(
            "WorkerScriptLoader::CancelMainThreadWithBindingAborted", loader,
            &loader::WorkerScriptLoader::CancelMainThreadWithBindingAborted));
      });

  if (NS_WARN_IF(!workerRef)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (loader->DispatchLoadScripts()) {
    syncLoop.Run();
  }
}

class ChannelGetterRunnable final : public WorkerMainThreadRunnable {
  const nsAString& mScriptURL;
  const ClientInfo mClientInfo;
  WorkerLoadInfo& mLoadInfo;
  nsresult mResult;

 public:
  ChannelGetterRunnable(WorkerPrivate* aParentWorker,
                        const nsAString& aScriptURL, WorkerLoadInfo& aLoadInfo)
      : WorkerMainThreadRunnable(aParentWorker,
                                 "ScriptLoader :: ChannelGetter"_ns),
        mScriptURL(aScriptURL)
        // ClientInfo should always be present since this should not be called
        // if parent's status is greater than Running.
        ,
        mClientInfo(aParentWorker->GlobalScope()->GetClientInfo().ref()),
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

}  //  anonymous namespace

namespace loader {

class ScriptLoaderRunnable final : public nsIRunnable, public nsINamed {
  RefPtr<WorkerScriptLoader> mScriptLoader;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit ScriptLoaderRunnable(WorkerScriptLoader* aScriptLoader)
      : mScriptLoader(aScriptLoader) {
    MOZ_ASSERT(aScriptLoader);
  }

 private:
  ~ScriptLoaderRunnable() = default;

  NS_IMETHOD
  Run() override {
    AssertIsOnMainThread();

    nsresult rv = mScriptLoader->LoadScripts();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mScriptLoader->CancelMainThread(rv);
    }

    return NS_OK;
  }

  NS_IMETHOD
  GetName(nsACString& aName) override {
    aName.AssignLiteral("ScriptLoaderRunnable");
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(ScriptLoaderRunnable, nsIRunnable, nsINamed)

class ScriptExecutorRunnable final : public MainThreadWorkerSyncRunnable {
  WorkerScriptLoader& mScriptLoader;
  const Span<ScriptLoadInfo> mLoadInfosToExecute;
  const bool mAllScriptsExecutable;

 public:
  ScriptExecutorRunnable(WorkerScriptLoader& aScriptLoader,
                         nsIEventTarget* aSyncLoopTarget,
                         Span<ScriptLoadInfo> aLoadInfosToExecute,
                         bool aAllScriptsExecutable);

 private:
  ~ScriptExecutorRunnable() = default;

  virtual bool IsDebuggerRunnable() const override;

  virtual bool PreRun(WorkerPrivate* aWorkerPrivate) override;

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override;

  virtual void PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                       bool aRunResult) override;

  nsresult Cancel() override;

  bool AllScriptsExecutable() const;
};

template <typename Unit>
static bool EvaluateSourceBuffer(JSContext* aCx,
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

WorkerScriptLoader::WorkerScriptLoader(
    WorkerPrivate* aWorkerPrivate,
    UniquePtr<SerializedStackHolder> aOriginStack,
    nsIEventTarget* aSyncLoopTarget, nsTArray<ScriptLoadInfo> aLoadInfos,
    const Maybe<ClientInfo>& aClientInfo,
    const Maybe<ServiceWorkerDescriptor>& aController, bool aIsMainScript,
    WorkerScriptType aWorkerScriptType, ErrorResult& aRv)
    : mWorkerPrivate(aWorkerPrivate),
      mOriginStack(std::move(aOriginStack)),
      mSyncLoopTarget(aSyncLoopTarget),
      mLoadInfos(std::move(aLoadInfos)),
      mClientInfo(aClientInfo),
      mController(aController),
      mIsMainScript(aIsMainScript),
      mWorkerScriptType(aWorkerScriptType),
      mCanceledMainThread(false),
      mRv(aRv) {
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aSyncLoopTarget);
  MOZ_ASSERT_IF(aIsMainScript, mLoadInfos.Length() == 1);
}

bool WorkerScriptLoader::DispatchLoadScripts() {
  RefPtr<ScriptLoaderRunnable> runnable = new ScriptLoaderRunnable(this);

  if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
    NS_ERROR("Failed to dispatch!");
    mRv.Throw(NS_ERROR_FAILURE);
    return false;
  }
  return true;
}

nsIURI* WorkerScriptLoader::GetBaseURI() {
  MOZ_ASSERT(mWorkerPrivate);
  nsIURI* baseURI;
  WorkerPrivate* parentWorker = mWorkerPrivate->GetParent();
  if (mIsMainScript) {
    if (parentWorker) {
      baseURI = parentWorker->GetBaseURI();
      NS_ASSERTION(baseURI, "Should have been set already!");
    } else {
      // May be null.
      baseURI = mWorkerPrivate->GetBaseURI();
    }
  } else {
    baseURI = mWorkerPrivate->GetBaseURI();
    NS_ASSERTION(baseURI, "Should have been set already!");
  }

  return baseURI;
}

void WorkerScriptLoader::LoadingFinished(ScriptLoadInfo& aLoadInfo,
                                         nsresult aRv) {
  AssertIsOnMainThread();

  aLoadInfo.mLoadResult = aRv;

  MOZ_ASSERT(!aLoadInfo.mLoadingFinished);
  aLoadInfo.mLoadingFinished = true;

  if (IsMainWorkerScript() && NS_SUCCEEDED(aRv)) {
    MOZ_DIAGNOSTIC_ASSERT(mWorkerPrivate->PrincipalURIMatchesScriptURL());
  }

  MaybeExecuteFinishedScripts(aLoadInfo);
}

void WorkerScriptLoader::MaybeExecuteFinishedScripts(
    const ScriptLoadInfo& aLoadInfo) {
  AssertIsOnMainThread();

  // We execute the last step if we don't have a pending operation with the
  // cache and the loading is completed.
  if (aLoadInfo.Finished()) {
    DispatchProcessPendingRequests();
  }
}

bool WorkerScriptLoader::StoreCSP() {
  // We must be on the same worker as we started on.
  mWorkerPrivate->AssertIsOnWorkerThread();
  if (!IsMainWorkerScript()) {
    return true;
  }

  if (!mWorkerPrivate->GetJSContext()) {
    return false;
  }

  MOZ_ASSERT(!mRv.Failed());

  // Move the CSP from the workerLoadInfo in the corresponding Client
  // where the CSP code expects it!
  mWorkerPrivate->StoreCSPOnClient();
  return true;
}

bool WorkerScriptLoader::ProcessPendingRequests(
    JSContext* aCx, const Span<ScriptLoadInfo> aLoadInfosToExecute) {
  mWorkerPrivate->AssertIsOnWorkerThread();
  // Don't run if something else has already failed.
  if (mExecutionAborted) {
    return true;
  }

  // If nothing else has failed, our ErrorResult better not be a failure
  // either.
  MOZ_ASSERT(!mRv.Failed(), "Who failed it and why?");

  // Slightly icky action at a distance, but there's no better place to stash
  // this value, really.
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  MOZ_ASSERT(global);

  for (ScriptLoadInfo& loadInfo : aLoadInfosToExecute) {
    // We don't have a ProcessRequest method (like we do on the DOM), as there
    // isn't much processing that we need to do per request that isn't related
    // to evaluation (the processsing done for the DOM is handled in
    // DataRecievedFrom{Cache,Network} for workers.
    // So, this inner loop calls EvaluateScript directly. This will change
    // once modules are introduced as we will have some extra work to do.
    if (mExecutionAborted) {
      break;
    }
    if (!EvaluateScript(aCx, loadInfo)) {
      mExecutionAborted = true;
      mMutedErrorFlag = loadInfo.mMutedErrorFlag.valueOr(true);
    }
  }

  return true;
}

nsresult WorkerScriptLoader::OnStreamComplete(ScriptLoadInfo& aLoadInfo,
                                              nsresult aStatus) {
  AssertIsOnMainThread();

  LoadingFinished(aLoadInfo, aStatus);
  return NS_OK;
}

void WorkerScriptLoader::CancelMainThread(nsresult aCancelResult) {
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
  for (ScriptLoadInfo& loadInfo : mLoadInfos) {
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
      LoadingFinished(loadInfo, aCancelResult);
    }
  }

  DispatchProcessPendingRequests();
}

void WorkerScriptLoader::DeleteCache() {
  AssertIsOnMainThread();

  if (!mCacheCreator) {
    return;
  }

  mCacheCreator->DeleteCache();
  mCacheCreator = nullptr;
}

nsresult WorkerScriptLoader::LoadScripts() {
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
    for (ScriptLoadInfo& loadInfo : mLoadInfos) {
      nsresult rv = LoadScript(loadInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        LoadingFinished(loadInfo, rv);
        return rv;
      }
    }

    return NS_OK;
  }

  MOZ_ASSERT(!mCacheCreator);
  mCacheCreator = new CacheCreator(mWorkerPrivate);

  for (ScriptLoadInfo& loadInfo : mLoadInfos) {
    mCacheCreator->AddLoader(MakeNotNull<RefPtr<CacheLoadHandler>>(
        mWorkerPrivate, loadInfo, IsMainWorkerScript(), this));
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

nsresult WorkerScriptLoader::LoadScript(ScriptLoadInfo& aLoadInfo) {
  AssertIsOnMainThread();
  MOZ_ASSERT_IF(IsMainWorkerScript(), !IsDebuggerScript());

  WorkerPrivate* parentWorker = mWorkerPrivate->GetParent();

  // For JavaScript debugging, the devtools server must run on the same
  // thread as the debuggee, indicating the worker uses content principal.
  // However, in Bug 863246, web content will no longer be able to load
  // resource:// URIs by default, so we need system principal to load
  // debugger scripts.
  nsIPrincipal* principal = (IsDebuggerScript())
                                ? nsContentUtils::GetSystemPrincipal()
                                : mWorkerPrivate->GetPrincipal();

  nsCOMPtr<nsILoadGroup> loadGroup = mWorkerPrivate->GetLoadGroup();
  MOZ_DIAGNOSTIC_ASSERT(principal);

  NS_ENSURE_TRUE(NS_LoadGroupMatchesPrincipal(loadGroup, principal),
                 NS_ERROR_FAILURE);

  // Figure out our base URI.
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

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

  nsresult& rv = aLoadInfo.mLoadResult;

  nsLoadFlags loadFlags = mWorkerPrivate->GetLoadFlags();

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
    rv = ConstructURI(aLoadInfo.mURL, baseURI, parentDoc, useDefaultEncoding,
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
                              mWorkerPrivate->CookieJarSettings(), referrerInfo,
                              getter_AddRefs(channel));
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
  RefPtr<NetworkLoadHandler> listener = new NetworkLoadHandler(this, aLoadInfo);

  RefPtr<ScriptResponseHeaderProcessor> headerProcessor = nullptr;

  // For each debugger script, a non-debugger script load of the same script
  // should have occured prior that processed the headers.
  if (!IsDebuggerScript()) {
    headerProcessor = MakeRefPtr<ScriptResponseHeaderProcessor>(mWorkerPrivate,
                                                                mIsMainScript);
  }

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), listener, headerProcessor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (IsMainWorkerScript()) {
    MOZ_DIAGNOSTIC_ASSERT(mClientInfo.isSome());

    // In order to get the correct foreign partitioned prinicpal, we need to
    // set the `IsThirdPartyContextToTopWindow` to the channel's loadInfo.
    // This flag reflects the fact that if the worker is created under a
    // third-party context.
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
    loadInfo->SetIsThirdPartyContextToTopWindow(
        mWorkerPrivate->IsThirdPartyContextToTopWindow());

    rv = AddClientChannelHelper(channel, std::move(mClientInfo),
                                Maybe<ClientInfo>(),
                                mWorkerPrivate->HybridEventTarget());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (StaticPrefs::browser_tabs_remote_useCrossOriginEmbedderPolicy()) {
    nsILoadInfo::CrossOriginEmbedderPolicy respectedCOEP =
        mWorkerPrivate->GetEmbedderPolicy();
    if (mWorkerPrivate->IsDedicatedWorker() &&
        respectedCOEP == nsILoadInfo::EMBEDDER_POLICY_NULL) {
      respectedCOEP = mWorkerPrivate->GetOwnerEmbedderPolicy();
    }

    nsCOMPtr<nsILoadInfo> channelLoadInfo = channel->LoadInfo();
    channelLoadInfo->SetLoadingEmbedderPolicy(respectedCOEP);
  }

  if (aLoadInfo.mCacheStatus != ScriptLoadInfo::ToBeCached) {
    rv = channel->AsyncOpen(loader);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    nsCOMPtr<nsIOutputStream> writer;

    // In case we return early.
    aLoadInfo.mCacheStatus = ScriptLoadInfo::Cancel;

    rv = NS_NewPipe(
        getter_AddRefs(aLoadInfo.mCacheReadStream), getter_AddRefs(writer), 0,
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

  aLoadInfo.mChannel.swap(channel);

  return NS_OK;
}

void WorkerScriptLoader::DispatchProcessPendingRequests() {
  AssertIsOnMainThread();

  if (IsMainWorkerScript()) {
    mWorkerPrivate->WorkerScriptLoaded();
  }

  const auto begin = mLoadInfos.begin();
  const auto end = mLoadInfos.end();
  using Iterator = decltype(begin);
  const auto maybeRangeToExecute =
      [begin, end]() -> Maybe<std::pair<Iterator, Iterator>> {
    // firstItToExecute is the first loadInfo where mExecutionScheduled is
    // unset.
    auto firstItToExecute =
        std::find_if(begin, end, [](const ScriptLoadInfo& loadInfo) {
          return !loadInfo.mExecutionScheduled;
        });

    if (firstItToExecute == end) {
      return Nothing();
    }

    // firstItUnexecutable is the first loadInfo that is not yet finished.
    // Update mExecutionScheduled on the ones we're about to schedule for
    // execution.
    const auto firstItUnexecutable =
        std::find_if(firstItToExecute, end, [](ScriptLoadInfo& loadInfo) {
          if (!loadInfo.Finished()) {
            return true;
          }

          // We can execute this one.
          loadInfo.mExecutionScheduled = true;

          return false;
        });

    return firstItUnexecutable == firstItToExecute
               ? Nothing()
               : Some(std::pair(firstItToExecute, firstItUnexecutable));
  }();

  // If there are no unexecutable load infos, we can unuse things before the
  // execution of the scripts and the stopping of the sync loop.
  if (maybeRangeToExecute) {
    if (maybeRangeToExecute->second == end) {
      mCacheCreator = nullptr;
    }

    RefPtr<ScriptExecutorRunnable> runnable = new ScriptExecutorRunnable(
        *this, mSyncLoopTarget,
        Span{maybeRangeToExecute->first, maybeRangeToExecute->second},
        /* aAllScriptsExecutable = */ end == maybeRangeToExecute->second);
    if (!runnable->Dispatch()) {
      MOZ_ASSERT(false, "This should never fail!");
    }
  }
}

bool WorkerScriptLoader::EvaluateScript(JSContext* aCx,
                                        ScriptLoadInfo& aLoadInfo) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  NS_ASSERTION(!aLoadInfo.mChannel, "Should no longer have a channel!");
  NS_ASSERTION(aLoadInfo.mExecutionScheduled, "Should be scheduled!");
  NS_ASSERTION(!aLoadInfo.mExecutionResult, "Should not have executed yet!");

  MOZ_ASSERT(!mRv.Failed(), "Who failed it and why?");
  mRv.MightThrowJSException();
  if (NS_FAILED(aLoadInfo.mLoadResult)) {
    workerinternals::ReportLoadError(mRv, aLoadInfo.mLoadResult,
                                     aLoadInfo.mURL);
    return false;
  }

  // If this is a top level script that succeeded, then mark the
  // Client execution ready and possible controlled by a service worker.
  if (IsMainWorkerScript()) {
    if (mController.isSome()) {
      MOZ_ASSERT(mWorkerScriptType == WorkerScript,
                 "Debugger clients can't be controlled.");
      mWorkerPrivate->GlobalScope()->Control(mController.ref());
    }
    mWorkerPrivate->ExecutionReady();
  }

  NS_ConvertUTF16toUTF8 filename(aLoadInfo.mURL);

  JS::CompileOptions options(aCx);
  options.setFileAndLine(filename.get(), 1).setNoScriptRval(true);

  MOZ_ASSERT(aLoadInfo.mMutedErrorFlag.isSome());
  options.setMutedErrors(aLoadInfo.mMutedErrorFlag.valueOr(true));

  if (aLoadInfo.mSourceMapURL) {
    options.setSourceMapURL(aLoadInfo.mSourceMapURL->get());
  }

  // Our ErrorResult still shouldn't be a failure.
  MOZ_ASSERT(!mRv.Failed(), "Who failed it and why?");

  // Transfer script length to a local variable, encoding-agnostically.
  size_t scriptLength = 0;
  std::swap(scriptLength, aLoadInfo.mScriptLength);

  // This transfers script data out of the active arm of |aLoadInfo.mScript|.
  bool successfullyEvaluated =
      aLoadInfo.mScriptIsUTF8
          ? EvaluateSourceBuffer(aCx, options, aLoadInfo.mScript.mUTF8,
                                 scriptLength)
          : EvaluateSourceBuffer(aCx, options, aLoadInfo.mScript.mUTF16,
                                 scriptLength);
  MOZ_ASSERT(aLoadInfo.ScriptTextIsNull());
  if (!successfullyEvaluated) {
    mRv.StealExceptionFromJSContext(aCx);
    return false;
  }
  aLoadInfo.mExecutionResult = true;
  return true;
}

void WorkerScriptLoader::ShutdownScriptLoader(JSContext* aCx,
                                              WorkerPrivate* aWorkerPrivate,
                                              bool aResult, bool aMutedError) {
  aWorkerPrivate->AssertIsOnWorkerThread();

  // TODO: replace this call MOZ_ASSERT(AllScriptsExecutable());

  if (IsMainWorkerScript()) {
    aWorkerPrivate->SetLoadingWorkerScript(false);
  }

  if (!aResult) {
    // At this point there are two possibilities:
    //
    // 1) mRv.Failed().  In that case we just want to leave it
    //    as-is, except if it has a JS exception and we need to mute JS
    //    exceptions.  In that case, we log the exception without firing any
    //    events and then replace it on the ErrorResult with a NetworkError,
    //    per spec.
    //
    // 2) mRv succeeded.  As far as I can tell, this can only
    //    happen when loading the main worker script and
    //    GetOrCreateGlobalScope() fails or if ScriptExecutorRunnable::Cancel
    //    got called.  Does it matter what we throw in this case?  I'm not
    //    sure...
    if (mRv.Failed()) {
      if (aMutedError && mRv.IsJSException()) {
        LogExceptionToConsole(aCx, aWorkerPrivate);
        mRv.Throw(NS_ERROR_DOM_NETWORK_ERR);
      }
    } else {
      mRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    }
  }

  aWorkerPrivate->StopSyncLoop(mSyncLoopTarget, aResult);
}

void WorkerScriptLoader::LogExceptionToConsole(JSContext* aCx,
                                               WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();

  MOZ_ASSERT(mRv.IsJSException());

  JS::Rooted<JS::Value> exn(aCx);
  if (!ToJSValue(aCx, std::move(mRv), &exn)) {
    return;
  }

  // Now the exception state should all be in exn.
  MOZ_ASSERT(!JS_IsExceptionPending(aCx));
  MOZ_ASSERT(!mRv.Failed());

  JS::ExceptionStack exnStack(aCx, exn, nullptr);
  JS::ErrorReportBuilder report(aCx);
  if (!report.init(aCx, exnStack, JS::ErrorReportBuilder::WithSideEffects)) {
    JS_ClearPendingException(aCx);
    return;
  }

  RefPtr<xpc::ErrorReport> xpcReport = new xpc::ErrorReport();
  xpcReport->Init(report.report(), report.toStringResult().c_str(),
                  aWorkerPrivate->IsChromeWorker(), aWorkerPrivate->WindowID());

  RefPtr<AsyncErrorReporter> r = new AsyncErrorReporter(xpcReport);
  NS_DispatchToMainThread(r);
}

NS_IMPL_ISUPPORTS(WorkerScriptLoader, nsINamed)

ScriptExecutorRunnable::ScriptExecutorRunnable(
    WorkerScriptLoader& aScriptLoader, nsIEventTarget* aSyncLoopTarget,
    Span<ScriptLoadInfo> aLoadInfosToExecute, bool aAllScriptsExecutable)
    : MainThreadWorkerSyncRunnable(aScriptLoader.mWorkerPrivate,
                                   aSyncLoopTarget),
      mScriptLoader(aScriptLoader),
      mLoadInfosToExecute(aLoadInfosToExecute),
      mAllScriptsExecutable(aAllScriptsExecutable) {}

bool ScriptExecutorRunnable::IsDebuggerRunnable() const {
  // ScriptExecutorRunnable is used to execute both worker and debugger scripts.
  // In the latter case, the runnable needs to be dispatched to the debugger
  // queue.
  return mScriptLoader.IsDebuggerScript();
}

bool ScriptExecutorRunnable::PreRun(WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();

  // We must be on the same worker as we started on.
  MOZ_ASSERT(
      mScriptLoader.mSyncLoopTarget == mSyncLoopTarget,
      "Unexpected SyncLoopTarget. Check if the sync loop was closed early");

  return mScriptLoader.StoreCSP();
}

bool ScriptExecutorRunnable::WorkerRun(JSContext* aCx,
                                       WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();

  // We must be on the same worker as we started on.
  MOZ_ASSERT(
      mScriptLoader.mSyncLoopTarget == mSyncLoopTarget,
      "Unexpected SyncLoopTarget. Check if the sync loop was closed early");

  return mScriptLoader.ProcessPendingRequests(aCx, mLoadInfosToExecute);
}

void ScriptExecutorRunnable::PostRun(JSContext* aCx,
                                     WorkerPrivate* aWorkerPrivate,
                                     bool aRunResult) {
  aWorkerPrivate->AssertIsOnWorkerThread();

  // We must be on the same worker as we started on.
  MOZ_ASSERT(
      mScriptLoader.mSyncLoopTarget == mSyncLoopTarget,
      "Unexpected SyncLoopTarget. Check if the sync loop was closed early");

  MOZ_ASSERT(!JS_IsExceptionPending(aCx), "Who left an exception on there?");

  if (AllScriptsExecutable()) {
    // The only way we can get here with an aborted execution but without
    // mScriptLoader.mRv being a failure is if we're loading the main worker
    // script and GetOrCreateGlobalScope() fails.  In that case we would have
    // returned false from WorkerRun, so assert that.
    MOZ_ASSERT_IF(
        mScriptLoader.mExecutionAborted && !mScriptLoader.mRv.Failed(),
        !aRunResult);
    // All done.
    mScriptLoader.ShutdownScriptLoader(aCx, aWorkerPrivate,
                                       !mScriptLoader.mExecutionAborted,
                                       mScriptLoader.mMutedErrorFlag);
  }
}

nsresult ScriptExecutorRunnable::Cancel() {
  // We need to check first if cancel is called twice
  nsresult rv = MainThreadWorkerSyncRunnable::Cancel();
  NS_ENSURE_SUCCESS(rv, rv);

  if (AllScriptsExecutable()) {
    mScriptLoader.ShutdownScriptLoader(mWorkerPrivate->GetJSContext(),
                                       mWorkerPrivate, false, false);
  }
  return NS_OK;
}

bool ScriptExecutorRunnable::AllScriptsExecutable() const {
  return mAllScriptsExecutable;
}

} /* namespace loader */

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
    case NS_ERROR_CORRUPTED_CONTENT:
      aRv.Throw(NS_ERROR_DOM_NETWORK_ERR);
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
  nsTArray<nsString> scriptURLs;

  scriptURLs.AppendElement(aScriptURL);

  LoadAllScripts(aWorkerPrivate, std::move(aOriginStack), scriptURLs, true,
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

  LoadAllScripts(aWorkerPrivate, std::move(aOriginStack), aScriptURLs, false,
                 aWorkerScriptType, aRv);
}

}  // namespace mozilla::dom::workerinternals
