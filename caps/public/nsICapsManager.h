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

#ifndef _NS_ICAPS_MANAGER_H_
#define _NS_ICAPS_MANAGER_H_

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIPrincipal.h"
#include "nsTarget.h"
//class nsITarget;
class nsICapsSecurityCallbacks;


#define NS_ALL_PRIVILEGES         ((nsITarget*)NULL)


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
    CreateCodebasePrincipal(const char *codebaseURL, nsIPrincipal * * prin) = 0;

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
    CreateCertificatePrincipal(const unsigned char * * certChain, PRUint32 * certChainLengths, PRUint32 noOfCerts, nsIPrincipal * * prin) = 0;

    /**
     * Returns the permission for given principal and target
     *
     * @param prin   - is either certificate principal or codebase principal
     * @param target - is NS_ALL_PRIVILEGES.
     * @param state  - the return value is passed in this parameter.
     */
    NS_IMETHOD
    GetPermission(nsIPrincipal* prin, nsTarget * target, PRInt16 * privilegeState) = 0;

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
    SetPermission(nsIPrincipal* prin, nsTarget * target, PRInt16 * privilegeState) = 0;

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
    AskPermission(nsIPrincipal* prin, nsTarget * target, PRInt16 * privilegeState) = 0;

    /* 
     * All of the following methods are used by JS (the code located
     * in lib/libmocha area).
     */

    /**
     * Initializes the capabilities subsytem (ie setting the system principal, initializing
     * privilege Manager, creating the capsManager factory etc 
     *
     * @param result - is true if principal was successfully registered with the system
     */
    NS_IMETHOD
    Initialize(PRBool *result) = 0;

    /**
     * Initializes the capabilities frame walking code.
     *
     * @param aInterface - interface for calling frame walking code.
     */
    NS_IMETHOD
    InitializeFrameWalker(nsICapsSecurityCallbacks* aInterface) = 0;

    /**
     * Registers the given Principal with the system.
     *
     * @param prin   - is either certificate principal or codebase principal
     * @param result - is true if principal was successfully registered with the system
     */
//NEED TO FIGURE THIS SUCKER OUT ARIEL
//    NS_IMETHOD
//    RegisterPrincipal(nsIPrincipal* prin, PRBool *result) = 0;

    /**
     * Prompts the user if they want to grant permission for the principal located
     * at the given stack depth for the given target. 
     *
     * @param context      - is the parameter JS needs to determinte the principal 
     * @param targetName   - is the name of the target.
     * @param callerDepth  - is the depth of JS stack frame, which JS uses to determinte the 
     *                       principal 
     * @param result - is true if user has given permission for the given principal and 
     *                 target
     */
    NS_IMETHOD
    EnablePrivilege(void* context, const char* targetName, PRInt32 callerDepth, PRBool *result) = 0;

    /**
     * Returns if the user granted permission for the principal located at the given 
     * stack depth for the given target. 
     *
     * @param context     - is the parameter JS needs to determinte the principal 
     * @param targetName  - is the name of the target.
     * @param callerDepth - is the depth of JS stack frame, which JS uses to determinte the 
     *                      principal 
     * @param result - is true if user has given permission for the given principal and 
     *                 target
     */
    NS_IMETHOD
    IsPrivilegeEnabled(void* context, const char* targetName, PRInt32 callerDepth, PRBool *result) = 0;

    /**
     * Reverts the permission (granted/denied) user gave for the principal located
     * at the given stack depth for the given target. 
     *
     * @param context      - is the parameter JS needs to determinte the principal 
     * @param targetName   - is the name of the target.
     * @param callerDepth  - is the depth of JS stack frame, which JS uses to determinte the 
     *                       principal 
     * @param result - is true if user has given permission for the given principal and 
     *                 target
     */
    NS_IMETHOD
    RevertPrivilege(void* context, const char* targetName, PRInt32 callerDepth, PRBool *result) = 0;

    /**
     * Disable permissions for the principal located at the given stack depth for the 
     * given target. 
     *
     * @param context      - is the parameter JS needs to determinte the principal 
     * @param targetName   - is the name of the target.
     * @param callerDepth  - is the depth of JS stack frame, which JS uses to determinte the 
     *                       principal 
     * @param result - is true if user has given permission for the given principal and 
     *                 target
     */
    NS_IMETHOD
    DisablePrivilege(void* context, const char* targetName, PRInt32 callerDepth, PRBool *result) = 0;


    /* XXX: Some of the arguments for the following interfaces may change.
     * This is a first cut. I need to talk to joki. We should get rid of void* parameters.
     */
    NS_IMETHOD
    ComparePrincipalArray(void* prin1Array, void* prin2Array, PRInt16 * comparisonType) = 0;

    NS_IMETHOD
    IntersectPrincipalArray(void* prin1Array, void* prin2Array, void* *result) = 0;

    NS_IMETHOD
    CanExtendTrust(void* fromPrinArray, void* toPrinArray, PRBool *result) = 0;


    /* interfaces for nsIPrincipal object, may be we should move some of them to nsIprincipal */
/*
    NS_IMETHOD
    NewPrincipal(nsPrincipalType type, void* key, PRUint32 key_len, void *zig, nsIPrincipal* *result) = 0;
*/
    NS_IMETHOD
    CreateMixedPrincipalArray(void *zig, char* name, const char* codebase, void** result) = 0;

    NS_IMETHOD
    NewPrincipalArray(PRUint32 count, void* *result) = 0;

    NS_IMETHOD
    FreePrincipalArray(void *prinArray) = 0;

    NS_IMETHOD
    GetPrincipalArrayElement(void *prinArrayArg, PRUint32 index, nsIPrincipal* *result) = 0;

    NS_IMETHOD
    SetPrincipalArrayElement(void *prinArrayArg, PRUint32 index, nsIPrincipal* principal) = 0;

    NS_IMETHOD
    GetPrincipalArraySize(void *prinArrayArg, PRUint32 *result) = 0;

    /* The following interfaces will replace all of the following old calls.
     *
     * nsCapsGetPermission(struct nsPrivilege *privilege)
     * nsCapsGetPrivilege(struct nsPrivilegeTable *annotation, struct nsTarget *target)
     *
     */
    NS_IMETHOD
    IsAllowed(void *annotation, char* target, PRBool *result) = 0;

    /* XXX: TODO: We need to set up the JS frame walking callbacks */

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
}
#endif // nsICapsManager_h___
