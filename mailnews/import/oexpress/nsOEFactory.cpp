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
  
/*
	Outlook Express (Win32) import module
*/

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIRegistry.h"
#include "nsIImportService.h"
#include "nsOEImport.h"

#include "OEDebugLog.h"

static NS_DEFINE_CID(kOEImportCID,       	NS_OEIMPORT_CID);
static NS_DEFINE_CID(kImportServiceCID,		NS_IMPORTSERVICE_CID);
static NS_DEFINE_CID(kRegistryCID,			NS_REGISTRY_CID);


// Module implementation for the Outlook Express import library
class nsOEImportModule : public nsIModule
{
public:
    nsOEImportModule();
    virtual ~nsOEImportModule();

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

MAKE_CTOR(ImportModule, OEImport)

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsOEImportModule::nsOEImportModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsOEImportModule::~nsOEImportModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsOEImportModule, kIModuleIID)

// Perform our one-time intialization for this module
nsresult
nsOEImportModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsOEImportModule::Shutdown()
{
    // Release the factory object
    mFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsOEImportModule::GetClassObject(nsIComponentManager *aCompMgr,
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
    if (aClass.Equals(kOEImportCID)) {
        if (!mFactory) {
            // Create and save away the factory object for creating
            // new instances of Sample. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mFactory),
                                      CreateNewOEImport);
        }
        fact = mFactory;
    }
    else {
		rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsOEImportModule: unable to create factory for %s\n", cs);
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
    { "Outlook Express Import Component", &kOEImportCID,
      "component://mozilla/import/import-oe", },
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
nsOEImportModule::RegisterSelf(nsIComponentManager *aCompMgr,
                          nsIFileSpec* aPath,
                          const char* registryLocation,
                          const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering Outlook Express import components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsOEImportModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    {
    	NS_WITH_SERVICE( nsIRegistry, reg, kRegistryCID, &rv);
    	if (NS_FAILED(rv)) {
	    	IMPORT_LOG0( "*** Import OExpress, ERROR GETTING THE Registry\n");
    		return rv;
    	}
    	
    	rv = reg->OpenDefault();
    	if (NS_FAILED(rv)) {
	    	IMPORT_LOG0( "*** Import OExpress, ERROR OPENING THE REGISTRY\n");
    		return( rv);
    	}
    	
		nsRegistryKey	importKey;
		
		rv = GetImportModulesRegKey( reg, &importKey);
		if (NS_FAILED( rv)) {
			IMPORT_LOG0( "*** Import OExpress, ERROR getting Netscape/Import registry key\n");
			return( rv);
		}		
		    	
		nsRegistryKey	key;
    	rv = reg->AddSubtree( importKey, "Outlook Express", &key);
    	if (NS_FAILED(rv)) return( rv);
    	
    	rv = reg->SetString( key, "Supports", kOESupportsString);
    	if (NS_FAILED(rv)) return( rv);
    	char *myCID = kOEImportCID.ToString();
    	rv = reg->SetString( key, "CLSID", myCID);
    	delete [] myCID;
    	if (NS_FAILED(rv)) return( rv);  	
    }
    
    return rv;
}


NS_IMETHODIMP
nsOEImportModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                            nsIFileSpec* aPath,
                            const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering Outlook Express import components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsOEImportModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsOEImportModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsOEImportModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsOEImportModule: Module already created.");

    // Create an initialize the layout module instance
    nsOEImportModule *m = new nsOEImportModule();
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



