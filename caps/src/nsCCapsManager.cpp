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
#include "nsRepository.h"
#include "nsCapsEnums.h"
#include "nsCCapsManager.h"
#include "nsICodebasePrincipal.h"
#include "nsICertPrincipal.h"
#include "nsCCodebasePrincipal.h"
#include "nsCCertPrincipal.h"
#include "nsCCodeSourcePrincipal.h"
#include "nsCaps.h"

static NS_DEFINE_CID(kCCapsManagerCID, NS_CCAPSMANAGER_CID);
static NS_DEFINE_IID(kICapsManagerIID, NS_ICAPSMANAGER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

#define ALL_JAVA_PERMISSION "AllJavaPermission"

////////////////////////////////////////////////////////////////////////////
// from nsISupports and AggregatedQueryInterface:

// Thes macro expands to the aggregated query interface scheme.

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
   nsCCodebasePrincipal *pNSCCodebasePrincipal = 
       new nsCCodebasePrincipal(codebaseURL, &result);
   if (pNSCCodebasePrincipal == NULL)
   {
      return NS_ERROR_OUT_OF_MEMORY;
   }
   pNSCCodebasePrincipal->AddRef();
   *prin = (nsIPrincipal *)pNSCCodebasePrincipal;
   return result;
}

NS_METHOD
nsCCapsManager::CreateCertPrincipal(const unsigned char **certChain, 
                                    PRUint32 *certChainLengths, 
                                    PRUint32 noOfCerts, 
                                    nsIPrincipal** prin)
{
   nsresult result = NS_OK;
   nsCCertPrincipal *pNSCCertPrincipal = 
       new nsCCertPrincipal(certChain, certChainLengths, noOfCerts, &result);
   if (pNSCCertPrincipal == NULL)
   {
      return NS_ERROR_OUT_OF_MEMORY;
   }
   pNSCCertPrincipal->AddRef();
   *prin = (nsIPrincipal *)pNSCCertPrincipal;
   return NS_OK;
}

NS_METHOD
nsCCapsManager::CreateCodeSourcePrincipal(const unsigned char **certChain, 
                                          PRUint32 *certChainLengths, 
                                          PRUint32 noOfCerts, 
                                          const char *codebaseURL, 
                                          nsIPrincipal** prin)
{
   nsresult result = NS_OK;
   nsCCodeSourcePrincipal *pNSCCodeSourcePrincipal = 
       new nsCCodeSourcePrincipal(certChain, certChainLengths, noOfCerts, 
                                  codebaseURL, &result);
   if (pNSCCodeSourcePrincipal == NULL)
   {
      return NS_ERROR_OUT_OF_MEMORY;
   }
   pNSCCodeSourcePrincipal->AddRef();
   *prin = (nsIPrincipal *)pNSCCodeSourcePrincipal;
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
nsCCapsManager::GetPermission(nsIPrincipal* pNSIPrincipal, 
                              nsITarget* ignoreTarget, 
                              nsPermission *state)
{
   *state = nsPermission_Unknown;
   nsTarget *target = nsTarget::findTarget(ALL_JAVA_PERMISSION);
   nsresult result = NS_OK;
   if( target == NULL )
   {
      return NS_OK;
   }
   if (m_pNSPrivilegeManager != NULL)
   {
      nsPrincipal *pNSPrincipal  = NULL;
      result = GetNSPrincipal(pNSIPrincipal, &pNSPrincipal);
      if( result != NS_OK)
      {
        return result;
      }

      nsPrivilege* privilege = 
          m_pNSPrivilegeManager->getPrincipalPrivilege(target, pNSPrincipal, 
                                                       NULL);
      *state = ConvertPrivilegeToPermission(privilege);
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
nsCCapsManager::SetPermission(nsIPrincipal* pNSIPrincipal, 
                              nsITarget* ignoreTarget,
                              nsPermission state)
{  
   nsTarget *target = nsTarget::findTarget(ALL_JAVA_PERMISSION);
   nsresult result = NS_OK;
   if( target == NULL )
   {
      return NS_OK;
   }
   if (m_pNSPrivilegeManager != NULL)
   {
      nsPrincipal *pNSPrincipal  = NULL;
      result = GetNSPrincipal(pNSIPrincipal, &pNSPrincipal);
      if( result != NS_OK)
      {
        return result;
      }
      nsPrivilege* privilege = ConvertPermissionToPrivilege(state);
      m_pNSPrivilegeManager->SetPermission(pNSPrincipal, target, privilege);
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
nsCCapsManager::AskPermission(nsIPrincipal* pNSIPrincipal, 
                              nsITarget* ignoreTarget, 
                              nsPermission *state)
{
   nsTarget *target = nsTarget::findTarget(ALL_JAVA_PERMISSION);
   nsresult result = NS_OK;
   if( target == NULL )
   {
      *state = nsPermission_Unknown;
      return NS_OK;
   }
   if (m_pNSPrivilegeManager != NULL)
   {
      nsPrincipal *pNSPrincipal  = NULL;
      result = GetNSPrincipal(pNSIPrincipal, &pNSPrincipal);
      if( result != NS_OK)
      {
        return result;
      }
      m_pNSPrivilegeManager->AskPermission(pNSPrincipal,target, NULL);
      nsPrivilege* privilege = 
          m_pNSPrivilegeManager->getPrincipalPrivilege(target, pNSPrincipal, 
                                                       NULL);
      *state = ConvertPrivilegeToPermission(privilege);
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
nsCCapsManager::Initialize(PRBool *result)
{
    *result = nsCapsInitialize();
    return NS_OK;
}

/**
 * Registers the given Principal with the system.
 *
 * @param prin   - is either certificate principal or codebase principal
 * @param result - is true if principal was successfully registered with the system
 */
NS_METHOD
nsCCapsManager::RegisterPrincipal(nsIPrincipal* pNSIPrincipal, PRBool *ret_val)
{
   nsresult result = NS_OK;
   if (m_pNSPrivilegeManager != NULL)
   {
      nsPrincipal *pNSPrincipal  = NULL;
      result = GetNSPrincipal(pNSIPrincipal, &pNSPrincipal);
      if( result != NS_OK)
      {
        return result;
      }
      m_pNSPrivilegeManager->registerPrincipal(pNSPrincipal);
   }
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
   nsTarget *target = nsTarget::findTarget((char*)targetName);
   nsresult result = NS_OK;
   if( target == NULL )
   {
      *ret_val = PR_FALSE;
      return NS_OK;
   }
   if (m_pNSPrivilegeManager != NULL)
   {
      *ret_val = m_pNSPrivilegeManager->enablePrivilege(context, target, callerDepth);
   }
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
   nsTarget *target = nsTarget::findTarget((char*)targetName);
   nsresult result = NS_OK;
   if( target == NULL )
   {
      *ret_val = PR_FALSE;
      return NS_OK;
   }
   if (m_pNSPrivilegeManager != NULL)
   {
      *ret_val = m_pNSPrivilegeManager->isPrivilegeEnabled(context, target, callerDepth);
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
   nsTarget *target = nsTarget::findTarget((char*)targetName);
   nsresult result = NS_OK;
   if( target == NULL )
   {
      *ret_val = PR_FALSE;
      return NS_OK;
   }
   if (m_pNSPrivilegeManager != NULL)
   {
      *ret_val = m_pNSPrivilegeManager->revertPrivilege(context, target, callerDepth);
   }
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
   nsTarget *target = nsTarget::findTarget((char*)targetName);
   nsresult result = NS_OK;
   if( target == NULL )
   {
      *ret_val = PR_FALSE;
      return NS_OK;
   }
   if (m_pNSPrivilegeManager != NULL)
   {
      *ret_val = m_pNSPrivilegeManager->disablePrivilege(context, target, callerDepth);
   }
   return NS_OK;
}


/* XXX: Some of the arguments for the following interfaces may change.
 * This is a first cut. I need to talk to joki. We should get rid of void* parameters.
 */
NS_METHOD
nsCCapsManager::ComparePrincipalArray(void* prin1Array, void* prin2Array, nsSetComparisonType *ret_val)
{
   nsresult result = NS_OK;
   *ret_val = nsSetComparisonType_NoSubset;
   if (m_pNSPrivilegeManager != NULL)
   {
     nsPrincipalArray *newPrin1Array=NULL;
     nsPrincipalArray *newPrin2Array=NULL;
     result = GetNSPrincipalArray((nsPrincipalArray*) prin1Array, &newPrin1Array);
     if (result != NS_OK) {
         return result;
     }
     result = GetNSPrincipalArray((nsPrincipalArray*) prin2Array, &newPrin2Array);
     if (result != NS_OK) {
         return result;
     }
     *ret_val = m_pNSPrivilegeManager->comparePrincipalArray(newPrin1Array, newPrin2Array);
     nsCapsFreePrincipalArray(newPrin1Array);
     nsCapsFreePrincipalArray(newPrin2Array);
   }
   return NS_OK;
}

NS_METHOD
nsCCapsManager::IntersectPrincipalArray(void* prin1Array, void* prin2Array, void* *ret_val)
{
   nsresult result = NS_OK;
   *ret_val = NULL;
   if (m_pNSPrivilegeManager != NULL)
   {
     nsPrincipalArray *newPrin1Array=NULL;
     nsPrincipalArray *newPrin2Array=NULL;
     nsPrincipalArray *intersectPrinArray=NULL;
     result = GetNSPrincipalArray((nsPrincipalArray*) prin1Array, &newPrin1Array);
     if (result != NS_OK) {
         return result;
     }
     result = GetNSPrincipalArray((nsPrincipalArray*) prin2Array, &newPrin2Array);
     if (result != NS_OK) {
         return result;
     }
     intersectPrinArray = m_pNSPrivilegeManager->intersectPrincipalArray(newPrin1Array, 
                                                                         newPrin2Array);
     CreateNSPrincipalArray(intersectPrinArray, (nsPrincipalArray**)ret_val);
     nsCapsFreePrincipalArray(newPrin1Array);
     nsCapsFreePrincipalArray(newPrin2Array);
   }
   return NS_OK;
}

NS_METHOD
nsCCapsManager::CanExtendTrust(void* fromPrinArray, void* toPrinArray, PRBool *ret_val)
{
   nsresult result = NS_OK;
   *ret_val = nsSetComparisonType_NoSubset;
   if (m_pNSPrivilegeManager != NULL)
   {
     nsPrincipalArray *newPrin1Array=NULL;
     nsPrincipalArray *newPrin2Array=NULL;
     result = GetNSPrincipalArray((nsPrincipalArray*) fromPrinArray, &newPrin1Array);
     if (result != NS_OK) {
         return result;
     }
     result = GetNSPrincipalArray((nsPrincipalArray*) toPrinArray, &newPrin2Array);
     if (result != NS_OK) {
         return result;
     }
     *ret_val = m_pNSPrivilegeManager->canExtendTrust(newPrin1Array, newPrin2Array);
     nsCapsFreePrincipalArray(newPrin1Array);
     nsCapsFreePrincipalArray(newPrin2Array);
   }
   return NS_OK;
}


/* interfaces for nsIPrincipal object, may be we should move some of them to nsIprincipal */

NS_METHOD
nsCCapsManager::NewPrincipal(nsPrincipalType type, void* key, PRUint32 key_len, void *zig, nsIPrincipal** ret_val)
{
  nsIPrincipal* pNSIPrincipal;
  nsPrincipal *pNSPrincipal = new nsPrincipal(type, key, key_len, zig);
  if (pNSPrincipal->isCodebase()) {
      pNSIPrincipal = (nsIPrincipal*)new nsCCodebasePrincipal(pNSPrincipal);
  } else {
      pNSIPrincipal = (nsIPrincipal*)new nsCCertPrincipal(pNSPrincipal);
  }
  *ret_val = pNSIPrincipal;
  return NS_OK;
}

NS_METHOD
nsCCapsManager::IsCodebaseExact(nsIPrincipal* pNSIPrincipal, PRBool *ret_val)
{
   nsresult result = NS_OK;
   nsPrincipal *pNSPrincipal  = NULL;
   *ret_val = PR_FALSE;
   result = GetNSPrincipal(pNSIPrincipal, &pNSPrincipal);
   if( result != NS_OK) {
       return result;
   }
   *ret_val = pNSPrincipal->isCodebaseExact();
    
   return NS_OK;
}

NS_METHOD
nsCCapsManager::ToString(nsIPrincipal* pNSIPrincipal, char* *ret_val)
{
   nsresult result = NS_OK;
   nsPrincipal *pNSPrincipal  = NULL;
   *ret_val = NULL;
   result = GetNSPrincipal(pNSIPrincipal, &pNSPrincipal);
   if( result != NS_OK) {
       return result;
   }
   *ret_val = pNSPrincipal->toString();
    
   return NS_OK;
}

NS_METHOD
nsCCapsManager::GetVendor(nsIPrincipal* pNSIPrincipal, char* *ret_val)
{
   nsresult result = NS_OK;
   nsPrincipal *pNSPrincipal  = NULL;
   *ret_val = NULL;
   result = GetNSPrincipal(pNSIPrincipal, &pNSPrincipal);
   if( result != NS_OK) {
       return result;
   }
   *ret_val = pNSPrincipal->getVendor();
    
   return NS_OK;
}

NS_METHOD
nsCCapsManager::NewPrincipalArray(PRUint32 count, void* *ret_val)
{
  *ret_val = nsCapsNewPrincipalArray(count);
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
 nsIPrincipal* pNSIPrincipal;
 nsPrincipal *pNSPrincipal = (nsPrincipal *)nsCapsGetPrincipalArrayElement(prinArrayArg, index);
 if (pNSPrincipal->isCodebase()) {
     pNSIPrincipal = (nsIPrincipal*)new nsCCodebasePrincipal(pNSPrincipal);
 } else {
     pNSIPrincipal = (nsIPrincipal*)new nsCCertPrincipal(pNSPrincipal);
 }
 *ret_val = pNSIPrincipal;
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
 *
 * nsCapsGetPermission(struct nsPrivilege *privilege)
 * nsCapsGetPrivilege(struct nsPrivilegeTable *annotation, struct nsTarget *target)
 *
 */
NS_METHOD
nsCCapsManager::IsAllowed(void *annotation, char* targetName, PRBool *ret_val)
{
   nsTarget *target = nsTarget::findTarget(targetName);
   nsresult result = NS_OK;
   if( target == NULL )
   {
      *ret_val = PR_FALSE;
      return NS_OK;
   }
   struct nsPrivilege * pNSPrivilege = nsCapsGetPrivilege((struct nsPrivilegeTable *)annotation,
                                                          target);
   if (pNSPrivilege == NULL) {
       *ret_val = PR_FALSE;
       return NS_OK;
    }
    nsPermissionState perm = nsCapsGetPermission(pNSPrivilege);

    *ret_val = (perm == nsPermissionState_Allowed);
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////
// from nsCCapsManager:

nsCCapsManager::nsCCapsManager(nsISupports *aOuter):m_pNSPrivilegeManager(NULL) 
{
    NS_INIT_AGGREGATED(aOuter);
    PRBool result;
    if (Initialize(&result) == NS_OK)
    {
      m_pNSPrivilegeManager = new nsPrivilegeManager();
    }
    else
    { 
      m_pNSPrivilegeManager = NULL;
    }
}

nsCCapsManager::~nsCCapsManager()
{
}

void
nsCCapsManager::CreateNSPrincipalArray(nsPrincipalArray* prinArray, 
                                       nsPrincipalArray* *pPrincipalArray)
{
 nsIPrincipal* pNSIPrincipal;
 nsPrincipal *pNSPrincipal  = NULL;

 PRUint32 count = prinArray->GetSize();
 nsPrincipalArray *newPrinArray = new nsPrincipalArray();
 newPrinArray->SetSize(count, 1);
 *pPrincipalArray = NULL;

 for (PRUint32 index = 0; index < count; index++) {
    pNSPrincipal = (nsPrincipal *)prinArray->Get(index);
    if (pNSPrincipal->isCodebase()) {
        pNSIPrincipal = (nsIPrincipal*)new nsCCodebasePrincipal(pNSPrincipal);
    } else {
        pNSIPrincipal = (nsIPrincipal*)new nsCCertPrincipal(pNSPrincipal);
    }
    newPrinArray->Set(index, pNSIPrincipal);
 }
 *pPrincipalArray = newPrinArray;
}


NS_METHOD
nsCCapsManager::GetNSPrincipalArray(nsPrincipalArray* prinArray, 
                                    nsPrincipalArray* *pPrincipalArray)
{
 nsIPrincipal* pNSIPrincipal;
 nsPrincipal *pNSPrincipal  = NULL;
 nsresult result = NS_OK;

 PRUint32 count = prinArray->GetSize();
 nsPrincipalArray *newPrinArray = new nsPrincipalArray();
 newPrinArray->SetSize(count, 1);
 *pPrincipalArray = NULL;

 for (PRUint32 index = 0; index < count; index++) {
    pNSIPrincipal = (nsIPrincipal*)prinArray->Get(index);
    result = GetNSPrincipal(pNSIPrincipal, &pNSPrincipal);
    if( result != NS_OK)
    {
       nsCapsFreePrincipalArray(newPrinArray);
       return result;
    }
    newPrinArray->Set(index, pNSPrincipal);
 }
 *pPrincipalArray = newPrinArray;
 return result;
}

NS_METHOD
nsCCapsManager::GetNSPrincipal(nsIPrincipal* pNSIPrincipal, 
                               nsPrincipal **ppNSPrincipal)
{
   nsISupports *pNSISupports   = NULL;
   nsPrincipal *pNSPrincipal  = NULL;
   *ppNSPrincipal = NULL;

   if ( pNSIPrincipal == NULL )
   {
      return NS_ERROR_NULL_POINTER;
   }

   NS_DEFINE_IID(kICertPrincipalIID, NS_ICERTPRINCIPAL_IID);
   NS_DEFINE_IID(kICodebasePrincipalIID, NS_ICODEBASEPRINCIPAL_IID);
   NS_DEFINE_IID(kICodeSourcePrincipalIID, NS_ICODESOURCEPRINCIPAL_IID);

   if (pNSIPrincipal->QueryInterface(kICodeSourcePrincipalIID,
                            (void**)&pNSISupports) == NS_OK) 
   {
      nsCCodeSourcePrincipal *pNSCCodeSourcePrincipal = (nsCCodeSourcePrincipal *)pNSIPrincipal;
      nsICertPrincipal       *pNSICertPrincipal       = pNSCCodeSourcePrincipal->GetCertPrincipal();
      nsICodebasePrincipal   *pNSICodebasePrincipal   = pNSCCodeSourcePrincipal->GetCodebasePrincipal();
      PRBool bIsTrusted = PR_FALSE;
      if(pNSICertPrincipal != NULL )
      {
          pNSICertPrincipal->IsTrusted(NULL, &bIsTrusted);
      }
      if (bIsTrusted)
      {
          nsCCertPrincipal *pNSCCertPrincipal = (nsCCertPrincipal *)pNSICertPrincipal;
          pNSPrincipal = pNSCCertPrincipal->GetPeer();
          pNSCCertPrincipal->Release();
      }
      else
      if(pNSICodebasePrincipal != NULL )
      {
          nsCCodebasePrincipal *pNSCCodebasePrincipal = (nsCCodebasePrincipal *)pNSICodebasePrincipal;
          pNSPrincipal = pNSCCodebasePrincipal->GetPeer();
          pNSCCodebasePrincipal->Release();
      }
      else
      {
         return NS_ERROR_NULL_POINTER;
      }
   }
   else
   if (pNSIPrincipal->QueryInterface(kICertPrincipalIID,
                            (void**)&pNSISupports) == NS_OK) 
   {
      nsCCertPrincipal *pNSCCertPrincipal = (nsCCertPrincipal *)pNSIPrincipal;
      pNSPrincipal = pNSCCertPrincipal->GetPeer();
      pNSCCertPrincipal->Release();
   }
   else
   if (pNSIPrincipal->QueryInterface(kICodebasePrincipalIID,
                            (void**)&pNSISupports) == NS_OK) 
   {
      nsCCodebasePrincipal *pNSCCodebasePrincipal = 
          (nsCCodebasePrincipal *)pNSIPrincipal;
      pNSPrincipal = pNSCCodebasePrincipal->GetPeer();
      pNSCCodebasePrincipal->Release();
   }
   else
   {
      return NS_ERROR_NO_INTERFACE;
   }
   *ppNSPrincipal = pNSPrincipal;
   return NS_OK;
}


nsPermission
nsCCapsManager::ConvertPrivilegeToPermission(nsPrivilege *pNSPrivilege)
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

void
nsCCapsManager::SetSystemPrivilegeManager()
{
    nsPrivilegeManager *pNSPrivilegeManager = 
        nsPrivilegeManager::getPrivilegeManager();
     if (   (m_pNSPrivilegeManager  != NULL )
         && (m_pNSPrivilegeManager != pNSPrivilegeManager)
        )
     {
        delete m_pNSPrivilegeManager;
        m_pNSPrivilegeManager = pNSPrivilegeManager;
     }
}
