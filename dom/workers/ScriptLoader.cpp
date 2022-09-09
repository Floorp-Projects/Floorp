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
#include "mozilla/Encoding.h"
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

using JS::loader::ScriptKind;
using JS::loader::ScriptLoadRequest;
using mozilla::ipc::PrincipalInfo;

namespace mozilla::dom::workerinternals {
namespace {

nsresult ConstructURI(const nsAString& aScriptURL, nsIURI* baseURI,
                      const mozilla::Encoding* aDocumentEncoding,
                      nsIURI** aResult) {
  nsresult rv;
  // Only top level workers' main script use the document charset for the
  // script uri encoding. Otherwise, default encoding (UTF-8) is applied.
  if (aDocumentEncoding) {
    nsAutoCString charset;
    aDocumentEncoding->Name(charset);
    rv = NS_NewURI(aResult, aScriptURL, charset.get(), baseURI);
  } else {
    rv = NS_NewURI(aResult, aScriptURL, nullptr, baseURI);
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
                    WorkerScriptType aWorkerScriptType, ErrorResult& aRv,
                    const mozilla::Encoding* aDocumentEncoding = nullptr) {
  aWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(!aScriptURLs.IsEmpty(), "Bad arguments!");

  AutoSyncLoopHolder syncLoop(aWorkerPrivate, Canceling);
  nsCOMPtr<nsIEventTarget> syncLoopTarget = syncLoop.GetEventTarget();
  if (!syncLoopTarget) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  RefPtr<loader::WorkerScriptLoader> loader =
      new loader::WorkerScriptLoader(aWorkerPrivate, std::move(aOriginStack),
                                     syncLoopTarget, aWorkerScriptType, aRv);

  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  loader->CreateScriptRequests(aScriptURLs, aDocumentEncoding, aIsMainScript);

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
    mResult = ConstructURI(mScriptURL, baseURI, nullptr, getter_AddRefs(url));
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

class ScriptExecutorRunnable final : public MainThreadWorkerSyncRunnable {
  WorkerScriptLoader& mScriptLoader;
  ScriptLoadRequest* mRequest;

 public:
  ScriptExecutorRunnable(WorkerScriptLoader& aScriptLoader,
                         nsIEventTarget* aSyncLoopTarget,
                         ScriptLoadRequest* aRequest);

 private:
  ~ScriptExecutorRunnable() = default;

  virtual bool IsDebuggerRunnable() const override;

  virtual bool PreRun(WorkerPrivate* aWorkerPrivate) override;

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override;

  nsresult Cancel() override;
};

template <typename Unit>
static bool EvaluateSourceBuffer(JSContext* aCx,
                                 const JS::CompileOptions& aOptions,
                                 JS::SourceText<Unit>& aSourceBuffer) {
  static_assert(std::is_same<Unit, char16_t>::value ||
                    std::is_same<Unit, Utf8Unit>::value,
                "inferred units must be UTF-8 or UTF-16");

  JS::Rooted<JS::Value> unused(aCx);
  return Evaluate(aCx, aOptions, aSourceBuffer, &unused);
}

WorkerScriptLoader::WorkerScriptLoader(
    WorkerPrivate* aWorkerPrivate,
    UniquePtr<SerializedStackHolder> aOriginStack,
    nsIEventTarget* aSyncLoopTarget, WorkerScriptType aWorkerScriptType,
    ErrorResult& aRv)
    : mOriginStack(std::move(aOriginStack)),
      mSyncLoopTarget(aSyncLoopTarget),
      mWorkerScriptType(aWorkerScriptType),
      mCancelMainThread(Nothing()),
      mRv(aRv),
      mCleanedUp(false),
      mCleanUpLock("cleanUpLock") {
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aSyncLoopTarget);

  RefPtr<WorkerScriptLoader> self = this;

  RefPtr<StrongWorkerRef> workerRef =
      StrongWorkerRef::Create(aWorkerPrivate, "ScriptLoader", [self]() {
        nsTArray<WorkerLoadContext*> scriptLoadList = self->GetLoadingList();
        NS_DispatchToMainThread(
            NewRunnableMethod<nsTArray<WorkerLoadContext*>&&>(
                "WorkerScriptLoader::CancelMainThreadWithBindingAborted", self,
                &WorkerScriptLoader::CancelMainThreadWithBindingAborted,
                std::move(scriptLoadList)));
      });

  if (workerRef) {
    mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  } else {
    mRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIGlobalObject* global = GetGlobal();

  mController = global->GetController();
}

void WorkerScriptLoader::CreateScriptRequests(
    const nsTArray<nsString>& aScriptURLs,
    const mozilla::Encoding* aDocumentEncoding, bool aIsMainScript) {
  for (const nsString& scriptURL : aScriptURLs) {
    RefPtr<ScriptLoadRequest> request =
        CreateScriptLoadRequest(scriptURL, aDocumentEncoding, aIsMainScript);
    mLoadingRequests.AppendElement(request);
  }
}

nsTArray<WorkerLoadContext*> WorkerScriptLoader::GetLoadingList() {
  nsTArray<WorkerLoadContext*> list;
  for (ScriptLoadRequest* req = mLoadingRequests.getFirst(); req;
       req = req->getNext()) {
    list.AppendElement(req->GetWorkerLoadContext());
  }
  return list;
}

already_AddRefed<ScriptLoadRequest> WorkerScriptLoader::CreateScriptLoadRequest(
    const nsString& aScriptURL, const mozilla::Encoding* aDocumentEncoding,
    bool aIsMainScript) {
  WorkerLoadContext::Kind kind =
      WorkerLoadContext::GetKind(aIsMainScript, IsDebuggerScript());

  Maybe<ClientInfo> clientInfo = GetGlobal()->GetClientInfo();

  RefPtr<WorkerLoadContext> loadContext =
      new WorkerLoadContext(kind, clientInfo);

  // Create ScriptLoadRequests for this WorkerScriptLoader
  ReferrerPolicy referrerPolicy = mWorkerRef->Private()->GetReferrerPolicy();

  // Only top level workers' main script use the document charset for the
  // script uri encoding. Otherwise, default encoding (UTF-8) is applied.
  MOZ_ASSERT_IF(bool(aDocumentEncoding),
                aIsMainScript && !mWorkerRef->Private()->GetParent());
  nsCOMPtr<nsIURI> baseURI = aIsMainScript ? GetInitialBaseURI() : GetBaseURI();
  nsCOMPtr<nsIURI> uri;
  nsresult rv =
      ConstructURI(aScriptURL, baseURI, aDocumentEncoding, getter_AddRefs(uri));
  // If we failed to construct the URI, handle it in the LoadContext so it is
  // thrown in the right order.
  if (NS_WARN_IF(NS_FAILED(rv))) {
    loadContext->mLoadResult = rv;
  }

  RefPtr<ScriptFetchOptions> fetchOptions =
      new ScriptFetchOptions(CORSMode::CORS_NONE, referrerPolicy, nullptr);

  RefPtr<ScriptLoadRequest> request =
      new ScriptLoadRequest(ScriptKind::eClassic, uri, fetchOptions,
                            SRIMetadata(), nullptr, /* = aReferrer */
                            loadContext);

  // Set the mURL, it will be used for error handling and debugging.
  request->mURL = NS_ConvertUTF16toUTF8(aScriptURL);

  return request.forget();
}

bool WorkerScriptLoader::DispatchLoadScript(ScriptLoadRequest* aRequest) {
  mWorkerRef->Private()->AssertIsOnWorkerThread();

  nsTArray<WorkerLoadContext*> scriptLoadList;
  scriptLoadList.AppendElement(aRequest->GetWorkerLoadContext());

  nsresult rv =
      NS_DispatchToMainThread(NewRunnableMethod<nsTArray<WorkerLoadContext*>&&>(
          "WorkerScriptLoader::LoadScripts", this,
          &WorkerScriptLoader::LoadScripts, std::move(scriptLoadList)));

  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to dispatch!");
    mRv.Throw(NS_ERROR_FAILURE);
    return false;
  }
  return true;
}

bool WorkerScriptLoader::DispatchLoadScripts() {
  mWorkerRef->Private()->AssertIsOnWorkerThread();

  nsTArray<WorkerLoadContext*> scriptLoadList = GetLoadingList();

  nsresult rv =
      NS_DispatchToMainThread(NewRunnableMethod<nsTArray<WorkerLoadContext*>&&>(
          "WorkerScriptLoader::LoadScripts", this,
          &WorkerScriptLoader::LoadScripts, std::move(scriptLoadList)));

  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to dispatch!");
    mRv.Throw(NS_ERROR_FAILURE);
    return false;
  }
  return true;
}

nsIURI* WorkerScriptLoader::GetInitialBaseURI() {
  MOZ_ASSERT(mWorkerRef->Private());
  nsIURI* baseURI;
  WorkerPrivate* parentWorker = mWorkerRef->Private()->GetParent();
  if (parentWorker) {
    baseURI = parentWorker->GetBaseURI();
  } else {
    // May be null.
    baseURI = mWorkerRef->Private()->GetBaseURI();
  }

  return baseURI;
}

nsIURI* WorkerScriptLoader::GetBaseURI() {
  MOZ_ASSERT(mWorkerRef);
  nsIURI* baseURI;
  baseURI = mWorkerRef->Private()->GetBaseURI();
  NS_ASSERTION(baseURI, "Should have been set already!");

  return baseURI;
}

nsIGlobalObject* WorkerScriptLoader::GetGlobal() {
  mWorkerRef->Private()->AssertIsOnWorkerThread();
  return mWorkerScriptType == WorkerScript
             ? static_cast<nsIGlobalObject*>(
                   mWorkerRef->Private()->GlobalScope())
             : mWorkerRef->Private()->DebuggerGlobalScope();
}

void WorkerScriptLoader::LoadingFinished(ScriptLoadRequest* aRequest,
                                         nsresult aRv) {
  AssertIsOnMainThread();

  WorkerLoadContext* loadContext = aRequest->GetWorkerLoadContext();

  loadContext->mLoadResult = aRv;
  MOZ_ASSERT(!loadContext->mLoadingFinished);
  loadContext->mLoadingFinished = true;

  if (loadContext->IsTopLevel() && NS_SUCCEEDED(aRv)) {
    MOZ_DIAGNOSTIC_ASSERT(
        mWorkerRef->Private()->PrincipalURIMatchesScriptURL());
  }

  MaybeExecuteFinishedScripts(aRequest);
}

void WorkerScriptLoader::MaybeExecuteFinishedScripts(
    ScriptLoadRequest* aRequest) {
  AssertIsOnMainThread();

  // We execute the last step if we don't have a pending operation with the
  // cache and the loading is completed.
  WorkerLoadContext* loadContext = aRequest->GetWorkerLoadContext();
  if (!loadContext->IsAwaitingPromise()) {
    loadContext->ClearCacheCreator();
    DispatchMaybeMoveToLoadedList(aRequest);
  }
}

void WorkerScriptLoader::MaybeMoveToLoadedList(ScriptLoadRequest* aRequest) {
  mWorkerRef->Private()->AssertIsOnWorkerThread();
  aRequest->SetReady();

  while (!mLoadingRequests.isEmpty()) {
    ScriptLoadRequest* request = mLoadingRequests.getFirst();
    if (!request->IsReadyToRun()) {
      break;
    }

    RefPtr<ScriptLoadRequest> req = mLoadingRequests.Steal(request);
    mLoadedRequests.AppendElement(req);
  }
}

bool WorkerScriptLoader::StoreCSP() {
  // We must be on the same worker as we started on.
  mWorkerRef->Private()->AssertIsOnWorkerThread();

  if (!mWorkerRef->Private()->GetJSContext()) {
    return false;
  }

  MOZ_ASSERT(!mRv.Failed());

  // Move the CSP from the workerLoadInfo in the corresponding Client
  // where the CSP code expects it!
  mWorkerRef->Private()->StoreCSPOnClient();
  return true;
}

bool WorkerScriptLoader::ProcessPendingRequests(JSContext* aCx) {
  mWorkerRef->Private()->AssertIsOnWorkerThread();
  // Don't run if something else has already failed.
  if (mExecutionAborted) {
    mLoadedRequests.CancelRequestsAndClear();
    TryShutdown();
    return true;
  }

  // If nothing else has failed, our ErrorResult better not be a failure
  // either.
  MOZ_ASSERT(!mRv.Failed(), "Who failed it and why?");

  // Slightly icky action at a distance, but there's no better place to stash
  // this value, really.
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  MOZ_ASSERT(global);

  while (!mLoadedRequests.isEmpty()) {
    RefPtr<ScriptLoadRequest> req = mLoadedRequests.StealFirst();
    WorkerLoadContext* loadInfo = req->GetWorkerLoadContext();
    // We don't have a ProcessRequest method (like we do on the DOM), as there
    // isn't much processing that we need to do per request that isn't related
    // to evaluation (the processsing done for the DOM is handled in
    // DataRecievedFrom{Cache,Network} for workers.
    // So, this inner loop calls EvaluateScript directly. This will change
    // once modules are introduced as we will have some extra work to do.
    if (!EvaluateScript(aCx, req)) {
      mExecutionAborted = true;
      mMutedErrorFlag = loadInfo->mMutedErrorFlag.valueOr(true);
      mLoadedRequests.CancelRequestsAndClear();
      break;
    }
  }

  TryShutdown();
  return true;
}

nsresult WorkerScriptLoader::OnStreamComplete(ScriptLoadRequest* aRequest,
                                              nsresult aStatus) {
  AssertIsOnMainThread();

  LoadingFinished(aRequest, aStatus);
  return NS_OK;
}

void WorkerScriptLoader::CancelMainThreadWithBindingAborted(
    nsTArray<WorkerLoadContext*>&& aContextList) {
  AssertIsOnMainThread();
  CancelMainThread(NS_BINDING_ABORTED, &aContextList);
}

void WorkerScriptLoader::CancelMainThread(
    nsresult aCancelResult, nsTArray<WorkerLoadContext*>* aContextList) {
  AssertIsOnMainThread();
  {
    MutexAutoLock lock(CleanUpLock());

    // Check if we have already cancelled, or if the worker has been killed
    // before we cancel.
    if (IsCancelled() || CleanedUp()) {
      return;
    }

    mCancelMainThread = Some(aCancelResult);

    // In the case of a cancellation, service workers fetching from the
    // cache will still be doing work despite CancelMainThread. Eagerly
    // clear the promises associated with these scripts.
    for (WorkerLoadContext* loadContext : *aContextList) {
      if (loadContext->IsAwaitingPromise()) {
        loadContext->mCachePromise->MaybeReject(NS_BINDING_ABORTED);
        loadContext->mCachePromise = nullptr;
      }
    }
  }
}

nsresult WorkerScriptLoader::LoadScripts(
    nsTArray<WorkerLoadContext*>&& aContextList) {
  AssertIsOnMainThread();

  // Convert the origin stack to JSON (which must be done on the main
  // thread) explicitly, so that we can use the stack to notify the net
  // monitor about every script we load.
  if (mOriginStack) {
    ConvertSerializedStackToJSON(std::move(mOriginStack), mOriginStackJSON);
  }

  if (!mWorkerRef->Private()->IsServiceWorker() || IsDebuggerScript()) {
    for (WorkerLoadContext* loadContext : aContextList) {
      nsresult rv = LoadScript(loadContext->mRequest);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        LoadingFinished(loadContext->mRequest, rv);
        CancelMainThread(rv, &aContextList);
        return rv;
      }
    }

    return NS_OK;
  }

  RefPtr<CacheCreator> cacheCreator = new CacheCreator(mWorkerRef->Private());

  for (WorkerLoadContext* loadContext : aContextList) {
    loadContext->SetCacheCreator(cacheCreator);
    loadContext->GetCacheCreator()->AddLoader(
        MakeNotNull<RefPtr<CacheLoadHandler>>(mWorkerRef, loadContext->mRequest,
                                              loadContext->IsTopLevel(), this));
  }

  // The worker may have a null principal on first load, but in that case its
  // parent definitely will have one.
  nsIPrincipal* principal = mWorkerRef->Private()->GetPrincipal();
  if (!principal) {
    WorkerPrivate* parentWorker = mWorkerRef->Private()->GetParent();
    MOZ_ASSERT(parentWorker, "Must have a parent!");
    principal = parentWorker->GetPrincipal();
  }

  nsresult rv = cacheCreator->Load(principal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    CancelMainThread(rv, &aContextList);
    return rv;
  }

  return NS_OK;
}

nsresult WorkerScriptLoader::LoadScript(ScriptLoadRequest* aRequest) {
  AssertIsOnMainThread();

  WorkerLoadContext* loadContext = aRequest->GetWorkerLoadContext();
  MOZ_ASSERT_IF(loadContext->IsTopLevel(), !IsDebuggerScript());

  // The URL passed to us for loading was invalid, stop loading at this point.
  if (loadContext->mLoadResult != NS_ERROR_NOT_INITIALIZED) {
    return loadContext->mLoadResult;
  }

  WorkerPrivate* parentWorker = mWorkerRef->Private()->GetParent();

  // For JavaScript debugging, the devtools server must run on the same
  // thread as the debuggee, indicating the worker uses content principal.
  // However, in Bug 863246, web content will no longer be able to load
  // resource:// URIs by default, so we need system principal to load
  // debugger scripts.
  nsIPrincipal* principal = (IsDebuggerScript())
                                ? nsContentUtils::GetSystemPrincipal()
                                : mWorkerRef->Private()->GetPrincipal();

  nsCOMPtr<nsILoadGroup> loadGroup = mWorkerRef->Private()->GetLoadGroup();
  MOZ_DIAGNOSTIC_ASSERT(principal);

  NS_ENSURE_TRUE(NS_LoadGroupMatchesPrincipal(loadGroup, principal),
                 NS_ERROR_FAILURE);

  // May be null.
  nsCOMPtr<Document> parentDoc = mWorkerRef->Private()->GetDocument();

  nsCOMPtr<nsIChannel> channel;
  if (loadContext->IsTopLevel()) {
    // May be null.
    channel = mWorkerRef->Private()->ForgetWorkerChannel();
  }

  nsCOMPtr<nsIIOService> ios(do_GetIOService());

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  NS_ASSERTION(secMan, "This should never be null!");

  nsresult& rv = loadContext->mLoadResult;

  nsLoadFlags loadFlags = mWorkerRef->Private()->GetLoadFlags();

  // Get the top-level worker.
  WorkerPrivate* topWorkerPrivate = mWorkerRef->Private();
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
    nsCOMPtr<nsIReferrerInfo> referrerInfo =
        ReferrerInfo::CreateForFetch(principal, nullptr);
    if (parentWorker && !loadContext->IsTopLevel()) {
      referrerInfo =
          static_cast<ReferrerInfo*>(referrerInfo.get())
              ->CloneWithNewPolicy(parentWorker->GetReferrerPolicy());
    }

    rv = ChannelFromScriptURL(
        principal, parentDoc, mWorkerRef->Private(), loadGroup, ios, secMan,
        aRequest->mURI, loadContext->mClientInfo, mController,
        loadContext->IsTopLevel(), mWorkerScriptType,
        mWorkerRef->Private()->ContentPolicyType(), loadFlags,
        mWorkerRef->Private()->CookieJarSettings(), referrerInfo,
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
  RefPtr<NetworkLoadHandler> listener = new NetworkLoadHandler(this, aRequest);

  RefPtr<ScriptResponseHeaderProcessor> headerProcessor = nullptr;

  // For each debugger script, a non-debugger script load of the same script
  // should have occured prior that processed the headers.
  if (!IsDebuggerScript()) {
    headerProcessor = MakeRefPtr<ScriptResponseHeaderProcessor>(
        mWorkerRef->Private(), loadContext->IsTopLevel());
  }

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), listener, headerProcessor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (loadContext->IsTopLevel()) {
    MOZ_DIAGNOSTIC_ASSERT(loadContext->mClientInfo.isSome());

    // In order to get the correct foreign partitioned prinicpal, we need to
    // set the `IsThirdPartyContextToTopWindow` to the channel's loadInfo.
    // This flag reflects the fact that if the worker is created under a
    // third-party context.
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
    loadInfo->SetIsThirdPartyContextToTopWindow(
        mWorkerRef->Private()->IsThirdPartyContextToTopWindow());

    Maybe<ClientInfo> clientInfo;
    clientInfo.emplace(loadContext->mClientInfo.ref());
    rv = AddClientChannelHelper(channel, std::move(clientInfo),
                                Maybe<ClientInfo>(),
                                mWorkerRef->Private()->HybridEventTarget());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (StaticPrefs::browser_tabs_remote_useCrossOriginEmbedderPolicy()) {
    nsILoadInfo::CrossOriginEmbedderPolicy respectedCOEP =
        mWorkerRef->Private()->GetEmbedderPolicy();
    if (mWorkerRef->Private()->IsDedicatedWorker() &&
        respectedCOEP == nsILoadInfo::EMBEDDER_POLICY_NULL) {
      respectedCOEP = mWorkerRef->Private()->GetOwnerEmbedderPolicy();
    }

    nsCOMPtr<nsILoadInfo> channelLoadInfo = channel->LoadInfo();
    channelLoadInfo->SetLoadingEmbedderPolicy(respectedCOEP);
  }

  if (loadContext->mCacheStatus != WorkerLoadContext::ToBeCached) {
    rv = channel->AsyncOpen(loader);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    nsCOMPtr<nsIOutputStream> writer;

    // In case we return early.
    loadContext->mCacheStatus = WorkerLoadContext::Cancel;

    rv = NS_NewPipe(
        getter_AddRefs(loadContext->mCacheReadStream), getter_AddRefs(writer),
        0,
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

  return NS_OK;
}

void WorkerScriptLoader::DispatchMaybeMoveToLoadedList(
    ScriptLoadRequest* aRequest) {
  AssertIsOnMainThread();

  if (aRequest->GetWorkerLoadContext()->IsTopLevel()) {
    mWorkerRef->Private()->WorkerScriptLoaded();
  }

  RefPtr<ScriptExecutorRunnable> runnable =
      new ScriptExecutorRunnable(*this, mSyncLoopTarget, aRequest);
  if (!runnable->Dispatch()) {
    MOZ_ASSERT(false, "This should never fail!");
  }
}

bool WorkerScriptLoader::EvaluateScript(JSContext* aCx,
                                        ScriptLoadRequest* aRequest) {
  mWorkerRef->Private()->AssertIsOnWorkerThread();

  WorkerLoadContext* loadContext = aRequest->GetWorkerLoadContext();

  NS_ASSERTION(aRequest->IsReadyToRun(), "Should be scheduled!");

  MOZ_ASSERT(!mRv.Failed(), "Who failed it and why?");
  mRv.MightThrowJSException();
  if (NS_FAILED(loadContext->mLoadResult)) {
    nsAutoString url = NS_ConvertUTF8toUTF16(aRequest->mURL);
    workerinternals::ReportLoadError(mRv, loadContext->mLoadResult, url);
    return false;
  }

  // If this is a top level script that succeeded, then mark the
  // Client execution ready and possible controlled by a service worker.
  if (loadContext->IsTopLevel()) {
    if (mController.isSome()) {
      MOZ_ASSERT(mWorkerScriptType == WorkerScript,
                 "Debugger clients can't be controlled.");
      mWorkerRef->Private()->GlobalScope()->Control(mController.ref());
    }
    mWorkerRef->Private()->ExecutionReady();
  }

  JS::CompileOptions options(aCx);
  // The full URL shouldn't be exposed to the debugger. See Bug 1634872
  options.setFileAndLine(aRequest->mURL.get(), 1).setNoScriptRval(true);

  MOZ_ASSERT(loadContext->mMutedErrorFlag.isSome());
  options.setMutedErrors(loadContext->mMutedErrorFlag.valueOr(true));

  if (aRequest->mSourceMapURL) {
    options.setSourceMapURL(aRequest->mSourceMapURL->get());
  }

  // Our ErrorResult still shouldn't be a failure.
  MOZ_ASSERT(!mRv.Failed(), "Who failed it and why?");

  // Get the source text.
  ScriptLoadRequest::MaybeSourceText maybeSource;
  nsresult rv = aRequest->GetScriptSource(aCx, &maybeSource);
  if (NS_FAILED(rv)) {
    mRv.StealExceptionFromJSContext(aCx);
    return false;
  }

  bool successfullyEvaluated =
      aRequest->IsUTF8Text()
          ? EvaluateSourceBuffer(aCx, options,
                                 maybeSource.ref<JS::SourceText<Utf8Unit>>())
          : EvaluateSourceBuffer(aCx, options,
                                 maybeSource.ref<JS::SourceText<char16_t>>());

  if (!successfullyEvaluated) {
    mRv.StealExceptionFromJSContext(aCx);
    return false;
  }
  return true;
}

void WorkerScriptLoader::TryShutdown() {
  if (AllScriptsExecuted()) {
    ShutdownScriptLoader(!mExecutionAborted, mMutedErrorFlag);
  }
}

void WorkerScriptLoader::ShutdownScriptLoader(bool aResult, bool aMutedError) {
  mWorkerRef->Private()->AssertIsOnWorkerThread();
  MOZ_ASSERT(AllScriptsExecuted());

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
        LogExceptionToConsole(mWorkerRef->Private()->GetJSContext(),
                              mWorkerRef->Private());
        mRv.Throw(NS_ERROR_DOM_NETWORK_ERR);
      }
    } else {
      mRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    }
  }

  // Lock, shutdown, and cleanup state. After this the Loader is closed.
  {
    MutexAutoLock lock(CleanUpLock());

    mWorkerRef->Private()->StopSyncLoop(mSyncLoopTarget, aResult);

    // Signal cleanup
    mCleanedUp = true;
    // Allow worker shutdown.
    mWorkerRef = nullptr;
  }
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
    ScriptLoadRequest* aRequest)
    : MainThreadWorkerSyncRunnable(aScriptLoader.mWorkerRef->Private(),
                                   aSyncLoopTarget),
      mScriptLoader(aScriptLoader),
      mRequest(aRequest) {}

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

  if (!mRequest->GetWorkerLoadContext()->IsTopLevel()) {
    return true;
  }

  return mScriptLoader.StoreCSP();
}

bool ScriptExecutorRunnable::WorkerRun(JSContext* aCx,
                                       WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();

  // We must be on the same worker as we started on.
  MOZ_ASSERT(
      mScriptLoader.mSyncLoopTarget == mSyncLoopTarget,
      "Unexpected SyncLoopTarget. Check if the sync loop was closed early");

  mScriptLoader.MaybeMoveToLoadedList(mRequest);
  return mScriptLoader.ProcessPendingRequests(aCx);
}

nsresult ScriptExecutorRunnable::Cancel() {
  // We need to check first if cancel is called twice
  nsresult rv = MainThreadWorkerSyncRunnable::Cancel();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mScriptLoader.AllScriptsExecuted()) {
    mScriptLoader.ShutdownScriptLoader(false, false);
  }
  return NS_OK;
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
                    WorkerScriptType aWorkerScriptType, ErrorResult& aRv,
                    const mozilla::Encoding* aDocumentEncoding) {
  nsTArray<nsString> scriptURLs;

  scriptURLs.AppendElement(aScriptURL);

  LoadAllScripts(aWorkerPrivate, std::move(aOriginStack), scriptURLs, true,
                 aWorkerScriptType, aRv, aDocumentEncoding);
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
