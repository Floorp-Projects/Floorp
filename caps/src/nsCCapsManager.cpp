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
      nsPrincipal *pNSPrinicipal  = NULL;
      result = GetNSPrincipal(pNSIPrincipal, &pNSPrinicipal);
      if( result != NS_OK)
      {
        return result;
      }

      nsPrivilege* privilege = 
          m_pNSPrivilegeManager->getPrincipalPrivilege(target, pNSPrinicipal, 
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
      nsPrincipal *pNSPrinicipal  = NULL;
      result = GetNSPrincipal(pNSIPrincipal, &pNSPrinicipal);
      if( result != NS_OK)
      {
        return result;
      }
      nsPrivilege* privilege = ConvertPermissionToPrivilege(state);
      m_pNSPrivilegeManager->SetPermission(pNSPrinicipal, target, privilege);
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
      nsPrincipal *pNSPrinicipal  = NULL;
      result = GetNSPrincipal(pNSIPrincipal, &pNSPrinicipal);
      if( result != NS_OK)
      {
        return result;
      }
      m_pNSPrivilegeManager->AskPermission(pNSPrinicipal,target, NULL);
      nsPrivilege* privilege = 
          m_pNSPrivilegeManager->getPrincipalPrivilege(target, pNSPrinicipal, 
                                                       NULL);
      *state = ConvertPrivilegeToPermission(privilege);
   }
   return NS_OK;
}

////////////////////////////////////////////////////////////////////////////
// from nsCCapsManager:

nsCCapsManager::nsCCapsManager(nsISupports *aOuter)
{
    NS_INIT_AGGREGATED(aOuter);
    m_pNSPrivilegeManager = new nsPrivilegeManager();
}

nsCCapsManager::~nsCCapsManager()
{
}

NS_METHOD
nsCCapsManager::GetNSPrincipal(nsIPrincipal* pNSIPrincipal, 
                               nsPrincipal **ppNSPrincipal)
{
   nsISupports *pNSISupports   = NULL;
   nsPrincipal *pNSPrinicipal  = NULL;
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
          pNSPrinicipal = pNSCCertPrincipal->GetPeer();
          pNSCCertPrincipal->Release();
      }
      else
      if(pNSICodebasePrincipal != NULL )
      {
          nsCCodebasePrincipal *pNSCCodebasePrincipal = (nsCCodebasePrincipal *)pNSICodebasePrincipal;
          pNSPrinicipal = pNSCCodebasePrincipal->GetPeer();
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
      pNSPrinicipal = pNSCCertPrincipal->GetPeer();
      pNSCCertPrincipal->Release();
   }
   else
   if (pNSIPrincipal->QueryInterface(kICodebasePrincipalIID,
                            (void**)&pNSISupports) == NS_OK) 
   {
      nsCCodebasePrincipal *pNSCCodebasePrincipal = 
          (nsCCodebasePrincipal *)pNSIPrincipal;
      pNSPrinicipal = pNSCCodebasePrincipal->GetPeer();
      pNSCCodebasePrincipal->Release();
   }
   else
   {
      return NS_ERROR_NO_INTERFACE;
   }
   *ppNSPrincipal = pNSPrinicipal;
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
