/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsIChromeRegistry.h"
#include "nsIChromeEntry.h"
#include "nscore.h"
#include "rdf.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsChromeProtocolHandler.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID, NS_GENERICFACTORY_CID);
static NS_DEFINE_CID(kChromeRegistryCID, NS_CHROMEREGISTRY_CID);
static NS_DEFINE_CID(kChromeEntryCID, NS_CHROMEENTRY_CID);
static NS_DEFINE_CID(kChromeProtocolHandlerCID, NS_CHROMEPROTOCOLHANDLER_CID);

static NS_IMETHODIMP
NS_ConstructChromeRegistry(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    nsIChromeRegistry* chromeRegistry;
    rv = NS_NewChromeRegistry(&chromeRegistry);
    if (NS_FAILED(rv)) {
        NS_ERROR("Unable to construct chrome registry");
        return rv;
    }
    rv = chromeRegistry->QueryInterface(aIID, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
    NS_RELEASE(chromeRegistry);
    return rv;
}

static NS_IMETHODIMP
NS_ConstructChromeEntry(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    nsIChromeEntry* chromeEntry;
    rv = NS_NewChromeEntry(&chromeEntry);
    if (NS_FAILED(rv)) {
        NS_ERROR("Unable to construct chrome entry");
        return rv;
    }
    rv = chromeEntry->QueryInterface(aIID, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
    NS_RELEASE(chromeEntry);
    return rv;
}


// Module implementation
class nsChromeModule : public nsIModule
{
public:
    nsChromeModule();
    virtual ~nsChromeModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mChromeRegistryFactory;
    nsCOMPtr<nsIGenericFactory> mChromeProtocolHandlerFactory;
    nsCOMPtr<nsIGenericFactory> mChromeEntryFactory;
};

//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.
//----------------------------------------------------------------------

nsChromeModule::nsChromeModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsChromeModule::~nsChromeModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsChromeModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult
nsChromeModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsChromeModule::Shutdown()
{
    // Release the factory objects
    mChromeRegistryFactory = nsnull;
    mChromeProtocolHandlerFactory = nsnull;
    mChromeEntryFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsChromeModule::GetClassObject(nsIComponentManager *aCompMgr,
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

    // Choose the appropriate factory, based on the desired instance
    // class type (aClass).
    nsCOMPtr<nsIGenericFactory> fact;
    if (aClass.Equals(kChromeRegistryCID)) {
        if (!mChromeRegistryFactory) {
            // Create and save away the factory object for creating
            // new instances of ChromeRegistry. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mChromeRegistryFactory),
                                      NS_ConstructChromeRegistry);
        }
        fact = mChromeRegistryFactory;
    }
    else if (aClass.Equals(kChromeEntryCID)) {
        if (!mChromeEntryFactory) {
            // Create and save away the factory object for creating
            // new instances of ChromeRegistry. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mChromeEntryFactory),
                                      NS_ConstructChromeEntry);
        }
        fact = mChromeRegistryFactory;
    }
    else if (aClass.Equals(kChromeProtocolHandlerCID)) {
        if (!mChromeProtocolHandlerFactory) {
            // Create and save away the factory object for creating
            // new instances of ChromeProtocolHandler. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mChromeProtocolHandlerFactory),
                                      nsChromeProtocolHandler::Create);
        }
        fact = mChromeProtocolHandlerFactory;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsChromeModule: unable to create factory for %s\n", cs);
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
    { "Chrome Registry", &kChromeRegistryCID,
      "component://netscape/chrome/chrome-registry", },
    { "Chrome Entry", &kChromeEntryCID,
      "component://netscape/chrome/chrome-entry", },
    { "Chrome Protocol Handler", &kChromeProtocolHandlerCID,
      NS_NETWORK_PROTOCOL_PROGID_PREFIX "chrome", },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsChromeModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFileSpec* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering chrome components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsChromeModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}

NS_IMETHODIMP
nsChromeModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFileSpec* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering chrome components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsChromeModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsChromeModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsChromeModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(return_cobj);
    NS_ENSURE_FALSE(gModule, NS_ERROR_FAILURE);

    // Create and initialize the module instance
    nsChromeModule *m = new nsChromeModule();
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
