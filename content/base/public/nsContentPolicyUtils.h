/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Zero-Knowledge Systems,
 * Inc.  Portions created by Zero-Knowledge are Copyright (C) 2000
 * Zero-Knowledge Systems, Inc.  All Rights Reserved.
 *
 * Contributor(s): 
 */

/*
 * Utility routines for checking content load/process policy settings.
 */

#include "nsString.h"
#include "nsIContentPolicy.h"
#include "nsIMemory.h"
#include "nsIServiceManager.h"

#ifndef __nsContentPolicyUtils_h__
#define __nsContentPolicyUtils_h__

class nsIDOMElement;

#define NS_CONTENTPOLICY_CONTRACTID   "@mozilla.org/layout/content-policy;1"
#define NS_CONTENTPOLICY_CATEGORY "content-policy"
#define NS_CONTENTPOLICY_CID						      \
  {0x0e3afd3d, 0xeb60, 0x4c2b,						      \
    { 0x96, 0x3b, 0x56, 0xd7, 0xc4, 0x39, 0xf1, 0x24 }}

/* Takes contentType, aURI, context, and window from its "caller"'s context. */
#define CHECK_CONTENT_POLICY(action, result)                                  \
    nsCOMPtr<nsIContentPolicy> policy =                                       \
         do_GetService(NS_CONTENTPOLICY_CONTRACTID);                          \
    if (!policy)                                                              \
        return NS_ERROR_FAILURE;                                              \
                                                                              \
    return policy-> action (contentType, aURI, context, window, result);

inline nsresult
NS_CheckContentLoadPolicy(PRInt32 contentType, nsIURI *aURI,
                          nsISupports *context, nsIDOMWindow *window,
                          PRBool *shouldLoad)
{
    CHECK_CONTENT_POLICY(ShouldLoad, shouldLoad);
}

inline nsresult
NS_CheckContentProcessPolicy(PRInt32 contentType, nsIURI *aURI,
                             nsISupports *context, nsIDOMWindow *window,
                             PRBool *shouldProcess)
{
    CHECK_CONTENT_POLICY(ShouldProcess, shouldProcess);
}

#undef CHECK_CONTENT_POLICY

#endif /* __nsContentPolicyUtils_h__ */
