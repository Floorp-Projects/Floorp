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

#ifndef nsCCertPrincipal_h___
#define nsCCertPrincipal_h___

#include "nsICertPrincipal.h"
#include "nsPrincipal.h"

class nsCCertPrincipal : public nsICertPrincipal {
public:
    ////////////////////////////////////////////////////////////////////////////
    // from nsISupports:


    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPrincipal:

    NS_IMETHOD
    IsTrusted(const char* scope, PRBool *pbIsTrusted);
     
    ////////////////////////////////////////////////////////////////////////////
    // from nsICertPrincipal:

    /**
     * returns the certificate's data that is passes in via Initialize method.
     *
     * @param certChain        - An array of pointers, with each pointer 
     *                           pointing to a certificate data.
     * @param certChainLengths  - An array of intergers. Each integer indicates 
     *                            the length of the cert that is in CertChain 
     *                             parametr.
     * @param noOfCerts - the number of certifcates that are in the certChain array
     */
    NS_IMETHOD
    GetCertData(const unsigned char ***certChain, PRUint32 **certChainLengths, PRUint32 *noOfCerts);

    /**
     * Returns the public key of the certificate.
     *
     * @param publicKey -     the Public Key data will be returned in this field.
     * @param publicKeySize - the length of public key data is returned in this
     *                        parameter.
     */
    NS_IMETHOD
    GetPublicKey(unsigned char **publicKey, PRUint32 *publicKeySize);

    /**
     * Returns the company name of the ceritificate (OU etc parameters of certificate)
     *
     * @param result - the certificate details about the signer.
     */
    NS_IMETHOD
    GetCompanyName(const char **ppCompanyName);

    /**
     * Returns the certificate issuer's data (OU etc parameters of certificate)
     *
     * @param result - the details about the issuer
     */
    NS_IMETHOD
    GetCertificateAuthority(const char **ppCertAuthority);

    /**
     * Returns the serial number of certificate 
     *
     * @param result - Returns the serial number of certificate 
     */
    NS_IMETHOD
    GetSerialNumber(const char **ppSerialNumber);

    /**
     * Returns the expiration date of certificate 
     *
     * @param result - Returns the expiration date of certificate 
     */
    NS_IMETHOD
    GetExpirationDate(const char **ppExpDate);

    /**
     * Returns the finger print of certificate 
     *
     * @param result - Returns the finger print of certificate 
     */
    NS_IMETHOD
    GetFingerPrint(const char **ppFingerPrint);

    ////////////////////////////////////////////////////////////////////////////
    // from nsCCertPrincipal:
    nsCCertPrincipal(const unsigned char **certChain, PRUint32 *certChainLengths, PRUint32 noOfCerts, nsresult *result);
    nsCCertPrincipal(nsPrincipal *pNSPrincipal);

    virtual ~nsCCertPrincipal(void);
    nsPrincipal *GetPeer(void);

protected:
    nsPrincipal *m_pNSPrincipal;
};

#endif // nsCCertPrincipal_h___
