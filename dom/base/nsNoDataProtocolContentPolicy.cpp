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
#include "nsString.h"
#include "nsIProtocolHandler.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"

NS_IMPL_ISUPPORTS(nsNoDataProtocolContentPolicy, nsIContentPolicy)

NS_IMETHODIMP
nsNoDataProtocolContentPolicy::ShouldLoad(nsIURI* aContentLocation,
                                          nsILoadInfo* aLoadInfo,
                                          int16_t* aDecision) {
  ExtContentPolicyType contentType = aLoadInfo->GetExternalContentPolicyType();

  *aDecision = nsIContentPolicy::ACCEPT;

  // Don't block for TYPE_OBJECT since such URIs are sometimes loaded by the
  // plugin, so they don't necessarily open external apps
  // TYPE_WEBSOCKET loads can only go to ws:// or wss://, so we don't need to
  // concern ourselves with them.
  if (contentType != ExtContentPolicy::TYPE_DOCUMENT &&
      contentType != ExtContentPolicy::TYPE_SUBDOCUMENT &&
      contentType != ExtContentPolicy::TYPE_OBJECT &&
      contentType != ExtContentPolicy::TYPE_WEBSOCKET) {
    // The following are just quick-escapes for the most common cases
    // where we would allow the content to be loaded anyway.
    nsAutoCString scheme;
    aContentLocation->GetScheme(scheme);
    if (scheme.EqualsLiteral("http") || scheme.EqualsLiteral("https") ||
        scheme.EqualsLiteral("file") || scheme.EqualsLiteral("chrome")) {
      return NS_OK;
    }

    if (nsContentUtils::IsExternalProtocol(aContentLocation)) {
      NS_SetRequestBlockingReason(
          aLoadInfo,
          nsILoadInfo::BLOCKING_REASON_CONTENT_POLICY_NO_DATA_PROTOCOL);
      *aDecision = nsIContentPolicy::REJECT_REQUEST;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNoDataProtocolContentPolicy::ShouldProcess(nsIURI* aContentLocation,
                                             nsILoadInfo* aLoadInfo,
                                             int16_t* aDecision) {
  return ShouldLoad(aContentLocation, aLoadInfo, aDecision);
}
