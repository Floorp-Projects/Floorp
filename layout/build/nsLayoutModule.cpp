/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsXBLAtoms.h"                 // to addref/release table
#include "nsCSSPseudoElements.h"        // to addref/release table
#include "nsCSSPseudoClasses.h"         // to addref/release table
#include "nsCSSAnonBoxes.h"             // to addref/release table
#include "nsCSSKeywords.h"              // to addref/release table
#include "nsCSSProps.h"                 // to addref/release table
#include "nsColorNames.h"               // to addref/release table
#include "nsContentCID.h"
#include "nsContentHTTPStartup.h"
#include "nsContentDLF.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsLayoutStylesheetCache.h"
#include "nsDOMCID.h"
#include "nsCSSOMFactory.h"
#include "nsInspectorCSSUtils.h"
#include "nsEventListenerManager.h"
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
#include "nsIControllers.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMEventGroup.h"
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
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLFragmentContentSink.h"
#include "nsHTMLStyleSheet.h"
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
#include "nsIFrameLoader.h"
#include "nsICaret.h"
#include "nsLayoutAtoms.h"
#include "nsPlainTextSerializer.h"
#include "mozSanitizingSerializer.h"
#include "nsRange.h"
#include "nsComputedDOMStyle.h"
#include "nsXMLContentSerializer.h"
#include "nsRuleNode.h"
#include "nsWyciwygProtocolHandler.h"
#include "nsContentAreaDragDrop.h"
#include "nsContentList.h"
#include "nsISyncLoadDOMService.h"
#include "nsCSSFrameConstructor.h"
#include "nsRepeatService.h"
#include "nsSprocketLayout.h"
#include "nsStackLayout.h"
#include "nsBox.h"
#include "nsSpaceManager.h"
#include "nsTextControlFrame.h"
#include "nsTextTransformer.h"
#include "nsIFrameTraversal.h"
#include "nsISelectionImageService.h"
#include "nsIPrintContext.h"
#include "nsIAutoCopy.h"
#include "nsIPrintPreviewContext.h"
#include "nsCSSLoader.h"
#include "nsXULAtoms.h"
#include "nsLayoutCID.h"
#include "nsImageLoadingContent.h"
#include "nsStyleSet.h"
#include "nsImageFrame.h"

// view stuff
#include "nsViewsCID.h"
#include "nsView.h"
#include "nsScrollPortView.h"
#include "nsViewManager.h"

// DOM includes
#include "nsDOMException.h"
#include "nsGlobalWindowCommands.h"
#include "nsIControllerCommandTable.h"
#include "nsJSProtocolHandler.h"
#include "nsGlobalWindow.h"
#include "nsDOMClassInfo.h"
#include "nsScriptNameSpaceManager.h"
#include "nsIControllerContext.h"
#include "nsDOMScriptObjectFactory.h"

class nsIDocumentLoaderFactory;

#define PRODUCT_NAME "Gecko"

#define NS_HTMLIMGELEMENT_CONTRACTID \
  "@mozilla.org/content/element/html;1?name=img"

#define NS_HTMLOPTIONELEMENT_CONTRACTID \
  "@mozilla.org/content/element/html;1?name=option"

/* 0ddf4df8-4dbb-4133-8b79-9afb966514f5 */
#define NS_PLUGINDOCLOADERFACTORY_CID \
{ 0x0ddf4df8, 0x4dbb, 0x4133, { 0x8b, 0x79, 0x9a, 0xfb, 0x96, 0x65, 0x14, 0xf5 } }

#define NS_WINDOWCOMMANDTABLE_CID \
 { /* 0DE2FBFA-6B7F-11D7-BBBA-0003938A9D96 */        \
  0x0DE2FBFA, 0x6B7F, 0x11D7, {0xBB, 0xBA, 0x00, 0x03, 0x93, 0x8A, 0x9D, 0x96} }

static NS_DEFINE_CID(kWindowCommandTableCID, NS_WINDOWCOMMANDTABLE_CID);

#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#include "nsIXULPopupListener.h"
#include "nsIXULPrototypeCache.h"
#include "nsIXULPrototypeDocument.h"
#include "nsIXULPrototypeDocument.h"
#include "nsIXULSortService.h"
#include "nsXULContentUtils.h"
#include "nsXULElement.h"

NS_IMETHODIMP
NS_NewXULContentBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult);

NS_IMETHODIMP
NS_NewXULTreeBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult);
#endif

PR_STATIC_CALLBACK(nsresult) Initialize(nsIModule* aSelf);
static void Shutdown();

#ifdef MOZ_MATHML
#include "nsMathMLAtoms.h"
#include "nsMathMLOperators.h"
#endif

#ifdef MOZ_SVG
#include "nsSVGAtoms.h"
#include "nsSVGTypeCIDs.h"
#include "nsISVGRenderer.h"
#include "nsSVGRect.h"
#ifdef MOZ_SVG_RENDERER_LIBART
void NS_InitSVGRendererLibartGlobals();
void NS_FreeSVGRendererLibartGlobals();
#endif
#ifdef MOZ_SVG_RENDERER_GDIPLUS
void NS_InitSVGRendererGDIPlusGlobals();
void NS_FreeSVGRendererGDIPlusGlobals();
#endif
#ifdef MOZ_SVG_RENDERER_CAIRO
void NS_InitSVGRendererCairoGlobals();
void NS_FreeSVGRendererCairoGlobals();
#endif
#endif

//-----------------------------------------------------------------------------

// Per bug 209804, it is necessary to observe the "xpcom-shutdown" event and
// perform shutdown of the layout modules at that time instead of waiting for
// our module destructor to run.  If we do not do this, then we risk holding
// references to objects in other component libraries that have already been
// shutdown (and possibly unloaded if 60709 is ever fixed).

class LayoutShutdownObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(LayoutShutdownObserver, nsIObserver)

NS_IMETHODIMP
LayoutShutdownObserver::Observe(nsISupports *aSubject,
                                const char *aTopic,
                                const PRUnichar *someData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
    Shutdown();
  return NS_OK;
}

//-----------------------------------------------------------------------------

static PRBool gInitialized = PR_FALSE;

// Perform our one-time intialization for this module

// static
nsresult PR_CALLBACK
Initialize(nsIModule* aSelf)
{
  NS_PRECONDITION(!gInitialized, "module already initialized");
  if (gInitialized) {
    return NS_OK;
  }

  gInitialized = PR_TRUE;
    
  nsresult rv = nsContentUtils::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsContentUtils");

    Shutdown();

    return rv;
  }

  // Register all of our atoms once
  nsCSSAnonBoxes::AddRefAtoms();
  nsCSSPseudoClasses::AddRefAtoms();
  nsCSSPseudoElements::AddRefAtoms();
  nsCSSKeywords::AddRefTable();
  nsCSSProps::AddRefTable();
  nsColorNames::AddRefTable();
  nsHTMLAtoms::AddRefAtoms();
  nsXBLAtoms::AddRefAtoms();
  nsLayoutAtoms::AddRefAtoms();
  nsXULAtoms::AddRefAtoms();

#ifdef MOZ_XUL
  rv = nsXULContentUtils::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsXULContentUtils");

    Shutdown();

    return rv;
  }
#endif

#ifdef MOZ_MATHML
  nsMathMLOperators::AddRefTable();
  nsMathMLAtoms::AddRefAtoms();
#endif

#ifdef MOZ_SVG
  nsSVGAtoms::AddRefAtoms();
#ifdef MOZ_SVG_RENDERER_LIBART
  NS_InitSVGRendererLibartGlobals();
#endif
#ifdef MOZ_SVG_RENDERER_GDIPLUS
  NS_InitSVGRendererGDIPlusGlobals();
#endif
#ifdef MOZ_SVG_RENDERER_CAIRO
  NS_InitSVGRendererCairoGlobals();
#endif
#endif

#ifdef DEBUG
  nsFrame::DisplayReflowStartup();
#endif
  rv = nsTextTransformer::Initialize();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsTextTransformer");

    Shutdown();

    return rv;
  }

  nsImageLoadingContent::Initialize();

  // Add our shutdown observer.
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");

  if (observerService) {
    LayoutShutdownObserver* observer =
      new LayoutShutdownObserver();

    if (!observer) {
      Shutdown();

      return NS_ERROR_OUT_OF_MEMORY;
    }

    observerService->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
  } else {
    NS_WARNING("Could not get an observer service.  We will leak on shutdown.");
  }

  return NS_OK;
}

// Shutdown this module, releasing all of the module resources

// static
void
Shutdown()
{
  NS_PRECONDITION(gInitialized, "module not initialized");
  if (!gInitialized)
    return;

  gInitialized = PR_FALSE;

  nsRange::Shutdown();
  nsGenericElement::Shutdown();
  nsEventListenerManager::Shutdown();
  nsContentList::Shutdown();
  nsComputedDOMStyle::Shutdown();
  CSSLoaderImpl::Shutdown();
#ifdef DEBUG
  nsFrame::DisplayReflowShutdown();
#endif

  // Release all of our atoms
  nsColorNames::ReleaseTable();
  nsCSSProps::ReleaseTable();
  nsCSSKeywords::ReleaseTable();
  nsRepeatService::Shutdown();
  nsStackLayout::Shutdown();
  nsBox::Shutdown();

#ifdef MOZ_XUL
  nsXULContentUtils::Finish();
  nsXULElement::ReleaseGlobals();
  nsXULPrototypeElement::ReleaseGlobals();
  nsXULPrototypeScript::ReleaseGlobals();
  nsSprocketLayout::Shutdown();
#endif

#ifdef MOZ_MATHML
  nsMathMLOperators::ReleaseTable();
#endif

#ifdef MOZ_SVG
#ifdef MOZ_SVG_RENDERER_LIBART
  NS_FreeSVGRendererLibartGlobals();
#endif
#ifdef MOZ_SVG_RENDERER_GDIPLUS
  NS_FreeSVGRendererGDIPlusGlobals();
#endif
#ifdef MOZ_SVG_RENDERER_CAIRO
  NS_FreeSVGRendererCairoGlobals();
#endif
#endif

  nsCSSFrameConstructor::ReleaseGlobals();
  nsTextTransformer::Shutdown();
  nsSpaceManager::Shutdown();
  nsTextControlFrame::ReleaseGlobals();
  nsImageFrame::ReleaseGlobals();

  NS_IF_RELEASE(nsContentDLF::gUAStyleSheet);
  NS_IF_RELEASE(nsRuleNode::gLangService);
  nsGenericHTMLElement::Shutdown();

  nsContentUtils::Shutdown();
  nsLayoutStylesheetCache::Shutdown();
  NS_NameSpaceManagerShutdown();
  nsImageLoadingContent::Shutdown();
  nsStyleSet::FreeGlobals();

  GlobalWindowImpl::ShutDown();
  nsDOMClassInfo::ShutDown();
}

#ifdef NS_DEBUG
nsresult NS_NewFrameUtil(nsIFrameUtil** aResult);
nsresult NS_NewLayoutDebugger(nsILayoutDebugger** aResult);
#endif

#ifdef MOZ_XUL
nsresult NS_NewBoxObject(nsIBoxObject** aResult);
nsresult NS_NewListBoxObject(nsIBoxObject** aResult);
nsresult NS_NewScrollBoxObject(nsIBoxObject** aResult);
nsresult NS_NewMenuBoxObject(nsIBoxObject** aResult);
nsresult NS_NewEditorBoxObject(nsIBoxObject** aResult);
nsresult NS_NewPopupBoxObject(nsIBoxObject** aResult);
nsresult NS_NewBrowserBoxObject(nsIBoxObject** aResult);
nsresult NS_NewIFrameBoxObject(nsIBoxObject** aResult);
nsresult NS_NewTreeBoxObject(nsIBoxObject** aResult);
nsresult NS_NewXULElementFactory(nsIElementFactory** aResult);
#endif

nsresult NS_CreateFrameTraversal(nsIFrameTraversal** aResult);
nsresult NS_NewLayoutHistoryState(nsILayoutHistoryState** aResult);
nsresult NS_NewAutoCopyService(nsIAutoCopyService** aResult);
nsresult NS_NewSelectionImageService(nsISelectionImageService** aResult);

nsresult NS_NewSelection(nsIFrameSelection** aResult);
nsresult NS_NewDomSelection(nsISelection** aResult);
nsresult NS_NewDocumentViewer(nsIDocumentViewer** aResult);
nsresult NS_NewRange(nsIDOMRange** aResult);
nsresult NS_NewRangeUtils(nsIRangeUtils** aResult);
nsresult NS_NewContentIterator(nsIContentIterator** aResult);
nsresult NS_NewPreContentIterator(nsIContentIterator** aResult);
nsresult NS_NewGenRegularIterator(nsIContentIterator** aResult);
nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aResult);
nsresult NS_NewGenSubtreeIterator(nsIContentIterator** aInstancePtrResult);
nsresult NS_NewContentDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult);
nsresult NS_NewHTMLElementFactory(nsIElementFactory** aResult);
nsresult NS_NewXMLElementFactory(nsIElementFactory** aResult);
nsresult NS_NewHTMLCopyTextEncoder(nsIDocumentEncoder** aResult);
nsresult NS_NewTextEncoder(nsIDocumentEncoder** aResult);
nsresult NS_NewXBLService(nsIXBLService** aResult);
nsresult NS_NewBindingManager(nsIBindingManager** aResult);
nsresult NS_NewNodeInfoManager(nsINodeInfoManager** aResult);
nsresult NS_NewContentPolicy(nsIContentPolicy** aResult);
nsresult NS_NewFrameLoader(nsIFrameLoader** aResult);
nsresult NS_NewSyncLoadDOMService(nsISyncLoadDOMService** aResult);
nsresult NS_NewDOMEventGroup(nsIDOMEventGroup** aResult);

NS_IMETHODIMP NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult);

#ifdef MOZ_MATHML
nsresult NS_NewMathMLElementFactory(nsIElementFactory** aResult);
#endif

#ifdef MOZ_SVG
#ifdef MOZ_SVG_RENDERER_GDIPLUS
nsresult NS_NewSVGRendererGDIPlus(nsISVGRenderer** aResult);
#endif // MOZ_SVG_RENDERER_GDIPLUS
#ifdef MOZ_SVG_RENDERER_LIBART
nsresult NS_NewSVGRendererLibart(nsISVGRenderer** aResult);
#endif // MOZ_SVG_RENDERER_LIBART
#ifdef MOZ_SVG_RENDERER_CAIRO
nsresult NS_NewSVGRendererCairo(nsISVGRenderer** aResult);
#endif // MOZ_SVG_RENDERER_CAIRO

nsresult NS_NewSVGElementFactory(nsIElementFactory** aResult);
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

#ifdef DEBUG
MAKE_CTOR(CreateNewFrameUtil,             nsIFrameUtil,                NS_NewFrameUtil)
MAKE_CTOR(CreateNewLayoutDebugger,        nsILayoutDebugger,           NS_NewLayoutDebugger)
#endif

MAKE_CTOR(CreateNewFrameTraversal,      nsIFrameTraversal,      NS_CreateFrameTraversal)
MAKE_CTOR(CreateNewLayoutHistoryState,  nsILayoutHistoryState,  NS_NewLayoutHistoryState)
MAKE_CTOR(CreateNewPresShell,           nsIPresShell,           NS_NewPresShell)
MAKE_CTOR(CreateNewPresState,           nsIPresState,           NS_NewPresState)
MAKE_CTOR(CreateNewGalleyContext,       nsIPresContext,         NS_NewGalleyContext)
MAKE_CTOR(CreateNewPrintContext,        nsIPrintContext,        NS_NewPrintContext)
MAKE_CTOR(CreateNewPrintPreviewContext, nsIPrintPreviewContext, NS_NewPrintPreviewContext)
#ifdef MOZ_XUL
MAKE_CTOR(CreateNewBoxObject,           nsIBoxObject,           NS_NewBoxObject)
MAKE_CTOR(CreateNewListBoxObject,       nsIBoxObject,           NS_NewListBoxObject)
MAKE_CTOR(CreateNewMenuBoxObject,       nsIBoxObject,           NS_NewMenuBoxObject)
MAKE_CTOR(CreateNewPopupBoxObject,      nsIBoxObject,           NS_NewPopupBoxObject)
MAKE_CTOR(CreateNewBrowserBoxObject,    nsIBoxObject,           NS_NewBrowserBoxObject)
MAKE_CTOR(CreateNewEditorBoxObject,     nsIBoxObject,           NS_NewEditorBoxObject)
MAKE_CTOR(CreateNewIFrameBoxObject,     nsIBoxObject,           NS_NewIFrameBoxObject)
MAKE_CTOR(CreateNewScrollBoxObject,     nsIBoxObject,           NS_NewScrollBoxObject)
MAKE_CTOR(CreateNewTreeBoxObject,       nsIBoxObject,           NS_NewTreeBoxObject)
#endif
MAKE_CTOR(CreateNewAutoCopyService,     nsIAutoCopyService,     NS_NewAutoCopyService)
MAKE_CTOR(CreateSelectionImageService,  nsISelectionImageService,NS_NewSelectionImageService)
#ifdef MOZ_SVG
#ifdef MOZ_SVG_RENDERER_GDIPLUS
MAKE_CTOR(CreateNewSVGRendererGDIPlus,  nsISVGRenderer,         NS_NewSVGRendererGDIPlus)
#endif // MOZ_SVG_RENDERER_GDIPLUS
#ifdef MOZ_SVG_RENDERER_LIBART
MAKE_CTOR(CreateNewSVGRendererLibart,   nsISVGRenderer,         NS_NewSVGRendererLibart)
#endif // MOZ_SVG_RENDERER_LIBART
#ifdef MOZ_SVG_RENDERER_CAIRO
MAKE_CTOR(CreateNewSVGRendererCairo,    nsISVGRenderer,         NS_NewSVGRendererCairo)
#endif // MOZ_SVG_RENDERER_CAIRO
#endif
MAKE_CTOR(CreateCaret,                  nsICaret,               NS_NewCaret)

MAKE_CTOR(CreateNameSpaceManager,         nsINameSpaceManager,         NS_GetNameSpaceManager)
MAKE_CTOR(CreateEventListenerManager,     nsIEventListenerManager,     NS_NewEventListenerManager)
MAKE_CTOR(CreateDOMEventGroup,            nsIDOMEventGroup,            NS_NewDOMEventGroup)
MAKE_CTOR(CreateDocumentViewer,           nsIDocumentViewer,           NS_NewDocumentViewer)
MAKE_CTOR(CreateCSSStyleSheet,            nsICSSStyleSheet,            NS_NewCSSStyleSheet)
MAKE_CTOR(CreateHTMLDocument,             nsIDocument,                 NS_NewHTMLDocument)
MAKE_CTOR(CreateHTMLCSSStyleSheet,        nsIHTMLCSSStyleSheet,        NS_NewHTMLCSSStyleSheet)
MAKE_CTOR(CreateDOMImplementation,        nsIDOMDOMImplementation,     NS_NewDOMImplementation)
MAKE_CTOR(CreateXMLDocument,              nsIDocument,                 NS_NewXMLDocument)
#ifdef MOZ_SVG
MAKE_CTOR(CreateSVGDocument,              nsIDocument,                 NS_NewSVGDocument)
#endif
MAKE_CTOR(CreateImageDocument,            nsIDocument,                 NS_NewImageDocument)
MAKE_CTOR(CreateCSSParser,                nsICSSParser,                NS_NewCSSParser)
MAKE_CTOR(CreateCSSLoader,                nsICSSLoader,                NS_NewCSSLoader)
MAKE_CTOR(CreateHTMLElementFactory,       nsIElementFactory,           NS_NewHTMLElementFactory)
MAKE_CTOR(CreateTextNode,                 nsITextContent,              NS_NewTextNode)
//MAKE_CTOR(CreateAnonymousElement,         nsIContent,                  NS_NewAnonymousElement)
MAKE_CTOR(CreateXMLElementFactory,        nsIElementFactory,           NS_NewXMLElementFactory)
//MAKE_CTOR(CreateSelection,                nsISelection,                NS_NewSelection)
MAKE_CTOR(CreateDOMSelection,             nsISelection,                NS_NewDomSelection)
MAKE_CTOR(CreateSelection,                nsIFrameSelection,           NS_NewSelection)
MAKE_CTOR(CreateRange,                    nsIDOMRange,                 NS_NewRange)
MAKE_CTOR(CreateRangeUtils,               nsIRangeUtils,               NS_NewRangeUtils)
MAKE_CTOR(CreateContentIterator,          nsIContentIterator,          NS_NewContentIterator)
MAKE_CTOR(CreatePreContentIterator,       nsIContentIterator,          NS_NewPreContentIterator)
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
MAKE_CTOR(CreateHTMLFragmentSink,         nsIHTMLFragmentContentSink,  NS_NewHTMLFragmentContentSink)
MAKE_CTOR(CreateHTMLFragmentSink2,        nsIHTMLFragmentContentSink,  NS_NewHTMLFragmentContentSink2)
MAKE_CTOR(CreateSanitizingHTMLSerializer, nsIContentSerializer,        NS_NewSanitizingHTMLSerializer)
MAKE_CTOR(CreateXBLService,               nsIXBLService,               NS_NewXBLService)
MAKE_CTOR(CreateBindingManager,           nsIBindingManager,           NS_NewBindingManager)
MAKE_CTOR(CreateContentPolicy,            nsIContentPolicy,            NS_NewContentPolicy)
MAKE_CTOR(CreateFrameLoader,              nsIFrameLoader,              NS_NewFrameLoader)
MAKE_CTOR(CreateNodeInfoManager,          nsINodeInfoManager,          NS_NewNodeInfoManager)
MAKE_CTOR(CreateComputedDOMStyle,         nsIComputedDOMStyle,         NS_NewComputedDOMStyle)
#ifdef MOZ_XUL
MAKE_CTOR(CreateXULSortService,           nsIXULSortService,           NS_NewXULSortService)
// NS_NewXULContentBuilder
// NS_NewXULTreeBuilder
MAKE_CTOR(CreateXULDocument,              nsIXULDocument,              NS_NewXULDocument)
MAKE_CTOR(CreateXULPopupListener,         nsIXULPopupListener,         NS_NewXULPopupListener)
// NS_NewXULControllers
// NS_NewXULPrototypeCache
MAKE_CTOR(CreateXULElementFactory,        nsIElementFactory,           NS_NewXULElementFactory)
#endif
#ifdef MOZ_MATHML
MAKE_CTOR(CreateMathMLElementFactory,     nsIElementFactory,           NS_NewMathMLElementFactory)
#endif
#ifdef MOZ_SVG
MAKE_CTOR(CreateSVGElementFactory,        nsIElementFactory,           NS_NewSVGElementFactory)
MAKE_CTOR(CreateSVGRect,                  nsIDOMSVGRect,               NS_NewSVGRect)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsContentHTTPStartup)
MAKE_CTOR(CreateContentDLF,               nsIDocumentLoaderFactory,    NS_NewContentDocumentLoaderFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCSSOMFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsInspectorCSSUtils)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWyciwygProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsContentAreaDragDrop)
MAKE_CTOR(CreateSyncLoadDOMService,       nsISyncLoadDOMService,       NS_NewSyncLoadDOMService)
MAKE_CTOR(CreatePluginDocument,           nsIDocument,                 NS_NewPluginDocument)

// views are not refcounted, so this is the same as
// NS_GENERIC_FACTORY_CONSTRUCTOR without the NS_ADDREF/NS_RELEASE
#define NS_GENERIC_FACTORY_CONSTRUCTOR_NOREFS(_InstanceClass)                 \
static NS_IMETHODIMP                                                          \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,               \
                            void **aResult)                                   \
{                                                                             \
    nsresult rv;                                                              \
                                                                              \
    _InstanceClass * inst;                                                    \
                                                                              \
    *aResult = NULL;                                                          \
    if (NULL != aOuter) {                                                     \
        rv = NS_ERROR_NO_AGGREGATION;                                         \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    NS_NEWXPCOM(inst, _InstanceClass);                                        \
    if (NULL == inst) {                                                       \
        rv = NS_ERROR_OUT_OF_MEMORY;                                          \
        return rv;                                                            \
    }                                                                         \
    rv = inst->QueryInterface(aIID, aResult);                                 \
                                                                              \
    return rv;                                                                \
}                                                                             \

NS_GENERIC_FACTORY_CONSTRUCTOR(nsViewManager)
NS_GENERIC_FACTORY_CONSTRUCTOR_NOREFS(nsView)
NS_GENERIC_FACTORY_CONSTRUCTOR_NOREFS(nsScrollPortView)

static NS_IMETHODIMP
CreateHTMLImgElement(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  *aResult = nsnull;
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  // Note! NS_NewHTMLImageElement is special cased to handle a null nodeinfo
  nsIHTMLContent* inst = NS_NewHTMLImageElement(nsnull);
  nsresult rv = NS_ERROR_OUT_OF_MEMORY;
  if (inst) {
    NS_ADDREF(inst);
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

  if (!catman)
    return NS_ERROR_FAILURE;

  nsXPIDLCString previous;
  nsresult rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                         "Image", NS_HTMLIMGELEMENT_CONTRACTID,
                                         PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  return catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_PROTO_ALIAS_CATEGORY,
                                  "Image", "HTMLImageElement",
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
  // Note! NS_NewHTMLOptionElement is special cased to handle a null nodeinfo
  nsIHTMLContent* inst = NS_NewHTMLOptionElement(nsnull);
  nsresult rv = NS_ERROR_OUT_OF_MEMORY;
  if (inst) {
    NS_ADDREF(inst);
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

  if (!catman)
    return NS_ERROR_FAILURE;

  nsXPIDLCString previous;
  nsresult rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                         "Option", NS_HTMLOPTIONELEMENT_CONTRACTID,
                                         PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  return catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_PROTO_ALIAS_CATEGORY,
                                  "Option", "HTMLOptionElement",
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

static NS_METHOD
CreateWindowCommandTableConstructor(nsISupports *aOuter,
                                    REFNSIID aIID, void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIControllerCommandTable> commandTable =
      do_CreateInstance(NS_CONTROLLERCOMMANDTABLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = nsWindowCommandRegistration::RegisterWindowCommands(commandTable);
  if (NS_FAILED(rv)) return rv;

  return commandTable->QueryInterface(aIID, aResult);
}

static NS_METHOD
CreateWindowControllerWithSingletonCommandTable(nsISupports *aOuter,
                                      REFNSIID aIID, void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIController> controller =
       do_CreateInstance("@mozilla.org/embedcomp/base-command-controller;1", &rv);

 if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIControllerCommandTable> windowCommandTable = do_GetService(kWindowCommandTableCID, &rv);
  if (NS_FAILED(rv)) return rv;

  // this is a singleton; make it immutable
  windowCommandTable->MakeImmutable();

  nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller, &rv);
  if (NS_FAILED(rv)) return rv;

  controllerContext->Init(windowCommandTable);
  if (NS_FAILED(rv)) return rv;

  return controller->QueryInterface(aIID, aResult);
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMScriptObjectFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBaseDOMException)

// The list of components we register
static const nsModuleComponentInfo gComponents[] = {
#ifdef MOZ_SVG
#ifdef MOZ_SVG_RENDERER_GDIPLUS
  { "SVG GdiPlus Renderer",
    NS_SVG_RENDERER_GDIPLUS_CID,
    NS_SVG_RENDERER_GDIPLUS_CONTRACTID,
    CreateNewSVGRendererGDIPlus },
#endif // MOZ_SVG_RENDERER_GDIPLUS
#ifdef MOZ_SVG_RENDERER_LIBART
  { "SVG Libart Renderer",
    NS_SVG_RENDERER_LIBART_CID,
    NS_SVG_RENDERER_LIBART_CONTRACTID,
    CreateNewSVGRendererLibart },
#endif // MOZ_SVG_RENDERER_LIBART
#ifdef MOZ_SVG_RENDERER_CAIRO
  { "SVG Cairo Renderer",
    NS_SVG_RENDERER_CAIRO_CID,
    NS_SVG_RENDERER_CAIRO_CONTRACTID,
    CreateNewSVGRendererCairo },
#endif // MOZ_SVG_RENDERER_CAIRO
#endif // MOZ_SVG

#ifdef DEBUG
  { "Frame utility",
    NS_FRAME_UTIL_CID,
    nsnull,
    CreateNewFrameUtil },
  { "Layout debugger",
    NS_LAYOUT_DEBUGGER_CID,
    nsnull,
    CreateNewLayoutDebugger },
#endif

  { "Frame Traversal",
    NS_FRAMETRAVERSAL_CID,
    nsnull,
    CreateNewFrameTraversal },

  { "Layout History State",
    NS_LAYOUT_HISTORY_STATE_CID,
    nsnull,
    CreateNewLayoutHistoryState },

  { "selection image storage",
    NS_SELECTIONIMAGESERVICE_CID,
    nsnull,
    CreateSelectionImageService },

  { "caret",
    NS_CARET_CID,
    "@mozilla.org/layout/caret;1",
    CreateCaret },

  // XXX ick
  { "Presentation shell",
    NS_PRESSHELL_CID,
    nsnull,
    CreateNewPresShell },

  { "Presentation state",
    NS_PRESSTATE_CID,
    nsnull,
    CreateNewPresState },

  { "Galley context",
    NS_GALLEYCONTEXT_CID,
    nsnull,
    CreateNewGalleyContext },

  { "Print context",
    NS_PRINTCONTEXT_CID,
    nsnull,
    CreateNewPrintContext },

  { "Print Preview context",
    NS_PRINT_PREVIEW_CONTEXT_CID,
    nsnull,
    CreateNewPrintPreviewContext },
  // XXX end ick

#ifdef MOZ_XUL
  { "XUL Box Object",
    NS_BOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject;1",
    CreateNewBoxObject },

  { "XUL Listbox Box Object",
    NS_LISTBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-listbox;1",
    CreateNewListBoxObject },

  { "XUL Menu Box Object",
    NS_MENUBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-menu;1",
    CreateNewMenuBoxObject },

  { "XUL Popup Box Object",
    NS_POPUPBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-popup;1",
    CreateNewPopupBoxObject },

  { "XUL Browser Box Object",
    NS_BROWSERBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-browser;1",
    CreateNewBrowserBoxObject },

  { "XUL Editor Box Object",
    NS_EDITORBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-editor;1",
    CreateNewEditorBoxObject },

  { "XUL Iframe Object",
    NS_IFRAMEBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-iframe;1",
    CreateNewIFrameBoxObject },

  { "XUL ScrollBox Object",
    NS_SCROLLBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-scrollbox;1",
    CreateNewScrollBoxObject },

  { "XUL Tree Box Object",
    NS_TREEBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-tree;1",
    CreateNewTreeBoxObject },
#endif

  { "AutoCopy Service",
    NS_AUTOCOPYSERVICE_CID,
    "@mozilla.org/autocopy;1",
    CreateNewAutoCopyService },

  { "Namespace manager",
    NS_NAMESPACEMANAGER_CID,
    NS_NAMESPACEMANAGER_CONTRACTID,
    CreateNameSpaceManager },

  { "Event listener manager",
    NS_EVENTLISTENERMANAGER_CID,
    nsnull,
    CreateEventListenerManager },

  { "DOM Event group",
    NS_DOMEVENTGROUP_CID,
    nsnull,
    CreateDOMEventGroup },

  { "Document Viewer",
    NS_DOCUMENT_VIEWER_CID,
    nsnull,
    CreateDocumentViewer },

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

#ifdef MOZ_SVG
  { "SVG document",
    NS_SVGDOCUMENT_CID,
    nsnull,
    CreateSVGDocument },
#endif

  { "Image document",
    NS_IMAGEDOCUMENT_CID,
    nsnull,
    CreateImageDocument },

  { "CSS parser",
    NS_CSSPARSER_CID,
    "@mozilla.org/content/css-parser;1",
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
    "@mozilla.org/content/dom-selection;1",
    CreateDOMSelection },

  { "Frame selection",
    NS_FRAMESELECTION_CID,
    nsnull,
    CreateSelection },

  { "Range",
    NS_RANGE_CID,
    "@mozilla.org/content/range;1",
    CreateRange },

  { "Range Utils",
    NS_RANGEUTILS_CID,
    "@mozilla.org/content/range-utils;1",
    CreateRangeUtils },

  { "Content iterator",
    NS_CONTENTITERATOR_CID,
    "@mozilla.org/content/post-content-iterator;1",
    CreateContentIterator },

  { "Pre Content iterator",
    NS_PRECONTENTITERATOR_CID,
    "@mozilla.org/content/pre-content-iterator;1",
    CreatePreContentIterator },

  { "Generated Content iterator",
    NS_GENERATEDCONTENTITERATOR_CID,
    "@mozilla.org/content/generated-content-iterator;1",
    CreateGeneratedContentIterator },

  { "Generated Subtree iterator",
    NS_GENERATEDSUBTREEITERATOR_CID,
    "@mozilla.org/content/generated-subtree-content-iterator;1",
    CreateGeneratedSubtreeIterator },

  { "Subtree iterator",
    NS_SUBTREEITERATOR_CID,
    "@mozilla.org/content/subtree-content-iterator;1",
    CreateSubtreeIterator },

  { "CSS Object Model Factory",
    NS_CSSOMFACTORY_CID,
    nsnull,
    nsCSSOMFactoryConstructor },

  { "Inspector CSS Utils",
    NS_INSPECTORCSSUTILS_CID,
    nsnull,
    nsInspectorCSSUtilsConstructor },

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

#ifdef MOZ_SVG
  { "SVG document encoder",
    NS_TEXT_ENCODER_CID,
    NS_DOC_ENCODER_CONTRACTID_BASE "image/svg+xml",
    CreateTextEncoder },
#endif

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

#ifdef MOZ_SVG
  { "SVG content serializer",
    NS_XMLCONTENTSERIALIZER_CID,
    NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "image/svg+xml",
    CreateXMLContentSerializer },
#endif

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

  { "html fragment sink",
    NS_HTMLFRAGMENTSINK_CID,
    NS_HTMLFRAGMENTSINK_CONTRACTID,
    CreateHTMLFragmentSink },

  { "html fragment sink 2",
    NS_HTMLFRAGMENTSINK2_CID,
    NS_HTMLFRAGMENTSINK2_CONTRACTID,
    CreateHTMLFragmentSink2 },

  { "HTML sanitizing content serializer",
    MOZ_SANITIZINGHTMLSERIALIZER_CID,
    MOZ_SANITIZINGHTMLSERIALIZER_CONTRACTID,
    CreateSanitizingHTMLSerializer },

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

  { "Frame Loader",
    NS_FRAMELOADER_CID,
    NS_FRAMELOADER_CONTRACTID,
    CreateFrameLoader },

  { "NodeInfoManager",
    NS_NODEINFOMANAGER_CID,
    NS_NODEINFOMANAGER_CONTRACTID,
    CreateNodeInfoManager },

  { "DOM CSS Computed Style Declaration",
    NS_COMPUTEDDOMSTYLE_CID,
    "@mozilla.org/DOM/Level2/CSS/computedStyleDeclaration;1",
    CreateComputedDOMStyle },

  { "XUL Controllers",
    NS_XULCONTROLLERS_CID,
    "@mozilla.org/xul/xul-controllers;1",
    NS_NewXULControllers },

#ifdef MOZ_XUL
  { "XUL Sort Service",
    NS_XULSORTSERVICE_CID,
    "@mozilla.org/xul/xul-sort-service;1",
    CreateXULSortService },

  { "XUL Template Builder",
    NS_XULTEMPLATEBUILDER_CID,
    "@mozilla.org/xul/xul-template-builder;1",
    NS_NewXULContentBuilder },

  { "XUL Tree Builder",
    NS_XULTREEBUILDER_CID,
    "@mozilla.org/xul/xul-tree-builder;1",
    NS_NewXULTreeBuilder },

  { "XUL Document",
    NS_XULDOCUMENT_CID,
    "@mozilla.org/xul/xul-document;1",
    CreateXULDocument },

  { "XUL PopupListener",
    NS_XULPOPUPLISTENER_CID,
    "@mozilla.org/xul/xul-popup-listener;1",
    CreateXULPopupListener },

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
#else
  { "XML Element Factory",
	NS_XULELEMENTFACTORY_CID,
    NS_ELEMENT_FACTORY_CONTRACTID_PREFIX "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
    CreateXMLElementFactory },
#endif

#ifdef MOZ_MATHML
  { "MathML Element Factory",
    NS_MATHMLELEMENTFACTORY_CID,
    NS_MATHML_ELEMENT_FACTORY_CONTRACTID,
    CreateMathMLElementFactory },
#endif

#ifdef MOZ_SVG
  { "SVG element factory",
    NS_SVGELEMENTFACTORY_CID,
    NS_SVG_ELEMENT_FACTORY_CONTRACTID,
    CreateSVGElementFactory },
  
  { "SVG Rect",
    NS_SVGRECT_CID,
    NS_SVGRECT_CONTRACTID,
    CreateSVGRect },
#endif

  { "Content HTTP Startup Listener",
    NS_CONTENTHTTPSTARTUP_CID,
    NS_CONTENTHTTPSTARTUP_CONTRACTID,
    nsContentHTTPStartupConstructor,
    nsContentHTTPStartup::RegisterHTTPStartup,
    nsContentHTTPStartup::UnregisterHTTPStartup },

  { "Document Loader Factory",
    NS_CONTENT_DOCUMENT_LOADER_FACTORY_CID,
    "@mozilla.org/content/document-loader-factory;1",
    CreateContentDLF,
    nsContentDLF::RegisterDocumentFactories,
    nsContentDLF::UnregisterDocumentFactories },

  { "Wyciwyg Handler",
    NS_WYCIWYGPROTOCOLHANDLER_CID,
    NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "wyciwyg",
    nsWyciwygProtocolHandlerConstructor },

  { "Content Area DragDrop",
    NS_CONTENTAREADRAGDROP_CID,
    NS_CONTENTAREADRAGDROP_CONTRACTID,
    nsContentAreaDragDropConstructor },

  { "SyncLoad DOM Service",
    NS_SYNCLOADDOMSERVICE_CID,
    NS_SYNCLOADDOMSERVICE_CONTRACTID,
    CreateSyncLoadDOMService },

  // DOM objects
  { "Script Object Factory",
    NS_DOM_SCRIPT_OBJECT_FACTORY_CID,
    nsnull,
    nsDOMScriptObjectFactoryConstructor
  },
  { "Base DOM Exception",
    NS_BASE_DOM_EXCEPTION_CID,
    nsnull,
    nsBaseDOMExceptionConstructor
  },
  { "JavaScript Protocol Handler",
    NS_JSPROTOCOLHANDLER_CID,
    NS_JSPROTOCOLHANDLER_CONTRACTID,
    nsJSProtocolHandler::Create },
  { "Window Command Table",
    NS_WINDOWCOMMANDTABLE_CID,
    "",
    CreateWindowCommandTableConstructor
  },
  { "Window Command Controller",
    NS_WINDOWCONTROLLER_CID,
    NS_WINDOWCONTROLLER_CONTRACTID,
    CreateWindowControllerWithSingletonCommandTable
  },

  // view stuff
  { "View Manager", NS_VIEW_MANAGER_CID, "@mozilla.org/view-manager;1",
    nsViewManagerConstructor },
  { "View", NS_VIEW_CID, "@mozilla.org/view;1", nsViewConstructor },
  { "Scroll Port View", NS_SCROLL_PORT_VIEW_CID,
    "@mozilla.org/scroll-port-view;1", nsScrollPortViewConstructor },

  { "Plugin Document Loader Factory",
    NS_PLUGINDOCLOADERFACTORY_CID,
    "@mozilla.org/content/plugin/document-loader-factory;1",
    CreateContentDLF },

  { "Plugin Document",
    NS_PLUGINDOCUMENT_CID,
    nsnull,
    CreatePluginDocument }
};

NS_IMPL_NSGETMODULE_WITH_CTOR(nsLayoutModule, gComponents, Initialize)
