/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Worklet.h"
#include "WorkletThread.h"

#include "mozilla/dom/WorkletBinding.h"
#include "mozilla/dom/WorkletGlobalScope.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/WorkletImpl.h"
#include "js/Modules.h"
#include "js/SourceText.h"
#include "nsIInputStreamPump.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsNetUtil.h"
#include "xpcprivate.h"

namespace mozilla {
namespace dom {

class ExecutionRunnable final : public Runnable {
 public:
  ExecutionRunnable(WorkletFetchHandler* aHandler, WorkletImpl* aWorkletImpl,
                    JS::UniqueTwoByteChars aScriptBuffer, size_t aScriptLength)
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
  JS::UniqueTwoByteChars mScriptBuffer;
  size_t mScriptLength;
  JSRuntime* mParentRuntime;
  nsresult mResult;
};

// ---------------------------------------------------------------------------
// WorkletFetchHandler

class WorkletFetchHandler final : public PromiseNativeHandler,
                                  public nsIStreamLoaderObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  static already_AddRefed<Promise> Fetch(Worklet* aWorklet,
                                         const nsAString& aModuleURL,
                                         const WorkletOptions& aOptions,
                                         CallerType aCallerType,
                                         ErrorResult& aRv) {
    MOZ_ASSERT(aWorklet);
    MOZ_ASSERT(NS_IsMainThread());

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
      promise->MaybeReject(rv);
      return promise.forget();
    }

    nsAutoCString spec;
    rv = resolvedURI->GetSpec(spec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
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

    RequestOrUSVString request;
    request.SetAsUSVString().Rebind(aModuleURL.Data(), aModuleURL.Length());

    RequestInit init;
    init.mCredentials.Construct(aOptions.mCredentials);

    RefPtr<Promise> fetchPromise =
        FetchRequest(global, request, init, aCallerType, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      promise->MaybeReject(aRv);
      return promise.forget();
    }

    RefPtr<WorkletFetchHandler> handler =
        new WorkletFetchHandler(aWorklet, spec, promise);
    fetchPromise->AppendNativeHandler(handler);

    aWorklet->AddImportFetchHandler(spec, handler);
    return promise.forget();
  }

  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override {
    MOZ_ASSERT(NS_IsMainThread());

    if (!aValue.isObject()) {
      RejectPromises(NS_ERROR_FAILURE);
      return;
    }

    RefPtr<Response> response;
    nsresult rv = UNWRAP_OBJECT(Response, &aValue.toObject(), response);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      RejectPromises(NS_ERROR_FAILURE);
      return;
    }

    if (!response->Ok()) {
      RejectPromises(NS_ERROR_DOM_NETWORK_ERR);
      return;
    }

    nsCOMPtr<nsIInputStream> inputStream;
    response->GetBody(getter_AddRefs(inputStream));
    if (!inputStream) {
      RejectPromises(NS_ERROR_DOM_NETWORK_ERR);
      return;
    }

    nsCOMPtr<nsIInputStreamPump> pump;
    rv = NS_NewInputStreamPump(getter_AddRefs(pump), inputStream.forget());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      RejectPromises(rv);
      return;
    }

    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      RejectPromises(rv);
      return;
    }

    rv = pump->AsyncRead(loader, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      RejectPromises(rv);
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

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                   nsresult aStatus, uint32_t aStringLen,
                   const uint8_t* aString) override {
    MOZ_ASSERT(NS_IsMainThread());

    if (NS_FAILED(aStatus)) {
      RejectPromises(aStatus);
      return NS_OK;
    }

    JS::UniqueTwoByteChars scriptTextBuf;
    size_t scriptTextLength;
    nsresult rv = ScriptLoader::ConvertToUTF16(
        nullptr, aString, aStringLen, NS_LITERAL_STRING("UTF-8"), nullptr,
        scriptTextBuf, scriptTextLength);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      RejectPromises(rv);
      return NS_OK;
    }

    // Moving the ownership of the buffer
    nsCOMPtr<nsIRunnable> runnable = new ExecutionRunnable(
        this, mWorklet->mImpl, std::move(scriptTextBuf), scriptTextLength);

    if (NS_FAILED(mWorklet->mImpl->SendControlMessage(runnable.forget()))) {
      RejectPromises(NS_ERROR_FAILURE);
      return NS_OK;
    }

    return NS_OK;
  }

  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override {
    MOZ_ASSERT(NS_IsMainThread());
    RejectPromises(NS_ERROR_DOM_NETWORK_ERR);
  }

  const nsCString& URL() const { return mURL; }

  void ExecutionFailed(nsresult aRv) {
    MOZ_ASSERT(NS_IsMainThread());
    RejectPromises(aRv);
  }

  void ExecutionSucceeded() {
    MOZ_ASSERT(NS_IsMainThread());
    ResolvePromises();
  }

 private:
  WorkletFetchHandler(Worklet* aWorklet, const nsACString& aURL,
                      Promise* aPromise)
      : mWorklet(aWorklet), mStatus(ePending), mErrorStatus(NS_OK), mURL(aURL) {
    MOZ_ASSERT(aWorklet);
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(NS_IsMainThread());

    mPromises.AppendElement(aPromise);
  }

  ~WorkletFetchHandler() {}

  void AddPromise(Promise* aPromise) {
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

  void RejectPromises(nsresult aResult) {
    MOZ_ASSERT(mStatus == ePending);
    MOZ_ASSERT(NS_FAILED(aResult));
    MOZ_ASSERT(NS_IsMainThread());

    for (uint32_t i = 0; i < mPromises.Length(); ++i) {
      mPromises[i]->MaybeReject(aResult);
    }
    mPromises.Clear();

    mStatus = eRejected;
    mErrorStatus = aResult;
    mWorklet = nullptr;
  }

  void ResolvePromises() {
    MOZ_ASSERT(mStatus == ePending);
    MOZ_ASSERT(NS_IsMainThread());

    for (uint32_t i = 0; i < mPromises.Length(); ++i) {
      mPromises[i]->MaybeResolveWithUndefined();
    }
    mPromises.Clear();

    mStatus = eResolved;
    mWorklet = nullptr;
  }

  RefPtr<Worklet> mWorklet;
  nsTArray<RefPtr<Promise>> mPromises;

  enum { ePending, eRejected, eResolved } mStatus;

  nsresult mErrorStatus;

  nsCString mURL;
};

NS_IMPL_ISUPPORTS(WorkletFetchHandler, nsIStreamLoaderObserver)

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
  compileOptions.setFileAndLine(mHandler->URL().get(), 0);
  compileOptions.setIsRunOnce(true);
  compileOptions.setNoScriptRval(true);

  JS::SourceText<char16_t> buffer;
  if (!buffer.init(aCx, std::move(mScriptBuffer), mScriptLength)) {
    return false;
  }
  JS::Rooted<JSObject*> module(aCx,
                               JS::CompileModule(aCx, compileOptions, buffer));
  if (!module) {
    return false;
  }
  // Link() was previously named Instantiate().
  // https://github.com/tc39/ecma262/pull/1312
  // Any imports will fail here - bug 1572644.
  if (!JS::ModuleInstantiate(aCx, module)) {
    return false;
  }
  aModule.set(module);
  return true;
}

void ExecutionRunnable::RunOnWorkletThread() {
  WorkletThread::EnsureCycleCollectedJSContext(mParentRuntime);

  WorkletGlobalScope* globalScope = mWorkletImpl->GetGlobalScope();
  MOZ_ASSERT(globalScope);

  AutoEntryScript aes(globalScope, "Worklet");
  JSContext* cx = aes.cx();

  JS::Rooted<JSObject*> globalObj(cx, globalScope->GetGlobalJSObject());
  JSAutoRealm ar(cx, globalObj);

  JS::Rooted<JSObject*> module(cx);
  if (!ParseAndLinkModule(cx, &module)) {
    ErrorResult error;
    error.MightThrowJSException();
    error.StealExceptionFromJSContext(cx);
    mResult = error.StealNSResult();
    return;
  }

  // https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
  // invokes
  // https://html.spec.whatwg.org/multipage/webappapis.html#run-a-module-script
  // without /rethrow errors/ and so unhandled exceptions do not cause the
  // promise to be rejected.
  JS::ModuleEvaluate(cx, module);
  JS::Rooted<JS::Value> unused(cx);

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

// ---------------------------------------------------------------------------
// Worklet

NS_IMPL_CYCLE_COLLECTION_CLASS(Worklet)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Worklet)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwnedObject)
  tmp->mImpl->NotifyWorkletFinished();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Worklet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwnedObject)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(Worklet)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Worklet)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Worklet)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Worklet)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Worklet::Worklet(nsPIDOMWindowInner* aWindow, RefPtr<WorkletImpl> aImpl,
                 nsISupports* aOwnedObject)
    : mWindow(aWindow), mOwnedObject(aOwnedObject), mImpl(std::move(aImpl)) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(mImpl);
  MOZ_ASSERT(NS_IsMainThread());

#ifdef RELEASE_OR_BETA
  MOZ_CRASH("This code should not go to release/beta yet!");
#endif
}

Worklet::~Worklet() { mImpl->NotifyWorkletFinished(); }

JSObject* Worklet::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto) {
  return mImpl->WrapWorklet(aCx, this, aGivenProto);
}

already_AddRefed<Promise> Worklet::AddModule(const nsAString& aModuleURL,
                                             const WorkletOptions& aOptions,
                                             CallerType aCallerType,
                                             ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  return WorkletFetchHandler::Fetch(this, aModuleURL, aOptions, aCallerType,
                                    aRv);
}

WorkletFetchHandler* Worklet::GetImportFetchHandler(const nsACString& aURI) {
  MOZ_ASSERT(NS_IsMainThread());
  return mImportHandlers.GetWeak(aURI);
}

void Worklet::AddImportFetchHandler(const nsACString& aURI,
                                    WorkletFetchHandler* aHandler) {
  MOZ_ASSERT(aHandler);
  MOZ_ASSERT(!mImportHandlers.GetWeak(aURI));
  MOZ_ASSERT(NS_IsMainThread());

  mImportHandlers.Put(aURI, aHandler);
}

}  // namespace dom
}  // namespace mozilla
