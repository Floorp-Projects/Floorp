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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsLWBrkCIID.h"
#include "nsILineBreakerFactory.h"
#include "nsIWordBreakerFactory.h"
#include "nsLWBreakerFImp.h"
#include "nsLWBRKDll.h"

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;

static NS_DEFINE_IID(kLWBrkCID, NS_LWBRK_CID);

// Module implementation for the sample library
class nsLWBrkModule : public nsIModule
{
public:
  nsLWBrkModule();
  virtual ~nsLWBrkModule();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIMODULE

protected:
  nsresult Initialize();

  void Shutdown();

  PRBool mInitialized;
  nsCOMPtr<nsIGenericFactory> mFactory;
};

//----------------------------------------------------------------------

// This function creates a new instance of nsLWBreakerFImp which is
// itself a factory. The callers know that and use it to create
// specific types of line/word breakers.
static NS_IMETHODIMP
CreateNewLWBrkThing(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_ERROR_NO_AGGREGATION;
  }
  nsLWBreakerFImp* inst = new nsLWBreakerFImp();
  if (!inst) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    delete inst;
    *aResult = nsnull;
  }
  return rv;
}

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsLWBrkModule::nsLWBrkModule()
  : mInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsLWBrkModule::~nsLWBrkModule()
{
  Shutdown();
}

NS_IMPL_ISUPPORTS(nsLWBrkModule, kIModuleIID)

// Perform our one-time intialization for this module
nsresult
nsLWBrkModule::Initialize()
{
  if (mInitialized) {
    return NS_OK;
  }
  mInitialized = PR_TRUE;
  return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsLWBrkModule::Shutdown()
{
  // Release the factory object
  mFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsLWBrkModule::GetClassObject(nsIComponentManager *aCompMgr,
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
  if (aClass.Equals(kLWBrkCID)) {
    if (!mFactory) {
      // Create a new factory-factory.
      rv = NS_NewGenericFactory(getter_AddRefs(mFactory),
                                CreateNewLWBrkThing);
    }
    fact = mFactory;
  }
  else {
		rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
    char* cs = aClass.ToString();
    printf("+++ nsLWBrkModule: unable to create factory for %s\n", cs);
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
  { "Line and Word Breaker", &kLWBrkCID,
    NS_LWBRK_PROGID, },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsLWBrkModule::RegisterSelf(nsIComponentManager *aCompMgr,
                            nsIFileSpec* aPath,
                            const char* registryLocation,
                            const char* componentType)
{
  nsresult rv = NS_OK;

#ifdef DEBUG
  printf("*** Registering lwbrk components\n");
#endif

  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                         cp->mProgID, aPath, PR_TRUE,
                                         PR_TRUE);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsLWBrkModule: unable to register %s component => %x\n",
             cp->mDescription, rv);
#endif
      break;
    }
    cp++;
  }

  return rv;
}

NS_IMETHODIMP
nsLWBrkModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                              nsIFileSpec* aPath,
                              const char* registryLocation)
{
#ifdef DEBUG
  printf("*** Unregistering sample components\n");
#endif
  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsLWBrkModule: unable to unregister %s component => %x\n",
             cp->mDescription, rv);
#endif
    }
    cp++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLWBrkModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
  if (!okToUnload) {
    return NS_ERROR_INVALID_POINTER;
  }
  *okToUnload = (g_InstanceCount == 0) && (g_LockCount == 0);
  return NS_OK;
}

//----------------------------------------------------------------------

static nsLWBrkModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(return_cobj, "Null argument");
  NS_ASSERTION(gModule == NULL, "nsLWBrkModule: Module already created.");

  // Create an initialize the layout module instance
  nsLWBrkModule *m = new nsLWBrkModule();
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
