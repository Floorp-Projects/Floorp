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

#if 1

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIScriptSecurityManager.h"
#include "nsScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsCodebasePrincipal.h"



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
	nsScriptSecurityManager *obj = nsScriptSecurityManager::GetScriptSecurityManager();
	if (!obj) 
		return NS_ERROR_OUT_OF_MEMORY;
	if (NS_FAILED(obj->QueryInterface(aIID, aResult)))
		return NS_ERROR_FAILURE;
	return NS_OK;
}



// Module implementation
class nsSecurityManagerModule : public nsIModule
{
public:
    nsSecurityManagerModule();
    virtual ~nsSecurityManagerModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mScriptSecurityManagerFactory;
    nsCOMPtr<nsIGenericFactory> mCodebasePrincipalFactory;
};



//----------------------------------------------------------------------



nsSecurityManagerModule::nsSecurityManagerModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsSecurityManagerModule::~nsSecurityManagerModule()
{
    Shutdown();
}



NS_IMPL_ISUPPORTS(nsSecurityManagerModule, NS_GET_IID(nsIModule))



// Perform our one-time intialization for this module
nsresult
nsSecurityManagerModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsSecurityManagerModule::Shutdown()
{
    // XXX FIX this:  nsScriptSecurityManager::GetScriptSecurityManager() leaks!

    // Release the factory object
    mScriptSecurityManagerFactory = nsnull;
    mCodebasePrincipalFactory = nsnull;
}



// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsSecurityManagerModule::GetClassObject(nsIComponentManager *aCompMgr,
                               const nsCID& aClass,
                               const nsIID& aIID,
                               void** r_classObj)
{
    nsresult rv;

    // Defensive programming: Initialize *r_classObj in case of error below
    if (!r_classObj) {
        return NS_ERROR_INVALID_POINTER;
    }
    *r_classObj = NULL;

    // Do one-time-only initialization if necessary
    if (!mInitialized) {
        rv = Initialize();
        if (NS_FAILED(rv)) {
            // Initialization failed! yikes!
            return rv;
        }
    }

    nsCOMPtr<nsIGenericFactory> fact;

    // Choose the appropriate factory, based on the desired instance
    // class type (aClass).
    if (aClass.Equals(nsScriptSecurityManager::GetCID()))
    {
    	if (!mScriptSecurityManagerFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mScriptSecurityManagerFactory),
                                      nsCodebasePrincipalConstructor);
        }
        fact = mScriptSecurityManagerFactory;
    }
    else if (aClass.Equals(nsCodebasePrincipal::GetCID()))
    {
    	if (!mCodebasePrincipalFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mCodebasePrincipalFactory),
                                      Construct_nsIScriptSecurityManager);
        }
        fact = mCodebasePrincipalFactory;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsSecurityManagerModule: unable to create factory for %s\n", cs);
        nsCRT::free(cs);
#endif
    }

    if (fact) {
        rv = fact->QueryInterface(aIID, r_classObj);
    }

    return rv;
}



//----------------------------------------

struct Components {
    const char* mDescription;
    const nsID* mCID;
    const char* mProgID;
};

// The list of components we register
static Components gComponents[] = {
    { "Script Security Manager", &nsScriptSecurityManager::GetCID(), NS_SCRIPTSECURITYMANAGER_PROGID },
    { "Codebase Principal", &nsCodebasePrincipal::GetCID(), NS_CODEBASEPRINCIPAL_PROGID }
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))



NS_IMETHODIMP
nsSecurityManagerModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFileSpec* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering nsSecurityManagerModule\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsSecurityManagerModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}



NS_IMETHODIMP
nsSecurityManagerModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFileSpec* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering nsSecurityManagerModule\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsSecurityManagerModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}



NS_IMETHODIMP
nsSecurityManagerModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}



//----------------------------------------------------------------------

static nsSecurityManagerModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(return_cobj);
    NS_ENSURE_NOT(gModule, NS_ERROR_FAILURE);

    // Create and initialize the module instance
    nsSecurityManagerModule *m = new nsSecurityManagerModule();
    if (!m) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Increase refcnt and store away nsIModule interface to m in return_cobj
    rv = m->QueryInterface(NS_GET_IID(nsIModule), (void**)return_cobj);
    if (NS_FAILED(rv)) {
        delete m;
        m = nsnull;
    }
    gModule = m;                  // WARNING: Weak Reference
    return rv;
}




#else




#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "nsIGenericFactory.h"
#include "nsIScriptSecurityManager.h"
#include "nsScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsCodebasePrincipal.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID, NS_GENERICFACTORY_CID);

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
	nsScriptSecurityManager *obj = nsScriptSecurityManager::GetScriptSecurityManager();
	if (!obj) 
		return NS_ERROR_OUT_OF_MEMORY;
	if (NS_FAILED(obj->QueryInterface(aIID, aResult)))
		return NS_ERROR_FAILURE;
	return NS_OK;
}


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
    printf("*** Registering Security\n");
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
    if (NS_FAILED(rv)) 
        return NS_ERROR_FAILURE;
    return NS_OK;
}



extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports *aServMgr, const char *aPath)
{
#ifdef DEBUG
	printf("*** Unregistering Security ***\n");
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
  return NS_OK;
}

#endif
