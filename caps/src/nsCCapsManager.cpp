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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsIComponentManager.h"
#include "nsCCapsManager.h"
#include "nsCodebasePrincipal.h"
#include "nsCertificatePrincipal.h"
#include "nsCaps.h"
#include "nsICapsSecurityCallbacks.h"
#include "nsLoadZig.h"
#include "secnav.h"
#ifdef MOZ_SECURITY
#include "navhook.h"
#include "jarutil.h"
#endif /* MOZ_SECURITY */

static NS_DEFINE_CID(kCCapsManagerCID, NS_CCAPSMANAGER_CID);
static NS_DEFINE_IID(kICapsManagerIID, NS_ICAPSMANAGER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

#define ALL_JAVA_PERMISSION "AllJavaPermission"

NS_IMPL_AGGREGATED(nsCCapsManager);

NS_METHOD
nsCCapsManager::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (aIID.Equals(kISupportsIID)) {
      *aInstancePtr = GetInner();
      AddRef();
      return NS_OK;
    }
    if (aIID.Equals(kICapsManagerIID)) {
        *aInstancePtr = this;
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}



////////////////////////////////////////////////////////////////////////////
// from nsICapsManager:

NS_METHOD
nsCCapsManager::CreateCodebasePrincipal(const char *codebaseURL, 
                                        nsIPrincipal** prin)
{
   nsresult result = NS_OK;
   nsCodebasePrincipal *pNSCCodebasePrincipal = 
       new nsCodebasePrincipal((PRInt16 *)nsIPrincipal::PrincipalType_CodebaseExact, codebaseURL);
   if (pNSCCodebasePrincipal == NULL)
   {
      return NS_ERROR_OUT_OF_MEMORY;
   }
   pNSCCodebasePrincipal->AddRef();
   *prin = (nsIPrincipal *)pNSCCodebasePrincipal;
   return result;
}

NS_METHOD
nsCCapsManager::CreateCertificatePrincipal(const unsigned char **certChain, 
                                    PRUint32 *certChainLengths, 
                                    PRUint32 noOfCerts, 
                                    nsIPrincipal** prin)
{
   nsresult result = NS_OK;
   nsCertificatePrincipal *pNSCCertPrincipal = 
       new nsCertificatePrincipal((PRInt16 *)nsIPrincipal::PrincipalType_Certificate,
       							 certChain, certChainLengths, noOfCerts, &result);
   if (pNSCCertPrincipal == NULL)
   {
      return NS_ERROR_OUT_OF_MEMORY;
   }
   pNSCCertPrincipal->AddRef();
   *prin = (nsIPrincipal *)pNSCCertPrincipal;
   return NS_OK;
}

/**
* Returns the permission for given principal and target
*
* @param prin   - is either certificate principal or codebase principal
* @param target - is NS_ALL_PRIVILEGES.
* @param state  - the return value is passed in this parameter.
*/
NS_METHOD
nsCCapsManager::GetPermission(nsIPrincipal * prin, nsITarget * ignoreTarget, PRInt16 * privilegeState)
{
	* privilegeState = nsIPrivilege::PrivilegeState_Blank;
	nsITarget * target = nsTarget::FindTarget(ALL_JAVA_PERMISSION);
	nsresult result = NS_OK;
	if( target == NULL ) return NS_OK;
	if (privilegeManager != NULL) {
		nsIPrivilege * privilege = privilegeManager->GetPrincipalPrivilege(target, prin, NULL);
// ARIEL WORK ON THIS SHIT
//		* privilegeState = this->ConvertPrivilegeToPermission(privilege);
	}
	return NS_OK;
}

/**
* Set the permission state for given principal and target. This wouldn't 
* prompt the end user with UI.
*
* @param prin   - is either certificate principal or codebase principal
* @param target - is NS_ALL_PRIVILEGES.
* @param state  - is permisson state that should be set for the given prin
*                 and target parameters.
*/
NS_METHOD
nsCCapsManager::SetPermission(nsIPrincipal * prin, nsITarget * ignoreTarget, PRInt16 * privilegeState)
{
	nsITarget * target = nsTarget::FindTarget(ALL_JAVA_PERMISSION);
	if(target == NULL ) return NS_OK;
	if (privilegeManager != NULL) {
// WORK ON THIS ARIEL
//		nsPrivilege* privilege = this->ConvertPermissionToPrivilege(privilegeState);
//		privilegeManager->SetPermission(prin, target, privilegeState);
	}
	return NS_OK;
}

/**
* Prompts the user if they want to grant permission for the given principal and 
* for the given target. 
*
* @param prin   - is either certificate principal or codebase principal
* @param target - is NS_ALL_PRIVILEGES.
* @param result - is the permission user has given for the given principal and 
*                 target
*/
NS_METHOD
nsCCapsManager::AskPermission(nsIPrincipal * prin, nsITarget * ignoreTarget, PRInt16 * privilegeState)
{
	nsITarget *target = nsTarget::FindTarget(ALL_JAVA_PERMISSION);
	if( target == NULL ) {
	   * privilegeState = nsIPrivilege::PrivilegeState_Blank;
	   return NS_OK;
	}
	if (privilegeManager != NULL) {
	   privilegeManager->AskPermission(prin, target, NULL);
	   nsIPrivilege * privilege = privilegeManager->GetPrincipalPrivilege(target, prin, NULL);
  //	   * privilegeState = ConvertPrivilegeToPermission(privilege);
	}
	return NS_OK;
}

/**
 * Initializes the capabilities subsytem (ie setting the system principal, initializing
 * privilege Manager, creating the capsManager factory etc
 *
 * @param result - is true if principal was successfully registered with the system
 */
NS_METHOD
nsCCapsManager::Initialize(PRBool * result)
{
	*result = nsCapsInitialize();
	return NS_OK;
}

/**
	* Initializes the capabilities frame walking code.
	*
	* @param aInterface - interface for calling frame walking code.
	*/
NS_METHOD
nsCCapsManager::InitializeFrameWalker(nsICapsSecurityCallbacks* aInterface)
{
	//XXX write me  
	return NS_OK;
}

/**
 * Registers the given Principal with the system.
 *
 * @param prin   - is either certificate principal or codebase principal
 * @param result - is true if principal was successfully registered with the system
 */
NS_METHOD
nsCCapsManager::RegisterPrincipal(nsIPrincipal * prin)
{
	if (privilegeManager != NULL) privilegeManager->RegisterPrincipal(prin);
	return NS_OK;
}

/**
 * Prompts the user if they want to grant permission for the principal located
 * at the given stack depth for the given target. 
 *
 * @param context      - is the parameter JS needs to determinte the principal 
 * @param targetName   - is the name of the target.
 * @param callerDepth  - is the depth of JS stack frame, which JS uses to determinte the 
 *                       principal 
 * @param ret_val - is true if user has given permission for the given principal and 
 *                  target
 */
NS_METHOD
nsCCapsManager::EnablePrivilege(void* context, const char* targetName, PRInt32 callerDepth, PRBool *ret_val)
{
   nsITarget *target = nsTarget::FindTarget((char*)targetName);
   nsresult result = NS_OK;
   if( target == NULL )
   {
      *ret_val = PR_FALSE;
      return NS_OK;
   }
   if (privilegeManager != NULL)
      *ret_val = privilegeManager->EnablePrivilege(context, target, callerDepth);
   return NS_OK;
}

/**
 * Returns if the user granted permission for the principal located at the given 
 * stack depth for the given target. 
 *
 * @param context     - is the parameter JS needs to determinte the principal 
 * @param targetName  - is the name of the target.
 * @param callerDepth - is the depth of JS stack frame, which JS uses to determinte the 
 *                      principal 
 * @param ret_val - is true if user has given permission for the given principal and 
 *                 target
 */
NS_METHOD
nsCCapsManager::IsPrivilegeEnabled(void* context, const char* targetName, PRInt32 callerDepth, PRBool *ret_val)
{
   nsITarget *target = nsTarget::FindTarget((char*)targetName);
   nsresult result = NS_OK;
   if( target == NULL )
   {
      *ret_val = PR_FALSE;
      return NS_OK;
   }
   if (privilegeManager != NULL)
   {
      *ret_val = privilegeManager->IsPrivilegeEnabled(context, target, callerDepth);
   }
   return NS_OK;
}

/**
 * Reverts the permission (granted/denied) user gave for the principal located
 * at the given stack depth for the given target. 
 *
 * @param context      - is the parameter JS needs to determinte the principal 
 * @param targetName   - is the name of the target.
 * @param callerDepth  - is the depth of JS stack frame, which JS uses to determinte the 
 *                       principal 
 * @param ret_val - is true if user has given permission for the given principal and 
 *                 target
 */
NS_METHOD
nsCCapsManager::RevertPrivilege(void* context, const char* targetName, PRInt32 callerDepth, PRBool *ret_val)
{
	nsITarget *target = nsTarget::FindTarget((char*)targetName);
	nsresult result = NS_OK;
	if( target == NULL ) {
		* ret_val = PR_FALSE;
		return NS_OK;
	}
	if (privilegeManager != NULL)
		* ret_val = privilegeManager->RevertPrivilege(context, target, callerDepth);
	return NS_OK;
}

/**
 * Disable permissions for the principal located at the given stack depth for the 
 * given target. 
 *
 * @param context      - is the parameter JS needs to determinte the principal 
 * @param targetName   - is the name of the target.
 * @param callerDepth  - is the depth of JS stack frame, which JS uses to determinte the 
 *                       principal 
 * @param ret_val - is true if user has given permission for the given principal and 
 *                 target
 */
NS_METHOD
nsCCapsManager::DisablePrivilege(void* context, const char* targetName, PRInt32 callerDepth, PRBool *ret_val)
{
	nsITarget *target = nsTarget::FindTarget((char*)targetName);
	nsresult result = NS_OK;
	if( target == NULL ) {
		* ret_val = PR_FALSE;
		return NS_OK;
	}
	if (privilegeManager != NULL)
		* ret_val = privilegeManager->DisablePrivilege(context, target, callerDepth);
	return NS_OK;
}

/* XXX: Some of the arguments for the following interfaces may change.
 * This is a first cut. I need to talk to joki. We should get rid of void* parameters.
 */
NS_METHOD
nsCCapsManager::ComparePrincipalArray(void* prin1Array, void* prin2Array, PRInt16 * comparisonType)
{
	nsresult result = NS_OK;
	* comparisonType = nsPrivilegeManager::SetComparisonType_NoSubset;
	if (privilegeManager != NULL) {
		nsPrincipalArray * newPrin1Array=NULL;
		nsPrincipalArray * newPrin2Array=NULL;
		result = GetNSPrincipalArray((nsPrincipalArray*) prin1Array, &newPrin1Array);
		if (result != NS_OK) return result;
		result = GetNSPrincipalArray((nsPrincipalArray*) prin2Array, &newPrin2Array);
		if (result != NS_OK) return result;
		* comparisonType = privilegeManager->ComparePrincipalArray(newPrin1Array, newPrin2Array);
		nsCapsFreePrincipalArray(newPrin1Array);
		nsCapsFreePrincipalArray(newPrin2Array);
	}
	return NS_OK;
}

NS_METHOD
nsCCapsManager::IntersectPrincipalArray(void * prin1Array, void * prin2Array, void * * ret_val)
{
	nsresult result = NS_OK;
	*ret_val = NULL;
	if (privilegeManager != NULL) {
		nsPrincipalArray *newPrin1Array=NULL;
		nsPrincipalArray *newPrin2Array=NULL;
		nsPrincipalArray *intersectPrinArray=NULL;
		result = this->GetNSPrincipalArray((nsPrincipalArray*) prin1Array, &newPrin1Array);
		if (result != NS_OK) return result;
		result = this->GetNSPrincipalArray((nsPrincipalArray*) prin2Array, &newPrin2Array);
		if (result != NS_OK) return result;
		intersectPrinArray = privilegeManager->IntersectPrincipalArray(newPrin1Array, newPrin2Array);
		this->CreateNSPrincipalArray(intersectPrinArray, (nsPrincipalArray**)ret_val);
		nsCapsFreePrincipalArray(newPrin1Array);
		nsCapsFreePrincipalArray(newPrin2Array);
	}
	return NS_OK;
}

NS_METHOD
nsCCapsManager::CanExtendTrust(void * fromPrinArray, void * toPrinArray, PRBool * ret_val)
{
	nsresult result = NS_OK;
	if (privilegeManager != NULL) {
		nsPrincipalArray *newPrin1Array=NULL;
		nsPrincipalArray *newPrin2Array=NULL;
		result = this->GetNSPrincipalArray((nsPrincipalArray*) fromPrinArray, &newPrin1Array);
		if (result != NS_OK) return result;
		result = this->GetNSPrincipalArray((nsPrincipalArray*) toPrinArray, &newPrin2Array);
		if (result != NS_OK) return result;
		*ret_val = privilegeManager->CanExtendTrust(newPrin1Array, newPrin2Array);
		nsCapsFreePrincipalArray(newPrin1Array);
		nsCapsFreePrincipalArray(newPrin2Array);
	}
	return NS_OK;
}


/* interfaces for nsIPrincipal object, may be we should move some of them to nsIprincipal */
/**************
//Principals must be created by type, the nsPrincipal data member is deprecated
NS_METHOD
nsCCapsManager::NewPrincipal(PRInt16 *principalType, void* key, PRUint32 key_len, void *zig, nsIPrincipal** ret_val)
{
  nsIPrincipal* pNSIPrincipal;
  nsPrincipal *pNSPrincipal = new nsPrincipal(type, key, key_len, zig);
  if (pNSPrincipal->isCodebase()) {
      pNSIPrincipal = (nsIPrincipal*)new nsCodebasePrincipal(pNSPrincipal);
  } else {
      pNSIPrincipal = (nsIPrincipal*)new nsCertificatePrincipal(pNSPrincipal);
  }
  *ret_val = pNSIPrincipal;
  return NS_OK;
}
*********************/
//XXX: todo: This method is covered by to nsIPrincipal object, should not be part of caps
//XXX: nsPrincipal struct if deprecated, access as nsIPrincipal
//do not use IsCodebaseExact, Tostring, or any other of the principal specific objects from here

NS_METHOD
nsCCapsManager::NewPrincipalArray(PRUint32 count, void* *ret_val)
{
  *ret_val = nsCapsNewPrincipalArray(count);
  return NS_OK;
}

/*
 * CreateMixedPrincipalArray take codebase and  ZIG file information and returns a
 * pointer to an array of nsIPrincipal objects.
 */
NS_METHOD
nsCCapsManager::CreateMixedPrincipalArray(void *aZig, char* name, const char* codebase, void** result)
{
	/*
	*result = NULL;
	PRBool hasCodebase;
	int i;
	PRUint32 count;
	nsIPrincipal *principal;
	hasCodebase = (PRBool)codebase;
	count = codebase ? 1 : 0;
#if 0
	SOBITEM *item;
	ZIG_Context * zig_aCx = NULL;
	ZIG *zig = (ZIG*)aZig;
	if (zig && name) {
		if ((zig_aCx = SOB_find(zig, name, ZIG_SIGN)) != NULL) {
			int zig_count=0;
			while (SOB_find_next(zig_aCx, &item) >= 0) zig_count++;
			SOB_find_end(zig_aCx);
			count += zig_count;
		} else  zig = NULL;
	}
#endif
	if (count == 0) return NS_OK;
	NewPrincipalArray(count, result);
	if (*result == NULL) return NS_ERROR_FAILURE;
#if 0
	if (zig && ((zig_aCx = SOB_find(zig, name, ZIG_SIGN)) == NULL)) return NS_ERROR_FAILURE;
	i = 0;
	while (zig && SOB_find_next(zig_aCx, &item) >= 0) {
		FINGERZIG *fingPrint;
		fingPrint = (FINGERZIG *) item->data;
		this->NewPrincipal(nsPrincipal::PrincipalType_CertificateKey, fingPrint->key,
		                           fingPrint->length, zig, &principal);
		RegisterPrincipal(principal, NULL);
		SetPrincipalArrayElement(*result, i++, principal);
	}
	if (zig) SOB_find_end(zig_aCx);
#endif
	if (codebase) {
		this->NewPrincipal(nsIPrincipal::PrincipalType_CodebaseExact, (void*)codebase,
		                           PL_strlen(codebase), NULL, &principal);
		RegisterPrincipal(principal, NULL);
		SetPrincipalArrayElement(*result, i++, principal);
	}
	*/
	return NS_OK;
}

NS_METHOD
nsCCapsManager::FreePrincipalArray(void *prinArray)
{
    nsCapsFreePrincipalArray(prinArray);
    return NS_OK;
}

NS_METHOD
nsCCapsManager::GetPrincipalArrayElement(void *prinArrayArg, PRUint32 index, nsIPrincipal* *ret_val)
{
	//method is deprecated, Principals must be accessed and indexed as nsIPrincipals, not by data member
	/*
 nsIPrincipal* pNSIPrincipal;
 nsPrincipal *pNSPrincipal = (nsPrincipal *)nsCapsGetPrincipalArrayElement(prinArrayArg, index);
 (pNSPrincipal->isCodebase()) ?
     pNSIPrincipal = (nsIPrincipal*)new nsCodebasePrincipal(pNSPrincipal)
	 :
     pNSIPrincipal = (nsIPrincipal*)new nsCertificatePrincipal(pNSPrincipal);
 }

 *ret_val = pNSIPrincipal;
 */
 *ret_val = NULL;
  return NS_OK;
}

NS_METHOD
nsCCapsManager::SetPrincipalArrayElement(void *prinArrayArg, PRUint32 index, nsIPrincipal* principal)
{
  nsCapsSetPrincipalArrayElement(prinArrayArg, index, principal);
  return NS_OK;
}

NS_METHOD
nsCCapsManager::GetPrincipalArraySize(void *prinArrayArg, PRUint32 *ret_val)
{
  *ret_val = nsCapsGetPrincipalArraySize(prinArrayArg);
  return NS_OK;
}

/* The following interfaces will replace all of the following old calls.
 * nsCapsGetPermission(struct nsPrivilege *privilege)
 * nsCapsGetPrivilege(struct nsPrivilegeTable *annotation, struct nsITarget *target)
 */
NS_METHOD
nsCCapsManager::IsAllowed(void *annotation, char* targetName, PRBool *ret_val)
{
	nsITarget *target = nsTarget::FindTarget(targetName);
	nsresult result = NS_OK;
	if( target == NULL ) {
		*ret_val = PR_FALSE;
		return NS_OK;
	}
	nsIPrivilege * pNSPrivilege = nsCapsGetPrivilege((nsPrivilegeTable * )annotation, target);
	if (pNSPrivilege == NULL) {
	   *ret_val = PR_FALSE;
	   return NS_OK;
	}
//	ARIEL WORK ON THIS
//	PRInt16 privilegeState = nsCapsGetPermission(pNSPrivilege);
//	*ret_val = (privilegeState == nsIPrivilege::PrivilegeState_Allowed);
	return NS_OK;
}


////////////////////////////////////////////////////////////////////////////
// from nsCCapsManager:

nsCCapsManager::nsCCapsManager(nsISupports *aOuter):privilegeManager(NULL) 
{
	NS_INIT_AGGREGATED(aOuter);
	PRBool result;
	privilegeManager = (Initialize(&result) == NS_OK) ? new nsPrivilegeManager(): NULL;
}

nsCCapsManager::~nsCCapsManager()
{
}

void
nsCCapsManager::CreateNSPrincipalArray(nsPrincipalArray* prinArray, 
                                       nsPrincipalArray* *pPrincipalArray)
{
	//prin arrays will either be removed, or updated to use the nsIPrincipal Object
/*
 nsIPrincipal* pNSIPrincipal;
 nsPrincipal *pNSPrincipal  = NULL;

 PRUint32 count = prinArray->GetSize();
 nsPrincipalArray *newPrinArray = new nsPrincipalArray();
 newPrinArray->SetSize(count, 1);
 *pPrincipalArray = NULL;

 for (PRUint32 index = 0; index < count; index++) {
    pNSPrincipal = (nsPrincipal *)prinArray->Get(index);
    if (pNSPrincipal->isCodebase()) {
        pNSIPrincipal = (nsIPrincipal*)new nsCodebasePrincipal(pNSPrincipal);
    } else {
        pNSIPrincipal = (nsIPrincipal*)new nsCertificatePrincipal(pNSPrincipal);
    }
    newPrinArray->Set(index, pNSIPrincipal);
 }
 *pPrincipalArray = newPrinArray;
 */
}
NS_METHOD
nsCCapsManager::GetNSPrincipalArray(nsPrincipalArray* prinArray, 
                                    nsPrincipalArray* *pPrincipalArray)
{
/*
 nsIPrincipal* pNSIPrincipal;
 nsIPrincipal *pNSPrincipal  = NULL;
 nsresult result = NS_OK;

 PRUint32 count = prinArray->GetSize();
 nsPrincipalArray *newPrinArray = new nsPrincipalArray();
 newPrinArray->SetSize(count, 1);
 *pPrincipalArray = NULL;
 for (PRUint32 index = 0; index < count; index++) {
    pNSIPrincipal = (nsIPrincipal*)prinArray->Get(index);
//    result = GetNSPrincipal(pNSIPrincipal, &pNSPrincipal);
    if( result != NS_OK)
    {
       nsCapsFreePrincipalArray(newPrinArray);
       return result;
    }
//    newPrinArray->Set(index, pNSPrincipal);
 }
 *pPrincipalArray = newPrinArray;
 return result;
 */
 return NS_OK;
}
/*
NS_METHOD
nsCCapsManager::GetNSPrincipal(nsIPrincipal* pNSIPrincipal, nsPrincipal **ppNSPrincipal)
{
   nsISupports *pNSISupports   = NULL;
   nsPrincipal *pNSPrincipal  = NULL;
   *ppNSPrincipal = NULL;

   if ( pNSIPrincipal == NULL )
   {
      return NS_ERROR_NULL_POINTER;
   }

   NS_DEFINE_IID(kICertificatePrincipalIID, NS_ICERTIFICATEPRINCIPAL_IID);
   NS_DEFINE_IID(kICodebasePrincipalIID, NS_ICODEBASEPRINCIPAL_IID);
//   NS_DEFINE_IID(kICodeSourcePrincipalIID, NS_ICODESOURCEPRINCIPAL_IID);

   if (pNSIPrincipal->QueryInterface(kICodeSourcePrincipalIID,
                            (void**)&pNSISupports) == NS_OK) 
   {
      nsCCodeSourcePrincipal *pNSCCodeSourcePrincipal = (nsCCodeSourcePrincipal *)pNSIPrincipal;
      nsICertificatePrincipal       *pNSICertPrincipal       = pNSCCodeSourcePrincipal->GetCertPrincipal();
      nsICodebasePrincipal   *pNSICodebasePrincipal   = pNSCCodeSourcePrincipal->GetCodebasePrincipal();
      PRBool bIsTrusted = PR_FALSE;
      if(pNSICertPrincipal != NULL )
      {
          pNSICertPrincipal->IsSecure(&bIsTrusted);
      }
      if (bIsTrusted)
      {
          nsCertificatePrincipal *pNSCCertPrincipal = (nsCertificatePrincipal *)pNSICertPrincipal;
//			peer object is deprecated, no longer use nsPrincipal object
//          pNSPrincipal = pNSCCertPrincipal->GetPeer();
          pNSCCertPrincipal->Release();
      }
      else
      if(pNSICodebasePrincipal != NULL )
      {
          nsCodebasePrincipal *pNSCCodebasePrincipal = (nsCodebasePrincipal *)pNSICodebasePrincipal;
//			peer object is deprecated, no longer use nsPrincipal object
//          pNSPrincipal = pNSCCodebasePrincipal->GetPeer();
          pNSCCodebasePrincipal->Release();
      }
      else
      {
         return NS_ERROR_NULL_POINTER;
      }
   }
   else
   if (pNSIPrincipal->QueryInterface(kICertificatePrincipalIID,
                            (void**)&pNSISupports) == NS_OK) 
   {
      nsCertificatePrincipal *pNSCCertPrincipal = (nsCertificatePrincipal *)pNSIPrincipal;
//			peer object is deprecated, no longer use nsPrincipal object
//      pNSPrincipal = pNSCCertPrincipal->GetPeer();
      pNSCCertPrincipal->Release();
   }
   else
   if (pNSIPrincipal->QueryInterface(kICodebasePrincipalIID,
                            (void**)&pNSISupports) == NS_OK) 
   {
      nsCodebasePrincipal *pNSCCodebasePrincipal = 
          (nsCodebasePrincipal *)pNSIPrincipal;
//			peer object is deprecated, no longer use nsPrincipal object
//      pNSPrincipal = pNSCCodebasePrincipal->GetPeer();
      pNSCCodebasePrincipal->Release();
   }
   else
   {
      return NS_ERROR_NO_INTERFACE;
   }
   *ppNSPrincipal = pNSPrincipal;
	return NS_OK;
}
*/
/*
nsPermission
nsCCapsManager::ConvertPrivilegeToPermission(nsPrivilege * pNSPrivilege)
{
  	if(pNSPrivilege->isAllowedForever())
     return nsPermission_AllowedForever;
  	if(pNSPrivilege->isForbiddenForever())
     return nsPermission_DeniedForever;
  	if(pNSPrivilege->isAllowed())
     return nsPermission_AllowedSession;
  	if(pNSPrivilege->isForbidden())
     return nsPermission_DeniedSession;

	  return nsPermission_Unknown;
}

nsPrivilege *
nsCCapsManager::ConvertPermissionToPrivilege(nsPermission state)
{
    nsPermissionState permission;
    nsDurationState duration;

    switch (state) {
    case nsPermission_AllowedForever: 
        permission = nsPermissionState_Allowed;
        duration = nsDurationState_Forever;
        break;
    case nsPermission_DeniedForever: 
        permission = nsPermissionState_Forbidden;
        duration = nsDurationState_Forever;
        break;
    case nsPermission_AllowedSession: 
        permission = nsPermissionState_Allowed;
        duration = nsDurationState_Session;
        break;
    case nsPermission_DeniedSession: 
        permission = nsPermissionState_Forbidden;
        duration = nsDurationState_Session;
        break;
    default:
        permission = nsPermissionState_Forbidden;
        duration = nsDurationState_Session;
        break;
    }
    return nsPrivilege::findPrivilege(permission, duration);
}
*/
void
nsCCapsManager::SetSystemPrivilegeManager()
{
	nsPrivilegeManager *pNSPrivilegeManager = nsPrivilegeManager::GetPrivilegeManager();
	if ((privilegeManager  != NULL ) && (privilegeManager != pNSPrivilegeManager)) {
		delete privilegeManager;
		privilegeManager = pNSPrivilegeManager;
	}
}
