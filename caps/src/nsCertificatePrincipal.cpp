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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/*describes principals for use in signed scripts*/
#include "nsCertificatePrincipal.h"
#include "nsCOMPtr.h"

static NS_DEFINE_IID(kICertificatePrincipalIID, NS_ICERTIFICATEPRINCIPAL_IID);

NS_IMPL_QUERY_INTERFACE2(nsCertificatePrincipal, nsICertificatePrincipal, nsIPrincipal)

NSBASEPRINCIPALS_ADDREF(nsCertificatePrincipal);
NSBASEPRINCIPALS_RELEASE(nsCertificatePrincipal);

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
nsCertificatePrincipal::GetIssuerName(char ** issuerName)
{
    *issuerName = (char*)mIssuerName;
	return NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetSerialNumber(char ** serialNumber)
{
    *serialNumber = (char*)mSerialNumber;
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
nsCertificatePrincipal::Equals(nsIPrincipal * other, PRBool * result)
{
    if (this == other) {
        *result = PR_TRUE;
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
    char* otherIssuer;
    otherCertificate->GetIssuerName(&otherIssuer);
    char* otherSerial;
    otherCertificate->GetSerialNumber(&otherSerial);
    *result = ( (PL_strcmp(mIssuerName, otherIssuer) == 0) &&
                (PL_strcmp(mSerialNumber, otherSerial) == 0) );
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

NS_IMETHODIMP
nsCertificatePrincipal::Init(const char* data)
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

    if(NS_FAILED(Init(issuer, serial))) return NS_ERROR_FAILURE;

    if (wordEnd[1] != '\0')
    {
        data = wordEnd+2; // Jump to beginning of caps data
        return nsBasePrincipal::Init(data);
    }
    else
        return NS_OK;
 }

NS_IMETHODIMP
nsCertificatePrincipal::Init(const char* aIssuerName, const char* aSerialNumber)
{
    mIssuerName = nsCRT::strdup(aIssuerName);
    mSerialNumber = nsCRT::strdup(aSerialNumber);
    if (!mIssuerName || !mSerialNumber) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

nsCertificatePrincipal::nsCertificatePrincipal()
{
    NS_INIT_ISUPPORTS();
}

nsCertificatePrincipal::~nsCertificatePrincipal(void)
{
    if (mIssuerName)
        nsCRT::free((char*)mIssuerName);
    if (mSerialNumber)
        nsCRT::free((char*)mSerialNumber);
}
