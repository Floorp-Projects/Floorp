/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoader.h"
#include "ModuleLoader.h"

#include "jsapi.h"
#include "js/CompileOptions.h"  // JS::CompileOptions, JS::InstantiateOptions
#include "js/ContextOptions.h"  // JS::ContextOptionsRef
#include "js/experimental/JSStencil.h"  // JS::Stencil, JS::CompileModuleScriptToStencil, JS::InstantiateModuleStencil
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
#include "nsContentSecurityManager.h"
#include "nsIContent.h"
#include "nsJSUtils.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
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
// DOM module loader
//////////////////////////////////////////////////////////////

ModuleLoader::ModuleLoader(ScriptLoader* aLoader,
                           nsIGlobalObject* aGlobalObject, Kind aKind)
    : ModuleLoaderBase(aLoader, aGlobalObject), mKind(aKind) {}

ScriptLoader* ModuleLoader::GetScriptLoader() {
  return static_cast<ScriptLoader*>(mLoader.get());
}

bool ModuleLoader::CanStartLoad(ModuleLoadRequest* aRequest, nsresult* aRvOut) {
  if (!GetScriptLoader()->GetDocument()) {
    *aRvOut = NS_ERROR_NULL_POINTER;
    return false;
  }

  // If this document is sandboxed without 'allow-scripts', abort.
  if (GetScriptLoader()->GetDocument()->HasScriptsBlockedBySandbox()) {
    *aRvOut = NS_OK;
    return false;
  }

  // To prevent dynamic code execution, content scripts can only
  // load moz-extension URLs.
  nsCOMPtr<nsIPrincipal> principal = aRequest->TriggeringPrincipal();
  if (BasePrincipal::Cast(principal)->ContentScriptAddonPolicy() &&
      !aRequest->mURI->SchemeIs("moz-extension")) {
    *aRvOut = NS_ERROR_DOM_WEBEXT_CONTENT_SCRIPT_URI;
    return false;
  }

  if (LOG_ENABLED()) {
    nsAutoCString url;
    aRequest->mURI->GetAsciiSpec(url);
    LOG(("ScriptLoadRequest (%p): Start Module Load (url = %s)", aRequest,
         url.get()));
  }

  return true;
}

nsresult ModuleLoader::StartFetch(ModuleLoadRequest* aRequest) {
  // According to the spec, module scripts have different behaviour to classic
  // scripts and always use CORS. Only exception: Non linkable about: pages
  // which load local module scripts.
  bool isAboutPageLoadingChromeURI = ScriptLoader::IsAboutPageLoadingChromeURI(
      aRequest, GetScriptLoader()->GetDocument());

  nsContentSecurityManager::CORSSecurityMapping corsMapping =
      isAboutPageLoadingChromeURI
          ? nsContentSecurityManager::CORSSecurityMapping::DISABLE_CORS_CHECKS
          : nsContentSecurityManager::CORSSecurityMapping::REQUIRE_CORS_CHECKS;

  nsSecurityFlags securityFlags =
      nsContentSecurityManager::ComputeSecurityFlags(aRequest->CORSMode(),
                                                     corsMapping);

  securityFlags |= nsILoadInfo::SEC_ALLOW_CHROME;

  // Delegate Shared Behavior to base ScriptLoader
  nsresult rv = GetScriptLoader()->StartLoadInternal(aRequest, securityFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // https://wicg.github.io/import-maps/#document-acquiring-import-maps
  //
  // An import map is accepted if and only if it is added (i.e., its
  // corresponding script element is added) before the first module load is
  // started, even if the loading of the import map file doesn’t finish before
  // the first module load is started.
  if (!aRequest->GetScriptLoadContext()->IsPreload()) {
    LOG(("ScriptLoadRequest (%p): SetAcquiringImportMaps false", aRequest));
    SetAcquiringImportMaps(false);
  }

  LOG(("ScriptLoadRequest (%p): Start fetching module", aRequest));

  return NS_OK;
}

void ModuleLoader::OnModuleLoadComplete(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsReadyToRun());

  if (aRequest->IsTopLevel()) {
    if (aRequest->IsDynamicImport() ||
        (aRequest->GetScriptLoadContext()->mIsInline &&
         aRequest->GetScriptLoadContext()->GetParserCreated() ==
             NOT_FROM_PARSER)) {
      GetScriptLoader()->RunScriptWhenSafe(aRequest);
    } else {
      GetScriptLoader()->MaybeMoveToLoadedList(aRequest);
      GetScriptLoader()->ProcessPendingRequests();
    }
  }

  aRequest->GetScriptLoadContext()->MaybeUnblockOnload();
}

nsresult ModuleLoader::CompileFetchedModule(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal, JS::CompileOptions& aOptions,
    ModuleLoadRequest* aRequest, JS::MutableHandle<JSObject*> aModuleOut) {
  if (aRequest->GetScriptLoadContext()->mWasCompiledOMT) {
    JS::Rooted<JS::InstantiationStorage> storage(aCx);
    RefPtr<JS::Stencil> stencil = JS::FinishOffThreadStencil(
        aCx, aRequest->GetScriptLoadContext()->mOffThreadToken,
        storage.address());

    aRequest->GetScriptLoadContext()->mOffThreadToken = nullptr;

    if (!stencil) {
      return NS_ERROR_FAILURE;
    }

    JS::InstantiateOptions instantiateOptions(aOptions);
    aModuleOut.set(JS::InstantiateModuleStencil(aCx, instantiateOptions,
                                                stencil, storage.address()));
    if (!aModuleOut) {
      return NS_ERROR_FAILURE;
    }

    if (aRequest->IsTextSource() &&
        ScriptLoader::ShouldCacheBytecode(aRequest)) {
      if (!JS::StartIncrementalEncoding(aCx, std::move(stencil))) {
        return NS_ERROR_FAILURE;
      }
    }

    return NS_OK;
  }

  if (!nsJSUtils::IsScriptable(aGlobal)) {
    return NS_OK;
  }

  RefPtr<JS::Stencil> stencil;
  if (aRequest->IsTextSource()) {
    MaybeSourceText maybeSource;
    nsresult rv = aRequest->GetScriptSource(aCx, &maybeSource);
    NS_ENSURE_SUCCESS(rv, rv);

    auto compile = [&](auto& source) {
      return JS::CompileModuleScriptToStencil(aCx, aOptions, source);
    };
    stencil = maybeSource.mapNonEmpty(compile);
  } else {
    MOZ_ASSERT(aRequest->IsBytecode());
    JS::DecodeOptions decodeOptions(aOptions);
    decodeOptions.borrowBuffer = true;

    auto& bytecode = aRequest->mScriptBytecode;
    auto& offset = aRequest->mBytecodeOffset;

    JS::TranscodeRange range(bytecode.begin() + offset,
                             bytecode.length() - offset);

    JS::TranscodeResult tr =
        JS::DecodeStencil(aCx, decodeOptions, range, getter_AddRefs(stencil));
    if (tr != JS::TranscodeResult::Ok) {
      return NS_ERROR_DOM_JS_DECODING_ERROR;
    }
  }

  if (!stencil) {
    return NS_ERROR_FAILURE;
  }

  JS::InstantiateOptions instantiateOptions(aOptions);
  aModuleOut.set(
      JS::InstantiateModuleStencil(aCx, instantiateOptions, stencil));
  if (!aModuleOut) {
    return NS_ERROR_FAILURE;
  }

  if (aRequest->IsTextSource() && ScriptLoader::ShouldCacheBytecode(aRequest)) {
    if (!JS::StartIncrementalEncoding(aCx, std::move(stencil))) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
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

already_AddRefed<ModuleLoadRequest> ModuleLoader::CreateStaticImport(
    nsIURI* aURI, ModuleLoadRequest* aParent) {
  RefPtr<ScriptLoadContext> newContext = new ScriptLoadContext();
  newContext->mIsInline = false;
  // Propagated Parent values. TODO: allow child modules to use root module's
  // script mode.
  newContext->mScriptMode = aParent->GetScriptLoadContext()->mScriptMode;

  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      aURI, aParent->mFetchOptions, SRIMetadata(), aParent->mURI, newContext,
      false, /* is top level */
      false, /* is dynamic import */
      aParent->mLoader, aParent->mVisitedSet, aParent->GetRootModule());

  return request.forget();
}

already_AddRefed<ModuleLoadRequest> ModuleLoader::CreateDynamicImport(
    JSContext* aCx, nsIURI* aURI, LoadedScript* aMaybeActiveScript,
    JS::Handle<JS::Value> aReferencingPrivate, JS::Handle<JSString*> aSpecifier,
    JS::Handle<JSObject*> aPromise) {
  MOZ_ASSERT(aSpecifier);
  MOZ_ASSERT(aPromise);

  RefPtr<ScriptFetchOptions> options = nullptr;
  nsIURI* baseURL = nullptr;
  RefPtr<ScriptLoadContext> context = new ScriptLoadContext();

  if (aMaybeActiveScript) {
    options = aMaybeActiveScript->GetFetchOptions();
    baseURL = aMaybeActiveScript->BaseURL();
  } else {
    // We don't have a referencing script so fall back on using
    // options from the document. This can happen when the user
    // triggers an inline event handler, as there is no active script
    // there.
    Document* document = GetScriptLoader()->GetDocument();

    nsCOMPtr<nsIPrincipal> principal = GetGlobalObject()->PrincipalOrNull();
    MOZ_ASSERT_IF(GetKind() == WebExtension,
                  BasePrincipal::Cast(principal)->ContentScriptAddonPolicy());
    MOZ_ASSERT_IF(GetKind() == Normal, principal == document->NodePrincipal());

    options = new ScriptFetchOptions(
        mozilla::CORS_NONE, document->GetReferrerPolicy(), principal, nullptr);
    baseURL = document->GetDocBaseURI();
  }

  context->mIsInline = false;
  context->mScriptMode = ScriptLoadContext::ScriptMode::eAsync;

  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      aURI, options, SRIMetadata(), baseURL, context, true,
      /* is top level */ true, /* is dynamic import */
      this, ModuleLoadRequest::NewVisitedSetForTopLevelImport(aURI), nullptr);

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
