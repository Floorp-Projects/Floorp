/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsISupports.h"
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
    NS_INIT_REFCNT();
    nsresult rv;
    NS_WITH_SERVICE(nsICategoryManager, catman, NS_CATEGORYMANAGER_CONTRACTID, &rv);
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
	nsCOMPtr<nsISupportsString> string = do_QueryInterface(item, &rv);
	if (NS_FAILED(rv))
	    continue;
	
	nsXPIDLCString contractid;
	if (NS_FAILED(string->GetData(getter_Copies(contractid))))
	    continue;

#ifdef DEBUG_shaver
	fprintf(stderr, "POLICY: loading %s\n", (const char *)contractid);
#endif
	/*
	 * Create this policy service and add to mPolicies.
	 *
	 * Should we try to parse as a CID, in case the component prefers to be
	 * registered that way?
	 */
	nsCOMPtr<nsISupports> policy = do_GetService(contractid, &rv);
	if (NS_SUCCEEDED(rv))
	    mPolicies->AppendElement(policy);
    }
	
}

nsContentPolicy::~nsContentPolicy()
{
}

#define POLICY_LOAD    0
#define POLICY_PROCESS 1

NS_IMETHODIMP
nsContentPolicy::CheckPolicy(PRInt32 policyType, PRInt32 contentType,
			     nsIDOMElement *element,
			     const PRUnichar *contentLocation,
                             PRBool *shouldProceed)
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
	if (policyType == POLICY_LOAD)
	    rv = policy->ShouldLoad(contentType, element, contentLocation,
				    shouldProceed);
	else
	    rv = policy->ShouldProcess(contentType, element, contentLocation,
				       shouldProceed);
	   
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
nsContentPolicy::ShouldLoad(PRInt32 contentType, nsIDOMElement *element,
			    const PRUnichar *contentLocation,
                            PRBool *shouldLoad)
{
    return CheckPolicy(POLICY_LOAD, contentType, element, contentLocation,
		       shouldLoad);
}

NS_IMETHODIMP
nsContentPolicy::ShouldProcess(PRInt32 contentType, nsIDOMElement *element,
			       const PRUnichar *contentLocation,
			       PRBool *shouldProcess)
{
    return CheckPolicy(POLICY_PROCESS, contentType, element, contentLocation,
		       shouldProcess);
}

