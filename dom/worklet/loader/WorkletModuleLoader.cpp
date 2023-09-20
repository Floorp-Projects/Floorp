/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkletModuleLoader.h"

#include "js/CompileOptions.h"  // JS::InstantiateOptions
#include "js/experimental/JSStencil.h"  // JS::CompileModuleScriptToStencil, JS::InstantiateModuleStencil
#include "js/loader/ModuleLoadRequest.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/Worklet.h"
#include "mozilla/dom/WorkletFetchHandler.h"
#include "nsStringBundle.h"

using JS::loader::ModuleLoadRequest;
using JS::loader::ResolveError;

namespace mozilla::dom::loader {

//////////////////////////////////////////////////////////////
// WorkletScriptLoader
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkletScriptLoader)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(WorkletScriptLoader)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WorkletScriptLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WorkletScriptLoader)

//////////////////////////////////////////////////////////////
// WorkletModuleLoader
//////////////////////////////////////////////////////////////

NS_IMPL_ADDREF_INHERITED(WorkletModuleLoader, ModuleLoaderBase)
NS_IMPL_RELEASE_INHERITED(WorkletModuleLoader, ModuleLoaderBase)

NS_IMPL_CYCLE_COLLECTION_INHERITED(WorkletModuleLoader, ModuleLoaderBase,
                                   mFetchingRequests)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkletModuleLoader)
NS_INTERFACE_MAP_END_INHERITING(ModuleLoaderBase)

WorkletModuleLoader::WorkletModuleLoader(WorkletScriptLoader* aScriptLoader,
                                         nsIGlobalObject* aGlobalObject)
    : ModuleLoaderBase(aScriptLoader, aGlobalObject,
                       GetCurrentSerialEventTarget()) {
  // This should be constructed on a worklet thread.
  MOZ_ASSERT(!NS_IsMainThread());
}

already_AddRefed<ModuleLoadRequest> WorkletModuleLoader::CreateStaticImport(
    nsIURI* aURI, ModuleLoadRequest* aParent) {
  const nsMainThreadPtrHandle<WorkletFetchHandler>& handlerRef =
      aParent->GetWorkletLoadContext()->GetHandlerRef();
  RefPtr<WorkletLoadContext> loadContext = new WorkletLoadContext(handlerRef);

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-the-descendants-of-a-module-script
  // Step 11. Perform the internal module script graph fetching procedure
  //
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure
  // Step 5. Fetch a single module script with referrer is referringScript's
  // base URL,
  nsIURI* referrer = aParent->mURI;
  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      aURI, aParent->ReferrerPolicy(), aParent->mFetchOptions, SRIMetadata(),
      referrer, loadContext, false, /* is top level */
      false,                        /* is dynamic import */
      this, aParent->mVisitedSet, aParent->GetRootModule());

  request->mURL = request->mURI->GetSpecOrDefault();
  return request.forget();
}

already_AddRefed<ModuleLoadRequest> WorkletModuleLoader::CreateDynamicImport(
    JSContext* aCx, nsIURI* aURI, LoadedScript* aMaybeActiveScript,
    JS::Handle<JS::Value> aReferencingPrivate, JS::Handle<JSString*> aSpecifier,
    JS::Handle<JSObject*> aPromise) {
  return nullptr;
}

bool WorkletModuleLoader::CanStartLoad(ModuleLoadRequest* aRequest,
                                       nsresult* aRvOut) {
  return true;
}

nsresult WorkletModuleLoader::StartFetch(ModuleLoadRequest* aRequest) {
  InsertRequest(aRequest->mURI, aRequest);

  RefPtr<StartFetchRunnable> runnable =
      new StartFetchRunnable(aRequest->GetWorkletLoadContext()->GetHandlerRef(),
                             aRequest->mURI, aRequest->mReferrer);
  NS_DispatchToMainThread(runnable.forget());
  return NS_OK;
}

nsresult WorkletModuleLoader::CompileFetchedModule(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal, JS::CompileOptions& aOptions,
    ModuleLoadRequest* aRequest, JS::MutableHandle<JSObject*> aModuleScript) {
  RefPtr<JS::Stencil> stencil;
  MOZ_ASSERT(aRequest->IsTextSource());

  MaybeSourceText maybeSource;
  nsresult rv = aRequest->GetScriptSource(aCx, &maybeSource);
  NS_ENSURE_SUCCESS(rv, rv);

  auto compile = [&](auto& source) {
    return JS::CompileModuleScriptToStencil(aCx, aOptions, source);
  };
  stencil = maybeSource.mapNonEmpty(compile);

  if (!stencil) {
    return NS_ERROR_FAILURE;
  }

  JS::InstantiateOptions instantiateOptions(aOptions);
  aModuleScript.set(
      JS::InstantiateModuleStencil(aCx, instantiateOptions, stencil));
  return aModuleScript ? NS_OK : NS_ERROR_FAILURE;
}

// AddModuleResultRunnable is a Runnable which will notify the result of
// Worklet::AddModule on the main thread.
class AddModuleResultRunnable final : public Runnable {
 public:
  explicit AddModuleResultRunnable(
      const nsMainThreadPtrHandle<WorkletFetchHandler>& aHandlerRef,
      bool aSucceeded)
      : Runnable("Worklet::AddModuleResultRunnable"),
        mHandlerRef(aHandlerRef),
        mSucceeded(aSucceeded) {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  ~AddModuleResultRunnable() = default;

  NS_IMETHOD
  Run() override;

 private:
  nsMainThreadPtrHandle<WorkletFetchHandler> mHandlerRef;
  bool mSucceeded;
};

NS_IMETHODIMP
AddModuleResultRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mSucceeded) {
    mHandlerRef->ExecutionSucceeded();
  } else {
    mHandlerRef->ExecutionFailed();
  }

  return NS_OK;
}

class AddModuleThrowErrorRunnable final : public Runnable,
                                          public StructuredCloneHolder {
 public:
  explicit AddModuleThrowErrorRunnable(
      const nsMainThreadPtrHandle<WorkletFetchHandler>& aHandlerRef)
      : Runnable("Worklet::AddModuleThrowErrorRunnable"),
        StructuredCloneHolder(CloningSupported, TransferringNotSupported,
                              StructuredCloneScope::SameProcess),
        mHandlerRef(aHandlerRef) {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  ~AddModuleThrowErrorRunnable() = default;

  NS_IMETHOD
  Run() override;

 private:
  nsMainThreadPtrHandle<WorkletFetchHandler> mHandlerRef;
};

NS_IMETHODIMP
AddModuleThrowErrorRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> global =
      do_QueryInterface(mHandlerRef->mWorklet->GetParentObject());
  MOZ_ASSERT(global);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(global))) {
    mHandlerRef->ExecutionFailed();
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> error(cx);
  ErrorResult result;
  Read(global, cx, &error, result);
  Unused << NS_WARN_IF(result.Failed());
  mHandlerRef->ExecutionFailed(error);

  return NS_OK;
}

void WorkletModuleLoader::OnModuleLoadComplete(ModuleLoadRequest* aRequest) {
  if (!aRequest->IsTopLevel()) {
    return;
  }

  const nsMainThreadPtrHandle<WorkletFetchHandler>& handlerRef =
      aRequest->GetWorkletLoadContext()->GetHandlerRef();

  auto addModuleFailed = MakeScopeExit([&] {
    RefPtr<AddModuleResultRunnable> runnable =
        new AddModuleResultRunnable(handlerRef, false);
    NS_DispatchToMainThread(runnable.forget());
  });

  if (!aRequest->mModuleScript) {
    return;
  }

  if (!aRequest->InstantiateModuleGraph()) {
    return;
  }

  nsresult rv = aRequest->EvaluateModule();
  if (NS_FAILED(rv)) {
    return;
  }

  bool hasError = aRequest->mModuleScript->HasErrorToRethrow();
  if (hasError) {
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(GetGlobalObject()))) {
      return;
    }

    JSContext* cx = jsapi.cx();
    JS::Rooted<JS::Value> error(cx, aRequest->mModuleScript->ErrorToRethrow());
    RefPtr<AddModuleThrowErrorRunnable> runnable =
        new AddModuleThrowErrorRunnable(handlerRef);
    ErrorResult result;
    runnable->Write(cx, error, result);
    if (NS_WARN_IF(result.Failed())) {
      return;
    }

    addModuleFailed.release();
    NS_DispatchToMainThread(runnable.forget());
    return;
  }

  addModuleFailed.release();
  RefPtr<AddModuleResultRunnable> runnable =
      new AddModuleResultRunnable(handlerRef, true);
  NS_DispatchToMainThread(runnable.forget());
}

// TODO: Bug 1808301: Call FormatLocalizedString from a worklet thread.
nsresult WorkletModuleLoader::GetResolveFailureMessage(
    ResolveError aError, const nsAString& aSpecifier, nsAString& aResult) {
  uint8_t index = static_cast<uint8_t>(aError);
  MOZ_ASSERT(index < static_cast<uint8_t>(ResolveError::Length));
  MOZ_ASSERT(mLocalizedStrs);
  MOZ_ASSERT(!mLocalizedStrs->IsEmpty());
  if (!mLocalizedStrs || NS_WARN_IF(mLocalizedStrs->IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  const nsString& localizedStr = mLocalizedStrs->ElementAt(index);

  AutoTArray<nsString, 1> params;
  params.AppendElement(aSpecifier);

  nsStringBundleBase::FormatString(localizedStr.get(), params, aResult);
  return NS_OK;
}

void WorkletModuleLoader::InsertRequest(nsIURI* aURI,
                                        ModuleLoadRequest* aRequest) {
  mFetchingRequests.InsertOrUpdate(aURI, aRequest);
}

void WorkletModuleLoader::RemoveRequest(nsIURI* aURI) {
  MOZ_ASSERT(mFetchingRequests.Remove(aURI));
}

ModuleLoadRequest* WorkletModuleLoader::GetRequest(nsIURI* aURI) const {
  RefPtr<ModuleLoadRequest> req;
  MOZ_ALWAYS_TRUE(mFetchingRequests.Get(aURI, getter_AddRefs(req)));
  return req;
}

}  // namespace mozilla::dom::loader
