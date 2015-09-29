/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Content policy implementation that prevents all loads of images,
 * subframes, etc from protocols that don't return data but rather open
 * applications (such as mailto).
 */

#include "nsNoDataProtocolContentPolicy.h"
#include "nsIDOMWindow.h"
#include "nsString.h"
#include "nsIProtocolHandler.h"
#include "nsIIOService.h"
#include "nsIExternalProtocolHandler.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"

NS_IMPL_ISUPPORTS(nsNoDataProtocolContentPolicy, nsIContentPolicy)

NS_IMETHODIMP
nsNoDataProtocolContentPolicy::ShouldLoad(uint32_t aContentType,
                                          nsIURI *aContentLocation,
                                          nsIURI *aRequestingLocation,
                                          nsISupports *aRequestingContext,
                                          const nsACString &aMimeGuess,
                                          nsISupports *aExtra,
                                          nsIPrincipal *aRequestPrincipal,
                                          int16_t *aDecision)
{
  MOZ_ASSERT(aContentType == nsContentUtils::InternalContentPolicyTypeToExternal(aContentType),
             "We should only see external content policy types here.");

  *aDecision = nsIContentPolicy::ACCEPT;

  // Don't block for TYPE_OBJECT since such URIs are sometimes loaded by the
  // plugin, so they don't necessarily open external apps
  // TYPE_WEBSOCKET loads can only go to ws:// or wss://, so we don't need to
  // concern ourselves with them.
  if (aContentType != TYPE_DOCUMENT &&
      aContentType != TYPE_SUBDOCUMENT &&
      aContentType != TYPE_OBJECT &&
      aContentType != TYPE_WEBSOCKET) {

    // The following are just quick-escapes for the most common cases
    // where we would allow the content to be loaded anyway.
    nsAutoCString scheme;
    aContentLocation->GetScheme(scheme);
    if (scheme.EqualsLiteral("http") ||
        scheme.EqualsLiteral("https") ||
        scheme.EqualsLiteral("ftp") ||
        scheme.EqualsLiteral("file") ||
        scheme.EqualsLiteral("chrome")) {
      return NS_OK;
    }

    bool shouldBlock;
    nsresult rv = NS_URIChainHasFlags(aContentLocation,
                                      nsIProtocolHandler::URI_DOES_NOT_RETURN_DATA,
                                      &shouldBlock);
    if (NS_SUCCEEDED(rv) && shouldBlock) {
      *aDecision = nsIContentPolicy::REJECT_REQUEST;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNoDataProtocolContentPolicy::ShouldProcess(uint32_t aContentType,
                                             nsIURI *aContentLocation,
                                             nsIURI *aRequestingLocation,
                                             nsISupports *aRequestingContext,
                                             const nsACString &aMimeGuess,
                                             nsISupports *aExtra,
                                             nsIPrincipal *aRequestPrincipal,
                                             int16_t *aDecision)
{
  return ShouldLoad(aContentType, aContentLocation, aRequestingLocation,
                    aRequestingContext, aMimeGuess, aExtra, aRequestPrincipal,
                    aDecision);
}
