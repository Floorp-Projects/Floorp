/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"
// On top because they include basictypes.h:
#include "mozilla/dom/SmsFilter.h"

#ifdef XP_WIN
#undef GetClassName
#endif

// JavaScript includes
#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsprvtd.h"    // we are using private JS typedefs...
#include "jsdbgapi.h"
#include "WrapperFactory.h"
#include "AccessCheck.h"
#include "XrayWrapper.h"

#include "xpcpublic.h"
#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "XPCQuickStubs.h"
#include "nsDOMQS.h"

#include "mozilla/dom/RegisterBindings.h"

#include "nscore.h"
#include "nsDOMClassInfo.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsIComponentRegistrar.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIXPConnect.h"
#include "nsIXPCSecurityManager.h"
#include "nsIStringBundle.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "xptcall.h"
#include "prprf.h"
#include "nsTArray.h"
#include "nsCSSValue.h"
#include "nsThreadUtils.h"
#include "nsDOMEventTargetHelper.h"

// General helper includes
#include "nsGlobalWindow.h"
#include "nsHistory.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsDOMWindowUtils.h"
#include "nsIDOMGlobalPropertyInitializer.h"
#include "mozilla/Preferences.h"
#include "nsLocation.h"
#include "mozilla/Attributes.h"
#include "mozilla/Telemetry.h"

// Window scriptable helper includes
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIScriptExternalNameSet.h"
#include "nsJSUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsScriptNameSpaceManager.h"
#include "nsIJSNativeInitializer.h"
#include "nsJSEnvironment.h"

// DOM base includes
#include "nsIDOMPluginArray.h"
#include "nsIDOMPlugin.h"
#include "nsIDOMMimeTypeArray.h"
#include "nsIDOMMimeType.h"
#include "nsIDOMLocation.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMJSWindow.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMHistory.h"
#include "nsIDOMMediaList.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMConstructor.h"

// DOM core includes
#include "nsError.h"
#include "nsIDOMDOMException.h"
#include "nsIDOMNode.h"
#include "nsIDOMDOMStringList.h"

// HTMLFormElement helper includes
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsHTMLDocument.h"

// Event related includes
#include "nsEventListenerManager.h"
#include "nsIDOMEventTarget.h"

// CSS related includes
#include "nsCSSRules.h"
#include "nsIDOMStyleSheet.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIDOMCSSRule.h"
#include "nsICSSRuleList.h"
#include "nsIDOMRect.h"
#include "nsDOMCSSAttrDeclaration.h"

// XBL related includes.
#include "nsXBLService.h"
#include "nsXBLBinding.h"
#include "nsBindingManager.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIDOMElement.h"
#include "nsStyleContext.h"
#include "nsAutoPtr.h"
#include "nsMemory.h"

// Tranformiix
#include "nsIDOMXPathEvaluator.h"
#include "nsIXSLTProcessor.h"
#include "nsIXSLTProcessorPrivate.h"

#include "nsXMLHttpRequest.h"
#include "nsIDOMSettingsManager.h"
#include "nsIDOMContactManager.h"
#include "nsIDOMPermissionSettings.h"
#include "nsIDOMApplicationRegistry.h"

// includes needed for the prototype chain interfaces
#include "nsIDOMNavigator.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMLinkStyle.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMCSSCharsetRule.h"
#include "nsIDOMCSSImportRule.h"
#include "nsIDOMCSSMediaRule.h"
#include "nsIDOMCSSFontFaceRule.h"
#include "nsIDOMCSSMozDocumentRule.h"
#include "nsIDOMCSSSupportsRule.h"
#include "nsIDOMMozCSSKeyframeRule.h"
#include "nsIDOMMozCSSKeyframesRule.h"
#include "nsIDOMCSSPageRule.h"
#include "nsIDOMCSSStyleRule.h"
#include "nsIDOMCSSStyleSheet.h"
#define MOZ_GENERATED_EVENTS_INCLUDES
#include "GeneratedEvents.h"
#undef MOZ_GENERATED_EVENTS_INCLUDES
#include "nsIDOMDeviceMotionEvent.h" //nsIDOMDeviceAcceleration
#include "nsIDOMXULCommandDispatcher.h"
#ifndef MOZ_DISABLE_CRYPTOLEGACY
#include "nsIDOMCRMFObject.h"
#include "nsIDOMCryptoLegacy.h"
#else
#include "nsIDOMCrypto.h"
#endif
#include "nsIControllers.h"
#include "nsISelection.h"
#include "nsIBoxObject.h"
#ifdef MOZ_XUL
#include "nsITreeSelection.h"
#include "nsITreeContentView.h"
#include "nsITreeView.h"
#include "nsIXULTemplateBuilder.h"
#include "nsTreeColumns.h"
#endif
#include "nsIDOMXPathExpression.h"
#include "nsIDOMNSXPathExpression.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMMozBrowserFrame.h"

#include "nsIDOMSVGAnimatedEnum.h"
#include "nsIDOMSVGAnimatedInteger.h"
#include "nsIDOMSVGAnimatedNumber.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGNumber.h"

// Storage includes
#include "DOMStorage.h"

// Drag and drop
#include "nsIDOMDataTransfer.h"

// Geolocation
#include "nsIDOMGeoPositionCoords.h"

// User media
#ifdef MOZ_MEDIA_NAVIGATOR
#include "nsIDOMNavigatorUserMedia.h"
#endif

// Workers
#include "mozilla/dom/workers/Workers.h"

#include "nsDOMFile.h"

#include "nsIDOMNavigatorDesktopNotification.h"
#include "nsIDOMNavigatorDeviceStorage.h"
#include "nsIDOMNavigatorGeolocation.h"
#include "Navigator.h"

#include "nsPluginArray.h"
#include "nsMimeTypeArray.h"

#include "nsIEventListenerService.h"
#include "nsIMessageManager.h"
#include "mozilla/dom/Element.h"
#include "HTMLLegendElement.h"

#include "mozilla/dom/indexedDB/IDBWrapperCache.h"
#include "mozilla/dom/indexedDB/IDBFileHandle.h"
#include "mozilla/dom/indexedDB/IDBRequest.h"
#include "mozilla/dom/indexedDB/IDBDatabase.h"
#include "mozilla/dom/indexedDB/IDBEvents.h"
#include "mozilla/dom/indexedDB/IDBObjectStore.h"
#include "mozilla/dom/indexedDB/IDBTransaction.h"
#include "mozilla/dom/indexedDB/IDBCursor.h"
#include "mozilla/dom/indexedDB/IDBKeyRange.h"
#include "mozilla/dom/indexedDB/IDBIndex.h"

using mozilla::dom::indexedDB::IDBWrapperCache;
using mozilla::dom::workers::ResolveWorkerClasses;

#include "nsIDOMMediaQueryList.h"

#include "mozilla/dom/Activity.h"

#include "nsDOMTouchEvent.h"

#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/HTMLCollectionBinding.h"

#include "BatteryManager.h"
#include "nsIDOMPowerManager.h"
#include "nsIDOMWakeLock.h"
#include "nsIDOMSmsManager.h"
#include "nsIDOMMobileMessageManager.h"
#include "nsIDOMMozSmsMessage.h"
#include "nsIDOMMozMmsMessage.h"
#include "nsIDOMSmsFilter.h"
#include "nsIDOMSmsSegmentInfo.h"
#include "nsIDOMMozMobileMessageThread.h"
#include "nsIDOMConnection.h"
#include "mozilla/dom/network/Utils.h"

#ifdef MOZ_B2G_RIL
#include "Telephony.h"
#include "TelephonyCall.h"
#include "nsIDOMMozVoicemail.h"
#include "nsIDOMIccManager.h"
#include "nsIDOMMozCellBroadcast.h"
#include "nsIDOMMobileConnection.h"
#endif // MOZ_B2G_RIL

#ifdef MOZ_B2G_FM
#include "FMRadio.h"
#endif

#ifdef MOZ_B2G_BT
#include "BluetoothManager.h"
#include "BluetoothAdapter.h"
#include "BluetoothDevice.h"
#endif

#include "nsIDOMNavigatorSystemMessages.h"
#include "DOMCameraManager.h"
#include "DOMCameraControl.h"
#include "DOMCameraCapabilities.h"
#include "nsIOpenWindowEventDetail.h"
#include "nsIAsyncScrollEventDetail.h"
#include "nsIDOMGlobalObjectConstructor.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "LockedFile.h"
#include "GeneratedEvents.h"
#include "nsDebug.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Likely.h"

#ifdef MOZ_TIME_MANAGER
#include "TimeManager.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

static NS_DEFINE_CID(kDOMSOF_CID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

static const char kDOMStringBundleURL[] =
  "chrome://global/locale/dom/dom.properties";

// NOTE: DEFAULT_SCRIPTABLE_FLAGS and DOM_DEFAULT_SCRIPTABLE_FLAGS
//       are defined in nsIDOMClassInfo.h.

#define WINDOW_SCRIPTABLE_FLAGS                                               \
 (nsIXPCScriptable::WANT_PRECREATE |                                          \
  nsIXPCScriptable::WANT_FINALIZE |                                           \
  nsIXPCScriptable::WANT_ENUMERATE |                                          \
  nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE |                               \
  nsIXPCScriptable::IS_GLOBAL_OBJECT |                                        \
  nsIXPCScriptable::WANT_OUTER_OBJECT)

#define NODE_SCRIPTABLE_FLAGS                                                 \
 ((DOM_DEFAULT_SCRIPTABLE_FLAGS |                                             \
   nsIXPCScriptable::WANT_ADDPROPERTY) &                                      \
  ~nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY)

// We need to let JavaScript QI elements to interfaces that are not in
// the classinfo since XBL can be used to dynamically implement new
// unknown interfaces on elements, accessibility relies on this being
// possible.

#define ELEMENT_SCRIPTABLE_FLAGS                                              \
  ((NODE_SCRIPTABLE_FLAGS & ~nsIXPCScriptable::CLASSINFO_INTERFACES_ONLY) |   \
   nsIXPCScriptable::WANT_POSTCREATE)

#define ARRAY_SCRIPTABLE_FLAGS                                                \
  (DOM_DEFAULT_SCRIPTABLE_FLAGS       |                                       \
   nsIXPCScriptable::WANT_GETPROPERTY |                                       \
   nsIXPCScriptable::WANT_ENUMERATE)

#define EVENTTARGET_SCRIPTABLE_FLAGS                                          \
  (DOM_DEFAULT_SCRIPTABLE_FLAGS       |                                       \
   nsIXPCScriptable::WANT_ADDPROPERTY)

#define IDBEVENTTARGET_SCRIPTABLE_FLAGS                                       \
  (EVENTTARGET_SCRIPTABLE_FLAGS)

#define DOMCLASSINFO_STANDARD_FLAGS                                           \
  (nsIClassInfo::MAIN_THREAD_ONLY |                                           \
   nsIClassInfo::DOM_OBJECT       |                                           \
   nsIClassInfo::SINGLETON_CLASSINFO)


#ifdef DEBUG
#define NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                \
    eDOMClassInfo_##_class##_id,
#else
#define NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                \
  // nothing
#endif

/**
 * To generate the bitmap for a class that we're sure doesn't implement any of
 * the interfaces in DOMCI_CASTABLE_INTERFACES.
 */
#define DOMCI_DATA_NO_CLASS(_dom_class)                                       \
const uint32_t kDOMClassInfo_##_dom_class##_interfaces =                      \
  0;

#ifndef MOZ_DISABLE_CRYPTOLEGACY
DOMCI_DATA_NO_CLASS(CRMFObject)
#endif
DOMCI_DATA_NO_CLASS(Crypto)

DOMCI_DATA_NO_CLASS(ContentFrameMessageManager)
DOMCI_DATA_NO_CLASS(ChromeMessageBroadcaster)
DOMCI_DATA_NO_CLASS(ChromeMessageSender)

DOMCI_DATA_NO_CLASS(DOMPrototype)
DOMCI_DATA_NO_CLASS(DOMConstructor)

#define NS_DEFINE_CLASSINFO_DATA_WITH_NAME(_class, _name, _helper,            \
                                           _flags)                            \
  { #_name,                                                                   \
    nullptr,                                                                   \
    { _helper::doCreate },                                                    \
    nullptr,                                                                   \
    nullptr,                                                                   \
    nullptr,                                                                   \
    _flags,                                                                   \
    true,                                                                  \
    0,                                                                        \
    false,                                                                 \
    false,                                                                 \
    NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                    \
  },

#define NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA_WITH_NAME(_class, _name,         \
                                                       _helper, _flags)       \
  { #_name,                                                                   \
    nullptr,                                                                   \
    { _helper::doCreate },                                                    \
    nullptr,                                                                   \
    nullptr,                                                                   \
    nullptr,                                                                   \
    _flags,                                                                   \
    true,                                                                  \
    0,                                                                        \
    true,                                                                  \
    false,                                                                 \
    NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                    \
  },

#define NS_DEFINE_CLASSINFO_DATA(_class, _helper, _flags)                     \
  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(_class, _class, _helper, _flags)

#define NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA(_class, _helper, _flags)          \
  NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA_WITH_NAME(_class, _class, _helper, \
                                                 _flags)

namespace {

class IDBEventTargetSH : public nsEventTargetSH
{
protected:
  IDBEventTargetSH(nsDOMClassInfoData* aData) : nsEventTargetSH(aData)
  { }

  virtual ~IDBEventTargetSH()
  { }

public:
  NS_IMETHOD PreCreate(nsISupports *aNativeObj, JSContext *aCx,
                       JSObject *aGlobalObj, JSObject **aParentObj);

  static nsIClassInfo *doCreate(nsDOMClassInfoData *aData)
  {
    return new IDBEventTargetSH(aData);
  }
};

} // anonymous namespace


// This list of NS_DEFINE_CLASSINFO_DATA macros is what gives the DOM
// classes their correct behavior when used through XPConnect. The
// arguments that are passed to NS_DEFINE_CLASSINFO_DATA are
//
// 1. Class name as it should appear in JavaScript, this name is also
//    used to find the id of the class in nsDOMClassInfo
//    (i.e. e<classname>_id)
// 2. Scriptable helper class
// 3. nsIClassInfo/nsIXPCScriptable flags (i.e. for GetScriptableFlags)

static nsDOMClassInfoData sClassInfoData[] = {
  // Base classes

  // The Window class lets you QI into interfaces that are not in the
  // flattened set (i.e. nsIXPCScriptable::CLASSINFO_INTERFACES_ONLY
  // is not set), because of this make sure all scriptable interfaces
  // that are implemented by nsGlobalWindow can securely be exposed
  // to JS.


  NS_DEFINE_CLASSINFO_DATA(Window, nsWindowSH,
                           DEFAULT_SCRIPTABLE_FLAGS |
                           WINDOW_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(Location, nsLocationSH,
                           ((DOM_DEFAULT_SCRIPTABLE_FLAGS |
                             nsIXPCScriptable::WANT_ADDPROPERTY) &
                            ~nsIXPCScriptable::ALLOW_PROP_MODS_TO_PROTOTYPE))

  NS_DEFINE_CLASSINFO_DATA(Navigator, nsNavigatorSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_PRECREATE |
                           nsIXPCScriptable::WANT_NEWRESOLVE)
  NS_DEFINE_CLASSINFO_DATA(Plugin, nsPluginSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(PluginArray, nsPluginArraySH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MimeType, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MimeTypeArray, nsMimeTypeArraySH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(History, nsHistorySH,
                           ARRAY_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_PRECREATE)
  NS_DEFINE_CLASSINFO_DATA(DOMPrototype, nsDOMConstructorSH,
                           DOM_BASE_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_PRECREATE |
                           nsIXPCScriptable::WANT_NEWRESOLVE |
                           nsIXPCScriptable::WANT_HASINSTANCE |
                           nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE)
  NS_DEFINE_CLASSINFO_DATA(DOMConstructor, nsDOMConstructorSH,
                           DOM_BASE_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_PRECREATE |
                           nsIXPCScriptable::WANT_NEWRESOLVE |
                           nsIXPCScriptable::WANT_HASINSTANCE |
                           nsIXPCScriptable::WANT_CALL |
                           nsIXPCScriptable::WANT_CONSTRUCT |
                           nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE)

  // Core classes
  NS_DEFINE_CLASSINFO_DATA(DOMException, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Element, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)

  // Misc Core related classes

  // Event
  NS_DEFINE_CLASSINFO_DATA(Event, nsEventSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

#define MOZ_GENERATED_EVENT_LIST
#define MOZ_GENERATED_EVENT(_event_interface)             \
  NS_DEFINE_CLASSINFO_DATA(_event_interface, nsEventSH,   \
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
#include "GeneratedEvents.h"
#undef MOZ_GENERATED_EVENT_LIST

  NS_DEFINE_CLASSINFO_DATA(DeviceAcceleration, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DeviceRotationRate, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // HTML element classes
  NS_DEFINE_CLASSINFO_DATA(HTMLFormElement, nsHTMLFormElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_GETPROPERTY |
                           nsIXPCScriptable::WANT_NEWENUMERATE)

  // CSS classes
  NS_DEFINE_CLASSINFO_DATA(CSSStyleRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSCharsetRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSImportRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSMediaRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSNameSpaceRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSRuleList, nsCSSRuleListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSGroupRuleRuleList, nsCSSRuleListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MediaList, nsMediaListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(StyleSheetList, nsStyleSheetListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSStyleSheet, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(Selection, nsDOMGenericSH,
                           DEFAULT_SCRIPTABLE_FLAGS)

  // XUL classes
#ifdef MOZ_XUL
  NS_DEFINE_CLASSINFO_DATA(XULCommandDispatcher, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif
  NS_DEFINE_CLASSINFO_DATA(XULControllers, nsNonDOMObjectSH,
                           DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(BoxObject, nsDOMGenericSH,
                           DEFAULT_SCRIPTABLE_FLAGS)
#ifdef MOZ_XUL
  NS_DEFINE_CLASSINFO_DATA(TreeSelection, nsDOMGenericSH,
                           DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(TreeContentView, nsDOMGenericSH,
                           DEFAULT_SCRIPTABLE_FLAGS)
#endif

  // Crypto classes
#ifndef MOZ_DISABLE_CRYPTOLEGACY
  NS_DEFINE_CLASSINFO_DATA(CRMFObject, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif
  NS_DEFINE_CLASSINFO_DATA(Crypto, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(CSSRect, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // DOM Chrome Window class.
  NS_DEFINE_CLASSINFO_DATA(ChromeWindow, nsWindowSH,
                           DEFAULT_SCRIPTABLE_FLAGS |
                           WINDOW_SCRIPTABLE_FLAGS)

#ifdef MOZ_XUL
  NS_DEFINE_CLASSINFO_DATA(XULTemplateBuilder, nsDOMGenericSH,
                           DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XULTreeBuilder, nsDOMGenericSH,
                           DEFAULT_SCRIPTABLE_FLAGS)
#endif

  NS_DEFINE_CLASSINFO_DATA(DOMStringList, nsStringListSH,
                           ARRAY_SCRIPTABLE_FLAGS)

#ifdef MOZ_XUL
  NS_DEFINE_CLASSINFO_DATA(TreeColumn, nsDOMGenericSH,
                           DEFAULT_SCRIPTABLE_FLAGS)
#endif

  NS_DEFINE_CLASSINFO_DATA(CSSMozDocumentRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(CSSSupportsRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // other SVG classes
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedEnumeration, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedInteger, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedNumber, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGLength, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGNumber, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozCanvasPrintState, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(WindowUtils, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XSLTProcessor, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XPathExpression, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(XPathNSResolver, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(XPathResult, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // WhatWG Storage

  // mrbkap says we don't need WANT_ADDPROPERTY on Storage objects
  // since a call to addProperty() is always followed by a call to
  // setProperty(), except in the case when a getter or setter is set
  // for a property. But we don't care about getters or setters here.
  NS_DEFINE_CLASSINFO_DATA(Storage, nsStorage2SH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_NEWRESOLVE |
                           nsIXPCScriptable::WANT_GETPROPERTY |
                           nsIXPCScriptable::WANT_SETPROPERTY |
                           nsIXPCScriptable::WANT_DELPROPERTY |
                           nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |
                           nsIXPCScriptable::WANT_NEWENUMERATE)

  NS_DEFINE_CLASSINFO_DATA(Blob, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(File, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(ModalContentWindow, nsWindowSH,
                           DEFAULT_SCRIPTABLE_FLAGS |
                           WINDOW_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(GeoPositionCoords, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozPowerManager, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozWakeLock, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsManager, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozMobileMessageManager, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsMessage, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozMmsMessage, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsFilter, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsSegmentInfo, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozMobileMessageThread, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozConnection, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

#ifdef MOZ_B2G_RIL
  NS_DEFINE_CLASSINFO_DATA(MozMobileConnection, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozCellBroadcast, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif

  NS_DEFINE_CLASSINFO_DATA(CSSFontFaceRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // data transfer for drag and drop
  NS_DEFINE_CLASSINFO_DATA(DataTransfer, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(EventListenerInfo, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA(ContentFrameMessageManager, nsEventTargetSH,
                                       DOM_DEFAULT_SCRIPTABLE_FLAGS |
                                       nsIXPCScriptable::IS_GLOBAL_OBJECT)
  NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA(ChromeMessageBroadcaster, nsDOMGenericSH,
                                       DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA(ChromeMessageSender, nsDOMGenericSH,
                                       DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(IDBFileHandle, FileHandle, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(IDBRequest, IDBEventTargetSH,
                           IDBEVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(IDBDatabase, IDBEventTargetSH,
                           IDBEVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(IDBObjectStore, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(IDBTransaction, IDBEventTargetSH,
                           IDBEVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(IDBCursor, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(IDBCursorWithValue, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(IDBKeyRange, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(IDBIndex, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(IDBOpenDBRequest, IDBEventTargetSH,
                           IDBEVENTTARGET_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(TouchList, nsDOMTouchListSH,
                           ARRAY_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozCSSKeyframeRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MozCSSKeyframesRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(CSSPageRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MediaQueryList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

#ifdef MOZ_B2G_RIL
  NS_DEFINE_CLASSINFO_DATA(Telephony, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(TelephonyCall, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MozVoicemail, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MozIccManager, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif

#ifdef MOZ_B2G_FM
  NS_DEFINE_CLASSINFO_DATA(FMRadio, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
#endif

#ifdef MOZ_B2G_BT
  NS_DEFINE_CLASSINFO_DATA(BluetoothManager, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(BluetoothAdapter, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(BluetoothDevice, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
#endif

  NS_DEFINE_CLASSINFO_DATA(CameraControl, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CameraCapabilities, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(OpenWindowEventDetail, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(AsyncScrollEventDetail, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(LockedFile, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSFontFeatureValuesRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
};

#define NS_DEFINE_CONTRACT_CTOR(_class, _contract_id)                           \
  static nsresult                                                               \
  _class##Ctor(nsISupports** aInstancePtrResult)                                \
  {                                                                             \
    nsresult rv = NS_OK;                                                        \
    nsCOMPtr<nsISupports> native = do_CreateInstance(_contract_id, &rv);        \
    native.forget(aInstancePtrResult);                                          \
    return rv;                                                                  \
  }

NS_DEFINE_CONTRACT_CTOR(XSLTProcessor,
                        "@mozilla.org/document-transformer;1?type=xslt")

#undef NS_DEFINE_CONTRACT_CTOR

#define NS_DEFINE_EVENT_CTOR(_class)                                 \
  static nsresult                                                    \
  NS_DOM##_class##Ctor(nsISupports** aInstancePtrResult)             \
  {                                                                  \
    nsIDOMEvent* e = nullptr;                                        \
    nsresult rv = NS_NewDOM##_class(&e, nullptr, nullptr, nullptr);  \
    *aInstancePtrResult = e;                                         \
    return rv;                                                       \
  }

#define MOZ_GENERATED_EVENT_LIST
#define MOZ_GENERATED_EVENT(_event_interface) \
  NS_DEFINE_EVENT_CTOR(_event_interface)
#include "GeneratedEvents.h"
#undef MOZ_GENERATED_EVENT_LIST

struct nsConstructorFuncMapData
{
  int32_t mDOMClassInfoID;
  nsDOMConstructorFunc mConstructorFunc;
};

#define NS_DEFINE_CONSTRUCTOR_FUNC_DATA(_class, _func)                        \
  { eDOMClassInfo_##_class##_id, _func },

#define NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(_class)   \
  { eDOMClassInfo_##_class##_id, NS_DOM##_class##Ctor },

static const nsConstructorFuncMapData kConstructorFuncMap[] =
{
  NS_DEFINE_CONSTRUCTOR_FUNC_DATA(Blob, nsDOMMultipartFile::NewBlob)
  NS_DEFINE_CONSTRUCTOR_FUNC_DATA(File, nsDOMMultipartFile::NewFile)
#define MOZ_GENERATED_EVENT_LIST
#define MOZ_GENERATED_EVENT(_event_interface) \
  NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(_event_interface)
#include "GeneratedEvents.h"
#undef MOZ_GENERATED_EVENT_LIST
  NS_DEFINE_CONSTRUCTOR_FUNC_DATA(MozSmsFilter, SmsFilter::NewSmsFilter)
  NS_DEFINE_CONSTRUCTOR_FUNC_DATA(XSLTProcessor, XSLTProcessorCtor)
};
#undef NS_DEFINE_CONSTRUCTOR_FUNC_DATA
#undef NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA

nsIXPConnect *nsDOMClassInfo::sXPConnect = nullptr;
nsIScriptSecurityManager *nsDOMClassInfo::sSecMan = nullptr;
bool nsDOMClassInfo::sIsInitialized = false;


jsid nsDOMClassInfo::sParent_id          = JSID_VOID;
jsid nsDOMClassInfo::sScrollbars_id      = JSID_VOID;
jsid nsDOMClassInfo::sLocation_id        = JSID_VOID;
jsid nsDOMClassInfo::sConstructor_id     = JSID_VOID;
jsid nsDOMClassInfo::s_content_id        = JSID_VOID;
jsid nsDOMClassInfo::sContent_id         = JSID_VOID;
jsid nsDOMClassInfo::sMenubar_id         = JSID_VOID;
jsid nsDOMClassInfo::sToolbar_id         = JSID_VOID;
jsid nsDOMClassInfo::sLocationbar_id     = JSID_VOID;
jsid nsDOMClassInfo::sPersonalbar_id     = JSID_VOID;
jsid nsDOMClassInfo::sStatusbar_id       = JSID_VOID;
jsid nsDOMClassInfo::sControllers_id     = JSID_VOID;
jsid nsDOMClassInfo::sLength_id          = JSID_VOID;
jsid nsDOMClassInfo::sScrollX_id         = JSID_VOID;
jsid nsDOMClassInfo::sScrollY_id         = JSID_VOID;
jsid nsDOMClassInfo::sScrollMaxX_id      = JSID_VOID;
jsid nsDOMClassInfo::sScrollMaxY_id      = JSID_VOID;
jsid nsDOMClassInfo::sItem_id            = JSID_VOID;
jsid nsDOMClassInfo::sNamedItem_id       = JSID_VOID;
jsid nsDOMClassInfo::sEnumerate_id       = JSID_VOID;
jsid nsDOMClassInfo::sNavigator_id       = JSID_VOID;
jsid nsDOMClassInfo::sTop_id             = JSID_VOID;
jsid nsDOMClassInfo::sDocument_id        = JSID_VOID;
jsid nsDOMClassInfo::sFrames_id          = JSID_VOID;
jsid nsDOMClassInfo::sSelf_id            = JSID_VOID;
jsid nsDOMClassInfo::sWrappedJSObject_id = JSID_VOID;
jsid nsDOMClassInfo::sURL_id             = JSID_VOID;
jsid nsDOMClassInfo::sOnload_id          = JSID_VOID;
jsid nsDOMClassInfo::sOnerror_id         = JSID_VOID;

static const JSClass *sObjectClass = nullptr;

/**
 * Set our JSClass pointer for the Object class
 */
static void
FindObjectClass(JSContext* cx, JSObject* aGlobalObject)
{
  NS_ASSERTION(!sObjectClass,
               "Double set of sObjectClass");
  JS::Rooted<JSObject*> obj(cx), proto(cx, aGlobalObject);
  do {
    obj = proto;
    js::GetObjectProto(cx, obj, &proto);
  } while (proto);

  sObjectClass = js::GetObjectJSClass(obj);
}

static inline JSString *
IdToString(JSContext *cx, jsid id)
{
  if (JSID_IS_STRING(id))
    return JSID_TO_STRING(id);
  jsval idval;
  if (!::JS_IdToValue(cx, id, &idval))
    return nullptr;
  return JS_ValueToString(cx, idval);
}

static inline nsresult
WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
           nsWrapperCache *cache, const nsIID* aIID, jsval *vp,
           nsIXPConnectJSObjectHolder** aHolder, bool aAllowWrapping)
{
  if (!native) {
    NS_ASSERTION(!aHolder || !*aHolder, "*aHolder should be null!");

    *vp = JSVAL_NULL;

    return NS_OK;
  }

  JSObject *wrapper = xpc_FastGetCachedWrapper(cache, scope, vp);
  if (wrapper) {
    return NS_OK;
  }

  return nsDOMClassInfo::XPConnect()->WrapNativeToJSVal(cx, scope, native,
                                                        cache, aIID,
                                                        aAllowWrapping, vp,
                                                        aHolder);
}

static inline nsresult
WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
           const nsIID* aIID, bool aAllowWrapping, jsval *vp,
           // If non-null aHolder will keep the jsval alive
           // while there's a ref to it
           nsIXPConnectJSObjectHolder** aHolder = nullptr)
{
  return WrapNative(cx, scope, native, nullptr, aIID, vp, aHolder,
                    aAllowWrapping);
}

// Same as the WrapNative above, but use these if aIID is nsISupports' IID.
static inline nsresult
WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
           bool aAllowWrapping, jsval *vp,
           // If non-null aHolder will keep the jsval alive
           // while there's a ref to it
           nsIXPConnectJSObjectHolder** aHolder = nullptr)
{
  return WrapNative(cx, scope, native, nullptr, nullptr, vp, aHolder,
                    aAllowWrapping);
}

static inline nsresult
WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
           nsWrapperCache *cache, bool aAllowWrapping, jsval *vp,
           // If non-null aHolder will keep the jsval alive
           // while there's a ref to it
           nsIXPConnectJSObjectHolder** aHolder = nullptr)
{
  return WrapNative(cx, scope, native, cache, nullptr, vp, aHolder,
                    aAllowWrapping);
}

// Used for cases where PreCreate needs to wrap the native parent, and the
// native parent is likely to have been wrapped already.  |native| must
// implement nsWrapperCache, and nativeWrapperCache must be |native|'s
// nsWrapperCache.
static inline nsresult
WrapNativeParent(JSContext *cx, JS::Handle<JSObject*> scope, nsISupports *native,
                 nsWrapperCache *nativeWrapperCache, JSObject **parentObj)
{
  // In the common case, |native| is a wrapper cache with an existing wrapper
#ifdef DEBUG
  nsWrapperCache* cache = nullptr;
  CallQueryInterface(native, &cache);
  NS_PRECONDITION(nativeWrapperCache &&
                  cache == nativeWrapperCache, "What happened here?");
#endif

  JS::Rooted<JSObject*> obj(cx, nativeWrapperCache->GetWrapper());
  if (obj) {
#ifdef DEBUG
    JS::Rooted<JS::Value> debugVal(cx);
    nsresult rv = WrapNative(cx, scope, native, nativeWrapperCache, false,
                             debugVal.address());
    NS_ASSERTION(NS_SUCCEEDED(rv) && JSVAL_TO_OBJECT(debugVal) == obj,
                 "Unexpected object in nsWrapperCache");
#endif
    *parentObj = obj;
    return NS_OK;
  }

  JS::Rooted<JS::Value> v(cx);
  nsresult rv = WrapNative(cx, scope, native, nativeWrapperCache, false, v.address());
  NS_ENSURE_SUCCESS(rv, rv);
  *parentObj = v.toObjectOrNull();
  return NS_OK;
}

template<class P>
static inline nsresult
WrapNativeParent(JSContext *cx, JS::Handle<JSObject*> scope, P *parent,
                 JSObject **parentObj)
{
  return WrapNativeParent(cx, scope, ToSupports(parent), parent, parentObj);
}

// Helper to handle torn-down inner windows.
static inline nsresult
SetParentToWindow(nsGlobalWindow *win, JSObject **parent)
{
  MOZ_ASSERT(win);
  MOZ_ASSERT(win->IsInnerWindow());
  *parent = win->FastGetGlobalJSObject();

  if (MOZ_UNLIKELY(!*parent)) {
    // The inner window has been torn down. The scope is dying, so don't create
    // any new wrappers.
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// static

nsISupports *
nsDOMClassInfo::GetNative(nsIXPConnectWrappedNative *wrapper, JSObject *obj)
{
  return wrapper ? wrapper->Native() : static_cast<nsISupports*>(js::GetObjectPrivate(obj));
}

nsresult
nsDOMClassInfo::DefineStaticJSVals(JSContext *cx)
{
#define SET_JSID_TO_STRING(_id, _cx, _str)                                    \
  if (JSString *str = ::JS_InternString(_cx, _str))                           \
      _id = INTERNED_STRING_TO_JSID(_cx, str);                                \
  else                                                                        \
      return NS_ERROR_OUT_OF_MEMORY;

  SET_JSID_TO_STRING(sParent_id,          cx, "parent");
  SET_JSID_TO_STRING(sScrollbars_id,      cx, "scrollbars");
  SET_JSID_TO_STRING(sLocation_id,        cx, "location");
  SET_JSID_TO_STRING(sConstructor_id,     cx, "constructor");
  SET_JSID_TO_STRING(s_content_id,        cx, "_content");
  SET_JSID_TO_STRING(sContent_id,         cx, "content");
  SET_JSID_TO_STRING(sMenubar_id,         cx, "menubar");
  SET_JSID_TO_STRING(sToolbar_id,         cx, "toolbar");
  SET_JSID_TO_STRING(sLocationbar_id,     cx, "locationbar");
  SET_JSID_TO_STRING(sPersonalbar_id,     cx, "personalbar");
  SET_JSID_TO_STRING(sStatusbar_id,       cx, "statusbar");
  SET_JSID_TO_STRING(sControllers_id,     cx, "controllers");
  SET_JSID_TO_STRING(sLength_id,          cx, "length");
  SET_JSID_TO_STRING(sScrollX_id,         cx, "scrollX");
  SET_JSID_TO_STRING(sScrollY_id,         cx, "scrollY");
  SET_JSID_TO_STRING(sScrollMaxX_id,      cx, "scrollMaxX");
  SET_JSID_TO_STRING(sScrollMaxY_id,      cx, "scrollMaxY");
  SET_JSID_TO_STRING(sItem_id,            cx, "item");
  SET_JSID_TO_STRING(sNamedItem_id,       cx, "namedItem");
  SET_JSID_TO_STRING(sEnumerate_id,       cx, "enumerateProperties");
  SET_JSID_TO_STRING(sNavigator_id,       cx, "navigator");
  SET_JSID_TO_STRING(sTop_id,             cx, "top");
  SET_JSID_TO_STRING(sDocument_id,        cx, "document");
  SET_JSID_TO_STRING(sFrames_id,          cx, "frames");
  SET_JSID_TO_STRING(sSelf_id,            cx, "self");
  SET_JSID_TO_STRING(sWrappedJSObject_id, cx, "wrappedJSObject");
  SET_JSID_TO_STRING(sURL_id,             cx, "URL");
  SET_JSID_TO_STRING(sOnload_id,          cx, "onload");
  SET_JSID_TO_STRING(sOnerror_id,         cx, "onerror");

  return NS_OK;
}

// static
bool
nsDOMClassInfo::ObjectIsNativeWrapper(JSContext* cx, JSObject* obj)
{
  return xpc::WrapperFactory::IsXrayWrapper(obj) &&
         xpc::AccessCheck::wrapperSubsumes(obj);
}

nsDOMClassInfo::nsDOMClassInfo(nsDOMClassInfoData* aData) : mData(aData)
{
}

nsDOMClassInfo::~nsDOMClassInfo()
{
  if (IS_EXTERNAL(mData->mCachedClassInfo)) {
    // Some compilers don't like delete'ing a const nsDOMClassInfo*
    nsDOMClassInfoData* data = const_cast<nsDOMClassInfoData*>(mData);
    delete static_cast<nsExternalDOMClassInfoData*>(data);
  }
}

NS_IMPL_ADDREF(nsDOMClassInfo)
NS_IMPL_RELEASE(nsDOMClassInfo)

NS_INTERFACE_MAP_BEGIN(nsDOMClassInfo)
  if (aIID.Equals(NS_GET_IID(nsXPCClassInfo)))
    foundInterface = static_cast<nsIClassInfo*>(
                                    static_cast<nsXPCClassInfo*>(this));
  else
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIClassInfo)
NS_INTERFACE_MAP_END


static JSClass sDOMConstructorProtoClass = {
  "DOM Constructor.prototype", 0,
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr
};


static const char *
CutPrefix(const char *aName) {
  static const char prefix_nsIDOM[] = "nsIDOM";
  static const char prefix_nsI[]    = "nsI";

  if (strncmp(aName, prefix_nsIDOM, sizeof(prefix_nsIDOM) - 1) == 0) {
    return aName + sizeof(prefix_nsIDOM) - 1;
  }

  if (strncmp(aName, prefix_nsI, sizeof(prefix_nsI) - 1) == 0) {
    return aName + sizeof(prefix_nsI) - 1;
  }

  return aName;
}

// static
nsresult
nsDOMClassInfo::RegisterClassProtos(int32_t aClassInfoID)
{
  nsScriptNameSpaceManager *nameSpaceManager =
    nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);
  bool found_old;

  const nsIID *primary_iid = sClassInfoData[aClassInfoID].mProtoChainInterface;

  if (!primary_iid || primary_iid == &NS_GET_IID(nsISupports)) {
    return NS_OK;
  }

  nsCOMPtr<nsIInterfaceInfoManager>
    iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
  NS_ENSURE_TRUE(iim, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIInterfaceInfo> if_info;
  bool first = true;

  iim->GetInfoForIID(primary_iid, getter_AddRefs(if_info));

  while (if_info) {
    const nsIID *iid = nullptr;

    if_info->GetIIDShared(&iid);
    NS_ENSURE_TRUE(iid, NS_ERROR_UNEXPECTED);

    if (iid->Equals(NS_GET_IID(nsISupports))) {
      break;
    }

    const char *name = nullptr;
    if_info->GetNameShared(&name);
    NS_ENSURE_TRUE(name, NS_ERROR_UNEXPECTED);

    nameSpaceManager->RegisterClassProto(CutPrefix(name), iid, &found_old);

    if (first) {
      first = false;
    } else if (found_old) {
      break;
    }

    nsCOMPtr<nsIInterfaceInfo> tmp(if_info);
    tmp->GetParent(getter_AddRefs(if_info));
  }

  return NS_OK;
}

// static
nsresult
nsDOMClassInfo::RegisterExternalClasses()
{
  nsScriptNameSpaceManager *nameSpaceManager =
    nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIComponentRegistrar> registrar;
  nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(registrar));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICategoryManager> cm =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISimpleEnumerator> e;
  rv = cm->EnumerateCategory(JAVASCRIPT_DOM_CLASS, getter_AddRefs(e));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString contractId;
  nsAutoCString categoryEntry;
  nsCOMPtr<nsISupports> entry;

  while (NS_SUCCEEDED(e->GetNext(getter_AddRefs(entry)))) {
    nsCOMPtr<nsISupportsCString> category(do_QueryInterface(entry));

    if (!category) {
      NS_WARNING("Category entry not an nsISupportsCString!");
      continue;
    }

    rv = category->GetData(categoryEntry);

    cm->GetCategoryEntry(JAVASCRIPT_DOM_CLASS, categoryEntry.get(),
                         getter_Copies(contractId));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCID *cid;
    rv = registrar->ContractIDToCID(contractId, &cid);
    if (NS_FAILED(rv)) {
      NS_WARNING("Bad contract id registered with the script namespace manager");
      continue;
    }

    rv = nameSpaceManager->RegisterExternalClassName(categoryEntry.get(), *cid);
    nsMemory::Free(cid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return nameSpaceManager->RegisterExternalInterfaces(true);
}

#define _DOM_CLASSINFO_MAP_BEGIN(_class, _ifptr, _has_class_if, _disabled)    \
  {                                                                           \
    nsDOMClassInfoData &d = sClassInfoData[eDOMClassInfo_##_class##_id];      \
    d.mProtoChainInterface = _ifptr;                                          \
    d.mHasClassInterface = _has_class_if;                                     \
    d.mInterfacesBitmap = kDOMClassInfo_##_class##_interfaces;                \
    d.mDisabled = _disabled;                                                  \
    static const nsIID *interface_list[] = {

#define DOM_CLASSINFO_MAP_BEGIN(_class, _interface)                           \
  _DOM_CLASSINFO_MAP_BEGIN(_class, &NS_GET_IID(_interface), true, false)

#define DOM_CLASSINFO_MAP_BEGIN_MAYBE_DISABLE(_class, _interface, _disable)   \
  _DOM_CLASSINFO_MAP_BEGIN(_class, &NS_GET_IID(_interface), true, _disable)

#define DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(_class, _interface)               \
  _DOM_CLASSINFO_MAP_BEGIN(_class, &NS_GET_IID(_interface), false, false)

#define DOM_CLASSINFO_MAP_ENTRY(_if)                                          \
      &NS_GET_IID(_if),

#define DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(_if, _cond)                       \
      (_cond) ? &NS_GET_IID(_if) : nullptr,

#define DOM_CLASSINFO_MAP_END                                                 \
      nullptr                                                                  \
    };                                                                        \
                                                                              \
    /* Compact the interface list */                                          \
    size_t count = ArrayLength(interface_list);                               \
    /* count is the number of array entries, which is one greater than the */ \
    /* number of interfaces due to the terminating null */                    \
    for (size_t i = 0; i < count - 1; ++i) {                                  \
      if (!interface_list[i]) {                                               \
        /* We are moving the element at index i+1 and successors, */          \
        /* so we must move only count - (i+1) elements total. */              \
        memmove(&interface_list[i], &interface_list[i+1],                     \
                sizeof(nsIID*) * (count - (i+1)));                            \
        /* Make sure to examine the new pointer we ended up with at this */   \
        /* slot, since it may be null too */                                  \
        --i;                                                                  \
        --count;                                                              \
      }                                                                       \
    }                                                                         \
                                                                              \
    d.mInterfaces = interface_list;                                           \
  }

#define DOM_CLASSINFO_DOCUMENT_MAP_ENTRIES                                    \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentXBL)                                \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)                                \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXPathEvaluator)                             \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeSelector)                               \
    DOM_CLASSINFO_MAP_ENTRY(nsIInlineEventHandlers)                           \
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsIDOMDocumentTouch,                  \
                                        nsDOMTouchEvent::PrefEnabled())


#define DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES                                \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElementCSSInlineStyle)                      \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)                                \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeSelector)                               \
    DOM_CLASSINFO_MAP_ENTRY(nsIInlineEventHandlers)                           \
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsITouchEventReceiver,                \
                                        nsDOMTouchEvent::PrefEnabled())

#define DOM_CLASSINFO_EVENT_MAP_ENTRIES                                       \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEvent)                                      \

#ifdef MOZ_B2G
#define DOM_CLASSINFO_WINDOW_MAP_ENTRIES(_support_indexed_db)                  \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindow)                                        \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowB2G)                                     \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMJSWindow)                                      \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)                                   \
  DOM_CLASSINFO_MAP_ENTRY(nsIInlineEventHandlers)                              \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowPerformance)                             \
  DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsIDOMStorageIndexedDB,                  \
                                      _support_indexed_db)                     \
  DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsITouchEventReceiver,                   \
                                      nsDOMTouchEvent::PrefEnabled())
#else // !MOZ_B2G
#define DOM_CLASSINFO_WINDOW_MAP_ENTRIES(_support_indexed_db)                  \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindow)                                        \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMJSWindow)                                      \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)                                   \
  DOM_CLASSINFO_MAP_ENTRY(nsIInlineEventHandlers)                              \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowPerformance)                             \
  DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsIDOMStorageIndexedDB,                  \
                                      _support_indexed_db)                     \
  DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsITouchEventReceiver,                   \
                                      nsDOMTouchEvent::PrefEnabled())
#endif // MOZ_B2G

nsresult
nsDOMClassInfo::Init()
{
  /* Errors that can trigger early returns are done first,
     otherwise nsDOMClassInfo is left in a half inited state. */
  MOZ_STATIC_ASSERT(sizeof(uintptr_t) == sizeof(void*),
                    "BAD! You'll need to adjust the size of uintptr_t to the "
                    "size of a pointer on your platform.");

  NS_ENSURE_TRUE(!sIsInitialized, NS_ERROR_ALREADY_INITIALIZED);

  nsScriptNameSpaceManager *nameSpaceManager = nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = CallGetService(nsIXPConnect::GetCID(), &sXPConnect);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPCFunctionThisTranslator> elt = new nsEventListenerThisTranslator();
  sXPConnect->SetFunctionThisTranslator(NS_GET_IID(nsIDOMEventListener), elt);

  nsCOMPtr<nsIScriptSecurityManager> sm =
    do_GetService("@mozilla.org/scriptsecuritymanager;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  sSecMan = sm;
  NS_ADDREF(sSecMan);

  AutoSafeJSContext cx;

  DOM_CLASSINFO_MAP_BEGIN(Window, nsIDOMWindow)
    DOM_CLASSINFO_WINDOW_MAP_ENTRIES(nsGlobalWindow::HasIndexedDBSupport())
#ifdef MOZ_WEBSPEECH
    DOM_CLASSINFO_MAP_ENTRY(nsISpeechSynthesisGetter)
#endif
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WindowUtils, nsIDOMWindowUtils)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowUtils)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Location, nsIDOMLocation)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLocation)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Navigator, nsIDOMNavigator)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigator)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigatorDeviceStorage)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigatorGeolocation)
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsIDOMNavigatorDesktopNotification,
                                        Navigator::HasDesktopNotificationSupport())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMClientInformation)
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsINavigatorBattery,
                                        battery::BatteryManager::HasSupport())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozNavigatorSms)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozNavigatorMobileMessage)
#ifdef MOZ_MEDIA_NAVIGATOR
    DOM_CLASSINFO_MAP_ENTRY(nsINavigatorUserMedia)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigatorUserMedia)
#endif
#ifdef MOZ_B2G_RIL
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigatorTelephony)
#endif
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsIDOMMozNavigatorNetwork,
                                        network::IsAPIEnabled())
#ifdef MOZ_B2G_RIL
    DOM_CLASSINFO_MAP_ENTRY(nsIMozNavigatorMobileConnection)
    DOM_CLASSINFO_MAP_ENTRY(nsIMozNavigatorCellBroadcast)
    DOM_CLASSINFO_MAP_ENTRY(nsIMozNavigatorVoicemail)
    DOM_CLASSINFO_MAP_ENTRY(nsIMozNavigatorIccManager)
#endif
#ifdef MOZ_B2G_BT
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigatorBluetooth)
#endif
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigatorCamera)
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsIDOMNavigatorSystemMessages,
                                        Activity::PrefEnabled())
#ifdef MOZ_TIME_MANAGER
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozNavigatorTime)
#endif
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
    DOM_CLASSINFO_MAP_ENTRY(nsIMozNavigatorAudioChannelManager)
#endif

  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Plugin, nsIDOMPlugin)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPlugin)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(PluginArray, nsIDOMPluginArray)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPluginArray)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MimeType, nsIDOMMimeType)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMimeType)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MimeTypeArray, nsIDOMMimeTypeArray)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMimeTypeArray)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(History, nsIDOMHistory)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHistory)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(DOMPrototype, nsIDOMDOMConstructor)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMConstructor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMConstructor, nsIDOMDOMConstructor)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMConstructor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMException, nsIDOMDOMException)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMException)
    DOM_CLASSINFO_MAP_ENTRY(nsIException)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Element, nsIDOMElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeSelector)
    DOM_CLASSINFO_MAP_ENTRY(nsIInlineEventHandlers)
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsITouchEventReceiver,
                                        nsDOMTouchEvent::PrefEnabled())
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Event, nsIDOMEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

#define MOZ_GENERATED_EVENT_LIST
#define MOZ_GENERATED_EVENT(_event_interface)                         \
  DOM_CLASSINFO_MAP_BEGIN(_event_interface, nsIDOM##_event_interface) \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM##_event_interface)                 \
    DOM_CLASSINFO_EVENT_MAP_ENTRIES                                   \
  DOM_CLASSINFO_MAP_END
#include "GeneratedEvents.h"
#undef MOZ_GENERATED_EVENT_LIST

  DOM_CLASSINFO_MAP_BEGIN(DeviceAcceleration, nsIDOMDeviceAcceleration)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDeviceAcceleration)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DeviceRotationRate, nsIDOMDeviceRotationRate)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDeviceRotationRate)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLFormElement, nsIDOMHTMLFormElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLFormElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSStyleRule, nsIDOMCSSStyleRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSStyleRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSCharsetRule, nsIDOMCSSCharsetRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSCharsetRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSImportRule, nsIDOMCSSImportRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSImportRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSMediaRule, nsIDOMCSSMediaRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSMediaRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(CSSNameSpaceRule, nsIDOMCSSRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSRuleList, nsIDOMCSSRuleList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSRuleList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(CSSGroupRuleRuleList, nsIDOMCSSRuleList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSRuleList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MediaList, nsIDOMMediaList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMediaList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(StyleSheetList, nsIDOMStyleSheetList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStyleSheetList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSStyleSheet, nsIDOMCSSStyleSheet)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSStyleSheet)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(CSSRect, nsIDOMRect)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMRect)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Selection, nsISelection)
    DOM_CLASSINFO_MAP_ENTRY(nsISelection)
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_XUL
  DOM_CLASSINFO_MAP_BEGIN(XULCommandDispatcher, nsIDOMXULCommandDispatcher)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULCommandDispatcher)
  DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XULControllers, nsIControllers)
    DOM_CLASSINFO_MAP_ENTRY(nsIControllers)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(BoxObject, nsIBoxObject)
    DOM_CLASSINFO_MAP_ENTRY(nsIBoxObject)
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_XUL
  DOM_CLASSINFO_MAP_BEGIN(TreeSelection, nsITreeSelection)
    DOM_CLASSINFO_MAP_ENTRY(nsITreeSelection)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(TreeContentView, nsITreeContentView)
    DOM_CLASSINFO_MAP_ENTRY(nsITreeContentView)
    DOM_CLASSINFO_MAP_ENTRY(nsITreeView)
  DOM_CLASSINFO_MAP_END
#endif

#ifndef MOZ_DISABLE_CRYPTOLEGACY
   DOM_CLASSINFO_MAP_BEGIN(CRMFObject, nsIDOMCRMFObject)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMCRMFObject)
   DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN(Crypto, nsIDOMCrypto)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCrypto)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ChromeWindow, nsIDOMWindow)
    DOM_CLASSINFO_WINDOW_MAP_ENTRIES(true)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMChromeWindow)
#ifdef MOZ_WEBSPEECH
    DOM_CLASSINFO_MAP_ENTRY(nsISpeechSynthesisGetter)
#endif
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_XUL
  DOM_CLASSINFO_MAP_BEGIN(XULTemplateBuilder, nsIXULTemplateBuilder)
    DOM_CLASSINFO_MAP_ENTRY(nsIXULTemplateBuilder)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XULTreeBuilder, nsIXULTreeBuilder)
    DOM_CLASSINFO_MAP_ENTRY(nsIXULTreeBuilder)
    DOM_CLASSINFO_MAP_ENTRY(nsIXULTemplateBuilder)
    DOM_CLASSINFO_MAP_ENTRY(nsITreeView)
  DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN(DOMStringList, nsIDOMDOMStringList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMStringList)
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_XUL
  DOM_CLASSINFO_MAP_BEGIN(TreeColumn, nsITreeColumn)
    DOM_CLASSINFO_MAP_ENTRY(nsITreeColumn)
  DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN(CSSMozDocumentRule, nsIDOMCSSMozDocumentRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSMozDocumentRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSSupportsRule, nsIDOMCSSSupportsRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSSupportsRule)
  DOM_CLASSINFO_MAP_END

  // The SVG document

  // other SVG classes
  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedEnumeration, nsIDOMSVGAnimatedEnumeration)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedEnumeration)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedInteger, nsIDOMSVGAnimatedInteger)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedInteger)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedNumber, nsIDOMSVGAnimatedNumber)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedNumber)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGLength, nsIDOMSVGLength)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLength)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGNumber, nsIDOMSVGNumber)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGNumber)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozCanvasPrintState, nsIDOMMozCanvasPrintState)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozCanvasPrintState)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XSLTProcessor, nsIXSLTProcessor)
    DOM_CLASSINFO_MAP_ENTRY(nsIXSLTProcessor)
    DOM_CLASSINFO_MAP_ENTRY(nsIXSLTProcessorPrivate)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XPathExpression, nsIDOMXPathExpression)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXPathExpression)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSXPathExpression)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XPathNSResolver, nsIDOMXPathNSResolver)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXPathNSResolver)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XPathResult, nsIDOMXPathResult)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXPathResult)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Storage, nsIDOMStorage)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorage)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Blob, nsIDOMBlob)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMBlob)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(File, nsIDOMFile)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMBlob)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMFile)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ModalContentWindow, nsIDOMWindow)
    DOM_CLASSINFO_WINDOW_MAP_ENTRIES(nsGlobalWindow::HasIndexedDBSupport())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMModalContentWindow)
#ifdef MOZ_WEBSPEECH
    DOM_CLASSINFO_MAP_ENTRY(nsISpeechSynthesisGetter)
#endif
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(GeoPositionCoords, nsIDOMGeoPositionCoords)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMGeoPositionCoords)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozPowerManager, nsIDOMMozPowerManager)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozPowerManager)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozWakeLock, nsIDOMMozWakeLock)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozWakeLock)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozSmsManager, nsIDOMMozSmsManager)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSmsManager)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozMobileMessageManager, nsIDOMMozMobileMessageManager)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozMobileMessageManager)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozSmsMessage, nsIDOMMozSmsMessage)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSmsMessage)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozMmsMessage, nsIDOMMozMmsMessage)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozMmsMessage)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozSmsFilter, nsIDOMMozSmsFilter)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSmsFilter)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozSmsSegmentInfo, nsIDOMMozSmsSegmentInfo)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSmsSegmentInfo)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozMobileMessageThread, nsIDOMMozMobileMessageThread)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozMobileMessageThread)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozConnection, nsIDOMMozConnection)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozConnection)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_B2G_RIL
  DOM_CLASSINFO_MAP_BEGIN(MozMobileConnection, nsIDOMMozMobileConnection)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozMobileConnection)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozCellBroadcast, nsIDOMMozCellBroadcast)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozCellBroadcast)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END
#endif // MOZ_B2G_RIL

  DOM_CLASSINFO_MAP_BEGIN(CSSFontFaceRule, nsIDOMCSSFontFaceRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSFontFaceRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DataTransfer, nsIDOMDataTransfer)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDataTransfer)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(EventListenerInfo, nsIEventListenerInfo)
    DOM_CLASSINFO_MAP_ENTRY(nsIEventListenerInfo)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ContentFrameMessageManager, nsISupports)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageListenerManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageSender)
    DOM_CLASSINFO_MAP_ENTRY(nsISyncMessageSender)
    DOM_CLASSINFO_MAP_ENTRY(nsIContentFrameMessageManager)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ChromeMessageBroadcaster, nsISupports)
    DOM_CLASSINFO_MAP_ENTRY(nsIFrameScriptLoader)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageListenerManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageBroadcaster)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ChromeMessageSender, nsISupports)
    DOM_CLASSINFO_MAP_ENTRY(nsIProcessChecker)
    DOM_CLASSINFO_MAP_ENTRY(nsIFrameScriptLoader)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageListenerManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageSender)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(IDBFileHandle, nsIDOMFileHandle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMFileHandle)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBFileHandle)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(IDBRequest, nsIIDBRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(IDBDatabase, nsIIDBDatabase)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBDatabase)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(IDBObjectStore, nsIIDBObjectStore)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBObjectStore)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(IDBTransaction, nsIIDBTransaction)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBTransaction)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(IDBCursor, nsIIDBCursor)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBCursor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(IDBCursorWithValue, nsIIDBCursorWithValue)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBCursor)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBCursorWithValue)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(IDBKeyRange, nsIIDBKeyRange)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBKeyRange)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(IDBIndex, nsIIDBIndex)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBIndex)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(IDBOpenDBRequest, nsIIDBOpenDBRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBOpenDBRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_MAYBE_DISABLE(TouchList, nsIDOMTouchList,
                                        !nsDOMTouchEvent::PrefEnabled())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMTouchList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozCSSKeyframeRule, nsIDOMMozCSSKeyframeRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozCSSKeyframeRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozCSSKeyframesRule, nsIDOMMozCSSKeyframesRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozCSSKeyframesRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSPageRule, nsIDOMCSSPageRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSPageRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MediaQueryList, nsIDOMMediaQueryList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMediaQueryList)
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_B2G_RIL
  DOM_CLASSINFO_MAP_BEGIN(Telephony, nsIDOMTelephony)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMTelephony)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(TelephonyCall, nsIDOMTelephonyCall)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMTelephonyCall)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozVoicemail, nsIDOMMozVoicemail)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozVoicemail)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozIccManager, nsIDOMMozIccManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozIccManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

#endif

#ifdef MOZ_B2G_FM
  DOM_CLASSINFO_MAP_BEGIN(FMRadio, nsIFMRadio)
    DOM_CLASSINFO_MAP_ENTRY(nsIFMRadio)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END
#endif

#ifdef MOZ_B2G_BT
  DOM_CLASSINFO_MAP_BEGIN(BluetoothManager, nsIDOMBluetoothManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMBluetoothManager)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(BluetoothAdapter, nsIDOMBluetoothAdapter)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMBluetoothAdapter)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(BluetoothDevice, nsIDOMBluetoothDevice)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMBluetoothDevice)
  DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN(CameraControl, nsICameraControl)
    DOM_CLASSINFO_MAP_ENTRY(nsICameraControl)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CameraCapabilities, nsICameraCapabilities)
    DOM_CLASSINFO_MAP_ENTRY(nsICameraCapabilities)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(OpenWindowEventDetail, nsIOpenWindowEventDetail)
    DOM_CLASSINFO_MAP_ENTRY(nsIOpenWindowEventDetail)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(AsyncScrollEventDetail, nsIAsyncScrollEventDetail)
    DOM_CLASSINFO_MAP_ENTRY(nsIAsyncScrollEventDetail)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(LockedFile, nsIDOMLockedFile)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLockedFile)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSFontFeatureValuesRule, nsIDOMCSSFontFeatureValuesRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSFontFeatureValuesRule)
  DOM_CLASSINFO_MAP_END

  MOZ_STATIC_ASSERT(MOZ_ARRAY_LENGTH(sClassInfoData) == eDOMClassInfoIDCount,
                    "The number of items in sClassInfoData doesn't match the "
                    "number of nsIDOMClassInfo ID's, this is bad! Fix it!");

#ifdef DEBUG
  for (size_t i = 0; i < eDOMClassInfoIDCount; i++) {
    if (!sClassInfoData[i].u.mConstructorFptr ||
        sClassInfoData[i].mDebugID != i) {
      MOZ_NOT_REACHED("Class info data out of sync, you forgot to update "
                      "nsDOMClassInfo.h and nsDOMClassInfo.cpp! Fix this, "
                      "mozilla will not work without this fixed!");
      return NS_ERROR_NOT_INITIALIZED;
    }
  }

  for (size_t i = 0; i < eDOMClassInfoIDCount; i++) {
    if (!sClassInfoData[i].mInterfaces) {
      MOZ_NOT_REACHED("Class info data without an interface list! Fix this, "
                      "mozilla will not work without this fixed!");

      return NS_ERROR_NOT_INITIALIZED;
     }
   }
#endif

  // Initialize static JSString's
  DefineStaticJSVals(cx);

  int32_t i;

  for (i = 0; i < eDOMClassInfoIDCount; ++i) {
    nsDOMClassInfoData& data = sClassInfoData[i];
    nameSpaceManager->RegisterClassName(data.mName, i, data.mChromeOnly,
                                        data.mDisabled, &data.mNameUTF16);
  }

  for (i = 0; i < eDOMClassInfoIDCount; ++i) {
    RegisterClassProtos(i);
  }

  RegisterExternalClasses();

  // Register new DOM bindings
  mozilla::dom::Register(nameSpaceManager);

  sIsInitialized = true;

  return NS_OK;
}

// static
int32_t
nsDOMClassInfo::GetArrayIndexFromId(JSContext *cx, JS::Handle<jsid> id, bool *aIsNumber)
{
  if (aIsNumber) {
    *aIsNumber = false;
  }

  int i;
  if (JSID_IS_INT(id)) {
      i = JSID_TO_INT(id);
  } else {
      jsval idval;
      double array_index;
      if (!::JS_IdToValue(cx, id, &idval) ||
          !::JS_ValueToNumber(cx, idval, &array_index) ||
          !::JS_DoubleIsInt32(array_index, &i)) {
        return -1;
      }
  }

  if (aIsNumber) {
    *aIsNumber = true;
  }

  return i;
}

NS_IMETHODIMP
nsDOMClassInfo::GetInterfaces(uint32_t *aCount, nsIID ***aArray)
{
  uint32_t count = 0;

  while (mData->mInterfaces[count]) {
    count++;
  }

  *aCount = count;

  if (!count) {
    *aArray = nullptr;

    return NS_OK;
  }

  *aArray = static_cast<nsIID **>(nsMemory::Alloc(count * sizeof(nsIID *)));
  NS_ENSURE_TRUE(*aArray, NS_ERROR_OUT_OF_MEMORY);

  uint32_t i;
  for (i = 0; i < count; i++) {
    nsIID *iid = static_cast<nsIID *>(nsMemory::Clone(mData->mInterfaces[i],
                                                         sizeof(nsIID)));

    if (!iid) {
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(i, *aArray);

      return NS_ERROR_OUT_OF_MEMORY;
    }

    *((*aArray) + i) = iid;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetHelperForLanguage(uint32_t language, nsISupports **_retval)
{
  if (language == nsIProgrammingLanguage::JAVASCRIPT) {
    *_retval = static_cast<nsIXPCScriptable *>(this);

    NS_ADDREF(*_retval);
  } else {
    *_retval = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetContractID(char **aContractID)
{
  *aContractID = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetClassDescription(char **aClassDescription)
{
  return GetClassName(aClassDescription);
}

NS_IMETHODIMP
nsDOMClassInfo::GetClassID(nsCID **aClassID)
{
  *aClassID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetClassIDNoAlloc(nsCID *aClassID)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMClassInfo::GetImplementationLanguage(uint32_t *aImplLanguage)
{
  *aImplLanguage = nsIProgrammingLanguage::CPLUSPLUS;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetFlags(uint32_t *aFlags)
{
  *aFlags = DOMCLASSINFO_STANDARD_FLAGS;

  return NS_OK;
}

// nsIXPCScriptable

NS_IMETHODIMP
nsDOMClassInfo::GetClassName(char **aClassName)
{
  *aClassName = NS_strdup(mData->mName);

  return NS_OK;
}

// virtual
uint32_t
nsDOMClassInfo::GetScriptableFlags()
{
  return mData->mScriptableFlags;
}

NS_IMETHODIMP
nsDOMClassInfo::PreCreate(nsISupports *nativeObj, JSContext *cx,
                          JSObject *globalObj, JSObject **parentObj)
{
  *parentObj = globalObj;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::Create(nsIXPConnectWrappedNative *wrapper,
                       JSContext *cx, JSObject *obj)
{
  NS_WARNING("nsDOMClassInfo::Create Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::PostCreate(nsIXPConnectWrappedNative *wrapper,
                           JSContext *cx, JSObject *obj)
{
  NS_WARNING("nsDOMClassInfo::PostCreate Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::PostTransplant(nsIXPConnectWrappedNative *wrapper,
                               JSContext *cx, JSObject *obj)
{
  MOZ_NOT_REACHED("nsDOMClassInfo::PostTransplant Don't call me!");
  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, jsval *vp,
                            bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::AddProperty Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::DelProperty Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, jsval *vp,
                            bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::GetProperty Don't call me!");

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, jsval *vp,
                            bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::SetProperty Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, bool *_retval)
{
#ifdef DEBUG
  if (!sSecMan) {
    NS_ERROR("No security manager!!!");
    return NS_OK;
  }

  // Ask the security manager if it's OK to enumerate
  nsresult rv =
    sSecMan->CheckPropertyAccess(cx, obj, mData->mName, sEnumerate_id,
                                 nsIXPCSecurityManager::ACCESS_GET_PROPERTY);

  NS_ASSERTION(NS_SUCCEEDED(rv),
               "XOWs should have stopped us from getting here!!!");
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                             JSContext *cx, JSObject *obj, uint32_t enum_op,
                             jsval *statep, jsid *idp, bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::NewEnumerate Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

nsresult
nsDOMClassInfo::ResolveConstructor(JSContext *cx, JSObject *aObj,
                                   JSObject **objp)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<JSObject*> global(cx, ::JS_GetGlobalForObject(cx, obj));

  JS::Rooted<JS::Value> val(cx);
  if (!::JS_LookupProperty(cx, global, mData->mName, val.address())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!JSVAL_IS_PRIMITIVE(val)) {
    // If val is not an (non-null) object there either is no
    // constructor for this class, or someone messed with
    // window.classname, just fall through and let the JS engine
    // return the Object constructor.

    if (!::JS_DefinePropertyById(cx, obj, sConstructor_id, val, JS_PropertyStub,
                                 JS_StrictPropertyStub, JSPROP_ENUMERATE)) {
      return NS_ERROR_UNEXPECTED;
    }

    *objp = obj;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                           JSObject *obj, jsid id, uint32_t flags,
                           JSObject **objp, bool *_retval)
{
  if (id == sConstructor_id) {
    return ResolveConstructor(cx, obj, objp);
  }

  return NS_OK;
}

nsISupports*
nsDOMTouchListSH::GetItemAt(nsISupports *aNative, uint32_t aIndex,
                            nsWrapperCache **aCache, nsresult *aResult)
{
  nsDOMTouchList* list = static_cast<nsDOMTouchList*>(aNative);
  return list->GetItemAt(aIndex);
}

NS_IMETHODIMP
nsDOMClassInfo::Convert(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, uint32_t type, jsval *vp,
                        bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::Convert Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Finalize(nsIXPConnectWrappedNative *wrapper, JSFreeOp *fop,
                         JSObject *obj)
{
  NS_WARNING("nsDOMClassInfo::Finalize Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::CheckAccess(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid aId, uint32_t mode,
                            jsval *vp, bool *_retval)
{
  JS::Rooted<jsid> id(cx, aId);
  uint32_t mode_type = mode & JSACC_TYPEMASK;

  if ((mode_type == JSACC_WATCH || mode_type == JSACC_PROTO) && sSecMan) {
    nsresult rv;
    JS::Rooted<JSObject*> real_obj(cx);
    if (wrapper) {
      real_obj = wrapper->GetJSObject();
      NS_ENSURE_STATE(real_obj);
    }
    else {
      real_obj = obj;
    }

    rv =
      sSecMan->CheckPropertyAccess(cx, real_obj, mData->mName, id,
                                   nsIXPCSecurityManager::ACCESS_GET_PROPERTY);

    if (NS_FAILED(rv)) {
      // Let XPConnect know that the access was not granted.
      *_retval = false;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JSObject *obj, const JS::CallArgs &args, bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::Call Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, const JS::CallArgs &args,
                          bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::Construct Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, const jsval &val, bool *bp,
                            bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::HasInstance Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::OuterObject(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                            JSObject * obj, JSObject * *_retval)
{
  NS_WARNING("nsDOMClassInfo::OuterObject Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

static nsresult
GetExternalClassInfo(nsScriptNameSpaceManager *aNameSpaceManager,
                     const nsString &aName,
                     const nsGlobalNameStruct *aStruct,
                     const nsGlobalNameStruct **aResult)
{
  NS_ASSERTION(aStruct->mType ==
                 nsGlobalNameStruct::eTypeExternalClassInfoCreator,
               "Wrong type!");

  nsresult rv;
  nsCOMPtr<nsIDOMCIExtension> creator(do_CreateInstance(aStruct->mCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMScriptObjectFactory> sof(do_GetService(kDOMSOF_CID));
  NS_ENSURE_TRUE(sof, NS_ERROR_FAILURE);

  rv = creator->RegisterDOMCI(NS_ConvertUTF16toUTF8(aName).get(), sof);
  NS_ENSURE_SUCCESS(rv, rv);

  const nsGlobalNameStruct *name_struct = aNameSpaceManager->LookupName(aName);
  if (name_struct &&
      name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    *aResult = name_struct;
  }
  else {
    NS_ERROR("Couldn't get the DOM ClassInfo data.");

    *aResult = nullptr;
  }

  return NS_OK;
}


static nsresult
ResolvePrototype(nsIXPConnect *aXPConnect, nsGlobalWindow *aWin, JSContext *cx,
                 JS::Handle<JSObject*> obj, const PRUnichar *name,
                 const nsDOMClassInfoData *ci_data,
                 const nsGlobalNameStruct *name_struct,
                 nsScriptNameSpaceManager *nameSpaceManager,
                 JSObject *dot_prototype, bool install, bool *did_resolve);


NS_IMETHODIMP
nsDOMClassInfo::PostCreatePrototype(JSContext * cx, JSObject * aProto)
{
  uint32_t flags = (mData->mScriptableFlags & DONT_ENUM_STATIC_PROPS)
                   ? 0
                   : JSPROP_ENUMERATE;

  uint32_t count = 0;
  while (mData->mInterfaces[count]) {
    count++;
  }

  JS::Rooted<JSObject*> proto(cx, aProto);
  if (!xpc::DOM_DefineQuickStubs(cx, proto, flags, count, mData->mInterfaces)) {
    JS_ClearPendingException(cx);
  }

  // This is called before any other location that requires
  // sObjectClass, so compute it here. We assume that nobody has had a
  // chance to monkey around with proto's prototype chain before this.
  if (!sObjectClass) {
    FindObjectClass(cx, proto);
    NS_ASSERTION(sObjectClass && !strcmp(sObjectClass->name, "Object"),
                 "Incorrect object class!");
  }

#ifdef DEBUG
    JS::Rooted<JSObject*> proto2(cx);
    JS_GetPrototype(cx, proto, proto2.address());
    NS_ASSERTION(proto2 && JS_GetClass(proto2) == sObjectClass,
                 "Hmm, somebody did something evil?");
#endif

#ifdef DEBUG
  if (mData->mHasClassInterface && mData->mProtoChainInterface &&
      mData->mProtoChainInterface != &NS_GET_IID(nsISupports)) {
    nsCOMPtr<nsIInterfaceInfoManager>
      iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));

    if (iim) {
      nsCOMPtr<nsIInterfaceInfo> if_info;
      iim->GetInfoForIID(mData->mProtoChainInterface,
                         getter_AddRefs(if_info));

      if (if_info) {
        nsXPIDLCString name;
        if_info->GetName(getter_Copies(name));
        NS_ASSERTION(nsCRT::strcmp(CutPrefix(name), mData->mName) == 0,
                     "Class name and proto chain interface name mismatch!");
      }
    }
  }
#endif

  // Make prototype delegation work correctly. Consider if a site sets
  // HTMLElement.prototype.foopy = function () { ... } Now, calling
  // document.body.foopy() needs to ensure that looking up foopy on
  // document.body's prototype will find the right function.
  JS::Rooted<JSObject*> global(cx, ::JS_GetGlobalForObject(cx, proto));

  // Only do this if the global object is a window.
  // XXX Is there a better way to check this?
  nsISupports *globalNative = XPConnect()->GetNativeOfWrapper(cx, global);
  nsCOMPtr<nsPIDOMWindow> piwin = do_QueryInterface(globalNative);
  if (!piwin) {
    return NS_OK;
  }

  nsGlobalWindow *win = nsGlobalWindow::FromSupports(globalNative);
  if (win->IsClosedOrClosing()) {
    return NS_OK;
  }

  // If the window is in a different compartment than the global object, then
  // it's likely that global is a sandbox object whose prototype is a window.
  // Don't do anything in this case.
  if (win->FastGetGlobalJSObject() &&
      js::GetObjectCompartment(global) != js::GetObjectCompartment(win->FastGetGlobalJSObject())) {
    return NS_OK;
  }

  if (win->IsOuterWindow()) {
    // XXXjst: Do security checks here when we remove the security
    // checks on the inner window.

    win = win->GetCurrentInnerWindowInternal();

    if (!win || !(global = win->GetGlobalJSObject()) ||
        win->IsClosedOrClosing()) {
      return NS_OK;
    }
  }

  // Don't overwrite a property set by content.
  JSBool found;
  if (!::JS_AlreadyHasOwnUCProperty(cx, global, reinterpret_cast<const jschar*>(mData->mNameUTF16),
                                    NS_strlen(mData->mNameUTF16), &found)) {
    return NS_ERROR_FAILURE;
  }

  nsScriptNameSpaceManager *nameSpaceManager =
    nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_OK);

  bool unused;
  return ResolvePrototype(sXPConnect, win, cx, global, mData->mNameUTF16,
                          mData, nullptr, nameSpaceManager, proto, !found,
                          &unused);
}

// static
nsIClassInfo *
NS_GetDOMClassInfoInstance(nsDOMClassInfoID aID)
{
  if (aID >= eDOMClassInfoIDCount) {
    NS_ERROR("Bad ID!");

    return nullptr;
  }

  if (!nsDOMClassInfo::sIsInitialized) {
    nsresult rv = nsDOMClassInfo::Init();

    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  if (!sClassInfoData[aID].mCachedClassInfo) {
    nsDOMClassInfoData& data = sClassInfoData[aID];

    data.mCachedClassInfo = data.u.mConstructorFptr(&data);
    NS_ENSURE_TRUE(data.mCachedClassInfo, nullptr);

    NS_ADDREF(data.mCachedClassInfo);
  }

  NS_ASSERTION(!IS_EXTERNAL(sClassInfoData[aID].mCachedClassInfo),
               "This is bad, internal class marked as external!");

  return sClassInfoData[aID].mCachedClassInfo;
}

// static
nsIClassInfo *
nsDOMClassInfo::GetClassInfoInstance(nsDOMClassInfoData* aData)
{
  NS_ASSERTION(IS_EXTERNAL(aData->mCachedClassInfo)
               || !aData->mCachedClassInfo,
               "This is bad, external class marked as internal!");

  if (!aData->mCachedClassInfo) {
    if (aData->u.mExternalConstructorFptr) {
      aData->mCachedClassInfo =
        aData->u.mExternalConstructorFptr(aData->mName);
    } else {
      aData->mCachedClassInfo = nsDOMGenericSH::doCreate(aData);
    }
    NS_ENSURE_TRUE(aData->mCachedClassInfo, nullptr);

    NS_ADDREF(aData->mCachedClassInfo);
    aData->mCachedClassInfo = MARK_EXTERNAL(aData->mCachedClassInfo);
  }

  return GET_CLEAN_CI_PTR(aData->mCachedClassInfo);
}


// static
void
nsDOMClassInfo::ShutDown()
{
  if (sClassInfoData[0].u.mConstructorFptr) {
    uint32_t i;

    for (i = 0; i < eDOMClassInfoIDCount; i++) {
      NS_IF_RELEASE(sClassInfoData[i].mCachedClassInfo);
    }
  }

  sParent_id          = JSID_VOID;
  sScrollbars_id      = JSID_VOID;
  sLocation_id        = JSID_VOID;
  sConstructor_id     = JSID_VOID;
  s_content_id        = JSID_VOID;
  sContent_id         = JSID_VOID;
  sMenubar_id         = JSID_VOID;
  sToolbar_id         = JSID_VOID;
  sLocationbar_id     = JSID_VOID;
  sPersonalbar_id     = JSID_VOID;
  sStatusbar_id       = JSID_VOID;
  sControllers_id     = JSID_VOID;
  sLength_id          = JSID_VOID;
  sScrollX_id         = JSID_VOID;
  sScrollY_id         = JSID_VOID;
  sScrollMaxX_id      = JSID_VOID;
  sScrollMaxY_id      = JSID_VOID;
  sItem_id            = JSID_VOID;
  sEnumerate_id       = JSID_VOID;
  sNavigator_id       = JSID_VOID;
  sTop_id             = JSID_VOID;
  sDocument_id        = JSID_VOID;
  sFrames_id          = JSID_VOID;
  sSelf_id            = JSID_VOID;
  sWrappedJSObject_id = JSID_VOID;
  sOnload_id          = JSID_VOID;
  sOnerror_id         = JSID_VOID;

  NS_IF_RELEASE(sXPConnect);
  NS_IF_RELEASE(sSecMan);
  sIsInitialized = false;
}

// Window helper

NS_IMETHODIMP
nsWindowSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                      JSObject *globalObj, JSObject **parentObj)
{
  // Normally ::PreCreate() is used to give XPConnect the parent
  // object for the object that's being wrapped, this parent object is
  // set as the parent of the wrapper and it's also used to find the
  // right scope for the object being wrapped. Now, in the case of the
  // global object the wrapper shouldn't have a parent but we supply
  // one here anyway (the global object itself) and this will be used
  // by XPConnect only to find the right scope, once the scope is
  // found XPConnect will find the existing wrapper (which always
  // exists since it's created on window construction), since an
  // existing wrapper is found the parent we supply here is ignored
  // after the wrapper is found.

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(nativeObj));
  NS_ASSERTION(sgo, "nativeObj not a global object!");

  nsGlobalWindow *win = nsGlobalWindow::FromSupports(nativeObj);
  NS_ASSERTION(win->IsInnerWindow(), "Should be inner window.");

  // We sometimes get a disconnected window during file api test. :-(
  if (!win->GetOuterWindowInternal())
    return NS_ERROR_FAILURE;

  // If we're bootstrapping, we don't have a JS object yet.
  if (win->GetOuterWindowInternal()->IsCreatingInnerWindow())
    return NS_OK;

  return SetParentToWindow(win, parentObj);
}

static JSClass sGlobalScopePolluterClass = {
  "Global Scope Polluter",
  JSCLASS_NEW_RESOLVE,
  JS_PropertyStub,
  JS_DeletePropertyStub,
  nsWindowSH::GlobalScopePolluterGetProperty,
  JS_StrictPropertyStub,
  JS_EnumerateStub,
  (JSResolveOp)nsWindowSH::GlobalScopePolluterNewResolve,
  JS_ConvertStub,
  nullptr
};


// static
JSBool
nsWindowSH::GlobalScopePolluterGetProperty(JSContext *cx, JSHandleObject obj,
                                           JSHandleId id, JS::MutableHandle<JS::Value> vp)
{
  // Someone is accessing a element by referencing its name/id in the
  // global scope, do a security check to make sure that's ok.

  nsresult rv =
    sSecMan->CheckPropertyAccess(cx, ::JS_GetGlobalForObject(cx, obj),
                                 "Window", id,
                                 nsIXPCSecurityManager::ACCESS_GET_PROPERTY);

  if (NS_FAILED(rv)) {
    // The security check failed. The security manager set a JS
    // exception for us.

    return JS_FALSE;
  }

  return JS_TRUE;
}

// Gets a subframe.
static JSBool
ChildWindowGetter(JSContext *cx, JSHandleObject obj, JSHandleId id,
                  JS::MutableHandle<JS::Value> vp)
{
  MOZ_ASSERT(JSID_IS_STRING(id));
  // Grab the native DOM window.
  vp.setUndefined();
  nsCOMPtr<nsISupports> winSupports =
    do_QueryInterface(nsDOMClassInfo::XPConnect()->GetNativeOfWrapper(cx, obj));
  if (!winSupports)
    return true;
  nsGlobalWindow *win = nsGlobalWindow::FromSupports(winSupports);

  // Find the child, if it exists.
  nsDependentJSString name(id);
  nsCOMPtr<nsIDOMWindow> child = win->GetChildWindow(name);
  if (!child)
    return true;

  // Wrap the child for JS.
  JS::Rooted<JS::Value> v(cx);
  nsresult rv = WrapNative(cx, JS_GetGlobalForScopeChain(cx), child,
                           /* aAllowWrapping = */ true, v.address());
  NS_ENSURE_SUCCESS(rv, false);
  vp.set(v);
  return true;
}

static nsHTMLDocument*
GetDocument(JSObject *obj)
{
  MOZ_ASSERT(js::GetObjectJSClass(obj) == &sHTMLDocumentAllClass);
  return static_cast<nsHTMLDocument*>(
    static_cast<nsINode*>(JS_GetPrivate(obj)));
}

// static
JSBool
nsWindowSH::GlobalScopePolluterNewResolve(JSContext *cx, JSHandleObject obj,
                                          JSHandleId id, unsigned flags,
                                          JS::MutableHandle<JSObject*> objp)
{
  if (!JSID_IS_STRING(id)) {
    // Nothing to do if we're resolving a non-string property.
    return JS_TRUE;
  }

  // Crash reports from the wild seem to get here during shutdown when there's
  // no more XPConnect singleton.
  nsIXPConnect *xpc = XPConnect();
  NS_ENSURE_TRUE(xpc, true);

  // Grab the DOM window.
  JSObject *global = JS_GetGlobalForObject(cx, obj);
  nsISupports *globalNative = xpc->GetNativeOfWrapper(cx, global);
  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(globalNative);
  MOZ_ASSERT(piWin);
  nsGlobalWindow* win = static_cast<nsGlobalWindow*>(piWin.get());

  if (win->GetLength() > 0) {
    nsDependentJSString name(id);
    nsCOMPtr<nsIDOMWindow> child_win = win->GetChildWindow(name);
    if (child_win) {
      // We found a subframe of the right name, so define the property
      // on the GSP. This property is a read-only accessor. Shadowing via
      // |var foo| in global scope is still allowed, since |var| only looks
      // up |own| properties. But unqualified shadowing will fail, per-spec.
      if (!JS_DefinePropertyById(cx, obj, id, JS::UndefinedValue(),
                                 ChildWindowGetter, JS_StrictPropertyStub,
                                 JSPROP_SHARED | JSPROP_ENUMERATE))
      {
        return false;
      }

      objp.set(obj);
      return true;
    }
  }

  JS::Rooted<JSObject*> proto(cx);
  if (!::JS_GetPrototype(cx, obj, proto.address())) {
    return JS_FALSE;
  }
  JSBool hasProp;

  if (!proto || !::JS_HasPropertyById(cx, proto, id, &hasProp) ||
      hasProp) {
    // No prototype, or the property exists on the prototype. Do
    // nothing.

    return JS_TRUE;
  }

  //
  // The rest of this function is for HTML documents only.
  //
  nsCOMPtr<nsIHTMLDocument> htmlDoc =
    do_QueryInterface(win->GetExtantDoc());
  if (!htmlDoc)
    return true;
  nsHTMLDocument *document = static_cast<nsHTMLDocument*>(htmlDoc.get());

  nsDependentJSString str(id);
  nsCOMPtr<nsISupports> result;
  nsWrapperCache *cache;
  {
    Element *element = document->GetElementById(str);
    result = element;
    cache = element;
  }

  if (!result) {
    result = document->ResolveName(str, &cache);
  }

  if (result) {
    JS::Rooted<JS::Value> v(cx);
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult rv = WrapNative(cx, obj, result, cache, true, v.address(),
                             getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, JS_FALSE);

    if (!JS_WrapValue(cx, v.address()) ||
        !JS_DefinePropertyById(cx, obj, id, v, JS_PropertyStub, JS_StrictPropertyStub, 0)) {
      return JS_FALSE;
    }

    objp.set(obj);
  }

  return JS_TRUE;
}

// static
JSBool
nsWindowSH::InvalidateGlobalScopePolluter(JSContext *cx,
                                          JS::Handle<JSObject*> aObj)
{
  JS::Rooted<JSObject*> proto(cx);
  JS::Rooted<JSObject*> obj(cx, aObj);

  for (;;) {
    if (!::JS_GetPrototype(cx, obj, proto.address())) {
      return JS_FALSE;
    }
    if (!proto) {
      break;
    }

    if (JS_GetClass(proto) == &sGlobalScopePolluterClass) {

      JS::Rooted<JSObject*> proto_proto(cx);
      if (!::JS_GetPrototype(cx, proto, proto_proto.address())) {
        return JS_FALSE;
      }

      // Pull the global scope polluter out of the prototype chain so
      // that it can be freed.
      ::JS_SplicePrototype(cx, obj, proto_proto);

      break;
    }

    obj = proto;
  }

  return JS_TRUE;
}

// static
nsresult
nsWindowSH::InstallGlobalScopePolluter(JSContext *cx, JS::Handle<JSObject*> obj)
{
  JS::Rooted<JSObject*> gsp(cx, ::JS_NewObjectWithUniqueType(cx, &sGlobalScopePolluterClass, nullptr, obj));
  if (!gsp) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  JS::Rooted<JSObject*> o(cx, obj), proto(cx);

  // Find the place in the prototype chain where we want this global
  // scope polluter (right before Object.prototype).

  for (;;) {
    if (!::JS_GetPrototype(cx, o, proto.address())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!proto) {
      break;
    }
    if (JS_GetClass(proto) == sObjectClass) {
      // Set the global scope polluters prototype to Object.prototype
      ::JS_SplicePrototype(cx, gsp, proto);

      break;
    }

    o = proto;
  }

  // And then set the prototype of the object whose prototype was
  // Object.prototype to be the global scope polluter.
  ::JS_SplicePrototype(cx, o, gsp);

  return NS_OK;
}

struct ResolveGlobalNameClosure
{
  JSContext* cx;
  JS::Handle<JSObject*> obj;
  bool* retval;
};

static PLDHashOperator
ResolveGlobalName(const nsAString& aName, void* aClosure)
{
  ResolveGlobalNameClosure* closure =
    static_cast<ResolveGlobalNameClosure*>(aClosure);
  JS::Rooted<JS::Value> dummy(closure->cx);
  bool ok = JS_LookupUCProperty(closure->cx, closure->obj,
                                aName.BeginReading(), aName.Length(),
                                dummy.address());
  if (!ok) {
    *closure->retval = false;
    return PL_DHASH_STOP;
  }
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsWindowSH::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                      JSObject *aObj, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  if (!xpc::WrapperFactory::IsXrayWrapper(obj)) {
    *_retval = JS_EnumerateStandardClasses(cx, obj);
    if (!*_retval) {
      return NS_OK;
    }

    // Now resolve everything from the namespace manager
    nsScriptNameSpaceManager *nameSpaceManager =
      nsJSRuntime::GetNameSpaceManager();
    if (!nameSpaceManager) {
      NS_ERROR("Can't get namespace manager.");
      return NS_ERROR_UNEXPECTED;
    }
    ResolveGlobalNameClosure closure = { cx, obj, _retval };
    nameSpaceManager->EnumerateGlobalNames(ResolveGlobalName, &closure);
  }

  return NS_OK;
}

static nsDOMConstructorFunc
FindConstructorFunc(const nsDOMClassInfoData *aDOMClassInfoData)
{
  for (uint32_t i = 0; i < ArrayLength(kConstructorFuncMap); ++i) {
    if (&sClassInfoData[kConstructorFuncMap[i].mDOMClassInfoID] ==
        aDOMClassInfoData) {
      return kConstructorFuncMap[i].mConstructorFunc;
    }
  }
  return nullptr;
}

static nsresult
BaseStubConstructor(nsIWeakReference* aWeakOwner,
                    const nsGlobalNameStruct *name_struct, JSContext *cx,
                    JS::Handle<JSObject*> obj, const JS::CallArgs &args)
{
  MOZ_ASSERT(obj);

  nsresult rv;
  nsCOMPtr<nsISupports> native;
  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    const nsDOMClassInfoData* ci_data =
      &sClassInfoData[name_struct->mDOMClassInfoID];
    nsDOMConstructorFunc func = FindConstructorFunc(ci_data);
    if (func) {
      rv = func(getter_AddRefs(native));
    } else {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
  } else if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    native = do_CreateInstance(name_struct->mCID, &rv);
  } else if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    native = do_CreateInstance(name_struct->mAlias->mCID, &rv);
  } else {
    native = do_CreateInstance(*name_struct->mData->mConstructorCID, &rv);
  }
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to create the object");
    return rv;
  }

  nsCOMPtr<nsIJSNativeInitializer> initializer(do_QueryInterface(native));
  nsCOMPtr<nsIDOMGlobalObjectConstructor> constructor(do_QueryInterface(native));
  if (initializer || constructor) {
    // Initialize object using the current inner window, but only if
    // the caller can access it.
    nsCOMPtr<nsPIDOMWindow> owner = do_QueryReferent(aWeakOwner);
    nsPIDOMWindow* outerWindow = owner ? owner->GetOuterWindow() : nullptr;
    nsPIDOMWindow* currentInner =
      outerWindow ? outerWindow->GetCurrentInnerWindow() : nullptr;
    if (!currentInner ||
        (owner != currentInner &&
         !nsContentUtils::CanCallerAccess(currentInner))) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    if (initializer) {
      rv = initializer->Initialize(currentInner, cx, obj, args);
      if (NS_FAILED(rv)) {
        return rv;
      }
    } else {
      nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS = do_QueryInterface(native);

      JS::Rooted<JSObject*> thisObject(cx, wrappedJS->GetJSObject());
      if (!thisObject) {
        return NS_ERROR_UNEXPECTED;
      }

      nsCxPusher pusher;
      pusher.Push(cx);

      JSAutoCompartment ac(cx, thisObject);

      JS::Rooted<JS::Value> funval(cx);
      if (!JS_GetProperty(cx, thisObject, "constructor", funval.address()) ||
          !funval.isObject()) {
        return NS_ERROR_UNEXPECTED;
      }

      // Check if the object is even callable.
      NS_ENSURE_STATE(JS_ObjectIsCallable(cx, &funval.toObject()));
      {
        // wrap parameters in the target compartment
        // we also pass in the calling window as the first argument
        unsigned argc = args.length() + 1;
        nsAutoArrayPtr<JS::Value> argv(new JS::Value[argc]);
        JS::AutoArrayRooter rooter(cx, 0, argv);

        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        nsCOMPtr<nsIDOMWindow> currentWin(do_GetInterface(currentInner));
        rv = WrapNative(cx, obj, currentWin, &NS_GET_IID(nsIDOMWindow),
                        true, &argv[0], getter_AddRefs(holder));
        if (!JS_WrapValue(cx, &argv[0]))
          return NS_ERROR_FAILURE;
        rooter.changeLength(1);

        for (size_t i = 1; i < argc; ++i) {
          argv[i] = args[i - 1];
          if (!JS_WrapValue(cx, &argv[i]))
            return NS_ERROR_FAILURE;
          rooter.changeLength(i + 1);
        }

        JS::Rooted<JS::Value> frval(cx);
        bool ret = JS_CallFunctionValue(cx, thisObject, funval, argc, argv,
                                        frval.address());

        if (!ret) {
          return NS_ERROR_FAILURE;
        }
      }
    }
  }

  return WrapNative(cx, obj, native, true, args.rval().address());
}

static nsresult
DefineInterfaceConstants(JSContext *cx, JS::Handle<JSObject*> obj, const nsIID *aIID)
{
  nsCOMPtr<nsIInterfaceInfoManager>
    iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
  NS_ENSURE_TRUE(iim, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIInterfaceInfo> if_info;

  nsresult rv = iim->GetInfoForIID(aIID, getter_AddRefs(if_info));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && if_info, rv);

  uint16_t constant_count;

  if_info->GetConstantCount(&constant_count);

  if (!constant_count) {
    return NS_OK;
  }

  nsCOMPtr<nsIInterfaceInfo> parent_if_info;

  rv = if_info->GetParent(getter_AddRefs(parent_if_info));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && parent_if_info, rv);

  uint16_t parent_constant_count, i;
  parent_if_info->GetConstantCount(&parent_constant_count);

  for (i = parent_constant_count; i < constant_count; i++) {
    const nsXPTConstant *c = nullptr;

    rv = if_info->GetConstant(i, &c);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && c, rv);

    uint16_t type = c->GetType().TagPart();

    jsval v;
    switch (type) {
      case nsXPTType::T_I8:
      case nsXPTType::T_U8:
      {
        v = INT_TO_JSVAL(c->GetValue()->val.u8);
        break;
      }
      case nsXPTType::T_I16:
      case nsXPTType::T_U16:
      {
        v = INT_TO_JSVAL(c->GetValue()->val.u16);
        break;
      }
      case nsXPTType::T_I32:
      {
        v = JS_NumberValue(c->GetValue()->val.i32);
        break;
      }
      case nsXPTType::T_U32:
      {
        v = JS_NumberValue(c->GetValue()->val.u32);
        break;
      }
      default:
      {
#ifdef DEBUG
        NS_ERROR("Non-numeric constant found in interface.");
#endif
        continue;
      }
    }

    if (!::JS_DefineProperty(cx, obj, c->GetName(), v,
                             JS_PropertyStub, JS_StrictPropertyStub,
                             JSPROP_ENUMERATE | JSPROP_READONLY |
                             JSPROP_PERMANENT)) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

// This code is temporary until we remove support for the constants defined
// on IDBCursor/IDBRequest/IDBTransaction

struct IDBConstant
{
  const char* interface;
  const char* name;
  const char* value;

  static const char* IDBCursor;
  static const char* IDBRequest;
  static const char* IDBTransaction;
};

const char* IDBConstant::IDBCursor = "IDBCursor";
const char* IDBConstant::IDBRequest = "IDBRequest";
const char* IDBConstant::IDBTransaction = "IDBTransaction";

static const IDBConstant sIDBConstants[] = {
  { IDBConstant::IDBCursor,      "NEXT",              "next" },
  { IDBConstant::IDBCursor,      "NEXT_NO_DUPLICATE", "nextunique" },
  { IDBConstant::IDBCursor,      "PREV",              "prev" },
  { IDBConstant::IDBCursor,      "PREV_NO_DUPLICATE", "prevunique" },
  { IDBConstant::IDBRequest,     "LOADING",           "pending" },
  { IDBConstant::IDBRequest,     "DONE",              "done" },
  { IDBConstant::IDBTransaction, "READ_ONLY",         "readonly" },
  { IDBConstant::IDBTransaction, "READ_WRITE",        "readwrite" },
  { IDBConstant::IDBTransaction, "VERSION_CHANGE",    "versionchange" },
};

static JSBool
IDBConstantGetter(JSContext *cx, JSHandleObject obj, JSHandleId id, JS::MutableHandle<JS::Value> vp)
{
  JSString *idstr = JSID_TO_STRING(id);
  unsigned index;
  for (index = 0; index < mozilla::ArrayLength(sIDBConstants); index++) {
    JSBool match;
    if (!JS_StringEqualsAscii(cx, idstr, sIDBConstants[index].name, &match)) {
      return JS_FALSE;
    }
    if (match) {
      break;
    }
  }
  MOZ_ASSERT(index < mozilla::ArrayLength(sIDBConstants));

  const IDBConstant& c = sIDBConstants[index];

  // Put a warning on the console
  nsString warnText =
    NS_LITERAL_STRING("The constant ") +
    NS_ConvertASCIItoUTF16(c.interface) +
    NS_LITERAL_STRING(".") +
    NS_ConvertASCIItoUTF16(c.name) +
    NS_LITERAL_STRING(" has been deprecated. Use the string value \"") +
    NS_ConvertASCIItoUTF16(c.value) +
    NS_LITERAL_STRING("\" instead.");

  uint64_t windowID = 0;
  nsIScriptContext* context = GetScriptContextFromJSContext(cx);
  if (context) {
    nsCOMPtr<nsPIDOMWindow> window =
      do_QueryInterface(context->GetGlobalObject());
    if (window) {
      window = window->GetCurrentInnerWindow();
    }
    NS_WARN_IF_FALSE(window, "Missing a window, got a door?");
    if (window) {
      windowID = window->WindowID();
    }
  }

  nsCOMPtr<nsIScriptError> errorObject =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  NS_WARN_IF_FALSE(errorObject, "Failed to create error object");
  if (errorObject) {
    nsresult rv = errorObject->InitWithWindowID(warnText,
                                                EmptyString(), // file name
                                                EmptyString(), // source line
                                                0, 0, // Line/col number
                                                nsIScriptError::warningFlag,
                                                "DOM Core", windowID);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to init error object");

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIConsoleService> consoleServ =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);
      if (consoleServ) {
        consoleServ->LogMessage(errorObject);
      }
    }
  }

  // Redefine property to remove getter
  NS_ConvertASCIItoUTF16 valStr(c.value);
  JS::Rooted<JS::Value> value(cx);
  if (!xpc::StringToJsval(cx, valStr, value.address())) {
    return JS_FALSE;
  }
  if (!::JS_DefineProperty(cx, obj, c.name, value,
                           JS_PropertyStub, JS_StrictPropertyStub,
                           JSPROP_ENUMERATE)) {
    return JS_FALSE;
  }

  // Return value
  vp.set(value);
  return JS_TRUE;
}

static nsresult
DefineIDBInterfaceConstants(JSContext *cx, JS::Handle<JSObject*> obj, const nsIID *aIID)
{
  const char* interface;
  if (aIID->Equals(NS_GET_IID(nsIIDBCursor))) {
    interface = IDBConstant::IDBCursor;
  }
  else if (aIID->Equals(NS_GET_IID(nsIIDBRequest))) {
    interface = IDBConstant::IDBRequest;
  }
  else if (aIID->Equals(NS_GET_IID(nsIIDBTransaction))) {
    interface = IDBConstant::IDBTransaction;
  }
  else {
    MOZ_NOT_REACHED("unexpected IID");
  }

  for (int8_t i = 0; i < (int8_t)mozilla::ArrayLength(sIDBConstants); ++i) {
    const IDBConstant& c = sIDBConstants[i];
    if (c.interface != interface) {
      continue;
    }

    if (!JS_DefineProperty(cx, obj, c.name, JSVAL_VOID,
                           IDBConstantGetter, JS_StrictPropertyStub,
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

class nsDOMConstructor MOZ_FINAL : public nsIDOMDOMConstructor
{
protected:
  nsDOMConstructor(const PRUnichar* aName,
                   bool aIsConstructable,
                   nsPIDOMWindow* aOwner)
    : mClassName(aName),
      mConstructable(aIsConstructable),
      mWeakOwner(do_GetWeakReference(aOwner))
  {
  }

public:

  static nsresult Create(const PRUnichar* aName,
                         const nsDOMClassInfoData* aData,
                         const nsGlobalNameStruct* aNameStruct,
                         nsPIDOMWindow* aOwner,
                         nsDOMConstructor** aResult);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMCONSTRUCTOR

  nsresult PreCreate(JSContext *cx, JSObject *globalObj, JSObject **parentObj);

  nsresult Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JS::Handle<JSObject*> obj, const JS::CallArgs &args,
                     bool *_retval);

  nsresult HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JS::Handle<JSObject*> obj, const jsval &val, bool *bp,
                       bool *_retval);

  nsresult Install(JSContext *cx, JS::Handle<JSObject*> target,
                   JS::Handle<JS::Value> aThisAsVal)
  {
    JS::Rooted<JS::Value> thisAsVal(cx, aThisAsVal);
    // The 'attrs' argument used to be JSPROP_PERMANENT. See bug 628612.
    JSBool ok = JS_WrapValue(cx, thisAsVal.address()) &&
      ::JS_DefineUCProperty(cx, target,
                            reinterpret_cast<const jschar *>(mClassName),
                            NS_strlen(mClassName), thisAsVal, JS_PropertyStub,
                            JS_StrictPropertyStub, 0);

    return ok ? NS_OK : NS_ERROR_UNEXPECTED;
  }

  nsresult ResolveInterfaceConstants(JSContext *cx, JS::Handle<JSObject*> obj);

private:
  const nsGlobalNameStruct *GetNameStruct()
  {
    if (!mClassName) {
      NS_ERROR("Can't get name");
      return nullptr;
    }

    const nsGlobalNameStruct *nameStruct;
#ifdef DEBUG
    nsresult rv =
#endif
      GetNameStruct(nsDependentString(mClassName), &nameStruct);

    NS_ASSERTION(NS_FAILED(rv) || nameStruct, "Name isn't in hash.");

    return nameStruct;
  }

  static nsresult GetNameStruct(const nsAString& aName,
                                const nsGlobalNameStruct **aNameStruct)
  {
    *aNameStruct = nullptr;

    nsScriptNameSpaceManager *nameSpaceManager =
      nsJSRuntime::GetNameSpaceManager();
    if (!nameSpaceManager) {
      NS_ERROR("Can't get namespace manager.");
      return NS_ERROR_UNEXPECTED;
    }

    *aNameStruct = nameSpaceManager->LookupName(aName);

    // Return NS_OK here, aName just isn't a DOM class but nothing failed.
    return NS_OK;
  }

  static bool IsConstructable(const nsDOMClassInfoData *aData)
  {
    if (IS_EXTERNAL(aData->mCachedClassInfo)) {
      const nsExternalDOMClassInfoData* data =
        static_cast<const nsExternalDOMClassInfoData*>(aData);
      return data->mConstructorCID != nullptr;
    }

    return FindConstructorFunc(aData);
  }
  static bool IsConstructable(const nsGlobalNameStruct *aNameStruct)
  {
    return
      (aNameStruct->mType == nsGlobalNameStruct::eTypeClassConstructor &&
       IsConstructable(&sClassInfoData[aNameStruct->mDOMClassInfoID])) ||
      (aNameStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfo &&
       IsConstructable(aNameStruct->mData)) ||
      aNameStruct->mType == nsGlobalNameStruct::eTypeExternalConstructor ||
      aNameStruct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias;
  }

  const PRUnichar*   mClassName;
  const bool mConstructable;
  nsWeakPtr          mWeakOwner;
};

//static
nsresult
nsDOMConstructor::Create(const PRUnichar* aName,
                         const nsDOMClassInfoData* aData,
                         const nsGlobalNameStruct* aNameStruct,
                         nsPIDOMWindow* aOwner,
                         nsDOMConstructor** aResult)
{
  *aResult = nullptr;
  // Prevent creating a constructor if aOwner is inner window which doesn't have
  // an outer window. If the outer window doesn't have an inner window or the
  // caller can't access the outer window's current inner window then try to use
  // the owner (so long as it is, in fact, an inner window). If that doesn't
  // work then prevent creation also.
  nsPIDOMWindow* outerWindow = aOwner->GetOuterWindow();
  nsPIDOMWindow* currentInner =
    outerWindow ? outerWindow->GetCurrentInnerWindow() : aOwner;
  if (!currentInner ||
      (aOwner != currentInner &&
       !nsContentUtils::CanCallerAccess(currentInner) &&
       !(currentInner = aOwner)->IsInnerWindow())) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  bool constructable = aNameStruct ?
                         IsConstructable(aNameStruct) :
                         IsConstructable(aData);

  *aResult = new nsDOMConstructor(aName, constructable, currentInner);
  NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMPL_ADDREF(nsDOMConstructor)
NS_IMPL_RELEASE(nsDOMConstructor)
NS_INTERFACE_MAP_BEGIN(nsDOMConstructor)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMConstructor)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
#ifdef DEBUG
    {
      const nsGlobalNameStruct *name_struct = GetNameStruct();
      NS_ASSERTION(!name_struct ||
                   mConstructable == IsConstructable(name_struct),
                   "Can't change constructability dynamically!");
    }
#endif
    foundInterface =
      NS_GetDOMClassInfoInstance(mConstructable ?
                                 eDOMClassInfo_DOMConstructor_id :
                                 eDOMClassInfo_DOMPrototype_id);
    if (!foundInterface) {
      *aInstancePtr = nullptr;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else
NS_INTERFACE_MAP_END

nsresult
nsDOMConstructor::PreCreate(JSContext *cx, JSObject *globalObj, JSObject **parentObj)
{
  nsCOMPtr<nsPIDOMWindow> owner(do_QueryReferent(mWeakOwner));
  if (!owner) {
    // Can't do anything.
    return NS_OK;
  }

  nsGlobalWindow *win = static_cast<nsGlobalWindow *>(owner.get());
  return SetParentToWindow(win, parentObj);
}

nsresult
nsDOMConstructor::Construct(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                            JS::Handle<JSObject*> obj, const JS::CallArgs &args,
                            bool *_retval)
{
  MOZ_ASSERT(obj);

  const nsGlobalNameStruct *name_struct = GetNameStruct();
  NS_ENSURE_TRUE(name_struct, NS_ERROR_FAILURE);

  if (!IsConstructable(name_struct)) {
    // ignore return value, we return JS_FALSE anyway
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  return BaseStubConstructor(mWeakOwner, name_struct, cx, obj, args);
}

nsresult
nsDOMConstructor::HasInstance(nsIXPConnectWrappedNative *wrapper,
                              JSContext * cx, JS::Handle<JSObject*> obj,
                              const jsval &v, bool *bp, bool *_retval)

{
  // No need to look these up in the hash.
  *bp = false;
  if (JSVAL_IS_PRIMITIVE(v)) {
    return NS_OK;
  }

  JS::Rooted<JSObject*> dom_obj(cx, v.toObjectOrNull());
  NS_ASSERTION(dom_obj, "nsDOMConstructor::HasInstance couldn't get object");

  // This might not be the right object, if there are wrappers. Unwrap if we can.
  JSObject *wrapped_obj = js::CheckedUnwrap(dom_obj, /* stopAtOuter = */ false);
  if (wrapped_obj)
      dom_obj = wrapped_obj;

  JSClass *dom_class = JS_GetClass(dom_obj);
  if (!dom_class) {
    NS_ERROR("nsDOMConstructor::HasInstance can't get class.");
    return NS_ERROR_UNEXPECTED;
  }

  const nsGlobalNameStruct *name_struct;
  nsresult rv = GetNameStruct(NS_ConvertASCIItoUTF16(dom_class->name), &name_struct);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!name_struct) {
    // This isn't a normal DOM object, see if this constructor lives on its
    // prototype chain.
    JS::Rooted<JS::Value> val(cx);
    if (!JS_GetProperty(cx, obj, "prototype", val.address())) {
      return NS_ERROR_UNEXPECTED;
    }

    if (JSVAL_IS_PRIMITIVE(val)) {
      return NS_OK;
    }

    JS::Rooted<JSObject*> dot_prototype(cx, val.toObjectOrNull());

    JS::Rooted<JSObject*> proto(cx, dom_obj);
    for (;;) {
      if (!JS_GetPrototype(cx, proto, proto.address())) {
        return NS_ERROR_UNEXPECTED;
      }
      if (!proto) {
        break;
      }
      if (proto == dot_prototype) {
        *bp = true;
        break;
      }
    }

    return NS_OK;
  }

  if (name_struct->mType != nsGlobalNameStruct::eTypeClassConstructor &&
      name_struct->mType != nsGlobalNameStruct::eTypeExternalClassInfo &&
      name_struct->mType != nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    // Doesn't have DOM interfaces.
    return NS_OK;
  }

  const nsGlobalNameStruct *class_name_struct = GetNameStruct();
  NS_ENSURE_TRUE(class_name_struct, NS_ERROR_FAILURE);

  if (name_struct == class_name_struct) {
    *bp = JS_TRUE;

    return NS_OK;
  }

  nsScriptNameSpaceManager *nameSpaceManager =
    nsJSRuntime::GetNameSpaceManager();
  NS_ASSERTION(nameSpaceManager, "Can't get namespace manager?");

  const nsIID *class_iid;
  if (class_name_struct->mType == nsGlobalNameStruct::eTypeInterface ||
      class_name_struct->mType == nsGlobalNameStruct::eTypeClassProto) {
    class_iid = &class_name_struct->mIID;
  } else if (class_name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    class_iid =
      sClassInfoData[class_name_struct->mDOMClassInfoID].mProtoChainInterface;
  } else if (class_name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    class_iid = class_name_struct->mData->mProtoChainInterface;
  } else if (class_name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    const nsGlobalNameStruct* alias_struct =
      nameSpaceManager->GetConstructorProto(class_name_struct);
    if (!alias_struct) {
      NS_ERROR("Couldn't get constructor prototype.");
      return NS_ERROR_UNEXPECTED;
    }

    if (alias_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
      class_iid =
        sClassInfoData[alias_struct->mDOMClassInfoID].mProtoChainInterface;
    } else if (alias_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
      class_iid = alias_struct->mData->mProtoChainInterface;
    } else {
      NS_ERROR("Expected eTypeClassConstructor or eTypeExternalClassInfo.");
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    *bp = JS_FALSE;

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    name_struct = nameSpaceManager->GetConstructorProto(name_struct);
    if (!name_struct) {
      NS_ERROR("Couldn't get constructor prototype.");
      return NS_ERROR_UNEXPECTED;
    }
  }

  NS_ASSERTION(name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor ||
               name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo,
               "The constructor was set up with a struct of the wrong type.");

  const nsDOMClassInfoData *ci_data = nullptr;
  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor &&
      name_struct->mDOMClassInfoID >= 0) {
    ci_data = &sClassInfoData[name_struct->mDOMClassInfoID];
  } else if (name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    ci_data = name_struct->mData;
  }

  nsCOMPtr<nsIInterfaceInfoManager>
    iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
  if (!iim) {
    NS_ERROR("nsDOMConstructor::HasInstance can't get interface info mgr.");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIInterfaceInfo> if_info;
  uint32_t count = 0;
  const nsIID* class_interface;
  while ((class_interface = ci_data->mInterfaces[count++])) {
    if (class_iid->Equals(*class_interface)) {
      *bp = JS_TRUE;

      return NS_OK;
    }

    iim->GetInfoForIID(class_interface, getter_AddRefs(if_info));
    if (!if_info) {
      NS_ERROR("nsDOMConstructor::HasInstance can't get interface info.");
      return NS_ERROR_UNEXPECTED;
    }

    if_info->HasAncestor(class_iid, bp);

    if (*bp) {
      return NS_OK;
    }
  }

  return NS_OK;
}

nsresult
nsDOMConstructor::ResolveInterfaceConstants(JSContext *cx, JS::Handle<JSObject*> obj)
{
  const nsGlobalNameStruct *class_name_struct = GetNameStruct();
  if (!class_name_struct)
    return NS_ERROR_UNEXPECTED;

  const nsIID *class_iid;
  if (class_name_struct->mType == nsGlobalNameStruct::eTypeInterface ||
      class_name_struct->mType == nsGlobalNameStruct::eTypeClassProto) {
    class_iid = &class_name_struct->mIID;
  } else if (class_name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    class_iid =
      sClassInfoData[class_name_struct->mDOMClassInfoID].mProtoChainInterface;
  } else if (class_name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    class_iid = class_name_struct->mData->mProtoChainInterface;
  } else {
    return NS_OK;
  }

  nsresult rv = DefineInterfaceConstants(cx, obj, class_iid);
  NS_ENSURE_SUCCESS(rv, rv);

  // Special case for |IDBKeyRange| which gets funny "static" functions.
  if (class_iid->Equals(NS_GET_IID(nsIIDBKeyRange)) &&
      !indexedDB::IDBKeyRange::DefineConstructors(cx, obj)) {
    return NS_ERROR_FAILURE;
  }

  // Special case a few IDB interfaces which for now are getting transitional
  // constants.
  if (class_iid->Equals(NS_GET_IID(nsIIDBCursor)) ||
      class_iid->Equals(NS_GET_IID(nsIIDBRequest)) ||
      class_iid->Equals(NS_GET_IID(nsIIDBTransaction))) {
    rv = DefineIDBInterfaceConstants(cx, obj, class_iid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMConstructor::ToString(nsAString &aResult)
{
  aResult.AssignLiteral("[object ");
  aResult.Append(mClassName);
  aResult.Append(PRUnichar(']'));

  return NS_OK;
}


static nsresult
GetXPCProto(nsIXPConnect *aXPConnect, JSContext *cx, nsGlobalWindow *aWin,
            const nsGlobalNameStruct *aNameStruct,
            nsIXPConnectJSObjectHolder **aProto)
{
  NS_ASSERTION(aNameStruct->mType ==
                 nsGlobalNameStruct::eTypeClassConstructor ||
               aNameStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfo,
               "Wrong type!");

  nsCOMPtr<nsIClassInfo> ci;
  if (aNameStruct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    int32_t id = aNameStruct->mDOMClassInfoID;
    NS_ABORT_IF_FALSE(id >= 0, "Negative DOM classinfo?!?");

    nsDOMClassInfoID ci_id = (nsDOMClassInfoID)id;

    ci = NS_GetDOMClassInfoInstance(ci_id);

    // In most cases we want to find the wrapped native prototype in
    // aWin's scope and use that prototype for
    // ClassName.prototype. But in the case where we're setting up
    // "Window.prototype" or "ChromeWindow.prototype" we want to do
    // the look up in aWin's outer window's scope since the inner
    // window's wrapped native prototype comes from the outer
    // window's scope.
    if (ci_id == eDOMClassInfo_Window_id ||
        ci_id == eDOMClassInfo_ModalContentWindow_id ||
        ci_id == eDOMClassInfo_ChromeWindow_id) {
      nsGlobalWindow *scopeWindow = aWin->GetOuterWindowInternal();

      if (scopeWindow) {
        aWin = scopeWindow;
      }
    }
  }
  else {
    ci = nsDOMClassInfo::GetClassInfoInstance(aNameStruct->mData);
  }
  NS_ENSURE_TRUE(ci, NS_ERROR_UNEXPECTED);

  nsresult rv =
    aXPConnect->GetWrappedNativePrototype(cx, aWin->GetGlobalJSObject(), ci,
                                          aProto);
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JSObject*> proto_obj(cx, (*aProto)->GetJSObject());
  if (!JS_WrapObject(cx, proto_obj.address())) {
    return NS_ERROR_FAILURE;
  }

  NS_IF_RELEASE(*aProto);
  return aXPConnect->HoldObject(cx, proto_obj, aProto);
}

// Either ci_data must be non-null or name_struct must be non-null and of type
// eTypeClassProto.
static nsresult
ResolvePrototype(nsIXPConnect *aXPConnect, nsGlobalWindow *aWin, JSContext *cx,
                 JS::Handle<JSObject*> obj, const PRUnichar *name,
                 const nsDOMClassInfoData *ci_data,
                 const nsGlobalNameStruct *name_struct,
                 nsScriptNameSpaceManager *nameSpaceManager,
                 JSObject* aDot_prototype, bool install, bool *did_resolve)
{
  JS::Rooted<JSObject*> dot_prototype(cx, aDot_prototype);
  NS_ASSERTION(ci_data ||
               (name_struct &&
                name_struct->mType == nsGlobalNameStruct::eTypeClassProto),
               "Wrong type or missing ci_data!");

  nsRefPtr<nsDOMConstructor> constructor;
  nsresult rv = nsDOMConstructor::Create(name, ci_data, name_struct, aWin,
                                         getter_AddRefs(constructor));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  JS::Rooted<JS::Value> v(cx);

  rv = WrapNative(cx, obj, constructor, &NS_GET_IID(nsIDOMDOMConstructor),
                  false, v.address(), getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(rv, rv);

  if (install) {
    rv = constructor->Install(cx, obj, v);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  JS::Rooted<JSObject*> class_obj(cx, holder->GetJSObject());
  NS_ASSERTION(class_obj, "The return value lied");

  const nsIID *primary_iid = &NS_GET_IID(nsISupports);

  if (!ci_data) {
    primary_iid = &name_struct->mIID;
  }
  else if (ci_data->mProtoChainInterface) {
    primary_iid = ci_data->mProtoChainInterface;
  }

  nsCOMPtr<nsIInterfaceInfo> if_info;
  nsCOMPtr<nsIInterfaceInfo> parent;
  const char *class_parent_name = nullptr;

  if (!primary_iid->Equals(NS_GET_IID(nsISupports))) {
    JSAutoCompartment ac(cx, class_obj);

    rv = DefineInterfaceConstants(cx, class_obj, primary_iid);
    NS_ENSURE_SUCCESS(rv, rv);

    // Special case for |IDBKeyRange| which gets funny "static" functions.
    if (primary_iid->Equals(NS_GET_IID(nsIIDBKeyRange)) &&
        !indexedDB::IDBKeyRange::DefineConstructors(cx, class_obj)) {
      return NS_ERROR_FAILURE;
    }

    // Special case a few IDB interfaces which for now are getting transitional
    // constants.
    if (primary_iid->Equals(NS_GET_IID(nsIIDBCursor)) ||
        primary_iid->Equals(NS_GET_IID(nsIIDBRequest)) ||
        primary_iid->Equals(NS_GET_IID(nsIIDBTransaction))) {
      rv = DefineIDBInterfaceConstants(cx, class_obj, primary_iid);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIInterfaceInfoManager>
      iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
    NS_ENSURE_TRUE(iim, NS_ERROR_NOT_AVAILABLE);

    iim->GetInfoForIID(primary_iid, getter_AddRefs(if_info));
    NS_ENSURE_TRUE(if_info, NS_ERROR_UNEXPECTED);

    const nsIID *iid = nullptr;

    if (ci_data && !ci_data->mHasClassInterface) {
      if_info->GetIIDShared(&iid);
    } else {
      if_info->GetParent(getter_AddRefs(parent));
      NS_ENSURE_TRUE(parent, NS_ERROR_UNEXPECTED);

      parent->GetIIDShared(&iid);
    }

    if (iid) {
      if (!iid->Equals(NS_GET_IID(nsISupports))) {
        if (ci_data && !ci_data->mHasClassInterface) {
          // If the class doesn't have a class interface the primary
          // interface is the interface that should be
          // constructor.prototype.__proto__.

          if_info->GetNameShared(&class_parent_name);
        } else {
          // If the class does have a class interface (or there's no
          // real class for this name) then the parent of the
          // primary interface is what we want on
          // constructor.prototype.__proto__.

          NS_ASSERTION(parent, "Whoa, this is bad, null parent here!");

          parent->GetNameShared(&class_parent_name);
        }
      }
    }
  }

  {
    JS::Rooted<JSObject*> winobj(cx, aWin->FastGetGlobalJSObject());

    JS::Rooted<JSObject*> proto(cx);

    if (class_parent_name) {
      JSAutoCompartment ac(cx, winobj);

      JS::Rooted<JS::Value> val(cx);
      if (!JS_LookupProperty(cx, winobj, CutPrefix(class_parent_name), val.address())) {
        return NS_ERROR_UNEXPECTED;
      }

      if (val.isObject()) {
        if (!JS_LookupProperty(cx, &val.toObject(), "prototype", val.address())) {
          return NS_ERROR_UNEXPECTED;
        }

        if (val.isObject()) {
          proto = &val.toObject();
        }
      }
    }

    if (dot_prototype) {
      JSAutoCompartment ac(cx, dot_prototype);
      JS::Rooted<JSObject*> xpc_proto_proto(cx);
      if (!::JS_GetPrototype(cx, dot_prototype, xpc_proto_proto.address())) {
        return NS_ERROR_UNEXPECTED;
      }

      if (proto &&
          (!xpc_proto_proto ||
           JS_GetClass(xpc_proto_proto) == sObjectClass)) {
        if (!JS_WrapObject(cx, proto.address()) ||
            !JS_SetPrototype(cx, dot_prototype, proto)) {
          return NS_ERROR_UNEXPECTED;
        }
      }
    } else {
      JSAutoCompartment ac(cx, winobj);
      if (!proto) {
        proto = JS_GetObjectPrototype(cx, winobj);
      }
      dot_prototype = ::JS_NewObjectWithUniqueType(cx,
                                                   &sDOMConstructorProtoClass,
                                                   proto,
                                                   winobj);
      NS_ENSURE_TRUE(dot_prototype, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  v = OBJECT_TO_JSVAL(dot_prototype);

  JSAutoCompartment ac(cx, class_obj);

  // Per ECMA, the prototype property is {DontEnum, DontDelete, ReadOnly}
  if (!JS_WrapValue(cx, v.address()) ||
      !JS_DefineProperty(cx, class_obj, "prototype", v,
                         JS_PropertyStub, JS_StrictPropertyStub,
                         JSPROP_PERMANENT | JSPROP_READONLY)) {
    return NS_ERROR_UNEXPECTED;
  }

  *did_resolve = true;

  return NS_OK;
}

static bool
OldBindingConstructorEnabled(const nsGlobalNameStruct *aStruct,
                             nsGlobalWindow *aWin)
{
  MOZ_ASSERT(aStruct->mType == nsGlobalNameStruct::eTypeClassConstructor ||
             aStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfo);

  // Don't expose chrome only constructors to content windows.
  if (aStruct->mChromeOnly &&
      !nsContentUtils::IsSystemPrincipal(aWin->GetPrincipal())) {
    return false;
  }

  // Don't expose CSSSupportsRule unless @supports processing is enabled.
  if (aStruct->mDOMClassInfoID == eDOMClassInfo_CSSSupportsRule_id) {
    if (!CSSSupportsRule::PrefEnabled()) {
      return false;
    }
  }

  return true;
}

// static
nsresult
nsWindowSH::GlobalResolve(nsGlobalWindow *aWin, JSContext *cx,
                          JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                          bool *did_resolve)
{
  *did_resolve = false;

  nsScriptNameSpaceManager *nameSpaceManager =
    nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nsDependentJSString name(id);

  const PRUnichar *class_name = nullptr;
  const nsGlobalNameStruct *name_struct =
    nameSpaceManager->LookupName(name, &class_name);

  if (!name_struct) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(class_name, NS_ERROR_UNEXPECTED);

  nsresult rv = NS_OK;

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfoCreator) {
    rv = GetExternalClassInfo(nameSpaceManager, name, name_struct,
                              &name_struct);
    if (NS_FAILED(rv) || !name_struct) {
      return rv;
    }
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeNewDOMBinding ||
      name_struct->mType == nsGlobalNameStruct::eTypeInterface ||
      name_struct->mType == nsGlobalNameStruct::eTypeClassProto ||
      name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    // Lookup new DOM bindings.
    mozilla::dom::DefineInterface define =
      name_struct->mDefineDOMInterface;
    if (define) {
      if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor &&
          !OldBindingConstructorEnabled(name_struct, aWin)) {
        return NS_OK;
      }

      Maybe<JSAutoCompartment> ac;
      JS::Rooted<JSObject*> global(cx);
      bool defineOnXray = xpc::WrapperFactory::IsXrayWrapper(obj);
      if (defineOnXray) {
        global = js::CheckedUnwrap(obj, /* stopAtOuter = */ false);
        if (!global) {
          return NS_ERROR_DOM_SECURITY_ERR;
        }
        ac.construct(cx, global);
      } else {
        global = obj;
      }

      // Check whether our constructor is enabled after we unwrap Xrays, since
      // we don't want to define an interface on the Xray if it's disabled in
      // the target global, even if it's enabled in the Xray's global.
      if (name_struct->mConstructorEnabled &&
          !(*name_struct->mConstructorEnabled)(cx, global)) {
        return NS_OK;
      }

      bool enabled;
      JS::Rooted<JSObject*> interfaceObject(cx, define(cx, global, id, &enabled));
      if (enabled) {
        if (!interfaceObject) {
          return NS_ERROR_FAILURE;
        }

        if (defineOnXray) {
          // This really should be handled by the Xray for the window.
          ac.destroy();
          if (!JS_WrapObject(cx, interfaceObject.address()) ||
              !JS_DefinePropertyById(cx, obj, id,
                                     JS::ObjectValue(*interfaceObject), JS_PropertyStub,
                                     JS_StrictPropertyStub, 0)) {
            return NS_ERROR_FAILURE;
          }
        }

        *did_resolve = true;

        return NS_OK;
      }
    }
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeInterface) {
    // We're resolving a name of a DOM interface for which there is no
    // direct DOM class, create a constructor object...
    nsRefPtr<nsDOMConstructor> constructor;
    rv = nsDOMConstructor::Create(class_name,
                                  nullptr,
                                  name_struct,
                                  static_cast<nsPIDOMWindow*>(aWin),
                                  getter_AddRefs(constructor));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    JS::Rooted<JS::Value> v(cx);
    rv = WrapNative(cx, obj, constructor, &NS_GET_IID(nsIDOMDOMConstructor),
                    false, v.address(), getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = constructor->Install(cx, obj, v);
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Rooted<JSObject*> class_obj(cx, holder->GetJSObject());
    NS_ASSERTION(class_obj, "The return value lied");

    // ... and define the constants from the DOM interface on that
    // constructor object.

    JSAutoCompartment ac(cx, class_obj);
    rv = DefineInterfaceConstants(cx, class_obj, &name_struct->mIID);
    NS_ENSURE_SUCCESS(rv, rv);

    *did_resolve = true;

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor ||
      name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    if (!OldBindingConstructorEnabled(name_struct, aWin)) {
      return NS_OK;
    }

    // Create the XPConnect prototype for our classinfo, PostCreateProto will
    // set up the prototype chain.
    nsCOMPtr<nsIXPConnectJSObjectHolder> proto_holder;
    rv = GetXPCProto(sXPConnect, cx, aWin, name_struct,
                     getter_AddRefs(proto_holder));

    if (NS_SUCCEEDED(rv) && obj != aWin->GetGlobalJSObject()) {
      JS::Rooted<JSObject*> dot_prototype(cx, proto_holder->GetJSObject());
      NS_ENSURE_STATE(dot_prototype);

      const nsDOMClassInfoData *ci_data;
      if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
        ci_data = &sClassInfoData[name_struct->mDOMClassInfoID];
      } else {
        ci_data = name_struct->mData;
      }

      return ResolvePrototype(sXPConnect, aWin, cx, obj, class_name, ci_data,
                              name_struct, nameSpaceManager, dot_prototype,
                              true, did_resolve);
    }

    *did_resolve = NS_SUCCEEDED(rv);

    return rv;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeClassProto) {
    // We don't have a XPConnect prototype object, let ResolvePrototype create
    // one.
    return ResolvePrototype(sXPConnect, aWin, cx, obj, class_name, nullptr,
                            name_struct, nameSpaceManager, nullptr, true,
                            did_resolve);
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    const nsGlobalNameStruct *alias_struct =
      nameSpaceManager->GetConstructorProto(name_struct);
    NS_ENSURE_TRUE(alias_struct, NS_ERROR_UNEXPECTED);

    // We need to use the XPConnect prototype for the DOM class that this
    // constructor is an alias for (for example for Image we need the prototype
    // for HTMLImageElement).
    nsCOMPtr<nsIXPConnectJSObjectHolder> proto_holder;
    rv = GetXPCProto(sXPConnect, cx, aWin, alias_struct,
                     getter_AddRefs(proto_holder));
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject* dot_prototype = proto_holder->GetJSObject();
    NS_ENSURE_STATE(dot_prototype);

    const nsDOMClassInfoData *ci_data;
    if (alias_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
      ci_data = &sClassInfoData[alias_struct->mDOMClassInfoID];
    } else if (alias_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
      ci_data = alias_struct->mData;
    } else {
      return NS_ERROR_UNEXPECTED;
    }

    return ResolvePrototype(sXPConnect, aWin, cx, obj, class_name, ci_data,
                            name_struct, nameSpaceManager, nullptr, true,
                            did_resolve);
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    nsRefPtr<nsDOMConstructor> constructor;
    rv = nsDOMConstructor::Create(class_name, nullptr, name_struct,
                                  static_cast<nsPIDOMWindow*>(aWin),
                                  getter_AddRefs(constructor));
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Rooted<JS::Value> val(cx);
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, obj, constructor, &NS_GET_IID(nsIDOMDOMConstructor),
                    false, val.address(), getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = constructor->Install(cx, obj, val);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(holder->GetJSObject(), "Why didn't we get a JSObject?");

    *did_resolve = true;

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeProperty) {
    if (name_struct->mChromeOnly && !nsContentUtils::IsCallerChrome())
      return NS_OK;

    nsCOMPtr<nsISupports> native(do_CreateInstance(name_struct->mCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Rooted<JS::Value> prop_val(cx, JS::UndefinedValue()); // Property value.

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsCOMPtr<nsIDOMGlobalPropertyInitializer> gpi(do_QueryInterface(native));
    if (gpi) {
      rv = gpi->Init(aWin, prop_val.address());
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (JSVAL_IS_PRIMITIVE(prop_val) && !JSVAL_IS_NULL(prop_val)) {
      JSObject *scope;

      if (aWin->IsOuterWindow()) {
        nsGlobalWindow *inner = aWin->GetCurrentInnerWindowInternal();
        NS_ENSURE_TRUE(inner, NS_ERROR_UNEXPECTED);

        scope = inner->GetGlobalJSObject();
      } else {
        scope = aWin->GetGlobalJSObject();
      }

      rv = WrapNative(cx, scope, native, true, prop_val.address(),
                      getter_AddRefs(holder));
    }

    NS_ENSURE_SUCCESS(rv, rv);

    if (!JS_WrapValue(cx, prop_val.address())) {
      return NS_ERROR_UNEXPECTED;
    }

    JSBool ok = ::JS_DefinePropertyById(cx, obj, id, prop_val,
                                        JS_PropertyStub, JS_StrictPropertyStub,
                                        JSPROP_ENUMERATE);

    *did_resolve = true;

    return ok ? NS_OK : NS_ERROR_FAILURE;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeDynamicNameSet) {
    nsCOMPtr<nsIScriptExternalNameSet> nameset =
      do_CreateInstance(name_struct->mCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIScriptContext *context = aWin->GetContext();
    NS_ENSURE_TRUE(context, NS_ERROR_UNEXPECTED);

    rv = nameset->InitializeNameSet(context);

    *did_resolve = true;
  }

  return rv;
}

// Native code for window._content getter, this simply maps
// window._content to window.content for backwards compatibility only.
static JSBool
ContentWindowGetter(JSContext *cx, unsigned argc, jsval *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj)
    return JS_FALSE;

  return ::JS_GetProperty(cx, obj, "content", vp);
}

template<class Interface>
static nsresult
LocationSetterGuts(JSContext *cx, JSObject *obj, jsval *vp)
{
  // This function duplicates some of the logic in XPC_WN_HelperSetProperty
  obj = js::CheckedUnwrap(obj, /* stopAtOuter = */ false);
  if (!IS_WN_REFLECTOR(obj))
      return NS_ERROR_XPC_BAD_CONVERT_JS;
  XPCWrappedNative *wrapper = XPCWrappedNative::Get(obj);

  // The error checks duplicate code in THROW_AND_RETURN_IF_BAD_WRAPPER
  NS_ENSURE_TRUE(!wrapper || wrapper->IsValid(), NS_ERROR_XPC_HAS_BEEN_SHUTDOWN);

  nsCOMPtr<Interface> xpcomObj = do_QueryWrappedNative(wrapper, obj);
  NS_ENSURE_TRUE(xpcomObj, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMLocation> location;
  nsresult rv = xpcomObj->GetLocation(getter_AddRefs(location));
  NS_ENSURE_SUCCESS(rv, rv);

  // Grab the value we're being set to before we stomp on |vp|
  JS::Rooted<JSString*> val(cx, ::JS_ValueToString(cx, *vp));
  NS_ENSURE_TRUE(val, NS_ERROR_UNEXPECTED);

  // Make sure |val| stays alive below
  JS::Anchor<JSString *> anchor(val);

  // We have to wrap location into vp before null-checking location, to
  // avoid assigning the wrong thing into the slot.
  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  rv = WrapNative(cx, JS_GetGlobalForScopeChain(cx), location,
                  &NS_GET_IID(nsIDOMLocation), true, vp,
                  getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!location) {
    // Make this a no-op
    return NS_OK;
  }

  nsDependentJSString depStr;
  NS_ENSURE_TRUE(depStr.init(cx, val), NS_ERROR_UNEXPECTED);

  return location->SetHref(depStr);
}

template<class Interface>
static JSBool
LocationSetter(JSContext *cx, JSHandleObject obj, JSHandleId id, JSBool strict,
               JS::MutableHandle<JS::Value> vp)
{
  nsresult rv = LocationSetterGuts<Interface>(cx, obj, vp.address());
  if (NS_FAILED(rv)) {
    xpc::Throw(cx, rv);
    return JS_FALSE;
  }

  return JS_TRUE;
}

static JSBool
LocationSetterUnwrapper(JSContext *cx, JSHandleObject obj_, JSHandleId id, JSBool strict,
                        JS::MutableHandle<JS::Value> vp)
{
  JS::RootedObject obj(cx, obj_);

  JSObject *wrapped = XPCWrapper::UnsafeUnwrapSecurityWrapper(obj);
  if (wrapped) {
    obj = wrapped;
  }

  return LocationSetter<nsIDOMWindow>(cx, obj, id, strict, vp);
}

struct InterfaceShimEntry {
  const char *geckoName;
  const char *domName;
};

// We add shims from Components.interfaces.nsIDOMFoo to window.Foo for each
// interface that has interface constants that sites might be getting off
// of Ci.
const InterfaceShimEntry kInterfaceShimMap[] =
{ { "nsIDOMFileReader", "FileReader" },
  { "nsIXMLHttpRequest", "XMLHttpRequest" },
  { "nsIDOMDOMException", "DOMException" },
  { "nsIDOMNode", "Node" },
  { "nsIDOMUserDataHandler", "UserDataHandler" },
  { "nsIDOMCSSPrimitiveValue", "CSSPrimitiveValue" },
  { "nsIDOMCSSRule", "CSSRule" },
  { "nsIDOMCSSValue", "CSSValue" },
  { "nsIDOMEvent", "Event" },
  { "nsIDOMNSEvent", "Event" },
  { "nsIDOMKeyEvent", "KeyEvent" },
  { "nsIDOMMouseEvent", "MouseEvent" },
  { "nsIDOMMouseScrollEvent", "MouseScrollEvent" },
  { "nsIDOMMutationEvent", "MutationEvent" },
  { "nsIDOMSimpleGestureEvent", "SimpleGestureEvent" },
  { "nsIDOMUIEvent", "UIEvent" },
  { "nsIDOMGeoPositionError", "GeoPositionError" },
  { "nsIDOMHTMLMediaElement", "HTMLMediaElement" },
  { "nsIDOMMediaError", "MediaError" },
  { "nsIDOMLoadStatus", "LoadStatus" },
  { "nsIDOMOfflineResourceList", "OfflineResourceList" },
  { "nsIDOMRange", "Range" },
  { "nsIDOMSVGFETurbulenceElement", "SVGFETurbulenceElement" },
  { "nsIDOMSVGFEMorphologyElement", "SVGFEMorphologyElement" },
  { "nsIDOMSVGFEConvolveMatrixElement", "SVGFEConvolveMatrixElement" },
  { "nsIDOMSVGFEDisplacementMapElement", "SVGFEDisplacementMapElement" },
  { "nsIDOMSVGLength", "SVGLength" },
  { "nsIDOMSVGUnitTypes", "SVGUnitTypes" },
  { "nsIDOMNodeFilter", "NodeFilter" },
  { "nsIDOMXPathNamespace", "XPathNamespace" },
  { "nsIDOMXPathResult", "XPathResult" },
  { "nsIDOMXULButtonElement", "XULButtonElement" },
  { "nsIDOMXULCheckboxElement", "XULCheckboxElement" },
  { "nsIDOMXULPopupElement", "XULPopupElement" } };

static nsresult
DefineComponentsShim(JSContext *cx, JS::HandleObject global)
{
  // Keep track of how often this happens.
  Telemetry::Accumulate(Telemetry::COMPONENTS_SHIM_ACCESSED_BY_CONTENT, true);

  // Create a fake Components object.
  JS::Rooted<JSObject*> components(cx, JS_NewObject(cx, nullptr, nullptr, global));
  NS_ENSURE_TRUE(components, NS_ERROR_OUT_OF_MEMORY);
  bool ok = JS_DefineProperty(cx, global, "Components", JS::ObjectValue(*components),
                              JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE);
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);

  // Create a fake interfaces object.
  JS::Rooted<JSObject*> interfaces(cx, JS_NewObject(cx, nullptr, nullptr, global));
  NS_ENSURE_TRUE(interfaces, NS_ERROR_OUT_OF_MEMORY);
  ok = JS_DefineProperty(cx, components, "interfaces", JS::ObjectValue(*interfaces),
                         JS_PropertyStub, JS_StrictPropertyStub,
                         JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);

  // Define a bunch of shims from the Ci.nsIDOMFoo to window.Foo for DOM
  // interfaces with constants.
  for (uint32_t i = 0; i < ArrayLength(kInterfaceShimMap); ++i) {

    // Grab the names from the table.
    const char *geckoName = kInterfaceShimMap[i].geckoName;
    const char *domName = kInterfaceShimMap[i].domName;

    // Look up the appopriate interface object on the global.
    JS::Rooted<JS::Value> v(cx, JS::UndefinedValue());
    ok = JS_GetProperty(cx, global, domName, v.address());
    NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
    if (!v.isObject()) {
      NS_WARNING("Unable to find interface object on global");
      continue;
    }

    // Define the shim on the interfaces object.
    ok = JS_DefineProperty(cx, interfaces, geckoName, v,
                           JS_PropertyStub, JS_StrictPropertyStub,
                           JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);
    NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj_, jsid id_, uint32_t flags,
                       JSObject **objp, bool *_retval)
{
  JS::RootedObject obj(cx, obj_);
  JS::RootedId id(cx, id_);

  if (!JSID_IS_STRING(id)) {
    return NS_OK;
  }

  MOZ_ASSERT(*_retval == true); // guaranteed by XPC_WN_Helper_NewResolve
  if (id == XPCJSRuntime::Get()->GetStringID(XPCJSRuntime::IDX_COMPONENTS)) {
    *objp = obj;
    return DefineComponentsShim(cx, obj);
  }

  nsGlobalWindow *win = nsGlobalWindow::FromWrapper(wrapper);
  MOZ_ASSERT(win->IsInnerWindow());

  nsIScriptContext *my_context = win->GetContextInternal();

  // Don't resolve standard classes on XrayWrappers, only resolve them if we're
  // resolving on the real global object.
  if (!xpc::WrapperFactory::IsXrayWrapper(obj)) {
    JSBool did_resolve = JS_FALSE;
    JSBool ok = JS_TRUE;
    JS::Rooted<JS::Value> exn(cx, JSVAL_VOID);

    {
      // Resolve standard classes on my_context's JSContext (or on cx,
      // if we don't have a my_context yet), in case the two contexts
      // have different origins.  We want lazy standard class
      // initialization to behave as if it were done eagerly, on each
      // window's own context (not on some other window-caller's
      // context).
      AutoPushJSContext my_cx(my_context ? my_context->GetNativeContext() : cx);
      JSAutoCompartment ac(my_cx, obj);

      ok = JS_ResolveStandardClass(my_cx, obj, id, &did_resolve);

      if (!ok) {
        // Trust the JS engine (or the script security manager) to set
        // the exception in the JS engine.

        if (!JS_GetPendingException(my_cx, exn.address())) {
          return NS_ERROR_UNEXPECTED;
        }

        // Return NS_OK to avoid stomping over the exception that was passed
        // down from the ResolveStandardClass call.
        // Note that the order of the JS_ClearPendingException and
        // JS_SetPendingException is important in the case that my_cx == cx.

        JS_ClearPendingException(my_cx);
      }
    }

    if (!ok) {
      JS_SetPendingException(cx, exn);
      *_retval = JS_FALSE;
      return NS_OK;
    }

    if (did_resolve) {
      *objp = obj;
      return NS_OK;
    }
  }

  // We want this code to be before the child frame lookup code
  // below so that a child frame named 'constructor' doesn't
  // shadow the window's constructor property.
  if (sConstructor_id == id) {
    return ResolveConstructor(cx, obj, objp);
  }

  if (!my_context || !my_context->IsContextInitialized()) {
    // The context is not yet initialized so there's nothing we can do
    // here yet.

    return NS_OK;
  }

  if (sLocation_id == id) {
    // This must be done even if we're just getting the value of
    // window.location (i.e. no checking flags & JSRESOLVE_ASSIGNING
    // here) since we must define window.location to prevent the
    // getter from being overriden (for security reasons).

    nsCOMPtr<nsIDOMLocation> location;
    nsresult rv = win->GetLocation(getter_AddRefs(location));
    NS_ENSURE_SUCCESS(rv, rv);

    // Make sure we wrap the location object in the window's scope.
    JS::Rooted<JSObject*> scope(cx, wrapper->GetJSObject());

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    JS::Rooted<JS::Value> v(cx);
    rv = WrapNative(cx, scope, location, &NS_GET_IID(nsIDOMLocation), true,
                    v.address(), getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    JSBool ok = JS_WrapValue(cx, v.address()) &&
                JS_DefinePropertyById(cx, obj, id, v, JS_PropertyStub,
                                      LocationSetterUnwrapper,
                                      JSPROP_PERMANENT | JSPROP_ENUMERATE);

    if (!ok) {
      return NS_ERROR_FAILURE;
    }

    *objp = obj;

    return NS_OK;
  }

  if (sTop_id == id) {
    nsCOMPtr<nsIDOMWindow> top;
    nsresult rv = win->GetScriptableTop(getter_AddRefs(top));
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Rooted<JS::Value> v(cx);
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, obj, top, &NS_GET_IID(nsIDOMWindow), true,
                    v.address(), getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    // Hold on to the top window object as a global property so we
    // don't need to worry about losing expando properties etc.
    if (!JS_DefinePropertyById(cx, obj, id, v, JS_PropertyStub, JS_StrictPropertyStub,
                               JSPROP_READONLY | JSPROP_PERMANENT |
                               JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    *objp = obj;

    return NS_OK;
  }

  // Handle resolving if id refers to a name resolved by DOM worker code.
  JS::RootedObject tmp(cx, NULL);
  if (!ResolveWorkerClasses(cx, obj, id, flags, &tmp)) {
    return NS_ERROR_FAILURE;
  }
  if (tmp) {
    *objp = tmp;
    return NS_OK;
  }

  // Check for names managed by the script namespace manager.  Call
  // GlobalResolve() after we call FindChildWithName() so that named child
  // frames will override external properties which have been registered with
  // the script namespace manager -- pages must be able to depend on frame
  // names working no matter how Gecko's been configured.
  bool did_resolve = false;
  nsresult rv = GlobalResolve(win, cx, obj, id, &did_resolve);
  NS_ENSURE_SUCCESS(rv, rv);

  if (did_resolve) {
    *objp = obj;
    return NS_OK;
  }

  // NB: By accident, we previously didn't support this over Xrays. This is a
  // deprecated non-standard feature, so there's no reason to start doing so
  // now.
  if ((s_content_id == id) && !xpc::WrapperFactory::IsXrayWrapper(obj)) {
    // Map window._content to window.content for backwards
    // compatibility, this should spit out an message on the JS
    // console.
    JS::Rooted<JSObject*> funObj(cx);
    JSFunction *fun = ::JS_NewFunction(cx, ContentWindowGetter, 0, 0,
                                       obj, "_content");
    if (!fun) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    funObj = ::JS_GetFunctionObject(fun);

    if (!JS_WrapObject(cx, funObj.address()) ||
        !JS_DefinePropertyById(cx, obj, id, JSVAL_VOID,
                               JS_DATA_TO_FUNC_PTR(JSPropertyOp, funObj.get()),
                               JS_StrictPropertyStub,
                               JSPROP_ENUMERATE | JSPROP_GETTER |
                               JSPROP_SHARED)) {
      return NS_ERROR_FAILURE;
    }

    *objp = obj;

    return NS_OK;
  }

  if (flags & JSRESOLVE_ASSIGNING) {
    if (IsReadonlyReplaceable(id)) {
      // A readonly "replaceable" property is being set.  Define the property
      // on obj with the value undefined to override the predefined property.
      // This isn't quite what WebIDL requires for [Replaceable] properties,
      // but it'll do until we move Window over to the new DOM bindings.

      if (!::JS_DefinePropertyById(cx, obj, id, JSVAL_VOID, JS_PropertyStub,
                                   JS_StrictPropertyStub, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
      *objp = obj;

      return NS_OK;
    }
  } else {
    if (sNavigator_id == id) {
      nsCOMPtr<nsIDOMNavigator> navigator;
      rv = win->GetNavigator(getter_AddRefs(navigator));
      NS_ENSURE_SUCCESS(rv, rv);

      JS::Rooted<JS::Value> v(cx);
      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      rv = WrapNative(cx, obj, navigator, &NS_GET_IID(nsIDOMNavigator), true,
                      v.address(), getter_AddRefs(holder));
      NS_ENSURE_SUCCESS(rv, rv);

      // Hold on to the navigator object as a global property so we
      // don't need to worry about losing expando properties etc.
      if (!::JS_DefinePropertyById(cx, obj, id, v,
                                   JS_PropertyStub, JS_StrictPropertyStub,
                                   JSPROP_READONLY | JSPROP_PERMANENT |
                                   JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
      *objp = obj;

      return NS_OK;
    }

    if (sDocument_id == id) {
      nsCOMPtr<nsIDocument> document = win->GetDoc();
      JS::Rooted<JS::Value> v(cx);
      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      rv = WrapNative(cx, JS_GetGlobalForScopeChain(cx), document, document,
                      &NS_GET_IID(nsIDOMDocument), v.address(), getter_AddRefs(holder),
                      false);
      NS_ENSURE_SUCCESS(rv, rv);

      // The PostCreate hook for the document will handle defining the
      // property
      *objp = obj;

      // NB: We need to do this for any Xray wrapper.
      if (xpc::WrapperFactory::IsXrayWrapper(obj)) {
        // Unless our object is a native wrapper, in which case we have to
        // define it ourselves.

        *_retval = JS_WrapValue(cx, v.address()) &&
                   JS_DefineProperty(cx, obj, "document", v,
                                     JS_PropertyStub, JS_StrictPropertyStub,
                                     JSPROP_READONLY | JSPROP_ENUMERATE);
        if (!*_retval) {
          return NS_ERROR_UNEXPECTED;
        }
      }

      return NS_OK;
    }
  }

  rv = nsDOMGenericSH::NewResolve(wrapper, cx, obj, id, flags, objp,
                                  _retval);

  if (NS_FAILED(rv) || *objp) {
    // Something went wrong, or the property got resolved. Return.
    return rv;
  }

  // Make a fast expando if we're assigning to (not declaring or
  // binding a name) a new undefined property that's not already
  // defined on our prototype chain. This way we can access this
  // expando w/o ever getting back into XPConnect.
  if (flags & JSRESOLVE_ASSIGNING) {
    JS::Rooted<JSObject*> realObj(cx, wrapper->GetJSObject());

    if (obj == realObj) {
      JS::Rooted<JSObject*> proto(cx);
      if (!js::GetObjectProto(cx, obj, &proto)) {
          *_retval = JS_FALSE;
          return NS_OK;
      }
      if (proto) {
        JS::Rooted<JSObject*> pobj(cx);
        JS::Rooted<JS::Value> val(cx);

        if (!::JS_LookupPropertyWithFlagsById(cx, proto, id, flags,
                                              pobj.address(), val.address())) {
          *_retval = JS_FALSE;

          return NS_OK;
        }

        if (pobj) {
          // A property was found on the prototype chain.
          *objp = pobj;
          return NS_OK;
        }
      }

      // Define a fast expando.  The key here is to use JS_PropertyStub as the
      // getter/setter, which makes us stay out of XPConnect when using this
      // property.
      //
      // We're adding a new property here, so we don't need to worry about
      // conflicting with any existing ones.
      //
      // Since we always create the undeclared property here, shortcutting the
      // normal process, we go out of our way to tell the JS engine to report
      // strict warnings/errors using js::ReportIfUndeclaredVarAssignment.
      JS::Rooted<JSString*> str(cx, JSID_TO_STRING(id));
      if (!js::ReportIfUndeclaredVarAssignment(cx, str) ||
          !::JS_DefinePropertyById(cx, obj, id, JSVAL_VOID, JS_PropertyStub,
                                   JS_StrictPropertyStub, JSPROP_ENUMERATE)) {
        *_retval = JS_FALSE;

        return NS_OK;
      }

      *objp = obj;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::Finalize(nsIXPConnectWrappedNative *wrapper, JSFreeOp *fop,
                     JSObject *obj)
{
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(sgo, NS_ERROR_UNEXPECTED);

  sgo->OnFinalize(obj);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::OuterObject(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                        JSObject * obj, JSObject * *_retval)
{
  nsGlobalWindow *origWin = nsGlobalWindow::FromWrapper(wrapper);
  nsGlobalWindow *win = origWin->GetOuterWindowInternal();

  if (!win) {
    // If we no longer have an outer window. No code should ever be
    // running on a window w/o an outer, which means this hook should
    // never be called when we have no outer. But just in case, return
    // null to prevent leaking an inner window to code in a different
    // window.
    *_retval = nullptr;
    return NS_ERROR_UNEXPECTED;
  }

  JS::Rooted<JSObject*> winObj(cx, win->FastGetGlobalJSObject());
  MOZ_ASSERT(winObj);

  // Note that while |wrapper| is same-compartment with cx, the outer window
  // might not be. If we're running script in an inactive scope and evalute
  // |this|, the outer window is actually a cross-compartment wrapper. So we
  // need to wrap here.
  if (!JS_WrapObject(cx, winObj.address())) {
    *_retval = nullptr;
    return NS_ERROR_UNEXPECTED;
  }

  *_retval = winObj;
  return NS_OK;
}

// DOM Location helper

NS_IMETHODIMP
nsLocationSH::CheckAccess(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, jsid id, uint32_t mode,
                          jsval *vp, bool *_retval)
{
  if ((mode & JSACC_TYPEMASK) == JSACC_PROTO && (mode & JSACC_WRITE)) {
    // No setting location.__proto__, ever!

    // Let XPConnect know that the access was not granted.
    *_retval = false;

    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return nsDOMGenericSH::CheckAccess(wrapper, cx, obj, id, mode, vp, _retval);
}

NS_IMETHODIMP
nsLocationSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                        JSObject *globalObj, JSObject **parentObj)
{
  // window.location can be held onto by both evil pages that want to track the
  // user's progress on the web and bookmarklets that want to use the location
  // object. Parent it to the outer window so that access checks do the Right
  // Thing.
  *parentObj = globalObj;

  nsCOMPtr<nsIDOMLocation> safeLoc(do_QueryInterface(nativeObj));
  if (!safeLoc) {
    // Oops, this wasn't really a location object. This can happen if someone
    // tries to use our scriptable helper as a real object and tries to wrap
    // it, see bug 319296
    return NS_OK;
  }

  nsLocation *loc = (nsLocation *)safeLoc.get();
  nsIDocShell *ds = loc->GetDocShell();
  if (!ds) {
    NS_WARNING("Refusing to create a location in the wrong scope");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_GetInterface(ds);
  if (!sgo) {
    NS_WARNING("Refusing to create a location in the wrong scope because the "
               "docshell is being destroyed");
    return NS_ERROR_UNEXPECTED;
  }

  *parentObj = sgo->GetGlobalJSObject();
  return *parentObj ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocationSH::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, jsid aId, jsval *vp, bool *_retval)
{
  // Shadowing protection. This will go away when nsLocation moves to the new
  // bindings.
  JS::Rooted<jsid> id(cx, aId);
  if (wrapper->HasNativeMember(id)) {
    JS_ReportError(cx, "Permission denied to shadow native property");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// DOM Navigator helper

NS_IMETHODIMP
nsNavigatorSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *aObj, jsid aId, uint32_t flags,
                          JSObject **objp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  if (!JSID_IS_STRING(id)) {
    return NS_OK;
  }

  nsScriptNameSpaceManager *nameSpaceManager =
    nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nsDependentJSString name(id);

  const nsGlobalNameStruct* name_struct =
    nameSpaceManager->LookupNavigatorName(name);
  if (!name_struct) {
    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeNewDOMBinding) {
    mozilla::dom::ConstructNavigatorProperty construct = name_struct->mConstructNavigatorProperty;
    MOZ_ASSERT(construct);

    JS::Rooted<JSObject*> naviObj(cx, js::CheckedUnwrap(obj, /* stopAtOuter = */ false));
    NS_ENSURE_TRUE(naviObj, NS_ERROR_DOM_SECURITY_ERR);

    JS::Rooted<JSObject*> domObject(cx);
    {
      JSAutoCompartment ac(cx, naviObj);

      // Check whether our constructor is enabled after we unwrap Xrays, since
      // we don't want to define an interface on the Xray if it's disabled in
      // the target global, even if it's enabled in the Xray's global.
      if (name_struct->mConstructorEnabled &&
          !(*name_struct->mConstructorEnabled)(cx, naviObj)) {
        return NS_OK;
      }

      domObject = construct(cx, naviObj);
      if (!domObject) {
        return NS_ERROR_FAILURE;
      }
    }

    if (!JS_WrapObject(cx, domObject.address()) ||
        !JS_DefinePropertyById(cx, obj, id,
                               JS::ObjectValue(*domObject),
                               nullptr, nullptr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    *_retval = true;
    *objp = obj;

    return NS_OK;
  }

  NS_ASSERTION(name_struct->mType == nsGlobalNameStruct::eTypeNavigatorProperty,
               "unexpected type");

  nsresult rv = NS_OK;

  nsCOMPtr<nsISupports> native(do_CreateInstance(name_struct->mCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JS::Value> prop_val(cx, JS::UndefinedValue()); // Property value.

  nsCOMPtr<nsIDOMGlobalPropertyInitializer> gpi(do_QueryInterface(native));

  if (gpi) {
    nsCOMPtr<nsIDOMNavigator> navigator = do_QueryWrappedNative(wrapper);
    nsIDOMWindow *window = static_cast<Navigator*>(navigator.get())->GetWindow();
    NS_ENSURE_TRUE(window, NS_ERROR_UNEXPECTED);

    rv = gpi->Init(window, prop_val.address());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (JSVAL_IS_PRIMITIVE(prop_val) && !JSVAL_IS_NULL(prop_val)) {
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, obj, native, true, prop_val.address(),
                    getter_AddRefs(holder));

    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!JS_WrapValue(cx, prop_val.address())) {
    return NS_ERROR_UNEXPECTED;
  }

  JSBool ok = ::JS_DefinePropertyById(cx, obj, id, prop_val,
                                      JS_PropertyStub, JS_StrictPropertyStub,
                                      JSPROP_ENUMERATE);

  *_retval = true;
  *objp = obj;

  return ok ? NS_OK : NS_ERROR_FAILURE;
}

// static
nsresult
nsNavigatorSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                         JSObject *globalObj, JSObject **parentObj)
{
  // window.navigator can hold expandos and thus we need to only ever
  // create one wrapper per navigator object so that expandos are
  // visible independently of who's looking it up.
  *parentObj = globalObj;

  nsCOMPtr<nsIDOMNavigator> safeNav(do_QueryInterface(nativeObj));
  if (!safeNav) {
    // Oops, this wasn't really a navigator object. This can happen if someone
    // tries to use our scriptable helper as a real object and tries to wrap
    // it, see bug 319296.
    return NS_OK;
  }

  Navigator *nav = static_cast<Navigator*>(safeNav.get());
  nsGlobalWindow *win = static_cast<nsGlobalWindow*>(nav->GetWindow());
  if (!win) {
    NS_WARNING("Refusing to create a navigator in the wrong scope");

    return NS_ERROR_UNEXPECTED;
  }
  return SetParentToWindow(win, parentObj);
}

// DOM Node helper

NS_IMETHODIMP
nsNodeSH::PreCreate(nsISupports *nativeObj, JSContext *cx, JSObject *aGlobalObj,
                    JSObject **parentObj)
{
  JS::Rooted<JSObject*> globalObj(cx, aGlobalObj);
  nsINode *node = static_cast<nsINode*>(nativeObj);

#ifdef DEBUG
  {
    nsCOMPtr<nsINode> node_qi(do_QueryInterface(nativeObj));

    // If this assertion fires the QI implementation for the object in
    // question doesn't use the nsINode pointer as the nsISupports
    // pointer. That must be fixed, or we'll crash...
    NS_ABORT_IF_FALSE(node_qi == node, "Uh, fix QI!");
  }
#endif

  // Make sure that we get the owner document of the content node, in case
  // we're in document teardown.  If we are, it's important to *not* use
  // globalObj as the nodes parent since that would give the node the
  // principal of globalObj (i.e. the principal of the document that's being
  // loaded) and not the principal of the document that's being unloaded.
  // See http://bugzilla.mozilla.org/show_bug.cgi?id=227417
  nsIDocument* doc = node->OwnerDoc();

  nsINode *native_parent;

  bool nodeIsElement = node->IsElement();
  if (nodeIsElement && node->AsElement()->IsXUL()) {
    // For XUL elements, use the parent, if any.
    native_parent = node->GetParent();

    if (!native_parent) {
      native_parent = doc;
    }
  } else if (!node->IsNodeOfType(nsINode::eDOCUMENT)) {
    NS_ASSERTION(node->IsNodeOfType(nsINode::eCONTENT) ||
                 node->IsNodeOfType(nsINode::eATTRIBUTE),
                 "Unexpected node type");

    // For attributes and non-XUL content, use the document as scope parent.
    native_parent = doc;

    // But for HTML form controls, use the form as scope parent.
    if (nodeIsElement) {
      if (node->IsNodeOfType(nsINode::eHTML_FORM_CONTROL)) {
        nsCOMPtr<nsIFormControl> form_control(do_QueryInterface(node));

        if (form_control) {
          Element *form = form_control->GetFormElement();

          if (form) {
            // Found a form, use it.
            native_parent = form;
          }
        }
      }
      else {
        // Legend isn't an HTML form control but should have its fieldset form
        // as scope parent at least for backward compatibility.
        HTMLLegendElement *legend =
          HTMLLegendElement::FromContent(node->AsElement());
        if (legend) {
          Element *form = legend->GetFormElement();

          if (form) {
            native_parent = form;
          }
        }
      }
    }
  } else {
    // We're called for a document object; set the parent to be the
    // document's global object

    // Document should know its global but if the owner window of the
    // document is already dead at this point, then just throw.
    nsIGlobalObject* scope = doc->GetScopeObject();
    NS_ENSURE_TRUE(scope, NS_ERROR_UNEXPECTED);

    *parentObj = scope->GetGlobalJSObject();
    // Guarding against the case when the native global is still alive
    // but the JS global is not.
    NS_ENSURE_TRUE(*parentObj, NS_ERROR_UNEXPECTED);

    // No slim wrappers for a document's scope object.
    return node->ChromeOnlyAccess() ?
      NS_SUCCESS_CHROME_ACCESS_ONLY : NS_OK;
  }

  // XXXjst: Maybe we need to find the global to use from the
  // nsIScriptGlobalObject that's reachable from the node we're about
  // to wrap here? But that's not always reachable, let's use
  // globalObj for now...

  nsresult rv = WrapNativeParent(cx, globalObj, native_parent, parentObj);
  NS_ENSURE_SUCCESS(rv, rv);

  return node->ChromeOnlyAccess() ? NS_SUCCESS_CHROME_ACCESS_ONLY : NS_OK;
}

NS_IMETHODIMP
nsNodeSH::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                      JSObject *obj, jsid id, jsval *vp, bool *_retval)
{
  nsNodeSH::PreserveWrapper(GetNative(wrapper, obj));
  return NS_OK;
}

NS_IMETHODIMP
nsNodeSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JSObject *aObj, jsid aId, uint32_t flags,
                     JSObject **objp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  if (id == sOnload_id || id == sOnerror_id) {
    // Make sure that this node can't go away while waiting for a
    // network load that could fire an event handler.
    // XXXbz won't this fail if the listener is added using
    // addEventListener?  On the other hand, even if I comment this
    // code out I can't seem to reproduce the bug it was trying to
    // fix....
    nsNodeSH::PreserveWrapper(GetNative(wrapper, obj));
  }

  return nsDOMGenericSH::NewResolve(wrapper, cx, obj, id, flags, objp,
                                    _retval);
}

NS_IMETHODIMP
nsNodeSH::GetFlags(uint32_t *aFlags)
{
  *aFlags = DOMCLASSINFO_STANDARD_FLAGS | nsIClassInfo::CONTENT_NODE;

  return NS_OK;
}

void
nsNodeSH::PreserveWrapper(nsISupports *aNative)
{
  nsINode *node = static_cast<nsINode*>(aNative);
  nsContentUtils::PreserveWrapper(aNative, node);
}

// EventTarget helper

NS_IMETHODIMP
nsEventTargetSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                           JSObject *aGlobalObj, JSObject **parentObj)
{
  JS::Rooted<JSObject*> globalObj(cx, aGlobalObj);
  nsDOMEventTargetHelper *target =
    nsDOMEventTargetHelper::FromSupports(nativeObj);

  nsCOMPtr<nsIScriptGlobalObject> native_parent;
  target->GetParentObject(getter_AddRefs(native_parent));

  *parentObj = native_parent ? native_parent->GetGlobalJSObject() : globalObj;

  return *parentObj ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsEventTargetSH::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, jsid id, jsval *vp, bool *_retval)
{
  nsEventTargetSH::PreserveWrapper(GetNative(wrapper, obj));

  return NS_OK;
}

void
nsEventTargetSH::PreserveWrapper(nsISupports *aNative)
{
  nsDOMEventTargetHelper *target =
    nsDOMEventTargetHelper::FromSupports(aNative);
  nsContentUtils::PreserveWrapper(aNative, target);
}

// Event helper

NS_IMETHODIMP
nsEventSH::PreCreate(nsISupports* aNativeObj, JSContext* aCx,
                     JSObject* aGlobalObj, JSObject** aParentObj)
{
  JS::Rooted<JSObject*> globalObj(aCx, aGlobalObj);
  nsDOMEvent* event =
    nsDOMEvent::FromSupports(aNativeObj);

  nsCOMPtr<nsIScriptGlobalObject> native_parent;
  event->GetParentObject(getter_AddRefs(native_parent));

  *aParentObj = native_parent ? native_parent->GetGlobalJSObject() : globalObj;

  return *aParentObj ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsEventSH::AddProperty(nsIXPConnectWrappedNative* aWrapper, JSContext* aCx,
                       JSObject* aObj, jsid Id, jsval* aVp, bool* aRetval)
{
  nsEventSH::PreserveWrapper(GetNative(aWrapper, aObj));
  return NS_OK;
}

void
nsEventSH::PreserveWrapper(nsISupports* aNative)
{
  nsDOMEvent* event =
    nsDOMEvent::FromSupports(aNative);
  nsContentUtils::PreserveWrapper(aNative, event);
}

// IDBEventTarget helper

NS_IMETHODIMP
IDBEventTargetSH::PreCreate(nsISupports *aNativeObj, JSContext *aCx,
                            JSObject *aGlobalObj, JSObject **aParentObj)
{
  JS::Rooted<JSObject*> globalObj(aCx, aGlobalObj);
  IDBWrapperCache *target = IDBWrapperCache::FromSupports(aNativeObj);
  JSObject *parent = target->GetParentObject();
  *aParentObj = parent ? parent : globalObj;
  return NS_OK;
}

// Element helper

NS_IMETHODIMP
nsElementSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj)
{
  nsresult rv = nsNodeSH::PreCreate(nativeObj, cx, globalObj, parentObj);
  NS_ENSURE_SUCCESS(rv, rv);

  Element *element = static_cast<Element*>(nativeObj);

#ifdef DEBUG
  {
    nsCOMPtr<nsIContent> content_qi(do_QueryInterface(nativeObj));

    // If this assertion fires the QI implementation for the object in
    // question doesn't use the nsIContent pointer as the nsISupports
    // pointer. That must be fixed, or we'll crash...
    NS_ABORT_IF_FALSE(content_qi == element, "Uh, fix QI!");
  }
#endif

  nsIDocument *doc = element->HasFlag(NODE_FORCE_XBL_BINDINGS) ?
                     element->OwnerDoc() :
                     element->GetCurrentDoc();

  if (!doc) {
    return rv;
  }

  if (element->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR) &&
      doc->BindingManager()->GetBinding(element)) {
    return rv;
  }

  mozilla::css::URLValue *bindingURL;
  bool ok = element->GetBindingURL(doc, &bindingURL);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  // Only allow slim wrappers if there's no binding.
  if (!bindingURL) {
    return rv;
  }

  element->SetFlags(NODE_ATTACH_BINDING_ON_POSTCREATE);

  return rv;
}

NS_IMETHODIMP
nsElementSH::PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj)
{
  Element *element = static_cast<Element*>(wrapper->Native());

#ifdef DEBUG
  {
    nsCOMPtr<nsIContent> content_qi(do_QueryWrappedNative(wrapper));

    // If this assertion fires the QI implementation for the object in
    // question doesn't use the nsIContent pointer as the nsISupports
    // pointer. That must be fixed, or we'll crash...
    NS_ABORT_IF_FALSE(content_qi == element, "Uh, fix QI!");
  }
#endif

  nsIDocument* doc;
  if (element->HasFlag(NODE_FORCE_XBL_BINDINGS)) {
    doc = element->OwnerDoc();
  }
  else {
    doc = element->GetCurrentDoc();
  }

  if (!doc) {
    // There's no baseclass that cares about this call so we just
    // return here.

    return NS_OK;
  }

  // We must ensure that the XBL Binding is installed before we hand
  // back this object.

  if (!element->HasFlag(NODE_ATTACH_BINDING_ON_POSTCREATE)) {
    // There's already a binding for this element so nothing left to
    // be done here.

    // In theory we could call ExecuteAttachedHandler here when it's safe to
    // run script if we also removed the binding from the PAQ queue, but that
    // seems like a scary change that would mosly just add more
    // inconsistencies.

    return NS_OK;
  }

  element->UnsetFlags(NODE_ATTACH_BINDING_ON_POSTCREATE);

  // Make sure the style context goes away _before_ we load the binding
  // since that can destroy the relevant presshell.
  mozilla::css::URLValue *bindingURL;
  bool ok = element->GetBindingURL(doc, &bindingURL);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  if (!bindingURL) {
    // No binding, nothing left to do here.
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri = bindingURL->GetURI();
  nsCOMPtr<nsIPrincipal> principal = bindingURL->mOriginPrincipal;

  // We have a binding that must be installed.
  bool dummy;

  nsXBLService* xblService = nsXBLService::GetInstance();
  NS_ENSURE_TRUE(xblService, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<nsXBLBinding> binding;
  xblService->LoadBindings(element, uri, principal, getter_AddRefs(binding), &dummy);

  if (binding) {
    if (nsContentUtils::IsSafeToRunScript()) {
      binding->ExecuteAttachedHandler();
    }
    else {
      nsContentUtils::AddScriptRunner(
        NS_NewRunnableMethod(binding, &nsXBLBinding::ExecuteAttachedHandler));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsElementSH::PostTransplant(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj)
{
  // XBL bindings are reapplied asynchronously when the node is inserted into a
  // new document and frame construction occurs.
  return NS_OK;
}


// Generic array scriptable helper.

NS_IMETHODIMP
nsGenericArraySH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *aObj, jsid aId, uint32_t flags,
                             JSObject **objp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  if (id == sLength_id) {
    // Bail early; this isn't something we're interested in
    return NS_OK;
  }

  bool is_number = false;
  int32_t n = GetArrayIndexFromId(cx, id, &is_number);

  if (is_number && n >= 0) {
    // XXX The following is a cheap optimization to avoid hitting xpconnect to
    // get the length. We may want to consider asking our concrete
    // implementation for the length, and falling back onto the GetProperty if
    // it doesn't provide one.

    uint32_t length;
    nsresult rv = GetLength(wrapper, cx, obj, &length);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t index = uint32_t(n);
    if (index < length) {
      *_retval = ::JS_DefineElement(cx, obj, index, JSVAL_VOID, nullptr, nullptr,
                                    JSPROP_ENUMERATE | JSPROP_SHARED);
      *objp = obj;
    }
  }

  return NS_OK;
}

nsresult
nsGenericArraySH::GetLength(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, uint32_t *length)
{
  *length = 0;

  JS::Rooted<JS::Value> lenval(cx);
  if (!JS_GetProperty(cx, obj, "length", lenval.address())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!JSVAL_IS_INT(lenval)) {
    // This can apparently happen with some sparse array impls falling back
    // onto this code.
    return NS_OK;
  }

  int32_t slen = JSVAL_TO_INT(lenval);
  if (slen < 0) {
    return NS_OK;
  }

  *length = (uint32_t)slen;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericArraySH::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *aObj, bool *_retval)
{
  // Recursion protection in case someone tries to be smart and call
  // the enumerate hook from a user defined .length getter, or
  // somesuch.

  JS::Rooted<JSObject*> obj(cx, aObj);
  static bool sCurrentlyEnumerating;

  if (sCurrentlyEnumerating) {
    // Don't recurse to death.
    return NS_OK;
  }

  sCurrentlyEnumerating = true;

  JS::Rooted<JS::Value> len_val(cx);
  JSBool ok = ::JS_GetProperty(cx, obj, "length", len_val.address());

  if (ok && JSVAL_IS_INT(len_val)) {
    int32_t length = JSVAL_TO_INT(len_val);

    for (int32_t i = 0; ok && i < length; ++i) {
      ok = ::JS_DefineElement(cx, obj, i, JSVAL_VOID, nullptr, nullptr,
                              JSPROP_ENUMERATE | JSPROP_SHARED);
    }
  }

  sCurrentlyEnumerating = false;

  return ok ? NS_OK : NS_ERROR_UNEXPECTED;
}

// Array scriptable helper

NS_IMETHODIMP
nsArraySH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *aObj, jsid aId, jsval *vp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  bool is_number = false;
  int32_t n = GetArrayIndexFromId(cx, id, &is_number);

  nsresult rv = NS_OK;

  if (is_number) {
    if (n < 0) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    // Make sure rv == NS_OK here, so GetItemAt implementations that never fail
    // don't have to set rv.
    rv = NS_OK;
    nsWrapperCache *cache = nullptr;
    nsISupports* array_item =
      GetItemAt(GetNative(wrapper, obj), n, &cache, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (array_item) {
      rv = WrapNative(cx, JS_GetGlobalForScopeChain(cx), array_item, cache,
                      true, vp);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = NS_SUCCESS_I_DID_SOMETHING;
    }
  }

  return rv;
}


// StringList scriptable helper

nsresult
nsStringListSH::GetStringAt(nsISupports *aNative, int32_t aIndex,
                            nsAString& aResult)
{
  nsCOMPtr<nsIDOMDOMStringList> list(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(list, NS_ERROR_UNEXPECTED);

  nsresult rv = list->Item(aIndex, aResult);
#ifdef DEBUG
  if (DOMStringIsNull(aResult)) {
    uint32_t length = 0;
    list->GetLength(&length);
    NS_ASSERTION(uint32_t(aIndex) >= length, "Item should only return null for out-of-bounds access");
  }
#endif
  return rv;
}


// Named Array helper

NS_IMETHODIMP
nsNamedArraySH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                           JSObject *aObj, jsid aId, uint32_t flags,
                           JSObject **objp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  if ((!(JSRESOLVE_ASSIGNING & flags)) && JSID_IS_STRING(id) &&
      !ObjectIsNativeWrapper(cx, obj)) {

    {
      JS::Rooted<JSObject*> realObj(cx, wrapper ? wrapper->GetJSObject() : obj);

      JSAutoCompartment ac(cx, realObj);
      JS::Rooted<JSObject*> proto(cx);
      if (!::JS_GetPrototype(cx, realObj, proto.address())) {
        return NS_ERROR_FAILURE;
      }

      if (proto) {
        JSBool hasProp;
        if (!::JS_HasPropertyById(cx, proto, id, &hasProp)) {
          *_retval = false;
          return NS_ERROR_FAILURE;
        }

        if (hasProp) {
          // We found the property we're resolving on the prototype,
          // nothing left to do here then.
          return NS_OK;
        }
      }
    }

    // Make sure rv == NS_OK here, so GetNamedItem implementations
    // that never fail don't have to set rv.
    nsresult rv = NS_OK;
    nsWrapperCache *cache;

    nsISupports* item = GetNamedItem(GetNative(wrapper, obj),
                                     nsDependentJSString(id), &cache, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (item) {
      *_retval = ::JS_DefinePropertyById(cx, obj, id, JSVAL_VOID, nullptr,
                                         nullptr, JSPROP_ENUMERATE | JSPROP_SHARED);

      *objp = obj;

      return *_retval ? NS_OK : NS_ERROR_FAILURE;
    }
  }

  return nsArraySH::NewResolve(wrapper, cx, obj, id, flags, objp, _retval);
}

NS_IMETHODIMP
nsNamedArraySH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *aObj, jsid aId, jsval *vp,
                            bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  if (JSID_IS_STRING(id) && !ObjectIsNativeWrapper(cx, obj)) {
    nsresult rv = NS_OK;
    nsWrapperCache *cache = nullptr;
    nsISupports* item = GetNamedItem(GetNative(wrapper, obj),
                                     nsDependentJSString(id), &cache, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (item) {
      rv = WrapNative(cx, JS_GetGlobalForScopeChain(cx), item, cache,
                      true, vp);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = NS_SUCCESS_I_DID_SOMETHING;
    }

    // Don't fall through to nsArraySH::GetProperty() here
    return rv;
  }

  return nsArraySH::GetProperty(wrapper, cx, obj, id, vp, _retval);
}


// HTMLAllCollection

JSClass sHTMLDocumentAllClass = {
  "HTML document.all class",
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_NEW_RESOLVE |
  JSCLASS_EMULATES_UNDEFINED | JSCLASS_HAS_RESERVED_SLOTS(1),
  JS_PropertyStub,                                         /* addProperty */
  JS_DeletePropertyStub,                                   /* delProperty */
  nsHTMLDocumentSH::DocumentAllGetProperty,                /* getProperty */
  JS_StrictPropertyStub,                                   /* setProperty */
  JS_EnumerateStub,
  (JSResolveOp)nsHTMLDocumentSH::DocumentAllNewResolve,
  JS_ConvertStub,
  nsHTMLDocumentSH::ReleaseDocument,
  nullptr,                                                  /* checkAccess */
  nsHTMLDocumentSH::CallToGetPropMapper
};


// static
JSBool
nsHTMLDocumentSH::GetDocumentAllNodeList(JSContext *cx,
                                         JS::Handle<JSObject*> obj,
                                         nsDocument *domdoc,
                                         nsContentList **nodeList)
{
  // The document.all object is a mix of the node list returned by
  // document.getElementsByTagName("*") and a map of elements in the
  // document exposed by their id and/or name. To make access to the
  // node list part (i.e. access to elements by index) not walk the
  // document each time, we create a nsContentList and hold on to it
  // in a reserved slot (0) on the document.all JSObject.
  nsresult rv = NS_OK;

  JS::Rooted<JS::Value> collection(cx, JS_GetReservedSlot(obj, 0));

  if (!JSVAL_IS_PRIMITIVE(collection)) {
    // We already have a node list in our reserved slot, use it.
    JS::Rooted<JSObject*> obj(cx, JSVAL_TO_OBJECT(collection));
    nsIHTMLCollection* htmlCollection;
    rv = mozilla::dom::UnwrapObject<nsIHTMLCollection>(cx, obj, htmlCollection);
    if (NS_SUCCEEDED(rv)) {
      NS_ADDREF(*nodeList = static_cast<nsContentList*>(htmlCollection));
    }
    else {
      nsISupports *native = nsDOMClassInfo::XPConnect()->GetNativeOfWrapper(cx, obj);
      if (native) {
        NS_ADDREF(*nodeList = nsContentList::FromSupports(native));
        rv = NS_OK;
      }
      else {
        rv = NS_ERROR_FAILURE;
      }
    }
  } else {
    // No node list for this document.all yet, create one...

    nsRefPtr<nsContentList> list =
      domdoc->GetElementsByTagName(NS_LITERAL_STRING("*"));
    if (!list) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult tmp = WrapNative(cx, JS_GetGlobalForScopeChain(cx),
                              static_cast<nsINodeList*>(list), list, false,
                              collection.address(), getter_AddRefs(holder));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }

    list.forget(nodeList);

    // ... and store it in our reserved slot.
    JS_SetReservedSlot(obj, 0, collection);
  }

  if (NS_FAILED(rv)) {
    xpc::Throw(cx, NS_ERROR_FAILURE);

    return JS_FALSE;
  }

  return *nodeList != nullptr;
}

JSBool
nsHTMLDocumentSH::DocumentAllGetProperty(JSContext *cx, JSHandleObject obj_,
                                         JSHandleId id, JS::MutableHandle<JS::Value> vp)
{
  JS::Rooted<JSObject*> obj(cx, obj_);

  // document.all.item and .namedItem get their value in the
  // newResolve hook, so nothing to do for those properties here. And
  // we need to return early to prevent <div id="item"> from shadowing
  // document.all.item(), etc.
  if (nsDOMClassInfo::sItem_id == id || nsDOMClassInfo::sNamedItem_id == id) {
    return JS_TRUE;
  }

  JS::Rooted<JSObject*> proto(cx);
  while (js::GetObjectJSClass(obj) != &sHTMLDocumentAllClass) {
    if (!js::GetObjectProto(cx, obj, &proto)) {
      return JS_FALSE;
    }

    if (!proto) {
      NS_ERROR("The JS engine lies!");
      return JS_TRUE;
    }

    obj = proto;
  }

  nsHTMLDocument *doc = GetDocument(obj);
  nsISupports *result;
  nsWrapperCache *cache;
  nsresult rv = NS_OK;

  if (JSID_IS_STRING(id)) {
    if (nsDOMClassInfo::sLength_id == id) {
      // Map document.all.length to the length of the collection
      // document.getElementsByTagName("*"), and make sure <div
      // id="length"> doesn't shadow document.all.length.

      nsRefPtr<nsContentList> nodeList;
      if (!GetDocumentAllNodeList(cx, obj, doc, getter_AddRefs(nodeList))) {
        return JS_FALSE;
      }

      uint32_t length;
      rv = nodeList->GetLength(&length);

      if (NS_FAILED(rv)) {
        xpc::Throw(cx, rv);

        return JS_FALSE;
      }

      vp.set(INT_TO_JSVAL(length));

      return JS_TRUE;
    }

    // For all other strings, look for an element by id or name.
    nsDependentJSString str(id);
    result = doc->GetDocumentAllResult(str, &cache, &rv);

    if (NS_FAILED(rv)) {
      xpc::Throw(cx, rv);
      return JS_FALSE;
    }
  } else if (JSID_IS_INT(id) && JSID_TO_INT(id) >= 0) {
    // Map document.all[n] (where n is a number) to the n:th item in
    // the document.all node list.

    nsRefPtr<nsContentList> nodeList;
    if (!GetDocumentAllNodeList(cx, obj, doc, getter_AddRefs(nodeList))) {
      return JS_FALSE;
    }

    nsIContent *node = nodeList->Item(JSID_TO_INT(id));

    result = node;
    cache = node;
  } else {
    result = nullptr;
  }

  if (result) {
    rv = WrapNative(cx, JS_GetGlobalForScopeChain(cx), result, cache, true, vp.address());
    if (NS_FAILED(rv)) {
      xpc::Throw(cx, rv);

      return JS_FALSE;
    }
  } else {
    vp.setUndefined();
  }

  return JS_TRUE;
}

JSBool
nsHTMLDocumentSH::DocumentAllNewResolve(JSContext *cx, JSHandleObject obj, JSHandleId id,
                                        unsigned flags, JS::MutableHandle<JSObject*> objp)
{
  JS::RootedValue v(cx);

  if (nsDOMClassInfo::sItem_id == id || nsDOMClassInfo::sNamedItem_id == id) {
    // Define the item() or namedItem() method.

    JSFunction *fnc = ::JS_DefineFunctionById(cx, obj, id, CallToGetPropMapper,
                                              0, JSPROP_ENUMERATE);
    objp.set(obj);

    return fnc != nullptr;
  }

  if (nsDOMClassInfo::sLength_id == id) {
    // document.all.length. Any jsval other than undefined would do
    // here, all we need is to get into the code below that defines
    // this propery on obj, the rest happens in
    // DocumentAllGetProperty().

    v = JSVAL_ONE;
  } else {
    if (!DocumentAllGetProperty(cx, obj, id, &v)) {
      return JS_FALSE;
    }
  }

  JSBool ok = JS_TRUE;

  if (v.get() != JSVAL_VOID) {
    ok = ::JS_DefinePropertyById(cx, obj, id, v, nullptr, nullptr, 0);
    objp.set(obj);
  }

  return ok;
}

void
nsHTMLDocumentSH::ReleaseDocument(JSFreeOp *fop, JSObject *obj)
{
  nsIHTMLDocument* doc = GetDocument(obj);
  if (doc) {
    xpc::DeferredRelease(doc);
  }
}

JSBool
nsHTMLDocumentSH::CallToGetPropMapper(JSContext *cx, unsigned argc, jsval *vp)
{
  // Handle document.all("foo") style access to document.all.

  if (argc != 1) {
    // XXX: Should throw NS_ERROR_XPC_NOT_ENOUGH_ARGS for argc < 1,
    // and create a new NS_ERROR_XPC_TOO_MANY_ARGS for argc > 1? IE
    // accepts nothing other than one arg.
    xpc::Throw(cx, NS_ERROR_INVALID_ARG);

    return JS_FALSE;
  }

  // Convert all types to string.
  JS::Rooted<JSString*> str(cx, ::JS_ValueToString(cx, JS_ARGV(cx, vp)[0]));
  if (!str) {
    return JS_FALSE;
  }

  // If we are called via document.all(id) instead of document.all.item(i) or
  // another method, use the document.all callee object as self.
  JSObject *self;
  JS::Value callee = JS_CALLEE(cx, vp);
  if (callee.isObject() &&
      JS_GetClass(&callee.toObject()) == &sHTMLDocumentAllClass) {
    self = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
  } else {
    self = JS_THIS_OBJECT(cx, vp);
    if (!self)
      return JS_FALSE;
  }

  size_t length;
  JS::Anchor<JSString *> anchor(str);
  const jschar *chars = ::JS_GetStringCharsAndLength(cx, str, &length);
  if (!chars) {
    return JS_FALSE;
  }

  return ::JS_GetUCProperty(cx, self, chars, length, vp);
}


// HTMLFormElement helper

NS_IMETHODIMP
nsHTMLFormElementSH::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                JSContext *cx, JSObject *aObj, jsid aId,
                                uint32_t flags, JSObject **objp,
                                bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  // For native wrappers, do not resolve random names on form
  if ((!(JSRESOLVE_ASSIGNING & flags)) && JSID_IS_STRING(id) &&
      (!ObjectIsNativeWrapper(cx, obj) ||
       xpc::WrapperFactory::XrayWrapperNotShadowing(obj, id))) {
    nsCOMPtr<nsIForm> form(do_QueryWrappedNative(wrapper, obj));

    nsDependentJSString name(id);
    nsWrapperCache* cache;
    nsCOMPtr<nsISupports> result =
      static_cast<nsHTMLFormElement*>(form.get())->FindNamedItem(name, &cache);

    if (result) {
      *_retval = ::JS_DefinePropertyById(cx, obj, id, JSVAL_VOID, nullptr,
                                         nullptr, JSPROP_ENUMERATE);

      *objp = obj;

      return *_retval ? NS_OK : NS_ERROR_FAILURE;
    }
  }

  return nsElementSH::NewResolve(wrapper, cx, obj, id, flags, objp, _retval);
}


NS_IMETHODIMP
nsHTMLFormElementSH::GetProperty(nsIXPConnectWrappedNative *wrapper,
                                 JSContext *cx, JSObject *aObj, jsid aId,
                                 jsval *vp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  nsCOMPtr<nsIForm> form(do_QueryWrappedNative(wrapper, obj));

  if (JSID_IS_STRING(id)) {
    // For native wrappers, do not get random names on form
    nsDependentJSString name(id);
    nsWrapperCache* cache;
    nsCOMPtr<nsISupports> result =
      static_cast<nsHTMLFormElement*>(form.get())->FindNamedItem(name, &cache);

    if (result) {
      // Wrap result, result can be either an element or a list of
      // elements
      nsresult rv = WrapNative(cx, obj, result, cache, true, vp);
      return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
    }
  } else {
    int32_t n = GetArrayIndexFromId(cx, id);

    if (n >= 0) {
      nsIFormControl* control = form->GetElementAt(n);

      if (control) {
        Element *element =
          static_cast<nsGenericHTMLFormElement*>(form->GetElementAt(n));
        nsresult rv = WrapNative(cx, JS_GetGlobalForScopeChain(cx), element,
                                 element, true, vp);
        return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElementSH::NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                                  JSContext *cx, JSObject *obj,
                                  uint32_t enum_op, jsval *statep,
                                  jsid *idp, bool *_retval)
{
  switch (enum_op) {
  case JSENUMERATE_INIT:
  case JSENUMERATE_INIT_ALL:
    {
      nsCOMPtr<nsIForm> form(do_QueryWrappedNative(wrapper, obj));

      if (!form) {
        *statep = JSVAL_NULL;
        return NS_ERROR_UNEXPECTED;
      }

      *statep = INT_TO_JSVAL(0);

      if (idp) {
        uint32_t count = form->GetElementCount();

        *idp = INT_TO_JSID(count);
      }

      break;
    }
  case JSENUMERATE_NEXT:
    {
      nsCOMPtr<nsIForm> form(do_QueryWrappedNative(wrapper, obj));
      NS_ENSURE_TRUE(form, NS_ERROR_FAILURE);

      int32_t index = (int32_t)JSVAL_TO_INT(*statep);

      uint32_t count = form->GetElementCount();

      if ((uint32_t)index < count) {
        nsIFormControl* controlNode = form->GetElementAt(index);
        NS_ENSURE_TRUE(controlNode, NS_ERROR_FAILURE);

        nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(controlNode);
        NS_ENSURE_TRUE(domElement, NS_ERROR_FAILURE);

        nsAutoString attr;
        domElement->GetAttribute(NS_LITERAL_STRING("name"), attr);
        if (attr.IsEmpty()) {
          // If name is not there, use index instead
          attr.AppendInt(index);
        }

        JSString *jsname =
          JS_NewUCStringCopyN(cx, reinterpret_cast<const jschar *>
                                                  (attr.get()),
                              attr.Length());
        NS_ENSURE_TRUE(jsname, NS_ERROR_OUT_OF_MEMORY);

        JS_ValueToId(cx, STRING_TO_JSVAL(jsname), idp);

        *statep = INT_TO_JSVAL(++index);
      } else {
        *statep = JSVAL_NULL;
      }

      break;
    }
  case JSENUMERATE_DESTROY:
    *statep = JSVAL_NULL;

    break;
  }

  return NS_OK;
}


// Plugin helper

nsISupports*
nsPluginSH::GetItemAt(nsISupports *aNative, uint32_t aIndex,
                      nsWrapperCache **aCache, nsresult *aResult)
{
  nsPluginElement* plugin = nsPluginElement::FromSupports(aNative);

  return plugin->GetItemAt(aIndex, aResult);
}

nsISupports*
nsPluginSH::GetNamedItem(nsISupports *aNative, const nsAString& aName,
                         nsWrapperCache **aCache, nsresult *aResult)
{
  nsPluginElement* plugin = nsPluginElement::FromSupports(aNative);

  return plugin->GetNamedItem(aName, aResult);
}


// PluginArray helper

nsISupports*
nsPluginArraySH::GetItemAt(nsISupports *aNative, uint32_t aIndex,
                           nsWrapperCache **aCache, nsresult *aResult)
{
  nsPluginArray* array = nsPluginArray::FromSupports(aNative);

  return array->GetItemAt(aIndex, aResult);
}

nsISupports*
nsPluginArraySH::GetNamedItem(nsISupports *aNative, const nsAString& aName,
                              nsWrapperCache **aCache, nsresult *aResult)
{
  nsPluginArray* array = nsPluginArray::FromSupports(aNative);

  return array->GetNamedItem(aName, aResult);
}


// MimeTypeArray helper

nsISupports*
nsMimeTypeArraySH::GetItemAt(nsISupports *aNative, uint32_t aIndex,
                             nsWrapperCache **aCache, nsresult *aResult)
{
  nsMimeTypeArray* array = nsMimeTypeArray::FromSupports(aNative);

  return array->GetItemAt(aIndex, aResult);
}

nsISupports*
nsMimeTypeArraySH::GetNamedItem(nsISupports *aNative, const nsAString& aName,
                                nsWrapperCache **aCache, nsresult *aResult)
{
  nsMimeTypeArray* array = nsMimeTypeArray::FromSupports(aNative);

  return array->GetNamedItem(aName, aResult);
}


// StringArray helper

NS_IMETHODIMP
nsStringArraySH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *aObj, jsid aId, jsval *vp,
                             bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  bool is_number = false;
  int32_t n = GetArrayIndexFromId(cx, id, &is_number);

  if (!is_number) {
    return NS_OK;
  }

  nsAutoString val;

  nsresult rv = GetStringAt(GetNative(wrapper, obj), n, val);
  NS_ENSURE_SUCCESS(rv, rv);

  if (DOMStringIsNull(val)) {
    *vp = JSVAL_VOID;
    return NS_SUCCESS_I_DID_SOMETHING;
  }

  NS_ENSURE_TRUE(xpc::NonVoidStringToJsval(cx, val, vp),
                 NS_ERROR_OUT_OF_MEMORY);
  return NS_SUCCESS_I_DID_SOMETHING;
}


// History helper

NS_IMETHODIMP
nsHistorySH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj)
{
  nsHistory *history = (nsHistory *)((nsIDOMHistory*)nativeObj);
  nsCOMPtr<nsPIDOMWindow> innerWindow;
  history->GetWindow(getter_AddRefs(innerWindow));
  if (!innerWindow) {
    NS_WARNING("refusing to create history object in the wrong scope");
    return NS_ERROR_FAILURE;
  }
  return SetParentToWindow(static_cast<nsGlobalWindow *>(innerWindow.get()),
                           parentObj);
}

NS_IMETHODIMP
nsHistorySH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *aObj, jsid aId, jsval *vp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  bool is_number = false;
  GetArrayIndexFromId(cx, id, &is_number);

  if (!is_number) {
    return NS_OK;
  }

  return nsStringArraySH::GetProperty(wrapper, cx, obj, id, vp, _retval);
}

nsresult
nsHistorySH::GetStringAt(nsISupports *aNative, int32_t aIndex,
                         nsAString& aResult)
{
  if (aIndex < 0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMHistory> history(do_QueryInterface(aNative));

  nsresult rv = history->Item(aIndex, aResult);
#ifdef DEBUG
  if (DOMStringIsNull(aResult)) {
    int32_t length = 0;
    history->GetLength(&length);
    NS_ASSERTION(aIndex >= length, "Item should only return null for out-of-bounds access");
  }
#endif
  return rv;
}


// MediaList helper

nsresult
nsMediaListSH::GetStringAt(nsISupports *aNative, int32_t aIndex,
                           nsAString& aResult)
{
  if (aIndex < 0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMMediaList> media_list(do_QueryInterface(aNative));

  nsresult rv = media_list->Item(uint32_t(aIndex), aResult);
#ifdef DEBUG
  if (DOMStringIsNull(aResult)) {
    uint32_t length = 0;
    media_list->GetLength(&length);
    NS_ASSERTION(uint32_t(aIndex) >= length, "Item should only return null for out-of-bounds access");
  }
#endif
  return rv;
}


// StyleSheetList helper

nsISupports*
nsStyleSheetListSH::GetItemAt(nsISupports *aNative, uint32_t aIndex,
                              nsWrapperCache **aCache, nsresult *rv)
{
  nsDOMStyleSheetList* list = nsDOMStyleSheetList::FromSupports(aNative);

  return list->GetItemAt(aIndex);
}


// CSSRuleList scriptable helper

nsISupports*
nsCSSRuleListSH::GetItemAt(nsISupports *aNative, uint32_t aIndex,
                           nsWrapperCache **aCache, nsresult *aResult)
{
  nsICSSRuleList* list = static_cast<nsICSSRuleList*>(aNative);
#ifdef DEBUG
  {
    nsCOMPtr<nsICSSRuleList> list_qi = do_QueryInterface(aNative);

    // If this assertion fires the QI implementation for the object in
    // question doesn't use the nsICSSRuleList pointer as the nsISupports
    // pointer. That must be fixed, or we'll crash...
    NS_ABORT_IF_FALSE(list_qi == list, "Uh, fix QI!");
  }
#endif

  return list->GetItemAt(aIndex, aResult);
}


// Storage2SH

// One reason we need a newResolve hook is that in order for
// enumeration of storage object keys to work the keys we're
// enumerating need to exist on the storage object for the JS engine
// to find them.

NS_IMETHODIMP
nsStorage2SH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid aId, uint32_t flags,
                         JSObject **objp, bool *_retval)
{
  JS::Rooted<jsid> id(cx, aId);
  if (ObjectIsNativeWrapper(cx, obj)) {
    return NS_OK;
  }

  JS::Rooted<JSObject*> realObj(cx, wrapper->GetJSObject());

  JSAutoCompartment ac(cx, realObj);

  // First check to see if the property is defined on our prototype,
  // after converting id to a string if it's an integer.

  JS::Rooted<JSString*> jsstr(cx, IdToString(cx, id));
  if (!jsstr) {
    return NS_OK;
  }

  JS::Rooted<JSObject*> proto(cx);
  if (!::JS_GetPrototype(cx, realObj, proto.address())) {
    return NS_ERROR_FAILURE;
  }
  JSBool hasProp;

  if (proto &&
      (::JS_HasPropertyById(cx, proto, id, &hasProp) &&
       hasProp)) {
    // We found the property we're resolving on the prototype,
    // nothing left to do here then.

    return NS_OK;
  }

  // We're resolving property that doesn't exist on the prototype,
  // check if the key exists in the storage object.

  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));

  nsDependentJSString depStr;
  NS_ENSURE_TRUE(depStr.init(cx, jsstr), NS_ERROR_UNEXPECTED);

  // GetItem() will return null if the caller can't access the session
  // storage item.
  nsAutoString data;
  nsresult rv = storage->GetItem(depStr, data);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!DOMStringIsNull(data)) {
    if (!::JS_DefinePropertyById(cx, realObj, id, JSVAL_VOID, nullptr,
                                 nullptr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    *objp = realObj;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsStorage2SH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *aObj, jsid aId, jsval *vp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);

  JSString* key = IdToString(cx, id);
  NS_ENSURE_TRUE(key, NS_ERROR_UNEXPECTED);

  nsDependentJSString keyStr;
  NS_ENSURE_TRUE(keyStr.init(cx, key), NS_ERROR_UNEXPECTED);

  // For native wrappers, do not get random names on storage objects.
  if (ObjectIsNativeWrapper(cx, obj)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoString val;
  nsresult rv = storage->GetItem(keyStr, val);
  NS_ENSURE_SUCCESS(rv, rv);

  if (DOMStringIsNull(val)) {
    // No such key.
    *vp = JSVAL_VOID;
  } else {
    JSString* str =
      JS_NewUCStringCopyN(cx, static_cast<const jschar *>(val.get()),
                          val.Length());
    NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

    *vp = STRING_TO_JSVAL(str);
  }

  return NS_SUCCESS_I_DID_SOMETHING;
}

NS_IMETHODIMP
nsStorage2SH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                          JSContext *cx, JSObject *obj, jsid aId,
                          jsval *vp, bool *_retval)
{
  JS::Rooted<jsid> id(cx, aId);
  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);

  JSString *key = IdToString(cx, id);
  NS_ENSURE_TRUE(key, NS_ERROR_UNEXPECTED);

  nsDependentJSString keyStr;
  NS_ENSURE_TRUE(keyStr.init(cx, key), NS_ERROR_UNEXPECTED);

  JSString *value = ::JS_ValueToString(cx, *vp);
  NS_ENSURE_TRUE(value, NS_ERROR_UNEXPECTED);

  nsDependentJSString valueStr;
  NS_ENSURE_TRUE(valueStr.init(cx, value), NS_ERROR_UNEXPECTED);

  nsresult rv = storage->SetItem(keyStr, valueStr);
  if (NS_SUCCEEDED(rv)) {
    rv = NS_SUCCESS_I_DID_SOMETHING;
  }

  return rv;
}

NS_IMETHODIMP
nsStorage2SH::DelProperty(nsIXPConnectWrappedNative *wrapper,
                          JSContext *cx, JSObject *obj, jsid aId,
                          bool *_retval)
{
  JS::Rooted<jsid> id(cx, aId);
  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);

  JSString *key = IdToString(cx, id);
  NS_ENSURE_TRUE(key, NS_ERROR_UNEXPECTED);

  nsDependentJSString keyStr;
  NS_ENSURE_TRUE(keyStr.init(cx, key), NS_ERROR_UNEXPECTED);

  nsresult rv = storage->RemoveItem(keyStr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *_retval = true;
  return NS_SUCCESS_I_DID_SOMETHING;
}


NS_IMETHODIMP
nsStorage2SH::NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                           JSObject *obj, uint32_t enum_op, jsval *statep,
                           jsid *idp, bool *_retval)
{
  if (enum_op == JSENUMERATE_INIT || enum_op == JSENUMERATE_INIT_ALL) {
    nsCOMPtr<nsPIDOMStorage> storage(do_QueryWrappedNative(wrapper));

    // XXXndeakin need to free the keys afterwards
    nsTArray<nsString> *keys = storage->GetKeys();
    NS_ENSURE_TRUE(keys, NS_ERROR_OUT_OF_MEMORY);

    *statep = PRIVATE_TO_JSVAL(keys);

    if (idp) {
      *idp = INT_TO_JSID(keys->Length());
    }
    return NS_OK;
  }

  nsTArray<nsString> *keys =
    (nsTArray<nsString> *)JSVAL_TO_PRIVATE(*statep);

  if (enum_op == JSENUMERATE_NEXT && keys->Length() != 0) {
    nsString& key = keys->ElementAt(0);
    JSString *str =
      JS_NewUCStringCopyN(cx, reinterpret_cast<const jschar *>
                                              (key.get()),
                          key.Length());
    NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

    JS_ValueToId(cx, STRING_TO_JSVAL(str), idp);

    keys->RemoveElementAt(0);

    return NS_OK;
  }

  // destroy the keys array if we have no keys or if we're done
  NS_ABORT_IF_FALSE(enum_op == JSENUMERATE_DESTROY ||
                    (enum_op == JSENUMERATE_NEXT && keys->Length() == 0),
                    "Bad call from the JS engine");
  delete keys;

  *statep = JSVAL_NULL;

  return NS_OK;
}

// nsIDOMEventListener::HandleEvent() 'this' converter helper

NS_INTERFACE_MAP_BEGIN(nsEventListenerThisTranslator)
  NS_INTERFACE_MAP_ENTRY(nsIXPCFunctionThisTranslator)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsEventListenerThisTranslator)
NS_IMPL_RELEASE(nsEventListenerThisTranslator)


NS_IMETHODIMP
nsEventListenerThisTranslator::TranslateThis(nsISupports *aInitialThis,
                                             nsISupports **_retval)
{
  nsCOMPtr<nsIDOMEvent> event(do_QueryInterface(aInitialThis));
  NS_ENSURE_TRUE(event, NS_ERROR_UNEXPECTED);

  nsCOMPtr<EventTarget> target = event->InternalDOMEvent()->GetCurrentTarget();
  *_retval = target.forget().get();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMConstructorSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                              JSObject *aGlobalObj, JSObject **parentObj)
{
  JS::Rooted<JSObject*> globalObj(cx, aGlobalObj);
  nsDOMConstructor *wrapped = static_cast<nsDOMConstructor *>(nativeObj);

#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMDOMConstructor> is_constructor =
      do_QueryInterface(nativeObj);
    NS_ASSERTION(is_constructor, "How did we not get a constructor?");
  }
#endif

  return wrapped->PreCreate(cx, globalObj, parentObj);
}

NS_IMETHODIMP
nsDOMConstructorSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                               JSObject *aObj, jsid aId, uint32_t flags,
                               JSObject **objp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  // For regular DOM constructors, we have our interface constants defined on
  // us by nsWindowSH::GlobalResolve. However, XrayWrappers can't see these
  // interface constants (as they look like expando properties) so we have to
  // specially resolve those constants here, but only for Xray wrappers.
  if (!ObjectIsNativeWrapper(cx, obj)) {
    return NS_OK;
  }

  JS::Rooted<JSObject*> nativePropsObj(cx, xpc::XrayUtils::GetNativePropertiesObject(cx, obj));
  nsDOMConstructor *wrapped =
    static_cast<nsDOMConstructor *>(wrapper->Native());
  nsresult rv = wrapped->ResolveInterfaceConstants(cx, nativePropsObj);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now re-lookup the ID to see if we should report back that we resolved the
  // looked-for constant. Note that we don't have to worry about infinitely
  // recurring back here because the Xray wrapper's holder object doesn't call
  // NewResolve hooks.
  JSBool found;
  if (!JS_HasPropertyById(cx, nativePropsObj, id, &found)) {
    *_retval = false;
    return NS_OK;
  }

  if (found) {
    *objp = obj;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMConstructorSH::Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *aObj, const JS::CallArgs &args, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  MOZ_ASSERT(obj);

  nsDOMConstructor *wrapped =
    static_cast<nsDOMConstructor *>(wrapper->Native());

#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMDOMConstructor> is_constructor =
      do_QueryWrappedNative(wrapper);
    NS_ASSERTION(is_constructor, "How did we not get a constructor?");
  }
#endif

  return wrapped->Construct(wrapper, cx, obj, args, _retval);
}

NS_IMETHODIMP
nsDOMConstructorSH::Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                              JSObject *aObj, const JS::CallArgs &args, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  MOZ_ASSERT(obj);

  nsDOMConstructor *wrapped =
    static_cast<nsDOMConstructor *>(wrapper->Native());

#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMDOMConstructor> is_constructor =
      do_QueryWrappedNative(wrapper);
    NS_ASSERTION(is_constructor, "How did we not get a constructor?");
  }
#endif

  return wrapped->Construct(wrapper, cx, obj, args, _retval);
}

NS_IMETHODIMP
nsDOMConstructorSH::HasInstance(nsIXPConnectWrappedNative *wrapper,
                                JSContext *cx, JSObject *aObj, const jsval &val,
                                bool *bp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  nsDOMConstructor *wrapped =
    static_cast<nsDOMConstructor *>(wrapper->Native());

#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMDOMConstructor> is_constructor =
      do_QueryWrappedNative(wrapper);
    NS_ASSERTION(is_constructor, "How did we not get a constructor?");
  }
#endif

  return wrapped->HasInstance(wrapper, cx, obj, val, bp, _retval);
}

NS_IMETHODIMP
nsNonDOMObjectSH::GetFlags(uint32_t *aFlags)
{
  // This is NOT a DOM Object.  Use this helper class for cases when you need
  // to do something like implement nsISecurityCheckedComponent in a meaningful
  // way.
  *aFlags = nsIClassInfo::MAIN_THREAD_ONLY | nsIClassInfo::SINGLETON_CLASSINFO;
  return NS_OK;
}
