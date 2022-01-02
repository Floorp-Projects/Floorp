/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrecompiledScript.h"

#include "nsIIncrementalStreamLoader.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CompilationAndEvaluation.h"
#include "js/experimental/JSStencil.h"  // JS::CompileGlobalScriptToStencil, JS::InstantiateGlobalStencil, JS::OffThreadCompileToStencil
#include "js/SourceText.h"
#include "js/Utility.h"

#include "mozilla/Attributes.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsCCUncollectableMarker.h"
#include "nsCycleCollectionParticipant.h"

using namespace JS;
using namespace mozilla;
using namespace mozilla::dom;

class AsyncScriptCompiler final : public nsIIncrementalStreamLoaderObserver,
                                  public Runnable {
 public:
  // Note: References to this class are never held by cycle-collected objects.
  // If at any point a reference is returned to a caller, please update this
  // class to implement cycle collection.
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIINCREMENTALSTREAMLOADEROBSERVER
  NS_DECL_NSIRUNNABLE

  AsyncScriptCompiler(JSContext* aCx, nsIGlobalObject* aGlobal,
                      const nsACString& aURL, Promise* aPromise)
      : mozilla::Runnable("AsyncScriptCompiler"),
        mOptions(aCx),
        mURL(aURL),
        mGlobalObject(aGlobal),
        mPromise(aPromise),
        mToken(nullptr),
        mScriptLength(0) {}

  [[nodiscard]] nsresult Start(JSContext* aCx,
                               const CompileScriptOptionsDictionary& aOptions,
                               nsIPrincipal* aPrincipal);

  inline void SetToken(JS::OffThreadToken* aToken) { mToken = aToken; }

 protected:
  virtual ~AsyncScriptCompiler() {
    if (mPromise->State() == Promise::PromiseState::Pending) {
      mPromise->MaybeReject(NS_ERROR_FAILURE);
    }
  }

 private:
  void Reject(JSContext* aCx);
  void Reject(JSContext* aCx, const char* aMxg);

  bool StartCompile(JSContext* aCx);
  void FinishCompile(JSContext* aCx);
  void Finish(JSContext* aCx, RefPtr<JS::Stencil> aStencil);

  OwningCompileOptions mOptions;
  nsCString mURL;
  nsCOMPtr<nsIGlobalObject> mGlobalObject;
  RefPtr<Promise> mPromise;
  nsString mCharset;
  JS::OffThreadToken* mToken;
  UniqueTwoByteChars mScriptText;
  size_t mScriptLength;
};

NS_IMPL_QUERY_INTERFACE_INHERITED(AsyncScriptCompiler, Runnable,
                                  nsIIncrementalStreamLoaderObserver)
NS_IMPL_ADDREF_INHERITED(AsyncScriptCompiler, Runnable)
NS_IMPL_RELEASE_INHERITED(AsyncScriptCompiler, Runnable)

nsresult AsyncScriptCompiler::Start(
    JSContext* aCx, const CompileScriptOptionsDictionary& aOptions,
    nsIPrincipal* aPrincipal) {
  mCharset = aOptions.mCharset;

  CompileOptions options(aCx);
  options.setFile(mURL.get()).setNoScriptRval(!aOptions.mHasReturnValue);

  if (!aOptions.mLazilyParse) {
    options.setForceFullParse();
  }

  if (NS_WARN_IF(!mOptions.copy(aCx, options))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), mURL);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(
      getter_AddRefs(channel), uri, aPrincipal,
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      nsIContentPolicy::TYPE_INTERNAL_CHROMEUTILS_COMPILED_SCRIPT);
  NS_ENSURE_SUCCESS(rv, rv);

  // allow deprecated HTTP request from SystemPrincipal
  nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
  loadInfo->SetAllowDeprecatedSystemRequests(true);
  nsCOMPtr<nsIIncrementalStreamLoader> loader;
  rv = NS_NewIncrementalStreamLoader(getter_AddRefs(loader), this);
  NS_ENSURE_SUCCESS(rv, rv);

  return channel->AsyncOpen(loader);
}

static void OffThreadScriptLoaderCallback(JS::OffThreadToken* aToken,
                                          void* aCallbackData) {
  RefPtr<AsyncScriptCompiler> scriptCompiler =
      dont_AddRef(static_cast<AsyncScriptCompiler*>(aCallbackData));

  scriptCompiler->SetToken(aToken);

  SchedulerGroup::Dispatch(TaskCategory::Other, scriptCompiler.forget());
}

bool AsyncScriptCompiler::StartCompile(JSContext* aCx) {
  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(aCx, std::move(mScriptText), mScriptLength)) {
    return false;
  }

  if (JS::CanCompileOffThread(aCx, mOptions, mScriptLength)) {
    if (!JS::CompileToStencilOffThread(aCx, mOptions, srcBuf,
                                       OffThreadScriptLoaderCallback,
                                       static_cast<void*>(this))) {
      return false;
    }

    NS_ADDREF(this);
    return true;
  }

  RefPtr<Stencil> stencil =
      JS::CompileGlobalScriptToStencil(aCx, mOptions, srcBuf);
  if (!stencil) {
    return false;
  }

  Finish(aCx, stencil);
  return true;
}

NS_IMETHODIMP
AsyncScriptCompiler::Run() {
  AutoJSAPI jsapi;
  if (jsapi.Init(mGlobalObject)) {
    FinishCompile(jsapi.cx());
  } else {
    jsapi.Init();
    JS::CancelOffThreadCompileToStencil(jsapi.cx(), mToken);

    mPromise->MaybeReject(NS_ERROR_FAILURE);
  }

  return NS_OK;
}

void AsyncScriptCompiler::FinishCompile(JSContext* aCx) {
  RefPtr<JS::Stencil> stencil =
      JS::FinishOffThreadCompileToStencil(aCx, mToken);
  if (stencil) {
    Finish(aCx, stencil);
  } else {
    Reject(aCx);
  }
}

void AsyncScriptCompiler::Finish(JSContext* aCx, RefPtr<JS::Stencil> aStencil) {
  RefPtr<PrecompiledScript> result =
      new PrecompiledScript(mGlobalObject, aStencil, mOptions);

  mPromise->MaybeResolve(result);
}

void AsyncScriptCompiler::Reject(JSContext* aCx) {
  RootedValue value(aCx, JS::UndefinedValue());
  if (JS_GetPendingException(aCx, &value)) {
    JS_ClearPendingException(aCx);
  }
  mPromise->MaybeReject(value);
}

void AsyncScriptCompiler::Reject(JSContext* aCx, const char* aMsg) {
  nsAutoString msg;
  msg.AppendASCII(aMsg);
  msg.AppendLiteral(": ");
  AppendUTF8toUTF16(mURL, msg);

  RootedValue exn(aCx);
  if (xpc::NonVoidStringToJsval(aCx, msg, &exn)) {
    JS_SetPendingException(aCx, exn);
  }

  Reject(aCx);
}

NS_IMETHODIMP
AsyncScriptCompiler::OnIncrementalData(nsIIncrementalStreamLoader* aLoader,
                                       nsISupports* aContext,
                                       uint32_t aDataLength,
                                       const uint8_t* aData,
                                       uint32_t* aConsumedData) {
  return NS_OK;
}

NS_IMETHODIMP
AsyncScriptCompiler::OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                                      nsISupports* aContext, nsresult aStatus,
                                      uint32_t aLength, const uint8_t* aBuf) {
  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobalObject)) {
    mPromise->MaybeReject(NS_ERROR_FAILURE);
    return NS_OK;
  }

  JSContext* cx = jsapi.cx();

  if (NS_FAILED(aStatus)) {
    Reject(cx, "Unable to load script");
    return NS_OK;
  }

  nsresult rv = ScriptLoader::ConvertToUTF16(
      nullptr, aBuf, aLength, mCharset, nullptr, mScriptText, mScriptLength);
  if (NS_FAILED(rv)) {
    Reject(cx, "Unable to decode script");
    return NS_OK;
  }

  if (!StartCompile(cx)) {
    Reject(cx);
  }

  return NS_OK;
}

namespace mozilla {
namespace dom {

/* static */
already_AddRefed<Promise> ChromeUtils::CompileScript(
    GlobalObject& aGlobal, const nsAString& aURL,
    const CompileScriptOptionsDictionary& aOptions, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  NS_ConvertUTF16toUTF8 url(aURL);
  RefPtr<AsyncScriptCompiler> compiler =
      new AsyncScriptCompiler(aGlobal.Context(), global, url, promise);

  nsresult rv = compiler->Start(aGlobal.Context(), aOptions,
                                aGlobal.GetSubjectPrincipal());
  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
  }

  return promise.forget();
}

PrecompiledScript::PrecompiledScript(nsISupports* aParent,
                                     RefPtr<JS::Stencil> aStencil,
                                     JS::ReadOnlyCompileOptions& aOptions)
    : mParent(aParent),
      mStencil(aStencil),
      mURL(aOptions.filename()),
      mHasReturnValue(!aOptions.noScriptRval) {
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aStencil);
#ifdef DEBUG
  JS::InstantiateOptions options(aOptions);
  options.assertDefault();
#endif
};

void PrecompiledScript::ExecuteInGlobal(JSContext* aCx, HandleObject aGlobal,
                                        MutableHandleValue aRval,
                                        ErrorResult& aRv) {
  {
    RootedObject targetObj(aCx, JS_FindCompilationScope(aCx, aGlobal));
    JSAutoRealm ar(aCx, targetObj);

    // See assertion in constructor.
    JS::InstantiateOptions options;
    Rooted<JSScript*> script(
        aCx, JS::InstantiateGlobalStencil(aCx, options, mStencil));
    if (!script) {
      aRv.NoteJSContextException(aCx);
      return;
    }

    if (!JS_ExecuteScript(aCx, script, aRval)) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }

  JS_WrapValue(aCx, aRval);
}

void PrecompiledScript::GetUrl(nsAString& aUrl) { CopyUTF8toUTF16(mURL, aUrl); }

bool PrecompiledScript::HasReturnValue() { return mHasReturnValue; }

JSObject* PrecompiledScript::WrapObject(JSContext* aCx,
                                        HandleObject aGivenProto) {
  return PrecompiledScript_Binding::Wrap(aCx, this, aGivenProto);
}

bool PrecompiledScript::IsBlackForCC(bool aTracingNeeded) {
  return (nsCCUncollectableMarker::sGeneration && HasKnownLiveWrapper() &&
          (!aTracingNeeded || HasNothingToTrace(this)));
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PrecompiledScript, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PrecompiledScript)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(PrecompiledScript)
  return tmp->IsBlackForCC(false);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(PrecompiledScript)
  return tmp->IsBlackForCC(true);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(PrecompiledScript)
  return tmp->IsBlackForCC(false);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PrecompiledScript)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PrecompiledScript)

}  // namespace dom
}  // namespace mozilla
