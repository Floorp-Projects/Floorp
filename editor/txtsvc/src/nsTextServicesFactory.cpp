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
 * Communications Corporation.	Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#include "nsCOMPtr.h"
#include "nsTextServicesDocument.h"
#include "nsTextServicesCID.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

static NS_DEFINE_CID(kCTextServicesDocumentCID, NS_TEXTSERVICESDOCUMENT_CID);

nsresult
NS_NewTextServicesDocument(nsTextServicesDocument** result);

// Module implementation
class nsTextServicesModule : public nsIModule
{
public:
    nsTextServicesModule();
    virtual ~nsTextServicesModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mFactory;
};

//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.

static NS_IMETHODIMP  
CreateNewTextServices(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{                                                                    
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }                                                                
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }                                                                
    nsTextServicesDocument* inst;                                                
    nsresult rv = NS_NewTextServicesDocument(&inst);                              
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
        return rv;                                                   
    }                                                                
    rv = inst->QueryInterface(aIID, aResult);                        
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;                                                       
}

//----------------------------------------------------------------------

nsTextServicesModule::nsTextServicesModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsTextServicesModule::~nsTextServicesModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsTextServicesModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult
nsTextServicesModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsTextServicesModule::Shutdown()
{
    // Release the factory object
    mFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsTextServicesModule::GetClassObject(nsIComponentManager *aCompMgr,
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
    if (aClass.Equals(kCTextServicesDocumentCID)) {
        if (!mFactory) {
            // Create and save away the factory object for creating
            // new instances of TextServices. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mFactory),
                                      CreateNewTextServices);
        }
        fact = mFactory;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsTextServicesModule: unable to create factory for %s\n", cs);
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
    { NULL, &kCTextServicesDocumentCID,
      NULL, },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsTextServicesModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFileSpec* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering TextServices components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsTextServicesModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}

NS_IMETHODIMP
nsTextServicesModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFileSpec* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering TextServices components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsTextServicesModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsTextServicesModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsTextServicesModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(return_cobj);
    NS_ENSURE_FALSE(gModule, NS_ERROR_FAILURE);

    // Create and initialize the module instance
    nsTextServicesModule *m = new nsTextServicesModule();
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
