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

#ifndef _NS_CCAPS_MANAGER_H_
#define _NS_CCAPS_MANAGER_H_

#include "nsIPrincipal.h"
#include "nsIPrivilege.h"
#include "nsISupports.h"
#include "nsICapsManager.h"
#include "nsAgg.h"
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
CreateCertificatePrincipal(const unsigned char **certChain, PRUint32 *certChainLengths, PRUint32 noOfCerts, nsIPrincipal** prin);

/**
 * Creates a CodeSourcePrincipal, which has both nsICodebasePrincipal 
 * and nsICertPrincipal
 *
 * @param certByteData     - The ceritificate's byte array data including the chain.
 * @param certByteDataSize - the length of certificate byte array.
 * @param codebaseURL - the codebase URL
 */
/*
NS_IMETHOD
CreateCodeSourcePrincipal(const unsigned char **certChain, PRUint32 *certChainLengths, PRUint32 noOfCerts, const char *codebaseURL, nsIPrincipal** prin);
*/

/**
 * Returns the permission for given principal and target
 *
 * @param prin   - is either certificate principal or codebase principal
 * @param target - is NS_ALL_PRIVILEGES.
 * @param state  - the return value is passed in this parameter.
 */
NS_IMETHOD
GetPermission(nsIPrincipal * prin, nsITarget * target, PRInt16 * privilegeState);

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
SetPermission(nsIPrincipal * prin, nsITarget* target, PRInt16 * privilegeState);

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
AskPermission(nsIPrincipal * prin, nsITarget * target, PRInt16 * privilegeState);



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
Initialize(PRBool * result);

NS_IMETHOD
InitializeFrameWalker(nsICapsSecurityCallbacks * aInterface);

/**
 * Registers the given Principal with the system.
 *
 * @param prin   - is either certificate principal or codebase principal
 * @param result - is true if principal was successfully registered with the system
 */
NS_IMETHOD
RegisterPrincipal(nsIPrincipal * prin);

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
EnablePrivilege(void* context, const char* targetName, PRInt32 callerDepth, PRBool *result);

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
IsPrivilegeEnabled(void* context, const char* targetName, PRInt32 callerDepth, PRBool *result);

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
RevertPrivilege(void* context, const char* targetName, PRInt32 callerDepth, PRBool *result);

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
DisablePrivilege(void* context, const char* targetName, PRInt32 callerDepth, PRBool *result);


/* XXX: Some of the arguments for the following interfaces may change.
 * This is a first cut. I need to talk to joki. We should get rid of void* parameters.
 */
NS_IMETHOD
ComparePrincipalArray(void* prin1Array, void* prin2Array, PRInt16 * comparisonType);

NS_IMETHOD
IntersectPrincipalArray(void* prin1Array, void* prin2Array, void* *result);

NS_IMETHOD
CanExtendTrust(void* fromPrinArray, void* toPrinArray, PRBool *result);

NS_IMETHOD
CreateMixedPrincipalArray(void *zig, char* name, const char* codebase, void** result);

NS_IMETHOD
NewPrincipalArray(PRUint32 count, void* *result);

NS_IMETHOD
FreePrincipalArray(void *prinArray);

NS_IMETHOD
GetPrincipalArrayElement(void *prinArrayArg, PRUint32 index, nsIPrincipal * * result);

NS_IMETHOD
SetPrincipalArrayElement(void *prinArrayArg, PRUint32 index, nsIPrincipal * principal);

NS_IMETHOD
GetPrincipalArraySize(void *prinArrayArg, PRUint32 *result);

/* The following interfaces will replace all of the following old calls.
 *
 * nsCapsGetPermission(struct nsPrivilege *privilege)
 * nsCapsGetPrivilege(struct nsPrivilegeTable *annotation, struct nsITarget *target)
 *
 */
NS_IMETHOD
IsAllowed(void *annotation, char* target, PRBool *result);

////////////////////////////////////////////////////////////////////////////
// from nsCCapsManager:

nsCCapsManager(nsISupports *aOuter);
virtual ~nsCCapsManager(void);

void
CreateNSPrincipalArray(nsPrincipalArray* prinArray, nsPrincipalArray* *pPrincipalArray);
/*
NS_METHOD
GetNSPrincipal(nsIPrincipal * pNSIPrincipal, nsIPrincipal ** ppNSPRincipal);
*/
NS_METHOD
GetNSPrincipalArray(nsPrincipalArray* prinArray, nsPrincipalArray* *pPrincipalArray);

void
SetSystemPrivilegeManager();

protected:
    nsPrivilegeManager * privilegeManager;  
};

#endif // nsCCapsManager_h___
