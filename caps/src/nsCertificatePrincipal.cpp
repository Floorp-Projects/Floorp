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
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 * Mitch Stoltz <mstoltz@netscape.com>
 */

/*describes principals for use in signed scripts*/
#include "nsCertificatePrincipal.h"
#include "prmem.h"
#include "nsCOMPtr.h"

static NS_DEFINE_IID(kICertificatePrincipalIID, NS_ICERTIFICATEPRINCIPAL_IID);

NS_IMPL_QUERY_INTERFACE2(nsCertificatePrincipal, nsICertificatePrincipal, nsIPrincipal)

NSBASEPRINCIPALS_ADDREF(nsCertificatePrincipal);
NSBASEPRINCIPALS_RELEASE(nsCertificatePrincipal);

//////////////////////////////////////////////////
// Methods implementing nsICertificatePrincipal //
//////////////////////////////////////////////////
NS_IMETHODIMP
nsCertificatePrincipal::GetCertificateID(char** aCertificateID)
{
    *aCertificateID = nsCRT::strdup(mCertificateID);
	return *mCertificateID ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetCommonName(char** aCommonName)
{
    *aCommonName = nsCRT::strdup(mCommonName);
	return *aCommonName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsCertificatePrincipal::SetCommonName(const char* aCommonName)
{
    PR_FREEIF(mCommonName);
    mCommonName = nsCRT::strdup(aCommonName);
	return * mCommonName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


///////////////////////////////////////
// Methods implementing nsIPrincipal //
///////////////////////////////////////
NS_IMETHODIMP 
nsCertificatePrincipal::CanEnableCapability(const char *capability,
                                            PRInt16 *result)
{
    if(NS_FAILED(nsBasePrincipal::CanEnableCapability(capability, result)))
        return NS_ERROR_FAILURE;
    if (*result == nsIPrincipal::ENABLE_UNKNOWN)
        *result = ENABLE_WITH_USER_PERMISSION;
    return NS_OK;
}

NS_IMETHODIMP 
nsCertificatePrincipal::ToString(char **result)
{
    return GetCertificateID(result);
}

NS_IMETHODIMP 
nsCertificatePrincipal::ToUserVisibleString(char **result)
{
    return GetCommonName(result);
}

NS_IMETHODIMP 
nsCertificatePrincipal::ToStreamableForm(char** aName, char** aData)
{
    if (!mPrefName) {
        nsCAutoString s("security.principal.certificate");
        s += mCapabilitiesOrdinal++;
        mPrefName = s.ToNewCString();
    }
    *aName = nsCRT::strdup(mPrefName);
    if (!*aName)
        return NS_ERROR_FAILURE;
    return nsBasePrincipal::ToStreamableForm(aName, aData);
}

NS_IMETHODIMP
nsCertificatePrincipal::Equals(nsIPrincipal * other, PRBool * result)
{
    *result = PR_FALSE;
    if (this == other) {
        *result = PR_TRUE;
        return NS_OK;
    }
    if (!other)
        return NS_OK;
    nsresult rv;
    nsCOMPtr<nsICertificatePrincipal> otherCertificate = 
        do_QueryInterface(other, &rv);
    if (NS_FAILED(rv))
        return NS_OK;
    //-- Compare cert ID's
    char* otherID;
    rv = otherCertificate->GetCertificateID(&otherID);
    if (NS_FAILED(rv)) 
    {
        PR_FREEIF(otherID);
        return rv;
    }
    *result = (PL_strcmp(mCertificateID, otherID) == 0);
    PR_FREEIF(otherID);
    return NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::HashValue(PRUint32 *result)
{
    char* str;
    if (NS_FAILED(ToString(&str)) || !str) return NS_ERROR_FAILURE;
    *result = nsCRT::HashValue(str);
    nsCRT::free(str);
    return NS_OK;
}

/////////////////////////////////////////////
// Constructor, Destructor, initialization //
/////////////////////////////////////////////
NS_IMETHODIMP
nsCertificatePrincipal::InitFromPersistent(const char *name, const char* data)
{
    // Parses preference strings of the form 
    // <certificateID><space><capabilities list>"
    // ie. "AB:CD:12:34 UniversalBrowserRead=Granted"

    if (!data)
        return NS_ERROR_ILLEGAL_VALUE;

    char* idEnd = PL_strchr(data, ' '); // Find end of certID
    if (idEnd)
        *idEnd = '\0';

    if (NS_FAILED(Init(data))) 
        return NS_ERROR_FAILURE;
    
    if (idEnd)
    {
        data = idEnd+1;
        while (*data == ' ')
            data++;
        if (data)
            return nsBasePrincipal::InitFromPersistent(name, data);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::Init(const char* aCertificateID)
{
    mCertificateID = nsCRT::strdup(aCertificateID);
    if (!mCertificateID) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

nsCertificatePrincipal::nsCertificatePrincipal() : mCertificateID(nsnull),
                                                   mCommonName(nsnull)
{
    NS_INIT_ISUPPORTS();
}

nsCertificatePrincipal::~nsCertificatePrincipal()
{
    PR_FREEIF(mCertificateID);
    PR_FREEIF(mCommonName);
}
