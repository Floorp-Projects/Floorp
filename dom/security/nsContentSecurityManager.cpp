/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentSecurityManager.h"
#include "nsEscape.h"
#include "nsDataHandler.h"
#include "nsIChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsINode.h"
#include "nsIStreamListener.h"
#include "nsILoadInfo.h"
#include "nsIOService.h"
#include "nsContentUtils.h"
#include "nsCORSListenerProxy.h"
#include "nsIStreamListener.h"
#include "nsIURIFixup.h"
#include "nsIImageLoadingContent.h"
#include "nsIRedirectHistoryEntry.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/Components.h"
#include "mozilla/Logging.h"
#include "xpcpublic.h"

NS_IMPL_ISUPPORTS(nsContentSecurityManager, nsIContentSecurityManager,
                  nsIChannelEventSink)

static mozilla::LazyLogModule sCSMLog("CSMLog");

/* static */
bool nsContentSecurityManager::AllowTopLevelNavigationToDataURI(
    nsIChannel* aChannel) {
  // Let's block all toplevel document navigations to a data: URI.
  // In all cases where the toplevel document is navigated to a
  // data: URI the triggeringPrincipal is a codeBasePrincipal, or
  // a NullPrincipal. In other cases, e.g. typing a data: URL into
  // the URL-Bar, the triggeringPrincipal is a SystemPrincipal;
  // we don't want to block those loads. Only exception, loads coming
  // from an external applicaton (e.g. Thunderbird) don't load
  // using a codeBasePrincipal, but we want to block those loads.
  if (!mozilla::net::nsIOService::BlockToplevelDataUriNavigations()) {
    return true;
  }
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  if (loadInfo->GetExternalContentPolicyType() !=
      nsIContentPolicy::TYPE_DOCUMENT) {
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
  bool isDataURI =
      (NS_SUCCEEDED(uri->SchemeIs("data", &isDataURI)) && isDataURI);
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

  // Whitelist data: images as long as they are not SVGs
  if (StringBeginsWith(contentType, NS_LITERAL_CSTRING("image/")) &&
      !contentType.EqualsLiteral("image/svg+xml")) {
    return true;
  }
  // Whitelist all plain text types as well as data: PDFs.
  if (nsContentUtils::IsPlainTextType(contentType) ||
      contentType.EqualsLiteral("application/pdf")) {
    return true;
  }
  // Redirecting to a toplevel data: URI is not allowed, hence we make
  // sure the RedirectChain is empty.
  if (!loadInfo->GetLoadTriggeredFromExternal() &&
      nsContentUtils::IsSystemPrincipal(loadInfo->TriggeringPrincipal()) &&
      loadInfo->RedirectChain().IsEmpty()) {
    return true;
  }
  nsAutoCString dataSpec;
  uri->GetSpec(dataSpec);
  if (dataSpec.Length() > 50) {
    dataSpec.Truncate(50);
    dataSpec.AppendLiteral("...");
  }
  nsCOMPtr<nsISupports> context = loadInfo->ContextForTopLevelLoad();
  nsCOMPtr<nsIBrowserChild> browserChild = do_QueryInterface(context);
  nsCOMPtr<Document> doc;
  if (browserChild) {
    doc = static_cast<mozilla::dom::BrowserChild*>(browserChild.get())
              ->GetTopLevelDocument();
  }
  NS_ConvertUTF8toUTF16 specUTF16(NS_UnescapeURL(dataSpec));
  const char16_t* params[] = {specUTF16.get()};
  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, NS_LITERAL_CSTRING("DATA_URI_BLOCKED"), doc,
      nsContentUtils::eSECURITY_PROPERTIES, "BlockTopLevelDataURINavigation",
      params, ArrayLength(params));
  return false;
}

/* static */
bool nsContentSecurityManager::AllowInsecureRedirectToDataURI(
    nsIChannel* aNewChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aNewChannel->LoadInfo();
  if (loadInfo->GetExternalContentPolicyType() !=
      nsIContentPolicy::TYPE_SCRIPT) {
    return true;
  }
  nsCOMPtr<nsIURI> newURI;
  nsresult rv = NS_GetFinalChannelURI(aNewChannel, getter_AddRefs(newURI));
  if (NS_FAILED(rv) || !newURI) {
    return true;
  }
  bool isDataURI =
      (NS_SUCCEEDED(newURI->SchemeIs("data", &isDataURI)) && isDataURI);
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
  NS_ConvertUTF8toUTF16 specUTF16(NS_UnescapeURL(dataSpec));
  const char16_t* params[] = {specUTF16.get()};
  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, NS_LITERAL_CSTRING("DATA_URI_BLOCKED"), doc,
      nsContentUtils::eSECURITY_PROPERTIES, "BlockSubresourceRedirectToData",
      params, ArrayLength(params));
  return false;
}

/* static */
void nsContentSecurityManager::AssertEvalNotUsingSystemPrincipal(
    nsIPrincipal* subjectPrincipal, JSContext* cx) {
  if (!subjectPrincipal->IsSystemPrincipal()) {
    return;
  }

  if (Preferences::GetBool("security.allow_eval_with_system_principal")) {
    return;
  }

  static StaticAutoPtr<nsTArray<nsCString>> sUrisAllowEval;
  JS::AutoFilename scriptFilename;
  if (JS::DescribeScriptedCaller(cx, &scriptFilename)) {
    if (!sUrisAllowEval) {
      sUrisAllowEval = new nsTArray<nsCString>();
      nsAutoCString urisAllowEval;
      Preferences::GetCString("security.uris_using_eval_with_system_principal",
                              urisAllowEval);
      for (const nsACString& filenameString : urisAllowEval.Split(',')) {
        sUrisAllowEval->AppendElement(filenameString);
      }
      ClearOnShutdown(&sUrisAllowEval);
    }

    nsAutoCString fileName;
    fileName = nsAutoCString(scriptFilename.get());
    // Extract file name alone if scriptFilename contains line number
    // separated by multiple space delimiters in few cases.
    int32_t fileNameIndex = fileName.FindChar(' ');
    if (fileNameIndex != -1) {
      fileName = Substring(fileName, 0, fileNameIndex);
    }
    ToLowerCase(fileName);

    for (auto& uriEntry : *sUrisAllowEval) {
      if (StringEndsWith(fileName, uriEntry)) {
        return;
      }
    }
  }

  MOZ_ASSERT(false, "do not use eval with system privileges");
}

/* static */
nsresult nsContentSecurityManager::CheckFTPSubresourceLoad(
    nsIChannel* aChannel) {
  // We dissallow using FTP resources as a subresource almost everywhere.
  // The only valid way to use FTP resources is loading it as
  // a top level document.
  if (!mozilla::net::nsIOService::BlockFTPSubresources()) {
    return NS_OK;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsContentPolicyType type = loadInfo->GetExternalContentPolicyType();

  // Allow top-level FTP documents and save-as download of FTP files on
  // HTTP pages.
  if (type == nsIContentPolicy::TYPE_DOCUMENT ||
      type == nsIContentPolicy::TYPE_SAVEAS_DOWNLOAD) {
    return NS_OK;
  }

  // Allow the system principal to load everything. This is meant to
  // temporarily fix downloads and pdf.js.
  nsIPrincipal* triggeringPrincipal = loadInfo->TriggeringPrincipal();
  if (nsContentUtils::IsSystemPrincipal(triggeringPrincipal)) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!uri) {
    return NS_OK;
  }

  bool isFtpURI = (NS_SUCCEEDED(uri->SchemeIs("ftp", &isFtpURI)) && isFtpURI);
  if (!isFtpURI) {
    return NS_OK;
  }

  // Allow loading FTP subresources in FTP documents, like XML.
  nsCOMPtr<nsIURI> triggeringURI;
  triggeringPrincipal->GetURI(getter_AddRefs(triggeringURI));
  if (triggeringURI && nsContentUtils::SchemeIs(triggeringURI, "ftp")) {
    return NS_OK;
  }

  nsCOMPtr<Document> doc;
  if (nsINode* node = loadInfo->LoadingNode()) {
    doc = node->OwnerDoc();
  }

  nsAutoCString spec;
  uri->GetSpec(spec);
  NS_ConvertUTF8toUTF16 specUTF16(NS_UnescapeURL(spec));
  const char16_t* params[] = {specUTF16.get()};

  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, NS_LITERAL_CSTRING("FTP_URI_BLOCKED"), doc,
      nsContentUtils::eSECURITY_PROPERTIES, "BlockSubresourceFTP", params,
      ArrayLength(params));

  return NS_ERROR_CONTENT_BLOCKED;
}

static nsresult ValidateSecurityFlags(nsILoadInfo* aLoadInfo) {
  nsSecurityFlags securityMode = aLoadInfo->GetSecurityMode();

  // We should never perform a security check on a loadInfo that uses the flag
  // SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK, because that is only used for
  // temporary loadInfos used for explicit nsIContentPolicy checks, but never be
  // set as a security flag on an actual channel.
  if (securityMode != nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS &&
      securityMode != nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED &&
      securityMode != nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS &&
      securityMode != nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL &&
      securityMode != nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
    MOZ_ASSERT(
        false,
        "need one securityflag from nsILoadInfo to perform security checks");
    return NS_ERROR_FAILURE;
  }

  // all good, found the right security flags
  return NS_OK;
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
  docShellTreeItem->GetRootTreeItem(getter_AddRefs(root));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(root));
  if (docShell) {
    appType = docShell->GetAppType();
  }

  return appType == nsIDocShell::APP_TYPE_EDITOR;
}

static nsresult DoCheckLoadURIChecks(nsIURI* aURI, nsILoadInfo* aLoadInfo) {
  // Bug 1228117: determine the correct security policy for DTD loads
  if (aLoadInfo->GetExternalContentPolicyType() == nsIContentPolicy::TYPE_DTD) {
    return NS_OK;
  }

  if (IsImageLoadInEditorAppType(aLoadInfo)) {
    return NS_OK;
  }

  uint32_t flags = nsIScriptSecurityManager::STANDARD;
  if (aLoadInfo->GetAllowChrome()) {
    flags |= nsIScriptSecurityManager::ALLOW_CHROME;
  }
  if (aLoadInfo->GetDisallowScript()) {
    flags |= nsIScriptSecurityManager::DISALLOW_SCRIPT;
  }

  // Only call CheckLoadURIWithPrincipal() using the TriggeringPrincipal and not
  // the LoadingPrincipal when SEC_ALLOW_CROSS_ORIGIN_* security flags are set,
  // to allow, e.g. user stylesheets to load chrome:// URIs.
  return nsContentUtils::GetSecurityManager()->CheckLoadURIWithPrincipal(
      aLoadInfo->TriggeringPrincipal(), aURI, flags);
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

  NS_ENSURE_FALSE(NS_HasBeenCrossOrigin(aChannel, true), NS_ERROR_DOM_BAD_URI);

  return NS_OK;
}

static nsresult DoCORSChecks(nsIChannel* aChannel, nsILoadInfo* aLoadInfo,
                             nsCOMPtr<nsIStreamListener>& aInAndOutListener) {
  MOZ_RELEASE_ASSERT(aInAndOutListener,
                     "can not perform CORS checks without a listener");

  // No need to set up CORS if TriggeringPrincipal is the SystemPrincipal.
  // For example, allow user stylesheets to load XBL from external files
  // without requiring CORS.
  if (nsContentUtils::IsSystemPrincipal(aLoadInfo->TriggeringPrincipal())) {
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
  nsContentPolicyType contentPolicyType =
      aLoadInfo->GetExternalContentPolicyType();
  nsContentPolicyType internalContentPolicyType =
      aLoadInfo->InternalContentPolicyType();
  nsCString mimeTypeGuess;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  switch (contentPolicyType) {
    case nsIContentPolicy::TYPE_OTHER: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_SCRIPT: {
      mimeTypeGuess = NS_LITERAL_CSTRING("application/javascript");
      break;
    }

    case nsIContentPolicy::TYPE_IMAGE: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_STYLESHEET: {
      mimeTypeGuess = NS_LITERAL_CSTRING("text/css");
      break;
    }

    case nsIContentPolicy::TYPE_OBJECT: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_DOCUMENT: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_SUBDOCUMENT: {
      mimeTypeGuess = NS_LITERAL_CSTRING("text/html");
      break;
    }

    case nsIContentPolicy::TYPE_REFRESH: {
      MOZ_ASSERT(false, "contentPolicyType not supported yet");
      break;
    }

    case nsIContentPolicy::TYPE_XBL: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_PING: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_XMLHTTPREQUEST: {
      // alias nsIContentPolicy::TYPE_DATAREQUEST:
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
        mimeTypeGuess = EmptyCString();
      } else {
        MOZ_ASSERT(internalContentPolicyType ==
                       nsIContentPolicy::TYPE_INTERNAL_EVENTSOURCE,
                   "can not set mime type guess for unexpected internal type");
        mimeTypeGuess = NS_LITERAL_CSTRING(TEXT_EVENT_STREAM);
      }
      break;
    }

    case nsIContentPolicy::TYPE_OBJECT_SUBREQUEST: {
      mimeTypeGuess = EmptyCString();
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

    case nsIContentPolicy::TYPE_DTD: {
      mimeTypeGuess = EmptyCString();
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::DOCUMENT_NODE,
                   "type_dtd requires requestingContext of type Document");
      }
#endif
      break;
    }

    case nsIContentPolicy::TYPE_FONT: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_MEDIA: {
      if (internalContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_TRACK) {
        mimeTypeGuess = NS_LITERAL_CSTRING("text/vtt");
      } else {
        mimeTypeGuess = EmptyCString();
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

    case nsIContentPolicy::TYPE_WEBSOCKET: {
      // Websockets have to use the proxied URI:
      // ws:// instead of http:// for CSP checks
      nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
          do_QueryInterface(aChannel);
      MOZ_ASSERT(httpChannelInternal);
      if (httpChannelInternal) {
        rv = httpChannelInternal->GetProxyURI(getter_AddRefs(uri));
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_CSP_REPORT: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_XSLT: {
      mimeTypeGuess = NS_LITERAL_CSTRING("application/xml");
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::DOCUMENT_NODE,
                   "type_xslt requires requestingContext of type Document");
      }
#endif
      break;
    }

    case nsIContentPolicy::TYPE_BEACON: {
      mimeTypeGuess = EmptyCString();
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::DOCUMENT_NODE,
                   "type_beacon requires requestingContext of type Document");
      }
#endif
      break;
    }

    case nsIContentPolicy::TYPE_FETCH: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_IMAGESET: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_WEB_MANIFEST: {
      mimeTypeGuess = NS_LITERAL_CSTRING("application/manifest+json");
      break;
    }

    case nsIContentPolicy::TYPE_SAVEAS_DOWNLOAD: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_SPECULATIVE: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    default:
      // nsIContentPolicy::TYPE_INVALID
      MOZ_ASSERT(false,
                 "can not perform security check without a valid contentType");
  }

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(uri, aLoadInfo, mimeTypeGuess, &shouldLoad,
                                 nsContentUtils::GetContentPolicy());

  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    NS_SetRequestBlockingReasonIfNull(
        aLoadInfo, nsILoadInfo::BLOCKING_REASON_CONTENT_POLICY_GENERAL);

    if ((NS_SUCCEEDED(rv) && shouldLoad == nsIContentPolicy::REJECT_TYPE) &&
        (contentPolicyType == nsIContentPolicy::TYPE_DOCUMENT ||
         contentPolicyType == nsIContentPolicy::TYPE_SUBDOCUMENT)) {
      // for docshell loads we might have to return SHOW_ALT.
      return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
    }
    return NS_ERROR_CONTENT_BLOCKED;
  }

  return NS_OK;
}

static void LogPrincipal(nsIPrincipal* aPrincipal,
                         const nsAString& aPrincipalName) {
  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("  %s: SystemPrincipal\n",
             NS_ConvertUTF16toUTF8(aPrincipalName).get()));
    return;
  }
  if (aPrincipal) {
    if (aPrincipal->GetIsNullPrincipal()) {
      MOZ_LOG(sCSMLog, LogLevel::Debug,
              ("  %s: NullPrincipal\n",
               NS_ConvertUTF16toUTF8(aPrincipalName).get()));
      return;
    }
    if (aPrincipal->GetIsExpandedPrincipal()) {
      nsCOMPtr<nsIExpandedPrincipal> expanded(do_QueryInterface(aPrincipal));
      const nsTArray<nsCOMPtr<nsIPrincipal>>& allowList = expanded->AllowList();
      nsAutoCString origin;
      origin.AssignLiteral("[Expanded Principal [");
      for (size_t i = 0; i < allowList.Length(); ++i) {
        if (i != 0) {
          origin.AppendLiteral(", ");
        }

        nsAutoCString subOrigin;
        DebugOnly<nsresult> rv = allowList.ElementAt(i)->GetOrigin(subOrigin);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
        origin.Append(subOrigin);
      }
      origin.AppendLiteral("]]");

      MOZ_LOG(sCSMLog, LogLevel::Debug,
              ("  %s: %s\n", NS_ConvertUTF16toUTF8(aPrincipalName).get(),
               origin.get()));
      return;
    }
    nsCOMPtr<nsIURI> principalURI;
    nsAutoCString principalSpec;
    aPrincipal->GetURI(getter_AddRefs(principalURI));
    if (principalURI) {
      principalURI->GetSpec(principalSpec);
    }
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("  %s: %s\n", NS_ConvertUTF16toUTF8(aPrincipalName).get(),
             principalSpec.get()));
    return;
  }
  MOZ_LOG(sCSMLog, LogLevel::Debug,
          ("  %s: nullptr\n", NS_ConvertUTF16toUTF8(aPrincipalName).get()));
}

static void LogSecurityFlags(nsSecurityFlags securityFlags) {
  struct DebugSecFlagType {
    unsigned long secFlag;
    char secTypeStr[128];
  };
  static const DebugSecFlagType secTypes[] = {
      {nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
       "SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK"},
      {nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS,
       "SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS"},
      {nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
       "SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED"},
      {nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS,
       "SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS"},
      {nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
       "SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL"},
      {nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS,
       "SEC_REQUIRE_CORS_DATA_INHERITS"},
      {nsILoadInfo::SEC_COOKIES_DEFAULT, "SEC_COOKIES_DEFAULT"},
      {nsILoadInfo::SEC_COOKIES_INCLUDE, "SEC_COOKIES_INCLUDE"},
      {nsILoadInfo::SEC_COOKIES_SAME_ORIGIN, "SEC_COOKIES_SAME_ORIGIN"},
      {nsILoadInfo::SEC_COOKIES_OMIT, "SEC_COOKIES_OMIT"},
      {nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL, "SEC_FORCE_INHERIT_PRINCIPAL"},
      {nsILoadInfo::SEC_SANDBOXED, "SEC_SANDBOXED"},
      {nsILoadInfo::SEC_ABOUT_BLANK_INHERITS, "SEC_ABOUT_BLANK_INHERITS"},
      {nsILoadInfo::SEC_ALLOW_CHROME, "SEC_ALLOW_CHROME"},
      {nsILoadInfo::SEC_DISALLOW_SCRIPT, "SEC_DISALLOW_SCRIPT"},
      {nsILoadInfo::SEC_DONT_FOLLOW_REDIRECTS, "SEC_DONT_FOLLOW_REDIRECTS"},
      {nsILoadInfo::SEC_LOAD_ERROR_PAGE, "SEC_LOAD_ERROR_PAGE"},
      {nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL_OVERRULE_OWNER,
       "SEC_FORCE_INHERIT_PRINCIPAL_OVERRULE_OWNER"}};

  for (const DebugSecFlagType flag : secTypes) {
    if (securityFlags & flag.secFlag) {
      MOZ_LOG(sCSMLog, LogLevel::Debug, ("    %s,\n", flag.secTypeStr));
    }
  }
}
static void DebugDoContentSecurityCheck(nsIChannel* aChannel,
                                        nsILoadInfo* aLoadInfo) {
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));

  // we only log http channels, unless loglevel is 5.
  if (httpChannel || MOZ_LOG_TEST(sCSMLog, LogLevel::Verbose)) {
    nsCOMPtr<nsIURI> channelURI;
    nsAutoCString channelSpec;
    nsAutoCString channelMethod;
    NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
    if (channelURI) {
      channelURI->GetSpec(channelSpec);
    }

    MOZ_LOG(sCSMLog, LogLevel::Debug, ("doContentSecurityCheck {\n"));
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("  channelURI: %s\n", channelSpec.get()));

    // Log HTTP-specific things
    if (httpChannel) {
      nsresult rv;
      rv = httpChannel->GetRequestMethod(channelMethod);
      if (!NS_FAILED(rv)) {
        MOZ_LOG(sCSMLog, LogLevel::Debug,
                ("  HTTP Method: %s\n", channelMethod.get()));
      }
    }

    // Log Principals
    nsCOMPtr<nsIPrincipal> requestPrincipal = aLoadInfo->TriggeringPrincipal();
    LogPrincipal(aLoadInfo->LoadingPrincipal(),
                 NS_LITERAL_STRING("loadingPrincipal"));
    LogPrincipal(requestPrincipal, NS_LITERAL_STRING("triggeringPrincipal"));
    LogPrincipal(aLoadInfo->PrincipalToInherit(),
                 NS_LITERAL_STRING("principalToInherit"));

    // Log Redirect Chain
    MOZ_LOG(sCSMLog, LogLevel::Debug, ("  RedirectChain:\n"));
    for (nsIRedirectHistoryEntry* redirectHistoryEntry :
         aLoadInfo->RedirectChain()) {
      nsCOMPtr<nsIPrincipal> principal;
      redirectHistoryEntry->GetPrincipal(getter_AddRefs(principal));
      LogPrincipal(principal, NS_LITERAL_STRING("->"));
    }

    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("  internalContentPolicyType: %d\n",
             aLoadInfo->InternalContentPolicyType()));
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("  externalContentPolicyType: %d\n",
             aLoadInfo->GetExternalContentPolicyType()));
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("  upgradeInsecureRequests: %s\n",
             aLoadInfo->GetUpgradeInsecureRequests() ? "true" : "false"));
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("  initalSecurityChecksDone: %s\n",
             aLoadInfo->GetInitialSecurityCheckDone() ? "true" : "false"));

    // Log CSPrequestPrincipal
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    requestPrincipal->GetCsp(getter_AddRefs(csp));
    if (csp) {
      nsAutoString parsedPolicyStr;
      uint32_t count = 0;
      csp->GetPolicyCount(&count);
      MOZ_LOG(sCSMLog, LogLevel::Debug, ("  CSP (%d): ", count));
      for (uint32_t i = 0; i < count; ++i) {
        csp->GetPolicyString(i, parsedPolicyStr);
        MOZ_LOG(sCSMLog, LogLevel::Debug,
                ("    %s\n", NS_ConvertUTF16toUTF8(parsedPolicyStr).get()));
      }
    }

    // Security Flags
    MOZ_LOG(sCSMLog, LogLevel::Debug, ("  securityFlags: "));
    LogSecurityFlags(aLoadInfo->GetSecurityFlags());
    MOZ_LOG(sCSMLog, LogLevel::Debug, ("}\n\n"));
  }
}

#if defined(DEBUG) || defined(FUZZING)
// Assert that we never use the SystemPrincipal to load remote documents
// i.e., HTTP, HTTPS, FTP URLs
static void AssertSystemPrincipalMustNotLoadRemoteDocuments(
    nsIChannel* aChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  // bail out, if we're not loading with a SystemPrincipal
  if (!nsContentUtils::IsSystemPrincipal(loadInfo->LoadingPrincipal())) {
    return;
  }
  nsContentPolicyType contentPolicyType =
      loadInfo->GetExternalContentPolicyType();
  if ((contentPolicyType != nsIContentPolicy::TYPE_DOCUMENT) &&
      (contentPolicyType != nsIContentPolicy::TYPE_SUBDOCUMENT)) {
    return;
  }
  nsCOMPtr<nsIURI> finalURI;
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(finalURI));
  // bail out, if URL isn't pointing to remote resource
  if (!nsContentUtils::SchemeIs(finalURI, "http") &&
      !nsContentUtils::SchemeIs(finalURI, "https") &&
      !nsContentUtils::SchemeIs(finalURI, "ftp")) {
    return;
  }

  // FIXME The discovery feature in about:addons uses the SystemPrincpal.
  // We should remove this exception with bug 1544011.
  static nsAutoCString sDiscoveryPrePath;
  static bool recvdPrefValue = false;
  if (!recvdPrefValue) {
    nsAutoCString discoveryURLString;
    Preferences::GetCString("extensions.webservice.discoverURL",
                            discoveryURLString);
    // discoverURL is by default suffixed with parameters in path like
    // /%LOCALE%/ so, we use the prePath for comparison
    nsCOMPtr<nsIURI> discoveryURL;
    NS_NewURI(getter_AddRefs(discoveryURL), discoveryURLString);
    if (discoveryURL) {
      discoveryURL->GetPrePath(sDiscoveryPrePath);
    }
    recvdPrefValue = true;
  }
  nsAutoCString requestedPrePath;
  finalURI->GetPrePath(requestedPrePath);
  if (requestedPrePath.Equals(sDiscoveryPrePath)) {
    return;
  }

  if (xpc::AreNonLocalConnectionsDisabled()) {
    bool disallowSystemPrincipalRemoteDocuments = Preferences::GetBool(
        "security.disallow_non_local_systemprincipal_in_tests");
    if (disallowSystemPrincipalRemoteDocuments) {
      // our own mochitest needs NS_ASSERTION instead of MOZ_ASSERT
      NS_ASSERTION(false, "SystemPrincipal must not load remote documents.");
      return;
    }
    // but other mochitest are exempt from this
    return;
  }
  MOZ_ASSERT(false, "SystemPrincipal must not load remote documents.");
}
#endif

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
  if (MOZ_UNLIKELY(MOZ_LOG_TEST(sCSMLog, LogLevel::Debug))) {
    DebugDoContentSecurityCheck(aChannel, loadInfo);
  }

#if defined(DEBUG) || defined(FUZZING)
  AssertSystemPrincipalMustNotLoadRemoteDocuments(aChannel);
#endif

  // if dealing with a redirected channel then we have already installed
  // streamlistener and redirect proxies and so we are done.
  if (loadInfo->GetInitialSecurityCheckDone()) {
    return NS_OK;
  }

  // make sure that only one of the five security flags is set in the loadinfo
  // e.g. do not require same origin and allow cross origin at the same time
  nsresult rv = ValidateSecurityFlags(loadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  if (loadInfo->GetSecurityMode() ==
      nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
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

  // now lets set the initalSecurityFlag for subsequent calls
  loadInfo->SetInitialSecurityCheckDone(true);

  // all security checks passed - lets allow the load
  return NS_OK;
}

NS_IMETHODIMP
nsContentSecurityManager::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aRedirFlags,
    nsIAsyncVerifyRedirectCallback* aCb) {
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
      oldPrincipal, newURI, flags);
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
               nsIContentPolicy::TYPE_DOCUMENT);
    nsIPrincipal* loadingPrincipal = loadInfo->LoadingPrincipal();

    // It doesn't matter what we pass for the third, data-inherits, argument.
    // Any protocol which inherits won't pay attention to cookies anyway.
    rv = loadingPrincipal->CheckMayLoad(uri, false, false);
    if (NS_FAILED(rv)) {
      AddLoadFlags(aChannel, nsIRequest::LOAD_ANONYMOUS);
    }
  } else if (cookiePolicy == nsILoadInfo::SEC_COOKIES_OMIT) {
    AddLoadFlags(aChannel, nsIRequest::LOAD_ANONYMOUS);
  }

  nsSecurityFlags securityMode = loadInfo->GetSecurityMode();

  // CORS mode is handled by nsCORSListenerProxy
  if (securityMode == nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
    if (NS_HasBeenCrossOrigin(aChannel)) {
      loadInfo->MaybeIncreaseTainting(LoadTainting::CORS);
    }
    return NS_OK;
  }

  // Allow subresource loads if TriggeringPrincipal is the SystemPrincipal.
  // For example, allow user stylesheets to load XBL from external files.
  if (nsContentUtils::IsSystemPrincipal(loadInfo->TriggeringPrincipal()) &&
      loadInfo->GetExternalContentPolicyType() !=
          nsIContentPolicy::TYPE_DOCUMENT &&
      loadInfo->GetExternalContentPolicyType() !=
          nsIContentPolicy::TYPE_SUBDOCUMENT) {
    return NS_OK;
  }

  // if none of the REQUIRE_SAME_ORIGIN flags are set, then SOP does not apply
  if ((securityMode == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS) ||
      (securityMode == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED)) {
    rv = DoSOPChecks(uri, loadInfo, aChannel);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if ((securityMode == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS) ||
      (securityMode == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL)) {
    if (NS_HasBeenCrossOrigin(aChannel)) {
      NS_ENSURE_FALSE(loadInfo->GetDontFollowRedirects(), NS_ERROR_DOM_BAD_URI);
      loadInfo->MaybeIncreaseTainting(LoadTainting::Opaque);
    }
    // Please note that DoCheckLoadURIChecks should only be enforced for
    // cross origin requests. If the flag SEC_REQUIRE_CORS_DATA_INHERITS is set
    // within the loadInfo, then then CheckLoadURIWithPrincipal is performed
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

NS_IMETHODIMP
nsContentSecurityManager::IsOriginPotentiallyTrustworthy(
    nsIPrincipal* aPrincipal, bool* aIsTrustWorthy) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aIsTrustWorthy);

  if (aPrincipal->IsSystemPrincipal()) {
    *aIsTrustWorthy = true;
    return NS_OK;
  }
  *aIsTrustWorthy = false;
  if (aPrincipal->GetIsNullPrincipal()) {
    return NS_OK;
  }

  MOZ_ASSERT(aPrincipal->GetIsCodebasePrincipal(),
             "Nobody is expected to call us with an nsIExpandedPrincipal");

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  *aIsTrustWorthy = nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(uri);

  return NS_OK;
}
