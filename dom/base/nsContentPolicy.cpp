/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=80 sw=2 et ts=8
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
#include "nsIBrowserChild.h"
#include "nsIContent.h"
#include "nsIImageLoadingContent.h"
#include "nsCOMArray.h"
#include "nsContentUtils.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "nsIContentSecurityPolicy.h"

class nsIDOMWindow;

using mozilla::LogLevel;

NS_IMPL_ISUPPORTS(nsContentPolicy, nsIContentPolicy)

static mozilla::LazyLogModule gConPolLog("nsContentPolicy");

nsresult NS_NewContentPolicy(nsIContentPolicy** aResult) {
  *aResult = new nsContentPolicy;
  NS_ADDREF(*aResult);
  return NS_OK;
}

nsContentPolicy::nsContentPolicy() : mPolicies(NS_CONTENTPOLICY_CATEGORY) {}

nsContentPolicy::~nsContentPolicy() = default;

#ifdef DEBUG
#  define WARN_IF_URI_UNINITIALIZED(uri, name)            \
    PR_BEGIN_MACRO                                        \
    if ((uri)) {                                          \
      nsAutoCString spec;                                 \
      (uri)->GetAsciiSpec(spec);                          \
      if (spec.IsEmpty()) {                               \
        NS_WARNING(name " is uninitialized, fix caller"); \
      }                                                   \
    }                                                     \
    PR_END_MACRO

#else  // ! defined(DEBUG)

#  define WARN_IF_URI_UNINITIALIZED(uri, name)

#endif  // defined(DEBUG)

inline nsresult nsContentPolicy::CheckPolicy(CPMethod policyMethod,
                                             nsIURI* contentLocation,
                                             nsILoadInfo* loadInfo,
                                             int16_t* decision) {
  nsCOMPtr<nsISupports> requestingContext = loadInfo->GetLoadingContext();
  // sanity-check passed-through parameters
  MOZ_ASSERT(decision, "Null out pointer");
  WARN_IF_URI_UNINITIALIZED(contentLocation, "Request URI");

#ifdef DEBUG
  {
    nsCOMPtr<nsINode> node(do_QueryInterface(requestingContext));
    nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(requestingContext));
    nsCOMPtr<nsIBrowserChild> browserChild(
        do_QueryInterface(requestingContext));
    NS_ASSERTION(!requestingContext || node || window || browserChild,
                 "Context should be a DOM node, DOM window or a browserChild!");
  }
#endif

  nsCOMPtr<mozilla::dom::Document> doc;
  nsCOMPtr<nsIContent> node = do_QueryInterface(requestingContext);
  if (node) {
    doc = node->OwnerDoc();
  }
  if (!doc) {
    doc = do_QueryInterface(requestingContext);
  }

  /*
   * Enumerate mPolicies and ask each of them, taking the logical AND of
   * their permissions.
   */
  nsresult rv;
  const nsCOMArray<nsIContentPolicy>& entries = mPolicies.GetCachedEntries();
  if (doc) {
    if (nsCOMPtr<nsIContentSecurityPolicy> csp = doc->GetCsp()) {
      csp->EnsureEventTarget(mozilla::GetMainThreadSerialEventTarget());
    }
  }

  int32_t count = entries.Count();
  for (int32_t i = 0; i < count; i++) {
    /* check the appropriate policy */
    rv = (entries[i]->*policyMethod)(contentLocation, loadInfo, decision);

    if (NS_SUCCEEDED(rv) && NS_CP_REJECTED(*decision)) {
      /* policy says no, no point continuing to check */
      return NS_OK;
    }
  }

  // everyone returned failure, or no policies: sanitize result
  *decision = nsIContentPolicy::ACCEPT;
  return NS_OK;
}

// uses the parameters from ShouldXYZ to produce and log a message
// logType must be a literal string constant
#define LOG_CHECK(logType)                                                     \
  PR_BEGIN_MACRO                                                               \
  /* skip all this nonsense if the call failed or logging is disabled */       \
  if (NS_SUCCEEDED(rv) && MOZ_LOG_TEST(gConPolLog, LogLevel::Debug)) {         \
    const char* resultName;                                                    \
    if (decision) {                                                            \
      resultName = NS_CP_ResponseName(*decision);                              \
    } else {                                                                   \
      resultName = "(null ptr)";                                               \
    }                                                                          \
    MOZ_LOG(                                                                   \
        gConPolLog, LogLevel::Debug,                                           \
        ("Content Policy: " logType ": <%s> result=%s",                        \
         contentLocation ? contentLocation->GetSpecOrDefault().get() : "None", \
         resultName));                                                         \
  }                                                                            \
  PR_END_MACRO

NS_IMETHODIMP
nsContentPolicy::ShouldLoad(nsIURI* contentLocation, nsILoadInfo* loadInfo,
                            int16_t* decision) {
  // ShouldProcess does not need a content location, but we do
  MOZ_ASSERT(contentLocation, "Must provide request location");
  nsresult rv = CheckPolicy(&nsIContentPolicy::ShouldLoad, contentLocation,
                            loadInfo, decision);
  LOG_CHECK("ShouldLoad");

  return rv;
}

NS_IMETHODIMP
nsContentPolicy::ShouldProcess(nsIURI* contentLocation, nsILoadInfo* loadInfo,
                               int16_t* decision) {
  nsresult rv = CheckPolicy(&nsIContentPolicy::ShouldProcess, contentLocation,
                            loadInfo, decision);
  LOG_CHECK("ShouldProcess");

  return rv;
}
