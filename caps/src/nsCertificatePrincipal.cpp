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
#include "nsPrincipal.h"

static NS_DEFINE_IID(kICertificatePrincipalIID, NS_ICERTIFICATEPRINCIPAL_IID);

////////////////////////////////////////////////////////////////////////////
// from nsISupports:

// These macros produce simple version of QueryInterface and AddRef.
// See the nsISupports.h header file for DETAILS.

NS_IMPL_ISUPPORTS(nsCertificatePrincipal,kICertificatePrincipalIID);

////////////////////////////////////////////////////////////////////////////
// from nsIPrincipal:

NS_IMETHODIMP
nsCertificatePrincipal::IsTrusted(const char* scope, PRBool *pbIsTrusted)
{
   if(m_pNSPrincipal == NULL)
   {
      *pbIsTrusted = PR_FALSE;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   *pbIsTrusted = m_pNSPrincipal->isTrustedCertChainPrincipal();
   return NS_OK;
}
     
////////////////////////////////////////////////////////////////////////////
// from nsICertificatePrincipal:
/**
 * Returns the public key of the certificate.
 *
 * @param publicKey -     the Public Key data will be returned in this field.
 * @param publicKeySize - the length of public key data is returned in this
 *                        parameter.
 */
NS_IMETHODIMP
nsCertificatePrincipal::GetPublicKey(char **publicKey, PRUint32 *publicKeySize)
{
   // XXX raman: fix it.
   PR_ASSERT(PR_FALSE);
   return NS_OK;
}

/**
 * Returns the company name of the ceritificate (OU etc parameters of certificate)
 *
 * @param result - the certificate details about the signer.
 */
NS_IMETHODIMP
nsCertificatePrincipal::GetCompanyName(char **ppCompanyName)
{
   if(m_pNSPrincipal == NULL)
   {
      *ppCompanyName = NULL;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   *ppCompanyName = m_pNSPrincipal->getCompanyName();
   return NS_OK;
}

/**
 * Returns the certificate issuer's data (OU etc parameters of certificate)
 *
 * @param result - the details about the issuer
 */
NS_IMETHODIMP
nsCertificatePrincipal::GetCertificateAuthority(char **ppCertAuthority)
{
   if(m_pNSPrincipal == NULL)
   {
      *ppCertAuthority = NULL;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   *ppCertAuthority = m_pNSPrincipal->getSecAuth();
   return NS_OK;
}

/**
 * Returns the serial number of certificate 
 *
 * @param result - Returns the serial number of certificate 
 */
NS_IMETHODIMP
nsCertificatePrincipal::GetSerialNumber(char **ppSerialNumber)
{
   if(m_pNSPrincipal == NULL)
   {
      *ppSerialNumber = NULL;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   *ppSerialNumber = m_pNSPrincipal->getSerialNo();
   return NS_OK;
}

/**
 * Returns the expiration date of certificate 
 *
 * @param result - Returns the expiration date of certificate 
 */
NS_IMETHODIMP
nsCertificatePrincipal::GetExpirationDate(char **ppExpDate)
{
   if(m_pNSPrincipal == NULL)
   {
      *ppExpDate = NULL;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   *ppExpDate = m_pNSPrincipal->getExpDate();
   return NS_OK;
}

/**
 * Returns the finger print of certificate 
 *
 * @param result - Returns the finger print of certificate 
 */
NS_IMETHODIMP
nsCertificatePrincipal::GetFingerPrint(char **ppFingerPrint)
{
   if(m_pNSPrincipal == NULL)
   {
      *ppFingerPrint = NULL;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   *ppFingerPrint = m_pNSPrincipal->getFingerPrint();
   return NS_OK;
}

////////////////////////////////////////////////////////////////////////////
// from nsCertificatePrincipal:

nsCertificatePrincipal::nsCertificatePrincipal(const unsigned char **certChain, 
                                   PRUint32 *certChainLengths, 
                                   PRUint32 noOfCerts, 
                                   nsresult *result)
{
   m_pNSPrincipal = new nsPrincipal(nsPrincipalType_CertChain, certChain, 
                                    certChainLengths, noOfCerts);
   if(m_pNSPrincipal == NULL)
   {
      *result = NS_ERROR_OUT_OF_MEMORY;
      return;
   }
   *result = NS_OK;
}

nsCertificatePrincipal::nsCertificatePrincipal(nsPrincipal *pNSPrincipal)
{
   m_pNSPrincipal = pNSPrincipal;
}

nsCertificatePrincipal::~nsCertificatePrincipal(void)
{
   delete m_pNSPrincipal;
}

nsPrincipal*
nsCertificatePrincipal::GetPeer(void)
{
   return m_pNSPrincipal;
}