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
