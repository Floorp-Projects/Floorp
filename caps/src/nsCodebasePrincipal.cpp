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
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsScriptSecurityManager.h"

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
    if (mPrefName.IsEmpty())
	{
        mPrefName.Assign("capability.principal.codebase.p");
        mPrefName.AppendInt(mCapabilitiesOrdinal++);
        mPrefName.Append(".id");
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
    // Either this principal must be preconfigured as a trusted source
    // (mTrusted), or else the codebase principal pref must be enabled
    if (!mTrusted)
    {
        static char pref[] = "signed.applets.codebase_principal_support";
        nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
        if (!prefBranch)
            return NS_ERROR_FAILURE;
        PRBool enabled;
        if (NS_FAILED(prefBranch->GetBoolPref(pref, &enabled)) || !enabled) 
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
nsCodebasePrincipal::Equals(nsIPrincipal *aOther, PRBool *result)
{
    if (this == aOther) 
    {
	 *result = PR_TRUE;
	 return NS_OK;
    }
    *result = PR_FALSE;
    if (!aOther)
        return NS_OK;

    // Get a URI from the other principal
    nsCOMPtr<nsICodebasePrincipal> otherCodebase(
        do_QueryInterface(aOther));
    if (!otherCodebase)
    {
        // Other principal is not a codebase, so return false
        return NS_OK;
    }
    nsCOMPtr<nsIURI> otherURI;
    otherCodebase->GetURI(getter_AddRefs(otherURI));

    NS_ENSURE_TRUE(otherURI, NS_ERROR_FAILURE);
    return nsScriptSecurityManager::SecurityCompareURIs(mURI,
                                                        otherURI,
                                                        result);
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

    return aStream->WriteCompoundObject(mURI, NS_GET_IID(nsIURI), PR_TRUE);
}

/////////////////////////////////////////////
// Constructor, Destructor, initialization //
/////////////////////////////////////////////

nsCodebasePrincipal::nsCodebasePrincipal() : mTrusted(PR_FALSE)
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
                                        const char* aGrantedList, const char* aDeniedList,
                                        PRBool aTrusted)
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), nsDependentCString(aURLStr), nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Malformed URI in security.principal preference.");
    if (NS_FAILED(rv)) return rv;

    if (NS_FAILED(Init(uri))) return NS_ERROR_FAILURE;
    // XXX: Add check for trusted = SSL only here?
    mTrusted = aTrusted;

    return nsBasePrincipal::InitFromPersistent(aPrefName, aURLStr, 
                                               aGrantedList, aDeniedList);
}

nsCodebasePrincipal::~nsCodebasePrincipal()
{
}
