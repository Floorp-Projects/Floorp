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
#include "nspr.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsLayoutModule.h"
#include "nsLayoutCID.h"
#include "nsIComponentManager.h"
#include "nsNetUtil.h"
#include "nsICSSStyleSheet.h"
#include "nsHTMLAtoms.h"
#include "nsCSSKeywords.h"  // to addref/release table
#include "nsCSSProps.h"     // to addref/release table
#include "nsCSSAtoms.h"     // to addref/release table
#include "nsColorNames.h"   // to addref/release table
#ifdef INCLUDE_XUL
#include "nsXULAtoms.h"
#endif
//MathML Mod - RBS
#ifdef MOZ_MATHML
#include "nsMathMLAtoms.h"
#include "nsMathMLOperators.h"
#endif
#include "nsLayoutAtoms.h"
#include "nsDOMCID.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"

#include "nsINodeInfo.h"

#include "nsIElementFactory.h"

#include "nsIDocumentEncoder.h"
#include "nsIContentSerializer.h"
#include "nsIHTMLToTextSink.h"

// SVG
#ifdef MOZ_SVG
#include "nsSVGAtoms.h"
#endif

// XXX
#include "nsIServiceManager.h"

#include "nsTextTransformer.h"
#ifdef INCLUDE_XUL
#include "nsBulletinBoardLayout.h"
#include "nsRepeatService.h"
#include "nsSprocketLayout.h"
#include "nsStackLayout.h"
#endif
#include "nsIHTTPProtocolHandler.h"
#include "gbdate.h"
#include "nsContentPolicyUtils.h"
#define PRODUCT_NAME "Gecko"

static NS_DEFINE_CID(kHTTPHandlerCID, NS_IHTTPHANDLER_CID);
static nsLayoutModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFile* location,
                                          nsIModule** return_cobj)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(return_cobj, "Null argument");
  NS_ASSERTION(gModule == NULL, "nsLayoutModule: Module already created.");

  // Create an initialize the layout module instance
  nsLayoutModule *m = new nsLayoutModule();
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

//----------------------------------------------------------------------


static NS_DEFINE_IID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);

class LayoutScriptNameSet : public nsIScriptExternalNameSet {
public:
  LayoutScriptNameSet();
  virtual ~LayoutScriptNameSet();

  NS_DECL_ISUPPORTS
  
  NS_IMETHOD InitializeClasses(nsIScriptContext* aScriptContext);
  NS_IMETHOD AddNameSet(nsIScriptContext* aScriptContext);
};

LayoutScriptNameSet::LayoutScriptNameSet()
{
  NS_INIT_REFCNT();
}

LayoutScriptNameSet::~LayoutScriptNameSet()
{
}

NS_IMPL_ISUPPORTS(LayoutScriptNameSet, NS_GET_IID(nsIScriptExternalNameSet));

NS_IMETHODIMP 
LayoutScriptNameSet::InitializeClasses(nsIScriptContext* aScriptContext)
{
  return NS_OK;
}

NS_IMETHODIMP
LayoutScriptNameSet::AddNameSet(nsIScriptContext* aScriptContext)
{
  nsresult result = NS_OK;
  nsIScriptNameSpaceManager* manager;
  static NS_DEFINE_IID(kHTMLImageElementCID, NS_HTMLIMAGEELEMENT_CID);
  static NS_DEFINE_IID(kHTMLOptionElementCID, NS_HTMLOPTIONELEMENT_CID);

  result = aScriptContext->GetNameSpaceManager(&manager);
  if (NS_OK == result) {
    result = manager->RegisterGlobalName(NS_ConvertToString("HTMLImageElement"),
                                         NS_GET_IID(nsIScriptObjectOwner),
                                         kHTMLImageElementCID,
                                         PR_TRUE);
    if (NS_FAILED(result)) {
      NS_RELEASE(manager);
      return result;
    }

    result = manager->RegisterGlobalName(NS_ConvertToString("HTMLOptionElement"),
                                         NS_GET_IID(nsIScriptObjectOwner),
                                         kHTMLOptionElementCID,
                                         PR_TRUE);
    if (NS_FAILED(result)) {
      NS_RELEASE(manager);
      return result;
    }
        
    NS_RELEASE(manager);
  }
  
  return result;
}

//----------------------------------------------------------------------


nsIScriptNameSetRegistry* nsLayoutModule::gRegistry;
nsICSSStyleSheet* nsLayoutModule::gUAStyleSheet = nsnull;

nsLayoutModule::nsLayoutModule()
  : mInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsLayoutModule::~nsLayoutModule()
{
  Shutdown();
}

NS_IMPL_ISUPPORTS(nsLayoutModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult
nsLayoutModule::Initialize()
{
  nsresult rv = NS_OK;

  if (mInitialized) {
    return NS_OK;
  }

  mInitialized = PR_TRUE;
    
  // Register all of our atoms once
  nsCSSAtoms::AddRefAtoms();
  nsCSSKeywords::AddRefTable();
  nsCSSProps::AddRefTable();
  nsColorNames::AddRefTable();
  nsHTMLAtoms::AddRefAtoms();
  nsLayoutAtoms::AddRefAtoms();
#ifdef INCLUDE_XUL
  nsXULAtoms::AddRefAtoms();
#endif
//MathML Mod - RBS
#ifdef MOZ_MATHML
  nsMathMLOperators::AddRefTable();
  nsMathMLAtoms::AddRefAtoms();
#endif

// SVG
#ifdef MOZ_SVG
  nsSVGAtoms::AddRefAtoms();
#endif

  // XXX Initialize the script name set thingy-ma-jigger
  if (!gRegistry) {
    rv = nsServiceManager::GetService(kCScriptNameSetRegistryCID,
                                      NS_GET_IID(nsIScriptNameSetRegistry),
                                      (nsISupports**) &gRegistry);
    if (NS_SUCCEEDED(rv)) {
      LayoutScriptNameSet* nameSet = new LayoutScriptNameSet();
      gRegistry->AddExternalNameSet(nameSet);
    }
  }

  SetUserAgent();

  rv = nsTextTransformer::Initialize();
  if (NS_FAILED(rv)) {
    return rv;
  }

  return rv;
}

// Shutdown this module, releasing all of the module resources
void
nsLayoutModule::Shutdown()
{
  if (!mInitialized) {
    return;
  }

#ifdef INCLUDE_XUL
  nsBulletinBoardLayout::Shutdown();
  nsRepeatService::Shutdown();
  nsSprocketLayout::Shutdown();
  nsStackLayout::Shutdown();
#endif

  // Release all of our atoms
  nsColorNames::ReleaseTable();
  nsCSSProps::ReleaseTable();
  nsCSSKeywords::ReleaseTable();
  nsCSSAtoms::ReleaseAtoms();
  nsHTMLAtoms::ReleaseAtoms();
  nsLayoutAtoms::ReleaseAtoms();
#ifdef INCLUDE_XUL
  nsXULAtoms::ReleaseAtoms();
#endif  
//MathML Mod - RBS
#ifdef MOZ_MATHML
  nsMathMLOperators::ReleaseTable();
  nsMathMLAtoms::ReleaseAtoms();
#endif
// SVG
#ifdef MOZ_SVG
  nsSVGAtoms::ReleaseAtoms();
#endif

  nsTextTransformer::Shutdown();

  NS_IF_RELEASE(gRegistry);
  NS_IF_RELEASE(gUAStyleSheet);
}

NS_IMETHODIMP
nsLayoutModule::GetClassObject(nsIComponentManager *aCompMgr,
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
  }

  nsCOMPtr<nsIFactory> f = new nsLayoutFactory(aClass);
  if (!f) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return f->QueryInterface(aIID, r_classObj);
}

//----------------------------------------

struct Components {
  const char* mDescription;
  nsID mCID;
  const char* mContractID;
};

// The HTML namespace.

// The list of components we register
static Components gComponents[] = {
  { "Frame utility", NS_FRAME_UTIL_CID, nsnull, },
  { "Print preview context", NS_PRINT_PREVIEW_CONTEXT_CID, nsnull, },
  { "Layout debugger", NS_LAYOUT_DEBUGGER_CID, nsnull, },

  { "CSS Frame Constructor", NS_CSSFRAMECONSTRUCTOR_CID, nsnull, },
  { "Frame Traversal", NS_FRAMETRAVERSAL_CID, nsnull, },

  // XXX ick
  { "Presentation shell", NS_PRESSHELL_CID, nsnull, },
  { "Presentation state", NS_PRESSTATE_CID, nsnull, },
  { "Galley context", NS_GALLEYCONTEXT_CID, nsnull, },
  { "Print context", NS_PRINTCONTEXT_CID, nsnull, },
  // XXX end ick

  { "XUL Box Object", NS_BOXOBJECT_CID, "@mozilla.org/layout/xul-boxobject;1" },
  { "XUL Tree Box Object", NS_TREEBOXOBJECT_CID, "@mozilla.org/layout/xul-boxobject-tree;1" },
  { "XUL Menu Box Object", NS_MENUBOXOBJECT_CID, "@mozilla.org/layout/xul-boxobject-menu;1" },
  { "XUL PopupSet Box Object", NS_POPUPSETBOXOBJECT_CID, "@mozilla.org/layout/xul-boxobject-popupset;1" },
  { "XUL Browser Box Object", NS_BROWSERBOXOBJECT_CID, "@mozilla.org/layout/xul-boxobject-browser;1" },
  { "XUL Editor Box Object", NS_EDITORBOXOBJECT_CID, "@mozilla.org/layout/xul-boxobject-editor;1" },
  { "XUL Iframe Object", NS_IFRAMEBOXOBJECT_CID, "@mozilla.org/layout/xul-boxobject-iframe;1" },
  { "XUL ScrollBox Object", NS_SCROLLBOXOBJECT_CID, "@mozilla.org/layout/xul-boxobject-scrollbox;1" },
  { "XUL Outliner Box Object", NS_OUTLINERBOXOBJECT_CID, "@mozilla.org/layout/xul-boxobject-outliner;1" },

  { "AutoCopy Service", NS_AUTOCOPYSERVICE_CID, "@mozilla.org/autocopy;1" }
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsLayoutModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFile* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
  nsresult rv = NS_OK;

#ifdef DEBUG
  printf("*** Registering layout components\n");
#endif

  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    rv = aCompMgr->RegisterComponentSpec(cp->mCID, cp->mDescription,
                                         cp->mContractID, aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsLayoutModule: unable to register %s component => %x\n",
             cp->mDescription, rv);
#endif
      break;
    }
    cp++;
  }

  rv = RegisterDocumentFactories(aCompMgr, aPath);

  return rv;
}

NS_IMETHODIMP
nsLayoutModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFile* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
  printf("*** Unregistering layout components\n");
#endif
  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    nsresult rv = aCompMgr->UnregisterComponentSpec(cp->mCID, aPath);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsLayoutModule: unable to unregister %s component => %x\n",
             cp->mDescription, rv);
#endif
    }
    cp++;
  }

  UnregisterDocumentFactories(aCompMgr, aPath);

  return NS_OK;
}

NS_IMETHODIMP
nsLayoutModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
  if (!okToUnload) {
    return NS_ERROR_INVALID_POINTER;
  }
  *okToUnload = PR_FALSE;
  return NS_ERROR_FAILURE;
}

void
nsLayoutModule::SetUserAgent( void )
{
  nsString productName; productName.AssignWithConversion(PRODUCT_NAME);
  nsString productVersion; productVersion.AssignWithConversion(PRODUCT_VERSION);
  nsresult rv = nsnull;

  nsCOMPtr<nsIHTTPProtocolHandler> theService(do_GetService(kHTTPHandlerCID,
								   &rv));

  if( NS_SUCCEEDED(rv) && (nsnull != theService) )
  {
    rv = theService->SetProduct(productName.GetUnicode());
	
    rv = theService->SetProductSub(productVersion.GetUnicode());
  }
}
