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


#include "prtypes.h"
#include "prmem.h"
#include "prmon.h"
#include "prlog.h"
#include "nsCaps.h"
#include "nsPrincipalManager.h"
#include "nsPrivilegeManager.h"
#include "nsIPrincipal.h"
#include "nsCertificatePrincipal.h"
#include "nsIPrivilege.h"
#include "nsPrivilegeTable.h"
#include "nsITarget.h"
#include "nsCCapsManager.h"

/* 
 * With the introduction of '-reg_mode' flag, 
 * we now have a variable that holds the information
 * to tell us whether or not navigator is running with 
 * that flag. registrationModeflag is introduced for that 
 * purpose. We have function APIs in place (in nsCaps.h) 
 * to access and modify this variable. 
 */
PRBool registrationModeFlag = PR_FALSE;

PR_BEGIN_EXTERN_C

/* 
 *             C  API  FOR  JS
 *
 * All of the following methods are used by JS (the code located
 * in lib/libmocha area).
 */

/* wrappers for nsPrivilegeManager object */
/*
PR_IMPLEMENT(PRBool) 
nsCapsInitialize() 
{
	if(bNSCapsInitialized_g == PR_TRUE) return PR_TRUE;
	bNSCapsInitialized_g = PR_TRUE;
	nsIPrincipal * sysPrin = NULL;
#if defined(_WIN32)
//	sysPrin = CreateSystemPrincipal("java/classes/java40.jar", "java/lang/Object.class");
#else
//	sysPrin = CreateSystemPrincipal("java40.jar", "java/lang/Object.class");
#endif
//	if (sysPrin == NULL) {
//		nsresult res;
//		sysPrin = new nsCertificatePrincipal((PRInt16 *)nsIPrincipal::PrincipalType_Certificate,(const unsigned char **) "52:54:45:4e:4e:45:54:49", 
//									(unsigned int *)strlen("52:54:45:4e:4e:45:54:49"),1,& res);
//  }
	nsPrivilegeManager *nsPrivManager = nsPrivilegeManager::GetPrivilegeManager();
	if (nsPrivManager == NULL) nsPrivilegeManagerInitialize();
	PR_ASSERT(nsPrivManager != NULL);
	nsPrincipalManager * nsPrinManager = nsPrincipalManager::GetPrincipalManager();
	if (nsPrinManager == NULL) nsPrincipalManagerInitialize();
	nsPrinManager->RegisterSystemPrincipal(sysPrin);
  // New a class factory object and the constructor will register itself
  // as the factory object in the repository. All other modules should
  // FindFactory and use createInstance to create a instance of nsCCapsManager
  // and ask for nsICapsManager interface.
  return PR_TRUE;
}
*/

/* wrappers for nsPrivilegeManager object */
PR_IMPLEMENT(PRBool) 
nsCapsRegisterPrincipal(class nsIPrincipal *principal) 
{
  nsPrincipalManager * prinMan;
  nsPrincipalManager::GetPrincipalManager(& prinMan);
  if(prinMan == NULL) return PR_FALSE; 
  prinMan->RegisterPrincipal(principal);
  return PR_TRUE;
}

PR_IMPLEMENT(PRBool) 
nsCapsEnablePrivilege(void * context, class nsITarget * target, PRInt32 callerDepth)
{
	if (nsPrivilegeManager::GetPrivilegeManager() == NULL) return PR_FALSE;
	else {
		PRBool result;
		nsPrivilegeManager::GetPrivilegeManager()->EnablePrivilege((nsIScriptContext *)context, target, NULL, callerDepth,& result);
		return result;
	}
}

PR_IMPLEMENT(PRBool) 
nsCapsIsPrivilegeEnabled(void* context, class nsITarget *target, PRInt32 callerDepth)
{
	if (nsPrivilegeManager::GetPrivilegeManager() == NULL) return PR_FALSE;
	else {
		PRBool result;
		nsPrivilegeManager::GetPrivilegeManager()->IsPrivilegeEnabled((nsIScriptContext *)context, target, callerDepth,& result);
		return result;
	}
}

PR_IMPLEMENT(PRBool) 
nsCapsRevertPrivilege(void* context, class nsITarget * target, PRInt32 callerDepth)
{
	if (nsPrivilegeManager::GetPrivilegeManager() == NULL ) return PR_FALSE;
	else {
		PRBool result;
		nsPrivilegeManager::GetPrivilegeManager()->RevertPrivilege((nsIScriptContext *)context, target, callerDepth,& result);
		return result;
	}
}

PR_IMPLEMENT(PRBool) 
nsCapsDisablePrivilege(void* context, class nsITarget * target, PRInt32 callerDepth)
{
	if (nsPrivilegeManager::GetPrivilegeManager() == NULL) return PR_FALSE;
	else {
		PRBool result;
		nsPrivilegeManager::GetPrivilegeManager()->DisablePrivilege((nsIScriptContext *)context, target, callerDepth,& result);
		return result;
	}
}

PR_IMPLEMENT(void*) 
nsCapsGetClassPrincipalsFromStack(void* context, PRInt32 callerDepth)
{
	nsPrincipalManager * nsPrinManager;
  nsPrincipalManager::GetPrincipalManager(& nsPrinManager);
	return (nsPrinManager == NULL) ? NULL 
	: (void *)nsPrinManager->GetClassPrincipalsFromStack((nsIScriptContext *)context, callerDepth);
}

//PR_IMPLEMENT(PRInt16) 
//nsCapsComparePrincipalArray(void * prin1Array, void * prin2Array)
//{
//	nsPrivilegeManager * nsPrivManager = nsPrivilegeManager::GetPrivilegeManager();
//	return (nsPrivManager == NULL) ? nsPrivilegeManager::SetComparisonType_NoSubset 
//	: nsPrivManager->ComparePrincipalArray((nsPrincipalArray*)prin1Array, (nsPrincipalArray*)prin2Array);
//}

//PR_IMPLEMENT(void*) 
//nsCapsIntersectPrincipalArray(void* prin1Array, void* prin2Array)
//{
//	nsPrivilegeManager *nsPrivManager = nsPrivilegeManager::GetPrivilegeManager();
//	return (nsPrivManager == NULL) ? NULL 
//	: nsPrivManager->IntersectPrincipalArray((nsPrincipalArray*)prin1Array, (nsPrincipalArray*)prin2Array);
//}

PR_IMPLEMENT(PRBool) 
nsCapsCanExtendTrust(void* from, void* to)
{
	nsPrincipalManager * nsPrinManager;
  nsPrincipalManager::GetPrincipalManager(& nsPrinManager);
	PRBool result = PR_FALSE;
	if (nsPrinManager != NULL) nsPrinManager->CanExtendTrust((nsIPrincipalArray *)from, (nsIPrincipalArray *)to,& result);
	return result;
}

/* wrappers for nsPrincipal object */
/*
PR_IMPLEMENT(class nsIPrincipal *) 
nsCapsNewPrincipal(PRInt16 prinType, void * key, PRUint32 key_len, void *zig)
{
// XXX ARIEL FIX:
// this is absolutely wrong, must be fixed ASAP
//  return new nsIPrincipal(prinType, key, key_len, zig);
	return NULL;
}
*/

PR_IMPLEMENT(const char *) 
nsCapsPrincipalToString(class nsIPrincipal *principal)
{
	char * prinStr;
	principal->ToString(& prinStr);
	return prinStr;
}

PR_IMPLEMENT(PRBool) 
nsCapsIsCodebaseExact(class nsIPrincipal *principal)
{
	PRInt16 prinType;
	principal->GetType(& prinType);
	return (prinType == (PRInt16) nsIPrincipal::PrincipalType_CodebaseExact) ? PR_TRUE : PR_FALSE;
}
/*
PR_IMPLEMENT(const char *) 
nsCapsPrincipalGetVendor(class nsIPrincipal *principal)
{
	//deprecated returning NULL
	//return principal->getVendor();
	return NULL;
}
*/
/* wrappers for nsTarget object */
PR_IMPLEMENT(class nsITarget *) 
nsCapsFindTarget(char * name)
{
  return nsTarget::FindTarget(name);
}

/* wrappers for nsPrivilege object */
PR_IMPLEMENT(PRInt16) 
nsCapsGetPermission(nsIPrivilege * privilege)
{
	PRInt16 privState;
	privilege->GetState(& privState);
	return privState;
}

/* wrappers for nsPrivilegeTable object */
PR_IMPLEMENT(nsIPrivilege *)
nsCapsGetPrivilege(nsPrivilegeTable * annotation, class nsITarget * target)
{
	return annotation->Get(target);
}


/* Methods for stack walking */
struct NSJSJavaFrameWrapper * (*nsCapsNewNSJSJavaFrameWrapperCallback)(void *) = NULL;
PR_IMPLEMENT(void)
setNewNSJSJavaFrameWrapperCallback(struct NSJSJavaFrameWrapper * (*fp)(void *))
{
    nsCapsNewNSJSJavaFrameWrapperCallback = fp;
}

void (*nsCapsFreeNSJSJavaFrameWrapperCallback)(struct NSJSJavaFrameWrapper *);
PR_IMPLEMENT(void)
setFreeNSJSJavaFrameWrapperCallback(void (*fp)(struct NSJSJavaFrameWrapper *))
{
    nsCapsFreeNSJSJavaFrameWrapperCallback = fp;
}

void (*nsCapsGetStartFrameCallback)(struct NSJSJavaFrameWrapper *);
PR_IMPLEMENT(void)
setGetStartFrameCallback(void (*fp)(struct NSJSJavaFrameWrapper *))
{
    nsCapsGetStartFrameCallback = fp;
}

PRBool (*nsCapsIsEndOfFrameCallback)(struct NSJSJavaFrameWrapper *);
PR_IMPLEMENT(void)
setIsEndOfFrameCallback(PRBool (*fp)(struct NSJSJavaFrameWrapper *))
{
    nsCapsIsEndOfFrameCallback = fp;
}

PRBool (*nsCapsIsValidFrameCallback)(struct NSJSJavaFrameWrapper *);
PR_IMPLEMENT(void)
setIsValidFrameCallback(PRBool (*fp)(struct NSJSJavaFrameWrapper *))
{
    nsCapsIsValidFrameCallback = fp;
}

void * (*nsCapsGetNextFrameCallback)(struct NSJSJavaFrameWrapper *, int *);
PR_IMPLEMENT(void)
setGetNextFrameCallback(void * (*fp)(struct NSJSJavaFrameWrapper *, int *))
{
    nsCapsGetNextFrameCallback = fp;
}

void * (*nsCapsGetPrincipalArrayCallback)(struct NSJSJavaFrameWrapper *);
PR_IMPLEMENT(void)
setOJIGetPrincipalArrayCallback(void * (*fp)(struct NSJSJavaFrameWrapper *))
{
	nsCapsGetPrincipalArrayCallback = fp;
}

void * (*nsCapsGetAnnotationCallback)(struct NSJSJavaFrameWrapper *);
PR_IMPLEMENT(void)
setOJIGetAnnotationCallback(void * (*fp)(struct NSJSJavaFrameWrapper *))
{
	nsCapsGetAnnotationCallback = fp;
}

void * (*nsCapsSetAnnotationCallback)(struct NSJSJavaFrameWrapper *, void *);
PR_IMPLEMENT(void)
setOJISetAnnotationCallback(void * (*fp)(struct NSJSJavaFrameWrapper *, void *))
{
	nsCapsSetAnnotationCallback = fp;
}

/* 
 * This function enables registration mode flag. 
 * Enabling this flag will allow only file based
 * urls ('file:') to run. This flag is enabled
 * when the AccountSetup application is started.
 */
void 
nsCapsEnableRegistrationModeFlag(void)
{
	registrationModeFlag = PR_TRUE;
}

/*
 * This function disables the registration mode flag.
 * Disabling this flag allows all valid urls types to
 * run. This will be disabled when the AccountSetup 
 * application is finished.
 */
void 
nsCapsDisableRegistrationModeFlag(void)
{
	registrationModeFlag = PR_FALSE;
}


/*
 * This function returns the current value of registration
 * mode flag.
 */
PRBool 
nsCapsGetRegistrationModeFlag(void)
{
	return registrationModeFlag;
}

PR_END_EXTERN_C

