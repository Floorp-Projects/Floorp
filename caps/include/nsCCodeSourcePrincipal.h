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

#ifndef nsCCodeSourcePrincipal_h___
#define nsCCodeSourcePrincipal_h___
#include "nsICodebasePrincipal.h"
#include "nsICertificatePrincipal.h"
#include "nsICodeSourcePrincipal.h"
#include "nsPrincipal.h"

class nsCCodeSourcePrincipal : public nsICodeSourcePrincipal {
public:
    ////////////////////////////////////////////////////////////////////////////
    // from nsISupports:


    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPrincipal:

    NS_IMETHOD
    IsTrusted(const char* scope, PRBool *pbIsTrusted);
     
    ////////////////////////////////////////////////////////////////////////////
    // from nsICodeSourcePrincipal:

    /**
     * Returns the public key of the certificate.
     *
     * @param publicKey -     the Public Key data will be returned in this field.
     * @param publicKeySize - the length of public key data is returned in this
     *                        parameter.
     */
    NS_IMETHOD
    GetPublicKey(char **publicKey, PRUint32 *publicKeySize);

    /**
     * Returns the company name of the ceritificate (OU etc parameters of certificate)
     *
     * @param result - the certificate details about the signer.
     */
    NS_IMETHOD
    GetCompanyName(char **ppCompanyName);

    /**
     * Returns the certificate issuer's data (OU etc parameters of certificate)
     *
     * @param result - the details about the issuer
     */
    NS_IMETHOD
    GetCertificateAuthority(char **ppCertAuthority);

    /**
     * Returns the serial number of certificate 
     *
     * @param result - Returns the serial number of certificate 
     */
    NS_IMETHOD
    GetSerialNumber(char **ppSerialNumber);

    /**
     * Returns the expiration date of certificate 
     *
     * @param result - Returns the expiration date of certificate 
     */
    NS_IMETHOD
    GetExpirationDate(char **ppExpDate);

    /**
     * Returns the finger print of certificate 
     *
     * @param result - Returns the finger print of certificate 
     */
    NS_IMETHOD
    GetFingerPrint(char **ppFingerPrint);

    /**
     * Returns the codebase URL of the principal.
     *
     * @param result - the resulting codebase URL
     */
    NS_IMETHOD
    GetURL(char **ppCodeBaseURL);

    ////////////////////////////////////////////////////////////////////////////
    // from nsCCodeSourcePrincipal:
    nsCCodeSourcePrincipal(const unsigned char **certChain, PRUint32 *certChainLengths, PRUint32 noOfCerts, const char *codebaseURL, nsresult *result);
    virtual ~nsCCodeSourcePrincipal(void);
    nsICertificatePrincipal     *GetCertPrincipal()     { return m_pNSICertPrincipal;}
    nsICodebasePrincipal *GetCodebasePrincipal() { return m_pNSICodebasePrincipal; }

protected:
    nsICertificatePrincipal     *m_pNSICertPrincipal;
    nsICodebasePrincipal *m_pNSICodebasePrincipal;
};

#endif // nsCCodeSourcePrincipal_h___
