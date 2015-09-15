#include "nsContentSecurityManager.h"
#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsILoadInfo.h"
#include "nsContentUtils.h"
#include "nsCORSListenerProxy.h"
#include "nsIStreamListener.h"

#include "mozilla/dom/Element.h"

nsresult
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

  // make sure that cors-with-credentials is only used in combination with CORS.
  if (aLoadInfo->GetRequireCorsWithCredentials() &&
      securityMode != nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
    MOZ_ASSERT(false, "can not use cors-with-credentials without cors");
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

nsresult
DoCheckLoadURIChecks(nsIURI* aURI, nsILoadInfo* aLoadInfo)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIPrincipal> loadingPrincipal = aLoadInfo->LoadingPrincipal();
  uint32_t flags = nsIScriptSecurityManager::STANDARD;
  if (aLoadInfo->GetAllowChrome()) {
    flags |= nsIScriptSecurityManager::ALLOW_CHROME;
  }

  rv = nsContentUtils::GetSecurityManager()->
    CheckLoadURIWithPrincipal(loadingPrincipal,
                              aURI,
                              flags);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the loadingPrincipal and the triggeringPrincipal are different, then make
  // sure the triggeringPrincipal is allowed to access that URI.
  nsCOMPtr<nsIPrincipal> triggeringPrincipal = aLoadInfo->TriggeringPrincipal();
  if (loadingPrincipal != triggeringPrincipal) {
    rv = nsContentUtils::GetSecurityManager()->
           CheckLoadURIWithPrincipal(triggeringPrincipal,
                                     aURI,
                                     flags);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
DoSOPChecks(nsIURI* aURI, nsILoadInfo* aLoadInfo)
{
  if (aLoadInfo->GetAllowChrome() && SchemeIs(aURI, "chrome")) {
    // Enforce same-origin policy, except to chrome.
    return DoCheckLoadURIChecks(aURI, aLoadInfo);
  }

  nsIPrincipal* loadingPrincipal = aLoadInfo->LoadingPrincipal();
  bool sameOriginDataInherits =
    aLoadInfo->GetSecurityMode() == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS;
  return loadingPrincipal->CheckMayLoad(aURI,
                                        true, // report to console
                                        sameOriginDataInherits);
}

nsresult
DoCORSChecks(nsIChannel* aChannel, nsILoadInfo* aLoadInfo,
             nsCOMPtr<nsIStreamListener>& aInAndOutListener)
{
  MOZ_ASSERT(aInAndOutListener, "can not perform CORS checks without a listener");
  nsIPrincipal* loadingPrincipal = aLoadInfo->LoadingPrincipal();
  nsRefPtr<nsCORSListenerProxy> corsListener =
    new nsCORSListenerProxy(aInAndOutListener,
                            loadingPrincipal,
                            aLoadInfo->GetRequireCorsWithCredentials());
  // XXX: @arg: DataURIHandling::Allow
  // lets use  DataURIHandling::Allow for now and then decide on callsite basis. see also:
  // http://mxr.mozilla.org/mozilla-central/source/dom/security/nsCORSListenerProxy.h#33
  nsresult rv = corsListener->Init(aChannel, DataURIHandling::Allow);
  NS_ENSURE_SUCCESS(rv, rv);
  aInAndOutListener = corsListener;
  return NS_OK;
}

nsresult
DoContentSecurityChecks(nsIURI* aURI, nsILoadInfo* aLoadInfo)
{
  nsContentPolicyType contentPolicyType = aLoadInfo->GetContentPolicyType();
  nsCString mimeTypeGuess;
  nsCOMPtr<nsINode> requestingContext = nullptr;
  nsContentPolicyType internalContentPolicyType =
    aLoadInfo->InternalContentPolicyType();

  switch(contentPolicyType) {
    case nsIContentPolicy::TYPE_OTHER: {
      mimeTypeGuess = EmptyCString();
      requestingContext = aLoadInfo->LoadingNode();
      break;
    }

    case nsIContentPolicy::TYPE_SCRIPT:
    case nsIContentPolicy::TYPE_IMAGE:
    case nsIContentPolicy::TYPE_STYLESHEET:
    case nsIContentPolicy::TYPE_OBJECT:
    case nsIContentPolicy::TYPE_DOCUMENT: {
      MOZ_ASSERT(false, "contentPolicyType not supported yet");
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

      if (internalContentPolicyType ==
            nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST) {
        mimeTypeGuess = NS_LITERAL_CSTRING("application/xml");
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

    case nsIContentPolicy::TYPE_DTD:
    case nsIContentPolicy::TYPE_FONT: {
      MOZ_ASSERT(false, "contentPolicyType not supported yet");
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

    case nsIContentPolicy::TYPE_WEBSOCKET:
    case nsIContentPolicy::TYPE_CSP_REPORT:
    case nsIContentPolicy::TYPE_XSLT: {
      MOZ_ASSERT(false, "contentPolicyType not supported yet");
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

    case nsIContentPolicy::TYPE_FETCH:
    case nsIContentPolicy::TYPE_IMAGESET: {
      MOZ_ASSERT(false, "contentPolicyType not supported yet");
      break;
    }

    default:
      // nsIContentPolicy::TYPE_INVALID
      MOZ_ASSERT(false, "can not perform security check without a valid contentType");
  }

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(contentPolicyType,
                                          aURI,
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

  // make sure that only one of the five security flags is set in the loadinfo
  // e.g. do not require same origin and allow cross origin at the same time
  nsresult rv = ValidateSecurityFlags(loadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // lets store the initialSecurityCheckDone flag which indicates whether the channel
  // was initialy evaluated by the contentSecurityManager. Once the inital
  // asyncOpen() of the channel went through the contentSecurityManager then
  // redirects do not have perform all the security checks, e.g. no reason
  // to setup CORS again.
  bool initialSecurityCheckDone = loadInfo->GetInitialSecurityCheckDone();

  // now lets set the initalSecurityFlag for subsequent calls
  rv = loadInfo->SetInitialSecurityCheckDone(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // since aChannel was openend using asyncOpen2() we have to make sure
  // that redirects of that channel also get openend using asyncOpen2()
  // please note that some implementations of ::AsyncOpen2 might already
  // have set that flag to true (e.g. nsViewSourceChannel) in which case
  // we just set the flag again.
  rv = loadInfo->SetEnforceSecurity(true);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> finalChannelURI;
  rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(finalChannelURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsSecurityFlags securityMode = loadInfo->GetSecurityMode();

  // if none of the REQUIRE_SAME_ORIGIN flags are set, then SOP does not apply
  if ((securityMode == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS) ||
      (securityMode == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED)) {
    rv = DoSOPChecks(finalChannelURI, loadInfo);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // if dealing with a redirected channel then we only enforce SOP
  // and can return at this point.
  if (initialSecurityCheckDone) {
    return NS_OK;
  }

  if ((securityMode == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS) ||
      (securityMode == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL)) {
    // Please note that DoCheckLoadURIChecks should only be enforced for
    // cross origin requests. If the flag SEC_REQUIRE_CORS_DATA_INHERITS is set
    // within the loadInfo, then then CheckLoadURIWithPrincipal is performed
    // within nsCorsListenerProxy
    rv = DoCheckLoadURIChecks(finalChannelURI, loadInfo);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (securityMode == nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
    rv = DoCORSChecks(aChannel, loadInfo, aInAndOutListener);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Perform all ContentPolicy checks (MixedContent, CSP, ...)
  rv = DoContentSecurityChecks(finalChannelURI, loadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // all security checks passed - lets allow the load
  return NS_OK;
}
