/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h" // for pre-compiled headers...
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsMsgDBCID.h"

// include files for components this factory creates...
#include "nsMailDatabase.h"
#include "nsNewsDatabase.h"
#include "nsImapMailDatabase.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);
static NS_DEFINE_CID(kCImapDB, NS_IMAPDB_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMailDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNewsDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapMailDatabase)

// Module implementation for the msg db library
class nsMsgDBModule : public nsIModule
{
public:
    nsMsgDBModule();
    virtual ~nsMsgDBModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mMailDBFactory;
    nsCOMPtr<nsIGenericFactory> mNewsDBFactory;
    nsCOMPtr<nsIGenericFactory> mImapDBFactory;
};


nsMsgDBModule::nsMsgDBModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsMsgDBModule::~nsMsgDBModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsMsgDBModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult nsMsgDBModule::Initialize()
{
    if (mInitialized)
        return NS_OK;

    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void nsMsgDBModule::Shutdown()
{
	nsMsgDatabase::CleanupCache();
    // Release the factory objects
    mMailDBFactory = null_nsCOMPtr();
    mNewsDBFactory = null_nsCOMPtr();
    mImapDBFactory = null_nsCOMPtr();
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP nsMsgDBModule::GetClassObject(nsIComponentManager *aCompMgr,
                               const nsCID& aClass,
                               const nsIID& aIID,
                               void** r_classObj)
{
    nsresult rv;

    // Defensive programming: Initialize *r_classObj in case of error below
    if (!r_classObj)
        return NS_ERROR_INVALID_POINTER;

    *r_classObj = NULL;

    // Do one-time-only initialization if necessary
    if (!mInitialized) 
    {
        rv = Initialize();
        if (NS_FAILED(rv)) // Initialization failed! yikes!
            return rv;
    }

    // Choose the appropriate factory, based on the desired instance
    // class type (aClass).
    nsCOMPtr<nsIGenericFactory> fact;

    if (aClass.Equals(kCMailDB))
    {
        if (!mMailDBFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMailDBFactory), &nsMailDatabaseConstructor);
        fact = mMailDBFactory;
    }
    else if (aClass.Equals(kCNewsDB))
    {
        if (!mNewsDBFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mNewsDBFactory), &nsNewsDatabaseConstructor);
        fact = mNewsDBFactory;
    }
    else if (aClass.Equals(kCImapDB))
    {
        if (!mImapDBFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mImapDBFactory), &nsImapMailDatabaseConstructor);
        fact = mImapDBFactory;
    }
    if (fact)
        rv = fact->QueryInterface(aIID, r_classObj);

    return rv;
}


struct Components {
    const char* mDescription;
    const nsID* mCID;
    const char* mContractID;
};

// The list of components we register
static Components gComponents[] = {
    { "Mail DB", &kCMailDB,
      nsnull },
    { "News DB", &kCNewsDB,
      nsnull },    
    { "Imap DB", &kCImapDB,
      nsnull },

};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP nsMsgDBModule::RegisterSelf(nsIComponentManager *aCompMgr,
                                          nsIFile* aPath,
                                          const char* registryLocation,
                                          const char* componentType)
{
    nsresult rv = NS_OK;

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) 
    {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mContractID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) 
            break;
        cp++;
    }

    return rv;
}

NS_IMETHODIMP nsMsgDBModule::UnregisterSelf(nsIComponentManager* aCompMgr, nsIFile* aPath, 
                                            const char* registryLocation)
{
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) 
    {
        aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP nsMsgDBModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload)
        return NS_ERROR_INVALID_POINTER;

    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsMsgDBModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFile* aPath,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsMsgDBModule: Module already created.");

    // Create an initialize the imap module instance
    nsMsgDBModule *module = new nsMsgDBModule();
    if (!module)
        return NS_ERROR_OUT_OF_MEMORY;

    // Increase refcnt and store away nsIModule interface to m in return_cobj
    rv = module->QueryInterface(NS_GET_IID(nsIModule), (void**)return_cobj);
    if (NS_FAILED(rv)) 
    {
        delete module;
        module = nsnull;
    }
    gModule = module;                  // WARNING: Weak Reference
    return rv;
}
