/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* Describes principals by their orginating uris */

#include "nsCodebasePrincipal.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"

NS_IMPL_QUERY_INTERFACE2(nsCodebasePrincipal, nsICodebasePrincipal, nsIPrincipal)

// special AddRef/Release to unify reference counts between XPCOM 
//  and JSPrincipals

NS_IMETHODIMP_(nsrefcnt)
nsCodebasePrincipal::AddRef(void)
{
    NS_PRECONDITION(PRInt32(mRefCnt) == 0, "illegal mRefCnt");
    NS_PRECONDITION(PRInt32(mJSPrincipals.refcount) >= 0, "illegal refcnt");
    ++mJSPrincipals.refcount;
    NS_LOG_ADDREF(this, mJSPrincipals.refcount, "nsCodebasePrincipal", sizeof(*this));
    return mJSPrincipals.refcount;
}

NS_IMETHODIMP_(nsrefcnt)
nsCodebasePrincipal::Release(void)
{
    NS_PRECONDITION(PRInt32(mRefCnt) == 0, "illegal mRefCnt");
    NS_PRECONDITION(0 != mJSPrincipals.refcount, "dup release");
    --mJSPrincipals.refcount;
    NS_LOG_RELEASE(this, mJSPrincipals.refcount, "nsCodebasePrincipal");
    if (mJSPrincipals.refcount == 0) {
#ifdef DEBUG_norris
        char *spec;
        mURI->GetSpec(&spec);
        fprintf(stderr, "Releasing principal for %s\n", spec);
        delete spec;
#endif
        NS_DELETEXPCOM(this);
        return 0;
    }  
    return mJSPrincipals.refcount;
}


////////////////////////////////////
// Methods implementing nsIPrincipal
////////////////////////////////////

NS_IMETHODIMP
nsCodebasePrincipal::ToString(char **result)
{
    // NB TODO
    return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::Equals(nsIPrincipal *other, PRBool *result)
{
    *result = PR_FALSE;
    if (this == other) {
        *result = PR_TRUE;
        return NS_OK;
    }
    nsCOMPtr<nsICodebasePrincipal> otherCodebase;
    if (NS_FAILED(other->QueryInterface(
            NS_GET_IID(nsICodebasePrincipal),
            (void **) getter_AddRefs(otherCodebase))))
    {
        *result = PR_FALSE;
        return NS_OK;
    }
    nsCOMPtr<nsIURI> otherURI;
    if (NS_FAILED(otherCodebase->GetURI(getter_AddRefs(otherURI)))) {
        return NS_ERROR_FAILURE;
    }
    if (!mURI || NS_FAILED(otherURI->Equals(mURI, result))) {
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::GetJSPrincipals(JSPrincipals **jsprin)
{
    if (mJSPrincipals.nsIPrincipalPtr == nsnull) {
        mJSPrincipals.nsIPrincipalPtr = this;
        // No need for a ADDREF since it is a self-reference
    }
    *jsprin = &mJSPrincipals;
    JSPRINCIPALS_HOLD(cx, *jsprin);
    return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::CanAccess(const char *capability, PRBool *result)
{
    // Codebases have no special privileges.
    *result = PR_FALSE;
    return NS_OK;
}


////////////////////////////////////////////
// Methods implementing nsICodebasePrincipal
////////////////////////////////////////////

NS_IMETHODIMP
nsCodebasePrincipal::GetURI(nsIURI **uri) 
{
    *uri = mURI;
    NS_ADDREF(*uri);
    return NS_OK;
}



NS_IMETHODIMP
nsCodebasePrincipal::SameOrigin(nsIPrincipal *other, PRBool *result)
{
    *result = PR_FALSE;
    if (this == other) {
        *result = PR_TRUE;
        return NS_OK;
    }
    nsCOMPtr<nsICodebasePrincipal> otherCodebase;
    if (NS_FAILED(other->QueryInterface(
            NS_GET_IID(nsICodebasePrincipal),
            (void **) getter_AddRefs(otherCodebase))))
    {
        return NS_OK;
    }
    nsCOMPtr<nsIURI> otherURI;
    if (NS_FAILED(otherCodebase->GetURI(getter_AddRefs(otherURI)))) {
        return NS_ERROR_FAILURE;
    }
    char *scheme1 = nsnull;
    nsresult rv = otherURI->GetScheme(&scheme1);
    char *scheme2 = nsnull;
    if (NS_SUCCEEDED(rv))
        rv = mURI->GetScheme(&scheme2);
    if (NS_SUCCEEDED(rv) && PL_strcmp(scheme1, scheme2) == 0) {

        if (PL_strcmp(scheme1, "file") == 0) {
            // All file: urls are considered to have the same origin.
            *result = PR_TRUE;
        } else {
            // Need to check the host
            char *host1 = nsnull;
            rv = otherURI->GetHost(&host1);
            char *host2 = nsnull;
            if (NS_SUCCEEDED(rv))
                rv = mURI->GetHost(&host2);
            *result = NS_SUCCEEDED(rv) && PL_strcmp(host1, host2) == 0;
            if (*result) {
                int port1;
                rv = otherURI->GetPort(&port1);
                int port2;
                if (NS_SUCCEEDED(rv))
                    rv = mURI->GetPort(&port2);
                *result = NS_SUCCEEDED(rv) && port1 == port2;
            }
            if (host1) nsCRT::free(host1);
            if (host2) nsCRT::free(host2);
        }
    }
    if (scheme1) nsCRT::free(scheme1);
    if (scheme2) nsCRT::free(scheme2);
    return NS_OK;
}

//////////////////////////////////////////
// Constructor, Destructor, initialization
//////////////////////////////////////////

nsCodebasePrincipal::nsCodebasePrincipal()
{
    NS_INIT_ISUPPORTS();
    mURI = nsnull;
}

NS_IMETHODIMP
nsCodebasePrincipal::Init(nsIURI *uri)
{
    char *codebase;
    if (NS_FAILED(uri->GetSpec(&codebase))) 
        return NS_ERROR_FAILURE;
    if (NS_FAILED(mJSPrincipals.Init(codebase))) {
        nsCRT::free(codebase);
        return NS_ERROR_FAILURE;
    }
    // JSPrincipals::Init adopts codebase, so no need to free now
    mURI = uri;
    NS_ADDREF(mURI);
    return NS_OK;
}

nsCodebasePrincipal::~nsCodebasePrincipal(void)
{
    if (mURI)
        NS_RELEASE(mURI);
}
