/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMixedContentBlocker.h"

#include "nsContentPolicyUtils.h"
#include "nsCSPContext.h"
#include "nsThreadUtils.h"
#include "nsINode.h"
#include "nsCOMPtr.h"
#include "nsIDocShell.h"
#include "nsISecurityEventSink.h"
#include "nsIWebProgressListener.h"
#include "nsContentUtils.h"
#include "nsIRequest.h"
#include "nsIDocument.h"
#include "nsIContentViewer.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIParentChannel.h"
#include "mozilla/Preferences.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsISecureBrowserUI.h"
#include "nsIDocumentLoader.h"
#include "nsIWebNavigation.h"
#include "nsLoadGroup.h"
#include "nsIScriptError.h"
#include "nsIURI.h"
#include "nsIChannelEventSink.h"
#include "nsNetUtil.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "mozilla/LoadInfo.h"
#include "nsISiteSecurityService.h"
#include "prnetdb.h"

#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/URIUtils.h"


using namespace mozilla;

enum nsMixedContentBlockerMessageType {
  eBlocked = 0x00,
  eUserOverride = 0x01
};

// Is mixed script blocking (fonts, plugin content, scripts, stylesheets,
// iframes, websockets, XHR) enabled?
bool nsMixedContentBlocker::sBlockMixedScript = false;

// Is mixed display content blocking (images, audio, video, <a ping>) enabled?
bool nsMixedContentBlocker::sBlockMixedDisplay = false;

// Do we move HSTS before mixed-content
bool nsMixedContentBlocker::sUseHSTS = false;
// Do we send an HSTS priming request
bool nsMixedContentBlocker::sSendHSTSPriming = false;
// Default HSTS Priming failure timeout to 7 days, in seconds
uint32_t nsMixedContentBlocker::sHSTSPrimingCacheTimeout = (60 * 60 * 24 * 7);

bool
IsEligibleForHSTSPriming(nsIURI* aContentLocation) {
  bool isHttpScheme = false;
  nsresult rv = aContentLocation->SchemeIs("http", &isHttpScheme);
  NS_ENSURE_SUCCESS(rv, false);
  if (!isHttpScheme) {
    return false;
  }

  int32_t port = -1;
  rv = aContentLocation->GetPort(&port);
  NS_ENSURE_SUCCESS(rv, false);
  int32_t defaultPort = NS_GetDefaultPort("https");

  if (port != -1 && port != defaultPort) {
    // HSTS priming requests are only sent if the port is the default port
    return false;
  }

  nsAutoCString hostname;
  rv = aContentLocation->GetHost(hostname);
  NS_ENSURE_SUCCESS(rv, false);

  PRNetAddr hostAddr;
  return (PR_StringToNetAddr(hostname.get(), &hostAddr) != PR_SUCCESS);
}

enum MixedContentHSTSState {
  MCB_HSTS_PASSIVE_NO_HSTS   = 0,
  MCB_HSTS_PASSIVE_WITH_HSTS = 1,
  MCB_HSTS_ACTIVE_NO_HSTS    = 2,
  MCB_HSTS_ACTIVE_WITH_HSTS  = 3
};

// Similar to the existing mixed-content HSTS, except MCB_HSTS_*_NO_HSTS is
// broken into two distinct states, indicating whether we plan to send a priming
// request or not. If we decided not go send a priming request, it could be
// because it is a type we do not support, or because we cached a previous
// negative response.
enum MixedContentHSTSPrimingState {
  eMCB_HSTS_PASSIVE_WITH_HSTS  = 0,
  eMCB_HSTS_ACTIVE_WITH_HSTS   = 1,
  eMCB_HSTS_PASSIVE_NO_PRIMING = 2,
  eMCB_HSTS_PASSIVE_DO_PRIMING = 3,
  eMCB_HSTS_ACTIVE_NO_PRIMING  = 4,
  eMCB_HSTS_ACTIVE_DO_PRIMING  = 5,
  eMCB_HSTS_PASSIVE_UPGRADE    = 6,
  eMCB_HSTS_ACTIVE_UPGRADE     = 7,
};

// Fired at the document that attempted to load mixed content.  The UI could
// handle this event, for example, by displaying an info bar that offers the
// choice to reload the page with mixed content permitted.
class nsMixedContentEvent : public Runnable
{
public:
  nsMixedContentEvent(nsISupports* aContext,
                      MixedContentTypes aType,
                      bool aRootHasSecureConnection)
    : mozilla::Runnable("nsMixedContentEvent")
    , mContext(aContext)
    , mType(aType)
    , mRootHasSecureConnection(aRootHasSecureConnection)
  {}

  NS_IMETHOD Run() override
  {
    NS_ASSERTION(mContext,
                 "You can't call this runnable without a requesting context");

    // To update the security UI in the tab with the blocked mixed content, call
    // nsISecurityEventSink::OnSecurityChange.  You can get to the event sink by
    // calling NS_CP_GetDocShellFromContext on the context, and QI'ing to
    // nsISecurityEventSink.


    // Mixed content was allowed and is about to load; get the document and
    // set the approriate flag to true if we are about to load Mixed Active
    // Content.
    nsCOMPtr<nsIDocShell> docShell = NS_CP_GetDocShellFromContext(mContext);
    if (!docShell) {
        return NS_OK;
    }
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    docShell->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    NS_ASSERTION(sameTypeRoot, "No document shell root tree item from document shell tree item!");

    // now get the document from sameTypeRoot
    nsCOMPtr<nsIDocument> rootDoc = sameTypeRoot->GetDocument();
    NS_ASSERTION(rootDoc, "No root document from document shell root tree item.");

    // Get eventSink and the current security state from the docShell
    nsCOMPtr<nsISecurityEventSink> eventSink = do_QueryInterface(docShell);
    NS_ASSERTION(eventSink, "No eventSink from docShell.");
    nsCOMPtr<nsIDocShell> rootShell = do_GetInterface(sameTypeRoot);
    NS_ASSERTION(rootShell, "No root docshell from document shell root tree item.");
    uint32_t state = nsIWebProgressListener::STATE_IS_BROKEN;
    nsCOMPtr<nsISecureBrowserUI> securityUI;
    rootShell->GetSecurityUI(getter_AddRefs(securityUI));
    // If there is no securityUI, document doesn't have a security state to
    // update.  But we still want to set the document flags, so we don't return
    // early.
    nsresult stateRV = NS_ERROR_FAILURE;
    if (securityUI) {
      stateRV = securityUI->GetState(&state);
    }

    if (mType == eMixedScript) {
       // See if the pref will change here. If it will, only then do we need to call OnSecurityChange() to update the UI.
       if (rootDoc->GetHasMixedActiveContentLoaded()) {
         return NS_OK;
       }
       rootDoc->SetHasMixedActiveContentLoaded(true);

      // Update the security UI in the tab with the allowed mixed active content
      if (securityUI) {
        // Bug 1182551 - before changing the security state to broken, check
        // that the root is actually secure.
        if (mRootHasSecureConnection) {
          // reset state security flag
          state = state >> 4 << 4;
          // set state security flag to broken, since there is mixed content
          state |= nsIWebProgressListener::STATE_IS_BROKEN;

          // If mixed display content is loaded, make sure to include that in the state.
          if (rootDoc->GetHasMixedDisplayContentLoaded()) {
            state |= nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT;
          }

          eventSink->OnSecurityChange(mContext,
                                      (state | nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT));
        } else {
          // root not secure, mixed active content loaded in an https subframe
          if (NS_SUCCEEDED(stateRV)) {
            eventSink->OnSecurityChange(mContext, (state | nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT));
          }
        }
      }

    } else if (mType == eMixedDisplay) {
      // See if the pref will change here. If it will, only then do we need to call OnSecurityChange() to update the UI.
      if (rootDoc->GetHasMixedDisplayContentLoaded()) {
        return NS_OK;
      }
      rootDoc->SetHasMixedDisplayContentLoaded(true);

      // Update the security UI in the tab with the allowed mixed display content.
      if (securityUI) {
        // Bug 1182551 - before changing the security state to broken, check
        // that the root is actually secure.
        if (mRootHasSecureConnection) {
          // reset state security flag
          state = state >> 4 << 4;
          // set state security flag to broken, since there is mixed content
          state |= nsIWebProgressListener::STATE_IS_BROKEN;

          // If mixed active content is loaded, make sure to include that in the state.
          if (rootDoc->GetHasMixedActiveContentLoaded()) {
            state |= nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT;
          }

          eventSink->OnSecurityChange(mContext,
                                      (state | nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT));
        } else {
          // root not secure, mixed display content loaded in an https subframe
          if (NS_SUCCEEDED(stateRV)) {
            eventSink->OnSecurityChange(mContext, (state | nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT));
          }
        }
      }
    }

    return NS_OK;
  }
private:
  // The requesting context for the content load. Generally, a DOM node from
  // the document that caused the load.
  nsCOMPtr<nsISupports> mContext;

  // The type of mixed content detected, e.g. active or display
  const MixedContentTypes mType;

  // Indicates whether the top level load is https or not.
  bool mRootHasSecureConnection;
};


nsMixedContentBlocker::nsMixedContentBlocker()
{
  // Cache the pref for mixed script blocking
  Preferences::AddBoolVarCache(&sBlockMixedScript,
                               "security.mixed_content.block_active_content");

  // Cache the pref for mixed display blocking
  Preferences::AddBoolVarCache(&sBlockMixedDisplay,
                               "security.mixed_content.block_display_content");

  // Cache the pref for HSTS
  Preferences::AddBoolVarCache(&sUseHSTS,
                               "security.mixed_content.use_hsts");

  // Cache the pref for sending HSTS priming
  Preferences::AddBoolVarCache(&sSendHSTSPriming,
                               "security.mixed_content.send_hsts_priming");

  // Cache the pref for HSTS priming failure cache time
  Preferences::AddUintVarCache(&sHSTSPrimingCacheTimeout,
                               "security.mixed_content.hsts_priming_cache_timeout");
}

nsMixedContentBlocker::~nsMixedContentBlocker()
{
}

NS_IMPL_ISUPPORTS(nsMixedContentBlocker, nsIContentPolicy, nsIChannelEventSink)

static void
LogMixedContentMessage(MixedContentTypes aClassification,
                       nsIURI* aContentLocation,
                       nsIDocument* aRootDoc,
                       nsMixedContentBlockerMessageType aMessageType)
{
  nsAutoCString messageCategory;
  uint32_t severityFlag;
  nsAutoCString messageLookupKey;

  if (aMessageType == eBlocked) {
    severityFlag = nsIScriptError::errorFlag;
    messageCategory.AssignLiteral("Mixed Content Blocker");
    if (aClassification == eMixedDisplay) {
      messageLookupKey.AssignLiteral("BlockMixedDisplayContent");
    } else {
      messageLookupKey.AssignLiteral("BlockMixedActiveContent");
    }
  } else {
    severityFlag = nsIScriptError::warningFlag;
    messageCategory.AssignLiteral("Mixed Content Message");
    if (aClassification == eMixedDisplay) {
      messageLookupKey.AssignLiteral("LoadingMixedDisplayContent2");
    } else {
      messageLookupKey.AssignLiteral("LoadingMixedActiveContent2");
    }
  }

  NS_ConvertUTF8toUTF16 locationSpecUTF16(aContentLocation->GetSpecOrDefault());
  const char16_t* strings[] = { locationSpecUTF16.get() };
  nsContentUtils::ReportToConsole(severityFlag, messageCategory, aRootDoc,
                                  nsContentUtils::eSECURITY_PROPERTIES,
                                  messageLookupKey.get(), strings, ArrayLength(strings));
}

/* nsIChannelEventSink implementation
 * This code is called when a request is redirected.
 * We check the channel associated with the new uri is allowed to load
 * in the current context
 */
NS_IMETHODIMP
nsMixedContentBlocker::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                              nsIChannel* aNewChannel,
                                              uint32_t aFlags,
                                              nsIAsyncVerifyRedirectCallback* aCallback)
{
  nsAsyncRedirectAutoCallback autoCallback(aCallback);

  if (!aOldChannel) {
    NS_ERROR("No channel when evaluating mixed content!");
    return NS_ERROR_FAILURE;
  }

  // If we are in the parent process in e10s, we don't have access to the
  // document node, and hence ShouldLoad will fail when we try to get
  // the docShell.  If that's the case, ignore mixed content checks
  // on redirects in the parent.  Let the child check for mixed content.
  nsCOMPtr<nsIParentChannel> is_ipc_channel;
  NS_QueryNotificationCallbacks(aNewChannel, is_ipc_channel);
  if (is_ipc_channel) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIURI> oldUri;
  rv = aOldChannel->GetURI(getter_AddRefs(oldUri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> newUri;
  rv = aNewChannel->GetURI(getter_AddRefs(newUri));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the loading Info from the old channel
  nsCOMPtr<nsILoadInfo> loadInfo;
  rv = aOldChannel->GetLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!loadInfo) {
    // XXX: We want to have a loadInfo on all channels, but we don't yet.
    // If an addon creates a channel, they may not set loadinfo. If that
    // channel redirects from one page to another page, we would get caught
    // in this code path. Hence, we have to return NS_OK. Once we have more
    // confidence that all channels have loadinfo, we can change this to
    // a failure. See bug 1077201.
    return NS_OK;
  }

  nsContentPolicyType contentPolicyType = loadInfo->InternalContentPolicyType();
  nsCOMPtr<nsIPrincipal> requestingPrincipal = loadInfo->LoadingPrincipal();

  // Since we are calling shouldLoad() directly on redirects, we don't go through the code
  // in nsContentPolicyUtils::NS_CheckContentLoadPolicy(). Hence, we have to
  // duplicate parts of it here.
  nsCOMPtr<nsIURI> requestingLocation;
  if (requestingPrincipal) {
    // We check to see if the loadingPrincipal is systemPrincipal and return
    // early if it is
    if (nsContentUtils::IsSystemPrincipal(requestingPrincipal)) {
      return NS_OK;
    }
    // We set the requestingLocation from the RequestingPrincipal.
    rv = requestingPrincipal->GetURI(getter_AddRefs(requestingLocation));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsISupports> requestingContext = loadInfo->LoadingNode();

  int16_t decision = REJECT_REQUEST;
  rv = ShouldLoad(contentPolicyType,
                  newUri,
                  requestingLocation,
                  requestingContext,
                  EmptyCString(),       // aMimeGuess
                  nullptr,              // aExtra
                  requestingPrincipal,
                  &decision);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nsMixedContentBlocker::sSendHSTSPriming) {
    // The LoadInfo passed in is for the original channel, HSTS priming needs to
    // be set on the new channel, if required. If the redirect changes
    // http->https, or vice-versa, the need for priming may change.
    nsCOMPtr<nsILoadInfo> newLoadInfo;
    rv = aNewChannel->GetLoadInfo(getter_AddRefs(newLoadInfo));
    NS_ENSURE_SUCCESS(rv, rv);
    if (newLoadInfo) {
      rv = nsMixedContentBlocker::MarkLoadInfoForPriming(newUri,
                                                         requestingContext,
                                                         newLoadInfo);
      if (NS_FAILED(rv)) {
        decision = REJECT_REQUEST;
        newLoadInfo->ClearHSTSPriming();
      }
    } else {
      decision = REJECT_REQUEST;
    }
  }

  // If the channel is about to load mixed content, abort the channel
  if (!NS_CP_ACCEPTED(decision)) {
    autoCallback.DontCallback();
    return NS_BINDING_FAILED;
  }

  return NS_OK;
}

/* This version of ShouldLoad() is non-static and called by the Content Policy
 * API and AsyncOnChannelRedirect().  See nsIContentPolicy::ShouldLoad()
 * for detailed description of the parameters.
 */
NS_IMETHODIMP
nsMixedContentBlocker::ShouldLoad(uint32_t aContentType,
                                  nsIURI* aContentLocation,
                                  nsIURI* aRequestingLocation,
                                  nsISupports* aRequestingContext,
                                  const nsACString& aMimeGuess,
                                  nsISupports* aExtra,
                                  nsIPrincipal* aRequestPrincipal,
                                  int16_t* aDecision)
{
  // We pass in false as the first parameter to ShouldLoad(), because the
  // callers of this method don't know whether the load went through cached
  // image redirects.  This is handled by direct callers of the static
  // ShouldLoad.
  nsresult rv = ShouldLoad(false,   // aHadInsecureImageRedirect
                           aContentType,
                           aContentLocation,
                           aRequestingLocation,
                           aRequestingContext,
                           aMimeGuess,
                           aExtra,
                           aRequestPrincipal,
                           aDecision);
  return rv;
}

bool
nsMixedContentBlocker::IsPotentiallyTrustworthyLoopbackURL(nsIURI* aURL) {
  nsAutoCString host;
  nsresult rv = aURL->GetHost(host);
  NS_ENSURE_SUCCESS(rv, false);

  // We could also allow 'localhost' (if we can guarantee that it resolves
  // to a loopback address), but Chrome doesn't support it as of writing. For
  // web compat, lets only allow what Chrome allows.
  return host.Equals("127.0.0.1") || host.Equals("::1");
}

/* Static version of ShouldLoad() that contains all the Mixed Content Blocker
 * logic.  Called from non-static ShouldLoad().
 */
nsresult
nsMixedContentBlocker::ShouldLoad(bool aHadInsecureImageRedirect,
                                  uint32_t aContentType,
                                  nsIURI* aContentLocation,
                                  nsIURI* aRequestingLocation,
                                  nsISupports* aRequestingContext,
                                  const nsACString& aMimeGuess,
                                  nsISupports* aExtra,
                                  nsIPrincipal* aRequestPrincipal,
                                  int16_t* aDecision)
{
  // Asserting that we are on the main thread here and hence do not have to lock
  // and unlock sBlockMixedScript and sBlockMixedDisplay before reading/writing
  // to them.
  MOZ_ASSERT(NS_IsMainThread());

  bool isPreload = nsContentUtils::IsPreloadType(aContentType);

  // The content policy type that we receive may be an internal type for
  // scripts.  Let's remember if we have seen a worker type, and reset it to the
  // external type in all cases right now.
  bool isWorkerType = aContentType == nsIContentPolicy::TYPE_INTERNAL_WORKER ||
                      aContentType == nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER ||
                      aContentType == nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER;
  aContentType = nsContentUtils::InternalContentPolicyTypeToExternal(aContentType);

  // Assume active (high risk) content and blocked by default
  MixedContentTypes classification = eMixedScript;
  // Make decision to block/reject by default
  *aDecision = REJECT_REQUEST;

  // Notes on non-obvious decisions:
  //
  // TYPE_DTD: A DTD can contain entity definitions that expand to scripts.
  //
  // TYPE_FONT: The TrueType hinting mechanism is basically a scripting
  // language that gets interpreted by the operating system's font rasterizer.
  // Mixed content web fonts are relatively uncommon, and we can can fall back
  // to built-in fonts with minimal disruption in almost all cases.
  //
  // TYPE_OBJECT_SUBREQUEST could actually be either active content (e.g. a
  // script that a plugin will execute) or display content (e.g. Flash video
  // content).  Until we have a way to determine active vs passive content
  // from plugin requests (bug 836352), we will treat this as passive content.
  // This is to prevent false positives from causing users to become
  // desensitized to the mixed content blocker.
  //
  // TYPE_CSP_REPORT: High-risk because they directly leak information about
  // the content of the page, and because blocking them does not have any
  // negative effect on the page loading.
  //
  // TYPE_PING: Ping requests are POSTS, not GETs like images and media.
  // Also, PING requests have no bearing on the rendering or operation of
  // the page when used as designed, so even though they are lower risk than
  // scripts, blocking them is basically risk-free as far as compatibility is
  // concerned.
  //
  // TYPE_STYLESHEET: XSLT stylesheets can insert scripts. CSS positioning
  // and other advanced CSS features can possibly be exploited to cause
  // spoofing attacks (e.g. make a "grant permission" button look like a
  // "refuse permission" button).
  //
  // TYPE_BEACON: Beacon requests are similar to TYPE_PING, and are blocked by
  // default.
  //
  // TYPE_WEBSOCKET: The Websockets API requires browsers to
  // reject mixed-content websockets: "If secure is false but the origin of
  // the entry script has a scheme component that is itself a secure protocol,
  // e.g. HTTPS, then throw a SecurityError exception." We already block mixed
  // content websockets within the websockets implementation, so we don't need
  // to do any blocking here, nor do we need to provide a way to undo or
  // override the blocking. Websockets without TLS are very flaky anyway in the
  // face of many HTTP-aware proxies. Compared to passive content, there is
  // additional risk that the script using WebSockets will disclose sensitive
  // information from the HTTPS page and/or eval (directly or indirectly)
  // received data.
  //
  // TYPE_XMLHTTPREQUEST: XHR requires either same origin or CORS, so most
  // mixed-content XHR will already be blocked by that check. This will also
  // block HTTPS-to-HTTP XHR with CORS. The same security concerns mentioned
  // above for WebSockets apply to XHR, and XHR should have the same security
  // properties as WebSockets w.r.t. mixed content. XHR's handling of redirects
  // amplifies these concerns.


  static_assert(TYPE_DATAREQUEST == TYPE_XMLHTTPREQUEST,
                "TYPE_DATAREQUEST is not a synonym for "
                "TYPE_XMLHTTPREQUEST");

  switch (aContentType) {
    // The top-level document cannot be mixed content by definition
    case TYPE_DOCUMENT:
      *aDecision = ACCEPT;
      return NS_OK;
    // Creating insecure websocket connections in a secure page is blocked already
    // in the websocket constructor. We don't need to check the blocking here
    // and we don't want to un-block
    case TYPE_WEBSOCKET:
      *aDecision = ACCEPT;
      return NS_OK;

    // Static display content is considered moderate risk for mixed content so
    // these will be blocked according to the mixed display preference
    case TYPE_IMAGE:
    case TYPE_MEDIA:
    case TYPE_OBJECT_SUBREQUEST:
      classification = eMixedDisplay;
      break;

    // Active content (or content with a low value/risk-of-blocking ratio)
    // that has been explicitly evaluated; listed here for documentation
    // purposes and to avoid the assertion and warning for the default case.
    case TYPE_BEACON:
    case TYPE_CSP_REPORT:
    case TYPE_DTD:
    case TYPE_FETCH:
    case TYPE_FONT:
    case TYPE_IMAGESET:
    case TYPE_OBJECT:
    case TYPE_SCRIPT:
    case TYPE_STYLESHEET:
    case TYPE_SUBDOCUMENT:
    case TYPE_PING:
    case TYPE_WEB_MANIFEST:
    case TYPE_XBL:
    case TYPE_XMLHTTPREQUEST:
    case TYPE_XSLT:
    case TYPE_OTHER:
      break;


    // This content policy works as a whitelist.
    default:
      MOZ_ASSERT(false, "Mixed content of unknown type");
  }

  // Make sure to get the URI the load started with. No need to check
  // outer schemes because all the wrapping pseudo protocols inherit the
  // security properties of the actual network request represented
  // by the innerMost URL.
  nsCOMPtr<nsIURI> innerContentLocation = NS_GetInnermostURI(aContentLocation);
  if (!innerContentLocation) {
    NS_ERROR("Can't get innerURI from aContentLocation");
    *aDecision = REJECT_REQUEST;
    return NS_OK;
  }

 /* Get the scheme of the sub-document resource to be requested. If it is
  * a safe to load in an https context then mixed content doesn't apply.
  *
  * Check Protocol Flags to determine if scheme is safe to load:
  * URI_DOES_NOT_RETURN_DATA - e.g.
  *   "mailto"
  * URI_IS_LOCAL_RESOURCE - e.g.
  *   "data",
  *   "resource",
  *   "moz-icon"
  * URI_INHERITS_SECURITY_CONTEXT - e.g.
  *   "javascript"
  * URI_SAFE_TO_LOAD_IN_SECURE_CONTEXT - e.g.
  *   "https",
  *   "moz-safe-about"
  *
  */
  bool schemeLocal = false;
  bool schemeNoReturnData = false;
  bool schemeInherits = false;
  bool schemeSecure = false;
  if (NS_FAILED(NS_URIChainHasFlags(innerContentLocation, nsIProtocolHandler::URI_IS_LOCAL_RESOURCE , &schemeLocal))  ||
      NS_FAILED(NS_URIChainHasFlags(innerContentLocation, nsIProtocolHandler::URI_DOES_NOT_RETURN_DATA, &schemeNoReturnData)) ||
      NS_FAILED(NS_URIChainHasFlags(innerContentLocation, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT, &schemeInherits)) ||
      NS_FAILED(NS_URIChainHasFlags(innerContentLocation, nsIProtocolHandler::URI_SAFE_TO_LOAD_IN_SECURE_CONTEXT, &schemeSecure))) {
    *aDecision = REJECT_REQUEST;
    return NS_ERROR_FAILURE;
  }
  // TYPE_IMAGE redirects are cached based on the original URI, not the final
  // destination and hence cache hits for images may not have the correct
  // innerContentLocation.  Check if the cached hit went through an http redirect,
  // and if it did, we can't treat this as a secure subresource.
  if (!aHadInsecureImageRedirect &&
      (schemeLocal || schemeNoReturnData || schemeInherits || schemeSecure)) {
    *aDecision = ACCEPT;
     return NS_OK;
  }

  // Since there are cases where aRequestingLocation and aRequestPrincipal are
  // definitely not the owning document, we try to ignore them by extracting the
  // requestingLocation in the following order:
  // 1) from the aRequestingContext, either extracting
  //    a) the node's principal, or the
  //    b) script object's principal.
  // 2) if aRequestingContext yields a principal but no location, we check
  //    if its the system principal. If it is, allow the load.
  // 3) Special case handling for:
  //    a) speculative loads, where shouldLoad is called twice (bug 839235)
  //       and the first speculative load does not include a context.
  //       In this case we use aRequestingLocation to set requestingLocation.
  //    b) TYPE_CSP_REPORT which does not provide a context. In this case we
  //       use aRequestingLocation to set requestingLocation.
  //    c) content scripts from addon code that do not provide aRequestingContext
  //       or aRequestingLocation, but do provide aRequestPrincipal.
  //       If aRequestPrincipal is an expanded principal, we allow the load.
  // 4) If we still end up not having a requestingLocation, we reject the load.

  nsCOMPtr<nsIPrincipal> principal;
  // 1a) Try to get the principal if aRequestingContext is a node.
  nsCOMPtr<nsINode> node = do_QueryInterface(aRequestingContext);
  if (node) {
    principal = node->NodePrincipal();
  }

  // 1b) Try using the window's script object principal if it's not a node.
  if (!principal) {
    nsCOMPtr<nsIScriptObjectPrincipal> scriptObjPrin = do_QueryInterface(aRequestingContext);
    if (scriptObjPrin) {
      principal = scriptObjPrin->GetPrincipal();
    }
  }

  nsCOMPtr<nsIURI> requestingLocation;
  if (principal) {
    principal->GetURI(getter_AddRefs(requestingLocation));
  }

  // 2) if aRequestingContext yields a principal but no location, we check if its a system principal.
  if (principal && !requestingLocation) {
    if (nsContentUtils::IsSystemPrincipal(principal)) {
      *aDecision = ACCEPT;
      return NS_OK;
    }
  }

  // 3a,b) Special case handling for speculative loads and TYPE_CSP_REPORT. In
  // such cases, aRequestingContext doesn't exist, so we use aRequestingLocation.
  // Unfortunately we can not distinguish between speculative and normal loads here,
  // otherwise we could special case this assignment.
  if (!requestingLocation) {
    requestingLocation = aRequestingLocation;
  }

  // 3c) Special case handling for content scripts from addons code, which only
  // provide a aRequestPrincipal; aRequestingContext and aRequestingLocation are
  // both null; if the aRequestPrincipal is an expandedPrincipal, we allow the load.
  if (!principal && !requestingLocation && aRequestPrincipal) {
    nsCOMPtr<nsIExpandedPrincipal> expanded = do_QueryInterface(aRequestPrincipal);
    if (expanded) {
      *aDecision = ACCEPT;
      return NS_OK;
    }
  }

  // 4) Giving up. We still don't have a requesting location, therefore we can't tell
  //    if this is a mixed content load. Deny to be safe.
  if (!requestingLocation) {
    *aDecision = REJECT_REQUEST;
    return NS_OK;
  }

  // Check the parent scheme. If it is not an HTTPS page then mixed content
  // restrictions do not apply.
  bool parentIsHttps;
  nsCOMPtr<nsIURI> innerRequestingLocation = NS_GetInnermostURI(requestingLocation);
  if (!innerRequestingLocation) {
    NS_ERROR("Can't get innerURI from requestingLocation");
    *aDecision = REJECT_REQUEST;
    return NS_OK;
  }

  nsresult rv = innerRequestingLocation->SchemeIs("https", &parentIsHttps);
  if (NS_FAILED(rv)) {
    NS_ERROR("requestingLocation->SchemeIs failed");
    *aDecision = REJECT_REQUEST;
    return NS_OK;
  }
  if (!parentIsHttps) {
    *aDecision = ACCEPT;
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> docShell = NS_CP_GetDocShellFromContext(aRequestingContext);
  NS_ENSURE_TRUE(docShell, NS_OK);

  // Disallow mixed content loads for workers, shared workers and service
  // workers.
  if (isWorkerType) {
    // For workers, we can assume that we're mixed content at this point, since
    // the parent is https, and the protocol associated with innerContentLocation
    // doesn't map to the secure URI flags checked above.  Assert this for
    // sanity's sake
#ifdef DEBUG
    bool isHttpsScheme = false;
    rv = innerContentLocation->SchemeIs("https", &isHttpsScheme);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(!isHttpsScheme);
#endif
    *aDecision = REJECT_REQUEST;
    return NS_OK;
  }

  bool isHttpScheme = false;
  rv = innerContentLocation->SchemeIs("http", &isHttpScheme);
  NS_ENSURE_SUCCESS(rv, rv);

  // Loopback origins are not considered mixed content even over HTTP. See:
  // https://w3c.github.io/webappsec-mixed-content/#should-block-fetch
  if (isHttpScheme &&
      IsPotentiallyTrustworthyLoopbackURL(innerContentLocation)) {
    *aDecision = ACCEPT;
    return NS_OK;
  }

  // The page might have set the CSP directive 'upgrade-insecure-requests'. In such
  // a case allow the http: load to succeed with the promise that the channel will
  // get upgraded to https before fetching any data from the netwerk.
  // Please see: nsHttpChannel::Connect()
  //
  // Please note that the CSP directive 'upgrade-insecure-requests' only applies to
  // http: and ws: (for websockets). Websockets are not subject to mixed content
  // blocking since insecure websockets are not allowed within secure pages. Hence,
  // we only have to check against http: here. Skip mixed content blocking if the
  // subresource load uses http: and the CSP directive 'upgrade-insecure-requests'
  // is present on the page.
  nsIDocument* document = docShell->GetDocument();
  MOZ_ASSERT(document, "Expected a document");
  if (isHttpScheme && document->GetUpgradeInsecureRequests(isPreload)) {
    *aDecision = ACCEPT;
    return NS_OK;
  }

  // The page might have set the CSP directive 'block-all-mixed-content' which
  // should block not only active mixed content loads but in fact all mixed content
  // loads, see https://www.w3.org/TR/mixed-content/#strict-checking
  // Block all non secure loads in case the CSP directive is present. Please note
  // that at this point we already know, based on |schemeSecure| that the load is
  // not secure, so we can bail out early at this point.
  if (document->GetBlockAllMixedContent(isPreload)) {
    // log a message to the console before returning.
    nsAutoCString spec;
    rv = aContentLocation->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ConvertUTF8toUTF16 reportSpec(spec);

    const char16_t* params[] = { reportSpec.get()};
    CSP_LogLocalizedStr(u"blockAllMixedContent",
                        params, ArrayLength(params),
                        EmptyString(), // aSourceFile
                        EmptyString(), // aScriptSample
                        0, // aLineNumber
                        0, // aColumnNumber
                        nsIScriptError::errorFlag, "CSP",
                        document->InnerWindowID());
    *aDecision = REJECT_REQUEST;
    return NS_OK;
  }

  // Determine if the rootDoc is https and if the user decided to allow Mixed Content
  bool rootHasSecureConnection = false;
  bool allowMixedContent = false;
  bool isRootDocShell = false;
  rv = docShell->GetAllowMixedContentAndConnectionData(&rootHasSecureConnection, &allowMixedContent, &isRootDocShell);
  if (NS_FAILED(rv)) {
    *aDecision = REJECT_REQUEST;
    return rv;
  }

  // Get the sameTypeRoot tree item from the docshell
  nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
  docShell->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
  NS_ASSERTION(sameTypeRoot, "No root tree item from docshell!");

  // When navigating an iframe, the iframe may be https
  // but its parents may not be.  Check the parents to see if any of them are https.
  // If none of the parents are https, allow the load.
  if (aContentType == TYPE_SUBDOCUMENT && !rootHasSecureConnection) {

    bool httpsParentExists = false;

    nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
    parentTreeItem = docShell;

    while(!httpsParentExists && parentTreeItem) {
      nsCOMPtr<nsIWebNavigation> parentAsNav(do_QueryInterface(parentTreeItem));
      NS_ASSERTION(parentAsNav, "No web navigation object from parent's docshell tree item");
      nsCOMPtr<nsIURI> parentURI;

      parentAsNav->GetCurrentURI(getter_AddRefs(parentURI));
      if (!parentURI) {
        // if getting the URI fails, assume there is a https parent and break.
        httpsParentExists = true;
        break;
      }

      nsCOMPtr<nsIURI> innerParentURI = NS_GetInnermostURI(parentURI);
      if (!innerParentURI) {
        NS_ERROR("Can't get innerURI from parentURI");
        *aDecision = REJECT_REQUEST;
        return NS_OK;
      }

      if (NS_FAILED(innerParentURI->SchemeIs("https", &httpsParentExists))) {
        // if getting the scheme fails, assume there is a https parent and break.
        httpsParentExists = true;
        break;
      }

      // When the parent and the root are the same, we have traversed all the way up
      // the same type docshell tree.  Break out of the while loop.
      if(sameTypeRoot == parentTreeItem) {
        break;
      }

      // update the parent to the grandparent.
      nsCOMPtr<nsIDocShellTreeItem> newParentTreeItem;
      parentTreeItem->GetSameTypeParent(getter_AddRefs(newParentTreeItem));
      parentTreeItem = newParentTreeItem;
    } // end while loop.

    if (!httpsParentExists) {
      *aDecision = nsIContentPolicy::ACCEPT;
      return NS_OK;
    }
  }

  // Get the root document from the sameTypeRoot
  nsCOMPtr<nsIDocument> rootDoc = sameTypeRoot->GetDocument();
  NS_ASSERTION(rootDoc, "No root document from document shell root tree item.");

  // Get eventSink and the current security state from the docShell
  nsCOMPtr<nsISecurityEventSink> eventSink = do_QueryInterface(docShell);
  NS_ASSERTION(eventSink, "No eventSink from docShell.");
  nsCOMPtr<nsIDocShell> rootShell = do_GetInterface(sameTypeRoot);
  NS_ASSERTION(rootShell, "No root docshell from document shell root tree item.");
  uint32_t state = nsIWebProgressListener::STATE_IS_BROKEN;
  nsCOMPtr<nsISecureBrowserUI> securityUI;
  rootShell->GetSecurityUI(getter_AddRefs(securityUI));
  // If there is no securityUI, document doesn't have a security state.
  // Allow load and return early.
  if (!securityUI) {
    *aDecision = nsIContentPolicy::ACCEPT;
    return NS_OK;
  }
  nsresult stateRV = securityUI->GetState(&state);

  OriginAttributes originAttributes;
  if (principal) {
    originAttributes = principal->OriginAttributesRef();
  } else if (aRequestPrincipal) {
    originAttributes = aRequestPrincipal->OriginAttributesRef();
  }

  bool active = (classification == eMixedScript);
  bool doHSTSPriming = false;
  if (IsEligibleForHSTSPriming(aContentLocation)) {
    bool hsts = false;
    bool cached = false;
    nsCOMPtr<nsISiteSecurityService> sss =
      do_GetService(NS_SSSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, aContentLocation,
        0, originAttributes, &cached, nullptr, &hsts);
    NS_ENSURE_SUCCESS(rv, rv);

    if (hsts && sUseHSTS) {
      // assume we will be upgraded later
      Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS_PRIMING_2,
          (active) ? MixedContentHSTSPrimingState::eMCB_HSTS_ACTIVE_UPGRADE
                   : MixedContentHSTSPrimingState::eMCB_HSTS_PASSIVE_UPGRADE);
      *aDecision = ACCEPT;
      return NS_OK;
    }

    // Send a priming request if the result is not already cached and priming
    // requests are allowed
    if (!cached && sSendHSTSPriming) {
      // add this URI as a priming location
      doHSTSPriming = true;
      document->AddHSTSPrimingLocation(innerContentLocation,
          HSTSPrimingState::eHSTS_PRIMING_ALLOW);
      *aDecision = ACCEPT;
    }
  }

  // At this point we know that the request is mixed content, and the only
  // question is whether we block it.  Record telemetry at this point as to
  // whether HSTS would have fixed things by making the content location
  // into an HTTPS URL.
  //
  // Note that we count this for redirects as well as primary requests. This
  // will cause some degree of double-counting, especially when mixed content
  // is not blocked (e.g., for images).  For more detail, see:
  //   https://bugzilla.mozilla.org/show_bug.cgi?id=1198572#c19
  //
  // We do not count requests aHadInsecureImageRedirect=true, since these are
  // just an artifact of the image caching system.
  if (!aHadInsecureImageRedirect) {
    if (XRE_IsParentProcess()) {
      AccumulateMixedContentHSTS(innerContentLocation, active, doHSTSPriming,
                                 originAttributes);
    } else {
      // Ask the parent process to do the same call
      mozilla::dom::ContentChild* cc = mozilla::dom::ContentChild::GetSingleton();
      if (cc) {
        mozilla::ipc::URIParams uri;
        SerializeURI(innerContentLocation, uri);
        cc->SendAccumulateMixedContentHSTS(uri, active, doHSTSPriming,
                                           originAttributes);
      }
    }
  }

  // set hasMixedContentObjectSubrequest on this object if necessary
  if (aContentType == TYPE_OBJECT_SUBREQUEST) {
    rootDoc->SetHasMixedContentObjectSubrequest(true);
  }

  // If the content is display content, and the pref says display content should be blocked, block it.
  if (sBlockMixedDisplay && classification == eMixedDisplay) {
    if (allowMixedContent) {
      LogMixedContentMessage(classification, aContentLocation, rootDoc, eUserOverride);
      *aDecision = nsIContentPolicy::ACCEPT;
      // See if mixed display content has already loaded on the page or if the state needs to be updated here.
      // If mixed display hasn't loaded previously, then we need to call OnSecurityChange() to update the UI.
      if (rootDoc->GetHasMixedDisplayContentLoaded()) {
        return NS_OK;
      }
      rootDoc->SetHasMixedDisplayContentLoaded(true);

      if (rootHasSecureConnection) {
        // reset state security flag
        state = state >> 4 << 4;
        // set state security flag to broken, since there is mixed content
        state |= nsIWebProgressListener::STATE_IS_BROKEN;

        // If mixed active content is loaded, make sure to include that in the state.
        if (rootDoc->GetHasMixedActiveContentLoaded()) {
          state |= nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT;
        }

        eventSink->OnSecurityChange(aRequestingContext,
                                    (state | nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT));
      } else {
        // User has overriden the pref and the root is not https;
        // mixed display content was allowed on an https subframe.
        if (NS_SUCCEEDED(stateRV)) {
          eventSink->OnSecurityChange(aRequestingContext, (state | nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT));
        }
      }
    } else {
      if (doHSTSPriming) {
        document->AddHSTSPrimingLocation(innerContentLocation,
            HSTSPrimingState::eHSTS_PRIMING_BLOCK);
        *aDecision = nsIContentPolicy::ACCEPT;
      } else {
        *aDecision = nsIContentPolicy::REJECT_REQUEST;
      }
      LogMixedContentMessage(classification, aContentLocation, rootDoc, eBlocked);
      if (!rootDoc->GetHasMixedDisplayContentBlocked() && NS_SUCCEEDED(stateRV)) {
        rootDoc->SetHasMixedDisplayContentBlocked(true);
        eventSink->OnSecurityChange(aRequestingContext, (state | nsIWebProgressListener::STATE_BLOCKED_MIXED_DISPLAY_CONTENT));
      }
    }
    return NS_OK;

  } else if (sBlockMixedScript && classification == eMixedScript) {
    // If the content is active content, and the pref says active content should be blocked, block it
    // unless the user has choosen to override the pref
    if (allowMixedContent) {
      LogMixedContentMessage(classification, aContentLocation, rootDoc, eUserOverride);
      *aDecision = nsIContentPolicy::ACCEPT;
      // See if the state will change here. If it will, only then do we need to call OnSecurityChange() to update the UI.
      if (rootDoc->GetHasMixedActiveContentLoaded()) {
        return NS_OK;
      }
      rootDoc->SetHasMixedActiveContentLoaded(true);

      if (rootHasSecureConnection) {
        // reset state security flag
        state = state >> 4 << 4;
        // set state security flag to broken, since there is mixed content
        state |= nsIWebProgressListener::STATE_IS_BROKEN;

        // If mixed display content is loaded, make sure to include that in the state.
        if (rootDoc->GetHasMixedDisplayContentLoaded()) {
          state |= nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT;
        }

        eventSink->OnSecurityChange(aRequestingContext,
                                    (state | nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT));

        return NS_OK;
      } else {
        // User has already overriden the pref and the root is not https;
        // mixed active content was allowed on an https subframe.
        if (NS_SUCCEEDED(stateRV)) {
          eventSink->OnSecurityChange(aRequestingContext, (state | nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT));
        }
        return NS_OK;
      }
    } else {
      //User has not overriden the pref by Disabling protection. Reject the request and update the security state.
      if (doHSTSPriming) {
        document->AddHSTSPrimingLocation(innerContentLocation,
            HSTSPrimingState::eHSTS_PRIMING_BLOCK);
        *aDecision = nsIContentPolicy::ACCEPT;
      } else {
        *aDecision = nsIContentPolicy::REJECT_REQUEST;
      }
      LogMixedContentMessage(classification, aContentLocation, rootDoc, eBlocked);
      // See if the pref will change here. If it will, only then do we need to call OnSecurityChange() to update the UI.
      if (rootDoc->GetHasMixedActiveContentBlocked()) {
        return NS_OK;
      }
      rootDoc->SetHasMixedActiveContentBlocked(true);

      // The user has not overriden the pref, so make sure they still have an option by calling eventSink
      // which will invoke the doorhanger
      if (NS_SUCCEEDED(stateRV)) {
         eventSink->OnSecurityChange(aRequestingContext, (state | nsIWebProgressListener::STATE_BLOCKED_MIXED_ACTIVE_CONTENT));
      }
      return NS_OK;
    }
  } else {
    // The content is not blocked by the mixed content prefs.

    // Log a message that we are loading mixed content.
    LogMixedContentMessage(classification, aContentLocation, rootDoc, eUserOverride);

    // Fire the event from a script runner as it is unsafe to run script
    // from within ShouldLoad
    nsContentUtils::AddScriptRunner(
      new nsMixedContentEvent(aRequestingContext, classification, rootHasSecureConnection));
    *aDecision = ACCEPT;
    return NS_OK;
  }
}

NS_IMETHODIMP
nsMixedContentBlocker::ShouldProcess(uint32_t aContentType,
                                     nsIURI* aContentLocation,
                                     nsIURI* aRequestingLocation,
                                     nsISupports* aRequestingContext,
                                     const nsACString& aMimeGuess,
                                     nsISupports* aExtra,
                                     nsIPrincipal* aRequestPrincipal,
                                     int16_t* aDecision)
{
  aContentType = nsContentUtils::InternalContentPolicyTypeToExternal(aContentType);

  if (!aContentLocation) {
    // aContentLocation may be null when a plugin is loading without an associated URI resource
    if (aContentType == TYPE_OBJECT) {
       *aDecision = ACCEPT;
       return NS_OK;
    } else {
       *aDecision = REJECT_REQUEST;
       return NS_ERROR_FAILURE;
    }
  }

  return ShouldLoad(aContentType, aContentLocation, aRequestingLocation,
                    aRequestingContext, aMimeGuess, aExtra, aRequestPrincipal,
                    aDecision);
}

// Record information on when HSTS would have made mixed content not mixed
// content (regardless of whether it was actually blocked)
void
nsMixedContentBlocker::AccumulateMixedContentHSTS(
  nsIURI* aURI, bool aActive, bool aHasHSTSPriming,
  const OriginAttributes& aOriginAttributes)
{
  // This method must only be called in the parent, because
  // nsSiteSecurityService is only available in the parent
  if (!XRE_IsParentProcess()) {
    MOZ_ASSERT(false);
    return;
  }

  bool hsts;
  nsresult rv;
  nsCOMPtr<nsISiteSecurityService> sss = do_GetService(NS_SSSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return;
  }
  rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, aURI, 0,
                        aOriginAttributes, nullptr, nullptr, &hsts);
  if (NS_FAILED(rv)) {
    return;
  }

  // states: would upgrade, would prime, hsts info cached
  // active, passive
  //
  if (!aActive) {
    if (!hsts) {
      Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS,
                            MCB_HSTS_PASSIVE_NO_HSTS);
      if (aHasHSTSPriming) {
        Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS_PRIMING_2,
                              eMCB_HSTS_PASSIVE_DO_PRIMING);
      } else {
        Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS_PRIMING_2,
                              eMCB_HSTS_PASSIVE_NO_PRIMING);
      }
    }
    else {
      Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS,
                            MCB_HSTS_PASSIVE_WITH_HSTS);
      Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS_PRIMING_2,
                            eMCB_HSTS_PASSIVE_WITH_HSTS);
    }
  } else {
    if (!hsts) {
      Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS,
                            MCB_HSTS_ACTIVE_NO_HSTS);
      if (aHasHSTSPriming) {
        Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS_PRIMING_2,
                              eMCB_HSTS_ACTIVE_DO_PRIMING);
      } else {
        Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS_PRIMING_2,
                              eMCB_HSTS_ACTIVE_NO_PRIMING);
      }
    }
    else {
      Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS,
                            MCB_HSTS_ACTIVE_WITH_HSTS);
      Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS_PRIMING_2,
                            eMCB_HSTS_ACTIVE_WITH_HSTS);
    }
  }
}

//static
nsresult
nsMixedContentBlocker::MarkLoadInfoForPriming(nsIURI* aURI,
                                              nsISupports* aRequestingContext,
                                              nsILoadInfo* aLoadInfo)
{
  nsresult rv;
  bool sendPriming = false;
  bool mixedContentWouldBlock = false;
  rv = GetHSTSPrimingFromRequestingContext(aURI,
                                           aRequestingContext,
                                           &sendPriming,
                                           &mixedContentWouldBlock);
  NS_ENSURE_SUCCESS(rv, rv);

  if (sendPriming) {
    aLoadInfo->SetHSTSPriming(mixedContentWouldBlock);
  }

  return NS_OK;
}

//static
nsresult
nsMixedContentBlocker::GetHSTSPrimingFromRequestingContext(nsIURI* aURI,
    nsISupports* aRequestingContext,
    bool* aSendPrimingRequest,
    bool* aMixedContentWouldBlock)
{
  *aSendPrimingRequest = false;
  *aMixedContentWouldBlock = false;
  // If we marked for priming, we used the innermost URI, so get that
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(aURI);
  if (!innerURI) {
    NS_ERROR("Can't get innerURI from aContentLocation");
    return NS_ERROR_CONTENT_BLOCKED;
  }

  bool isHttp = false;
  innerURI->SchemeIs("http", &isHttp);
  if (!isHttp) {
    // there is nothign to do
    return NS_OK;
  }

  // If the DocShell was marked for HSTS priming, propagate that to the LoadInfo
  nsCOMPtr<nsIDocShell> docShell = NS_CP_GetDocShellFromContext(aRequestingContext);
  if (!docShell) {
    return NS_OK;
  }
  nsCOMPtr<nsIDocument> document = docShell->GetDocument();
  if (!document) {
    return NS_OK;
  }

  HSTSPrimingState status = document->GetHSTSPrimingStateForLocation(innerURI);
  if (status != HSTSPrimingState::eNO_HSTS_PRIMING) {
    *aSendPrimingRequest = (status != HSTSPrimingState::eNO_HSTS_PRIMING);
    *aMixedContentWouldBlock = (status == HSTSPrimingState::eHSTS_PRIMING_BLOCK);
  }

  return NS_OK;
}
