/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "XPCModule.h"
#include "mozilla/ModuleUtils.h"
#include "nsLayoutStatics.h"
#include "nsContentCID.h"
#include "nsContentDLF.h"
#include "nsContentPolicyUtils.h"
#include "nsDataDocumentContentPolicy.h"
#include "nsNoDataProtocolContentPolicy.h"
#include "nsDOMCID.h"
#include "nsHTMLContentSerializer.h"
#include "nsHTMLParts.h"
#include "nsGenericHTMLElement.h"
#include "nsIComponentManager.h"
#include "nsIContentIterator.h"
#include "nsIContentSerializer.h"
#include "nsIContentViewer.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsIFactory.h"
#include "nsIFrameUtil.h"
#include "nsHTMLStyleSheet.h"
#include "nsILayoutDebugger.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPresShell.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsISelection.h"
#include "nsCaret.h"
#include "nsPlainTextSerializer.h"
#include "nsXMLContentSerializer.h"
#include "nsXHTMLContentSerializer.h"
#include "nsRuleNode.h"
#include "nsContentAreaDragDrop.h"
#include "nsContentList.h"
#include "nsBox.h"
#include "nsIFrameTraversal.h"
#include "nsLayoutCID.h"
#include "nsStyleSheetService.h"
#include "nsFocusManager.h"
#include "ThirdPartyUtil.h"
#include "mozilla/Services.h"
#include "nsStructuredCloneContainer.h"

#include "nsIEventListenerService.h"
#include "nsIFrameMessageManager.h"

// Transformiix stuff
#include "nsXPathEvaluator.h"
#include "txMozillaXSLTProcessor.h"
#include "txNodeSetAdaptor.h"

#include "nsDOMParser.h"
#include "nsDOMSerializer.h"
#include "nsXMLHttpRequest.h"
#include "nsChannelPolicy.h"
#include "nsWebSocket.h"
#include "nsEventSource.h"

// view stuff
#include "nsViewsCID.h"
#include "nsViewManager.h"
#include "nsContentCreatorFunctions.h"

// DOM includes
#include "nsDOMException.h"
#include "nsDOMFileReader.h"
#include "nsFormData.h"
#include "nsBlobProtocolHandler.h"
#include "nsGlobalWindowCommands.h"
#include "nsIControllerCommandTable.h"
#include "nsJSProtocolHandler.h"
#include "nsScriptNameSpaceManager.h"
#include "nsIControllerContext.h"
#include "nsDOMScriptObjectFactory.h"
#include "nsDOMStorage.h"
#include "nsJSON.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "mozilla/dom/DOMRequest.h"

using mozilla::dom::indexedDB::IndexedDatabaseManager;
using mozilla::dom::DOMRequestService;

#ifdef MOZ_B2G_RIL
#include "SystemWorkerManager.h"
using mozilla::dom::gonk::SystemWorkerManager;
#define SYSTEMWORKERMANAGER_CID \
  {0xd53b6524, 0x6ac3, 0x42b0, {0xae, 0xca, 0x62, 0xb3, 0xc4, 0xe5, 0x2b, 0x04}}
#define SYSTEMWORKERMANAGER_CONTRACTID \
  "@mozilla.org/telephony/system-worker-manager;1"
#endif

#ifdef MOZ_WIDGET_GONK
#include "AudioManager.h"
using mozilla::dom::gonk::AudioManager;
#endif
#include "nsDOMMutationObserver.h"

// Editor stuff
#include "nsEditorCID.h"
#include "nsEditor.h"
#include "nsPlaintextEditor.h"
#include "nsEditorController.h" //CID

#include "nsHTMLEditor.h"
#include "nsTextServicesDocument.h"
#include "nsTextServicesCID.h"

#include "nsScriptSecurityManager.h"
#include "nsPrincipal.h"
#include "nsSystemPrincipal.h"
#include "nsNullPrincipal.h"
#include "nsNetCID.h"
#ifndef MOZ_WIDGET_GONK
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_PLATFORM_MAEMO)
#include "nsHapticFeedback.h"
#endif
#endif
#include "nsParserUtils.h"

#define NS_EDITORCOMMANDTABLE_CID \
{ 0x4f5e62b8, 0xd659, 0x4156, { 0x84, 0xfc, 0x2f, 0x60, 0x99, 0x40, 0x03, 0x69 }}

#define NS_EDITINGCOMMANDTABLE_CID \
{ 0xcb38a746, 0xbeb8, 0x43f3, { 0x94, 0x29, 0x77, 0x96, 0xe1, 0xa9, 0x3f, 0xb4 }}

#define NS_HAPTICFEEDBACK_CID \
{ 0x1f15dbc8, 0xbfaa, 0x45de, \
{ 0x8a, 0x46, 0x08, 0xe2, 0xe2, 0x63, 0x26, 0xb0 } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsPlaintextEditor)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsParserUtils)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsTextServicesDocument)
#ifdef ENABLE_EDITOR_API_LOG
#include "nsHTMLEditorLog.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLEditorLog)
#else
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLEditor)
#endif

#include "nsHTMLCanvasFrame.h"

#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsIDOMWebGLRenderingContext.h"

class nsIDocumentLoaderFactory;

#define PRODUCT_NAME "Gecko"

#define NS_HTMLIMGELEMENT_CONTRACTID \
  "@mozilla.org/content/element/html;1?name=img"

#define NS_HTMLOPTIONELEMENT_CONTRACTID \
  "@mozilla.org/content/element/html;1?name=option"

#ifdef MOZ_MEDIA
#define NS_HTMLAUDIOELEMENT_CONTRACTID \
  "@mozilla.org/content/element/html;1?name=audio"
#endif

/* 0ddf4df8-4dbb-4133-8b79-9afb966514f5 */
#define NS_PLUGINDOCLOADERFACTORY_CID \
{ 0x0ddf4df8, 0x4dbb, 0x4133, { 0x8b, 0x79, 0x9a, 0xfb, 0x96, 0x65, 0x14, 0xf5 } }

#define NS_WINDOWCOMMANDTABLE_CID \
 { /* 0DE2FBFA-6B7F-11D7-BBBA-0003938A9D96 */        \
  0x0DE2FBFA, 0x6B7F, 0x11D7, {0xBB, 0xBA, 0x00, 0x03, 0x93, 0x8A, 0x9D, 0x96} }

#include "nsIBoxObject.h"

#ifdef MOZ_XUL
#include "inDOMView.h"
#endif /* MOZ_XUL */

#include "inDeepTreeWalker.h"
#include "inFlasher.h"
#include "inCSSValueSearch.h"
#include "inDOMUtils.h"

#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#include "nsIXULSortService.h"

nsresult
NS_NewXULContentBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult);

nsresult
NS_NewXULTreeBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult);
#endif

static void Shutdown();

#ifdef MOZ_XTF
#include "nsIXTFService.h"
#include "nsIXMLContentBuilder.h"
#endif

#include "nsGeolocation.h"
#include "nsDeviceSensors.h"
#include "nsCSPService.h"
#include "nsISmsService.h"
#include "nsISmsDatabaseService.h"
#include "mozilla/dom/sms/SmsRequestManager.h"
#include "mozilla/dom/sms/SmsServicesFactory.h"
#include "nsIPowerManagerService.h"

using namespace mozilla::dom::sms;

#include "mozilla/dom/power/PowerManagerService.h"

using mozilla::dom::power::PowerManagerService;

// Transformiix
/* 5d5d92cd-6bf8-11d9-bf4a-000a95dc234c */
#define TRANSFORMIIX_NODESET_CID \
{ 0x5d5d92cd, 0x6bf8, 0x11d9, { 0xbf, 0x4a, 0x0, 0x0a, 0x95, 0xdc, 0x23, 0x4c } }

#define TRANSFORMIIX_NODESET_CONTRACTID \
"@mozilla.org/transformiix-nodeset;1"

// Factory Constructor
NS_GENERIC_FACTORY_CONSTRUCTOR(txMozillaXSLTProcessor)
NS_GENERIC_AGGREGATED_CONSTRUCTOR_INIT(nsXPathEvaluator, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(txNodeSetAdaptor, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMSerializer)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsXMLHttpRequest, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEventSource)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebSocket)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDOMFileReader, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFormData)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlobProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMParser)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsDOMStorageManager,
                                         nsDOMStorageManager::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsChannelPolicy)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(IndexedDatabaseManager,
                                         IndexedDatabaseManager::FactoryCreate)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(DOMRequestService,
                                         DOMRequestService::FactoryCreate)
#ifdef MOZ_B2G_RIL
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(SystemWorkerManager,
                                         SystemWorkerManager::FactoryCreate)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMMutationObserver)

#ifdef MOZ_WIDGET_GONK
NS_GENERIC_FACTORY_CONSTRUCTOR(AudioManager)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceSensors)

#ifndef MOZ_WIDGET_GONK
#if defined(ANDROID) || defined(MOZ_PLATFORM_MAEMO)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHapticFeedback)
#endif
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(ThirdPartyUtil, Init)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsISmsService, SmsServicesFactory::CreateSmsService)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsISmsDatabaseService, SmsServicesFactory::CreateSmsDatabaseService)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIPowerManagerService,
                                         PowerManagerService::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(SmsRequestManager)

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
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Shutdown();
    nsContentUtils::XPCOMShutdown();
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------

static bool gInitialized = false;

// Perform our one-time intialization for this module

// static
nsresult
Initialize()
{
  if (gInitialized) {
    NS_RUNTIMEABORT("Recursive layout module initialization");
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(sizeof(PtrBits) == sizeof(void *),
               "Eeek! You'll need to adjust the size of PtrBits to the size "
               "of a pointer on your platform.");

  gInitialized = true;

  nsresult rv;
  rv = xpcModuleCtor();
  if (NS_FAILED(rv))
    return rv;

  rv = nsLayoutStatics::Initialize();
  if (NS_FAILED(rv)) {
    Shutdown();
    return rv;
  }

  // Add our shutdown observer.
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();

  if (observerService) {
    LayoutShutdownObserver* observer = new LayoutShutdownObserver();

    if (!observer) {
      Shutdown();

      return NS_ERROR_OUT_OF_MEMORY;
    }

    observerService->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
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

  gInitialized = false;

  nsLayoutStatics::Release();
}

#ifdef NS_DEBUG
nsresult NS_NewFrameUtil(nsIFrameUtil** aResult);
nsresult NS_NewLayoutDebugger(nsILayoutDebugger** aResult);
#endif

nsresult NS_NewBoxObject(nsIBoxObject** aResult);

#ifdef MOZ_XUL
nsresult NS_NewListBoxObject(nsIBoxObject** aResult);
nsresult NS_NewScrollBoxObject(nsIBoxObject** aResult);
nsresult NS_NewMenuBoxObject(nsIBoxObject** aResult);
nsresult NS_NewPopupBoxObject(nsIBoxObject** aResult);
nsresult NS_NewContainerBoxObject(nsIBoxObject** aResult);
nsresult NS_NewTreeBoxObject(nsIBoxObject** aResult);
#endif

nsresult NS_NewCanvasRenderingContext2D(nsIDOMCanvasRenderingContext2D** aResult);
nsresult NS_NewCanvasRenderingContext2DThebes(nsIDOMCanvasRenderingContext2D** aResult);
nsresult NS_NewCanvasRenderingContextWebGL(nsIDOMWebGLRenderingContext** aResult);

nsresult NS_CreateFrameTraversal(nsIFrameTraversal** aResult);

nsresult NS_NewDomSelection(nsISelection** aResult);
nsresult NS_NewContentViewer(nsIContentViewer** aResult);
nsresult NS_NewContentIterator(nsIContentIterator** aResult);
nsresult NS_NewPreContentIterator(nsIContentIterator** aResult);
nsresult NS_NewGenRegularIterator(nsIContentIterator** aResult);
nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aResult);
nsresult NS_NewGenSubtreeIterator(nsIContentIterator** aInstancePtrResult);
nsresult NS_NewContentDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult);
nsresult NS_NewHTMLCopyTextEncoder(nsIDocumentEncoder** aResult);
nsresult NS_NewTextEncoder(nsIDocumentEncoder** aResult);
nsresult NS_NewContentPolicy(nsIContentPolicy** aResult);

nsresult NS_NewEventListenerService(nsIEventListenerService** aResult);
nsresult NS_NewGlobalMessageManager(nsIChromeFrameMessageManager** aResult);
nsresult NS_NewParentProcessMessageManager(nsIFrameMessageManager** aResult);
nsresult NS_NewChildProcessMessageManager(nsISyncMessageSender** aResult);

nsresult NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult);

#define MAKE_CTOR(ctor_, iface_, func_)                   \
static nsresult                                           \
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
MAKE_CTOR(CreateNewPresShell,           nsIPresShell,           NS_NewPresShell)
MAKE_CTOR(CreateNewBoxObject,           nsIBoxObject,           NS_NewBoxObject)

#ifdef MOZ_XUL
MAKE_CTOR(CreateNewListBoxObject,       nsIBoxObject,           NS_NewListBoxObject)
MAKE_CTOR(CreateNewMenuBoxObject,       nsIBoxObject,           NS_NewMenuBoxObject)
MAKE_CTOR(CreateNewPopupBoxObject,      nsIBoxObject,           NS_NewPopupBoxObject)
MAKE_CTOR(CreateNewScrollBoxObject,     nsIBoxObject,           NS_NewScrollBoxObject)
MAKE_CTOR(CreateNewTreeBoxObject,       nsIBoxObject,           NS_NewTreeBoxObject)
MAKE_CTOR(CreateNewContainerBoxObject,  nsIBoxObject,           NS_NewContainerBoxObject)
#endif // MOZ_XUL

#ifdef MOZ_XUL
NS_GENERIC_FACTORY_CONSTRUCTOR(inDOMView)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(inDeepTreeWalker)
NS_GENERIC_FACTORY_CONSTRUCTOR(inFlasher)
NS_GENERIC_FACTORY_CONSTRUCTOR(inCSSValueSearch)
NS_GENERIC_FACTORY_CONSTRUCTOR(inDOMUtils)

MAKE_CTOR(CreateNameSpaceManager,         nsINameSpaceManager,         NS_GetNameSpaceManager)
MAKE_CTOR(CreateContentViewer,            nsIContentViewer,            NS_NewContentViewer)
MAKE_CTOR(CreateHTMLDocument,             nsIDocument,                 NS_NewHTMLDocument)
MAKE_CTOR(CreateXMLDocument,              nsIDocument,                 NS_NewXMLDocument)
MAKE_CTOR(CreateSVGDocument,              nsIDocument,                 NS_NewSVGDocument)
MAKE_CTOR(CreateImageDocument,            nsIDocument,                 NS_NewImageDocument)
MAKE_CTOR(CreateDOMSelection,             nsISelection,                NS_NewDomSelection)
MAKE_CTOR(CreateContentIterator,          nsIContentIterator,          NS_NewContentIterator)
MAKE_CTOR(CreatePreContentIterator,       nsIContentIterator,          NS_NewPreContentIterator)
MAKE_CTOR(CreateSubtreeIterator,          nsIContentIterator,          NS_NewContentSubtreeIterator)
// CreateHTMLImgElement, see below
// CreateHTMLOptionElement, see below
// CreateHTMLAudioElement, see below
MAKE_CTOR(CreateTextEncoder,              nsIDocumentEncoder,          NS_NewTextEncoder)
MAKE_CTOR(CreateHTMLCopyTextEncoder,      nsIDocumentEncoder,          NS_NewHTMLCopyTextEncoder)
MAKE_CTOR(CreateXMLContentSerializer,     nsIContentSerializer,        NS_NewXMLContentSerializer)
MAKE_CTOR(CreateHTMLContentSerializer,    nsIContentSerializer,        NS_NewHTMLContentSerializer)
MAKE_CTOR(CreateXHTMLContentSerializer,   nsIContentSerializer,        NS_NewXHTMLContentSerializer)
MAKE_CTOR(CreatePlainTextSerializer,      nsIContentSerializer,        NS_NewPlainTextSerializer)
MAKE_CTOR(CreateContentPolicy,            nsIContentPolicy,            NS_NewContentPolicy)
#ifdef MOZ_XUL
MAKE_CTOR(CreateXULSortService,           nsIXULSortService,           NS_NewXULSortService)
// NS_NewXULContentBuilder
// NS_NewXULTreeBuilder
MAKE_CTOR(CreateXULDocument,              nsIXULDocument,              NS_NewXULDocument)
// NS_NewXULControllers
#endif
#ifdef MOZ_XTF
MAKE_CTOR(CreateXTFService,               nsIXTFService,               NS_NewXTFService)
MAKE_CTOR(CreateXMLContentBuilder,        nsIXMLContentBuilder,        NS_NewXMLContentBuilder)
#endif
MAKE_CTOR(CreateContentDLF,               nsIDocumentLoaderFactory,    NS_NewContentDocumentLoaderFactory)
MAKE_CTOR(CreateEventListenerService,     nsIEventListenerService,     NS_NewEventListenerService)
MAKE_CTOR(CreateGlobalMessageManager,     nsIChromeFrameMessageManager,NS_NewGlobalMessageManager)
MAKE_CTOR(CreateParentMessageManager,     nsIFrameMessageManager,NS_NewParentProcessMessageManager)
MAKE_CTOR(CreateChildMessageManager,     nsISyncMessageSender,NS_NewChildProcessMessageManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDataDocumentContentPolicy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNoDataProtocolContentPolicy)
MAKE_CTOR(CreatePluginDocument,           nsIDocument,                 NS_NewPluginDocument)
#ifdef MOZ_MEDIA
MAKE_CTOR(CreateVideoDocument,            nsIDocument,                 NS_NewVideoDocument)
#endif
MAKE_CTOR(CreateFocusManager,             nsIFocusManager,      NS_NewFocusManager)

MAKE_CTOR(CreateCanvasRenderingContext2D, nsIDOMCanvasRenderingContext2D, NS_NewCanvasRenderingContext2D)
MAKE_CTOR(CreateCanvasRenderingContext2DThebes, nsIDOMCanvasRenderingContext2D, NS_NewCanvasRenderingContext2DThebes)
MAKE_CTOR(CreateCanvasRenderingContextWebGL, nsIDOMWebGLRenderingContext, NS_NewCanvasRenderingContextWebGL)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsStyleSheetService, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJSURI)

// views are not refcounted, so this is the same as
// NS_GENERIC_FACTORY_CONSTRUCTOR without the NS_ADDREF/NS_RELEASE
#define NS_GENERIC_FACTORY_CONSTRUCTOR_NOREFS(_InstanceClass)                 \
static nsresult                                                               \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,               \
                            void **aResult)                                   \
{                                                                             \
    nsresult rv;                                                              \
                                                                              \
    *aResult = NULL;                                                          \
    if (NULL != aOuter) {                                                     \
        rv = NS_ERROR_NO_AGGREGATION;                                         \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    _InstanceClass * inst = new _InstanceClass();                             \
    if (NULL == inst) {                                                       \
        rv = NS_ERROR_OUT_OF_MEMORY;                                          \
        return rv;                                                            \
    }                                                                         \
    rv = inst->QueryInterface(aIID, aResult);                                 \
                                                                              \
    return rv;                                                                \
}                                                                             \

NS_GENERIC_FACTORY_CONSTRUCTOR(nsViewManager)

static nsresult
CreateHTMLImgElement(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  *aResult = nsnull;
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  // Note! NS_NewHTMLImageElement is special cased to handle a null nodeinfo
  nsCOMPtr<nsINodeInfo> ni;
  nsIContent* inst = NS_NewHTMLImageElement(ni.forget());
  nsresult rv = NS_ERROR_OUT_OF_MEMORY;
  if (inst) {
    NS_ADDREF(inst);
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);
  }
  return rv;
}

static nsresult
CreateHTMLOptionElement(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  *aResult = nsnull;
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  // Note! NS_NewHTMLOptionElement is special cased to handle a null nodeinfo
  nsCOMPtr<nsINodeInfo> ni;
  nsIContent* inst = NS_NewHTMLOptionElement(ni.forget());
  nsresult rv = NS_ERROR_OUT_OF_MEMORY;
  if (inst) {
    NS_ADDREF(inst);
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);
  }
  return rv;
}

#ifdef MOZ_MEDIA
static nsresult
CreateHTMLAudioElement(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  *aResult = nsnull;
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  // Note! NS_NewHTMLAudioElement is special cased to handle a null nodeinfo
  nsCOMPtr<nsINodeInfo> ni;
  nsCOMPtr<nsIContent> inst(NS_NewHTMLAudioElement(ni.forget()));
  return inst ? inst->QueryInterface(aIID, aResult) : NS_ERROR_OUT_OF_MEMORY;
}
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMScriptObjectFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBaseDOMException)

#define NS_GEOLOCATION_CID \
  { 0x1E1C3FF, 0x94A, 0xD048, { 0x44, 0xB4, 0x62, 0xD2, 0x9C, 0x7B, 0x4F, 0x39 } }

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsGeolocation, Init)

#define NS_GEOLOCATION_SERVICE_CID \
  { 0x404d02a, 0x1CA, 0xAAAB, { 0x47, 0x62, 0x94, 0x4b, 0x1b, 0xf2, 0xf7, 0xb5 } }

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsGeolocationService, nsGeolocationService::GetGeolocationService)

NS_GENERIC_FACTORY_CONSTRUCTOR(CSPService)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrincipal)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecurityNameSet)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsSystemPrincipal,
    nsScriptSecurityManager::SystemPrincipalSingletonConstructor)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNullPrincipal, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStructuredCloneContainer)

static nsresult
Construct_nsIScriptSecurityManager(nsISupports *aOuter, REFNSIID aIID, 
                                   void **aResult)
{
    if (!aResult)
        return NS_ERROR_NULL_POINTER;
    *aResult = nsnull;
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsScriptSecurityManager *obj = nsScriptSecurityManager::GetScriptSecurityManager();
    if (!obj) 
        return NS_ERROR_OUT_OF_MEMORY;
    if (NS_FAILED(obj->QueryInterface(aIID, aResult)))
        return NS_ERROR_FAILURE;
    return NS_OK;
}

#ifdef DEBUG
NS_DEFINE_NAMED_CID(NS_FRAME_UTIL_CID);
NS_DEFINE_NAMED_CID(NS_LAYOUT_DEBUGGER_CID);
#endif
NS_DEFINE_NAMED_CID(NS_FRAMETRAVERSAL_CID);
NS_DEFINE_NAMED_CID(NS_PRESSHELL_CID);
NS_DEFINE_NAMED_CID(NS_BOXOBJECT_CID);
#ifdef MOZ_XUL
NS_DEFINE_NAMED_CID(NS_LISTBOXOBJECT_CID);
NS_DEFINE_NAMED_CID(NS_MENUBOXOBJECT_CID);
NS_DEFINE_NAMED_CID(NS_POPUPBOXOBJECT_CID);
NS_DEFINE_NAMED_CID(NS_CONTAINERBOXOBJECT_CID);
NS_DEFINE_NAMED_CID(NS_SCROLLBOXOBJECT_CID);
NS_DEFINE_NAMED_CID(NS_TREEBOXOBJECT_CID);
#endif // MOZ_XUL
#ifdef MOZ_XUL
NS_DEFINE_NAMED_CID(IN_DOMVIEW_CID);
#endif
NS_DEFINE_NAMED_CID(IN_DEEPTREEWALKER_CID);
NS_DEFINE_NAMED_CID(IN_FLASHER_CID);
NS_DEFINE_NAMED_CID(IN_CSSVALUESEARCH_CID);
NS_DEFINE_NAMED_CID(IN_DOMUTILS_CID);
NS_DEFINE_NAMED_CID(NS_NAMESPACEMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_CONTENT_VIEWER_CID);
NS_DEFINE_NAMED_CID(NS_HTMLDOCUMENT_CID);
NS_DEFINE_NAMED_CID(NS_XMLDOCUMENT_CID);
NS_DEFINE_NAMED_CID(NS_SVGDOCUMENT_CID);
NS_DEFINE_NAMED_CID(NS_IMAGEDOCUMENT_CID);
NS_DEFINE_NAMED_CID(NS_DOMSELECTION_CID);
NS_DEFINE_NAMED_CID(NS_CONTENTITERATOR_CID);
NS_DEFINE_NAMED_CID(NS_PRECONTENTITERATOR_CID);
NS_DEFINE_NAMED_CID(NS_SUBTREEITERATOR_CID);
NS_DEFINE_NAMED_CID(NS_HTMLIMAGEELEMENT_CID);
NS_DEFINE_NAMED_CID(NS_HTMLOPTIONELEMENT_CID);
#ifdef MOZ_MEDIA
NS_DEFINE_NAMED_CID(NS_HTMLAUDIOELEMENT_CID);
#endif
NS_DEFINE_NAMED_CID(NS_CANVASRENDERINGCONTEXT2D_CID);
NS_DEFINE_NAMED_CID(NS_CANVASRENDERINGCONTEXT2DTHEBES_CID);
NS_DEFINE_NAMED_CID(NS_CANVASRENDERINGCONTEXTWEBGL_CID);
NS_DEFINE_NAMED_CID(NS_TEXT_ENCODER_CID);
NS_DEFINE_NAMED_CID(NS_HTMLCOPY_TEXT_ENCODER_CID);
NS_DEFINE_NAMED_CID(NS_XMLCONTENTSERIALIZER_CID);
NS_DEFINE_NAMED_CID(NS_XHTMLCONTENTSERIALIZER_CID);
NS_DEFINE_NAMED_CID(NS_HTMLCONTENTSERIALIZER_CID);
NS_DEFINE_NAMED_CID(NS_PLAINTEXTSERIALIZER_CID);
NS_DEFINE_NAMED_CID(NS_PARSERUTILS_CID);
NS_DEFINE_NAMED_CID(NS_SCRIPTABLEUNESCAPEHTML_CID);
NS_DEFINE_NAMED_CID(NS_CONTENTPOLICY_CID);
NS_DEFINE_NAMED_CID(NS_DATADOCUMENTCONTENTPOLICY_CID);
NS_DEFINE_NAMED_CID(NS_NODATAPROTOCOLCONTENTPOLICY_CID);
NS_DEFINE_NAMED_CID(NS_XULCONTROLLERS_CID);
#ifdef MOZ_XUL
NS_DEFINE_NAMED_CID(NS_XULSORTSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_XULTEMPLATEBUILDER_CID);
NS_DEFINE_NAMED_CID(NS_XULTREEBUILDER_CID);
NS_DEFINE_NAMED_CID(NS_XULDOCUMENT_CID);
#endif
#ifdef MOZ_XTF
NS_DEFINE_NAMED_CID(NS_XTFSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_XMLCONTENTBUILDER_CID);
#endif
NS_DEFINE_NAMED_CID(NS_CONTENT_DOCUMENT_LOADER_FACTORY_CID);
NS_DEFINE_NAMED_CID(NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
NS_DEFINE_NAMED_CID(NS_BASE_DOM_EXCEPTION_CID);
NS_DEFINE_NAMED_CID(NS_JSPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_JSURI_CID);
NS_DEFINE_NAMED_CID(NS_WINDOWCOMMANDTABLE_CID);
NS_DEFINE_NAMED_CID(NS_WINDOWCONTROLLER_CID);
NS_DEFINE_NAMED_CID(NS_VIEW_MANAGER_CID);
NS_DEFINE_NAMED_CID(NS_PLUGINDOCLOADERFACTORY_CID);
NS_DEFINE_NAMED_CID(NS_PLUGINDOCUMENT_CID);
#ifdef MOZ_MEDIA
NS_DEFINE_NAMED_CID(NS_VIDEODOCUMENT_CID);
#endif
NS_DEFINE_NAMED_CID(NS_STYLESHEETSERVICE_CID);
NS_DEFINE_NAMED_CID(TRANSFORMIIX_XSLT_PROCESSOR_CID);
NS_DEFINE_NAMED_CID(TRANSFORMIIX_XPATH_EVALUATOR_CID);
NS_DEFINE_NAMED_CID(TRANSFORMIIX_NODESET_CID);
NS_DEFINE_NAMED_CID(NS_XMLSERIALIZER_CID);
NS_DEFINE_NAMED_CID(NS_FILEREADER_CID);
NS_DEFINE_NAMED_CID(NS_FORMDATA_CID);
NS_DEFINE_NAMED_CID(NS_BLOBPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_XMLHTTPREQUEST_CID);
NS_DEFINE_NAMED_CID(NS_EVENTSOURCE_CID);
NS_DEFINE_NAMED_CID(NS_WEBSOCKET_CID);
NS_DEFINE_NAMED_CID(NS_DOMPARSER_CID);
NS_DEFINE_NAMED_CID(NS_DOMSTORAGE2_CID);
NS_DEFINE_NAMED_CID(NS_DOMSTORAGEMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_DOMJSON_CID);
NS_DEFINE_NAMED_CID(NS_TEXTEDITOR_CID);
NS_DEFINE_NAMED_CID(INDEXEDDB_MANAGER_CID);
NS_DEFINE_NAMED_CID(DOMREQUEST_SERVICE_CID);
#ifdef MOZ_B2G_RIL
NS_DEFINE_NAMED_CID(SYSTEMWORKERMANAGER_CID);
#endif
#ifdef MOZ_WIDGET_GONK
NS_DEFINE_NAMED_CID(NS_AUDIOMANAGER_CID);
#endif
#ifdef ENABLE_EDITOR_API_LOG
NS_DEFINE_NAMED_CID(NS_HTMLEDITOR_CID);
#else
NS_DEFINE_NAMED_CID(NS_HTMLEDITOR_CID);
#endif
NS_DEFINE_NAMED_CID(NS_EDITORCONTROLLER_CID);
NS_DEFINE_NAMED_CID(NS_EDITINGCONTROLLER_CID);
NS_DEFINE_NAMED_CID(NS_EDITORCOMMANDTABLE_CID);
NS_DEFINE_NAMED_CID(NS_EDITINGCOMMANDTABLE_CID);
NS_DEFINE_NAMED_CID(NS_TEXTSERVICESDOCUMENT_CID);
NS_DEFINE_NAMED_CID(NS_GEOLOCATION_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_GEOLOCATION_CID);
NS_DEFINE_NAMED_CID(NS_FOCUSMANAGER_CID);
NS_DEFINE_NAMED_CID(CSPSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_EVENTLISTENERSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_GLOBALMESSAGEMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_PARENTPROCESSMESSAGEMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_CHILDPROCESSMESSAGEMANAGER_CID);
NS_DEFINE_NAMED_CID(NSCHANNELPOLICY_CID);
NS_DEFINE_NAMED_CID(NS_SCRIPTSECURITYMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_PRINCIPAL_CID);
NS_DEFINE_NAMED_CID(NS_SYSTEMPRINCIPAL_CID);
NS_DEFINE_NAMED_CID(NS_NULLPRINCIPAL_CID);
NS_DEFINE_NAMED_CID(NS_SECURITYNAMESET_CID);
NS_DEFINE_NAMED_CID(THIRDPARTYUTIL_CID);
NS_DEFINE_NAMED_CID(NS_STRUCTUREDCLONECONTAINER_CID);
NS_DEFINE_NAMED_CID(NS_DOMMUTATIONOBSERVER_CID);
NS_DEFINE_NAMED_CID(NS_DEVICE_SENSORS_CID);

#ifndef MOZ_WIDGET_GONK
#if defined(ANDROID) || defined(MOZ_PLATFORM_MAEMO)
NS_DEFINE_NAMED_CID(NS_HAPTICFEEDBACK_CID);
#endif
#endif
NS_DEFINE_NAMED_CID(SMS_SERVICE_CID);
NS_DEFINE_NAMED_CID(SMS_DATABASE_SERVICE_CID);
NS_DEFINE_NAMED_CID(SMS_REQUEST_MANAGER_CID);
NS_DEFINE_NAMED_CID(NS_POWERMANAGERSERVICE_CID);

static nsresult
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

static nsresult
CreateWindowControllerWithSingletonCommandTable(nsISupports *aOuter,
                                      REFNSIID aIID, void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIController> controller =
       do_CreateInstance("@mozilla.org/embedcomp/base-command-controller;1", &rv);

 if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIControllerCommandTable> windowCommandTable = do_GetService(kNS_WINDOWCOMMANDTABLE_CID, &rv);
  if (NS_FAILED(rv)) return rv;

  // this is a singleton; make it immutable
  windowCommandTable->MakeImmutable();

  nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller, &rv);
  if (NS_FAILED(rv)) return rv;

  controllerContext->Init(windowCommandTable);
  if (NS_FAILED(rv)) return rv;

  return controller->QueryInterface(aIID, aResult);
}

// Constructor of a controller which is set up to use, internally, a
// singleton command-table pre-filled with editor commands.
static nsresult
nsEditorControllerConstructor(nsISupports *aOuter, REFNSIID aIID,
                              void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIController> controller = do_CreateInstance("@mozilla.org/embedcomp/base-command-controller;1", &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIControllerCommandTable> editorCommandTable = do_GetService(kNS_EDITORCOMMANDTABLE_CID, &rv);
  if (NS_FAILED(rv)) return rv;

  // this guy is a singleton, so make it immutable
  editorCommandTable->MakeImmutable();

  nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = controllerContext->Init(editorCommandTable);
  if (NS_FAILED(rv)) return rv;

  return controller->QueryInterface(aIID, aResult);
}

// Constructor of a controller which is set up to use, internally, a
// singleton command-table pre-filled with editing commands.
static nsresult
nsEditingControllerConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIController> controller = do_CreateInstance("@mozilla.org/embedcomp/base-command-controller;1", &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIControllerCommandTable> editingCommandTable = do_GetService(kNS_EDITINGCOMMANDTABLE_CID, &rv);
  if (NS_FAILED(rv)) return rv;

  // this guy is a singleton, so make it immutable
  editingCommandTable->MakeImmutable();

  nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = controllerContext->Init(editingCommandTable);
  if (NS_FAILED(rv)) return rv;

  return controller->QueryInterface(aIID, aResult);
}

// Constructor for a command-table pre-filled with editor commands
static nsresult
nsEditorCommandTableConstructor(nsISupports *aOuter, REFNSIID aIID,
                                            void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIControllerCommandTable> commandTable =
      do_CreateInstance(NS_CONTROLLERCOMMANDTABLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = nsEditorController::RegisterEditorCommands(commandTable);
  if (NS_FAILED(rv)) return rv;

  // we don't know here whether we're being created as an instance,
  // or a service, so we can't become immutable

  return commandTable->QueryInterface(aIID, aResult);
}

// Constructor for a command-table pre-filled with editing commands
static nsresult
nsEditingCommandTableConstructor(nsISupports *aOuter, REFNSIID aIID,
                                              void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIControllerCommandTable> commandTable =
      do_CreateInstance(NS_CONTROLLERCOMMANDTABLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = nsEditorController::RegisterEditingCommands(commandTable);
  if (NS_FAILED(rv)) return rv;

  // we don't know here whether we're being created as an instance,
  // or a service, so we can't become immutable

  return commandTable->QueryInterface(aIID, aResult);
}


static const mozilla::Module::CIDEntry kLayoutCIDs[] = {
  XPCONNECT_CIDENTRIES
#ifdef DEBUG
  { &kNS_FRAME_UTIL_CID, false, NULL, CreateNewFrameUtil },
  { &kNS_LAYOUT_DEBUGGER_CID, false, NULL, CreateNewLayoutDebugger },
#endif
  { &kNS_FRAMETRAVERSAL_CID, false, NULL, CreateNewFrameTraversal },
  { &kNS_PRESSHELL_CID, false, NULL, CreateNewPresShell },
  { &kNS_BOXOBJECT_CID, false, NULL, CreateNewBoxObject },
#ifdef MOZ_XUL
  { &kNS_LISTBOXOBJECT_CID, false, NULL, CreateNewListBoxObject },
  { &kNS_MENUBOXOBJECT_CID, false, NULL, CreateNewMenuBoxObject },
  { &kNS_POPUPBOXOBJECT_CID, false, NULL, CreateNewPopupBoxObject },
  { &kNS_CONTAINERBOXOBJECT_CID, false, NULL, CreateNewContainerBoxObject },
  { &kNS_SCROLLBOXOBJECT_CID, false, NULL, CreateNewScrollBoxObject },
  { &kNS_TREEBOXOBJECT_CID, false, NULL, CreateNewTreeBoxObject },
#endif // MOZ_XUL
#ifdef MOZ_XUL
  { &kIN_DOMVIEW_CID, false, NULL, inDOMViewConstructor },
#endif
  { &kIN_DEEPTREEWALKER_CID, false, NULL, inDeepTreeWalkerConstructor },
  { &kIN_FLASHER_CID, false, NULL, inFlasherConstructor },
  { &kIN_CSSVALUESEARCH_CID, false, NULL, inCSSValueSearchConstructor },
  { &kIN_DOMUTILS_CID, false, NULL, inDOMUtilsConstructor },
  { &kNS_NAMESPACEMANAGER_CID, false, NULL, CreateNameSpaceManager },
  { &kNS_CONTENT_VIEWER_CID, false, NULL, CreateContentViewer },
  { &kNS_HTMLDOCUMENT_CID, false, NULL, CreateHTMLDocument },
  { &kNS_XMLDOCUMENT_CID, false, NULL, CreateXMLDocument },
  { &kNS_SVGDOCUMENT_CID, false, NULL, CreateSVGDocument },
  { &kNS_IMAGEDOCUMENT_CID, false, NULL, CreateImageDocument },
  { &kNS_DOMSELECTION_CID, false, NULL, CreateDOMSelection },
  { &kNS_CONTENTITERATOR_CID, false, NULL, CreateContentIterator },
  { &kNS_PRECONTENTITERATOR_CID, false, NULL, CreatePreContentIterator },
  { &kNS_SUBTREEITERATOR_CID, false, NULL, CreateSubtreeIterator },
  { &kNS_HTMLIMAGEELEMENT_CID, false, NULL, CreateHTMLImgElement },
  { &kNS_HTMLOPTIONELEMENT_CID, false, NULL, CreateHTMLOptionElement },
#ifdef MOZ_MEDIA
  { &kNS_HTMLAUDIOELEMENT_CID, false, NULL, CreateHTMLAudioElement },
#endif
  { &kNS_CANVASRENDERINGCONTEXT2DTHEBES_CID, false, NULL, CreateCanvasRenderingContext2DThebes },
  { &kNS_CANVASRENDERINGCONTEXT2D_CID, false, NULL, CreateCanvasRenderingContext2D },
  { &kNS_CANVASRENDERINGCONTEXTWEBGL_CID, false, NULL, CreateCanvasRenderingContextWebGL },
  { &kNS_TEXT_ENCODER_CID, false, NULL, CreateTextEncoder },
  { &kNS_HTMLCOPY_TEXT_ENCODER_CID, false, NULL, CreateHTMLCopyTextEncoder },
  { &kNS_XMLCONTENTSERIALIZER_CID, false, NULL, CreateXMLContentSerializer },
  { &kNS_HTMLCONTENTSERIALIZER_CID, false, NULL, CreateHTMLContentSerializer },
  { &kNS_XHTMLCONTENTSERIALIZER_CID, false, NULL, CreateXHTMLContentSerializer },
  { &kNS_PLAINTEXTSERIALIZER_CID, false, NULL, CreatePlainTextSerializer },
  { &kNS_PARSERUTILS_CID, false, NULL, nsParserUtilsConstructor },
  { &kNS_SCRIPTABLEUNESCAPEHTML_CID, false, NULL, nsParserUtilsConstructor },
  { &kNS_CONTENTPOLICY_CID, false, NULL, CreateContentPolicy },
  { &kNS_DATADOCUMENTCONTENTPOLICY_CID, false, NULL, nsDataDocumentContentPolicyConstructor },
  { &kNS_NODATAPROTOCOLCONTENTPOLICY_CID, false, NULL, nsNoDataProtocolContentPolicyConstructor },
  { &kNS_XULCONTROLLERS_CID, false, NULL, NS_NewXULControllers },
#ifdef MOZ_XUL
  { &kNS_XULSORTSERVICE_CID, false, NULL, CreateXULSortService },
  { &kNS_XULTEMPLATEBUILDER_CID, false, NULL, NS_NewXULContentBuilder },
  { &kNS_XULTREEBUILDER_CID, false, NULL, NS_NewXULTreeBuilder },
  { &kNS_XULDOCUMENT_CID, false, NULL, CreateXULDocument },
#endif
#ifdef MOZ_XTF
  { &kNS_XTFSERVICE_CID, false, NULL, CreateXTFService },
  { &kNS_XMLCONTENTBUILDER_CID, false, NULL, CreateXMLContentBuilder },
#endif
  { &kNS_CONTENT_DOCUMENT_LOADER_FACTORY_CID, false, NULL, CreateContentDLF },
  { &kNS_DOM_SCRIPT_OBJECT_FACTORY_CID, false, NULL, nsDOMScriptObjectFactoryConstructor },
  { &kNS_BASE_DOM_EXCEPTION_CID, false, NULL, nsBaseDOMExceptionConstructor },
  { &kNS_JSPROTOCOLHANDLER_CID, false, NULL, nsJSProtocolHandler::Create },
  { &kNS_JSURI_CID, false, NULL, nsJSURIConstructor },
  { &kNS_WINDOWCOMMANDTABLE_CID, false, NULL, CreateWindowCommandTableConstructor },
  { &kNS_WINDOWCONTROLLER_CID, false, NULL, CreateWindowControllerWithSingletonCommandTable },
  { &kNS_VIEW_MANAGER_CID, false, NULL, nsViewManagerConstructor },
  { &kNS_PLUGINDOCLOADERFACTORY_CID, false, NULL, CreateContentDLF },
  { &kNS_PLUGINDOCUMENT_CID, false, NULL, CreatePluginDocument },
#ifdef MOZ_MEDIA
  { &kNS_VIDEODOCUMENT_CID, false, NULL, CreateVideoDocument },
#endif
  { &kNS_STYLESHEETSERVICE_CID, false, NULL, nsStyleSheetServiceConstructor },
  { &kTRANSFORMIIX_XSLT_PROCESSOR_CID, false, NULL, txMozillaXSLTProcessorConstructor },
  { &kTRANSFORMIIX_XPATH_EVALUATOR_CID, false, NULL, nsXPathEvaluatorConstructor },
  { &kTRANSFORMIIX_NODESET_CID, false, NULL, txNodeSetAdaptorConstructor },
  { &kNS_XMLSERIALIZER_CID, false, NULL, nsDOMSerializerConstructor },
  { &kNS_FILEREADER_CID, false, NULL, nsDOMFileReaderConstructor },
  { &kNS_FORMDATA_CID, false, NULL, nsFormDataConstructor },
  { &kNS_BLOBPROTOCOLHANDLER_CID, false, NULL, nsBlobProtocolHandlerConstructor },
  { &kNS_XMLHTTPREQUEST_CID, false, NULL, nsXMLHttpRequestConstructor },
  { &kNS_EVENTSOURCE_CID, false, NULL, nsEventSourceConstructor },
  { &kNS_WEBSOCKET_CID, false, NULL, nsWebSocketConstructor },
  { &kNS_DOMPARSER_CID, false, NULL, nsDOMParserConstructor },
  { &kNS_DOMSTORAGE2_CID, false, NULL, NS_NewDOMStorage2 },
  { &kNS_DOMSTORAGEMANAGER_CID, false, NULL, nsDOMStorageManagerConstructor },
  { &kNS_DOMJSON_CID, false, NULL, NS_NewJSON },
  { &kNS_TEXTEDITOR_CID, false, NULL, nsPlaintextEditorConstructor },
  { &kINDEXEDDB_MANAGER_CID, false, NULL, IndexedDatabaseManagerConstructor },
  { &kDOMREQUEST_SERVICE_CID, false, NULL, DOMRequestServiceConstructor },
#ifdef MOZ_B2G_RIL
  { &kSYSTEMWORKERMANAGER_CID, true, NULL, SystemWorkerManagerConstructor },
#endif
#ifdef MOZ_WIDGET_GONK
  { &kNS_AUDIOMANAGER_CID, true, NULL, AudioManagerConstructor },
#endif
#ifdef ENABLE_EDITOR_API_LOG
  { &kNS_HTMLEDITOR_CID, false, NULL, nsHTMLEditorLogConstructor },
#else
  { &kNS_HTMLEDITOR_CID, false, NULL, nsHTMLEditorConstructor },
#endif
  { &kNS_EDITORCONTROLLER_CID, false, NULL, nsEditorControllerConstructor },
  { &kNS_EDITINGCONTROLLER_CID, false, NULL, nsEditingControllerConstructor },
  { &kNS_EDITORCOMMANDTABLE_CID, false, NULL, nsEditorCommandTableConstructor },
  { &kNS_EDITINGCOMMANDTABLE_CID, false, NULL, nsEditingCommandTableConstructor },
  { &kNS_TEXTSERVICESDOCUMENT_CID, false, NULL, nsTextServicesDocumentConstructor },
  { &kNS_GEOLOCATION_SERVICE_CID, false, NULL, nsGeolocationServiceConstructor },
  { &kNS_GEOLOCATION_CID, false, NULL, nsGeolocationConstructor },
  { &kNS_FOCUSMANAGER_CID, false, NULL, CreateFocusManager },
  { &kCSPSERVICE_CID, false, NULL, CSPServiceConstructor },
  { &kNS_EVENTLISTENERSERVICE_CID, false, NULL, CreateEventListenerService },
  { &kNS_GLOBALMESSAGEMANAGER_CID, false, NULL, CreateGlobalMessageManager },
  { &kNS_PARENTPROCESSMESSAGEMANAGER_CID, false, NULL, CreateParentMessageManager },
  { &kNS_CHILDPROCESSMESSAGEMANAGER_CID, false, NULL, CreateChildMessageManager },
  { &kNSCHANNELPOLICY_CID, false, NULL, nsChannelPolicyConstructor },
  { &kNS_SCRIPTSECURITYMANAGER_CID, false, NULL, Construct_nsIScriptSecurityManager },
  { &kNS_PRINCIPAL_CID, false, NULL, nsPrincipalConstructor },
  { &kNS_SYSTEMPRINCIPAL_CID, false, NULL, nsSystemPrincipalConstructor },
  { &kNS_NULLPRINCIPAL_CID, false, NULL, nsNullPrincipalConstructor },
  { &kNS_SECURITYNAMESET_CID, false, NULL, nsSecurityNameSetConstructor },
  { &kNS_DEVICE_SENSORS_CID, false, NULL, nsDeviceSensorsConstructor },
#ifndef MOZ_WIDGET_GONK
#if defined(ANDROID) || defined(MOZ_PLATFORM_MAEMO)
  { &kNS_HAPTICFEEDBACK_CID, false, NULL, nsHapticFeedbackConstructor },
#endif
#endif
  { &kTHIRDPARTYUTIL_CID, false, NULL, ThirdPartyUtilConstructor },
  { &kNS_STRUCTUREDCLONECONTAINER_CID, false, NULL, nsStructuredCloneContainerConstructor },
  { &kNS_DOMMUTATIONOBSERVER_CID, false, NULL, nsDOMMutationObserverConstructor },
  { &kSMS_SERVICE_CID, false, NULL, nsISmsServiceConstructor },
  { &kSMS_DATABASE_SERVICE_CID, false, NULL, nsISmsDatabaseServiceConstructor },
  { &kSMS_REQUEST_MANAGER_CID, false, NULL, SmsRequestManagerConstructor },
  { &kNS_POWERMANAGERSERVICE_CID, false, NULL, nsIPowerManagerServiceConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kLayoutContracts[] = {
  XPCONNECT_CONTRACTS
  { "@mozilla.org/layout/xul-boxobject;1", &kNS_BOXOBJECT_CID },
#ifdef MOZ_XUL
  { "@mozilla.org/layout/xul-boxobject-listbox;1", &kNS_LISTBOXOBJECT_CID },
  { "@mozilla.org/layout/xul-boxobject-menu;1", &kNS_MENUBOXOBJECT_CID },
  { "@mozilla.org/layout/xul-boxobject-popup;1", &kNS_POPUPBOXOBJECT_CID },
  { "@mozilla.org/layout/xul-boxobject-container;1", &kNS_CONTAINERBOXOBJECT_CID },
  { "@mozilla.org/layout/xul-boxobject-scrollbox;1", &kNS_SCROLLBOXOBJECT_CID },
  { "@mozilla.org/layout/xul-boxobject-tree;1", &kNS_TREEBOXOBJECT_CID },
#endif // MOZ_XUL
#ifdef MOZ_XUL
  { "@mozilla.org/inspector/dom-view;1", &kIN_DOMVIEW_CID },
#endif
  { "@mozilla.org/inspector/deep-tree-walker;1", &kIN_DEEPTREEWALKER_CID },
  { "@mozilla.org/inspector/flasher;1", &kIN_FLASHER_CID },
  { "@mozilla.org/inspector/search;1?type=cssvalue", &kIN_CSSVALUESEARCH_CID },
  { "@mozilla.org/inspector/dom-utils;1", &kIN_DOMUTILS_CID },
  { NS_NAMESPACEMANAGER_CONTRACTID, &kNS_NAMESPACEMANAGER_CID },
  { "@mozilla.org/xml/xml-document;1", &kNS_XMLDOCUMENT_CID },
  { "@mozilla.org/svg/svg-document;1", &kNS_SVGDOCUMENT_CID },
  { "@mozilla.org/content/dom-selection;1", &kNS_DOMSELECTION_CID },
  { "@mozilla.org/content/post-content-iterator;1", &kNS_CONTENTITERATOR_CID },
  { "@mozilla.org/content/pre-content-iterator;1", &kNS_PRECONTENTITERATOR_CID },
  { "@mozilla.org/content/subtree-content-iterator;1", &kNS_SUBTREEITERATOR_CID },
  { NS_HTMLIMGELEMENT_CONTRACTID, &kNS_HTMLIMAGEELEMENT_CID },
  { NS_HTMLOPTIONELEMENT_CONTRACTID, &kNS_HTMLOPTIONELEMENT_CID },
#ifdef MOZ_MEDIA
  { NS_HTMLAUDIOELEMENT_CONTRACTID, &kNS_HTMLAUDIOELEMENT_CID },
#endif
  { "@mozilla.org/content/canvas-rendering-context;1?id=2d", &kNS_CANVASRENDERINGCONTEXT2D_CID },
  { "@mozilla.org/content/2dthebes-canvas-rendering-context;1", &kNS_CANVASRENDERINGCONTEXT2DTHEBES_CID },
  { "@mozilla.org/content/canvas-rendering-context;1?id=moz-webgl", &kNS_CANVASRENDERINGCONTEXTWEBGL_CID },
  { "@mozilla.org/content/canvas-rendering-context;1?id=experimental-webgl", &kNS_CANVASRENDERINGCONTEXTWEBGL_CID },
  { NS_DOC_ENCODER_CONTRACTID_BASE "text/xml", &kNS_TEXT_ENCODER_CID },
  { NS_DOC_ENCODER_CONTRACTID_BASE "application/xml", &kNS_TEXT_ENCODER_CID },
  { NS_DOC_ENCODER_CONTRACTID_BASE "application/xhtml+xml", &kNS_TEXT_ENCODER_CID },
  { NS_DOC_ENCODER_CONTRACTID_BASE "image/svg+xml", &kNS_TEXT_ENCODER_CID },
  { NS_DOC_ENCODER_CONTRACTID_BASE "text/html", &kNS_TEXT_ENCODER_CID },
  { NS_DOC_ENCODER_CONTRACTID_BASE "text/plain", &kNS_TEXT_ENCODER_CID },
  { NS_HTMLCOPY_ENCODER_CONTRACTID, &kNS_HTMLCOPY_TEXT_ENCODER_CID },
  { NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "text/xml", &kNS_XMLCONTENTSERIALIZER_CID },
  { NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "application/xml", &kNS_XMLCONTENTSERIALIZER_CID },
  { NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "application/xhtml+xml", &kNS_XHTMLCONTENTSERIALIZER_CID },
  { NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "image/svg+xml", &kNS_XMLCONTENTSERIALIZER_CID },
  { NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "text/html", &kNS_HTMLCONTENTSERIALIZER_CID },
  { NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "application/vnd.mozilla.xul+xml", &kNS_XMLCONTENTSERIALIZER_CID },
  { NS_CONTENTSERIALIZER_CONTRACTID_PREFIX "text/plain", &kNS_PLAINTEXTSERIALIZER_CID },
  { NS_PARSERUTILS_CONTRACTID, &kNS_PARSERUTILS_CID },
  { NS_SCRIPTABLEUNESCAPEHTML_CONTRACTID, &kNS_SCRIPTABLEUNESCAPEHTML_CID },
  { NS_CONTENTPOLICY_CONTRACTID, &kNS_CONTENTPOLICY_CID },
  { NS_DATADOCUMENTCONTENTPOLICY_CONTRACTID, &kNS_DATADOCUMENTCONTENTPOLICY_CID },
  { NS_NODATAPROTOCOLCONTENTPOLICY_CONTRACTID, &kNS_NODATAPROTOCOLCONTENTPOLICY_CID },
  { "@mozilla.org/xul/xul-controllers;1", &kNS_XULCONTROLLERS_CID },
#ifdef MOZ_XUL
  { "@mozilla.org/xul/xul-sort-service;1", &kNS_XULSORTSERVICE_CID },
  { "@mozilla.org/xul/xul-template-builder;1", &kNS_XULTEMPLATEBUILDER_CID },
  { "@mozilla.org/xul/xul-tree-builder;1", &kNS_XULTREEBUILDER_CID },
  { "@mozilla.org/xul/xul-document;1", &kNS_XULDOCUMENT_CID },
#endif
#ifdef MOZ_XTF
  { NS_XTFSERVICE_CONTRACTID, &kNS_XTFSERVICE_CID },
  { NS_XMLCONTENTBUILDER_CONTRACTID, &kNS_XMLCONTENTBUILDER_CID },
#endif
  { CONTENT_DLF_CONTRACTID, &kNS_CONTENT_DOCUMENT_LOADER_FACTORY_CID },
  { NS_JSPROTOCOLHANDLER_CONTRACTID, &kNS_JSPROTOCOLHANDLER_CID },
  { NS_WINDOWCONTROLLER_CONTRACTID, &kNS_WINDOWCONTROLLER_CID },
  { "@mozilla.org/view-manager;1", &kNS_VIEW_MANAGER_CID },
  { PLUGIN_DLF_CONTRACTID, &kNS_PLUGINDOCLOADERFACTORY_CID },
  { NS_STYLESHEETSERVICE_CONTRACTID, &kNS_STYLESHEETSERVICE_CID },
  { TRANSFORMIIX_XSLT_PROCESSOR_CONTRACTID, &kTRANSFORMIIX_XSLT_PROCESSOR_CID },
  { NS_XPATH_EVALUATOR_CONTRACTID, &kTRANSFORMIIX_XPATH_EVALUATOR_CID },
  { TRANSFORMIIX_NODESET_CONTRACTID, &kTRANSFORMIIX_NODESET_CID },
  { NS_XMLSERIALIZER_CONTRACTID, &kNS_XMLSERIALIZER_CID },
  { NS_FILEREADER_CONTRACTID, &kNS_FILEREADER_CID },
  { NS_FORMDATA_CONTRACTID, &kNS_FORMDATA_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX BLOBURI_SCHEME, &kNS_BLOBPROTOCOLHANDLER_CID },
  { NS_XMLHTTPREQUEST_CONTRACTID, &kNS_XMLHTTPREQUEST_CID },
  { NS_EVENTSOURCE_CONTRACTID, &kNS_EVENTSOURCE_CID },
  { NS_WEBSOCKET_CONTRACTID, &kNS_WEBSOCKET_CID },
  { NS_DOMPARSER_CONTRACTID, &kNS_DOMPARSER_CID },
  { "@mozilla.org/dom/storage;2", &kNS_DOMSTORAGE2_CID },
  { "@mozilla.org/dom/storagemanager;1", &kNS_DOMSTORAGEMANAGER_CID },
  { "@mozilla.org/dom/json;1", &kNS_DOMJSON_CID },
  { "@mozilla.org/editor/texteditor;1", &kNS_TEXTEDITOR_CID },
  { INDEXEDDB_MANAGER_CONTRACTID, &kINDEXEDDB_MANAGER_CID },
  { DOMREQUEST_SERVICE_CONTRACTID, &kDOMREQUEST_SERVICE_CID },
#ifdef MOZ_B2G_RIL  
  { SYSTEMWORKERMANAGER_CONTRACTID, &kSYSTEMWORKERMANAGER_CID },
#endif
#ifdef MOZ_WIDGET_GONK
  { NS_AUDIOMANAGER_CONTRACTID, &kNS_AUDIOMANAGER_CID },
#endif
#ifdef ENABLE_EDITOR_API_LOG
  { "@mozilla.org/editor/htmleditor;1", &kNS_HTMLEDITOR_CID },
#else
  { "@mozilla.org/editor/htmleditor;1", &kNS_HTMLEDITOR_CID },
#endif
  { "@mozilla.org/editor/editorcontroller;1", &kNS_EDITORCONTROLLER_CID },
  { "@mozilla.org/editor/editingcontroller;1", &kNS_EDITINGCONTROLLER_CID },
  { "@mozilla.org/textservices/textservicesdocument;1", &kNS_TEXTSERVICESDOCUMENT_CID },
  { "@mozilla.org/geolocation/service;1", &kNS_GEOLOCATION_SERVICE_CID },
  { "@mozilla.org/geolocation;1", &kNS_GEOLOCATION_CID },
  { "@mozilla.org/focus-manager;1", &kNS_FOCUSMANAGER_CID },
  { CSPSERVICE_CONTRACTID, &kCSPSERVICE_CID },
  { NS_EVENTLISTENERSERVICE_CONTRACTID, &kNS_EVENTLISTENERSERVICE_CID },
  { NS_GLOBALMESSAGEMANAGER_CONTRACTID, &kNS_GLOBALMESSAGEMANAGER_CID },
  { NS_PARENTPROCESSMESSAGEMANAGER_CONTRACTID, &kNS_PARENTPROCESSMESSAGEMANAGER_CID },
  { NS_CHILDPROCESSMESSAGEMANAGER_CONTRACTID, &kNS_CHILDPROCESSMESSAGEMANAGER_CID },
  { NSCHANNELPOLICY_CONTRACTID, &kNSCHANNELPOLICY_CID },
  { NS_SCRIPTSECURITYMANAGER_CONTRACTID, &kNS_SCRIPTSECURITYMANAGER_CID },
  { NS_GLOBAL_CHANNELEVENTSINK_CONTRACTID, &kNS_SCRIPTSECURITYMANAGER_CID },
  { NS_PRINCIPAL_CONTRACTID, &kNS_PRINCIPAL_CID },
  { NS_SYSTEMPRINCIPAL_CONTRACTID, &kNS_SYSTEMPRINCIPAL_CID },
  { NS_NULLPRINCIPAL_CONTRACTID, &kNS_NULLPRINCIPAL_CID },
  { NS_SECURITYNAMESET_CONTRACTID, &kNS_SECURITYNAMESET_CID },
  { NS_DEVICE_SENSORS_CONTRACTID, &kNS_DEVICE_SENSORS_CID },
#ifndef MOZ_WIDGET_GONK
#if defined(ANDROID) || defined(MOZ_PLATFORM_MAEMO)
  { "@mozilla.org/widget/hapticfeedback;1", &kNS_HAPTICFEEDBACK_CID },
#endif
#endif
  { THIRDPARTYUTIL_CONTRACTID, &kTHIRDPARTYUTIL_CID },
  { NS_STRUCTUREDCLONECONTAINER_CONTRACTID, &kNS_STRUCTUREDCLONECONTAINER_CID },
  { NS_DOMMUTATIONOBSERVER_CONTRACTID, &kNS_DOMMUTATIONOBSERVER_CID },
  { SMS_SERVICE_CONTRACTID, &kSMS_SERVICE_CID },
  { SMS_DATABASE_SERVICE_CONTRACTID, &kSMS_DATABASE_SERVICE_CID },
  { SMS_REQUEST_MANAGER_CONTRACTID, &kSMS_REQUEST_MANAGER_CID },
  { POWERMANAGERSERVICE_CONTRACTID, &kNS_POWERMANAGERSERVICE_CID },
  { NULL }
};

static const mozilla::Module::CategoryEntry kLayoutCategories[] = {
  XPCONNECT_CATEGORIES
  { JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY, "Image", NS_HTMLIMGELEMENT_CONTRACTID },
  { JAVASCRIPT_GLOBAL_CONSTRUCTOR_PROTO_ALIAS_CATEGORY, "Image", "HTMLImageElement" },
  { JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY, "Option", NS_HTMLOPTIONELEMENT_CONTRACTID },
  { JAVASCRIPT_GLOBAL_CONSTRUCTOR_PROTO_ALIAS_CATEGORY, "Option", "HTMLOptionElement" },
#ifdef MOZ_MEDIA
  { JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY, "Audio", NS_HTMLAUDIOELEMENT_CONTRACTID },
  { JAVASCRIPT_GLOBAL_CONSTRUCTOR_PROTO_ALIAS_CATEGORY, "Audio", "HTMLAudioElement" },
#endif
  { "content-policy", NS_DATADOCUMENTCONTENTPOLICY_CONTRACTID, NS_DATADOCUMENTCONTENTPOLICY_CONTRACTID },
  { "content-policy", NS_NODATAPROTOCOLCONTENTPOLICY_CONTRACTID, NS_NODATAPROTOCOLCONTENTPOLICY_CONTRACTID },
  { "content-policy", "CSPService", CSPSERVICE_CONTRACTID },
  { "net-channel-event-sinks", "CSPService", CSPSERVICE_CONTRACTID },
  { JAVASCRIPT_GLOBAL_STATIC_NAMESET_CATEGORY, "PrivilegeManager", NS_SECURITYNAMESET_CONTRACTID },
  { "app-startup", "Script Security Manager", "service," NS_SCRIPTSECURITYMANAGER_CONTRACTID },
  CONTENTDLF_CATEGORIES
#ifdef MOZ_B2G_RIL
  { "profile-after-change", "Telephony System Worker Manager", SYSTEMWORKERMANAGER_CONTRACTID },
#endif
  { NULL }
};

static void
LayoutModuleDtor()
{
  nsScriptSecurityManager::Shutdown();
  xpcModuleDtor();
}

static const mozilla::Module kLayoutModule = {
  mozilla::Module::kVersion,
  kLayoutCIDs,
  kLayoutContracts,
  kLayoutCategories,
  NULL,
  Initialize,
  LayoutModuleDtor
};
  
NSMODULE_DEFN(nsLayoutModule) = &kLayoutModule;
