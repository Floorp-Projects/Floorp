/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef _NS_PRIVILEGE_MANAGER_H_
#define _NS_PRIVILEGE_MANAGER_H_

#include "prtypes.h"
#include "prio.h"
#include "prmon.h"
#include "nsHashtable.h"

#include "nsVector.h"
#include "nsCaps.h"
#include "nsTarget.h"
#include "nsPrincipal.h"
#include "nsPrivilege.h"
#include "nsPrivilegeTable.h"
#include "nsSystemPrivilegeTable.h"
#include "nsCapsEnums.h"

extern PRBool nsCaps_lock(void);
extern void nsCaps_unlock(void);

PR_BEGIN_EXTERN_C
PRBool CMGetBoolPref(char * pref_name);
PR_END_EXTERN_C

PRBool nsPrivilegeManagerInitialize(void);

struct nsPrivilegeManager {

public:
	/* Public Methods */

	nsPrivilegeManager(void);
	virtual ~nsPrivilegeManager(void);

	void registerSystemPrincipal(nsPrincipal *principal);

	void registerPrincipal(nsPrincipal *principal);

	PRBool unregisterPrincipal(nsPrincipal *principal);

	PRBool isPrivilegeEnabled(nsTarget *target, PRInt32 callerDepth);

	PRBool isPrivilegeEnabled(void* context, nsTarget *target, 
                              PRInt32 callerDepth);

	PRBool enablePrivilege(nsTarget *target, PRInt32 callerDepth);

	PRBool enablePrivilege(void* context, nsTarget *target, PRInt32 callerDepth);

	PRBool enablePrivilege(nsTarget *target, nsPrincipal *preferredPrincipal, 
                           PRInt32 callerDepth);

	PRBool enablePrivilege(void* context, nsTarget *target, 
                           nsPrincipal *preferredPrincipal, 
                           PRInt32 callerDepth);

	PRBool revertPrivilege(nsTarget *target, PRInt32 callerDepth);

	PRBool revertPrivilege(void* context, nsTarget *target, PRInt32 callerDepth);

	PRBool disablePrivilege(nsTarget *target, PRInt32 callerDepth);

	PRBool disablePrivilege(void* context, nsTarget *target, PRInt32 callerDepth);

	PRBool enablePrincipalPrivilegeHelper(nsTarget *target, PRInt32 callerDepth, 
                                          nsPrincipal *preferredPrin, void * data,
                                          nsTarget *impersonator);

	PRBool enablePrincipalPrivilegeHelper(void* context, nsTarget *target, 
                                          PRInt32 callerDepth, 
                                          nsPrincipal *preferredPrin, 
                                          void * data, 
                                          nsTarget *impersonator);

	nsPrivilegeTable *enableScopePrivilegeHelper(nsTarget *target, 
                                                 PRInt32 callerDepth, 
                                                 void *data, 
                                                 PRBool helpingSetScopePrivilege, 
                                                 nsPrincipal *prefPrin);

	nsPrivilegeTable *enableScopePrivilegeHelper(void* context, nsTarget *target, 
                                                 PRInt32 callerDepth, void *data, 
                                                 PRBool helpingSetScopePrivilege, 
                                                 nsPrincipal *prefPrin);

	PRBool AskPermission(nsPrincipal* useThisPrin, nsTarget* target, void* data);

	void SetPermission(nsPrincipal *useThisPrin, nsTarget *target, 
                       nsPrivilege *newPrivilege);

	void registerPrincipalAndSetPrivileges(nsPrincipal *principal, 
                                           nsTarget *target, 
                                           nsPrivilege *newPrivilege);

	void updatePrivilegeTable(nsTarget *target, 
                              nsPrivilegeTable *privTable, 
                              nsPrivilege *newPrivilege);

	PRBool checkPrivilegeGranted(nsTarget *target, PRInt32 callerDepth);

	PRBool checkPrivilegeGranted(void* context, nsTarget *target, 
                                 PRInt32 callerDepth);

	PRBool checkPrivilegeGranted(nsTarget *target, nsPrincipal *principal, 
                                 void *data);

	PRBool checkPrivilegeGranted(nsTarget *target, PRInt32 callerDepth, 
                                 void *data);

	PRBool checkPrivilegeGranted(void* context, nsTarget *target, 
                                 PRInt32 callerDepth, void *data);

	nsPrivilege *getPrincipalPrivilege(nsTarget *target, nsPrincipal *prin, 
                                       void *data);

	static nsPrivilegeManager * getPrivilegeManager(void);

	static nsPrincipalArray* getMyPrincipals(PRInt32 callerDepth);

	static nsPrincipalArray* getMyPrincipals(void* context, PRInt32 callerDepth);

	static nsPrincipal * getSystemPrincipal(void);

	static PRBool hasSystemPrincipal(nsPrincipalArray *prinArray);

	static nsPrincipal* getUnsignedPrincipal(void);

	static nsPrincipal* getUnknownPrincipal(void);

	nsSetComparisonType comparePrincipalArray(nsPrincipalArray* prin1Array, 
                                              nsPrincipalArray* prin2Array);

	nsPrincipalArray* intersectPrincipalArray(nsPrincipalArray* prin1Array, 
                                              nsPrincipalArray* prin2Array);

	PRBool canExtendTrust(nsPrincipalArray* prin1Array, 
                          nsPrincipalArray* prin2Array);

	PRBool checkMatchPrincipal(nsPrincipal *principal, PRInt32 callerDepth);

	PRBool checkMatchPrincipal(void* context, nsPrincipal *principal, 
                               PRInt32 callerDepth);

    /* Helper functions for ADMIN UI */
	const char * getAllPrincipalsString(void);

	nsPrincipal * getPrincipalFromString(char *prinName);

	void getTargetsWithPrivileges(char *prinName, char** forever, 
                                  char** session, char **denied);

	PRBool removePrincipal(char *prinName);

	PRBool removePrincipalsPrivilege(char *prinName, char *targetName);

	void remove(nsPrincipal *prin, nsTarget *target);

	/* The following are old native methods */
	char * checkPrivilegeEnabled(nsTargetArray* targetArray, 
                                 PRInt32 callerDepth, void *data);

	char * checkPrivilegeEnabled(void* context, nsTargetArray* targetArray, 
                                 PRInt32 callerDepth, void *data);

	nsPrincipalArray* getClassPrincipalsFromStack(PRInt32 callerDepth);

	nsPrincipalArray* getClassPrincipalsFromStack(void* context, 
                                                  PRInt32 callerDepth);

	nsPrivilegeTable * getPrivilegeTableFromStack(PRInt32 callerDepth, 
                                                  PRBool createIfNull);

	nsPrivilegeTable * getPrivilegeTableFromStack(void* context, 
                                                  PRInt32 callerDepth, 
                                                  PRBool createIfNull);

	/* End of native methods */

private:

	nsHashtable *itsPrinToPrivTable;
	nsHashtable *itsPrinToMacroTargetPrivTable;
	nsHashtable *itsPrinNameToPrincipalTable;

	static PRBool theSecurityInited;

	static char * SignedAppletDBName;

	static PRBool theInited;

	/* Private Field Accessors */

	/* Private Methods */

	void addToPrinNameToPrincipalTable(nsPrincipal *prin);

	PRBool enablePrivilegePrivate(void* context, nsTarget *target, 
                                  nsPrincipal *preferredPrincipal, 
                                  PRInt32 callerDepth);

	nsPermissionState getPrincipalPrivilege(nsTarget *target, 
                                            nsPrincipalArray* callerPrinArray, 
                                            void *data);

	PRBool isPermissionGranted(nsTarget *target, 
                               nsPrincipalArray* callerPrinArray, 
                               void *data);


  /* The following methods are used to save and load the persistent store */
	void save(nsPrincipal *prin, nsTarget *target, nsPrivilege *newPrivilege);

	void load(void);

};


#endif /* _NS_PRIVILEGE_MANAGER_H_ */
