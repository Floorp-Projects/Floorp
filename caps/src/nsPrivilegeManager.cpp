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

#include "nsPrivilegeManager.h"
#include "nsPrivilege.h"
#include "nsPrincipalArray.h"
#include "nsPrincipalManager.h"
#include "prmem.h"
#include "prmon.h"
#include "prlog.h"
#include "prprf.h"
#include "plbase64.h"
#include "plstr.h"
#include "jpermission.h"
#ifdef ENABLE_RDF
#include "rdf.h"
#include "jsec2rdf.h"
#endif /* ENABLE_RDF */


static nsPrivilegeManager * thePrivilegeManager = NULL;
/*
static nsIPrincipal * theSystemPrincipal = NULL;
static nsIPrincipal * theUnsignedPrincipal;
static nsIPrincipal * theUnknownPrincipal;
static nsIPrincipalArray * theUnknownPrincipalArray;
static nsIPrincipalArray * theUnsignedPrincipalArray;
*/
static nsIPrivilege * thePrivilegeCache[nsIPrivilege::PrivilegeState_NumberOfPrivileges][nsIPrivilege::PrivilegeDuration_NumberOfDurations];
static PRMonitor *caps_lock = NULL;

/* We could avoid the following globals if nsHashTable's Enumerate accepted
 * a void * as argument and it passed that argument as a parameter to the 
 * callback function.
 */
char * gForever;
char * gSession;
char * gDenied;
nsPrivilegeTable * gPrivilegeTable;

#ifdef ENABLE_RDF
static nsIPrincipal * RDF_getPrincipal(JSec_Principal jsec_pr);
static JSec_Principal RDF_CreatePrincipal(nsPrincipal *prin);
#endif /* ENABLE_RDF */

static PRBool RDF_RemovePrincipal(nsIPrincipal *prin);
static PRBool RDF_RemovePrincipalsPrivilege(nsIPrincipal * prin, nsITarget * target);

PR_BEGIN_EXTERN_C
#include "xp.h"
#include "prefapi.h"

PRBool
CMGetBoolPref(char * pref_name) 
{
#ifdef MOZ_SECURITY
	XP_Bool pref;
	if (PREF_GetBoolPref(pref_name, &pref) >=0)  return pref;
#endif /* MOZ_SECURITY */
	return FALSE;
}
PR_END_EXTERN_C


PRBool
nsCaps_lock(void)
{
	if(caps_lock == NULL) {
		caps_lock = PR_NewMonitor();
		if(caps_lock == NULL) return PR_FALSE;
	} 
	PR_EnterMonitor(caps_lock);
	return PR_TRUE;
}

void
nsCaps_unlock(void)
{
	PR_ASSERT(caps_lock != NULL);
	if(caps_lock != NULL) PR_ExitMonitor(caps_lock);
}

static NS_DEFINE_IID(kIPrivilegeManagerIID, NS_IPRIVILEGEMANAGER_IID);

NS_IMPL_ISUPPORTS(nsPrivilegeManager, kIPrivilegeManagerIID);

nsPrivilegeManager::nsPrivilegeManager(void)
{
  nsCaps_lock();
  itsPrinToPrivTable = new nsHashtable();
  itsPrinToMacroTargetPrivTable = new nsHashtable();
  nsCaps_unlock();
}

nsPrivilegeManager::~nsPrivilegeManager(void)
{
	nsCaps_lock();
	if (itsPrinToPrivTable) delete itsPrinToPrivTable;
	if (itsPrinToMacroTargetPrivTable) delete itsPrinToMacroTargetPrivTable;
	nsCaps_unlock();
}

nsIPrivilege * 
nsPrivilegeManager::FindPrivilege(PRInt16 privState, PRInt16 privDuration) {
	return thePrivilegeCache[privState][privDuration];
}

nsIPrivilege * 
nsPrivilegeManager::FindPrivilege(nsIPrivilege * priv)
{
	PRInt16 privState;
	PRInt16 privDuration;
	priv->GetState(& privState);
	priv->GetDuration(& privDuration);
	return nsPrivilegeManager::FindPrivilege(privState, privDuration);
}

nsIPrivilege *
nsPrivilegeManager::FindPrivilege(char * privStr)
{
	PRInt16 privState; 
	PRInt16 privDuration;
	if (XP_STRCMP(privStr, "allowed in scope") == 0) {
		privState = nsIPrivilege::PrivilegeState_Allowed;
		privDuration = nsIPrivilege::PrivilegeDuration_Scope;
	} else if (XP_STRCMP(privStr, "allowed in session") == 0) {
		privState = nsIPrivilege::PrivilegeState_Allowed;
		privDuration = nsIPrivilege::PrivilegeDuration_Session;
	} else if (XP_STRCMP(privStr, "allowed forever") == 0) {
		privState = nsIPrivilege::PrivilegeState_Allowed;
		privDuration = nsIPrivilege::PrivilegeDuration_Forever;
	} else if (XP_STRCMP(privStr, "forbidden forever") == 0) {
		privState = nsIPrivilege::PrivilegeState_Forbidden;
		privDuration = nsIPrivilege::PrivilegeDuration_Forever;
	} else if (XP_STRCMP(privStr, "forbidden in session") == 0) {
		privState = nsIPrivilege::PrivilegeState_Forbidden;
		privDuration = nsIPrivilege::PrivilegeDuration_Session;
	} else if (XP_STRCMP(privStr, "forbidden in scope") == 0) {
		privState = nsIPrivilege::PrivilegeState_Forbidden;
		privDuration = nsIPrivilege::PrivilegeDuration_Scope;
	} else if (XP_STRCMP(privStr, "blank forever") == 0) {
		privState = nsIPrivilege::PrivilegeState_Blank;
		privDuration = nsIPrivilege::PrivilegeDuration_Forever;
	} else if (XP_STRCMP(privStr, "blank in session") == 0) {
		privState = nsIPrivilege::PrivilegeState_Blank;
		privDuration = nsIPrivilege::PrivilegeDuration_Session;
	} else if (XP_STRCMP(privStr, "blank in scope") == 0) {
		privState = nsIPrivilege::PrivilegeState_Blank;
		privDuration = nsIPrivilege::PrivilegeDuration_Scope;
	} else {
		privState = nsIPrivilege::PrivilegeState_Blank;
		privDuration = nsIPrivilege::PrivilegeDuration_Scope;
	}
	return nsPrivilegeManager::FindPrivilege(privState, privDuration);
}

nsIPrivilege *
nsPrivilegeManager::Add(nsIPrivilege * priv1, nsIPrivilege * priv2) {
	nsresult rv;
	PRInt16 * p1state = NULL, * p2state = NULL;
	rv = priv1->GetState(p1state);
	rv = priv2->GetState(p2state);
	return (p1state < p2state) ? priv1 : priv2; 
}

void 
nsPrivilegeManager::RegisterSystemPrincipal(nsIPrincipal * prin)
{
	PrincipalKey prinKey(prin);
	nsCaps_lock();
	if (NULL == itsPrinToPrivTable->Get(&prinKey)) 
		itsPrinToPrivTable->Put(&prinKey, new nsSystemPrivilegeTable());
	if (NULL == itsPrinToMacroTargetPrivTable->Get(&prinKey)) 
		itsPrinToMacroTargetPrivTable->Put(&prinKey, new nsSystemPrivilegeTable());
//	nsPrincipalManager::GetPrincipalManager()->SetSystemPrincipal(prin);
	CreateSystemTargets(prin);
	// Load the signed applet's ACL from the persistence store
	this->Load();
	nsCaps_unlock();
}

void 
nsPrivilegeManager::RegisterPrincipal(nsIPrincipal * prin)
{
	//
	// the new PrivilegeTable will have all privileges "blank forever"
	// until changed by calls to enablePrincipalPrivilegeHelper
	//
	PrincipalKey prinKey(prin);
	nsCaps_lock();
	if (NULL == itsPrinToPrivTable->Get(&prinKey)) {
		itsPrinToPrivTable->Put(&prinKey, new nsPrivilegeTable());
	}
	if (NULL == itsPrinToMacroTargetPrivTable->Get(&prinKey)) {
		itsPrinToMacroTargetPrivTable->Put(& prinKey, new nsPrivilegeTable());
	}
//	nsPrincipalManager::GetPrincipalManager()->AddToPrincipalNameToPrincipalTable(prin);
	nsCaps_unlock();
}


PRBool 
nsPrivilegeManager::UnregisterPrincipal(nsIPrincipal * prin)
{
	PRBool result;
	prin->Equals(nsPrincipalManager::GetPrincipalManager()->GetSystemPrincipal(),& result);
	if (result) return PR_FALSE;
	PrincipalKey prinKey(prin);
	nsCaps_lock();
	/* Get the privilegetables and free them up */
	nsPrivilegeTable *pt = (nsPrivilegeTable *)itsPrinToPrivTable->Get(&prinKey);
	if (pt != NULL)  delete pt;
	nsPrivilegeTable *mpt = (nsPrivilegeTable *)itsPrinToMacroTargetPrivTable->Get(&prinKey);
	if (mpt != NULL) delete mpt;
	/* Remove the principal */
	void *old_prin = itsPrinToPrivTable->Remove(&prinKey);
	void *old_prin1 = itsPrinToMacroTargetPrivTable->Remove(&prinKey);
	/* remove principal from PrinNameToPrincipalTable */
	nsPrincipalManager::GetPrincipalManager()->RemoveFromPrincipalNameToPrincipalTable(prin);
	/* remove the principal from RDF also */
	RDF_RemovePrincipal(prin);
	nsCaps_unlock();
	return ((old_prin == NULL) && (old_prin1 == NULL)) ? PR_FALSE : PR_TRUE;
}

PRBool 
nsPrivilegeManager::IsPrivilegeEnabled(nsITarget * target, PRInt32 callerDepth)
{
	PRBool result;
	IsPrivilegeEnabled(NULL, target, callerDepth,& result);
	return result;
}


NS_IMETHODIMP
nsPrivilegeManager::IsPrivilegeEnabled(nsIScriptContext * context, nsITarget * target, PRInt32 callerDepth, PRBool * result)
{
	nsTargetArray * targetArray = new nsTargetArray();
	nsITarget * targ = nsTarget::FindTarget(target);
	if (targ != target) {
		* result = PR_FALSE;
		return NS_OK;
	}
	targetArray->Add(targ);
	* result = (this->CheckPrivilegeEnabled(context, targetArray, callerDepth, NULL) != NULL) ? PR_FALSE : PR_TRUE;
	return NS_OK;
}

PRBool 
nsPrivilegeManager::EnablePrivilege(nsITarget * target, PRInt32 callerDepth)
{
  return this->EnablePrivilegePrivate(NULL, target, NULL, callerDepth);
}

PRBool 
nsPrivilegeManager::EnablePrivilege(nsIScriptContext * context, nsITarget * target, PRInt32 callerDepth)
{
  return this->EnablePrivilegePrivate(context, target, NULL, callerDepth);
}

PRBool 
nsPrivilegeManager::EnablePrivilege(nsITarget * target, nsIPrincipal * preferredPrincipal, PRInt32 callerDepth)
{
  return this->EnablePrivilegePrivate(NULL, target, preferredPrincipal, callerDepth);
}

NS_IMETHODIMP
nsPrivilegeManager::EnablePrivilege(nsIScriptContext * context, nsITarget * target, nsIPrincipal * preferredPrincipal, PRInt32 callerDepth, PRBool * result)
{
	* result = this->EnablePrivilegePrivate(context, target, preferredPrincipal, callerDepth);
	return NS_OK;
}

PRBool 
nsPrivilegeManager::RevertPrivilege(nsITarget * target, PRInt32 callerDepth)
{
	PRBool result;
	this->RevertPrivilege(NULL, target, callerDepth,& result);
	return result;
}

NS_IMETHODIMP
nsPrivilegeManager::RevertPrivilege(nsIScriptContext * context, nsITarget * target, PRInt32 callerDepth, PRBool * result)
{
	nsITarget * targ = nsTarget::FindTarget(target);
	if (targ != target) {
		 * result = PR_FALSE;
		 return NS_OK;
	}
	nsPrivilegeTable *privTable = this->GetPrivilegeTableFromStack(context, callerDepth, PR_TRUE);
	nsCaps_lock();
	privTable->Put(target, nsPrivilegeManager::FindPrivilege(nsIPrivilege::PrivilegeState_Blank, nsIPrivilege::PrivilegeDuration_Scope));
	nsCaps_unlock();
	* result = PR_TRUE;
	return NS_OK;
}


PRBool 
nsPrivilegeManager::DisablePrivilege(nsITarget * target, PRInt32 callerDepth)
{
	PRBool result;
	result = this->DisablePrivilege(NULL, target, callerDepth,& result);
	return result;
}

NS_IMETHODIMP
nsPrivilegeManager::DisablePrivilege(nsIScriptContext * context, nsITarget * target, PRInt32 callerDepth, PRBool * result)
{
	nsITarget * targ = nsTarget::FindTarget(target);
	if (targ != target) {
		* result = PR_FALSE;
		return NS_OK;
	}
	nsPrivilegeTable *privTable = this->GetPrivilegeTableFromStack(context, callerDepth, PR_TRUE);
	nsCaps_lock();
	privTable->Put(target, nsPrivilegeManager::FindPrivilege(nsIPrivilege::PrivilegeState_Forbidden, nsIPrivilege::PrivilegeDuration_Scope));
	nsCaps_unlock();
	* result = PR_TRUE;
	return NS_OK;
}

PRBool 
nsPrivilegeManager::EnablePrincipalPrivilegeHelper(nsITarget *target, PRInt32 callerDepth, 
						nsIPrincipal * preferredPrin, void * data, nsITarget *impersonator)
{
	return this->EnablePrincipalPrivilegeHelper(NULL, target, callerDepth, preferredPrin, data, impersonator);
}

PRBool 
nsPrivilegeManager::EnablePrincipalPrivilegeHelper(nsIScriptContext * context, nsITarget *target, 
		PRInt32 callerDepth, nsIPrincipal * preferredPrin, void * data, nsITarget *impersonator)
{
	nsIPrincipalArray* callerPrinArray;
	nsIPrincipal * useThisPrin = NULL;
	/* Get the registered target */
	nsITarget *targ = nsTarget::FindTarget(target);
	if (targ != target) return PR_FALSE;
	callerPrinArray = nsPrincipalManager::GetPrincipalManager()->GetClassPrincipalsFromStack((nsIScriptContext *)context, callerDepth);
	if (preferredPrin != NULL) {
		nsIPrincipal * callerPrin;
		PRUint32 i;
		callerPrinArray->GetPrincipalArraySize(& i);
		for (i; i-- > 0;) {
			callerPrinArray->GetPrincipalArrayElement(i, & callerPrin);
			PRBool result;
			callerPrin->Equals(preferredPrin, & result);
			if (result
//XXX ARIEL: update this code soon
//      &&
//          ((callerPrin->isCert() ||
//            callerPrin->isCertFingerprint()))
			) {

				useThisPrin = callerPrin;
				break;
			}
		}
		if ((useThisPrin == NULL) && (impersonator)){
			nsITarget *t1;
			t1 = impersonator;
			PRBool penabled;
			this->IsPrivilegeEnabled(context, t1, 0, & penabled);
			if (!penabled) return PR_FALSE;
			useThisPrin = preferredPrin;
			callerPrinArray = new nsPrincipalArray();
			callerPrinArray->AddPrincipalArrayElement(preferredPrin);
		}
	}
	if (this->IsPermissionGranted(target, callerPrinArray, data)) return PR_TRUE;

	//
	// before we do the user dialog, we need to figure out which principal
	// gets the user's blessing.  The applet is allowed to bias this
	// by offering a preferred principal.  We only honor this if the
	// principal is *registered* (stored in itsPrinToPrivTable) and
	// is based on cryptographic credentials, rather than a codebase.
	//
	// if no preferredPrin is specified, or we don't like preferredPrin,
	// we'll use the first principal on the calling class.  We know that
	// cryptographic credentials go first in the list, so this should
	// get us something reasonable.
	//
	if (useThisPrin == NULL) {
		PRUint32 size;
		callerPrinArray->GetPrincipalArraySize(& size);
		if (size == 0) return PR_FALSE;
		// throw new ForbiddenTargetException("request's caller has no principal!");
		callerPrinArray->GetPrincipalArrayElement(0,& useThisPrin);
	}
	// Do a user dialog
	PRBool result;
	AskPermission(useThisPrin, target, data,& result);
	return result;
}


nsPrivilegeTable *
nsPrivilegeManager::EnableScopePrivilegeHelper(nsITarget *target, PRInt32 callerDepth, void *data, PRBool helpingSetScopePrivilege, 
                                               nsIPrincipal *prefPrin)
{
  return this->EnableScopePrivilegeHelper(NULL, target, callerDepth, data, 
                                    helpingSetScopePrivilege, prefPrin);
}


nsPrivilegeTable *
nsPrivilegeManager::EnableScopePrivilegeHelper(nsIScriptContext * context, nsITarget *target, PRInt32 callerDepth, 
                                               void *data, PRBool helpingSetScopePrivilege, nsIPrincipal * prefPrin)
{
  nsPrivilegeTable *privTable;
  nsIPrivilege * allowedScope;
  PRBool res;

  nsITarget * targ = nsTarget::FindTarget(target);
  if (targ != target) return NULL;
    //throw new ForbiddenTargetException(target + " is not a registered target");
  (prefPrin != NULL) ?
  res = this->CheckPrivilegeGranted(target, prefPrin, data) :
	this->CheckPrivilegeGranted(context, target, callerDepth, data,& res);
  if (res == PR_FALSE) return NULL;
  privTable = this->GetPrivilegeTableFromStack(context, callerDepth, 
                                         (helpingSetScopePrivilege ? PR_FALSE : PR_TRUE));
  if (helpingSetScopePrivilege) {
    if (privTable == NULL)  privTable = new nsPrivilegeTable();
  }
                
  allowedScope = nsPrivilegeManager::FindPrivilege(nsIPrivilege::PrivilegeState_Allowed, nsIPrivilege::PrivilegeDuration_Scope);
  this->UpdatePrivilegeTable(target, privTable, allowedScope);
  return privTable;
}

NS_IMETHODIMP
nsPrivilegeManager::AskPermission(nsIPrincipal * useThisPrin, nsITarget * target, void * data, PRBool * result)
{
	/*
  PRBool ret_val = PR_FALSE;
  nsPrivilege* newPrivilege = NULL;
//   Get the Lock to display the dialog 
  nsCaps_lock();
  nsIPrincipalArray* callerPrinArray = new nsIPrincipalArray();
  callerPrinArray->Add(useThisPrin);
  if (PR_TRUE == this->IsPermissionGranted(target, callerPrinArray, data)) {
    ret_val = PR_TRUE;
    goto done;
  }
  // Do a user dialog
  newPrivilege = target->EnablePrivilege(useThisPrin, data);
  // Forbidden for session is equivelent to decide later.
  // If the privilege is DECIDE_LATER then throw exception.
  // That is user should be prompted again when this applet 
  // performs the same privileged operation
  //
  if ((!newPrivilege->IsAllowed()) &&
      (newPrivilege->GetDuration() == nsIPrivilege::PrivilegeDuration_Session)) {
    // "User didn't grant the " + target->getName() + " privilege.";
    ret_val = PR_FALSE;
    goto done;
  }
  this->SetPermission(useThisPrin, target, newPrivilege);
  // if newPrivilege is FORBIDDEN then throw an exception
  if (newPrivilege->IsForbidden()) {
    // "User didn't grant the " + target->getName() + " privilege.";
    ret_val = PR_FALSE;
    goto done;
  }

  ret_val = PR_TRUE;

done:
  delete callerPrinArray;
  nsCaps_unlock();
  */
  return NS_OK;
}

NS_IMETHODIMP
nsPrivilegeManager::SetPermission(nsIPrincipal * useThisPrin, nsITarget * target, nsIPrivilege * newPrivilege)
{
	/*
  registerPrincipalAndSetPrivileges(useThisPrin, target, newPrivilege);
//XXX ARIEL - THIS LOOKS SO WRONG, FIX IT!!!!!!!!!!!!!!!
  //System.out.println("Privilege table modified for: " + 
  // 		useThisPrin.toVerboseString() + " for target " + 
  //		target + " Privilege " + newPrivilege);

  // Save the signed applet's ACL to the persistence store
//  char* err = useThisPrin->savePrincipalPermanently();
  if ((err == NULL) && 
      (newPrivilege->getDuration() == nsDurationState_Forever)) {

    //XXX: How do we save permanent access for unsigned principals
    ///
	PRBool * result;
	useThisPrin->Equals(theUnsignedPrincipal, result);
    if (!result) save(useThisPrin, target, newPrivilege);
  }
  */
	return NS_OK;
}


void 
nsPrivilegeManager::RegisterPrincipalAndSetPrivileges(nsIPrincipal * prin, nsITarget *target, 
                                                      nsIPrivilege * newPrivilege)
{
  nsPrivilegeTable *privTable;
  this->RegisterPrincipal(prin);
  //Store the list of targets for which the user has given privilege
  PrincipalKey prinKey(prin);
  nsCaps_lock();
  privTable = (nsPrivilegeTable *)itsPrinToMacroTargetPrivTable->Get(&prinKey);
  privTable->Put(target, newPrivilege);
  nsCaps_unlock();

  privTable = (nsPrivilegeTable *)itsPrinToPrivTable->Get(&prinKey);
  this->UpdatePrivilegeTable(target, privTable, newPrivilege); 
}


void 
nsPrivilegeManager::UpdatePrivilegeTable(nsITarget * target, nsPrivilegeTable * privTable, nsIPrivilege * newPrivilege)
{
	nsTargetArray * primitiveTargets;
	target->GetFlattenedTargetArray(& primitiveTargets);
	nsIPrivilege * oldPrivilege, * privilege;
	nsITarget * primTarget;
  nsCaps_lock();
  for (int i = primitiveTargets->GetSize(); i-- > 0;) {
    primTarget = (nsITarget *)primitiveTargets->Get(i);
    oldPrivilege = privTable->Get(primTarget);
    privilege = (oldPrivilege != NULL) ? nsPrivilegeManager::Add(oldPrivilege, newPrivilege) :  newPrivilege;
    privTable->Put(primTarget, privilege);
  }
  nsCaps_unlock();
}

PRBool 
nsPrivilegeManager::CheckPrivilegeGranted(nsITarget *target, PRInt32 callerDepth)
{
	PRBool result;
	this->CheckPrivilegeGranted(NULL, target, callerDepth, NULL,& result);
	return result;
}

PRBool 
nsPrivilegeManager::CheckPrivilegeGranted(nsIScriptContext * context, nsITarget *target, PRInt32 callerDepth)
{
	PRBool result;
	CheckPrivilegeGranted(context, target, callerDepth, NULL,& result);
	return result;
}

PRBool 
nsPrivilegeManager::CheckPrivilegeGranted(nsITarget *target, nsIPrincipal *prin, void *data)
{
	nsIPrivilege * privilege;
	this->GetPrincipalPrivilege(target, prin, data,& privilege);
	PRBool allowed;
	privilege->IsAllowed(& allowed);
	return (allowed) ? PR_TRUE : PR_FALSE;
}

PRBool 
nsPrivilegeManager::CheckPrivilegeGranted(nsITarget *target, PRInt32 callerDepth, void *data)
{
	PRBool result;
	CheckPrivilegeGranted(NULL, target, callerDepth, data,& result);
	return result;
}

NS_IMETHODIMP
nsPrivilegeManager::CheckPrivilegeGranted(nsIScriptContext * context, nsITarget * target, PRInt32 callerDepth, void * data, PRBool * result)
{
	nsIPrincipalArray* callerPrinArray = 
		nsPrincipalManager::GetPrincipalManager()->GetClassPrincipalsFromStack((nsIScriptContext *)context, callerDepth);
	PRInt16 privilegeState = this->GetPrincipalPrivilege(target, callerPrinArray, data);
	* result = (privilegeState == nsIPrivilege::PrivilegeState_Allowed) ? PR_TRUE : PR_FALSE;
	return NS_OK;
}

nsPrivilegeManager *
nsPrivilegeManager::GetPrivilegeManager(void)
{
	return thePrivilegeManager;
}
/*
nsIPrincipalArray * 
nsPrivilegeManager::GetMyPrincipals(PRInt32 callerDepth)
{
	return nsPrivilegeManager::GetMyPrincipals(NULL, callerDepth);
}

nsIPrincipalArray * 
nsPrivilegeManager::GetMyPrincipals(void* context, PRInt32 callerDepth)
{
	return (thePrivilegeManager == NULL) ? NULL
	: thePrivilegeManager->GetClassPrincipalsFromStack(context, callerDepth);
}
*/
/*
nsIPrincipal * 
nsPrivilegeManager::GetSystemPrincipal(void)
{
	return theSystemPrincipal;
}
*/
/*
PRBool 
nsPrivilegeManager::HasSystemPrincipal(nsIPrincipalArray *prinArray)
{
	nsIPrincipal * sysPrin = nsPrivilegeManager::GetSystemPrincipal();
	nsIPrincipal * prin;
	if (sysPrin == NULL) return PR_FALSE;
	PRUint32 i;
	prinArray->GetPrincipalArraySize(& i);
	for (i; i-- > 0;) {
		prinArray->GetPrincipalArrayElement(i,& prin);
		PRBool result;
		sysPrin->Equals(prin, & result);
		if (result) return PR_TRUE;
	}
	return PR_FALSE;
}
*/
/*
nsIPrincipal * 
nsPrivilegeManager::GetUnsignedPrincipal(void)
{
	return theUnsignedPrincipal;
}
*/
/*
nsIPrincipal * 
nsPrivilegeManager::GetUnknownPrincipal(void)
{
	return theUnknownPrincipal;
}
*/
/*
PRBool 
nsPrivilegeManager::CanExtendTrust(nsIPrincipalArray * from, nsIPrincipalArray * to)
{
	if ((from == NULL) || (to == NULL)) return PR_FALSE;
	nsIPrincipalArray * intersect;
	from->IntersectPrincipalArray(to,& intersect);
	PRUint32 intersectSize = 0, fromSize = 0;
	intersect->GetPrincipalArraySize(& intersectSize);
	from->GetPrincipalArraySize(& fromSize);
	if (intersectSize == fromSize) return PR_TRUE;
	if (intersectSize == 0 || (intersectSize != (fromSize - 1))) return PR_FALSE;
	nsIPrincipal * prin;
	PRUint32 i;
	for (i=0; i < intersectSize; i++) {
		intersect->GetPrincipalArrayElement(i, & prin);
		PRInt16 prinType = nsIPrincipal::PrincipalType_Unknown;
		prin->GetType(& prinType);
		if (prinType == nsIPrincipal::PrincipalType_CodebaseExact ||
			prinType == nsIPrincipal::PrincipalType_CodebaseRegex)
			return PR_FALSE;
	}
	PRUint32 codebaseCount = 0;
	for (i=0; i < fromSize; i++) {
		from->GetPrincipalArrayElement(i, & prin);
		PRInt16 prinType = nsIPrincipal::PrincipalType_Unknown;
		prin->GetType(& prinType);
		if (prinType == nsIPrincipal::PrincipalType_CodebaseExact ||
			prinType == nsIPrincipal::PrincipalType_CodebaseRegex)
			codebaseCount++;
	}
	return (codebaseCount == 1) ? PR_TRUE : PR_FALSE;
}
*/
/*
PRBool 
nsPrivilegeManager::CheckMatchPrincipal(nsIPrincipal * prin, PRInt32 callerDepth)
{
	return this->CheckMatchPrincipal(NULL, prin, callerDepth);
}
*/
/*
PRBool 
nsPrivilegeManager::CheckMatchPrincipal(void * context, nsIPrincipal * prin, PRInt32 callerDepth)
{
	nsIPrincipalArray * prinArray = new nsPrincipalArray(0);
	prinArray->AddPrincipalArrayElement(prin);
	nsIPrincipalArray * classPrinArray = this->GetClassPrincipalsFromStack(context, callerDepth);
	PRInt16 compType = 0;
	prinArray->ComparePrincipalArray(classPrinArray,& compType);
	return (compType != nsPrincipalArray::SetComparisonType_NoSubset) ? PR_TRUE : PR_FALSE;
}
*/

static PRBool 
GetPermissionsString(nsHashKey * aKey, void * aData, void * closure) 
{
	/*
	TargetKey *targetKey = (TargetKey *) aKey;
	nsTarget *target = targetKey->itsTarget;
	nsIPrivilege * priv = (nsIPrivilege *)aData;
	char * desc = target->GetDescription();
	if (priv->isAllowedForever())  gForever = PR_sprintf_append(gForever, "<option>%s", desc);
	else if (priv->isForbiddenForever()) gDenied = PR_sprintf_append(gDenied, "<option>%s", desc);
	else if (priv->isAllowed())  gSession = PR_sprintf_append(gSession, "<option>%s", desc);
	*/
	return PR_TRUE;
}

void 
nsPrivilegeManager::GetTargetsWithPrivileges(char *prinName, char** forever, 
                                             char** session, char **denied)
{
	/* Admin UI */
	nsCaps_lock();
	*forever = gForever = NULL;
	*session = gSession = NULL;
	*denied = gDenied = NULL;
	nsIPrincipal * prin = nsPrincipalManager::GetPrincipalManager()->GetPrincipalFromString(prinName);
	if (prin == NULL) {
		nsCaps_unlock();
		return;
	}
	PrincipalKey prinKey(prin);
	nsPrivilegeTable * pt = (nsPrivilegeTable *)itsPrinToMacroTargetPrivTable->Get(& prinKey);
	if (pt == NULL) {
		nsCaps_unlock();
		return;
	}
	pt->Enumerate(GetPermissionsString);
	/* The caller should free up the allocated memory */
	*forever = gForever;
	*session = gSession;
	*denied = gDenied;
	gForever = NULL;
	gSession = NULL;
	gDenied = NULL;
	nsCaps_unlock();
}

PRBool 
nsPrivilegeManager::RemovePrincipal(char * prinName)
{
	/* Admin UI */
	nsCaps_lock();
	nsIPrincipal * prin = nsPrincipalManager::GetPrincipalManager()->GetPrincipalFromString(prinName);
	if (prin == NULL) {
		nsCaps_unlock();
		return PR_FALSE;
	}
	this->UnregisterPrincipal(prin);
	nsCaps_unlock();
	return PR_TRUE;
}

NS_IMETHODIMP
nsPrivilegeManager::RemovePrincipalsPrivilege(const char * prinName, const char * targetDesc, PRBool * result)
{
	/* Admin UI */
	nsIPrincipal * prin = nsPrincipalManager::GetPrincipalManager()->GetPrincipalFromString((char *)prinName);
	if (prin == NULL) {
		* result = PR_FALSE;
		return NS_OK;
	}
	/* targetDesc is passed to the admin UI in getPermissionsString */
	nsITarget * target = nsTarget::GetTargetFromDescription((char *)targetDesc);
	if (target == NULL) {
		* result = PR_FALSE;
		return NS_OK;
	}
	this->Remove(prin, target);
	* result = PR_TRUE;
	return NS_OK;
}

static PRBool 
UpdatePrivileges(nsHashKey *aKey, void *aData, void* closure) 
{
	/* Admin UI */
	TargetKey * targetKey = (TargetKey *) aKey;
	nsITarget * target = targetKey->itsTarget;
	nsPrivilege * priv = (nsPrivilege *)aData;
	nsPrivilegeManager * mgr = nsPrivilegeManager::GetPrivilegeManager();
	mgr->UpdatePrivilegeTable(target, gPrivilegeTable, priv);
	return PR_TRUE;
}

void 
nsPrivilegeManager::Remove(nsIPrincipal * prin, nsITarget * target)
{
	/* Admin UI */
	nsCaps_lock();
	if ((prin == NULL) || (target == NULL) ||
	    (itsPrinToMacroTargetPrivTable == NULL)) {
		nsCaps_unlock();
		return;
	}
	PrincipalKey prinKey(prin);
	nsPrivilegeTable * mpt = (nsPrivilegeTable *)itsPrinToMacroTargetPrivTable->Get(&prinKey);
	if (mpt == NULL) {
		nsCaps_unlock();
		return;
	}
	mpt->Remove(target);
	/* remove the prin/target from RDF also */
	RDF_RemovePrincipalsPrivilege(prin, target);
	/* Regenerate the expnaded prvileges for this principal */
	nsPrivilegeTable *pt = (nsPrivilegeTable *)itsPrinToPrivTable->Get(&prinKey);
	if (pt != NULL) delete pt;
	gPrivilegeTable = pt = new nsPrivilegeTable();
	itsPrinToPrivTable->Put(&prinKey, pt);
	mpt->Enumerate(UpdatePrivileges);
	gPrivilegeTable = NULL;
	nsCaps_unlock();
}

// 			PRIVATE METHODS 

PRBool 
nsPrivilegeManager::EnablePrivilegePrivate(nsIScriptContext * context, nsITarget *target, nsIPrincipal * prefPrin, PRInt32 callerDepth)
{
	if (PR_FALSE == this->EnablePrincipalPrivilegeHelper(context, target, callerDepth, prefPrin, NULL, NULL)) return PR_FALSE;
	return (NULL == this->EnableScopePrivilegeHelper(context, target, callerDepth, NULL, PR_FALSE, prefPrin))  ? PR_FALSE : PR_TRUE;
}

PRInt16
nsPrivilegeManager::GetPrincipalPrivilege(nsITarget * target, nsIPrincipalArray * callerPrinArray, void *data)
{
	nsIPrivilege * privilege;
	nsIPrincipal * principal;
	PRBool isAllowed = PR_FALSE;
	PRUint32 i;
	callerPrinArray->GetPrincipalArraySize(& i);
	for (i; i-- > 0; ) {
		callerPrinArray->GetPrincipalArrayElement(i,& principal);
		this->GetPrincipalPrivilege(target, principal, data,& privilege);
		if (privilege == NULL) continue;
		PRInt16 privilegeState;
		privilege->GetState(& privilegeState);
		switch(privilegeState) {
			case nsIPrivilege::PrivilegeState_Allowed: isAllowed = PR_TRUE;
			case nsIPrivilege::PrivilegeState_Blank: continue;
			default: PR_ASSERT(FALSE);
			case nsIPrivilege::PrivilegeState_Forbidden: return nsIPrivilege::PrivilegeState_Forbidden;
	  	}
	}
	return (isAllowed) ? (PRInt16)nsIPrivilege::PrivilegeState_Allowed : (PRInt16)nsIPrivilege::PrivilegeState_Blank;
}

NS_IMETHODIMP
nsPrivilegeManager::GetPrincipalPrivilege(nsITarget * target, nsIPrincipal * prin, void * data, nsIPrivilege * * result)
{
	if ( (prin == NULL) || (target == NULL) ) {
		* result = nsPrivilegeManager::FindPrivilege(nsIPrivilege::PrivilegeState_Blank, nsIPrivilege::PrivilegeDuration_Session);
		return NS_OK;
	}
//	if (nsPrincipalManager::GetPrincipalManager()->GetSystemPrincipal() == prin) {
//		* result = nsPrivilegeManager::FindPrivilege(nsIPrivilege::PrivilegeState_Allowed, nsIPrivilege::PrivilegeDuration_Session);
//		return NS_OK;
//	}
	PrincipalKey prinKey(prin);
	nsPrivilegeTable *privTable = (nsPrivilegeTable *) itsPrinToPrivTable->Get(&prinKey);
	if (privTable == NULL) {
		* result = nsPrivilegeManager::FindPrivilege(nsIPrivilege::PrivilegeState_Blank, nsIPrivilege::PrivilegeDuration_Session);
		return NS_OK;
	}
	nsITarget * tempTarget = nsTarget::FindTarget(target);
	if (tempTarget != target) {
		* result = nsPrivilegeManager::FindPrivilege(nsIPrivilege::PrivilegeState_Blank, nsIPrivilege::PrivilegeDuration_Session);
		return NS_OK;
	}
	* result = privTable->Get(target);
	return NS_OK;
}

PRBool 
nsPrivilegeManager::IsPermissionGranted(nsITarget * target, nsIPrincipalArray * callerPrinArray, void * data)
{
	PRInt16 privilegeState = this->GetPrincipalPrivilege(target, callerPrinArray, data);
	switch(privilegeState) {
		case nsIPrivilege::PrivilegeState_Allowed: return PR_TRUE;
		default: PR_ASSERT(PR_FALSE);
		case nsIPrivilege::PrivilegeState_Forbidden:
		case nsIPrivilege::PrivilegeState_Blank: return PR_FALSE;
	}
}

char *
nsPrivilegeManager::CheckPrivilegeEnabled(nsTargetArray * targetArray, PRInt32 callerDepth, void *data)
{
	return this->CheckPrivilegeEnabled(NULL, targetArray, callerDepth, data);
}

char *
nsPrivilegeManager::CheckPrivilegeEnabled(nsIScriptContext * context, nsTargetArray * targetArray, PRInt32 callerDepth, void *data)
{
	/*
	struct NSJSJavaFrameWrapper *wrapper = NULL;
	nsTarget *target;
	nsPrivilegeTable *annotation;
	nsPrivilege *privilege;
	nsIPrincipalArray * prinArray = theUnknownPrincipalArray;
	int depth = 0;
	nsPermissionState perm;
	nsPermissionState scopePerm;
	nsPermissionState prinPerm;
	PRBool saw_non_system_code = PR_FALSE;
	PRBool saw_unsigned_principal = PR_FALSE;
	PRInt32 noOfTargets;
	PRInt32 idx;
	char *errMsg = NULL; 
	nsIPrincipal * principal;
	PRInt32 noOfPrincipals;
	PRBool saw_dangerous_code = PR_FALSE;
	if (targetArray == NULL) return "internal error - null target array";
	if (nsCapsNewNSJSJavaFrameWrapperCallback == NULL) return NULL;
	wrapper = (* nsCapsNewNSJSJavaFrameWrapperCallback)(context);
	if (wrapper == NULL) return NULL;
	noOfTargets = targetArray->GetSize();
	for ((*nsCapsGetStartFrameCallback)(wrapper); 
		(!(*nsCapsIsEndOfFrameCallback)(wrapper));
		)
		if ((*nsCapsIsValidFrameCallback)(wrapper)) {
			if (depth >= callerDepth) {
				scopePerm = prinPerm = nsIPrivilege::PrivilegeState_Blank;
				for (idx = 0; idx < noOfTargets; idx++) {
					target = (nsTarget *)targetArray->Get(idx);
					if (!target) {
						errMsg = "internal error - a null target in the Array";
						goto done;
					}
					annotation =(nsPrivilegeTable *) (*nsCapsGetAnnotationCallback)(wrapper);
					prinArray = (nsIPrincipalArray *) (*nsCapsGetPrincipalArrayCallback)(wrapper);
				
			 * When the Registration Mode flag is enabled, we allow secure
			 * operations if and only iff the principal codebase is 'file:'.
			 * That means we load files only after recognizing that they
			 * reside on local harddrive. Any other code is considered as
			 * dangerous and an exception will be thrown in such cases.
					if ((nsCapsGetRegistrationModeFlag()) && (prinArray != NULL)){
						noOfPrincipals = prinArray->GetSize();
						for (idx=0; idx < noOfPrincipals; idx++){
							principal = (nsIPrincipal *) prinArray->Get(idx);
//XXX ARIEL: figure this out
//							if (!(principal->isFileCodeBase())){
//								saw_dangerous_code = PR_TRUE;
//								errMsg = "access to target Forbidden - Illegal url code base is detected";
//								goto done;
							}
						}
					}
 * frame->annotation holds a PrivilegeTable, describing
 * the scope privileges of this frame.  We'll check
 * if it permits the target, and if so, we just return.
 * If it forbids the target, we'll throw an exception,
 * and return.  If it's blank, things get funny.
 *
 * In the blank case, we need to continue walking up
 * the stack, looking for a non-blank privilege.  To
 * prevent "luring" attacks, these blank frames must
 * have the ability that they *could have requested*
 * privilege, but actually didn't.  This property
 * insures that we don't have a non-permitted (attacker)
 * class somewhere in the call chain between the request
 * for scope privilege and the chedk for privilege.

 * If there isn't a annotation, then we assume the default
 * value of blank and avoid allocating a new annotation.
				if (annotation) {
					privilege = annotation->Get(target);
					PR_ASSERT(privilege != NULL);
					perm = privilege->etPermission();
					scopePerm = nsPrivilege::add(perm, scopePerm);
				}
				if (prinArray != NULL) {
XXX: We need to allow sub-classing of Target, so that
 * we would call the method on Developer Sub-class'ed Target.
 * That would allow us to implement Parameterized targets
 * May be checkPrivilegeEnabled should go back into Java.
					privilege = target->CheckPrivilegeEnabled(prinArray,data);
					PR_ASSERT(privilege != NULL);
					perm = privilege->getPermission();
					scopePerm = nsPrivilege::add(perm, scopePerm);
					perm = this->GetPrincipalPrivilege(target,prinArray,data);
					prinPerm = nsPrivilege::add(perm, prinPerm);
					(!nsPrivilegeManager::HasSystemPrincipal(prinArray))
					? saw_non_system_code = PR_TRUE : saw_unsigned_principal = PR_TRUE;
				}
			}
			if (scopePerm == nsIPrivilege::PrivilegeState_Allowed) goto done;
			if ((scopePerm == nsIPrivilege::PrivilegeState_Forbidden) ||
				(saw_non_system_code && 
				(prinPerm != nsIPrivilege::PrivilegeState_Allowed))) {
				errMsg = "access to target forbidden";
				goto done;
			}
		}
	}
//	if (!(* nsCapsGetNextFrameCallback)(wrapper, &depth)) break;
 * If we get here, there is no non-blank capability on the stack,
 * and there is no ClassLoader, thus give privilege for now
	if (saw_non_system_code) {
		errMsg = "access to target forbidden. Target was not enabled on stack (stack included non-system code)";
		goto done;
	}
	if (CMGetBoolPref("signed.applets.local_classes_have_30_powers")) goto done;
	errMsg =  "access to target forbidden. Target was not enabled on stack (stack included only system code)";
done:
 * If the Registration Mode flag is set and principals have
 * 'file:' code base, we set the error message to NULL.
	if ((nsCapsGetRegistrationModeFlag()) && !(saw_dangerous_code)) errMsg = NULL;
	(*nsCapsFreeNSJSJavaFrameWrapperCallback)(wrapper);
	return errMsg;
	*/
	return NULL;
}
/*
nsIPrincipalArray *
nsPrivilegeManager::GetClassPrincipalsFromStack(PRInt32 callerDepth)
{
  return this->GetClassPrincipalsFromStack(NULL, callerDepth);
}

nsIPrincipalArray *
nsPrivilegeManager::GetClassPrincipalsFromStack(void* context, PRInt32 callerDepth)
{
  nsIPrincipalArray * principalArray = theUnknownPrincipalArray;
  int depth = 0;
  struct NSJSJavaFrameWrapper *wrapper = NULL;
  if (nsCapsNewNSJSJavaFrameWrapperCallback == NULL) return NULL;
  wrapper = (*nsCapsNewNSJSJavaFrameWrapperCallback)(context);
  if (wrapper == NULL) return NULL;
  for ((*nsCapsGetStartFrameCallback)(wrapper) ; 
       (!(*nsCapsIsEndOfFrameCallback)(wrapper)) ;
       ) {
    if ((*nsCapsIsValidFrameCallback)(wrapper)) {
      if (depth >= callerDepth) {
        principalArray = (nsIPrincipalArray *) (*nsCapsGetPrincipalArrayCallback)(wrapper);
	break;
      }
    }
    if (!(*nsCapsGetNextFrameCallback)(wrapper, &depth)) break;
  }
  (*nsCapsFreeNSJSJavaFrameWrapperCallback)(wrapper);
  return principalArray;
}
*/

nsPrivilegeTable * 
nsPrivilegeManager::GetPrivilegeTableFromStack(PRInt32 callerDepth, PRBool createIfNull)
{
  return this->GetPrivilegeTableFromStack(NULL, callerDepth, createIfNull);
}

nsPrivilegeTable * 
nsPrivilegeManager::GetPrivilegeTableFromStack(nsIScriptContext * context, PRInt32 callerDepth, PRBool createIfNull)
{
	nsPrivilegeTable *privTable = NULL;
	int depth = 0;
	struct NSJSJavaFrameWrapper *wrapper = NULL;
	nsPrivilegeTable *annotation;
	if (nsCapsNewNSJSJavaFrameWrapperCallback == NULL) return NULL;
	wrapper = (*nsCapsNewNSJSJavaFrameWrapperCallback)(context);
	if (wrapper == NULL) return NULL;
	for ((*nsCapsGetStartFrameCallback)(wrapper) ; 
		(!(*nsCapsIsEndOfFrameCallback)(wrapper)) ;
		) {
		if ((*nsCapsIsValidFrameCallback)(wrapper)) {
			if (depth >= callerDepth) {
/*
 * it's possible for the annotation to be NULL, meaning
 * nobody's ever asked for it before.  The correct
 * response is to create a new PrivilegeTable (with
 * default "blank forever" privileges), assign that
 * to the annotation, and return it.
 */
				annotation = (nsPrivilegeTable *) (*nsCapsGetAnnotationCallback)(wrapper);
				if (createIfNull && annotation == NULL) {
					privTable = new nsPrivilegeTable();
					if (privTable == NULL) break;
					PR_ASSERT(privTable != NULL);
					(*nsCapsSetAnnotationCallback)(wrapper, privTable);
				} else privTable = annotation;
				break;
			}
		}
		if (!(*nsCapsGetNextFrameCallback)(wrapper, &depth)) break;
	}
	(*nsCapsFreeNSJSJavaFrameWrapperCallback)(wrapper);
	return privTable;
}

#ifdef ENABLE_RDF
static JSec_Principal 
RDF_CreatePrincipal(nsIPrincipal *prin)
{
  /* For certificate principals we should convert the key into string, because 
   * key could have NULL characters before we store it in RDF. 
   */
  char *key = prin->getKey();
  PRUint32 keyLen = prin->getKeyLength();
  char *prinName = PL_Base64Encode(key, keyLen, NULL);
  JSec_Principal pr = RDFJSec_NewPrincipal(prinName);
  char buf[256];
  XP_SPRINTF(buf, "%d", keyLen);
  RDFJSec_SetPrincipalAttribute(pr, "keyLen", (void *)buf);
  PRInt16 * prinType = prin->getType();
  XP_SPRINTF(buf, "%d", type);
  RDFJSec_SetPrincipalAttribute(pr, "type", (void *)buf);
  RDFJSec_AddPrincipal(pr);
  return pr;
}
#endif /* ENABLE_RDF */


static PRBool 
RDF_RemovePrincipal(nsIPrincipal *prin)
{
  PRBool found = PR_FALSE;

#ifdef ENABLE_RDF
  nsCaps_lock();
  RDFJSec_InitPrivilegeDB();

  RDF_Cursor prin_cursor = RDFJSec_ListAllPrincipals();
  if (prin_cursor == NULL) {
    nsCaps_unlock();
    return PR_FALSE;
  }
  
  JSec_Principal jsec_prin;
  nsIPrincipal *cur_prin = NULL;
  while ((jsec_prin = RDFJSec_NextPrincipal(prin_cursor)) != NULL) {
    if ((cur_prin = RDF_getPrincipal(jsec_prin)) == NULL) {
      continue;
    }
    if (prin->equals(cur_prin)) {
      found = PR_TRUE;
      break;
    }
  }

  RDFJSec_ReleaseCursor(prin_cursor);
  if (found) {
    RDFJSec_DeletePrincipal(jsec_prin);
  }
  nsCaps_unlock();

#endif /* ENABLE_RDF */
  return found;
}


#ifdef ENABLE_RDF

static nsIPrincipal *
RDF_getPrincipal(JSec_Principal jsec_pr)
{
  char *enc_key = RDFJSec_PrincipalID(jsec_pr);
  char *key = PL_Base64Decode(enc_key, 0, NULL);
  char *key_len_str = (char *)RDFJSec_AttributeOfPrincipal(jsec_pr, "keyLen");
  PRUint32 keyLen;
  sscanf(key_len_str, "%d", &keyLen);
  PRInt32 type_int;
  PRInt16 * prinType;
  char *type_str = (char *)RDFJSec_AttributeOfPrincipal(jsec_pr, "type");
  sscanf(type_str, "%d", &type_int);
  prinType = (PRInt16 *)type_int;
//  nsIPrincipal *prin = new nsIPrincipal(type, key, keyLen);
  PR_Free(key);
  PR_Free(enc_key);
  return prin;
}

static JSec_Target 
RDF_CreateTarget(nsITarget * target)
{
  char *targetName = target->getName();
  nsIPrincipal *prin = target->getPrincipal();
  JSec_Principal pr = RDF_CreatePrincipal(prin);
  return RDFJSec_NewTarget(targetName, pr);
}

static nsITarget *
RDF_getTarget(JSec_Target jsec_target)
{
	char * targetName = RDFJSec_GetTargetName(jsec_target);
	return nsTarget::FindTarget(targetName);
}
#endif /* ENABLE_RDF */

static PRBool 
RDF_RemovePrincipalsPrivilege(nsIPrincipal * prin, nsITarget * target)
{
  PRBool found = PR_FALSE;
#ifdef ENABLE_RDF
  nsCaps_lock();
  RDFJSec_InitPrivilegeDB();
  RDF_Cursor prin_cursor = RDFJSec_ListAllPrincipals();
  if (prin_cursor == NULL) {
    nsCaps_unlock();
    return PR_FALSE;
  }
  JSec_Principal jsec_prin;
  nsIPrincipal *cur_prin = NULL;
  JSec_PrincipalUse jsec_pr_use = NULL;
  while ((jsec_prin = RDFJSec_NextPrincipal(prin_cursor)) != NULL) {
    /* Find the principal */
    if ((cur_prin = RDF_getPrincipal(jsec_prin)) == NULL) {
      continue;
    }
    if (!cur_prin->equals(prin)) continue;
    /* Found the principal. Find the target for this principal */ 
    RDF_Cursor prin_use_cursor = RDFJSec_ListAllPrincipalUses(jsec_prin);
    while ((jsec_pr_use = RDFJSec_NextPrincipalUse(prin_use_cursor)) != NULL) {
      JSec_Target jsec_target = RDFJSec_TargetOfPrincipalUse(jsec_pr_use);
      if (jsec_target == NULL) continue;
      nsTarget *cur_target = RDF_getTarget(jsec_target);
      if ((cur_target == NULL) || (!cur_target->equals(target))) continue;
      found = PR_TRUE;
      break;
    }
    RDFJSec_ReleaseCursor(prin_use_cursor);
    if (found) break;
  }

  RDFJSec_ReleaseCursor(prin_cursor);
  if (found) RDFJSec_DeletePrincipalUse(jsec_prin, jsec_pr_use);
  nsCaps_unlock();
#endif /* ENABLE_RDF */
  return found;
}


/* The following methods are used to save and load the persistent store */

void 
nsPrivilegeManager::Save(nsIPrincipal * prin, nsITarget *target, nsIPrivilege *newPrivilege)
{
	PRBool eq;
	prin->Equals(nsPrincipalManager::GetPrincipalManager()->GetSystemPrincipal(),& eq);
	if (eq) return;
#ifdef ENABLE_RDF
	nsCaps_lock();
	RDFJSec_InitPrivilegeDB();
	JSec_Principal pr = RDF_CreatePrincipal(prin);
	JSec_Target tr = RDF_CreateTarget(target);
	char *privilege = newPrivilege->toString();
	JSec_PrincipalUse prUse = RDFJSec_NewPrincipalUse(pr, tr, privilege);
	RDFJSec_AddPrincipalUse(pr, prUse);
	nsCaps_unlock();
#endif /* ENABLE_RDF */
}

/* The following routine should be called after setting up the system targets 
 * XXX: May be we should add a PR_ASSERT for that.
 */
void 
nsPrivilegeManager::Load(void)
{
#ifdef ENABLE_RDF
	nsCaps_lock();
	RDFJSec_InitPrivilegeDB();
	RDF_Cursor prin_cursor = RDFJSec_ListAllPrincipals();
	if (prin_cursor == NULL) {
	  nsCaps_unlock();
	  return;
	}
	JSec_Principal jsec_prin;
	nsIPrincipal * prin;
	while ((jsec_prin = RDFJSec_NextPrincipal(prin_cursor)) != NULL) {
		if ((prin = RDF_getPrincipal(jsec_prin)) == NULL) continue;
		RDF_Cursor prin_use_cursor = RDFJSec_ListAllPrincipalUses(jsec_prin);
		JSec_PrincipalUse jsec_pr_use;
		while ((jsec_pr_use = RDFJSec_NextPrincipalUse(prin_use_cursor)) != NULL) {
			char *privilege_str = RDFJSec_PrivilegeOfPrincipalUse(jsec_pr_use);
			if (privilege_str == NULL) continue;
			JSec_Target jsec_target = RDFJSec_TargetOfPrincipalUse(jsec_pr_use);
			if (jsec_target == NULL) continue;
			nsITarget *target = RDF_getTarget(jsec_target);
			if (target == NULL) continue;
			nsPrivilege *privilege = nsPrivilege::findPrivilege(privilege_str);
			registerPrincipalAndSetPrivileges(prin, target, privilege);
		}
		RDFJSec_ReleaseCursor(prin_use_cursor);
	}
	RDFJSec_ReleaseCursor(prin_cursor);
	nsCaps_unlock();
#endif /* ENABLE_RDF */
}


PRBool 
nsPrivilegeManagerInitialize(void) 
{
	thePrivilegeManager = new nsPrivilegeManager();
	PRInt16 privState = 0, durationState = 0;
	for (PRInt16 i = 0; i < nsIPrivilege::PrivilegeState_NumberOfPrivileges; i++) {
		for(PRInt16 j = 0; j < nsIPrivilege::PrivilegeDuration_NumberOfDurations; j++) {
			privState = i;
			durationState = j;
			thePrivilegeCache[i][j] = new nsPrivilege(privState, durationState);
		}
	}
#ifdef ENABLE_RDF
  RDFJSec_InitPrivilegeDB();
#endif /* ENABLE_RDF */

  return PR_TRUE;
}

PRBool nsPrivilegeManager::theInited = nsPrivilegeManagerInitialize();
