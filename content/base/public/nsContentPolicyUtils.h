/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Timothy Watt <riceman+moz@mail.rit.edu>
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

/*
 * Utility routines for checking content load/process policy settings,
 * and routines helpful for content policy implementors.
 */

#ifndef __nsContentPolicyUtils_h__
#define __nsContentPolicyUtils_h__

// for PR_LOGGING
#include "prlog.h"

#include "nsString.h"
#include "nsIContentPolicy.h"
#include "nsIMemory.h"
#include "nsIServiceManager.h"
#include "nsIDOMDocument.h"

//XXXtw sadly, this makes consumers of nsContentPolicyUtils depend on widget
#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"

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
    CASE_RETURN( TYPE_OTHER      );
    CASE_RETURN( TYPE_SCRIPT     );
    CASE_RETURN( TYPE_IMAGE      );
    CASE_RETURN( TYPE_STYLESHEET );
    CASE_RETURN( TYPE_OBJECT     );
    CASE_RETURN( TYPE_DOCUMENT   );
    CASE_RETURN( TYPE_SUBDOCUMENT);
    CASE_RETURN( TYPE_REFRESH    );
  default:
    return "<Unknown Type>";
  }
}

#endif // defined(PR_LOGGING)

#undef CASE_RETURN

/* Passes on parameters from its "caller"'s context. */
#define CHECK_CONTENT_POLICY(action)                                          \
    nsCOMPtr<nsIContentPolicy> policy =                                       \
         do_GetService(NS_CONTENTPOLICY_CONTRACTID);                          \
    if (!policy)                                                              \
        return NS_ERROR_FAILURE;                                              \
                                                                              \
    return policy-> action (contentType, contentLocation, requestOrigin,      \
                            context, mimeType, extra, decision);

/**
 * Alias for calling ShouldLoad on the content policy service.
 * Parameters are the same as nsIContentPolicy::shouldLoad.
 */
inline nsresult
NS_CheckContentLoadPolicy(PRUint32          contentType,
                          nsIURI           *contentLocation,
                          nsIURI           *requestOrigin,
                          nsISupports      *context,
                          const nsACString &mimeType,
                          nsISupports      *extra,
                          PRInt16          *decision)
{
    CHECK_CONTENT_POLICY(ShouldLoad);
}

/**
 * Alias for calling ShouldProcess on the content policy service.
 * Parameters are the same as nsIContentPolicy::shouldProcess.
 */
inline nsresult
NS_CheckContentProcessPolicy(PRUint32          contentType,
                             nsIURI           *contentLocation,
                             nsIURI           *requestOrigin,
                             nsISupports      *context,
                             const nsACString &mimeType,
                             nsISupports      *extra,
                             PRInt16          *decision)
{
    CHECK_CONTENT_POLICY(ShouldProcess);
}

#undef CHECK_CONTENT_POLICY

/**
 * Helper function to get an nsIDocShell given a context.
 * If the context is a document or window, the corresponding docshell will be
 * returned.
 * If the context is a non-document DOM node, the docshell of its ownerDocument
 * will be returned.
 *
 * @param aContext the context to find a docshell for (can be null)
 * @return a WEAK pointer to the docshell, or nsnull if it could
 *     not be obtained
 */
static nsIDocShell*
NS_CP_GetDocShellFromContext(nsISupports *aContext)
{
    if (!aContext) {
        return nsnull;
    }

    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal = do_QueryInterface(aContext);

    if (!scriptGlobal) {
        // our context might be a document (which also QIs to nsIDOMNode), so
        // try that first
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(aContext);
        if (!doc) {
            // we were not a document after all, get our ownerDocument,
            // hopefully
            nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aContext);
            if (node) {
                nsCOMPtr<nsIDOMDocument> ownerDoc;
                node->GetOwnerDocument(getter_AddRefs(ownerDoc));
                // and turn it into an nsIDocument
                doc = do_QueryInterface(ownerDoc);
            }
        }

        if (doc) {
            scriptGlobal = doc->GetScriptGlobalObject();
        }
    }

    if (!scriptGlobal) {
        return nsnull;
    }

    return scriptGlobal->GetDocShell();
}

#endif /* __nsContentPolicyUtils_h__ */
