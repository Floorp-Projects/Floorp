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
 * Copyright (C) 1999-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 * Mitch Stoltz
 */

/* Describes principals by their orginating uris */

#include "nsCodebasePrincipal.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsXPIDLString.h"

NS_IMPL_QUERY_INTERFACE2(nsCodebasePrincipal, nsICodebasePrincipal, nsIPrincipal)

NSBASEPRINCIPALS_ADDREF(nsCodebasePrincipal);
NSBASEPRINCIPALS_RELEASE(nsCodebasePrincipal);

///////////////////////////////////////
// Methods implementing nsIPrincipal //
///////////////////////////////////////
NS_IMETHODIMP
nsCodebasePrincipal::ToString(char **result)
{
    return GetOrigin(result);
}

NS_IMETHODIMP
nsCodebasePrincipal::ToUserVisibleString(char **result)
{
    return GetOrigin(result);
}

NS_IMETHODIMP 
nsCodebasePrincipal::GetPreferences(char** aPrefName, char** aID, 
                                    char** aGrantedList, char** aDeniedList)
{
    if (!mPrefName) {
        nsCAutoString s;
        s.Assign("capability.principal.codebase.p");
        s.AppendInt(mCapabilitiesOrdinal++);
        s.Append(".id");
        mPrefName = s.ToNewCString();
    }
    return nsBasePrincipal::GetPreferences(aPrefName, aID, 
                                           aGrantedList, aDeniedList);
}

NS_IMETHODIMP
nsCodebasePrincipal::HashValue(PRUint32 *result)
{
    nsXPIDLCString origin;
    if (NS_FAILED(GetOrigin(getter_Copies(origin))))
        return NS_ERROR_FAILURE;
    *result = nsCRT::HashCode(origin, nsnull);
    return NS_OK;
}

NS_IMETHODIMP 
nsCodebasePrincipal::CanEnableCapability(const char *capability, 
                                         PRInt16 *result)
{
    // check to see if the codebase principal pref is enabled.
    static char pref[] = "signed.applets.codebase_principal_support";
    nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, "component://netscape/preferences", &rv);
	if (NS_FAILED(rv))
		return NS_ERROR_FAILURE;
	PRBool enabled;
    if (NS_FAILED(prefs->GetBoolPref(pref, &enabled)) || !enabled) {
        // Deny unless subject is executing from file: or resource: 
        nsXPIDLCString scheme;
        if (NS_FAILED(mURI->GetScheme(getter_Copies(scheme))) ||
            (PL_strcmp(scheme, "file") != 0 &&
             PL_strcmp(scheme, "resource") != 0))
        {
            *result = nsIPrincipal::ENABLE_DENIED;
            return NS_OK;
        }
    }
    rv = nsBasePrincipal::CanEnableCapability(capability, result);
    if (*result == nsIPrincipal::ENABLE_UNKNOWN)
        *result = ENABLE_WITH_USER_PERMISSION;
    return NS_OK;
}

///////////////////////////////////////////////
// Methods implementing nsICodebasePrincipal //
///////////////////////////////////////////////

NS_IMETHODIMP
nsCodebasePrincipal::GetURI(nsIURI **uri) 
{
    *uri = mURI;
    NS_ADDREF(*uri);
    return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::GetOrigin(char **origin) 
{
    nsXPIDLCString s;
    if (NS_FAILED(mURI->GetScheme(getter_Copies(s))))
        return NS_ERROR_FAILURE;

    nsCAutoString t;
    t.Assign(s);
    t.Append("://");
    if (NS_SUCCEEDED(mURI->GetHost(getter_Copies(s)))) {
        t.Append(s);
    } else if (NS_SUCCEEDED(mURI->GetSpec(getter_Copies(s)))) {
        // Some URIs (e.g., nsSimpleURI) don't support host. Just
        // get the full spec.
        t.Assign(s);
    } else {
        return NS_ERROR_FAILURE;
    }
    *origin = t.ToNewCString();
    return *origin ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsCodebasePrincipal::Equals(nsIPrincipal *other, PRBool *result)
{

    //-- Equals is defined as object equality or same origin
    *result = PR_FALSE;
    if (this == other) {
        *result = PR_TRUE;
        return NS_OK;
    }
    if (other == nsnull) {
        *result = PR_FALSE;
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
        } else if (PL_strcmp(scheme1, "imap") == 0 ||
                   PL_strcmp(scheme1, "mailbox") == 0) 
        {
            // Each message is a distinct trust domain; use the 
            // whole spec for comparison
            nsXPIDLCString spec1;
            if (NS_FAILED(otherURI->GetSpec(getter_Copies(spec1))))
                return NS_ERROR_FAILURE;
            nsXPIDLCString spec2;
            if (NS_FAILED(mURI->GetSpec(getter_Copies(spec2))))
                return NS_ERROR_FAILURE;
            *result = PL_strcmp(spec1, spec2) == 0;
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

/////////////////////////////////////////////
// Constructor, Destructor, initialization //
/////////////////////////////////////////////

nsCodebasePrincipal::nsCodebasePrincipal()
{
    NS_INIT_ISUPPORTS();
}

nsresult
nsCodebasePrincipal::Init(nsIURI *uri)
{
    char *codebase;
    if (uri == nsnull || NS_FAILED(uri->GetSpec(&codebase))) 
        return NS_ERROR_FAILURE;
    if (NS_FAILED(mJSPrincipals.Init(codebase))) {
        nsCRT::free(codebase);
        return NS_ERROR_FAILURE;
    }
    // JSPrincipals::Init adopts codebase, so no need to free now
    mURI = uri;
    return NS_OK;
}

// This method overrides nsBasePrincipal::InitFromPersistent
nsresult
nsCodebasePrincipal::InitFromPersistent(const char* aPrefName, const char* aURLStr, 
                                        const char* aGrantedList, const char* aDeniedList)
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), aURLStr, nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Malformed URI in security.principal preference.");
    if (NS_FAILED(rv)) return rv;

    if (NS_FAILED(Init(uri))) return NS_ERROR_FAILURE;

    return nsBasePrincipal::InitFromPersistent(aPrefName, aURLStr, 
                                               aGrantedList, aDeniedList);
}

nsCodebasePrincipal::~nsCodebasePrincipal()
{
}
