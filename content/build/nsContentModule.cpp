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
#include "nsContentModule.h"
#include "nsContentCID.h"
#include "nsIComponentManager.h"
#include "nsNetUtil.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSLoader.h"
#include "nsHTMLAtoms.h"
#include "nsCSSKeywords.h"  // to addref/release table
#include "nsCSSProps.h"     // to addref/release table
#include "nsCSSAtoms.h"     // to addref/release table
#include "nsColorNames.h"   // to addref/release table
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

// XXX
#include "nsIServiceManager.h"

#include "nsRange.h"
#include "nsGenericElement.h"
#include "nsContentHTTPStartup.h"
#include "nsContentPolicyUtils.h"
#define PRODUCT_NAME "Gecko"

#ifdef MOZ_XUL
#include "nsXULAtoms.h"
#include "nsXULContentUtils.h"
#endif

static nsContentModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFile* location,
                                          nsIModule** return_cobj)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(return_cobj, "Null argument");
  NS_ASSERTION(gModule == NULL, "nsContentModule: Module already created.");

  // Create an initialize the Content module instance
  nsContentModule *m = new nsContentModule();
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

class ContentScriptNameSet : public nsIScriptExternalNameSet {
public:
  ContentScriptNameSet();
  virtual ~ContentScriptNameSet();

  NS_DECL_ISUPPORTS
  
  NS_IMETHOD InitializeClasses(nsIScriptContext* aScriptContext);
  NS_IMETHOD AddNameSet(nsIScriptContext* aScriptContext);
};

ContentScriptNameSet::ContentScriptNameSet()
{
  NS_INIT_REFCNT();
}

ContentScriptNameSet::~ContentScriptNameSet()
{
}

NS_IMPL_ISUPPORTS(ContentScriptNameSet, NS_GET_IID(nsIScriptExternalNameSet));

NS_IMETHODIMP 
ContentScriptNameSet::InitializeClasses(nsIScriptContext* aScriptContext)
{
  return NS_OK;
}

NS_IMETHODIMP
ContentScriptNameSet::AddNameSet(nsIScriptContext* aScriptContext)
{
  nsresult result = NS_OK;
  nsIScriptNameSpaceManager* manager;
  static NS_DEFINE_IID(kHTMLImageElementCID, NS_HTMLIMAGEELEMENT_CID);
  static NS_DEFINE_IID(kHTMLOptionElementCID, NS_HTMLOPTIONELEMENT_CID);

  result = aScriptContext->GetNameSpaceManager(&manager);
  if (NS_OK == result) {
    result = manager->RegisterGlobalName(NS_LITERAL_STRING("HTMLImageElement"),
                                         NS_GET_IID(nsIScriptObjectOwner),
                                         kHTMLImageElementCID,
                                         PR_TRUE);
    if (NS_FAILED(result)) {
      NS_RELEASE(manager);
      return result;
    }

    result = manager->RegisterGlobalName(NS_LITERAL_STRING("HTMLOptionElement"),
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


nsIScriptNameSetRegistry* nsContentModule::gRegistry;
nsICSSStyleSheet* nsContentModule::gUAStyleSheet = nsnull;

nsContentModule::nsContentModule()
  : mInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsContentModule::~nsContentModule()
{
  Shutdown();
}

NS_IMPL_ISUPPORTS(nsContentModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult
nsContentModule::Initialize()
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

#ifdef MOZ_XUL
  nsXULAtoms::AddRefAtoms();
  nsXULContentUtils::Init();
#endif

  // XXX Initialize the script name set thingy-ma-jigger
  if (!gRegistry) {
    rv = nsServiceManager::GetService(kCScriptNameSetRegistryCID,
                                      NS_GET_IID(nsIScriptNameSetRegistry),
                                      (nsISupports**) &gRegistry);
    if (NS_SUCCEEDED(rv)) {
      ContentScriptNameSet* nameSet = new ContentScriptNameSet();
      gRegistry->AddExternalNameSet(nameSet);
    }
  }

  return rv;
}

// Shutdown this module, releasing all of the module resources
void
nsContentModule::Shutdown()
{
  if (!mInitialized) {
    return;
  }

  nsRange::Shutdown();
  nsGenericElement::Shutdown();
    
  // Release all of our atoms
  nsColorNames::ReleaseTable();
  nsCSSProps::ReleaseTable();
  nsCSSKeywords::ReleaseTable();
  nsCSSAtoms::ReleaseAtoms();
  nsHTMLAtoms::ReleaseAtoms();
  nsLayoutAtoms::ReleaseAtoms();

#ifdef MOZ_XUL
  nsXULContentUtils::Finish();
  nsXULAtoms::ReleaseAtoms();
#endif

  NS_IF_RELEASE(gRegistry);
  NS_IF_RELEASE(gUAStyleSheet);
}

NS_IMETHODIMP
nsContentModule::GetClassObject(nsIComponentManager *aCompMgr,
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

  nsCOMPtr<nsIFactory> f = new nsContentFactory(aClass);
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
  { "Namespace manager", NS_NAMESPACEMANAGER_CID, nsnull, },
  { "Event listener manager", NS_EVENTLISTENERMANAGER_CID, nsnull, },
  { "Event state manager", NS_EVENTSTATEMANAGER_CID, nsnull, },

  { "Document Viewer", NS_DOCUMENT_VIEWER_CID, nsnull, },
  { "HTML Style Sheet", NS_HTMLSTYLESHEET_CID, nsnull, },
  { "Style Set", NS_STYLESET_CID, nsnull, },
  { "Frame Selection", NS_FRAMESELECTION_CID, nsnull, },
  { "CSS Style Sheet", NS_CSS_STYLESHEET_CID, nsnull, },

  { "HTML document", NS_HTMLDOCUMENT_CID, nsnull, },
  { "HTML-CSS style sheet", NS_HTML_CSS_STYLESHEET_CID, nsnull, },

  { "DOM implementation", NS_DOM_IMPLEMENTATION_CID, nsnull, },

  { "XML document", NS_XMLDOCUMENT_CID, nsnull, },
  { "Image document", NS_IMAGEDOCUMENT_CID, nsnull, },

  { "CSS parser", NS_CSSPARSER_CID, nsnull, },
  { "CSS loader", NS_CSS_LOADER_CID, nsnull, },

  { "HTML element factory", NS_HTML_ELEMENT_FACTORY_CID, NS_HTML_ELEMENT_FACTORY_CONTRACTID, },
  { "Text element", NS_TEXTNODE_CID, nsnull, },

  { "Anonymous Content", NS_ANONYMOUSCONTENT_CID, nsnull, },

  { "Attribute Content", NS_ATTRIBUTECONTENT_CID, nsnull, },

  { "HTML Attributes", NS_HTMLATTRIBUTES_CID, nsnull, },

  { "XML element factory", NS_XML_ELEMENT_FACTORY_CID, NS_XML_ELEMENT_FACTORY_CONTRACTID, },

  { "Selection", NS_SELECTION_CID, nsnull, },
  { "Dom selection", NS_DOMSELECTION_CID, nsnull, },
  { "Frame selection", NS_FRAMESELECTION_CID, nsnull, },
  { "Range", NS_RANGE_CID, nsnull, },
  { "Content iterator", NS_CONTENTITERATOR_CID, nsnull, },
  { "Generated Content iterator", NS_GENERATEDCONTENTITERATOR_CID, nsnull, },
  { "Generated Subtree iterator", NS_GENERATEDSUBTREEITERATOR_CID, nsnull, },
  { "Subtree iterator", NS_SUBTREEITERATOR_CID, nsnull, },

  // XXX ick
  { "HTML image element", NS_HTMLIMAGEELEMENT_CID, nsnull, },
  { "HTML option element", NS_HTMLOPTIONELEMENT_CID, nsnull, },
  // XXX end ick

  { "XML document encoder", NS_TEXT_ENCODER_CID,
    NS_DOC_ENCODER_CONTRACTID_BASE "text/xml", },
  { "HTML document encoder", NS_TEXT_ENCODER_CID,
    NS_DOC_ENCODER_CONTRACTID_BASE "text/html", },
  { "Plaintext document encoder", NS_TEXT_ENCODER_CID,
    NS_DOC_ENCODER_CONTRACTID_BASE "text/plain", },
  { "HTML copy encoder", NS_HTMLCOPY_TEXT_ENCODER_CID,
    NS_HTMLCOPY_ENCODER_CONTRACTID, },
  { "XML content serializer", NS_XMLCONTENTSERIALIZER_CID,
    NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "text/xml", },
  { "HTML content serializer", NS_HTMLCONTENTSERIALIZER_CID,
    NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "text/html", },
  { "XUL content serializer", NS_XMLCONTENTSERIALIZER_CID,
    NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "application/vnd.mozilla.xul+xml", },
  { "plaintext content serializer", NS_PLAINTEXTSERIALIZER_CID,
    NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "text/plain", },
  { "plaintext sink", NS_PLAINTEXTSERIALIZER_CID,
    NS_PLAINTEXTSINK_CONTRACTID, },

  { "XBL Service", NS_XBLSERVICE_CID, "@mozilla.org/xbl;1" },
  { "XBL Binding Manager", NS_BINDINGMANAGER_CID, "@mozilla.org/xbl/binding-manager;1" },
  
  { "Content policy service", NS_CONTENTPOLICY_CID, NS_CONTENTPOLICY_CONTRACTID },
  { "NodeInfoManager", NS_NODEINFOMANAGER_CID, NS_NODEINFOMANAGER_CONTRACTID },
  { "DOM CSS Computed Style Declaration", NS_COMPUTEDDOMSTYLE_CID,
    "@mozilla.org/DOM/Level2/CSS/computedStyleDeclaration;1" },

#ifdef MOZ_XUL
  { "XUL Sort Service", NS_XULSORTSERVICE_CID,
    "@mozilla.org/xul/xul-sort-service;1", },
  { "XUL Template Builder", NS_XULTEMPLATEBUILDER_CID,
    "@mozilla.org/xul/xul-template-builder;1", },
  { "XUL Outliner Builder", NS_XULOUTLINERBUILDER_CID,
    "@mozilla.org/xul/xul-outliner-builder;1", },
  { "XUL Content Sink", NS_XULCONTENTSINK_CID,
    "@mozilla.org/xul/xul-content-sink;1", },
  { "XUL Document", NS_XULDOCUMENT_CID,
    "@mozilla.org/xul/xul-document;1", },
  { "XUL PopupListener", NS_XULPOPUPLISTENER_CID,
    "@mozilla.org/xul/xul-popup-listener;1", },
  { "XUL Controllers", NS_XULCONTROLLERS_CID,
    "@mozilla.org/xul/xul-controllers;1", },
  { "XUL Prototype Cache", NS_XULPROTOTYPECACHE_CID,
    "@mozilla.org/xul/xul-prototype-cache;1", },
  { "XUL Element Factory", NS_XULELEMENTFACTORY_CID,
    NS_ELEMENT_FACTORY_CONTRACTID_PREFIX "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", },
#endif
  { "Controller Command Manager", NS_CONTROLLERCOMMANDMANAGER_CID,
    "@mozilla.org/content/controller-command-manager;1", },
  { "Content HTTP Startup Listener", NS_CONTENTHTTPSTARTUP_CID,
    NS_CONTENTHTTPSTARTUP_CONTRACTID, },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsContentModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFile* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
  nsresult rv = NS_OK;

#ifdef DEBUG
  printf("*** Registering content components\n");
#endif

  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    rv = aCompMgr->RegisterComponentSpec(cp->mCID, cp->mDescription,
                                         cp->mContractID, aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsContentModule: unable to register %s component => %x\n",
             cp->mDescription, rv);
#endif
      break;
    }
    cp++;
  }

  // not fatal if these fail
  nsContentHTTPStartup::RegisterHTTPStartup();

  rv = RegisterDocumentFactories(aCompMgr, aPath);
  
  return rv;
}

NS_IMETHODIMP
nsContentModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFile* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
  printf("*** Unregistering content components\n");
#endif
  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    nsresult rv = aCompMgr->UnregisterComponentSpec(cp->mCID, aPath);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsContentModule: unable to unregister %s component => %x\n",
             cp->mDescription, rv);
#endif
    }
    cp++;
  }

  nsContentHTTPStartup::UnregisterHTTPStartup();
  UnregisterDocumentFactories(aCompMgr, aPath);

  return NS_OK;
}

NS_IMETHODIMP
nsContentModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
  if (!okToUnload) {
    return NS_ERROR_INVALID_POINTER;
  }
  *okToUnload = PR_FALSE;
  return NS_ERROR_FAILURE;
}

