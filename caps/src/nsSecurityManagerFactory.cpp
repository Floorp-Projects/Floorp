/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
/*Factory for internal browser security resource managers*/

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "nsIGenericFactory.h"
//#include "nsICapsManager.h"
//#include "nsCCapsManager.h"
//#include "nsPrincipalManager.h"
//#include "nsIPrivilegeManager.h"
//#include "nsPrivilegeManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsCodebasePrincipal.h"

//static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID, NS_GENERICFACTORY_CID);
//static NS_DEFINE_CID(kCCapsManagerCID, NS_CCAPSMANAGER_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCodebasePrincipal)

static NS_IMETHODIMP
Construct_nsIScriptSecurityManager(nsISupports *aOuter, REFNSIID aIID, 
								   void **aResult)
{
	if (!aResult)
		return NS_ERROR_NULL_POINTER;
	*aResult = nsnull;
	if (aOuter)
		return NS_ERROR_NO_AGGREGATION;
	nsISupports *obj = nsScriptSecurityManager::GetScriptSecurityManager();
	if (!obj) 
		return NS_ERROR_OUT_OF_MEMORY;
	if (NS_FAILED(obj->QueryInterface(aIID, aResult)))
		return NS_ERROR_FAILURE;
	return NS_OK;
}


static NS_IMETHODIMP
Construct_nsISecurityManager(nsISupports * aOuter, REFNSIID aIID, void * * aResult)
{
	nsresult rv;
	nsISupports *obj;
	if(!aResult)
	{
		rv = NS_ERROR_NULL_POINTER;
		goto done;
	}
	*aResult = NULL;
	if(aOuter)
	{
		rv = NS_ERROR_NO_AGGREGATION;
		goto done;
	}
	obj = nsnull; // nsCCapsManager::GetSecurityManager();
	if(!obj)
	{
		rv = NS_ERROR_OUT_OF_MEMORY;
		goto done;
	}
	rv = obj->QueryInterface(aIID, aResult);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
done:
	return rv;
}

static NS_IMETHODIMP
Construct_nsIPrivilegeManager(nsISupports * aOuter, REFNSIID aIID, void * * aResult)
{
	nsresult rv;
	nsISupports * obj;
	if(!aResult)
	{
		rv = NS_ERROR_NULL_POINTER;
		goto done;
	}
	*aResult = NULL;
	if(aOuter)
	{
		rv = NS_ERROR_NO_AGGREGATION;
		goto done;
	}
	obj = nsnull; /// nsPrivilegeManager::GetPrivilegeManager();
	if(!obj)
	{
		rv = NS_ERROR_OUT_OF_MEMORY;
		goto done;
	}
	rv = obj->QueryInterface(aIID, aResult);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
done:
	return rv;
}

/*
static NS_IMETHODIMP
Construct_nsIPrincipalManager(nsISupports * aOuter, REFNSIID aIID, void * * aResult)
{
	nsresult rv;
	nsPrincipalManager* obj = nsnull;
	if(!aResult) return NS_ERROR_NULL_POINTER;
	*aResult = NULL;
	if(aOuter) return NS_ERROR_NO_AGGREGATION;
	rv = nsnull; //nsPrincipalManager::GetPrincipalManager(&obj);
	if(!obj) return NS_ERROR_OUT_OF_MEMORY;
  if(NS_FAILED(rv)) return rv;
	rv = obj->QueryInterface(aIID, aResult);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
	return rv;
}
*/

extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports *aServMgr, const nsCID &aClass, 
             const char *aClassName, const char *aProgID, 
             nsIFactory **aFactory)
{
    NS_ASSERTION(aFactory != nsnull, "bad factory pointer");
    nsresult rv;
    NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, 
                     kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) 
        return NS_ERROR_FAILURE;
    nsIGenericFactory *factory;
    rv = compMgr->CreateInstance(kGenericFactoryCID, nsnull, 
                                 NS_GET_IID(nsIGenericFactory), 
                                 (void **) &factory);
    if (NS_FAILED(rv)) 
        return NS_ERROR_FAILURE;
    if (aClass.Equals(nsScriptSecurityManager::GetCID())) 
        rv = factory->SetConstructor(Construct_nsIScriptSecurityManager);
    else if (aClass.Equals(nsCodebasePrincipal::GetCID())) 
        rv = factory->SetConstructor(nsCodebasePrincipalConstructor);
    /*
    else if (aClass.Equals(kCCapsManagerCID)) 
        rv = factory->SetConstructor(Construct_nsISecurityManager);
    else if (aClass.Equals(nsPrivilegeManager::GetCID())) 
        rv = factory->SetConstructor(Construct_nsIPrivilegeManager);
    else if (aClass.Equals(nsPrincipalManager::GetCID())) 
        rv = factory->SetConstructor(Construct_nsIPrincipalManager);
    */
    else {
        NS_ASSERTION(0, "incorrectly registered");
        rv = NS_ERROR_NO_INTERFACE;
    }
    if (NS_FAILED(rv)) {
        NS_RELEASE(factory);
        return NS_ERROR_FAILURE;
    }
    *aFactory = factory;
    return NS_OK;
}

/***************************************************************************/

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports *aServMgr)
{
	return PR_FALSE;
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports *aServMgr, const char *aPath)
{
#ifdef DEBUG
    printf("***Registering Security***\n");
#endif
    nsresult rv;
    NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, 
                     kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) 
        return NS_ERROR_FAILURE;
    rv = compMgr->RegisterComponent(nsScriptSecurityManager::GetCID(),
                                    NS_SCRIPTSECURITYMANAGER_CLASSNAME,
                                    NS_SCRIPTSECURITYMANAGER_PROGID, aPath, 
                                    PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) 
        return NS_ERROR_FAILURE;
    rv = compMgr->RegisterComponent(nsCodebasePrincipal::GetCID(),
                                    NS_CODEBASEPRINCIPAL_CLASSNAME,
                                    NS_CODEBASEPRINCIPAL_PROGID, aPath, 
                                    PR_TRUE, PR_TRUE);  
    if (NS_FAILED(rv)) 
        return NS_ERROR_FAILURE;
    /*
    rv = compMgr->RegisterComponent(kCCapsManagerCID, NS_CCAPSMANAGER_CLASSNAME,
                                    NS_CCAPSMANAGER_PROGID, aPath, PR_TRUE, 
                                    PR_TRUE);
    if (NS_FAILED(rv)) 
        return NS_ERROR_FAILURE;
    rv = compMgr->RegisterComponent(nsPrivilegeManager::GetCID(), 
                                    NS_PRIVILEGEMANAGER_CLASSNAME, 
                                    NS_PRIVILEGEMANAGER_PROGID, aPath, 
                                    PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) 
        return NS_ERROR_FAILURE;
    rv = compMgr->RegisterComponent(nsPrincipalManager::GetCID(),
                                    NS_PRINCIPALMANAGER_CLASSNAME,
                                    NS_PRINCIPALMANAGER_PROGID, aPath, 
                                    PR_TRUE, PR_TRUE);
    */
    if (NS_FAILED(rv)) 
        return NS_ERROR_FAILURE;
    return NS_OK;
}
extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports *aServMgr, const char *aPath)
{
#ifdef DEBUG
	printf("*** Unregistering Security***\n");
#endif
	nsresult rv;
  NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, 
                   kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) 
      return NS_ERROR_FAILURE;
  rv = compMgr->UnregisterComponent(nsScriptSecurityManager::GetCID(), aPath);
  if (NS_FAILED(rv)) 
      return NS_ERROR_FAILURE;
  rv = compMgr->UnregisterComponent(nsCodebasePrincipal::GetCID(), aPath);
  if (NS_FAILED(rv)) 
      return NS_ERROR_FAILURE;
  /*
  rv = compMgr->UnregisterComponent(kCCapsManagerCID, aPath);
  if (NS_FAILED(rv)) 
      return NS_ERROR_FALURE;
  rv = compMgr->UnregisterComponent(nsPrivilegeManager::GetCID(), aPath);
  if (NS_FAILED(rv)) 
      return NS_ERROR_FALURE;
  rv = compMgr->UnregisterComponent(nsPrincipalManager::GetCID(), aPath);
  if (NS_FAILED(rv)) 
      return NS_ERROR_FALURE;
  */
  return NS_OK;
}
