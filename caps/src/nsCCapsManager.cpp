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
#include "nsPrincipalArray.h"
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

NS_IMETHODIMP
nsCCapsManager::GetPrincipalManager(nsIPrincipalManager * * prinMan)
{
	* prinMan = principalManager;
	return NS_OK;
}

NS_IMETHODIMP
nsCCapsManager::GetPrivilegeManager(nsIPrivilegeManager * * privMan)
{
	* privMan = privilegeManager;
	return NS_OK;
}

NS_IMETHODIMP
nsCCapsManager::CreateCodebasePrincipal(const char * codebaseURL, nsIPrincipal * * prin)
{
	return nsPrincipalManager::GetPrincipalManager()->CreateCodebasePrincipal(codebaseURL, prin);
}

NS_IMETHODIMP
nsCCapsManager::CreateCertificatePrincipal(const unsigned char **certChain, PRUint32 * certChainLengths, 
											PRUint32 noOfCerts, nsIPrincipal** prin)
{
	return nsPrincipalManager::GetPrincipalManager()->CreateCertificatePrincipal(certChain,certChainLengths,noOfCerts,prin);
}

NS_METHOD
nsCCapsManager::GetPermission(nsIPrincipal * prin, nsITarget * ignoreTarget, PRInt16 * privilegeState)
{
	* privilegeState = nsIPrivilege::PrivilegeState_Blank;
	nsITarget * target = nsTarget::FindTarget(ALL_JAVA_PERMISSION);
	nsresult result = NS_OK;
	if( target == NULL ) return NS_OK;
	if (privilegeManager != NULL) {
		nsIPrivilege * privilege;
		privilegeManager->GetPrincipalPrivilege(target, prin, NULL, & privilege);
// ARIEL WORK ON THIS SHIT
//		* privilegeState = this->ConvertPrivilegeToPermission(privilege);
	}
	return NS_OK;
}

NS_METHOD
nsCCapsManager::SetPermission(nsIPrincipal * prin, nsITarget * ignoreTarget, PRInt16 privilegeState)
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

NS_METHOD
nsCCapsManager::AskPermission(nsIPrincipal * prin, nsITarget * ignoreTarget, PRInt16 * privilegeState)
{
	nsITarget * target = nsTarget::FindTarget(ALL_JAVA_PERMISSION);
	if( target == NULL ) {
		* privilegeState = nsIPrivilege::PrivilegeState_Blank;
		return NS_OK;
	}
	if (privilegeManager != NULL) {
		PRBool perm;
		privilegeManager->AskPermission(prin, target, NULL, & perm);
		nsIPrivilege * privilege;
		privilegeManager->GetPrincipalPrivilege(target, prin, NULL,& privilege);
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
	* result = nsCapsInitialize();
	return NS_OK;
}

NS_METHOD
nsCCapsManager::InitializeFrameWalker(nsICapsSecurityCallbacks* aInterface)
{
	//XXX write me  
	return NS_OK;
}

NS_METHOD
nsCCapsManager::RegisterPrincipal(nsIPrincipal * prin)
{
//	if (principalManager != NULL) privilegeManager->RegisterPrincipal(prin);
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
nsCCapsManager::EnablePrivilege(nsIScriptContext * context, const char* targetName, PRInt32 callerDepth, PRBool * ret_val)
{
	nsITarget *target = nsTarget::FindTarget((char*)targetName);
	nsresult result = NS_OK;
	if( target == NULL )
	{
		* ret_val = PR_FALSE;
		return NS_OK;
	}
//	if (privilegeManager != NULL)
//		ret_val = privilegeManager->EnablePrivilege(context, target, NULL, callerDepth, ret_val);
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
nsCCapsManager::IsPrivilegeEnabled(nsIScriptContext * context, const char* targetName, PRInt32 callerDepth, PRBool *ret_val)
{
	nsITarget *target = nsTarget::FindTarget((char*)targetName);
	nsresult result = NS_OK;
	if( target == NULL )
	{
		* ret_val = PR_FALSE;
		return NS_OK;
	}
	if (privilegeManager != NULL)
		privilegeManager->IsPrivilegeEnabled(context, target, callerDepth, ret_val);
	return NS_OK;
}

NS_METHOD
nsCCapsManager::RevertPrivilege(nsIScriptContext * context, const char* targetName, PRInt32 callerDepth, PRBool *ret_val)
{
	nsITarget *target = nsTarget::FindTarget((char*)targetName);
	nsresult result = NS_OK;
	if( target == NULL ) {
		* ret_val = PR_FALSE;
		return NS_OK;
	}
	if (privilegeManager != NULL)
		privilegeManager->RevertPrivilege(context, target, callerDepth,ret_val);
	return NS_OK;
}

NS_METHOD
nsCCapsManager::DisablePrivilege(nsIScriptContext * context, const char* targetName, PRInt32 callerDepth, PRBool *ret_val)
{
	nsITarget *target = nsTarget::FindTarget((char*)targetName);
	nsresult result = NS_OK;
	if( target == NULL ) {
		* ret_val = PR_FALSE;
		return NS_OK;
	}
	if (privilegeManager != NULL)
		privilegeManager->DisablePrivilege(context, target, callerDepth,ret_val);
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

/*
 * CreateMixedPrincipalArray take codebase and  ZIG file information and returns a
 * pointer to an array of nsIPrincipal objects.

 */
 /*
NS_METHOD
nsCCapsManager::CreateMixedPrincipalArray(void *aZig, const char * name, const char* codebase, nsIPrincipalArray * * result)
{
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
	return NS_OK;
}
*/

/* The following interfaces will replace all of the following old calls.
 * nsCapsGetPermission(struct nsPrivilege *privilege)
 * nsCapsGetPrivilege(struct nsPrivilegeTable *annotation, struct nsITarget *target)
 */
NS_METHOD
nsCCapsManager::IsAllowed(void *annotation, const char * targetName, PRBool * ret_val)
{
	nsITarget *target = nsTarget::FindTarget((char *)targetName);
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

nsCCapsManager::nsCCapsManager(nsISupports * aOuter):privilegeManager(NULL) 
{
	NS_INIT_AGGREGATED(aOuter);
//	PRBool result;
//	privilegeManager = (Initialize(& result) == NS_OK) ? new nsPrivilegeManager(): NULL;
}

nsCCapsManager::~nsCCapsManager()
{
}

void
nsCCapsManager::CreateNSPrincipalArray(nsIPrincipalArray* prinArray, 
                                       nsIPrincipalArray* *pPrincipalArray)
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
/*
NS_METHOD
nsCCapsManager::GetNSPrincipalArray(nsPrincipalArray* prinArray, 
                                    nsPrincipalArray* *pPrincipalArray)
{


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
 return NS_OK;
}
*/
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
	nsIPrivilegeManager * pNSPrivilegeManager = (nsIPrivilegeManager *)nsPrivilegeManager::GetPrivilegeManager();
	if ((privilegeManager  != NULL ) && (privilegeManager != pNSPrivilegeManager)) {
		delete privilegeManager;
		privilegeManager = pNSPrivilegeManager;
	}
}

void
nsCCapsManager::SetSystemPrincipalManager()
{
	nsIPrincipalManager * prinMan = (nsIPrincipalManager *)nsPrincipalManager::GetPrincipalManager();
	if ((principalManager != NULL ) && (principalManager != prinMan)) {
		delete principalManager;
		principalManager = prinMan;
	}
}