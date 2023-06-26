/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FramingChecker.h"

#include <stdint.h>  // uint32_t

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsHttpChannel.h"
#include "nsContentSecurityUtils.h"
#include "nsGlobalWindowOuter.h"
#include "nsIContentPolicy.h"
#include "nsIScriptError.h"
#include "nsLiteralString.h"
#include "nsTArray.h"
#include "nsStringFwd.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"

#include "nsIObserverService.h"

using namespace mozilla;
using namespace mozilla::dom;

/* static */
void FramingChecker::ReportError(const char* aMessageTag,
                                 nsIHttpChannel* aChannel, nsIURI* aURI,
                                 const nsAString& aPolicy) {
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(aURI);

  nsCOMPtr<net::HttpBaseChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel) {
    return;
  }

  // Get the URL spec
  nsAutoCString spec;
  nsresult rv = aURI->GetAsciiSpec(spec);
  if (NS_FAILED(rv)) {
    return;
  }

  nsTArray<nsString> params;
  params.AppendElement(aPolicy);
  params.AppendElement(NS_ConvertUTF8toUTF16(spec));

  httpChannel->AddConsoleReport(nsIScriptError::errorFlag, "X-Frame-Options"_ns,
                                nsContentUtils::eSECURITY_PROPERTIES, spec, 0,
                                0, nsDependentCString(aMessageTag), params);

  // we are notifying observers for testing purposes because there is no event
  // to gather that an iframe load was blocked or not.
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  nsAutoString policy(aPolicy);
  observerService->NotifyObservers(aURI, "xfo-on-violate-policy", policy.get());
}

// Ignore x-frame-options if CSP with frame-ancestors exists
static bool ShouldIgnoreFrameOptions(nsIChannel* aChannel,
                                     nsIContentSecurityPolicy* aCSP) {
  NS_ENSURE_TRUE(aChannel, false);
  if (!aCSP) {
    return false;
  }

  bool enforcesFrameAncestors = false;
  aCSP->GetEnforcesFrameAncestors(&enforcesFrameAncestors);
  if (!enforcesFrameAncestors) {
    // if CSP does not contain frame-ancestors, then there
    // is nothing to do here.
    return false;
  }

  return true;
}

// Check if X-Frame-Options permits this document to be loaded as a
// subdocument. This will iterate through and check any number of
// X-Frame-Options policies in the request (comma-separated in a header,
// multiple headers, etc).
// This is based on:
// https://html.spec.whatwg.org/multipage/document-lifecycle.html#the-x-frame-options-header
/* static */
bool FramingChecker::CheckFrameOptions(nsIChannel* aChannel,
                                       nsIContentSecurityPolicy* aCsp,
                                       bool& outIsFrameCheckingSkipped) {
  // Step 1. If navigable is not a child navigable return true
  if (!aChannel) {
    return true;
  }

  // xfo check only makes sense for subdocument and object loads, if this is
  // not a load of such type, there is nothing to do here.
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  ExtContentPolicyType contentType = loadInfo->GetExternalContentPolicyType();
  if (contentType != ExtContentPolicy::TYPE_SUBDOCUMENT &&
      contentType != ExtContentPolicy::TYPE_OBJECT) {
    return true;
  }

  // xfo can only hang off an httpchannel, if this is not an httpChannel
  // then there is nothing to do here.
  nsCOMPtr<nsIHttpChannel> httpChannel;
  nsresult rv = nsContentSecurityUtils::GetHttpChannelFromPotentialMultiPart(
      aChannel, getter_AddRefs(httpChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }
  if (!httpChannel) {
    return true;
  }

  // ignore XFO checks on channels that will be redirected
  uint32_t responseStatus;
  rv = httpChannel->GetResponseStatus(&responseStatus);
  if (NS_FAILED(rv)) {
    // GetResponseStatus returning failure is expected in several situations, so
    // do not warn if it fails.
    return true;
  }
  if (mozilla::net::nsHttpChannel::IsRedirectStatus(responseStatus)) {
    return true;
  }

  nsAutoCString xfoHeaderValue;
  Unused << httpChannel->GetResponseHeader("X-Frame-Options"_ns,
                                           xfoHeaderValue);

  // Step 10. (paritally) if the only header we received was empty, then we
  // process it as if it wasn't sent at all.
  if (xfoHeaderValue.IsEmpty()) {
    return true;
  }

  // Step 2. xfo checks are ignored in the case where CSP frame-ancestors is
  // present, if so, there is nothing to do here.
  if (ShouldIgnoreFrameOptions(aChannel, aCsp)) {
    outIsFrameCheckingSkipped = true;
    return true;
  }

  // Step 3-4. reduce the header options to a unique set and count how many
  // unique values (that we track) are encountered. this avoids using a set to
  // stop attackers from inheriting arbitrary values in memory and reduce the
  // complexity of the code.
  XFOHeader xfoOptions;
  for (const nsACString& next : xfoHeaderValue.Split(',')) {
    nsAutoCString option(next);
    option.StripWhitespace();

    if (option.LowerCaseEqualsLiteral("allowall")) {
      xfoOptions.ALLOWALL = true;
    } else if (option.LowerCaseEqualsLiteral("sameorigin")) {
      xfoOptions.SAMEORIGIN = true;
    } else if (option.LowerCaseEqualsLiteral("deny")) {
      xfoOptions.DENY = true;
    } else {
      xfoOptions.INVALID = true;
    }
  }

  nsCOMPtr<nsIURI> uri;
  httpChannel->GetURI(getter_AddRefs(uri));

  // Step 6. if header has multiple contradicting directives return early and
  // prohibit the load. ALLOWALL is considered here for legacy reasons.
  uint32_t xfoUniqueOptions = xfoOptions.DENY + xfoOptions.ALLOWALL +
                              xfoOptions.SAMEORIGIN + xfoOptions.INVALID;
  if (xfoUniqueOptions > 1 &&
      (xfoOptions.DENY || xfoOptions.ALLOWALL || xfoOptions.SAMEORIGIN)) {
    ReportError("XFrameOptionsInvalid", httpChannel, uri, u"invalid"_ns);
    return false;
  }

  // Step 7 (multiple INVALID values) and partially Step 10 (single INVALID
  // value). if header has any invalid options, but no valid directives (DENY,
  // ALLOWALL, SAMEORIGIN) then allow the load.
  if (xfoOptions.INVALID) {
    ReportError("XFrameOptionsInvalid", httpChannel, uri, u"invalid"_ns);
    return true;
  }

  // Step 8. if the value of the header is DENY prohibit the load.
  if (xfoOptions.DENY) {
    ReportError("XFrameOptionsDeny", httpChannel, uri, u"deny"_ns);
    return false;
  }

  // Step 9. If the X-Frame-Options value is SAMEORIGIN, then the top frame in
  // the parent chain must be from the same origin as this document.
  RefPtr<mozilla::dom::BrowsingContext> ctx;
  loadInfo->GetBrowsingContext(getter_AddRefs(ctx));

  while (ctx && xfoOptions.SAMEORIGIN) {
    nsCOMPtr<nsIPrincipal> principal;
    // Generally CheckFrameOptions is consulted from within the
    // DocumentLoadListener in the parent process. For loads of type object and
    // embed it's called from the Document in the content process.
    if (XRE_IsParentProcess()) {
      WindowGlobalParent* window = ctx->Canonical()->GetCurrentWindowGlobal();
      if (window) {
        // Using the URI of the Principal and not the document because
        // window.open inherits the principal and hence the URI of the opening
        // context needed for same origin checks.
        principal = window->DocumentPrincipal();
      }
    } else if (nsPIDOMWindowOuter* windowOuter = ctx->GetDOMWindow()) {
      principal = nsGlobalWindowOuter::Cast(windowOuter)->GetPrincipal();
    }

    if (principal && principal->IsSystemPrincipal()) {
      return true;
    }

    // one of the ancestors is not same origin as this document
    if (!principal || !principal->IsSameOrigin(uri)) {
      ReportError("XFrameOptionsDeny", httpChannel, uri, u"sameorigin"_ns);
      return false;
    }
    ctx = ctx->GetParent();
  }

  // Step 10.
  return true;
}
