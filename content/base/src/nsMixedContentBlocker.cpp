/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMixedContentBlocker.h"
#include "nsContentPolicyUtils.h"

#include "nsINode.h"
#include "nsCOMPtr.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsISecurityEventSink.h"
#include "nsIWebProgressListener.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsIRequest.h"
#include "nsIDocument.h"
#include "nsIContentViewer.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "mozilla/Preferences.h"
#include "nsIScriptObjectPrincipal.h"

#include "prlog.h"

using namespace mozilla;

// Is mixed script blocking (fonts, plugin content, scripts, stylesheets,
// iframes, websockets, XHR) enabled?
bool nsMixedContentBlocker::sBlockMixedScript = false;

// Is mixed display content blocking (images, audio, video, <a ping>) enabled?
bool nsMixedContentBlocker::sBlockMixedDisplay = false;

// Fired at the document that attempted to load mixed content.  The UI could
// handle this event, for example, by displaying an info bar that offers the
// choice to reload the page with mixed content permitted.
class nsMixedContentEvent : public nsRunnable
{
public:
  nsMixedContentEvent(nsISupports *aContext, MixedContentTypes aType)
    : mContext(aContext), mType(aType)
  {}

  NS_IMETHOD Run()
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
    nsCOMPtr<nsIDocShellTreeItem> currentDocShellTreeItem(do_QueryInterface(docShell));
    if (!currentDocShellTreeItem) {
        return NS_OK;
    }
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    currentDocShellTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    NS_ASSERTION(sameTypeRoot, "No document shell root tree item from document shell tree item!");

    // now get the document from sameTypeRoot
    nsCOMPtr<nsIDocument> rootDoc = do_GetInterface(sameTypeRoot);
    NS_ASSERTION(rootDoc, "No root document from document shell root tree item.");


    if (mType == eMixedScript) {
      rootDoc->SetHasMixedActiveContentLoaded(true);

      // Update the security UI in the tab with the blocked mixed content
      nsCOMPtr<nsISecurityEventSink> eventSink = do_QueryInterface(docShell);
      if (eventSink) {
        eventSink->OnSecurityChange(mContext, nsIWebProgressListener::STATE_IS_BROKEN);
      }

    } else {
        if (mType == eMixedDisplay) {
          //Do Nothing for now; state will already be set STATE_IS_BROKEN
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
};


nsMixedContentBlocker::nsMixedContentBlocker()
{
  // Cache the pref for mixed script blocking
  Preferences::AddBoolVarCache(&sBlockMixedScript,
                               "security.mixed_content.block_active_content");

  // Cache the pref for mixed display blocking
  Preferences::AddBoolVarCache(&sBlockMixedDisplay,
                               "security.mixed_content.block_display_content");
}

nsMixedContentBlocker::~nsMixedContentBlocker()
{
}

NS_IMPL_ISUPPORTS1(nsMixedContentBlocker, nsIContentPolicy)

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
  // Asserting that we are on the main thread here and hence do not have to lock
  // and unlock sBlockMixedScript and sBlockMixedDisplay before reading/writing
  // to them.
  MOZ_ASSERT(NS_IsMainThread());

  // Assume active (high risk) content and blocked by default
  MixedContentTypes classification = eMixedScript;


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
  // content).
  //
  // TYPE_CSP_REPORT: High-risk because they directly leak information about
  // the content of the page, and because blocking them does not have any
  // negative effect on the page loading.
  //
  // TYPE_PING: Ping requests are POSTS, not GETs like images and media.
  // Also, PING requests have no bearing on the rendering or operation of
  // the page when used as designed, so even though they are lower risk than
  // scripts, blocking them is basically risk-free as far as compatibility is
  // concerned.  Ping is turned off by default in Firefox, so unless a user
  // opts into ping, no request will be made.  Categorizing this as Mixed
  // Display Content for now, but this is subject to change.
  //
  // TYPE_STYLESHEET: XSLT stylesheets can insert scripts. CSS positioning
  // and other advanced CSS features can possibly be exploited to cause
  // spoofing attacks (e.g. make a "grant permission" button look like a
  // "refuse permission" button).
  //
  // TYPE_WEBSOCKET: The Websockets API requires browsers to
  // reject mixed-content websockets: "If secure is false but the origin of
  // the entry script has a scheme component that is itself a secure protocol,
  // e.g. HTTPS, then throw a SecurityError exception." We already block mixed
  // content websockets within the websockets implementation, so we don't need
  // to do any blocking here, nor do we need to provide a way to undo or
  // override the blocking. Websockets without TLS are very flaky anyway in the
  // face of many HTTP-aware proxies. Compared to psasive content, there is
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


  MOZ_STATIC_ASSERT(TYPE_DATAREQUEST == TYPE_XMLHTTPREQUEST,
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
    case TYPE_PING:
      classification = eMixedDisplay;
      break;

    // Active content (or content with a low value/risk-of-blocking ratio)
    // that has been explicitly evaluated; listed here for documentation
    // purposes and to avoid the assertion and warning for the default case.
    case TYPE_CSP_REPORT:
    case TYPE_DTD:
    case TYPE_FONT:
    case TYPE_OBJECT:
    case TYPE_OBJECT_SUBREQUEST:
    case TYPE_SCRIPT:
    case TYPE_STYLESHEET:
    case TYPE_SUBDOCUMENT:
    case TYPE_XBL:
    case TYPE_XMLHTTPREQUEST:
    case TYPE_OTHER:
      break;


    // This content policy works as a whitelist.
    default:
      MOZ_NOT_REACHED("Mixed content of unknown type");
      NS_WARNING("Mixed content of unknown type");
      break;
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
  if (NS_FAILED(NS_URIChainHasFlags(aContentLocation, nsIProtocolHandler::URI_IS_LOCAL_RESOURCE , &schemeLocal))  ||
      NS_FAILED(NS_URIChainHasFlags(aContentLocation, nsIProtocolHandler::URI_DOES_NOT_RETURN_DATA, &schemeNoReturnData)) ||
      NS_FAILED(NS_URIChainHasFlags(aContentLocation, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT, &schemeInherits)) ||
      NS_FAILED(NS_URIChainHasFlags(aContentLocation, nsIProtocolHandler::URI_SAFE_TO_LOAD_IN_SECURE_CONTEXT, &schemeSecure))) {
    return NS_ERROR_FAILURE;
  }

  if (schemeLocal || schemeNoReturnData || schemeInherits || schemeSecure) {
     return NS_OK;
  }

  // We need aRequestingLocation to pull out the scheme. If it isn't passed
  // in, get it from the aRequestingPricipal
  if (!aRequestingLocation) {
    if (!aRequestPrincipal) {
      // If we don't have aRequestPrincipal, try getting it from the
      // DOM node using aRequestingContext
      nsCOMPtr<nsINode> node = do_QueryInterface(aRequestingContext);
      if (node) {
        aRequestPrincipal = node->NodePrincipal();
      } else {
        // Try using the window's script object principal if it's not a node.
        nsCOMPtr<nsIScriptObjectPrincipal> scriptObjPrin = do_QueryInterface(aRequestingContext);
        if (scriptObjPrin) {
          aRequestPrincipal = scriptObjPrin->GetPrincipal();
        }
      }
    }
    if (aRequestPrincipal) {
      nsCOMPtr<nsIURI> principalUri;
      nsresult rvalue = aRequestPrincipal->GetURI(getter_AddRefs(principalUri));
      if (NS_SUCCEEDED(rvalue)) {
        aRequestingLocation = principalUri;
      }
    }

    if (!aRequestingLocation) {
      // If content scripts from an addon are causing this load, they have an
      // ExpandedPrincipal instead of a Principal. This is pseudo-privileged code, so allow
      // the load. Or if this is system principal, allow the load.
      nsCOMPtr<nsIExpandedPrincipal> expanded = do_QueryInterface(aRequestPrincipal);
      if (expanded || (aRequestPrincipal && nsContentUtils::IsSystemPrincipal(aRequestPrincipal))) {
        *aDecision = ACCEPT;
        return NS_OK;
      } else {
        // We still don't have a requesting location and there is no Expanded Principal.
        // We can't tell if this is a mixed content load.  Deny to be safe.
        *aDecision = REJECT_REQUEST;
        return NS_OK;
      }
    }
  }

  // Check the parent scheme. If it is not an HTTPS page then mixed content
  // restrictions do not apply.
  bool parentIsHttps;
  nsresult rv = aRequestingLocation->SchemeIs("https", &parentIsHttps);
  if (NS_FAILED(rv)) {
    NS_ERROR("aRequestingLocation->SchemeIs failed");
    *aDecision = REJECT_REQUEST;
    return NS_OK;
  }
  if (!parentIsHttps) {
    *aDecision = ACCEPT;
    return NS_OK;
  }

  // If we are here we have mixed content.
  // If the content is display content, and the pref says display content should be blocked, block it.
  // If the content is mixed content, and the pref says mixed content should be blocked, block it.
  if ( (sBlockMixedDisplay && classification == eMixedDisplay) || (sBlockMixedScript && classification == eMixedScript) ) {
     *aDecision = nsIContentPolicy::REJECT_REQUEST;
     return NS_OK;
  } else {
    // The content is not blocked by the mixed content prefs.

    // Fire the event from a script runner as it is unsafe to run script
    // from within ShouldLoad
    nsContentUtils::AddScriptRunner(
      new nsMixedContentEvent(aRequestingContext, classification));
    return NS_OK;
  }

  *aDecision = REJECT_REQUEST;
  return NS_OK;
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
  if (!aContentLocation) {
    // aContentLocation may be null when a plugin is loading without an associated URI resource
    if (aContentType == TYPE_OBJECT) {
       return NS_OK;
    } else {
       return NS_ERROR_FAILURE;
    }
  }

  return ShouldLoad(aContentType, aContentLocation, aRequestingLocation,
                    aRequestingContext, aMimeGuess, aExtra, aRequestPrincipal,
                    aDecision);
}
