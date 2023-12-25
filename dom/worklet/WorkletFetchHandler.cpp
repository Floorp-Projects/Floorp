/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "WorkletFetchHandler.h"

#include "js/loader/ModuleLoadRequest.h"
#include "js/ContextOptions.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/ScriptLoadHandler.h"  // ScriptDecoder
#include "mozilla/dom/Worklet.h"
#include "mozilla/dom/WorkletBinding.h"
#include "mozilla/dom/WorkletGlobalScope.h"
#include "mozilla/dom/WorkletImpl.h"
#include "mozilla/dom/WorkletThread.h"
#include "mozilla/dom/worklet/WorkletModuleLoader.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/TaskQueue.h"
#include "nsIInputStreamPump.h"
#include "nsIThreadRetargetableRequest.h"
#include "xpcpublic.h"

using JS::loader::ModuleLoadRequest;
using JS::loader::ParserMetadata;
using JS::loader::ScriptFetchOptions;
using mozilla::dom::loader::WorkletModuleLoader;

namespace mozilla::dom {

// A Runnable to call ModuleLoadRequest::StartModuleLoad on a worklet thread.
class StartModuleLoadRunnable final : public Runnable {
 public:
  StartModuleLoadRunnable(
      WorkletImpl* aWorkletImpl,
      const nsMainThreadPtrHandle<WorkletFetchHandler>& aHandlerRef,
      nsCOMPtr<nsIURI> aURI, nsIURI* aReferrer,
      const nsTArray<nsString>& aLocalizedStrs)
      : Runnable("Worklet::StartModuleLoadRunnable"),
        mWorkletImpl(aWorkletImpl),
        mHandlerRef(aHandlerRef),
        mURI(std::move(aURI)),
        mReferrer(aReferrer),
        mLocalizedStrs(aLocalizedStrs),
        mParentRuntime(
            JS_GetParentRuntime(CycleCollectedJSContext::Get()->Context())) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mParentRuntime);
    xpc::SetPrefableContextOptions(mContextOptions);
  }

  ~StartModuleLoadRunnable() = default;

  NS_IMETHOD Run() override;

 private:
  NS_IMETHOD RunOnWorkletThread();

  RefPtr<WorkletImpl> mWorkletImpl;
  nsMainThreadPtrHandle<WorkletFetchHandler> mHandlerRef;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mReferrer;
  const nsTArray<nsString>& mLocalizedStrs;
  JSRuntime* mParentRuntime;
  JS::ContextOptions mContextOptions;
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
  WorkletThread::EnsureCycleCollectedJSContext(mParentRuntime, mContextOptions);

  WorkletGlobalScope* globalScope = mWorkletImpl->GetGlobalScope();
  if (!globalScope) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  // To fetch a worklet/module worker script graph:
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-worklet/module-worker-script-graph
  // Step 1. Let options be a script fetch options whose cryptographic nonce is
  // the empty string, integrity metadata is the empty string, parser metadata
  // is "not-parser-inserted", credentials mode is credentials mode, referrer
  // policy is the empty string, and fetch priority is "auto".
  RefPtr<ScriptFetchOptions> fetchOptions = new ScriptFetchOptions(
      CORSMode::CORS_NONE, /* aNonce = */ u""_ns, RequestPriority::Auto,
      ParserMetadata::NotParserInserted,
      /*triggeringPrincipal*/ nullptr);

  WorkletModuleLoader* moduleLoader =
      static_cast<WorkletModuleLoader*>(globalScope->GetModuleLoader());
  MOZ_ASSERT(moduleLoader);

  if (!moduleLoader->HasSetLocalizedStrings()) {
    moduleLoader->SetLocalizedStrings(&mLocalizedStrs);
  }

  RefPtr<WorkletLoadContext> loadContext = new WorkletLoadContext(mHandlerRef);

  // Part of Step 2. This sets the Top-level flag to true
  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      mURI, ReferrerPolicy::_empty, fetchOptions, SRIMetadata(), mReferrer,
      loadContext, true, /* is top level */
      false,             /* is dynamic import */
      moduleLoader, ModuleLoadRequest::NewVisitedSetForTopLevelImport(mURI),
      nullptr);

  request->mURL = request->mURI->GetSpecOrDefault();
  request->NoCacheEntryFound();

  return request->StartModuleLoad();
}

StartFetchRunnable::StartFetchRunnable(
    const nsMainThreadPtrHandle<WorkletFetchHandler>& aHandlerRef, nsIURI* aURI,
    nsIURI* aReferrer)
    : Runnable("Worklet::StartFetchRunnable"),
      mHandlerRef(aHandlerRef),
      mURI(aURI),
      mReferrer(aReferrer) {
  MOZ_ASSERT(!NS_IsMainThread());
}

NS_IMETHODIMP
StartFetchRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> global =
      do_QueryInterface(mHandlerRef->mWorklet->GetParentObject());
  MOZ_ASSERT(global);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(global))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  nsresult rv = mHandlerRef->StartFetch(cx, mURI, mReferrer);
  if (NS_FAILED(rv)) {
    mHandlerRef->HandleFetchFailed(mURI);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// A Runnable to call ModuleLoadRequest::OnFetchComplete on a worklet thread.
class FetchCompleteRunnable final : public Runnable {
 public:
  FetchCompleteRunnable(WorkletImpl* aWorkletImpl, nsIURI* aURI,
                        nsresult aResult,
                        UniquePtr<uint8_t[]> aScriptBuffer = nullptr,
                        size_t aScriptLength = 0)
      : Runnable("Worklet::FetchCompleteRunnable"),
        mWorkletImpl(aWorkletImpl),
        mURI(aURI),
        mResult(aResult),
        mScriptBuffer(std::move(aScriptBuffer)),
        mScriptLength(aScriptLength) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  ~FetchCompleteRunnable() = default;

  NS_IMETHOD Run() override;

 private:
  NS_IMETHOD RunOnWorkletThread();

  RefPtr<WorkletImpl> mWorkletImpl;
  nsCOMPtr<nsIURI> mURI;
  nsresult mResult;
  UniquePtr<uint8_t[]> mScriptBuffer;
  size_t mScriptLength;
};

NS_IMETHODIMP
FetchCompleteRunnable::Run() {
  MOZ_ASSERT(WorkletThread::IsOnWorkletThread());
  return RunOnWorkletThread();
}

NS_IMETHODIMP FetchCompleteRunnable::RunOnWorkletThread() {
  WorkletGlobalScope* globalScope = mWorkletImpl->GetGlobalScope();
  if (!globalScope) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  WorkletModuleLoader* moduleLoader =
      static_cast<WorkletModuleLoader*>(globalScope->GetModuleLoader());
  MOZ_ASSERT(moduleLoader);
  MOZ_ASSERT(mURI);
  ModuleLoadRequest* request = moduleLoader->GetRequest(mURI);
  MOZ_ASSERT(request);

  // Set the Source type to "text" for decoding.
  request->SetTextSource(request->mLoadContext.get());

  nsresult rv;
  if (mScriptBuffer) {
    UniquePtr<ScriptDecoder> decoder = MakeUnique<ScriptDecoder>(
        UTF_8_ENCODING, ScriptDecoder::BOMHandling::Remove);
    rv = decoder->DecodeRawData(request, mScriptBuffer.get(), mScriptLength,
                                true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  request->mBaseURL = mURI;
  request->OnFetchComplete(mResult);

  if (NS_FAILED(mResult)) {
    if (request->IsTopLevel()) {
      request->LoadFailed();
    } else {
      request->Cancel();
    }
  }

  moduleLoader->RemoveRequest(mURI);
  return NS_OK;
}

//////////////////////////////////////////////////////////////
// WorkletFetchHandler
//////////////////////////////////////////////////////////////
NS_IMPL_CYCLE_COLLECTING_ADDREF(WorkletFetchHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WorkletFetchHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkletFetchHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(WorkletFetchHandler)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WorkletFetchHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWorklet, mPromises)
  tmp->mErrorToRethrow.setUndefined();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WorkletFetchHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWorklet, mPromises)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(WorkletFetchHandler)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mErrorToRethrow)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

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
      handler->AddPromise(aCx, promise);
      return promise.forget();
    }
  }

  RefPtr<WorkletFetchHandler> handler =
      new WorkletFetchHandler(aWorklet, promise, aOptions.mCredentials);

  nsMainThreadPtrHandle<WorkletFetchHandler> handlerRef{
      new nsMainThreadPtrHolder<WorkletFetchHandler>("FetchHandler", handler)};

  // Determine request's Referrer
  // https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
  // Step 3.  Switch on request’s referrer:
  //    "client"
  //    Step 1.4. Let referrerSource be document’s URL.
  nsIURI* referrer = doc->GetDocumentURIAsReferrer();
  nsCOMPtr<nsIRunnable> runnable = new StartModuleLoadRunnable(
      aWorklet->mImpl, handlerRef, std::move(resolvedURI), referrer,
      aWorklet->GetLocalizedStrings());

  if (NS_FAILED(aWorklet->mImpl->SendControlMessage(runnable.forget()))) {
    return nullptr;
  }

  promiseSettledGuard.release();

  aWorklet->AddImportFetchHandler(spec, handler);
  return promise.forget();
}

WorkletFetchHandler::WorkletFetchHandler(Worklet* aWorklet, Promise* aPromise,
                                         RequestCredentials aCredentials)
    : mWorklet(aWorklet), mStatus(ePending), mCredentials(aCredentials) {
  MOZ_ASSERT(aWorklet);
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(NS_IsMainThread());

  mPromises.AppendElement(aPromise);
}

WorkletFetchHandler::~WorkletFetchHandler() { mozilla::DropJSObjects(this); }

void WorkletFetchHandler::ExecutionFailed() {
  MOZ_ASSERT(NS_IsMainThread());
  RejectPromises(NS_ERROR_DOM_ABORT_ERR);
}

void WorkletFetchHandler::ExecutionFailed(JS::Handle<JS::Value> aError) {
  MOZ_ASSERT(NS_IsMainThread());
  RejectPromises(aError);
}

void WorkletFetchHandler::ExecutionSucceeded() {
  MOZ_ASSERT(NS_IsMainThread());
  ResolvePromises();
}

void WorkletFetchHandler::AddPromise(JSContext* aCx, Promise* aPromise) {
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(NS_IsMainThread());

  switch (mStatus) {
    case ePending:
      mPromises.AppendElement(aPromise);
      return;

    case eRejected:
      if (mHasError) {
        JS::Rooted<JS::Value> error(aCx, mErrorToRethrow);
        aPromise->MaybeReject(error);
      } else {
        aPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      }
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
  mWorklet = nullptr;
}

void WorkletFetchHandler::RejectPromises(JS::Handle<JS::Value> aValue) {
  MOZ_ASSERT(mStatus == ePending);
  MOZ_ASSERT(NS_IsMainThread());

  mWorklet->Impl()->OnAddModulePromiseSettled();

  for (uint32_t i = 0; i < mPromises.Length(); ++i) {
    mPromises[i]->MaybeReject(aValue);
  }
  mPromises.Clear();

  mHasError = true;
  mErrorToRethrow = aValue;

  mozilla::HoldJSObjects(this);

  mStatus = eRejected;
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

nsresult WorkletFetchHandler::StartFetch(JSContext* aCx, nsIURI* aURI,
                                         nsIURI* aReferrer) {
  nsAutoCString spec;
  nsresult res = aURI->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(res))) {
    return NS_ERROR_FAILURE;
  }

  RequestOrUSVString requestInput;

  nsAutoString url;
  CopyUTF8toUTF16(spec, url);
  requestInput.SetAsUSVString().ShareOrDependUpon(url);

  RootedDictionary<RequestInit> requestInit(aCx);
  requestInit.mCredentials.Construct(mCredentials);

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
  // Step 8. mode is "cors"
  requestInit.mMode.Construct(RequestMode::Cors);

  if (aReferrer) {
    nsAutoString referrer;
    res = aReferrer->GetSpec(spec);
    if (NS_WARN_IF(NS_FAILED(res))) {
      return NS_ERROR_FAILURE;
    }

    CopyUTF8toUTF16(spec, referrer);
    requestInit.mReferrer.Construct(referrer);
  }

  nsCOMPtr<nsIGlobalObject> global =
      do_QueryInterface(mWorklet->GetParentObject());
  MOZ_ASSERT(global);

  IgnoredErrorResult rv;
  SafeRefPtr<Request> request =
      Request::Constructor(global, aCx, requestInput, requestInit, rv);
  if (rv.Failed()) {
    return NS_ERROR_FAILURE;
  }

  request->OverrideContentPolicyType(mWorklet->Impl()->ContentPolicyType());

  RequestOrUSVString finalRequestInput;
  finalRequestInput.SetAsRequest() = request.unsafeGetRawPtr();

  RefPtr<Promise> fetchPromise = FetchRequest(
      global, finalRequestInput, requestInit, CallerType::System, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<WorkletScriptHandler> scriptHandler =
      new WorkletScriptHandler(mWorklet, aURI);
  fetchPromise->AppendNativeHandler(scriptHandler);
  return NS_OK;
}

void WorkletFetchHandler::HandleFetchFailed(nsIURI* aURI) {
  nsCOMPtr<nsIRunnable> runnable = new FetchCompleteRunnable(
      mWorklet->mImpl, aURI, NS_ERROR_FAILURE, nullptr, 0);

  if (NS_WARN_IF(
          NS_FAILED(mWorklet->mImpl->SendControlMessage(runnable.forget())))) {
    NS_WARNING("Failed to dispatch FetchCompleteRunnable to a worklet thread.");
  }
}

//////////////////////////////////////////////////////////////
// WorkletScriptHandler
//////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS(WorkletScriptHandler, nsIStreamLoaderObserver)

WorkletScriptHandler::WorkletScriptHandler(Worklet* aWorklet, nsIURI* aURI)
    : mWorklet(aWorklet), mURI(aURI) {}

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
    RefPtr<TaskQueue> queue = TaskQueue::Create(
        sts.forget(), "WorkletScriptHandler STS Delivery Queue");
    rv = rr->RetargetDeliveryTo(queue);
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

  // Copy the buffer and decode it on worklet thread, as we can only access
  // ModuleLoadRequest on worklet thread.
  UniquePtr<uint8_t[]> scriptTextBuf = MakeUnique<uint8_t[]>(aStringLen);
  memcpy(scriptTextBuf.get(), aString, aStringLen);

  nsCOMPtr<nsIRunnable> runnable = new FetchCompleteRunnable(
      mWorklet->mImpl, mURI, NS_OK, std::move(scriptTextBuf), aStringLen);

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

void WorkletScriptHandler::HandleFailure(nsresult aResult) {
  DispatchFetchCompleteToWorklet(aResult);
}

void WorkletScriptHandler::DispatchFetchCompleteToWorklet(nsresult aRv) {
  nsCOMPtr<nsIRunnable> runnable =
      new FetchCompleteRunnable(mWorklet->mImpl, mURI, aRv, nullptr, 0);

  if (NS_WARN_IF(
          NS_FAILED(mWorklet->mImpl->SendControlMessage(runnable.forget())))) {
    NS_WARNING("Failed to dispatch FetchCompleteRunnable to a worklet thread.");
  }
}

}  // namespace mozilla::dom
