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
#include "nsIStringBundle.h"

using namespace mozilla;

void FramingChecker::ReportError(const char* aMessageTag,
                                 nsIDocShellTreeItem* aParentDocShellItem,
                                 nsIURI* aChildURI, const nsAString& aPolicy) {
  MOZ_ASSERT(aParentDocShellItem, "Need a parent docshell");
  if (!aChildURI || !aParentDocShellItem) {
    return;
  }

  Document* parentDocument = aParentDocShellItem->GetDocument();
  nsCOMPtr<nsIURI> parentURI;
  parentDocument->NodePrincipal()->GetURI(getter_AddRefs(parentURI));
  MOZ_ASSERT(!parentDocument->NodePrincipal()->IsSystemPrincipal(),
             "Should not get system principal here.");

  // Get the parent URL spec
  nsAutoCString parentSpec;
  nsresult rv;
  rv = parentURI->GetAsciiSpec(parentSpec);
  if (NS_FAILED(rv)) {
    return;
  }

  // Get the child URL spec
  nsAutoCString childSpec;
  rv = aChildURI->GetAsciiSpec(childSpec);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIStringBundleService> bundleService =
      mozilla::services::GetStringBundleService();
  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle(
      "chrome://global/locale/security/security.properties",
      getter_AddRefs(bundle));
  if (NS_FAILED(rv)) {
    return;
  }

  if (NS_WARN_IF(!bundle)) {
    return;
  }

  nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  if (!console || !error) {
    return;
  }

  // Localize the error message
  nsAutoString message;
  AutoTArray<nsString, 3> formatStrings;
  formatStrings.AppendElement(aPolicy);
  CopyASCIItoUTF16(childSpec, *formatStrings.AppendElement());
  CopyASCIItoUTF16(parentSpec, *formatStrings.AppendElement());
  rv = bundle->FormatStringFromName(aMessageTag, formatStrings, message);
  if (NS_FAILED(rv)) {
    return;
  }

  rv = error->InitWithWindowID(message, EmptyString(), EmptyString(), 0, 0,
                               nsIScriptError::errorFlag, "X-Frame-Options",
                               parentDocument->InnerWindowID());
  if (NS_FAILED(rv)) {
    return;
  }
  console->LogMessage(error);
}

/* static */
bool FramingChecker::CheckOneFrameOptionsPolicy(nsIHttpChannel* aHttpChannel,
                                                const nsAString& aPolicy,
                                                nsIDocShell* aDocShell) {
  nsresult rv;
  // Find the top docshell in our parent chain that doesn't have the system
  // principal and use it for the principal comparison.  Finding the top
  // content-type docshell doesn't work because some chrome documents are
  // loaded in content docshells (see bug 593387).
  nsCOMPtr<nsIDocShellTreeItem> thisDocShellItem(aDocShell);
  nsCOMPtr<nsIDocShellTreeItem> parentDocShellItem;
  nsCOMPtr<nsIDocShellTreeItem> curDocShellItem = thisDocShellItem;
  nsCOMPtr<Document> topDoc;
  nsCOMPtr<nsIScriptSecurityManager> ssm =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);

  if (!ssm) {
    MOZ_CRASH();
  }

  nsCOMPtr<nsIURI> uri;
  aHttpChannel->GetURI(getter_AddRefs(uri));

  // return early if header does not have one of the values with meaning
  if (!aPolicy.LowerCaseEqualsLiteral("deny") &&
      !aPolicy.LowerCaseEqualsLiteral("sameorigin")) {
    nsCOMPtr<nsIDocShellTreeItem> root;
    curDocShellItem->GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
    ReportError("XFOInvalid", root, uri, aPolicy);
    return true;
  }

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

  // GetInProcessScriptableTop, not GetTop, because we want this to respect
  // <iframe mozbrowser> boundaries.
  nsCOMPtr<nsPIDOMWindowOuter> topWindow =
      thisWindow->GetInProcessScriptableTop();

  // if the document is in the top window, it's not in a frame.
  if (thisWindow == topWindow) {
    return true;
  }

  // If the X-Frame-Options value is SAMEORIGIN, then the top frame in the
  // parent chain must be from the same origin as this document.
  bool checkSameOrigin = aPolicy.LowerCaseEqualsLiteral("sameorigin");
  nsCOMPtr<nsIURI> topUri;

  // Traverse up the parent chain and stop when we see a docshell whose
  // parent has a system principal, or a docshell corresponding to
  // <iframe mozbrowser>.
  while (NS_SUCCEEDED(curDocShellItem->GetInProcessParent(
             getter_AddRefs(parentDocShellItem))) &&
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
          ReportError("XFOSameOrigin", curDocShellItem, uri, aPolicy);
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
    ReportError("XFODeny", curDocShellItem, uri, aPolicy);
    return false;
  }

  return true;
}

// Ignore x-frame-options if CSP with frame-ancestors exists
static bool ShouldIgnoreFrameOptions(nsIChannel* aChannel,
                                     nsIContentSecurityPolicy* aCSP) {
  NS_ENSURE_TRUE(aChannel, false);
  NS_ENSURE_TRUE(aCSP, false);

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

// Check if X-Frame-Options permits this document to be loaded as a subdocument.
// This will iterate through and check any number of X-Frame-Options policies
// in the request (comma-separated in a header, multiple headers, etc).
/* static */
bool FramingChecker::CheckFrameOptions(nsIChannel* aChannel,
                                       nsIDocShell* aDocShell,
                                       nsIContentSecurityPolicy* aCsp) {
  if (!aChannel || !aDocShell) {
    return true;
  }

  if (ShouldIgnoreFrameOptions(aChannel, aCsp)) {
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
