/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMixedContentBlocker.h"
#include "nsContentPolicyUtils.h"

#include "nsINode.h"
#include "nsCOMPtr.h"
#include "nsIDocShell.h"
#include "nsISecurityEventSink.h"
#include "nsIWebProgressListener.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

// Is mixed script blocking (fonts, plugin content, scripts, stylesheets,
// iframes, websockets, XHR) enabled?
bool nsMixedContentBlocker::sBlockMixedScript = false;

// Is mixed display content blocking (images, audio, video, <a ping>) enabled?
bool nsMixedContentBlocker::sBlockMixedDisplay = false;

// Fired at the document that attempted to load mixed content.  The UI could
// handle this event, for example, by displaying an info bar that offers the
// choice to reload the page with mixed content permitted.
//
// Disabled for now until bug 782654 is fixed
/*
class nsMixedContentBlockedEvent : public nsRunnable
{
public:
  nsMixedContentBlockedEvent(nsISupports *aContext, unsigned short aType)
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

    return NS_OK;
  }
private:
  // The requesting context for the content load. Generally, a DOM node from
  // the document that caused the load.
  nsCOMPtr<nsISupports> mContext;

  // The type of mixed content that was blocked, i.e. active or display
  unsigned short mType;
};
*/

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
  // Default policy: allow the load if we find no reason to block it.
  *aDecision = nsIContentPolicy::ACCEPT;

  // If mixed script blocking and mixed display blocking are turned off
  // we can return early
  if (!sBlockMixedScript && !sBlockMixedDisplay) {
    return NS_OK;
  }

  // Top-level load cannot be mixed content so allow it
  if (aContentType == nsIContentPolicy::TYPE_DOCUMENT) {
    return NS_OK;
  }

  // We need aRequestingLocation to pull out the scheme. If it isn't passed
  // in, get it from the DOM node.
  if (!aRequestingLocation) {
    nsCOMPtr<nsINode> node = do_QueryInterface(aRequestingContext);
    if (node) {
      nsCOMPtr<nsIURI> principalUri;
      node->NodePrincipal()->GetURI(getter_AddRefs(principalUri));
      aRequestingLocation = principalUri;
    }
    // If we still don't have a requesting location then we can't tell if
    // this is a mixed content load.  Deny to be safe.
    if (!aRequestingLocation) {
      *aDecision = nsIContentPolicy::REJECT_REQUEST;
      return NS_OK;
    }
  }

  // Check the parent scheme. If it is not an HTTPS page then mixed content
  // restrictions do not apply.
  bool parentIsHttps;
  if (NS_FAILED(aRequestingLocation->SchemeIs("https", &parentIsHttps)) ||
      !parentIsHttps) {
    return NS_OK;
  }

  // Get the scheme of the sub-document resource to be requested. If it is
  // an HTTPS load then mixed content doesn't apply.
  bool isHttps;
  if (NS_FAILED(aContentLocation->SchemeIs("https", &isHttps)) || isHttps) {
    return NS_OK;
  }

  // If we are here we have mixed content.

  // Decide whether or not to allow the mixed content based on what type of
  // content it is and if the user permitted it.
  switch (aContentType) {
    case nsIContentPolicy::TYPE_FONT:
    case nsIContentPolicy::TYPE_OBJECT:
    case nsIContentPolicy::TYPE_SCRIPT:
    case nsIContentPolicy::TYPE_STYLESHEET:
    case nsIContentPolicy::TYPE_SUBDOCUMENT:
    case nsIContentPolicy::TYPE_WEBSOCKET:
    case nsIContentPolicy::TYPE_XMLHTTPREQUEST:
      // fonts, plugin content, scripts, stylesheets, iframes, websockets and
      // XHRs are considered high risk for mixed content so these are blocked
      // per the mixed script preference
      if (sBlockMixedScript) {
        *aDecision = nsIContentPolicy::REJECT_REQUEST;

        // Fire the event from a script runner as it is unsafe to run script
        // from within ShouldLoad
        // Disabled until bug 782654 is fixed.
        /*
        nsContentUtils::AddScriptRunner(
          new nsMixedContentBlockedEvent(aRequestingContext, eBlockedMixedScript));
        */
      }
      break;

    case nsIContentPolicy::TYPE_IMAGE:
    case nsIContentPolicy::TYPE_MEDIA:
    case nsIContentPolicy::TYPE_PING:
      // display (static) content are considered moderate risk for mixed content
      // so these will be blocked according to the mixed display preference
      if (sBlockMixedDisplay) {
        *aDecision = nsIContentPolicy::REJECT_REQUEST;

        // Fire the event from a script runner as it is unsafe to run script
        // from within ShouldLoad
        // Disabled until bug 782654 is fixed.
        /*
        nsContentUtils::AddScriptRunner(
          new nsMixedContentBlockedEvent(aRequestingContext, eBlockedMixedDisplay));
        */
      }
      break;

    default:
      // other types of mixed content are allowed
      break;
  }

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
  return ShouldLoad(aContentType, aContentLocation, aRequestingLocation,
                    aRequestingContext, aMimeGuess, aExtra, aRequestPrincipal,
                    aDecision);
}
