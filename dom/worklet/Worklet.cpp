/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Worklet.h"
#include "WorkletGlobalScope.h"
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

namespace {

class WorkletFetchHandler : public PromiseNativeHandler
                          , public nsIStreamLoaderObserver
{
public:
  NS_DECL_ISUPPORTS

  static already_AddRefed<Promise>
  Fetch(Worklet* aWorklet, const nsAString& aModuleURL, ErrorResult& aRv)
  {
    RefPtr<Promise> promise = Promise::Create(aWorklet->GetParentObject(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    RequestOrUSVString request;
    request.SetAsUSVString().Rebind(aModuleURL.Data(), aModuleURL.Length());

    RequestInit init;

    RefPtr<Promise> fetchPromise =
      FetchRequest(aWorklet->GetParentObject(), request, init, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      promise->MaybeReject(aRv);
      return promise.forget();
    }

    RefPtr<WorkletFetchHandler> handler =
      new WorkletFetchHandler(aWorklet, aModuleURL, promise);
    fetchPromise->AppendNativeHandler(handler);

    return promise.forget();
  }

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    if (!aValue.isObject()) {
      mPromise->MaybeReject(NS_ERROR_FAILURE);
      return;
    }

    RefPtr<Response> response;
    nsresult rv = UNWRAP_OBJECT(Response, &aValue.toObject(), response);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(rv);
      return;
    }

    if (!response->Ok()) {
      mPromise->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
      return;
    }

    nsCOMPtr<nsIInputStream> inputStream;
    response->GetBody(getter_AddRefs(inputStream));
    if (!inputStream) {
      mPromise->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
      return;
    }

    nsCOMPtr<nsIInputStreamPump> pump;
    rv = NS_NewInputStreamPump(getter_AddRefs(pump), inputStream);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(rv);
      return;
    }

    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(rv);
      return;
    }

    rv = pump->AsyncRead(loader, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(rv);
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
      mPromise->MaybeReject(aStatus);
      return NS_OK;
    }

    char16_t* scriptTextBuf;
    size_t scriptTextLength;
    nsresult rv =
      nsScriptLoader::ConvertToUTF16(nullptr, aString, aStringLen,
                                     NS_LITERAL_STRING("UTF-8"), nullptr,
                                     scriptTextBuf, scriptTextLength);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(rv);
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

    // We only need the setNoScriptRval bit when compiling off-thread here,
    // since otherwise nsJSUtils::EvaluateString will set it up for us.
    compileOptions.setNoScriptRval(true);

    JS::Rooted<JS::Value> unused(cx);
    if (!JS::Evaluate(cx, compileOptions, buffer, &unused)) {
      ErrorResult error;
      error.StealExceptionFromJSContext(cx);
      mPromise->MaybeReject(error);
      return NS_OK;
    }

    // All done.
    mPromise->MaybeResolveWithUndefined();
    return NS_OK;
  }

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    mPromise->MaybeReject(aCx, aValue);
  }

private:
  WorkletFetchHandler(Worklet* aWorklet, const nsAString& aURL,
                      Promise* aPromise)
    : mWorklet(aWorklet)
    , mPromise(aPromise)
    , mURL(aURL)
  {}

  ~WorkletFetchHandler()
  {}

  RefPtr<Worklet> mWorklet;
  RefPtr<Promise> mPromise;

  nsString mURL;
};

NS_IMPL_ISUPPORTS(WorkletFetchHandler, nsIStreamLoaderObserver)

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Worklet, mGlobal, mScope)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Worklet)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Worklet)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Worklet)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Worklet::Worklet(nsIGlobalObject* aGlobal, nsIPrincipal* aPrincipal)
  : mGlobal(aGlobal)
  , mPrincipal(aPrincipal)
{}

Worklet::~Worklet()
{}

JSObject*
Worklet::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WorkletBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
Worklet::Import(const nsAString& aModuleURL, ErrorResult& aRv)
{
  return WorkletFetchHandler::Fetch(this, aModuleURL, aRv);
}

WorkletGlobalScope*
Worklet::GetOrCreateGlobalScope(JSContext* aCx)
{
  if (!mScope) {
    mScope = new WorkletGlobalScope();

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

} // dom namespace
} // mozilla namespace
