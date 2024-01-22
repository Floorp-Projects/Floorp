/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoader.h"
#include "ScriptLoadHandler.h"
#include "ScriptTrace.h"
#include "ModuleLoader.h"
#include "nsGenericHTMLElement.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/FetchPriority.h"
#include "mozilla/dom/RequestBinding.h"
#include "nsIChildChannel.h"
#include "zlib.h"

#include "prsystem.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"         // JS::GetArrayLength
#include "js/ColumnNumber.h"  // JS::ColumnNumberOneOrigin
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"  // JS::CompileOptions, JS::OwningCompileOptions, JS::DecodeOptions, JS::OwningDecodeOptions, JS::DelazificationOption
#include "js/ContextOptions.h"  // JS::ContextOptionsRef
#include "js/experimental/JSStencil.h"  // JS::Stencil, JS::InstantiationStorage
#include "js/experimental/CompileScript.h"  // JS::FrontendContext, JS::NewFrontendContext, JS::DestroyFrontendContext, JS::SetNativeStackQuota, JS::ThreadStackQuotaForSize, JS::CompilationStorage, JS::CompileGlobalScriptToStencil, JS::CompileModuleScriptToStencil, JS::DecodeStencil, JS::PrepareForInstantiate
#include "js/friend/ErrorMessages.h"        // js::GetErrorMessage, JSMSG_*
#include "js/loader/ScriptLoadRequest.h"
#include "ScriptCompression.h"
#include "js/loader/LoadedScript.h"
#include "js/loader/ModuleLoadRequest.h"
#include "js/MemoryFunctions.h"
#include "js/Modules.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty
#include "js/Realm.h"
#include "js/SourceText.h"
#include "js/Transcoding.h"  // JS::TranscodeRange, JS::TranscodeResult, JS::IsTranscodeFailureResult
#include "js/Utility.h"
#include "xpcpublic.h"
#include "GeckoProfiler.h"
#include "nsContentSecurityManager.h"
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
#include "mozilla/Mutex.h"  // mozilla::Mutex
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_javascript.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsAboutProtocolUtils.h"
#include "nsGkAtoms.h"
#include "nsNetUtil.h"
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
#include "mozilla/EventQueue.h"
#include "mozilla/LoadInfo.h"
#include "ReferrerInfo.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Attributes.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/TaskController.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit
#include "nsIScriptError.h"
#include "nsIAsyncOutputStream.h"
#include "js/loader/ModuleLoaderBase.h"
#include "mozilla/Maybe.h"

using JS::SourceText;
using namespace JS::loader;

using mozilla::Telemetry::LABELS_DOM_SCRIPT_PRELOAD_RESULT;

namespace mozilla::dom {

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
    mScriptLoader->Destroy();
    MOZ_ASSERT(!mScriptLoader);
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
// ScriptLoader
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScriptLoader)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(ScriptLoader, mNonAsyncExternalScriptInsertedRequests,
                         mLoadingAsyncRequests, mLoadedAsyncRequests,
                         mOffThreadCompilingRequests, mDeferRequests,
                         mXSLTRequests, mParserBlockingRequest,
                         mBytecodeEncodingQueue, mPreloads,
                         mPendingChildLoaders, mModuleLoader,
                         mWebExtModuleLoaders, mShadowRealmModuleLoaders)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ScriptLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ScriptLoader)

ScriptLoader::ScriptLoader(Document* aDocument)
    : mDocument(aDocument),
      mParserBlockingBlockerCount(0),
      mBlockerCount(0),
      mNumberOfProcessors(0),
      mTotalFullParseSize(0),
      mPhysicalSizeOfMemory(-1),
      mEnabled(true),
      mDeferEnabled(false),
      mSpeculativeOMTParsingEnabled(false),
      mDeferCheckpointReached(false),
      mBlockingDOMContentLoaded(false),
      mLoadEventFired(false),
      mGiveUpEncoding(false),
      mReporter(new ConsoleReportCollector()) {
  LOG(("ScriptLoader::ScriptLoader %p", this));

  mSpeculativeOMTParsingEnabled = StaticPrefs::
      dom_script_loader_external_scripts_speculative_omt_parse_enabled();

  mShutdownObserver = new AsyncCompileShutdownObserver(this);
  nsContentUtils::RegisterShutdownObserver(mShutdownObserver);
}

ScriptLoader::~ScriptLoader() {
  LOG(("ScriptLoader::~ScriptLoader %p", this));

  mObservers.Clear();

  if (mParserBlockingRequest) {
    FireScriptAvailable(NS_ERROR_ABORT, mParserBlockingRequest);
  }

  for (ScriptLoadRequest* req = mXSLTRequests.getFirst(); req;
       req = req->getNext()) {
    FireScriptAvailable(NS_ERROR_ABORT, req);
  }

  for (ScriptLoadRequest* req = mDeferRequests.getFirst(); req;
       req = req->getNext()) {
    FireScriptAvailable(NS_ERROR_ABORT, req);
  }

  for (ScriptLoadRequest* req = mLoadingAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    FireScriptAvailable(NS_ERROR_ABORT, req);
  }

  for (ScriptLoadRequest* req = mLoadedAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    FireScriptAvailable(NS_ERROR_ABORT, req);
  }

  for (ScriptLoadRequest* req =
           mNonAsyncExternalScriptInsertedRequests.getFirst();
       req; req = req->getNext()) {
    FireScriptAvailable(NS_ERROR_ABORT, req);
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

  mModuleLoader = nullptr;
}

void ScriptLoader::SetGlobalObject(nsIGlobalObject* aGlobalObject) {
  if (!aGlobalObject) {
    // The document is being detached.
    CancelAndClearScriptLoadRequests();
    return;
  }

  MOZ_ASSERT(!HasPendingRequests());

  if (!mModuleLoader) {
    // The module loader is associated with a global object, so don't create it
    // until we have a global set.
    mModuleLoader = new ModuleLoader(this, aGlobalObject, ModuleLoader::Normal);
  }

  MOZ_ASSERT(mModuleLoader->GetGlobalObject() == aGlobalObject);
  MOZ_ASSERT(aGlobalObject->GetModuleLoader(dom::danger::GetJSContext()) ==
             mModuleLoader);
}

void ScriptLoader::RegisterContentScriptModuleLoader(ModuleLoader* aLoader) {
  MOZ_ASSERT(aLoader);
  MOZ_ASSERT(aLoader->GetScriptLoader() == this);

  mWebExtModuleLoaders.AppendElement(aLoader);
}

void ScriptLoader::RegisterShadowRealmModuleLoader(ModuleLoader* aLoader) {
  MOZ_ASSERT(aLoader);
  MOZ_ASSERT(aLoader->GetScriptLoader() == this);

  mShadowRealmModuleLoaders.AppendElement(aLoader);
}

// Collect telemtry data about the cache information, and the kind of source
// which are being loaded, and where it is being loaded from.
static void CollectScriptTelemetry(ScriptLoadRequest* aRequest) {
  using namespace mozilla::Telemetry;

  MOZ_ASSERT(aRequest->IsFetching());

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
  if (aRequest->mFetchSourceOnly) {
    if (aRequest->GetScriptLoadContext()->mIsInline) {
      AccumulateCategorical(LABELS_DOM_SCRIPT_LOADING_SOURCE::Inline);
    } else if (aRequest->IsTextSource()) {
      AccumulateCategorical(LABELS_DOM_SCRIPT_LOADING_SOURCE::SourceFallback);
    }
  } else {
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
  if (!aScriptElement->AsElement()->GetAttr(nsGkAtoms::_for, forAttr) ||
      !aScriptElement->AsElement()->GetAttr(nsGkAtoms::event, eventAttr)) {
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
  if (aRequest->GetScriptLoadContext()->IsPreload()) {
    return aRequest->IsModuleRequest()
               ? nsIContentPolicy::TYPE_INTERNAL_MODULE_PRELOAD
               : nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD;
  }

  return aRequest->IsModuleRequest() ? nsIContentPolicy::TYPE_INTERNAL_MODULE
                                     : nsIContentPolicy::TYPE_INTERNAL_SCRIPT;
}

nsresult ScriptLoader::CheckContentPolicy(Document* aDocument,
                                          nsIScriptElement* aElement,
                                          const nsAString& aNonce,
                                          ScriptLoadRequest* aRequest) {
  nsContentPolicyType contentPolicyType =
      ScriptLoadRequestToContentPolicyType(aRequest);

  nsCOMPtr<nsINode> requestingNode = do_QueryInterface(aElement);
  nsCOMPtr<nsILoadInfo> secCheckLoadInfo = new net::LoadInfo(
      aDocument->NodePrincipal(),  // loading principal
      aDocument->NodePrincipal(),  // triggering principal
      requestingNode, nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
      contentPolicyType);
  secCheckLoadInfo->SetParserCreatedScript(aElement->GetParserCreated() !=
                                           mozilla::dom::NOT_FROM_PARSER);
  // Use nonce of the current element, instead of the preload, because those
  // are allowed to differ.
  secCheckLoadInfo->SetCspNonce(aNonce);
  if (aRequest->mIntegrity.IsValid()) {
    MOZ_ASSERT(!aRequest->mIntegrity.IsEmpty());
    secCheckLoadInfo->SetIntegrityMetadata(
        aRequest->mIntegrity.GetIntegrityString());
  }

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv =
      NS_CheckContentLoadPolicy(aRequest->mURI, secCheckLoadInfo, &shouldLoad,
                                nsContentUtils::GetContentPolicy());
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

nsIURI* ScriptLoader::GetBaseURI() const {
  MOZ_ASSERT(mDocument);
  return mDocument->GetDocBaseURI();
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
  NS_IMETHOD Run() override { return mLoader->ProcessRequest(mRequest); }
};

void ScriptLoader::RunScriptWhenSafe(ScriptLoadRequest* aRequest) {
  auto* runnable = new ScriptRequestProcessor(this, aRequest);
  nsContentUtils::AddScriptRunner(runnable);
}

nsresult ScriptLoader::RestartLoad(ScriptLoadRequest* aRequest) {
  aRequest->DropBytecode();
  TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                 "scriptloader_fallback");

  // Notify preload restart so that we can register this preload request again.
  aRequest->GetScriptLoadContext()->NotifyRestart(mDocument);

  // Start a new channel from which we explicitly request to stream the source
  // instead of the bytecode.
  aRequest->mFetchSourceOnly = true;
  nsresult rv;
  if (aRequest->IsModuleRequest()) {
    rv = aRequest->AsModuleRequest()->RestartModuleLoad();
  } else {
    rv = StartLoad(aRequest, Nothing());
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Close the current channel and this ScriptLoadHandler as we created a new
  // one for the same request.
  return NS_BINDING_RETARGETED;
}

nsresult ScriptLoader::StartLoad(
    ScriptLoadRequest* aRequest,
    const Maybe<nsAutoString>& aCharsetForPreload) {
  if (aRequest->IsModuleRequest()) {
    return aRequest->AsModuleRequest()->StartModuleLoad();
  }

  return StartClassicLoad(aRequest, aCharsetForPreload);
}

nsresult ScriptLoader::StartClassicLoad(
    ScriptLoadRequest* aRequest,
    const Maybe<nsAutoString>& aCharsetForPreload) {
  MOZ_ASSERT(aRequest->IsFetching());
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);
  aRequest->SetUnknownDataType();

  // If this document is sandboxed without 'allow-scripts', abort.
  if (mDocument->HasScriptsBlockedBySandbox()) {
    return NS_OK;
  }

  if (LOG_ENABLED()) {
    nsAutoCString url;
    aRequest->mURI->GetAsciiSpec(url);
    LOG(("ScriptLoadRequest (%p): Start Classic Load (url = %s)", aRequest,
         url.get()));
  }

  nsSecurityFlags securityFlags =
      nsContentSecurityManager::ComputeSecurityFlags(
          aRequest->CORSMode(), nsContentSecurityManager::CORSSecurityMapping::
                                    CORS_NONE_MAPS_TO_DISABLED_CORS_CHECKS);

  securityFlags |= nsILoadInfo::SEC_ALLOW_CHROME;

  nsresult rv = StartLoadInternal(aRequest, securityFlags, aCharsetForPreload);

  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

static bool IsWebExtensionRequest(ScriptLoadRequest* aRequest) {
  if (!aRequest->IsModuleRequest()) {
    return false;
  }

  ModuleLoader* loader =
      ModuleLoader::From(aRequest->AsModuleRequest()->mLoader);
  return loader->GetKind() == ModuleLoader::WebExtension;
}

static nsresult CreateChannelForScriptLoading(nsIChannel** aOutChannel,
                                              Document* aDocument,
                                              ScriptLoadRequest* aRequest,
                                              nsSecurityFlags aSecurityFlags) {
  nsContentPolicyType contentPolicyType =
      ScriptLoadRequestToContentPolicyType(aRequest);
  nsCOMPtr<nsINode> context;
  if (aRequest->GetScriptLoadContext()->GetScriptElement()) {
    context =
        do_QueryInterface(aRequest->GetScriptLoadContext()->GetScriptElement());
  } else {
    context = aDocument;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = aDocument->GetDocumentLoadGroup();
  nsCOMPtr<nsPIDOMWindowOuter> window = aDocument->GetWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_NULL_POINTER);
  nsIDocShell* docshell = window->GetDocShell();
  nsCOMPtr<nsIInterfaceRequestor> prompter(do_QueryInterface(docshell));

  return NS_NewChannelWithTriggeringPrincipal(
      aOutChannel, aRequest->mURI, context, aRequest->TriggeringPrincipal(),
      aSecurityFlags, contentPolicyType,
      nullptr,  // aPerformanceStorage
      loadGroup, prompter);
}

static void PrepareLoadInfoForScriptLoading(nsIChannel* aChannel,
                                            const ScriptLoadRequest* aRequest) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  loadInfo->SetParserCreatedScript(aRequest->ParserMetadata() ==
                                   ParserMetadata::ParserInserted);
  loadInfo->SetCspNonce(aRequest->Nonce());
  if (aRequest->mIntegrity.IsValid()) {
    MOZ_ASSERT(!aRequest->mIntegrity.IsEmpty());
    loadInfo->SetIntegrityMetadata(aRequest->mIntegrity.GetIntegrityString());
  }
}

// static
void ScriptLoader::PrepareCacheInfoChannel(nsIChannel* aChannel,
                                           ScriptLoadRequest* aRequest) {
  // To avoid decoding issues, the build-id is part of the bytecode MIME type
  // constant.
  aRequest->mCacheInfo = nullptr;
  nsCOMPtr<nsICacheInfoChannel> cic(do_QueryInterface(aChannel));
  if (cic && StaticPrefs::dom_script_loader_bytecode_cache_enabled()) {
    MOZ_ASSERT(!IsWebExtensionRequest(aRequest),
               "Can not bytecode cache WebExt code");
    if (!aRequest->mFetchSourceOnly) {
      // Inform the HTTP cache that we prefer to have information coming from
      // the bytecode cache instead of the sources, if such entry is already
      // registered.
      LOG(("ScriptLoadRequest (%p): Maybe request bytecode", aRequest));
      cic->PreferAlternativeDataType(
          ScriptLoader::BytecodeMimeTypeFor(aRequest), ""_ns,
          nsICacheInfoChannel::PreferredAlternativeDataDeliveryType::ASYNC);
    } else {
      // If we are explicitly loading from the sources, such as after a
      // restarted request, we might still want to save the bytecode after.
      //
      // The following tell the cache to look for an alternative data type which
      // does not exist, such that we can later save the bytecode with a
      // different alternative data type.
      LOG(("ScriptLoadRequest (%p): Request saving bytecode later", aRequest));
      cic->PreferAlternativeDataType(
          kNullMimeType, ""_ns,
          nsICacheInfoChannel::PreferredAlternativeDataDeliveryType::ASYNC);
    }
  }
}

static void AdjustPriorityAndClassOfServiceForLinkPreloadScripts(
    nsIChannel* aChannel, ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->GetScriptLoadContext()->IsLinkPreloadScript());

  if (!StaticPrefs::network_fetchpriority_enabled()) {
    // Put it to the group that is not blocked by leaders and doesn't block
    // follower at the same time.
    // Giving it a much higher priority will make this request be processed
    // ahead of other Unblocked requests, but with the same weight as
    // Leaders. This will make us behave similar way for both http2 and http1.
    ScriptLoadContext::PrioritizeAsPreload(aChannel);
    return;
  }

  if (nsCOMPtr<nsIClassOfService> cos = do_QueryInterface(aChannel)) {
    cos->AddClassFlags(nsIClassOfService::Unblocked);
  }

  if (nsCOMPtr<nsISupportsPriority> supportsPriority =
          do_QueryInterface(aChannel)) {
    LOG(("Is <link rel=[module]preload"));

    const RequestPriority fetchPriority = aRequest->FetchPriority();
    // The spec defines the priority to be set in an implementation defined
    // manner (<https://fetch.spec.whatwg.org/#concept-fetch>, step 15 and
    // <https://html.spec.whatwg.org/#concept-script-fetch-options-fetch-priority>).
    // For web-compatibility, the fetch priority mapping from
    // <https://web.dev/articles/fetch-priority#browser_priority_and_fetchpriority>
    // is taken.
    const int32_t supportsPriorityValue = [&]() {
      switch (fetchPriority) {
        case RequestPriority::Auto: {
          return nsISupportsPriority::PRIORITY_HIGH;
        }
        case RequestPriority::High: {
          return nsISupportsPriority::PRIORITY_HIGH;
        }
        case RequestPriority::Low: {
          return nsISupportsPriority::PRIORITY_LOW;
        }
        default: {
          MOZ_ASSERT_UNREACHABLE();
          return nsISupportsPriority::PRIORITY_NORMAL;
        }
      }
    }();

    LogPriorityMapping(ScriptLoader::gScriptLoaderLog,
                       ToFetchPriority(fetchPriority), supportsPriorityValue);

    supportsPriority->SetPriority(supportsPriorityValue);
  }
}

void AdjustPriorityForNonLinkPreloadScripts(nsIChannel* aChannel,
                                            ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(!aRequest->GetScriptLoadContext()->IsLinkPreloadScript());

  if (!StaticPrefs::network_fetchpriority_enabled()) {
    return;
  }

  if (nsCOMPtr<nsISupportsPriority> supportsPriority =
          do_QueryInterface(aChannel)) {
    LOG(("Is not <link rel=[module]preload"));
    const RequestPriority fetchPriority = aRequest->FetchPriority();

    // The spec defines the priority to be set in an implementation defined
    // manner (<https://fetch.spec.whatwg.org/#concept-fetch>, step 15 and
    // <https://html.spec.whatwg.org/#concept-script-fetch-options-fetch-priority>).
    // <testing/web-platform/mozilla/tests/fetch/fetchpriority/support/script-tests-data.js>
    // provides more context for the priority mapping.
    const int32_t supportsPriorityValue = [&]() {
      switch (fetchPriority) {
        case RequestPriority::Auto: {
          if (aRequest->IsModuleRequest()) {
            return nsISupportsPriority::PRIORITY_HIGH;
          }

          const ScriptLoadContext* scriptLoadContext =
              aRequest->GetScriptLoadContext();
          if (scriptLoadContext->IsAsyncScript() ||
              scriptLoadContext->IsDeferredScript()) {
            return nsISupportsPriority::PRIORITY_LOW;
          }

          if (scriptLoadContext->mScriptFromHead) {
            return nsISupportsPriority::PRIORITY_HIGH;
          }

          return nsISupportsPriority::PRIORITY_NORMAL;
        }
        case RequestPriority::Low: {
          return nsISupportsPriority::PRIORITY_LOW;
        }
        case RequestPriority::High: {
          return nsISupportsPriority::PRIORITY_HIGH;
        }
        default: {
          MOZ_ASSERT_UNREACHABLE();
          return nsISupportsPriority::PRIORITY_NORMAL;
        }
      }
    }();

    if (supportsPriorityValue) {
      LogPriorityMapping(ScriptLoader::gScriptLoaderLog,
                         ToFetchPriority(fetchPriority), supportsPriorityValue);
      supportsPriority->SetPriority(supportsPriorityValue);
    }
  }
}

// static
void ScriptLoader::PrepareRequestPriorityAndRequestDependencies(
    nsIChannel* aChannel, ScriptLoadRequest* aRequest) {
  if (aRequest->GetScriptLoadContext()->IsLinkPreloadScript()) {
    // This is <link rel="preload" as="script"> or <link rel="modulepreload">
    // initiated speculative load
    // (https://developer.mozilla.org/en-US/docs/Web/Performance/Speculative_loading).
    AdjustPriorityAndClassOfServiceForLinkPreloadScripts(aChannel, aRequest);
    ScriptLoadContext::AddLoadBackgroundFlag(aChannel);
  } else if (nsCOMPtr<nsIClassOfService> cos = do_QueryInterface(aChannel)) {
    AdjustPriorityForNonLinkPreloadScripts(aChannel, aRequest);

    if (aRequest->GetScriptLoadContext()->mScriptFromHead &&
        aRequest->GetScriptLoadContext()->IsBlockingScript()) {
      // synchronous head scripts block loading of most other non js/css
      // content such as images, Leader implicitely disallows tailing
      cos->AddClassFlags(nsIClassOfService::Leader);
    } else if (aRequest->GetScriptLoadContext()->IsDeferredScript() &&
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

      if (aRequest->GetScriptLoadContext()->IsAsyncScript()) {
        // async scripts are allowed tailing, since those and only those
        // don't block DOMContentLoaded; this flag doesn't enforce tailing,
        // just overweights the Unblocked flag when the channel is found
        // to be a thrird-party tracker and thus set the Tail flag to engage
        // tailing.
        cos->AddClassFlags(nsIClassOfService::TailAllowed);
      }
    }
  }
}

// static
nsresult ScriptLoader::PrepareHttpRequestAndInitiatorType(
    nsIChannel* aChannel, ScriptLoadRequest* aRequest,
    const Maybe<nsAutoString>& aCharsetForPreload) {
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
  nsresult rv = NS_OK;

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

    nsAutoString hintCharset;
    if (!aRequest->GetScriptLoadContext()->IsPreload() &&
        aRequest->GetScriptLoadContext()->GetScriptElement()) {
      aRequest->GetScriptLoadContext()->GetScriptElement()->GetScriptCharset(
          hintCharset);
    } else if (aCharsetForPreload.isSome()) {
      hintCharset = aCharsetForPreload.ref();
    }

    rv = httpChannel->SetClassicScriptHintCharset(hintCharset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set the initiator type
  nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(httpChannel));
  if (timedChannel) {
    if (aRequest->mEarlyHintPreloaderId) {
      timedChannel->SetInitiatorType(u"early-hints"_ns);
    } else if (aRequest->GetScriptLoadContext()->IsLinkPreloadScript()) {
      timedChannel->SetInitiatorType(u"link"_ns);
    } else {
      timedChannel->SetInitiatorType(u"script"_ns);
    }
  }

  return rv;
}

nsresult ScriptLoader::PrepareIncrementalStreamLoader(
    nsIIncrementalStreamLoader** aOutLoader, ScriptLoadRequest* aRequest) {
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

  nsresult rv = NS_NewIncrementalStreamLoader(aOutLoader, handler);
  NS_ENSURE_SUCCESS(rv, rv);
  return rv;
}

nsresult ScriptLoader::StartLoadInternal(
    ScriptLoadRequest* aRequest, nsSecurityFlags securityFlags,
    const Maybe<nsAutoString>& aCharsetForPreload) {
  nsCOMPtr<nsIChannel> channel;
  nsresult rv = CreateChannelForScriptLoading(
      getter_AddRefs(channel), mDocument, aRequest, securityFlags);

  NS_ENSURE_SUCCESS(rv, rv);

  if (aRequest->mEarlyHintPreloaderId) {
    nsCOMPtr<nsIHttpChannelInternal> channelInternal =
        do_QueryInterface(channel);
    NS_ENSURE_TRUE(channelInternal != nullptr, NS_ERROR_FAILURE);

    rv = channelInternal->SetEarlyHintPreloaderId(
        aRequest->mEarlyHintPreloaderId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PrepareLoadInfoForScriptLoading(channel, aRequest);

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobal = GetScriptGlobalObject();
  if (!scriptGlobal) {
    return NS_ERROR_FAILURE;
  }

  ScriptLoader::PrepareCacheInfoChannel(channel, aRequest);

  LOG(("ScriptLoadRequest (%p): mode=%u tracking=%d", aRequest,
       unsigned(aRequest->GetScriptLoadContext()->mScriptMode),
       aRequest->GetScriptLoadContext()->IsTracking()));

  PrepareRequestPriorityAndRequestDependencies(channel, aRequest);

  rv =
      PrepareHttpRequestAndInitiatorType(channel, aRequest, aCharsetForPreload);
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::net::PredictorLearn(
      aRequest->mURI, mDocument->GetDocumentURI(),
      nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE,
      mDocument->NodePrincipal()->OriginAttributesRef());

  nsCOMPtr<nsIIncrementalStreamLoader> loader;
  rv = PrepareIncrementalStreamLoader(getter_AddRefs(loader), aRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  auto key = PreloadHashKey::CreateAsScript(
      aRequest->mURI, aRequest->CORSMode(), aRequest->mKind);
  aRequest->GetScriptLoadContext()->NotifyOpen(
      key, channel, mDocument,
      aRequest->GetScriptLoadContext()->IsLinkPreloadScript(),
      aRequest->IsModuleRequest());

  rv = channel->AsyncOpen(loader);

  if (NS_FAILED(rv)) {
    // Make sure to inform any <link preload> tags about failure to load the
    // resource.
    aRequest->GetScriptLoadContext()->NotifyStart(channel);
    aRequest->GetScriptLoadContext()->NotifyStop(rv);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

bool ScriptLoader::PreloadURIComparator::Equals(const PreloadInfo& aPi,
                                                nsIURI* const& aURI) const {
  bool same;
  return NS_SUCCEEDED(aPi.mRequest->mURI->Equals(aURI, &same)) && same;
}

static bool CSPAllowsInlineScript(nsIScriptElement* aElement,
                                  const nsAString& aNonce,
                                  Document* aDocument) {
  nsCOMPtr<nsIContentSecurityPolicy> csp = aDocument->GetCsp();
  if (!csp) {
    // no CSP --> allow
    return true;
  }

  bool parserCreated =
      aElement->GetParserCreated() != mozilla::dom::NOT_FROM_PARSER;
  nsCOMPtr<Element> element = do_QueryInterface(aElement);

  bool allowInlineScript = false;
  nsresult rv = csp->GetAllowsInline(
      nsIContentSecurityPolicy::SCRIPT_SRC_ELEM_DIRECTIVE,
      false /* aHasUnsafeHash */, aNonce, parserCreated, element,
      nullptr /* nsICSPEventListener */, u""_ns,
      aElement->GetScriptLineNumber(),
      aElement->GetScriptColumnNumber().oneOriginValue(), &allowInlineScript);
  return NS_SUCCEEDED(rv) && allowInlineScript;
}

namespace {
RequestPriority FetchPriorityToRequestPriority(
    const FetchPriority aFetchPriority) {
  switch (aFetchPriority) {
    case FetchPriority::High:
      return RequestPriority::High;
    case FetchPriority::Low:
      return RequestPriority::Low;
    case FetchPriority::Auto:
      return RequestPriority::Auto;
  }

  MOZ_ASSERT_UNREACHABLE();
  return RequestPriority::Auto;
}
}  // namespace

already_AddRefed<ScriptLoadRequest> ScriptLoader::CreateLoadRequest(
    ScriptKind aKind, nsIURI* aURI, nsIScriptElement* aElement,
    nsIPrincipal* aTriggeringPrincipal, CORSMode aCORSMode,
    const nsAString& aNonce, RequestPriority aRequestPriority,
    const SRIMetadata& aIntegrity, ReferrerPolicy aReferrerPolicy,
    ParserMetadata aParserMetadata) {
  nsIURI* referrer = mDocument->GetDocumentURIAsReferrer();
  nsCOMPtr<Element> domElement = do_QueryInterface(aElement);
  RefPtr<ScriptFetchOptions> fetchOptions =
      new ScriptFetchOptions(aCORSMode, aNonce, aRequestPriority,
                             aParserMetadata, aTriggeringPrincipal, domElement);
  RefPtr<ScriptLoadContext> context = new ScriptLoadContext();

  if (aKind == ScriptKind::eClassic || aKind == ScriptKind::eImportMap) {
    RefPtr<ScriptLoadRequest> aRequest =
        new ScriptLoadRequest(aKind, aURI, aReferrerPolicy, fetchOptions,
                              aIntegrity, referrer, context);

    aRequest->NoCacheEntryFound();
    return aRequest.forget();
  }

  MOZ_ASSERT(aKind == ScriptKind::eModule);
  RefPtr<ModuleLoadRequest> aRequest = ModuleLoader::CreateTopLevel(
      aURI, aReferrerPolicy, fetchOptions, aIntegrity, referrer, this, context);
  return aRequest.forget();
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

  ScriptKind scriptKind;
  if (aElement->GetScriptIsModule()) {
    scriptKind = ScriptKind::eModule;
  } else if (aElement->GetScriptIsImportMap()) {
    scriptKind = ScriptKind::eImportMap;
  } else {
    scriptKind = ScriptKind::eClassic;
  }

  // Step 13. Check that the script is not an eventhandler
  if (IsScriptEventHandler(scriptKind, scriptContent)) {
    return false;
  }

  // "In modern user agents that support module scripts, the script element with
  // the nomodule attribute will be ignored".
  // "The nomodule attribute must not be specified on module scripts (and will
  // be ignored if it is)."
  if (scriptKind == ScriptKind::eClassic && scriptContent->IsHTMLElement() &&
      scriptContent->AsElement()->HasAttr(nsGkAtoms::nomodule)) {
    return false;
  }

  // Step 15. and later in the HTML5 spec
  if (aElement->GetScriptExternal()) {
    return ProcessExternalScript(aElement, scriptKind, scriptContent);
  }

  return ProcessInlineScript(aElement, scriptKind);
}

static ParserMetadata GetParserMetadata(nsIScriptElement* aElement) {
  return aElement->GetParserCreated() == mozilla::dom::NOT_FROM_PARSER
             ? ParserMetadata::NotParserInserted
             : ParserMetadata::ParserInserted;
}

bool ScriptLoader::ProcessExternalScript(nsIScriptElement* aElement,
                                         ScriptKind aScriptKind,
                                         nsIContent* aScriptContent) {
  LOG(("ScriptLoader (%p): Process external script for element %p", this,
       aElement));

  // https://html.spec.whatwg.org/multipage/scripting.html#prepare-the-script-element
  // Step 30.1. If el's type is "importmap", then queue an element task on the
  // DOM manipulation task source given el to fire an event named error at el,
  // and return.
  if (aScriptKind == ScriptKind::eImportMap) {
    NS_DispatchToCurrentThread(
        NewRunnableMethod("nsIScriptElement::FireErrorEvent", aElement,
                          &nsIScriptElement::FireErrorEvent));
    nsContentUtils::ReportToConsole(
        nsIScriptError::warningFlag, "Script Loader"_ns, mDocument,
        nsContentUtils::eDOM_PROPERTIES, "ImportMapExternalNotSupported");
    return false;
  }

  nsCOMPtr<nsIURI> scriptURI = aElement->GetScriptURI();
  if (!scriptURI) {
    // Asynchronously report the failure to create a URI object
    NS_DispatchToCurrentThread(
        NewRunnableMethod("nsIScriptElement::FireErrorEvent", aElement,
                          &nsIScriptElement::FireErrorEvent));
    return false;
  }

  nsAutoString nonce;
  if (nsString* cspNonce = static_cast<nsString*>(
          aScriptContent->GetProperty(nsGkAtoms::nonce))) {
    nonce = *cspNonce;
  }

  SRIMetadata sriMetadata;
  {
    nsAutoString integrity;
    aScriptContent->AsElement()->GetAttr(nsGkAtoms::integrity, integrity);
    GetSRIMetadata(integrity, &sriMetadata);
  }

  RefPtr<ScriptLoadRequest> request =
      LookupPreloadRequest(aElement, aScriptKind, sriMetadata);

  if (request &&
      NS_FAILED(CheckContentPolicy(mDocument, aElement, nonce, request))) {
    LOG(("ScriptLoader (%p): content policy check failed for preload", this));

    // Probably plans have changed; even though the preload was allowed seems
    // like the actual load is not; let's cancel the preload request.
    request->Cancel();
    AccumulateCategorical(LABELS_DOM_SCRIPT_PRELOAD_RESULT::RejectedByPolicy);
    return false;
  }

  if (request && request->IsModuleRequest() &&
      mModuleLoader->HasImportMapRegistered() &&
      request->mState > ScriptLoadRequest::State::Fetching) {
    // We don't preload module scripts after seeing an import map but a script
    // can dynamically insert an import map after preloading has happened.
    //
    // In the case of an import map is inserted after preloading has happened,
    // We also check if the request is fetched, if not then we can reuse the
    // preloaded request.
    request->Cancel();
    request = nullptr;
  }

  if (request) {
    // Use the preload request.

    LOG(("ScriptLoadRequest (%p): Using preload request", request.get()));

    // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-script-tree
    // Step 1. Disallow further import maps given settings object.
    if (request->IsModuleRequest()) {
      LOG(("ScriptLoadRequest (%p): Disallow further import maps.",
           request.get()));
      mModuleLoader->DisallowImportMaps();
    }

    // It's possible these attributes changed since we started the preload so
    // update them here.
    request->GetScriptLoadContext()->SetScriptMode(
        aElement->GetScriptDeferred(), aElement->GetScriptAsync(), false);

    // The request will be added to another list or set as
    // mParserBlockingRequest below.
    if (request->GetScriptLoadContext()->mInCompilingList) {
      mOffThreadCompilingRequests.Remove(request);
      request->GetScriptLoadContext()->mInCompilingList = false;
    }

    AccumulateCategorical(LABELS_DOM_SCRIPT_PRELOAD_RESULT::Used);
  } else {
    // No usable preload found.

    nsCOMPtr<nsIPrincipal> principal =
        aElement->GetScriptURITriggeringPrincipal();
    if (!principal) {
      principal = aScriptContent->NodePrincipal();
    }

    CORSMode ourCORSMode = aElement->GetCORSMode();
    const FetchPriority fetchPriority = aElement->GetFetchPriority();
    ReferrerPolicy referrerPolicy = GetReferrerPolicy(aElement);
    ParserMetadata parserMetadata = GetParserMetadata(aElement);

    request = CreateLoadRequest(aScriptKind, scriptURI, aElement, principal,
                                ourCORSMode, nonce,
                                FetchPriorityToRequestPriority(fetchPriority),
                                sriMetadata, referrerPolicy, parserMetadata);
    request->GetScriptLoadContext()->mIsInline = false;
    request->GetScriptLoadContext()->SetScriptMode(
        aElement->GetScriptDeferred(), aElement->GetScriptAsync(), false);
    // keep request->GetScriptLoadContext()->mScriptFromHead to false so we
    // don't treat non preloaded scripts as blockers for full page load. See bug
    // 792438.

    LOG(("ScriptLoadRequest (%p): Created request for external script",
         request.get()));

    nsresult rv = StartLoad(request, Nothing());
    if (NS_FAILED(rv)) {
      ReportErrorToConsole(request, rv);

      // Asynchronously report the load failure
      nsCOMPtr<nsIRunnable> runnable =
          NewRunnableMethod("nsIScriptElement::FireErrorEvent", aElement,
                            &nsIScriptElement::FireErrorEvent);
      if (mDocument) {
        mDocument->Dispatch(runnable.forget());
      } else {
        NS_DispatchToCurrentThread(runnable.forget());
      }
      return false;
    }
  }

  // We should still be in loading stage of script unless we're loading a
  // module or speculatively off-main-thread parsing a script.
  NS_ASSERTION(SpeculativeOMTParsingEnabled() ||
                   !request->GetScriptLoadContext()->CompileStarted() ||
                   request->IsModuleRequest(),
               "Request should not yet be in compiling stage.");

  if (request->GetScriptLoadContext()->IsAsyncScript()) {
    AddAsyncRequest(request);
    if (request->IsFinished()) {
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
    request->GetScriptLoadContext()->mIsNonAsyncScriptInserted = true;
    mNonAsyncExternalScriptInsertedRequests.AppendElement(request);
    if (request->IsFinished()) {
      // The script is available already. Run it ASAP when the event
      // loop gets a chance to spin.
      ProcessPendingRequestsAsync();
    }
    return false;
  }
  // we now have a parser-inserted request that may or may not be still
  // loading
  if (request->GetScriptLoadContext()->IsDeferredScript()) {
    // We don't want to run this yet.
    // If we come here, the script is a parser-created script and it has
    // the defer attribute but not the async attribute OR it is a module
    // script without the async attribute. Since a
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
    request->GetScriptLoadContext()->mIsXSLT = true;
    mXSLTRequests.AppendElement(request);
    if (request->IsFinished()) {
      // The script is available already. Run it ASAP when the event
      // loop gets a chance to spin.
      ProcessPendingRequestsAsync();
    }
    return true;
  }

  if (request->IsFinished() && ReadyToExecuteParserBlockingScripts()) {
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

  nsCOMPtr<nsINode> node = do_QueryInterface(aElement);
  nsAutoString nonce;
  if (nsString* cspNonce =
          static_cast<nsString*>(node->GetProperty(nsGkAtoms::nonce))) {
    nonce = *cspNonce;
  }

  // Does CSP allow this inline script to run?
  if (!CSPAllowsInlineScript(aElement, nonce, mDocument)) {
    return false;
  }

  // Check if adding an import map script is allowed. If not, we bail out
  // early to prevent creating a load request.
  if (aScriptKind == ScriptKind::eImportMap) {
    // https://html.spec.whatwg.org/multipage/scripting.html#prepare-the-script-element
    // Step 31.2 type is "importmap":
    //   Step 1. If el's relevant global object's import maps allowed is false,
    //   then queue an element task on the DOM manipulation task source given el
    //   to fire an event named error at el, and return.
    if (!mModuleLoader->IsImportMapAllowed()) {
      NS_WARNING("ScriptLoader: import maps allowed is false.");
      const char* msg = mModuleLoader->HasImportMapRegistered()
                            ? "ImportMapNotAllowedMultiple"
                            : "ImportMapNotAllowedAfterModuleLoad";
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      "Script Loader"_ns, mDocument,
                                      nsContentUtils::eDOM_PROPERTIES, msg);
      NS_DispatchToCurrentThread(
          NewRunnableMethod("nsIScriptElement::FireErrorEvent", aElement,
                            &nsIScriptElement::FireErrorEvent));
      return false;
    }
  }

  // Inline classic scripts ignore their CORS mode and are always CORS_NONE.
  CORSMode corsMode = CORS_NONE;
  if (aScriptKind == ScriptKind::eModule) {
    corsMode = aElement->GetCORSMode();
  }
  // <https://html.spec.whatwg.org/multipage/scripting.html#prepare-the-script-element>
  // step 29 specifies to use the fetch priority. Presumably it has no effect
  // for inline scripts.
  const auto fetchPriority = aElement->GetFetchPriority();

  ReferrerPolicy referrerPolicy = GetReferrerPolicy(aElement);
  ParserMetadata parserMetadata = GetParserMetadata(aElement);

  RefPtr<ScriptLoadRequest> request =
      CreateLoadRequest(aScriptKind, mDocument->GetDocumentURI(), aElement,
                        mDocument->NodePrincipal(), corsMode, nonce,
                        FetchPriorityToRequestPriority(fetchPriority),
                        SRIMetadata(),  // SRI doesn't apply
                        referrerPolicy, parserMetadata);
  request->GetScriptLoadContext()->mIsInline = true;
  request->GetScriptLoadContext()->mLineNo = aElement->GetScriptLineNumber();
  request->GetScriptLoadContext()->mColumnNo =
      aElement->GetScriptColumnNumber();
  request->mFetchSourceOnly = true;
  request->SetTextSource(request->mLoadContext.get());
  TRACE_FOR_TEST_BOOL(request->GetScriptLoadContext()->GetScriptElement(),
                      "scriptloader_load_source");
  CollectScriptTelemetry(request);

  // Only the 'async' attribute is heeded on an inline module script and
  // inline classic scripts ignore both these attributes.
  MOZ_ASSERT(!aElement->GetScriptDeferred());
  MOZ_ASSERT_IF(!request->IsModuleRequest(), !aElement->GetScriptAsync());
  request->GetScriptLoadContext()->SetScriptMode(
      false, aElement->GetScriptAsync(), false);

  LOG(("ScriptLoadRequest (%p): Created request for inline script",
       request.get()));

  request->mBaseURL = mDocument->GetDocBaseURI();

  if (request->IsModuleRequest()) {
    // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-an-inline-module-script-graph
    // Step 1. Disallow further import maps given settings object.
    mModuleLoader->DisallowImportMaps();

    ModuleLoadRequest* modReq = request->AsModuleRequest();
    if (aElement->GetParserCreated() != NOT_FROM_PARSER) {
      if (aElement->GetScriptAsync()) {
        AddAsyncRequest(modReq);
      } else {
        AddDeferRequest(modReq);
      }
    }

    // This calls OnFetchComplete directly since there's no need to start
    // fetching an inline script.
    nsresult rv = modReq->OnFetchComplete(NS_OK);
    if (NS_FAILED(rv)) {
      ReportErrorToConsole(modReq, rv);
      HandleLoadError(modReq, rv);
    }

    return false;
  }

  if (request->IsImportMapRequest()) {
    // https://html.spec.whatwg.org/multipage/scripting.html#prepare-the-script-element
    // Step 31.2 type is "importmap":
    //   Impl note: Step 1 is done above before creating a ScriptLoadRequest.
    MOZ_ASSERT(mModuleLoader->IsImportMapAllowed());

    //   Step 2. Set el's relevant global object's import maps allowed to false.
    mModuleLoader->DisallowImportMaps();

    //   Step 3. Let result be the result of creating an import map parse result
    //   given source text and base URL.
    UniquePtr<ImportMap> importMap = mModuleLoader->ParseImportMap(request);
    if (!importMap) {
      // If parsing import maps fails, the exception will be reported in
      // ModuleLoaderBase::ParseImportMap, and the registration of the import
      // map will bail out early.
      return false;
    }

    // TODO: Bug 1781758: Move RegisterImportMap into EvaluateScriptElement.
    //
    // https://html.spec.whatwg.org/multipage/scripting.html#execute-the-script-element
    // The spec defines 'register an import map' should be done in
    // 'execute the script element', because inside 'execute the script element'
    // it will perform a 'preparation-time document check'.
    // However, as import maps could be only inline scripts by now, the
    // 'preparation-time document check' will never fail for import maps.
    // So we simply call 'register an import map' here.
    mModuleLoader->RegisterImportMap(std::move(importMap));
    return false;
  }

  request->mState = ScriptLoadRequest::State::Ready;
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
  RefPtr<ScriptLoadRequest> request = mPreloads[i].mRequest;
  if (aScriptKind != request->mKind) {
    return nullptr;
  }

  // Found preloaded request. Note that a script-inserted script can steal a
  // preload!
  request->GetScriptLoadContext()->SetIsLoadRequest(aElement);

  if (request->GetScriptLoadContext()->mWasCompiledOMT &&
      !request->IsModuleRequest()) {
    request->SetReady();
  }

  nsString preloadCharset(mPreloads[i].mCharset);
  mPreloads.RemoveElementAt(i);

  // Double-check that the charset the preload used is the same as the charset
  // we have now.
  nsAutoString elementCharset;
  aElement->GetScriptCharset(elementCharset);

  // Bug 1832361: charset and crossorigin attributes shouldn't affect matching
  // of module scripts and modulepreload
  if (!request->IsModuleRequest() &&
      (!elementCharset.Equals(preloadCharset) ||
       aElement->GetCORSMode() != request->CORSMode())) {
    // Drop the preload.
    request->Cancel();
    AccumulateCategorical(LABELS_DOM_SCRIPT_PRELOAD_RESULT::RequestMismatch);
    return nullptr;
  }

  if (!aSRIMetadata.CanTrustBeDelegatedTo(request->mIntegrity)) {
    // Don't cancel link preload requests, we want to deliver onload according
    // the result of the load, cancellation would unexpectedly lead to error
    // notification.
    if (!request->GetScriptLoadContext()->IsLinkPreloadScript()) {
      request->Cancel();
    }
    return nullptr;
  }

  // Report any errors that we skipped while preloading.
  ReportPreloadErrorsToConsole(request);

  // This makes sure the pending preload (if exists) for this resource is
  // properly marked as used and thus not notified in the console as unused.
  request->GetScriptLoadContext()->NotifyUsage(mDocument);
  // A used preload must no longer be found in the Document's hash table.  Any
  // <link preload> tag after the <script> tag will start a new request, that
  // can be satisfied from a different cache, but not from the preload cache.
  request->GetScriptLoadContext()->RemoveSelf(mDocument);

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

void ScriptLoader::CancelAndClearScriptLoadRequests() {
  // Cancel all requests that have not been executed and remove them.

  if (mParserBlockingRequest) {
    mParserBlockingRequest->Cancel();
    mParserBlockingRequest = nullptr;
  }

  mDeferRequests.CancelRequestsAndClear();
  mLoadingAsyncRequests.CancelRequestsAndClear();
  mLoadedAsyncRequests.CancelRequestsAndClear();
  mNonAsyncExternalScriptInsertedRequests.CancelRequestsAndClear();
  mXSLTRequests.CancelRequestsAndClear();
  mOffThreadCompilingRequests.CancelRequestsAndClear();

  if (mModuleLoader) {
    mModuleLoader->CancelAndClearDynamicImports();
  }

  for (ModuleLoader* loader : mWebExtModuleLoaders) {
    loader->CancelAndClearDynamicImports();
  }

  for (ModuleLoader* loader : mShadowRealmModuleLoaders) {
    loader->CancelAndClearDynamicImports();
  }

  for (size_t i = 0; i < mPreloads.Length(); i++) {
    mPreloads[i].mRequest->Cancel();
  }
  mPreloads.Clear();
}

nsresult ScriptLoader::CompileOffThreadOrProcessRequest(
    ScriptLoadRequest* aRequest) {
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");

  if (!aRequest->GetScriptLoadContext()->mCompileOrDecodeTask &&
      !aRequest->GetScriptLoadContext()->CompileStarted()) {
    bool couldCompile = false;
    nsresult rv = AttemptOffThreadScriptCompile(aRequest, &couldCompile);
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

namespace {

class OffThreadCompilationCompleteTask : public Task {
 public:
  OffThreadCompilationCompleteTask(ScriptLoadRequest* aRequest,
                                   ScriptLoader* aLoader)
      : Task(Kind::MainThreadOnly, EventQueuePriority::Normal),
        mRequest(aRequest),
        mLoader(aLoader) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void RecordStartTime() { mStartTime = TimeStamp::Now(); }
  void RecordStopTime() { mStopTime = TimeStamp::Now(); }

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  bool GetName(nsACString& aName) override {
    aName.AssignLiteral("dom::OffThreadCompilationCompleteTask");
    return true;
  }
#endif

  TaskResult Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    RefPtr<ScriptLoadContext> context = mRequest->GetScriptLoadContext();

    if (!context->mCompileOrDecodeTask) {
      // Request has been cancelled by MaybeCancelOffThreadScript.
      return TaskResult::Complete;
    }

    RecordStopTime();

    if (profiler_is_active()) {
      ProfilerString8View scriptSourceString;
      if (mRequest->IsTextSource()) {
        scriptSourceString = "ScriptCompileOffThread";
      } else {
        MOZ_ASSERT(mRequest->IsBytecode());
        scriptSourceString = "BytecodeDecodeOffThread";
      }

      nsAutoCString profilerLabelString;
      mRequest->GetScriptLoadContext()->GetProfilerLabel(profilerLabelString);
      PROFILER_MARKER_TEXT(scriptSourceString, JS,
                           MarkerTiming::Interval(mStartTime, mStopTime),
                           profilerLabelString);
    }

    (void)mLoader->ProcessOffThreadRequest(mRequest);

    mRequest = nullptr;
    mLoader = nullptr;
    return TaskResult::Complete;
  }

 private:
  // NOTE:
  // These fields are main-thread only, and this task shouldn't be freed off
  // main thread.
  //
  // This is guaranteed by not having off-thread tasks which depends on this
  // task, because otherwise the off-thread task's mDependencies can be the
  // last reference, which results in freeing this task off main thread.
  //
  // If such task is added, these fields must be moved to separate storage.
  RefPtr<ScriptLoadRequest> mRequest;
  RefPtr<ScriptLoader> mLoader;

  TimeStamp mStartTime;
  TimeStamp mStopTime;
};

} /* anonymous namespace */

// TODO: This uses the same heuristics and the same threshold as the
//       JS::CanCompileOffThread / JS::CanDecodeOffThread APIs, but the
//       heuristics needs to be updated to reflect the change regarding the
//       Stencil API, and also the thread management on the consumer side
//       (bug 1846160).
static constexpr size_t OffThreadMinimumTextLength = 5 * 1000;
static constexpr size_t OffThreadMinimumBytecodeLength = 5 * 1000;

nsresult ScriptLoader::AttemptOffThreadScriptCompile(
    ScriptLoadRequest* aRequest, bool* aCouldCompileOut) {
  // If speculative parsing is enabled, the request may not be ready to run if
  // the element is not yet available.
  MOZ_ASSERT_IF(!SpeculativeOMTParsingEnabled() && !aRequest->IsModuleRequest(),
                aRequest->IsFinished());
  MOZ_ASSERT(!aRequest->GetScriptLoadContext()->mWasCompiledOMT);
  MOZ_ASSERT(aCouldCompileOut && !*aCouldCompileOut);

  // Don't off-thread compile inline scripts.
  if (aRequest->GetScriptLoadContext()->mIsInline) {
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
  JS::CompileOptions options(cx);

  // Introduction script will actually be computed and set when the script is
  // collected from offthread
  JS::Rooted<JSScript*> dummyIntroductionScript(cx);
  nsresult rv = FillCompileOptionsForRequest(cx, aRequest, &options,
                                             &dummyIntroductionScript);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aRequest->IsTextSource()) {
    if (!StaticPrefs::javascript_options_parallel_parsing() ||
        aRequest->ScriptTextLength() < OffThreadMinimumTextLength) {
      TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                     "scriptloader_main_thread_compile");
      return NS_OK;
    }
  } else {
    MOZ_ASSERT(aRequest->IsBytecode());

    JS::TranscodeRange bytecode = aRequest->Bytecode();
    if (!StaticPrefs::javascript_options_parallel_parsing() ||
        bytecode.length() < OffThreadMinimumBytecodeLength) {
      return NS_OK;
    }
  }

  RefPtr<CompileOrDecodeTask> compileOrDecodeTask;
  rv = CreateOffThreadTask(cx, aRequest, options,
                           getter_AddRefs(compileOrDecodeTask));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<OffThreadCompilationCompleteTask> completeTask =
      new OffThreadCompilationCompleteTask(aRequest, this);

  completeTask->RecordStartTime();

  aRequest->GetScriptLoadContext()->mCompileOrDecodeTask = compileOrDecodeTask;
  completeTask->AddDependency(compileOrDecodeTask);

  TaskController::Get()->AddTask(compileOrDecodeTask.forget());
  TaskController::Get()->AddTask(completeTask.forget());

  aRequest->GetScriptLoadContext()->BlockOnload(mDocument);

  // Once the compilation is finished, the completeTask will be run on
  // the main thread to call ScriptLoader::ProcessOffThreadRequest for the
  // request.
  aRequest->mState = ScriptLoadRequest::State::Compiling;

  // Requests that are not tracked elsewhere are added to a list while they are
  // being compiled off-thread, so we can cancel the compilation later if
  // necessary.
  //
  // Non-top-level modules not tracked because these are cancelled from their
  // importing module.
  if (aRequest->IsTopLevel() && !aRequest->isInList()) {
    mOffThreadCompilingRequests.AppendElement(aRequest);
    aRequest->GetScriptLoadContext()->mInCompilingList = true;
  }

  *aCouldCompileOut = true;

  return NS_OK;
}

CompileOrDecodeTask::CompileOrDecodeTask()
    : Task(Kind::OffMainThreadOnly, EventQueuePriority::Normal),
      mMutex("CompileOrDecodeTask"),
      mOptions(JS::OwningCompileOptions::ForFrontendContext()) {}

CompileOrDecodeTask::~CompileOrDecodeTask() {
  if (mFrontendContext) {
    JS::DestroyFrontendContext(mFrontendContext);
    mFrontendContext = nullptr;
  }
}

nsresult CompileOrDecodeTask::InitFrontendContext() {
  mFrontendContext = JS::NewFrontendContext();
  if (!mFrontendContext) {
    mIsCancelled = true;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

void CompileOrDecodeTask::DidRunTask(const MutexAutoLock& aProofOfLock,
                                     RefPtr<JS::Stencil>&& aStencil) {
  if (aStencil) {
    if (!JS::PrepareForInstantiate(mFrontendContext, *aStencil,
                                   mInstantiationStorage)) {
      aStencil = nullptr;
    }
  }

  mStencil = std::move(aStencil);
}

already_AddRefed<JS::Stencil> CompileOrDecodeTask::StealResult(
    JSContext* aCx, JS::InstantiationStorage* aInstantiationStorage) {
  JS::FrontendContext* fc = mFrontendContext;
  mFrontendContext = nullptr;
  auto destroyFrontendContext =
      mozilla::MakeScopeExit([&]() { JS::DestroyFrontendContext(fc); });

  MOZ_ASSERT(fc);

  if (JS::HadFrontendErrors(fc)) {
    (void)JS::ConvertFrontendErrorsToRuntimeErrors(aCx, fc, mOptions);
    return nullptr;
  }

  if (!mStencil && JS::IsTranscodeFailureResult(mResult)) {
    // Decode failure with bad content isn't reported as error.
    JS_ReportErrorASCII(aCx, "failed to decode cache");
    return nullptr;
  }

  // Report warnings.
  if (!JS::ConvertFrontendErrorsToRuntimeErrors(aCx, fc, mOptions)) {
    return nullptr;
  }

  MOZ_ASSERT(mStencil,
             "If this task is cancelled, StealResult shouldn't be called");

  // This task is started and finished successfully.
  *aInstantiationStorage = std::move(mInstantiationStorage);

  return mStencil.forget();
}

void CompileOrDecodeTask::Cancel() {
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);

  mIsCancelled = true;
}

enum class CompilationTarget { Script, Module };

template <CompilationTarget target>
class ScriptOrModuleCompileTask final : public CompileOrDecodeTask {
 public:
  explicit ScriptOrModuleCompileTask(
      ScriptLoader::MaybeSourceText&& aMaybeSource)
      : CompileOrDecodeTask(), mMaybeSource(std::move(aMaybeSource)) {}

  nsresult Init(JS::CompileOptions& aOptions) {
    nsresult rv = InitFrontendContext();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mOptions.copy(mFrontendContext, aOptions)) {
      mIsCancelled = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
  }

  TaskResult Run() override {
    MutexAutoLock lock(mMutex);

    if (IsCancelled(lock)) {
      return TaskResult::Complete;
    }

    RefPtr<JS::Stencil> stencil = Compile();

    DidRunTask(lock, std::move(stencil));
    return TaskResult::Complete;
  }

 private:
  already_AddRefed<JS::Stencil> Compile() {
    size_t stackSize = TaskController::GetThreadStackSize();
    JS::SetNativeStackQuota(mFrontendContext,
                            JS::ThreadStackQuotaForSize(stackSize));

    JS::CompilationStorage compileStorage;
    auto compile = [&](auto& source) {
      if constexpr (target == CompilationTarget::Script) {
        return JS::CompileGlobalScriptToStencil(mFrontendContext, mOptions,
                                                source, compileStorage);
      }
      return JS::CompileModuleScriptToStencil(mFrontendContext, mOptions,
                                              source, compileStorage);
    };
    return mMaybeSource.mapNonEmpty(compile);
  }

 public:
#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  bool GetName(nsACString& aName) override {
    if constexpr (target == CompilationTarget::Script) {
      aName.AssignLiteral("ScriptCompileTask");
    } else {
      aName.AssignLiteral("ModuleCompileTask");
    }
    return true;
  }
#endif

 private:
  ScriptLoader::MaybeSourceText mMaybeSource;
};

using ScriptCompileTask =
    class ScriptOrModuleCompileTask<CompilationTarget::Script>;
using ModuleCompileTask =
    class ScriptOrModuleCompileTask<CompilationTarget::Module>;

class ScriptDecodeTask final : public CompileOrDecodeTask {
 public:
  explicit ScriptDecodeTask(const JS::TranscodeRange& aRange)
      : mRange(aRange) {}

  nsresult Init(JS::DecodeOptions& aOptions) {
    nsresult rv = InitFrontendContext();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mDecodeOptions.copy(mFrontendContext, aOptions)) {
      mIsCancelled = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
  }

  TaskResult Run() override {
    MutexAutoLock lock(mMutex);

    if (IsCancelled(lock)) {
      return TaskResult::Complete;
    }

    RefPtr<JS::Stencil> stencil = Decode();

    JS::OwningCompileOptions compileOptions(
        (JS::OwningCompileOptions::ForFrontendContext()));
    mOptions.steal(std::move(mDecodeOptions));

    DidRunTask(lock, std::move(stencil));
    return TaskResult::Complete;
  }

 private:
  already_AddRefed<JS::Stencil> Decode() {
    // NOTE: JS::DecodeStencil doesn't need the stack quota.

    JS::CompilationStorage compileStorage;
    RefPtr<JS::Stencil> stencil;
    mResult = JS::DecodeStencil(mFrontendContext, mDecodeOptions, mRange,
                                getter_AddRefs(stencil));
    return stencil.forget();
  }

 public:
#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  bool GetName(nsACString& aName) override {
    aName.AssignLiteral("ScriptDecodeTask");
    return true;
  }
#endif

 private:
  JS::OwningDecodeOptions mDecodeOptions;

  JS::TranscodeRange mRange;
};

nsresult ScriptLoader::CreateOffThreadTask(
    JSContext* aCx, ScriptLoadRequest* aRequest, JS::CompileOptions& aOptions,
    CompileOrDecodeTask** aCompileOrDecodeTask) {
  if (aRequest->IsBytecode()) {
    JS::TranscodeRange bytecode = aRequest->Bytecode();
    JS::DecodeOptions decodeOptions(aOptions);
    RefPtr<ScriptDecodeTask> decodeTask = new ScriptDecodeTask(bytecode);
    nsresult rv = decodeTask->Init(decodeOptions);
    NS_ENSURE_SUCCESS(rv, rv);
    decodeTask.forget(aCompileOrDecodeTask);
    return NS_OK;
  }

  MaybeSourceText maybeSource;
  nsresult rv = aRequest->GetScriptSource(aCx, &maybeSource,
                                          aRequest->mLoadContext.get());
  NS_ENSURE_SUCCESS(rv, rv);

  if (ShouldApplyDelazifyStrategy(aRequest)) {
    ApplyDelazifyStrategy(&aOptions);
    mTotalFullParseSize +=
        aRequest->ScriptTextLength() > 0
            ? static_cast<uint32_t>(aRequest->ScriptTextLength())
            : 0;

    LOG(
        ("ScriptLoadRequest (%p): non-on-demand-only (omt) Parsing Enabled "
         "for url=%s mTotalFullParseSize=%u",
         aRequest, aRequest->mURI->GetSpecOrDefault().get(),
         mTotalFullParseSize));
  }

  if (aRequest->IsModuleRequest()) {
    RefPtr<ModuleCompileTask> compileTask =
        new ModuleCompileTask(std::move(maybeSource));
    rv = compileTask->Init(aOptions);
    NS_ENSURE_SUCCESS(rv, rv);
    compileTask.forget(aCompileOrDecodeTask);
    return NS_OK;
  }

  if (StaticPrefs::dom_expose_test_interfaces()) {
    switch (aOptions.eagerDelazificationStrategy()) {
      case JS::DelazificationOption::OnDemandOnly:
        TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                       "delazification_on_demand_only");
        break;
      case JS::DelazificationOption::CheckConcurrentWithOnDemand:
      case JS::DelazificationOption::ConcurrentDepthFirst:
        TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                       "delazification_concurrent_depth_first");
        break;
      case JS::DelazificationOption::ConcurrentLargeFirst:
        TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                       "delazification_concurrent_large_first");
        break;
      case JS::DelazificationOption::ParseEverythingEagerly:
        TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                       "delazification_parse_everything_eagerly");
        break;
    }
  }

  RefPtr<ScriptCompileTask> compileTask =
      new ScriptCompileTask(std::move(maybeSource));
  rv = compileTask->Init(aOptions);
  NS_ENSURE_SUCCESS(rv, rv);
  compileTask.forget(aCompileOrDecodeTask);
  return NS_OK;
}

nsresult ScriptLoader::ProcessOffThreadRequest(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsCompiling());
  MOZ_ASSERT(!aRequest->GetScriptLoadContext()->mWasCompiledOMT);

  if (aRequest->IsCanceled()) {
    return NS_OK;
  }

  aRequest->GetScriptLoadContext()->mWasCompiledOMT = true;

  if (aRequest->GetScriptLoadContext()->mInCompilingList) {
    mOffThreadCompilingRequests.Remove(aRequest);
    aRequest->GetScriptLoadContext()->mInCompilingList = false;
  }

  if (aRequest->IsModuleRequest()) {
    MOZ_ASSERT(aRequest->GetScriptLoadContext()->mCompileOrDecodeTask);
    ModuleLoadRequest* request = aRequest->AsModuleRequest();
    return request->OnFetchComplete(NS_OK);
  }

  // Element may not be ready yet if speculatively compiling, so process the
  // request in ProcessPendingRequests when it is available.
  MOZ_ASSERT_IF(!SpeculativeOMTParsingEnabled(),
                aRequest->GetScriptLoadContext()->GetScriptElement());
  if (!aRequest->GetScriptLoadContext()->GetScriptElement()) {
    // Unblock onload here in case this request never gets executed.
    aRequest->GetScriptLoadContext()->MaybeUnblockOnload();
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
  if ((aRequest->GetScriptLoadContext()->IsAsyncScript() ||
       aRequest->GetScriptLoadContext()->IsBlockingScript()) &&
      !aRequest->isInList()) {
    return ProcessRequest(aRequest);
  }

  // Process other scripts in the proper order.
  ProcessPendingRequests();
  return NS_OK;
}

nsresult ScriptLoader::ProcessRequest(ScriptLoadRequest* aRequest) {
  LOG(("ScriptLoadRequest (%p): Process request", aRequest));

  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");
  NS_ASSERTION(aRequest->IsFinished(),
               "Processing a request that is not ready to run.");

  NS_ENSURE_ARG(aRequest);

  auto unblockOnload = MakeScopeExit(
      [&] { aRequest->GetScriptLoadContext()->MaybeUnblockOnload(); });

  if (aRequest->IsModuleRequest()) {
    ModuleLoadRequest* request = aRequest->AsModuleRequest();
    if (request->IsDynamicImport()) {
      request->ProcessDynamicImport();
      return NS_OK;
    }

    if (request->mModuleScript) {
      if (!request->InstantiateModuleGraph()) {
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
      do_QueryInterface(aRequest->GetScriptLoadContext()->GetScriptElement());

  nsCOMPtr<Document> doc;
  if (!aRequest->GetScriptLoadContext()->mIsInline ||
      aRequest->IsModuleRequest()) {
    doc = scriptElem->OwnerDoc();
  }

  nsCOMPtr<nsIScriptElement> oldParserInsertedScript;
  uint32_t parserCreated = aRequest->GetScriptLoadContext()->GetParserCreated();
  if (parserCreated) {
    oldParserInsertedScript = mCurrentParserInsertedScript;
    mCurrentParserInsertedScript =
        aRequest->GetScriptLoadContext()->GetScriptElement();
  }

  aRequest->GetScriptLoadContext()->GetScriptElement()->BeginEvaluating();

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
    rv = EvaluateScriptElement(aRequest);
    if (doc) {
      doc->DecrementIgnoreDestructiveWritesCounter();
    }

    nsContentUtils::DispatchTrustedEvent(scriptElem->OwnerDoc(), scriptElem,
                                         u"afterscriptexecute"_ns,
                                         CanBubble::eYes, Cancelable::eNo);
  }

  FireScriptEvaluated(rv, aRequest);

  aRequest->GetScriptLoadContext()->GetScriptElement()->EndEvaluating();

  if (parserCreated) {
    mCurrentParserInsertedScript = oldParserInsertedScript;
  }

  if (aRequest->GetScriptLoadContext()->mCompileOrDecodeTask) {
    // The request was parsed off-main-thread, but the result of the off
    // thread parse was not actually needed to process the request
    // (disappearing window, some other error, ...). Finish the
    // request to avoid leaks.
    MOZ_ASSERT(!aRequest->IsModuleRequest());
    aRequest->GetScriptLoadContext()->MaybeCancelOffThreadScript();
  }

  // Free any source data, but keep the bytecode content as we might have to
  // save it later.
  aRequest->ClearScriptSource();
  if (aRequest->IsBytecode()) {
    // We received bytecode as input, thus we were decoding, and we will not be
    // encoding the bytecode once more. We can safely clear the content of this
    // buffer.
    aRequest->DropBytecode();
  }

  return rv;
}

void ScriptLoader::FireScriptAvailable(nsresult aResult,
                                       ScriptLoadRequest* aRequest) {
  for (int32_t i = 0; i < mObservers.Count(); i++) {
    nsCOMPtr<nsIScriptLoaderObserver> obs = mObservers[i];
    obs->ScriptAvailable(
        aResult, aRequest->GetScriptLoadContext()->GetScriptElement(),
        aRequest->GetScriptLoadContext()->mIsInline, aRequest->mURI,
        aRequest->GetScriptLoadContext()->mLineNo);
  }

  bool isInlineClassicScript = aRequest->GetScriptLoadContext()->mIsInline &&
                               !aRequest->IsModuleRequest();
  RefPtr<nsIScriptElement> scriptElement =
      aRequest->GetScriptLoadContext()->GetScriptElement();
  scriptElement->ScriptAvailable(aResult, scriptElement, isInlineClassicScript,
                                 aRequest->mURI,
                                 aRequest->GetScriptLoadContext()->mLineNo);
}

// TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
MOZ_CAN_RUN_SCRIPT_BOUNDARY void ScriptLoader::FireScriptEvaluated(
    nsresult aResult, ScriptLoadRequest* aRequest) {
  for (int32_t i = 0; i < mObservers.Count(); i++) {
    nsCOMPtr<nsIScriptLoaderObserver> obs = mObservers[i];
    RefPtr<nsIScriptElement> scriptElement =
        aRequest->GetScriptLoadContext()->GetScriptElement();
    obs->ScriptEvaluated(aResult, scriptElement,
                         aRequest->GetScriptLoadContext()->mIsInline);
  }

  RefPtr<nsIScriptElement> scriptElement =
      aRequest->GetScriptLoadContext()->GetScriptElement();
  scriptElement->ScriptEvaluated(aResult, scriptElement,
                                 aRequest->GetScriptLoadContext()->mIsInline);
}

already_AddRefed<nsIGlobalObject> ScriptLoader::GetGlobalForRequest(
    ScriptLoadRequest* aRequest) {
  if (aRequest->IsModuleRequest()) {
    ModuleLoader* loader =
        ModuleLoader::From(aRequest->AsModuleRequest()->mLoader);
    nsCOMPtr<nsIGlobalObject> global = loader->GetGlobalObject();
    return global.forget();
  }

  return GetScriptGlobalObject();
}

already_AddRefed<nsIScriptGlobalObject> ScriptLoader::GetScriptGlobalObject() {
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
    JSContext* aCx, ScriptLoadRequest* aRequest, JS::CompileOptions* aOptions,
    JS::MutableHandle<JSScript*> aIntroductionScript) {
  // It's very important to use aRequest->mURI, not the final URI of the channel
  // aRequest ended up getting script data from, as the script filename.
  nsresult rv = aRequest->mURI->GetSpec(aRequest->mURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mDocument) {
    mDocument->NoteScriptTrackingStatus(
        aRequest->mURL, aRequest->GetScriptLoadContext()->IsTracking());
  }

  const char* introductionType;
  if (aRequest->IsModuleRequest() &&
      !aRequest->AsModuleRequest()->IsTopLevel()) {
    introductionType = "importedModule";
  } else if (!aRequest->GetScriptLoadContext()->mIsInline) {
    introductionType = "srcScript";
  } else if (aRequest->GetScriptLoadContext()->GetParserCreated() ==
             FROM_PARSER_NETWORK) {
    introductionType = "inlineScript";
  } else {
    introductionType = "injectedScript";
  }
  aOptions->setIntroductionInfoToCaller(aCx, introductionType,
                                        aIntroductionScript);
  aOptions->setFileAndLine(aRequest->mURL.get(),
                           aRequest->GetScriptLoadContext()->mLineNo);
  // The column is only relevant for inline scripts in order for SpiderMonkey to
  // properly compute offsets relatively to the script position within the HTML
  // file. injectedScript are not concerned and are always considered to start
  // at column 0.
  if (aRequest->GetScriptLoadContext()->mIsInline &&
      aRequest->GetScriptLoadContext()->GetParserCreated() ==
          FROM_PARSER_NETWORK) {
    aOptions->setColumn(aRequest->GetScriptLoadContext()->mColumnNo);
  }
  aOptions->setIsRunOnce(true);
  aOptions->setNoScriptRval(true);
  if (aRequest->mSourceMapURL) {
    aOptions->setSourceMapURL(aRequest->mSourceMapURL->get());
  }
  if (aRequest->mOriginPrincipal) {
    nsCOMPtr<nsIGlobalObject> globalObject = GetGlobalForRequest(aRequest);
    nsIPrincipal* scriptPrin = globalObject->PrincipalOrNull();
    MOZ_ASSERT(scriptPrin);
    bool subsumes = scriptPrin->Subsumes(aRequest->mOriginPrincipal);
    aOptions->setMutedErrors(!subsumes);
  }

  if (aRequest->IsModuleRequest()) {
    aOptions->setHideScriptFromDebugger(true);
  }

  aOptions->setDeferDebugMetadata(true);

  aOptions->borrowBuffer = true;

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
  uint32_t fetchCountMin = 4;

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
    sourceLength = aRequest->ReceivedScriptTextLength();
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
    uint32_t fetchCount = 0;
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

static nsresult ExecuteCompiledScript(JSContext* aCx, JSExecutionContext& aExec,
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

nsresult ScriptLoader::EvaluateScriptElement(ScriptLoadRequest* aRequest) {
  using namespace mozilla::Telemetry;
  MOZ_ASSERT(aRequest->IsFinished());

  // We need a document to evaluate scripts.
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> scriptContent(
      do_QueryInterface(aRequest->GetScriptLoadContext()->GetScriptElement()));
  MOZ_ASSERT(scriptContent);
  Document* ownerDoc = scriptContent->OwnerDoc();
  if (ownerDoc != mDocument) {
    // Willful violation of HTML5 as of 2010-12-01
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIGlobalObject> globalObject;
  nsCOMPtr<nsIScriptContext> context;
  if (!IsWebExtensionRequest(aRequest)) {
    // Otherwise we have to ensure that there is a nsIScriptContext.
    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal = GetScriptGlobalObject();
    if (!scriptGlobal) {
      return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT_IF(
        aRequest->IsModuleRequest(),
        aRequest->AsModuleRequest()->GetGlobalObject() == scriptGlobal);

    // Make sure context is a strong reference since we access it after
    // we've executed a script, which may cause all other references to
    // the context to go away.
    context = scriptGlobal->GetScriptContext();
    if (!context) {
      return NS_ERROR_FAILURE;
    }

    globalObject = scriptGlobal;
  }

  // Update our current script.
  // This must be destroyed after destroying nsAutoMicroTask, see:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1620505#c4
  nsIScriptElement* currentScript =
      aRequest->IsModuleRequest()
          ? nullptr
          : aRequest->GetScriptLoadContext()->GetScriptElement();
  AutoCurrentScriptUpdater scriptUpdater(this, currentScript);

  Maybe<AutoSetProcessingScriptTag> setProcessingScriptTag;
  if (context) {
    setProcessingScriptTag.emplace(context);
  }

  // https://wicg.github.io/import-maps/#integration-script-type
  // Switch on the script's type for scriptElement:
  // "importmap"
  //    Assert: Never reached.
  MOZ_ASSERT(!aRequest->IsImportMapRequest());

  if (aRequest->IsModuleRequest()) {
    return aRequest->AsModuleRequest()->EvaluateModule();
  }

  return EvaluateScript(globalObject, aRequest);
}

nsresult ScriptLoader::CompileOrDecodeClassicScript(
    JSContext* aCx, JSExecutionContext& aExec, ScriptLoadRequest* aRequest) {
  nsAutoCString profilerLabelString;
  aRequest->GetScriptLoadContext()->GetProfilerLabel(profilerLabelString);

  nsresult rv;
  if (aRequest->IsBytecode()) {
    if (aRequest->GetScriptLoadContext()->mCompileOrDecodeTask) {
      LOG(("ScriptLoadRequest (%p): Decode Bytecode & instantiate and Execute",
           aRequest));
      rv = aExec.JoinOffThread(aRequest->GetScriptLoadContext());
    } else {
      LOG(("ScriptLoadRequest (%p): Decode Bytecode and Execute", aRequest));
      AUTO_PROFILER_MARKER_TEXT("BytecodeDecodeMainThread", JS,
                                MarkerInnerWindowIdFromJSContext(aCx),
                                profilerLabelString);

      rv = aExec.Decode(aRequest->Bytecode());
    }

    // We do not expect to be saving anything when we already have some
    // bytecode.
    MOZ_ASSERT(!aRequest->mCacheInfo);
    return rv;
  }

  MOZ_ASSERT(aRequest->IsSource());
  bool encodeBytecode = ShouldCacheBytecode(aRequest);
  aExec.SetEncodeBytecode(encodeBytecode);

  if (aRequest->GetScriptLoadContext()->mCompileOrDecodeTask) {
    // Off-main-thread parsing.
    LOG(
        ("ScriptLoadRequest (%p): instantiate off-thread result and "
         "Execute",
         aRequest));
    MOZ_ASSERT(aRequest->IsTextSource());
    rv = aExec.JoinOffThread(aRequest->GetScriptLoadContext());
  } else {
    // Main thread parsing (inline and small scripts)
    LOG(("ScriptLoadRequest (%p): Compile And Exec", aRequest));
    MOZ_ASSERT(aRequest->IsTextSource());
    MaybeSourceText maybeSource;
    rv = aRequest->GetScriptSource(aCx, &maybeSource,
                                   aRequest->mLoadContext.get());
    if (NS_SUCCEEDED(rv)) {
      AUTO_PROFILER_MARKER_TEXT("ScriptCompileMainThread", JS,
                                MarkerInnerWindowIdFromJSContext(aCx),
                                profilerLabelString);

      auto compile = [&](auto& source) { return aExec.Compile(source); };

      MOZ_ASSERT(!maybeSource.empty());
      TimeStamp startTime = TimeStamp::Now();
      rv = maybeSource.mapNonEmpty(compile);
      mMainThreadParseTime += TimeStamp::Now() - startTime;
    }
  }
  return rv;
}

/* static */
nsCString& ScriptLoader::BytecodeMimeTypeFor(ScriptLoadRequest* aRequest) {
  if (aRequest->IsModuleRequest()) {
    return nsContentUtils::JSModuleBytecodeMimeType();
  }
  return nsContentUtils::JSScriptBytecodeMimeType();
}

void ScriptLoader::MaybePrepareForBytecodeEncodingBeforeExecute(
    ScriptLoadRequest* aRequest, JS::Handle<JSScript*> aScript) {
  if (!ShouldCacheBytecode(aRequest)) {
    return;
  }

  aRequest->MarkForBytecodeEncoding(aScript);
}

nsresult ScriptLoader::MaybePrepareForBytecodeEncodingAfterExecute(
    ScriptLoadRequest* aRequest, nsresult aRv) {
  if (aRequest->IsMarkedForBytecodeEncoding()) {
    TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                   "scriptloader_encode");
    // Check that the TranscodeBuffer which is going to receive the encoded
    // bytecode only contains the SRI, and nothing more.
    //
    // NOTE: This assertion will fail once we start encoding more data after the
    //       first encode.
    MOZ_ASSERT(aRequest->GetSRILength() == aRequest->SRIAndBytecode().length());
    RegisterForBytecodeEncoding(aRequest);
    MOZ_ASSERT(IsAlreadyHandledForBytecodeEncodingPreparation(aRequest));

    return aRv;
  }

  LOG(("ScriptLoadRequest (%p): Bytecode-cache: disabled (rv = %X)", aRequest,
       unsigned(aRv)));
  TRACE_FOR_TEST_NONE(aRequest->GetScriptLoadContext()->GetScriptElement(),
                      "scriptloader_no_encode");
  aRequest->mCacheInfo = nullptr;
  MOZ_ASSERT(IsAlreadyHandledForBytecodeEncodingPreparation(aRequest));

  return aRv;
}

bool ScriptLoader::IsAlreadyHandledForBytecodeEncodingPreparation(
    ScriptLoadRequest* aRequest) {
  return aRequest->isInList() || !aRequest->mCacheInfo;
}

void ScriptLoader::MaybePrepareModuleForBytecodeEncodingBeforeExecute(
    JSContext* aCx, ModuleLoadRequest* aRequest) {
  {
    ModuleScript* moduleScript = aRequest->mModuleScript;
    JS::Rooted<JSObject*> module(aCx, moduleScript->ModuleRecord());

    if (aRequest->IsMarkedForBytecodeEncoding()) {
      // This module is imported multiple times, and already marked.
      return;
    }

    if (ShouldCacheBytecode(aRequest)) {
      aRequest->MarkModuleForBytecodeEncoding();
    }
  }

  for (ModuleLoadRequest* childRequest : aRequest->mImports) {
    MaybePrepareModuleForBytecodeEncodingBeforeExecute(aCx, childRequest);
  }
}

nsresult ScriptLoader::MaybePrepareModuleForBytecodeEncodingAfterExecute(
    ModuleLoadRequest* aRequest, nsresult aRv) {
  if (IsAlreadyHandledForBytecodeEncodingPreparation(aRequest)) {
    // This module is imported multiple times and already handled.
    return aRv;
  }

  aRv = MaybePrepareForBytecodeEncodingAfterExecute(aRequest, aRv);

  for (ModuleLoadRequest* childRequest : aRequest->mImports) {
    aRv = MaybePrepareModuleForBytecodeEncodingAfterExecute(childRequest, aRv);
  }

  return aRv;
}

nsresult ScriptLoader::EvaluateScript(nsIGlobalObject* aGlobalObject,
                                      ScriptLoadRequest* aRequest) {
  nsAutoMicroTask mt;
  AutoEntryScript aes(aGlobalObject, "EvaluateScript", true);
  JSContext* cx = aes.cx();

  nsAutoCString profilerLabelString;
  aRequest->GetScriptLoadContext()->GetProfilerLabel(profilerLabelString);

  // Create a ClassicScript object and associate it with the JSScript.
  MOZ_ASSERT(aRequest->mLoadedScript->IsClassicScript());
  MOZ_ASSERT(aRequest->mLoadedScript->GetFetchOptions() ==
             aRequest->mFetchOptions);
  MOZ_ASSERT(aRequest->mLoadedScript->GetURI() == aRequest->mURI);
  aRequest->mLoadedScript->SetBaseURL(aRequest->mBaseURL);
  RefPtr<ClassicScript> classicScript =
      aRequest->mLoadedScript->AsClassicScript();
  JS::Rooted<JS::Value> classicScriptValue(cx, JS::PrivateValue(classicScript));

  JS::CompileOptions options(cx);
  JS::Rooted<JSScript*> introductionScript(cx);
  nsresult rv =
      FillCompileOptionsForRequest(cx, aRequest, &options, &introductionScript);

  if (NS_FAILED(rv)) {
    return rv;
  }

  // Apply the delazify strategy if the script is small.
  if (aRequest->IsTextSource() &&
      aRequest->ScriptTextLength() < OffThreadMinimumTextLength &&
      ShouldApplyDelazifyStrategy(aRequest)) {
    ApplyDelazifyStrategy(&options);
    mTotalFullParseSize +=
        aRequest->ScriptTextLength() > 0
            ? static_cast<uint32_t>(aRequest->ScriptTextLength())
            : 0;

    LOG(
        ("ScriptLoadRequest (%p): non-on-demand-only (non-omt) Parsing Enabled "
         "for url=%s mTotalFullParseSize=%u",
         aRequest, aRequest->mURI->GetSpecOrDefault().get(),
         mTotalFullParseSize));
  }

  TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                 "scriptloader_execute");
  JS::Rooted<JSObject*> global(cx, aGlobalObject->GetGlobalJSObject());
  JSExecutionContext exec(cx, global, options, classicScriptValue,
                          introductionScript);

  rv = CompileOrDecodeClassicScript(cx, exec, aRequest);

  if (NS_FAILED(rv)) {
    return rv;
  }

  // TODO (yulia): rewrite this section. rv can be a failing pattern other than
  // NS_OK which will pass the NS_FAILED check above. If we call exec.GetScript
  // in that case, it will crash.
  if (rv == NS_OK) {
    JS::Rooted<JSScript*> script(cx, exec.GetScript());
    MaybePrepareForBytecodeEncodingBeforeExecute(aRequest, script);

    {
      LOG(("ScriptLoadRequest (%p): Evaluate Script", aRequest));
      AUTO_PROFILER_MARKER_TEXT("ScriptExecution", JS,
                                MarkerInnerWindowIdFromJSContext(cx),
                                profilerLabelString);

      rv = ExecuteCompiledScript(cx, exec, classicScript);
    }
  }

  // This must be called also for compilation failure case, in order to
  // dispatch test-only event.
  rv = MaybePrepareForBytecodeEncodingAfterExecute(aRequest, rv);

  // Even if we are not saving the bytecode of the current script, we have
  // to trigger the encoding of the bytecode, as the current script can
  // call functions of a script for which we are recording the bytecode.
  LOG(("ScriptLoadRequest (%p): ScriptLoader = %p", aRequest, this));
  MaybeTriggerBytecodeEncoding();

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
  MOZ_ASSERT(aRequest->IsMarkedForBytecodeEncoding());
  MOZ_DIAGNOSTIC_ASSERT(!aRequest->isInList());
  mBytecodeEncodingQueue.AppendElement(aRequest);
}

void ScriptLoader::LoadEventFired() {
  mLoadEventFired = true;
  MaybeTriggerBytecodeEncoding();

  if (!mMainThreadParseTime.IsZero()) {
    Telemetry::Accumulate(
        Telemetry::JS_PAGELOAD_PARSE_MS,
        static_cast<uint32_t>(mMainThreadParseTime.ToMilliseconds()));
  }
}

void ScriptLoader::Destroy() {
  if (mShutdownObserver) {
    mShutdownObserver->Unregister();
    mShutdownObserver = nullptr;
  }

  CancelAndClearScriptLoadRequests();
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
  nsCOMPtr<nsIScriptGlobalObject> globalObject = GetScriptGlobalObject();
  if (!globalObject) {
    GiveUpBytecodeEncoding();
    return;
  }

  nsCOMPtr<nsIScriptContext> context = globalObject->GetScriptContext();
  if (!context) {
    GiveUpBytecodeEncoding();
    return;
  }

  AutoEntryScript aes(globalObject, "encode bytecode", true);
  RefPtr<ScriptLoadRequest> request;
  while (!mBytecodeEncodingQueue.isEmpty()) {
    request = mBytecodeEncodingQueue.StealFirst();
    MOZ_ASSERT(!IsWebExtensionRequest(request),
               "Bytecode for web extension content scrips is not cached");
    EncodeRequestBytecode(aes.cx(), request);
    request->DropBytecode();
    request->DropBytecodeCacheReferences();
  }
}

void ScriptLoader::EncodeRequestBytecode(JSContext* aCx,
                                         ScriptLoadRequest* aRequest) {
  using namespace mozilla::Telemetry;
  nsresult rv = NS_OK;
  MOZ_ASSERT(aRequest->mCacheInfo);
  auto bytecodeFailed = mozilla::MakeScopeExit([&]() {
    TRACE_FOR_TEST_NONE(aRequest->GetScriptLoadContext()->GetScriptElement(),
                        "scriptloader_bytecode_failed");
  });

  bool result;
  if (aRequest->IsModuleRequest()) {
    ModuleScript* moduleScript = aRequest->AsModuleRequest()->mModuleScript;
    JS::Rooted<JSObject*> module(aCx, moduleScript->ModuleRecord());
    result =
        JS::FinishIncrementalEncoding(aCx, module, aRequest->SRIAndBytecode());
  } else {
    JS::Rooted<JSScript*> script(aCx, aRequest->mScriptForBytecodeEncoding);
    result =
        JS::FinishIncrementalEncoding(aCx, script, aRequest->SRIAndBytecode());
  }
  if (!result) {
    // Encoding can be aborted for non-supported syntax (e.g. asm.js), or
    // any other internal error.
    // We don't care the error and just give up encoding.
    JS_ClearPendingException(aCx);

    LOG(("ScriptLoadRequest (%p): Cannot serialize bytecode", aRequest));
    return;
  }

  Vector<uint8_t> compressedBytecode;
  // TODO probably need to move this to a helper thread
  if (!ScriptBytecodeCompress(aRequest->SRIAndBytecode(),
                              aRequest->GetSRILength(), compressedBytecode)) {
    return;
  }

  if (compressedBytecode.length() >= UINT32_MAX) {
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
      BytecodeMimeTypeFor(aRequest),
      static_cast<int64_t>(compressedBytecode.length()),
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
  rv = output->Write(reinterpret_cast<char*>(compressedBytecode.begin()),
                     compressedBytecode.length(), &n);
  LOG(
      ("ScriptLoadRequest (%p): Write bytecode cache (rv = %X, length = %u, "
       "written = %u)",
       aRequest, unsigned(rv), unsigned(compressedBytecode.length()), n));
  if (NS_FAILED(rv)) {
    return;
  }

  MOZ_RELEASE_ASSERT(compressedBytecode.length() == n);

  bytecodeFailed.release();
  TRACE_FOR_TEST_NONE(aRequest->GetScriptLoadContext()->GetScriptElement(),
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
  nsCOMPtr<nsIScriptGlobalObject> globalObject = GetScriptGlobalObject();
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
    TRACE_FOR_TEST_NONE(request->GetScriptLoadContext()->GetScriptElement(),
                        "scriptloader_bytecode_failed");
    MOZ_ASSERT(!IsWebExtensionRequest(request));

    if (aes.isSome()) {
      if (request->IsModuleRequest()) {
        ModuleScript* moduleScript = request->AsModuleRequest()->mModuleScript;
        JS::Rooted<JSObject*> module(aes->cx(), moduleScript->ModuleRecord());
        JS::AbortIncrementalEncoding(module);
      } else {
        JS::Rooted<JSScript*> script(aes->cx(),
                                     request->mScriptForBytecodeEncoding);
        JS::AbortIncrementalEncoding(script);
      }
    }

    request->DropBytecode();
    request->DropBytecodeCacheReferences();
  }
}

bool ScriptLoader::HasPendingRequests() const {
  return mParserBlockingRequest || !mXSLTRequests.isEmpty() ||
         !mLoadedAsyncRequests.isEmpty() ||
         !mNonAsyncExternalScriptInsertedRequests.isEmpty() ||
         !mDeferRequests.isEmpty() || HasPendingDynamicImports() ||
         !mPendingChildLoaders.IsEmpty();
  // mOffThreadCompilingRequests are already being processed.
}

bool ScriptLoader::HasPendingDynamicImports() const {
  if (mModuleLoader && mModuleLoader->HasPendingDynamicImports()) {
    return true;
  }

  for (ModuleLoader* loader : mWebExtModuleLoaders) {
    if (loader->HasPendingDynamicImports()) {
      return true;
    }
  }

  for (ModuleLoader* loader : mShadowRealmModuleLoaders) {
    if (loader->HasPendingDynamicImports()) {
      return true;
    }
  }

  return false;
}

void ScriptLoader::ProcessPendingRequestsAsync() {
  if (HasPendingRequests()) {
    nsCOMPtr<nsIRunnable> task =
        NewRunnableMethod("dom::ScriptLoader::ProcessPendingRequests", this,
                          &ScriptLoader::ProcessPendingRequests);
    if (mDocument) {
      mDocument->Dispatch(task.forget());
    } else {
      NS_DispatchToCurrentThread(task.forget());
    }
  }
}

void ScriptLoader::ProcessPendingRequests() {
  RefPtr<ScriptLoadRequest> request;

  if (mParserBlockingRequest && mParserBlockingRequest->IsFinished() &&
      ReadyToExecuteParserBlockingScripts()) {
    request.swap(mParserBlockingRequest);
    UnblockParser(request);
    ProcessRequest(request);
    ContinueParserAsync(request);
  }

  while (ReadyToExecuteParserBlockingScripts() && !mXSLTRequests.isEmpty() &&
         mXSLTRequests.getFirst()->IsFinished()) {
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
         mNonAsyncExternalScriptInsertedRequests.getFirst()->IsFinished()) {
    // Violate the HTML5 spec and execute these in the insertion order in
    // order to make LABjs and the "order" plug-in for RequireJS work with
    // their Gecko-sniffed code path. See
    // http://lists.w3.org/Archives/Public/public-html/2010Oct/0088.html
    request = mNonAsyncExternalScriptInsertedRequests.StealFirst();
    ProcessRequest(request);
  }

  if (mDeferCheckpointReached && mXSLTRequests.isEmpty()) {
    while (ReadyToExecuteScripts() && !mDeferRequests.isEmpty() &&
           mDeferRequests.getFirst()->IsFinished()) {
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
  std::tie(encoding, std::ignore) = Encoding::ForBOM(data);
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
nsresult ScriptLoader::ConvertToUTF16(
    nsIChannel* aChannel, const uint8_t* aData, uint32_t aLength,
    const nsAString& aHintCharset, Document* aDocument,
    UniquePtr<char16_t[], JS::FreePolicy>& aBufOut, size_t& aLengthOut) {
  char16_t* bufOut;
  nsresult rv = ConvertToUnicode(aChannel, aData, aLength, aHintCharset,
                                 aDocument, bufOut, aLengthOut);
  if (NS_SUCCEEDED(rv)) {
    aBufOut.reset(bufOut);
  }
  return rv;
}

/* static */
nsresult ScriptLoader::ConvertToUTF8(
    nsIChannel* aChannel, const uint8_t* aData, uint32_t aLength,
    const nsAString& aHintCharset, Document* aDocument,
    UniquePtr<Utf8Unit[], JS::FreePolicy>& aBufOut, size_t& aLengthOut) {
  Utf8Unit* bufOut;
  nsresult rv = ConvertToUnicode(aChannel, aData, aLength, aHintCharset,
                                 aDocument, bufOut, aLengthOut);
  if (NS_SUCCEEDED(rv)) {
    aBufOut.reset(bufOut);
  }
  return rv;
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
      JS::TranscodeBuffer& bytecode = aRequest->SRIAndBytecode();
      MOZ_ASSERT_IF(NS_SUCCEEDED(rv), bytecode.length() == sriLength);

      // TODO: (Bug 1800896) This code should be moved into SaveSRIHash, and the
      // SRI out-param can be removed.
      aRequest->SetSRILength(sriLength);
      if (aRequest->GetSRILength() != sriLength) {
        // The bytecode is aligned in the bytecode buffer, and space might be
        // reserved for padding after the SRI hash.
        if (!bytecode.resize(aRequest->GetSRILength())) {
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
  JS::TranscodeBuffer& bytecode = aRequest->SRIAndBytecode();
  MOZ_ASSERT(bytecode.empty());

  uint32_t len;

  // If the integrity metadata does not correspond to a valid hash function,
  // IsComplete would be false.
  if (!aRequest->mIntegrity.IsEmpty() && aSRIDataVerifier->IsComplete()) {
    MOZ_ASSERT(bytecode.length() == 0);

    // Encode the SRI computed hash.
    len = aSRIDataVerifier->DataSummaryLength();

    if (!bytecode.resize(len)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    DebugOnly<nsresult> res =
        aSRIDataVerifier->ExportDataSummary(len, bytecode.begin());
    MOZ_ASSERT(NS_SUCCEEDED(res));
  } else {
    MOZ_ASSERT(bytecode.length() == 0);

    // Encode a dummy SRI hash.
    len = SRICheckDataVerifier::EmptyDataSummaryLength();

    if (!bytecode.resize(len)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    DebugOnly<nsresult> res =
        SRICheckDataVerifier::ExportEmptyDataSummary(len, bytecode.begin());
    MOZ_ASSERT(NS_SUCCEEDED(res));
  }

  // Verify that the exported and predicted length correspond.
  DebugOnly<uint32_t> srilen{};
  MOZ_ASSERT(NS_SUCCEEDED(
      SRICheckDataVerifier::DataSummaryLength(len, bytecode.begin(), &srilen)));
  MOZ_ASSERT(srilen == len);

  *sriLength = len;

  return NS_OK;
}

void ScriptLoader::ReportErrorToConsole(ScriptLoadRequest* aRequest,
                                        nsresult aResult) const {
  MOZ_ASSERT(aRequest);

  if (aRequest->GetScriptLoadContext()->IsPreload()) {
    // Skip reporting errors in preload requests. If the request is actually
    // used then we will report the error in ReportPreloadErrorsToConsole below.
    aRequest->GetScriptLoadContext()->mUnreportedPreloadError = aResult;
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

  nsIScriptElement* element =
      aRequest->GetScriptLoadContext()->GetScriptElement();
  uint32_t lineNo = element ? element->GetScriptLineNumber() : 0;
  JS::ColumnNumberOneOrigin columnNo;
  if (element) {
    columnNo = element->GetScriptColumnNumber();
  }

  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, "Script Loader"_ns, mDocument,
      nsContentUtils::eDOM_PROPERTIES, message, params, nullptr, u""_ns, lineNo,
      columnNo.oneOriginValue());
}

void ScriptLoader::ReportWarningToConsole(
    ScriptLoadRequest* aRequest, const char* aMessageName,
    const nsTArray<nsString>& aParams) const {
  nsIScriptElement* element =
      aRequest->GetScriptLoadContext()->GetScriptElement();
  uint32_t lineNo = element ? element->GetScriptLineNumber() : 0;
  JS::ColumnNumberOneOrigin columnNo;
  if (element) {
    columnNo = element->GetScriptColumnNumber();
  }
  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, "Script Loader"_ns, mDocument,
      nsContentUtils::eDOM_PROPERTIES, aMessageName, aParams, nullptr, u""_ns,
      lineNo, columnNo.oneOriginValue());
}

void ScriptLoader::ReportPreloadErrorsToConsole(ScriptLoadRequest* aRequest) {
  if (NS_FAILED(aRequest->GetScriptLoadContext()->mUnreportedPreloadError)) {
    ReportErrorToConsole(
        aRequest, aRequest->GetScriptLoadContext()->mUnreportedPreloadError);
    aRequest->GetScriptLoadContext()->mUnreportedPreloadError = NS_OK;
  }

  if (aRequest->IsModuleRequest()) {
    for (const auto& childRequest : aRequest->AsModuleRequest()->mImports) {
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
    nsCOMPtr<nsIContent> cont =
        do_QueryInterface(aRequest->GetScriptLoadContext()->GetScriptElement());
    mDocument->AddBlockedNodeByClassifier(cont);
  }

  if (aRequest->IsModuleRequest()) {
    MOZ_ASSERT(!aRequest->GetScriptLoadContext()->mIsInline);
    aRequest->AsModuleRequest()->OnFetchComplete(aResult);
  }

  if (aRequest->GetScriptLoadContext()->mInDeferList) {
    MOZ_ASSERT_IF(aRequest->IsModuleRequest(),
                  aRequest->AsModuleRequest()->IsTopLevel());
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mDeferRequests.Steal(aRequest);
      FireScriptAvailable(aResult, req);
    }
  } else if (aRequest->GetScriptLoadContext()->mInAsyncList) {
    MOZ_ASSERT_IF(aRequest->IsModuleRequest(),
                  aRequest->AsModuleRequest()->IsTopLevel());
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mLoadingAsyncRequests.Steal(aRequest);
      FireScriptAvailable(aResult, req);
    }
  } else if (aRequest->GetScriptLoadContext()->mIsNonAsyncScriptInserted) {
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req =
          mNonAsyncExternalScriptInsertedRequests.Steal(aRequest);
      FireScriptAvailable(aResult, req);
    }
  } else if (aRequest->GetScriptLoadContext()->mIsXSLT) {
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mXSLTRequests.Steal(aRequest);
      FireScriptAvailable(aResult, req);
    }
  } else if (aRequest->GetScriptLoadContext()->IsPreload()) {
    if (aRequest->IsModuleRequest()) {
      aRequest->Cancel();
    }
    if (aRequest->IsTopLevel()) {
      // Request may already have been removed by
      // CancelAndClearScriptLoadRequests.
      mPreloads.RemoveElement(aRequest, PreloadRequestComparator());
    }
    MOZ_ASSERT(!aRequest->isInList());
    AccumulateCategorical(LABELS_DOM_SCRIPT_PRELOAD_RESULT::LoadError);
  } else if (aRequest->IsModuleRequest()) {
    ModuleLoadRequest* modReq = aRequest->AsModuleRequest();
    if (modReq->IsDynamicImport()) {
      MOZ_ASSERT(modReq->IsTopLevel());
      if (aRequest->isInList()) {
        modReq->CancelDynamicImport(aResult);
      }
    } else {
      MOZ_ASSERT(!modReq->isInList());
      modReq->Cancel();
    }
  } else if (mParserBlockingRequest == aRequest) {
    MOZ_ASSERT(!aRequest->isInList());
    mParserBlockingRequest = nullptr;
    UnblockParser(aRequest);

    // Ensure that we treat aRequest->GetScriptLoadContext()->GetScriptElement()
    // as our current parser-inserted script while firing onerror on it.
    MOZ_ASSERT(aRequest->GetScriptLoadContext()
                   ->GetScriptElement()
                   ->GetParserCreated());
    nsCOMPtr<nsIScriptElement> oldParserInsertedScript =
        mCurrentParserInsertedScript;
    mCurrentParserInsertedScript =
        aRequest->GetScriptLoadContext()->GetScriptElement();
    FireScriptAvailable(aResult, aRequest);
    ContinueParserAsync(aRequest);
    mCurrentParserInsertedScript = oldParserInsertedScript;
  } else {
    // This happens for blocking requests cancelled by ParsingComplete().
    // Ignore cancellation status for link-preload requests, as cancellation can
    // be omitted for them when SRI is stronger on consumer tags.
    MOZ_ASSERT(aRequest->IsCanceled() ||
               aRequest->GetScriptLoadContext()->IsLinkPreloadScript());
    MOZ_ASSERT(!aRequest->isInList());
  }
}

void ScriptLoader::UnblockParser(ScriptLoadRequest* aParserBlockingRequest) {
  aParserBlockingRequest->GetScriptLoadContext()
      ->GetScriptElement()
      ->UnblockParser();
}

void ScriptLoader::ContinueParserAsync(
    ScriptLoadRequest* aParserBlockingRequest) {
  aParserBlockingRequest->GetScriptLoadContext()
      ->GetScriptElement()
      ->ContinueParserAsync();
}

uint32_t ScriptLoader::NumberOfProcessors() {
  if (mNumberOfProcessors > 0) {
    return mNumberOfProcessors;
  }

  int32_t numProcs = PR_GetNumberOfProcessors();
  if (numProcs > 0) {
    mNumberOfProcessors = numProcs;
  }
  return mNumberOfProcessors;
}

int32_t ScriptLoader::PhysicalSizeOfMemoryInGB() {
  // 0 is a valid result from PR_GetPhysicalMemorySize() which
  // means a failure occured.
  if (mPhysicalSizeOfMemory >= 0) {
    return mPhysicalSizeOfMemory;
  }

  // Save the size in GB.
  mPhysicalSizeOfMemory =
      static_cast<int32_t>(PR_GetPhysicalMemorySize() >> 30);
  return mPhysicalSizeOfMemory;
}

static bool IsInternalURIScheme(nsIURI* uri) {
  return uri->SchemeIs("moz-extension") || uri->SchemeIs("resource") ||
         uri->SchemeIs("chrome");
}

bool ScriptLoader::ShouldApplyDelazifyStrategy(ScriptLoadRequest* aRequest) {
  // Full parse everything if negative.
  if (StaticPrefs::dom_script_loader_delazification_max_size() < 0) {
    return true;
  }

  // Be conservative on machines with 2GB or less of memory.
  if (PhysicalSizeOfMemoryInGB() <=
      StaticPrefs::dom_script_loader_delazification_min_mem()) {
    return false;
  }

  uint32_t max_size = static_cast<uint32_t>(
      StaticPrefs::dom_script_loader_delazification_max_size());
  uint32_t script_size =
      aRequest->ScriptTextLength() > 0
          ? static_cast<uint32_t>(aRequest->ScriptTextLength())
          : 0;

  if (mTotalFullParseSize + script_size < max_size) {
    return true;
  }

  if (LOG_ENABLED()) {
    nsCString url = aRequest->mURI->GetSpecOrDefault();
    LOG(
        ("ScriptLoadRequest (%p): non-on-demand-only Parsing Disabled for (%s) "
         "with size=%u because mTotalFullParseSize=%u would exceed max_size=%u",
         aRequest, url.get(), script_size, mTotalFullParseSize, max_size));
  }

  return false;
}

void ScriptLoader::ApplyDelazifyStrategy(JS::CompileOptions* aOptions) {
  JS::DelazificationOption strategy =
      JS::DelazificationOption::ParseEverythingEagerly;
  uint32_t strategyIndex =
      StaticPrefs::dom_script_loader_delazification_strategy();

  // Assert that all enumerated values of DelazificationOption are dense between
  // OnDemandOnly and ParseEverythingEagerly.
#ifdef DEBUG
  uint32_t count = 0;
  uint32_t mask = 0;
#  define _COUNT_ENTRIES(Name) count++;
#  define _MASK_ENTRIES(Name) \
    mask |= 1 << uint32_t(JS::DelazificationOption::Name);

  FOREACH_DELAZIFICATION_STRATEGY(_COUNT_ENTRIES);
  MOZ_ASSERT(count == uint32_t(strategy) + 1);
  FOREACH_DELAZIFICATION_STRATEGY(_MASK_ENTRIES);
  MOZ_ASSERT(((mask + 1) & mask) == 0);
#  undef _COUNT_ENTRIES
#  undef _MASK_ENTRIES
#endif

  // Any strategy index larger than ParseEverythingEagerly would default to
  // ParseEverythingEagerly.
  if (strategyIndex <= uint32_t(strategy)) {
    strategy = JS::DelazificationOption(uint8_t(strategyIndex));
  }

  aOptions->setEagerDelazificationStrategy(strategy);
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
    if (aRequest->GetScriptLoadContext()->mIsNonAsyncScriptInserted &&
        !StaticPrefs::
            dom_script_loader_external_scripts_speculate_non_parser_inserted_enabled()) {
      return false;
    }

    // Async and link preload scripts do not need to be parsed right away.
    if (aRequest->GetScriptLoadContext()->IsAsyncScript() &&
        !StaticPrefs::
            dom_script_loader_external_scripts_speculate_async_enabled()) {
      return false;
    }

    if (aRequest->GetScriptLoadContext()->IsLinkPreloadScript() &&
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
  MOZ_ASSERT(aRequest->IsFetching());
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

    if (aRequest->IsModuleRequest()) {
      // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
      // Update script's referrer-policy if there's a Referrer-Policy header in
      // the HTTP response.
      ReferrerPolicy policy =
          nsContentUtils::GetReferrerPolicyFromChannel(httpChannel);
      if (policy != ReferrerPolicy::_empty) {
        aRequest->UpdateReferrerPolicy(policy);
      }
    }

    nsAutoCString sourceMapURL;
    if (nsContentUtils::GetSourceMapURL(httpChannel, sourceMapURL)) {
      aRequest->mSourceMapURL = Some(NS_ConvertUTF8toUTF16(sourceMapURL));
    }

    nsCOMPtr<nsIClassifiedChannel> classifiedChannel = do_QueryInterface(req);
    MOZ_ASSERT(classifiedChannel);
    if (classifiedChannel &&
        classifiedChannel->IsThirdPartyTrackingResource()) {
      aRequest->GetScriptLoadContext()->SetIsTracking();
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
                   (aRequest->IsModuleRequest() &&
                    (aRequest->AsModuleRequest()->IsRegisteredDynamicImport() ||
                     !aRequest->AsModuleRequest()->IsTopLevel())) ||
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
    rv = AttemptOffThreadScriptCompile(request, &couldCompile);
    NS_ENSURE_SUCCESS(rv, rv);
    if (couldCompile) {
      return NS_OK;
    }

    // Otherwise compile it right away and start fetching descendents.
    return request->OnFetchComplete(NS_OK);
  }

  // The script is now loaded and ready to run.
  aRequest->SetReady();

  // If speculative parsing is enabled attempt to compile all
  // external scripts off-main-thread.  Otherwise, only omt compile scripts
  // blocking the parser.
  if (ShouldCompileOffThread(aRequest)) {
    MOZ_ASSERT(!aRequest->IsModuleRequest());
    bool couldCompile = false;
    nsresult rv = AttemptOffThreadScriptCompile(aRequest, &couldCompile);
    NS_ENSURE_SUCCESS(rv, rv);
    if (couldCompile) {
      MOZ_ASSERT(aRequest->mState == ScriptLoadRequest::State::Compiling,
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
  if (aTerminated) {
    CancelAndClearScriptLoadRequests();

    // Have to call this even if aTerminated so we'll correctly unblock onload.
    DeferCheckpointReached();
  }
}

void ScriptLoader::PreloadURI(
    nsIURI* aURI, const nsAString& aCharset, const nsAString& aType,
    const nsAString& aCrossOrigin, const nsAString& aNonce,
    const nsAString& aFetchPriority, const nsAString& aIntegrity,
    bool aScriptFromHead, bool aAsync, bool aDefer, bool aLinkPreload,
    const ReferrerPolicy aReferrerPolicy, uint64_t aEarlyHintPreloaderId) {
  NS_ENSURE_TRUE_VOID(mDocument);
  // Check to see if scripts has been turned off.
  if (!mEnabled || !mDocument->IsScriptEnabled()) {
    return;
  }

  ScriptKind scriptKind = ScriptKind::eClassic;

  static const char kASCIIWhitespace[] = "\t\n\f\r ";

  nsAutoString type(aType);
  type.Trim(kASCIIWhitespace);
  if (type.LowerCaseEqualsASCII("module")) {
    scriptKind = ScriptKind::eModule;
  }

  if (scriptKind == ScriptKind::eClassic && !aType.IsEmpty() &&
      !nsContentUtils::IsJavascriptMIMEType(aType)) {
    // Unknown type.  Don't load it.
    return;
  }

  SRIMetadata sriMetadata;
  GetSRIMetadata(aIntegrity, &sriMetadata);

  const auto requestPriority = FetchPriorityToRequestPriority(
      nsGenericHTMLElement::ToFetchPriority(aFetchPriority));

  // For link type "modulepreload":
  // https://html.spec.whatwg.org/multipage/links.html#link-type-modulepreload
  // Step 11. Let options be a script fetch options whose cryptographic nonce is
  // cryptographic nonce, integrity metadata is integrity metadata, parser
  // metadata is "not-parser-inserted", credentials mode is credentials mode,
  // referrer policy is referrer policy, and fetch priority is fetch priority.
  //
  // We treat speculative <script> loads as parser-inserted, because they
  // come from a parser. This will also match how they should be treated
  // as a normal load.
  RefPtr<ScriptLoadRequest> request =
      CreateLoadRequest(scriptKind, aURI, nullptr, mDocument->NodePrincipal(),
                        Element::StringToCORSMode(aCrossOrigin), aNonce,
                        requestPriority, sriMetadata, aReferrerPolicy,
                        aLinkPreload ? ParserMetadata::NotParserInserted
                                     : ParserMetadata::ParserInserted);
  request->GetScriptLoadContext()->mIsInline = false;
  request->GetScriptLoadContext()->mScriptFromHead = aScriptFromHead;
  request->GetScriptLoadContext()->SetScriptMode(aDefer, aAsync, aLinkPreload);
  request->GetScriptLoadContext()->SetIsPreloadRequest();
  request->mEarlyHintPreloaderId = aEarlyHintPreloaderId;

  if (LOG_ENABLED()) {
    nsAutoCString url;
    aURI->GetAsciiSpec(url);
    LOG(("ScriptLoadRequest (%p): Created preload request for %s",
         request.get(), url.get()));
  }

  nsAutoString charset(aCharset);
  nsresult rv = StartLoad(request, Some(charset));
  if (NS_FAILED(rv)) {
    return;
  }

  PreloadInfo* pi = mPreloads.AppendElement();
  pi->mRequest = request;
  pi->mCharset = aCharset;
}

void ScriptLoader::AddDeferRequest(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->GetScriptLoadContext()->IsDeferredScript());
  MOZ_ASSERT(!aRequest->GetScriptLoadContext()->mInDeferList &&
             !aRequest->GetScriptLoadContext()->mInAsyncList);
  MOZ_ASSERT(!aRequest->GetScriptLoadContext()->mInCompilingList);

  aRequest->GetScriptLoadContext()->mInDeferList = true;
  mDeferRequests.AppendElement(aRequest);
  if (mDeferEnabled && aRequest == mDeferRequests.getFirst() && mDocument &&
      !mBlockingDOMContentLoaded) {
    MOZ_ASSERT(mDocument->GetReadyStateEnum() == Document::READYSTATE_LOADING);
    mBlockingDOMContentLoaded = true;
    mDocument->BlockDOMContentLoaded();
  }
}

void ScriptLoader::AddAsyncRequest(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->GetScriptLoadContext()->IsAsyncScript());
  MOZ_ASSERT(!aRequest->GetScriptLoadContext()->mInDeferList &&
             !aRequest->GetScriptLoadContext()->mInAsyncList);
  MOZ_ASSERT(!aRequest->GetScriptLoadContext()->mInCompilingList);

  aRequest->GetScriptLoadContext()->mInAsyncList = true;
  if (aRequest->IsFinished()) {
    mLoadedAsyncRequests.AppendElement(aRequest);
  } else {
    mLoadingAsyncRequests.AppendElement(aRequest);
  }
}

void ScriptLoader::MaybeMoveToLoadedList(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsFinished());
  MOZ_ASSERT(aRequest->IsTopLevel());

  // If it's async, move it to the loaded list.
  // aRequest->GetScriptLoadContext()->mInAsyncList really _should_ be in a
  // list, but the consequences if it's not are bad enough we want to avoid
  // trying to move it if it's not.
  if (aRequest->GetScriptLoadContext()->mInAsyncList) {
    MOZ_ASSERT(aRequest->isInList());
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mLoadingAsyncRequests.Steal(aRequest);
      mLoadedAsyncRequests.AppendElement(req);
    }
  } else if (aRequest->IsModuleRequest() &&
             aRequest->AsModuleRequest()->IsDynamicImport()) {
    // Process dynamic imports with async scripts.
    MOZ_ASSERT(!aRequest->isInList());
    mLoadedAsyncRequests.AppendElement(aRequest);
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

}  // namespace mozilla::dom
