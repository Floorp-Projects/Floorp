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
#include "nsIPrincipal.h"
#include "nsPrincipalTools.h"
#include "nsIPrivilege.h"
#include "nsPrivilegeTable.h"
#include "nsSystemPrivilegeTable.h"

extern PRBool nsCaps_lock(void);
extern void nsCaps_unlock(void);

PR_BEGIN_EXTERN_C
PRBool CMGetBoolPref(char * pref_name);
PR_END_EXTERN_C

PRBool nsPrivilegeManagerInitialize(void);

struct nsPrivilegeManager {

public:

enum { SetComparisonType_ProperSubset=-1 };
enum { SetComparisonType_Equal=0 };
enum { SetComparisonType_NoSubset=1 };

nsPrivilegeManager(void);
virtual ~nsPrivilegeManager(void);

static nsIPrivilege *
FindPrivilege(PRInt16 privState, PRInt16 privDuration);

static nsIPrivilege *
FindPrivilege(nsIPrivilege * perm);

static nsIPrivilege *
FindPrivilege(char * privStr);

static nsIPrivilege *
Add(nsIPrivilege * privilege1, nsIPrivilege * privilege2);

void 
RegisterSystemPrincipal(nsIPrincipal * principal);

void 
RegisterPrincipal(nsIPrincipal * principal);

PRBool 
UnregisterPrincipal(nsIPrincipal * principal);

PRBool 
IsPrivilegeEnabled(nsITarget *target, PRInt32 callerDepth);

PRBool 
IsPrivilegeEnabled(void* context, nsITarget *target, 
					PRInt32 callerDepth);

PRBool 
EnablePrivilege(nsITarget * target, PRInt32 callerDepth);

PRBool 
EnablePrivilege(void * context, nsITarget * target, PRInt32 callerDepth);

PRBool 
EnablePrivilege(nsITarget * target, nsIPrincipal * preferredPrincipal, 
				PRInt32 callerDepth);

PRBool 
EnablePrivilege(void* context, nsITarget *target, nsIPrincipal * preferredPrincipal, 
				PRInt32 callerDepth);

PRBool 
RevertPrivilege(nsITarget * target, PRInt32 callerDepth);

PRBool 
RevertPrivilege(void* context, nsITarget *target, PRInt32 callerDepth);

PRBool 
DisablePrivilege(nsITarget *target, PRInt32 callerDepth);

PRBool 
DisablePrivilege(void * context, nsITarget *target, PRInt32 callerDepth);

PRBool
EnablePrincipalPrivilegeHelper(nsITarget *target, PRInt32 callerDepth, 
								nsIPrincipal * preferredPrin, void * data,
								nsITarget *impersonator);

PRBool 
EnablePrincipalPrivilegeHelper(void* context, nsITarget *target, 
								PRInt32 callerDepth, 
								nsIPrincipal * preferredPrin, 
								void * data, 
								nsITarget *impersonator);

nsPrivilegeTable * 
EnableScopePrivilegeHelper(nsITarget *target, 
							PRInt32 callerDepth, 
							void *data, 
							PRBool helpingSetScopePrivilege, 
							nsIPrincipal * prefPrin);

nsPrivilegeTable *
EnableScopePrivilegeHelper(void* context, nsITarget *target, 
							PRInt32 callerDepth, void *data, 
							PRBool helpingSetScopePrivilege, 
							nsIPrincipal * prefPrin);

PRBool 
AskPermission(nsIPrincipal * useThisPrin, nsITarget* target, void* data);

void 
SetPermission(nsIPrincipal * useThisPrin, nsITarget * target, nsIPrivilege * newPrivilege);

void 
RegisterPrincipalAndSetPrivileges(nsIPrincipal * principal, nsITarget * target, nsIPrivilege * newPrivilege);

void 
UpdatePrivilegeTable(nsITarget *target, nsPrivilegeTable * privTable, nsIPrivilege * newPrivilege);

PRBool 
CheckPrivilegeGranted(nsITarget *target, PRInt32 callerDepth);

PRBool 
CheckPrivilegeGranted(void* context, nsITarget *target, PRInt32 callerDepth);

PRBool 
CheckPrivilegeGranted(nsITarget * target, nsIPrincipal * principal, void *data);

PRBool 
CheckPrivilegeGranted(nsITarget * target, PRInt32 callerDepth, void * data);

PRBool 
CheckPrivilegeGranted(void * context, nsITarget * target, PRInt32 callerDepth, void * data);

nsIPrivilege *
GetPrincipalPrivilege(nsITarget * target, nsIPrincipal * prin, void * data);

static nsPrivilegeManager * 
GetPrivilegeManager(void);

static nsPrincipalArray * 
GetMyPrincipals(PRInt32 callerDepth);

static nsPrincipalArray * 
GetMyPrincipals(void* context, PRInt32 callerDepth);

static nsIPrincipal * 
GetSystemPrincipal(void);

static PRBool 
HasSystemPrincipal(nsPrincipalArray * prinArray);

static nsIPrincipal * 
GetUnsignedPrincipal(void);

static nsIPrincipal * 
GetUnknownPrincipal(void);

PRInt16 
ComparePrincipalArray(nsPrincipalArray * prin1Array, nsPrincipalArray * prin2Array);

nsPrincipalArray * 
IntersectPrincipalArray(nsPrincipalArray * pa1, nsPrincipalArray * pa2);

PRBool 
CanExtendTrust(nsPrincipalArray * pa1, nsPrincipalArray * pa2);

PRBool 
CheckMatchPrincipal(nsIPrincipal * principal, PRInt32 callerDepth);

PRBool 
CheckMatchPrincipal(void* context, nsIPrincipal * principal, PRInt32 callerDepth);

/* Helper functions for ADMIN UI */
const char * 
GetAllPrincipalsString(void);

nsIPrincipal * 
GetPrincipalFromString(char *prinName);

void 
GetTargetsWithPrivileges(char *prinName, char** forever, char** session, char **denied);

PRBool 
RemovePrincipal(char *prinName);

PRBool 
RemovePrincipalsPrivilege(char *prinName, char *targetName);

void 
Remove(nsIPrincipal *prin, nsITarget *target);

/* The following are old native methods */
char * 
CheckPrivilegeEnabled(nsTargetArray * targetArray, PRInt32 callerDepth, void *data);

char * 
CheckPrivilegeEnabled(void* context, nsTargetArray * targetArray, PRInt32 callerDepth, void *data);

nsPrincipalArray * 
GetClassPrincipalsFromStack(PRInt32 callerDepth);

nsPrincipalArray * 
GetClassPrincipalsFromStack(void* context, PRInt32 callerDepth);

nsPrivilegeTable * 
GetPrivilegeTableFromStack(PRInt32 callerDepth, PRBool createIfNull);

nsPrivilegeTable * 
GetPrivilegeTableFromStack(void* context, PRInt32 callerDepth, PRBool createIfNull);

/* End of native methods */

private:

nsHashtable * itsPrinToPrivTable;
nsHashtable * itsPrinToMacroTargetPrivTable;
nsHashtable * itsPrinNameToPrincipalTable;

static PRBool theSecurityInited;

static char * SignedAppletDBName;

static PRBool theInited;

/* Private Methods */

void 
AddToPrincipalNameToPrincipalTable(nsIPrincipal * prin);

PRBool 
EnablePrivilegePrivate(void* context, nsITarget *target, nsIPrincipal *preferredPrincipal, 
						PRInt32 callerDepth);

PRInt16 
GetPrincipalPrivilege(nsITarget * target, nsPrincipalArray * callerPrinArray, void * data);

PRBool 
IsPermissionGranted(nsITarget *target, nsPrincipalArray* callerPrinArray, void *data);


  /* The following methods are used to save and load the persistent store */
void 
Save(nsIPrincipal * prin, nsITarget * target, nsIPrivilege * newPrivilege);

void 
Load(void);

};


#endif /* _NS_PRIVILEGE_MANAGER_H_ */
