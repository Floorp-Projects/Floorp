/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArray.h"
#include "nsContentSecurityManager.h"
#include "nsContentSecurityUtils.h"
#include "nsContentPolicyUtils.h"
#include "nsEscape.h"
#include "nsDataHandler.h"
#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "nsIHttpChannelInternal.h"
#include "nsINode.h"
#include "nsIStreamListener.h"
#include "nsILoadInfo.h"
#include "nsIMIMEService.h"
#include "nsIOService.h"
#include "nsContentUtils.h"
#include "nsCORSListenerProxy.h"
#include "nsIParentChannel.h"
#include "nsIRedirectHistoryEntry.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsSandboxFlags.h"
#include "nsIXPConnect.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/Components.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryComms.h"
#include "xpcpublic.h"
#include "nsMimeTypes.h"

#include "jsapi.h"
#include "js/RegExp.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::Telemetry;

NS_IMPL_ISUPPORTS(nsContentSecurityManager, nsIContentSecurityManager,
                  nsIChannelEventSink)

mozilla::LazyLogModule sCSMLog("CSMLog");

// These first two are used for off-the-main-thread checks of
// general.config.filename
//   (which can't be checked off-main-thread).
Atomic<bool, mozilla::Relaxed> sJSHacksChecked(false);
Atomic<bool, mozilla::Relaxed> sJSHacksPresent(false);
Atomic<bool, mozilla::Relaxed> sCSSHacksChecked(false);
Atomic<bool, mozilla::Relaxed> sCSSHacksPresent(false);
Atomic<bool, mozilla::Relaxed> sTelemetryEventEnabled(false);

/* static */
bool nsContentSecurityManager::AllowTopLevelNavigationToDataURI(
    nsIChannel* aChannel) {
  // Let's block all toplevel document navigations to a data: URI.
  // In all cases where the toplevel document is navigated to a
  // data: URI the triggeringPrincipal is a contentPrincipal, or
  // a NullPrincipal. In other cases, e.g. typing a data: URL into
  // the URL-Bar, the triggeringPrincipal is a SystemPrincipal;
  // we don't want to block those loads. Only exception, loads coming
  // from an external applicaton (e.g. Thunderbird) don't load
  // using a contentPrincipal, but we want to block those loads.
  if (!StaticPrefs::security_data_uri_block_toplevel_data_uri_navigations()) {
    return true;
  }
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  if (loadInfo->GetExternalContentPolicyType() !=
      ExtContentPolicy::TYPE_DOCUMENT) {
    return true;
  }
  if (loadInfo->GetForceAllowDataURI()) {
    // if the loadinfo explicitly allows the data URI navigation, let's allow it
    // now
    return true;
  }
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, true);
  bool isDataURI = uri->SchemeIs("data");
  if (!isDataURI) {
    return true;
  }

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, true);
  nsAutoCString contentType;
  bool base64;
  rv = nsDataHandler::ParseURI(spec, contentType, nullptr, base64, nullptr);
  NS_ENSURE_SUCCESS(rv, true);

  // Allow data: images as long as they are not SVGs
  if (StringBeginsWith(contentType, "image/"_ns) &&
      !contentType.EqualsLiteral("image/svg+xml")) {
    return true;
  }
  // Allow all data: PDFs. or JSON documents
  if (contentType.EqualsLiteral(APPLICATION_JSON) ||
      contentType.EqualsLiteral(TEXT_JSON) ||
      contentType.EqualsLiteral(APPLICATION_PDF)) {
    return true;
  }
  // Redirecting to a toplevel data: URI is not allowed, hence we make
  // sure the RedirectChain is empty.
  if (!loadInfo->GetLoadTriggeredFromExternal() &&
      loadInfo->TriggeringPrincipal()->IsSystemPrincipal() &&
      loadInfo->RedirectChain().IsEmpty()) {
    return true;
  }

  // We're going to block the request, construct the localized error message to
  // report to the console.
  nsAutoCString dataSpec;
  uri->GetSpec(dataSpec);
  if (dataSpec.Length() > 50) {
    dataSpec.Truncate(50);
    dataSpec.AppendLiteral("...");
  }
  AutoTArray<nsString, 1> params;
  CopyUTF8toUTF16(NS_UnescapeURL(dataSpec), *params.AppendElement());
  nsAutoString errorText;
  rv = nsContentUtils::FormatLocalizedString(
      nsContentUtils::eSECURITY_PROPERTIES, "BlockTopLevelDataURINavigation",
      params, errorText);
  NS_ENSURE_SUCCESS(rv, false);

  // Report the localized error message to the console for the loading
  // BrowsingContext's current inner window.
  RefPtr<BrowsingContext> target = loadInfo->GetBrowsingContext();
  nsContentUtils::ReportToConsoleByWindowID(
      errorText, nsIScriptError::warningFlag, "DATA_URI_BLOCKED"_ns,
      target ? target->GetCurrentInnerWindowId() : 0);
  return false;
}

/* static */
bool nsContentSecurityManager::AllowInsecureRedirectToDataURI(
    nsIChannel* aNewChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aNewChannel->LoadInfo();
  if (loadInfo->GetExternalContentPolicyType() !=
      ExtContentPolicy::TYPE_SCRIPT) {
    return true;
  }
  nsCOMPtr<nsIURI> newURI;
  nsresult rv = NS_GetFinalChannelURI(aNewChannel, getter_AddRefs(newURI));
  if (NS_FAILED(rv) || !newURI) {
    return true;
  }
  bool isDataURI = newURI->SchemeIs("data");
  if (!isDataURI) {
    return true;
  }

  // Web Extensions are exempt from that restriction and are allowed to redirect
  // a channel to a data: URI. When a web extension redirects a channel, we set
  // a flag on the loadInfo which allows us to identify such redirects here.
  if (loadInfo->GetAllowInsecureRedirectToDataURI()) {
    return true;
  }

  nsAutoCString dataSpec;
  newURI->GetSpec(dataSpec);
  if (dataSpec.Length() > 50) {
    dataSpec.Truncate(50);
    dataSpec.AppendLiteral("...");
  }
  nsCOMPtr<Document> doc;
  nsINode* node = loadInfo->LoadingNode();
  if (node) {
    doc = node->OwnerDoc();
  }
  AutoTArray<nsString, 1> params;
  CopyUTF8toUTF16(NS_UnescapeURL(dataSpec), *params.AppendElement());
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  "DATA_URI_BLOCKED"_ns, doc,
                                  nsContentUtils::eSECURITY_PROPERTIES,
                                  "BlockSubresourceRedirectToData", params);
  return false;
}

/* static */
nsresult nsContentSecurityManager::CheckFTPSubresourceLoad(
    nsIChannel* aChannel) {
  // We dissallow using FTP resources as a subresource everywhere.
  // The only valid way to use FTP resources is loading it as
  // a top level document.

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  ExtContentPolicyType type = loadInfo->GetExternalContentPolicyType();

  // Allow top-level FTP documents and save-as download of FTP files on
  // HTTP pages.
  if (type == ExtContentPolicy::TYPE_DOCUMENT ||
      type == ExtContentPolicy::TYPE_SAVEAS_DOWNLOAD) {
    return NS_OK;
  }

  // Allow the system principal to load everything. This is meant to
  // temporarily fix downloads and pdf.js.
  nsIPrincipal* triggeringPrincipal = loadInfo->TriggeringPrincipal();
  if (triggeringPrincipal->IsSystemPrincipal()) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!uri) {
    return NS_OK;
  }

  bool isFtpURI = uri->SchemeIs("ftp");
  if (!isFtpURI) {
    return NS_OK;
  }

  nsCOMPtr<Document> doc;
  if (nsINode* node = loadInfo->LoadingNode()) {
    doc = node->OwnerDoc();
  }

  nsAutoCString spec;
  uri->GetSpec(spec);
  AutoTArray<nsString, 1> params;
  CopyUTF8toUTF16(NS_UnescapeURL(spec), *params.AppendElement());

  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, "FTP_URI_BLOCKED"_ns, doc,
      nsContentUtils::eSECURITY_PROPERTIES, "BlockSubresourceFTP", params);

  return NS_ERROR_CONTENT_BLOCKED;
}

static nsresult ValidateSecurityFlags(nsILoadInfo* aLoadInfo) {
  nsSecurityFlags securityMode = aLoadInfo->GetSecurityMode();

  // We should never perform a security check on a loadInfo that uses the flag
  // SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK, because that is only used for
  // temporary loadInfos used for explicit nsIContentPolicy checks, but never be
  // set as a security flag on an actual channel.
  if (securityMode !=
          nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_INHERITS_SEC_CONTEXT &&
      securityMode != nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED &&
      securityMode !=
          nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT &&
      securityMode != nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL &&
      securityMode != nsILoadInfo::SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT) {
    MOZ_ASSERT(
        false,
        "need one securityflag from nsILoadInfo to perform security checks");
    return NS_ERROR_FAILURE;
  }

  // all good, found the right security flags
  return NS_OK;
}

static already_AddRefed<nsIPrincipal> GetExtensionSandboxPrincipal(
    nsILoadInfo* aLoadInfo) {
  // An extension is allowed to load resources from itself when its pages are
  // loaded into a sandboxed frame.  Extension resources in a sandbox have
  // a null principal and no access to extension APIs.  See "sandbox" in
  // MDN extension docs for more information.
  if (!aLoadInfo->TriggeringPrincipal()->GetIsNullPrincipal()) {
    return nullptr;
  }
  RefPtr<Document> doc;
  aLoadInfo->GetLoadingDocument(getter_AddRefs(doc));
  if (!doc || !(doc->GetSandboxFlags() & SANDBOXED_ORIGIN)) {
    return nullptr;
  }

  // node principal is also a null principal here, so we need to
  // create a principal using documentURI, which is the moz-extension
  // uri for the page if this is an extension sandboxed page.
  nsCOMPtr<nsIPrincipal> docPrincipal = BasePrincipal::CreateContentPrincipal(
      doc->GetDocumentURI(), doc->NodePrincipal()->OriginAttributesRef());

  if (!BasePrincipal::Cast(docPrincipal)->AddonPolicy()) {
    return nullptr;
  }
  return docPrincipal.forget();
}

static bool IsImageLoadInEditorAppType(nsILoadInfo* aLoadInfo) {
  // Editor apps get special treatment here, editors can load images
  // from anywhere.  This allows editor to insert images from file://
  // into documents that are being edited.
  nsContentPolicyType type = aLoadInfo->InternalContentPolicyType();
  if (type != nsIContentPolicy::TYPE_INTERNAL_IMAGE &&
      type != nsIContentPolicy::TYPE_INTERNAL_IMAGE_PRELOAD &&
      type != nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON &&
      type != nsIContentPolicy::TYPE_IMAGESET) {
    return false;
  }

  auto appType = nsIDocShell::APP_TYPE_UNKNOWN;
  nsINode* node = aLoadInfo->LoadingNode();
  if (!node) {
    return false;
  }
  Document* doc = node->OwnerDoc();
  if (!doc) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem = doc->GetDocShell();
  if (!docShellTreeItem) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> root;
  docShellTreeItem->GetInProcessRootTreeItem(getter_AddRefs(root));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(root));
  if (docShell) {
    appType = docShell->GetAppType();
  }

  return appType == nsIDocShell::APP_TYPE_EDITOR;
}

static nsresult DoCheckLoadURIChecks(nsIURI* aURI, nsILoadInfo* aLoadInfo) {
  // In practice, these DTDs are just used for localization, so applying the
  // same principal check as Fluent.
  if (aLoadInfo->InternalContentPolicyType() ==
      nsIContentPolicy::TYPE_INTERNAL_DTD) {
    RefPtr<Document> doc;
    aLoadInfo->GetLoadingDocument(getter_AddRefs(doc));
    bool allowed = false;
    aLoadInfo->TriggeringPrincipal()->IsL10nAllowed(
        doc ? doc->GetDocumentURI() : nullptr, &allowed);

    return allowed ? NS_OK : NS_ERROR_DOM_BAD_URI;
  }

  // This is used in order to allow a privileged DOMParser to parse documents
  // that need to access localization DTDs. We just allow through
  // TYPE_INTERNAL_FORCE_ALLOWED_DTD no matter what the triggering principal is.
  if (aLoadInfo->InternalContentPolicyType() ==
      nsIContentPolicy::TYPE_INTERNAL_FORCE_ALLOWED_DTD) {
    return NS_OK;
  }

  if (IsImageLoadInEditorAppType(aLoadInfo)) {
    return NS_OK;
  }

  nsCOMPtr<nsIPrincipal> triggeringPrincipal = aLoadInfo->TriggeringPrincipal();
  nsCOMPtr<nsIPrincipal> addonPrincipal =
      GetExtensionSandboxPrincipal(aLoadInfo);
  if (addonPrincipal) {
    // call CheckLoadURIWithPrincipal() as below to continue other checks, but
    // with the addon principal.
    triggeringPrincipal = addonPrincipal;
  }

  // Only call CheckLoadURIWithPrincipal() using the TriggeringPrincipal and not
  // the LoadingPrincipal when SEC_ALLOW_CROSS_ORIGIN_* security flags are set,
  // to allow, e.g. user stylesheets to load chrome:// URIs.
  return nsContentUtils::GetSecurityManager()->CheckLoadURIWithPrincipal(
      triggeringPrincipal, aURI, aLoadInfo->CheckLoadURIFlags(),
      aLoadInfo->GetInnerWindowID());
}

static bool URIHasFlags(nsIURI* aURI, uint32_t aURIFlags) {
  bool hasFlags;
  nsresult rv = NS_URIChainHasFlags(aURI, aURIFlags, &hasFlags);
  NS_ENSURE_SUCCESS(rv, false);

  return hasFlags;
}

static nsresult DoSOPChecks(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                            nsIChannel* aChannel) {
  if (aLoadInfo->GetAllowChrome() &&
      (URIHasFlags(aURI, nsIProtocolHandler::URI_IS_UI_RESOURCE) ||
       nsContentUtils::SchemeIs(aURI, "moz-safe-about"))) {
    // UI resources are allowed.
    return DoCheckLoadURIChecks(aURI, aLoadInfo);
  }

  if (NS_HasBeenCrossOrigin(aChannel, true)) {
    NS_SetRequestBlockingReason(aLoadInfo,
                                nsILoadInfo::BLOCKING_REASON_NOT_SAME_ORIGIN);
    return NS_ERROR_DOM_BAD_URI;
  }

  return NS_OK;
}

static nsresult DoCORSChecks(nsIChannel* aChannel, nsILoadInfo* aLoadInfo,
                             nsCOMPtr<nsIStreamListener>& aInAndOutListener) {
  MOZ_RELEASE_ASSERT(aInAndOutListener,
                     "can not perform CORS checks without a listener");

  // No need to set up CORS if TriggeringPrincipal is the SystemPrincipal.
  if (aLoadInfo->TriggeringPrincipal()->IsSystemPrincipal()) {
    return NS_OK;
  }

  // We use the triggering principal here, rather than the loading principal
  // to ensure that anonymous CORS content in the browser resources and in
  // WebExtensions is allowed to load.
  nsIPrincipal* principal = aLoadInfo->TriggeringPrincipal();
  RefPtr<nsCORSListenerProxy> corsListener = new nsCORSListenerProxy(
      aInAndOutListener, principal,
      aLoadInfo->GetCookiePolicy() == nsILoadInfo::SEC_COOKIES_INCLUDE);
  // XXX: @arg: DataURIHandling::Allow
  // lets use  DataURIHandling::Allow for now and then decide on callsite basis.
  // see also:
  // http://mxr.mozilla.org/mozilla-central/source/dom/security/nsCORSListenerProxy.h#33
  nsresult rv = corsListener->Init(aChannel, DataURIHandling::Allow);
  NS_ENSURE_SUCCESS(rv, rv);
  aInAndOutListener = corsListener;
  return NS_OK;
}

static nsresult DoContentSecurityChecks(nsIChannel* aChannel,
                                        nsILoadInfo* aLoadInfo) {
  ExtContentPolicyType contentPolicyType =
      aLoadInfo->GetExternalContentPolicyType();
  nsContentPolicyType internalContentPolicyType =
      aLoadInfo->InternalContentPolicyType();
  nsCString mimeTypeGuess;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  switch (contentPolicyType) {
    case ExtContentPolicy::TYPE_OTHER: {
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_SCRIPT: {
      mimeTypeGuess = "application/javascript"_ns;
      break;
    }

    case ExtContentPolicy::TYPE_IMAGE: {
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_STYLESHEET: {
      mimeTypeGuess = "text/css"_ns;
      break;
    }

    case ExtContentPolicy::TYPE_OBJECT: {
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_DOCUMENT: {
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_SUBDOCUMENT: {
      mimeTypeGuess = "text/html"_ns;
      break;
    }

    case ExtContentPolicy::TYPE_PING: {
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_XMLHTTPREQUEST: {
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::DOCUMENT_NODE,
                   "type_xml requires requestingContext of type Document");
      }
#endif
      // We're checking for the external TYPE_XMLHTTPREQUEST here in case
      // an addon creates a request with that type.
      if (internalContentPolicyType ==
              nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST ||
          internalContentPolicyType == nsIContentPolicy::TYPE_XMLHTTPREQUEST) {
        mimeTypeGuess.Truncate();
      } else {
        MOZ_ASSERT(internalContentPolicyType ==
                       nsIContentPolicy::TYPE_INTERNAL_EVENTSOURCE,
                   "can not set mime type guess for unexpected internal type");
        mimeTypeGuess = nsLiteralCString(TEXT_EVENT_STREAM);
      }
      break;
    }

    case ExtContentPolicy::TYPE_OBJECT_SUBREQUEST: {
      mimeTypeGuess.Truncate();
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(
            !node || node->NodeType() == nsINode::ELEMENT_NODE,
            "type_subrequest requires requestingContext of type Element");
      }
#endif
      break;
    }

    case ExtContentPolicy::TYPE_DTD: {
      mimeTypeGuess.Truncate();
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::DOCUMENT_NODE,
                   "type_dtd requires requestingContext of type Document");
      }
#endif
      break;
    }

    case ExtContentPolicy::TYPE_FONT:
    case ExtContentPolicy::TYPE_UA_FONT: {
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_MEDIA: {
      if (internalContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_TRACK) {
        mimeTypeGuess = "text/vtt"_ns;
      } else {
        mimeTypeGuess.Truncate();
      }
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::ELEMENT_NODE,
                   "type_media requires requestingContext of type Element");
      }
#endif
      break;
    }

    case ExtContentPolicy::TYPE_WEBSOCKET: {
      // Websockets have to use the proxied URI:
      // ws:// instead of http:// for CSP checks
      nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
          do_QueryInterface(aChannel);
      MOZ_ASSERT(httpChannelInternal);
      if (httpChannelInternal) {
        rv = httpChannelInternal->GetProxyURI(getter_AddRefs(uri));
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_CSP_REPORT: {
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_XSLT: {
      mimeTypeGuess = "application/xml"_ns;
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::DOCUMENT_NODE,
                   "type_xslt requires requestingContext of type Document");
      }
#endif
      break;
    }

    case ExtContentPolicy::TYPE_BEACON: {
      mimeTypeGuess.Truncate();
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::DOCUMENT_NODE,
                   "type_beacon requires requestingContext of type Document");
      }
#endif
      break;
    }

    case ExtContentPolicy::TYPE_FETCH: {
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_IMAGESET: {
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_WEB_MANIFEST: {
      mimeTypeGuess = "application/manifest+json"_ns;
      break;
    }

    case ExtContentPolicy::TYPE_SAVEAS_DOWNLOAD: {
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_SPECULATIVE: {
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_PROXIED_WEBRTC_MEDIA: {
      mimeTypeGuess.Truncate();
      break;
    }

    case ExtContentPolicy::TYPE_INVALID:
      MOZ_ASSERT(false,
                 "can not perform security check without a valid contentType");
      // Do not add default: so that compilers can catch the missing case.
  }

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(uri, aLoadInfo, mimeTypeGuess, &shouldLoad,
                                 nsContentUtils::GetContentPolicy());

  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    NS_SetRequestBlockingReasonIfNull(
        aLoadInfo, nsILoadInfo::BLOCKING_REASON_CONTENT_POLICY_GENERAL);

    if (NS_SUCCEEDED(rv) &&
        (contentPolicyType == ExtContentPolicy::TYPE_DOCUMENT ||
         contentPolicyType == ExtContentPolicy::TYPE_SUBDOCUMENT)) {
      if (shouldLoad == nsIContentPolicy::REJECT_TYPE) {
        // for docshell loads we might have to return SHOW_ALT.
        return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
      }
      if (shouldLoad == nsIContentPolicy::REJECT_POLICY) {
        return NS_ERROR_BLOCKED_BY_POLICY;
      }
    }
    return NS_ERROR_CONTENT_BLOCKED;
  }

  return NS_OK;
}

static void LogHTTPSOnlyInfo(nsILoadInfo* aLoadInfo) {
  MOZ_LOG(sCSMLog, LogLevel::Verbose, ("  httpsOnlyFirstStatus:"));
  uint32_t httpsOnlyStatus = aLoadInfo->GetHttpsOnlyStatus();

  if (httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_UNINITIALIZED) {
    MOZ_LOG(sCSMLog, LogLevel::Verbose, ("    - HTTPS_ONLY_UNINITIALIZED"));
  }
  if (httpsOnlyStatus &
      nsILoadInfo::HTTPS_ONLY_UPGRADED_LISTENER_NOT_REGISTERED) {
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("    - HTTPS_ONLY_UPGRADED_LISTENER_NOT_REGISTERED"));
  }
  if (httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_UPGRADED_LISTENER_REGISTERED) {
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("    - HTTPS_ONLY_UPGRADED_LISTENER_REGISTERED"));
  }
  if (httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_EXEMPT) {
    MOZ_LOG(sCSMLog, LogLevel::Verbose, ("    - HTTPS_ONLY_EXEMPT"));
  }
  if (httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_TOP_LEVEL_LOAD_IN_PROGRESS) {
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("    - HTTPS_ONLY_TOP_LEVEL_LOAD_IN_PROGRESS"));
  }
  if (httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_DOWNLOAD_IN_PROGRESS) {
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("    - HTTPS_ONLY_DOWNLOAD_IN_PROGRESS"));
  }
  if (httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_DO_NOT_LOG_TO_CONSOLE) {
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("    - HTTPS_ONLY_DO_NOT_LOG_TO_CONSOLE"));
  }
  if (httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_UPGRADED_HTTPS_FIRST) {
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("    - HTTPS_ONLY_UPGRADED_HTTPS_FIRST"));
  }
}

static void LogPrincipal(nsIPrincipal* aPrincipal,
                         const nsAString& aPrincipalName,
                         const uint8_t& aNestingLevel) {
  nsPrintfCString aIndentationString("%*s", aNestingLevel * 2, "");

  if (aPrincipal && aPrincipal->IsSystemPrincipal()) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("%s%s: SystemPrincipal\n", aIndentationString.get(),
             NS_ConvertUTF16toUTF8(aPrincipalName).get()));
    return;
  }
  if (aPrincipal) {
    if (aPrincipal->GetIsNullPrincipal()) {
      MOZ_LOG(sCSMLog, LogLevel::Debug,
              ("%s%s: NullPrincipal\n", aIndentationString.get(),
               NS_ConvertUTF16toUTF8(aPrincipalName).get()));
      return;
    }
    if (aPrincipal->GetIsExpandedPrincipal()) {
      nsCOMPtr<nsIExpandedPrincipal> expanded(do_QueryInterface(aPrincipal));
      nsAutoCString origin;
      origin.AssignLiteral("[Expanded Principal [");

      StringJoinAppend(origin, ", "_ns, expanded->AllowList(),
                       [](nsACString& dest, nsIPrincipal* principal) {
                         nsAutoCString subOrigin;
                         DebugOnly<nsresult> rv =
                             principal->GetOrigin(subOrigin);
                         MOZ_ASSERT(NS_SUCCEEDED(rv));
                         dest.Append(subOrigin);
                       });

      origin.AppendLiteral("]]");

      MOZ_LOG(sCSMLog, LogLevel::Debug,
              ("%s%s: %s\n", aIndentationString.get(),
               NS_ConvertUTF16toUTF8(aPrincipalName).get(), origin.get()));
      return;
    }
    nsAutoCString principalSpec;
    aPrincipal->GetAsciiSpec(principalSpec);
    if (aPrincipalName.IsEmpty()) {
      MOZ_LOG(sCSMLog, LogLevel::Debug,
              ("%s - \"%s\"\n", aIndentationString.get(), principalSpec.get()));
    } else {
      MOZ_LOG(
          sCSMLog, LogLevel::Debug,
          ("%s%s: \"%s\"\n", aIndentationString.get(),
           NS_ConvertUTF16toUTF8(aPrincipalName).get(), principalSpec.get()));
    }
    return;
  }
  MOZ_LOG(sCSMLog, LogLevel::Debug,
          ("%s%s: nullptr\n", aIndentationString.get(),
           NS_ConvertUTF16toUTF8(aPrincipalName).get()));
}

static void LogSecurityFlags(nsSecurityFlags securityFlags) {
  struct DebugSecFlagType {
    unsigned long secFlag;
    char secTypeStr[128];
  };
  static const DebugSecFlagType secTypes[] = {
      {nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
       "SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK"},
      {nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_INHERITS_SEC_CONTEXT,
       "SEC_REQUIRE_SAME_ORIGIN_INHERITS_SEC_CONTEXT"},
      {nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
       "SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED"},
      {nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT,
       "SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT"},
      {nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
       "SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL"},
      {nsILoadInfo::SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT,
       "SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT"},
      {nsILoadInfo::SEC_COOKIES_DEFAULT, "SEC_COOKIES_DEFAULT"},
      {nsILoadInfo::SEC_COOKIES_INCLUDE, "SEC_COOKIES_INCLUDE"},
      {nsILoadInfo::SEC_COOKIES_SAME_ORIGIN, "SEC_COOKIES_SAME_ORIGIN"},
      {nsILoadInfo::SEC_COOKIES_OMIT, "SEC_COOKIES_OMIT"},
      {nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL, "SEC_FORCE_INHERIT_PRINCIPAL"},
      {nsILoadInfo::SEC_ABOUT_BLANK_INHERITS, "SEC_ABOUT_BLANK_INHERITS"},
      {nsILoadInfo::SEC_ALLOW_CHROME, "SEC_ALLOW_CHROME"},
      {nsILoadInfo::SEC_DISALLOW_SCRIPT, "SEC_DISALLOW_SCRIPT"},
      {nsILoadInfo::SEC_DONT_FOLLOW_REDIRECTS, "SEC_DONT_FOLLOW_REDIRECTS"},
      {nsILoadInfo::SEC_LOAD_ERROR_PAGE, "SEC_LOAD_ERROR_PAGE"},
      {nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL_OVERRULE_OWNER,
       "SEC_FORCE_INHERIT_PRINCIPAL_OVERRULE_OWNER"}};

  for (const DebugSecFlagType& flag : secTypes) {
    if (securityFlags & flag.secFlag) {
      // the logging level should be in sync with the logging level in
      // DebugDoContentSecurityCheck()
      MOZ_LOG(sCSMLog, LogLevel::Verbose, ("    - %s\n", flag.secTypeStr));
    }
  }
}
static void DebugDoContentSecurityCheck(nsIChannel* aChannel,
                                        nsILoadInfo* aLoadInfo) {
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));

  MOZ_LOG(sCSMLog, LogLevel::Debug, ("\n#DebugDoContentSecurityCheck Begin\n"));

  // we only log http channels, unless loglevel is 5.
  if (httpChannel || MOZ_LOG_TEST(sCSMLog, LogLevel::Verbose)) {
    MOZ_LOG(sCSMLog, LogLevel::Verbose, ("doContentSecurityCheck:\n"));

    nsAutoCString remoteType;
    if (XRE_IsParentProcess()) {
      nsCOMPtr<nsIParentChannel> parentChannel;
      NS_QueryNotificationCallbacks(aChannel, parentChannel);
      if (parentChannel) {
        parentChannel->GetRemoteType(remoteType);
      }
    } else {
      remoteType.Assign(
          mozilla::dom::ContentChild::GetSingleton()->GetRemoteType());
    }
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("  processType: \"%s\"\n", remoteType.get()));

    nsCOMPtr<nsIURI> channelURI;
    nsAutoCString channelSpec;
    nsAutoCString channelMethod;
    NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
    if (channelURI) {
      channelURI->GetSpec(channelSpec);
    }
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("  channelURI: \"%s\"\n", channelSpec.get()));

    // Log HTTP-specific things
    if (httpChannel) {
      nsresult rv;
      rv = httpChannel->GetRequestMethod(channelMethod);
      if (!NS_FAILED(rv)) {
        MOZ_LOG(sCSMLog, LogLevel::Verbose,
                ("  httpMethod: %s\n", channelMethod.get()));
      }
    }

    // Log Principals
    nsCOMPtr<nsIPrincipal> requestPrincipal = aLoadInfo->TriggeringPrincipal();
    LogPrincipal(aLoadInfo->GetLoadingPrincipal(), u"loadingPrincipal"_ns, 1);
    LogPrincipal(requestPrincipal, u"triggeringPrincipal"_ns, 1);
    LogPrincipal(aLoadInfo->PrincipalToInherit(), u"principalToInherit"_ns, 1);

    // Log Redirect Chain
    MOZ_LOG(sCSMLog, LogLevel::Verbose, ("  redirectChain:\n"));
    for (nsIRedirectHistoryEntry* redirectHistoryEntry :
         aLoadInfo->RedirectChain()) {
      nsCOMPtr<nsIPrincipal> principal;
      redirectHistoryEntry->GetPrincipal(getter_AddRefs(principal));
      LogPrincipal(principal, u""_ns, 2);
    }

    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("  internalContentPolicyType: %s\n",
             NS_CP_ContentTypeName(aLoadInfo->InternalContentPolicyType())));
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("  externalContentPolicyType: %s\n",
             NS_CP_ContentTypeName(aLoadInfo->GetExternalContentPolicyType())));
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("  upgradeInsecureRequests: %s\n",
             aLoadInfo->GetUpgradeInsecureRequests() ? "true" : "false"));
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("  initialSecurityChecksDone: %s\n",
             aLoadInfo->GetInitialSecurityCheckDone() ? "true" : "false"));
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("  allowDeprecatedSystemRequests: %s\n",
             aLoadInfo->GetAllowDeprecatedSystemRequests() ? "true" : "false"));

    // Log CSPrequestPrincipal
    nsCOMPtr<nsIContentSecurityPolicy> csp = aLoadInfo->GetCsp();
    MOZ_LOG(sCSMLog, LogLevel::Debug, ("  CSP:"));
    if (csp) {
      nsAutoString parsedPolicyStr;
      uint32_t count = 0;
      csp->GetPolicyCount(&count);
      for (uint32_t i = 0; i < count; ++i) {
        csp->GetPolicyString(i, parsedPolicyStr);
        // we need to add quotation marks, as otherwise yaml parsers may fail
        // with CSP directives
        // no need to escape quote marks in the parsed policy string, as URLs in
        // there are already encoded
        MOZ_LOG(sCSMLog, LogLevel::Debug,
                ("  - \"%s\"\n", NS_ConvertUTF16toUTF8(parsedPolicyStr).get()));
      }
    }

    // Security Flags
    MOZ_LOG(sCSMLog, LogLevel::Verbose, ("  securityFlags:"));
    LogSecurityFlags(aLoadInfo->GetSecurityFlags());
    LogHTTPSOnlyInfo(aLoadInfo);
    MOZ_LOG(sCSMLog, LogLevel::Debug, ("\n#DebugDoContentSecurityCheck End\n"));
  }
}

/* static */
void nsContentSecurityManager::MeasureUnexpectedPrivilegedLoads(
    nsILoadInfo* aLoadInfo, nsIURI* aFinalURI, const nsACString& aRemoteType) {
  if (!StaticPrefs::dom_security_unexpected_system_load_telemetry_enabled()) {
    return;
  }
  nsContentSecurityUtils::DetectJsHacks();
  nsContentSecurityUtils::DetectCssHacks();
  // The detection only work on the main-thread.
  // To avoid races and early reports, we need to ensure the checks actually
  // happened.
  if (MOZ_UNLIKELY(sJSHacksPresent || !sJSHacksChecked || sCSSHacksPresent ||
                   !sCSSHacksChecked)) {
    return;
  }

  ExtContentPolicyType contentPolicyType =
      aLoadInfo->GetExternalContentPolicyType();
  // restricting reported types to script, styles and documents
  // to be continued in follow-ups of bug 1697163.
  if (contentPolicyType != ExtContentPolicyType::TYPE_SCRIPT &&
      contentPolicyType != ExtContentPolicyType::TYPE_STYLESHEET &&
      contentPolicyType != ExtContentPolicyType::TYPE_DOCUMENT) {
    return;
  }

  // Gather redirected schemes in string
  nsAutoCString loggedRedirects;
  const nsTArray<nsCOMPtr<nsIRedirectHistoryEntry>>& redirects =
      aLoadInfo->RedirectChain();
  if (!redirects.IsEmpty()) {
    nsCOMPtr<nsIRedirectHistoryEntry> end = redirects.LastElement();
    for (nsIRedirectHistoryEntry* entry : redirects) {
      nsCOMPtr<nsIPrincipal> principal;
      entry->GetPrincipal(getter_AddRefs(principal));
      if (principal) {
        nsAutoCString scheme;
        principal->GetScheme(scheme);
        loggedRedirects.Append(scheme);
        if (entry != end) {
          loggedRedirects.AppendLiteral(", ");
        }
      }
    }
  }

  nsAutoCString uriString;
  if (aFinalURI) {
    aFinalURI->GetAsciiSpec(uriString);
  } else {
    uriString.AssignLiteral("");
  }
  FilenameTypeAndDetails fileNameTypeAndDetails =
      nsContentSecurityUtils::FilenameToFilenameType(
          NS_ConvertUTF8toUTF16(uriString), true);

  nsCString loggedFileDetails = "unknown"_ns;
  if (fileNameTypeAndDetails.second.isSome()) {
    loggedFileDetails.Assign(
        NS_ConvertUTF16toUTF8(fileNameTypeAndDetails.second.value()));
  }
  // sanitize remoteType because it may contain sensitive
  // info, like URLs. e.g. `webIsolated=https://example.com`
  nsAutoCString loggedRemoteType(dom::RemoteTypePrefix(aRemoteType));
  nsAutoCString loggedContentType(NS_CP_ContentTypeName(contentPolicyType));

  MOZ_LOG(sCSMLog, LogLevel::Debug, ("UnexpectedPrivilegedLoadTelemetry:\n"));
  MOZ_LOG(sCSMLog, LogLevel::Debug,
          ("- contentType: %s\n", loggedContentType.get()));
  MOZ_LOG(sCSMLog, LogLevel::Debug,
          ("- URL (not to be reported): %s\n", uriString.get()));
  MOZ_LOG(sCSMLog, LogLevel::Debug,
          ("- remoteType: %s\n", loggedRemoteType.get()));
  MOZ_LOG(sCSMLog, LogLevel::Debug,
          ("- fileInfo: %s\n", fileNameTypeAndDetails.first.get()));
  MOZ_LOG(sCSMLog, LogLevel::Debug,
          ("- fileDetails: %s\n", loggedFileDetails.get()));
  MOZ_LOG(sCSMLog, LogLevel::Debug,
          ("- redirects: %s\n\n", loggedRedirects.get()));

  // Send Telemetry
  auto extra = Some<nsTArray<EventExtraEntry>>(
      {EventExtraEntry{"contenttype"_ns, loggedContentType},
       EventExtraEntry{"remotetype"_ns, loggedRemoteType},
       EventExtraEntry{"filedetails"_ns, loggedFileDetails},
       EventExtraEntry{"redirects"_ns, loggedRedirects}});

  if (!sTelemetryEventEnabled.exchange(true)) {
    Telemetry::SetEventRecordingEnabled("security"_ns, true);
  }

  Telemetry::EventID eventType =
      Telemetry::EventID::Security_Unexpectedload_Systemprincipal;
  Telemetry::RecordEvent(eventType, mozilla::Some(fileNameTypeAndDetails.first),
                         extra);
}

/* static */
nsresult nsContentSecurityManager::CheckAllowLoadInSystemPrivilegedContext(
    nsIChannel* aChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsCOMPtr<nsIPrincipal> inspectedPrincipal = loadInfo->GetLoadingPrincipal();
  if (!inspectedPrincipal) {
    return NS_OK;
  }
  // Check if we are actually dealing with a privileged request
  if (!inspectedPrincipal->IsSystemPrincipal()) {
    return NS_OK;
  }
  // loads with the allow flag are waived through
  // until refactored (e.g., Shavar, OCSP)
  if (loadInfo->GetAllowDeprecatedSystemRequests()) {
    return NS_OK;
  }
  ExtContentPolicyType contentPolicyType =
      loadInfo->GetExternalContentPolicyType();
  // For now, let's not inspect top-level document loads
  if (contentPolicyType == ExtContentPolicy::TYPE_DOCUMENT) {
    return NS_OK;
  }

  // allowing some fetches due to their lowered risk
  // i.e., data & downloads fetches do limited parsing, no rendering
  // remote images are too widely used (favicons, about:addons etc.)
  if ((contentPolicyType == ExtContentPolicy::TYPE_FETCH) ||
      (contentPolicyType == ExtContentPolicy::TYPE_XMLHTTPREQUEST) ||
      (contentPolicyType == ExtContentPolicy::TYPE_WEBSOCKET) ||
      (contentPolicyType == ExtContentPolicy::TYPE_SAVEAS_DOWNLOAD) ||
      (contentPolicyType == ExtContentPolicy::TYPE_IMAGE)) {
    return NS_OK;
  }

  // Allow the user interface (e.g., schemes like chrome, resource)
  nsCOMPtr<nsIURI> finalURI;
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(finalURI));
  bool isUiResource = false;
  if (NS_SUCCEEDED(NS_URIChainHasFlags(
          finalURI, nsIProtocolHandler::URI_IS_UI_RESOURCE, &isUiResource)) &&
      isUiResource) {
    return NS_OK;
  }
  // For about: and extension-based URIs, which don't get
  // URI_IS_UI_RESOURCE, first remove layers of view-source:, if present.
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(finalURI);

  nsAutoCString remoteType;
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIParentChannel> parentChannel;
    NS_QueryNotificationCallbacks(aChannel, parentChannel);
    if (parentChannel) {
      parentChannel->GetRemoteType(remoteType);
    }
  } else {
    remoteType.Assign(
        mozilla::dom::ContentChild::GetSingleton()->GetRemoteType());
  }

  // GetInnerURI can return null for malformed nested URIs like moz-icon:trash
  if (!innerURI) {
    MeasureUnexpectedPrivilegedLoads(loadInfo, innerURI, remoteType);
    if (StaticPrefs::security_disallow_privileged_no_finaluri_loads()) {
      aChannel->Cancel(NS_ERROR_CONTENT_BLOCKED);
      return NS_ERROR_CONTENT_BLOCKED;
    }
    return NS_OK;
  }
  // loads of userContent.css during startup and tests that show up as file:
  if (innerURI->SchemeIs("file")) {
    if ((contentPolicyType == ExtContentPolicy::TYPE_STYLESHEET) ||
        (contentPolicyType == ExtContentPolicy::TYPE_OTHER)) {
      return NS_OK;
    }
  }
  // (1) loads from within omni.ja and system add-ons use jar:
  // this is safe to allow, because we do not support remote jar.
  // (2) about: resources are always allowed: they are part of the build.
  // (3) extensions are signed or the user has made bad decisions.
  if (innerURI->SchemeIs("jar") || innerURI->SchemeIs("about") ||
      innerURI->SchemeIs("moz-extension")) {
    return NS_OK;
  }

  nsAutoCString requestedURL;
  innerURI->GetAsciiSpec(requestedURL);
  MOZ_LOG(sCSMLog, LogLevel::Warning,
          ("SystemPrincipal should not load remote resources. URL: %s, type %d",
           requestedURL.get(), int(contentPolicyType)));

  // The load types that we want to disallow, will extend over time and
  // prioritized by risk. The most risky/dangerous are load-types are documents,
  // subdocuments, scripts and styles in that order. The most dangerous URL
  // schemes to cover are HTTP, HTTPS, data, blob in that order. Meta bug
  // 1725112 will track upcoming restrictions

  // Telemetry for unexpected privileged loads.
  // pref check & data sanitization happens in the called function
  MeasureUnexpectedPrivilegedLoads(loadInfo, innerURI, remoteType);

  // Relaxing restrictions for our test suites:
  // (1) AreNonLocalConnectionsDisabled() disables network, so
  // http://mochitest is actually local and allowed. (2) The marionette test
  // framework uses injections and data URLs to execute scripts, checking for
  // the environment variable breaks the attack but not the tests.
  if (xpc::AreNonLocalConnectionsDisabled() ||
      mozilla::EnvHasValue("MOZ_MARIONETTE")) {
    bool disallowSystemPrincipalRemoteDocuments = Preferences::GetBool(
        "security.disallow_non_local_systemprincipal_in_tests");
    if (disallowSystemPrincipalRemoteDocuments) {
      // our own mochitest needs NS_ASSERTION instead of MOZ_ASSERT
      NS_ASSERTION(false, "SystemPrincipal must not load remote documents.");
      aChannel->Cancel(NS_ERROR_CONTENT_BLOCKED);
      return NS_ERROR_CONTENT_BLOCKED;
    }
    // but other mochitest are exempt from this
    return NS_OK;
  }

  if (contentPolicyType == ExtContentPolicy::TYPE_SUBDOCUMENT) {
    if (StaticPrefs::security_disallow_privileged_https_subdocuments_loads() &&
        (innerURI->SchemeIs("http") || innerURI->SchemeIs("https"))) {
      MOZ_ASSERT(
          false,
          "Disallowing SystemPrincipal load of subdocuments on HTTP(S).");
      aChannel->Cancel(NS_ERROR_CONTENT_BLOCKED);
      return NS_ERROR_CONTENT_BLOCKED;
    }
    if ((StaticPrefs::security_disallow_privileged_data_subdocuments_loads()) &&
        (innerURI->SchemeIs("data"))) {
      MOZ_ASSERT(
          false,
          "Disallowing SystemPrincipal load of subdocuments on data URL.");
      aChannel->Cancel(NS_ERROR_CONTENT_BLOCKED);
      return NS_ERROR_CONTENT_BLOCKED;
    }
  }
  if (contentPolicyType == ExtContentPolicy::TYPE_SCRIPT) {
    if ((StaticPrefs::security_disallow_privileged_https_script_loads()) &&
        (innerURI->SchemeIs("http") || innerURI->SchemeIs("https"))) {
      MOZ_ASSERT(false,
                 "Disallowing SystemPrincipal load of scripts on HTTP(S).");
      aChannel->Cancel(NS_ERROR_CONTENT_BLOCKED);
      return NS_ERROR_CONTENT_BLOCKED;
    }
  }
  if (contentPolicyType == ExtContentPolicy::TYPE_STYLESHEET) {
    if (StaticPrefs::security_disallow_privileged_https_stylesheet_loads() &&
        (innerURI->SchemeIs("http") || innerURI->SchemeIs("https"))) {
      MOZ_ASSERT(false,
                 "Disallowing SystemPrincipal load of stylesheets on HTTP(S).");
      aChannel->Cancel(NS_ERROR_CONTENT_BLOCKED);
      return NS_ERROR_CONTENT_BLOCKED;
    }
  }
  return NS_OK;
}

/*
 * Disallow about pages in the privilegedaboutcontext (e.g., password manager,
 * newtab etc.) to load remote scripts. Regardless of whether this is coming
 * from the contentprincipal or the systemprincipal.
 */
/* static */
nsresult nsContentSecurityManager::CheckAllowLoadInPrivilegedAboutContext(
    nsIChannel* aChannel) {
  // bail out if check is disabled
  if (StaticPrefs::security_disallow_privilegedabout_remote_script_loads()) {
    return NS_OK;
  }

  nsAutoCString remoteType;
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIParentChannel> parentChannel;
    NS_QueryNotificationCallbacks(aChannel, parentChannel);
    if (parentChannel) {
      parentChannel->GetRemoteType(remoteType);
    }
  } else {
    remoteType.Assign(
        mozilla::dom::ContentChild::GetSingleton()->GetRemoteType());
  }

  // only perform check for privileged about process
  if (!remoteType.Equals(PRIVILEGEDABOUT_REMOTE_TYPE)) {
    return NS_OK;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  ExtContentPolicyType contentPolicyType =
      loadInfo->GetExternalContentPolicyType();
  // only check for script loads
  if (contentPolicyType != ExtContentPolicy::TYPE_SCRIPT) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> finalURI;
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(finalURI));
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(finalURI);

  bool isLocal;
  NS_URIChainHasFlags(innerURI, nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                      &isLocal);
  // We allow URLs that are URI_IS_LOCAL (but that includes `data`
  // and `blob` which are also undesirable.
  if ((isLocal) && (!innerURI->SchemeIs("data")) &&
      (!innerURI->SchemeIs("blob"))) {
    return NS_OK;
  }
  MOZ_ASSERT(
      false,
      "Disallowing privileged about process to load scripts on HTTP(S).");
  aChannel->Cancel(NS_ERROR_CONTENT_BLOCKED);
  return NS_ERROR_CONTENT_BLOCKED;
}

/*
 * Every protocol handler must set one of the five security flags
 * defined in nsIProtocolHandler - if not - deny the load.
 */
nsresult nsContentSecurityManager::CheckChannelHasProtocolSecurityFlag(
    nsIChannel* aChannel) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString scheme;
  rv = uri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIProtocolHandler> handler;
  rv = ios->GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t flags;
  rv = handler->DoGetProtocolFlags(uri, &flags);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t securityFlagsSet = 0;
  if (flags & nsIProtocolHandler::URI_LOADABLE_BY_ANYONE) {
    securityFlagsSet += 1;
  }
  if (flags & nsIProtocolHandler::URI_DANGEROUS_TO_LOAD) {
    securityFlagsSet += 1;
  }
  if (flags & nsIProtocolHandler::URI_IS_UI_RESOURCE) {
    securityFlagsSet += 1;
  }
  if (flags & nsIProtocolHandler::URI_IS_LOCAL_FILE) {
    securityFlagsSet += 1;
  }
  if (flags & nsIProtocolHandler::URI_LOADABLE_BY_SUBSUMERS) {
    securityFlagsSet += 1;
  }

  // Ensure that only "1" valid security flags is set.
  if (securityFlagsSet == 1) {
    return NS_OK;
  }

  MOZ_ASSERT(false, "protocol must use one valid security flag");
  return NS_ERROR_CONTENT_BLOCKED;
}

// We should not allow loading non-JavaScript files as scripts using
// a file:// URL.
static nsresult CheckAllowFileProtocolScriptLoad(nsIChannel* aChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  ExtContentPolicyType type = loadInfo->GetExternalContentPolicyType();

  // Only check script loads.
  if (type != ExtContentPolicy::TYPE_SCRIPT) {
    return NS_OK;
  }

  if (!StaticPrefs::security_block_fileuri_script_with_wrong_mime()) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!uri || !uri->SchemeIs("file")) {
    return NS_OK;
  }

  nsCOMPtr<nsIMIMEService> mime = do_GetService("@mozilla.org/mime;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // GetTypeFromURI fails for missing or unknown file-extensions.
  nsAutoCString contentType;
  rv = mime->GetTypeFromURI(uri, contentType);
  if (NS_FAILED(rv) || !nsContentUtils::IsJavascriptMIMEType(
                           NS_ConvertUTF8toUTF16(contentType))) {
    Telemetry::Accumulate(Telemetry::SCRIPT_FILE_PROTOCOL_CORRECT_MIME, false);

    nsCOMPtr<Document> doc;
    if (nsINode* node = loadInfo->LoadingNode()) {
      doc = node->OwnerDoc();
    }

    nsAutoCString spec;
    uri->GetSpec(spec);

    AutoTArray<nsString, 1> params;
    CopyUTF8toUTF16(NS_UnescapeURL(spec), *params.AppendElement());
    CopyUTF8toUTF16(NS_UnescapeURL(contentType), *params.AppendElement());

    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    "FILE_SCRIPT_BLOCKED"_ns, doc,
                                    nsContentUtils::eSECURITY_PROPERTIES,
                                    "BlockFileScriptWithWrongMimeType", params);

    return NS_ERROR_CONTENT_BLOCKED;
  }

  Telemetry::Accumulate(Telemetry::SCRIPT_FILE_PROTOCOL_CORRECT_MIME, true);
  return NS_OK;
}

/*
 * Based on the security flags provided in the loadInfo of the channel,
 * doContentSecurityCheck() performs the following content security checks
 * before opening the channel:
 *
 * (1) Same Origin Policy Check (if applicable)
 * (2) Allow Cross Origin but perform sanity checks whether a principal
 *     is allowed to access the following URL.
 * (3) Perform CORS check (if applicable)
 * (4) ContentPolicy checks (Content-Security-Policy, Mixed Content, ...)
 *
 * @param aChannel
 *    The channel to perform the security checks on.
 * @param aInAndOutListener
 *    The streamListener that is passed to channel->AsyncOpen() that is now
 * potentially wrappend within nsCORSListenerProxy() and becomes the
 * corsListener that now needs to be set as new streamListener on the channel.
 */
nsresult nsContentSecurityManager::doContentSecurityCheck(
    nsIChannel* aChannel, nsCOMPtr<nsIStreamListener>& aInAndOutListener) {
  NS_ENSURE_ARG(aChannel);
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  if (MOZ_UNLIKELY(MOZ_LOG_TEST(sCSMLog, LogLevel::Verbose))) {
    DebugDoContentSecurityCheck(aChannel, loadInfo);
  }

  nsresult rv = CheckAllowLoadInSystemPrivilegedContext(aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CheckAllowLoadInPrivilegedAboutContext(aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CheckChannelHasProtocolSecurityFlag(aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  // if dealing with a redirected channel then we have already installed
  // streamlistener and redirect proxies and so we are done.
  if (loadInfo->GetInitialSecurityCheckDone()) {
    return NS_OK;
  }

  // make sure that only one of the five security flags is set in the loadinfo
  // e.g. do not require same origin and allow cross origin at the same time
  rv = ValidateSecurityFlags(loadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  if (loadInfo->GetSecurityMode() ==
      nsILoadInfo::SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT) {
    rv = DoCORSChecks(aChannel, loadInfo, aInAndOutListener);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = CheckChannel(aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  // Perform all ContentPolicy checks (MixedContent, CSP, ...)
  rv = DoContentSecurityChecks(aChannel, loadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // Apply this after CSP to match Chrome.
  rv = CheckFTPSubresourceLoad(aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CheckAllowFileProtocolScriptLoad(aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  // now lets set the initialSecurityFlag for subsequent calls
  loadInfo->SetInitialSecurityCheckDone(true);

  // all security checks passed - lets allow the load
  return NS_OK;
}

NS_IMETHODIMP
nsContentSecurityManager::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aRedirFlags,
    nsIAsyncVerifyRedirectCallback* aCb) {
  // Since we compare the principal from the loadInfo to the URI's
  // princicpal, it's possible that the checks fail when doing an internal
  // redirect. We can just return early instead, since we should never
  // need to block an internal redirect.
  if (aRedirFlags & nsIChannelEventSink::REDIRECT_INTERNAL) {
    aCb->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aOldChannel->LoadInfo();
  nsresult rv = CheckChannel(aNewChannel);
  if (NS_SUCCEEDED(rv)) {
    rv = CheckFTPSubresourceLoad(aNewChannel);
  }
  if (NS_FAILED(rv)) {
    aOldChannel->Cancel(rv);
    return rv;
  }

  // Also verify that the redirecting server is allowed to redirect to the
  // given URI
  nsCOMPtr<nsIPrincipal> oldPrincipal;
  nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
      aOldChannel, getter_AddRefs(oldPrincipal));

  nsCOMPtr<nsIURI> newURI;
  Unused << NS_GetFinalChannelURI(aNewChannel, getter_AddRefs(newURI));
  NS_ENSURE_STATE(oldPrincipal && newURI);

  // Do not allow insecure redirects to data: URIs
  if (!AllowInsecureRedirectToDataURI(aNewChannel)) {
    // cancel the old channel and return an error
    aOldChannel->Cancel(NS_ERROR_CONTENT_BLOCKED);
    return NS_ERROR_CONTENT_BLOCKED;
  }

  const uint32_t flags =
      nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT |
      nsIScriptSecurityManager::DISALLOW_SCRIPT;
  rv = nsContentUtils::GetSecurityManager()->CheckLoadURIWithPrincipal(
      oldPrincipal, newURI, flags, loadInfo->GetInnerWindowID());
  NS_ENSURE_SUCCESS(rv, rv);

  aCb->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

static void AddLoadFlags(nsIRequest* aRequest, nsLoadFlags aNewFlags) {
  nsLoadFlags flags;
  aRequest->GetLoadFlags(&flags);
  flags |= aNewFlags;
  aRequest->SetLoadFlags(flags);
}

/*
 * Check that this channel passes all security checks. Returns an error code
 * if this requesst should not be permitted.
 */
nsresult nsContentSecurityManager::CheckChannel(nsIChannel* aChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  // Handle cookie policies
  uint32_t cookiePolicy = loadInfo->GetCookiePolicy();
  if (cookiePolicy == nsILoadInfo::SEC_COOKIES_SAME_ORIGIN) {
    // We shouldn't have the SEC_COOKIES_SAME_ORIGIN flag for top level loads
    MOZ_ASSERT(loadInfo->GetExternalContentPolicyType() !=
               ExtContentPolicy::TYPE_DOCUMENT);
    nsIPrincipal* loadingPrincipal = loadInfo->GetLoadingPrincipal();

    // It doesn't matter what we pass for the second, data-inherits, argument.
    // Any protocol which inherits won't pay attention to cookies anyway.
    rv = loadingPrincipal->CheckMayLoad(uri, false);
    if (NS_FAILED(rv)) {
      AddLoadFlags(aChannel, nsIRequest::LOAD_ANONYMOUS);
    }
  } else if (cookiePolicy == nsILoadInfo::SEC_COOKIES_OMIT) {
    AddLoadFlags(aChannel, nsIRequest::LOAD_ANONYMOUS);
  }

  nsSecurityFlags securityMode = loadInfo->GetSecurityMode();

  // CORS mode is handled by nsCORSListenerProxy
  if (securityMode == nsILoadInfo::SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT) {
    if (NS_HasBeenCrossOrigin(aChannel)) {
      loadInfo->MaybeIncreaseTainting(LoadTainting::CORS);
    }
    return NS_OK;
  }

  // Allow subresource loads if TriggeringPrincipal is the SystemPrincipal.
  if (loadInfo->TriggeringPrincipal()->IsSystemPrincipal() &&
      loadInfo->GetExternalContentPolicyType() !=
          ExtContentPolicy::TYPE_DOCUMENT &&
      loadInfo->GetExternalContentPolicyType() !=
          ExtContentPolicy::TYPE_SUBDOCUMENT) {
    return NS_OK;
  }

  // if none of the REQUIRE_SAME_ORIGIN flags are set, then SOP does not apply
  if ((securityMode ==
       nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_INHERITS_SEC_CONTEXT) ||
      (securityMode == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED)) {
    rv = DoSOPChecks(uri, loadInfo, aChannel);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if ((securityMode ==
       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT) ||
      (securityMode ==
       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL)) {
    if (NS_HasBeenCrossOrigin(aChannel)) {
      NS_ENSURE_FALSE(loadInfo->GetDontFollowRedirects(), NS_ERROR_DOM_BAD_URI);
      loadInfo->MaybeIncreaseTainting(LoadTainting::Opaque);
    }
    // Please note that DoCheckLoadURIChecks should only be enforced for
    // cross origin requests. If the flag SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT
    // is set within the loadInfo, then CheckLoadURIWithPrincipal is performed
    // within nsCorsListenerProxy
    rv = DoCheckLoadURIChecks(uri, loadInfo);
    NS_ENSURE_SUCCESS(rv, rv);
    // TODO: Bug 1371237
    // consider calling SetBlockedRequest in
    // nsContentSecurityManager::CheckChannel
  }

  return NS_OK;
}

// ==== nsIContentSecurityManager implementation =====

NS_IMETHODIMP
nsContentSecurityManager::PerformSecurityCheck(
    nsIChannel* aChannel, nsIStreamListener* aStreamListener,
    nsIStreamListener** outStreamListener) {
  nsCOMPtr<nsIStreamListener> inAndOutListener = aStreamListener;
  nsresult rv = doContentSecurityCheck(aChannel, inAndOutListener);
  NS_ENSURE_SUCCESS(rv, rv);

  inAndOutListener.forget(outStreamListener);
  return NS_OK;
}
