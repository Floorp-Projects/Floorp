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

static NS_DEFINE_IID(kICertificatePrincipalIID, NS_ICERTIFICATEPRINCIPAL_IID);

NS_IMPL_QUERY_INTERFACE2(nsCertificatePrincipal, nsICertificatePrincipal, nsIPrincipal)

NSBASEPRINCIPALS_ADDREF(nsCertificatePrincipal);
NSBASEPRINCIPALS_RELEASE(nsCertificatePrincipal);

NS_IMETHODIMP 
nsCertificatePrincipal::CanEnableCapability(const char *capability,
                                            PRInt16 *result)
{
    // XXX: query database as to whether this principal has this capability enabled
    *result = nsIPrincipal::ENABLE_DENIED;
    return NS_OK;
}

NS_IMETHODIMP 
nsCertificatePrincipal::SetCanEnableCapability(const char *capability, 
                                               PRInt16 canEnable)
{
    // XXX: modify database as to whether this principal has this capability enabled
    return NS_ERROR_FAILURE;
}


// Unclear if we need any of these methods, and if so, where they should live.
NS_IMETHODIMP
nsCertificatePrincipal::GetPublicKey(char ** publicKey)
{
	* publicKey = (char *)this->itsKey;
	return (itsKey == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetCompanyName(char * * companyName)
{
	companyName = & this->itsCompanyName;
	return (itsCompanyName == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetCertificateAuthority(char * * certificateAuthority)
{
	certificateAuthority = & this->itsCertificateAuthority;
	return (itsCertificateAuthority == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetSerialNumber(char * * serialNumber)
{
	serialNumber = & this->itsSerialNumber;
	return (itsSerialNumber == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetExpirationDate(char * * expirationDate)
{
	expirationDate = & this->itsExpirationDate;
	return (itsExpirationDate == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetFingerPrint(char * * fingerPrint)
{
	fingerPrint = & this->itsFingerPrint;
	return (itsFingerPrint == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}


NS_IMETHODIMP 
nsCertificatePrincipal::ToString(char **result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCertificatePrincipal::Equals(nsIPrincipal * other, PRBool * result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCertificatePrincipal::HashValue(PRUint32 *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsCertificatePrincipal::nsCertificatePrincipal(PRInt16 type, const char * key)
{
	this->itsType = type;
	this->itsKey = key;
}

nsCertificatePrincipal::nsCertificatePrincipal(PRInt16 type, const unsigned char **certChain, 
								PRUint32 *certChainLengths, PRUint32 noOfCerts)
{
	this->itsType = type;
}

nsCertificatePrincipal::~nsCertificatePrincipal(void)
{
}
