/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "pratom.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsIServiceManager.h"
#include "nsII18nCompatibility.h"
#include "nsI18nCompatibility.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kI18nCompatibilityCID, NS_I18NCOMPATIBILITY_CID);
static NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);

 
///////////////////////////////////////////////////////////////////////////////////////////

class nsI18nCompatibility : public nsII18nCompatibility {
 public: 
  NS_DECL_ISUPPORTS 

  /* wstring CSIDtoCharsetName (in unsigned short csid); */
  NS_IMETHOD  CSIDtoCharsetName(PRUint16 csid, PRUnichar **_retval);

  nsI18nCompatibility() {NS_INIT_REFCNT();}
  virtual ~nsI18nCompatibility() {}
};

NS_IMPL_ISUPPORTS(nsI18nCompatibility, nsII18nCompatibility::GetIID());

NS_IMETHODIMP nsI18nCompatibility::CSIDtoCharsetName(PRUint16 csid, PRUnichar **_retval)
{
  nsString charsetname(I18N_CSIDtoCharsetName(csid));

  *_retval = charsetname.ToNewUnicode();

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;

NS_IMETHODIMP
NS_NewI18nCompatibility(nsISupports* aOuter, const nsIID& aIID,
                        void** aResult)
{
  nsresult rv;

  if (!aResult) {                                                  
    return NS_ERROR_INVALID_POINTER;                             
  }                                                                
  if (aOuter) {                                                    
    *aResult = nsnull;                                           
    return NS_ERROR_NO_AGGREGATION;                              
  }                                                                
  nsI18nCompatibility * inst = new nsI18nCompatibility();
  if (inst == NULL) {                                             
    *aResult = nsnull;                                           
    return NS_ERROR_OUT_OF_MEMORY;
  }                                                                
  rv = inst->QueryInterface(aIID, aResult);                        
  if (NS_FAILED(rv)) {
    delete inst;
    *aResult = nsnull;                                           
  }                                                              
  return rv;                                                     
}

//----------------------------------------------------------------------------

class nsI18nCompModule : public nsIModule 
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMODULE

private:

  PRBool mInitialized;

  void Shutdown();

public:

  nsI18nCompModule();

  virtual ~nsI18nCompModule();

  nsresult Initialize();

protected:
  nsCOMPtr<nsIGenericFactory> mFactory;
};

static nsI18nCompModule * gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager * compMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(return_cobj);
  NS_ENSURE_FALSE(gModule, NS_ERROR_FAILURE);

  // Create an initialize the module instance
  nsI18nCompModule * m = new nsI18nCompModule();
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

NS_IMPL_ISUPPORTS(nsI18nCompModule, nsIModule::GetIID())

nsI18nCompModule::nsI18nCompModule()
: mInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsI18nCompModule::~nsI18nCompModule()
{
  Shutdown();
}

nsresult nsI18nCompModule::Initialize()
{
  return NS_OK;
}

void nsI18nCompModule::Shutdown()
{
  mFactory = nsnull;
}

NS_IMETHODIMP nsI18nCompModule::GetClassObject(nsIComponentManager *aCompMgr,
                                               const nsCID& aClass,
                                               const nsIID& aIID,
                                               void ** r_classObj)
{
  nsresult rv;

  // Defensive programming: Initialize *r_classObj in case of error below
  if (!r_classObj) {
    return NS_ERROR_INVALID_POINTER;
  }
  *r_classObj = NULL;

  if (!mInitialized) {
    rv = Initialize();
    if (NS_FAILED(rv)) {
      return rv;
    }
    mInitialized = PR_TRUE;
  }

  nsCOMPtr<nsIGenericFactory> fact;
  if (aClass.Equals(kI18nCompatibilityCID)) {
    if (!mFactory) {
      rv = NS_NewGenericFactory(getter_AddRefs(mFactory), 
          NS_NewI18nCompatibility);
    }
    fact = mFactory;
  } else {
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }

  if (fact) {
    rv = fact->QueryInterface(aIID, r_classObj);
  }

  return rv;
}

NS_IMETHODIMP nsI18nCompModule::RegisterSelf(nsIComponentManager *aCompMgr,
                                              nsIFileSpec* aPath,
                                              const char* registryLocation,
                                              const char* componentType)
{
  nsresult rv;
  rv = aCompMgr->RegisterComponentSpec(kI18nCompatibilityCID, 
                                  "I18n compatibility", 
                                  NS_I18NCOMPATIBILITY_PROGID, 
                                  aPath,
                                  PR_TRUE, PR_TRUE);
  return rv;
}

NS_IMETHODIMP nsI18nCompModule::UnregisterSelf(nsIComponentManager *aCompMgr,
                                                nsIFileSpec* aPath,
                                                const char* registryLocation)
{
  nsresult rv;

  rv = aCompMgr->UnregisterComponentSpec(kI18nCompatibilityCID, aPath);

  return rv;
}

NS_IMETHODIMP nsI18nCompModule::CanUnload(nsIComponentManager *aCompMgr, 
                                           PRBool *okToUnload)
{
  if (!okToUnload) {
    return NS_ERROR_INVALID_POINTER;
  }
  *okToUnload = PR_FALSE;
  return NS_ERROR_FAILURE;
}

