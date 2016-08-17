#include "nsContentSecurityManager.h"
#include "nsIChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIStreamListener.h"
#include "nsILoadInfo.h"
#include "nsContentUtils.h"
#include "nsCORSListenerProxy.h"
#include "nsIStreamListener.h"

#include "mozilla/dom/Element.h"

NS_IMPL_ISUPPORTS(nsContentSecurityManager,
                  nsIContentSecurityManager,
                  nsIChannelEventSink)

static nsresult
ValidateSecurityFlags(nsILoadInfo* aLoadInfo)
{
  nsSecurityFlags securityMode = aLoadInfo->GetSecurityMode();

  if (securityMode != nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS &&
      securityMode != nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED &&
      securityMode != nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS &&
      securityMode != nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL &&
      securityMode != nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
    MOZ_ASSERT(false, "need one securityflag from nsILoadInfo to perform security checks");
    return NS_ERROR_FAILURE;
  }

  // all good, found the right security flags
  return NS_OK;
}

static bool SchemeIs(nsIURI* aURI, const char* aScheme)
{
  nsCOMPtr<nsIURI> baseURI = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(baseURI, false);

  bool isScheme = false;
  return NS_SUCCEEDED(baseURI->SchemeIs(aScheme, &isScheme)) && isScheme;
}


static bool IsImageLoadInEditorAppType(nsILoadInfo* aLoadInfo)
{
  // Editor apps get special treatment here, editors can load images
  // from anywhere.  This allows editor to insert images from file://
  // into documents that are being edited.
  nsContentPolicyType type = aLoadInfo->InternalContentPolicyType();
  if (type != nsIContentPolicy::TYPE_INTERNAL_IMAGE  &&
      type != nsIContentPolicy::TYPE_INTERNAL_IMAGE_PRELOAD &&
      type != nsIContentPolicy::TYPE_IMAGESET) {
    return false;
  }

  uint32_t appType = nsIDocShell::APP_TYPE_UNKNOWN;
  nsINode* node = aLoadInfo->LoadingNode();
  if (!node) {
    return false;
  }
  nsIDocument* doc = node->OwnerDoc();
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
  if (!docShell || NS_FAILED(docShell->GetAppType(&appType))) {
    appType = nsIDocShell::APP_TYPE_UNKNOWN;
  }

  return appType == nsIDocShell::APP_TYPE_EDITOR;
}

static nsresult
DoCheckLoadURIChecks(nsIURI* aURI, nsILoadInfo* aLoadInfo)
{
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
  return nsContentUtils::GetSecurityManager()->
           CheckLoadURIWithPrincipal(aLoadInfo->TriggeringPrincipal(),
                                     aURI,
                                     flags);
}

static bool
URIHasFlags(nsIURI* aURI, uint32_t aURIFlags)
{
  bool hasFlags;
  nsresult rv = NS_URIChainHasFlags(aURI, aURIFlags, &hasFlags);
  NS_ENSURE_SUCCESS(rv, false);

  return hasFlags;
}

static nsresult
DoSOPChecks(nsIURI* aURI, nsILoadInfo* aLoadInfo, nsIChannel* aChannel)
{
  if (aLoadInfo->GetAllowChrome() &&
      (URIHasFlags(aURI, nsIProtocolHandler::URI_IS_UI_RESOURCE) ||
       SchemeIs(aURI, "moz-safe-about"))) {
    // UI resources are allowed.
    return DoCheckLoadURIChecks(aURI, aLoadInfo);
  }

  NS_ENSURE_FALSE(NS_HasBeenCrossOrigin(aChannel, true),
                  NS_ERROR_DOM_BAD_URI);

  return NS_OK;
}

static nsresult
DoCORSChecks(nsIChannel* aChannel, nsILoadInfo* aLoadInfo,
             nsCOMPtr<nsIStreamListener>& aInAndOutListener)
{
  MOZ_RELEASE_ASSERT(aInAndOutListener, "can not perform CORS checks without a listener");

  // No need to set up CORS if TriggeringPrincipal is the SystemPrincipal.
  // For example, allow user stylesheets to load XBL from external files
  // without requiring CORS.
  if (nsContentUtils::IsSystemPrincipal(aLoadInfo->TriggeringPrincipal())) {
    return NS_OK;
  }

  nsIPrincipal* loadingPrincipal = aLoadInfo->LoadingPrincipal();
  RefPtr<nsCORSListenerProxy> corsListener =
    new nsCORSListenerProxy(aInAndOutListener,
                            loadingPrincipal,
                            aLoadInfo->GetCookiePolicy() ==
                              nsILoadInfo::SEC_COOKIES_INCLUDE);
  // XXX: @arg: DataURIHandling::Allow
  // lets use  DataURIHandling::Allow for now and then decide on callsite basis. see also:
  // http://mxr.mozilla.org/mozilla-central/source/dom/security/nsCORSListenerProxy.h#33
  nsresult rv = corsListener->Init(aChannel, DataURIHandling::Allow);
  NS_ENSURE_SUCCESS(rv, rv);
  aInAndOutListener = corsListener;
  return NS_OK;
}

static nsresult
DoContentSecurityChecks(nsIChannel* aChannel, nsILoadInfo* aLoadInfo)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsContentPolicyType contentPolicyType =
    aLoadInfo->GetExternalContentPolicyType();
  nsContentPolicyType internalContentPolicyType =
    aLoadInfo->InternalContentPolicyType();
  nsCString mimeTypeGuess;
  nsCOMPtr<nsINode> requestingContext = nullptr;

#ifdef DEBUG
  // Don't enforce TYPE_DOCUMENT assertions for loads
  // initiated by javascript tests.
  bool skipContentTypeCheck = false;
  skipContentTypeCheck = Preferences::GetBool("network.loadinfo.skip_type_assertion");
#endif

  switch(contentPolicyType) {
    case nsIContentPolicy::TYPE_OTHER: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_SCRIPT: {
      mimeTypeGuess = NS_LITERAL_CSTRING("application/javascript");
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_IMAGE: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_STYLESHEET: {
      mimeTypeGuess = NS_LITERAL_CSTRING("text/css");
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_OBJECT: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_DOCUMENT: {
      MOZ_ASSERT(skipContentTypeCheck || false, "contentPolicyType not supported yet");
      break;
    }

    case nsIContentPolicy::TYPE_SUBDOCUMENT: {
      mimeTypeGuess = NS_LITERAL_CSTRING("text/html");
      requestingContext = aLoadInfo->LoadingNode();
      MOZ_ASSERT(!requestingContext ||
                 requestingContext->NodeType() == nsIDOMNode::DOCUMENT_NODE,
                 "type_subdocument requires requestingContext of type Document");
      break;
    }

    case nsIContentPolicy::TYPE_REFRESH: {
      MOZ_ASSERT(false, "contentPolicyType not supported yet");
      break;
    }

    case nsIContentPolicy::TYPE_XBL: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_PING: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_XMLHTTPREQUEST: {
      // alias nsIContentPolicy::TYPE_DATAREQUEST:
      requestingContext = aLoadInfo->LoadingNode();
      MOZ_ASSERT(!requestingContext ||
                 requestingContext->NodeType() == nsIDOMNode::DOCUMENT_NODE,
                 "type_xml requires requestingContext of type Document");

      // We're checking for the external TYPE_XMLHTTPREQUEST here in case
      // an addon creates a request with that type.
      if (internalContentPolicyType ==
            nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST ||
          internalContentPolicyType ==
            nsIContentPolicy::TYPE_XMLHTTPREQUEST) {
        mimeTypeGuess = EmptyCString();
      }
      else {
        MOZ_ASSERT(internalContentPolicyType ==
                   nsIContentPolicy::TYPE_INTERNAL_EVENTSOURCE,
                   "can not set mime type guess for unexpected internal type");
        mimeTypeGuess = NS_LITERAL_CSTRING(TEXT_EVENT_STREAM);
      }
      break;
    }

    case nsIContentPolicy::TYPE_OBJECT_SUBREQUEST: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      MOZ_ASSERT(!requestingContext ||
                 requestingContext->NodeType() == nsIDOMNode::ELEMENT_NODE,
                 "type_subrequest requires requestingContext of type Element");
      break;
    }

    case nsIContentPolicy::TYPE_DTD: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      MOZ_ASSERT(!requestingContext ||
                 requestingContext->NodeType() == nsIDOMNode::DOCUMENT_NODE,
                 "type_dtd requires requestingContext of type Document");
      break;
    }

    case nsIContentPolicy::TYPE_FONT: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_MEDIA: {
      if (internalContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_TRACK) {
        mimeTypeGuess = NS_LITERAL_CSTRING("text/vtt");
      }
      else {
        mimeTypeGuess = EmptyCString();
      }
      requestingContext = aLoadInfo->LoadingNode();
      MOZ_ASSERT(!requestingContext ||
                 requestingContext->NodeType() == nsIDOMNode::ELEMENT_NODE,
                 "type_media requires requestingContext of type Element");
      break;
    }

    case nsIContentPolicy::TYPE_WEBSOCKET: {
      // Websockets have to use the proxied URI:
      // ws:// instead of http:// for CSP checks
      nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal
        = do_QueryInterface(aChannel);
      MOZ_ASSERT(httpChannelInternal);
      if (httpChannelInternal) {
        httpChannelInternal->GetProxyURI(getter_AddRefs(uri));
      }
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_CSP_REPORT: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_XSLT: {
      mimeTypeGuess = NS_LITERAL_CSTRING("application/xml");
      requestingContext = aLoadInfo->LoadingNode();
      MOZ_ASSERT(!requestingContext ||
                 requestingContext->NodeType() == nsIDOMNode::DOCUMENT_NODE,
                 "type_xslt requires requestingContext of type Document");
      break;
    }

    case nsIContentPolicy::TYPE_BEACON: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      MOZ_ASSERT(!requestingContext ||
                 requestingContext->NodeType() == nsIDOMNode::DOCUMENT_NODE,
                 "type_beacon requires requestingContext of type Document");
      break;
    }

    case nsIContentPolicy::TYPE_FETCH: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_IMAGESET: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_WEB_MANIFEST: {
      mimeTypeGuess = NS_LITERAL_CSTRING("application/manifest+json");
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    default:
      // nsIContentPolicy::TYPE_INVALID
      MOZ_ASSERT(false, "can not perform security check without a valid contentType");
  }

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(internalContentPolicyType,
                                 uri,
                                 aLoadInfo->LoadingPrincipal(),
                                 requestingContext,
                                 mimeTypeGuess,
                                 nullptr,        //extra,
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_CP_REJECTED(shouldLoad)) {
    return NS_ERROR_CONTENT_BLOCKED;
  }
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
 *    The streamListener that is passed to channel->AsyncOpen2() that is now potentially
 *    wrappend within nsCORSListenerProxy() and becomes the corsListener that now needs
 *    to be set as new streamListener on the channel.
 */
nsresult
nsContentSecurityManager::doContentSecurityCheck(nsIChannel* aChannel,
                                                 nsCOMPtr<nsIStreamListener>& aInAndOutListener)
{
  NS_ENSURE_ARG(aChannel);
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();

  if (!loadInfo) {
    MOZ_ASSERT(false, "channel needs to have loadInfo to perform security checks");
    return NS_ERROR_UNEXPECTED;
  }

  // if dealing with a redirected channel then we have already installed
  // streamlistener and redirect proxies and so we are done.
  if (loadInfo->GetInitialSecurityCheckDone()) {
    return NS_OK;
  }

  // make sure that only one of the five security flags is set in the loadinfo
  // e.g. do not require same origin and allow cross origin at the same time
  nsresult rv = ValidateSecurityFlags(loadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // since aChannel was openend using asyncOpen2() we have to make sure
  // that redirects of that channel also get openend using asyncOpen2()
  // please note that some implementations of ::AsyncOpen2 might already
  // have set that flag to true (e.g. nsViewSourceChannel) in which case
  // we just set the flag again.
  loadInfo->SetEnforceSecurity(true);

  if (loadInfo->GetSecurityMode() == nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
    rv = DoCORSChecks(aChannel, loadInfo, aInAndOutListener);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = CheckChannel(aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  // Perform all ContentPolicy checks (MixedContent, CSP, ...)
  rv = DoContentSecurityChecks(aChannel, loadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // now lets set the initalSecurityFlag for subsequent calls
  loadInfo->SetInitialSecurityCheckDone(true);

  // all security checks passed - lets allow the load
  return NS_OK;
}

NS_IMETHODIMP
nsContentSecurityManager::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                                 nsIChannel* aNewChannel,
                                                 uint32_t aRedirFlags,
                                                 nsIAsyncVerifyRedirectCallback *aCb)
{
  nsCOMPtr<nsILoadInfo> loadInfo = aOldChannel->GetLoadInfo();
  // Are we enforcing security using LoadInfo?
  if (loadInfo && loadInfo->GetEnforceSecurity()) {
    nsresult rv = CheckChannel(aNewChannel);
    if (NS_FAILED(rv)) {
      aOldChannel->Cancel(rv);
      return rv;
    }
  }

  // Also verify that the redirecting server is allowed to redirect to the
  // given URI
  nsCOMPtr<nsIPrincipal> oldPrincipal;
  nsContentUtils::GetSecurityManager()->
    GetChannelResultPrincipal(aOldChannel, getter_AddRefs(oldPrincipal));

  nsCOMPtr<nsIURI> newURI;
  aNewChannel->GetURI(getter_AddRefs(newURI));
  nsCOMPtr<nsIURI> newOriginalURI;
  aNewChannel->GetOriginalURI(getter_AddRefs(newOriginalURI));

  NS_ENSURE_STATE(oldPrincipal && newURI && newOriginalURI);

  const uint32_t flags =
      nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT |
      nsIScriptSecurityManager::DISALLOW_SCRIPT;
  nsresult rv = nsContentUtils::GetSecurityManager()->
    CheckLoadURIWithPrincipal(oldPrincipal, newURI, flags);
  if (NS_SUCCEEDED(rv) && newOriginalURI != newURI) {
      rv = nsContentUtils::GetSecurityManager()->
        CheckLoadURIWithPrincipal(oldPrincipal, newOriginalURI, flags);
  }
  NS_ENSURE_SUCCESS(rv, rv);  

  aCb->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

static void
AddLoadFlags(nsIRequest *aRequest, nsLoadFlags aNewFlags)
{
  nsLoadFlags flags;
  aRequest->GetLoadFlags(&flags);
  flags |= aNewFlags;
  aRequest->SetLoadFlags(flags);
}

/*
 * Check that this channel passes all security checks. Returns an error code
 * if this requesst should not be permitted.
 */
nsresult
nsContentSecurityManager::CheckChannel(nsIChannel* aChannel)
{
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  MOZ_ASSERT(loadInfo);

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
  }
  else if (cookiePolicy == nsILoadInfo::SEC_COOKIES_OMIT) {
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
      loadInfo->GetExternalContentPolicyType() != nsIContentPolicy::TYPE_DOCUMENT &&
      loadInfo->GetExternalContentPolicyType() != nsIContentPolicy::TYPE_SUBDOCUMENT) {
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
      loadInfo->MaybeIncreaseTainting(LoadTainting::Opaque);
    }
    // Please note that DoCheckLoadURIChecks should only be enforced for
    // cross origin requests. If the flag SEC_REQUIRE_CORS_DATA_INHERITS is set
    // within the loadInfo, then then CheckLoadURIWithPrincipal is performed
    // within nsCorsListenerProxy
    rv = DoCheckLoadURIChecks(uri, loadInfo);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// ==== nsIContentSecurityManager implementation =====

NS_IMETHODIMP
nsContentSecurityManager::PerformSecurityCheck(nsIChannel* aChannel,
                                               nsIStreamListener* aStreamListener,
                                               nsIStreamListener** outStreamListener)
{
  nsCOMPtr<nsIStreamListener> inAndOutListener = aStreamListener;
  nsresult rv = doContentSecurityCheck(aChannel, inAndOutListener);
  NS_ENSURE_SUCCESS(rv, rv);

  inAndOutListener.forget(outStreamListener);
  return NS_OK;
}

NS_IMETHODIMP
nsContentSecurityManager::IsOriginPotentiallyTrustworthy(nsIPrincipal* aPrincipal,
                                                         bool* aIsTrustWorthy)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aIsTrustWorthy);

  if (aPrincipal->GetIsSystemPrincipal()) {
    *aIsTrustWorthy = true;
    return NS_OK;
  }

  // The following implements:
  // https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy

  *aIsTrustWorthy = false;

  if (aPrincipal->GetIsNullPrincipal()) {
    return NS_OK;
  }

  MOZ_ASSERT(aPrincipal->GetIsCodebasePrincipal(),
             "Nobody is expected to call us with an nsIExpandedPrincipal");

  nsCOMPtr<nsIURI> uri;
  aPrincipal->GetURI(getter_AddRefs(uri));

  nsAutoCString scheme;
  nsresult rv = uri->GetScheme(scheme);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  // Blobs are expected to inherit their principal so we don't expect to have
  // a codebase principal with scheme 'blob' here.  We can't assert that though
  // since someone could mess with a non-blob URI to give it that scheme.
  NS_WARN_IF_FALSE(!scheme.EqualsLiteral("blob"),
                   "IsOriginPotentiallyTrustworthy ignoring blob scheme");

  // According to the specification, the user agent may choose to extend the
  // trust to other, vendor-specific URL schemes. We use this for "resource:",
  // which is technically a substituting protocol handler that is not limited to
  // local resource mapping, but in practice is never mapped remotely as this
  // would violate assumptions a lot of code makes.
  if (scheme.EqualsLiteral("https") ||
      scheme.EqualsLiteral("file") ||
      scheme.EqualsLiteral("resource") ||
      scheme.EqualsLiteral("app") ||
      scheme.EqualsLiteral("moz-extension") ||
      scheme.EqualsLiteral("wss")) {
    *aIsTrustWorthy = true;
    return NS_OK;
  }

  nsAutoCString host;
  rv = uri->GetHost(host);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  if (host.Equals("127.0.0.1") ||
      host.Equals("localhost") ||
      host.Equals("::1")) {
    *aIsTrustWorthy = true;
    return NS_OK;
  }
  return NS_OK;
}
