/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoader.h"

#include "nsIChannel.h"
#include "nsIChannelPolicy.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIHttpChannel.h"
#include "nsIIOService.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamLoader.h"
#include "nsIURI.h"

#include "jsapi.h"
#include "nsChannelPolicy.h"
#include "nsError.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsDocShellCID.h"
#include "nsISupportsPrimitives.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "xpcpublic.h"

#include "mozilla/dom/Exceptions.h"
#include "Principal.h"
#include "WorkerFeature.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

#define MAX_CONCURRENT_SCRIPTS 1000

USING_WORKERS_NAMESPACE

using mozilla::dom::workers::exceptions::ThrowDOMExceptionForNSResult;

namespace {

nsresult
ChannelFromScriptURL(nsIPrincipal* principal,
                     nsIURI* baseURI,
                     nsIDocument* parentDoc,
                     nsILoadGroup* loadGroup,
                     nsIIOService* ios,
                     nsIScriptSecurityManager* secMan,
                     const nsAString& aScriptURL,
                     bool aIsWorkerScript,
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

  // If this script loader is being used to make a new worker then we need
  // to do a same-origin check. Otherwise we need to clear the load with the
  // security manager.
  if (aIsWorkerScript) {
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

  // Get Content Security Policy from parent document to pass into channel.
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = principal->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  if (csp) {
    channelPolicy = do_CreateInstance(NSCHANNELPOLICY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = channelPolicy->SetContentSecurityPolicy(csp);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = channelPolicy->SetLoadType(nsIContentPolicy::TYPE_SCRIPT);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  uint32_t flags = nsIRequest::LOAD_NORMAL | nsIChannel::LOAD_CLASSIFY_URI;

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), uri, ios, loadGroup, nullptr,
                     flags, channelPolicy);
  NS_ENSURE_SUCCESS(rv, rv);

  channel.forget(aChannel);
  return rv;
}

struct ScriptLoadInfo
{
  ScriptLoadInfo()
  : mScriptTextBuf(nullptr)
  , mScriptTextLength(0)
  , mLoadResult(NS_ERROR_NOT_INITIALIZED), mExecutionScheduled(false)
  , mExecutionResult(false)
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
  nsCOMPtr<nsIChannel> mChannel;
  jschar* mScriptTextBuf;
  size_t mScriptTextLength;

  nsresult mLoadResult;
  bool mExecutionScheduled;
  bool mExecutionResult;
};

class ScriptLoaderRunnable;

class ScriptExecutorRunnable MOZ_FINAL : public MainThreadWorkerSyncRunnable
{
  ScriptLoaderRunnable& mScriptLoader;
  uint32_t mFirstIndex;
  uint32_t mLastIndex;

public:
  ScriptExecutorRunnable(ScriptLoaderRunnable& aScriptLoader,
                         nsIEventTarget* aSyncLoopTarget, uint32_t aFirstIndex,
                         uint32_t aLastIndex);

private:
  ~ScriptExecutorRunnable()
  { }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE;

  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
          MOZ_OVERRIDE;

  NS_DECL_NSICANCELABLERUNNABLE

  void
  ShutdownScriptLoader(JSContext* aCx,
                       WorkerPrivate* aWorkerPrivate,
                       bool aResult);
};

class ScriptLoaderRunnable MOZ_FINAL : public WorkerFeature,
                                       public nsIRunnable,
                                       public nsIStreamLoaderObserver
{
  friend class ScriptExecutorRunnable;

  WorkerPrivate* mWorkerPrivate;
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;
  nsTArray<ScriptLoadInfo> mLoadInfos;
  bool mIsWorkerScript;
  bool mCanceled;
  bool mCanceledMainThread;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ScriptLoaderRunnable(WorkerPrivate* aWorkerPrivate,
                       nsIEventTarget* aSyncLoopTarget,
                       nsTArray<ScriptLoadInfo>& aLoadInfos,
                       bool aIsWorkerScript)
  : mWorkerPrivate(aWorkerPrivate), mSyncLoopTarget(aSyncLoopTarget),
    mIsWorkerScript(aIsWorkerScript), mCanceled(false),
    mCanceledMainThread(false)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aSyncLoopTarget);
    MOZ_ASSERT_IF(aIsWorkerScript, aLoadInfos.Length() == 1);

    mLoadInfos.SwapElements(aLoadInfos);
  }

private:
  ~ScriptLoaderRunnable()
  { }

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    AssertIsOnMainThread();

    if (NS_FAILED(RunInternal())) {
      CancelMainThread();
    }

    return NS_OK;
  }

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                   nsresult aStatus, uint32_t aStringLen,
                   const uint8_t* aString) MOZ_OVERRIDE
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

    loadInfo.mLoadResult = OnStreamCompleteInternal(aLoader, aContext, aStatus,
                                                    aStringLen, aString,
                                                    loadInfo);

    ExecuteFinishedScripts();

    return NS_OK;
  }

  virtual bool
  Notify(JSContext* aCx, Status aStatus) MOZ_OVERRIDE
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

  void
  CancelMainThread()
  {
    AssertIsOnMainThread();

    if (mCanceledMainThread) {
      return;
    }

    mCanceledMainThread = true;

    // Cancel all the channels that were already opened.
    for (uint32_t index = 0; index < mLoadInfos.Length(); index++) {
      ScriptLoadInfo& loadInfo = mLoadInfos[index];

      if (loadInfo.mChannel &&
          NS_FAILED(loadInfo.mChannel->Cancel(NS_BINDING_ABORTED))) {
        NS_WARNING("Failed to cancel channel!");
        loadInfo.mChannel = nullptr;
        loadInfo.mLoadResult = NS_BINDING_ABORTED;
      }
    }

    ExecuteFinishedScripts();
  }

  nsresult
  RunInternal()
  {
    AssertIsOnMainThread();

    WorkerPrivate* parentWorker = mWorkerPrivate->GetParent();

    // Figure out which principal to use.
    nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
    if (!principal) {
      NS_ASSERTION(parentWorker, "Must have a principal!");
      NS_ASSERTION(mIsWorkerScript, "Must have a principal for importScripts!");

      principal = parentWorker->GetPrincipal();
    }
    NS_ASSERTION(principal, "This should never be null here!");

    // Figure out our base URI.
    nsCOMPtr<nsIURI> baseURI;
    if (mIsWorkerScript) {
      if (parentWorker) {
        baseURI = parentWorker->GetBaseURI();
        NS_ASSERTION(baseURI, "Should have been set already!");
      }
      else {
        // May be null.
        baseURI = mWorkerPrivate->GetBaseURI();
      }
    }
    else {
      baseURI = mWorkerPrivate->GetBaseURI();
      NS_ASSERTION(baseURI, "Should have been set already!");
    }

    // May be null.
    nsCOMPtr<nsIDocument> parentDoc = mWorkerPrivate->GetDocument();

    nsCOMPtr<nsIChannel> channel;
    if (mIsWorkerScript) {
      // May be null.
      channel = mWorkerPrivate->ForgetWorkerChannel();
    }

    // All of these can potentially be null, but that should be ok. We'll either
    // succeed without them or fail below.
    nsCOMPtr<nsILoadGroup> loadGroup;
    if (parentDoc) {
      loadGroup = parentDoc->GetDocumentLoadGroup();
    }

    nsCOMPtr<nsIIOService> ios(do_GetIOService());

    nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
    NS_ASSERTION(secMan, "This should never be null!");

    for (uint32_t index = 0; index < mLoadInfos.Length(); index++) {
      ScriptLoadInfo& loadInfo = mLoadInfos[index];
      nsresult& rv = loadInfo.mLoadResult;

      if (!channel) {
        rv = ChannelFromScriptURL(principal, baseURI, parentDoc, loadGroup, ios,
                                  secMan, loadInfo.mURL, mIsWorkerScript,
                                                getter_AddRefs(channel));
        if (NS_FAILED(rv)) {
          return rv;
        }
      }

      // We need to know which index we're on in OnStreamComplete so we know
      // where to put the result.
      nsCOMPtr<nsISupportsPRUint32> indexSupports =
        do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = indexSupports->SetData(index);
      NS_ENSURE_SUCCESS(rv, rv);

      // We don't care about progress so just use the simple stream loader for
      // OnStreamComplete notification only.
      nsCOMPtr<nsIStreamLoader> loader;
      rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = channel->AsyncOpen(loader, indexSupports);
      NS_ENSURE_SUCCESS(rv, rv);

      loadInfo.mChannel.swap(channel);
    }

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

    if (!aLoadInfo.mScriptTextBuf || !aLoadInfo.mScriptTextLength) {
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
    if (mIsWorkerScript) {
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
      rv = ssm->GetChannelPrincipal(channel, getter_AddRefs(channelPrincipal));
      NS_ENSURE_SUCCESS(rv, rv);

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

      mWorkerPrivate->SetPrincipal(channelPrincipal);

      if (parent) {
        // XHR Params Allowed
        mWorkerPrivate->SetXHRParamsAllowed(parent->XHRParamsAllowed());

        // Set Eval and ContentSecurityPolicy
        mWorkerPrivate->SetCSP(parent->GetCSP());
        mWorkerPrivate->SetEvalAllowed(parent->IsEvalAllowed());
      }
    }

    return NS_OK;
  }

  void
  ExecuteFinishedScripts()
  {
    AssertIsOnMainThread();

    if (mIsWorkerScript) {
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

        // If we still have a channel then the load is not complete.
        if (loadInfo.mChannel) {
          break;
        }

        // We can execute this one.
        loadInfo.mExecutionScheduled = true;

        lastIndex = index;
      }
    }

    if (firstIndex != UINT32_MAX && lastIndex != UINT32_MAX) {
      nsRefPtr<ScriptExecutorRunnable> runnable =
        new ScriptExecutorRunnable(*this, mSyncLoopTarget, firstIndex,
                                   lastIndex);
      if (!runnable->Dispatch(nullptr)) {
        MOZ_ASSERT(false, "This should never fail!");
      }
    }
  }
};

NS_IMPL_ISUPPORTS(ScriptLoaderRunnable, nsIRunnable, nsIStreamLoaderObserver)

class ChannelGetterRunnable MOZ_FINAL : public nsRunnable
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
    aParentWorker->AssertIsOnWorkerThread();
    MOZ_ASSERT(aSyncLoopTarget);
  }

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    AssertIsOnMainThread();

    nsIPrincipal* principal = mParentWorker->GetPrincipal();
    NS_ASSERTION(principal, "This should never be null here!");

    // Figure out our base URI.
    nsCOMPtr<nsIURI> baseURI = mParentWorker->GetBaseURI();
    NS_ASSERTION(baseURI, "Should have been set already!");

    // May be null.
    nsCOMPtr<nsIDocument> parentDoc = mParentWorker->GetDocument();

    nsCOMPtr<nsIChannel> channel;
    mResult =
      scriptloader::ChannelFromScriptURLMainThread(principal, baseURI,
                                                   parentDoc, mScriptURL,
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
                                            uint32_t aFirstIndex,
                                            uint32_t aLastIndex)
: MainThreadWorkerSyncRunnable(aScriptLoader.mWorkerPrivate, aSyncLoopTarget),
  mScriptLoader(aScriptLoader), mFirstIndex(aFirstIndex), mLastIndex(aLastIndex)
{
  MOZ_ASSERT(aFirstIndex <= aLastIndex);
  MOZ_ASSERT(aLastIndex < aScriptLoader.mLoadInfos.Length());
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

    JS::SourceBufferHolder srcBuf(loadInfo.mScriptTextBuf,
                                  loadInfo.mScriptTextLength,
                                  JS::SourceBufferHolder::GiveOwnership);
    loadInfo.mScriptTextBuf = nullptr;
    loadInfo.mScriptTextLength = 0;

    if (!JS::Evaluate(aCx, global, options, srcBuf)) {
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
  aWorkerPrivate->RemoveFeature(aCx, &mScriptLoader);
  aWorkerPrivate->StopSyncLoop(mSyncLoopTarget, aResult);
}

bool
LoadAllScripts(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               nsTArray<ScriptLoadInfo>& aLoadInfos, bool aIsWorkerScript)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(!aLoadInfos.IsEmpty(), "Bad arguments!");

  AutoSyncLoopHolder syncLoop(aWorkerPrivate);

  nsRefPtr<ScriptLoaderRunnable> loader =
    new ScriptLoaderRunnable(aWorkerPrivate, syncLoop.EventTarget(),
                             aLoadInfos, aIsWorkerScript);

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
                               const nsAString& aScriptURL,
                               nsIChannel** aChannel)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsILoadGroup> loadGroup;
  if (aParentDoc) {
    loadGroup = aParentDoc->GetDocumentLoadGroup();
  }

  nsCOMPtr<nsIIOService> ios(do_GetIOService());

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  NS_ASSERTION(secMan, "This should never be null!");

  return ChannelFromScriptURL(aPrincipal, aBaseURI, aParentDoc, loadGroup,
                              ios, secMan, aScriptURL, true, aChannel);
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
LoadWorkerScript(JSContext* aCx)
{
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  NS_ASSERTION(worker, "This should never be null!");

  nsTArray<ScriptLoadInfo> loadInfos;

  ScriptLoadInfo* info = loadInfos.AppendElement();
  info->mURL = worker->ScriptURL();

  return LoadAllScripts(aCx, worker, loadInfos, true);
}

void
Load(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
     const Sequence<nsString>& aScriptURLs, ErrorResult& aRv)
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

  if (!LoadAllScripts(aCx, aWorkerPrivate, loadInfos, false)) {
    // LoadAllScripts can fail if we're shutting down.
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

} // namespace scriptloader

END_WORKERS_NAMESPACE
