/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Describes principals by their orginating uris */

#include "nsCodebasePrincipal.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsIJARURI.h"
#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

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
        mPrefName = ToNewCString(s);
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
    nsBasePrincipal::CanEnableCapability(capability, result);
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
    nsresult rv;
    nsCAutoString s;
    if (NS_SUCCEEDED(mURI->GetHost(s)))
        rv = mURI->GetPrePath(s);
    else {
        // Some URIs (e.g., nsSimpleURI) don't support host. Just
        // get the full spec.
        rv = mURI->GetSpec(s);
    }
    if (NS_FAILED(rv)) return rv;

    *origin = ToNewCString(s);
    return *origin ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsCodebasePrincipal::GetSpec(char **spec) 
{
    nsCAutoString buf;
    nsresult rv = mURI->GetSpec(buf);
    if (NS_FAILED(rv)) return rv;

    *spec = ToNewCString(buf);
    return *spec ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP
nsCodebasePrincipal::Equals(nsIPrincipal *other, PRBool *result)
{
    // Equals is defined as object equality or same origin
    *result = PR_FALSE;
    if (this == other) 
	{
        *result = PR_TRUE;
        return NS_OK;
    }
    if (other == nsnull) 
        // return false
        return NS_OK;

    // Get the other principal's URI
    nsCOMPtr<nsICodebasePrincipal> otherCodebase;
    if (NS_FAILED(other->QueryInterface(
            NS_GET_IID(nsICodebasePrincipal),
            (void **) getter_AddRefs(otherCodebase))))
        return NS_OK;
    nsCOMPtr<nsIURI> otherURI;
    if (NS_FAILED(otherCodebase->GetURI(getter_AddRefs(otherURI))))
        return NS_ERROR_FAILURE;

    // If either uri is a jar URI, get the base URI
    nsCOMPtr<nsIJARURI> jarURI;
    nsCOMPtr<nsIURI> myBaseURI(mURI);
    while((jarURI = do_QueryInterface(myBaseURI)))
    {
        jarURI->GetJARFile(getter_AddRefs(myBaseURI));
    }
    while((jarURI = do_QueryInterface(otherURI)))
    {
        jarURI->GetJARFile(getter_AddRefs(otherURI));
    }

    if (!myBaseURI || !otherURI)
        return NS_ERROR_FAILURE;

    // Compare schemes
    nsCAutoString otherScheme;
    nsresult rv = otherURI->GetScheme(otherScheme);
    nsCAutoString myScheme;
    if (NS_SUCCEEDED(rv))
        rv = myBaseURI->GetScheme(myScheme);
    if (NS_SUCCEEDED(rv) && otherScheme.Equals(myScheme)) 
    {
        if (otherScheme.Equals("file"))
        {
            // All file: urls are considered to have the same origin.
            *result = PR_TRUE;
        }
        else if (otherScheme.Equals("imap")    ||
                 otherScheme.Equals("mailbox") ||
                 otherScheme.Equals("news"))
        {
            // Each message is a distinct trust domain; use the 
            // whole spec for comparison
            nsCAutoString otherSpec;
            if (NS_FAILED(otherURI->GetSpec(otherSpec)))
                return NS_ERROR_FAILURE;
            nsCAutoString mySpec;
            if (NS_FAILED(myBaseURI->GetSpec(mySpec)))
                return NS_ERROR_FAILURE;
            *result = otherSpec.Equals(mySpec);
        } 
		else
		{
            // Compare hosts
            nsCAutoString otherHost;
            rv = otherURI->GetHost(otherHost);
            nsCAutoString myHost;
            if (NS_SUCCEEDED(rv))
                rv = myBaseURI->GetHost(myHost);
            *result = NS_SUCCEEDED(rv) && otherHost.Equals(myHost);
            if (*result) 
            {
                // Compare ports
                PRInt32 otherPort;
                rv = otherURI->GetPort(&otherPort);
                PRInt32 myPort;
                if (NS_SUCCEEDED(rv))
                    rv = myBaseURI->GetPort(&myPort);
                *result = NS_SUCCEEDED(rv) && otherPort == myPort;
                // If the port comparison failed, see if either URL has a
                // port of -1. If so, replace -1 with the default port
                // for that scheme.
                if(!*result && (myPort == -1 || otherPort == -1))
                {
                    PRInt32 defaultPort;
                    //XXX had to hard-code the defualt port for http(s) here.
                    //    remove this after darin fixes bug 113206
                    if (myScheme.Equals("http"))
                        defaultPort = 80;
                    else if (myScheme.Equals("https"))
                        defaultPort = 443;
                    else
                    {
                        nsCOMPtr<nsIIOService> ioService(
                            do_GetService(NS_IOSERVICE_CONTRACTID));
                        if (!ioService)
                            return NS_ERROR_FAILURE;
                        nsCOMPtr<nsIProtocolHandler> protocolHandler;
                        rv = ioService->GetProtocolHandler(myScheme.get(),
                                                           getter_AddRefs(protocolHandler));
                        if (NS_FAILED(rv))
                            return rv;
                    
                        rv = protocolHandler->GetDefaultPort(&defaultPort);
                        if (NS_FAILED(rv) || defaultPort == -1)
                            return NS_OK; // No default port for this scheme
                    }
                    if (myPort == -1)
                        myPort = defaultPort;
                    else if (otherPort == -1)
                        otherPort = defaultPort;
                    *result = otherPort == myPort;
                }
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
    nsCAutoString codebase;
    if (uri == nsnull || NS_FAILED(uri->GetSpec(codebase))) 
        return NS_ERROR_FAILURE;
    if (NS_FAILED(mJSPrincipals.Init(ToNewCString(codebase))))
        return NS_ERROR_FAILURE;
    // JSPrincipals::Init adopts its input
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
    rv = NS_NewURI(getter_AddRefs(uri), nsDependentCString(aURLStr), nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Malformed URI in security.principal preference.");
    if (NS_FAILED(rv)) return rv;

    if (NS_FAILED(Init(uri))) return NS_ERROR_FAILURE;

    return nsBasePrincipal::InitFromPersistent(aPrefName, aURLStr, 
                                               aGrantedList, aDeniedList);
}

nsCodebasePrincipal::~nsCodebasePrincipal()
{
}
