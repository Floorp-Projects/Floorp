/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim: ft=cpp tw=78 sw=4 et ts=8
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the "@mozilla.org/layout/content-policy;1" contract.
 */

#include "mozilla/Logging.h"

#include "nsISupports.h"
#include "nsXPCOM.h"
#include "nsContentPolicyUtils.h"
#include "mozilla/dom/nsCSPService.h"
#include "nsContentPolicy.h"
#include "nsIURI.h"
#include "nsIDocShell.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMWindow.h"
#include "nsIContent.h"
#include "nsILoadContext.h"
#include "nsCOMArray.h"
#include "nsContentUtils.h"
#include "mozilla/dom/nsMixedContentBlocker.h"

using mozilla::LogLevel;

NS_IMPL_ISUPPORTS(nsContentPolicy, nsIContentPolicy)

static mozilla::LazyLogModule gConPolLog("nsContentPolicy");

nsresult
NS_NewContentPolicy(nsIContentPolicy **aResult)
{
  *aResult = new nsContentPolicy;
  NS_ADDREF(*aResult);
  return NS_OK;
}

nsContentPolicy::nsContentPolicy()
    : mPolicies(NS_CONTENTPOLICY_CATEGORY)
    , mSimplePolicies(NS_SIMPLECONTENTPOLICY_CATEGORY)
{
}

nsContentPolicy::~nsContentPolicy()
{
}

#ifdef DEBUG
#define WARN_IF_URI_UNINITIALIZED(uri,name)                         \
  PR_BEGIN_MACRO                                                    \
    if ((uri)) {                                                    \
        nsAutoCString spec;                                         \
        (uri)->GetAsciiSpec(spec);                                  \
        if (spec.IsEmpty()) {                                       \
            NS_WARNING(name " is uninitialized, fix caller");       \
        }                                                           \
    }                                                               \
  PR_END_MACRO

#else  // ! defined(DEBUG)

#define WARN_IF_URI_UNINITIALIZED(uri,name)

#endif // defined(DEBUG)

inline nsresult
nsContentPolicy::CheckPolicy(CPMethod          policyMethod,
                             SCPMethod         simplePolicyMethod,
                             nsContentPolicyType contentType,
                             nsIURI           *contentLocation,
                             nsIURI           *requestingLocation,
                             nsISupports      *requestingContext,
                             const nsACString &mimeType,
                             nsISupports      *extra,
                             nsIPrincipal     *requestPrincipal,
                             int16_t           *decision)
{
    //sanity-check passed-through parameters
    NS_PRECONDITION(decision, "Null out pointer");
    WARN_IF_URI_UNINITIALIZED(contentLocation, "Request URI");
    WARN_IF_URI_UNINITIALIZED(requestingLocation, "Requesting URI");

#ifdef DEBUG
    {
        nsCOMPtr<nsIDOMNode> node(do_QueryInterface(requestingContext));
        nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(requestingContext));
        NS_ASSERTION(!requestingContext || node || window,
                     "Context should be a DOM node or a DOM window!");
    }
#endif

    /*
     * There might not be a requestinglocation. This can happen for
     * iframes with an image as src. Get the uri from the dom node.
     * See bug 254510
     */
    if (!requestingLocation) {
        nsCOMPtr<nsIDocument> doc;
        nsCOMPtr<nsIContent> node = do_QueryInterface(requestingContext);
        if (node) {
            doc = node->OwnerDoc();
        }
        if (!doc) {
            doc = do_QueryInterface(requestingContext);
        }
        if (doc) {
            requestingLocation = doc->GetDocumentURI();
        }
    }

    nsContentPolicyType externalType =
        nsContentUtils::InternalContentPolicyTypeToExternal(contentType);

    nsContentPolicyType externalTypeOrMCBInternal =
        nsContentUtils::InternalContentPolicyTypeToExternalOrMCBInternal(contentType);

    nsContentPolicyType externalTypeOrCSPInternal =
       nsContentUtils::InternalContentPolicyTypeToExternalOrCSPInternal(contentType);

    nsCOMPtr<nsIContentPolicy> mixedContentBlocker =
        do_GetService(NS_MIXEDCONTENTBLOCKER_CONTRACTID);

    nsCOMPtr<nsIContentPolicy> cspService =
      do_GetService(CSPSERVICE_CONTRACTID);

    /* 
     * Enumerate mPolicies and ask each of them, taking the logical AND of
     * their permissions.
     */
    nsresult rv;
    nsCOMArray<nsIContentPolicy> entries;
    mPolicies.GetEntries(entries);
    int32_t count = entries.Count();
    for (int32_t i = 0; i < count; i++) {
        /* check the appropriate policy */
        // Send the internal content policy type to the mixed content blocker
        // which needs to know about TYPE_INTERNAL_WORKER,
        // TYPE_INTERNAL_SHARED_WORKER and TYPE_INTERNAL_SERVICE_WORKER
        // and also preloads: TYPE_INTERNAL_SCRIPT_PRELOAD,
        // TYPE_INTERNAL_IMAGE_PRELOAD, TYPE_INTERNAL_STYLESHEET_PRELOAD
        bool isMixedContentBlocker = mixedContentBlocker == entries[i];
        nsContentPolicyType type = externalType;
        if (isMixedContentBlocker) {
            type = externalTypeOrMCBInternal;
        }
        // Send the internal content policy type for CSP which needs to
        // know about preloads and workers, in particular:
        // * TYPE_INTERNAL_SCRIPT_PRELOAD
        // * TYPE_INTERNAL_IMAGE_PRELOAD
        // * TYPE_INTERNAL_STYLESHEET_PRELOAD
        // * TYPE_INTERNAL_WORKER
        // * TYPE_INTERNAL_SHARED_WORKER
        // * TYPE_INTERNAL_SERVICE_WORKER
        bool isCSP = cspService == entries[i];
        if (isCSP) {
          type = externalTypeOrCSPInternal;
        }
        rv = (entries[i]->*policyMethod)(type, contentLocation,
                                         requestingLocation, requestingContext,
                                         mimeType, extra, requestPrincipal,
                                         decision);

        if (NS_SUCCEEDED(rv) && NS_CP_REJECTED(*decision)) {
            /* policy says no, no point continuing to check */
            return NS_OK;
        }
    }

    nsCOMPtr<nsIDOMElement> topFrameElement;
    bool isTopLevel = true;
    nsCOMPtr<nsPIDOMWindow> window;
    if (nsCOMPtr<nsINode> node = do_QueryInterface(requestingContext)) {
        window = node->OwnerDoc()->GetWindow();
    } else {
        window = do_QueryInterface(requestingContext);
    }

    if (window) {
        nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
        nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);
        loadContext->GetTopFrameElement(getter_AddRefs(topFrameElement));

        MOZ_ASSERT(window->IsOuterWindow());

        if (topFrameElement) {
            nsCOMPtr<nsPIDOMWindow> topWindow = window->GetScriptableTop();
            isTopLevel = topWindow == window;
        } else {
            // If we don't have a top frame element, then requestingContext is
            // part of the top-level XUL document. Presumably it's the <browser>
            // element that content is being loaded into, so we call it the
            // topFrameElement.
            topFrameElement = do_QueryInterface(requestingContext);
            isTopLevel = true;
        }
    }

    nsCOMArray<nsISimpleContentPolicy> simpleEntries;
    mSimplePolicies.GetEntries(simpleEntries);
    count = simpleEntries.Count();
    for (int32_t i = 0; i < count; i++) {
        /* check the appropriate policy */
        rv = (simpleEntries[i]->*simplePolicyMethod)(externalType, contentLocation,
                                                     requestingLocation,
                                                     topFrameElement, isTopLevel,
                                                     mimeType, extra, requestPrincipal,
                                                     decision);

        if (NS_SUCCEEDED(rv) && NS_CP_REJECTED(*decision)) {
            /* policy says no, no point continuing to check */
            return NS_OK;
        }
    }

    // everyone returned failure, or no policies: sanitize result
    *decision = nsIContentPolicy::ACCEPT;
    return NS_OK;
}

//uses the parameters from ShouldXYZ to produce and log a message
//logType must be a literal string constant
#define LOG_CHECK(logType)                                                    \
  PR_BEGIN_MACRO                                                              \
    /* skip all this nonsense if the call failed or logging is disabled */    \
    if (NS_SUCCEEDED(rv) && MOZ_LOG_TEST(gConPolLog, LogLevel::Debug)) {          \
      const char *resultName;                                                 \
      if (decision) {                                                         \
        resultName = NS_CP_ResponseName(*decision);                           \
      } else {                                                                \
        resultName = "(null ptr)";                                            \
      }                                                                       \
      nsAutoCString spec("None");                                             \
      if (contentLocation) {                                                  \
          contentLocation->GetSpec(spec);                                     \
      }                                                                       \
      nsAutoCString refSpec("None");                                          \
      if (requestingLocation) {                                               \
          requestingLocation->GetSpec(refSpec);                               \
      }                                                                       \
      MOZ_LOG(gConPolLog, LogLevel::Debug,                                        \
             ("Content Policy: " logType ": <%s> <Ref:%s> result=%s",         \
              spec.get(), refSpec.get(), resultName)                          \
             );                                                               \
    }                                                                         \
  PR_END_MACRO

NS_IMETHODIMP
nsContentPolicy::ShouldLoad(uint32_t          contentType,
                            nsIURI           *contentLocation,
                            nsIURI           *requestingLocation,
                            nsISupports      *requestingContext,
                            const nsACString &mimeType,
                            nsISupports      *extra,
                            nsIPrincipal     *requestPrincipal,
                            int16_t          *decision)
{
    // ShouldProcess does not need a content location, but we do
    NS_PRECONDITION(contentLocation, "Must provide request location");
    nsresult rv = CheckPolicy(&nsIContentPolicy::ShouldLoad,
                              &nsISimpleContentPolicy::ShouldLoad,
                              contentType,
                              contentLocation, requestingLocation,
                              requestingContext, mimeType, extra,
                              requestPrincipal, decision);
    LOG_CHECK("ShouldLoad");

    return rv;
}

NS_IMETHODIMP
nsContentPolicy::ShouldProcess(uint32_t          contentType,
                               nsIURI           *contentLocation,
                               nsIURI           *requestingLocation,
                               nsISupports      *requestingContext,
                               const nsACString &mimeType,
                               nsISupports      *extra,
                               nsIPrincipal     *requestPrincipal,
                               int16_t          *decision)
{
    nsresult rv = CheckPolicy(&nsIContentPolicy::ShouldProcess,
                              &nsISimpleContentPolicy::ShouldProcess,
                              contentType,
                              contentLocation, requestingLocation,
                              requestingContext, mimeType, extra,
                              requestPrincipal, decision);
    LOG_CHECK("ShouldProcess");

    return rv;
}
