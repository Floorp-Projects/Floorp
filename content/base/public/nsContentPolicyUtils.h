/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Utility routines for checking content load/process policy settings,
 * and routines helpful for content policy implementors.
 *
 * XXXbz it would be nice if some of this stuff could be out-of-lined in
 * nsContentUtils.  That would work for almost all the callers...
 */

#ifndef __nsContentPolicyUtils_h__
#define __nsContentPolicyUtils_h__

#include "nsIContentPolicy.h"
#include "nsIServiceManager.h"
#include "nsIContent.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"

//XXXtw sadly, this makes consumers of nsContentPolicyUtils depend on widget
#include "nsIDocument.h"
#include "nsPIDOMWindow.h"

class nsACString;

#define NS_CONTENTPOLICY_CONTRACTID   "@mozilla.org/layout/content-policy;1"
#define NS_CONTENTPOLICY_CATEGORY "content-policy"
#define NS_CONTENTPOLICY_CID                              \
  {0x0e3afd3d, 0xeb60, 0x4c2b,                            \
     { 0x96, 0x3b, 0x56, 0xd7, 0xc4, 0x39, 0xf1, 0x24 }}

/**
 * Evaluates to true if val is ACCEPT.
 *
 * @param val the status returned from shouldProcess/shouldLoad
 */
#define NS_CP_ACCEPTED(val) ((val) == nsIContentPolicy::ACCEPT)

/**
 * Evaluates to true if val is a REJECT_* status
 *
 * @param val the status returned from shouldProcess/shouldLoad
 */
#define NS_CP_REJECTED(val) ((val) != nsIContentPolicy::ACCEPT)

// Offer convenient translations of constants -> const char*

// convenience macro to reduce some repetative typing...
// name is the name of a constant from this interface
#define CASE_RETURN(name)          \
  case nsIContentPolicy:: name :   \
    return #name

#ifdef PR_LOGGING
/**
 * Returns a string corresponding to the name of the response constant, or
 * "<Unknown Response>" if an unknown response value is given.
 *
 * The return value is static and must not be freed.
 *
 * @param response the response code
 * @return the name of the given response code
 */
inline const char *
NS_CP_ResponseName(PRInt16 response)
{
  switch (response) {
    CASE_RETURN( REJECT_REQUEST );
    CASE_RETURN( REJECT_TYPE    );
    CASE_RETURN( REJECT_SERVER  );
    CASE_RETURN( REJECT_OTHER   );
    CASE_RETURN( ACCEPT         );
  default:
    return "<Unknown Response>";
  }
}

/**
 * Returns a string corresponding to the name of the content type constant, or
 * "<Unknown Type>" if an unknown content type value is given.
 *
 * The return value is static and must not be freed.
 *
 * @param contentType the content type code
 * @return the name of the given content type code
 */
inline const char *
NS_CP_ContentTypeName(PRUint32 contentType)
{
  switch (contentType) {
    CASE_RETURN( TYPE_OTHER             );
    CASE_RETURN( TYPE_SCRIPT            );
    CASE_RETURN( TYPE_IMAGE             );
    CASE_RETURN( TYPE_STYLESHEET        );
    CASE_RETURN( TYPE_OBJECT            );
    CASE_RETURN( TYPE_DOCUMENT          );
    CASE_RETURN( TYPE_SUBDOCUMENT       );
    CASE_RETURN( TYPE_REFRESH           );
    CASE_RETURN( TYPE_XBL               );
    CASE_RETURN( TYPE_PING              );
    CASE_RETURN( TYPE_XMLHTTPREQUEST    );
    CASE_RETURN( TYPE_OBJECT_SUBREQUEST );
    CASE_RETURN( TYPE_DTD               );
    CASE_RETURN( TYPE_FONT              );
    CASE_RETURN( TYPE_MEDIA             );
    CASE_RETURN( TYPE_WEBSOCKET         );
   default:
    return "<Unknown Type>";
  }
}

#endif // defined(PR_LOGGING)

#undef CASE_RETURN

/* Passes on parameters from its "caller"'s context. */
#define CHECK_CONTENT_POLICY(action)                                          \
  PR_BEGIN_MACRO                                                              \
    nsCOMPtr<nsIContentPolicy> policy =                                       \
         do_GetService(NS_CONTENTPOLICY_CONTRACTID);                          \
    if (!policy)                                                              \
        return NS_ERROR_FAILURE;                                              \
                                                                              \
    return policy-> action (contentType, contentLocation, requestOrigin,      \
                            context, mimeType, extra, decision);              \
  PR_END_MACRO

/* Passes on parameters from its "caller"'s context. */
#define CHECK_CONTENT_POLICY_WITH_SERVICE(action, _policy)                    \
  PR_BEGIN_MACRO                                                              \
    return _policy-> action (contentType, contentLocation, requestOrigin,     \
                             context, mimeType, extra, decision);             \
  PR_END_MACRO

/**
 * Check whether we can short-circuit this check and bail out.  If not, get the
 * origin URI to use.
 *
 * Note: requestOrigin is scoped outside the PR_BEGIN_MACRO/PR_END_MACRO on
 * purpose */
#define CHECK_PRINCIPAL                                                       \
  nsCOMPtr<nsIURI> requestOrigin;                                             \
  PR_BEGIN_MACRO                                                              \
  if (originPrincipal) {                                                      \
      nsCOMPtr<nsIScriptSecurityManager> secMan = aSecMan;                    \
      if (!secMan) {                                                          \
          secMan = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);        \
      }                                                                       \
      if (secMan) {                                                           \
          bool isSystem;                                                    \
          nsresult rv = secMan->IsSystemPrincipal(originPrincipal,            \
                                                  &isSystem);                 \
          NS_ENSURE_SUCCESS(rv, rv);                                          \
          if (isSystem) {                                                     \
              *decision = nsIContentPolicy::ACCEPT;                           \
              return NS_OK;                                                   \
          }                                                                   \
      }                                                                       \
      nsresult rv = originPrincipal->GetURI(getter_AddRefs(requestOrigin));   \
      NS_ENSURE_SUCCESS(rv, rv);                                              \
  }                                                                           \
  PR_END_MACRO

/**
 * Alias for calling ShouldLoad on the content policy service.  Parameters are
 * the same as nsIContentPolicy::shouldLoad, except for the originPrincipal
 * parameter, which should be non-null if possible, and the last two
 * parameters, which can be used to pass in pointer to some useful services if
 * the caller already has them.  The origin URI to pass to shouldLoad will be
 * the URI of originPrincipal, unless originPrincipal is null (in which case a
 * null origin URI will be passed).
 */
inline nsresult
NS_CheckContentLoadPolicy(PRUint32          contentType,
                          nsIURI           *contentLocation,
                          nsIPrincipal     *originPrincipal,
                          nsISupports      *context,
                          const nsACString &mimeType,
                          nsISupports      *extra,
                          PRInt16          *decision,
                          nsIContentPolicy *policyService = nsnull,
                          nsIScriptSecurityManager* aSecMan = nsnull)
{
    CHECK_PRINCIPAL;
    if (policyService) {
        CHECK_CONTENT_POLICY_WITH_SERVICE(ShouldLoad, policyService);
    }
    CHECK_CONTENT_POLICY(ShouldLoad);
}

/**
 * Alias for calling ShouldProcess on the content policy service.  Parameters
 * are the same as nsIContentPolicy::shouldLoad, except for the originPrincipal
 * parameter, which should be non-null if possible, and the last two
 * parameters, which can be used to pass in pointer to some useful services if
 * the caller already has them.  The origin URI to pass to shouldLoad will be
 * the URI of originPrincipal, unless originPrincipal is null (in which case a
 * null origin URI will be passed).
 */
inline nsresult
NS_CheckContentProcessPolicy(PRUint32          contentType,
                             nsIURI           *contentLocation,
                             nsIPrincipal     *originPrincipal,
                             nsISupports      *context,
                             const nsACString &mimeType,
                             nsISupports      *extra,
                             PRInt16          *decision,
                             nsIContentPolicy *policyService = nsnull,
                             nsIScriptSecurityManager* aSecMan = nsnull)
{
    CHECK_PRINCIPAL;
    if (policyService) {
        CHECK_CONTENT_POLICY_WITH_SERVICE(ShouldProcess, policyService);
    }
    CHECK_CONTENT_POLICY(ShouldProcess);
}

#undef CHECK_CONTENT_POLICY
#undef CHECK_CONTENT_POLICY_WITH_SERVICE

/**
 * Helper function to get an nsIDocShell given a context.
 * If the context is a document or window, the corresponding docshell will be
 * returned.
 * If the context is a non-document DOM node, the docshell of its ownerDocument
 * will be returned.
 *
 * @param aContext the context to find a docshell for (can be null)
 *
 * @return a WEAK pointer to the docshell, or nsnull if it could
 *     not be obtained
 *     
 * @note  As of this writing, calls to nsIContentPolicy::Should{Load,Process}
 * for TYPE_DOCUMENT and TYPE_SUBDOCUMENT pass in an aContext that either
 * points to the frameElement of the window the load is happening in
 * (in which case NS_CP_GetDocShellFromContext will return the parent of the
 * docshell the load is happening in), or points to the window the load is
 * happening in (in which case NS_CP_GetDocShellFromContext will return
 * the docshell the load is happening in).  It's up to callers to QI aContext
 * and handle things accordingly if they want the docshell the load is
 * happening in.  These are somewhat odd semantics, and bug 466687 has been
 * filed to consider improving them.
 */
inline nsIDocShell*
NS_CP_GetDocShellFromContext(nsISupports *aContext)
{
    if (!aContext) {
        return nsnull;
    }

    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aContext);

    if (!window) {
        // our context might be a document (which also QIs to nsIDOMNode), so
        // try that first
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(aContext);
        if (!doc) {
            // we were not a document after all, get our ownerDocument,
            // hopefully
            nsCOMPtr<nsIContent> content = do_QueryInterface(aContext);
            if (content) {
                doc = content->OwnerDoc();
            }
        }

        if (doc) {
            if (doc->GetDisplayDocument()) {
                doc = doc->GetDisplayDocument();
            }
            
            window = doc->GetWindow();
        }
    }

    if (!window) {
        return nsnull;
    }

    return window->GetDocShell();
}

#endif /* __nsContentPolicyUtils_h__ */
