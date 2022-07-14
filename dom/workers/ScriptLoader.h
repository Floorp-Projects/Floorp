/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_scriptloader_h__
#define mozilla_dom_workers_scriptloader_h__

#include "js/loader/ScriptLoadRequest.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/Maybe.h"
#include "nsIContentPolicy.h"
#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"

class nsIChannel;
class nsICookieJarSettings;
class nsILoadGroup;
class nsIPrincipal;
class nsIReferrerInfo;
class nsIURI;

namespace mozilla {

class ErrorResult;

namespace dom {

class ClientInfo;
class Document;
struct WorkerLoadInfo;
class WorkerPrivate;
class SerializedStackHolder;

enum WorkerScriptType { WorkerScript, DebuggerScript };

namespace workerinternals {

namespace loader {
class ScriptLoaderRunnable;
class ScriptExecutorRunnable;
class CachePromiseHandler;
class CacheLoadHandler;
class CacheCreator;
class NetworkLoadHandler;

/*
 * [DOMDOC] WorkerScriptLoader
 *
 * The WorkerScriptLoader is the primary class responsible for loading all
 * Workers, including: ServiceWorkers, SharedWorkers, RemoteWorkers, and
 * dedicated Workers. Our implementation also includes a subtype of dedicated
 * workers: ChromeWorker, which exposes information that isn't normally
 * accessible on a dedicated worker. See [1] for more information.
 *
 * Due to constraints around fetching, this class currently delegates the
 * "Fetch" portion of its work load to the main thread. Unlike the DOM
 * ScriptLoader, the WorkerScriptLoader is not persistent and is not reused for
 * subsequent loads. That means for each iteration of loading (for example,
 * loading the main script, followed by a load triggered by ImportScripts), we
 * recreate this class, and handle the case independently.
 *
 * The flow of requests across the boundaries looks like this:
 *
 *    +----------------------------+
 *    | new WorkerScriptLoader(..) |
 *    +----------------------------+
 *                |
 *                | Create the loader, along with the ScriptLoadRequests
 *                | call DispatchLoadScripts()
 *                | Create ScriptLoaderRunnable
 *                |
 *  #####################################################################
 *                             Enter Main thread
 *  #####################################################################
 *                |
 *                V
 *            +---------------+    For each request: Is a normal Worker?
 *            | LoadScripts() |--------------------------------------+
 *            +---------------+                                      |
 *                   |                                               |
 *                   | For each request: Is a ServiceWorker?         |
 *                   |                                               |
 *                   V                                               V
 *   +==================+   No script in cache?   +====================+
 *   | CacheLoadHandler |------------------------>| NetworkLoadHandler |
 *   +==================+                         +====================+
 *      :                                            :
 *      : Loaded from Cache                          : Loaded by Network
 *      :            +--------------------+          :
 *      +----------->| OnStreamComplete() |<---------+
 *                   +--------------------+
 *                             |
 *                             | A request is ready, is it in post order?
 *                             |
 *                             | call DispatchPendingProcessRequests()
 *                             | This creates ScriptExecutorRunnable
 *                             |
 *  #####################################################################
 *                           Enter worker thread
 *  #####################################################################
 *                             |
 *                             |
 *                             V
 *               +-------------------------+       All Scripts Executed?
 *               | ProcessPendingScripts() |-------------+
 *               +-------------------------+             |
 *                                                       |
 *                                                       | yes.
 *                                                       |
 *                                                       V
 *                                          +------------------------+
 *                                          | ShutdownScriptLoader() |
 *                                          +------------------------+
 */

class WorkerScriptLoader final : public nsINamed {
  friend class ScriptLoaderRunnable;
  friend class ScriptExecutorRunnable;
  friend class CachePromiseHandler;
  friend class CacheLoadHandler;
  friend class CacheCreator;
  friend class NetworkLoadHandler;

  using ScriptLoadRequest = JS::loader::ScriptLoadRequest;
  using ScriptLoadRequestList = JS::loader::ScriptLoadRequestList;
  using ScriptFetchOptions = JS::loader::ScriptFetchOptions;

  WorkerPrivate* const mWorkerPrivate;
  UniquePtr<SerializedStackHolder> mOriginStack;
  nsString mOriginStackJSON;
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;
  JS::loader::ScriptLoadRequestList mLoadingRequests;
  JS::loader::ScriptLoadRequestList mLoadedRequests;
  Maybe<ClientInfo> mClientInfo;
  Maybe<ServiceWorkerDescriptor> mController;
  const bool mIsMainScript;
  WorkerScriptType mWorkerScriptType;
  Maybe<nsresult> mCancelMainThread;
  ErrorResult& mRv;
  bool mExecutionAborted = false;
  bool mMutedErrorFlag = false;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  WorkerScriptLoader(WorkerPrivate* aWorkerPrivate,
                     UniquePtr<SerializedStackHolder> aOriginStack,
                     nsIEventTarget* aSyncLoopTarget,
                     const nsTArray<nsString>& aScriptURLs,
                     const mozilla::Encoding* aDocumentEncoding,
                     const Maybe<ClientInfo>& aClientInfo,
                     const Maybe<ServiceWorkerDescriptor>& aController,
                     bool aIsMainScript, WorkerScriptType aWorkerScriptType,
                     ErrorResult& aRv);

  void CancelMainThreadWithBindingAborted() {
    CancelMainThread(NS_BINDING_ABORTED);
  }

  bool DispatchLoadScripts();

 protected:
  nsIURI* GetBaseURI();

  void MaybeExecuteFinishedScripts(ScriptLoadRequest* aRequest);

  void MaybeMoveToLoadedList(ScriptLoadRequest* aRequest);

  bool StoreCSP();

  bool ProcessPendingRequests(JSContext* aCx);

  bool AllScriptsExecuted() {
    return mLoadingRequests.isEmpty() && mLoadedRequests.isEmpty();
  }

  nsresult OnStreamComplete(ScriptLoadRequest* aRequest, nsresult aStatus);

  // Are we loading the primary script, which is not a Debugger Script?
  bool IsMainWorkerScript() const {
    return mIsMainScript && mWorkerScriptType == WorkerScript;
  }

  // Are we loading the primary script, regardless of the script type?
  bool IsMainScript() const { return mIsMainScript; }

  bool IsDebuggerScript() const { return mWorkerScriptType == DebuggerScript; }

  void SetController(const Maybe<ServiceWorkerDescriptor>& aDescriptor) {
    mController = aDescriptor;
  }

  Maybe<ServiceWorkerDescriptor>& GetController() { return mController; }

  bool IsCancelled() { return mCancelMainThread.isSome(); }

  void CancelMainThread(nsresult aCancelResult);

  nsresult LoadScripts();

  nsresult LoadScript(ScriptLoadRequest* aRequest);

  void ShutdownScriptLoader(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                            bool aResult, bool aMutedError);

 private:
  ~WorkerScriptLoader() = default;

  NS_IMETHOD
  GetName(nsACString& aName) override {
    aName.AssignLiteral("WorkerScriptLoader");
    return NS_OK;
  }

  void LoadingFinished(ScriptLoadRequest* aRequest, nsresult aRv);

  void DispatchMaybeMoveToLoadedList(ScriptLoadRequest* aRequest);

  bool EvaluateScript(JSContext* aCx, ScriptLoadRequest* aRequest);

  void LogExceptionToConsole(JSContext* aCx, WorkerPrivate* aWorkerPrivate);
};

}  // namespace loader

nsresult ChannelFromScriptURLMainThread(
    nsIPrincipal* aPrincipal, Document* aParentDoc, nsILoadGroup* aLoadGroup,
    nsIURI* aScriptURL, const Maybe<ClientInfo>& aClientInfo,
    nsContentPolicyType aContentPolicyType,
    nsICookieJarSettings* aCookieJarSettings, nsIReferrerInfo* aReferrerInfo,
    nsIChannel** aChannel);

nsresult ChannelFromScriptURLWorkerThread(JSContext* aCx,
                                          WorkerPrivate* aParent,
                                          const nsAString& aScriptURL,
                                          WorkerLoadInfo& aLoadInfo);

void ReportLoadError(ErrorResult& aRv, nsresult aLoadResult,
                     const nsAString& aScriptURL);

void LoadMainScript(WorkerPrivate* aWorkerPrivate,
                    UniquePtr<SerializedStackHolder> aOriginStack,
                    const nsAString& aScriptURL,
                    WorkerScriptType aWorkerScriptType, ErrorResult& aRv,
                    const mozilla::Encoding* aDocumentEncoding);

void Load(WorkerPrivate* aWorkerPrivate,
          UniquePtr<SerializedStackHolder> aOriginStack,
          const nsTArray<nsString>& aScriptURLs,
          WorkerScriptType aWorkerScriptType, ErrorResult& aRv);

}  // namespace workerinternals

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_workers_scriptloader_h__ */
