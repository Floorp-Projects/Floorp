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
nsCertificatePrincipal::GetIssuerName(char ** issuerName)
{
    *issuerName = nsCRT::strdup(mIssuerName);
	return *issuerName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetSerialNumber(char ** serialNumber)
{
    *serialNumber = nsCRT::strdup(mSerialNumber);
	return *serialNumber ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetCompanyName(char ** companyName)
{
    *companyName = nsCRT::strdup(mCompanyName);
	return * companyName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
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
    nsAutoString str;
    str += "[Certificate ";
    str += mIssuerName;
    str += ' ';
    str += mSerialNumber;
    str += ']';
    *result = str.ToNewCString();
    return (*result) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP 
nsCertificatePrincipal::ToUserVisibleString(char **result)
{
    return GetCompanyName(result);
}

NS_IMETHODIMP
nsCertificatePrincipal::Equals(nsIPrincipal * other, PRBool * result)
{
    if (this == other) {
        *result = PR_TRUE;
        return NS_OK;
    }
    if (!other) {
        *result = PR_FALSE;
        return NS_OK;
    }
    nsresult rv;
    nsCOMPtr<nsICertificatePrincipal> otherCertificate = 
        do_QueryInterface(other, &rv);
    if (NS_FAILED(rv))
    {
        *result = PR_FALSE;
        return NS_OK;
    }
    //-- Compare issuer name and serial number; 
    //   these comprise the unique id of the cert
    char* otherIssuer;
    otherCertificate->GetIssuerName(&otherIssuer);
    char* otherSerial;
    otherCertificate->GetSerialNumber(&otherSerial);
    *result = ( (PL_strcmp(mIssuerName, otherIssuer) == 0) &&
                (PL_strcmp(mSerialNumber, otherSerial) == 0) );
    PR_FREEIF(otherIssuer);
    PR_FREEIF(otherSerial);
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
    // "[Certificate Issuer Serial#] capabilities string"
    // ie. "[Certificate CertCo 12:34:AB:CD] UniversalBrowserRead=1"

    if (!data)
        return NS_ERROR_ILLEGAL_VALUE;

    data = PL_strchr(data, ' '); // Jump to issuer
    NS_ASSERTION(data, "Malformed security.principal preference");
    data += 1;

    char* wordEnd = PL_strchr(data, ' '); // Find end of issuer
    NS_ASSERTION(wordEnd, "Malformed security.principal preference");
    *wordEnd = '\0';
    const char* issuer = data;

    data = wordEnd+1; // Jump to serial#
    wordEnd = PL_strchr(data, ']'); // Find end of serial#
    NS_ASSERTION(wordEnd, "Malformed security.principal preference");
    *wordEnd = '\0';
    const char* serial = data;

    if (NS_FAILED(Init(issuer, serial, nsnull))) 
        return NS_ERROR_FAILURE;

    if (wordEnd[1] != '\0') {
        data = wordEnd+2; // Jump to beginning of caps data
        return nsBasePrincipal::InitFromPersistent(name, data);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::Init(const char* aIssuerName, const char* aSerialNumber,
                             const char* aCompanyName)
{
    mIssuerName = nsCRT::strdup(aIssuerName);
    mSerialNumber = nsCRT::strdup(aSerialNumber);
    mCompanyName = nsCRT::strdup(aCompanyName);
    if (!mIssuerName || !mSerialNumber ||
        !mCompanyName) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

nsCertificatePrincipal::nsCertificatePrincipal() : mIssuerName(nsnull),
                                                   mSerialNumber(nsnull),
                                                   mCompanyName(nsnull)
{
    NS_INIT_ISUPPORTS();
}

nsCertificatePrincipal::~nsCertificatePrincipal()
{
    PR_FREEIF(mIssuerName);
    PR_FREEIF(mSerialNumber);
    PR_FREEIF(mCompanyName);
}
