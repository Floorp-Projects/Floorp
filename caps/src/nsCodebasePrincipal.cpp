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

NS_IMPL_QUERY_INTERFACE3_CI(nsCodebasePrincipal,
                            nsICodebasePrincipal,
                            nsIPrincipal,
                            nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER3(nsCodebasePrincipal,
                             nsICodebasePrincipal,
                             nsIPrincipal,
                             nsISerializable)

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
    if (!mPrefName)
	{
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
    nsXPIDLCString spec;
    if (NS_FAILED(GetSpec(getter_Copies(spec))))
        return NS_ERROR_FAILURE;
    *result = nsCRT::HashCode(spec, nsnull);
    return NS_OK;
}

NS_IMETHODIMP 
nsCodebasePrincipal::CanEnableCapability(const char *capability, 
                                         PRInt16 *result)
{
    // check to see if the codebase principal pref is enabled.
    static char pref[] = "signed.applets.codebase_principal_support";
    nsresult rv;
	nsCOMPtr<nsIPref> prefs(do_GetService("@mozilla.org/preferences;1", &rv));
	if (NS_FAILED(rv))
		return NS_ERROR_FAILURE;
	PRBool enabled;
    if (NS_FAILED(prefs->GetBoolPref(pref, &enabled)) || !enabled) 
	{
        // Deny unless subject is executing from file: or resource: 
        PRBool isFile = PR_FALSE;
        PRBool isRes = PR_FALSE;

        if (NS_FAILED(mURI->SchemeIs("file", &isFile)) || 
            NS_FAILED(mURI->SchemeIs("resource", &isRes)) ||
            (!isFile && !isRes))
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
    if (NS_SUCCEEDED(mURI->GetHost(getter_Copies(s))))
		return mURI->GetPrePath(origin);

    // Some URIs (e.g., nsSimpleURI) don't support host. Just
    // get the full spec.
	return mURI->GetSpec(origin);
}

NS_IMETHODIMP
nsCodebasePrincipal::GetSpec(char **spec) 
{
	return mURI->GetSpec(spec);
}


NS_IMETHODIMP
nsCodebasePrincipal::Equals(nsIPrincipal *other, PRBool *result)
{

    //-- Equals is defined as object equality or same origin
    *result = PR_FALSE;
    if (this == other) 
	{
        *result = PR_TRUE;
        return NS_OK;
    }
    if (other == nsnull) 
        // return false
        return NS_OK;
    nsCOMPtr<nsICodebasePrincipal> otherCodebase;
    if (NS_FAILED(other->QueryInterface(
            NS_GET_IID(nsICodebasePrincipal),
            (void **) getter_AddRefs(otherCodebase))))
        return NS_OK;
    nsCOMPtr<nsIURI> otherURI;
    if (NS_FAILED(otherCodebase->GetURI(getter_AddRefs(otherURI))))
        return NS_ERROR_FAILURE;
    nsXPIDLCString otherScheme;
    nsresult rv = otherURI->GetScheme(getter_Copies(otherScheme));
    nsXPIDLCString myScheme;
    if (NS_SUCCEEDED(rv))
        rv = mURI->GetScheme(getter_Copies(myScheme));
    if (NS_SUCCEEDED(rv) && PL_strcmp(otherScheme, myScheme) == 0) 
	{

        if (PL_strcmp(otherScheme, "file") == 0)
        {
            // All file: urls are considered to have the same origin.
            *result = PR_TRUE;
        }
        else if (PL_strcmp(otherScheme, "imap")    == 0 ||
	             PL_strcmp(otherScheme, "mailbox") == 0 ||
                 PL_strcmp(otherScheme, "news")    == 0) 
        {
            // Each message is a distinct trust domain; use the 
            // whole spec for comparison
            nsXPIDLCString otherSpec;
            if (NS_FAILED(otherURI->GetSpec(getter_Copies(otherSpec))))
                return NS_ERROR_FAILURE;
            nsXPIDLCString mySpec;
            if (NS_FAILED(mURI->GetSpec(getter_Copies(mySpec))))
                return NS_ERROR_FAILURE;
            *result = PL_strcmp(otherSpec, mySpec) == 0;
        } 
		else
		{
            // Need to check the host
            nsXPIDLCString otherHost;
            rv = otherURI->GetHost(getter_Copies(otherHost));
            nsXPIDLCString myHost;
            if (NS_SUCCEEDED(rv))
                rv = mURI->GetHost(getter_Copies(myHost));
            *result = NS_SUCCEEDED(rv) && PL_strcmp(otherHost, myHost) == 0;
            if (*result) 
			{
                int otherPort;
                rv = otherURI->GetPort(&otherPort);
                int myPort;
                if (NS_SUCCEEDED(rv))
                    rv = mURI->GetPort(&myPort);
                *result = NS_SUCCEEDED(rv) && otherPort == myPort;
            }
        }
    }
    return NS_OK;
}

//////////////////////////////////////////
// Methods implementing nsISerializable //
//////////////////////////////////////////

NS_IMETHODIMP
nsCodebasePrincipal::Read(nsIObjectInputStream* aStream)
{
    nsresult rv;

    rv = nsBasePrincipal::Read(aStream);
    if (NS_FAILED(rv)) return rv;

    return aStream->ReadObject(PR_TRUE, getter_AddRefs(mURI));
}

NS_IMETHODIMP
nsCodebasePrincipal::Write(nsIObjectOutputStream* aStream)
{
    nsresult rv;

    rv = nsBasePrincipal::Write(aStream);
    if (NS_FAILED(rv)) return rv;

    return aStream->WriteObject(mURI, PR_TRUE);
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
    if (NS_FAILED(mJSPrincipals.Init(codebase)))
	{
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
