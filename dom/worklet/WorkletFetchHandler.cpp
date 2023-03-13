/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "WorkletFetchHandler.h"

#include "js/loader/ModuleLoadRequest.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/Worklet.h"
#include "mozilla/dom/WorkletBinding.h"
#include "mozilla/dom/WorkletGlobalScope.h"
#include "mozilla/dom/WorkletImpl.h"
#include "mozilla/dom/WorkletThread.h"
#include "mozilla/dom/worklet/WorkletModuleLoader.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/ScopeExit.h"
#include "nsIInputStreamPump.h"
#include "nsIThreadRetargetableRequest.h"

using JS::loader::ModuleLoadRequest;
using JS::loader::ScriptFetchOptions;
using mozilla::dom::loader::WorkletModuleLoader;

namespace mozilla::dom {

// A Runnable to call ModuleLoadRequest::StartModuleLoad on a worklet thread.
class StartModuleLoadRunnable final : public Runnable {
 public:
  StartModuleLoadRunnable(
      WorkletImpl* aWorkletImpl,
      const nsMainThreadPtrHandle<WorkletFetchHandler>& aHandlerRef,
      nsCOMPtr<nsIURI> aURI, nsIURI* aReferrer)
      : Runnable("Worklet::StartModuleLoadRunnable"),
        mWorkletImpl(aWorkletImpl),
        mHandlerRef(aHandlerRef),
        mURI(std::move(aURI)),
        mReferrer(aReferrer),
        mParentRuntime(
            JS_GetParentRuntime(CycleCollectedJSContext::Get()->Context())) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mParentRuntime);
  }

  ~StartModuleLoadRunnable() = default;

  NS_IMETHOD Run() override;

 private:
  NS_IMETHOD RunOnWorkletThread();

  RefPtr<WorkletImpl> mWorkletImpl;
  nsMainThreadPtrHandle<WorkletFetchHandler> mHandlerRef;
  nsCOMPtr<nsIURI> mURI;
  nsIURI* mReferrer;
  JSRuntime* mParentRuntime;
};

NS_IMETHODIMP
StartModuleLoadRunnable::Run() {
  // WorkletThread::IsOnWorkletThread() cannot be used here because it depends
  // on a WorkletJSContext having been created for this thread.  That does not
  // happen until the global scope is created the first time
  // RunOnWorkletThread() is called.
  MOZ_ASSERT(!NS_IsMainThread());
  return RunOnWorkletThread();
}

NS_IMETHODIMP StartModuleLoadRunnable::RunOnWorkletThread() {
  // This can be called on a GraphRunner thread or a DOM Worklet thread.
  WorkletThread::EnsureCycleCollectedJSContext(mParentRuntime);

  WorkletGlobalScope* globalScope = mWorkletImpl->GetGlobalScope();
  if (!globalScope) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  // To fetch a worklet/module worker script graph:
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-worklet/module-worker-script-graph
  // Step 1. Let options be a script fetch options. And referrer policy is the
  // empty string.
  ReferrerPolicy referrerPolicy = ReferrerPolicy::_empty;
  RefPtr<ScriptFetchOptions> fetchOptions = new ScriptFetchOptions(
      CORSMode::CORS_NONE, referrerPolicy, /*triggeringPrincipal*/ nullptr);

  WorkletModuleLoader* moduleLoader =
      static_cast<WorkletModuleLoader*>(globalScope->GetModuleLoader());
  MOZ_ASSERT(moduleLoader);

  RefPtr<WorkletLoadContext> loadContext = new WorkletLoadContext(mHandlerRef);

  // Part of Step 2. This sets the Top-level flag to true
  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      mURI, fetchOptions, SRIMetadata(), mReferrer, loadContext,
      true,  /* is top level */
      false, /* is dynamic import */
      moduleLoader, ModuleLoadRequest::NewVisitedSetForTopLevelImport(mURI),
      nullptr);

  request->mURL = request->mURI->GetSpecOrDefault();

  return request->StartModuleLoad();
}

class ExecutionRunnable final : public Runnable {
 public:
  ExecutionRunnable(WorkletFetchHandler* aHandler, WorkletImpl* aWorkletImpl,
                    UniquePtr<Utf8Unit[], JS::FreePolicy> aScriptBuffer,
                    size_t aScriptLength)
      : Runnable("Worklet::ExecutionRunnable"),
        mHandler(aHandler),
        mWorkletImpl(aWorkletImpl),
        mScriptBuffer(std::move(aScriptBuffer)),
        mScriptLength(aScriptLength),
        mParentRuntime(
            JS_GetParentRuntime(CycleCollectedJSContext::Get()->Context())),
        mResult(NS_ERROR_FAILURE) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mParentRuntime);
  }

  NS_IMETHOD
  Run() override;

 private:
  void RunOnWorkletThread();

  void RunOnMainThread();

  bool ParseAndLinkModule(JSContext* aCx, JS::MutableHandle<JSObject*> aModule);

  RefPtr<WorkletFetchHandler> mHandler;
  RefPtr<WorkletImpl> mWorkletImpl;
  UniquePtr<Utf8Unit[], JS::FreePolicy> mScriptBuffer;
  size_t mScriptLength;
  JSRuntime* mParentRuntime;
  nsresult mResult;
};

NS_IMETHODIMP
ExecutionRunnable::Run() {
  // WorkletThread::IsOnWorkletThread() cannot be used here because it depends
  // on a WorkletJSContext having been created for this thread.  That does not
  // happen until the global scope is created the first time
  // RunOnWorkletThread() is called.
  if (!NS_IsMainThread()) {
    RunOnWorkletThread();
    return NS_DispatchToMainThread(this);
  }

  RunOnMainThread();
  return NS_OK;
}

bool ExecutionRunnable::ParseAndLinkModule(
    JSContext* aCx, JS::MutableHandle<JSObject*> aModule) {
  JS::CompileOptions compileOptions(aCx);
  compileOptions.setIntroductionType("Worklet");
  compileOptions.setFileAndLine(mHandler->URL().get(), 1);
  compileOptions.setIsRunOnce(true);
  compileOptions.setNoScriptRval(true);

  JS::SourceText<Utf8Unit> buffer;
  if (!buffer.init(aCx, std::move(mScriptBuffer), mScriptLength)) {
    return false;
  }
  JS::Rooted<JSObject*> module(aCx,
                               JS::CompileModule(aCx, compileOptions, buffer));
  if (!module) {
    return false;
  }
  // Any imports will fail here - bug 1572644.
  if (!JS::ModuleLink(aCx, module)) {
    return false;
  }
  aModule.set(module);
  return true;
}

void ExecutionRunnable::RunOnWorkletThread() {
  // This can be called on a GraphRunner thread or a DOM Worklet thread.
  WorkletThread::EnsureCycleCollectedJSContext(mParentRuntime);

  WorkletGlobalScope* globalScope = mWorkletImpl->GetGlobalScope();
  if (!globalScope) {
    mResult = NS_ERROR_DOM_UNKNOWN_ERR;
    return;
  }

  AutoEntryScript aes(globalScope, "Worklet");
  JSContext* cx = aes.cx();

  JS::Rooted<JSObject*> module(cx);
  if (!ParseAndLinkModule(cx, &module)) {
    mResult = NS_ERROR_DOM_ABORT_ERR;
    return;
  }

  // https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
  // invokes
  // https://html.spec.whatwg.org/multipage/webappapis.html#run-a-module-script
  // without /rethrow errors/ and so unhandled exceptions do not cause the
  // promise to be rejected.
  JS::Rooted<JS::Value> rval(cx);
  JS::ModuleEvaluate(cx, module, &rval);
  // With top-level await, we need to unwrap the module promise, or we end up
  // with less helpfull error messages. A modules return value can either be a
  // promise or undefined. If the value is defined, we have an async module and
  // can unwrap it.
  if (!rval.isUndefined() && rval.isObject()) {
    JS::Rooted<JSObject*> aEvaluationPromise(cx);
    aEvaluationPromise.set(&rval.toObject());
    if (!JS::ThrowOnModuleEvaluationFailure(cx, aEvaluationPromise)) {
      mResult = NS_ERROR_DOM_ABORT_ERR;
      return;
    }
  }

  // All done.
  mResult = NS_OK;
}

void ExecutionRunnable::RunOnMainThread() {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_FAILED(mResult)) {
    mHandler->ExecutionFailed(mResult);
    return;
  }

  mHandler->ExecutionSucceeded();
}

//////////////////////////////////////////////////////////////
// WorkletFetchHandler
//////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS(WorkletFetchHandler, nsISupports)

// static
already_AddRefed<Promise> WorkletFetchHandler::AddModule(
    Worklet* aWorklet, JSContext* aCx, const nsAString& aModuleURL,
    const WorkletOptions& aOptions, ErrorResult& aRv) {
  MOZ_ASSERT(aWorklet);
  MOZ_ASSERT(NS_IsMainThread());

  aWorklet->Impl()->OnAddModuleStarted();

  auto promiseSettledGuard =
      MakeScopeExit([&] { aWorklet->Impl()->OnAddModulePromiseSettled(); });

  nsCOMPtr<nsIGlobalObject> global =
      do_QueryInterface(aWorklet->GetParentObject());
  MOZ_ASSERT(global);

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = aWorklet->GetParentObject();
  MOZ_ASSERT(window);

  nsCOMPtr<Document> doc;
  doc = window->GetExtantDoc();
  if (!doc) {
    promise->MaybeReject(NS_ERROR_FAILURE);
    return promise.forget();
  }

  nsCOMPtr<nsIURI> resolvedURI;
  nsresult rv = NS_NewURI(getter_AddRefs(resolvedURI), aModuleURL, nullptr,
                          doc->GetBaseURI());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // https://html.spec.whatwg.org/multipage/worklets.html#dom-worklet-addmodule
    // Step 3. If this fails, then return a promise rejected with a
    // "SyntaxError" DOMException.
    rv = NS_ERROR_DOM_SYNTAX_ERR;

    promise->MaybeReject(rv);
    return promise.forget();
  }

  nsAutoCString spec;
  rv = resolvedURI->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    rv = NS_ERROR_DOM_SYNTAX_ERR;

    promise->MaybeReject(rv);
    return promise.forget();
  }

  // Maybe we already have an handler for this URI
  {
    WorkletFetchHandler* handler = aWorklet->GetImportFetchHandler(spec);
    if (handler) {
      handler->AddPromise(promise);
      return promise.forget();
    }
  }

  RequestOrUSVString requestInput;
  requestInput.SetAsUSVString().ShareOrDependUpon(aModuleURL);

  RootedDictionary<RequestInit> requestInit(aCx);
  requestInit.mCredentials.Construct(aOptions.mCredentials);

  SafeRefPtr<Request> request =
      Request::Constructor(global, aCx, requestInput, requestInit, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  request->OverrideContentPolicyType(aWorklet->Impl()->ContentPolicyType());

  RequestOrUSVString finalRequestInput;
  finalRequestInput.SetAsRequest() = request.unsafeGetRawPtr();

  RefPtr<Promise> fetchPromise = FetchRequest(
      global, finalRequestInput, requestInit, CallerType::System, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    // OK to just return null, since caller will ignore return value
    // anyway if aRv is a failure.
    return nullptr;
  }

  RefPtr<WorkletScriptHandler> scriptHandler =
      new WorkletScriptHandler(aWorklet);
  fetchPromise->AppendNativeHandler(scriptHandler);

  RefPtr<WorkletFetchHandler> handler =
      new WorkletFetchHandler(aWorklet, spec, promise);

  nsMainThreadPtrHandle<WorkletFetchHandler> handlerRef{
      new nsMainThreadPtrHolder<WorkletFetchHandler>("FetchHandler", handler)};

  // Determine request's Referrer
  // https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
  // Step 3.  Switch on request’s referrer:
  //    "client"
  //    Step 1.4. Let referrerSource be document’s URL.
  nsIURI* referrer = doc->GetDocumentURIAsReferrer();
  nsCOMPtr<nsIRunnable> runnable = new StartModuleLoadRunnable(
      aWorklet->mImpl, handlerRef, std::move(resolvedURI), referrer);

  if (NS_FAILED(aWorklet->mImpl->SendControlMessage(runnable.forget()))) {
    return nullptr;
  }

  promiseSettledGuard.release();

  aWorklet->AddImportFetchHandler(spec, handler);
  return promise.forget();
}

WorkletFetchHandler::WorkletFetchHandler(Worklet* aWorklet,
                                         const nsACString& aURL,
                                         Promise* aPromise)
    : mWorklet(aWorklet), mStatus(ePending), mErrorStatus(NS_OK), mURL(aURL) {
  MOZ_ASSERT(aWorklet);
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(NS_IsMainThread());

  mPromises.AppendElement(aPromise);
}

void WorkletFetchHandler::ExecutionFailed(nsresult aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  RejectPromises(aRv);
}

void WorkletFetchHandler::ExecutionSucceeded() {
  MOZ_ASSERT(NS_IsMainThread());
  ResolvePromises();
}

void WorkletFetchHandler::AddPromise(Promise* aPromise) {
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(NS_IsMainThread());

  switch (mStatus) {
    case ePending:
      mPromises.AppendElement(aPromise);
      return;

    case eRejected:
      MOZ_ASSERT(NS_FAILED(mErrorStatus));
      aPromise->MaybeReject(mErrorStatus);
      return;

    case eResolved:
      aPromise->MaybeResolveWithUndefined();
      return;
  }
}

void WorkletFetchHandler::RejectPromises(nsresult aResult) {
  MOZ_ASSERT(mStatus == ePending);
  MOZ_ASSERT(NS_FAILED(aResult));
  MOZ_ASSERT(NS_IsMainThread());

  mWorklet->Impl()->OnAddModulePromiseSettled();

  for (uint32_t i = 0; i < mPromises.Length(); ++i) {
    mPromises[i]->MaybeReject(aResult);
  }
  mPromises.Clear();

  mStatus = eRejected;
  mErrorStatus = aResult;
  mWorklet = nullptr;
}

void WorkletFetchHandler::ResolvePromises() {
  MOZ_ASSERT(mStatus == ePending);
  MOZ_ASSERT(NS_IsMainThread());

  mWorklet->Impl()->OnAddModulePromiseSettled();

  for (uint32_t i = 0; i < mPromises.Length(); ++i) {
    mPromises[i]->MaybeResolveWithUndefined();
  }
  mPromises.Clear();

  mStatus = eResolved;
  mWorklet = nullptr;
}

//////////////////////////////////////////////////////////////
// WorkletScriptHandler
//////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS(WorkletScriptHandler, nsIStreamLoaderObserver)

WorkletScriptHandler::WorkletScriptHandler(Worklet* aWorklet)
    : mWorklet(aWorklet) {}

void WorkletScriptHandler::ResolvedCallback(JSContext* aCx,
                                            JS::Handle<JS::Value> aValue,
                                            ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aValue.isObject()) {
    HandleFailure(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<Response> response;
  nsresult rv = UNWRAP_OBJECT(Response, &aValue.toObject(), response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    HandleFailure(NS_ERROR_FAILURE);
    return;
  }

  // https://html.spec.whatwg.org/multipage/worklets.html#dom-worklet-addmodule
  // Step 6.4.1. If script is null, then:
  //   Step 1.1.2. Reject promise with an "AbortError" DOMException.
  if (!response->Ok()) {
    HandleFailure(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  response->GetBody(getter_AddRefs(inputStream));
  if (!inputStream) {
    HandleFailure(NS_ERROR_DOM_NETWORK_ERR);
    return;
  }

  nsCOMPtr<nsIInputStreamPump> pump;
  rv = NS_NewInputStreamPump(getter_AddRefs(pump), inputStream.forget());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    HandleFailure(rv);
    return;
  }

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    HandleFailure(rv);
    return;
  }

  rv = pump->AsyncRead(loader);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    HandleFailure(rv);
    return;
  }

  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(pump);
  if (rr) {
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    rv = rr->RetargetDeliveryTo(sts);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch the nsIInputStreamPump to a IO thread.");
    }
  }
}

NS_IMETHODIMP WorkletScriptHandler::OnStreamComplete(nsIStreamLoader* aLoader,
                                                     nsISupports* aContext,
                                                     nsresult aStatus,
                                                     uint32_t aStringLen,
                                                     const uint8_t* aString) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_FAILED(aStatus)) {
    HandleFailure(aStatus);
    return NS_OK;
  }

  UniquePtr<Utf8Unit[], JS::FreePolicy> scriptTextBuf;
  size_t scriptTextLength;
  nsresult rv =
      ScriptLoader::ConvertToUTF8(nullptr, aString, aStringLen, u"UTF-8"_ns,
                                  nullptr, scriptTextBuf, scriptTextLength);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    HandleFailure(rv);
    return NS_OK;
  }

  // Moving the ownership of the buffer
  //
  // Note: ExecutableRunnable will be removed by later patch, so we simply pass
  // nullptr as the first argument to bypass the compilation error.
  nsCOMPtr<nsIRunnable> runnable = new ExecutionRunnable(
      nullptr, mWorklet->mImpl, std::move(scriptTextBuf), scriptTextLength);

  if (NS_FAILED(mWorklet->mImpl->SendControlMessage(runnable.forget()))) {
    HandleFailure(NS_ERROR_FAILURE);
  }

  return NS_OK;
}

void WorkletScriptHandler::RejectedCallback(JSContext* aCx,
                                            JS::Handle<JS::Value> aValue,
                                            ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  // https://html.spec.whatwg.org/multipage/worklets.html#dom-worklet-addmodule
  // Step 6.4.1. If script is null, then:
  //   Step 1.1.2. Reject promise with an "AbortError" DOMException.
  HandleFailure(NS_ERROR_DOM_ABORT_ERR);
}

void WorkletScriptHandler::HandleFailure(nsresult aResult) {}

}  // namespace mozilla::dom
