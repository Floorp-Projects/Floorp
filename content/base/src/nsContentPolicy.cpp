/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim: ft=cpp tw=78 sw=4 et ts=8
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Zero-Knowledge Systems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prlog.h"

#include "nsISupports.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsContentPolicyUtils.h"
#include "nsContentPolicy.h"
#include "nsICategoryManager.h"
#include "nsIURI.h"
#include "nsIDOMNode.h"
#include "nsIDOMWindow.h"
#include "nsIContent.h"

NS_IMPL_ISUPPORTS1(nsContentPolicy, nsIContentPolicy)

#ifdef PR_LOGGING
static PRLogModuleInfo* gConPolLog;
#endif

nsresult
NS_NewContentPolicy(nsIContentPolicy **aResult)
{
  *aResult = new nsContentPolicy;
  if (!*aResult)
      return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

/*
 * This constructor does far too much.  I wish there was a way to get
 * an Init method called by the service manager after the factory
 * returned the new object, so that errors could be propagated back to
 * the caller correctly.
 */
nsContentPolicy::nsContentPolicy()
{
#ifdef PR_LOGGING
    if (! gConPolLog) {
        gConPolLog = PR_NewLogModule("nsContentPolicy");
    }
#endif
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman = 
             do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return; /* log an error? */

    /*
     * I'd like to use GetCategoryContents, so that I can size the array
     * correctly on the first go and avoid the enumerator overhead, but it's
     * not yet implemented (see nsCategoryManager.cpp).  No biggie, I guess.
     */
    nsCOMPtr<nsISimpleEnumerator> catEnum;
    rv = catman->EnumerateCategory(NS_CONTENTPOLICY_CATEGORY,
                                   getter_AddRefs(catEnum));
    if (NS_FAILED(rv))
        return; /* no category, no problem */

    PRBool hasMore;
    if (NS_FAILED(catEnum->HasMoreElements(&hasMore)) || !hasMore)
       return;
    
    /* 
     * Populate mPolicies with policy services named by contractids in the
     * "content-policy" category.
     */
    nsCOMPtr<nsISupports> item;
    while (NS_SUCCEEDED(catEnum->GetNext(getter_AddRefs(item)))) {
        nsCOMPtr<nsISupportsCString> string = do_QueryInterface(item, &rv);
        if (NS_FAILED(rv))
            continue;

        nsCAutoString contractid;
        if (NS_FAILED(string->GetData(contractid)))
            continue;

        PR_LOG(gConPolLog, PR_LOG_DEBUG,
                ("POLICY: loading %s\n", contractid.get()));

        /*
         * Create this policy service and add to mPolicies.
         *
         * Should we try to parse as a CID, in case the component prefers to be
         * registered that way?
         */
        nsCOMPtr<nsIContentPolicy> policy = do_GetService(contractid.get(),
                                                          &rv);
        if (NS_SUCCEEDED(rv) && policy) {
            mPolicies.AppendObject(policy);
        }
    }
	
}

nsContentPolicy::~nsContentPolicy()
{
}

#ifdef DEBUG
#define WARN_IF_URI_UNINITIALIZED(uri,name)                         \
  PR_BEGIN_MACRO                                                    \
    if ((uri)) {                                                    \
        nsCAutoString spec;                                         \
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
                             PRUint32          contentType,
                             nsIURI           *contentLocation,
                             nsIURI           *requestingLocation,
                             nsISupports      *requestingContext,
                             const nsACString &mimeType,
                             nsISupports      *extra,
                             PRInt16           *decision)
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
            doc = node->GetOwnerDoc();
        }
        if (!doc) {
            doc = do_QueryInterface(requestingContext);
        }
        if (doc) {
            requestingLocation = doc->GetDocumentURI();
        }
    }

    PRInt32 count = mPolicies.Count();
    nsresult rv = NS_OK;

    /* 
     * Enumerate mPolicies and ask each of them, taking the logical AND of
     * their permissions.
     */
    for (PRInt32 i = 0; i < count; i++) {
        nsIContentPolicy *policy = mPolicies[i];
        if (!policy) { //shouldn't happen
            NS_ERROR("Somehow a null policy got into the list");
            continue;
        }

        /* check the appropriate policy */
        rv = (policy->*policyMethod)(contentType, contentLocation,
                                     requestingLocation, requestingContext,
                                     mimeType, extra, decision);

        if (NS_SUCCEEDED(rv) && NS_CP_REJECTED(*decision)) {
            /* policy says no, no point continuing to check */
            return NS_OK;
        }
    }

    // everyone returned failure, or no policies: sanitize result
    *decision = nsIContentPolicy::ACCEPT;
    return NS_OK;
}

#ifdef PR_LOGGING

//uses the parameters from ShouldXYZ to produce and log a message
//logType must be a literal string constant
#define LOG_CHECK(logType)                                                    \
  PR_BEGIN_MACRO                                                              \
    /* skip all this nonsense if the call failed */                           \
    if (NS_SUCCEEDED(rv)) {                                                   \
      const char *resultName;                                                 \
      if (decision) {                                                         \
        resultName = NS_CP_ResponseName(*decision);                           \
      } else {                                                                \
        resultName = "(null ptr)";                                            \
      }                                                                       \
      nsCAutoString spec("None");                                             \
      if (contentLocation) {                                                  \
          contentLocation->GetSpec(spec);                                     \
      }                                                                       \
      nsCAutoString refSpec("None");                                          \
      if (requestingLocation) {                                               \
          requestingLocation->GetSpec(refSpec);                               \
      }                                                                       \
      PR_LOG(gConPolLog, PR_LOG_DEBUG,                                        \
             ("Content Policy: " logType ": <%s> <Ref:%s> result=%s",         \
              spec.get(), refSpec.get(), resultName)                          \
             );                                                               \
    }                                                                         \
  PR_END_MACRO

#else //!defined(PR_LOGGING)

#define LOG_CHECK(logType)

#endif //!defined(PR_LOGGING)

NS_IMETHODIMP
nsContentPolicy::ShouldLoad(PRUint32          contentType,
                            nsIURI           *contentLocation,
                            nsIURI           *requestingLocation,
                            nsISupports      *requestingContext,
                            const nsACString &mimeType,
                            nsISupports      *extra,
                            PRInt16          *decision)
{
    // ShouldProcess does not need a content location, but we do
    NS_PRECONDITION(contentLocation, "Must provide request location");
    nsresult rv = CheckPolicy(&nsIContentPolicy::ShouldLoad, contentType,
                              contentLocation, requestingLocation,
                              requestingContext, mimeType, extra, decision);
    LOG_CHECK("ShouldLoad");

    return rv;
}

NS_IMETHODIMP
nsContentPolicy::ShouldProcess(PRUint32          contentType,
                               nsIURI           *contentLocation,
                               nsIURI           *requestingLocation,
                               nsISupports      *requestingContext,
                               const nsACString &mimeType,
                               nsISupports      *extra,
                               PRInt16          *decision)
{
    nsresult rv = CheckPolicy(&nsIContentPolicy::ShouldProcess, contentType,
                              contentLocation, requestingLocation,
                              requestingContext, mimeType, extra, decision);
    LOG_CHECK("ShouldProcess");

    return rv;
}
