/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FramingChecker.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsCSPUtils.h"
#include "nsDocShell.h"
#include "nsHttpChannel.h"
#include "nsIChannel.h"
#include "nsIConsoleReportCollector.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIScriptError.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "nsTArray.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/net/HttpBaseChannel.h"

#include "nsIObserverService.h"

using namespace mozilla;

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

  httpChannel->AddConsoleReport(nsIScriptError::errorFlag,
                                NS_LITERAL_CSTRING("X-Frame-Options"),
                                nsContentUtils::eSECURITY_PROPERTIES, spec, 0,
                                0, nsDependentCString(aMessageTag), params);
}

/* static */
bool FramingChecker::CheckOneFrameOptionsPolicy(nsIHttpChannel* aHttpChannel,
                                                const nsAString& aPolicy) {
  nsCOMPtr<nsIURI> uri;
  aHttpChannel->GetURI(getter_AddRefs(uri));

  // return early if header does not have one of the values with meaning
  if (!aPolicy.LowerCaseEqualsLiteral("deny") &&
      !aPolicy.LowerCaseEqualsLiteral("sameorigin")) {
    ReportError("XFrameOptionsInvalid", aHttpChannel, uri, aPolicy);
    return true;
  }

  // If the X-Frame-Options value is SAMEORIGIN, then the top frame in the
  // parent chain must be from the same origin as this document.
  bool checkSameOrigin = aPolicy.LowerCaseEqualsLiteral("sameorigin");

  nsCOMPtr<nsILoadInfo> loadInfo = aHttpChannel->LoadInfo();
  RefPtr<mozilla::dom::BrowsingContext> ctx;
  loadInfo->GetBrowsingContext(getter_AddRefs(ctx));

  while (ctx) {
    nsCOMPtr<nsIPrincipal> principal;
    WindowGlobalParent* window = ctx->Canonical()->GetCurrentWindowGlobal();
    if (window) {
      // Using the URI of the Principal and not the document because e.g.
      // window.open inherits the principal and hence the URI of the
      // opening context needed for same origin checks.
      principal = window->DocumentPrincipal();
    }

    if (principal && principal->IsSystemPrincipal()) {
      return true;
    }

    if (checkSameOrigin) {
      bool isPrivateWin = false;
      bool isSameOrigin = false;
      if (principal) {
        isPrivateWin = principal->OriginAttributesRef().mPrivateBrowsingId > 0;
        principal->IsSameOrigin(uri, isPrivateWin, &isSameOrigin);
      }
      // one of the ancestors is not same origin as this document
      if (!isSameOrigin) {
        ReportError("XFrameOptionsDeny", aHttpChannel, uri, aPolicy);
        return false;
      }
    }
    ctx = ctx->GetParent();
  }

  // If the value of the header is DENY, and the previous condition is
  // not met (current docshell is not the top docshell), prohibit the
  // load.
  if (aPolicy.LowerCaseEqualsLiteral("deny")) {
    ReportError("XFrameOptionsDeny", aHttpChannel, uri, aPolicy);
    return false;
  }

  return true;
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

  // log warning to console that xfo is ignored because of CSP
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  uint64_t innerWindowID = loadInfo->GetInnerWindowID();
  bool privateWindow = !!loadInfo->GetOriginAttributes().mPrivateBrowsingId;
  AutoTArray<nsString, 2> params = {NS_LITERAL_STRING("x-frame-options"),
                                    NS_LITERAL_STRING("frame-ancestors")};
  CSP_LogLocalizedStr("IgnoringSrcBecauseOfDirective", params,
                      EmptyString(),  // no sourcefile
                      EmptyString(),  // no scriptsample
                      0,              // no linenumber
                      0,              // no columnnumber
                      nsIScriptError::warningFlag,
                      NS_LITERAL_CSTRING("IgnoringSrcBecauseOfDirective"),
                      innerWindowID, privateWindow);

  return true;
}

// Check if X-Frame-Options permits this document to be loaded as a
// subdocument. This will iterate through and check any number of
// X-Frame-Options policies in the request (comma-separated in a header,
// multiple headers, etc).
/* static */
bool FramingChecker::CheckFrameOptions(nsIChannel* aChannel,
                                       nsIContentSecurityPolicy* aCsp) {
  MOZ_ASSERT(XRE_IsParentProcess(), "check frame options only in parent");

  if (!aChannel) {
    return true;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsContentPolicyType contentType = loadInfo->GetExternalContentPolicyType();

  // xfo check only makes sense for subdocument and object loads, if this is
  // not a load of such type, there is nothing to do here.
  if (contentType != nsIContentPolicy::TYPE_SUBDOCUMENT &&
      contentType != nsIContentPolicy::TYPE_OBJECT) {
    return true;
  }

  // xfo checks are ignored in case CSP frame-ancestors is present,
  // if so, there is nothing to do here.
  if (ShouldIgnoreFrameOptions(aChannel, aCsp)) {
    return true;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel;
  nsresult rv = nsContentSecurityUtils::GetHttpChannelFromPotentialMultiPart(
      aChannel, getter_AddRefs(httpChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }

  // xfo can only hang off an httpchannel, if this is not an httpChannel
  // then there is nothing to do here.
  if (!httpChannel) {
    return true;
  }

  // ignore XFO checks on channels that will be redirected
  uint32_t responseStatus;
  rv = httpChannel->GetResponseStatus(&responseStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }
  if (mozilla::net::nsHttpChannel::IsRedirectStatus(responseStatus)) {
    return true;
  }

  nsAutoCString xfoHeaderCValue;
  Unused << httpChannel->GetResponseHeader(
      NS_LITERAL_CSTRING("X-Frame-Options"), xfoHeaderCValue);
  NS_ConvertUTF8toUTF16 xfoHeaderValue(xfoHeaderCValue);

  // if no header value, there's nothing to do.
  if (xfoHeaderValue.IsEmpty()) {
    return true;
  }

  // iterate through all the header values (usually there's only one, but can
  // be many.  If any want to deny the load, deny the load.
  nsCharSeparatedTokenizer tokenizer(xfoHeaderValue, ',');
  while (tokenizer.hasMoreTokens()) {
    const nsAString& tok = tokenizer.nextToken();
    if (!CheckOneFrameOptionsPolicy(httpChannel, tok)) {
      // if xfo blocks the load we are notifying observers for
      // testing purposes because there is no event to gather
      // what an iframe load was blocked or not.
      nsCOMPtr<nsIURI> uri;
      httpChannel->GetURI(getter_AddRefs(uri));
      nsCOMPtr<nsIObserverService> observerService =
          mozilla::services::GetObserverService();
      nsAutoString policy(tok);
      observerService->NotifyObservers(uri, "xfo-on-violate-policy",
                                       policy.get());
      return false;
    }
  }
  return true;
}
