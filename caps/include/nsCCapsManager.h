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

#ifndef nsCCapsManager_h___
#define nsCCapsManager_h___

#include "nsIPrincipal.h"
#include "nsISupports.h"
#include "nsICapsManager.h"
#include "nsAgg.h"

#include "nsPrincipal.h"
#include "nsPrivilegeManager.h"

/**
 * nsCCapsManager implements the nsICapsManager 
 * interface for navigator. This is used to create new principals and get and
 * set permissions for each of the principals created.
 * nsCCapsManager registers itself as the factory object and it will always return
 * a pointer to itself as in the createInstance. This is because there should be only
 * one nsCCapsManager in navigator. 
 */
class nsCCapsManager : public nsICapsManager {
public:
    ////////////////////////////////////////////////////////////////////////////
    // from nsISupports and AggregatedQueryInterface:

    NS_DECL_AGGREGATED

    ////////////////////////////////////////////////////////////////////////////
    // from nsICapsManager:

    NS_IMETHOD
    CreateCodebasePrincipal(const char *codebaseURL, nsIPrincipal** prin);

    NS_IMETHOD
    CreateCertPrincipal(const unsigned char **certChain, PRUint32 *certChainLengths, PRUint32 noOfCerts, nsIPrincipal** prin);

    /**
     * Creates a CodeSourcePrincipal, which has both nsICodebasePrincipal 
     * and nsICertPrincipal
     *
     * @param certByteData     - The ceritificate's byte array data including the chain.
     * @param certByteDataSize - the length of certificate byte array.
     * @param codebaseURL - the codebase URL
     */
    NS_IMETHOD
    CreateCodeSourcePrincipal(const unsigned char **certChain, PRUint32 *certChainLengths, PRUint32 noOfCerts, const char *codebaseURL, nsIPrincipal** prin);


    /**
     * Returns the permission for given principal and target
     *
     * @param prin   - is either certificate principal or codebase principal
     * @param target - is NS_ALL_PRIVILEGES.
     * @param state  - the return value is passed in this parameter.
     */
    NS_IMETHOD
    GetPermission(nsIPrincipal* prin, nsITarget* target, nsPermission *state);

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
    SetPermission(nsIPrincipal* prin, nsITarget* target, nsPermission state);

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
    AskPermission(nsIPrincipal* prin, nsITarget* target, nsPermission *result);

    ////////////////////////////////////////////////////////////////////////////
    // from nsCCapsManager:

    nsCCapsManager(nsISupports *aOuter);
    virtual ~nsCCapsManager(void);
    NS_METHOD
    GetNSPrincipal(nsIPrincipal* pNSIPrincipal, nsPrincipal **ppNSPRincipal);

    nsPermission
    ConvertPrivilegeToPermission(nsPrivilege *pNSPrivilege);

    nsPrivilege *
    ConvertPermissionToPrivilege(nsPermission state);

    void
    SetSystemPrivilegeManager();


protected:
    nsPrivilegeManager    *m_pNSPrivilegeManager;  
};

#endif // nsCCapsManager_h___
