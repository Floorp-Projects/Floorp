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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeDecodeHelper.h"
#include "nsIUnicodeEncodeHelper.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetAlias.h"
#include "nsITextToSubURI.h"
#include "nsIServiceManager.h"
#include "nsCharsetMenu.h"
#include "rdf.h"
#include "nsUConvDll.h"
#include "nsFileSpec.h"
#include "nsIFile.h"
//----------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kCharsetMenuCID, NS_CHARSETMENU_CID);
static NS_DEFINE_CID(kTextToSubURICID, NS_TEXTTOSUBURI_CID);
static NS_DEFINE_CID(kPlatformCharsetCID, NS_PLATFORMCHARSET_CID);

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;

//----------------------------------------------------------------------

class nsUConvModule : public nsIModule {
public:
  nsUConvModule();
  virtual ~nsUConvModule();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIMODULE

  nsresult Initialize();

protected:
  void Shutdown();

  PRBool mInitialized;
};

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsUConvModule::nsUConvModule()
  : mInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsUConvModule::~nsUConvModule()
{
  Shutdown();
}

NS_IMPL_ISUPPORTS(nsUConvModule, kIModuleIID)

nsresult
nsUConvModule::Initialize()
{
  return NS_OK;
}

void
nsUConvModule::Shutdown()
{
}

NS_IMETHODIMP
nsUConvModule::GetClassObject(nsIComponentManager *aCompMgr,
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

  if (!mInitialized) {
    rv = Initialize();
    if (NS_FAILED(rv)) {
      return rv;
    }
    mInitialized = PR_TRUE;
  }

  nsCOMPtr<nsIGenericFactory> fact;

  if (aClass.Equals(kCharsetConverterManagerCID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), NS_NewCharsetConverterManager);
  }
  else if (aClass.Equals(kUnicodeDecodeHelperCID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), NS_NewUnicodeDecodeHelper);
  }
  else if (aClass.Equals(kUnicodeEncodeHelperCID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), NS_NewUnicodeEncodeHelper);
  }
  else if (aClass.Equals(kPlatformCharsetCID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), NS_NewPlatformCharset);
  }
  else if (aClass.Equals(kCharsetAliasCID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), NS_NewCharsetAlias);
  }
  else if (aClass.Equals(kCharsetMenuCID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), NS_NewCharsetMenu);
  }
  else if (aClass.Equals(kTextToSubURICID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), NS_NewTextToSubURI);
  }
  else {
		return NS_ERROR_FACTORY_NOT_REGISTERED;
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
  { "Charset Conversion Manager", &kCharsetConverterManagerCID,
    NS_CHARSETCONVERTERMANAGER_CONTRACTID, },
  { "Unicode Decode Helper", &kUnicodeDecodeHelperCID,
    NS_UNICODEDECODEHELPER_CONTRACTID, },
  { "Unicode Encode Helper", &kUnicodeEncodeHelperCID,
    NS_UNICODEENCODEHELPER_CONTRACTID, },
  { "Platform Charset Information", &kPlatformCharsetCID,
    NS_PLATFORMCHARSET_CONTRACTID, },
  { "Charset Alias Information",  &kCharsetAliasCID,
    NS_CHARSETALIAS_CONTRACTID, },
  { NS_CHARSETMENU_PID, &kCharsetMenuCID,
    NS_RDF_DATASOURCE_CONTRACTID_PREFIX NS_CHARSETMENU_PID, },
  { "Text To Sub URI Helper", &kTextToSubURICID,
    NS_ITEXTTOSUBURI_CONTRACTID, },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsUConvModule::RegisterSelf(nsIComponentManager *aCompMgr,
                            nsIFile* aPath,
                            const char* registryLocation,
                            const char* componentType)
{
  nsresult rv = NS_OK;

#ifdef DEBUG
  printf("*** Registering uconv components\n");
#endif

  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                         cp->mContractID, aPath, PR_TRUE,
                                         PR_TRUE);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsUConvModule: unable to register %s component => %x\n",
             cp->mDescription, rv);
#endif
      break;
    }
    cp++;
  }

  // XXX also unregister this stuff when time comes
  rv = NS_RegisterConverterManagerData();

  return rv;
}

NS_IMETHODIMP
nsUConvModule::UnregisterSelf(nsIComponentManager *aCompMgr,
                             nsIFile* aPath,
                             const char* registryLocation)
{
#ifdef DEBUG
  printf("*** Unregistering uconv components\n");
#endif
  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsUConvModule: unable to unregister %s component => %x\n",
             cp->mDescription, rv);
#endif
    }
    cp++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUConvModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
  if (!okToUnload) {
    return NS_ERROR_INVALID_POINTER;
  }
  *okToUnload = PR_FALSE;
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsUConvModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                             nsIFile* aPath,
                             nsIModule** return_cobj)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(return_cobj, "Null argument");
  NS_ASSERTION(gModule == NULL, "nsUConvModule: Module already created.");

  // Create an initialize the layout module instance
  nsUConvModule *m = new nsUConvModule();
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
