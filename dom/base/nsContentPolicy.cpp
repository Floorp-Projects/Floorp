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
#include "nsIDOMNode.h"
#include "nsIDOMWindow.h"
#include "nsITabChild.h"
#include "nsIContent.h"
#include "nsIImageLoadingContent.h"
#include "nsILoadContext.h"
#include "nsCOMArray.h"
#include "nsContentUtils.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "nsIContentSecurityPolicy.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/TaskCategory.h"

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
                             nsIURI           *contentLocation,
                             nsILoadInfo      *loadInfo,
                             const nsACString &mimeType,
                             int16_t           *decision)
{
    nsContentPolicyType contentType = loadInfo->InternalContentPolicyType();
    nsCOMPtr<nsISupports> requestingContext = loadInfo->GetLoadingContext();
    nsCOMPtr<nsIPrincipal> requestPrincipal = loadInfo->TriggeringPrincipal();
    nsCOMPtr<nsIURI> requestingLocation;
    nsCOMPtr<nsIPrincipal> loadingPrincipal = loadInfo->LoadingPrincipal();
    if (loadingPrincipal) {
      loadingPrincipal->GetURI(getter_AddRefs(requestingLocation));
    }

    //sanity-check passed-through parameters
    NS_PRECONDITION(decision, "Null out pointer");
    WARN_IF_URI_UNINITIALIZED(contentLocation, "Request URI");
    WARN_IF_URI_UNINITIALIZED(requestingLocation, "Requesting URI");

#ifdef DEBUG
    {
        nsCOMPtr<nsIDOMNode> node(do_QueryInterface(requestingContext));
        nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(requestingContext));
        nsCOMPtr<nsITabChild> tabChild(do_QueryInterface(requestingContext));
        NS_ASSERTION(!requestingContext || node || window || tabChild,
                     "Context should be a DOM node, DOM window or a tabChild!");
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

    /*
     * Enumerate mPolicies and ask each of them, taking the logical AND of
     * their permissions.
     */
    nsresult rv;
    const nsCOMArray<nsIContentPolicy>& entries = mPolicies.GetCachedEntries();

    nsCOMPtr<nsPIDOMWindowOuter> window;
    if (nsCOMPtr<nsINode> node = do_QueryInterface(requestingContext)) {
        window = node->OwnerDoc()->GetWindow();
    } else {
        window = do_QueryInterface(requestingContext);
    }

    if (requestPrincipal) {
        nsCOMPtr<nsIContentSecurityPolicy> csp;
        requestPrincipal->GetCsp(getter_AddRefs(csp));
        if (csp && window) {
            csp->EnsureEventTarget(window->EventTargetFor(mozilla::TaskCategory::Other));
        }
    }

    int32_t count = entries.Count();
    for (int32_t i = 0; i < count; i++) {
        /* check the appropriate policy */
        rv = (entries[i]->*policyMethod)(contentLocation, loadInfo,
                                         mimeType, decision);

        if (NS_SUCCEEDED(rv) && NS_CP_REJECTED(*decision)) {
            // If we are blocking an image, we have to let the
            // ImageLoadingContent know that we blocked the load.
            if (externalType == nsIContentPolicy::TYPE_IMAGE ||
                externalType == nsIContentPolicy::TYPE_IMAGESET) {
              nsCOMPtr<nsIImageLoadingContent> img =
                do_QueryInterface(requestingContext);
              if (img) {
                img->SetBlockedRequest(*decision);
              }
            }
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
#define LOG_CHECK(logType)                                                     \
  PR_BEGIN_MACRO                                                               \
    nsCOMPtr<nsIURI> requestingLocation;                                       \
    nsCOMPtr<nsIPrincipal> loadingPrincipal = loadInfo->LoadingPrincipal();    \
    if (loadingPrincipal) {                                                    \
      loadingPrincipal->GetURI(getter_AddRefs(requestingLocation));            \
    }                                                                          \
    /* skip all this nonsense if the call failed or logging is disabled */     \
    if (NS_SUCCEEDED(rv) && MOZ_LOG_TEST(gConPolLog, LogLevel::Debug)) {       \
      const char *resultName;                                                  \
      if (decision) {                                                          \
        resultName = NS_CP_ResponseName(*decision);                            \
      } else {                                                                 \
        resultName = "(null ptr)";                                             \
      }                                                                        \
      MOZ_LOG(gConPolLog, LogLevel::Debug,                                     \
             ("Content Policy: " logType ": <%s> <Ref:%s> result=%s",          \
              contentLocation ? contentLocation->GetSpecOrDefault().get()      \
                              : "None",                                        \
              requestingLocation ? requestingLocation->GetSpecOrDefault().get()\
                                 : "None",                                     \
              resultName)                                                      \
             );                                                                \
    }                                                                          \
  PR_END_MACRO

NS_IMETHODIMP
nsContentPolicy::ShouldLoad(nsIURI           *contentLocation,
                            nsILoadInfo      *loadInfo,
                            const nsACString &mimeType,
                            int16_t          *decision)
{
    // ShouldProcess does not need a content location, but we do
    NS_PRECONDITION(contentLocation, "Must provide request location");
    nsresult rv = CheckPolicy(&nsIContentPolicy::ShouldLoad,
                              contentLocation, loadInfo,
                              mimeType, decision);
    LOG_CHECK("ShouldLoad");

    return rv;
}

NS_IMETHODIMP
nsContentPolicy::ShouldProcess(nsIURI           *contentLocation,
                               nsILoadInfo      *loadInfo,
                               const nsACString &mimeType,
                               int16_t          *decision)
{
    nsresult rv = CheckPolicy(&nsIContentPolicy::ShouldProcess,
                              contentLocation, loadInfo,
                              mimeType, decision);
    LOG_CHECK("ShouldProcess");

    return rv;
}
