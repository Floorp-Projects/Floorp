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
#include "nsStructuredCloneContainer.h"

#include "nsIEventListenerService.h"
#include "nsIMessageManager.h"

// Transformiix stuff
#include "mozilla/dom/XPathEvaluator.h"
#include "txMozillaXSLTProcessor.h"
#include "txNodeSetAdaptor.h"

#include "mozilla/dom/DOMParser.h"
#include "nsDOMSerializer.h"
#include "nsXMLHttpRequest.h"
#include "nsChannelPolicy.h"

// view stuff
#include "nsContentCreatorFunctions.h"

// DOM includes
#include "nsDOMBlobBuilder.h"
#include "nsDOMFileReader.h"

#include "nsFormData.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsHostObjectURI.h"
#include "nsGlobalWindowCommands.h"
#include "nsIControllerCommandTable.h"
#include "nsJSProtocolHandler.h"
#include "nsScriptNameSpaceManager.h"
#include "nsIControllerContext.h"
#include "nsDOMScriptObjectFactory.h"
#include "DOMStorageManager.h"
#include "nsJSON.h"
#include "nsZipArchive.h"
#include "mozIApplicationClearPrivateDataParams.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Activity.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/EventSource.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "mozilla/dom/network/TCPSocketChild.h"
#include "mozilla/dom/network/TCPSocketParent.h"
#include "mozilla/dom/network/TCPServerSocketChild.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/OSFileConstants.h"
#include "mozilla/Services.h"

#ifdef MOZ_WEBSPEECH
#include "mozilla/dom/FakeSpeechRecognitionService.h"
#include "mozilla/dom/nsSynthVoiceRegistry.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "SystemWorkerManager.h"
using mozilla::dom::gonk::SystemWorkerManager;
#define SYSTEMWORKERMANAGER_CID \
  {0xd53b6524, 0x6ac3, 0x42b0, {0xae, 0xca, 0x62, 0xb3, 0xc4, 0xe5, 0x2b, 0x04}}
#define SYSTEMWORKERMANAGER_CONTRACTID \
  "@mozilla.org/telephony/system-worker-manager;1"
#endif

#ifdef MOZ_B2G_BT
#include "BluetoothService.h"
using mozilla::dom::bluetooth::BluetoothService;
#define BLUETOOTHSERVICE_CID \
  {0xa753b487, 0x3344, 0x4de4, {0xad, 0x5f, 0x06, 0x36, 0x76, 0xa7, 0xc1, 0x04}}
#define BLUETOOTHSERVICE_CONTRACTID \
  "@mozilla.org/bluetooth/service;1"
#endif

#ifdef MOZ_WIDGET_GONK
#include "AudioManager.h"
using mozilla::dom::gonk::AudioManager;
#include "nsVolumeService.h"
using mozilla::system::nsVolumeService;
#endif

#include "AudioChannelAgent.h"
using mozilla::dom::AudioChannelAgent;

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
#if defined(MOZ_WIDGET_ANDROID)
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
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLEditor)

#include "nsHTMLCanvasFrame.h"

#include "nsIDOMWebGLRenderingContext.h"

class nsIDocumentLoaderFactory;

#define PRODUCT_NAME "Gecko"

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

#include "nsGeolocation.h"
#include "nsDeviceSensors.h"
#ifdef MOZ_GAMEPAD
#include "mozilla/dom/GamepadService.h"
#endif
#include "nsCSPService.h"
#include "nsISmsService.h"
#include "nsIMmsService.h"
#include "nsIMobileMessageService.h"
#include "nsIMobileMessageDatabaseService.h"
#include "mozilla/dom/mobilemessage/MobileMessageService.h"
#include "mozilla/dom/mobilemessage/SmsServicesFactory.h"
#include "nsIPowerManagerService.h"
#include "nsIAlarmHalService.h"
#include "nsIMediaManager.h"
#include "nsMixedContentBlocker.h"

#include "AudioChannelService.h"

#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/alarm/AlarmHalService.h"
#include "mozilla/dom/time/TimeService.h"
#include "StreamingProtocolService.h"

#include "mozilla/dom/telephony/TelephonyFactory.h"
#include "nsITelephonyProvider.h"

#ifdef MOZ_WIDGET_GONK
#include "GonkGPSGeolocationProvider.h"
#endif
#include "MediaManager.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::mobilemessage;
using namespace mozilla::dom::telephony;
using mozilla::dom::alarm::AlarmHalService;
using mozilla::dom::indexedDB::IndexedDatabaseManager;
using mozilla::dom::power::PowerManagerService;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::TCPSocketChild;
using mozilla::dom::TCPSocketParent;
using mozilla::dom::TCPServerSocketChild;
using mozilla::dom::time::TimeService;
using mozilla::net::StreamingProtocolControllerService;

// Transformiix
/* 5d5d92cd-6bf8-11d9-bf4a-000a95dc234c */
#define TRANSFORMIIX_NODESET_CID \
{ 0x5d5d92cd, 0x6bf8, 0x11d9, { 0xbf, 0x4a, 0x0, 0x0a, 0x95, 0xdc, 0x23, 0x4c } }

#define TRANSFORMIIX_NODESET_CONTRACTID \
"@mozilla.org/transformiix-nodeset;1"

// Factory Constructor
NS_GENERIC_FACTORY_CONSTRUCTOR(txMozillaXSLTProcessor)
NS_GENERIC_FACTORY_CONSTRUCTOR(XPathEvaluator)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(txNodeSetAdaptor, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMSerializer)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsXMLHttpRequest, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(EventSource)
NS_GENERIC_FACTORY_CONSTRUCTOR(Activity)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDOMFileReader, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFormData)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlobProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMediaStreamProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMediaSourceProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontTableProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHostObjectURI)
NS_GENERIC_FACTORY_CONSTRUCTOR(DOMParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(Exception)
NS_GENERIC_FACTORY_CONSTRUCTOR(DOMSessionStorageManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(DOMLocalStorageManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsChannelPolicy)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(IndexedDatabaseManager,
                                         IndexedDatabaseManager::FactoryCreate)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(DOMRequestService,
                                         DOMRequestService::FactoryCreate)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(QuotaManager,
                                         QuotaManager::FactoryCreate)
#ifdef MOZ_WIDGET_GONK
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(SystemWorkerManager,
                                         SystemWorkerManager::FactoryCreate)
#endif
#ifdef MOZ_B2G_BT
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(BluetoothService,
                                         BluetoothService::FactoryCreate)
#endif

#ifdef MOZ_WEBSPEECH
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsSynthVoiceRegistry,
                                         nsSynthVoiceRegistry::GetInstanceForService)
#endif

#ifdef MOZ_WIDGET_GONK
NS_GENERIC_FACTORY_CONSTRUCTOR(AudioManager)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(AudioChannelAgent)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceSensors)

#ifndef MOZ_WIDGET_GONK
#if defined(ANDROID)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHapticFeedback)
#endif
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(ThirdPartyUtil, Init)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsISmsService,
                                         SmsServicesFactory::CreateSmsService)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIMmsService,
                                         SmsServicesFactory::CreateMmsService)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIMobileMessageService,
                                         MobileMessageService::GetInstance)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIMobileMessageDatabaseService,
                                         SmsServicesFactory::CreateMobileMessageDatabaseService)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIPowerManagerService,
                                         PowerManagerService::GetInstance)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIAlarmHalService,
                                         AlarmHalService::GetInstance)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsITimeService,
                                         TimeService::GetInstance)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIStreamingProtocolControllerService,
                                         StreamingProtocolControllerService::GetInstance)

#ifdef MOZ_GAMEPAD
using mozilla::dom::GamepadServiceTest;
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(GamepadServiceTest,
                                         GamepadServiceTest::CreateService)
#endif

#ifdef MOZ_WIDGET_GONK
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIGeolocationProvider,
                                         GonkGPSGeolocationProvider::GetSingleton)
// Since the nsVolumeService constructor calls into nsIPowerManagerService,
// we need it to be constructed sometime after nsIPowerManagerService.
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsVolumeService,
                                         nsVolumeService::GetSingleton)
#endif
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIMediaManagerService,
                                         MediaManager::GetInstance)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsITelephonyProvider,
                                         TelephonyFactory::CreateTelephonyProvider)

//-----------------------------------------------------------------------------

// Per bug 209804, it is necessary to observe the "xpcom-shutdown" event and
// perform shutdown of the layout modules at that time instead of waiting for
// our module destructor to run.  If we do not do this, then we risk holding
// references to objects in other component libraries that have already been
// shutdown (and possibly unloaded if 60709 is ever fixed).

class LayoutShutdownObserver MOZ_FINAL : public nsIObserver
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

  static_assert(sizeof(uintptr_t) == sizeof(void*),
                "Eeek! You'll need to adjust the size of uintptr_t to the "
                "size of a pointer on your platform.");

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

#ifdef DEBUG
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

nsresult NS_NewCanvasRenderingContextWebGL(nsIDOMWebGLRenderingContext** aResult);

nsresult NS_CreateFrameTraversal(nsIFrameTraversal** aResult);

nsresult NS_NewDomSelection(nsISelection** aResult);
nsresult NS_NewContentViewer(nsIContentViewer** aResult);
nsresult NS_NewGenRegularIterator(nsIContentIterator** aResult);
nsresult NS_NewGenSubtreeIterator(nsIContentIterator** aInstancePtrResult);
nsresult NS_NewContentDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult);
nsresult NS_NewHTMLCopyTextEncoder(nsIDocumentEncoder** aResult);
nsresult NS_NewTextEncoder(nsIDocumentEncoder** aResult);
nsresult NS_NewContentPolicy(nsIContentPolicy** aResult);

nsresult NS_NewEventListenerService(nsIEventListenerService** aResult);
nsresult NS_NewGlobalMessageManager(nsIMessageBroadcaster** aResult);
nsresult NS_NewParentProcessMessageManager(nsIMessageBroadcaster** aResult);
nsresult NS_NewChildProcessMessageManager(nsISyncMessageSender** aResult);

nsresult NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult);

#define MAKE_CTOR(ctor_, iface_, func_)                   \
static nsresult                                           \
ctor_(nsISupports* aOuter, REFNSIID aIID, void** aResult) \
{                                                         \
  *aResult = nullptr;                                      \
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

// As above, but expects
//   already_AddRefed<nsIFoo> NS_NewFoo();
// instead of
//   nsresult NS_NewFoo(nsIFoo**);
#define MAKE_CTOR2(ctor_, iface_, func_)                  \
static nsresult                                           \
ctor_(nsISupports* aOuter, REFNSIID aIID, void** aResult) \
{                                                         \
  *aResult = nullptr;                                      \
  if (aOuter) {                                           \
    return NS_ERROR_NO_AGGREGATION;                       \
  }                                                       \
  nsCOMPtr<iface_> inst = func_();                        \
  inst.forget(aResult);                                   \
  return NS_OK;                                           \
}

#ifdef DEBUG
MAKE_CTOR(CreateNewFrameUtil,             nsIFrameUtil,                NS_NewFrameUtil)
MAKE_CTOR(CreateNewLayoutDebugger,        nsILayoutDebugger,           NS_NewLayoutDebugger)
#endif

MAKE_CTOR(CreateNewFrameTraversal,      nsIFrameTraversal,      NS_CreateFrameTraversal)
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
MAKE_CTOR(CreateDOMBlob,                  nsISupports,                 nsDOMMultipartFile::NewBlob)
MAKE_CTOR(CreateDOMFile,                  nsISupports,                 nsDOMMultipartFile::NewFile)
MAKE_CTOR(CreateDOMSelection,             nsISelection,                NS_NewDomSelection)
MAKE_CTOR2(CreateContentIterator,         nsIContentIterator,          NS_NewContentIterator)
MAKE_CTOR2(CreatePreContentIterator,      nsIContentIterator,          NS_NewPreContentIterator)
MAKE_CTOR2(CreateSubtreeIterator,         nsIContentIterator,          NS_NewContentSubtreeIterator)
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
MAKE_CTOR(CreateContentDLF,               nsIDocumentLoaderFactory,    NS_NewContentDocumentLoaderFactory)
MAKE_CTOR(CreateEventListenerService,     nsIEventListenerService,     NS_NewEventListenerService)
MAKE_CTOR(CreateGlobalMessageManager,     nsIMessageBroadcaster,       NS_NewGlobalMessageManager)
MAKE_CTOR(CreateParentMessageManager,     nsIMessageBroadcaster,       NS_NewParentProcessMessageManager)
MAKE_CTOR(CreateChildMessageManager,      nsISyncMessageSender,        NS_NewChildProcessMessageManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDataDocumentContentPolicy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNoDataProtocolContentPolicy)
MAKE_CTOR(CreatePluginDocument,           nsIDocument,                 NS_NewPluginDocument)
MAKE_CTOR(CreateVideoDocument,            nsIDocument,                 NS_NewVideoDocument)
MAKE_CTOR(CreateFocusManager,             nsIFocusManager,      NS_NewFocusManager)

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
    *aResult = nullptr;                                                       \
    if (nullptr != aOuter) {                                                  \
        rv = NS_ERROR_NO_AGGREGATION;                                         \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    _InstanceClass * inst = new _InstanceClass();                             \
    if (nullptr == inst) {                                                    \
        rv = NS_ERROR_OUT_OF_MEMORY;                                          \
        return rv;                                                            \
    }                                                                         \
    rv = inst->QueryInterface(aIID, aResult);                                 \
                                                                              \
    return rv;                                                                \
}                                                                             \

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMScriptObjectFactory)

#define NS_GEOLOCATION_CID \
  { 0x1E1C3FF, 0x94A, 0xD048, { 0x44, 0xB4, 0x62, 0xD2, 0x9C, 0x7B, 0x4F, 0x39 } }

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(Geolocation, Init)

#define NS_GEOLOCATION_SERVICE_CID \
  { 0x404d02a, 0x1CA, 0xAAAB, { 0x47, 0x62, 0x94, 0x4b, 0x1b, 0xf2, 0xf7, 0xb5 } }

#define NS_AUDIOCHANNEL_SERVICE_CID \
  { 0xf712e983, 0x048a, 0x443f, { 0x88, 0x02, 0xfc, 0xc3, 0xd9, 0x27, 0xce, 0xac }}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsGeolocationService, nsGeolocationService::GetGeolocationService)

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(AudioChannelService, AudioChannelService::GetAudioChannelService)

#ifdef MOZ_WEBSPEECH
NS_GENERIC_FACTORY_CONSTRUCTOR(FakeSpeechRecognitionService)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(CSPService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMixedContentBlocker)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrincipal)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecurityNameSet)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsSystemPrincipal,
    nsScriptSecurityManager::SystemPrincipalSingletonConstructor)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNullPrincipal, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStructuredCloneContainer)

NS_GENERIC_FACTORY_CONSTRUCTOR(OSFileConstantsService)
NS_GENERIC_FACTORY_CONSTRUCTOR(TCPSocketChild)
NS_GENERIC_FACTORY_CONSTRUCTOR(TCPSocketParent)
NS_GENERIC_FACTORY_CONSTRUCTOR(TCPServerSocketChild)

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"

  MAKE_CTOR(CreateA11yService, nsIAccessibilityService, NS_GetAccessibilityService)
#endif

static nsresult
Construct_nsIScriptSecurityManager(nsISupports *aOuter, REFNSIID aIID,
                                   void **aResult)
{
    if (!aResult)
        return NS_ERROR_NULL_POINTER;
    *aResult = nullptr;
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
NS_DEFINE_NAMED_CID(NS_DOMMULTIPARTBLOB_CID);
NS_DEFINE_NAMED_CID(NS_DOMMULTIPARTFILE_CID);
NS_DEFINE_NAMED_CID(NS_DOMSELECTION_CID);
NS_DEFINE_NAMED_CID(NS_CONTENTITERATOR_CID);
NS_DEFINE_NAMED_CID(NS_PRECONTENTITERATOR_CID);
NS_DEFINE_NAMED_CID(NS_SUBTREEITERATOR_CID);
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
NS_DEFINE_NAMED_CID(NS_CONTENT_DOCUMENT_LOADER_FACTORY_CID);
NS_DEFINE_NAMED_CID(NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
NS_DEFINE_NAMED_CID(NS_JSPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_JSURI_CID);
NS_DEFINE_NAMED_CID(NS_WINDOWCOMMANDTABLE_CID);
NS_DEFINE_NAMED_CID(NS_WINDOWCONTROLLER_CID);
NS_DEFINE_NAMED_CID(NS_PLUGINDOCLOADERFACTORY_CID);
NS_DEFINE_NAMED_CID(NS_PLUGINDOCUMENT_CID);
NS_DEFINE_NAMED_CID(NS_VIDEODOCUMENT_CID);
NS_DEFINE_NAMED_CID(NS_STYLESHEETSERVICE_CID);
NS_DEFINE_NAMED_CID(TRANSFORMIIX_XSLT_PROCESSOR_CID);
NS_DEFINE_NAMED_CID(TRANSFORMIIX_XPATH_EVALUATOR_CID);
NS_DEFINE_NAMED_CID(TRANSFORMIIX_NODESET_CID);
NS_DEFINE_NAMED_CID(NS_XMLSERIALIZER_CID);
NS_DEFINE_NAMED_CID(NS_FILEREADER_CID);
NS_DEFINE_NAMED_CID(NS_FORMDATA_CID);
NS_DEFINE_NAMED_CID(NS_BLOBPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_MEDIASTREAMPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_MEDIASOURCEPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_FONTTABLEPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_HOSTOBJECTURI_CID);
NS_DEFINE_NAMED_CID(NS_XMLHTTPREQUEST_CID);
NS_DEFINE_NAMED_CID(NS_EVENTSOURCE_CID);
NS_DEFINE_NAMED_CID(NS_DOMACTIVITY_CID);
NS_DEFINE_NAMED_CID(NS_DOMPARSER_CID);
NS_DEFINE_NAMED_CID(NS_DOMSESSIONSTORAGEMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_DOMLOCALSTORAGEMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_DOMJSON_CID);
NS_DEFINE_NAMED_CID(NS_TEXTEDITOR_CID);
NS_DEFINE_NAMED_CID(INDEXEDDB_MANAGER_CID);
NS_DEFINE_NAMED_CID(DOMREQUEST_SERVICE_CID);
NS_DEFINE_NAMED_CID(QUOTA_MANAGER_CID);
#ifdef MOZ_WIDGET_GONK
NS_DEFINE_NAMED_CID(SYSTEMWORKERMANAGER_CID);
#endif
#ifdef MOZ_B2G_BT
NS_DEFINE_NAMED_CID(BLUETOOTHSERVICE_CID);
#endif
#ifdef MOZ_WIDGET_GONK
NS_DEFINE_NAMED_CID(NS_AUDIOMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_VOLUMESERVICE_CID);
#endif

NS_DEFINE_NAMED_CID(NS_AUDIOCHANNELAGENT_CID);

NS_DEFINE_NAMED_CID(NS_HTMLEDITOR_CID);
NS_DEFINE_NAMED_CID(NS_EDITORCONTROLLER_CID);
NS_DEFINE_NAMED_CID(NS_EDITINGCONTROLLER_CID);
NS_DEFINE_NAMED_CID(NS_EDITORCOMMANDTABLE_CID);
NS_DEFINE_NAMED_CID(NS_EDITINGCOMMANDTABLE_CID);
NS_DEFINE_NAMED_CID(NS_TEXTSERVICESDOCUMENT_CID);
NS_DEFINE_NAMED_CID(NS_GEOLOCATION_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_GEOLOCATION_CID);
NS_DEFINE_NAMED_CID(NS_AUDIOCHANNEL_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_FOCUSMANAGER_CID);
NS_DEFINE_NAMED_CID(CSPSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_MIXEDCONTENTBLOCKER_CID);
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
NS_DEFINE_NAMED_CID(NS_DEVICE_SENSORS_CID);

#ifndef MOZ_WIDGET_GONK
#if defined(ANDROID)
NS_DEFINE_NAMED_CID(NS_HAPTICFEEDBACK_CID);
#endif
#endif
NS_DEFINE_NAMED_CID(SMS_SERVICE_CID);
NS_DEFINE_NAMED_CID(MMS_SERVICE_CID);
NS_DEFINE_NAMED_CID(MOBILE_MESSAGE_SERVICE_CID);
NS_DEFINE_NAMED_CID(MOBILE_MESSAGE_DATABASE_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_POWERMANAGERSERVICE_CID);
NS_DEFINE_NAMED_CID(OSFILECONSTANTSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_ALARMHALSERVICE_CID);
NS_DEFINE_NAMED_CID(TCPSOCKETCHILD_CID);
NS_DEFINE_NAMED_CID(TCPSOCKETPARENT_CID);
NS_DEFINE_NAMED_CID(TCPSERVERSOCKETCHILD_CID);
NS_DEFINE_NAMED_CID(NS_TIMESERVICE_CID);
NS_DEFINE_NAMED_CID(NS_MEDIASTREAMCONTROLLERSERVICE_CID);
#ifdef MOZ_WIDGET_GONK
NS_DEFINE_NAMED_CID(GONK_GPS_GEOLOCATION_PROVIDER_CID);
#endif
NS_DEFINE_NAMED_CID(NS_MEDIAMANAGERSERVICE_CID);
#ifdef MOZ_GAMEPAD
NS_DEFINE_NAMED_CID(NS_GAMEPAD_TEST_CID);
#endif
#ifdef MOZ_WEBSPEECH
NS_DEFINE_NAMED_CID(NS_FAKE_SPEECH_RECOGNITION_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_SYNTHVOICEREGISTRY_CID);
#endif

#ifdef ACCESSIBILITY
NS_DEFINE_NAMED_CID(NS_ACCESSIBILITY_SERVICE_CID);
#endif
NS_DEFINE_NAMED_CID(TELEPHONY_PROVIDER_CID);

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
  { &kNS_FRAME_UTIL_CID, false, nullptr, CreateNewFrameUtil },
  { &kNS_LAYOUT_DEBUGGER_CID, false, nullptr, CreateNewLayoutDebugger },
#endif
  { &kNS_FRAMETRAVERSAL_CID, false, nullptr, CreateNewFrameTraversal },
  { &kNS_BOXOBJECT_CID, false, nullptr, CreateNewBoxObject },
#ifdef MOZ_XUL
  { &kNS_LISTBOXOBJECT_CID, false, nullptr, CreateNewListBoxObject },
  { &kNS_MENUBOXOBJECT_CID, false, nullptr, CreateNewMenuBoxObject },
  { &kNS_POPUPBOXOBJECT_CID, false, nullptr, CreateNewPopupBoxObject },
  { &kNS_CONTAINERBOXOBJECT_CID, false, nullptr, CreateNewContainerBoxObject },
  { &kNS_SCROLLBOXOBJECT_CID, false, nullptr, CreateNewScrollBoxObject },
  { &kNS_TREEBOXOBJECT_CID, false, nullptr, CreateNewTreeBoxObject },
#endif // MOZ_XUL
#ifdef MOZ_XUL
  { &kIN_DOMVIEW_CID, false, nullptr, inDOMViewConstructor },
#endif
  { &kIN_DEEPTREEWALKER_CID, false, nullptr, inDeepTreeWalkerConstructor },
  { &kIN_FLASHER_CID, false, nullptr, inFlasherConstructor },
  { &kIN_CSSVALUESEARCH_CID, false, nullptr, inCSSValueSearchConstructor },
  { &kIN_DOMUTILS_CID, false, nullptr, inDOMUtilsConstructor },
  { &kNS_NAMESPACEMANAGER_CID, false, nullptr, CreateNameSpaceManager },
  { &kNS_CONTENT_VIEWER_CID, false, nullptr, CreateContentViewer },
  { &kNS_HTMLDOCUMENT_CID, false, nullptr, CreateHTMLDocument },
  { &kNS_XMLDOCUMENT_CID, false, nullptr, CreateXMLDocument },
  { &kNS_SVGDOCUMENT_CID, false, nullptr, CreateSVGDocument },
  { &kNS_IMAGEDOCUMENT_CID, false, nullptr, CreateImageDocument },
  { &kNS_DOMMULTIPARTBLOB_CID, false, nullptr, CreateDOMBlob },
  { &kNS_DOMMULTIPARTFILE_CID, false, nullptr, CreateDOMFile },
  { &kNS_DOMSELECTION_CID, false, nullptr, CreateDOMSelection },
  { &kNS_CONTENTITERATOR_CID, false, nullptr, CreateContentIterator },
  { &kNS_PRECONTENTITERATOR_CID, false, nullptr, CreatePreContentIterator },
  { &kNS_SUBTREEITERATOR_CID, false, nullptr, CreateSubtreeIterator },
  { &kNS_CANVASRENDERINGCONTEXTWEBGL_CID, false, nullptr, CreateCanvasRenderingContextWebGL },
  { &kNS_TEXT_ENCODER_CID, false, nullptr, CreateTextEncoder },
  { &kNS_HTMLCOPY_TEXT_ENCODER_CID, false, nullptr, CreateHTMLCopyTextEncoder },
  { &kNS_XMLCONTENTSERIALIZER_CID, false, nullptr, CreateXMLContentSerializer },
  { &kNS_HTMLCONTENTSERIALIZER_CID, false, nullptr, CreateHTMLContentSerializer },
  { &kNS_XHTMLCONTENTSERIALIZER_CID, false, nullptr, CreateXHTMLContentSerializer },
  { &kNS_PLAINTEXTSERIALIZER_CID, false, nullptr, CreatePlainTextSerializer },
  { &kNS_PARSERUTILS_CID, false, nullptr, nsParserUtilsConstructor },
  { &kNS_SCRIPTABLEUNESCAPEHTML_CID, false, nullptr, nsParserUtilsConstructor },
  { &kNS_CONTENTPOLICY_CID, false, nullptr, CreateContentPolicy },
  { &kNS_DATADOCUMENTCONTENTPOLICY_CID, false, nullptr, nsDataDocumentContentPolicyConstructor },
  { &kNS_NODATAPROTOCOLCONTENTPOLICY_CID, false, nullptr, nsNoDataProtocolContentPolicyConstructor },
  { &kNS_XULCONTROLLERS_CID, false, nullptr, NS_NewXULControllers },
#ifdef MOZ_XUL
  { &kNS_XULSORTSERVICE_CID, false, nullptr, CreateXULSortService },
  { &kNS_XULTEMPLATEBUILDER_CID, false, nullptr, NS_NewXULContentBuilder },
  { &kNS_XULTREEBUILDER_CID, false, nullptr, NS_NewXULTreeBuilder },
  { &kNS_XULDOCUMENT_CID, false, nullptr, CreateXULDocument },
#endif
  { &kNS_CONTENT_DOCUMENT_LOADER_FACTORY_CID, false, nullptr, CreateContentDLF },
  { &kNS_DOM_SCRIPT_OBJECT_FACTORY_CID, false, nullptr, nsDOMScriptObjectFactoryConstructor },
  { &kNS_JSPROTOCOLHANDLER_CID, false, nullptr, nsJSProtocolHandler::Create },
  { &kNS_JSURI_CID, false, nullptr, nsJSURIConstructor },
  { &kNS_WINDOWCOMMANDTABLE_CID, false, nullptr, CreateWindowCommandTableConstructor },
  { &kNS_WINDOWCONTROLLER_CID, false, nullptr, CreateWindowControllerWithSingletonCommandTable },
  { &kNS_PLUGINDOCLOADERFACTORY_CID, false, nullptr, CreateContentDLF },
  { &kNS_PLUGINDOCUMENT_CID, false, nullptr, CreatePluginDocument },
  { &kNS_VIDEODOCUMENT_CID, false, nullptr, CreateVideoDocument },
  { &kNS_STYLESHEETSERVICE_CID, false, nullptr, nsStyleSheetServiceConstructor },
  { &kTRANSFORMIIX_XSLT_PROCESSOR_CID, false, nullptr, txMozillaXSLTProcessorConstructor },
  { &kTRANSFORMIIX_XPATH_EVALUATOR_CID, false, nullptr, XPathEvaluatorConstructor },
  { &kTRANSFORMIIX_NODESET_CID, false, nullptr, txNodeSetAdaptorConstructor },
  { &kNS_XMLSERIALIZER_CID, false, nullptr, nsDOMSerializerConstructor },
  { &kNS_FILEREADER_CID, false, nullptr, nsDOMFileReaderConstructor },
  { &kNS_FORMDATA_CID, false, nullptr, nsFormDataConstructor },
  { &kNS_BLOBPROTOCOLHANDLER_CID, false, nullptr, nsBlobProtocolHandlerConstructor },
  { &kNS_MEDIASTREAMPROTOCOLHANDLER_CID, false, nullptr, nsMediaStreamProtocolHandlerConstructor },
  { &kNS_MEDIASOURCEPROTOCOLHANDLER_CID, false, nullptr, nsMediaSourceProtocolHandlerConstructor },
  { &kNS_FONTTABLEPROTOCOLHANDLER_CID, false, nullptr, nsFontTableProtocolHandlerConstructor },
  { &kNS_HOSTOBJECTURI_CID, false, nullptr, nsHostObjectURIConstructor },
  { &kNS_XMLHTTPREQUEST_CID, false, nullptr, nsXMLHttpRequestConstructor },
  { &kNS_EVENTSOURCE_CID, false, nullptr, EventSourceConstructor },
  { &kNS_DOMACTIVITY_CID, false, nullptr, ActivityConstructor },
  { &kNS_DOMPARSER_CID, false, nullptr, DOMParserConstructor },
  { &kNS_XPCEXCEPTION_CID, false, nullptr, ExceptionConstructor },
  { &kNS_DOMSESSIONSTORAGEMANAGER_CID, false, nullptr, DOMSessionStorageManagerConstructor },
  { &kNS_DOMLOCALSTORAGEMANAGER_CID, false, nullptr, DOMLocalStorageManagerConstructor },
  { &kNS_DOMJSON_CID, false, nullptr, NS_NewJSON },
  { &kNS_TEXTEDITOR_CID, false, nullptr, nsPlaintextEditorConstructor },
  { &kINDEXEDDB_MANAGER_CID, false, nullptr, IndexedDatabaseManagerConstructor },
  { &kDOMREQUEST_SERVICE_CID, false, nullptr, DOMRequestServiceConstructor },
  { &kQUOTA_MANAGER_CID, false, nullptr, QuotaManagerConstructor },
#ifdef MOZ_WIDGET_GONK
  { &kSYSTEMWORKERMANAGER_CID, true, nullptr, SystemWorkerManagerConstructor },
#endif
#ifdef MOZ_B2G_BT
  { &kBLUETOOTHSERVICE_CID, true, nullptr, BluetoothServiceConstructor },
#endif
#ifdef MOZ_WIDGET_GONK
  { &kNS_AUDIOMANAGER_CID, true, nullptr, AudioManagerConstructor },
  { &kNS_VOLUMESERVICE_CID, true, nullptr, nsVolumeServiceConstructor },
#endif
  { &kNS_AUDIOCHANNELAGENT_CID, true, nullptr, AudioChannelAgentConstructor },
  { &kNS_HTMLEDITOR_CID, false, nullptr, nsHTMLEditorConstructor },
  { &kNS_EDITORCONTROLLER_CID, false, nullptr, nsEditorControllerConstructor },
  { &kNS_EDITINGCONTROLLER_CID, false, nullptr, nsEditingControllerConstructor },
  { &kNS_EDITORCOMMANDTABLE_CID, false, nullptr, nsEditorCommandTableConstructor },
  { &kNS_EDITINGCOMMANDTABLE_CID, false, nullptr, nsEditingCommandTableConstructor },
  { &kNS_TEXTSERVICESDOCUMENT_CID, false, nullptr, nsTextServicesDocumentConstructor },
  { &kNS_GEOLOCATION_SERVICE_CID, false, nullptr, nsGeolocationServiceConstructor },
  { &kNS_GEOLOCATION_CID, false, nullptr, GeolocationConstructor },
  { &kNS_AUDIOCHANNEL_SERVICE_CID, false, nullptr, AudioChannelServiceConstructor },
  { &kNS_FOCUSMANAGER_CID, false, nullptr, CreateFocusManager },
#ifdef MOZ_WEBSPEECH
  { &kNS_FAKE_SPEECH_RECOGNITION_SERVICE_CID, false, nullptr, FakeSpeechRecognitionServiceConstructor },
  { &kNS_SYNTHVOICEREGISTRY_CID, true, nullptr, nsSynthVoiceRegistryConstructor },
#endif
  { &kCSPSERVICE_CID, false, nullptr, CSPServiceConstructor },
  { &kNS_MIXEDCONTENTBLOCKER_CID, false, nullptr, nsMixedContentBlockerConstructor },
  { &kNS_EVENTLISTENERSERVICE_CID, false, nullptr, CreateEventListenerService },
  { &kNS_GLOBALMESSAGEMANAGER_CID, false, nullptr, CreateGlobalMessageManager },
  { &kNS_PARENTPROCESSMESSAGEMANAGER_CID, false, nullptr, CreateParentMessageManager },
  { &kNS_CHILDPROCESSMESSAGEMANAGER_CID, false, nullptr, CreateChildMessageManager },
  { &kNSCHANNELPOLICY_CID, false, nullptr, nsChannelPolicyConstructor },
  { &kNS_SCRIPTSECURITYMANAGER_CID, false, nullptr, Construct_nsIScriptSecurityManager },
  { &kNS_PRINCIPAL_CID, false, nullptr, nsPrincipalConstructor },
  { &kNS_SYSTEMPRINCIPAL_CID, false, nullptr, nsSystemPrincipalConstructor },
  { &kNS_NULLPRINCIPAL_CID, false, nullptr, nsNullPrincipalConstructor },
  { &kNS_SECURITYNAMESET_CID, false, nullptr, nsSecurityNameSetConstructor },
  { &kNS_DEVICE_SENSORS_CID, false, nullptr, nsDeviceSensorsConstructor },
#ifndef MOZ_WIDGET_GONK
#if defined(ANDROID)
  { &kNS_HAPTICFEEDBACK_CID, false, nullptr, nsHapticFeedbackConstructor },
#endif
#endif
  { &kTHIRDPARTYUTIL_CID, false, nullptr, ThirdPartyUtilConstructor },
  { &kNS_STRUCTUREDCLONECONTAINER_CID, false, nullptr, nsStructuredCloneContainerConstructor },
  { &kSMS_SERVICE_CID, false, nullptr, nsISmsServiceConstructor },
  { &kMMS_SERVICE_CID, false, nullptr, nsIMmsServiceConstructor },
  { &kMOBILE_MESSAGE_SERVICE_CID, false, nullptr, nsIMobileMessageServiceConstructor },
  { &kMOBILE_MESSAGE_DATABASE_SERVICE_CID, false, nullptr, nsIMobileMessageDatabaseServiceConstructor },
  { &kNS_POWERMANAGERSERVICE_CID, false, nullptr, nsIPowerManagerServiceConstructor },
  { &kOSFILECONSTANTSSERVICE_CID, true, nullptr, OSFileConstantsServiceConstructor },
  { &kNS_ALARMHALSERVICE_CID, false, nullptr, nsIAlarmHalServiceConstructor },
  { &kTCPSOCKETCHILD_CID, false, nullptr, TCPSocketChildConstructor },
  { &kTCPSOCKETPARENT_CID, false, nullptr, TCPSocketParentConstructor },
  { &kTCPSERVERSOCKETCHILD_CID, false, nullptr, TCPServerSocketChildConstructor },
  { &kNS_TIMESERVICE_CID, false, nullptr, nsITimeServiceConstructor },
  { &kNS_MEDIASTREAMCONTROLLERSERVICE_CID, false, nullptr, nsIStreamingProtocolControllerServiceConstructor },
#ifdef MOZ_WIDGET_GONK
  { &kGONK_GPS_GEOLOCATION_PROVIDER_CID, false, nullptr, nsIGeolocationProviderConstructor },
#endif
  { &kNS_MEDIAMANAGERSERVICE_CID, false, nullptr, nsIMediaManagerServiceConstructor },
#ifdef MOZ_GAMEPAD
  { &kNS_GAMEPAD_TEST_CID, false, nullptr, GamepadServiceTestConstructor },
#endif
#ifdef ACCESSIBILITY
  { &kNS_ACCESSIBILITY_SERVICE_CID, false, nullptr, CreateA11yService },
#endif
  { &kTELEPHONY_PROVIDER_CID, false, nullptr, nsITelephonyProviderConstructor },
  { nullptr }
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
  { NS_DOMMULTIPARTBLOB_CONTRACTID, &kNS_DOMMULTIPARTBLOB_CID },
  { NS_DOMMULTIPARTFILE_CONTRACTID, &kNS_DOMMULTIPARTFILE_CID },
  { "@mozilla.org/content/dom-selection;1", &kNS_DOMSELECTION_CID },
  { "@mozilla.org/content/post-content-iterator;1", &kNS_CONTENTITERATOR_CID },
  { "@mozilla.org/content/pre-content-iterator;1", &kNS_PRECONTENTITERATOR_CID },
  { "@mozilla.org/content/subtree-content-iterator;1", &kNS_SUBTREEITERATOR_CID },
  { "@mozilla.org/content/canvas-rendering-context;1?id=moz-webgl", &kNS_CANVASRENDERINGCONTEXTWEBGL_CID },
  { "@mozilla.org/content/canvas-rendering-context;1?id=experimental-webgl", &kNS_CANVASRENDERINGCONTEXTWEBGL_CID },
#ifdef MOZ_WEBGL_CONFORMANT
  { "@mozilla.org/content/canvas-rendering-context;1?id=webgl", &kNS_CANVASRENDERINGCONTEXTWEBGL_CID },
#endif
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
  { CONTENT_DLF_CONTRACTID, &kNS_CONTENT_DOCUMENT_LOADER_FACTORY_CID },
  { NS_JSPROTOCOLHANDLER_CONTRACTID, &kNS_JSPROTOCOLHANDLER_CID },
  { NS_WINDOWCONTROLLER_CONTRACTID, &kNS_WINDOWCONTROLLER_CID },
  { PLUGIN_DLF_CONTRACTID, &kNS_PLUGINDOCLOADERFACTORY_CID },
  { NS_STYLESHEETSERVICE_CONTRACTID, &kNS_STYLESHEETSERVICE_CID },
  { TRANSFORMIIX_XSLT_PROCESSOR_CONTRACTID, &kTRANSFORMIIX_XSLT_PROCESSOR_CID },
  { NS_XPATH_EVALUATOR_CONTRACTID, &kTRANSFORMIIX_XPATH_EVALUATOR_CID },
  { TRANSFORMIIX_NODESET_CONTRACTID, &kTRANSFORMIIX_NODESET_CID },
  { NS_XMLSERIALIZER_CONTRACTID, &kNS_XMLSERIALIZER_CID },
  { NS_FILEREADER_CONTRACTID, &kNS_FILEREADER_CID },
  { NS_FORMDATA_CONTRACTID, &kNS_FORMDATA_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX BLOBURI_SCHEME, &kNS_BLOBPROTOCOLHANDLER_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX MEDIASTREAMURI_SCHEME, &kNS_MEDIASTREAMPROTOCOLHANDLER_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX MEDIASOURCEURI_SCHEME, &kNS_MEDIASOURCEPROTOCOLHANDLER_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX FONTTABLEURI_SCHEME, &kNS_FONTTABLEPROTOCOLHANDLER_CID },
  { NS_XMLHTTPREQUEST_CONTRACTID, &kNS_XMLHTTPREQUEST_CID },
  { NS_EVENTSOURCE_CONTRACTID, &kNS_EVENTSOURCE_CID },
  { NS_DOMACTIVITY_CONTRACTID, &kNS_DOMACTIVITY_CID },
  { NS_DOMPARSER_CONTRACTID, &kNS_DOMPARSER_CID },
  { XPC_EXCEPTION_CONTRACTID, &kNS_XPCEXCEPTION_CID },
  { "@mozilla.org/dom/localStorage-manager;1", &kNS_DOMLOCALSTORAGEMANAGER_CID },
  // Keeping the old ContractID for backward compatibility
  { "@mozilla.org/dom/storagemanager;1", &kNS_DOMLOCALSTORAGEMANAGER_CID },
  { "@mozilla.org/dom/sessionStorage-manager;1", &kNS_DOMSESSIONSTORAGEMANAGER_CID },
  { "@mozilla.org/dom/json;1", &kNS_DOMJSON_CID },
  { "@mozilla.org/editor/texteditor;1", &kNS_TEXTEDITOR_CID },
  { INDEXEDDB_MANAGER_CONTRACTID, &kINDEXEDDB_MANAGER_CID },
  { DOMREQUEST_SERVICE_CONTRACTID, &kDOMREQUEST_SERVICE_CID },
  { QUOTA_MANAGER_CONTRACTID, &kQUOTA_MANAGER_CID },
#ifdef MOZ_WIDGET_GONK
  { SYSTEMWORKERMANAGER_CONTRACTID, &kSYSTEMWORKERMANAGER_CID },
#endif
#ifdef MOZ_B2G_BT
  { BLUETOOTHSERVICE_CONTRACTID, &kBLUETOOTHSERVICE_CID },
#endif
#ifdef MOZ_WIDGET_GONK
  { NS_AUDIOMANAGER_CONTRACTID, &kNS_AUDIOMANAGER_CID },
  { NS_VOLUMESERVICE_CONTRACTID, &kNS_VOLUMESERVICE_CID },
#endif
  { NS_AUDIOCHANNELAGENT_CONTRACTID, &kNS_AUDIOCHANNELAGENT_CID },
  { "@mozilla.org/editor/htmleditor;1", &kNS_HTMLEDITOR_CID },
  { "@mozilla.org/editor/editorcontroller;1", &kNS_EDITORCONTROLLER_CID },
  { "@mozilla.org/editor/editingcontroller;1", &kNS_EDITINGCONTROLLER_CID },
  { "@mozilla.org/textservices/textservicesdocument;1", &kNS_TEXTSERVICESDOCUMENT_CID },
  { "@mozilla.org/geolocation/service;1", &kNS_GEOLOCATION_SERVICE_CID },
  { "@mozilla.org/geolocation;1", &kNS_GEOLOCATION_CID },
  { "@mozilla.org/audiochannel/service;1", &kNS_AUDIOCHANNEL_SERVICE_CID },
  { "@mozilla.org/focus-manager;1", &kNS_FOCUSMANAGER_CID },
#ifdef MOZ_WEBSPEECH
  { NS_SPEECH_RECOGNITION_SERVICE_CONTRACTID_PREFIX "fake", &kNS_FAKE_SPEECH_RECOGNITION_SERVICE_CID },
  { NS_SYNTHVOICEREGISTRY_CONTRACTID, &kNS_SYNTHVOICEREGISTRY_CID },
#endif
  { CSPSERVICE_CONTRACTID, &kCSPSERVICE_CID },
  { NS_MIXEDCONTENTBLOCKER_CONTRACTID, &kNS_MIXEDCONTENTBLOCKER_CID },
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
#if defined(ANDROID)
  { "@mozilla.org/widget/hapticfeedback;1", &kNS_HAPTICFEEDBACK_CID },
#endif
#endif
  { THIRDPARTYUTIL_CONTRACTID, &kTHIRDPARTYUTIL_CID },
  { NS_STRUCTUREDCLONECONTAINER_CONTRACTID, &kNS_STRUCTUREDCLONECONTAINER_CID },
  { SMS_SERVICE_CONTRACTID, &kSMS_SERVICE_CID },
  { MMS_SERVICE_CONTRACTID, &kMMS_SERVICE_CID },
  { MOBILE_MESSAGE_SERVICE_CONTRACTID, &kMOBILE_MESSAGE_SERVICE_CID },
  { MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID, &kMOBILE_MESSAGE_DATABASE_SERVICE_CID },
  { POWERMANAGERSERVICE_CONTRACTID, &kNS_POWERMANAGERSERVICE_CID },
  { OSFILECONSTANTSSERVICE_CONTRACTID, &kOSFILECONSTANTSSERVICE_CID },
  { ALARMHALSERVICE_CONTRACTID, &kNS_ALARMHALSERVICE_CID },
  { "@mozilla.org/tcp-socket-child;1", &kTCPSOCKETCHILD_CID },
  { "@mozilla.org/tcp-socket-parent;1", &kTCPSOCKETPARENT_CID },
  { "@mozilla.org/tcp-server-socket-child;1", &kTCPSERVERSOCKETCHILD_CID },
  { TIMESERVICE_CONTRACTID, &kNS_TIMESERVICE_CID },
  { MEDIASTREAMCONTROLLERSERVICE_CONTRACTID, &kNS_MEDIASTREAMCONTROLLERSERVICE_CID },
#ifdef MOZ_WIDGET_GONK
  { GONK_GPS_GEOLOCATION_PROVIDER_CONTRACTID, &kGONK_GPS_GEOLOCATION_PROVIDER_CID },
#endif
#ifdef MOZ_GAMEPAD
  { NS_GAMEPAD_TEST_CONTRACTID, &kNS_GAMEPAD_TEST_CID },
#endif
  { MEDIAMANAGERSERVICE_CONTRACTID, &kNS_MEDIAMANAGERSERVICE_CID },
#ifdef ACCESSIBILITY
  { "@mozilla.org/accessibilityService;1", &kNS_ACCESSIBILITY_SERVICE_CID },
  { "@mozilla.org/accessibleRetrieval;1", &kNS_ACCESSIBILITY_SERVICE_CID },
#endif
  { TELEPHONY_PROVIDER_CONTRACTID, &kTELEPHONY_PROVIDER_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kLayoutCategories[] = {
  XPCONNECT_CATEGORIES
  { "content-policy", NS_DATADOCUMENTCONTENTPOLICY_CONTRACTID, NS_DATADOCUMENTCONTENTPOLICY_CONTRACTID },
  { "content-policy", NS_NODATAPROTOCOLCONTENTPOLICY_CONTRACTID, NS_NODATAPROTOCOLCONTENTPOLICY_CONTRACTID },
  { "content-policy", "CSPService", CSPSERVICE_CONTRACTID },
  { "content-policy", NS_MIXEDCONTENTBLOCKER_CONTRACTID, NS_MIXEDCONTENTBLOCKER_CONTRACTID },
  { "net-channel-event-sinks", "CSPService", CSPSERVICE_CONTRACTID },
  { JAVASCRIPT_GLOBAL_STATIC_NAMESET_CATEGORY, "PrivilegeManager", NS_SECURITYNAMESET_CONTRACTID },
  { "app-startup", "Script Security Manager", "service," NS_SCRIPTSECURITYMANAGER_CONTRACTID },
  { TOPIC_WEB_APP_CLEAR_DATA, "QuotaManager", "service," QUOTA_MANAGER_CONTRACTID },
#ifdef MOZ_WIDGET_GONK
  { "app-startup", "Volume Service", "service," NS_VOLUMESERVICE_CONTRACTID },
#endif
  CONTENTDLF_CATEGORIES
#ifdef MOZ_WIDGET_GONK
  { "profile-after-change", "Gonk System Worker Manager", SYSTEMWORKERMANAGER_CONTRACTID },
#endif
#ifdef MOZ_B2G_BT
  { "profile-after-change", "Bluetooth Service", BLUETOOTHSERVICE_CONTRACTID },
#endif
  { nullptr }
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
  nullptr,
  Initialize,
  LayoutModuleDtor
};

NSMODULE_DEFN(nsLayoutModule) = &kLayoutModule;
