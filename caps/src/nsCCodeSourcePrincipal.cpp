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

#include "nsCCodeSourcePrincipal.h"
#include "nsCCodebasePrincipal.h"
#include "nsCCertPrincipal.h"
#include "nsPrincipal.h"

NS_DEFINE_IID(kICodeSourcePrincipalIID, NS_ICODESOURCEPRINCIPAL_IID);

////////////////////////////////////////////////////////////////////////////
// from nsISupports:

// These macros produce simple version of QueryInterface and AddRef.
// See the nsISupports.h header file for DETAILS.

NS_IMPL_ADDREF(nsCCodeSourcePrincipal)
NS_IMPL_RELEASE(nsCCodeSourcePrincipal)
NS_IMPL_QUERY_INTERFACE(nsCCodeSourcePrincipal, kICodeSourcePrincipalIID);

////////////////////////////////////////////////////////////////////////////
// from nsIPrincipal:

NS_METHOD
nsCCodeSourcePrincipal::IsTrusted(char* scope, PRBool *pbIsTrusted)
{
   if(m_pNSICertPrincipal == NULL) 
   {
      if(m_pNSICodebasePrincipal == NULL)
      {
         *pbIsTrusted = PR_FALSE;
         return NS_ERROR_ILLEGAL_VALUE;
      }
      else 
      {
         return m_pNSICodebasePrincipal->IsTrusted(scope, pbIsTrusted);
      }
   }
   return m_pNSICertPrincipal->IsTrusted(scope, pbIsTrusted);
}
     
////////////////////////////////////////////////////////////////////////////
// from nsICodeSourcePrincipal:

/**
 * returns the certificate's data that is passes in via Initialize method.
 *
 * @param certByteData     - The ceritificate's byte array data including the chain.
 * @param certByteDataSize - the length of certificate byte array.
 */
NS_METHOD
nsCCodeSourcePrincipal::GetCertData(const unsigned char ***certChain, 
                                    PRUint32 **certChainLengths, 
                                    PRUint32 *noOfCerts)
{
    *certChain     = NULL;
    *certChainLengths = 0;
    *noOfCerts = 0;
    if(m_pNSICertPrincipal == NULL)
    {
        return NS_ERROR_ILLEGAL_VALUE;
    }
    /* XXX: Raman fix it. Return the correct data */
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
nsCCodeSourcePrincipal::GetPublicKey(unsigned char **publicKey, 
                                     PRUint32 *publicKeySize)
{
   if(m_pNSICertPrincipal == NULL)
   {
      *publicKey     = NULL;
      *publicKeySize = 0;
      return NS_ERROR_ILLEGAL_VALUE;
   }
  	return m_pNSICertPrincipal->GetPublicKey(publicKey, publicKeySize);
}

/**
 * Returns the company name of the ceritificate (OU etc parameters of certificate)
 *
 * @param result - the certificate details about the signer.
 */
NS_METHOD
nsCCodeSourcePrincipal::GetCompanyName(const char **ppCompanyName)
{
   if(m_pNSICertPrincipal == NULL)
   {
      *ppCompanyName = NULL;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   return m_pNSICertPrincipal->GetCompanyName(ppCompanyName);
}

/**
 * Returns the certificate issuer's data (OU etc parameters of certificate)
 *
 * @param result - the details about the issuer
 */
NS_METHOD
nsCCodeSourcePrincipal::GetCertificateAuthority(const char **ppCertAuthority)
{
   if(m_pNSICertPrincipal == NULL)
   {
      *ppCertAuthority = NULL;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   return m_pNSICertPrincipal->GetCertificateAuthority(ppCertAuthority);
}

/**
 * Returns the serial number of certificate 
 *
 * @param result - Returns the serial number of certificate 
 */
NS_METHOD
nsCCodeSourcePrincipal::GetSerialNumber(const char **ppSerialNumber)
{
   if(m_pNSICertPrincipal == NULL)
   {
      *ppSerialNumber = NULL;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   return m_pNSICertPrincipal->GetSerialNumber(ppSerialNumber);
}

/**
 * Returns the expiration date of certificate 
 *
 * @param result - Returns the expiration date of certificate 
 */
NS_METHOD
nsCCodeSourcePrincipal::GetExpirationDate(const char **ppExpDate)
{
   if(m_pNSICertPrincipal == NULL)
   {
      *ppExpDate = NULL;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   return m_pNSICertPrincipal->GetExpirationDate(ppExpDate);
}

/**
 * Returns the finger print of certificate 
 *
 * @param result - Returns the finger print of certificate 
 */
NS_METHOD
nsCCodeSourcePrincipal::GetFingerPrint(const char **ppFingerPrint)
{
   if(m_pNSICertPrincipal == NULL)
   {
      *ppFingerPrint = NULL;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   return m_pNSICertPrincipal->GetFingerPrint(ppFingerPrint);
}


/**
 * Returns the codebase URL of the principal.
 *
 * @param result - the resulting codebase URL
 */
NS_METHOD
nsCCodeSourcePrincipal::GetURL(const char **ppCodeBaseURL)
{
   if(m_pNSICodebasePrincipal == NULL)
   {
      *ppCodeBaseURL = NULL;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   return m_pNSICodebasePrincipal->GetURL(ppCodeBaseURL);
}


////////////////////////////////////////////////////////////////////////////
// from nsCCodeSourcePrincipal:

nsCCodeSourcePrincipal::nsCCodeSourcePrincipal(const unsigned char **certChain, 
                                               PRUint32 *certChainLengths, 
                                               PRUint32 noOfCerts, 
                                               const char *codebaseURL, 
                                               nsresult *result)
{
   *result = NS_OK;
   nsCCertPrincipal *pNSCCertPrincipal = 
       new nsCCertPrincipal(certChain, certChainLengths, noOfCerts, result);
   if (pNSCCertPrincipal == NULL)
   {
      return;
   }
   m_pNSICertPrincipal = (nsICertPrincipal *)pNSCCertPrincipal;
   m_pNSICertPrincipal->AddRef();
 

   nsCCodebasePrincipal *pNSCCodebasePrincipal = 
       new nsCCodebasePrincipal(codebaseURL, result);
   if (pNSCCodebasePrincipal == NULL)
   {
      return;
   }
   m_pNSICodebasePrincipal = (nsICodebasePrincipal *)pNSCCodebasePrincipal;
   m_pNSICodebasePrincipal->AddRef();

}

nsCCodeSourcePrincipal::~nsCCodeSourcePrincipal(void)
{
   m_pNSICertPrincipal->Release();
   m_pNSICodebasePrincipal->Release();
}



