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
  
/*
	Eudora import module
*/

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIRegistry.h"
#include "nsIImportService.h"
#include "nsEudoraImport.h"

#include "EudoraDebugLog.h"

static NS_DEFINE_CID(kEudoraImportCID,    	NS_EUDORAIMPORT_CID);
static NS_DEFINE_CID(kImportServiceCID,		NS_IMPORTSERVICE_CID);
static NS_DEFINE_CID(kRegistryCID,			NS_REGISTRY_CID);


// Module implementation for the Eudora import library
class nsEudoraImportModule : public nsIModule
{
public:
    nsEudoraImportModule();
    virtual ~nsEudoraImportModule();

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

#define MAKE_CTOR(_iface, _name)                                     \
static NS_IMETHODIMP                                                 \
CreateNew##_name(nsISupports* aOuter, REFNSIID aIID, void **aResult) \
{                                                                    \
    if (!aResult) {                                                  \
        return NS_ERROR_INVALID_POINTER;                             \
    }                                                                \
    if (aOuter) {                                                    \
        *aResult = nsnull;                                           \
        return NS_ERROR_NO_AGGREGATION;                              \
    }                                                                \
    nsI##_iface* inst;                                                \
    nsresult rv = NS_New##_name(&inst);                              \
    if (NS_FAILED(rv)) {                                             \
        *aResult = nsnull;                                           \
        return rv;                                                   \
    }                                                                \
    rv = inst->QueryInterface(aIID, aResult);                        \
    if (NS_FAILED(rv)) {                                             \
        *aResult = nsnull;                                           \
    }                                                                \
    NS_RELEASE(inst);             /* get rid of extra refcnt */      \
    return rv;                                                       \
}

MAKE_CTOR(ImportModule, EudoraImport)

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsEudoraImportModule::nsEudoraImportModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsEudoraImportModule::~nsEudoraImportModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsEudoraImportModule, kIModuleIID)

// Perform our one-time intialization for this module
nsresult
nsEudoraImportModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsEudoraImportModule::Shutdown()
{
    // Release the factory object
    mFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsEudoraImportModule::GetClassObject(nsIComponentManager *aCompMgr,
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
    if (aClass.Equals(kEudoraImportCID)) {
        if (!mFactory) {
            // Create and save away the factory object for creating
            // new instances of Sample. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mFactory),
                                      CreateNewEudoraImport);
        }
        fact = mFactory;
    }
    else {
		rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsEudoraImportModule: unable to create factory for %s\n", cs);
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
    { "Eudora Import Component", &kEudoraImportCID,
      "component://mozilla/import/import-eudora", },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

nsresult GetImportModulesRegKey( nsIRegistry *reg, nsRegistryKey *pKey)
{
	nsRegistryKey	nScapeKey;

	nsresult rv = reg->GetSubtree( nsIRegistry::Common, "Netscape", &nScapeKey);
	if (NS_FAILED(rv)) {
		rv = reg->AddSubtree( nsIRegistry::Common, "Netscape", &nScapeKey);
	}
	if (NS_FAILED( rv))
		return( rv);

	nsRegistryKey	iKey;
	rv = reg->GetSubtree( nScapeKey, "Import", &iKey);
	if (NS_FAILED( rv)) {
		rv = reg->AddSubtree( nScapeKey, "Import", &iKey);
	}
	
	if (NS_FAILED( rv))
		return( rv);

	rv = reg->GetSubtree( iKey, "Modules", pKey);
	if (NS_FAILED( rv)) {
		rv = reg->AddSubtree( iKey, "Modules", pKey);
	}

	return( rv);
}

NS_IMETHODIMP
nsEudoraImportModule::RegisterSelf(nsIComponentManager *aCompMgr,
                          nsIFileSpec* aPath,
                          const char* registryLocation,
                          const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering Eudora import components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsEudoraImportModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    {
    	NS_WITH_SERVICE( nsIRegistry, reg, kRegistryCID, &rv);
    	if (NS_FAILED(rv)) {
	    	IMPORT_LOG0( "*** Import Eudora, ERROR GETTING THE Registry\n");
    		return rv;
    	}
    	
    	rv = reg->OpenDefault();
    	if (NS_FAILED(rv)) {
	    	IMPORT_LOG0( "*** Import Eudora, ERROR OPENING THE REGISTRY\n");
    		return( rv);
    	}
    	
		nsRegistryKey	importKey;
		
		rv = GetImportModulesRegKey( reg, &importKey);
		if (NS_FAILED( rv)) {
			IMPORT_LOG0( "*** Import Eudora, ERROR getting Netscape/Import registry key\n");
			return( rv);
		}		
		    	
		nsRegistryKey	key;
    	rv = reg->AddSubtree( importKey, "Eudora", &key);
    	if (NS_FAILED(rv)) return( rv);
    	
    	rv = reg->SetString( key, "Supports", kEudoraSupportsString);
    	if (NS_FAILED(rv)) return( rv);
    	char *myCID = kEudoraImportCID.ToString();
    	rv = reg->SetString( key, "CLSID", myCID);
    	delete [] myCID;
    	if (NS_FAILED(rv)) return( rv);  	
    }
    
    return rv;
}


NS_IMETHODIMP
nsEudoraImportModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                            nsIFileSpec* aPath,
                            const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering Eudora import components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsEudoraImportModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsEudoraImportModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsEudoraImportModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsEudoraImportModule: Module already created.");

    // Create an initialize the layout module instance
    nsEudoraImportModule *m = new nsEudoraImportModule();
    if (!m) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Increase refcnt and store away nsIModule interface to m in return_cobj
    rv = m->QueryInterface(nsIModule::GetIID(), (void**)return_cobj);
    if (NS_FAILED(rv)) {
        delete m;
        m = nsnull;
    }
    gModule = m;                  // WARNING: Weak Reference
    return rv;
}



