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
#include "nsIPrincipalManager.h"
#include "nsIPrivilegeManager.h"
#include "nsIPrivilege.h"
#include "nsISupports.h"
#include "nsICapsManager.h"
#include "nsAgg.h"
#include "nsPrivilegeManager.h"
#include "nsPrincipalManager.h"

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

NS_DECL_ISUPPORTS

nsPrivilegeManager * thePrivilegeManager;
nsPrincipalManager * thePrincipalManager;

NS_IMETHOD
GetPrincipalManager(nsIPrincipalManager * * prinMan);

NS_IMETHOD
GetPrivilegeManager(nsIPrivilegeManager * * privMan);

NS_IMETHOD
CreateCodebasePrincipal(const char *codebaseURL, nsIPrincipal** prin);

NS_IMETHOD
CreateCertificatePrincipal(const unsigned char **certChain, PRUint32 *certChainLengths, PRUint32 noOfCerts, nsIPrincipal** prin);

NS_IMETHOD
GetPermission(nsIPrincipal * prin, nsITarget * target, PRInt16 * privilegeState);

NS_IMETHOD
SetPermission(nsIPrincipal * prin, nsITarget* target, PRInt16 privilegeState);

NS_IMETHOD
AskPermission(nsIPrincipal * prin, nsITarget * target, PRInt16 * privilegeState);

NS_IMETHOD
Initialize(PRBool * result);

NS_IMETHOD
InitializeFrameWalker(nsICapsSecurityCallbacks * aInterface);

NS_IMETHOD
RegisterPrincipal(nsIPrincipal * prin);

NS_IMETHOD
EnablePrivilege(nsIScriptContext * context, const char* targetName, PRInt32 callerDepth, PRBool *result);

NS_IMETHOD
IsPrivilegeEnabled(nsIScriptContext * context, const char* targetName, PRInt32 callerDepth, PRBool *result);

NS_IMETHOD
RevertPrivilege(nsIScriptContext * context, const char* targetName, PRInt32 callerDepth, PRBool *result);

NS_IMETHOD
DisablePrivilege(nsIScriptContext * context, const char* targetName, PRInt32 callerDepth, PRBool *result);

//NS_IMETHOD
//CreateMixedPrincipalArray(void * zig, const char * name, const char* codebase, nsIPrincipalArray * * result);

NS_IMETHOD
IsAllowed(void * annotation, const char * target, PRBool * result);

static nsCCapsManager *
GetSecurityManager();

virtual ~nsCCapsManager(void);

private:

nsCCapsManager(void);

/*
void
CreateNSPrincipalArray(nsIPrincipalArray * prinArray, nsIPrincipalArray * * pPrincipalArray);

NS_METHOD
GetNSPrincipalArray(nsIPrincipalArray * prinArray, nsIPrincipalArray * * pPrincipalArray);
*/
};

#endif // nsCCapsManager_h___
