/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsCOMPtr.h"
#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsParserCIID.h"
#include "nsParser.h"
#include "nsILoggingSink.h"
#include "nsWellFormedDTD.h"
#include "CNavDTD.h"
#include "nsXIFDTD.h"

static NS_DEFINE_IID(kParserCID, NS_PARSER_IID);
static NS_DEFINE_IID(kParserNodeCID, NS_PARSER_NODE_IID);
static NS_DEFINE_IID(kLoggingSinkCID, NS_LOGGING_SINK_IID);
static NS_DEFINE_CID(kWellFormedDTDCID, NS_WELLFORMEDDTD_CID);
static NS_DEFINE_CID(kNavDTDCID, NS_CNAVDTD_CID);
static NS_DEFINE_CID(kXIFDTDCID, NS_XIF_DTD_CID);

struct Components {
  const char* mDescription;
  const nsID* mCID;
};

static Components gComponents[] = {
  { "Parser", &kParserCID },
  { "ParserNode", &kParserNodeCID },
  { "Logging sink", &kLoggingSinkCID },
  { "Well formed DTD", &kWellFormedDTDCID },
  { "Navigator HTML DTD", &kNavDTDCID },
  { "XIF DTD", &kXIFDTDCID },
};

#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]));

// Factory method to create new instances of nsParser
static nsresult
CreateNewParser(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_NOINTERFACE;
  }
  nsParser* inst = new nsParser();
  if (!inst) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    *aResult = nsnull;
    delete inst;
  }
  return rv;
}

// Factory method to create new instances of nsParserNode
static nsresult
CreateNewParserNode(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_NOINTERFACE;
  }
  nsCParserNode* inst = new nsCParserNode();
  if (!inst) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    *aResult = nsnull;
    delete inst;
  }
  return rv;
}

// Factory method to create new instances of nsILoggingSink
static nsresult
CreateNewLoggingSink(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_NOINTERFACE;
  }
  nsIContentSink* inst;
  nsresult rv = NS_NewHTMLLoggingSink(&inst);
  if (NS_FAILED(rv)) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    *aResult = nsnull;
  }
  NS_RELEASE(inst);             // get rid of extra refcnt
  return rv;
}

// Factory method to create new instances of nsWellFormedDTD
static nsresult
CreateNewWellFormedDTD(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_NOINTERFACE;
  }
  nsIDTD* inst;
  nsresult rv = NS_NewWellFormed_DTD(&inst);
  if (NS_FAILED(rv)) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    *aResult = nsnull;
  }
  NS_RELEASE(inst);             // get rid of extra refcnt
  return rv;
}

// Factory method to create new instances of nsNavHTMLDTD
static nsresult
CreateNewNavHTMLDTD(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_NOINTERFACE;
  }
  nsIDTD* inst;
  nsresult rv = NS_NewNavHTMLDTD(&inst);
  if (NS_FAILED(rv)) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    *aResult = nsnull;
  }
  NS_RELEASE(inst);             // get rid of extra refcnt
  return rv;
}

// Factory method to create new instances of nsXIFDTD
static nsresult
CreateNewXIFDTD(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_NOINTERFACE;
  }
  nsIDTD* inst;
  nsresult rv = NS_NewXIFDTD(&inst);
  if (!inst) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    *aResult = nsnull;
  }
  NS_RELEASE(inst);             // get rid of extra refcnt
  return rv;
}

//----------------------------------------------------------------------

class nsParserModule : public nsIModule {
public:
  nsParserModule();
  virtual ~nsParserModule();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIMODULE

  nsresult Initialize();

protected:
  void Shutdown();

  PRBool mInitialized;
};

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsParserModule::nsParserModule()
  : mInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsParserModule::~nsParserModule()
{
  Shutdown();
}

NS_IMPL_ISUPPORTS(nsParserModule, kIModuleIID)

nsresult
nsParserModule::Initialize()
{
  return NS_OK;
}

void
nsParserModule::Shutdown()
{
}

NS_IMETHODIMP
nsParserModule::GetClassObject(nsIComponentManager *aCompMgr,
                               const nsCID& aClass,
                               const nsIID& aIID,
                               void** r_classObj)
{
  nsresult rv;

  if (!mInitialized) {
    rv = Initialize();
    if (NS_FAILED(rv)) {
      return rv;
    }
    mInitialized = PR_TRUE;
  }

  nsCOMPtr<nsIGenericFactory> fact;

  if (aClass.Equals(kParserCID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewParser);
  }
  else if (aClass.Equals(kParserNodeCID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewParserNode);
  }
  else if (aClass.Equals(kLoggingSinkCID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewLoggingSink);
  }
  else if (aClass.Equals(kWellFormedDTDCID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewWellFormedDTD);
  }
  else if (aClass.Equals(kNavDTDCID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewNavHTMLDTD);
  }
  else if (aClass.Equals(kXIFDTDCID)) {
    rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewXIFDTD);
  }
  else {
		rv = NS_ERROR_FACTORY_NOT_REGISTERED;
  }

  if (fact) {
    rv = fact->QueryInterface(aIID, r_classObj);
  }

  return rv;
}

NS_IMETHODIMP
nsParserModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFileSpec* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
  nsresult rv = NS_OK;
  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                         nsnull, aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsParserModule: unable to register %s component => %x\n",
             cp->mDescription, rv);
#endif
      break;
    }
    cp++;
  }
  return rv;
}

NS_IMETHODIMP
nsParserModule::UnregisterSelf(nsIComponentManager *aCompMgr,
                               nsIFileSpec* aPath,
                               const char* registryLocation)
{
  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsParserModule: unable to unregister %s component => %x\n",
             cp->mDescription, rv);
#endif
    }
    cp++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsParserModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
  if (!okToUnload) {
    return NS_ERROR_INVALID_POINTER;
  }
  *okToUnload = PR_FALSE;
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

#if 0
static nsParserModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(return_cobj, "Null argument");
  NS_ASSERTION(gModule == NULL, "nsParserModule: Module already created.");

  // Create an initialize the layout module instance
  nsParserModule *m = new nsParserModule();
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
#endif
