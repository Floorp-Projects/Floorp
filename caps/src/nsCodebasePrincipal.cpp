/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* Describes principals by thier orginating uris */
#include "nsCodebasePrincipal.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"


static NS_DEFINE_IID(kICodebasePrincipalIID, NS_ICODEBASEPRINCIPAL_IID);
static char gFileScheme[] = "file";

NS_IMPL_ISUPPORTS(nsCodebasePrincipal, kICodebasePrincipalIID);

NS_IMETHODIMP
nsCodebasePrincipal::GetJSPrincipals(JSPrincipals **jsprin)
{
    if (itsJSPrincipals.nsIPrincipalPtr == nsnull) {
        itsJSPrincipals.nsIPrincipalPtr = this;
        NS_ADDREF(itsJSPrincipals.nsIPrincipalPtr);
        // matching release in nsDestroyJSPrincipals
    }
    *jsprin = &itsJSPrincipals;
    JSPRINCIPALS_HOLD(cx, *jsprin);
    return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::GetURI(nsIURI **uri) 
{
    *uri = itsURI;
    NS_ADDREF(*uri);
    return NS_OK;
}

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
    if (!NS_SUCCEEDED(other->QueryInterface(
            NS_GET_IID(nsICodebasePrincipal),
            (void **) getter_AddRefs(otherCodebase))))
    {
        *result = PR_FALSE;
        return NS_OK;
    }
    nsCOMPtr<nsIURI> otherURI;
    if (!NS_SUCCEEDED(otherCodebase->GetURI(getter_AddRefs(otherURI)))) {
        return NS_ERROR_FAILURE;
    }
    if (!itsURI || !NS_SUCCEEDED(otherURI->Equals(itsURI, result))) {
        return NS_ERROR_FAILURE;
    }
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
    if (!NS_SUCCEEDED(other->QueryInterface(
            NS_GET_IID(nsICodebasePrincipal),
            (void **) getter_AddRefs(otherCodebase))))
    {
        return NS_OK;
    }
    nsCOMPtr<nsIURI> otherURI;
    if (!NS_SUCCEEDED(otherCodebase->GetURI(getter_AddRefs(otherURI)))) {
        return NS_ERROR_FAILURE;
    }
    char *scheme1 = nsnull;
    nsresult rv = otherURI->GetScheme(&scheme1);
    char *scheme2 = nsnull;
    if (NS_SUCCEEDED(rv))
        rv = itsURI->GetScheme(&scheme2);
    if (NS_SUCCEEDED(rv) && PL_strcmp(scheme1, scheme2) == 0) {

        if (PL_strcmp(scheme1, gFileScheme) == 0) {
            // All file: urls are considered to have the same origin.
            *result = PR_TRUE;
        } else {
            // Need to check the host
            char *host1 = nsnull;
            rv = otherURI->GetHost(&host1);
            char *host2 = nsnull;
            if (NS_SUCCEEDED(rv))
                rv = itsURI->GetHost(&host2);
            *result = NS_SUCCEEDED(rv) && PL_strcmp(host1, host2) == 0;
            if (*result) {
                int port1;
                rv = otherURI->GetPort(&port1);
                int port2;
                if (NS_SUCCEEDED(rv))
                    rv = itsURI->GetPort(&port2);
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

nsCodebasePrincipal::nsCodebasePrincipal()
{
    NS_INIT_ISUPPORTS();
    itsURI = nsnull;
}

NS_IMETHODIMP
nsCodebasePrincipal::Init(nsIURI *uri)
{
    char *codebase;
    if (!NS_SUCCEEDED(uri->GetSpec(&codebase))) 
        return NS_ERROR_FAILURE;
    if (!NS_SUCCEEDED(itsJSPrincipals.Init(codebase))) 
        return NS_ERROR_FAILURE;
    NS_ADDREF(this);
    itsURI = uri;
    NS_ADDREF(itsURI);
    return NS_OK;
}

nsCodebasePrincipal::~nsCodebasePrincipal(void)
{
    if (itsURI)
        NS_RELEASE(itsURI);
}
