/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoader.h"

#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIHttpChannel.h"
#include "nsIIOService.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamLoader.h"
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
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "xpcpublic.h"

#include "mozilla/Assertions.h"
#include "mozilla/LoadContext.h"
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
  char16_t* mScriptTextBuf;
  size_t mScriptTextLength;

  nsresult mLoadResult;
  bool mExecutionScheduled;
  bool mExecutionResult;
};

class ScriptLoaderRunnable;

class ScriptExecutorRunnable final : public MainThreadWorkerSyncRunnable
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

class ScriptLoaderRunnable final : public WorkerFeature,
                                   public nsIRunnable,
                                   public nsIStreamLoaderObserver
{
  friend class ScriptExecutorRunnable;

  WorkerPrivate* mWorkerPrivate;
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;
  nsTArray<ScriptLoadInfo> mLoadInfos;
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

    loadInfo.mLoadResult = OnStreamCompleteInternal(aLoader, aContext, aStatus,
                                                    aStringLen, aString,
                                                    loadInfo);

    ExecuteFinishedScripts();

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
    nsCOMPtr<nsIURI> baseURI;
    if (mIsMainScript) {
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
    if (IsMainWorkerScript()) {
      // May be null.
      channel = mWorkerPrivate->ForgetWorkerChannel();
    }

    nsCOMPtr<nsIIOService> ios(do_GetIOService());

    nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
    NS_ASSERTION(secMan, "This should never be null!");

    for (uint32_t index = 0; index < mLoadInfos.Length(); index++) {
      ScriptLoadInfo& loadInfo = mLoadInfos[index];
      nsresult& rv = loadInfo.mLoadResult;

      if (!channel) {
        rv = ChannelFromScriptURL(principal, baseURI, parentDoc, loadGroup, ios,
                                  secMan, loadInfo.mURL, mIsMainScript,
                                  mWorkerScriptType, getter_AddRefs(channel));
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

      // If the load principal is the system principal then the channel
      // principal must also be the system principal (we do not allow chrome
      // code to create workers with non-chrome scripts). Otherwise this channel
      // principal must be same origin with the load principal (we check again
      // here in case redirects changed the location of the script).
      if (nsContentUtils::IsSystemPrincipal(loadPrincipal)) {
        if (!nsContentUtils::IsSystemPrincipal(channelPrincipal)) {
          // See if this is a resource URI. Since JSMs usually come from
          // resource:// URIs we're currently considering all URIs with the
          // URI_IS_UI_RESOURCE flag as valid for creating privileged workers.
          bool isResource;
          rv = NS_URIChainHasFlags(finalURI,
                                   nsIProtocolHandler::URI_IS_UI_RESOURCE,
                                   &isResource);
          NS_ENSURE_SUCCESS(rv, rv);

          if (isResource) {
            // Assign the system principal to the resource:// worker only if it
            // was loaded from code using the system principal.
            channelPrincipal = loadPrincipal;
          } else {
            return NS_ERROR_DOM_BAD_URI;
          }
        }
      }
      else  {
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
    NS_ASSERTION(principal, "This should never be null here!");

    // Figure out our base URI.
    nsCOMPtr<nsIURI> baseURI = mParentWorker->GetBaseURI();
    NS_ASSERTION(baseURI, "Should have been set already!");

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
                                            uint32_t aFirstIndex,
                                            uint32_t aLastIndex)
: MainThreadWorkerSyncRunnable(aScriptLoader.mWorkerPrivate, aSyncLoopTarget),
  mScriptLoader(aScriptLoader), mFirstIndex(aFirstIndex), mLastIndex(aLastIndex)
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

  if (!LoadAllScripts(aCx, aWorkerPrivate, loadInfos, false, WorkerScript)) {
    // LoadAllScripts can fail if we're shutting down.
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

} // namespace scriptloader

END_WORKERS_NAMESPACE
