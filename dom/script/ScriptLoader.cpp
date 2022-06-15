/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoader.h"
#include "ScriptLoadHandler.h"
#include "ScriptTrace.h"
#include "ModuleLoader.h"

#include "zlib.h"

#include "prsystem.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"  // JS::GetArrayLength
#include "js/CompilationAndEvaluation.h"
#include "js/ContextOptions.h"        // JS::ContextOptionsRef
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "js/loader/ScriptLoadRequest.h"
#include "ScriptCompression.h"
#include "js/loader/LoadedScript.h"
#include "js/loader/ModuleLoadRequest.h"
#include "js/MemoryFunctions.h"
#include "js/Modules.h"
#include "js/OffThreadScriptCompilation.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty
#include "js/Realm.h"
#include "js/SourceText.h"
#include "js/Transcoding.h"
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
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_javascript.h"
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
                         mWebExtModuleLoaders)

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
    return;
  }

  MOZ_ASSERT(!HasPendingRequests());

  if (mModuleLoader) {
    MOZ_ASSERT(mModuleLoader->GetGlobalObject() == aGlobalObject);
    return;
  }

  // The module loader is associated with a global object, so don't create it
  // until we have a global set.
  mModuleLoader = new ModuleLoader(this, aGlobalObject, ModuleLoader::Normal);
}

void ScriptLoader::RegisterContentScriptModuleLoader(ModuleLoader* aLoader) {
  MOZ_ASSERT(aLoader);
  MOZ_ASSERT(aLoader->GetScriptLoader() == this);

  mWebExtModuleLoaders.AppendElement(aLoader);
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
  if (aRequest->GetScriptLoadContext()->IsPreload()) {
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
  NS_IMETHOD Run() override {
    if (mRequest->IsModuleRequest() &&
        mRequest->AsModuleRequest()->IsDynamicImport()) {
      mRequest->AsModuleRequest()->ProcessDynamicImport();
      return NS_OK;
    }

    return mLoader->ProcessRequest(mRequest);
  }
};

void ScriptLoader::RunScriptWhenSafe(ScriptLoadRequest* aRequest) {
  auto* runnable = new ScriptRequestProcessor(this, aRequest);
  nsContentUtils::AddScriptRunner(runnable);
}

nsresult ScriptLoader::RestartLoad(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsBytecode());
  aRequest->mScriptBytecode.clearAndFree();
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
    rv = StartLoad(aRequest);
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Close the current channel and this ScriptLoadHandler as we created a new
  // one for the same request.
  return NS_BINDING_RETARGETED;
}

nsresult ScriptLoader::StartLoad(ScriptLoadRequest* aRequest) {
  if (aRequest->IsModuleRequest()) {
    return aRequest->AsModuleRequest()->StartModuleLoad();
  }

  return StartClassicLoad(aRequest);
}

nsresult ScriptLoader::StartClassicLoad(ScriptLoadRequest* aRequest) {
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

  nsresult rv = StartLoadInternal(aRequest, securityFlags);

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

nsresult ScriptLoader::StartLoadInternal(ScriptLoadRequest* aRequest,
                                         nsSecurityFlags securityFlags) {
  nsContentPolicyType contentPolicyType =
      ScriptLoadRequestToContentPolicyType(aRequest);
  nsCOMPtr<nsINode> context;
  if (aRequest->GetScriptLoadContext()->GetScriptElement()) {
    context =
        do_QueryInterface(aRequest->GetScriptLoadContext()->GetScriptElement());
  } else {
    context = mDocument;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = mDocument->GetDocumentLoadGroup();
  nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_NULL_POINTER);
  nsIDocShell* docshell = window->GetDocShell();
  nsCOMPtr<nsIInterfaceRequestor> prompter(do_QueryInterface(docshell));

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

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobal = GetScriptGlobalObject();
  if (!scriptGlobal) {
    return NS_ERROR_FAILURE;
  }

  // To avoid decoding issues, the build-id is part of the bytecode MIME type
  // constant.
  aRequest->mCacheInfo = nullptr;
  nsCOMPtr<nsICacheInfoChannel> cic(do_QueryInterface(channel));
  if (cic && StaticPrefs::dom_script_loader_bytecode_cache_enabled()) {
    MOZ_ASSERT(!IsWebExtensionRequest(aRequest),
               "Can not bytecode cache WebExt code");
    if (!aRequest->mFetchSourceOnly) {
      // Inform the HTTP cache that we prefer to have information coming from
      // the bytecode cache instead of the sources, if such entry is already
      // registered.
      LOG(("ScriptLoadRequest (%p): Maybe request bytecode", aRequest));
      cic->PreferAlternativeDataType(
          BytecodeMimeTypeFor(aRequest), ""_ns,
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

  LOG(("ScriptLoadRequest (%p): mode=%u tracking=%d", aRequest,
       unsigned(aRequest->GetScriptLoadContext()->mScriptMode),
       aRequest->GetScriptLoadContext()->IsTracking()));

  if (aRequest->GetScriptLoadContext()->IsLinkPreloadScript()) {
    // This is <link rel="preload" as="script"> initiated speculative load,
    // put it to the group that is not blocked by leaders and doesn't block
    // follower at the same time. Giving it a much higher priority will make
    // this request be processed ahead of other Unblocked requests, but with
    // the same weight as Leaders.  This will make us behave similar way for
    // both http2 and http1.
    ScriptLoadContext::PrioritizeAsPreload(channel);
    ScriptLoadContext::AddLoadBackgroundFlag(channel);
  } else if (nsCOMPtr<nsIClassOfService> cos = do_QueryInterface(channel)) {
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
    if (aRequest->GetScriptLoadContext()->IsLinkPreloadScript()) {
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
  aRequest->GetScriptLoadContext()->NotifyOpen(
      key, channel, mDocument,
      aRequest->GetScriptLoadContext()->IsLinkPreloadScript());

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

already_AddRefed<ScriptLoadRequest> ScriptLoader::CreateLoadRequest(
    ScriptKind aKind, nsIURI* aURI, nsIScriptElement* aElement,
    nsIPrincipal* aTriggeringPrincipal, CORSMode aCORSMode,
    const SRIMetadata& aIntegrity, ReferrerPolicy aReferrerPolicy) {
  nsIURI* referrer = mDocument->GetDocumentURIAsReferrer();
  nsCOMPtr<Element> domElement = do_QueryInterface(aElement);
  RefPtr<ScriptFetchOptions> fetchOptions = new ScriptFetchOptions(
      aCORSMode, aReferrerPolicy, aTriggeringPrincipal, domElement);
  RefPtr<ScriptLoadContext> context = new ScriptLoadContext();

  if (aKind == ScriptKind::eClassic || aKind == ScriptKind::eImportMap) {
    RefPtr<ScriptLoadRequest> aRequest = new ScriptLoadRequest(
        aKind, aURI, fetchOptions, aIntegrity, referrer, context);

    return aRequest.forget();
  }

  MOZ_ASSERT(aKind == ScriptKind::eModule);
  RefPtr<ModuleLoadRequest> aRequest = ModuleLoader::CreateTopLevel(
      aURI, fetchOptions, aIntegrity, referrer, this, context);
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

  nsAutoString type;
  bool hasType = aElement->GetScriptType(type);

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
                                         const nsAutoString& aTypeAttr,
                                         nsIContent* aScriptContent) {
  LOG(("ScriptLoader (%p): Process external script for element %p", this,
       aElement));

  // Bug 1765745: Support external import maps.
  if (aScriptKind == ScriptKind::eImportMap) {
    NS_DispatchToCurrentThread(
        NewRunnableMethod("nsIScriptElement::FireErrorEvent", aElement,
                          &nsIScriptElement::FireErrorEvent));
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

    // https://wicg.github.io/import-maps/#document-acquiring-import-maps
    // If this preload request is for a module load, set acquiring import maps
    // to false.
    if (request->IsModuleRequest()) {
      LOG(("ScriptLoadRequest (%p): Set acquiring import maps to false",
           request.get()));
      mModuleLoader->SetAcquiringImportMaps(false);
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
    ReferrerPolicy referrerPolicy = GetReferrerPolicy(aElement);

    request = CreateLoadRequest(aScriptKind, scriptURI, aElement, principal,
                                ourCORSMode, sriMetadata, referrerPolicy);
    request->GetScriptLoadContext()->mIsInline = false;
    request->GetScriptLoadContext()->SetScriptMode(
        aElement->GetScriptDeferred(), aElement->GetScriptAsync(), false);
    // keep request->GetScriptLoadContext()->mScriptFromHead to false so we
    // don't treat non preloaded scripts as blockers for full page load. See bug
    // 792438.

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
  NS_ASSERTION(SpeculativeOMTParsingEnabled() ||
                   !request->GetScriptLoadContext()->CompileStarted() ||
                   request->IsModuleRequest(),
               "Request should not yet be in compiling stage.");

  if (request->GetScriptLoadContext()->IsAsyncScript()) {
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
    request->GetScriptLoadContext()->mIsNonAsyncScriptInserted = true;
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
  if (request->GetScriptLoadContext()->IsDeferredScript()) {
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
    request->GetScriptLoadContext()->mIsXSLT = true;
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
  request->GetScriptLoadContext()->mIsInline = true;
  request->GetScriptLoadContext()->mLineNo = aElement->GetScriptLineNumber();
  request->mFetchSourceOnly = true;
  request->SetTextSource();
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
    // https://wicg.github.io/import-maps/#document-acquiring-import-maps
    // Set acquiring import maps to false for inline modules.
    mModuleLoader->SetAcquiringImportMaps(false);

    ModuleLoadRequest* modReq = request->AsModuleRequest();
    if (aElement->GetParserCreated() != NOT_FROM_PARSER) {
      if (aElement->GetScriptAsync()) {
        AddAsyncRequest(modReq);
      } else {
        AddDeferRequest(modReq);
      }
    }

    {
      // We must perform a microtask checkpoint when inserting script elements
      // as specified by: https://html.spec.whatwg.org/#parsing-main-incdata
      // For the non-inline module cases this happens in ProcessRequest.
      mozilla::nsAutoMicroTask mt;
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
    // https://wicg.github.io/import-maps/#integration-prepare-a-script
    // If the script's type is "importmap":
    //
    // Step 1: If the element's node document's acquiring import maps is false,
    // then queue a task to fire an event named error at the element, and
    // return.
    if (!mModuleLoader->GetAcquiringImportMaps()) {
      NS_WARNING("ScriptLoader: acquiring import maps is false.");
      NS_DispatchToCurrentThread(
          NewRunnableMethod("nsIScriptElement::FireErrorEvent", aElement,
                            &nsIScriptElement::FireErrorEvent));
      return false;
    }

    // Step 2: Set the element's node document's acquiring import maps to false.
    mModuleLoader->SetAcquiringImportMaps(false);

    UniquePtr<ImportMap> importMap = mModuleLoader->ParseImportMap(request);

    // https://wicg.github.io/import-maps/#register-an-import-map
    //
    // Step 1. If element’s the script’s result is null, then fire an event
    // named error at element, and return.
    if (!importMap) {
      NS_DispatchToCurrentThread(
          NewRunnableMethod("nsIScriptElement::FireErrorEvent", aElement,
                            &nsIScriptElement::FireErrorEvent));
      return false;
    }

    // Step 3. Assert: element’s the script’s type is "importmap".
    MOZ_ASSERT(aElement->GetScriptIsImportMap());

    // Step 4 to step 9 is done in RegisterImportMap.
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

  // Found preloaded request. Note that a script-inserted script can steal a
  // preload!
  RefPtr<ScriptLoadRequest> request = mPreloads[i].mRequest;
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
    if (!request->GetScriptLoadContext()->IsLinkPreloadScript()) {
      request->Cancel();
    }
    return nullptr;
  }

  // Report any errors that we skipped while preloading.
  ReportPreloadErrorsToConsole(request);

  // This makes sure the pending preload (if exists) for this resource is
  // properly marked as used and thus not notified in the console as unused.
  request->GetScriptLoadContext()->NotifyUsage();
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

  for (ScriptLoadRequest* req =
           mNonAsyncExternalScriptInsertedRequests.getFirst();
       req; req = req->getNext()) {
    req->Cancel();
  }

  for (size_t i = 0; i < mPreloads.Length(); i++) {
    mPreloads[i].mRequest->Cancel();
  }

  mOffThreadCompilingRequests.CancelRequestsAndClear();
}

nsresult ScriptLoader::ProcessOffThreadRequest(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->mState == ScriptLoadRequest::State::Compiling);
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
    MOZ_ASSERT(aRequest->GetScriptLoadContext()->mOffThreadToken);
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

NS_IMETHODIMP
NotifyOffThreadScriptLoadCompletedRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  // We want these to be dropped on the main thread, once we return from this
  // function.
  RefPtr<ScriptLoadRequest> request = std::move(mRequest);

  // Runnable pointer should have been cleared in the offthread callback.
  MOZ_ASSERT(!request->GetScriptLoadContext()->mRunnable);

  if (profiler_is_active()) {
    ProfilerString8View scriptSourceString;
    if (request->IsTextSource()) {
      scriptSourceString = "ScriptCompileOffThread";
    } else {
      MOZ_ASSERT(request->IsBytecode());
      scriptSourceString = "BytecodeDecodeOffThread";
    }

    nsAutoCString profilerLabelString;
    request->GetScriptLoadContext()->GetProfilerLabel(profilerLabelString);
    PROFILER_MARKER_TEXT(
        scriptSourceString, JS,
        MarkerTiming::Interval(
            request->GetScriptLoadContext()->mOffThreadParseStartTime,
            request->GetScriptLoadContext()->mOffThreadParseStopTime),
        profilerLabelString);
  }

  RefPtr<ScriptLoader> loader = std::move(mLoader);

  // Request was already cancelled at some earlier point.
  if (!request->GetScriptLoadContext()->mOffThreadToken) {
    return NS_OK;
  }

  return loader->ProcessOffThreadRequest(request);
}

static void OffThreadScriptLoaderCallback(JS::OffThreadToken* aToken,
                                          void* aCallbackData) {
  RefPtr<NotifyOffThreadScriptLoadCompletedRunnable> aRunnable = dont_AddRef(
      static_cast<NotifyOffThreadScriptLoadCompletedRunnable*>(aCallbackData));
  MOZ_ASSERT(
      aRunnable.get() ==
      aRunnable->GetScriptLoadRequest()->GetScriptLoadContext()->mRunnable);

  aRunnable->GetScriptLoadRequest()
      ->GetScriptLoadContext()
      ->mOffThreadParseStopTime = TimeStamp::Now();

  LogRunnable::Run run(aRunnable);

  aRunnable->SetToken(aToken);

  // If mRunnable was cleared then request was canceled so do nothing.
  if (!aRunnable->GetScriptLoadRequest()
           ->GetScriptLoadContext()
           ->mRunnable.exchange(nullptr)) {
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
    if (!JS::CanCompileOffThread(cx, options, aRequest->ScriptTextLength())) {
      TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                     "scriptloader_main_thread_compile");
      return NS_OK;
    }
  } else {
    MOZ_ASSERT(aRequest->IsBytecode());

    size_t length =
        aRequest->mScriptBytecode.length() - aRequest->mBytecodeOffset;
    JS::DecodeOptions decodeOptions(options);
    if (!JS::CanDecodeOffThread(cx, decodeOptions, length)) {
      return NS_OK;
    }
  }

  RefPtr<NotifyOffThreadScriptLoadCompletedRunnable> runnable =
      new NotifyOffThreadScriptLoadCompletedRunnable(aRequest, this);

  // Emulate dispatch.  CompileOffThreadModule will call
  // OffThreadScriptLoaderCallback were we will emulate run.
  LogRunnable::LogDispatch(runnable);

  aRequest->GetScriptLoadContext()->mOffThreadParseStartTime = TimeStamp::Now();

  // Save the runnable so it can be properly cleared during cancellation.
  aRequest->GetScriptLoadContext()->mRunnable = runnable.get();
  auto signalOOM = mozilla::MakeScopeExit(
      [&aRequest]() { aRequest->GetScriptLoadContext()->mRunnable = nullptr; });

  if (aRequest->IsBytecode()) {
    JS::DecodeOptions decodeOptions(options);
    aRequest->GetScriptLoadContext()->mOffThreadToken =
        JS::DecodeStencilOffThread(cx, decodeOptions, aRequest->mScriptBytecode,
                                   aRequest->mBytecodeOffset,
                                   OffThreadScriptLoaderCallback,
                                   static_cast<void*>(runnable));
    if (!aRequest->GetScriptLoadContext()->mOffThreadToken) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else if (aRequest->IsModuleRequest()) {
    MOZ_ASSERT(aRequest->IsTextSource());
    MaybeSourceText maybeSource;
    nsresult rv = aRequest->GetScriptSource(cx, &maybeSource);
    NS_ENSURE_SUCCESS(rv, rv);

    auto compile = [&](auto& source) {
      return JS::CompileModuleToStencilOffThread(
          cx, options, source, OffThreadScriptLoaderCallback, runnable.get());
    };

    MOZ_ASSERT(!maybeSource.empty());
    JS::OffThreadToken* token = maybeSource.mapNonEmpty(compile);
    if (!token) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    aRequest->GetScriptLoadContext()->mOffThreadToken = token;
  } else {
    MOZ_ASSERT(aRequest->IsTextSource());

    if (ShouldApplyDelazifyStrategy(aRequest)) {
      ApplyDelazifyStrategy(&options);
      mTotalFullParseSize +=
          aRequest->ScriptTextLength() > 0
              ? static_cast<uint32_t>(aRequest->ScriptTextLength())
              : 0;

      LOG(
          ("ScriptLoadRequest (%p): non-on-demand-only Parsing Enabled for "
           "url=%s mTotalFullParseSize=%u",
           aRequest, aRequest->mURI->GetSpecOrDefault().get(),
           mTotalFullParseSize));
    }

    MaybeSourceText maybeSource;
    nsresult rv = aRequest->GetScriptSource(cx, &maybeSource);
    NS_ENSURE_SUCCESS(rv, rv);

    if (StaticPrefs::dom_expose_test_interfaces()) {
      switch (options.eagerDelazificationStrategy()) {
        case JS::DelazificationOption::OnDemandOnly:
          TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                         "delazification_on_demand_only");
          break;
        case JS::DelazificationOption::CheckConcurrentWithOnDemand:
        case JS::DelazificationOption::ConcurrentDepthFirst:
          TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                         "delazification_concurrent_depth_first");
          break;
        case JS::DelazificationOption::ParseEverythingEagerly:
          TRACE_FOR_TEST(aRequest->GetScriptLoadContext()->GetScriptElement(),
                         "delazification_parse_everything_eagerly");
          break;
      }
    }

    auto compile = [&](auto& source) {
      return JS::CompileToStencilOffThread(
          cx, options, source, OffThreadScriptLoaderCallback, runnable.get());
    };

    MOZ_ASSERT(!maybeSource.empty());
    JS::OffThreadToken* token = maybeSource.mapNonEmpty(compile);
    if (!token) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    aRequest->GetScriptLoadContext()->mOffThreadToken = token;
  }
  signalOOM.release();

  aRequest->GetScriptLoadContext()->BlockOnload(mDocument);

  // Once the compilation is finished, an event would be added to the event loop
  // to call ScriptLoader::ProcessOffThreadRequest with the same request.
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
  Unused << runnable.forget();
  return NS_OK;
}

nsresult ScriptLoader::CompileOffThreadOrProcessRequest(
    ScriptLoadRequest* aRequest) {
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");

  if (!aRequest->GetScriptLoadContext()->mOffThreadToken &&
      !aRequest->GetScriptLoadContext()->CompileStarted()) {
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

nsresult ScriptLoader::ProcessRequest(ScriptLoadRequest* aRequest) {
  LOG(("ScriptLoadRequest (%p): Process request", aRequest));

  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");
  NS_ASSERTION(aRequest->IsReadyToRun(),
               "Processing a request that is not ready to run.");

  NS_ENSURE_ARG(aRequest);

  auto unblockOnload = MakeScopeExit(
      [&] { aRequest->GetScriptLoadContext()->MaybeUnblockOnload(); });

  if (aRequest->IsModuleRequest()) {
    ModuleLoadRequest* request = aRequest->AsModuleRequest();
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

  if (aRequest->GetScriptLoadContext()->mOffThreadToken) {
    // The request was parsed off-main-thread, but the result of the off
    // thread parse was not actually needed to process the request
    // (disappearing window, some other error, ...). Finish the
    // request to avoid leaks in the JS engine.
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
    aRequest->mScriptBytecode.clearAndFree();
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

  aOptions->allocateInstantiationStorage = true;

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
  MOZ_ASSERT(aRequest->IsReadyToRun());

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
    if (aRequest->GetScriptLoadContext()->mOffThreadToken) {
      LOG(("ScriptLoadRequest (%p): Decode Bytecode & Join and Execute",
           aRequest));
      rv = aExec.JoinDecode(&aRequest->GetScriptLoadContext()->mOffThreadToken);
    } else {
      LOG(("ScriptLoadRequest (%p): Decode Bytecode and Execute", aRequest));
      AUTO_PROFILER_MARKER_TEXT("BytecodeDecodeMainThread", JS,
                                MarkerInnerWindowIdFromJSContext(aCx),
                                profilerLabelString);

      rv = aExec.Decode(aRequest->mScriptBytecode, aRequest->mBytecodeOffset);
    }

    // We do not expect to be saving anything when we already have some
    // bytecode.
    MOZ_ASSERT(!aRequest->mCacheInfo);
    return rv;
  }

  MOZ_ASSERT(aRequest->IsSource());
  bool encodeBytecode = ShouldCacheBytecode(aRequest);
  aExec.SetEncodeBytecode(encodeBytecode);

  if (aRequest->GetScriptLoadContext()->mOffThreadToken) {
    // Off-main-thread parsing.
    LOG(
        ("ScriptLoadRequest (%p): Join (off-thread parsing) and "
         "Execute",
         aRequest));
    MOZ_ASSERT(aRequest->IsTextSource());
    rv = aExec.JoinCompile(&aRequest->GetScriptLoadContext()->mOffThreadToken);
  } else {
    // Main thread parsing (inline and small scripts)
    LOG(("ScriptLoadRequest (%p): Compile And Exec", aRequest));
    MOZ_ASSERT(aRequest->IsTextSource());
    MaybeSourceText maybeSource;
    rv = aRequest->GetScriptSource(aCx, &maybeSource);
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
    // NOTE: This assertion will fail once we start encoding more data after the
    //       first encode.
    MOZ_ASSERT(aRequest->mBytecodeOffset == aRequest->mScriptBytecode.length());
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
  RefPtr<ClassicScript> classicScript =
      new ClassicScript(aRequest->mFetchOptions, aRequest->mBaseURL);
  JS::Rooted<JS::Value> classicScriptValue(cx, JS::PrivateValue(classicScript));

  JS::CompileOptions options(cx);
  JS::Rooted<JSScript*> introductionScript(cx);
  nsresult rv =
      FillCompileOptionsForRequest(cx, aRequest, &options, &introductionScript);

  if (NS_FAILED(rv)) {
    return rv;
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

  CancelScriptLoadRequests();
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
    request->mScriptBytecode.clearAndFree();
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
        JS::FinishIncrementalEncoding(aCx, module, aRequest->mScriptBytecode);
  } else {
    JS::Rooted<JSScript*> script(aCx, aRequest->mScriptForBytecodeEncoding);
    result =
        JS::FinishIncrementalEncoding(aCx, script, aRequest->mScriptBytecode);
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
  if (!ScriptBytecodeCompress(aRequest->mScriptBytecode,
                              aRequest->mBytecodeOffset, compressedBytecode)) {
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
      bool result;
      if (request->IsModuleRequest()) {
        ModuleScript* moduleScript = request->AsModuleRequest()->mModuleScript;
        JS::Rooted<JSObject*> module(aes->cx(), moduleScript->ModuleRecord());
        result = JS::FinishIncrementalEncoding(aes->cx(), module,
                                               request->mScriptBytecode);
      } else {
        JS::Rooted<JSScript*> script(aes->cx(),
                                     request->mScriptForBytecodeEncoding);
        result = JS::FinishIncrementalEncoding(aes->cx(), script,
                                               request->mScriptBytecode);
      }
      if (!result) {
        JS_ClearPendingException(aes->cx());
      }
    }

    request->mScriptBytecode.clearAndFree();
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

  return false;
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
  uint32_t columnNo = element ? element->GetScriptColumnNumber() : 0;

  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  "Script Loader"_ns, mDocument,
                                  nsContentUtils::eDOM_PROPERTIES, message,
                                  params, nullptr, u""_ns, lineNo, columnNo);
}

void ScriptLoader::ReportWarningToConsole(
    ScriptLoadRequest* aRequest, const char* aMessageName,
    const nsTArray<nsString>& aParams) const {
  nsIScriptElement* element =
      aRequest->GetScriptLoadContext()->GetScriptElement();
  uint32_t lineNo = element ? element->GetScriptLineNumber() : 0;
  uint32_t columnNo = element ? element->GetScriptColumnNumber() : 0;
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  "Script Loader"_ns, mDocument,
                                  nsContentUtils::eDOM_PROPERTIES, aMessageName,
                                  aParams, nullptr, u""_ns, lineNo, columnNo);
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
        modReq->CancelDynamicImport(aResult);
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
    rv = AttemptAsyncScriptCompile(request, &couldCompile);
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
    nsresult rv = AttemptAsyncScriptCompile(aRequest, &couldCompile);
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
  if (!aTerminated) {
    return;
  }
  mDeferRequests.CancelRequestsAndClear();
  mLoadingAsyncRequests.CancelRequestsAndClear();
  mLoadedAsyncRequests.CancelRequestsAndClear();
  mNonAsyncExternalScriptInsertedRequests.CancelRequestsAndClear();
  mXSLTRequests.CancelRequestsAndClear();

  if (mModuleLoader) {
    mModuleLoader->CancelAndClearDynamicImports();
  }

  for (ModuleLoader* loader : mWebExtModuleLoaders) {
    loader->CancelAndClearDynamicImports();
  }

  if (mParserBlockingRequest) {
    mParserBlockingRequest->Cancel();
    mParserBlockingRequest = nullptr;
  }

  // Cancel any unused scripts that were compiled speculatively
  for (size_t i = 0; i < mPreloads.Length(); i++) {
    mPreloads[i].mRequest->GetScriptLoadContext()->MaybeCancelOffThreadScript();
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
  request->GetScriptLoadContext()->mIsInline = false;
  request->GetScriptLoadContext()->mScriptFromHead = aScriptFromHead;
  request->GetScriptLoadContext()->SetScriptMode(aDefer, aAsync, aLinkPreload);
  request->GetScriptLoadContext()->SetIsPreloadRequest();

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
  if (aRequest->IsReadyToRun()) {
    mLoadedAsyncRequests.AppendElement(aRequest);
  } else {
    mLoadingAsyncRequests.AppendElement(aRequest);
  }
}

void ScriptLoader::MaybeMoveToLoadedList(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->IsReadyToRun());

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
