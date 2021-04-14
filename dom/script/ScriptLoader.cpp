/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoader.h"
#include "ScriptLoadHandler.h"
#include "ScriptLoadRequest.h"
#include "ScriptTrace.h"
#include "LoadedScript.h"
#include "ModuleLoadRequest.h"

#include "prsystem.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"  // JS::GetArrayLength
#include "js/CompilationAndEvaluation.h"
#include "js/ContextOptions.h"        // JS::ContextOptionsRef
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "js/MemoryFunctions.h"
#include "js/Modules.h"  // JS::FinishDynamicModuleImport, JS::{G,S}etModuleResolveHook, JS::Get{ModulePrivate,ModuleScript,RequestedModule{s,Specifier,SourcePos}}, JS::SetModule{DynamicImport,Metadata}Hook
#include "js/OffThreadScriptCompilation.h"
#include "js/Realm.h"
#include "js/SourceText.h"
#include "js/Utility.h"
#include "xpcpublic.h"
#include "GeckoProfiler.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"
#include "nsJSUtils.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/JSExecutionContext.h"
#include "mozilla/dom/ScriptDecoding.h"  // mozilla::dom::ScriptDecoding
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SRILogHelper.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsAboutProtocolUtils.h"
#include "nsGkAtoms.h"
#include "nsNetUtil.h"
#include "nsGlobalWindowInner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsContentPolicyUtils.h"
#include "nsIClassifiedChannel.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIClassOfService.h"
#include "nsICacheInfoChannel.h"
#include "nsITimedChannel.h"
#include "nsIScriptElement.h"
#include "nsISupportsPriority.h"
#include "nsIDocShell.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsError.h"
#include "nsThreadUtils.h"
#include "nsDocShellCID.h"
#include "nsIContentSecurityPolicy.h"
#include "mozilla/Logging.h"
#include "nsCRT.h"
#include "nsContentCreatorFunctions.h"
#include "nsProxyRelease.h"
#include "nsSandboxFlags.h"
#include "nsContentTypeParser.h"
#include "nsINetworkPredictor.h"
#include "nsMimeTypes.h"
#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/LoadInfo.h"
#include "ReferrerInfo.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Attributes.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit
#include "nsIScriptError.h"
#include "nsIAsyncOutputStream.h"

using JS::SourceText;

using mozilla::Telemetry::LABELS_DOM_SCRIPT_PRELOAD_RESULT;

namespace mozilla {
namespace dom {

LazyLogModule ScriptLoader::gCspPRLog("CSP");
LazyLogModule ScriptLoader::gScriptLoaderLog("ScriptLoader");

#undef LOG
#define LOG(args) \
  MOZ_LOG(ScriptLoader::gScriptLoaderLog, mozilla::LogLevel::Debug, args)

#define LOG_ENABLED() \
  MOZ_LOG_TEST(ScriptLoader::gScriptLoaderLog, mozilla::LogLevel::Debug)

// Alternate Data MIME type used by the ScriptLoader to register that we want to
// store bytecode without reading it.
static constexpr auto kNullMimeType = "javascript/null"_ns;

/////////////////////////////////////////////////////////////
// AsyncCompileShutdownObserver
/////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(AsyncCompileShutdownObserver, nsIObserver)

void AsyncCompileShutdownObserver::OnShutdown() {
  if (mScriptLoader) {
    mScriptLoader->Shutdown();
    Unregister();
  }
}

void AsyncCompileShutdownObserver::Unregister() {
  if (mScriptLoader) {
    mScriptLoader = nullptr;
    nsContentUtils::UnregisterShutdownObserver(this);
  }
}

NS_IMETHODIMP
AsyncCompileShutdownObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aData) {
  OnShutdown();
  return NS_OK;
}

//////////////////////////////////////////////////////////////
// ScriptLoader::PreloadInfo
//////////////////////////////////////////////////////////////

inline void ImplCycleCollectionUnlink(ScriptLoader::PreloadInfo& aField) {
  ImplCycleCollectionUnlink(aField.mRequest);
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    ScriptLoader::PreloadInfo& aField, const char* aName, uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aField.mRequest, aName, aFlags);
}

//////////////////////////////////////////////////////////////
// ScriptLoader::mFetchingModules / ScriptLoader::mFetchingModules
//////////////////////////////////////////////////////////////

inline void ImplCycleCollectionUnlink(
    nsRefPtrHashtable<ModuleMapKey,
                      mozilla::GenericNonExclusivePromise::Private>& aField) {
  for (auto iter = aField.Iter(); !iter.Done(); iter.Next()) {
    ImplCycleCollectionUnlink(iter.Key());

    RefPtr<GenericNonExclusivePromise::Private> promise = iter.UserData();
    if (promise) {
      promise->Reject(NS_ERROR_ABORT, __func__);
    }
  }

  aField.Clear();
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsRefPtrHashtable<ModuleMapKey,
                      mozilla::GenericNonExclusivePromise::Private>& aField,
    const char* aName, uint32_t aFlags = 0) {
  for (auto iter = aField.Iter(); !iter.Done(); iter.Next()) {
    ImplCycleCollectionTraverse(aCallback, iter.Key(), "mFetchingModules key",
                                aFlags);
  }
}

inline void ImplCycleCollectionUnlink(
    nsRefPtrHashtable<ModuleMapKey, ModuleScript>& aField) {
  for (auto iter = aField.Iter(); !iter.Done(); iter.Next()) {
    ImplCycleCollectionUnlink(iter.Key());
  }

  aField.Clear();
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsRefPtrHashtable<ModuleMapKey, ModuleScript>& aField, const char* aName,
    uint32_t aFlags = 0) {
  for (auto iter = aField.Iter(); !iter.Done(); iter.Next()) {
    ImplCycleCollectionTraverse(aCallback, iter.Key(), "mFetchedModules key",
                                aFlags);
    CycleCollectionNoteChild(aCallback, iter.UserData(), "mFetchedModules data",
                             aFlags);
  }
}

//////////////////////////////////////////////////////////////
// ScriptLoader
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScriptLoader)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(ScriptLoader, mNonAsyncExternalScriptInsertedRequests,
                         mLoadingAsyncRequests, mLoadedAsyncRequests,
                         mDeferRequests, mXSLTRequests, mDynamicImportRequests,
                         mParserBlockingRequest, mBytecodeEncodingQueue,
                         mPreloads, mPendingChildLoaders, mFetchedModules,
                         mFetchingModules)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ScriptLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ScriptLoader)

ScriptLoader::ScriptLoader(Document* aDocument)
    : mDocument(aDocument),
      mParserBlockingBlockerCount(0),
      mBlockerCount(0),
      mNumberOfProcessors(0),
      mEnabled(true),
      mDeferEnabled(false),
      mSpeculativeOMTParsingEnabled(false),
      mDeferCheckpointReached(false),
      mBlockingDOMContentLoaded(false),
      mLoadEventFired(false),
      mGiveUpEncoding(false),
      mReporter(new ConsoleReportCollector()) {
  LOG(("ScriptLoader::ScriptLoader %p", this));
  EnsureModuleHooksInitialized();

  mSpeculativeOMTParsingEnabled = StaticPrefs::
      dom_script_loader_external_scripts_speculative_omt_parse_enabled();

  mShutdownObserver = new AsyncCompileShutdownObserver(this);
  nsContentUtils::RegisterShutdownObserver(mShutdownObserver);
}

ScriptLoader::~ScriptLoader() {
  LOG(("ScriptLoader::~ScriptLoader %p", this));

  mObservers.Clear();

  if (mParserBlockingRequest) {
    mParserBlockingRequest->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mXSLTRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mDeferRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mLoadingAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mLoadedAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mDynamicImportRequests.getFirst(); req;
       req = req->getNext()) {
    FinishDynamicImportAndReject(req->AsModuleRequest(), NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req =
           mNonAsyncExternalScriptInsertedRequests.getFirst();
       req; req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  // Unblock the kids, in case any of them moved to a different document
  // subtree in the meantime and therefore aren't actually going away.
  for (uint32_t j = 0; j < mPendingChildLoaders.Length(); ++j) {
    mPendingChildLoaders[j]->RemoveParserBlockingScriptExecutionBlocker();
  }

  for (size_t i = 0; i < mPreloads.Length(); i++) {
    AccumulateCategorical(LABELS_DOM_SCRIPT_PRELOAD_RESULT::NotUsed);
  }

  if (mShutdownObserver) {
    mShutdownObserver->Unregister();
    mShutdownObserver = nullptr;
  }
}

// Collect telemtry data about the cache information, and the kind of source
// which are being loaded, and where it is being loaded from.
static void CollectScriptTelemetry(ScriptLoadRequest* aRequest) {
  using namespace mozilla::Telemetry;

  // Skip this function if we are not running telemetry.
  if (!CanRecordExtended()) {
    return;
  }

  // Report the script kind.
  if (aRequest->IsModuleRequest()) {
    AccumulateCategorical(LABELS_DOM_SCRIPT_KIND::ModuleScript);
  } else {
    AccumulateCategorical(LABELS_DOM_SCRIPT_KIND::ClassicScript);
  }

  // Report the type of source. This is used to monitor the status of the
  // JavaScript Start-up Bytecode Cache, with the expectation of an almost zero
  // source-fallback and alternate-data being roughtly equal to source loads.
  if (aRequest->IsLoadingSource()) {
    if (aRequest->mIsInline) {
      AccumulateCategorical(LABELS_DOM_SCRIPT_LOADING_SOURCE::Inline);
    } else if (aRequest->IsTextSource()) {
      AccumulateCategorical(LABELS_DOM_SCRIPT_LOADING_SOURCE::SourceFallback);
    }
  } else {
    MOZ_ASSERT(aRequest->IsLoading());
    if (aRequest->IsTextSource()) {
      AccumulateCategorical(LABELS_DOM_SCRIPT_LOADING_SOURCE::Source);
    } else if (aRequest->IsBytecode()) {
      AccumulateCategorical(LABELS_DOM_SCRIPT_LOADING_SOURCE::AltData);
    }
  }
}

// Helper method for checking if the script element is an event-handler
// This means that it has both a for-attribute and a event-attribute.
// Also, if the for-attribute has a value that matches "\s*window\s*",
// and the event-attribute matches "\s*onload([ \(].*)?" then it isn't an
// eventhandler. (both matches are case insensitive).
// This is how IE seems to filter out a window's onload handler from a
// <script for=... event=...> element.

static bool IsScriptEventHandler(ScriptKind kind, nsIContent* aScriptElement) {
  if (kind != ScriptKind::eClassic) {
    return false;
  }

  if (!aScriptElement->IsHTMLElement()) {
    return false;
  }

  nsAutoString forAttr, eventAttr;
  if (!aScriptElement->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::_for,
                                            forAttr) ||
      !aScriptElement->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::event,
                                            eventAttr)) {
    return false;
  }

  const nsAString& for_str =
      nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(forAttr);
  if (!for_str.LowerCaseEqualsLiteral("window")) {
    return true;
  }

  // We found for="window", now check for event="onload".
  const nsAString& event_str =
      nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(eventAttr, false);
  if (!StringBeginsWith(event_str, u"onload"_ns,
                        nsCaseInsensitiveStringComparator)) {
    // It ain't "onload.*".

    return true;
  }

  nsAutoString::const_iterator start, end;
  event_str.BeginReading(start);
  event_str.EndReading(end);

  start.advance(6);  // advance past "onload"

  if (start != end && *start != '(' && *start != ' ') {
    // We got onload followed by something other than space or
    // '('. Not good enough.

    return true;
  }

  return false;
}

nsContentPolicyType ScriptLoadRequestToContentPolicyType(
    ScriptLoadRequest* aRequest) {
  if (aRequest->IsPreload()) {
    return aRequest->IsModuleRequest()
               ? nsIContentPolicy::TYPE_INTERNAL_MODULE_PRELOAD
               : nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD;
  }

  return aRequest->IsModuleRequest() ? nsIContentPolicy::TYPE_INTERNAL_MODULE
                                     : nsIContentPolicy::TYPE_INTERNAL_SCRIPT;
}

nsresult ScriptLoader::CheckContentPolicy(Document* aDocument,
                                          nsISupports* aContext,
                                          const nsAString& aType,
                                          ScriptLoadRequest* aRequest) {
  nsContentPolicyType contentPolicyType =
      ScriptLoadRequestToContentPolicyType(aRequest);

  nsCOMPtr<nsINode> requestingNode = do_QueryInterface(aContext);
  nsCOMPtr<nsILoadInfo> secCheckLoadInfo = new net::LoadInfo(
      aDocument->NodePrincipal(),  // loading principal
      aDocument->NodePrincipal(),  // triggering principal
      requestingNode, nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
      contentPolicyType);

  // snapshot the nonce at load start time for performing CSP checks
  if (contentPolicyType == nsIContentPolicy::TYPE_INTERNAL_SCRIPT ||
      contentPolicyType == nsIContentPolicy::TYPE_INTERNAL_MODULE) {
    nsCOMPtr<nsINode> node = do_QueryInterface(aContext);
    if (node) {
      nsString* cspNonce =
          static_cast<nsString*>(node->GetProperty(nsGkAtoms::nonce));
      if (cspNonce) {
        secCheckLoadInfo->SetCspNonce(*cspNonce);
      }
    }
  }

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(
      aRequest->mURI, secCheckLoadInfo, NS_LossyConvertUTF16toASCII(aType),
      &shouldLoad, nsContentUtils::GetContentPolicy());
  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    if (NS_FAILED(rv) || shouldLoad != nsIContentPolicy::REJECT_TYPE) {
      return NS_ERROR_CONTENT_BLOCKED;
    }
    return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
  }

  return NS_OK;
}

/* static */
bool ScriptLoader::IsAboutPageLoadingChromeURI(ScriptLoadRequest* aRequest,
                                               Document* aDocument) {
  // if the uri to be loaded is not of scheme chrome:, there is nothing to do.
  if (!aRequest->mURI->SchemeIs("chrome")) {
    return false;
  }

  // we can either get here with a regular contentPrincipal or with a
  // NullPrincipal in case we are showing an error page in a sandboxed iframe.
  // In either case if the about: page is linkable from content, there is
  // nothing to do.
  uint32_t aboutModuleFlags = 0;
  nsresult rv = NS_OK;

  nsCOMPtr<nsIPrincipal> triggeringPrincipal = aRequest->TriggeringPrincipal();
  if (triggeringPrincipal->GetIsContentPrincipal()) {
    if (!triggeringPrincipal->SchemeIs("about")) {
      return false;
    }
    rv = triggeringPrincipal->GetAboutModuleFlags(&aboutModuleFlags);
    NS_ENSURE_SUCCESS(rv, false);
  } else if (triggeringPrincipal->GetIsNullPrincipal()) {
    nsCOMPtr<nsIURI> docURI = aDocument->GetDocumentURI();
    if (!docURI->SchemeIs("about")) {
      return false;
    }

    nsCOMPtr<nsIAboutModule> aboutModule;
    rv = NS_GetAboutModule(docURI, getter_AddRefs(aboutModule));
    if (NS_FAILED(rv) || !aboutModule) {
      return false;
    }
    rv = aboutModule->GetURIFlags(docURI, &aboutModuleFlags);
    NS_ENSURE_SUCCESS(rv, false);
  } else {
    return false;
  }

  if (aboutModuleFlags & nsIAboutModule::MAKE_LINKABLE) {
    return false;
  }

  // seems like an about page wants to load a chrome URI.
  return true;
}

bool ScriptLoader::ModuleMapContainsURL(nsIURI* aURL,
                                        nsIGlobalObject* aGlobal) const {
  // Returns whether we have fetched, or are currently fetching, a module script
  // for a URL.
  ModuleMapKey key(aURL, aGlobal);
  return mFetchingModules.Contains(key) || mFetchedModules.Contains(key);
}

void ScriptLoader::SetModuleFetchStarted(ModuleLoadRequest* aRequest) {
  // Update the module map to indicate that a module is currently being fetched.

  MOZ_ASSERT(aRequest->IsLoading());
  MOZ_ASSERT(
      !ModuleMapContainsURL(aRequest->mURI, aRequest->GetWebExtGlobal()));
  ModuleMapKey key(aRequest->mURI, aRequest->GetWebExtGlobal());
  mFetchingModules.InsertOrUpdate(
      key, RefPtr<GenericNonExclusivePromise::Private>{});
}

void ScriptLoader::SetModuleFetchFinishedAndResumeWaitingRequests(
    ModuleLoadRequest* aRequest, nsresult aResult) {
  // Update module map with the result of fetching a single module script.
  //
  // If any requests for the same URL are waiting on this one to complete, they
  // will have ModuleLoaded or LoadFailed on them when the promise is
  // resolved/rejected. This is set up in StartLoad.

  LOG(
      ("ScriptLoadRequest (%p): Module fetch finished (script == %p, result == "
       "%u)",
       aRequest, aRequest->mModuleScript.get(), unsigned(aResult)));

  ModuleMapKey key(aRequest->mURI, aRequest->GetWebExtGlobal());
  RefPtr<GenericNonExclusivePromise::Private> promise;
  if (!mFetchingModules.Remove(key, getter_AddRefs(promise))) {
    LOG(("ScriptLoadRequest (%p): Key not found in mFetchingModules",
         aRequest));
    return;
  }

  RefPtr<ModuleScript> moduleScript(aRequest->mModuleScript);
  MOZ_ASSERT(NS_FAILED(aResult) == !moduleScript);

  mFetchedModules.InsertOrUpdate(key, RefPtr{moduleScript});

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

RefPtr<GenericNonExclusivePromise> ScriptLoader::WaitForModuleFetch(
    nsIURI* aURL, nsIGlobalObject* aGlobal) {
  MOZ_ASSERT(ModuleMapContainsURL(aURL, aGlobal));

  ModuleMapKey key(aURL, aGlobal);
  if (auto entry = mFetchingModules.Lookup(key)) {
    if (!entry.Data()) {
      entry.Data() = new GenericNonExclusivePromise::Private(__func__);
    }
    return entry.Data();
  }

  RefPtr<ModuleScript> ms;
  MOZ_ALWAYS_TRUE(mFetchedModules.Get(key, getter_AddRefs(ms)));
  if (!ms) {
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
  }

  return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
}

ModuleScript* ScriptLoader::GetFetchedModule(nsIURI* aURL,
                                             nsIGlobalObject* aGlobal) const {
  if (LOG_ENABLED()) {
    nsAutoCString url;
    aURL->GetAsciiSpec(url);
    LOG(("GetFetchedModule %s %p", url.get(), aGlobal));
  }

  bool found;
  ModuleMapKey key(aURL, aGlobal);
  ModuleScript* ms = mFetchedModules.GetWeak(key, &found);
  MOZ_ASSERT(found);
  return ms;
}

nsresult ScriptLoader::ProcessFetchedModuleSource(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(!aRequest->mModuleScript);

  nsresult rv = CreateModuleScript(aRequest);
  MOZ_ASSERT(NS_FAILED(rv) == !aRequest->mModuleScript);

  aRequest->ClearScriptSource();

  if (NS_FAILED(rv)) {
    aRequest->LoadFailed();
    return rv;
  }

  if (!aRequest->mIsInline) {
    SetModuleFetchFinishedAndResumeWaitingRequests(aRequest, rv);
  }

  if (!aRequest->mModuleScript->HasParseError()) {
    StartFetchingModuleDependencies(aRequest);
  }

  return NS_OK;
}

static nsresult ResolveRequestedModules(ModuleLoadRequest* aRequest,
                                        nsCOMArray<nsIURI>* aUrlsOut);

nsresult ScriptLoader::CreateModuleScript(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(!aRequest->mModuleScript);
  MOZ_ASSERT(aRequest->mBaseURL);

  LOG(("ScriptLoadRequest (%p): Create module script", aRequest));

  nsCOMPtr<nsIGlobalObject> globalObject;
  nsCOMPtr<nsIScriptContext> context;
  if (aRequest->GetWebExtGlobal()) {
    globalObject = aRequest->GetWebExtGlobal();
  } else {
    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal =
        GetScriptGlobalObject(WebExtGlobal::Handled);
    if (!scriptGlobal) {
      return NS_ERROR_FAILURE;
    }

    context = scriptGlobal->GetScriptContext();
    if (!context) {
      return NS_ERROR_FAILURE;
    }

    globalObject = scriptGlobal;
  }

  nsAutoMicroTask mt;

  AutoAllowLegacyScriptExecution exemption;

  AutoEntryScript aes(globalObject, "CompileModule", true);

  bool oldProcessingScriptTag = false;
  if (context) {
    oldProcessingScriptTag = context->GetProcessingScriptTag();
    context->SetProcessingScriptTag(true);
  }

  nsresult rv;
  {
    JSContext* cx = aes.cx();
    JS::Rooted<JSObject*> module(cx);

    if (aRequest->mWasCompiledOMT) {
      module = JS::FinishOffThreadModule(cx, aRequest->mOffThreadToken);
      aRequest->mOffThreadToken = nullptr;
      rv = module ? NS_OK : NS_ERROR_FAILURE;
    } else {
      JS::Rooted<JSObject*> global(cx, globalObject->GetGlobalJSObject());

      JS::CompileOptions options(cx);
      rv = FillCompileOptionsForRequest(aes, aRequest, global, &options);

      if (NS_SUCCEEDED(rv)) {
        MaybeSourceText maybeSource;
        rv = GetScriptSource(cx, aRequest, &maybeSource);
        if (NS_SUCCEEDED(rv)) {
          rv = maybeSource.constructed<SourceText<char16_t>>()
                   ? nsJSUtils::CompileModule(
                         cx, maybeSource.ref<SourceText<char16_t>>(), global,
                         options, &module)
                   : nsJSUtils::CompileModule(
                         cx, maybeSource.ref<SourceText<Utf8Unit>>(), global,
                         options, &module);
        }
      }
    }

    MOZ_ASSERT(NS_SUCCEEDED(rv) == (module != nullptr));

    RefPtr<ModuleScript> moduleScript =
        new ModuleScript(aRequest->mFetchOptions, aRequest->mBaseURL);
    aRequest->mModuleScript = moduleScript;

    if (!module) {
      LOG(("ScriptLoadRequest (%p):   compilation failed (%d)", aRequest,
           unsigned(rv)));

      MOZ_ASSERT(aes.HasException());
      JS::Rooted<JS::Value> error(cx);
      if (!aes.StealException(&error)) {
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

  if (context) {
    context->SetProcessingScriptTag(oldProcessingScriptTag);
  }

  LOG(("ScriptLoadRequest (%p):   module script == %p", aRequest,
       aRequest->mModuleScript.get()));

  return rv;
}

static nsresult HandleResolveFailure(JSContext* aCx, LoadedScript* aScript,
                                     const nsAString& aSpecifier,
                                     uint32_t aLineNumber,
                                     uint32_t aColumnNumber,
                                     JS::MutableHandle<JS::Value> errorOut) {
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
      nsContentUtils::eDOM_PROPERTIES, "ModuleResolveFailure", errorParams,
      errorText);
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JSString*> string(aCx, JS_NewUCStringCopyZ(aCx, errorText.get()));
  if (!string) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!JS::CreateError(aCx, JSEXN_TYPEERR, nullptr, filename, aLineNumber,
                       aColumnNumber, nullptr, string, errorOut)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

static already_AddRefed<nsIURI> ResolveModuleSpecifier(
    ScriptLoader* aLoader, LoadedScript* aScript, const nsAString& aSpecifier) {
  // The following module specifiers are allowed by the spec:
  //  - a valid absolute URL
  //  - a valid relative URL that starts with "/", "./" or "../"
  //
  // Bareword module specifiers are currently disallowed as these may be given
  // special meanings in the future.

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aSpecifier);
  if (NS_SUCCEEDED(rv)) {
    return uri.forget();
  }

  if (rv != NS_ERROR_MALFORMED_URI) {
    return nullptr;
  }

  if (!StringBeginsWith(aSpecifier, u"/"_ns) &&
      !StringBeginsWith(aSpecifier, u"./"_ns) &&
      !StringBeginsWith(aSpecifier, u"../"_ns)) {
    return nullptr;
  }

  // Get the document's base URL if we don't have a referencing script here.
  nsCOMPtr<nsIURI> baseURL;
  if (aScript) {
    baseURL = aScript->BaseURL();
  } else {
    baseURL = aLoader->GetDocument()->GetDocBaseURI();
  }

  rv = NS_NewURI(getter_AddRefs(uri), aSpecifier, nullptr, baseURL);
  if (NS_SUCCEEDED(rv)) {
    return uri.forget();
  }

  return nullptr;
}

static nsresult ResolveRequestedModules(ModuleLoadRequest* aRequest,
                                        nsCOMArray<nsIURI>* aUrlsOut) {
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

  JS::Rooted<JS::Value> element(cx);
  for (uint32_t i = 0; i < length; i++) {
    if (!JS_GetElement(cx, requestedModules, i, &element)) {
      return NS_ERROR_FAILURE;
    }

    JS::Rooted<JSString*> str(cx, JS::GetRequestedModuleSpecifier(cx, element));
    MOZ_ASSERT(str);

    nsAutoJSString specifier;
    if (!specifier.init(cx, str)) {
      return NS_ERROR_FAILURE;
    }

    // Let url be the result of resolving a module specifier given module script
    // and requested.
    nsCOMPtr<nsIURI> uri =
        ResolveModuleSpecifier(aRequest->mLoader, ms, specifier);
    if (!uri) {
      uint32_t lineNumber = 0;
      uint32_t columnNumber = 0;
      JS::GetRequestedModuleSourcePos(cx, element, &lineNumber, &columnNumber);

      JS::Rooted<JS::Value> error(cx);
      nsresult rv = HandleResolveFailure(cx, ms, specifier, lineNumber,
                                         columnNumber, &error);
      NS_ENSURE_SUCCESS(rv, rv);

      ms->SetParseError(error);
      return NS_ERROR_FAILURE;
    }

    if (aUrlsOut) {
      aUrlsOut->AppendElement(uri.forget());
    }
  }

  return NS_OK;
}

void ScriptLoader::StartFetchingModuleDependencies(
    ModuleLoadRequest* aRequest) {
  LOG(("ScriptLoadRequest (%p): Start fetching module dependencies", aRequest));

  MOZ_ASSERT(aRequest->mModuleScript);
  MOZ_ASSERT(!aRequest->mModuleScript->HasParseError());
  MOZ_ASSERT(!aRequest->IsReadyToRun());

  auto visitedSet = aRequest->mVisitedSet;
  MOZ_ASSERT(visitedSet->Contains(aRequest->mURI));

  aRequest->mProgress = ModuleLoadRequest::Progress::eFetchingImports;

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

  // For each url in urls, fetch a module script tree given url, module script's
  // CORS setting, and module script's settings object.
  nsTArray<RefPtr<GenericPromise>> importsReady;
  for (auto url : urls) {
    RefPtr<GenericPromise> childReady =
        StartFetchingModuleAndDependencies(aRequest, url);
    importsReady.AppendElement(childReady);
  }

  // Wait for all imports to become ready.
  RefPtr<GenericPromise::AllPromiseType> allReady =
      GenericPromise::All(GetMainThreadSerialEventTarget(), importsReady);
  allReady->Then(GetMainThreadSerialEventTarget(), __func__, aRequest,
                 &ModuleLoadRequest::DependenciesLoaded,
                 &ModuleLoadRequest::ModuleErrored);
}

RefPtr<GenericPromise> ScriptLoader::StartFetchingModuleAndDependencies(
    ModuleLoadRequest* aParent, nsIURI* aURI) {
  MOZ_ASSERT(aURI);

  RefPtr<ModuleLoadRequest> childRequest =
      ModuleLoadRequest::CreateStaticImport(aURI, aParent);

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

  RefPtr<GenericPromise> ready = childRequest->mReady.Ensure(__func__);

  nsresult rv = StartLoad(childRequest);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(!childRequest->mModuleScript);
    LOG(("ScriptLoadRequest (%p):   rejecting %p", aParent,
         &childRequest->mReady));

    ReportErrorToConsole(childRequest, rv);
    childRequest->mReady.Reject(rv, __func__);
    return ready;
  }

  return ready;
}

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

  auto script = static_cast<LoadedScript*>(aReferencingPrivate.toPrivate());
  if (script->IsEventScript()) {
    return nullptr;
  }

  MOZ_ASSERT_IF(
      script->IsModuleScript(),
      JS::GetModulePrivate(script->AsModuleScript()->ModuleRecord()) ==
          aReferencingPrivate);

  return script;
}

// 8.1.3.8.1 HostResolveImportedModule(referencingModule, moduleRequest)
JSObject* HostResolveImportedModule(JSContext* aCx,
                                    JS::Handle<JS::Value> aReferencingPrivate,
                                    JS::Handle<JSObject*> aModuleRequest) {
  JS::Rooted<JSObject*> module(aCx);
  ScriptLoader::ResolveImportedModule(aCx, aReferencingPrivate, aModuleRequest,
                                      &module);
  return module;
}

/* static */
void ScriptLoader::ResolveImportedModule(
    JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate,
    JS::Handle<JSObject*> aModuleRequest,
    JS::MutableHandle<JSObject*> aModuleOut) {
  MOZ_ASSERT(!aModuleOut);

  RefPtr<LoadedScript> script(GetLoadedScriptOrNull(aCx, aReferencingPrivate));

  JS::Rooted<JSString*> specifierString(
      aCx, JS::GetModuleRequestSpecifier(aCx, aModuleRequest));
  if (!specifierString) {
    return;
  }

  // Let url be the result of resolving a module specifier given referencing
  // module script and specifier.
  nsAutoJSString string;
  if (!string.init(aCx, specifierString)) {
    return;
  }

  RefPtr<ScriptLoader> loader = GetCurrentScriptLoader(aCx);
  if (!loader) {
    return;
  }

  nsCOMPtr<nsIURI> uri = ResolveModuleSpecifier(loader, script, string);

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

  // Let resolved module script be moduleMap[url]. (This entry must exist for us
  // to have gotten to this point.)
  ModuleScript* ms = loader->GetFetchedModule(uri, global);
  MOZ_ASSERT(ms, "Resolved module not found in module map");
  MOZ_ASSERT(!ms->HasParseError());
  MOZ_ASSERT(ms->ModuleRecord());

  aModuleOut.set(ms->ModuleRecord());
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

  nsCOMPtr<nsIURI> uri = ResolveModuleSpecifier(loader, script, specifier);
  if (!uri) {
    JS::Rooted<JS::Value> error(aCx);
    nsresult rv = HandleResolveFailure(aCx, script, specifier, 0, 0, &error);
    if (NS_FAILED(rv)) {
      JS_ReportOutOfMemory(aCx);
      return false;
    }

    JS_SetPendingException(aCx, error);
    return false;
  }

  // Create a new top-level load request.
  ScriptFetchOptions* options;
  nsIURI* baseURL;
  if (script) {
    options = script->GetFetchOptions();
    baseURL = script->BaseURL();
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

    options = new ScriptFetchOptions(mozilla::CORS_NONE,
                                     document->GetReferrerPolicy(), nullptr,
                                     principal, global);
    baseURL = document->GetDocBaseURI();
  }

  RefPtr<ModuleLoadRequest> request = ModuleLoadRequest::CreateDynamicImport(
      uri, options, baseURL, loader, aReferencingPrivate, specifierString,
      aPromise);

  loader->StartDynamicImport(request);
  return true;
}

void ScriptLoader::StartDynamicImport(ModuleLoadRequest* aRequest) {
  LOG(("ScriptLoadRequest (%p): Start dynamic import", aRequest));

  mDynamicImportRequests.AppendElement(aRequest);

  nsresult rv = StartLoad(aRequest);
  if (NS_FAILED(rv)) {
    ReportErrorToConsole(aRequest, rv);
    FinishDynamicImportAndReject(aRequest, rv);
  }
}

void ScriptLoader::FinishDynamicImportAndReject(ModuleLoadRequest* aRequest,
                                                nsresult aResult) {
  AutoJSAPI jsapi;
  MOZ_ASSERT(NS_FAILED(aResult));
  MOZ_ALWAYS_TRUE(jsapi.Init(aRequest->mDynamicPromise));
  if (!JS::ContextOptionsRef(jsapi.cx()).topLevelAwait()) {
    // This is used so that Top Level Await functionality can be turned off
    // entirely. It will be removed in bug#1676612.
    FinishDynamicImport_NoTLA(jsapi.cx(), aRequest, aResult);
  } else {
    // Path for when Top Level Await is enabled.
    FinishDynamicImport(jsapi.cx(), aRequest, aResult, nullptr);
  }
}

// This is used so that Top Level Await functionality can be turned off
// entirely. It will be removed in bug#1676612.
void ScriptLoader::FinishDynamicImport_NoTLA(JSContext* aCx,
                                             ModuleLoadRequest* aRequest,
                                             nsresult aResult) {
  LOG(("ScriptLoadRequest (%p): Finish dynamic import %x %d", aRequest,
       unsigned(aResult), JS_IsExceptionPending(aCx)));

  // Complete the dynamic import, report failures indicated by aResult or as a
  // pending exception on the context.

  JS::DynamicImportStatus status =
      (NS_FAILED(aResult) || JS_IsExceptionPending(aCx))
          ? JS::DynamicImportStatus::Failed
          : JS::DynamicImportStatus::Ok;

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
  if (!moduleRequest) {
    JS_ReportOutOfMemory(aCx);
  }

  JS::FinishDynamicModuleImport_NoTLA(aCx, status, referencingScript,
                                      moduleRequest, promise);

  // FinishDynamicModuleImport clears any pending exception.
  MOZ_ASSERT(!JS_IsExceptionPending(aCx));

  aRequest->ClearDynamicImport();
}

void ScriptLoader::FinishDynamicImport(
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
  if (!moduleRequest) {
    JS_ReportOutOfMemory(aCx);
  }

  JS::FinishDynamicModuleImport(aCx, aEvaluationPromise, referencingScript,
                                moduleRequest, promise);

  // FinishDynamicModuleImport clears any pending exception.
  MOZ_ASSERT(!JS_IsExceptionPending(aCx));

  aRequest->ClearDynamicImport();
}

static void DynamicImportPrefChangedCallback(const char* aPrefName,
                                             void* aClosure) {
  bool enabled = Preferences::GetBool(aPrefName);
  JS::ModuleDynamicImportHook hook =
      enabled ? HostImportModuleDynamically : nullptr;

  AutoJSAPI jsapi;
  jsapi.Init();
  JSRuntime* rt = JS_GetRuntime(jsapi.cx());
  JS::SetModuleDynamicImportHook(rt, hook);
}

void ScriptLoader::EnsureModuleHooksInitialized() {
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

  Preferences::RegisterCallbackAndCall(DynamicImportPrefChangedCallback,
                                       "javascript.options.dynamicImport",
                                       (void*)nullptr);
}

void ScriptLoader::CheckModuleDependenciesLoaded(ModuleLoadRequest* aRequest) {
  LOG(("ScriptLoadRequest (%p): Check dependencies loaded", aRequest));

  RefPtr<ModuleScript> moduleScript = aRequest->mModuleScript;
  if (!moduleScript || moduleScript->HasParseError()) {
    return;
  }

  for (auto childRequest : aRequest->mImports) {
    ModuleScript* childScript = childRequest->mModuleScript;
    if (!childScript) {
      aRequest->mModuleScript = nullptr;
      LOG(("ScriptLoadRequest (%p):   %p failed (load error)", aRequest,
           childRequest.get()));
      return;
    }
  }

  LOG(("ScriptLoadRequest (%p):   all ok", aRequest));
}

class ScriptRequestProcessor : public Runnable {
 private:
  RefPtr<ScriptLoader> mLoader;
  RefPtr<ScriptLoadRequest> mRequest;

 public:
  ScriptRequestProcessor(ScriptLoader* aLoader, ScriptLoadRequest* aRequest)
      : Runnable("dom::ScriptRequestProcessor"),
        mLoader(aLoader),
        mRequest(aRequest) {}
  NS_IMETHOD Run() override {
    if (mRequest->IsModuleRequest() &&
        mRequest->AsModuleRequest()->IsDynamicImport()) {
      mLoader->ProcessDynamicImport(mRequest->AsModuleRequest());
      return NS_OK;
    }

    return mLoader->ProcessRequest(mRequest);
  }
};

void ScriptLoader::RunScriptWhenSafe(ScriptLoadRequest* aRequest) {
  auto runnable = new ScriptRequestProcessor(this, aRequest);
  nsContentUtils::AddScriptRunner(runnable);
}

void ScriptLoader::ProcessLoadedModuleTree(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsReadyToRun());

  if (aRequest->IsTopLevel()) {
    if (aRequest->IsDynamicImport()) {
      MOZ_ASSERT(aRequest->isInList());
      RefPtr<ScriptLoadRequest> req = mDynamicImportRequests.Steal(aRequest);
      RunScriptWhenSafe(req);
    } else if (aRequest->mIsInline &&
               aRequest->GetParserCreated() == NOT_FROM_PARSER) {
      MOZ_ASSERT(!aRequest->isInList());
      RunScriptWhenSafe(aRequest);
    } else {
      MaybeMoveToLoadedList(aRequest);
      ProcessPendingRequests();
    }
  }

  aRequest->MaybeUnblockOnload();
}

JS::Value ScriptLoader::FindFirstParseError(ModuleLoadRequest* aRequest) {
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

bool ScriptLoader::InstantiateModuleTree(ModuleLoadRequest* aRequest) {
  // Instantiate a top-level module and record any error.

  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aRequest->IsTopLevel());

  LOG(("ScriptLoadRequest (%p): Instantiate module tree", aRequest));

  ModuleScript* moduleScript = aRequest->mModuleScript;
  MOZ_ASSERT(moduleScript);

  JS::Value parseError = FindFirstParseError(aRequest);
  if (!parseError.isUndefined()) {
    moduleScript->SetErrorToRethrow(parseError);
    LOG(("ScriptLoadRequest (%p):   found parse error", aRequest));
    return true;
  }

  MOZ_ASSERT(moduleScript->ModuleRecord());

  nsAutoMicroTask mt;
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(moduleScript->ModuleRecord()))) {
    return false;
  }

  JS::Rooted<JSObject*> module(jsapi.cx(), moduleScript->ModuleRecord());
  bool ok = NS_SUCCEEDED(nsJSUtils::ModuleInstantiate(jsapi.cx(), module));

  if (!ok) {
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

nsresult ScriptLoader::InitDebuggerDataForModuleTree(
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
    nsresult rv = InitDebuggerDataForModuleTree(aCx, childRequest);
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

nsresult ScriptLoader::RestartLoad(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsBytecode());
  aRequest->mScriptBytecode.clearAndFree();
  TRACE_FOR_TEST(aRequest->GetScriptElement(), "scriptloader_fallback");

  // Notify preload restart so that we can register this preload request again.
  aRequest->NotifyRestart(mDocument);

  // Start a new channel from which we explicitly request to stream the source
  // instead of the bytecode.
  aRequest->mProgress = ScriptLoadRequest::Progress::eLoading_Source;
  nsresult rv = StartLoad(aRequest);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Close the current channel and this ScriptLoadHandler as we created a new
  // one for the same request.
  return NS_BINDING_RETARGETED;
}

nsresult ScriptLoader::StartLoad(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsLoading());
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);
  aRequest->SetUnknownDataType();

  // If this document is sandboxed without 'allow-scripts', abort.
  if (mDocument->HasScriptsBlockedBySandbox()) {
    return NS_OK;
  }

  if (LOG_ENABLED()) {
    nsAutoCString url;
    aRequest->mURI->GetAsciiSpec(url);
    LOG(("ScriptLoadRequest (%p): Start Load (url = %s)", aRequest, url.get()));
  }

  if (aRequest->IsModuleRequest()) {
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
    if (ModuleMapContainsURL(request->mURI, aRequest->GetWebExtGlobal())) {
      LOG(("ScriptLoadRequest (%p): Waiting for module fetch", aRequest));
      WaitForModuleFetch(request->mURI, aRequest->GetWebExtGlobal())
          ->Then(GetMainThreadSerialEventTarget(), __func__, request,
                 &ModuleLoadRequest::ModuleLoaded,
                 &ModuleLoadRequest::LoadFailed);
      return NS_OK;
    }
  }

  nsContentPolicyType contentPolicyType =
      ScriptLoadRequestToContentPolicyType(aRequest);
  nsCOMPtr<nsINode> context;
  if (aRequest->GetScriptElement()) {
    context = do_QueryInterface(aRequest->GetScriptElement());
  } else {
    context = mDocument;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = mDocument->GetDocumentLoadGroup();
  nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_NULL_POINTER);
  nsIDocShell* docshell = window->GetDocShell();
  nsCOMPtr<nsIInterfaceRequestor> prompter(do_QueryInterface(docshell));

  nsSecurityFlags securityFlags;
  if (aRequest->IsModuleRequest()) {
    // According to the spec, module scripts have different behaviour to classic
    // scripts and always use CORS. Only exception: Non linkable about: pages
    // which load local module scripts.
    if (IsAboutPageLoadingChromeURI(aRequest, mDocument)) {
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
  } else {
    securityFlags =
        aRequest->CORSMode() == CORS_NONE
            ? nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL
            : nsILoadInfo::SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT;
    if (aRequest->CORSMode() == CORS_ANONYMOUS) {
      securityFlags |= nsILoadInfo::SEC_COOKIES_SAME_ORIGIN;
    } else if (aRequest->CORSMode() == CORS_USE_CREDENTIALS) {
      securityFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;
    }
  }
  securityFlags |= nsILoadInfo::SEC_ALLOW_CHROME;

  nsCOMPtr<nsIChannel> channel;
  nsresult rv = NS_NewChannelWithTriggeringPrincipal(
      getter_AddRefs(channel), aRequest->mURI, context,
      aRequest->TriggeringPrincipal(), securityFlags, contentPolicyType,
      nullptr,  // aPerformanceStorage
      loadGroup, prompter);

  NS_ENSURE_SUCCESS(rv, rv);

  // snapshot the nonce at load start time for performing CSP checks
  if (contentPolicyType == nsIContentPolicy::TYPE_INTERNAL_SCRIPT ||
      contentPolicyType == nsIContentPolicy::TYPE_INTERNAL_MODULE) {
    if (context) {
      nsString* cspNonce =
          static_cast<nsString*>(context->GetProperty(nsGkAtoms::nonce));
      if (cspNonce) {
        nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
        loadInfo->SetCspNonce(*cspNonce);
      }
    }
  }

  // Module requests aren't cached, so ignore WebExtGlobal here.
  nsCOMPtr<nsIScriptGlobalObject> globalObject =
      GetScriptGlobalObject(WebExtGlobal::Ignore);
  if (!globalObject) {
    return NS_ERROR_FAILURE;
  }

  // To avoid decoding issues, the build-id is part of the JSBytecodeMimeType
  // constant.
  aRequest->mCacheInfo = nullptr;
  nsCOMPtr<nsICacheInfoChannel> cic(do_QueryInterface(channel));
  if (cic && StaticPrefs::dom_script_loader_bytecode_cache_enabled() &&
      // Globals with instrumentation have modified script bytecode and can't
      // use cached bytecode.
      !js::GlobalHasInstrumentation(globalObject->GetGlobalJSObject()) &&
      // Bug 1436400: no bytecode cache support for modules yet.
      !aRequest->IsModuleRequest()) {
    MOZ_ASSERT(!aRequest->GetWebExtGlobal(),
               "Can not bytecode cache WebExt code");
    if (!aRequest->IsLoadingSource()) {
      // Inform the HTTP cache that we prefer to have information coming from
      // the bytecode cache instead of the sources, if such entry is already
      // registered.
      LOG(("ScriptLoadRequest (%p): Maybe request bytecode", aRequest));
      cic->PreferAlternativeDataType(nsContentUtils::JSBytecodeMimeType(),
                                     ""_ns, true);
    } else {
      // If we are explicitly loading from the sources, such as after a
      // restarted request, we might still want to save the bytecode after.
      //
      // The following tell the cache to look for an alternative data type which
      // does not exist, such that we can later save the bytecode with a
      // different alternative data type.
      LOG(("ScriptLoadRequest (%p): Request saving bytecode later", aRequest));
      cic->PreferAlternativeDataType(kNullMimeType, ""_ns, true);
    }
  }

  LOG(("ScriptLoadRequest (%p): mode=%u tracking=%d", aRequest,
       unsigned(aRequest->mScriptMode), aRequest->IsTracking()));

  if (aRequest->IsLinkPreloadScript()) {
    // This is <link rel="preload" as="script"> initiated speculative load,
    // put it to the group that is not blocked by leaders and doesn't block
    // follower at the same time. Giving it a much higher priority will make
    // this request be processed ahead of other Unblocked requests, but with
    // the same weight as Leaders.  This will make us behave similar way for
    // both http2 and http1.
    ScriptLoadRequest::PrioritizeAsPreload(channel);
    ScriptLoadRequest::AddLoadBackgroundFlag(channel);
  } else if (nsCOMPtr<nsIClassOfService> cos = do_QueryInterface(channel)) {
    if (aRequest->mScriptFromHead && aRequest->IsBlockingScript()) {
      // synchronous head scripts block loading of most other non js/css
      // content such as images, Leader implicitely disallows tailing
      cos->AddClassFlags(nsIClassOfService::Leader);
    } else if (aRequest->IsDeferredScript() &&
               !StaticPrefs::network_http_tailing_enabled()) {
      // Bug 1395525 and the !StaticPrefs::network_http_tailing_enabled() bit:
      // We want to make sure that turing tailing off by the pref makes the
      // browser behave exactly the same way as before landing the tailing
      // patch.

      // head/body deferred scripts are blocked by leaders but are not
      // allowed tailing because they block DOMContentLoaded
      cos->AddClassFlags(nsIClassOfService::TailForbidden);
    } else {
      // other scripts (=body sync or head/body async) are neither blocked
      // nor prioritized
      cos->AddClassFlags(nsIClassOfService::Unblocked);

      if (aRequest->IsAsyncScript()) {
        // async scripts are allowed tailing, since those and only those
        // don't block DOMContentLoaded; this flag doesn't enforce tailing,
        // just overweights the Unblocked flag when the channel is found
        // to be a thrird-party tracker and thus set the Tail flag to engage
        // tailing.
        cos->AddClassFlags(nsIClassOfService::TailAllowed);
      }
    }
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    // HTTP content negotation has little value in this context.
    nsAutoCString acceptTypes("*/*");
    rv = httpChannel->SetRequestHeader("Accept"_ns, acceptTypes, false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIReferrerInfo> referrerInfo =
        new ReferrerInfo(aRequest->mReferrer, aRequest->ReferrerPolicy());
    rv = httpChannel->SetReferrerInfoWithoutClone(referrerInfo);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIHttpChannelInternal> internalChannel(
        do_QueryInterface(httpChannel));
    if (internalChannel) {
      rv = internalChannel->SetIntegrityMetadata(
          aRequest->mIntegrity.GetIntegrityString());
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  mozilla::net::PredictorLearn(
      aRequest->mURI, mDocument->GetDocumentURI(),
      nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE,
      mDocument->NodePrincipal()->OriginAttributesRef());

  // Set the initiator type
  nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(httpChannel));
  if (timedChannel) {
    if (aRequest->IsLinkPreloadScript()) {
      timedChannel->SetInitiatorType(u"link"_ns);
    } else {
      timedChannel->SetInitiatorType(u"script"_ns);
    }
  }

  UniquePtr<mozilla::dom::SRICheckDataVerifier> sriDataVerifier;
  if (!aRequest->mIntegrity.IsEmpty()) {
    nsAutoCString sourceUri;
    if (mDocument->GetDocumentURI()) {
      mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
    }
    sriDataVerifier = MakeUnique<SRICheckDataVerifier>(aRequest->mIntegrity,
                                                       sourceUri, mReporter);
  }

  RefPtr<ScriptLoadHandler> handler =
      new ScriptLoadHandler(this, aRequest, std::move(sriDataVerifier));

  nsCOMPtr<nsIIncrementalStreamLoader> loader;
  rv = NS_NewIncrementalStreamLoader(getter_AddRefs(loader), handler);
  NS_ENSURE_SUCCESS(rv, rv);

  auto key = PreloadHashKey::CreateAsScript(
      aRequest->mURI, aRequest->CORSMode(), aRequest->mKind);
  aRequest->NotifyOpen(key, channel, mDocument,
                       aRequest->IsLinkPreloadScript());

  rv = channel->AsyncOpen(loader);

  if (NS_FAILED(rv)) {
    // Make sure to inform any <link preload> tags about failure to load the
    // resource.
    aRequest->NotifyStart(channel);
    aRequest->NotifyStop(rv);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  if (aRequest->IsModuleRequest()) {
    // We successfully started fetching a module so put its URL in the module
    // map and mark it as fetching.
    SetModuleFetchStarted(aRequest->AsModuleRequest());
    LOG(("ScriptLoadRequest (%p): Start fetching module", aRequest));
  }

  return NS_OK;
}

bool ScriptLoader::PreloadURIComparator::Equals(const PreloadInfo& aPi,
                                                nsIURI* const& aURI) const {
  bool same;
  return NS_SUCCEEDED(aPi.mRequest->mURI->Equals(aURI, &same)) && same;
}

static bool CSPAllowsInlineScript(nsIScriptElement* aElement,
                                  Document* aDocument) {
  nsCOMPtr<nsIContentSecurityPolicy> csp = aDocument->GetCsp();
  nsresult rv = NS_OK;

  if (!csp) {
    // no CSP --> allow
    return true;
  }

  // query the nonce
  nsCOMPtr<Element> scriptContent = do_QueryInterface(aElement);
  nsAutoString nonce;
  if (scriptContent) {
    nsString* cspNonce =
        static_cast<nsString*>(scriptContent->GetProperty(nsGkAtoms::nonce));
    if (cspNonce) {
      nonce = *cspNonce;
    }
  }

  bool parserCreated =
      aElement->GetParserCreated() != mozilla::dom::NOT_FROM_PARSER;

  bool allowInlineScript = false;
  rv = csp->GetAllowsInline(
      nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE, nonce, parserCreated,
      scriptContent, nullptr /* nsICSPEventListener */, u""_ns,
      aElement->GetScriptLineNumber(), aElement->GetScriptColumnNumber(),
      &allowInlineScript);
  return NS_SUCCEEDED(rv) && allowInlineScript;
}

ScriptLoadRequest* ScriptLoader::CreateLoadRequest(
    ScriptKind aKind, nsIURI* aURI, nsIScriptElement* aElement,
    nsIPrincipal* aTriggeringPrincipal, CORSMode aCORSMode,
    const SRIMetadata& aIntegrity, ReferrerPolicy aReferrerPolicy) {
  nsIURI* referrer = mDocument->GetDocumentURIAsReferrer();
  nsCOMPtr<Element> domElement = do_QueryInterface(aElement);
  ScriptFetchOptions* fetchOptions = new ScriptFetchOptions(
      aCORSMode, aReferrerPolicy, domElement, aTriggeringPrincipal, nullptr);

  if (aKind == ScriptKind::eClassic) {
    return new ScriptLoadRequest(aKind, aURI, fetchOptions, aIntegrity,
                                 referrer);
  }

  MOZ_ASSERT(aKind == ScriptKind::eModule);
  return ModuleLoadRequest::CreateTopLevel(aURI, fetchOptions, aIntegrity,
                                           referrer, this);
}

bool ScriptLoader::ProcessScriptElement(nsIScriptElement* aElement) {
  // We need a document to evaluate scripts.
  NS_ENSURE_TRUE(mDocument, false);

  // Check to see if scripts has been turned off.
  if (!mEnabled || !mDocument->IsScriptEnabled()) {
    return false;
  }

  NS_ASSERTION(!aElement->IsMalformed(), "Executing malformed script");

  nsCOMPtr<nsIContent> scriptContent = do_QueryInterface(aElement);

  nsAutoString type;
  bool hasType = aElement->GetScriptType(type);

  ScriptKind scriptKind = aElement->GetScriptIsModule() ? ScriptKind::eModule
                                                        : ScriptKind::eClassic;

  // Step 13. Check that the script is not an eventhandler
  if (IsScriptEventHandler(scriptKind, scriptContent)) {
    return false;
  }

  // For classic scripts, check the type attribute to determine language and
  // version. If type exists, it trumps the deprecated 'language='
  if (scriptKind == ScriptKind::eClassic) {
    if (!type.IsEmpty()) {
      NS_ENSURE_TRUE(nsContentUtils::IsJavascriptMIMEType(type), false);
    } else if (!hasType) {
      // no 'type=' element
      // "language" is a deprecated attribute of HTML, so we check it only for
      // HTML script elements.
      if (scriptContent->IsHTMLElement()) {
        nsAutoString language;
        scriptContent->AsElement()->GetAttr(kNameSpaceID_None,
                                            nsGkAtoms::language, language);
        if (!language.IsEmpty()) {
          if (!nsContentUtils::IsJavaScriptLanguage(language)) {
            return false;
          }
        }
      }
    }
  }

  // "In modern user agents that support module scripts, the script element with
  // the nomodule attribute will be ignored".
  // "The nomodule attribute must not be specified on module scripts (and will
  // be ignored if it is)."
  if (mDocument->ModuleScriptsEnabled() && scriptKind == ScriptKind::eClassic &&
      scriptContent->IsHTMLElement() &&
      scriptContent->AsElement()->HasAttr(kNameSpaceID_None,
                                          nsGkAtoms::nomodule)) {
    return false;
  }

  // Step 15. and later in the HTML5 spec
  if (aElement->GetScriptExternal()) {
    return ProcessExternalScript(aElement, scriptKind, type, scriptContent);
  }

  return ProcessInlineScript(aElement, scriptKind);
}

bool ScriptLoader::ProcessExternalScript(nsIScriptElement* aElement,
                                         ScriptKind aScriptKind,
                                         nsAutoString aTypeAttr,
                                         nsIContent* aScriptContent) {
  LOG(("ScriptLoader (%p): Process external script for element %p", this,
       aElement));

  nsCOMPtr<nsIURI> scriptURI = aElement->GetScriptURI();
  if (!scriptURI) {
    // Asynchronously report the failure to create a URI object
    NS_DispatchToCurrentThread(
        NewRunnableMethod("nsIScriptElement::FireErrorEvent", aElement,
                          &nsIScriptElement::FireErrorEvent));
    return false;
  }

  SRIMetadata sriMetadata;
  {
    nsAutoString integrity;
    aScriptContent->AsElement()->GetAttr(kNameSpaceID_None,
                                         nsGkAtoms::integrity, integrity);
    GetSRIMetadata(integrity, &sriMetadata);
  }

  RefPtr<ScriptLoadRequest> request =
      LookupPreloadRequest(aElement, aScriptKind, sriMetadata);

  if (request &&
      NS_FAILED(CheckContentPolicy(mDocument, aElement, aTypeAttr, request))) {
    LOG(("ScriptLoader (%p): content policy check failed for preload", this));

    // Probably plans have changed; even though the preload was allowed seems
    // like the actual load is not; let's cancel the preload request.
    request->Cancel();
    AccumulateCategorical(LABELS_DOM_SCRIPT_PRELOAD_RESULT::RejectedByPolicy);
    return false;
  }

  if (request) {
    // Use the preload request.

    LOG(("ScriptLoadRequest (%p): Using preload request", request.get()));

    // It's possible these attributes changed since we started the preload so
    // update them here.
    request->SetScriptMode(aElement->GetScriptDeferred(),
                           aElement->GetScriptAsync(), false);

    AccumulateCategorical(LABELS_DOM_SCRIPT_PRELOAD_RESULT::Used);
  } else {
    // No usable preload found.

    nsCOMPtr<nsIPrincipal> principal =
        aElement->GetScriptURITriggeringPrincipal();
    if (!principal) {
      principal = aScriptContent->NodePrincipal();
    }

    CORSMode ourCORSMode = aElement->GetCORSMode();
    ReferrerPolicy referrerPolicy = GetReferrerPolicy(aElement);

    request = CreateLoadRequest(aScriptKind, scriptURI, aElement, principal,
                                ourCORSMode, sriMetadata, referrerPolicy);
    request->mIsInline = false;
    request->SetScriptMode(aElement->GetScriptDeferred(),
                           aElement->GetScriptAsync(), false);
    // keep request->mScriptFromHead to false so we don't treat non preloaded
    // scripts as blockers for full page load. See bug 792438.

    LOG(("ScriptLoadRequest (%p): Created request for external script",
         request.get()));

    nsresult rv = StartLoad(request);
    if (NS_FAILED(rv)) {
      ReportErrorToConsole(request, rv);

      // Asynchronously report the load failure
      nsCOMPtr<nsIRunnable> runnable =
          NewRunnableMethod("nsIScriptElement::FireErrorEvent", aElement,
                            &nsIScriptElement::FireErrorEvent);
      if (mDocument) {
        mDocument->Dispatch(TaskCategory::Other, runnable.forget());
      } else {
        NS_DispatchToCurrentThread(runnable);
      }
      return false;
    }
  }

  // We should still be in loading stage of script unless we're loading a
  // module or speculatively off-main-thread parsing a script.
  NS_ASSERTION(SpeculativeOMTParsingEnabled() || !request->InCompilingStage() ||
                   request->IsModuleRequest(),
               "Request should not yet be in compiling stage.");

  if (request->IsAsyncScript()) {
    AddAsyncRequest(request);
    if (request->IsReadyToRun()) {
      // The script is available already. Run it ASAP when the event
      // loop gets a chance to spin.

      // KVKV TODO: Instead of processing immediately, try off-thread-parsing
      // it and only schedule a pending ProcessRequest if that fails.
      ProcessPendingRequestsAsync();
    }
    return false;
  }
  if (!aElement->GetParserCreated()) {
    // Violate the HTML5 spec in order to make LABjs and the "order" plug-in
    // for RequireJS work with their Gecko-sniffed code path. See
    // http://lists.w3.org/Archives/Public/public-html/2010Oct/0088.html
    request->mIsNonAsyncScriptInserted = true;
    mNonAsyncExternalScriptInsertedRequests.AppendElement(request);
    if (request->IsReadyToRun()) {
      // The script is available already. Run it ASAP when the event
      // loop gets a chance to spin.
      ProcessPendingRequestsAsync();
    }
    return false;
  }
  // we now have a parser-inserted request that may or may not be still
  // loading
  if (request->IsDeferredScript()) {
    // We don't want to run this yet.
    // If we come here, the script is a parser-created script and it has
    // the defer attribute but not the async attribute. Since a
    // a parser-inserted script is being run, we came here by the parser
    // running the script, which means the parser is still alive and the
    // parse is ongoing.
    NS_ASSERTION(mDocument->GetCurrentContentSink() ||
                     aElement->GetParserCreated() == FROM_PARSER_XSLT,
                 "Non-XSLT Defer script on a document without an active "
                 "parser; bug 592366.");
    AddDeferRequest(request);
    return false;
  }

  if (aElement->GetParserCreated() == FROM_PARSER_XSLT) {
    // Need to maintain order for XSLT-inserted scripts
    NS_ASSERTION(!mParserBlockingRequest,
                 "Parser-blocking scripts and XSLT scripts in the same doc!");
    request->mIsXSLT = true;
    mXSLTRequests.AppendElement(request);
    if (request->IsReadyToRun()) {
      // The script is available already. Run it ASAP when the event
      // loop gets a chance to spin.
      ProcessPendingRequestsAsync();
    }
    return true;
  }

  if (request->IsReadyToRun() && ReadyToExecuteParserBlockingScripts()) {
    // The request has already been loaded and there are no pending style
    // sheets. If the script comes from the network stream, cheat for
    // performance reasons and avoid a trip through the event loop.
    if (aElement->GetParserCreated() == FROM_PARSER_NETWORK) {
      return ProcessRequest(request) == NS_ERROR_HTMLPARSER_BLOCK;
    }
    // Otherwise, we've got a document.written script, make a trip through
    // the event loop to hide the preload effects from the scripts on the
    // Web page.
    NS_ASSERTION(!mParserBlockingRequest,
                 "There can be only one parser-blocking script at a time");
    NS_ASSERTION(mXSLTRequests.isEmpty(),
                 "Parser-blocking scripts and XSLT scripts in the same doc!");
    mParserBlockingRequest = request;
    ProcessPendingRequestsAsync();
    return true;
  }

  // The script hasn't loaded yet or there's a style sheet blocking it.
  // The script will be run when it loads or the style sheet loads.
  NS_ASSERTION(!mParserBlockingRequest,
               "There can be only one parser-blocking script at a time");
  NS_ASSERTION(mXSLTRequests.isEmpty(),
               "Parser-blocking scripts and XSLT scripts in the same doc!");
  mParserBlockingRequest = request;
  return true;
}

bool ScriptLoader::ProcessInlineScript(nsIScriptElement* aElement,
                                       ScriptKind aScriptKind) {
  // Is this document sandboxed without 'allow-scripts'?
  if (mDocument->HasScriptsBlockedBySandbox()) {
    return false;
  }

  // Does CSP allow this inline script to run?
  if (!CSPAllowsInlineScript(aElement, mDocument)) {
    return false;
  }

  // Inline classic scripts ignore their CORS mode and are always CORS_NONE.
  CORSMode corsMode = CORS_NONE;
  if (aScriptKind == ScriptKind::eModule) {
    corsMode = aElement->GetCORSMode();
  }

  ReferrerPolicy referrerPolicy = GetReferrerPolicy(aElement);
  RefPtr<ScriptLoadRequest> request =
      CreateLoadRequest(aScriptKind, mDocument->GetDocumentURI(), aElement,
                        mDocument->NodePrincipal(), corsMode,
                        SRIMetadata(),  // SRI doesn't apply
                        referrerPolicy);
  request->mIsInline = true;
  request->mLineNo = aElement->GetScriptLineNumber();
  request->mProgress = ScriptLoadRequest::Progress::eLoading_Source;
  request->SetTextSource();
  TRACE_FOR_TEST_BOOL(request->GetScriptElement(), "scriptloader_load_source");
  CollectScriptTelemetry(request);

  // Only the 'async' attribute is heeded on an inline module script and
  // inline classic scripts ignore both these attributes.
  MOZ_ASSERT(!aElement->GetScriptDeferred());
  MOZ_ASSERT_IF(!request->IsModuleRequest(), !aElement->GetScriptAsync());
  request->SetScriptMode(false, aElement->GetScriptAsync(), false);

  LOG(("ScriptLoadRequest (%p): Created request for inline script",
       request.get()));

  request->mBaseURL = mDocument->GetDocBaseURI();

  if (request->IsModuleRequest()) {
    ModuleLoadRequest* modReq = request->AsModuleRequest();
    if (aElement->GetParserCreated() != NOT_FROM_PARSER) {
      if (aElement->GetScriptAsync()) {
        AddAsyncRequest(modReq);
      } else {
        AddDeferRequest(modReq);
      }
    }

    nsresult rv = ProcessFetchedModuleSource(modReq);
    if (NS_FAILED(rv)) {
      ReportErrorToConsole(modReq, rv);
      HandleLoadError(modReq, rv);
    }

    return false;
  }
  request->mProgress = ScriptLoadRequest::Progress::eReady;
  if (aElement->GetParserCreated() == FROM_PARSER_XSLT &&
      (!ReadyToExecuteParserBlockingScripts() || !mXSLTRequests.isEmpty())) {
    // Need to maintain order for XSLT-inserted scripts
    NS_ASSERTION(!mParserBlockingRequest,
                 "Parser-blocking scripts and XSLT scripts in the same doc!");
    mXSLTRequests.AppendElement(request);
    return true;
  }
  if (aElement->GetParserCreated() == NOT_FROM_PARSER) {
    NS_ASSERTION(
        !nsContentUtils::IsSafeToRunScript(),
        "A script-inserted script is inserted without an update batch?");
    RunScriptWhenSafe(request);
    return false;
  }
  if (aElement->GetParserCreated() == FROM_PARSER_NETWORK &&
      !ReadyToExecuteParserBlockingScripts()) {
    NS_ASSERTION(!mParserBlockingRequest,
                 "There can be only one parser-blocking script at a time");
    mParserBlockingRequest = request;
    NS_ASSERTION(mXSLTRequests.isEmpty(),
                 "Parser-blocking scripts and XSLT scripts in the same doc!");
    return true;
  }
  // We now have a document.written inline script or we have an inline script
  // from the network but there is no style sheet that is blocking scripts.
  // Don't check for style sheets blocking scripts in the document.write
  // case to avoid style sheet network activity affecting when
  // document.write returns. It's not really necessary to do this if
  // there's no document.write currently on the call stack. However,
  // this way matches IE more closely than checking if document.write
  // is on the call stack.
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Not safe to run a parser-inserted script?");
  return ProcessRequest(request) == NS_ERROR_HTMLPARSER_BLOCK;
}

ScriptLoadRequest* ScriptLoader::LookupPreloadRequest(
    nsIScriptElement* aElement, ScriptKind aScriptKind,
    const SRIMetadata& aSRIMetadata) {
  MOZ_ASSERT(aElement);

  nsTArray<PreloadInfo>::index_type i =
      mPreloads.IndexOf(aElement->GetScriptURI(), 0, PreloadURIComparator());
  if (i == nsTArray<PreloadInfo>::NoIndex) {
    return nullptr;
  }

  // Found preloaded request. Note that a script-inserted script can steal a
  // preload!
  RefPtr<ScriptLoadRequest> request = mPreloads[i].mRequest;
  request->SetIsLoadRequest(aElement);

  if (request->mWasCompiledOMT && !request->IsModuleRequest()) {
    request->SetReady();
  }

  nsString preloadCharset(mPreloads[i].mCharset);
  mPreloads.RemoveElementAt(i);

  // Double-check that the charset the preload used is the same as the charset
  // we have now.
  nsAutoString elementCharset;
  aElement->GetScriptCharset(elementCharset);

  if (!elementCharset.Equals(preloadCharset) ||
      aElement->GetCORSMode() != request->CORSMode() ||
      aScriptKind != request->mKind) {
    // Drop the preload.
    request->Cancel();
    AccumulateCategorical(LABELS_DOM_SCRIPT_PRELOAD_RESULT::RequestMismatch);
    return nullptr;
  }

  if (!aSRIMetadata.CanTrustBeDelegatedTo(request->mIntegrity)) {
    // Don't cancel link preload requests, we want to deliver onload according
    // the result of the load, cancellation would unexpectedly lead to error
    // notification.
    if (!request->IsLinkPreloadScript()) {
      request->Cancel();
    }
    return nullptr;
  }

  // Report any errors that we skipped while preloading.
  ReportPreloadErrorsToConsole(request);

  // This makes sure the pending preload (if exists) for this resource is
  // properly marked as used and thus not notified in the console as unused.
  request->NotifyUsage();
  // A used preload must no longer be found in the Document's hash table.  Any
  // <link preload> tag after the <script> tag will start a new request, that
  // can be satisfied from a different cache, but not from the preload cache.
  request->RemoveSelf(mDocument);

  return request;
}

void ScriptLoader::GetSRIMetadata(const nsAString& aIntegrityAttr,
                                  SRIMetadata* aMetadataOut) {
  MOZ_ASSERT(aMetadataOut->IsEmpty());

  if (aIntegrityAttr.IsEmpty()) {
    return;
  }

  MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
          ("ScriptLoader::GetSRIMetadata, integrity=%s",
           NS_ConvertUTF16toUTF8(aIntegrityAttr).get()));

  nsAutoCString sourceUri;
  if (mDocument->GetDocumentURI()) {
    mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
  }
  SRICheck::IntegrityMetadata(aIntegrityAttr, sourceUri, mReporter,
                              aMetadataOut);
}

ReferrerPolicy ScriptLoader::GetReferrerPolicy(nsIScriptElement* aElement) {
  ReferrerPolicy scriptReferrerPolicy = aElement->GetReferrerPolicy();
  if (scriptReferrerPolicy != ReferrerPolicy::_empty) {
    return scriptReferrerPolicy;
  }
  return mDocument->GetReferrerPolicy();
}

namespace {

class NotifyOffThreadScriptLoadCompletedRunnable : public Runnable {
  RefPtr<ScriptLoadRequest> mRequest;
  RefPtr<ScriptLoader> mLoader;
  nsCOMPtr<nsISerialEventTarget> mEventTarget;
  JS::OffThreadToken* mToken;

 public:
  ScriptLoadRequest* GetScriptLoadRequest() { return mRequest; }

  NotifyOffThreadScriptLoadCompletedRunnable(ScriptLoadRequest* aRequest,
                                             ScriptLoader* aLoader)
      : Runnable("dom::NotifyOffThreadScriptLoadCompletedRunnable"),
        mRequest(aRequest),
        mLoader(aLoader),
        mToken(nullptr) {
    MOZ_ASSERT(NS_IsMainThread());
    if (DocGroup* docGroup = aLoader->GetDocGroup()) {
      mEventTarget = docGroup->EventTargetFor(TaskCategory::Other);
    }
  }

  virtual ~NotifyOffThreadScriptLoadCompletedRunnable();

  void SetToken(JS::OffThreadToken* aToken) {
    MOZ_ASSERT(aToken && !mToken);
    mToken = aToken;
  }

  static void Dispatch(
      already_AddRefed<NotifyOffThreadScriptLoadCompletedRunnable>&& aSelf) {
    RefPtr<NotifyOffThreadScriptLoadCompletedRunnable> self = aSelf;
    nsCOMPtr<nsISerialEventTarget> eventTarget = self->mEventTarget;
    eventTarget->Dispatch(self.forget());
  }

  NS_DECL_NSIRUNNABLE
};

} /* anonymous namespace */

void ScriptLoader::Shutdown() {
  CancelScriptLoadRequests();
  GiveUpBytecodeEncoding();
}

void ScriptLoader::CancelScriptLoadRequests() {
  // Cancel all requests that have not been executed.
  if (mParserBlockingRequest) {
    mParserBlockingRequest->Cancel();
  }

  for (ScriptLoadRequest* req = mXSLTRequests.getFirst(); req;
       req = req->getNext()) {
    req->Cancel();
  }

  for (ScriptLoadRequest* req = mDeferRequests.getFirst(); req;
       req = req->getNext()) {
    req->Cancel();
  }

  for (ScriptLoadRequest* req = mLoadingAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    req->Cancel();
  }

  for (ScriptLoadRequest* req = mLoadedAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    req->Cancel();
  }

  for (ScriptLoadRequest* req = mDynamicImportRequests.getFirst(); req;
       req = req->getNext()) {
    req->Cancel();
  }

  for (ScriptLoadRequest* req =
           mNonAsyncExternalScriptInsertedRequests.getFirst();
       req; req = req->getNext()) {
    req->Cancel();
  }

  for (size_t i = 0; i < mPreloads.Length(); i++) {
    mPreloads[i].mRequest->Cancel();
  }
}

nsresult ScriptLoader::ProcessOffThreadRequest(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->mProgress == ScriptLoadRequest::Progress::eCompiling);
  MOZ_ASSERT(!aRequest->mWasCompiledOMT);

  aRequest->mWasCompiledOMT = true;

  if (aRequest->IsModuleRequest()) {
    MOZ_ASSERT(aRequest->mOffThreadToken);
    ModuleLoadRequest* request = aRequest->AsModuleRequest();
    return ProcessFetchedModuleSource(request);
  }

  // Element may not be ready yet if speculatively compiling, so process the
  // request in ProcessPendingRequests when it is available.
  MOZ_ASSERT_IF(!SpeculativeOMTParsingEnabled(), aRequest->GetScriptElement());
  if (!aRequest->GetScriptElement()) {
    // Unblock onload here in case this request never gets executed.
    aRequest->MaybeUnblockOnload();
    return NS_OK;
  }

  aRequest->SetReady();

  if (aRequest == mParserBlockingRequest) {
    if (!ReadyToExecuteParserBlockingScripts()) {
      // If not ready to execute scripts, schedule an async call to
      // ProcessPendingRequests to handle it.
      ProcessPendingRequestsAsync();
      return NS_OK;
    }

    // Same logic as in top of ProcessPendingRequests.
    mParserBlockingRequest = nullptr;
    UnblockParser(aRequest);
    ProcessRequest(aRequest);
    ContinueParserAsync(aRequest);
    return NS_OK;
  }

  // Async scripts and blocking scripts can be executed right away.
  if ((aRequest->IsAsyncScript() || aRequest->IsBlockingScript()) &&
      !aRequest->isInList()) {
    return ProcessRequest(aRequest);
  }

  // Process other scripts in the proper order.
  ProcessPendingRequests();
  return NS_OK;
}

NotifyOffThreadScriptLoadCompletedRunnable::
    ~NotifyOffThreadScriptLoadCompletedRunnable() {
  if (MOZ_UNLIKELY(mRequest || mLoader) && !NS_IsMainThread()) {
    NS_ReleaseOnMainThread(
        "NotifyOffThreadScriptLoadCompletedRunnable::mRequest",
        mRequest.forget());
    NS_ReleaseOnMainThread(
        "NotifyOffThreadScriptLoadCompletedRunnable::mLoader",
        mLoader.forget());
  }
}

static void GetProfilerLabelForRequest(ScriptLoadRequest* aRequest,
                                       nsACString& aOutString) {
#ifdef MOZ_GECKO_PROFILER
  if (!profiler_is_active()) {
    aOutString.Append("<script> element");
    return;
  }
  aOutString.Append("<script");
  if (aRequest->IsAsyncScript()) {
    aOutString.Append(" async");
  } else if (aRequest->IsDeferredScript()) {
    aOutString.Append(" defer");
  }
  if (aRequest->IsModuleRequest()) {
    aOutString.Append(" type=\"module\"");
  }

  nsAutoCString url;
  if (aRequest->mURI) {
    aRequest->mURI->GetAsciiSpec(url);
  } else {
    url = "<unknown>";
  }

  if (aRequest->mIsInline) {
    if (aRequest->GetParserCreated() != NOT_FROM_PARSER) {
      aOutString.Append("> inline at line ");
      aOutString.AppendInt(aRequest->mLineNo);
      aOutString.Append(" of ");
    } else {
      aOutString.Append("> inline (dynamically created) in ");
    }
    aOutString.Append(url);
  } else {
    aOutString.Append(" src=\"");
    aOutString.Append(url);
    aOutString.Append("\">");
  }
#else
  aOutString.Append("<script> element");
#endif
}

NS_IMETHODIMP
NotifyOffThreadScriptLoadCompletedRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  // We want these to be dropped on the main thread, once we return from this
  // function.
  RefPtr<ScriptLoadRequest> request = std::move(mRequest);

  // Runnable pointer should have been cleared in the offthread callback.
  MOZ_ASSERT(!request->mRunnable);

#ifdef MOZ_GECKO_PROFILER
  if (profiler_is_active()) {
    ProfilerString8View scriptSourceString;
    if (request->IsTextSource()) {
      scriptSourceString = "ScriptCompileOffThread";
    } else {
      MOZ_ASSERT(request->IsBytecode());
      scriptSourceString = "BytecodeDecodeOffThread";
    }

    nsAutoCString profilerLabelString;
    GetProfilerLabelForRequest(request, profilerLabelString);
    PROFILER_MARKER_TEXT(
        scriptSourceString, JS,
        MarkerTiming::Interval(request->mOffThreadParseStartTime,
                               request->mOffThreadParseStopTime),
        profilerLabelString);
  }
#endif

  RefPtr<ScriptLoader> loader = std::move(mLoader);

  // Request was already cancelled at some earlier point.
  if (!request->mOffThreadToken) {
    return NS_OK;
  }

  return loader->ProcessOffThreadRequest(request);
}

static void OffThreadScriptLoaderCallback(JS::OffThreadToken* aToken,
                                          void* aCallbackData) {
  RefPtr<NotifyOffThreadScriptLoadCompletedRunnable> aRunnable = dont_AddRef(
      static_cast<NotifyOffThreadScriptLoadCompletedRunnable*>(aCallbackData));
  MOZ_ASSERT(aRunnable.get() == aRunnable->GetScriptLoadRequest()->mRunnable);

#ifdef MOZ_GECKO_PROFILER
  aRunnable->GetScriptLoadRequest()->mOffThreadParseStopTime =
      TimeStamp::NowUnfuzzed();
#endif

  LogRunnable::Run run(aRunnable);

  aRunnable->SetToken(aToken);

  // If mRunnable was cleared then request was canceled so do nothing.
  if (!aRunnable->GetScriptLoadRequest()->mRunnable.exchange(nullptr)) {
    return;
  }

  NotifyOffThreadScriptLoadCompletedRunnable::Dispatch(aRunnable.forget());
}

nsresult ScriptLoader::AttemptAsyncScriptCompile(ScriptLoadRequest* aRequest,
                                                 bool* aCouldCompileOut) {
  // If speculative parsing is enabled, the request may not be ready to run if
  // the element is not yet available.
  MOZ_ASSERT_IF(!SpeculativeOMTParsingEnabled() && !aRequest->IsModuleRequest(),
                aRequest->IsReadyToRun());
  MOZ_ASSERT(!aRequest->mWasCompiledOMT);
  MOZ_ASSERT(aCouldCompileOut && !*aCouldCompileOut);

  // Don't off-thread compile inline scripts.
  if (aRequest->mIsInline) {
    return NS_OK;
  }

  nsCOMPtr<nsIGlobalObject> globalObject = GetGlobalForRequest(aRequest);
  if (!globalObject) {
    return NS_ERROR_FAILURE;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(globalObject)) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> global(cx, globalObject->GetGlobalJSObject());
  JS::CompileOptions options(cx);

  nsresult rv = FillCompileOptionsForRequest(jsapi, aRequest, global, &options);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aRequest->IsTextSource()) {
    if (!JS::CanCompileOffThread(cx, options, aRequest->ScriptTextLength())) {
      return NS_OK;
    }
  } else {
    MOZ_ASSERT(aRequest->IsBytecode());

    // NOTE: Regardless of using stencil XDR or not, we use off-thread parse
    //       global and instantiate off-thread, to avoid regressing performance.
    options.useOffThreadParseGlobal = true;

    size_t length =
        aRequest->mScriptBytecode.length() - aRequest->mBytecodeOffset;
    if (!JS::CanDecodeOffThread(cx, options, length)) {
      return NS_OK;
    }
  }

  RefPtr<NotifyOffThreadScriptLoadCompletedRunnable> runnable =
      new NotifyOffThreadScriptLoadCompletedRunnable(aRequest, this);

  // Emulate dispatch.  CompileOffThreadModule will call
  // OffThreadScriptLoaderCallback were we will emulate run.
  LogRunnable::LogDispatch(runnable);

#ifdef MOZ_GECKO_PROFILER
  aRequest->mOffThreadParseStartTime = TimeStamp::NowUnfuzzed();
#endif

  // Save the runnable so it can be properly cleared during cancellation.
  aRequest->mRunnable = runnable.get();
  auto signalOOM =
      mozilla::MakeScopeExit([&aRequest]() { aRequest->mRunnable = nullptr; });

  if (aRequest->IsModuleRequest()) {
    MOZ_ASSERT(aRequest->IsTextSource());
    MaybeSourceText maybeSource;
    nsresult rv = GetScriptSource(cx, aRequest, &maybeSource);
    NS_ENSURE_SUCCESS(rv, rv);

    aRequest->mOffThreadToken =
        maybeSource.constructed<SourceText<char16_t>>()
            ? JS::CompileOffThreadModule(
                  cx, options, maybeSource.ref<SourceText<char16_t>>(),
                  OffThreadScriptLoaderCallback, static_cast<void*>(runnable))
            : JS::CompileOffThreadModule(
                  cx, options, maybeSource.ref<SourceText<Utf8Unit>>(),
                  OffThreadScriptLoaderCallback, static_cast<void*>(runnable));
    if (!aRequest->mOffThreadToken) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else if (aRequest->IsBytecode()) {
    aRequest->mOffThreadToken = JS::DecodeOffThreadScript(
        cx, options, aRequest->mScriptBytecode, aRequest->mBytecodeOffset,
        OffThreadScriptLoaderCallback, static_cast<void*>(runnable));
    if (!aRequest->mOffThreadToken) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    MOZ_ASSERT(aRequest->IsTextSource());
    MaybeSourceText maybeSource;
    nsresult rv = GetScriptSource(cx, aRequest, &maybeSource);
    NS_ENSURE_SUCCESS(rv, rv);

    aRequest->mOffThreadToken =
        maybeSource.constructed<SourceText<char16_t>>()
            ? JS::CompileOffThread(
                  cx, options, maybeSource.ref<SourceText<char16_t>>(),
                  OffThreadScriptLoaderCallback, static_cast<void*>(runnable))
            : JS::CompileOffThread(
                  cx, options, maybeSource.ref<SourceText<Utf8Unit>>(),
                  OffThreadScriptLoaderCallback, static_cast<void*>(runnable));
    if (!aRequest->mOffThreadToken) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  signalOOM.release();

  aRequest->BlockOnload(mDocument);

  // Once the compilation is finished, an event would be added to the event loop
  // to call ScriptLoader::ProcessOffThreadRequest with the same request.
  aRequest->mProgress = ScriptLoadRequest::Progress::eCompiling;

  *aCouldCompileOut = true;
  Unused << runnable.forget();
  return NS_OK;
}

nsresult ScriptLoader::CompileOffThreadOrProcessRequest(
    ScriptLoadRequest* aRequest) {
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");

  if (!aRequest->mOffThreadToken && !aRequest->InCompilingStage()) {
    bool couldCompile = false;
    nsresult rv = AttemptAsyncScriptCompile(aRequest, &couldCompile);
    if (NS_FAILED(rv)) {
      HandleLoadError(aRequest, rv);
      return rv;
    }

    if (couldCompile) {
      return NS_OK;
    }
  }

  return ProcessRequest(aRequest);
}

nsresult ScriptLoader::GetScriptSource(JSContext* aCx,
                                       ScriptLoadRequest* aRequest,
                                       MaybeSourceText* aMaybeSource) {
  // If there's no script text, we try to get it from the element
  if (aRequest->mIsInline) {
    nsAutoString inlineData;
    aRequest->GetScriptElement()->GetScriptText(inlineData);

    size_t nbytes = inlineData.Length() * sizeof(char16_t);
    JS::UniqueTwoByteChars chars(
        static_cast<char16_t*>(JS_malloc(aCx, nbytes)));
    if (!chars) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    memcpy(chars.get(), inlineData.get(), nbytes);

    SourceText<char16_t> srcBuf;
    if (!srcBuf.init(aCx, std::move(chars), inlineData.Length())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    aMaybeSource->construct<SourceText<char16_t>>(std::move(srcBuf));
    return NS_OK;
  }

  size_t length = aRequest->ScriptTextLength();
  if (aRequest->IsUTF16Text()) {
    JS::UniqueTwoByteChars chars;
    chars.reset(aRequest->ScriptText<char16_t>().extractOrCopyRawBuffer());
    if (!chars) {
      JS_ReportOutOfMemory(aCx);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    SourceText<char16_t> srcBuf;
    if (!srcBuf.init(aCx, std::move(chars), length)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    aMaybeSource->construct<SourceText<char16_t>>(std::move(srcBuf));
    return NS_OK;
  }

  MOZ_ASSERT(aRequest->IsUTF8Text());
  UniquePtr<Utf8Unit[], JS::FreePolicy> chars;
  chars.reset(aRequest->ScriptText<Utf8Unit>().extractOrCopyRawBuffer());
  if (!chars) {
    JS_ReportOutOfMemory(aCx);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(aCx, std::move(chars), length)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  aMaybeSource->construct<SourceText<Utf8Unit>>(std::move(srcBuf));
  return NS_OK;
}

nsresult ScriptLoader::ProcessRequest(ScriptLoadRequest* aRequest) {
  LOG(("ScriptLoadRequest (%p): Process request", aRequest));

  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");
  NS_ASSERTION(aRequest->IsReadyToRun(),
               "Processing a request that is not ready to run.");

  NS_ENSURE_ARG(aRequest);

  auto unblockOnload = MakeScopeExit([&] { aRequest->MaybeUnblockOnload(); });

  if (aRequest->IsModuleRequest()) {
    ModuleLoadRequest* request = aRequest->AsModuleRequest();
    if (request->mModuleScript) {
      if (!InstantiateModuleTree(request)) {
        request->mModuleScript = nullptr;
      }
    }

    if (!request->mModuleScript) {
      // There was an error fetching a module script.  Nothing to do here.
      LOG(("ScriptLoadRequest (%p):   Error loading request, firing error",
           aRequest));
      FireScriptAvailable(NS_ERROR_FAILURE, aRequest);
      return NS_OK;
    }
  }

  nsCOMPtr<nsINode> scriptElem =
      do_QueryInterface(aRequest->GetScriptElement());

  nsCOMPtr<Document> doc;
  if (!aRequest->mIsInline || aRequest->IsModuleRequest()) {
    doc = scriptElem->OwnerDoc();
  }

  nsCOMPtr<nsIScriptElement> oldParserInsertedScript;
  uint32_t parserCreated = aRequest->GetParserCreated();
  if (parserCreated) {
    oldParserInsertedScript = mCurrentParserInsertedScript;
    mCurrentParserInsertedScript = aRequest->GetScriptElement();
  }

  aRequest->GetScriptElement()->BeginEvaluating();

  FireScriptAvailable(NS_OK, aRequest);

  // The window may have gone away by this point, in which case there's no point
  // in trying to run the script.

  {
    // Try to perform a microtask checkpoint
    nsAutoMicroTask mt;
  }

  nsPIDOMWindowInner* pwin = mDocument->GetInnerWindow();
  bool runScript = !!pwin;
  if (runScript) {
    nsContentUtils::DispatchTrustedEvent(
        scriptElem->OwnerDoc(), scriptElem, u"beforescriptexecute"_ns,
        CanBubble::eYes, Cancelable::eYes, &runScript);
  }

  // Inner window could have gone away after firing beforescriptexecute
  pwin = mDocument->GetInnerWindow();
  if (!pwin) {
    runScript = false;
  }

  nsresult rv = NS_OK;
  if (runScript) {
    if (doc) {
      doc->IncrementIgnoreDestructiveWritesCounter();
    }
    rv = EvaluateScript(aRequest);
    if (doc) {
      doc->DecrementIgnoreDestructiveWritesCounter();
    }

    nsContentUtils::DispatchTrustedEvent(scriptElem->OwnerDoc(), scriptElem,
                                         u"afterscriptexecute"_ns,
                                         CanBubble::eYes, Cancelable::eNo);
  }

  FireScriptEvaluated(rv, aRequest);

  aRequest->GetScriptElement()->EndEvaluating();

  if (parserCreated) {
    mCurrentParserInsertedScript = oldParserInsertedScript;
  }

  if (aRequest->mOffThreadToken) {
    // The request was parsed off-main-thread, but the result of the off
    // thread parse was not actually needed to process the request
    // (disappearing window, some other error, ...). Finish the
    // request to avoid leaks in the JS engine.
    MOZ_ASSERT(!aRequest->IsModuleRequest());
    aRequest->MaybeCancelOffThreadScript();
  }

  // Free any source data, but keep the bytecode content as we might have to
  // save it later.
  aRequest->ClearScriptSource();
  if (aRequest->IsBytecode()) {
    // We received bytecode as input, thus we were decoding, and we will not be
    // encoding the bytecode once more. We can safely clear the content of this
    // buffer.
    aRequest->mScriptBytecode.clearAndFree();
  }

  return rv;
}

void ScriptLoader::ProcessDynamicImport(ModuleLoadRequest* aRequest) {
  if (aRequest->mModuleScript) {
    if (!InstantiateModuleTree(aRequest)) {
      aRequest->mModuleScript = nullptr;
    }
  }

  nsresult rv = NS_ERROR_FAILURE;
  if (aRequest->mModuleScript) {
    rv = EvaluateScript(aRequest);
  }

  if (NS_FAILED(rv)) {
    FinishDynamicImportAndReject(aRequest, rv);
  }
}

void ScriptLoader::FireScriptAvailable(nsresult aResult,
                                       ScriptLoadRequest* aRequest) {
  for (int32_t i = 0; i < mObservers.Count(); i++) {
    nsCOMPtr<nsIScriptLoaderObserver> obs = mObservers[i];
    obs->ScriptAvailable(aResult, aRequest->GetScriptElement(),
                         aRequest->mIsInline, aRequest->mURI,
                         aRequest->mLineNo);
  }

  aRequest->FireScriptAvailable(aResult);
}

void ScriptLoader::FireScriptEvaluated(nsresult aResult,
                                       ScriptLoadRequest* aRequest) {
  for (int32_t i = 0; i < mObservers.Count(); i++) {
    nsCOMPtr<nsIScriptLoaderObserver> obs = mObservers[i];
    obs->ScriptEvaluated(aResult, aRequest->GetScriptElement(),
                         aRequest->mIsInline);
  }

  aRequest->FireScriptEvaluated(aResult);
}

already_AddRefed<nsIGlobalObject> ScriptLoader::GetGlobalForRequest(
    ScriptLoadRequest* aRequest) {
  if (aRequest->GetWebExtGlobal()) {
    nsCOMPtr<nsIGlobalObject> global = aRequest->GetWebExtGlobal();
    return global.forget();
  }

  return GetScriptGlobalObject(WebExtGlobal::Handled);
}

already_AddRefed<nsIScriptGlobalObject> ScriptLoader::GetScriptGlobalObject(
    WebExtGlobal) {
  if (!mDocument) {
    return nullptr;
  }

  nsPIDOMWindowInner* pwin = mDocument->GetInnerWindow();
  if (!pwin) {
    return nullptr;
  }

  nsCOMPtr<nsIScriptGlobalObject> globalObject = do_QueryInterface(pwin);
  NS_ASSERTION(globalObject, "windows must be global objects");

  // and make sure we are setup for this type of script.
  nsresult rv = globalObject->EnsureScriptEnvironment();
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return globalObject.forget();
}

nsresult ScriptLoader::FillCompileOptionsForRequest(
    const mozilla::dom::AutoJSAPI& jsapi, ScriptLoadRequest* aRequest,
    JS::Handle<JSObject*> aScopeChain, JS::CompileOptions* aOptions) {
  // It's very important to use aRequest->mURI, not the final URI of the channel
  // aRequest ended up getting script data from, as the script filename.
  nsresult rv = aRequest->mURI->GetSpec(aRequest->mURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mDocument) {
    mDocument->NoteScriptTrackingStatus(aRequest->mURL, aRequest->IsTracking());
  }

  const char* introductionType;
  if (aRequest->IsModuleRequest() &&
      !aRequest->AsModuleRequest()->IsTopLevel()) {
    introductionType = "importedModule";
  } else if (!aRequest->mIsInline) {
    introductionType = "srcScript";
  } else if (aRequest->GetParserCreated() == FROM_PARSER_NETWORK) {
    introductionType = "inlineScript";
  } else {
    introductionType = "injectedScript";
  }
  aOptions->setIntroductionInfoToCaller(jsapi.cx(), introductionType);
  aOptions->setFileAndLine(aRequest->mURL.get(), aRequest->mLineNo);
  aOptions->setIsRunOnce(true);
  aOptions->setNoScriptRval(true);
  if (aRequest->mSourceMapURL) {
    aOptions->setSourceMapURL(aRequest->mSourceMapURL->get());
  }
  if (aRequest->mOriginPrincipal) {
    nsIPrincipal* scriptPrin = nsContentUtils::ObjectPrincipal(aScopeChain);
    bool subsumes = scriptPrin->Subsumes(aRequest->mOriginPrincipal);
    aOptions->setMutedErrors(!subsumes);
  }

  if (aRequest->IsModuleRequest()) {
    aOptions->hideScriptFromDebugger = true;
  }

  return NS_OK;
}

/* static */
bool ScriptLoader::ShouldCacheBytecode(ScriptLoadRequest* aRequest) {
  using mozilla::TimeDuration;
  using mozilla::TimeStamp;

  // We need the nsICacheInfoChannel to exist to be able to open the alternate
  // data output stream. This pointer would only be non-null if the bytecode was
  // activated at the time the channel got created in StartLoad.
  if (!aRequest->mCacheInfo) {
    LOG(("ScriptLoadRequest (%p): Cannot cache anything (cacheInfo = %p)",
         aRequest, aRequest->mCacheInfo.get()));
    return false;
  }

  // Look at the preference to know which strategy (parameters) should be used
  // when the bytecode cache is enabled.
  int32_t strategy = StaticPrefs::dom_script_loader_bytecode_cache_strategy();

  // List of parameters used by the strategies.
  bool hasSourceLengthMin = false;
  bool hasFetchCountMin = false;
  size_t sourceLengthMin = 100;
  int32_t fetchCountMin = 4;

  LOG(("ScriptLoadRequest (%p): Bytecode-cache: strategy = %d.", aRequest,
       strategy));
  switch (strategy) {
    case -2: {
      // Reader mode, keep requesting alternate data but no longer save it.
      LOG(("ScriptLoadRequest (%p): Bytecode-cache: Encoding disabled.",
           aRequest));
      return false;
    }
    case -1: {
      // Eager mode, skip heuristics!
      hasSourceLengthMin = false;
      hasFetchCountMin = false;
      break;
    }
    default:
    case 0: {
      hasSourceLengthMin = true;
      hasFetchCountMin = true;
      sourceLengthMin = 1024;
      // If we were to optimize only for speed, without considering the impact
      // on memory, we should set this threshold to 2. (Bug 900784 comment 120)
      fetchCountMin = 4;
      break;
    }
  }

  // If the script is too small/large, do not attempt at creating a bytecode
  // cache for this script, as the overhead of parsing it might not be worth the
  // effort.
  if (hasSourceLengthMin) {
    size_t sourceLength;
    size_t minLength;
    MOZ_ASSERT(aRequest->IsTextSource());
    sourceLength = aRequest->mScriptTextLength;
    minLength = sourceLengthMin;
    if (sourceLength < minLength) {
      LOG(("ScriptLoadRequest (%p): Bytecode-cache: Script is too small.",
           aRequest));
      return false;
    }
  }

  // Check that we loaded the cache entry a few times before attempting any
  // bytecode-cache optimization, such that we do not waste time on entry which
  // are going to be dropped soon.
  if (hasFetchCountMin) {
    int32_t fetchCount = 0;
    if (NS_FAILED(aRequest->mCacheInfo->GetCacheTokenFetchCount(&fetchCount))) {
      LOG(("ScriptLoadRequest (%p): Bytecode-cache: Cannot get fetchCount.",
           aRequest));
      return false;
    }
    LOG(("ScriptLoadRequest (%p): Bytecode-cache: fetchCount = %d.", aRequest,
         fetchCount));
    if (fetchCount < fetchCountMin) {
      return false;
    }
  }

  LOG(("ScriptLoadRequest (%p): Bytecode-cache: Trigger encoding.", aRequest));
  return true;
}

class MOZ_RAII AutoSetProcessingScriptTag {
  nsCOMPtr<nsIScriptContext> mContext;
  bool mOldTag;

 public:
  explicit AutoSetProcessingScriptTag(nsIScriptContext* aContext)
      : mContext(aContext), mOldTag(mContext->GetProcessingScriptTag()) {
    mContext->SetProcessingScriptTag(true);
  }

  ~AutoSetProcessingScriptTag() { mContext->SetProcessingScriptTag(mOldTag); }
};

static nsresult ExecuteCompiledScript(JSContext* aCx,
                                      ScriptLoadRequest* aRequest,
                                      JSExecutionContext& aExec,
                                      ClassicScript* aLoaderScript) {
  JS::Rooted<JSScript*> script(aCx, aExec.GetScript());
  if (!script) {
    // Compilation succeeds without producing a script if scripting is
    // disabled for the global.
    return NS_OK;
  }

  if (JS::GetScriptPrivate(script).isUndefined()) {
    aLoaderScript->AssociateWithScript(script);
  }

  return aExec.ExecScript();
}

nsresult ScriptLoader::EvaluateScript(ScriptLoadRequest* aRequest) {
  using namespace mozilla::Telemetry;
  MOZ_ASSERT(aRequest->IsReadyToRun());

  // We need a document to evaluate scripts.
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  bool isDynamicImport = aRequest->IsModuleRequest() &&
                         aRequest->AsModuleRequest()->IsDynamicImport();
  if (!isDynamicImport) {
    nsCOMPtr<nsIContent> scriptContent(
        do_QueryInterface(aRequest->GetScriptElement()));
    MOZ_ASSERT(scriptContent);
    Document* ownerDoc = scriptContent->OwnerDoc();
    if (ownerDoc != mDocument) {
      // Willful violation of HTML5 as of 2010-12-01
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIGlobalObject> globalObject;
  nsCOMPtr<nsIScriptContext> context;
  if (aRequest->GetWebExtGlobal()) {
    // Executing a module from a WebExtension content-script.
    globalObject = aRequest->GetWebExtGlobal();
  } else {
    // Otherwise we have to ensure that there is a nsIScriptContext.
    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal =
        GetScriptGlobalObject(WebExtGlobal::Handled);
    if (!scriptGlobal) {
      return NS_ERROR_FAILURE;
    }

    // Make sure context is a strong reference since we access it after
    // we've executed a script, which may cause all other references to
    // the context to go away.
    context = scriptGlobal->GetScriptContext();
    if (!context) {
      return NS_ERROR_FAILURE;
    }

    globalObject = scriptGlobal;
  }

#ifdef MOZ_GECKO_PROFILER
  nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow();
  nsIDocShell* docShell = window ? window->GetDocShell() : nullptr;
#endif
  nsAutoCString profilerLabelString;
  GetProfilerLabelForRequest(aRequest, profilerLabelString);

  // New script entry point required, due to the "Create a script" sub-step of
  // http://www.whatwg.org/specs/web-apps/current-work/#execute-the-script-block
  nsAutoMicroTask mt;
  AutoEntryScript aes(globalObject, profilerLabelString.get(), true);
  JSContext* cx = aes.cx();
  JS::Rooted<JSObject*> global(cx, globalObject->GetGlobalJSObject());

  Maybe<AutoSetProcessingScriptTag> setProcessingScriptTag;
  if (context) {
    setProcessingScriptTag.emplace(context);
  }

  nsresult rv;
  {
    if (aRequest->IsModuleRequest()) {
      // When a module is already loaded, it is not feched a second time and the
      // mDataType of the request might remain set to DataType::Unknown.
      MOZ_ASSERT(aRequest->IsTextSource() || aRequest->IsUnknownDataType());
      LOG(("ScriptLoadRequest (%p): Evaluate Module", aRequest));
      AUTO_PROFILER_MARKER_TEXT("ModuleEvaluation", JS,
                                MarkerInnerWindowIdFromDocShell(docShell),
                                profilerLabelString);

      // currentScript is set to null for modules.
      AutoCurrentScriptUpdater scriptUpdater(this, nullptr);

      ModuleLoadRequest* request = aRequest->AsModuleRequest();
      MOZ_ASSERT(request->mModuleScript);
      MOZ_ASSERT(!request->mOffThreadToken);

      ModuleScript* moduleScript = request->mModuleScript;
      if (moduleScript->HasErrorToRethrow()) {
        LOG(("ScriptLoadRequest (%p):   module has error to rethrow",
             aRequest));
        JS::Rooted<JS::Value> error(cx, moduleScript->ErrorToRethrow());
        JS_SetPendingException(cx, error);
        // For a dynamic import, the promise is rejected.  Otherwise an error
        // is either reported by AutoEntryScript.
        if (!JS::ContextOptionsRef(cx).topLevelAwait()) {
          if (request->IsDynamicImport()) {
            FinishDynamicImport_NoTLA(cx, request, NS_OK);
          }
        } else {
          if (request->IsDynamicImport()) {
            FinishDynamicImport(cx, request, NS_OK, nullptr);
          }
        }
        return NS_OK;
      }

      JS::Rooted<JSObject*> module(cx, moduleScript->ModuleRecord());
      MOZ_ASSERT(module);

      rv = InitDebuggerDataForModuleTree(cx, request);
      NS_ENSURE_SUCCESS(rv, rv);

      TRACE_FOR_TEST(aRequest->GetScriptElement(),
                     "scriptloader_evaluate_module");

      JS::Rooted<JS::Value> rval(cx);

      rv = nsJSUtils::ModuleEvaluate(cx, module, &rval);

      if (NS_SUCCEEDED(rv)) {
        // If we have an infinite loop in a module, which is stopped by the
        // user, the module evaluation will fail, but we will not have an
        // AutoEntryScript exception.
        MOZ_ASSERT(!aes.HasException());
      }

      if (NS_FAILED(rv)) {
        LOG(("ScriptLoadRequest (%p):   evaluation failed", aRequest));
        // For a dynamic import, the promise is rejected.  Otherwise an error is
        // either reported by AutoEntryScript.
        rv = NS_OK;
      }

      if (!JS::ContextOptionsRef(cx).topLevelAwait()) {
        if (request->IsDynamicImport()) {
          FinishDynamicImport_NoTLA(cx, request, rv);
        }
      } else {
        // Path for when Top Level Await is enabled
        JS::Rooted<JSObject*> aEvaluationPromise(cx);
        if (NS_SUCCEEDED(rv)) {
          // If the user cancels the evaluation on an infinite loop, we need
          // to skip this step. In that case, ModuleEvaluate will not return a
          // promise, rval will be undefined. We should treat it as a failed
          // evaluation, and reject appropriately.
          aEvaluationPromise.set(&rval.toObject());
        }
        if (request->IsDynamicImport()) {
          FinishDynamicImport(cx, request, rv, aEvaluationPromise);
        } else {
          // If this is not a dynamic import, and if the promise is rejected,
          // the value is unwrapped from the promise value.
          if (!JS::ThrowOnModuleEvaluationFailure(cx, aEvaluationPromise)) {
            LOG(("ScriptLoadRequest (%p):   evaluation failed on throw",
                 aRequest));
            // For a dynamic import, the promise is rejected.  Otherwise an
            // error is either reported by AutoEntryScript.
            rv = NS_OK;
          }
        }
      }

      TRACE_FOR_TEST_NONE(aRequest->GetScriptElement(),
                          "scriptloader_no_encode");
      aRequest->mCacheInfo = nullptr;
    } else {
      // Update our current script.
      AutoCurrentScriptUpdater scriptUpdater(this,
                                             aRequest->GetScriptElement());

      // Create a ClassicScript object and associate it with the JSScript.
      RefPtr<ClassicScript> classicScript =
          new ClassicScript(aRequest->mFetchOptions, aRequest->mBaseURL);

      JS::CompileOptions options(cx);
      rv = FillCompileOptionsForRequest(aes, aRequest, global, &options);
      options.setPrivateValue(JS::PrivateValue(classicScript));

      if (NS_SUCCEEDED(rv)) {
        if (aRequest->IsBytecode()) {
          TRACE_FOR_TEST(aRequest->GetScriptElement(), "scriptloader_execute");
          JSExecutionContext exec(cx, global);
          if (aRequest->mOffThreadToken) {
            LOG(("ScriptLoadRequest (%p): Decode Bytecode & Join and Execute",
                 aRequest));
            rv = exec.JoinDecode(&aRequest->mOffThreadToken);
          } else {
            LOG(("ScriptLoadRequest (%p): Decode Bytecode and Execute",
                 aRequest));
            AUTO_PROFILER_MARKER_TEXT("BytecodeDecodeMainThread", JS,
                                      MarkerInnerWindowIdFromDocShell(docShell),
                                      profilerLabelString);

            rv = exec.Decode(options, aRequest->mScriptBytecode,
                             aRequest->mBytecodeOffset);
          }

          if (rv == NS_OK) {
            AUTO_PROFILER_MARKER_TEXT("ScriptExecution", JS,
                                      MarkerInnerWindowIdFromDocShell(docShell),
                                      profilerLabelString);
            rv = ExecuteCompiledScript(cx, aRequest, exec, classicScript);
          }

          // We do not expect to be saving anything when we already have some
          // bytecode.
          MOZ_ASSERT(!aRequest->mCacheInfo);
        } else {
          MOZ_ASSERT(aRequest->IsSource());
          JS::Rooted<JSScript*> script(cx);
          bool encodeBytecode = ShouldCacheBytecode(aRequest);

          {
            JSExecutionContext exec(cx, global);
            exec.SetEncodeBytecode(encodeBytecode);
            TRACE_FOR_TEST(aRequest->GetScriptElement(),
                           "scriptloader_execute");
            if (aRequest->mOffThreadToken) {
              // Off-main-thread parsing.
              LOG(
                  ("ScriptLoadRequest (%p): Join (off-thread parsing) and "
                   "Execute",
                   aRequest));
              MOZ_ASSERT(aRequest->IsTextSource());
              rv = exec.JoinCompile(&aRequest->mOffThreadToken);
            } else {
              // Main thread parsing (inline and small scripts)
              LOG(("ScriptLoadRequest (%p): Compile And Exec", aRequest));
              MOZ_ASSERT(aRequest->IsTextSource());
              MaybeSourceText maybeSource;
              rv = GetScriptSource(cx, aRequest, &maybeSource);
              if (NS_SUCCEEDED(rv)) {
                AUTO_PROFILER_MARKER_TEXT(
                    "ScriptCompileMainThread", JS,
                    MarkerInnerWindowIdFromDocShell(docShell),
                    profilerLabelString);

                rv =
                    maybeSource.constructed<SourceText<char16_t>>()
                        ? exec.Compile(options,
                                       maybeSource.ref<SourceText<char16_t>>())
                        : exec.Compile(options,
                                       maybeSource.ref<SourceText<Utf8Unit>>());
              }
            }

            if (rv == NS_OK) {
              script = exec.GetScript();
              AUTO_PROFILER_MARKER_TEXT(
                  "ScriptExecution", JS,
                  MarkerInnerWindowIdFromDocShell(docShell),
                  profilerLabelString);
              rv = ExecuteCompiledScript(cx, aRequest, exec, classicScript);
            }
          }

          // Queue the current script load request to later save the bytecode.
          if (script && encodeBytecode) {
            aRequest->SetScript(script);
            TRACE_FOR_TEST(aRequest->GetScriptElement(), "scriptloader_encode");
            MOZ_ASSERT(aRequest->mBytecodeOffset ==
                       aRequest->mScriptBytecode.length());
            RegisterForBytecodeEncoding(aRequest);
          } else {
            LOG(
                ("ScriptLoadRequest (%p): Bytecode-cache: disabled (rv = %X, "
                 "script = %p)",
                 aRequest, unsigned(rv), script.get()));
            TRACE_FOR_TEST_NONE(aRequest->GetScriptElement(),
                                "scriptloader_no_encode");
            aRequest->mCacheInfo = nullptr;
          }
        }
      }
    }

    // Even if we are not saving the bytecode of the current script, we have
    // to trigger the encoding of the bytecode, as the current script can
    // call functions of a script for which we are recording the bytecode.
    LOG(("ScriptLoadRequest (%p): ScriptLoader = %p", aRequest, this));
    MaybeTriggerBytecodeEncoding();
  }

  return rv;
}

/* static */
LoadedScript* ScriptLoader::GetActiveScript(JSContext* aCx) {
  JS::Value value = JS::GetScriptedCallerPrivate(aCx);
  if (value.isUndefined()) {
    return nullptr;
  }

  return static_cast<LoadedScript*>(value.toPrivate());
}

void ScriptLoader::RegisterForBytecodeEncoding(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->mCacheInfo);
  MOZ_ASSERT(aRequest->mScript);
  MOZ_DIAGNOSTIC_ASSERT(!aRequest->isInList());
  mBytecodeEncodingQueue.AppendElement(aRequest);
}

void ScriptLoader::LoadEventFired() {
  mLoadEventFired = true;
  MaybeTriggerBytecodeEncoding();
}

void ScriptLoader::Destroy() {
  // Off thread compilations will be canceled in ProcessRequest after the inner
  // window is removed in Document::Destroy()
  if (mShutdownObserver) {
    mShutdownObserver->Unregister();
    mShutdownObserver = nullptr;
  }
  GiveUpBytecodeEncoding();
}

void ScriptLoader::MaybeTriggerBytecodeEncoding() {
  // If we already gave up, ensure that we are not going to enqueue any script,
  // and that we finalize them properly.
  if (mGiveUpEncoding) {
    LOG(("ScriptLoader (%p): Keep giving-up bytecode encoding.", this));
    GiveUpBytecodeEncoding();
    return;
  }

  // We wait for the load event to be fired before saving the bytecode of
  // any script to the cache. It is quite common to have load event
  // listeners trigger more JavaScript execution, that we want to save as
  // part of this start-up bytecode cache.
  if (!mLoadEventFired) {
    LOG(("ScriptLoader (%p): Wait for the load-end event to fire.", this));
    return;
  }

  // No need to fire any event if there is no bytecode to be saved.
  if (mBytecodeEncodingQueue.isEmpty()) {
    LOG(("ScriptLoader (%p): No script in queue to be encoded.", this));
    return;
  }

  // Wait until all scripts are loaded before saving the bytecode, such that
  // we capture most of the intialization of the page.
  if (HasPendingRequests()) {
    LOG(("ScriptLoader (%p): Wait for other pending request to finish.", this));
    return;
  }

  // Create a new runnable dedicated to encoding the content of the bytecode of
  // all enqueued scripts when the document is idle. In case of failure, we
  // give-up on encoding the bytecode.
  nsCOMPtr<nsIRunnable> encoder = NewRunnableMethod(
      "ScriptLoader::EncodeBytecode", this, &ScriptLoader::EncodeBytecode);
  if (NS_FAILED(NS_DispatchToCurrentThreadQueue(encoder.forget(),
                                                EventQueuePriority::Idle))) {
    GiveUpBytecodeEncoding();
    return;
  }

  LOG(("ScriptLoader (%p): Schedule bytecode encoding.", this));
}

void ScriptLoader::EncodeBytecode() {
  LOG(("ScriptLoader (%p): Start bytecode encoding.", this));

  // If any script got added in the previous loop cycle, wait until all
  // remaining script executions are completed, such that we capture most of
  // the initialization.
  if (HasPendingRequests()) {
    return;
  }

  // Should not be encoding modules at all.
  nsCOMPtr<nsIScriptGlobalObject> globalObject =
      GetScriptGlobalObject(WebExtGlobal::Ignore);
  if (!globalObject) {
    GiveUpBytecodeEncoding();
    return;
  }

  nsCOMPtr<nsIScriptContext> context = globalObject->GetScriptContext();
  if (!context) {
    GiveUpBytecodeEncoding();
    return;
  }

  TimeStamp startTime = TimeStamp::Now();

  AutoEntryScript aes(globalObject, "encode bytecode", true);
  RefPtr<ScriptLoadRequest> request;
  while (!mBytecodeEncodingQueue.isEmpty()) {
    request = mBytecodeEncodingQueue.StealFirst();
    MOZ_ASSERT(!request->IsModuleRequest());
    MOZ_ASSERT(!request->GetWebExtGlobal(), "Not handling global above");
    EncodeRequestBytecode(aes.cx(), request);
    request->mScriptBytecode.clearAndFree();
    request->DropBytecodeCacheReferences();
  }

  TimeDuration delta = TimeStamp::Now() - startTime;
  Telemetry::Accumulate(Telemetry::JS_BYTECODE_CACHING_TIME,
                        static_cast<uint32_t>(delta.ToMilliseconds()));
}

void ScriptLoader::EncodeRequestBytecode(JSContext* aCx,
                                         ScriptLoadRequest* aRequest) {
  using namespace mozilla::Telemetry;
  nsresult rv = NS_OK;
  MOZ_ASSERT(aRequest->mCacheInfo);
  auto bytecodeFailed = mozilla::MakeScopeExit([&]() {
    TRACE_FOR_TEST_NONE(aRequest->GetScriptElement(),
                        "scriptloader_bytecode_failed");
  });

  JS::RootedScript script(aCx, aRequest->mScript);
  if (!JS::FinishIncrementalEncoding(aCx, script, aRequest->mScriptBytecode)) {
    // Encoding can be aborted for non-supported syntax (e.g. asm.js), or
    // any other internal error.
    // We don't care the error and just give up encoding.
    JS_ClearPendingException(aCx);

    LOG(("ScriptLoadRequest (%p): Cannot serialize bytecode", aRequest));
    return;
  }

  if (aRequest->mScriptBytecode.length() >= UINT32_MAX) {
    LOG(
        ("ScriptLoadRequest (%p): Bytecode cache is too large to be decoded "
         "correctly.",
         aRequest));
    return;
  }

  // Open the output stream to the cache entry alternate data storage. This
  // might fail if the stream is already open by another request, in which
  // case, we just ignore the current one.
  nsCOMPtr<nsIAsyncOutputStream> output;
  rv = aRequest->mCacheInfo->OpenAlternativeOutputStream(
      nsContentUtils::JSBytecodeMimeType(), aRequest->mScriptBytecode.length(),
      getter_AddRefs(output));
  if (NS_FAILED(rv)) {
    LOG(
        ("ScriptLoadRequest (%p): Cannot open bytecode cache (rv = %X, output "
         "= %p)",
         aRequest, unsigned(rv), output.get()));
    return;
  }
  MOZ_ASSERT(output);

  auto closeOutStream = mozilla::MakeScopeExit([&]() {
    rv = output->CloseWithStatus(rv);
    LOG(("ScriptLoadRequest (%p): Closing (rv = %X)", aRequest, unsigned(rv)));
  });

  uint32_t n;
  rv = output->Write(reinterpret_cast<char*>(aRequest->mScriptBytecode.begin()),
                     aRequest->mScriptBytecode.length(), &n);
  LOG((
      "ScriptLoadRequest (%p): Write bytecode cache (rv = %X, length = %u, "
      "written = %u)",
      aRequest, unsigned(rv), unsigned(aRequest->mScriptBytecode.length()), n));
  if (NS_FAILED(rv)) {
    return;
  }

  MOZ_RELEASE_ASSERT(aRequest->mScriptBytecode.length() == n);

  bytecodeFailed.release();
  TRACE_FOR_TEST_NONE(aRequest->GetScriptElement(),
                      "scriptloader_bytecode_saved");
}

void ScriptLoader::GiveUpBytecodeEncoding() {
  // If the document went away prematurely, we still want to set this, in order
  // to avoid queuing more scripts.
  mGiveUpEncoding = true;

  // Ideally we prefer to properly end the incremental encoder, such that we
  // would not keep a large buffer around.  If we cannot, we fallback on the
  // removal of all request from the current list and these large buffers would
  // be removed at the same time as the source object.
  nsCOMPtr<nsIScriptGlobalObject> globalObject =
      GetScriptGlobalObject(WebExtGlobal::Ignore);
  AutoAllowLegacyScriptExecution exemption;
  Maybe<AutoEntryScript> aes;

  if (globalObject) {
    nsCOMPtr<nsIScriptContext> context = globalObject->GetScriptContext();
    if (context) {
      aes.emplace(globalObject, "give-up bytecode encoding", true);
    }
  }

  while (!mBytecodeEncodingQueue.isEmpty()) {
    RefPtr<ScriptLoadRequest> request = mBytecodeEncodingQueue.StealFirst();
    LOG(("ScriptLoadRequest (%p): Cannot serialize bytecode", request.get()));
    TRACE_FOR_TEST_NONE(request->GetScriptElement(),
                        "scriptloader_bytecode_failed");
    MOZ_ASSERT(!request->IsModuleRequest());
    MOZ_ASSERT(!request->GetWebExtGlobal());

    if (aes.isSome()) {
      JS::RootedScript script(aes->cx(), request->mScript);
      if (!JS::FinishIncrementalEncoding(aes->cx(), script,
                                         request->mScriptBytecode)) {
        JS_ClearPendingException(aes->cx());
      }
    }

    request->mScriptBytecode.clearAndFree();
    request->DropBytecodeCacheReferences();
  }
}

bool ScriptLoader::HasPendingRequests() {
  return mParserBlockingRequest || !mXSLTRequests.isEmpty() ||
         !mLoadedAsyncRequests.isEmpty() ||
         !mNonAsyncExternalScriptInsertedRequests.isEmpty() ||
         !mDeferRequests.isEmpty() || !mDynamicImportRequests.isEmpty() ||
         !mPendingChildLoaders.IsEmpty();
}

void ScriptLoader::ProcessPendingRequestsAsync() {
  if (HasPendingRequests()) {
    nsCOMPtr<nsIRunnable> task =
        NewRunnableMethod("dom::ScriptLoader::ProcessPendingRequests", this,
                          &ScriptLoader::ProcessPendingRequests);
    if (mDocument) {
      mDocument->Dispatch(TaskCategory::Other, task.forget());
    } else {
      NS_DispatchToCurrentThread(task.forget());
    }
  }
}

void ScriptLoader::ProcessPendingRequests() {
  RefPtr<ScriptLoadRequest> request;

  if (mParserBlockingRequest && mParserBlockingRequest->IsReadyToRun() &&
      ReadyToExecuteParserBlockingScripts()) {
    request.swap(mParserBlockingRequest);
    UnblockParser(request);
    ProcessRequest(request);
    ContinueParserAsync(request);
  }

  while (ReadyToExecuteParserBlockingScripts() && !mXSLTRequests.isEmpty() &&
         mXSLTRequests.getFirst()->IsReadyToRun()) {
    request = mXSLTRequests.StealFirst();
    ProcessRequest(request);
  }

  while (ReadyToExecuteScripts() && !mLoadedAsyncRequests.isEmpty()) {
    request = mLoadedAsyncRequests.StealFirst();
    if (request->IsModuleRequest()) {
      ProcessRequest(request);
    } else {
      CompileOffThreadOrProcessRequest(request);
    }
  }

  while (ReadyToExecuteScripts() &&
         !mNonAsyncExternalScriptInsertedRequests.isEmpty() &&
         mNonAsyncExternalScriptInsertedRequests.getFirst()->IsReadyToRun()) {
    // Violate the HTML5 spec and execute these in the insertion order in
    // order to make LABjs and the "order" plug-in for RequireJS work with
    // their Gecko-sniffed code path. See
    // http://lists.w3.org/Archives/Public/public-html/2010Oct/0088.html
    request = mNonAsyncExternalScriptInsertedRequests.StealFirst();
    ProcessRequest(request);
  }

  if (mDeferCheckpointReached && mXSLTRequests.isEmpty()) {
    while (ReadyToExecuteScripts() && !mDeferRequests.isEmpty() &&
           mDeferRequests.getFirst()->IsReadyToRun()) {
      request = mDeferRequests.StealFirst();
      ProcessRequest(request);
    }
  }

  while (!mPendingChildLoaders.IsEmpty() &&
         ReadyToExecuteParserBlockingScripts()) {
    RefPtr<ScriptLoader> child = mPendingChildLoaders[0];
    mPendingChildLoaders.RemoveElementAt(0);
    child->RemoveParserBlockingScriptExecutionBlocker();
  }

  if (mDeferCheckpointReached && mDocument && !mParserBlockingRequest &&
      mNonAsyncExternalScriptInsertedRequests.isEmpty() &&
      mXSLTRequests.isEmpty() && mDeferRequests.isEmpty() &&
      MaybeRemovedDeferRequests()) {
    return ProcessPendingRequests();
  }

  if (mDeferCheckpointReached && mDocument && !mParserBlockingRequest &&
      mLoadingAsyncRequests.isEmpty() && mLoadedAsyncRequests.isEmpty() &&
      mNonAsyncExternalScriptInsertedRequests.isEmpty() &&
      mXSLTRequests.isEmpty() && mDeferRequests.isEmpty()) {
    // No more pending scripts; time to unblock onload.
    // OK to unblock onload synchronously here, since callers must be
    // prepared for the world changing anyway.
    mDeferCheckpointReached = false;
    mDocument->UnblockOnload(true);
  }
}

bool ScriptLoader::ReadyToExecuteParserBlockingScripts() {
  // Make sure the SelfReadyToExecuteParserBlockingScripts check is first, so
  // that we don't block twice on an ancestor.
  if (!SelfReadyToExecuteParserBlockingScripts()) {
    return false;
  }

  if (mDocument && mDocument->GetWindowContext()) {
    for (WindowContext* wc =
             mDocument->GetWindowContext()->GetParentWindowContext();
         wc; wc = wc->GetParentWindowContext()) {
      if (Document* doc = wc->GetDocument()) {
        ScriptLoader* ancestor = doc->ScriptLoader();
        if (!ancestor->SelfReadyToExecuteParserBlockingScripts() &&
            ancestor->AddPendingChildLoader(this)) {
          AddParserBlockingScriptExecutionBlocker();
          return false;
        }
      }
    }
  }

  return true;
}

template <typename Unit>
static nsresult ConvertToUnicode(nsIChannel* aChannel, const uint8_t* aData,
                                 uint32_t aLength,
                                 const nsAString& aHintCharset,
                                 Document* aDocument, Unit*& aBufOut,
                                 size_t& aLengthOut) {
  if (!aLength) {
    aBufOut = nullptr;
    aLengthOut = 0;
    return NS_OK;
  }

  auto data = Span(aData, aLength);

  // The encoding info precedence is as follows from high to low:
  // The BOM
  // HTTP Content-Type (if name recognized)
  // charset attribute (if name recognized)
  // The encoding of the document

  UniquePtr<Decoder> unicodeDecoder;

  const Encoding* encoding;
  size_t bomLength;
  Tie(encoding, bomLength) = Encoding::ForBOM(data);
  if (encoding) {
    unicodeDecoder = encoding->NewDecoderWithBOMRemoval();
  }

  if (!unicodeDecoder && aChannel) {
    nsAutoCString label;
    if (NS_SUCCEEDED(aChannel->GetContentCharset(label)) &&
        (encoding = Encoding::ForLabel(label))) {
      unicodeDecoder = encoding->NewDecoderWithoutBOMHandling();
    }
  }

  if (!unicodeDecoder && (encoding = Encoding::ForLabel(aHintCharset))) {
    unicodeDecoder = encoding->NewDecoderWithoutBOMHandling();
  }

  if (!unicodeDecoder && aDocument) {
    unicodeDecoder =
        aDocument->GetDocumentCharacterSet()->NewDecoderWithoutBOMHandling();
  }

  if (!unicodeDecoder) {
    // Curiously, there are various callers that don't pass aDocument. The
    // fallback in the old code was ISO-8859-1, which behaved like
    // windows-1252.
    unicodeDecoder = WINDOWS_1252_ENCODING->NewDecoderWithoutBOMHandling();
  }

  auto signalOOM = mozilla::MakeScopeExit([&aBufOut, &aLengthOut]() {
    aBufOut = nullptr;
    aLengthOut = 0;
  });

  CheckedInt<size_t> bufferLength =
      ScriptDecoding<Unit>::MaxBufferLength(unicodeDecoder, aLength);
  if (!bufferLength.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CheckedInt<size_t> bufferByteSize = bufferLength * sizeof(Unit);
  if (!bufferByteSize.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  aBufOut = static_cast<Unit*>(js_malloc(bufferByteSize.value()));
  if (!aBufOut) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  signalOOM.release();
  aLengthOut = ScriptDecoding<Unit>::DecodeInto(
      unicodeDecoder, data, Span(aBufOut, bufferLength.value()),
      /* aEndOfSource = */ true);
  return NS_OK;
}

/* static */
nsresult ScriptLoader::ConvertToUTF16(nsIChannel* aChannel,
                                      const uint8_t* aData, uint32_t aLength,
                                      const nsAString& aHintCharset,
                                      Document* aDocument, char16_t*& aBufOut,
                                      size_t& aLengthOut) {
  return ConvertToUnicode(aChannel, aData, aLength, aHintCharset, aDocument,
                          aBufOut, aLengthOut);
}

/* static */
nsresult ScriptLoader::ConvertToUTF8(nsIChannel* aChannel, const uint8_t* aData,
                                     uint32_t aLength,
                                     const nsAString& aHintCharset,
                                     Document* aDocument, Utf8Unit*& aBufOut,
                                     size_t& aLengthOut) {
  return ConvertToUnicode(aChannel, aData, aLength, aHintCharset, aDocument,
                          aBufOut, aLengthOut);
}

nsresult ScriptLoader::OnStreamComplete(
    nsIIncrementalStreamLoader* aLoader, ScriptLoadRequest* aRequest,
    nsresult aChannelStatus, nsresult aSRIStatus,
    SRICheckDataVerifier* aSRIDataVerifier) {
  NS_ASSERTION(aRequest, "null request in stream complete handler");
  NS_ENSURE_TRUE(aRequest, NS_ERROR_FAILURE);

  nsresult rv = VerifySRI(aRequest, aLoader, aSRIStatus, aSRIDataVerifier);

  if (NS_SUCCEEDED(rv)) {
    // If we are loading from source, save the computed SRI hash or a dummy SRI
    // hash in case we are going to save the bytecode of this script in the
    // cache.
    if (aRequest->IsSource()) {
      uint32_t sriLength = 0;
      rv = SaveSRIHash(aRequest, aSRIDataVerifier, &sriLength);
      MOZ_ASSERT_IF(NS_SUCCEEDED(rv),
                    aRequest->mScriptBytecode.length() == sriLength);

      aRequest->mBytecodeOffset = JS::AlignTranscodingBytecodeOffset(sriLength);
      if (aRequest->mBytecodeOffset != sriLength) {
        // We need extra padding after SRI hash.
        if (!aRequest->mScriptBytecode.resize(aRequest->mBytecodeOffset)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }

    if (NS_SUCCEEDED(rv)) {
      rv = PrepareLoadedRequest(aRequest, aLoader, aChannelStatus);
    }

    if (NS_FAILED(rv)) {
      ReportErrorToConsole(aRequest, rv);
    }
  }

  if (NS_FAILED(rv)) {
    // When loading bytecode, we verify the SRI hash. If it does not match the
    // one from the document we restart the load, forcing us to load the source
    // instead. If this happens do not remove the current request from script
    // loader's data structures or fire any events.
    if (aChannelStatus != NS_BINDING_RETARGETED) {
      HandleLoadError(aRequest, rv);
    }
  }

  // Process our request and/or any pending ones
  ProcessPendingRequests();

  return rv;
}

nsresult ScriptLoader::VerifySRI(ScriptLoadRequest* aRequest,
                                 nsIIncrementalStreamLoader* aLoader,
                                 nsresult aSRIStatus,
                                 SRICheckDataVerifier* aSRIDataVerifier) const {
  nsCOMPtr<nsIRequest> channelRequest;
  aLoader->GetRequest(getter_AddRefs(channelRequest));
  nsCOMPtr<nsIChannel> channel;
  channel = do_QueryInterface(channelRequest);

  nsresult rv = NS_OK;
  if (!aRequest->mIntegrity.IsEmpty() && NS_SUCCEEDED((rv = aSRIStatus))) {
    MOZ_ASSERT(aSRIDataVerifier);
    MOZ_ASSERT(mReporter);

    nsAutoCString sourceUri;
    if (mDocument && mDocument->GetDocumentURI()) {
      mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
    }
    rv = aSRIDataVerifier->Verify(aRequest->mIntegrity, channel, sourceUri,
                                  mReporter);
    if (channelRequest) {
      mReporter->FlushReportsToConsole(
          nsContentUtils::GetInnerWindowID(channelRequest));
    } else {
      mReporter->FlushConsoleReports(mDocument);
    }
    if (NS_FAILED(rv)) {
      rv = NS_ERROR_SRI_CORRUPT;
    }
  }

  return rv;
}

nsresult ScriptLoader::SaveSRIHash(ScriptLoadRequest* aRequest,
                                   SRICheckDataVerifier* aSRIDataVerifier,
                                   uint32_t* sriLength) const {
  MOZ_ASSERT(aRequest->IsSource());
  MOZ_ASSERT(aRequest->mScriptBytecode.empty());

  uint32_t len;

  // If the integrity metadata does not correspond to a valid hash function,
  // IsComplete would be false.
  if (!aRequest->mIntegrity.IsEmpty() && aSRIDataVerifier->IsComplete()) {
    MOZ_ASSERT(aRequest->mScriptBytecode.length() == 0);

    // Encode the SRI computed hash.
    len = aSRIDataVerifier->DataSummaryLength();

    if (!aRequest->mScriptBytecode.resize(len)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    DebugOnly<nsresult> res = aSRIDataVerifier->ExportDataSummary(
        len, aRequest->mScriptBytecode.begin());
    MOZ_ASSERT(NS_SUCCEEDED(res));
  } else {
    MOZ_ASSERT(aRequest->mScriptBytecode.length() == 0);

    // Encode a dummy SRI hash.
    len = SRICheckDataVerifier::EmptyDataSummaryLength();

    if (!aRequest->mScriptBytecode.resize(len)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    DebugOnly<nsresult> res = SRICheckDataVerifier::ExportEmptyDataSummary(
        len, aRequest->mScriptBytecode.begin());
    MOZ_ASSERT(NS_SUCCEEDED(res));
  }

  // Verify that the exported and predicted length correspond.
  mozilla::DebugOnly<uint32_t> srilen;
  MOZ_ASSERT(NS_SUCCEEDED(SRICheckDataVerifier::DataSummaryLength(
      len, aRequest->mScriptBytecode.begin(), &srilen)));
  MOZ_ASSERT(srilen == len);

  *sriLength = len;

  return NS_OK;
}

void ScriptLoader::ReportErrorToConsole(ScriptLoadRequest* aRequest,
                                        nsresult aResult) const {
  MOZ_ASSERT(aRequest);

  if (aRequest->IsPreload()) {
    // Skip reporting errors in preload requests. If the request is actually
    // used then we will report the error in ReportPreloadErrorsToConsole below.
    aRequest->mUnreportedPreloadError = aResult;
    return;
  }

  bool isScript = !aRequest->IsModuleRequest();
  const char* message;
  if (aResult == NS_ERROR_MALFORMED_URI) {
    message = isScript ? "ScriptSourceMalformed" : "ModuleSourceMalformed";
  } else if (aResult == NS_ERROR_DOM_BAD_URI) {
    message = isScript ? "ScriptSourceNotAllowed" : "ModuleSourceNotAllowed";
  } else if (aResult == NS_ERROR_DOM_WEBEXT_CONTENT_SCRIPT_URI) {
    MOZ_ASSERT(!isScript);
    message = "WebExtContentScriptModuleSourceNotAllowed";
  } else if (net::UrlClassifierFeatureFactory::IsClassifierBlockingErrorCode(
                 aResult)) {
    // Blocking classifier error codes already show their own console messages.
    return;
  } else {
    message = isScript ? "ScriptSourceLoadFailed" : "ModuleSourceLoadFailed";
  }

  AutoTArray<nsString, 1> params;
  CopyUTF8toUTF16(aRequest->mURI->GetSpecOrDefault(), *params.AppendElement());

  nsIScriptElement* element = aRequest->GetScriptElement();
  uint32_t lineNo = element ? element->GetScriptLineNumber() : 0;
  uint32_t columnNo = element ? element->GetScriptColumnNumber() : 0;

  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  "Script Loader"_ns, mDocument,
                                  nsContentUtils::eDOM_PROPERTIES, message,
                                  params, nullptr, u""_ns, lineNo, columnNo);
}

void ScriptLoader::ReportPreloadErrorsToConsole(ScriptLoadRequest* aRequest) {
  if (NS_FAILED(aRequest->mUnreportedPreloadError)) {
    ReportErrorToConsole(aRequest, aRequest->mUnreportedPreloadError);
    aRequest->mUnreportedPreloadError = NS_OK;
  }

  if (aRequest->IsModuleRequest()) {
    for (auto childRequest : aRequest->AsModuleRequest()->mImports) {
      ReportPreloadErrorsToConsole(childRequest);
    }
  }
}

void ScriptLoader::HandleLoadError(ScriptLoadRequest* aRequest,
                                   nsresult aResult) {
  /*
   * Handle script not loading error because source was an tracking URL (or
   * fingerprinting, cryptomining, etc).
   * We make a note of this script node by including it in a dedicated
   * array of blocked tracking nodes under its parent document.
   */
  if (net::UrlClassifierFeatureFactory::IsClassifierBlockingErrorCode(
          aResult)) {
    nsCOMPtr<nsIContent> cont = do_QueryInterface(aRequest->GetScriptElement());
    mDocument->AddBlockedNodeByClassifier(cont);
  }

  if (aRequest->IsModuleRequest() && !aRequest->mIsInline) {
    auto request = aRequest->AsModuleRequest();
    SetModuleFetchFinishedAndResumeWaitingRequests(request, aResult);
  }

  if (aRequest->mInDeferList) {
    MOZ_ASSERT_IF(aRequest->IsModuleRequest(),
                  aRequest->AsModuleRequest()->IsTopLevel());
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mDeferRequests.Steal(aRequest);
      FireScriptAvailable(aResult, req);
    }
  } else if (aRequest->mInAsyncList) {
    MOZ_ASSERT_IF(aRequest->IsModuleRequest(),
                  aRequest->AsModuleRequest()->IsTopLevel());
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mLoadingAsyncRequests.Steal(aRequest);
      FireScriptAvailable(aResult, req);
    }
  } else if (aRequest->mIsNonAsyncScriptInserted) {
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req =
          mNonAsyncExternalScriptInsertedRequests.Steal(aRequest);
      FireScriptAvailable(aResult, req);
    }
  } else if (aRequest->mIsXSLT) {
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mXSLTRequests.Steal(aRequest);
      FireScriptAvailable(aResult, req);
    }
  } else if (aRequest->IsPreload()) {
    if (aRequest->IsModuleRequest()) {
      aRequest->Cancel();
    }
    if (aRequest->IsTopLevel()) {
      MOZ_ALWAYS_TRUE(
          mPreloads.RemoveElement(aRequest, PreloadRequestComparator()));
    }
    MOZ_ASSERT(!aRequest->isInList());
    AccumulateCategorical(LABELS_DOM_SCRIPT_PRELOAD_RESULT::LoadError);
  } else if (aRequest->IsModuleRequest()) {
    ModuleLoadRequest* modReq = aRequest->AsModuleRequest();
    if (modReq->IsDynamicImport()) {
      MOZ_ASSERT(modReq->IsTopLevel());
      if (aRequest->isInList()) {
        RefPtr<ScriptLoadRequest> req = mDynamicImportRequests.Steal(aRequest);
        modReq->Cancel();
        // FinishDynamicImport must happen exactly once for each dynamic import
        // request. If the load is aborted we do it when we remove the request
        // from mDynamicImportRequests.
        FinishDynamicImportAndReject(modReq, aResult);
      }
    } else {
      MOZ_ASSERT(!modReq->IsTopLevel());
      MOZ_ASSERT(!modReq->isInList());
      modReq->Cancel();
      // The error is handled for the top level module.
    }
  } else if (mParserBlockingRequest == aRequest) {
    MOZ_ASSERT(!aRequest->isInList());
    mParserBlockingRequest = nullptr;
    UnblockParser(aRequest);

    // Ensure that we treat aRequest->GetScriptElement() as our current
    // parser-inserted script while firing onerror on it.
    MOZ_ASSERT(aRequest->GetScriptElement()->GetParserCreated());
    nsCOMPtr<nsIScriptElement> oldParserInsertedScript =
        mCurrentParserInsertedScript;
    mCurrentParserInsertedScript = aRequest->GetScriptElement();
    FireScriptAvailable(aResult, aRequest);
    ContinueParserAsync(aRequest);
    mCurrentParserInsertedScript = oldParserInsertedScript;
  } else {
    // This happens for blocking requests cancelled by ParsingComplete().
    // Ignore cancellation status for link-preload requests, as cancellation can
    // be omitted for them when SRI is stronger on consumer tags.
    MOZ_ASSERT(aRequest->IsCanceled() || aRequest->IsLinkPreloadScript());
    MOZ_ASSERT(!aRequest->isInList());
  }
}

void ScriptLoader::UnblockParser(ScriptLoadRequest* aParserBlockingRequest) {
  aParserBlockingRequest->GetScriptElement()->UnblockParser();
}

void ScriptLoader::ContinueParserAsync(
    ScriptLoadRequest* aParserBlockingRequest) {
  aParserBlockingRequest->GetScriptElement()->ContinueParserAsync();
}

uint32_t ScriptLoader::NumberOfProcessors() {
  if (mNumberOfProcessors > 0) return mNumberOfProcessors;

  int32_t numProcs = PR_GetNumberOfProcessors();
  if (numProcs > 0) mNumberOfProcessors = numProcs;
  return mNumberOfProcessors;
}

static bool IsInternalURIScheme(nsIURI* uri) {
  return uri->SchemeIs("moz-extension") || uri->SchemeIs("resource") ||
         uri->SchemeIs("chrome");
}

bool ScriptLoader::ShouldCompileOffThread(ScriptLoadRequest* aRequest) {
  if (NumberOfProcessors() <= 1) {
    return false;
  }
  if (aRequest == mParserBlockingRequest) {
    return true;
  }
  if (SpeculativeOMTParsingEnabled()) {
    // Processing non async inserted scripts too early can potentially delay the
    // load event from firing so focus on other scripts instead.
    if (aRequest->mIsNonAsyncScriptInserted &&
        !StaticPrefs::
            dom_script_loader_external_scripts_speculate_non_parser_inserted_enabled()) {
      return false;
    }

    // Async and link preload scripts do not need to be parsed right away.
    if (aRequest->IsAsyncScript() &&
        !StaticPrefs::
            dom_script_loader_external_scripts_speculate_async_enabled()) {
      return false;
    }

    if (aRequest->IsLinkPreloadScript() &&
        !StaticPrefs::
            dom_script_loader_external_scripts_speculate_link_preload_enabled()) {
      return false;
    }

    return true;
  }
  return false;
}

nsresult ScriptLoader::PrepareLoadedRequest(ScriptLoadRequest* aRequest,
                                            nsIIncrementalStreamLoader* aLoader,
                                            nsresult aStatus) {
  if (NS_FAILED(aStatus)) {
    return aStatus;
  }

  if (aRequest->IsCanceled()) {
    return NS_BINDING_ABORTED;
  }
  MOZ_ASSERT(aRequest->IsLoading());
  CollectScriptTelemetry(aRequest);

  // If we don't have a document, then we need to abort further
  // evaluation.
  if (!mDocument) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If the load returned an error page, then we need to abort
  nsCOMPtr<nsIRequest> req;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(req));
  NS_ASSERTION(req, "StreamLoader's request went away prematurely");
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(req);
  if (httpChannel) {
    bool requestSucceeded;
    rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
    if (NS_SUCCEEDED(rv) && !requestSucceeded) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsAutoCString sourceMapURL;
    if (nsContentUtils::GetSourceMapURL(httpChannel, sourceMapURL)) {
      aRequest->mSourceMapURL = Some(NS_ConvertUTF8toUTF16(sourceMapURL));
    }

    nsCOMPtr<nsIClassifiedChannel> classifiedChannel = do_QueryInterface(req);
    MOZ_ASSERT(classifiedChannel);
    if (classifiedChannel &&
        classifiedChannel->IsThirdPartyTrackingResource()) {
      aRequest->SetIsTracking();
    }
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(req);
  // If this load was subject to a CORS check, don't flag it with a separate
  // origin principal, so that it will treat our document's principal as the
  // origin principal.  Module loads always use CORS.
  if (!aRequest->IsModuleRequest() && aRequest->CORSMode() == CORS_NONE) {
    rv = nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
        channel, getter_AddRefs(aRequest->mOriginPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // This assertion could fire errorously if we ran out of memory when
  // inserting the request in the array. However it's an unlikely case
  // so if you see this assertion it is likely something else that is
  // wrong, especially if you see it more than once.
  NS_ASSERTION(mDeferRequests.Contains(aRequest) ||
                   mLoadingAsyncRequests.Contains(aRequest) ||
                   mNonAsyncExternalScriptInsertedRequests.Contains(aRequest) ||
                   mXSLTRequests.Contains(aRequest) ||
                   mDynamicImportRequests.Contains(aRequest) ||
                   (aRequest->IsModuleRequest() &&
                    !aRequest->AsModuleRequest()->IsTopLevel() &&
                    !aRequest->isInList()) ||
                   mPreloads.Contains(aRequest, PreloadRequestComparator()) ||
                   mParserBlockingRequest == aRequest,
               "aRequest should be pending!");

  nsCOMPtr<nsIURI> uri;
  rv = channel->GetOriginalURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  // Fixup moz-extension: and resource: URIs, because the channel URI will
  // point to file:, which won't be allowed to load.
  if (uri && IsInternalURIScheme(uri)) {
    aRequest->mBaseURL = uri;
  } else {
    channel->GetURI(getter_AddRefs(aRequest->mBaseURL));
  }

  if (aRequest->IsModuleRequest()) {
    MOZ_ASSERT(aRequest->IsSource());
    ModuleLoadRequest* request = aRequest->AsModuleRequest();

    // When loading a module, only responses with a JavaScript MIME type are
    // acceptable.
    nsAutoCString mimeType;
    channel->GetContentType(mimeType);
    NS_ConvertUTF8toUTF16 typeString(mimeType);
    if (!nsContentUtils::IsJavascriptMIMEType(typeString)) {
      return NS_ERROR_FAILURE;
    }

    // Attempt to compile off main thread.
    bool couldCompile = false;
    rv = AttemptAsyncScriptCompile(request, &couldCompile);
    NS_ENSURE_SUCCESS(rv, rv);
    if (couldCompile) {
      return NS_OK;
    }

    // Otherwise compile it right away and start fetching descendents.
    return ProcessFetchedModuleSource(request);
  }

  // The script is now loaded and ready to run.
  aRequest->SetReady();

  // If speculative parsing is enabled attempt to compile all
  // external scripts off-main-thread.  Otherwise, only omt compile scripts
  // blocking the parser.
  if (ShouldCompileOffThread(aRequest)) {
    MOZ_ASSERT(!aRequest->IsModuleRequest());
    bool couldCompile = false;
    nsresult rv = AttemptAsyncScriptCompile(aRequest, &couldCompile);
    NS_ENSURE_SUCCESS(rv, rv);
    if (couldCompile) {
      MOZ_ASSERT(aRequest->mProgress == ScriptLoadRequest::Progress::eCompiling,
                 "Request should be off-thread compiling now.");
      return NS_OK;
    }

    // If off-thread compile was rejected, continue with regular processing.
  }

  MaybeMoveToLoadedList(aRequest);

  return NS_OK;
}

void ScriptLoader::DeferCheckpointReached() {
  if (mDeferEnabled) {
    // Have to check because we apparently get ParsingComplete
    // without BeginDeferringScripts in some cases
    mDeferCheckpointReached = true;
  }

  mDeferEnabled = false;
  ProcessPendingRequests();
}

void ScriptLoader::ParsingComplete(bool aTerminated) {
  if (!aTerminated) {
    return;
  }
  mDeferRequests.Clear();
  mLoadingAsyncRequests.Clear();
  mLoadedAsyncRequests.Clear();
  mNonAsyncExternalScriptInsertedRequests.Clear();
  mXSLTRequests.Clear();

  for (ScriptLoadRequest* req = mDynamicImportRequests.getFirst(); req;
       req = req->getNext()) {
    req->Cancel();
    // FinishDynamicImport must happen exactly once for each dynamic import
    // request. If the load is aborted we do it when we remove the request
    // from mDynamicImportRequests.
    FinishDynamicImportAndReject(req->AsModuleRequest(), NS_ERROR_ABORT);
  }
  mDynamicImportRequests.Clear();

  if (mParserBlockingRequest) {
    mParserBlockingRequest->Cancel();
    mParserBlockingRequest = nullptr;
  }

  // Cancel any unused scripts that were compiled speculatively
  for (size_t i = 0; i < mPreloads.Length(); i++) {
    mPreloads[i].mRequest->MaybeCancelOffThreadScript();
  }

  // Have to call this even if aTerminated so we'll correctly unblock
  // onload and all.
  DeferCheckpointReached();
}

void ScriptLoader::PreloadURI(nsIURI* aURI, const nsAString& aCharset,
                              const nsAString& aType,
                              const nsAString& aCrossOrigin,
                              const nsAString& aIntegrity, bool aScriptFromHead,
                              bool aAsync, bool aDefer, bool aNoModule,
                              bool aLinkPreload,
                              const ReferrerPolicy aReferrerPolicy) {
  NS_ENSURE_TRUE_VOID(mDocument);
  // Check to see if scripts has been turned off.
  if (!mEnabled || !mDocument->IsScriptEnabled()) {
    return;
  }

  ScriptKind scriptKind = ScriptKind::eClassic;

  if (mDocument->ModuleScriptsEnabled()) {
    // Don't load nomodule scripts.
    if (aNoModule) {
      return;
    }

    static const char kASCIIWhitespace[] = "\t\n\f\r ";

    nsAutoString type(aType);
    type.Trim(kASCIIWhitespace);
    if (type.LowerCaseEqualsASCII("module")) {
      scriptKind = ScriptKind::eModule;
    }
  }

  if (scriptKind == ScriptKind::eClassic && !aType.IsEmpty() &&
      !nsContentUtils::IsJavascriptMIMEType(aType)) {
    // Unknown type.  Don't load it.
    return;
  }

  SRIMetadata sriMetadata;
  GetSRIMetadata(aIntegrity, &sriMetadata);

  RefPtr<ScriptLoadRequest> request = CreateLoadRequest(
      scriptKind, aURI, nullptr, mDocument->NodePrincipal(),
      Element::StringToCORSMode(aCrossOrigin), sriMetadata, aReferrerPolicy);
  request->mIsInline = false;
  request->mScriptFromHead = aScriptFromHead;
  request->SetScriptMode(aDefer, aAsync, aLinkPreload);
  request->SetIsPreloadRequest();

  if (LOG_ENABLED()) {
    nsAutoCString url;
    aURI->GetAsciiSpec(url);
    LOG(("ScriptLoadRequest (%p): Created preload request for %s",
         request.get(), url.get()));
  }

  nsresult rv = StartLoad(request);
  if (NS_FAILED(rv)) {
    return;
  }

  PreloadInfo* pi = mPreloads.AppendElement();
  pi->mRequest = request;
  pi->mCharset = aCharset;
}

void ScriptLoader::AddDeferRequest(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsDeferredScript());
  MOZ_ASSERT(!aRequest->mInDeferList && !aRequest->mInAsyncList);

  aRequest->mInDeferList = true;
  mDeferRequests.AppendElement(aRequest);
  if (mDeferEnabled && aRequest == mDeferRequests.getFirst() && mDocument &&
      !mBlockingDOMContentLoaded) {
    MOZ_ASSERT(mDocument->GetReadyStateEnum() == Document::READYSTATE_LOADING);
    mBlockingDOMContentLoaded = true;
    mDocument->BlockDOMContentLoaded();
  }
}

void ScriptLoader::AddAsyncRequest(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsAsyncScript());
  MOZ_ASSERT(!aRequest->mInDeferList && !aRequest->mInAsyncList);

  aRequest->mInAsyncList = true;
  if (aRequest->IsReadyToRun()) {
    mLoadedAsyncRequests.AppendElement(aRequest);
  } else {
    mLoadingAsyncRequests.AppendElement(aRequest);
  }
}

void ScriptLoader::MaybeMoveToLoadedList(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsReadyToRun());

  // If it's async, move it to the loaded list.  aRequest->mInAsyncList really
  // _should_ be in a list, but the consequences if it's not are bad enough we
  // want to avoid trying to move it if it's not.
  if (aRequest->mInAsyncList) {
    MOZ_ASSERT(aRequest->isInList());
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mLoadingAsyncRequests.Steal(aRequest);
      mLoadedAsyncRequests.AppendElement(req);
    }
  }
}

bool ScriptLoader::MaybeRemovedDeferRequests() {
  if (mDeferRequests.isEmpty() && mDocument && mBlockingDOMContentLoaded) {
    mBlockingDOMContentLoaded = false;
    mDocument->UnblockDOMContentLoaded();
    return true;
  }
  return false;
}

DocGroup* ScriptLoader::GetDocGroup() const { return mDocument->GetDocGroup(); }

void ScriptLoader::BeginDeferringScripts() {
  mDeferEnabled = true;
  if (mDeferCheckpointReached) {
    // We already completed a parse and were just waiting for some async
    // scripts to load (and were already blocking the load event waiting for
    // that to happen), when document.open() happened and now we're doing a
    // new parse.  We shouldn't block the load event again, but _should_ reset
    // mDeferCheckpointReached to false.  It'll get set to true again when the
    // DeferCheckpointReached call that corresponds to this
    // BeginDeferringScripts call happens (on document.close()), since we just
    // set mDeferEnabled to true.
    mDeferCheckpointReached = false;
  } else {
    if (mDocument) {
      mDocument->BlockOnload();
    }
  }
}

nsAutoScriptLoaderDisabler::nsAutoScriptLoaderDisabler(Document* aDoc) {
  mLoader = aDoc->ScriptLoader();
  mWasEnabled = mLoader->GetEnabled();
  if (mWasEnabled) {
    mLoader->SetEnabled(false);
  }
}

nsAutoScriptLoaderDisabler::~nsAutoScriptLoaderDisabler() {
  if (mWasEnabled) {
    mLoader->SetEnabled(true);
  }
}

#undef TRACE_FOR_TEST
#undef TRACE_FOR_TEST_BOOL
#undef TRACE_FOR_TEST_NONE

#undef LOG

}  // namespace dom
}  // namespace mozilla
