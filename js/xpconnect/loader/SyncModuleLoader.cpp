/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SyncModuleLoader.h"

#include "nsISupportsImpl.h"

#include "js/loader/ModuleLoadRequest.h"
#include "js/RootingAPI.h"          // JS::Rooted
#include "js/PropertyAndElement.h"  // JS_SetProperty
#include "js/Value.h"               // JS::Value, JS::NumberValue
#include "mozJSModuleLoader.h"

using namespace JS::loader;

namespace mozilla {
namespace loader {

//////////////////////////////////////////////////////////////
// SyncScriptLoader
//////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS0(SyncScriptLoader)

nsIURI* SyncScriptLoader::GetBaseURI() const { return nullptr; }

void SyncScriptLoader::ReportErrorToConsole(ScriptLoadRequest* aRequest,
                                            nsresult aResult) const {}

void SyncScriptLoader::ReportWarningToConsole(
    ScriptLoadRequest* aRequest, const char* aMessageName,
    const nsTArray<nsString>& aParams) const {}

nsresult SyncScriptLoader::FillCompileOptionsForRequest(
    JSContext* cx, ScriptLoadRequest* aRequest, JS::CompileOptions* aOptions,
    JS::MutableHandle<JSScript*> aIntroductionScript) {
  return NS_OK;
}

//////////////////////////////////////////////////////////////
// SyncModuleLoader
//////////////////////////////////////////////////////////////

NS_IMPL_ADDREF_INHERITED(SyncModuleLoader, JS::loader::ModuleLoaderBase)
NS_IMPL_RELEASE_INHERITED(SyncModuleLoader, JS::loader::ModuleLoaderBase)

NS_IMPL_CYCLE_COLLECTION_INHERITED(SyncModuleLoader,
                                   JS::loader::ModuleLoaderBase, mLoadRequests)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SyncModuleLoader)
NS_INTERFACE_MAP_END_INHERITING(JS::loader::ModuleLoaderBase)

SyncModuleLoader::SyncModuleLoader(SyncScriptLoader* aScriptLoader,
                                   nsIGlobalObject* aGlobalObject)
    : ModuleLoaderBase(aScriptLoader, aGlobalObject) {}

SyncModuleLoader::~SyncModuleLoader() { MOZ_ASSERT(mLoadRequests.isEmpty()); }

already_AddRefed<ModuleLoadRequest> SyncModuleLoader::CreateStaticImport(
    nsIURI* aURI, ModuleLoadRequest* aParent) {
  RefPtr<SyncLoadContext> context = new SyncLoadContext();
  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      aURI, aParent->ReferrerPolicy(), aParent->mFetchOptions,
      dom::SRIMetadata(), aParent->mURI, context, false, /* is top level */
      false,                                             /* is dynamic import */
      this, aParent->mVisitedSet, aParent->GetRootModule());
  request->NoCacheEntryFound();
  return request.forget();
}

already_AddRefed<ModuleLoadRequest> SyncModuleLoader::CreateDynamicImport(
    JSContext* aCx, nsIURI* aURI, LoadedScript* aMaybeActiveScript,
    JS::Handle<JSString*> aSpecifier, JS::Handle<JSObject*> aPromise) {
  RefPtr<SyncLoadContext> context = new SyncLoadContext();
  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      aURI, aMaybeActiveScript->ReferrerPolicy(),
      aMaybeActiveScript->GetFetchOptions(), dom::SRIMetadata(),
      aMaybeActiveScript->BaseURL(), context,
      /* aIsTopLevel = */ true, /* aIsDynamicImport =  */ true, this,
      ModuleLoadRequest::NewVisitedSetForTopLevelImport(aURI), nullptr);

  request->SetDynamicImport(aMaybeActiveScript, aSpecifier, aPromise);
  request->NoCacheEntryFound();

  return request.forget();
}

void SyncModuleLoader::OnDynamicImportStarted(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsDynamicImport());
  MOZ_ASSERT(!mLoadRequests.Contains(aRequest));

  if (aRequest->IsFetching()) {
    // This module is newly imported.
    //
    // DynamicImportRequests() can contain multiple requests when a dynamic
    // import is performed while evaluating the top-level script of other
    // dynamic imports.
    //
    // mLoadRequests should be empty given evaluation is performed after
    // handling all fetching requests.
    MOZ_ASSERT(DynamicImportRequests().Contains(aRequest));
    MOZ_ASSERT(mLoadRequests.isEmpty());

    nsresult rv = OnFetchComplete(aRequest, NS_OK);
    if (NS_FAILED(rv)) {
      mLoadRequests.CancelRequestsAndClear();
      CancelDynamicImport(aRequest, rv);
      return;
    }

    rv = ProcessRequests();
    if (NS_FAILED(rv)) {
      CancelDynamicImport(aRequest, rv);
      return;
    }
  } else {
    // This module had already been imported.
    MOZ_ASSERT(DynamicImportRequests().isEmpty());
    MOZ_ASSERT(mLoadRequests.isEmpty());
  }

  ProcessDynamicImport(aRequest);
}

bool SyncModuleLoader::CanStartLoad(ModuleLoadRequest* aRequest,
                                    nsresult* aRvOut) {
  return mozJSModuleLoader::IsTrustedScheme(aRequest->mURI);
}

nsresult SyncModuleLoader::StartFetch(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->HasLoadContext());

  aRequest->mBaseURL = aRequest->mURI;

  // Loading script source and compilation are intertwined in
  // mozJSModuleLoader. Perform both operations here but only report load
  // failures. Compilation failure is reported in CompileFetchedModule.

  dom::AutoJSAPI jsapi;
  if (!jsapi.Init(GetGlobalObject())) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::RootedScript script(cx);
  nsresult rv =
      mozJSModuleLoader::LoadSingleModuleScript(this, cx, aRequest, &script);
  MOZ_ASSERT_IF(jsapi.HasException(), NS_FAILED(rv));
  MOZ_ASSERT(bool(script) == NS_SUCCEEDED(rv));

  // Check for failure to load script source and abort.
  bool threwException = jsapi.HasException();
  if (NS_FAILED(rv) && !threwException) {
    nsAutoCString uri;
    nsresult rv2 = aRequest->mURI->GetSpec(uri);
    NS_ENSURE_SUCCESS(rv2, rv2);

    JS_ReportErrorUTF8(cx, "Failed to load %s", PromiseFlatCString(uri).get());

    // Remember the error for MaybeReportLoadError.
    if (!mLoadException.initialized()) {
      mLoadException.init(cx);
    }
    if (!jsapi.StealException(&mLoadException)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (mLoadException.isObject()) {
      // Expose `nsresult`.
      JS::Rooted<JS::Value> resultVal(cx, JS::NumberValue(uint32_t(rv)));
      JS::Rooted<JSObject*> exceptionObj(cx, &mLoadException.toObject());
      if (!JS_SetProperty(cx, exceptionObj, "result", resultVal)) {
        // Ignore the error and keep reporting the exception without the result
        // property.
        JS_ClearPendingException(cx);
      }
    }

    return rv;
  }

  // Otherwise remember the results in this context so we can report them later.
  SyncLoadContext* context = aRequest->GetSyncLoadContext();
  context->mRv = rv;
  if (threwException) {
    context->mExceptionValue.init(cx);
    if (!jsapi.StealException(&context->mExceptionValue)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  if (script) {
    context->mScript.init(cx);
    context->mScript = script;
  }

  if (!aRequest->IsDynamicImport()) {
    // NOTE: Dynamic import is stored into mDynamicImportRequests.
    mLoadRequests.AppendElement(aRequest);
  }

  return NS_OK;
}

nsresult SyncModuleLoader::CompileFetchedModule(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal, JS::CompileOptions& aOptions,
    ModuleLoadRequest* aRequest, JS::MutableHandle<JSObject*> aModuleOut) {
  // Compilation already happened in StartFetch. Report the result here.
  SyncLoadContext* context = aRequest->GetSyncLoadContext();
  nsresult rv = context->mRv;
  if (context->mScript) {
    aModuleOut.set(JS::GetModuleObject(context->mScript));
    context->mScript = nullptr;
  }
  if (NS_FAILED(rv)) {
    JS_SetPendingException(aCx, context->mExceptionValue);
    context->mExceptionValue = JS::UndefinedValue();
  }

  MOZ_ASSERT(JS_IsExceptionPending(aCx) == NS_FAILED(rv));
  MOZ_ASSERT(bool(aModuleOut) == NS_SUCCEEDED(rv));

  return rv;
}

void SyncModuleLoader::MaybeReportLoadError(JSContext* aCx) {
  if (JS_IsExceptionPending(aCx)) {
    // Do not override.
    return;
  }

  if (mLoadException.isUndefined()) {
    return;
  }

  JS_SetPendingException(aCx, mLoadException);
  mLoadException = JS::UndefinedValue();
}

void SyncModuleLoader::OnModuleLoadComplete(ModuleLoadRequest* aRequest) {}

nsresult SyncModuleLoader::ProcessRequests() {
  // Work list to drive module loader since this is all synchronous.
  while (!mLoadRequests.isEmpty()) {
    RefPtr<ScriptLoadRequest> request = mLoadRequests.StealFirst();
    nsresult rv = OnFetchComplete(request->AsModuleRequest(), NS_OK);
    if (NS_FAILED(rv)) {
      mLoadRequests.CancelRequestsAndClear();
      return rv;
    }
  }

  return NS_OK;
}

}  // namespace loader
}  // namespace mozilla
