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

#include "nsCCertPrincipal.h"
#include "nsPrincipal.h"

NS_DEFINE_IID(kICertPrincipalIID, NS_ICERTPRINCIPAL_IID);

////////////////////////////////////////////////////////////////////////////
// from nsISupports:

// These macros produce simple version of QueryInterface and AddRef.
// See the nsISupports.h header file for DETAILS.

NS_IMPL_ADDREF(nsCCertPrincipal)
NS_IMPL_RELEASE(nsCCertPrincipal)
NS_IMPL_QUERY_INTERFACE(nsCCertPrincipal, kICertPrincipalIID);

////////////////////////////////////////////////////////////////////////////
// from nsIPrincipal:

NS_METHOD
nsCCertPrincipal::IsTrusted(char* scope, PRBool *pbIsTrusted)
{
   if(m_pNSPrincipal == NULL)
   {
      *pbIsTrusted = PR_FALSE;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   *pbIsTrusted = m_pNSPrincipal->isSecurePrincipal();
   return NS_OK;
}
     
////////////////////////////////////////////////////////////////////////////
// from nsICertPrincipal:

/**
 * returns the certificate's data that is passes in via Initialize method.
 *
 * @param certByteData     - The ceritificate's byte array data including the chain.
 * @param certByteDataSize - the length of certificate byte array.
 */
NS_METHOD
nsCCertPrincipal::GetCertData(unsigned char **certByteData, PRUint32 *certByteDataSize)
{
   if(m_pNSPrincipal == NULL)
   {
      *certByteData     = NULL;
      *certByteDataSize = 0;
      return NS_ERROR_ILLEGAL_VALUE;
   }
	  *certByteData     = (unsigned char *)m_pNSPrincipal->getKey();
  	*certByteDataSize = m_pNSPrincipal->getKeyLength();

   return NS_OK;
}

/**
 * Returns the public key of the certificate.
 *
 * @param publicKey -     the Public Key data will be returned in this field.
 * @param publicKeySize - the length of public key data is returned in this
 *                        parameter.
 */
NS_METHOD
nsCCertPrincipal::GetPublicKey(unsigned char **publicKey, PRUint32 *publicKeySize)
{
   // =-= sudu/raman: fix it.
   PR_ASSERT(PR_FALSE);
   return NS_OK;
}

/**
 * Returns the company name of the ceritificate (OU etc parameters of certificate)
 *
 * @param result - the certificate details about the signer.
 */
NS_METHOD
nsCCertPrincipal::GetCompanyName(const char **ppCompanyName)
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
NS_METHOD
nsCCertPrincipal::GetCertificateAuthority(const char **ppCertAuthority)
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
NS_METHOD
nsCCertPrincipal::GetSerialNumber(const char **ppSerialNumber)
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
NS_METHOD
nsCCertPrincipal::GetExpirationDate(const char **ppExpDate)
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
NS_METHOD
nsCCertPrincipal::GetFingerPrint(const char **ppFingerPrint)
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
// from nsCCertPrincipal:

nsCCertPrincipal::nsCCertPrincipal(const unsigned char *certByteData, PRUint32 certByteDataSize, nsresult *result)
{
   // XXX raman fix the error condition.
   m_pNSPrincipal = new nsPrincipal(nsPrincipalType_Cert, (void *)certByteData, certByteDataSize);
   if(m_pNSPrincipal == NULL)
   {
      *result = NS_ERROR_OUT_OF_MEMORY;
      return;
   }
   *result = NS_OK;
}

nsCCertPrincipal::~nsCCertPrincipal(void)
{
   delete m_pNSPrincipal;
}

nsPrincipal*
nsCCertPrincipal::GetPeer(void)
{
   return m_pNSPrincipal;
}


