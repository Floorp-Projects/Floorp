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

#include "nsCertificatePrincipal.h"

static NS_DEFINE_IID(kICertificatePrincipalIID, NS_ICERTIFICATEPRINCIPAL_IID);

NS_IMPL_ISUPPORTS(nsCertificatePrincipal, kICertificatePrincipalIID);

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
nsCertificatePrincipal::GetType(PRInt16 * type)
{
	type = & this->itsType;
	return NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::IsSecure(PRBool * result)
{
	*result =  (this->itsType == (PRInt16)nsIPrincipal::PrincipalType_Unknown) ? PR_FALSE : PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP 
nsCertificatePrincipal::ToString(char **result)
{
	return NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::HashCode(PRUint32 * code) 
{
	code=0;
	return NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::Equals(nsIPrincipal * other, PRBool * result)
{
	return NS_OK;
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
	/*
   m_pNSPrincipal = new nsPrincipal(nsPrincipalType_CertChain, certChain, 
                                    certChainLengths, noOfCerts);
   if(m_pNSPrincipal == NULL)
   {
      *result = NS_ERROR_OUT_OF_MEMORY;
      return;
   }
   *result = NS_OK;
   */
}

nsCertificatePrincipal::~nsCertificatePrincipal(void)
{
}
