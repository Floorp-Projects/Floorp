/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsXBLAtoms.h"     // to addref/release table
#include "nsCSSAtoms.h"     // to addref/release table
#include "nsCSSKeywords.h"  // to addref/release table
#include "nsCSSProps.h"     // to addref/release table
#include "nsColorNames.h"   // to addref/release table
#include "nsContentCID.h"
#include "nsContentHTTPStartup.h"
#include "nsContentDLF.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsDOMCID.h"
#include "nsCSSOMFactory.h"
#include "nsEventStateManager.h"
#include "nsGenericElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLContentSerializer.h"
#include "nsGenericHTMLElement.h"
#include "nsIBindingManager.h"
#include "nsICSSLoader.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsICategoryManager.h"
#include "nsIComponentManager.h"
#include "nsIComputedDOMStyle.h"
#include "nsIContentIterator.h"
#include "nsIContentSerializer.h"
#include "nsIController.h"
#include "nsIControllerCommand.h"
#include "nsIControllers.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMRange.h"
#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsIDocumentViewer.h"
#include "nsIElementFactory.h"
#include "nsIEventListenerManager.h"
#include "nsIFactory.h"
#include "nsIFrameSelection.h"
#include "nsIFrameUtil.h"
#include "nsIGenericFactory.h"
#include "nsIHTMLAttributes.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLToTextSink.h"
#include "nsILayoutDebugger.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIPrivateDOMImplementation.h"
#include "nsIRangeUtils.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsISelection.h"
#include "nsITextContent.h"
#include "nsIXBLService.h"
#include "nsLayoutAtoms.h"
#include "nsPlainTextSerializer.h"
#include "nsRange.h"
#include "nsXMLContentSerializer.h"

class nsIDocumentLoaderFactory;

#define PRODUCT_NAME "Gecko"

#define NS_HTMLIMGELEMENT_CONTRACTID \
  "@mozilla.org/content/element/html;1?name=img"

#define NS_HTMLOPTIONELEMENT_CONTRACTID \
  "@mozilla.org/content/element/html;1?name=option"


#ifdef MOZ_XUL
#include "nsIRDFContentModelBuilder.h"
#include "nsIXULContentSink.h"
#include "nsIXULDocument.h"
#include "nsIXULPopupListener.h"
#include "nsIXULPrototypeCache.h"
#include "nsIXULPrototypeDocument.h"
#include "nsIXULPrototypeDocument.h"
#include "nsIXULSortService.h"
#include "nsXULAtoms.h"
#include "nsXULContentUtils.h"
#include "nsXULElement.h"
#endif

// jst says, ``we need this to avoid holding on to XPConnect past its
// destruction. By being an XPCOM shutdown observer we can make sure
// we release the content global reference to XPConnect before
// XPConnect is shutdown, which cuts down on leaks n' whatnot...''
class ContentShutdownObserver : public nsIObserver
{
public:
  ContentShutdownObserver() { NS_INIT_ISUPPORTS(); }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(ContentShutdownObserver, nsIObserver)

NS_IMETHODIMP
ContentShutdownObserver::Observe(nsISupports *aSubject,
                                 const char *aTopic,
                                 const PRUnichar *someData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
    nsContentUtils::Shutdown();

  return NS_OK;
}

static PRBool gInitialized = PR_FALSE;

// Perform our one-time intialization for this module
PR_STATIC_CALLBACK(nsresult)
Initialize(nsIModule* aSelf)
{
  // XXXwaterson turns out we initialize the module twice, because
  // nsXULAtoms::AddRefAtoms() creates a namespace manager using the
  // component manager. We should probably fix that.
  //NS_PRECONDITION(! gInitialized, "module already initialized");
  if (gInitialized)
    return NS_OK;

  gInitialized = PR_TRUE;
    
  // Register all of our atoms once
  nsCSSAtoms::AddRefAtoms();
  nsCSSKeywords::AddRefTable();
  nsCSSProps::AddRefTable();
  nsColorNames::AddRefTable();
  nsHTMLAtoms::AddRefAtoms();
  nsXBLAtoms::AddRefAtoms();
  nsLayoutAtoms::AddRefAtoms();

#ifdef MOZ_XUL
  nsXULAtoms::AddRefAtoms();
  nsXULContentUtils::Init();
#endif

  nsContentUtils::Init();

  // Add our shutdown observer.
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");

  if (observerService) {
    ContentShutdownObserver* observer =
      new ContentShutdownObserver();

    if (observer)
      observerService->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
  }

  return NS_OK;
}

// Shutdown this module, releasing all of the module resources
PR_STATIC_CALLBACK(void)
Shutdown(nsIModule* aSelf)
{
  NS_PRECONDITION(gInitialized, "module not initialized");
  if (! gInitialized)
    return;

  gInitialized = PR_FALSE;

  nsRange::Shutdown();
  nsGenericElement::Shutdown();

  // Release all of our atoms
  nsColorNames::ReleaseTable();
  nsCSSProps::ReleaseTable();
  nsCSSKeywords::ReleaseTable();
  nsCSSAtoms::ReleaseAtoms();
  nsHTMLAtoms::ReleaseAtoms();
  nsXBLAtoms::ReleaseAtoms();
  nsLayoutAtoms::ReleaseAtoms();

#ifdef MOZ_XUL
  nsXULContentUtils::Finish();
  nsXULAtoms::ReleaseAtoms();
  nsXULElement::ReleaseGlobals();
#endif

  NS_IF_RELEASE(nsContentDLF::gUAStyleSheet);

  nsContentUtils::Shutdown();
  nsGenericHTMLElement::Shutdown();

  NS_NameSpaceManagerShutdown();
}

extern nsresult NS_NewSelection(nsIFrameSelection** aResult);
extern nsresult NS_NewDomSelection(nsISelection** aResult);
extern nsresult NS_NewDocumentViewer(nsIDocumentViewer** aResult);
extern nsresult NS_NewRange(nsIDOMRange** aResult);
extern nsresult NS_NewRangeUtils(nsIRangeUtils** aResult);
extern nsresult NS_NewContentIterator(nsIContentIterator** aResult);
extern nsresult NS_NewGenRegularIterator(nsIContentIterator** aResult);
extern nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aResult);
extern nsresult NS_NewGenSubtreeIterator(nsIContentIterator** aInstancePtrResult);
extern nsresult NS_NewContentDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult);
extern nsresult NS_NewHTMLElementFactory(nsIElementFactory** aResult);
extern nsresult NS_NewXMLElementFactory(nsIElementFactory** aResult);
extern nsresult NS_NewHTMLCopyTextEncoder(nsIDocumentEncoder** aResult);
extern nsresult NS_NewTextEncoder(nsIDocumentEncoder** aResult);
extern nsresult NS_NewXBLService(nsIXBLService** aResult);
extern nsresult NS_NewBindingManager(nsIBindingManager** aResult);
extern nsresult NS_NewNodeInfoManager(nsINodeInfoManager** aResult);
extern nsresult NS_NewContentPolicy(nsIContentPolicy** aResult);

#ifdef MOZ_XUL
extern nsresult NS_NewXULElementFactory(nsIElementFactory** aResult);
extern NS_IMETHODIMP NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult);
#endif

#define MAKE_CTOR(ctor_, iface_, func_)                   \
static NS_IMETHODIMP                                      \
ctor_(nsISupports* aOuter, REFNSIID aIID, void** aResult) \
{                                                         \
  *aResult = nsnull;                                      \
  if (aOuter)                                             \
    return NS_ERROR_NO_AGGREGATION;                       \
  iface_* inst;                                           \
  nsresult rv = func_(&inst);                             \
  if (NS_SUCCEEDED(rv)) {                                 \
    rv = inst->QueryInterface(aIID, aResult);             \
    NS_RELEASE(inst);                                     \
  }                                                       \
  return rv;                                              \
}

MAKE_CTOR(CreateNameSpaceManager,         nsINameSpaceManager,         NS_NewNameSpaceManager)
MAKE_CTOR(CreateEventListenerManager,     nsIEventListenerManager,     NS_NewEventListenerManager)
MAKE_CTOR(CreateEventStateManager,        nsIEventStateManager,        NS_NewEventStateManager)
MAKE_CTOR(CreateDocumentViewer,           nsIDocumentViewer,           NS_NewDocumentViewer)
MAKE_CTOR(CreateHTMLStyleSheet,           nsIHTMLStyleSheet,           NS_NewHTMLStyleSheet)
MAKE_CTOR(CreateStyleSet,                 nsIStyleSet,                 NS_NewStyleSet)
MAKE_CTOR(CreateCSSStyleSheet,            nsICSSStyleSheet,            NS_NewCSSStyleSheet)
MAKE_CTOR(CreateHTMLDocument,             nsIDocument,                 NS_NewHTMLDocument)
MAKE_CTOR(CreateHTMLCSSStyleSheet,        nsIHTMLCSSStyleSheet,        NS_NewHTMLCSSStyleSheet)
MAKE_CTOR(CreateDOMImplementation,        nsIDOMDOMImplementation,     NS_NewDOMImplementation)
MAKE_CTOR(CreateXMLDocument,              nsIDocument,                 NS_NewXMLDocument)
MAKE_CTOR(CreateImageDocument,            nsIDocument,                 NS_NewImageDocument)
MAKE_CTOR(CreateCSSParser,                nsICSSParser,                NS_NewCSSParser)
MAKE_CTOR(CreateCSSLoader,                nsICSSLoader,                NS_NewCSSLoader)
MAKE_CTOR(CreateHTMLElementFactory,       nsIElementFactory,           NS_NewHTMLElementFactory)
MAKE_CTOR(CreateTextNode,                 nsIContent,                  NS_NewTextNode)
//MAKE_CTOR(CreateAnonymousElement,         nsIContent,                  NS_NewAnonymousElement)
MAKE_CTOR(CreateAttributeContent,         nsIContent,                  NS_NewAttributeContent)
MAKE_CTOR(CreateXMLElementFactory,        nsIElementFactory,           NS_NewXMLElementFactory)
//MAKE_CTOR(CreateSelection,                nsISelection,                NS_NewSelection)
MAKE_CTOR(CreateDOMSelection,             nsISelection,                NS_NewDomSelection)
MAKE_CTOR(CreateSelection,                nsIFrameSelection,           NS_NewSelection)
MAKE_CTOR(CreateRange,                    nsIDOMRange,                 NS_NewRange)
MAKE_CTOR(CreateRangeUtils,               nsIRangeUtils,               NS_NewRangeUtils)
MAKE_CTOR(CreateContentIterator,          nsIContentIterator,          NS_NewContentIterator)
MAKE_CTOR(CreateGeneratedContentIterator, nsIContentIterator,          NS_NewGenRegularIterator)
MAKE_CTOR(CreateGeneratedSubtreeIterator, nsIContentIterator,          NS_NewGenSubtreeIterator)
MAKE_CTOR(CreateSubtreeIterator,          nsIContentIterator,          NS_NewContentSubtreeIterator)
// CreateHTMLImgElement, see below
// CreateHTMLOptionElement, see below
MAKE_CTOR(CreateTextEncoder,              nsIDocumentEncoder,          NS_NewTextEncoder)
MAKE_CTOR(CreateHTMLCopyTextEncoder,      nsIDocumentEncoder,          NS_NewHTMLCopyTextEncoder)
MAKE_CTOR(CreateXMLContentSerializer,     nsIContentSerializer,        NS_NewXMLContentSerializer)
MAKE_CTOR(CreateHTMLContentSerializer,    nsIContentSerializer,        NS_NewHTMLContentSerializer)
MAKE_CTOR(CreatePlainTextSerializer,      nsIContentSerializer,        NS_NewPlainTextSerializer)
MAKE_CTOR(CreateXBLService,               nsIXBLService,               NS_NewXBLService)
MAKE_CTOR(CreateBindingManager,           nsIBindingManager,           NS_NewBindingManager)
MAKE_CTOR(CreateContentPolicy,            nsIContentPolicy,            NS_NewContentPolicy)
MAKE_CTOR(CreateNodeInfoManager,          nsINodeInfoManager,          NS_NewNodeInfoManager)
MAKE_CTOR(CreateComputedDOMStyle,         nsIComputedDOMStyle,         NS_NewComputedDOMStyle)
#ifdef MOZ_XUL
MAKE_CTOR(CreateXULSortService,           nsIXULSortService,           NS_NewXULSortService)
// NS_NewXULContentBuilder
// NS_NewXULOutlinerBuilder
MAKE_CTOR(CreateXULContentSink,           nsIXULContentSink,           NS_NewXULContentSink)
MAKE_CTOR(CreateXULDocument,              nsIXULDocument,              NS_NewXULDocument)
MAKE_CTOR(CreateXULPopupListener,         nsIXULPopupListener,         NS_NewXULPopupListener)
// NS_NewXULControllers
// NS_NewXULPrototypeCache
MAKE_CTOR(CreateXULElementFactory,        nsIElementFactory,           NS_NewXULElementFactory)
#endif
MAKE_CTOR(CreateControllerCommandManager, nsIControllerCommandManager, NS_NewControllerCommandManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsContentHTTPStartup)
MAKE_CTOR(CreateContentDLF,                nsIDocumentLoaderFactory,   NS_NewContentDocumentLoaderFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCSSOMFactory)

static NS_IMETHODIMP
CreateHTMLImgElement(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  *aResult = nsnull;
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  nsIHTMLContent* inst;
  // Note! NS_NewHTMLImageElement is special cased to handle a null nodeinfo
  nsresult rv = NS_NewHTMLImageElement(&inst, nsnull);
  if (NS_SUCCEEDED(rv)) {
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);
  }
  return rv;
}

static NS_IMETHODIMP
RegisterHTMLImgElement(nsIComponentManager *aCompMgr,
                       nsIFile* aPath,
                       const char* aRegistryLocation,
                       const char* aComponentType,
                       const nsModuleComponentInfo* aInfo)
{
  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);

  if (! catman)
    return NS_ERROR_FAILURE;

  nsXPIDLCString previous;
  return catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                  "Image", NS_HTMLIMGELEMENT_CONTRACTID,
                                  PR_TRUE, PR_TRUE, getter_Copies(previous));
}

static NS_IMETHODIMP
UnregisterHTMLImgElement(nsIComponentManager* aCompMgr,
                         nsIFile* aPath,
                         const char* aRegistryLocation,
                         const nsModuleComponentInfo* aInfo)
{
  // XXX remove category entry
  return NS_OK;
}

static NS_IMETHODIMP
CreateHTMLOptionElement(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  *aResult = nsnull;
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  nsIHTMLContent* inst;
  // Note! NS_NewHTMLOptionElement is special cased to handle a null nodeinfo
  nsresult rv = NS_NewHTMLOptionElement(&inst, nsnull);
  if (NS_SUCCEEDED(rv)) {
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);
  }
  return rv;
}

static NS_IMETHODIMP
RegisterHTMLOptionElement(nsIComponentManager *aCompMgr,
                          nsIFile* aPath,
                          const char* aRegistryLocation,
                          const char* aComponentType,
                          const nsModuleComponentInfo* aInfo)
{
  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);

  if (! catman)
    return NS_ERROR_FAILURE;

  nsXPIDLCString previous;
  return catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                  "Option", NS_HTMLOPTIONELEMENT_CONTRACTID,
                                  PR_TRUE, PR_TRUE, getter_Copies(previous));
}

static NS_IMETHODIMP
UnregisterHTMLOptionElement(nsIComponentManager* aCompMgr,
                            nsIFile* aPath,
                            const char* aRegistryLocation,
                            const nsModuleComponentInfo* aInfo)
{
  // XXX remove category entry
  return NS_OK;
}

// The list of components we register
static nsModuleComponentInfo gComponents[] = {
  { "Namespace manager",
    NS_NAMESPACEMANAGER_CID,
    nsnull,
    CreateNameSpaceManager },

  { "Event listener manager",
    NS_EVENTLISTENERMANAGER_CID,
    nsnull,
    CreateEventListenerManager },

  { "Event state manager",
    NS_EVENTSTATEMANAGER_CID,
    nsnull,
    CreateEventStateManager },

  { "Document Viewer",
    NS_DOCUMENT_VIEWER_CID,
    nsnull,
    CreateDocumentViewer },

  { "HTML Style Sheet",
    NS_HTMLSTYLESHEET_CID,
    nsnull,
    CreateHTMLStyleSheet },

  { "Style Set",
    NS_STYLESET_CID,
    nsnull,
    CreateStyleSet },

  { "CSS Style Sheet",
    NS_CSS_STYLESHEET_CID,
    nsnull,
    CreateCSSStyleSheet },

  { "HTML document",
    NS_HTMLDOCUMENT_CID,
    nsnull,
    CreateHTMLDocument },

  { "HTML-CSS style sheet",
    NS_HTML_CSS_STYLESHEET_CID,
    nsnull,
    CreateHTMLCSSStyleSheet },

  { "DOM implementation",
    NS_DOM_IMPLEMENTATION_CID,
    nsnull,
    CreateDOMImplementation },


  { "XML document",
    NS_XMLDOCUMENT_CID,
    nsnull,
    CreateXMLDocument },

  { "Image document",
    NS_IMAGEDOCUMENT_CID,
    nsnull,
    CreateImageDocument },

  { "CSS parser",
    NS_CSSPARSER_CID,
    nsnull,
    CreateCSSParser },

  { "CSS loader",
    NS_CSS_LOADER_CID,
    nsnull,
    CreateCSSLoader },

  { "HTML element factory",
    NS_HTML_ELEMENT_FACTORY_CID,
    NS_HTML_ELEMENT_FACTORY_CONTRACTID,
    CreateHTMLElementFactory },

  { "Text element",
    NS_TEXTNODE_CID,
    nsnull,
    CreateTextNode },

#if 0 // XXX apparently there is no such thing?
  { "Anonymous Content",
    NS_ANONYMOUSCONTENT_CID,
    nsnull,
    CreateAnonymousElement },
#endif

  { "Attribute Content",
    NS_ATTRIBUTECONTENT_CID,
    nsnull,
    CreateAttributeContent },

  { "XML element factory",
    NS_XML_ELEMENT_FACTORY_CID,
    NS_XML_ELEMENT_FACTORY_CONTRACTID,
    CreateXMLElementFactory },

#if 0 // XXX apparently there is no such thing?
  { "Selection",
    NS_SELECTION_CID,
    nsnull,
    CreateSelection },
#endif

  { "Dom selection",
    NS_DOMSELECTION_CID,
    nsnull,
    CreateDOMSelection },

  { "Frame selection",
    NS_FRAMESELECTION_CID,
    nsnull,
    CreateSelection },

  { "Range",
    NS_RANGE_CID,
    nsnull,
    CreateRange },

  { "Range Utils",
    NS_RANGEUTILS_CID,
    nsnull,
    CreateRangeUtils },

  { "Content iterator",
    NS_CONTENTITERATOR_CID,
    nsnull,
    CreateContentIterator },

  { "Generated Content iterator",
    NS_GENERATEDCONTENTITERATOR_CID,
    nsnull,
    CreateGeneratedContentIterator },

  { "Generated Subtree iterator",
    NS_GENERATEDSUBTREEITERATOR_CID,
    nsnull,
    CreateGeneratedSubtreeIterator },

  { "Subtree iterator",
    NS_SUBTREEITERATOR_CID,
    nsnull,
    CreateSubtreeIterator },

  { "CSS Object Model Factory",
    NS_CSSOMFACTORY_CID,
    nsnull,
    nsCSSOMFactoryConstructor },

  // Needed to support "new Option;" and "new Image;" in JavaScript
  { "HTML img element",
    NS_HTMLIMAGEELEMENT_CID,
    NS_HTMLIMGELEMENT_CONTRACTID,
    CreateHTMLImgElement,
    RegisterHTMLImgElement, 
    UnregisterHTMLImgElement },

  { "HTML option element",
    NS_HTMLOPTIONELEMENT_CID,
    NS_HTMLOPTIONELEMENT_CONTRACTID,
    CreateHTMLOptionElement,
    RegisterHTMLOptionElement,
    UnregisterHTMLOptionElement },

  { "XML document encoder",
    NS_TEXT_ENCODER_CID,
    NS_DOC_ENCODER_CONTRACTID_BASE "text/xml",
    CreateTextEncoder },

  { "XML document encoder",
    NS_TEXT_ENCODER_CID,
    NS_DOC_ENCODER_CONTRACTID_BASE "application/xml",
    CreateTextEncoder },

  { "XML document encoder",
    NS_TEXT_ENCODER_CID,
    NS_DOC_ENCODER_CONTRACTID_BASE "application/xhtml+xml",
    CreateTextEncoder },

  { "HTML document encoder",
    NS_TEXT_ENCODER_CID,
    NS_DOC_ENCODER_CONTRACTID_BASE "text/html",
    CreateTextEncoder },

  { "Plaintext document encoder",
    NS_TEXT_ENCODER_CID,
    NS_DOC_ENCODER_CONTRACTID_BASE "text/plain",
    CreateTextEncoder },

  { "HTML copy encoder",
    NS_HTMLCOPY_TEXT_ENCODER_CID,
    NS_HTMLCOPY_ENCODER_CONTRACTID,
    CreateHTMLCopyTextEncoder },

  { "XML content serializer",
    NS_XMLCONTENTSERIALIZER_CID,
    NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "text/xml",
    CreateXMLContentSerializer },

  { "XML content serializer",
    NS_XMLCONTENTSERIALIZER_CID,
    NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "application/xml",
    CreateXMLContentSerializer },

  { "XML content serializer",
    NS_XMLCONTENTSERIALIZER_CID,
    NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "application/xhtml+xml",
    CreateXMLContentSerializer },

  { "HTML content serializer",
    NS_HTMLCONTENTSERIALIZER_CID,
    NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "text/html",
    CreateHTMLContentSerializer },

  { "XUL content serializer",
    NS_XMLCONTENTSERIALIZER_CID,
    NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "application/vnd.mozilla.xul+xml",
    CreateXMLContentSerializer },

  { "plaintext content serializer",
    NS_PLAINTEXTSERIALIZER_CID,
    NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "text/plain",
    CreatePlainTextSerializer },

  { "plaintext sink",
    NS_PLAINTEXTSERIALIZER_CID,
    NS_PLAINTEXTSINK_CONTRACTID,
    CreatePlainTextSerializer },

  { "XBL Service",
    NS_XBLSERVICE_CID,
    "@mozilla.org/xbl;1",
    CreateXBLService },

  { "XBL Binding Manager",
    NS_BINDINGMANAGER_CID,
    "@mozilla.org/xbl/binding-manager;1",
    CreateBindingManager },

  { "Content policy service",
    NS_CONTENTPOLICY_CID,
    NS_CONTENTPOLICY_CONTRACTID,
    CreateContentPolicy },

  { "NodeInfoManager",
    NS_NODEINFOMANAGER_CID,
    NS_NODEINFOMANAGER_CONTRACTID,
    CreateNodeInfoManager },

  { "DOM CSS Computed Style Declaration",
    NS_COMPUTEDDOMSTYLE_CID,
    "@mozilla.org/DOM/Level2/CSS/computedStyleDeclaration;1",
    CreateComputedDOMStyle },

#ifdef MOZ_XUL
  { "XUL Sort Service",
    NS_XULSORTSERVICE_CID,
    "@mozilla.org/xul/xul-sort-service;1",
    CreateXULSortService },

  { "XUL Template Builder",
    NS_XULTEMPLATEBUILDER_CID,
    "@mozilla.org/xul/xul-template-builder;1",
    NS_NewXULContentBuilder },

  { "XUL Outliner Builder",
    NS_XULOUTLINERBUILDER_CID,
    "@mozilla.org/xul/xul-outliner-builder;1",
    NS_NewXULOutlinerBuilder },

  { "XUL Content Sink",
    NS_XULCONTENTSINK_CID,
    "@mozilla.org/xul/xul-content-sink;1",
    CreateXULContentSink },

  { "XUL Document",
    NS_XULDOCUMENT_CID,
    "@mozilla.org/xul/xul-document;1",
    CreateXULDocument },

  { "XUL PopupListener",
    NS_XULPOPUPLISTENER_CID,
    "@mozilla.org/xul/xul-popup-listener;1",
    CreateXULPopupListener },

  { "XUL Controllers",
    NS_XULCONTROLLERS_CID,
    "@mozilla.org/xul/xul-controllers;1",
    NS_NewXULControllers },

  { "XUL Prototype Cache",
    NS_XULPROTOTYPECACHE_CID,
    "@mozilla.org/xul/xul-prototype-cache;1",
    NS_NewXULPrototypeCache },

  { NS_XULPROTOTYPEDOCUMENT_CLASSNAME,
    NS_XULPROTOTYPEDOCUMENT_CID,
    nsnull,
    NS_NewXULPrototypeDocument },

  { "XUL Element Factory",
    NS_XULELEMENTFACTORY_CID,
    NS_ELEMENT_FACTORY_CONTRACTID_PREFIX "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
    CreateXULElementFactory },
#endif

  { "Controller Command Manager",
    NS_CONTROLLERCOMMANDMANAGER_CID,
    "@mozilla.org/content/controller-command-manager;1",
    CreateControllerCommandManager },

  { "Content HTTP Startup Listener",
    NS_CONTENTHTTPSTARTUP_CID,
    NS_CONTENTHTTPSTARTUP_CONTRACTID,
    nsContentHTTPStartupConstructor,
    nsContentHTTPStartup::RegisterHTTPStartup,
    nsContentHTTPStartup::UnregisterHTTPStartup },

  { "Document Loader Factory",
    NS_CONTENT_DOCUMENT_LOADER_FACTORY_CID,
    "@mozilla.org:/content/document-loader-factory;1",
    CreateContentDLF,
    nsContentDLF::RegisterDocumentFactories,
    nsContentDLF::UnregisterDocumentFactories },
};

NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(nsContentModule, gComponents, Initialize, Shutdown)
