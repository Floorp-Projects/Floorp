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

#ifndef nsICapsManager_h___
#define nsICapsManager_h___

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIPrincipal.h"

class nsITarget;

#define NS_ALL_PRIVILEGES         ((nsITarget*)NULL)

enum nsPermission {
    nsPermission_Unknown,
    nsPermission_AllowedSession,
    nsPermission_DeniedSession,
    nsPermission_AllowedForever,
    nsPermission_DeniedForever
};

/**
 * The nsICapsManager interface is used to construct and manage principals.
 * Principals are either certificates, or codebase principals. To construct
 * a principal, QueryInterface on nsIPluginManager and then call CreateCertPrincipal
 * or CreateCodebasePrincipal on the returned nsICapsManager.
 * Once a principal is obtained, it can be passed to the nsICapsManager's
 * SetPermission operations.
 */
class nsICapsManager : public nsISupports {
public:
    /**
     * Intializes the principal object with the codebase URL.
     *
     * @param codebaseURL - the codebase URL
     * @param prin  - the return value is passed in this parameter.
     */
    NS_IMETHOD
    CreateCodebasePrincipal(const char *codebaseURL, nsIPrincipal** prin) = 0;

    /**
     * Initializes the Certificate principal with the certificate data. I am 
     * thinking Certificate data should contain all the data about the 
     * certificate, ie. the signer and the certificate chain.
     *
     * @param certChain        - An array of pointers, with each pointer 
     *                           pointing to a certificate data.
     * @param certChainLengths  - An array of intergers. Each integer indicates 
     *                            the length of the cert that is in CertChain 
     *                             parametr.
     * @param noOfCerts - the number of certifcates that are in the certChain array
     * @param prin  - the return value is passed in this parameter.
     */
    NS_IMETHOD
    CreateCertPrincipal(const unsigned char **certChain, PRUint32 *certChainLengths, PRUint32 noOfCerts, nsIPrincipal** prin) = 0;


    /**
     * Creates a CodeSourcePrincipal, which has both nsICodebasePrincipal 
     * and nsICertPrincipal
     *
     *
     * @param certChain        - An array of pointers, with each pointer 
     *                           pointing to a certificate data.
     * @param certChainLengths  - An array of intergers. Each integer indicates 
     *                            the length of the cert that is in CertChain 
     *                             parametr.
     * @param noOfCerts - the number of certifcates that are in the certChain array
     * @param codebaseURL - the codebase URL
     * @param prin  - the return value is passed in this parameter.
     */
    NS_IMETHOD
    CreateCodeSourcePrincipal(const unsigned char **certChain, PRUint32 *certChainLengths, PRUint32 noOfCerts, const char *codebaseURL, nsIPrincipal** prin) = 0;


    /**
     * Returns the permission for given principal and target
     *
     * @param prin   - is either certificate principal or codebase principal
     * @param target - is NS_ALL_PRIVILEGES.
     * @param state  - the return value is passed in this parameter.
     */
    NS_IMETHOD
    GetPermission(nsIPrincipal* prin, nsITarget* target, nsPermission *state) = 0;

    /**
     * Set the permission state for given principal and target. This wouldn't 
     * prompt the end user with UI.
     *
     * @param prin   - is either certificate principal or codebase principal
     * @param target - is NS_ALL_PRIVILEGES.
     * @param state  - is permisson state that should be set for the given prin
     *                 and target parameters.
     */
    NS_IMETHOD
    SetPermission(nsIPrincipal* prin, nsITarget* target, nsPermission state) = 0;

    /**
     * Prompts the user if they want to grant permission for the given principal and 
     * for the given target. 
     *
     * @param prin   - is either certificate principal or codebase principal
     * @param target - is NS_ALL_PRIVILEGES.
     * @param result - is the permission user has given for the given principal and 
     *                 target
     */
    NS_IMETHOD
    AskPermission(nsIPrincipal* prin, nsITarget* target, nsPermission *result) = 0;

};

#define NS_ICAPSMANAGER_IID                          \
{ /* dc7d0bb0-25e1-11d2-8160-006008119d7a */         \
    0xdc7d0bb0,                                      \
    0x25e1,                                          \
    0x11d2,                                          \
    {0x81, 0x60, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#define NS_CCAPSMANAGER_CID                          \
{ /* fd347500-307f-11d2-97f0-00805f8a28d0 */         \
    0xfd347500,                                      \
    0x307f,                                          \
    0x11d2,                                          \
    {0x97, 0xf0, 0x00, 0x80, 0x5f, 0x8a, 0x28, 0xd0} \
};
#endif // nsICapsManager_h___
