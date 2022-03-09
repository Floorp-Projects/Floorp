/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoader.h"
#include "ModuleLoader.h"

#include "jsapi.h"
#include "js/ContextOptions.h"  // JS::ContextOptionsRef
#include "js/MemoryFunctions.h"
#include "js/Modules.h"  // JS::FinishDynamicModuleImport, JS::{G,S}etModuleResolveHook, JS::Get{ModulePrivate,ModuleScript,RequestedModule{s,Specifier,SourcePos}}, JS::SetModule{DynamicImport,Metadata}Hook
#include "js/OffThreadScriptCompilation.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty
#include "js/Realm.h"
#include "js/SourceText.h"
#include "js/loader/LoadedScript.h"
#include "js/loader/ScriptLoadRequest.h"
#include "js/loader/ModuleLoaderBase.h"
#include "js/loader/ModuleLoadRequest.h"
#include "xpcpublic.h"
#include "GeckoProfiler.h"
#include "nsIContent.h"
#include "nsJSUtils.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Preferences.h"
#include "nsGlobalWindowInner.h"
#include "nsIPrincipal.h"
#include "mozilla/LoadInfo.h"

using JS::SourceText;
using namespace JS::loader;

namespace mozilla::dom {

#undef LOG
#define LOG(args) \
  MOZ_LOG(ScriptLoader::gScriptLoaderLog, mozilla::LogLevel::Debug, args)

#define LOG_ENABLED() \
  MOZ_LOG_TEST(ScriptLoader::gScriptLoaderLog, mozilla::LogLevel::Debug)

//////////////////////////////////////////////////////////////
// DOM module loader Helpers
//////////////////////////////////////////////////////////////
static ScriptLoader* GetCurrentScriptLoader(JSContext* aCx) {
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

  nsGlobalWindowInner* innerWindow = nullptr;
  if (nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(global)) {
    innerWindow = nsGlobalWindowInner::Cast(win);
  } else {
    innerWindow = xpc::SandboxWindowOrNull(object, aCx);
  }

  if (!innerWindow) {
    return nullptr;
  }

  Document* document = innerWindow->GetDocument();
  if (!document) {
    return nullptr;
  }

  ScriptLoader* loader = document->ScriptLoader();
  if (!loader) {
    return nullptr;
  }

  reportError.release();
  return loader;
}

static LoadedScript* GetLoadedScriptOrNull(
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

//////////////////////////////////////////////////////////////
// DOM module loader Host Hooks
//////////////////////////////////////////////////////////////
bool HostGetSupportedImportAssertions(JSContext* aCx,
                                      JS::ImportAssertionVector& aValues) {
  MOZ_ASSERT(aValues.empty());

  if (!aValues.reserve(1)) {
    JS_ReportOutOfMemory(aCx);
    return false;
  }

  aValues.infallibleAppend(JS::ImportAssertion::Type);

  return true;
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
JSObject* HostResolveImportedModule(JSContext* aCx,
                                    JS::Handle<JS::Value> aReferencingPrivate,
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

    RefPtr<ScriptLoader> loader = GetCurrentScriptLoader(aCx);
    if (!loader) {
      return nullptr;
    }

    nsCOMPtr<nsIURI> uri =
        ModuleLoaderBase::ResolveModuleSpecifier(loader, script, string);

    // This cannot fail because resolving a module specifier must have been
    // previously successful with these same two arguments.
    MOZ_ASSERT(uri, "Failed to resolve previously-resolved module specifier");

    // Use sandboxed global when doing a WebExtension content-script load.
    nsCOMPtr<nsIGlobalObject> global;
    if (BasePrincipal::Cast(nsContentUtils::SubjectPrincipal(aCx))
            ->ContentScriptAddonPolicy()) {
      global = xpc::CurrentNativeGlobal(aCx);
      MOZ_ASSERT(global);
      MOZ_ASSERT(
          xpc::IsWebExtensionContentScriptSandbox(global->GetGlobalJSObject()));
    }

    // Let resolved module script be moduleMap[url]. (This entry must exist for
    // us to have gotten to this point.)
    ModuleScript* ms = loader->GetModuleLoader()->GetFetchedModule(uri, global);
    MOZ_ASSERT(ms, "Resolved module not found in module map");
    MOZ_ASSERT(!ms->HasParseError());
    MOZ_ASSERT(ms->ModuleRecord());

    module.set(ms->ModuleRecord());
  }
  return module;
}

bool HostPopulateImportMeta(JSContext* aCx,
                            JS::Handle<JS::Value> aReferencingPrivate,
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

  return JS_DefineProperty(aCx, aMetaObject, "url", urlString,
                           JSPROP_ENUMERATE);
}

bool HostImportModuleDynamically(JSContext* aCx,
                                 JS::Handle<JS::Value> aReferencingPrivate,
                                 JS::Handle<JSObject*> aModuleRequest,
                                 JS::Handle<JSObject*> aPromise) {
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

  RefPtr<ScriptLoader> loader = GetCurrentScriptLoader(aCx);
  if (!loader) {
    return false;
  }

  nsCOMPtr<nsIURI> uri =
      ModuleLoaderBase::ResolveModuleSpecifier(loader, script, specifier);
  if (!uri) {
    JS::Rooted<JS::Value> error(aCx);
    nsresult rv = ModuleLoaderBase::HandleResolveFailure(aCx, script, specifier,
                                                         0, 0, &error);
    if (NS_FAILED(rv)) {
      JS_ReportOutOfMemory(aCx);
      return false;
    }

    JS_SetPendingException(aCx, error);
    return false;
  }

  // Create a new top-level load request.
  RefPtr<ScriptFetchOptions> options;
  nsIURI* baseURL = nullptr;
  nsCOMPtr<Element> element;
  RefPtr<ScriptLoadContext> context;
  if (script) {
    options = script->GetFetchOptions();
    baseURL = script->BaseURL();
    nsCOMPtr<Element> element = script->GetScriptElement();
    context = new ScriptLoadContext(element);
  } else {
    // We don't have a referencing script so fall back on using
    // options from the document. This can happen when the user
    // triggers an inline event handler, as there is no active script
    // there.
    Document* document = loader->GetDocument();

    // Use the document's principal for all loads, except WebExtension
    // content-scripts.
    // Only remember the global for content-scripts as well.
    nsCOMPtr<nsIPrincipal> principal = nsContentUtils::SubjectPrincipal(aCx);
    nsCOMPtr<nsIGlobalObject> global = xpc::CurrentNativeGlobal(aCx);
    if (!BasePrincipal::Cast(principal)->ContentScriptAddonPolicy()) {
      principal = document->NodePrincipal();
      MOZ_ASSERT(global);
      global = nullptr;  // Null global is the usual case for most loads.
    } else {
      MOZ_ASSERT(
          xpc::IsWebExtensionContentScriptSandbox(global->GetGlobalJSObject()));
    }

    options = new ScriptFetchOptions(
        mozilla::CORS_NONE, document->GetReferrerPolicy(), principal, global);
    baseURL = document->GetDocBaseURI();
    context = new ScriptLoadContext(nullptr);
  }

  RefPtr<ModuleLoadRequest> request = ModuleLoader::CreateDynamicImport(
      uri, options, baseURL, context, loader, aReferencingPrivate,
      specifierString, aPromise);

  loader->GetModuleLoader()->StartDynamicImport(request);
  return true;
}

void DynamicImportPrefChangedCallback(const char* aPrefName, void* aClosure) {
  bool enabled = Preferences::GetBool(aPrefName);
  JS::ModuleDynamicImportHook hook =
      enabled ? HostImportModuleDynamically : nullptr;

  AutoJSAPI jsapi;
  jsapi.Init();
  JSRuntime* rt = JS_GetRuntime(jsapi.cx());
  JS::SetModuleDynamicImportHook(rt, hook);
}

//////////////////////////////////////////////////////////////
// DOM module loader
//////////////////////////////////////////////////////////////

ModuleLoader::ModuleLoader(ScriptLoader* aLoader) : ModuleLoaderBase(aLoader) {
  EnsureModuleHooksInitialized();
}

void ModuleLoader::EnsureModuleHooksInitialized() {
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

  Preferences::RegisterCallbackAndCall(DynamicImportPrefChangedCallback,
                                       "javascript.options.dynamicImport",
                                       (void*)nullptr);
}

ScriptLoader* ModuleLoader::GetScriptLoader() {
  return static_cast<ScriptLoader*>(mLoader.get());
}

nsresult ModuleLoader::StartModuleLoad(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsLoading());
  NS_ENSURE_TRUE(GetScriptLoader()->GetDocument(), NS_ERROR_NULL_POINTER);
  aRequest->SetUnknownDataType();

  // If this document is sandboxed without 'allow-scripts', abort.
  if (GetScriptLoader()->GetDocument()->HasScriptsBlockedBySandbox()) {
    return NS_OK;
  }

  if (LOG_ENABLED()) {
    nsAutoCString url;
    aRequest->mURI->GetAsciiSpec(url);
    LOG(("ScriptLoadRequest (%p): Start Module Load (url = %s)", aRequest,
         url.get()));
  }

  // To prevent dynamic code execution, content scripts can only
  // load moz-extension URLs.
  nsCOMPtr<nsIPrincipal> principal = aRequest->TriggeringPrincipal();
  if (BasePrincipal::Cast(principal)->ContentScriptAddonPolicy() &&
      !aRequest->mURI->SchemeIs("moz-extension")) {
    return NS_ERROR_DOM_WEBEXT_CONTENT_SCRIPT_URI;
  }

  // Check whether the module has been fetched or is currently being fetched,
  // and if so wait for it rather than starting a new fetch.
  ModuleLoadRequest* request = aRequest->AsModuleRequest();
  if (ModuleMapContainsURL(request->mURI,
                           aRequest->GetLoadContext()->GetWebExtGlobal())) {
    LOG(("ScriptLoadRequest (%p): Waiting for module fetch", aRequest));
    WaitForModuleFetch(request->mURI,
                       aRequest->GetLoadContext()->GetWebExtGlobal())
        ->Then(GetMainThreadSerialEventTarget(), __func__, request,
               &ModuleLoadRequest::ModuleLoaded,
               &ModuleLoadRequest::LoadFailed);
    return NS_OK;
  }

  nsSecurityFlags securityFlags;

  // According to the spec, module scripts have different behaviour to classic
  // scripts and always use CORS. Only exception: Non linkable about: pages
  // which load local module scripts.
  if (GetScriptLoader()->IsAboutPageLoadingChromeURI(
          aRequest, GetScriptLoader()->GetDocument())) {
    securityFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL;
  } else {
    securityFlags = nsILoadInfo::SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT;
    if (aRequest->CORSMode() == CORS_NONE ||
        aRequest->CORSMode() == CORS_ANONYMOUS) {
      securityFlags |= nsILoadInfo::SEC_COOKIES_SAME_ORIGIN;
    } else {
      MOZ_ASSERT(aRequest->CORSMode() == CORS_USE_CREDENTIALS);
      securityFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;
    }
  }

  securityFlags |= nsILoadInfo::SEC_ALLOW_CHROME;

  // Delegate Shared Behavior to base ScriptLoader
  nsresult rv = GetScriptLoader()->StartLoadInternal(aRequest, securityFlags);

  NS_ENSURE_SUCCESS(rv, rv);

  // We successfully started fetching a module so put its URL in the module
  // map and mark it as fetching.
  SetModuleFetchStarted(aRequest->AsModuleRequest());
  LOG(("ScriptLoadRequest (%p): Start fetching module", aRequest));

  return NS_OK;
}

void ModuleLoader::ProcessLoadedModuleTree(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsReadyToRun());

  if (aRequest->IsTopLevel()) {
    if (aRequest->IsDynamicImport()) {
      MOZ_ASSERT(aRequest->isInList());
      RefPtr<ScriptLoadRequest> req = mDynamicImportRequests.Steal(aRequest);
      GetScriptLoader()->RunScriptWhenSafe(aRequest);
    } else if (aRequest->GetLoadContext()->mIsInline &&
               aRequest->GetLoadContext()->GetParserCreated() ==
                   NOT_FROM_PARSER) {
      MOZ_ASSERT(!aRequest->isInList());
      GetScriptLoader()->RunScriptWhenSafe(aRequest);
    } else {
      GetScriptLoader()->MaybeMoveToLoadedList(aRequest);
      GetScriptLoader()->ProcessPendingRequests();
    }
  }

  aRequest->GetLoadContext()->MaybeUnblockOnload();
}

nsresult ModuleLoader::CompileOrFinishModuleScript(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal, JS::CompileOptions& aOptions,
    ModuleLoadRequest* aRequest, JS::MutableHandle<JSObject*> aModule) {
  nsresult rv;
  if (aRequest->GetLoadContext()->mWasCompiledOMT) {
    JS::Rooted<JS::InstantiationStorage> storage(aCx);

    RefPtr<JS::Stencil> stencil = JS::FinishCompileModuleToStencilOffThread(
        aCx, aRequest->GetLoadContext()->mOffThreadToken, storage.address());
    if (stencil) {
      JS::InstantiateOptions instantiateOptions(aOptions);
      aModule.set(JS::InstantiateModuleStencil(aCx, instantiateOptions, stencil,
                                               storage.address()));
    }

    aRequest->GetLoadContext()->mOffThreadToken = nullptr;
    rv = aModule ? NS_OK : NS_ERROR_FAILURE;
  } else {
    MaybeSourceText maybeSource;
    rv = aRequest->GetScriptSource(aCx, &maybeSource);
    if (NS_SUCCEEDED(rv)) {
      rv = maybeSource.constructed<SourceText<char16_t>>()
               ? nsJSUtils::CompileModule(
                     aCx, maybeSource.ref<SourceText<char16_t>>(), aGlobal,
                     aOptions, aModule)
               : nsJSUtils::CompileModule(
                     aCx, maybeSource.ref<SourceText<Utf8Unit>>(), aGlobal,
                     aOptions, aModule);
    }
  }
  return rv;
}

/* static */
already_AddRefed<ModuleLoadRequest> ModuleLoader::CreateTopLevel(
    nsIURI* aURI, ScriptFetchOptions* aFetchOptions,
    const SRIMetadata& aIntegrity, nsIURI* aReferrer, ScriptLoader* aLoader,
    ScriptLoadContext* aContext) {
  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      aURI, aFetchOptions, aIntegrity, aReferrer, aContext, true,
      /* is top level */ false, /* is dynamic import */
      aLoader->GetModuleLoader(),
      ModuleLoadRequest::NewVisitedSetForTopLevelImport(aURI), nullptr);

  return request.forget();
}

/* static */
already_AddRefed<ModuleLoadRequest> ModuleLoader::CreateStaticImport(
    nsIURI* aURI, ModuleLoadRequest* aParent) {
  RefPtr<ScriptLoadContext> newContext =
      new ScriptLoadContext(aParent->GetLoadContext()->mElement);
  newContext->mIsInline = false;
  // Propagated Parent values. TODO: allow child modules to use root module's
  // script mode.
  newContext->mScriptMode = aParent->GetLoadContext()->mScriptMode;

  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      aURI, aParent->mFetchOptions, SRIMetadata(), aParent->mURI, newContext,
      false, /* is top level */
      false, /* is dynamic import */
      aParent->mLoader, aParent->mVisitedSet, aParent->GetRootModule());

  return request.forget();
}

/* static */
already_AddRefed<ModuleLoadRequest> ModuleLoader::CreateDynamicImport(
    nsIURI* aURI, ScriptFetchOptions* aFetchOptions, nsIURI* aBaseURL,
    ScriptLoadContext* aContext, ScriptLoader* aLoader,
    JS::Handle<JS::Value> aReferencingPrivate, JS::Handle<JSString*> aSpecifier,
    JS::Handle<JSObject*> aPromise) {
  MOZ_ASSERT(aSpecifier);
  MOZ_ASSERT(aPromise);

  aContext->mIsInline = false;
  aContext->mScriptMode = ScriptLoadContext::ScriptMode::eAsync;

  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      aURI, aFetchOptions, SRIMetadata(), aBaseURL, aContext, true,
      /* is top level */ true, /* is dynamic import */
      aLoader->GetModuleLoader(),
      ModuleLoadRequest::NewVisitedSetForTopLevelImport(aURI), nullptr);

  request->mDynamicReferencingPrivate = aReferencingPrivate;
  request->mDynamicSpecifier = aSpecifier;
  request->mDynamicPromise = aPromise;

  HoldJSObjects(request.get());

  return request.forget();
}

ModuleLoader::~ModuleLoader() {
  LOG(("ModuleLoader::~ModuleLoader %p", this));
  mLoader = nullptr;
}

#undef LOG
#undef LOG_ENABLED

}  // namespace mozilla::dom
