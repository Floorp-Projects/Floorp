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
/* handles operations dealing with enabling and disabling privileges */
#ifndef _NS_PRIVILEGE_MANAGER_H_
#define _NS_PRIVILEGE_MANAGER_H_

#include "nsIPrivilegeManager.h"
#include "prtypes.h"
#include "prio.h"
#include "prmon.h"
#include "nsHashtable.h"
#include "nsVector.h"
#include "nsCaps.h"
#include "nsTarget.h"
#include "nsIPrincipal.h"
#include "nsIPrincipalArray.h"
#include "nsIPrincipalManager.h"
#include "nsPrincipalManager.h"
#include "nsIPrivilege.h"
#include "nsPrivilegeTable.h"
#include "nsSystemPrivilegeTable.h"

extern PRBool nsCaps_lock(void);
extern void nsCaps_unlock(void);

PR_BEGIN_EXTERN_C
PRBool CMGetBoolPref(char * pref_name);
PR_END_EXTERN_C

class nsPrivilegeManager : public nsIPrivilegeManager {

public:

nsHashtable * itsPrinToPrivTable;
nsHashtable * itsPrinToMacroTargetPrivTable;

NS_DECL_ISUPPORTS

static nsPrivilegeManager *
GetPrivilegeManager();

virtual ~nsPrivilegeManager(void);

static nsIPrivilege *
FindPrivilege(PRInt16 privState, PRInt16 privDuration);

static nsIPrivilege *
FindPrivilege(nsIPrivilege * perm);

static nsIPrivilege *
FindPrivilege(char * privStr);

static nsIPrivilege *
Add(nsIPrivilege * privilege1, nsIPrivilege * privilege2);

PRBool 
IsPrivilegeEnabled(nsITarget *target, PRInt32 callerDepth);

NS_IMETHOD 
IsPrivilegeEnabled(nsIScriptContext * context, nsITarget * target, PRInt32 callerDepth, PRBool * result);

PRBool 
EnablePrivilege(nsITarget * target, PRInt32 callerDepth);

PRBool 
EnablePrivilege(nsIScriptContext * context, nsITarget * target, PRInt32 callerDepth);

PRBool 
EnablePrivilege(nsITarget * target, nsIPrincipal * preferredPrincipal, PRInt32 callerDepth);

NS_IMETHOD
EnablePrivilege(nsIScriptContext * context, nsITarget * target, nsIPrincipal * preferredPrincipal, PRInt32 callerDepth, PRBool * result);

PRBool 
RevertPrivilege(nsITarget * target, PRInt32 callerDepth);

NS_IMETHOD
RevertPrivilege(nsIScriptContext * context, nsITarget *target, PRInt32 callerDepth, PRBool * result);

PRBool 
DisablePrivilege(nsITarget *target, PRInt32 callerDepth);

NS_IMETHOD
DisablePrivilege(nsIScriptContext * context, nsITarget *target, PRInt32 callerDepth, PRBool * result);

PRBool
EnablePrincipalPrivilegeHelper(nsITarget *target, PRInt32 callerDepth, 
								nsIPrincipal * preferredPrin, void * data,
								nsITarget *impersonator);

PRBool 
EnablePrincipalPrivilegeHelper(nsIScriptContext * context, nsITarget *target, PRInt32 callerDepth, 
								nsIPrincipal * preferredPrin, void * data, 
								nsITarget *impersonator);

nsPrivilegeTable * 
EnableScopePrivilegeHelper(nsITarget *target, PRInt32 callerDepth, 
							void *data, PRBool helpingSetScopePrivilege, 
							nsIPrincipal * prefPrin);

nsPrivilegeTable *
EnableScopePrivilegeHelper(nsIScriptContext * context, nsITarget *target, PRInt32 callerDepth, void *data, 
							PRBool helpingSetScopePrivilege, nsIPrincipal * prefPrin);

NS_IMETHOD
AskPermission(nsIPrincipal * useThisPrin, nsITarget* target, void* data, PRBool * result);

NS_IMETHOD
SetPermission(nsIPrincipal * useThisPrin, nsITarget * target, nsIPrivilege * newPrivilege);

void 
UpdatePrivilegeTable(nsITarget *target, nsPrivilegeTable * privTable, nsIPrivilege * newPrivilege);

PRBool 
CheckPrivilegeGranted(nsITarget *target, PRInt32 callerDepth);

PRBool 
CheckPrivilegeGranted(nsIScriptContext * context, nsITarget *target, PRInt32 callerDepth);

PRBool 
CheckPrivilegeGranted(nsITarget * target, nsIPrincipal * principal, void *data);

PRBool 
CheckPrivilegeGranted(nsITarget * target, PRInt32 callerDepth, void * data);

NS_IMETHOD
CheckPrivilegeGranted(nsIScriptContext * context, nsITarget * target, PRInt32 callerDepth, void * data, PRBool * result);

NS_IMETHOD
GetPrincipalPrivilege(nsITarget * target, nsIPrincipal * prin, void * data, nsIPrivilege * * result);

char * 
CheckPrivilegeEnabled(nsTargetArray * targetArray, PRInt32 callerDepth, void *data);

char * 
CheckPrivilegeEnabled(nsIScriptContext * context, nsTargetArray * targetArray, PRInt32 callerDepth, void *data);

void 
GetTargetsWithPrivileges(char *prinName, char** forever, char** session, char **denied);

nsPrivilegeTable * 
GetPrivilegeTableFromStack(PRInt32 callerDepth, PRBool createIfNull);

nsPrivilegeTable * 
GetPrivilegeTableFromStack(nsIScriptContext * context, PRInt32 callerDepth, PRBool createIfNull);

NS_IMETHODIMP
RemovePrincipalsPrivilege(const char * prinName, const char * targetName, PRBool * result);

void 
Remove(nsIPrincipal *prin, nsITarget *target);

PRBool 
RemovePrincipal(char *prinName);

void 
RegisterPrincipalAndSetPrivileges(nsIPrincipal * principal, nsITarget * target, nsIPrivilege * newPrivilege);

void 
Save(nsIPrincipal * prin, nsITarget * target, nsIPrivilege * newPrivilege);

void 
Load(void);

private:
nsPrivilegeManager(void);

static char * SignedAppletDBName;

PRBool 
EnablePrivilegePrivate(nsIScriptContext * context, nsITarget *target, nsIPrincipal *preferredPrincipal, 
						PRInt32 callerDepth);

PRInt16 
GetPrincipalPrivilege(nsITarget * target, nsIPrincipalArray * callerPrinArray, void * data);

PRBool 
IsPermissionGranted(nsITarget *target, nsIPrincipalArray * callerPrinArray, void *data);

};

#endif /* _NS_PRIVILEGE_MANAGER_H_ */
