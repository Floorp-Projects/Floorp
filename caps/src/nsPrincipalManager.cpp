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
/*creates, registers, and performs logical operations on principals*/
#include "prmem.h"
#include "prmon.h"
#include "prlog.h"
#include "prprf.h"
#include "plbase64.h"
#include "plstr.h"
#include "nsPrincipalManager.h"
#include "nsPrincipalArray.h"
#include "nsCaps.h"
#include "nsCertificatePrincipal.h"
#include "nsCodebasePrincipal.h"
#include "nsPrivilegeManager.h"

#define UNSIGNED_PRINCIPAL_KEY "4a:52:4f:53:4b:49:4e:44"
#define UNKNOWN_PRINCIPAL_KEY "52:4f:53:4b:49:4e:44:4a"

static nsIPrincipal * theSystemPrincipal = NULL;
static nsIPrincipal * theUnsignedPrincipal = NULL;
static nsIPrincipal * theUnknownPrincipal = NULL;
static nsIPrincipalArray * theUnknownPrincipalArray = NULL;
static nsIPrincipalArray * theUnsignedPrincipalArray = NULL;
char * gListOfPrincipals;


static PRBool RDF_RemovePrincipal(nsIPrincipal * prin);
static PRBool GetPrincipalString(nsHashKey * aKey, void * aData, void * closure);

static NS_DEFINE_IID(kIPrincipalManagerIID, NS_IPRINCIPALMANAGER_IID);

NS_IMPL_ISUPPORTS(nsPrincipalManager, kIPrincipalManagerIID);

nsIPrincipal * 
nsPrincipalManager::GetSystemPrincipal(void)
{
	return theSystemPrincipal;
}
nsIPrincipal * 
nsPrincipalManager::GetUnsignedPrincipal(void)
{
	return theUnsignedPrincipal;
}

nsIPrincipal * 
nsPrincipalManager::GetUnknownPrincipal(void)
{
	return theUnknownPrincipal;
}

PRBool 
nsPrincipalManager::HasSystemPrincipal(nsIPrincipalArray * prinArray)
{
	nsIPrincipal * sysPrin = nsPrincipalManager::GetSystemPrincipal();
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

NS_IMETHODIMP
nsPrincipalManager::CreateCodebasePrincipal(const char * codebaseURL, nsIPrincipal * * prin) {
	* prin = new nsCodebasePrincipal(nsIPrincipal::PrincipalType_CodebaseExact, codebaseURL);
	if (prin == NULL) return NS_ERROR_OUT_OF_MEMORY;
	(* prin)->AddRef();
	return NS_OK;
}

NS_IMETHODIMP
nsPrincipalManager::CreateCertificatePrincipal(const unsigned char * * certChain, PRUint32 * certChainLengths, PRUint32 noOfCerts, nsIPrincipal * * prin)
{
	* prin = new nsCertificatePrincipal(nsIPrincipal::PrincipalType_Certificate,
											 certChain, certChainLengths, noOfCerts);
	if (!prin) return NS_ERROR_OUT_OF_MEMORY;
	(* prin)->AddRef();
	return NS_OK;
}


NS_IMETHODIMP
nsPrincipalManager::CanExtendTrust(nsIPrincipalArray * from, nsIPrincipalArray * to, PRBool * result)
{
	if ((from == NULL) || (to == NULL)) {
		* result = PR_FALSE;
		return NS_OK;
	}
	nsIPrincipalArray * intersect;
	from->IntersectPrincipalArray(to,& intersect);
	PRUint32 intersectSize = 0, fromSize = 0;
	intersect->GetPrincipalArraySize(& intersectSize);
	from->GetPrincipalArraySize(& fromSize);
	if (intersectSize == fromSize) {
		* result = PR_TRUE;
		return NS_OK;
	}
	if (intersectSize == 0 || (intersectSize != (fromSize - 1))) return PR_FALSE;
	nsIPrincipal * prin;
	PRUint32 i;
	for (i=0; i < intersectSize; i++) {
		intersect->GetPrincipalArrayElement(i, & prin);
		PRInt16 prinType = nsIPrincipal::PrincipalType_Unknown;
		prin->GetType(& prinType);
		if (prinType == nsIPrincipal::PrincipalType_CodebaseExact ||
			prinType == nsIPrincipal::PrincipalType_CodebaseRegex) {
			* result = PR_FALSE;
			return NS_OK;
		}
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
	* result = (codebaseCount == 1) ? PR_TRUE : PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP
nsPrincipalManager::CheckMatchPrincipal(nsIScriptContext * context, nsIPrincipal * prin, PRInt32 callerDepth, PRBool * result)
{
	nsIPrincipalArray * prinArray = new nsPrincipalArray();
	prinArray->AddPrincipalArrayElement(prin);
	nsIPrincipalArray * classPrinArray = this->GetClassPrincipalsFromStack(context, callerDepth);
	PRInt16 compType = 0;
	prinArray->ComparePrincipalArray(classPrinArray,& compType);
	* result = (compType != nsPrincipalArray::SetComparisonType_NoSubset) ? PR_TRUE : PR_FALSE;
	return NS_OK;
}
nsIPrincipalArray *
nsPrincipalManager::GetClassPrincipalsFromStack(PRInt32 callerDepth)
{
	return this->GetClassPrincipalsFromStack(NULL, callerDepth);
}

nsIPrincipalArray *
nsPrincipalManager::GetClassPrincipalsFromStack(nsIScriptContext * context, PRInt32 callerDepth)
{
	nsIPrincipalArray * principalArray = theUnknownPrincipalArray;
	int depth = 0;
	struct NSJSJavaFrameWrapper * wrapper = NULL;
	if (nsCapsNewNSJSJavaFrameWrapperCallback == NULL) return NULL;
	wrapper = (* nsCapsNewNSJSJavaFrameWrapperCallback)((void *)context);
	if (wrapper == NULL) return NULL;
	for ((* nsCapsGetStartFrameCallback)(wrapper);(!(* nsCapsIsEndOfFrameCallback)(wrapper));) {
		if ((* nsCapsIsValidFrameCallback)(wrapper)) {
			if (depth >= callerDepth) {
				principalArray = (nsIPrincipalArray *) (* nsCapsGetPrincipalArrayCallback)(wrapper);
				break;
			}
		}
		if (!(* nsCapsGetNextFrameCallback)(wrapper, &depth)) break;
	}
	(* nsCapsFreeNSJSJavaFrameWrapperCallback)(wrapper);
	return principalArray;
}

nsIPrincipalArray *
nsPrincipalManager::GetMyPrincipals(PRInt32 callerDepth)
{
	return nsPrincipalManager::GetMyPrincipals(NULL, callerDepth);
}

nsIPrincipalArray *
nsPrincipalManager::GetMyPrincipals(nsIScriptContext * context, PRInt32 callerDepth)
{
	return nsPrincipalManager::GetPrincipalManager()->GetClassPrincipalsFromStack(context, callerDepth);
}



nsIPrincipal * 
nsPrincipalManager::GetPrincipalFromString(char * prinName)
{
	StringKey prinNameKey(prinName);
	nsCaps_lock();
	nsIPrincipal * prin = (nsIPrincipal *)itsPrinNameToPrincipalTable->Get(& prinNameKey);
	nsCaps_unlock();
	return prin;
}

void 
nsPrincipalManager::AddToPrincipalNameToPrincipalTable(nsIPrincipal * prin)
{
	char * prinName;
	prin->ToString(& prinName);
	if (prinName == NULL) return;
	StringKey prinNameKey(prinName);
	nsCaps_lock();
	if (!itsPrinNameToPrincipalTable->Get(& prinNameKey))
		itsPrinNameToPrincipalTable->Put(& prinNameKey, prin);
	nsCaps_unlock();

}

void 
nsPrincipalManager::RemoveFromPrincipalNameToPrincipalTable(nsIPrincipal * prin) 
{
	char * prinName;
	prin->ToString(& prinName);
	StringKey prinNameKey(prinName);
	nsCaps_lock();
	itsPrinNameToPrincipalTable->Remove(& prinNameKey);
	nsCaps_unlock();
}

static PRBool 
GetPrincipalString(nsHashKey * aKey, void * aData, void * closure) 
{
	/* Admin UI */
	/* XXX: Ignore empty strings */
	const char *string = ((StringKey *) aKey)->itsString;
	gListOfPrincipals = (!gListOfPrincipals)
	? PR_sprintf_append(gListOfPrincipals, "\"%s\"", string)
	: PR_sprintf_append(gListOfPrincipals, ",\"%s\"", string);
	return PR_TRUE;
}

const char * 
nsPrincipalManager::GetAllPrincipalsString(void)
{
	/* Admin UI */
	char *principalStrings=NULL;
	if (itsPrinNameToPrincipalTable == NULL) return NULL;
	nsCaps_lock();
	gListOfPrincipals = NULL;
	itsPrinNameToPrincipalTable->Enumerate(GetPrincipalString);
	if (gListOfPrincipals) {
	  principalStrings = PL_strdup(gListOfPrincipals);
	  PR_Free(gListOfPrincipals);
	}
	gListOfPrincipals = NULL;
	nsCaps_unlock();
	return principalStrings;
}

void 
nsPrincipalManager::RegisterSystemPrincipal(nsIPrincipal * prin)
{
	nsPrivilegeManager * itsPrivilegeManager = nsPrivilegeManager::GetPrivilegeManager();
	PrincipalKey prinKey(prin);
	nsCaps_lock();
	if (!itsPrivilegeManager->itsPrinToPrivTable->Get(& prinKey)) 
		itsPrivilegeManager->itsPrinToPrivTable->Put(& prinKey, new nsSystemPrivilegeTable());
	if (!itsPrivilegeManager->itsPrinToMacroTargetPrivTable->Get(& prinKey)) 
		itsPrivilegeManager->itsPrinToMacroTargetPrivTable->Put(& prinKey, new nsSystemPrivilegeTable());
	theSystemPrincipal = prin;
	CreateSystemTargets(prin);
	// Load the signed applet's ACL from the persistence store
	itsPrivilegeManager->Load();
	nsCaps_unlock();
}

NS_IMETHODIMP
nsPrincipalManager::RegisterPrincipal(nsIPrincipal * prin)
{
	nsPrivilegeManager * itsPrivilegeManager = nsPrivilegeManager::GetPrivilegeManager();
	PrincipalKey prinKey(prin);
	nsCaps_lock();
	if (!itsPrivilegeManager->itsPrinToPrivTable->Get(& prinKey)) 
		itsPrivilegeManager->itsPrinToPrivTable->Put(&prinKey, new nsPrivilegeTable());
	if (!itsPrivilegeManager->itsPrinToMacroTargetPrivTable->Get(& prinKey)) {
		itsPrivilegeManager->itsPrinToMacroTargetPrivTable->Put(& prinKey, new nsPrivilegeTable());
	}
	this->AddToPrincipalNameToPrincipalTable(prin);
	nsCaps_unlock();
	return NS_OK;
}

NS_IMETHODIMP
nsPrincipalManager::UnregisterPrincipal(nsIPrincipal * prin, PRBool * result)
{
	nsPrivilegeManager * itsPrivilegeManager = nsPrivilegeManager::GetPrivilegeManager();
	PRBool eq;
	prin->Equals(this->GetSystemPrincipal(),& eq);
	if (result) {
		* result = PR_FALSE;
		return NS_OK;
	}
	PrincipalKey prinKey(prin);
	nsCaps_lock();
	/* Get the privilegetables and free them up */
	nsPrivilegeTable * pt =
		(nsPrivilegeTable *)itsPrivilegeManager->itsPrinToPrivTable->Get(& prinKey);
	nsPrivilegeTable * mpt =
		(nsPrivilegeTable *)itsPrivilegeManager->itsPrinToMacroTargetPrivTable->Get(& prinKey);
	if(pt) delete pt;
	if(mpt) delete mpt;
	/* Remove the principal */
	void * old_prin = itsPrivilegeManager->itsPrinToPrivTable->Remove(& prinKey);
	void * old_prin1 = itsPrivilegeManager->itsPrinToMacroTargetPrivTable->Remove(& prinKey);
	/* remove principal from PrinNameToPrincipalTable */
	this->RemoveFromPrincipalNameToPrincipalTable(prin);
	/* remove the principal from RDF also */
	RDF_RemovePrincipal(prin);
	nsCaps_unlock();
	* result = ((old_prin == NULL) && (old_prin1 == NULL)) ? PR_FALSE : PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP
nsPrincipalManager::NewPrincipalArray(PRUint32 count, nsIPrincipalArray * * result)
{
	* result = (nsIPrincipalArray *) new nsPrincipalArray(count);
	return NS_OK;
}



nsPrincipalManager::nsPrincipalManager(void)
{
	NS_INIT_REFCNT();
	NS_ADDREF(this);
	nsCaps_lock();
	itsPrinNameToPrincipalTable = new nsHashtable();
	theUnsignedPrincipal = new nsCertificatePrincipal(nsIPrincipal::PrincipalType_Certificate, UNSIGNED_PRINCIPAL_KEY);
	theUnsignedPrincipalArray = new nsPrincipalArray();
	theUnsignedPrincipalArray->AddPrincipalArrayElement(theUnsignedPrincipal);
	theUnknownPrincipal = new nsCertificatePrincipal(nsIPrincipal::PrincipalType_Certificate, UNKNOWN_PRINCIPAL_KEY);
	theUnknownPrincipalArray = new nsPrincipalArray();
	theUnknownPrincipalArray->AddPrincipalArrayElement(theUnknownPrincipal);
	nsCaps_unlock();
}

nsPrincipalManager::~nsPrincipalManager(void) {
	nsCaps_lock();
	if (itsPrinNameToPrincipalTable) delete itsPrinNameToPrincipalTable;
	nsCaps_unlock();
}

nsPrincipalManager *
nsPrincipalManager::GetPrincipalManager()
{
	static nsPrincipalManager * prinMan = NULL;
	if(!prinMan)
		prinMan = new nsPrincipalManager();
	return prinMan;
}

static PRBool 
RDF_RemovePrincipal(nsIPrincipal * prin)
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
	nsIPrincipal * cur_prin = NULL;
	while ((jsec_prin = RDFJSec_NextPrincipal(prin_cursor)) != NULL) {
		if ((cur_prin = RDF_getPrincipal(jsec_prin)) == NULL) continue;
		PRBool eq;
		prin->Equals(cur_prin,& eq);
		if (eq) {
			found = PR_TRUE;
			break;
		}
	}
	RDFJSec_ReleaseCursor(prin_cursor);
	if (found) RDFJSec_DeletePrincipal(jsec_prin);
	nsCaps_unlock();
#endif /* ENABLE_RDF */
	return found;
}