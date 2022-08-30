/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"
#include "LoadedScript.h"
#include "ModuleLoadRequest.h"
#include "ScriptLoadRequest.h"
#include "mozilla/dom/ScriptSettings.h"  // AutoJSAPI
#include "mozilla/dom/ScriptTrace.h"

#include "js/Array.h"  // JS::GetArrayLength
#include "js/CompilationAndEvaluation.h"
#include "js/ContextOptions.h"        // JS::ContextOptionsRef
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "js/Modules.h"  // JS::FinishDynamicModuleImport, JS::{G,S}etModuleResolveHook, JS::Get{ModulePrivate,ModuleScript,RequestedModule{s,Specifier,SourcePos}}, JS::SetModule{DynamicImport,Metadata}Hook
#include "js/OffThreadScriptCompilation.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty, JS_GetElement
#include "js/SourceText.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/ScriptLoadContext.h"
#include "mozilla/CycleCollectedJSContext.h"  // nsAutoMicroTask
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsContentUtils.h"
#include "nsICacheInfoChannel.h"  // nsICacheInfoChannel
#include "nsNetUtil.h"            // NS_NewURI
#include "xpcpublic.h"

using mozilla::Err;
using mozilla::Preferences;
using mozilla::UniquePtr;
using mozilla::WrapNotNull;
using mozilla::dom::AutoJSAPI;

namespace JS::loader {

mozilla::LazyLogModule ModuleLoaderBase::gCspPRLog("CSP");
mozilla::LazyLogModule ModuleLoaderBase::gModuleLoaderBaseLog(
    "ModuleLoaderBase");

#undef LOG
#define LOG(args)                                                           \
  MOZ_LOG(ModuleLoaderBase::gModuleLoaderBaseLog, mozilla::LogLevel::Debug, \
          args)

#define LOG_ENABLED() \
  MOZ_LOG_TEST(ModuleLoaderBase::gModuleLoaderBaseLog, mozilla::LogLevel::Debug)
// ModuleLoaderBase
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ModuleLoaderBase)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(ModuleLoaderBase, mFetchedModules,
                         mDynamicImportRequests, mGlobalObject, mEventTarget,
                         mLoader)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ModuleLoaderBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ModuleLoaderBase)

// static
void ModuleLoaderBase::EnsureModuleHooksInitialized() {
  AutoJSAPI jsapi;
  jsapi.Init();
  JSRuntime* rt = JS_GetRuntime(jsapi.cx());
  if (JS::GetModuleResolveHook(rt)) {
    return;
  }

  JS::SetModuleResolveHook(rt, HostResolveImportedModule);
  JS::SetModuleMetadataHook(rt, HostPopulateImportMeta);
  JS::SetScriptPrivateReferenceHooks(rt, HostAddRefTopLevelScript,
                                     HostReleaseTopLevelScript);
  JS::SetSupportedAssertionsHook(rt, HostGetSupportedImportAssertions);
  JS::SetModuleDynamicImportHook(rt, HostImportModuleDynamically);
}

// 8.1.3.8.1 HostResolveImportedModule(referencingModule, moduleRequest)
/**
 * Implement the HostResolveImportedModule abstract operation.
 *
 * Resolve a module specifier string and look this up in the module
 * map, returning the result. This is only called for previously
 * loaded modules and always succeeds.
 *
 * @param aReferencingPrivate A JS::Value which is either undefined
 *                            or contains a LoadedScript private pointer.
 * @param aModuleRequest A module request object.
 * @returns module This is set to the module found.
 */
// static
JSObject* ModuleLoaderBase::HostResolveImportedModule(
    JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate,
    JS::Handle<JSObject*> aModuleRequest) {
  JS::Rooted<JSObject*> module(aCx);

  {
    // LoadedScript should only live in this block, otherwise it will be a GC
    // hazard
    RefPtr<LoadedScript> script(
        GetLoadedScriptOrNull(aCx, aReferencingPrivate));

    JS::Rooted<JSString*> specifierString(
        aCx, JS::GetModuleRequestSpecifier(aCx, aModuleRequest));
    if (!specifierString) {
      return nullptr;
    }

    // Let url be the result of resolving a module specifier given referencing
    // module script and specifier.
    nsAutoJSString string;
    if (!string.init(aCx, specifierString)) {
      return nullptr;
    }

    RefPtr<ModuleLoaderBase> loader = GetCurrentModuleLoader(aCx);
    if (!loader) {
      return nullptr;
    }

    auto result = loader->ResolveModuleSpecifier(script, string);
    // This cannot fail because resolving a module specifier must have been
    // previously successful with these same two arguments.
    MOZ_ASSERT(result.isOk());
    nsCOMPtr<nsIURI> uri = result.unwrap();
    MOZ_ASSERT(uri, "Failed to resolve previously-resolved module specifier");

    // Let resolved module script be moduleMap[url]. (This entry must exist for
    // us to have gotten to this point.)
    ModuleScript* ms = loader->GetFetchedModule(uri);
    MOZ_ASSERT(ms, "Resolved module not found in module map");
    MOZ_ASSERT(!ms->HasParseError());
    MOZ_ASSERT(ms->ModuleRecord());

    module.set(ms->ModuleRecord());
  }
  return module;
}

// static
bool ModuleLoaderBase::ImportMetaResolve(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedValue modulePrivate(
      cx, js::GetFunctionNativeReserved(&args.callee(), ModulePrivateSlot));

  // https://html.spec.whatwg.org/#hostgetimportmetaproperties
  // Step 4.1. Set specifier to ? ToString(specifier).
  //
  // https://tc39.es/ecma262/#sec-tostring
  RootedValue v(cx, args.get(ImportMetaResolveSpecifierArg));
  RootedString specifier(cx, JS::ToString(cx, v));
  if (!specifier) {
    return false;
  }

  // Step 4.2, 4.3 are implemented in ImportMetaResolveImpl.
  RootedString url(cx, ImportMetaResolveImpl(cx, modulePrivate, specifier));
  if (!url) {
    return false;
  }

  // Step 4.4. Return the serialization of url.
  args.rval().setString(url);
  return true;
}

// static
JSString* ModuleLoaderBase::ImportMetaResolveImpl(
    JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate,
    JS::Handle<JSString*> aSpecifier) {
  RootedString urlString(aCx);

  {
    // ModuleScript should only live in this block, otherwise it will be a GC
    // hazard
    RefPtr<ModuleScript> script =
        static_cast<ModuleScript*>(aReferencingPrivate.toPrivate());
    MOZ_ASSERT(script->IsModuleScript());
    MOZ_ASSERT(JS::GetModulePrivate(script->ModuleRecord()) ==
               aReferencingPrivate);

    RefPtr<ModuleLoaderBase> loader = GetCurrentModuleLoader(aCx);
    if (!loader) {
      return nullptr;
    }

    nsAutoJSString specifier;
    if (!specifier.init(aCx, aSpecifier)) {
      return nullptr;
    }

    auto result = loader->ResolveModuleSpecifier(script, specifier);
    if (result.isErr()) {
      JS::Rooted<JS::Value> error(aCx);
      nsresult rv = HandleResolveFailure(aCx, script, specifier,
                                         result.unwrapErr(), 0, 0, &error);
      if (NS_FAILED(rv)) {
        JS_ReportOutOfMemory(aCx);
        return nullptr;
      }

      JS_SetPendingException(aCx, error);

      return nullptr;
    }

    nsCOMPtr<nsIURI> uri = result.unwrap();
    nsAutoCString url;
    MOZ_ALWAYS_SUCCEEDS(uri->GetAsciiSpec(url));

    urlString.set(JS_NewStringCopyZ(aCx, url.get()));
  }

  return urlString;
}

// static
bool ModuleLoaderBase::HostPopulateImportMeta(
    JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate,
    JS::Handle<JSObject*> aMetaObject) {
  RefPtr<ModuleScript> script =
      static_cast<ModuleScript*>(aReferencingPrivate.toPrivate());
  MOZ_ASSERT(script->IsModuleScript());
  MOZ_ASSERT(JS::GetModulePrivate(script->ModuleRecord()) ==
             aReferencingPrivate);

  nsAutoCString url;
  MOZ_DIAGNOSTIC_ASSERT(script->BaseURL());
  MOZ_ALWAYS_SUCCEEDS(script->BaseURL()->GetAsciiSpec(url));

  JS::Rooted<JSString*> urlString(aCx, JS_NewStringCopyZ(aCx, url.get()));
  if (!urlString) {
    JS_ReportOutOfMemory(aCx);
    return false;
  }

  // https://html.spec.whatwg.org/#import-meta-url
  if (!JS_DefineProperty(aCx, aMetaObject, "url", urlString,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  // https://html.spec.whatwg.org/#import-meta-resolve
  // Define 'resolve' function on the import.meta object.
  JSFunction* resolveFunc = js::DefineFunctionWithReserved(
      aCx, aMetaObject, "resolve", ImportMetaResolve, ImportMetaResolveNumArgs,
      JSPROP_ENUMERATE);
  if (!resolveFunc) {
    return false;
  }

  // Store the 'active script' of the meta object into the function slot.
  // https://html.spec.whatwg.org/#active-script
  RootedObject resolveFuncObj(aCx, JS_GetFunctionObject(resolveFunc));
  js::SetFunctionNativeReserved(resolveFuncObj, ModulePrivateSlot,
                                aReferencingPrivate);

  return true;
}

// static
bool ModuleLoaderBase::HostImportModuleDynamically(
    JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate,
    JS::Handle<JSObject*> aModuleRequest, JS::Handle<JSObject*> aPromise) {
  RefPtr<LoadedScript> script(GetLoadedScriptOrNull(aCx, aReferencingPrivate));

  JS::Rooted<JSString*> specifierString(
      aCx, JS::GetModuleRequestSpecifier(aCx, aModuleRequest));
  if (!specifierString) {
    return false;
  }

  // Attempt to resolve the module specifier.
  nsAutoJSString specifier;
  if (!specifier.init(aCx, specifierString)) {
    return false;
  }

  RefPtr<ModuleLoaderBase> loader = GetCurrentModuleLoader(aCx);
  if (!loader) {
    return false;
  }

  auto result = loader->ResolveModuleSpecifier(script, specifier);
  if (result.isErr()) {
    JS::Rooted<JS::Value> error(aCx);
    nsresult rv = HandleResolveFailure(aCx, script, specifier,
                                       result.unwrapErr(), 0, 0, &error);
    if (NS_FAILED(rv)) {
      JS_ReportOutOfMemory(aCx);
      return false;
    }

    JS_SetPendingException(aCx, error);
    return false;
  }

  // Create a new top-level load request.
  nsCOMPtr<nsIURI> uri = result.unwrap();
  RefPtr<ModuleLoadRequest> request = loader->CreateDynamicImport(
      aCx, uri, script, aReferencingPrivate, specifierString, aPromise);

  if (!request) {
    JS_ReportErrorASCII(aCx, "Dynamic import not supported in this context");
    return false;
  }

  loader->StartDynamicImport(request);
  return true;
}

bool ModuleLoaderBase::HostGetSupportedImportAssertions(
    JSContext* aCx, JS::ImportAssertionVector& aValues) {
  MOZ_ASSERT(aValues.empty());

  if (!aValues.reserve(1)) {
    JS_ReportOutOfMemory(aCx);
    return false;
  }

  aValues.infallibleAppend(JS::ImportAssertion::Type);

  return true;
}

// static
ModuleLoaderBase* ModuleLoaderBase::GetCurrentModuleLoader(JSContext* aCx) {
  auto reportError = mozilla::MakeScopeExit([aCx]() {
    JS_ReportErrorASCII(aCx, "No ScriptLoader found for the current context");
  });

  JS::Rooted<JSObject*> object(aCx, JS::CurrentGlobalOrNull(aCx));
  if (!object) {
    return nullptr;
  }

  nsIGlobalObject* global = xpc::NativeGlobal(object);
  if (!global) {
    return nullptr;
  }

  ModuleLoaderBase* loader = global->GetModuleLoader(aCx);
  if (!loader) {
    return nullptr;
  }

  MOZ_ASSERT(loader->mGlobalObject == global);

  reportError.release();
  return loader;
}

// static
LoadedScript* ModuleLoaderBase::GetLoadedScriptOrNull(
    JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate) {
  if (aReferencingPrivate.isUndefined()) {
    return nullptr;
  }

  auto* script = static_cast<LoadedScript*>(aReferencingPrivate.toPrivate());
  if (script->IsEventScript()) {
    return nullptr;
  }

  MOZ_ASSERT_IF(
      script->IsModuleScript(),
      JS::GetModulePrivate(script->AsModuleScript()->ModuleRecord()) ==
          aReferencingPrivate);

  return script;
}

nsresult ModuleLoaderBase::StartModuleLoad(ModuleLoadRequest* aRequest) {
  return StartOrRestartModuleLoad(aRequest, RestartRequest::No);
}

nsresult ModuleLoaderBase::RestartModuleLoad(ModuleLoadRequest* aRequest) {
  return StartOrRestartModuleLoad(aRequest, RestartRequest::Yes);
}

nsresult ModuleLoaderBase::StartOrRestartModuleLoad(ModuleLoadRequest* aRequest,
                                                    RestartRequest aRestart) {
  MOZ_ASSERT(aRequest->mLoader == this);
  MOZ_ASSERT(aRequest->IsFetching());

  aRequest->SetUnknownDataType();

  // If we're restarting the request, the module should already be in the
  // "fetching" map.
  MOZ_ASSERT_IF(aRestart == RestartRequest::Yes,
                IsModuleFetching(aRequest->mURI));

  // Check with the derived class whether we should load this module.
  nsresult rv = NS_OK;
  if (!CanStartLoad(aRequest, &rv)) {
    return rv;
  }

  // Check whether the module has been fetched or is currently being fetched,
  // and if so wait for it rather than starting a new fetch.
  ModuleLoadRequest* request = aRequest->AsModuleRequest();

  if (aRestart == RestartRequest::No && ModuleMapContainsURL(request->mURI)) {
    LOG(("ScriptLoadRequest (%p): Waiting for module fetch", aRequest));
    WaitForModuleFetch(request->mURI)
        ->Then(mEventTarget, __func__, request,
               &ModuleLoadRequest::ModuleLoaded,
               &ModuleLoadRequest::LoadFailed);
    return NS_OK;
  }

  rv = StartFetch(aRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  // We successfully started fetching a module so put its URL in the module
  // map and mark it as fetching.
  if (aRestart == RestartRequest::No) {
    SetModuleFetchStarted(aRequest->AsModuleRequest());
  }

  return NS_OK;
}

bool ModuleLoaderBase::ModuleMapContainsURL(nsIURI* aURL) const {
  return IsModuleFetching(aURL) || IsModuleFetched(aURL);
}

bool ModuleLoaderBase::IsModuleFetching(nsIURI* aURL) const {
  return mFetchingModules.Contains(aURL);
}

bool ModuleLoaderBase::IsModuleFetched(nsIURI* aURL) const {
  return mFetchedModules.Contains(aURL);
}

nsresult ModuleLoaderBase::GetFetchedModuleURLs(nsTArray<nsCString>& aURLs) {
  for (const auto& entry : mFetchedModules) {
    nsIURI* uri = entry.GetData()->BaseURL();

    nsAutoCString spec;
    nsresult rv = uri->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    aURLs.AppendElement(spec);
  }

  return NS_OK;
}

void ModuleLoaderBase::SetModuleFetchStarted(ModuleLoadRequest* aRequest) {
  // Update the module map to indicate that a module is currently being fetched.

  MOZ_ASSERT(aRequest->IsFetching());
  MOZ_ASSERT(!ModuleMapContainsURL(aRequest->mURI));

  mFetchingModules.InsertOrUpdate(
      aRequest->mURI, RefPtr<mozilla::GenericNonExclusivePromise::Private>{});
}

void ModuleLoaderBase::SetModuleFetchFinishedAndResumeWaitingRequests(
    ModuleLoadRequest* aRequest, nsresult aResult) {
  // Update module map with the result of fetching a single module script.
  //
  // If any requests for the same URL are waiting on this one to complete, they
  // will have ModuleLoaded or LoadFailed on them when the promise is
  // resolved/rejected. This is set up in StartLoad.

  MOZ_ASSERT(aRequest->mLoader == this);

  LOG(
      ("ScriptLoadRequest (%p): Module fetch finished (script == %p, result == "
       "%u)",
       aRequest, aRequest->mModuleScript.get(), unsigned(aResult)));

  RefPtr<mozilla::GenericNonExclusivePromise::Private> promise;
  if (!mFetchingModules.Remove(aRequest->mURI, getter_AddRefs(promise))) {
    LOG(
        ("ScriptLoadRequest (%p): Key not found in mFetchingModules, "
         "assuming we have an inline module or have finished fetching already",
         aRequest));
    return;
  }

  RefPtr<ModuleScript> moduleScript(aRequest->mModuleScript);
  MOZ_ASSERT(NS_FAILED(aResult) == !moduleScript);

  mFetchedModules.InsertOrUpdate(aRequest->mURI, RefPtr{moduleScript});

  if (promise) {
    if (moduleScript) {
      LOG(("ScriptLoadRequest (%p):   resolving %p", aRequest, promise.get()));
      promise->Resolve(true, __func__);
    } else {
      LOG(("ScriptLoadRequest (%p):   rejecting %p", aRequest, promise.get()));
      promise->Reject(aResult, __func__);
    }
  }
}

RefPtr<mozilla::GenericNonExclusivePromise>
ModuleLoaderBase::WaitForModuleFetch(nsIURI* aURL) {
  MOZ_ASSERT(ModuleMapContainsURL(aURL));

  nsURIHashKey key(aURL);
  if (auto entry = mFetchingModules.Lookup(aURL)) {
    if (!entry.Data()) {
      entry.Data() = new mozilla::GenericNonExclusivePromise::Private(__func__);
    }
    return entry.Data();
  }

  RefPtr<ModuleScript> ms;
  MOZ_ALWAYS_TRUE(mFetchedModules.Get(aURL, getter_AddRefs(ms)));
  if (!ms) {
    return mozilla::GenericNonExclusivePromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  return mozilla::GenericNonExclusivePromise::CreateAndResolve(true, __func__);
}

ModuleScript* ModuleLoaderBase::GetFetchedModule(nsIURI* aURL) const {
  if (LOG_ENABLED()) {
    nsAutoCString url;
    aURL->GetAsciiSpec(url);
    LOG(("GetFetchedModule %s", url.get()));
  }

  bool found;
  ModuleScript* ms = mFetchedModules.GetWeak(aURL, &found);
  MOZ_ASSERT(found);
  return ms;
}

nsresult ModuleLoaderBase::OnFetchComplete(ModuleLoadRequest* aRequest,
                                           nsresult aRv) {
  MOZ_ASSERT(aRequest->mLoader == this);
  MOZ_ASSERT(!aRequest->mModuleScript);

  nsresult rv = aRv;
  if (NS_SUCCEEDED(rv)) {
    rv = CreateModuleScript(aRequest);

    aRequest->ClearScriptSource();

    if (NS_FAILED(rv)) {
      aRequest->LoadFailed();
      return rv;
    }
  }

  MOZ_ASSERT(NS_SUCCEEDED(rv) == bool(aRequest->mModuleScript));
  SetModuleFetchFinishedAndResumeWaitingRequests(aRequest, rv);

  if (aRequest->mModuleScript && !aRequest->mModuleScript->HasParseError()) {
    StartFetchingModuleDependencies(aRequest);
  }

  return NS_OK;
}

nsresult ModuleLoaderBase::CreateModuleScript(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(!aRequest->mModuleScript);
  MOZ_ASSERT(aRequest->mBaseURL);

  LOG(("ScriptLoadRequest (%p): Create module script", aRequest));

  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobalObject)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  {
    JSContext* cx = jsapi.cx();
    JS::Rooted<JSObject*> module(cx);

    JS::CompileOptions options(cx);
    JS::RootedScript introductionScript(cx);
    rv = mLoader->FillCompileOptionsForRequest(cx, aRequest, &options,
                                               &introductionScript);

    if (NS_SUCCEEDED(rv)) {
      JS::Rooted<JSObject*> global(cx, mGlobalObject->GetGlobalJSObject());
      rv = CompileFetchedModule(cx, global, options, aRequest, &module);
    }

    MOZ_ASSERT(NS_SUCCEEDED(rv) == (module != nullptr));

    if (module) {
      JS::RootedValue privateValue(cx);
      JS::RootedScript moduleScript(cx, JS::GetModuleScript(module));
      JS::InstantiateOptions instantiateOptions(options);
      if (!JS::UpdateDebugMetadata(cx, moduleScript, instantiateOptions,
                                   privateValue, nullptr, introductionScript,
                                   nullptr)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    RefPtr<ModuleScript> moduleScript =
        new ModuleScript(aRequest->mFetchOptions, aRequest->mBaseURL);
    aRequest->mModuleScript = moduleScript;

    if (!module) {
      LOG(("ScriptLoadRequest (%p):   compilation failed (%d)", aRequest,
           unsigned(rv)));

      MOZ_ASSERT(jsapi.HasException());
      JS::Rooted<JS::Value> error(cx);
      if (!jsapi.StealException(&error)) {
        aRequest->mModuleScript = nullptr;
        return NS_ERROR_FAILURE;
      }

      moduleScript->SetParseError(error);
      aRequest->ModuleErrored();
      return NS_OK;
    }

    moduleScript->SetModuleRecord(module);

    // Validate requested modules and treat failure to resolve module specifiers
    // the same as a parse error.
    rv = ResolveRequestedModules(aRequest, nullptr);
    if (NS_FAILED(rv)) {
      aRequest->ModuleErrored();
      return NS_OK;
    }
  }

  LOG(("ScriptLoadRequest (%p):   module script == %p", aRequest,
       aRequest->mModuleScript.get()));

  return rv;
}

nsresult ModuleLoaderBase::HandleResolveFailure(
    JSContext* aCx, LoadedScript* aScript, const nsAString& aSpecifier,
    ResolveError aError, uint32_t aLineNumber, uint32_t aColumnNumber,
    JS::MutableHandle<JS::Value> aErrorOut) {
  JS::Rooted<JSString*> filename(aCx);
  if (aScript) {
    nsAutoCString url;
    aScript->BaseURL()->GetAsciiSpec(url);
    filename = JS_NewStringCopyZ(aCx, url.get());
  } else {
    filename = JS_NewStringCopyZ(aCx, "(unknown)");
  }

  if (!filename) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  AutoTArray<nsString, 1> errorParams;
  errorParams.AppendElement(aSpecifier);

  nsAutoString errorText;
  nsresult rv = nsContentUtils::FormatLocalizedString(
      nsContentUtils::eDOM_PROPERTIES, ResolveErrorInfo::GetString(aError),
      errorParams, errorText);
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JSString*> string(aCx, JS_NewUCStringCopyZ(aCx, errorText.get()));
  if (!string) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!JS::CreateError(aCx, JSEXN_TYPEERR, nullptr, filename, aLineNumber,
                       aColumnNumber, nullptr, string, JS::NothingHandleValue,
                       aErrorOut)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

// Helper for getting import maps pref across main thread and workers
bool ImportMapsEnabled() {
  if (NS_IsMainThread()) {
    return mozilla::StaticPrefs::dom_importMaps_enabled();
  }
  return false;
}

ResolveResult ModuleLoaderBase::ResolveModuleSpecifier(
    LoadedScript* aScript, const nsAString& aSpecifier) {
  // If import map is enabled, forward to the updated 'Resolve a module
  // specifier' algorithm defined in Import maps spec.
  //
  // Once import map is enabled by default,
  // ModuleLoaderBase::ResolveModuleSpecifier should be replaced by
  // ImportMap::ResolveModuleSpecifier.
  if (ImportMapsEnabled()) {
    return ImportMap::ResolveModuleSpecifier(mImportMap.get(), mLoader, aScript,
                                             aSpecifier);
  }

  // The following module specifiers are allowed by the spec:
  //  - a valid absolute URL
  //  - a valid relative URL that starts with "/", "./" or "../"
  //
  // Bareword module specifiers are handled in Import maps.

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aSpecifier);
  if (NS_SUCCEEDED(rv)) {
    return WrapNotNull(uri);
  }

  if (rv != NS_ERROR_MALFORMED_URI) {
    return Err(ResolveError::Failure);
  }

  if (!StringBeginsWith(aSpecifier, u"/"_ns) &&
      !StringBeginsWith(aSpecifier, u"./"_ns) &&
      !StringBeginsWith(aSpecifier, u"../"_ns)) {
    return Err(ResolveError::FailureMayBeBare);
  }

  // Get the document's base URL if we don't have a referencing script here.
  nsCOMPtr<nsIURI> baseURL;
  if (aScript) {
    baseURL = aScript->BaseURL();
  } else {
    baseURL = mLoader->GetBaseURI();
  }

  rv = NS_NewURI(getter_AddRefs(uri), aSpecifier, nullptr, baseURL);
  if (NS_SUCCEEDED(rv)) {
    return WrapNotNull(uri);
  }

  return Err(ResolveError::Failure);
}

nsresult ModuleLoaderBase::ResolveRequestedModules(
    ModuleLoadRequest* aRequest, nsCOMArray<nsIURI>* aUrlsOut) {
  ModuleScript* ms = aRequest->mModuleScript;

  AutoJSAPI jsapi;
  if (!jsapi.Init(ms->ModuleRecord())) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> moduleRecord(cx, ms->ModuleRecord());
  JS::Rooted<JSObject*> requestedModules(cx);
  requestedModules = JS::GetRequestedModules(cx, moduleRecord);
  MOZ_ASSERT(requestedModules);

  uint32_t length;
  if (!JS::GetArrayLength(cx, requestedModules, &length)) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JS::Value> requestedModule(cx);
  for (uint32_t i = 0; i < length; i++) {
    if (!JS_GetElement(cx, requestedModules, i, &requestedModule)) {
      return NS_ERROR_FAILURE;
    }

    JS::Rooted<JSString*> str(
        cx, JS::GetRequestedModuleSpecifier(cx, requestedModule));
    MOZ_ASSERT(str);

    nsAutoJSString specifier;
    if (!specifier.init(cx, str)) {
      return NS_ERROR_FAILURE;
    }

    // Let url be the result of resolving a module specifier given module script
    // and requested.
    ModuleLoaderBase* loader = aRequest->mLoader;
    auto result = loader->ResolveModuleSpecifier(ms, specifier);
    if (result.isErr()) {
      uint32_t lineNumber = 0;
      uint32_t columnNumber = 0;
      JS::GetRequestedModuleSourcePos(cx, requestedModule, &lineNumber,
                                      &columnNumber);

      JS::Rooted<JS::Value> error(cx);
      nsresult rv = HandleResolveFailure(cx, ms, specifier, result.unwrapErr(),
                                         lineNumber, columnNumber, &error);
      NS_ENSURE_SUCCESS(rv, rv);

      ms->SetParseError(error);
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIURI> uri = result.unwrap();
    if (aUrlsOut) {
      aUrlsOut->AppendElement(uri.forget());
    }
  }

  return NS_OK;
}

void ModuleLoaderBase::StartFetchingModuleDependencies(
    ModuleLoadRequest* aRequest) {
  LOG(("ScriptLoadRequest (%p): Start fetching module dependencies", aRequest));

  if (aRequest->IsCanceled()) {
    return;
  }

  MOZ_ASSERT(aRequest->mModuleScript);
  MOZ_ASSERT(!aRequest->mModuleScript->HasParseError());
  MOZ_ASSERT(!aRequest->IsReadyToRun());

  auto visitedSet = aRequest->mVisitedSet;
  MOZ_ASSERT(visitedSet->Contains(aRequest->mURI));

  aRequest->mState = ModuleLoadRequest::State::LoadingImports;

  nsCOMArray<nsIURI> urls;
  nsresult rv = ResolveRequestedModules(aRequest, &urls);
  if (NS_FAILED(rv)) {
    aRequest->mModuleScript = nullptr;
    aRequest->ModuleErrored();
    return;
  }

  // Remove already visited URLs from the list. Put unvisited URLs into the
  // visited set.
  int32_t i = 0;
  while (i < urls.Count()) {
    nsIURI* url = urls[i];
    if (visitedSet->Contains(url)) {
      urls.RemoveObjectAt(i);
    } else {
      visitedSet->PutEntry(url);
      i++;
    }
  }

  if (urls.Count() == 0) {
    // There are no descendants to load so this request is ready.
    aRequest->DependenciesLoaded();
    return;
  }

  // For each url in urls, fetch a module script graph given url, module
  // script's CORS setting, and module script's settings object.
  nsTArray<RefPtr<mozilla::GenericPromise>> importsReady;
  for (auto* url : urls) {
    RefPtr<mozilla::GenericPromise> childReady =
        StartFetchingModuleAndDependencies(aRequest, url);
    importsReady.AppendElement(childReady);
  }

  // Wait for all imports to become ready.
  RefPtr<mozilla::GenericPromise::AllPromiseType> allReady =
      mozilla::GenericPromise::All(mEventTarget, importsReady);
  allReady->Then(mEventTarget, __func__, aRequest,
                 &ModuleLoadRequest::DependenciesLoaded,
                 &ModuleLoadRequest::ModuleErrored);
}

RefPtr<mozilla::GenericPromise>
ModuleLoaderBase::StartFetchingModuleAndDependencies(ModuleLoadRequest* aParent,
                                                     nsIURI* aURI) {
  MOZ_ASSERT(aURI);

  RefPtr<ModuleLoadRequest> childRequest = CreateStaticImport(aURI, aParent);

  aParent->mImports.AppendElement(childRequest);

  if (LOG_ENABLED()) {
    nsAutoCString url1;
    aParent->mURI->GetAsciiSpec(url1);

    nsAutoCString url2;
    aURI->GetAsciiSpec(url2);

    LOG(("ScriptLoadRequest (%p): Start fetching dependency %p", aParent,
         childRequest.get()));
    LOG(("StartFetchingModuleAndDependencies \"%s\" -> \"%s\"", url1.get(),
         url2.get()));
  }

  RefPtr<mozilla::GenericPromise> ready = childRequest->mReady.Ensure(__func__);

  nsresult rv = StartModuleLoad(childRequest);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(!childRequest->mModuleScript);
    LOG(("ScriptLoadRequest (%p):   rejecting %p", aParent,
         &childRequest->mReady));

    mLoader->ReportErrorToConsole(childRequest, rv);
    childRequest->mReady.Reject(rv, __func__);
    return ready;
  }

  return ready;
}

void ModuleLoaderBase::StartDynamicImport(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->mLoader == this);

  LOG(("ScriptLoadRequest (%p): Start dynamic import", aRequest));

  mDynamicImportRequests.AppendElement(aRequest);

  nsresult rv = StartModuleLoad(aRequest);
  if (NS_FAILED(rv)) {
    mLoader->ReportErrorToConsole(aRequest, rv);
    FinishDynamicImportAndReject(aRequest, rv);
  }
}

void ModuleLoaderBase::FinishDynamicImportAndReject(ModuleLoadRequest* aRequest,
                                                    nsresult aResult) {
  AutoJSAPI jsapi;
  MOZ_ASSERT(NS_FAILED(aResult));
  MOZ_ALWAYS_TRUE(jsapi.Init(aRequest->mDynamicPromise));
  FinishDynamicImport(jsapi.cx(), aRequest, aResult, nullptr);
}

void ModuleLoaderBase::FinishDynamicImport(
    JSContext* aCx, ModuleLoadRequest* aRequest, nsresult aResult,
    JS::Handle<JSObject*> aEvaluationPromise) {
  // If aResult is a failed result, we don't have an EvaluationPromise. If it
  // succeeded, evaluationPromise may still be null, but in this case it will
  // be handled by rejecting the dynamic module import promise in the JSAPI.
  MOZ_ASSERT_IF(NS_FAILED(aResult), !aEvaluationPromise);
  LOG(("ScriptLoadRequest (%p): Finish dynamic import %x %d", aRequest,
       unsigned(aResult), JS_IsExceptionPending(aCx)));

  // Complete the dynamic import, report failures indicated by aResult or as a
  // pending exception on the context.

  if (NS_FAILED(aResult) &&
      aResult != NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW_UNCATCHABLE) {
    MOZ_ASSERT(!JS_IsExceptionPending(aCx));
    JS_ReportErrorNumberUC(aCx, js::GetErrorMessage, nullptr,
                           JSMSG_DYNAMIC_IMPORT_FAILED);
  }

  JS::Rooted<JS::Value> referencingScript(aCx,
                                          aRequest->mDynamicReferencingPrivate);
  JS::Rooted<JSString*> specifier(aCx, aRequest->mDynamicSpecifier);
  JS::Rooted<JSObject*> promise(aCx, aRequest->mDynamicPromise);

  JS::Rooted<JSObject*> moduleRequest(aCx,
                                      JS::CreateModuleRequest(aCx, specifier));

  JS::FinishDynamicModuleImport(aCx, aEvaluationPromise, referencingScript,
                                moduleRequest, promise);

  // FinishDynamicModuleImport clears any pending exception.
  MOZ_ASSERT(!JS_IsExceptionPending(aCx));

  aRequest->ClearDynamicImport();
}

ModuleLoaderBase::ModuleLoaderBase(ScriptLoaderInterface* aLoader,
                                   nsIGlobalObject* aGlobalObject,
                                   nsISerialEventTarget* aEventTarget)
    : mGlobalObject(aGlobalObject),
      mEventTarget(aEventTarget),
      mLoader(aLoader) {
  MOZ_ASSERT(mGlobalObject);
  MOZ_ASSERT(mEventTarget);
  MOZ_ASSERT(mLoader);

  EnsureModuleHooksInitialized();
}

ModuleLoaderBase::~ModuleLoaderBase() {
  mDynamicImportRequests.CancelRequestsAndClear();

  LOG(("ModuleLoaderBase::~ModuleLoaderBase %p", this));
}

void ModuleLoaderBase::Shutdown() {
  MOZ_ASSERT(mFetchingModules.IsEmpty());

  for (const auto& entry : mFetchedModules) {
    entry.GetData()->Shutdown();
  }

  mFetchedModules.Clear();
  mGlobalObject = nullptr;
  mEventTarget = nullptr;
  mLoader = nullptr;
}

bool ModuleLoaderBase::HasPendingDynamicImports() const {
  return !mDynamicImportRequests.isEmpty();
}

void ModuleLoaderBase::CancelDynamicImport(ModuleLoadRequest* aRequest,
                                           nsresult aResult) {
  MOZ_ASSERT(aRequest->mLoader == this);

  RefPtr<ScriptLoadRequest> req = mDynamicImportRequests.Steal(aRequest);
  aRequest->Cancel();
  // FinishDynamicImport must happen exactly once for each dynamic import
  // request. If the load is aborted we do it when we remove the request
  // from mDynamicImportRequests.
  FinishDynamicImportAndReject(aRequest, aResult);
}

void ModuleLoaderBase::RemoveDynamicImport(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsDynamicImport());
  mDynamicImportRequests.Remove(aRequest);
}

#ifdef DEBUG
bool ModuleLoaderBase::HasDynamicImport(
    const ModuleLoadRequest* aRequest) const {
  MOZ_ASSERT(aRequest->mLoader == this);
  return mDynamicImportRequests.Contains(
      const_cast<ModuleLoadRequest*>(aRequest));
}
#endif

JS::Value ModuleLoaderBase::FindFirstParseError(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest);

  ModuleScript* moduleScript = aRequest->mModuleScript;
  MOZ_ASSERT(moduleScript);

  if (moduleScript->HasParseError()) {
    return moduleScript->ParseError();
  }

  for (ModuleLoadRequest* childRequest : aRequest->mImports) {
    JS::Value error = FindFirstParseError(childRequest);
    if (!error.isUndefined()) {
      return error;
    }
  }

  return JS::UndefinedValue();
}

bool ModuleLoaderBase::InstantiateModuleGraph(ModuleLoadRequest* aRequest) {
  // Instantiate a top-level module and record any error.

  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aRequest->mLoader == this);
  MOZ_ASSERT(aRequest->IsTopLevel());

  LOG(("ScriptLoadRequest (%p): Instantiate module graph", aRequest));

  AUTO_PROFILER_LABEL("ModuleLoaderBase::InstantiateModuleGraph", JS);

  ModuleScript* moduleScript = aRequest->mModuleScript;
  MOZ_ASSERT(moduleScript);

  JS::Value parseError = FindFirstParseError(aRequest);
  if (!parseError.isUndefined()) {
    moduleScript->SetErrorToRethrow(parseError);
    LOG(("ScriptLoadRequest (%p):   found parse error", aRequest));
    return true;
  }

  MOZ_ASSERT(moduleScript->ModuleRecord());

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(moduleScript->ModuleRecord()))) {
    return false;
  }

  JS::Rooted<JSObject*> module(jsapi.cx(), moduleScript->ModuleRecord());
  if (!xpc::Scriptability::AllowedIfExists(module)) {
    return true;
  }

  if (!JS::ModuleLink(jsapi.cx(), module)) {
    LOG(("ScriptLoadRequest (%p): Instantiate failed", aRequest));
    MOZ_ASSERT(jsapi.HasException());
    JS::RootedValue exception(jsapi.cx());
    if (!jsapi.StealException(&exception)) {
      return false;
    }
    MOZ_ASSERT(!exception.isUndefined());
    moduleScript->SetErrorToRethrow(exception);
  }

  return true;
}

nsresult ModuleLoaderBase::InitDebuggerDataForModuleGraph(
    JSContext* aCx, ModuleLoadRequest* aRequest) {
  // JS scripts can be associated with a DOM element for use by the debugger,
  // but preloading can cause scripts to be compiled before DOM script element
  // nodes have been created. This method ensures that this association takes
  // place before the first time a module script is run.

  MOZ_ASSERT(aRequest);

  ModuleScript* moduleScript = aRequest->mModuleScript;
  if (moduleScript->DebuggerDataInitialized()) {
    return NS_OK;
  }

  for (ModuleLoadRequest* childRequest : aRequest->mImports) {
    nsresult rv = InitDebuggerDataForModuleGraph(aCx, childRequest);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  JS::Rooted<JSObject*> module(aCx, moduleScript->ModuleRecord());
  MOZ_ASSERT(module);

  // The script is now ready to be exposed to the debugger.
  JS::Rooted<JSScript*> script(aCx, JS::GetModuleScript(module));
  JS::ExposeScriptToDebugger(aCx, script);

  moduleScript->SetDebuggerDataInitialized();
  return NS_OK;
}

void ModuleLoaderBase::ProcessDynamicImport(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->mLoader == this);

  if (aRequest->mModuleScript) {
    if (!InstantiateModuleGraph(aRequest)) {
      aRequest->mModuleScript = nullptr;
    }
  }

  nsresult rv = NS_ERROR_FAILURE;
  if (aRequest->mModuleScript) {
    rv = EvaluateModule(aRequest);
  }

  if (NS_FAILED(rv)) {
    FinishDynamicImportAndReject(aRequest, rv);
  }
}

nsresult ModuleLoaderBase::EvaluateModule(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->mLoader == this);

  mozilla::nsAutoMicroTask mt;
  mozilla::dom::AutoEntryScript aes(mGlobalObject, "EvaluateModule",
                                    NS_IsMainThread());

  return EvaluateModuleInContext(aes.cx(), aRequest,
                                 JS::ReportModuleErrorsAsync);
}

nsresult ModuleLoaderBase::EvaluateModuleInContext(
    JSContext* aCx, ModuleLoadRequest* aRequest,
    JS::ModuleErrorBehaviour errorBehaviour) {
  MOZ_ASSERT(aRequest->mLoader == this);

  AUTO_PROFILER_LABEL("ModuleLoaderBase::EvaluateModule", JS);

  nsAutoCString profilerLabelString;
  if (aRequest->HasScriptLoadContext()) {
    aRequest->GetScriptLoadContext()->GetProfilerLabel(profilerLabelString);
  }

  LOG(("ScriptLoadRequest (%p): Evaluate Module", aRequest));
  AUTO_PROFILER_MARKER_TEXT("ModuleEvaluation", JS,
                            MarkerInnerWindowIdFromJSContext(aCx),
                            profilerLabelString);

  ModuleLoadRequest* request = aRequest->AsModuleRequest();
  MOZ_ASSERT(request->mModuleScript);
  MOZ_ASSERT_IF(request->HasScriptLoadContext(),
                !request->GetScriptLoadContext()->mOffThreadToken);

  ModuleScript* moduleScript = request->mModuleScript;
  if (moduleScript->HasErrorToRethrow()) {
    LOG(("ScriptLoadRequest (%p):   module has error to rethrow", aRequest));
    JS::Rooted<JS::Value> error(aCx, moduleScript->ErrorToRethrow());
    JS_SetPendingException(aCx, error);
    // For a dynamic import, the promise is rejected.  Otherwise an error
    // is either reported by AutoEntryScript.
    if (request->IsDynamicImport()) {
      FinishDynamicImport(aCx, request, NS_OK, nullptr);
    }
    return NS_OK;
  }

  JS::Rooted<JSObject*> module(aCx, moduleScript->ModuleRecord());
  MOZ_ASSERT(module);

  if (!xpc::Scriptability::AllowedIfExists(module)) {
    return NS_OK;
  }

  nsresult rv = InitDebuggerDataForModuleGraph(aCx, request);
  NS_ENSURE_SUCCESS(rv, rv);

  if (request->HasScriptLoadContext()) {
    TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                   "scriptloader_evaluate_module");
  }

  JS::Rooted<JS::Value> rval(aCx);

  mLoader->MaybePrepareModuleForBytecodeEncodingBeforeExecute(aCx, request);

  bool ok = JS::ModuleEvaluate(aCx, module, &rval);

  // ModuleEvaluate will usually set a pending exception if it returns false,
  // unless the user cancels execution.
  MOZ_ASSERT_IF(ok, !JS_IsExceptionPending(aCx));

  if (!ok) {
    LOG(("ScriptLoadRequest (%p):   evaluation failed", aRequest));
    // For a dynamic import, the promise is rejected. Otherwise an error is
    // reported by AutoEntryScript.
  }

  // ModuleEvaluate returns a promise unless the user cancels the execution in
  // which case rval will be undefined. We should treat it as a failed
  // evaluation, and reject appropriately.
  JS::Rooted<JSObject*> evaluationPromise(aCx);
  if (rval.isObject()) {
    evaluationPromise.set(&rval.toObject());
  }

  if (request->IsDynamicImport()) {
    FinishDynamicImport(aCx, request, NS_OK, evaluationPromise);
  } else {
    // If this is not a dynamic import, and if the promise is rejected,
    // the value is unwrapped from the promise value.
    if (!JS::ThrowOnModuleEvaluationFailure(aCx, evaluationPromise,
                                            errorBehaviour)) {
      LOG(("ScriptLoadRequest (%p):   evaluation failed on throw", aRequest));
      // For a dynamic import, the promise is rejected. Otherwise an error is
      // reported by AutoEntryScript.
    }
  }

  rv = mLoader->MaybePrepareModuleForBytecodeEncodingAfterExecute(request,
                                                                  NS_OK);

  mLoader->MaybeTriggerBytecodeEncoding();

  return rv;
}

void ModuleLoaderBase::CancelAndClearDynamicImports() {
  for (ScriptLoadRequest* req = mDynamicImportRequests.getFirst(); req;
       req = req->getNext()) {
    req->Cancel();
    // FinishDynamicImport must happen exactly once for each dynamic import
    // request. If the load is aborted we do it when we remove the request
    // from mDynamicImportRequests.
    FinishDynamicImportAndReject(req->AsModuleRequest(), NS_ERROR_ABORT);
  }
  mDynamicImportRequests.CancelRequestsAndClear();
}

UniquePtr<ImportMap> ModuleLoaderBase::ParseImportMap(
    ScriptLoadRequest* aRequest) {
  AutoJSAPI jsapi;
  if (!jsapi.Init(GetGlobalObject())) {
    return nullptr;
  }

  MOZ_ASSERT(aRequest->IsTextSource());
  MaybeSourceText maybeSource;
  nsresult rv = aRequest->GetScriptSource(jsapi.cx(), &maybeSource);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  JS::SourceText<char16_t>& text = maybeSource.ref<SourceText<char16_t>>();
  ReportWarningHelper warning{mLoader, aRequest};

  // https://html.spec.whatwg.org/multipage/scripting.html#prepare-a-script
  // https://wicg.github.io/import-maps/#integration-prepare-a-script
  // Insert the following case to prepare a script step 25.2:
  // (Impl Note: the latest html spec is step 27.2)
  // Switch on the script's type:
  // "importmap"
  // Step 1. Let import map parse result be the result of create an import map
  // parse result, given source text, base URL and settings object.
  //
  // Impl note: According to the spec, ImportMap::ParseString will throw a
  // TypeError if there's any invalid key/value in the text. After the parsing
  // is done, we should report the error if there's any, this is done in
  // ~AutoJSAPI.
  //
  // See https://wicg.github.io/import-maps/#register-an-import-map, step 7.
  return ImportMap::ParseString(jsapi.cx(), text, aRequest->mBaseURL, warning);
}

void ModuleLoaderBase::RegisterImportMap(UniquePtr<ImportMap> aImportMap) {
  // Check for aImportMap is done in ScriptLoader.
  MOZ_ASSERT(aImportMap);

  // Step 8. Set element’s node document's import map to import map parse
  // result’s import map.
  mImportMap = std::move(aImportMap);
}

#undef LOG
#undef LOG_ENABLED

}  // namespace JS::loader
