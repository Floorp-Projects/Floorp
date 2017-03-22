/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Worklet.h"
#include "AudioWorkletGlobalScope.h"
#include "PaintWorkletGlobalScope.h"

#include "mozilla/dom/WorkletBinding.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/RegisterWorkletBindings.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "xpcprivate.h"

namespace mozilla {
namespace dom {

// ---------------------------------------------------------------------------
// WorkletFetchHandler

class WorkletFetchHandler : public PromiseNativeHandler
                          , public nsIStreamLoaderObserver
{
public:
  NS_DECL_ISUPPORTS

  static already_AddRefed<Promise>
  Fetch(Worklet* aWorklet, const nsAString& aModuleURL, CallerType aCallerType,
        ErrorResult& aRv)
  {
    MOZ_ASSERT(aWorklet);

    nsCOMPtr<nsIGlobalObject> global =
      do_QueryInterface(aWorklet->GetParentObject());
    MOZ_ASSERT(global);

    RefPtr<Promise> promise = Promise::Create(global, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    nsCOMPtr<nsPIDOMWindowInner> window = aWorklet->GetParentObject();
    MOZ_ASSERT(window);

    nsCOMPtr<nsIDocument> doc;
    doc = window->GetExtantDoc();
    if (!doc) {
      promise->MaybeReject(NS_ERROR_FAILURE);
      return promise.forget();
    }

    nsCOMPtr<nsIURI> baseURI = doc->GetBaseURI();
    nsCOMPtr<nsIURI> resolvedURI;
    nsresult rv = NS_NewURI(getter_AddRefs(resolvedURI), aModuleURL, nullptr, baseURI);
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

    RefPtr<Promise> fetchPromise =
      FetchRequest(global, request, init, aCallerType, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      promise->MaybeReject(aRv);
      return promise.forget();
    }

    RefPtr<WorkletFetchHandler> handler =
      new WorkletFetchHandler(aWorklet, aModuleURL, promise);
    fetchPromise->AppendNativeHandler(handler);

    aWorklet->AddImportFetchHandler(spec, handler);
    return promise.forget();
  }

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
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
    rv = NS_NewInputStreamPump(getter_AddRefs(pump), inputStream);
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
                   const uint8_t* aString) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (NS_FAILED(aStatus)) {
      RejectPromises(aStatus);
      return NS_OK;
    }

    char16_t* scriptTextBuf;
    size_t scriptTextLength;
    nsresult rv =
      nsScriptLoader::ConvertToUTF16(nullptr, aString, aStringLen,
                                     NS_LITERAL_STRING("UTF-8"), nullptr,
                                     scriptTextBuf, scriptTextLength);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      RejectPromises(rv);
      return NS_OK;
    }

    // Moving the ownership of the buffer
    JS::SourceBufferHolder buffer(scriptTextBuf, scriptTextLength,
                                  JS::SourceBufferHolder::GiveOwnership);

    AutoJSAPI jsapi;
    jsapi.Init();

    RefPtr<WorkletGlobalScope> globalScope =
      mWorklet->GetOrCreateGlobalScope(jsapi.cx());
    MOZ_ASSERT(globalScope);

    AutoEntryScript aes(globalScope, "Worklet");
    JSContext* cx = aes.cx();

    JS::Rooted<JSObject*> globalObj(cx, globalScope->GetGlobalJSObject());

    (void) new XPCWrappedNativeScope(cx, globalObj);

    JS::CompileOptions compileOptions(cx);
    compileOptions.setIntroductionType("Worklet");
    compileOptions.setFileAndLine(NS_ConvertUTF16toUTF8(mURL).get(), 0);
    compileOptions.setVersion(JSVERSION_DEFAULT);
    compileOptions.setIsRunOnce(true);
    compileOptions.setNoScriptRval(true);

    JSAutoCompartment comp(cx, globalObj);

    JS::Rooted<JS::Value> unused(cx);
    if (!JS::Evaluate(cx, compileOptions, buffer, &unused)) {
      ErrorResult error;
      error.MightThrowJSException();
      error.StealExceptionFromJSContext(cx);
      RejectPromises(error.StealNSResult());
      return NS_OK;
    }

    // All done.
    ResolvePromises();
    return NS_OK;
  }

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    RejectPromises(NS_ERROR_DOM_NETWORK_ERR);
  }

private:
  WorkletFetchHandler(Worklet* aWorklet, const nsAString& aURL,
                      Promise* aPromise)
    : mWorklet(aWorklet)
    , mStatus(ePending)
    , mErrorStatus(NS_OK)
    , mURL(aURL)
  {
    MOZ_ASSERT(aWorklet);
    MOZ_ASSERT(aPromise);

    mPromises.AppendElement(aPromise);
  }

  ~WorkletFetchHandler()
  {}

  void
  AddPromise(Promise* aPromise)
  {
    MOZ_ASSERT(aPromise);

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

  void
  RejectPromises(nsresult aResult)
  {
    MOZ_ASSERT(mStatus == ePending);
    MOZ_ASSERT(NS_FAILED(aResult));

    for (uint32_t i = 0; i < mPromises.Length(); ++i) {
      mPromises[i]->MaybeReject(aResult);
    }
    mPromises.Clear();

    mStatus = eRejected;
    mErrorStatus = aResult;
    mWorklet = nullptr;
  }

  void
  ResolvePromises()
  {
    MOZ_ASSERT(mStatus == ePending);

    for (uint32_t i = 0; i < mPromises.Length(); ++i) {
      mPromises[i]->MaybeResolveWithUndefined();
    }
    mPromises.Clear();

    mStatus = eResolved;
    mWorklet = nullptr;
  }

  RefPtr<Worklet> mWorklet;
  nsTArray<RefPtr<Promise>> mPromises;

  enum {
    ePending,
    eRejected,
    eResolved
  } mStatus;

  nsresult mErrorStatus;

  nsString mURL;
};

NS_IMPL_ISUPPORTS(WorkletFetchHandler, nsIStreamLoaderObserver)

// ---------------------------------------------------------------------------
// Worklet

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Worklet, mWindow, mScope)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Worklet)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Worklet)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Worklet)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Worklet::Worklet(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
                 WorkletType aWorkletType)
  : mWindow(aWindow)
  , mPrincipal(aPrincipal)
  , mWorkletType(aWorkletType)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aPrincipal);

#ifdef RELEASE_OR_BETA
  MOZ_CRASH("This code should not go to release/beta yet!");
#endif
}

Worklet::~Worklet()
{}

JSObject*
Worklet::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WorkletBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
Worklet::Import(const nsAString& aModuleURL, CallerType aCallerType,
                ErrorResult& aRv)
{
  return WorkletFetchHandler::Fetch(this, aModuleURL, aCallerType, aRv);
}

WorkletGlobalScope*
Worklet::GetOrCreateGlobalScope(JSContext* aCx)
{
  if (!mScope) {
    switch (mWorkletType) {
      case eAudioWorklet:
        mScope = new AudioWorkletGlobalScope(mWindow);
        break;
      case ePaintWorklet:
        mScope = new PaintWorkletGlobalScope(mWindow);
        break;
    }

    JS::Rooted<JSObject*> global(aCx);
    NS_ENSURE_TRUE(mScope->WrapGlobalObject(aCx, mPrincipal, &global), nullptr);

    JSAutoCompartment ac(aCx, global);

    // Init Web IDL bindings
    if (!RegisterWorkletBindings(aCx, global)) {
      mScope = nullptr;
      return nullptr;
    }

    JS_FireOnNewGlobalObject(aCx, global);
  }

  return mScope;
}

WorkletFetchHandler*
Worklet::GetImportFetchHandler(const nsACString& aURI)
{
  return mImportHandlers.GetWeak(aURI);
}

void
Worklet::AddImportFetchHandler(const nsACString& aURI,
                               WorkletFetchHandler* aHandler)
{
  MOZ_ASSERT(aHandler);
  MOZ_ASSERT(!mImportHandlers.GetWeak(aURI));

  mImportHandlers.Put(aURI, aHandler);
}

} // dom namespace
} // mozilla namespace
