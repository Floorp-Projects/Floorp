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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIComponentManager.h"
#include "nsIFactory.h"

#include "nsILocaleService.h"
#include "nsILocaleFactory.h"
#include "nsLocaleFactory.h"
#include "nsLocaleCID.h"
#include "nsIOS2Locale.h"
#include "nsLocaleOS2.h"
#include "nsCollationOS2.h"
#include "nsIScriptableDateFormat.h"
#include "nsDateTimeFormatOS2.h"
#include "nsLocaleFactoryOS2.h"
#include "nsDateTimeFormatCID.h"
#include "nsCollationCID.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

//
// kLocaleFactory for the nsILocaleFactory interface
//
NS_DEFINE_IID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
NS_DEFINE_IID(kILocaleFactoryIID,NS_ILOCALEFACTORY_IID);
NS_DEFINE_CID(kOS2LocaleFactoryCID, NS_OS2LOCALEFACTORY_CID);
NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID);

//
// for the collation and formatting interfaces
//
NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);
NS_DEFINE_IID(kICollationFactoryIID, NS_ICOLLATIONFACTORY_IID);                                                         
NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);                                                         
NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);
NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
NS_DEFINE_CID(kCollationCID, NS_COLLATION_CID);
NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
NS_DEFINE_CID(kScriptableDateFormatCID, NS_SCRIPTABLEDATEFORMAT_CID);

// Module implementation for the Locale library
class nsLocaleModule : public nsIModule
{
public:
  nsLocaleModule();
  virtual ~nsLocaleModule();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIMODULE

protected:
  nsresult Initialize();

  void Shutdown();

  PRBool mInitialized;
};

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsLocaleModule::nsLocaleModule()
  : mInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsLocaleModule::~nsLocaleModule()
{
  Shutdown();
}

NS_IMPL_ISUPPORTS(nsLocaleModule, kIModuleIID)

// Perform our one-time intialization for this module
nsresult
nsLocaleModule::Initialize()
{
  if (mInitialized) {
    return NS_OK;
  }
  mInitialized = PR_TRUE;
  return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsLocaleModule::Shutdown()
{
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsLocaleModule::GetClassObject(nsIComponentManager *aCompMgr,
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

  nsCOMPtr<nsIFactory> fact;

  // first check for the nsILocaleFactory interfaces
	if (aClass.Equals(kLocaleFactoryCID)) {
		nsLocaleFactory *factory = new nsLocaleFactory();
    if (!factory) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else {
      fact = do_QueryInterface(factory, &rv);
      if (!fact) {
        delete factory;
      }
    }
	}
  else if (aClass.Equals(kLocaleServiceCID)) {
		nsLocaleServiceFactory *factory = new nsLocaleServiceFactory();
    if (!factory) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else {
      fact = do_QueryInterface(factory, &rv);
      if (!fact) {
        delete factory;
      }
    }
  }
  else if (aClass.Equals(kOS2LocaleFactoryCID)) {
		nsLocaleFactoryOS2 *factory = new nsLocaleFactoryOS2();
    if (!factory) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else {
      fact = do_QueryInterface(factory, &rv);
      if (!fact) {
        delete factory;
      }
    }
  }
  else {
    // let the nsLocaleFactory logic take over from here
    nsLocaleFactoryOS2* factory = new nsLocaleFactoryOS2(aClass);
    if (!factory) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else {
      fact = do_QueryInterface(factory, &rv);
      if (!fact) {
        delete factory;
      }
    }
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
  const char* mContractID;
};

// The list of components we register
static Components gComponents[] = {
  { "nsLocale component", &kLocaleFactoryCID,
    NS_LOCALE_CONTRACTID, },
  { "nsLocaleService component", &kLocaleServiceCID,
    NS_LOCALESERVICE_CONTRACTID, },
  { "OS/2 locale", &kOS2LocaleFactoryCID,
    NULL, },
  { "Collation factory", &kCollationFactoryCID,
    NULL, },
  { "Collation", &kCollationCID,
    NULL, },
  { "Date/Time formatter", &kDateTimeFormatCID,
    NULL, },
  { "Scriptable Date Format", &kScriptableDateFormatCID,
    NS_SCRIPTABLEDATEFORMAT_CONTRACTID, },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsLocaleModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFile* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
  nsresult rv = NS_OK;

#ifdef DEBUG
  printf("*** Registering locale components\n");
#endif

  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                         cp->mContractID, aPath, PR_TRUE,
                                         PR_TRUE);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsLocaleModule: unable to register %s component => %x\n",
             cp->mDescription, rv);
#endif
      break;
    }
    cp++;
  }

  return rv;
}

NS_IMETHODIMP
nsLocaleModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFile* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
  printf("*** Unregistering locale components\n");
#endif
  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsLocaleModule: unable to unregister %s component => %x\n",
             cp->mDescription, rv);
#endif
    }
    cp++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocaleModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
  if (!okToUnload) {
    return NS_ERROR_INVALID_POINTER;
  }
  *okToUnload = PR_FALSE;
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsLocaleModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFile* location,
                                          nsIModule** return_cobj)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(return_cobj, "Null argument");
  NS_ASSERTION(gModule == NULL, "nsLocaleModule: Module already created.");

  // Create and initialize the layout module instance
  nsLocaleModule *m = new nsLocaleModule();
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
