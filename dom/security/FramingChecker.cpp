/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FramingChecker.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsCSPUtils.h"
#include "nsDocShell.h"
#include "nsIChannel.h"
#include "nsIConsoleService.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIScriptError.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"
#include "mozilla/NullPrincipal.h"

using namespace mozilla;

/* static */
bool FramingChecker::CheckOneFrameOptionsPolicy(nsIHttpChannel* aHttpChannel,
                                                const nsAString& aPolicy,
                                                nsIDocShell* aDocShell) {
  static const char allowFrom[] = "allow-from";
  const uint32_t allowFromLen = ArrayLength(allowFrom) - 1;
  bool isAllowFrom =
      StringHead(aPolicy, allowFromLen).LowerCaseEqualsLiteral(allowFrom);

  // return early if header does not have one of the values with meaning
  if (!aPolicy.LowerCaseEqualsLiteral("deny") &&
      !aPolicy.LowerCaseEqualsLiteral("sameorigin") && !isAllowFrom) {
    return true;
  }

  nsCOMPtr<nsIURI> uri;
  aHttpChannel->GetURI(getter_AddRefs(uri));

  // XXXkhuey when does this happen?  Is returning true safe here?
  if (!aDocShell) {
    return true;
  }

  // We need to check the location of this window and the location of the top
  // window, if we're not the top.  X-F-O: SAMEORIGIN requires that the
  // document must be same-origin with top window.  X-F-O: DENY requires that
  // the document must never be framed.
  nsCOMPtr<nsPIDOMWindowOuter> thisWindow = aDocShell->GetWindow();
  // If we don't have DOMWindow there is no risk of clickjacking
  if (!thisWindow) {
    return true;
  }

  // GetScriptableTop, not GetTop, because we want this to respect
  // <iframe mozbrowser> boundaries.
  nsCOMPtr<nsPIDOMWindowOuter> topWindow = thisWindow->GetScriptableTop();

  // if the document is in the top window, it's not in a frame.
  if (thisWindow == topWindow) {
    return true;
  }

  // Find the top docshell in our parent chain that doesn't have the system
  // principal and use it for the principal comparison.  Finding the top
  // content-type docshell doesn't work because some chrome documents are
  // loaded in content docshells (see bug 593387).
  nsCOMPtr<nsIDocShellTreeItem> thisDocShellItem(aDocShell);
  nsCOMPtr<nsIDocShellTreeItem> parentDocShellItem;
  nsCOMPtr<nsIDocShellTreeItem> curDocShellItem = thisDocShellItem;
  nsCOMPtr<Document> topDoc;
  nsresult rv;
  nsCOMPtr<nsIScriptSecurityManager> ssm =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (!ssm) {
    MOZ_CRASH();
  }

  // If the X-Frame-Options value is SAMEORIGIN, then the top frame in the
  // parent chain must be from the same origin as this document.
  bool checkSameOrigin = aPolicy.LowerCaseEqualsLiteral("sameorigin");
  nsCOMPtr<nsIURI> topUri;

  // Traverse up the parent chain and stop when we see a docshell whose
  // parent has a system principal, or a docshell corresponding to
  // <iframe mozbrowser>.
  while (NS_SUCCEEDED(
             curDocShellItem->GetParent(getter_AddRefs(parentDocShellItem))) &&
         parentDocShellItem) {
    nsCOMPtr<nsIDocShell> curDocShell = do_QueryInterface(curDocShellItem);
    if (curDocShell && curDocShell->GetIsMozBrowser()) {
      break;
    }

    topDoc = parentDocShellItem->GetDocument();
    if (topDoc) {
      if (topDoc->NodePrincipal()->IsSystemPrincipal()) {
        // Found a system-principled doc: last docshell was top.
        break;
      }

      if (checkSameOrigin) {
        topDoc->NodePrincipal()->GetURI(getter_AddRefs(topUri));
        bool isPrivateWin =
            topDoc->NodePrincipal()->OriginAttributesRef().mPrivateBrowsingId >
            0;
        rv = ssm->CheckSameOriginURI(uri, topUri, true, isPrivateWin);

        // one of the ancestors is not same origin as this document
        if (NS_FAILED(rv)) {
          ReportXFOViolation(curDocShellItem, uri, eSAMEORIGIN);
          return false;
        }
      }
    } else {
      return false;
    }
    curDocShellItem = parentDocShellItem;
  }

  // If this document has the top non-SystemPrincipal docshell it is not being
  // framed or it is being framed by a chrome document, which we allow.
  if (curDocShellItem == thisDocShellItem) {
    return true;
  }

  // If the value of the header is DENY, and the previous condition is
  // not met (current docshell is not the top docshell), prohibit the
  // load.
  if (aPolicy.LowerCaseEqualsLiteral("deny")) {
    ReportXFOViolation(curDocShellItem, uri, eDENY);
    return false;
  }

  topDoc = curDocShellItem->GetDocument();
  topDoc->NodePrincipal()->GetURI(getter_AddRefs(topUri));

  // If the X-Frame-Options value is "allow-from [uri]", then the top
  // frame in the parent chain must be from that origin
  if (isAllowFrom) {
    if (aPolicy.Length() == allowFromLen ||
        (aPolicy[allowFromLen] != ' ' && aPolicy[allowFromLen] != '\t')) {
      ReportXFOViolation(curDocShellItem, uri, eALLOWFROM);
      return false;
    }
    rv = NS_NewURI(getter_AddRefs(uri), Substring(aPolicy, allowFromLen));
    if (NS_FAILED(rv)) {
      return false;
    }
    bool isPrivateWin =
        topDoc->NodePrincipal()->OriginAttributesRef().mPrivateBrowsingId > 0;
    rv = ssm->CheckSameOriginURI(uri, topUri, true, isPrivateWin);
    if (NS_FAILED(rv)) {
      ReportXFOViolation(curDocShellItem, uri, eALLOWFROM);
      return false;
    }
  }

  return true;
}

// Ignore x-frame-options if CSP with frame-ancestors exists
static bool ShouldIgnoreFrameOptions(nsIChannel* aChannel,
                                     nsIPrincipal* aPrincipal) {
  NS_ENSURE_TRUE(aChannel, false);
  NS_ENSURE_TRUE(aPrincipal, false);

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  aPrincipal->GetCsp(getter_AddRefs(csp));
  if (!csp) {
    // if there is no CSP, then there is nothing to do here
    return false;
  }

  bool enforcesFrameAncestors = false;
  csp->GetEnforcesFrameAncestors(&enforcesFrameAncestors);
  if (!enforcesFrameAncestors) {
    // if CSP does not contain frame-ancestors, then there
    // is nothing to do here.
    return false;
  }

  // log warning to console that xfo is ignored because of CSP
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  uint64_t innerWindowID = loadInfo->GetInnerWindowID();
  bool privateWindow = !!loadInfo->GetOriginAttributes().mPrivateBrowsingId;
  const char16_t* params[] = {u"x-frame-options", u"frame-ancestors"};
  CSP_LogLocalizedStr("IgnoringSrcBecauseOfDirective", params,
                      ArrayLength(params),
                      EmptyString(),  // no sourcefile
                      EmptyString(),  // no scriptsample
                      0,              // no linenumber
                      0,              // no columnnumber
                      nsIScriptError::warningFlag,
                      NS_LITERAL_CSTRING("IgnoringSrcBecauseOfDirective"),
                      innerWindowID, privateWindow);

  return true;
}

// Check if X-Frame-Options permits this document to be loaded as a subdocument.
// This will iterate through and check any number of X-Frame-Options policies
// in the request (comma-separated in a header, multiple headers, etc).
/* static */
bool FramingChecker::CheckFrameOptions(nsIChannel* aChannel,
                                       nsIDocShell* aDocShell,
                                       nsIPrincipal* aPrincipal) {
  if (!aChannel || !aDocShell) {
    return true;
  }

  if (ShouldIgnoreFrameOptions(aChannel, aPrincipal)) {
    return true;
  }

  nsresult rv;
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel) {
    // check if it is hiding in a multipart channel
    rv = nsDocShell::Cast(aDocShell)->GetHttpChannel(
        aChannel, getter_AddRefs(httpChannel));
    if (NS_FAILED(rv)) {
      return false;
    }
  }

  if (!httpChannel) {
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
    if (!CheckOneFrameOptionsPolicy(httpChannel, tok, aDocShell)) {
      // cancel the load and display about:blank
      httpChannel->Cancel(NS_BINDING_ABORTED);
      if (aDocShell) {
        nsCOMPtr<nsIWebNavigation> webNav(do_QueryObject(aDocShell));
        if (webNav) {
          nsCOMPtr<nsILoadInfo> loadInfo = httpChannel->LoadInfo();
          RefPtr<NullPrincipal> principal =
              NullPrincipal::CreateWithInheritedAttributes(
                  loadInfo->TriggeringPrincipal());

          LoadURIOptions loadURIOptions;
          loadURIOptions.mTriggeringPrincipal = principal;
          webNav->LoadURI(NS_LITERAL_STRING("about:blank"), loadURIOptions);
        }
      }
      return false;
    }
  }

  return true;
}

/* static */
void FramingChecker::ReportXFOViolation(nsIDocShellTreeItem* aTopDocShellItem,
                                        nsIURI* aThisURI, XFOHeader aHeader) {
  MOZ_ASSERT(aTopDocShellItem, "Need a top docshell");

  nsCOMPtr<nsPIDOMWindowOuter> topOuterWindow = aTopDocShellItem->GetWindow();
  if (!topOuterWindow) {
    return;
  }

  nsPIDOMWindowInner* topInnerWindow = topOuterWindow->GetCurrentInnerWindow();
  if (!topInnerWindow) {
    return;
  }

  nsCOMPtr<nsIURI> topURI;

  nsCOMPtr<Document> document = aTopDocShellItem->GetDocument();
  nsresult rv = document->NodePrincipal()->GetURI(getter_AddRefs(topURI));
  if (NS_FAILED(rv)) {
    return;
  }

  if (!topURI) {
    return;
  }

  nsCString topURIString;
  nsCString thisURIString;

  rv = topURI->GetSpec(topURIString);
  if (NS_FAILED(rv)) {
    return;
  }

  rv = aThisURI->GetSpec(thisURIString);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  nsCOMPtr<nsIScriptError> errorObject =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);

  if (!consoleService || !errorObject) {
    return;
  }

  nsString msg = NS_LITERAL_STRING("Load denied by X-Frame-Options: ");
  msg.Append(NS_ConvertUTF8toUTF16(thisURIString));

  switch (aHeader) {
    case eDENY:
      msg.AppendLiteral(" does not permit framing.");
      break;
    case eSAMEORIGIN:
      msg.AppendLiteral(" does not permit cross-origin framing.");
      break;
    case eALLOWFROM:
      msg.AppendLiteral(" does not permit framing by ");
      msg.Append(NS_ConvertUTF8toUTF16(topURIString));
      msg.Append('.');
      break;
  }

  // It is ok to use InitWithSanitizedSource, because the source string is
  // empty.
  rv = errorObject->InitWithSanitizedSource(
      msg, EmptyString(), EmptyString(), 0, 0, nsIScriptError::errorFlag,
      "X-Frame-Options", topInnerWindow->WindowID());
  if (NS_FAILED(rv)) {
    return;
  }

  consoleService->LogMessage(errorObject);
}
