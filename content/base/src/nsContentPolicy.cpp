/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsISupports.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsContentPolicyUtils.h"
#include "nsContentPolicy.h"
#include "nsICategoryManager.h"

NS_IMPL_ISUPPORTS1(nsContentPolicy, nsIContentPolicy)

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
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman = 
             do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
	/* log an error? */
	return;

    /*
     * I'd like to use GetCategoryContents, so that I can size the array
     * correctly on the first go and avoid the enumerator overhead, but it's
     * not yet implemented (see nsCategoryManager.cpp).  No biggie, I guess.
     */
    nsCOMPtr<nsISimpleEnumerator> catEnum;
    if (NS_FAILED(catman->EnumerateCategory(NS_CONTENTPOLICY_CATEGORY,
					    getter_AddRefs(catEnum)))) {
	/* no category, no problem */
	return;
    }

    PRBool hasMore;
    if (NS_FAILED(catEnum->HasMoreElements(&hasMore)) || !hasMore ||
       NS_FAILED(NS_NewISupportsArray(getter_AddRefs(mPolicies)))) {
       return;
    }
    
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

#ifdef DEBUG_shaver
	fprintf(stderr, "POLICY: loading %s\n", contractid.get());
#endif
	/*
	 * Create this policy service and add to mPolicies.
	 *
	 * Should we try to parse as a CID, in case the component prefers to be
	 * registered that way?
	 */
	nsCOMPtr<nsISupports> policy = do_GetService(contractid.get(), &rv);
	if (NS_SUCCEEDED(rv))
	    mPolicies->AppendElement(policy);
    }
	
}

nsContentPolicy::~nsContentPolicy()
{
}

#define POLICY_LOAD    (PRInt32)0
#define POLICY_PROCESS (PRInt32)1

NS_IMETHODIMP
nsContentPolicy::CheckPolicy(PRInt32 policyType, PRInt32 contentType,
                             nsIURI *contentLocation, nsISupports *context,
                             nsIDOMWindow *window, PRBool *shouldProceed)
{
    *shouldProceed = PR_TRUE;
    if (!mPolicies)
	return NS_OK;

    /* 
     * Enumerate mPolicies and ask each of them, taking the logical AND of
     * their permissions.
     */
    nsresult rv;
    nsCOMPtr<nsIContentPolicy> policy;
    PRUint32 count;
    if (NS_FAILED(rv = mPolicies->Count(&count)))
	return NS_OK;

    for (PRUint32 i = 0; i < count; i++) {
	rv = mPolicies->QueryElementAt(i, NS_GET_IID(nsIContentPolicy),
				       getter_AddRefs(policy));
	if (NS_FAILED(rv))
	    continue;
	
	/* check the appropriate policy */
	if (policyType == POLICY_LOAD) {
	    rv = policy->ShouldLoad(contentType, contentLocation, context,
                                    window, shouldProceed);
	} else {
	    rv = policy->ShouldProcess(contentType, contentLocation, context,
                                       window, shouldProceed);
        }
	   
	if (NS_SUCCEEDED(rv) && !*shouldProceed)
	    /* policy says no, no point continuing to check */
	    return NS_OK;
    }

    /*
     * One of the policy objects might be misbehaving and setting shouldProceed
     * to PR_FALSE before returning an error, so force it back to PR_TRUE
     * here.
     */
    *shouldProceed = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsContentPolicy::ShouldLoad(PRInt32 contentType, nsIURI *contentLocation,
                            nsISupports *context, nsIDOMWindow *window,
                            PRBool *shouldLoad)
{
    return CheckPolicy(POLICY_LOAD, contentType, contentLocation, context,
                       window, shouldLoad);
}

NS_IMETHODIMP
nsContentPolicy::ShouldProcess(PRInt32 contentType, nsIURI *contentLocation,
                               nsISupports *context, nsIDOMWindow *window,
                               PRBool *shouldProcess)
{
    return CheckPolicy(POLICY_PROCESS, contentType, contentLocation, context,
		       window, shouldProcess);
}

