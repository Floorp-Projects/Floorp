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

#ifndef nsICertPrincipal_h___
#define nsICertPrincipal_h___

#include "nsIPrincipal.h"

enum { nsFingerPrint_Size = 16 };
typedef unsigned char nsFingerPrint[nsFingerPrint_Size];

class nsICertPrincipal : public nsIPrincipal {
public:

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
    GetCertData(const unsigned char ***certChain, PRUint32 **certChainLengths, PRUint32 *noOfCerts) = 0;

    /**
     * Returns the public key of the certificate.
     *
     * @param publicKey -     the Public Key data will be returned in this field.
     * @param publicKeySize - the length of public key data is returned in this
     *                        parameter.
     */
    NS_IMETHOD
    GetPublicKey(unsigned char **publicKey, PRUint32 *publicKeySize) = 0;

    /**
     * Returns the company name of the ceritificate (OU etc parameters of certificate)
     *
     * @param result - the certificate details about the signer.
     */
    NS_IMETHOD
    GetCompanyName(const char **ppCompanyName) = 0;

    /**
     * Returns the certificate issuer's data (OU etc parameters of certificate)
     *
     * @param result - the details about the issuer
     */
    NS_IMETHOD
    GetCertificateAuthority(const char **ppCertAuthority) = 0;

    /**
     * Returns the serial number of certificate 
     *
     * @param result - Returns the serial number of certificate 
     */
    NS_IMETHOD
    GetSerialNumber(const char **ppSerialNumber) = 0;

    /**
     * Returns the expiration date of certificate 
     *
     * @param result - Returns the expiration date of certificate 
     */
    NS_IMETHOD
    GetExpirationDate(const char **ppExpDate) = 0;

    /**
     * Returns the finger print of certificate 
     *
     * @param result - Returns the finger print of certificate 
     */
    NS_IMETHOD
    GetFingerPrint(const char **ppFingerPrint) = 0;

};

#define NS_ICERTPRINCIPAL_IID                        \
{ /* ebfefcd0-25e1-11d2-8160-006008119d7a */         \
    0xebfefcd0,                                      \
    0x25e1,                                          \
    0x11d2,                                          \
    {0x81, 0x60, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#endif // nsICertPrincipal_h___
