/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@netscape.com> (original author)
 *   Ms2ger <ms2ger@gmail.com>
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

#include "mozilla/Util.h"
// On top because they include basictypes.h:
#include "SmsFilter.h"

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
#include "nsIJSContextStack.h"
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
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "nsDOMEventTargetHelper.h"

// General helper includes
#include "nsGlobalWindow.h"
#include "nsHistory.h"
#include "nsIContent.h"
#include "nsIAttribute.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMPopStateEvent.h"
#include "nsIDOMHashChangeEvent.h"
#include "nsContentUtils.h"
#include "nsDOMWindowUtils.h"
#include "nsIDOMGlobalPropertyInitializer.h"
#include "mozilla/Preferences.h"
#include "nsLocation.h"

// Window scriptable helper includes
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIScriptExternalNameSet.h"
#include "nsJSUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsScriptNameSpaceManager.h"
#include "nsIScriptObjectOwner.h"
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
#include "nsIDOMPerformanceTiming.h"
#include "nsIDOMPerformanceNavigation.h"
#include "nsIDOMPerformance.h"
#include "nsClientRect.h"

// DOM core includes
#include "nsDOMError.h"
#include "nsIDOMDOMException.h"
#include "nsIDOMNode.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMDOMStringList.h"
#include "nsIDOMDOMTokenList.h"
#include "nsIDOMDOMSettableTokenList.h"

#include "nsDOMStringMap.h"

// HTMLFormElement helper includes
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsHTMLDocument.h"

// Constraint Validation API helper includes
#include "nsIDOMValidityState.h"

// HTMLSelectElement helper includes
#include "nsIDOMHTMLSelectElement.h"

// HTMLEmbed/ObjectElement helper includes
#include "nsNPAPIPluginInstance.h"
#include "nsIObjectFrame.h"
#include "nsIObjectLoadingContent.h"
#include "nsIPluginHost.h"

#include "nsIDOMHTMLOptionElement.h"
#include "nsGenericElement.h"

// Event related includes
#include "nsEventListenerManager.h"
#include "nsIDOMEventTarget.h"

// CSS related includes
#include "nsIDOMStyleSheet.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSSRule.h"
#include "nsICSSRuleList.h"
#include "nsIDOMRect.h"
#include "nsIDOMRGBColor.h"
#include "nsIDOMNSRGBAColor.h"
#include "nsDOMCSSAttrDeclaration.h"

// XBL related includes.
#include "nsIXBLService.h"
#include "nsXBLBinding.h"
#include "nsBindingManager.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIDOMElement.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsStyleContext.h"
#include "nsAutoPtr.h"
#include "nsMemory.h"

// Tranformiix
#include "nsIDOMXPathEvaluator.h"
#include "nsIXSLTProcessor.h"
#include "nsIXSLTProcessorPrivate.h"

#include "nsIDOMLSProgressEvent.h"
#include "nsIDOMParser.h"
#include "nsIDOMSerializer.h"
#include "nsXMLHttpRequest.h"
#include "nsWebSocket.h"
#include "nsIDOMCloseEvent.h"
#include "nsEventSource.h"
#include "nsIDOMSettingsManager.h"

// includes needed for the prototype chain interfaces
#include "nsIDOMNavigator.h"
#include "nsIDOMBarProp.h"
#include "nsIDOMScreen.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocumentFragment.h"
#include "nsDOMAttribute.h"
#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMDataContainerEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMCompositionEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMMouseScrollEvent.h"
#include "nsIDOMDragEvent.h"
#include "nsIDOMCommandEvent.h"
#include "nsIDOMPopupBlockedEvent.h"
#include "nsIDOMBeforeUnloadEvent.h"
#include "nsIDOMMutationEvent.h"
#include "nsIDOMSmartCardEvent.h"
#include "nsIDOMXULCommandEvent.h"
#include "nsIDOMPageTransitionEvent.h"
#include "nsIDOMMessageEvent.h"
#include "nsPaintRequest.h"
#include "nsIDOMNotifyPaintEvent.h"
#include "nsIDOMNotifyAudioAvailableEvent.h"
#include "nsIDOMScrollAreaEvent.h"
#include "nsIDOMTransitionEvent.h"
#include "nsIDOMAnimationEvent.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMLinkStyle.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLBRElement.h"
#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsIDOMHTMLDataListElement.h"
#include "nsIDOMHTMLDListElement.h"
#include "nsIDOMHTMLDirectoryElement.h"
#include "nsIDOMHTMLDivElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMHTMLFieldSetElement.h"
#include "nsIDOMHTMLFontElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLFrameSetElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLHRElement.h"
#include "nsIDOMHTMLHeadElement.h"
#include "nsIDOMHTMLHeadingElement.h"
#include "nsIDOMHTMLHtmlElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLLIElement.h"
#include "nsIDOMHTMLLabelElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLMenuElement.h"
#include "nsIDOMHTMLMenuItemElement.h"
#include "nsIDOMHTMLMetaElement.h"
#include "nsIDOMHTMLModElement.h"
#include "nsIDOMHTMLOListElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLOutputElement.h"
#include "nsIDOMHTMLParagraphElement.h"
#include "nsIDOMHTMLParamElement.h"
#include "nsIDOMHTMLPreElement.h"
#include "nsIDOMHTMLProgressElement.h"
#include "nsIDOMHTMLQuoteElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMHTMLStyleElement.h"
#include "nsIDOMHTMLTableCaptionElem.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLTableColElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLTitleElement.h"
#include "nsIDOMHTMLUListElement.h"
#include "nsIDOMHTMLUnknownElement.h"
#include "nsIDOMMediaError.h"
#include "nsIDOMTimeRanges.h"
#include "nsIDOMHTMLSourceElement.h"
#include "nsIDOMHTMLVideoElement.h"
#include "nsIDOMHTMLAudioElement.h"
#if defined (MOZ_MEDIA)
#include "nsIDOMMediaStream.h"
#endif
#include "nsIDOMProgressEvent.h"
#include "nsIDOMCSS2Properties.h"
#include "nsIDOMCSSCharsetRule.h"
#include "nsIDOMCSSImportRule.h"
#include "nsIDOMCSSMediaRule.h"
#include "nsIDOMCSSFontFaceRule.h"
#include "nsIDOMCSSMozDocumentRule.h"
#include "nsIDOMMozCSSKeyframeRule.h"
#include "nsIDOMMozCSSKeyframesRule.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIDOMCSSStyleRule.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsDOMCSSValueList.h"
#include "nsIDOMDeviceOrientationEvent.h"
#include "nsIDOMDeviceMotionEvent.h"
#include "nsIDOMRange.h"
#include "nsIDOMNodeIterator.h"
#include "nsIDOMTreeWalker.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMCrypto.h"
#include "nsIDOMCRMFObject.h"
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
#include "nsIDOMXPathException.h"
#include "nsIDOMXPathExpression.h"
#include "nsIDOMNSXPathExpression.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMMozBrowserFrame.h"

#include "nsIDOMGetSVGDocument.h"
#include "nsIDOMSVGAElement.h"
#include "nsIDOMSVGAltGlyphElement.h"
#include "nsIDOMSVGAngle.h"
#include "nsIDOMSVGAnimatedAngle.h"
#include "nsIDOMSVGAnimatedBoolean.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsIDOMSVGAnimatedInteger.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGAnimatedLengthList.h"
#include "nsIDOMSVGAnimatedNumber.h"
#include "nsIDOMSVGAnimatedNumberList.h"
#include "nsIDOMSVGAnimatedPathData.h"
#include "nsIDOMSVGAnimatedPoints.h"
#include "nsIDOMSVGAnimatedRect.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsIDOMSVGAnimPresAspRatio.h"
#include "nsIDOMSVGAnimateElement.h"
#include "nsIDOMSVGAnimateTransformElement.h"
#include "nsIDOMSVGAnimateMotionElement.h"
#include "nsIDOMSVGMpathElement.h"
#include "nsIDOMSVGSetElement.h"
#include "nsIDOMSVGAnimationElement.h"
#include "nsIDOMElementTimeControl.h"
#include "nsIDOMTimeEvent.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsIDOMSVGCircleElement.h"
#include "nsIDOMSVGClipPathElement.h"
#include "nsIDOMSVGDefsElement.h"
#include "nsIDOMSVGDescElement.h"
#include "nsIDOMSVGDocument.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGEllipseElement.h"
#include "nsIDOMSVGEvent.h"
#include "nsIDOMSVGException.h"
#include "nsIDOMSVGFilterElement.h"
#include "nsIDOMSVGFilters.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsIDOMSVGForeignObjectElem.h"
#include "nsIDOMSVGGElement.h"
#include "nsIDOMSVGGradientElement.h"
#include "nsIDOMSVGImageElement.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGLengthList.h"
#include "nsIDOMSVGLineElement.h"
#include "nsIDOMSVGLocatable.h"
#include "nsIDOMSVGMarkerElement.h"
#include "nsIDOMSVGMaskElement.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGMetadataElement.h"
#include "nsIDOMSVGNumber.h"
#include "nsIDOMSVGNumberList.h"
#include "nsIDOMSVGPathElement.h"
#include "nsIDOMSVGPathSeg.h"
#include "nsIDOMSVGPathSegList.h"
#include "nsIDOMSVGPatternElement.h"
#include "nsIDOMSVGPoint.h"
#include "nsIDOMSVGPointList.h"
#include "nsIDOMSVGPolygonElement.h"
#include "nsIDOMSVGPolylineElement.h"
#include "nsIDOMSVGPresAspectRatio.h"
#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGRectElement.h"
#include "nsIDOMSVGScriptElement.h"
#include "nsIDOMSVGStopElement.h"
#include "nsIDOMSVGStylable.h"
#include "nsIDOMSVGStyleElement.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGSwitchElement.h"
#include "nsIDOMSVGSymbolElement.h"
#include "nsIDOMSVGTests.h"
#include "nsIDOMSVGTextElement.h"
#include "nsIDOMSVGTextPathElement.h"
#include "nsIDOMSVGTitleElement.h"
#include "nsIDOMSVGTransform.h"
#include "nsIDOMSVGTransformable.h"
#include "nsIDOMSVGTransformList.h"
#include "nsIDOMSVGTSpanElement.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsIDOMSVGURIReference.h"
#include "nsIDOMSVGUseElement.h"
#include "nsIDOMSVGZoomAndPan.h"
#include "nsIDOMSVGZoomEvent.h"

#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsIDOMWebGLRenderingContext.h"

#include "nsIImageDocument.h"

// Storage includes
#include "nsDOMStorage.h"

// Drag and drop
#include "nsIDOMDataTransfer.h"

// Geolocation
#include "nsIDOMGeoGeolocation.h"
#include "nsIDOMGeoPosition.h"
#include "nsIDOMGeoPositionCoords.h"
#include "nsIDOMGeoPositionError.h"

// Workers
#include "mozilla/dom/workers/Workers.h"

#include "nsDOMFile.h"
#include "nsDOMFileReader.h"
#include "nsIDOMFormData.h"

#include "nsIDOMDOMStringMap.h"

#include "nsIDOMDesktopNotification.h"
#include "nsIDOMNavigatorDesktopNotification.h"
#include "nsIDOMNavigatorGeolocation.h"
#include "Navigator.h"

#include "nsPluginArray.h"
#include "nsMimeTypeArray.h"

// Simple gestures include
#include "nsIDOMSimpleGestureEvent.h"
#include "nsIDOMMozTouchEvent.h"

#include "nsIEventListenerService.h"
#include "nsIFrameMessageManager.h"
#include "mozilla/dom/Element.h"
#include "nsHTMLSelectElement.h"
#include "nsHTMLLegendElement.h"

#include "DOMSVGLengthList.h"
#include "DOMSVGNumberList.h"
#include "DOMSVGPathSegList.h"
#include "DOMSVGPointList.h"
#include "DOMSVGStringList.h"
#include "DOMSVGTransformList.h"

#include "mozilla/dom/indexedDB/IDBWrapperCache.h"
#include "mozilla/dom/indexedDB/IDBFactory.h"
#include "mozilla/dom/indexedDB/IDBRequest.h"
#include "mozilla/dom/indexedDB/IDBDatabase.h"
#include "mozilla/dom/indexedDB/IDBEvents.h"
#include "mozilla/dom/indexedDB/IDBObjectStore.h"
#include "mozilla/dom/indexedDB/IDBTransaction.h"
#include "mozilla/dom/indexedDB/IDBCursor.h"
#include "mozilla/dom/indexedDB/IDBKeyRange.h"
#include "mozilla/dom/indexedDB/IDBIndex.h"

using mozilla::dom::indexedDB::IDBWrapperCache;

#include "nsIDOMMediaQueryList.h"

#include "nsDOMTouchEvent.h"
#include "nsIDOMCustomEvent.h"
#include "nsDOMMutationObserver.h"

#include "nsWrapperCacheInlines.h"
#include "dombindings.h"

#include "nsIDOMBatteryManager.h"
#include "BatteryManager.h"
#include "nsIDOMPowerManager.h"
#include "nsIDOMWakeLock.h"
#include "nsIDOMSmsManager.h"
#include "nsIDOMSmsMessage.h"
#include "nsIDOMSmsEvent.h"
#include "nsIDOMSmsRequest.h"
#include "nsIDOMSmsFilter.h"
#include "nsIDOMSmsCursor.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMConnection.h"
#include "nsIDOMMobileConnection.h"
#include "mozilla/dom/network/Utils.h"

#ifdef MOZ_B2G_RIL
#include "Telephony.h"
#include "TelephonyCall.h"
#include "CallEvent.h"
#endif

#ifdef MOZ_B2G_BT
#include "BluetoothAdapter.h"
#endif

#include "DOMError.h"
#include "DOMRequest.h"

#include "mozilla/Likely.h"

#undef None // something included above defines this preprocessor symbol, maybe Xlib headers
#include "WebGLContext.h"

using namespace mozilla;
using namespace mozilla::dom;

static NS_DEFINE_CID(kDOMSOF_CID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

static const char kDOMStringBundleURL[] =
  "chrome://global/locale/dom/dom.properties";

// NOTE: DEFAULT_SCRIPTABLE_FLAGS and DOM_DEFAULT_SCRIPTABLE_FLAGS
//       are defined in nsIDOMClassInfo.h.

#define WINDOW_SCRIPTABLE_FLAGS                                               \
 (nsIXPCScriptable::WANT_GETPROPERTY |                                        \
  nsIXPCScriptable::WANT_PRECREATE |                                          \
  nsIXPCScriptable::WANT_FINALIZE |                                           \
  nsIXPCScriptable::WANT_ENUMERATE |                                          \
  nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE |                               \
  nsIXPCScriptable::USE_STUB_EQUALITY_HOOK |                                  \
  nsIXPCScriptable::IS_GLOBAL_OBJECT |                                        \
  nsIXPCScriptable::WANT_OUTER_OBJECT)

#define NODE_SCRIPTABLE_FLAGS                                                 \
 ((DOM_DEFAULT_SCRIPTABLE_FLAGS |                                             \
   nsIXPCScriptable::USE_STUB_EQUALITY_HOOK |                                 \
   nsIXPCScriptable::WANT_ADDPROPERTY) &                                      \
  ~nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY)

// We need to let JavaScript QI elements to interfaces that are not in
// the classinfo since XBL can be used to dynamically implement new
// unknown interfaces on elements, accessibility relies on this being
// possible.

#define ELEMENT_SCRIPTABLE_FLAGS                                              \
  ((NODE_SCRIPTABLE_FLAGS & ~nsIXPCScriptable::CLASSINFO_INTERFACES_ONLY) |   \
   nsIXPCScriptable::WANT_POSTCREATE |                                        \
   nsIXPCScriptable::WANT_ENUMERATE)

#define EXTERNAL_OBJ_SCRIPTABLE_FLAGS                                         \
  ((ELEMENT_SCRIPTABLE_FLAGS &                                                \
    ~nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY) |                          \
   nsIXPCScriptable::WANT_POSTCREATE |                                        \
   nsIXPCScriptable::WANT_GETPROPERTY |                                       \
   nsIXPCScriptable::WANT_SETPROPERTY |                                       \
   nsIXPCScriptable::WANT_CALL)

#define DOCUMENT_SCRIPTABLE_FLAGS                                             \
  (NODE_SCRIPTABLE_FLAGS |                                                    \
   nsIXPCScriptable::WANT_POSTCREATE |                                        \
   nsIXPCScriptable::WANT_ENUMERATE)

#define ARRAY_SCRIPTABLE_FLAGS                                                \
  (DOM_DEFAULT_SCRIPTABLE_FLAGS       |                                       \
   nsIXPCScriptable::WANT_GETPROPERTY |                                       \
   nsIXPCScriptable::WANT_ENUMERATE)

#define DOMSTRINGMAP_SCRIPTABLE_FLAGS                                         \
  (DOM_DEFAULT_SCRIPTABLE_FLAGS       |                                       \
   nsIXPCScriptable::WANT_ENUMERATE   |                                       \
   nsIXPCScriptable::WANT_PRECREATE   |                                       \
   nsIXPCScriptable::WANT_DELPROPERTY |                                       \
   nsIXPCScriptable::WANT_SETPROPERTY |                                       \
   nsIXPCScriptable::WANT_GETPROPERTY)

#define EVENTTARGET_SCRIPTABLE_FLAGS                                          \
  (DOM_DEFAULT_SCRIPTABLE_FLAGS       |                                       \
   nsIXPCScriptable::WANT_ADDPROPERTY)

#define IDBEVENTTARGET_SCRIPTABLE_FLAGS                                       \
  (EVENTTARGET_SCRIPTABLE_FLAGS)

#define DOMCLASSINFO_STANDARD_FLAGS                                           \
  (nsIClassInfo::MAIN_THREAD_ONLY | nsIClassInfo::DOM_OBJECT)


#ifdef NS_DEBUG
#define NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                \
    eDOMClassInfo_##_class##_id,
#else
#define NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                \
  // nothing
#endif

DOMCI_DATA(Crypto, void)
DOMCI_DATA(CRMFObject, void)
DOMCI_DATA(SmartCardEvent, void)
DOMCI_DATA(ContentFrameMessageManager, void)

DOMCI_DATA(DOMPrototype, void)
DOMCI_DATA(DOMConstructor, void)

#define NS_DEFINE_CLASSINFO_DATA_WITH_NAME(_class, _name, _helper,            \
                                           _flags)                            \
  { #_name,                                                                   \
    nsnull,                                                                   \
    { _helper::doCreate },                                                    \
    nsnull,                                                                   \
    nsnull,                                                                   \
    nsnull,                                                                   \
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
    nsnull,                                                                   \
    { _helper::doCreate },                                                    \
    nsnull,                                                                   \
    nsnull,                                                                   \
    nsnull,                                                                   \
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
                           (DOM_DEFAULT_SCRIPTABLE_FLAGS &
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
  NS_DEFINE_CLASSINFO_DATA(BarProp, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(History, nsHistorySH,
                           ARRAY_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_PRECREATE)
  NS_DEFINE_CLASSINFO_DATA(PerformanceTiming, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(PerformanceNavigation, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Performance, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Screen, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
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
  NS_DEFINE_CLASSINFO_DATA(XMLDocument, nsDocumentSH,
                           DOCUMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DocumentType, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DOMImplementation, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DOMException, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DOMTokenList, nsDOMTokenListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DOMSettableTokenList, nsDOMTokenListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DocumentFragment, nsNodeSH, NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Element, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Attr, nsAttributeSH,
                           NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Text, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Comment, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CDATASection, nsNodeSH, NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(ProcessingInstruction, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(NodeList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(NamedNodeMap, nsNamedNodeMapSH,
                           ARRAY_SCRIPTABLE_FLAGS)

  // Misc Core related classes

  // Event
  NS_DEFINE_CLASSINFO_DATA(Event, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MutationEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(UIEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MouseEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MouseScrollEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DragEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(KeyboardEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CompositionEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(PopupBlockedEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  // Device Orientation
  NS_DEFINE_CLASSINFO_DATA(DeviceOrientationEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DeviceMotionEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DeviceAcceleration, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DeviceRotationRate, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // Misc HTML classes
  NS_DEFINE_CLASSINFO_DATA(HTMLDocument, nsHTMLDocumentSH,
                           DOCUMENT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_GETPROPERTY)
  NS_DEFINE_CLASSINFO_DATA(HTMLOptionsCollection, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLCollection, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // HTML element classes
  NS_DEFINE_CLASSINFO_DATA(HTMLElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLAnchorElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLAppletElement, nsHTMLPluginObjElementSH,
                           EXTERNAL_OBJ_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLAreaElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLBRElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLBaseElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLBodyElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLButtonElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLDataListElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLDListElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLDirectoryElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLDivElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLEmbedElement, nsHTMLPluginObjElementSH,
                           EXTERNAL_OBJ_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLFieldSetElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLFontElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLFormElement, nsHTMLFormElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_GETPROPERTY |
                           nsIXPCScriptable::WANT_NEWENUMERATE)
  NS_DEFINE_CLASSINFO_DATA(HTMLFrameElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLFrameSetElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLHRElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLHeadElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLHeadingElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLHtmlElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLIFrameElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLImageElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLInputElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLLIElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLLabelElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLLegendElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLLinkElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLMapElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLMenuElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLMenuItemElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLMetaElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLModElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLOListElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLObjectElement, nsHTMLPluginObjElementSH,
                           EXTERNAL_OBJ_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLOptGroupElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLOptionElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLOutputElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLParagraphElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLParamElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLPreElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLProgressElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLQuoteElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLScriptElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLSelectElement, nsHTMLSelectElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_SETPROPERTY |
                           nsIXPCScriptable::WANT_GETPROPERTY)
  NS_DEFINE_CLASSINFO_DATA(HTMLSpanElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLStyleElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTableCaptionElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTableCellElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTableColElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTableElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTableRowElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTableSectionElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTextAreaElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTitleElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLUListElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLUnknownElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)

  // Constraint Validation API classes
  NS_DEFINE_CLASSINFO_DATA(ValidityState, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

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
  NS_DEFINE_CLASSINFO_DATA(CSSStyleDeclaration, nsCSSStyleDeclSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(ROCSSPrimitiveValue, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // Range classes
  NS_DEFINE_CLASSINFO_DATA(Range, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Selection, nsDOMGenericSH,
                           DEFAULT_SCRIPTABLE_FLAGS)

  // XUL classes
#ifdef MOZ_XUL
  NS_DEFINE_CLASSINFO_DATA(XULDocument, nsDocumentSH,
                           DOCUMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(XULElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
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
  NS_DEFINE_CLASSINFO_DATA(Crypto, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CRMFObject, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // DOM Traversal classes
  NS_DEFINE_CLASSINFO_DATA(TreeWalker, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // We are now trying to preserve binary compat in classinfo.  No
  // more putting things in those categories up there.  New entries
  // are to be added to the end of the list
  NS_DEFINE_CLASSINFO_DATA(CSSRect, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // DOM Chrome Window class.
  NS_DEFINE_CLASSINFO_DATA(ChromeWindow, nsWindowSH,
                           DEFAULT_SCRIPTABLE_FLAGS |
                           WINDOW_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(CSSRGBColor, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(CSSValueList, nsCSSValueListSH,
                           ARRAY_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(ContentList, HTMLCollection,
                                     nsDOMGenericSH,
                                     DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XMLStylesheetProcessingInstruction, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(ImageDocument, nsHTMLDocumentSH,
                           DOCUMENT_SCRIPTABLE_FLAGS)

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

  NS_DEFINE_CLASSINFO_DATA(TreeColumns, nsTreeColumnsSH,
                           ARRAY_SCRIPTABLE_FLAGS)
#endif

  NS_DEFINE_CLASSINFO_DATA(CSSMozDocumentRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(BeforeUnloadEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // SVG document
  NS_DEFINE_CLASSINFO_DATA(SVGDocument, nsDocumentSH,
                           DOCUMENT_SCRIPTABLE_FLAGS)

  // SVG element classes
  NS_DEFINE_CLASSINFO_DATA(SVGAElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAltGlyphElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimateElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimateTransformElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimateMotionElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGMpathElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGSetElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(TimeEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGCircleElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGClipPathElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGDefsElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGDescElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGEllipseElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEBlendElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEColorMatrixElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEComponentTransferElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFECompositeElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEConvolveMatrixElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEDiffuseLightingElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEDisplacementMapElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEDistantLightElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEFloodElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEFuncAElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEFuncBElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEFuncGElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEFuncRElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEGaussianBlurElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEImageElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEMergeElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEMergeNodeElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEMorphologyElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEOffsetElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEPointLightElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFESpecularLightingElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFESpotLightElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFETileElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFETurbulenceElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFilterElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGGElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGImageElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGLinearGradientElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGLineElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGMarkerElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGMaskElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGMetadataElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPatternElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPolygonElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPolylineElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGRadialGradientElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGRectElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGScriptElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGStopElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGStyleElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGSVGElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGSwitchElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGSymbolElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGTextElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGTextPathElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGTitleElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGTSpanElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(SVGUnknownElement, SVGElement, nsElementSH,
                                     ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGUseElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)

  // other SVG classes
  NS_DEFINE_CLASSINFO_DATA(SVGAngle, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedAngle, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedBoolean, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedEnumeration, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedInteger, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedLength, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedLengthList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedNumber, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)    
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedNumberList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)    
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedPreserveAspectRatio, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedRect, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedString, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedTransformList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGException, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGLength, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGLengthList, nsSVGLengthListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGMatrix, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGNumber, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGNumberList, nsSVGNumberListSH,
                           ARRAY_SCRIPTABLE_FLAGS)    
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegArcAbs, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegArcRel, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegClosePath, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegCurvetoCubicAbs, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegCurvetoCubicRel, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegCurvetoCubicSmoothAbs, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegCurvetoCubicSmoothRel, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegCurvetoQuadraticAbs, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegCurvetoQuadraticRel, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegCurvetoQuadraticSmoothAbs, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegCurvetoQuadraticSmoothRel, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegLinetoAbs, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegLinetoHorizontalAbs, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegLinetoHorizontalRel, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegLinetoRel, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegLinetoVerticalAbs, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegLinetoVerticalRel, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegList, nsSVGPathSegListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegMovetoAbs, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegMovetoRel, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPoint, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPointList, nsSVGPointListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPreserveAspectRatio, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGRect, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGStringList, nsSVGStringListSH,
                           ARRAY_SCRIPTABLE_FLAGS)    
  NS_DEFINE_CLASSINFO_DATA(SVGTransform, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGTransformList, nsSVGTransformListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGZoomEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(HTMLCanvasElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CanvasRenderingContext2D, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CanvasGradient, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CanvasPattern, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(TextMetrics, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(ImageData, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(SmartCardEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(PageTransitionEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(WindowUtils, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XSLTProcessor, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XPathEvaluator, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(XPathException, nsDOMGenericSH,
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
  NS_DEFINE_CLASSINFO_DATA(StorageObsolete, nsStorageSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_NEWRESOLVE |
                           nsIXPCScriptable::WANT_GETPROPERTY |
                           nsIXPCScriptable::WANT_SETPROPERTY |
                           nsIXPCScriptable::WANT_DELPROPERTY |
                           nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |
                           nsIXPCScriptable::WANT_NEWENUMERATE)

  NS_DEFINE_CLASSINFO_DATA(Storage, nsStorage2SH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_NEWRESOLVE |
                           nsIXPCScriptable::WANT_GETPROPERTY |
                           nsIXPCScriptable::WANT_SETPROPERTY |
                           nsIXPCScriptable::WANT_DELPROPERTY |
                           nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |
                           nsIXPCScriptable::WANT_NEWENUMERATE)
  NS_DEFINE_CLASSINFO_DATA(StorageItem, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(StorageEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(StorageEventObsolete, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(DOMParser, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(XMLSerializer, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XMLHttpProgressEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(XMLHttpRequest, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(EventSource, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(ClientRect, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(ClientRectList, nsClientRectListSH,
                           ARRAY_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(SVGForeignObjectElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XULCommandEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(CommandEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(OfflineResourceList, nsOfflineResourceListSH,
                           ARRAY_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(FileList, nsFileListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Blob, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(File, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(FileReader, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MozURLProperty, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MozBlobBuilder, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(DOMStringMap, nsDOMStringMapSH,
                           DOMSTRINGMAP_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(ModalContentWindow, nsWindowSH,
                           DEFAULT_SCRIPTABLE_FLAGS |
                           WINDOW_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(DataContainerEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MessageEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(GeoGeolocation, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  
  NS_DEFINE_CLASSINFO_DATA(GeoPosition, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS) 
  
  NS_DEFINE_CLASSINFO_DATA(GeoPositionCoords, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(GeoPositionError, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozBatteryManager, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozPowerManager, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozWakeLock, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsManager, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsMessage, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsRequest, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsFilter, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsCursor, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozConnection, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozMobileConnection, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(CSSFontFaceRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSFontFaceStyleDecl, nsCSSStyleDeclSH,
                           ARRAY_SCRIPTABLE_FLAGS)

#if defined(MOZ_MEDIA)
  NS_DEFINE_CLASSINFO_DATA(HTMLVideoElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLSourceElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MediaError, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLAudioElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(TimeRanges, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MediaStream, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif

  NS_DEFINE_CLASSINFO_DATA(ProgressEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XMLHttpRequestUpload, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)

  // DOM Traversal NodeIterator class  
  NS_DEFINE_CLASSINFO_DATA(NodeIterator, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // data transfer for drag and drop
  NS_DEFINE_CLASSINFO_DATA(DataTransfer, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(NotifyPaintEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(NotifyAudioAvailableEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(SimpleGestureEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozTouchEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(MathMLElement, Element, nsElementSH,
                                     ELEMENT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(WebGLRenderingContext, nsWebGLViewportHandlerSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(WebGLBuffer, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(WebGLTexture, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(WebGLProgram, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(WebGLShader, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(WebGLFramebuffer, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(WebGLRenderbuffer, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(WebGLUniformLocation, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(WebGLShaderPrecisionFormat, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(WebGLActiveInfo, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(WebGLExtension, WebGLExtensionSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_ADDPROPERTY)
  NS_DEFINE_CLASSINFO_DATA(WebGLExtensionStandardDerivatives, WebGLExtensionSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_ADDPROPERTY)
  NS_DEFINE_CLASSINFO_DATA(WebGLExtensionTextureFilterAnisotropic, WebGLExtensionSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_ADDPROPERTY)
  NS_DEFINE_CLASSINFO_DATA(WebGLExtensionLoseContext, WebGLExtensionSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_ADDPROPERTY)

  NS_DEFINE_CLASSINFO_DATA(PaintRequest, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(PaintRequestList, nsPaintRequestListSH,
                           ARRAY_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(ScrollAreaEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(PopStateEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HashChangeEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(EventListenerInfo, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(TransitionEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(AnimationEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(ContentFrameMessageManager, nsEventTargetSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS | nsIXPCScriptable::IS_GLOBAL_OBJECT)

  NS_DEFINE_CLASSINFO_DATA(FormData, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(DesktopNotification, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DesktopNotificationCenter, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(WebSocket, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(CloseEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(IDBFactory, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
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
  NS_DEFINE_CLASSINFO_DATA(IDBVersionChangeEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(IDBOpenDBRequest, IDBEventTargetSH,
                           IDBEVENTTARGET_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(Touch, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(TouchList, nsDOMTouchListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(TouchEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozCSSKeyframeRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MozCSSKeyframesRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MediaQueryList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CustomEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MozMutationObserver, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MutationRecord, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MozSettingsEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

#ifdef MOZ_B2G_RIL
  NS_DEFINE_CLASSINFO_DATA(Telephony, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(TelephonyCall, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CallEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif

#ifdef MOZ_B2G_BT
  NS_DEFINE_CLASSINFO_DATA(BluetoothAdapter, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
#endif

  NS_DEFINE_CLASSINFO_DATA(DOMError, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(DOMRequest, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
};

// Objects that should be constructable through |new Name();|
struct nsContractIDMapData
{
  PRInt32 mDOMClassInfoID;
  const char *mContractID;
};

#define NS_DEFINE_CONSTRUCTOR_DATA(_class, _contract_id)                      \
  { eDOMClassInfo_##_class##_id, _contract_id },

static const nsContractIDMapData kConstructorMap[] =
{
  NS_DEFINE_CONSTRUCTOR_DATA(DOMParser, NS_DOMPARSER_CONTRACTID)
  NS_DEFINE_CONSTRUCTOR_DATA(FileReader, NS_FILEREADER_CONTRACTID)
  NS_DEFINE_CONSTRUCTOR_DATA(FormData, NS_FORMDATA_CONTRACTID)
  NS_DEFINE_CONSTRUCTOR_DATA(XMLSerializer, NS_XMLSERIALIZER_CONTRACTID)
  NS_DEFINE_CONSTRUCTOR_DATA(XMLHttpRequest, NS_XMLHTTPREQUEST_CONTRACTID)
  NS_DEFINE_CONSTRUCTOR_DATA(WebSocket, NS_WEBSOCKET_CONTRACTID)
  NS_DEFINE_CONSTRUCTOR_DATA(XPathEvaluator, NS_XPATH_EVALUATOR_CONTRACTID)
  NS_DEFINE_CONSTRUCTOR_DATA(XSLTProcessor,
                             "@mozilla.org/document-transformer;1?type=xslt")
  NS_DEFINE_CONSTRUCTOR_DATA(EventSource, NS_EVENTSOURCE_CONTRACTID)
  NS_DEFINE_CONSTRUCTOR_DATA(MozMutationObserver, NS_DOMMUTATIONOBSERVER_CONTRACTID)
};

#define NS_DEFINE_EVENT_CTOR(_class)                        \
  nsresult                                                  \
  NS_DOM##_class##Ctor(nsISupports** aInstancePtrResult)    \
  {                                                         \
    nsIDOMEvent* e = nsnull;                                \
    nsresult rv = NS_NewDOM##_class(&e, nsnull, nsnull);    \
    *aInstancePtrResult = e;                                \
    return rv;                                              \
  }

NS_DEFINE_EVENT_CTOR(Event)
NS_DEFINE_EVENT_CTOR(CustomEvent)
NS_DEFINE_EVENT_CTOR(PopStateEvent)
NS_DEFINE_EVENT_CTOR(HashChangeEvent)
NS_DEFINE_EVENT_CTOR(PageTransitionEvent)
NS_DEFINE_EVENT_CTOR(CloseEvent)
NS_DEFINE_EVENT_CTOR(MozSettingsEvent)
NS_DEFINE_EVENT_CTOR(UIEvent)
NS_DEFINE_EVENT_CTOR(MouseEvent)
nsresult
NS_DOMStorageEventCtor(nsISupports** aInstancePtrResult)
{
  nsDOMStorageEvent* e = new nsDOMStorageEvent();
  return CallQueryInterface(e, aInstancePtrResult);
}

struct nsConstructorFuncMapData
{
  PRInt32 mDOMClassInfoID;
  nsDOMConstructorFunc mConstructorFunc;
};

#define NS_DEFINE_CONSTRUCTOR_FUNC_DATA(_class, _func)                        \
  { eDOMClassInfo_##_class##_id, _func },

#define NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(_class)   \
  { eDOMClassInfo_##_class##_id, NS_DOM##_class##Ctor },

static const nsConstructorFuncMapData kConstructorFuncMap[] =
{
  NS_DEFINE_CONSTRUCTOR_FUNC_DATA(Blob, nsDOMMultipartFile::NewBlob)
  NS_DEFINE_CONSTRUCTOR_FUNC_DATA(File, nsDOMFileFile::NewFile)
  NS_DEFINE_CONSTRUCTOR_FUNC_DATA(MozBlobBuilder, NS_NewBlobBuilder)
  NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(Event)
  NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(CustomEvent)
  NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(PopStateEvent)
  NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(HashChangeEvent)
  NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(PageTransitionEvent)
  NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(CloseEvent)
  NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(MozSettingsEvent)
  NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(UIEvent)
  NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(MouseEvent)
  NS_DEFINE_EVENT_CONSTRUCTOR_FUNC_DATA(StorageEvent)
  NS_DEFINE_CONSTRUCTOR_FUNC_DATA(MozSmsFilter, sms::SmsFilter::NewSmsFilter)
};

nsIXPConnect *nsDOMClassInfo::sXPConnect = nsnull;
nsIScriptSecurityManager *nsDOMClassInfo::sSecMan = nsnull;
bool nsDOMClassInfo::sIsInitialized = false;
bool nsDOMClassInfo::sDisableDocumentAllSupport = false;
bool nsDOMClassInfo::sDisableGlobalScopePollutionSupport = false;


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
jsid nsDOMClassInfo::sDialogArguments_id = JSID_VOID;
jsid nsDOMClassInfo::sControllers_id     = JSID_VOID;
jsid nsDOMClassInfo::sLength_id          = JSID_VOID;
jsid nsDOMClassInfo::sInnerHeight_id     = JSID_VOID;
jsid nsDOMClassInfo::sInnerWidth_id      = JSID_VOID;
jsid nsDOMClassInfo::sOuterHeight_id     = JSID_VOID;
jsid nsDOMClassInfo::sOuterWidth_id      = JSID_VOID;
jsid nsDOMClassInfo::sScreenX_id         = JSID_VOID;
jsid nsDOMClassInfo::sScreenY_id         = JSID_VOID;
jsid nsDOMClassInfo::sStatus_id          = JSID_VOID;
jsid nsDOMClassInfo::sName_id            = JSID_VOID;
jsid nsDOMClassInfo::sScrollX_id         = JSID_VOID;
jsid nsDOMClassInfo::sScrollY_id         = JSID_VOID;
jsid nsDOMClassInfo::sScrollMaxX_id      = JSID_VOID;
jsid nsDOMClassInfo::sScrollMaxY_id      = JSID_VOID;
jsid nsDOMClassInfo::sItem_id            = JSID_VOID;
jsid nsDOMClassInfo::sNamedItem_id       = JSID_VOID;
jsid nsDOMClassInfo::sEnumerate_id       = JSID_VOID;
jsid nsDOMClassInfo::sNavigator_id       = JSID_VOID;
jsid nsDOMClassInfo::sDocument_id        = JSID_VOID;
jsid nsDOMClassInfo::sFrames_id          = JSID_VOID;
jsid nsDOMClassInfo::sSelf_id            = JSID_VOID;
jsid nsDOMClassInfo::sOpener_id          = JSID_VOID;
jsid nsDOMClassInfo::sAll_id             = JSID_VOID;
jsid nsDOMClassInfo::sTags_id            = JSID_VOID;
jsid nsDOMClassInfo::sAddEventListener_id= JSID_VOID;
jsid nsDOMClassInfo::sBaseURIObject_id   = JSID_VOID;
jsid nsDOMClassInfo::sNodePrincipal_id   = JSID_VOID;
jsid nsDOMClassInfo::sDocumentURIObject_id=JSID_VOID;
jsid nsDOMClassInfo::sWrappedJSObject_id = JSID_VOID;
jsid nsDOMClassInfo::sURL_id             = JSID_VOID;
jsid nsDOMClassInfo::sKeyPath_id         = JSID_VOID;
jsid nsDOMClassInfo::sAutoIncrement_id   = JSID_VOID;
jsid nsDOMClassInfo::sUnique_id          = JSID_VOID;
jsid nsDOMClassInfo::sMultiEntry_id      = JSID_VOID;
jsid nsDOMClassInfo::sOnload_id          = JSID_VOID;
jsid nsDOMClassInfo::sOnerror_id         = JSID_VOID;

static const JSClass *sObjectClass = nsnull;

/**
 * Set our JSClass pointer for the Object class
 */
static void
FindObjectClass(JSObject* aGlobalObject)
{
  NS_ASSERTION(!sObjectClass,
               "Double set of sObjectClass");
  JSObject *obj, *proto = aGlobalObject;
  do {
    obj = proto;
    proto = js::GetObjectProto(obj);
  } while (proto);

  sObjectClass = js::GetObjectJSClass(obj);
}

static void
PrintWarningOnConsole(JSContext *cx, const char *stringBundleProperty)
{
  nsCOMPtr<nsIStringBundleService> stringService =
    mozilla::services::GetStringBundleService();
  if (!stringService) {
    return;
  }

  nsCOMPtr<nsIStringBundle> bundle;
  stringService->CreateBundle(kDOMStringBundleURL, getter_AddRefs(bundle));
  if (!bundle) {
    return;
  }

  nsXPIDLString msg;
  bundle->GetStringFromName(NS_ConvertASCIItoUTF16(stringBundleProperty).get(),
                            getter_Copies(msg));

  if (msg.IsEmpty()) {
    NS_ERROR("Failed to get strings from dom.properties!");
    return;
  }

  nsCOMPtr<nsIConsoleService> consoleService
    (do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!consoleService) {
    return;
  }

  nsCOMPtr<nsIScriptError> scriptError =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  if (!scriptError) {
    return;
  }

  unsigned lineno = 0;
  JSScript *script;
  nsAutoString sourcefile;

  if (JS_DescribeScriptedCaller(cx, &script, &lineno)) {
    if (const char *filename = ::JS_GetScriptFilename(cx, script)) {
      CopyUTF8toUTF16(nsDependentCString(filename), sourcefile);
    }
  }

  nsresult rv = scriptError->InitWithWindowID(msg.get(),
                                              sourcefile.get(),
                                              EmptyString().get(),
                                              lineno,
                                              0, // column for error is not available
                                              nsIScriptError::warningFlag,
                                              "DOM:HTML",
                                              nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(cx));

  if (NS_SUCCEEDED(rv)) {
    consoleService->LogMessage(scriptError);
  }
}

static inline JSString *
IdToString(JSContext *cx, jsid id)
{
  if (JSID_IS_STRING(id))
    return JSID_TO_STRING(id);
  jsval idval;
  if (!::JS_IdToValue(cx, id, &idval))
    return nsnull;
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
           nsIXPConnectJSObjectHolder** aHolder = nsnull)
{
  return WrapNative(cx, scope, native, nsnull, aIID, vp, aHolder,
                    aAllowWrapping);
}

// Same as the WrapNative above, but use these if aIID is nsISupports' IID.
static inline nsresult
WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
           bool aAllowWrapping, jsval *vp,
           // If non-null aHolder will keep the jsval alive
           // while there's a ref to it
           nsIXPConnectJSObjectHolder** aHolder = nsnull)
{
  return WrapNative(cx, scope, native, nsnull, nsnull, vp, aHolder,
                    aAllowWrapping);
}

static inline nsresult
WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
           nsWrapperCache *cache, bool aAllowWrapping, jsval *vp,
           // If non-null aHolder will keep the jsval alive
           // while there's a ref to it
           nsIXPConnectJSObjectHolder** aHolder = nsnull)
{
  return WrapNative(cx, scope, native, cache, nsnull, vp, aHolder,
                    aAllowWrapping);
}

// Used for cases where PreCreate needs to wrap the native parent, and the
// native parent is likely to have been wrapped already.  |native| must
// implement nsWrapperCache, and nativeWrapperCache must be |native|'s
// nsWrapperCache.
static inline nsresult
WrapNativeParent(JSContext *cx, JSObject *scope, nsISupports *native,
                                        nsWrapperCache *nativeWrapperCache,
                                        JSObject **parentObj)
{
  // In the common case, |native| is a wrapper cache with an existing wrapper
#ifdef DEBUG
  nsWrapperCache* cache = nsnull;
  CallQueryInterface(native, &cache);
  NS_PRECONDITION(nativeWrapperCache &&
                  cache == nativeWrapperCache, "What happened here?");
#endif
  
  JSObject* obj = nativeWrapperCache->GetWrapper();
  if (obj) {
#ifdef DEBUG
    jsval debugVal;
    nsresult rv = WrapNative(cx, scope, native, nativeWrapperCache, false,
                             &debugVal);
    NS_ASSERTION(NS_SUCCEEDED(rv) && JSVAL_TO_OBJECT(debugVal) == obj,
                 "Unexpected object in nsWrapperCache");
#endif
    *parentObj = obj;
    return NS_OK;
  }

  jsval v;
  nsresult rv = WrapNative(cx, scope, native, nativeWrapperCache, false, &v);
  NS_ENSURE_SUCCESS(rv, rv);
  *parentObj = JSVAL_TO_OBJECT(v);
  return NS_OK;
}

// Helper to handle torn-down inner windows.
static inline nsresult
SetParentToWindow(nsGlobalWindow *win, JSObject **parent)
{
  MOZ_ASSERT(win);
  MOZ_ASSERT(win->IsInnerWindow());
  *parent = win->FastGetGlobalJSObject();

  if (MOZ_UNLIKELY(!*parent)) {
    // The only known case where this can happen is when the inner window has
    // been torn down. See bug 691178 comment 11.
    MOZ_ASSERT(win->IsClosedOrClosing());
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

  JSAutoRequest ar(cx);

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
  SET_JSID_TO_STRING(sDialogArguments_id, cx, "dialogArguments");
  SET_JSID_TO_STRING(sControllers_id,     cx, "controllers");
  SET_JSID_TO_STRING(sLength_id,          cx, "length");
  SET_JSID_TO_STRING(sInnerHeight_id,     cx, "innerHeight");
  SET_JSID_TO_STRING(sInnerWidth_id,      cx, "innerWidth");
  SET_JSID_TO_STRING(sOuterHeight_id,     cx, "outerHeight");
  SET_JSID_TO_STRING(sOuterWidth_id,      cx, "outerWidth");
  SET_JSID_TO_STRING(sScreenX_id,         cx, "screenX");
  SET_JSID_TO_STRING(sScreenY_id,         cx, "screenY");
  SET_JSID_TO_STRING(sStatus_id,          cx, "status");
  SET_JSID_TO_STRING(sName_id,            cx, "name");
  SET_JSID_TO_STRING(sScrollX_id,         cx, "scrollX");
  SET_JSID_TO_STRING(sScrollY_id,         cx, "scrollY");
  SET_JSID_TO_STRING(sScrollMaxX_id,      cx, "scrollMaxX");
  SET_JSID_TO_STRING(sScrollMaxY_id,      cx, "scrollMaxY");
  SET_JSID_TO_STRING(sItem_id,            cx, "item");
  SET_JSID_TO_STRING(sNamedItem_id,       cx, "namedItem");
  SET_JSID_TO_STRING(sEnumerate_id,       cx, "enumerateProperties");
  SET_JSID_TO_STRING(sNavigator_id,       cx, "navigator");
  SET_JSID_TO_STRING(sDocument_id,        cx, "document");
  SET_JSID_TO_STRING(sFrames_id,          cx, "frames");
  SET_JSID_TO_STRING(sSelf_id,            cx, "self");
  SET_JSID_TO_STRING(sOpener_id,          cx, "opener");
  SET_JSID_TO_STRING(sAll_id,             cx, "all");
  SET_JSID_TO_STRING(sTags_id,            cx, "tags");
  SET_JSID_TO_STRING(sAddEventListener_id,cx, "addEventListener");
  SET_JSID_TO_STRING(sBaseURIObject_id,   cx, "baseURIObject");
  SET_JSID_TO_STRING(sNodePrincipal_id,   cx, "nodePrincipal");
  SET_JSID_TO_STRING(sDocumentURIObject_id,cx,"documentURIObject");
  SET_JSID_TO_STRING(sWrappedJSObject_id, cx, "wrappedJSObject");
  SET_JSID_TO_STRING(sURL_id,             cx, "URL");
  SET_JSID_TO_STRING(sKeyPath_id,         cx, "keyPath");
  SET_JSID_TO_STRING(sAutoIncrement_id,   cx, "autoIncrement");
  SET_JSID_TO_STRING(sUnique_id,          cx, "unique");
  SET_JSID_TO_STRING(sMultiEntry_id,      cx, "multiEntry");
  SET_JSID_TO_STRING(sOnload_id,          cx, "onload");
  SET_JSID_TO_STRING(sOnerror_id,         cx, "onerror");

  return NS_OK;
}

static nsresult
CreateExceptionFromResult(JSContext *cx, nsresult aResult)
{
  nsCOMPtr<nsIExceptionService> xs =
    do_GetService(NS_EXCEPTIONSERVICE_CONTRACTID);
  if (!xs) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIExceptionManager> xm;
  nsresult rv = xs->GetCurrentExceptionManager(getter_AddRefs(xm));
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIException> exception;
  rv = xm->GetExceptionFromProvider(aResult, 0, getter_AddRefs(exception));
  if (NS_FAILED(rv) || !exception) {
    return NS_ERROR_FAILURE;
  }

  jsval jv;
  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  rv = WrapNative(cx, ::JS_GetGlobalObject(cx), exception,
                  &NS_GET_IID(nsIException), false, &jv,
                  getter_AddRefs(holder));
  if (NS_FAILED(rv) || JSVAL_IS_NULL(jv)) {
    return NS_ERROR_FAILURE;
  }

  JSAutoEnterCompartment ac;

  if (JSVAL_IS_OBJECT(jv)) {
    if (!ac.enter(cx, JSVAL_TO_OBJECT(jv))) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  JS_SetPendingException(cx, jv);
  return NS_OK;
}

// static
nsresult
nsDOMClassInfo::ThrowJSException(JSContext *cx, nsresult aResult)
{
  JSAutoRequest ar(cx);

  if (NS_SUCCEEDED(CreateExceptionFromResult(cx, aResult))) {
    return NS_OK;
  }

  // XXX This probably wants to be localized, but that can fail in ways that
  // are hard to report correctly.
  JSString *str =
    JS_NewStringCopyZ(cx, "An error occurred throwing an exception");
  if (!str) {
    // JS_NewStringCopyZ reported the error for us.
    return NS_OK; 
  }
  JS_SetPendingException(cx, STRING_TO_JSVAL(str));
  return NS_OK;
}

// static
bool
nsDOMClassInfo::ObjectIsNativeWrapper(JSContext* cx, JSObject* obj)
{
  return xpc::WrapperFactory::IsXrayWrapper(obj) &&
         !xpc::WrapperFactory::IsPartiallyTransparent(obj);
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
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsnull
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
nsDOMClassInfo::RegisterClassName(PRInt32 aClassInfoID)
{
  nsScriptNameSpaceManager *nameSpaceManager =
    nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nameSpaceManager->RegisterClassName(sClassInfoData[aClassInfoID].mName,
                                      aClassInfoID,
                                      sClassInfoData[aClassInfoID].mChromeOnly,
                                      sClassInfoData[aClassInfoID].mDisabled,
                                      &sClassInfoData[aClassInfoID].mNameUTF16);

  return NS_OK;
}

// static
nsresult
nsDOMClassInfo::RegisterClassProtos(PRInt32 aClassInfoID)
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
    const nsIID *iid = nsnull;

    if_info->GetIIDShared(&iid);
    NS_ENSURE_TRUE(iid, NS_ERROR_UNEXPECTED);

    if (iid->Equals(NS_GET_IID(nsISupports))) {
      break;
    }

    const char *name = nsnull;
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
  nsCAutoString categoryEntry;
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
      (_cond) ? &NS_GET_IID(_if) : nsnull,

#define DOM_CLASSINFO_MAP_END                                                 \
      nsnull                                                                  \
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSEvent)                                    \

#define DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES                                    \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMUIEvent)                                    \
    DOM_CLASSINFO_EVENT_MAP_ENTRIES

#define DOM_CLASSINFO_WINDOW_MAP_ENTRIES(_support_indexed_db)                  \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindow)                                        \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMJSWindow)                                      \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)                                   \
  DOM_CLASSINFO_MAP_ENTRY(nsIInlineEventHandlers)                              \
  DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsIDOMStorageIndexedDB,                  \
                                      _support_indexed_db)                     \
  DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsIDOMWindowPerformance,                 \
                                      nsGlobalWindow::HasPerformanceSupport()) \
  DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsITouchEventReceiver,                   \
                                      nsDOMTouchEvent::PrefEnabled())

nsresult
nsDOMClassInfo::Init()
{
  /* Errors that can trigger early returns are done first,
     otherwise nsDOMClassInfo is left in a half inited state. */
  NS_ASSERTION(sizeof(PtrBits) == sizeof(void*),
               "BAD! You'll need to adjust the size of PtrBits to the size "
               "of a pointer on your platform.");

  NS_ENSURE_TRUE(!sIsInitialized, NS_ERROR_ALREADY_INITIALIZED);

  nsScriptNameSpaceManager *nameSpaceManager = nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = CallGetService(nsIXPConnect::GetCID(), &sXPConnect);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPCFunctionThisTranslator> old;

  nsCOMPtr<nsIXPCFunctionThisTranslator> elt = new nsEventListenerThisTranslator();
  NS_ENSURE_TRUE(elt, NS_ERROR_OUT_OF_MEMORY);

  sXPConnect->SetFunctionThisTranslator(NS_GET_IID(nsIDOMEventListener),
                                        elt, getter_AddRefs(old));

  nsCOMPtr<nsIXPCFunctionThisTranslator> mctl = new nsMutationCallbackThisTranslator();
  NS_ENSURE_TRUE(elt, NS_ERROR_OUT_OF_MEMORY);

  sXPConnect->SetFunctionThisTranslator(NS_GET_IID(nsIMutationObserverCallback),
                                        mctl, getter_AddRefs(old));

  nsCOMPtr<nsIScriptSecurityManager> sm =
    do_GetService("@mozilla.org/scriptsecuritymanager;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  sSecMan = sm;
  NS_ADDREF(sSecMan);

  nsCOMPtr<nsIThreadJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  JSContext *cx = nsnull;

  rv = stack->GetSafeJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  DOM_CLASSINFO_MAP_BEGIN(Window, nsIDOMWindow)
    DOM_CLASSINFO_WINDOW_MAP_ENTRIES(nsGlobalWindow::HasIndexedDBSupport())
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WindowUtils, nsIDOMWindowUtils)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowUtils)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Location, nsIDOMLocation)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLocation)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Navigator, nsIDOMNavigator)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigator)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigatorGeolocation)
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsIDOMNavigatorDesktopNotification,
                                        Navigator::HasDesktopNotificationSupport())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMClientInformation)
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsIDOMMozNavigatorBattery,
                                        battery::BatteryManager::HasSupport())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozNavigatorSms)
#ifdef MOZ_B2G_RIL
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigatorTelephony)
#endif
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsIDOMMozNavigatorNetwork,
                                        network::IsAPIEnabled())
#ifdef MOZ_B2G_BT
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigatorBluetooth)
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

  DOM_CLASSINFO_MAP_BEGIN(BarProp, nsIDOMBarProp)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMBarProp)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(History, nsIDOMHistory)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHistory)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_MAYBE_DISABLE(PerformanceTiming, nsIDOMPerformanceTiming,
                                        !nsGlobalWindow::HasPerformanceSupport())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPerformanceTiming)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_MAYBE_DISABLE(PerformanceNavigation, nsIDOMPerformanceNavigation,
                                        !nsGlobalWindow::HasPerformanceSupport())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPerformanceNavigation)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_MAYBE_DISABLE(Performance, nsIDOMPerformance,
                                        !nsGlobalWindow::HasPerformanceSupport())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPerformance)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Screen, nsIDOMScreen)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMScreen)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(DOMPrototype, nsIDOMDOMConstructor)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMConstructor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMConstructor, nsIDOMDOMConstructor)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMConstructor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XMLDocument, nsIDOMXMLDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXMLDocument)
    DOM_CLASSINFO_DOCUMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DocumentType, nsIDOMDocumentType)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentType)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMImplementation, nsIDOMDOMImplementation)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMImplementation)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMException, nsIDOMDOMException)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMException)
    DOM_CLASSINFO_MAP_ENTRY(nsIException)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMTokenList, nsIDOMDOMTokenList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMTokenList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMSettableTokenList, nsIDOMDOMSettableTokenList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMSettableTokenList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DocumentFragment, nsIDOMDocumentFragment)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentFragment)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeSelector)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Element, nsIDOMElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeSelector)
    DOM_CLASSINFO_MAP_ENTRY(nsIInlineEventHandlers)
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsITouchEventReceiver,
                                        nsDOMTouchEvent::PrefEnabled())
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Attr, nsIDOMAttr)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMAttr)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Text, nsIDOMText)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMText)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Comment, nsIDOMComment)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMComment)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CDATASection, nsIDOMCDATASection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCDATASection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(ProcessingInstruction, nsIDOMProcessingInstruction)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMProcessingInstruction)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCharacterData)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(NodeList, nsIDOMNodeList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(NamedNodeMap, nsIDOMNamedNodeMap)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNamedNodeMap)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Event, nsIDOMEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(PopupBlockedEvent, nsIDOMPopupBlockedEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPopupBlockedEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DeviceOrientationEvent, nsIDOMDeviceOrientationEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDeviceOrientationEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DeviceMotionEvent, nsIDOMDeviceMotionEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDeviceMotionEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DeviceAcceleration, nsIDOMDeviceAcceleration)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDeviceAcceleration)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DeviceRotationRate, nsIDOMDeviceRotationRate)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDeviceRotationRate)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SmartCardEvent, nsIDOMSmartCardEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSmartCardEvent)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(PageTransitionEvent, nsIDOMPageTransitionEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPageTransitionEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MutationEvent, nsIDOMMutationEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMutationEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(UIEvent, nsIDOMUIEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END
  
  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(KeyboardEvent, nsIDOMKeyEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMKeyEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CompositionEvent, nsIDOMCompositionEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCompositionEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MouseEvent, nsIDOMMouseEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMouseEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MouseScrollEvent, nsIDOMMouseScrollEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMouseScrollEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMouseEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DragEvent, nsIDOMDragEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDragEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMouseEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(PopStateEvent, nsIDOMPopStateEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPopStateEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HashChangeEvent, nsIDOMHashChangeEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHashChangeEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLDocument, nsIDOMHTMLDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLDocument)
    DOM_CLASSINFO_DOCUMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLOptionsCollection, nsIDOMHTMLOptionsCollection)
    // Order is significant.  nsIDOMHTMLOptionsCollection.length shadows
    // nsIDOMHTMLCollection.length, which is readonly.
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLOptionsCollection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLCollection)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLCollection, nsIDOMHTMLCollection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLCollection)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLElement, nsIDOMHTMLElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLAnchorElement, nsIDOMHTMLAnchorElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLAnchorElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLAppletElement, nsIDOMHTMLAppletElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLAppletElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLAreaElement, nsIDOMHTMLAreaElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLAreaElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLBRElement, nsIDOMHTMLBRElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLBRElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLBaseElement, nsIDOMHTMLBaseElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLBaseElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLBodyElement, nsIDOMHTMLBodyElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLBodyElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLButtonElement, nsIDOMHTMLButtonElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLButtonElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLDataListElement, nsIDOMHTMLDataListElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLDataListElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLDListElement, nsIDOMHTMLDListElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLDListElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLDirectoryElement, nsIDOMHTMLDirectoryElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLDirectoryElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLDivElement, nsIDOMHTMLDivElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLDivElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLEmbedElement, nsIDOMHTMLEmbedElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLEmbedElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMGetSVGDocument)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLFieldSetElement, nsIDOMHTMLFieldSetElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLFieldSetElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLFontElement, nsIDOMHTMLFontElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLFontElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLFormElement, nsIDOMHTMLFormElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLFormElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLFrameElement, nsIDOMHTMLFrameElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLFrameElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozBrowserFrame)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLFrameSetElement, nsIDOMHTMLFrameSetElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLFrameSetElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLHRElement, nsIDOMHTMLHRElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLHRElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLHeadElement, nsIDOMHTMLHeadElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLHeadElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLHeadingElement, nsIDOMHTMLHeadingElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLHeadingElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLHtmlElement, nsIDOMHTMLHtmlElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLHtmlElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLIFrameElement, nsIDOMHTMLIFrameElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLIFrameElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMGetSVGDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozBrowserFrame)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLImageElement, nsIDOMHTMLImageElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLImageElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLInputElement, nsIDOMHTMLInputElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLInputElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLLIElement, nsIDOMHTMLLIElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLLIElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLLabelElement, nsIDOMHTMLLabelElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLLabelElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLLegendElement, nsIDOMHTMLLegendElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLLegendElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLLinkElement, nsIDOMHTMLLinkElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLLinkElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLinkStyle)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLMapElement, nsIDOMHTMLMapElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLMapElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLMenuElement, nsIDOMHTMLMenuElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLMenuElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLMenuItemElement, nsIDOMHTMLMenuItemElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLMenuItemElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLMetaElement, nsIDOMHTMLMetaElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLMetaElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLModElement, nsIDOMHTMLModElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLModElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLOListElement, nsIDOMHTMLOListElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLOListElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLObjectElement, nsIDOMHTMLObjectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLObjectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMGetSVGDocument)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLOptGroupElement, nsIDOMHTMLOptGroupElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLOptGroupElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLOptionElement, nsIDOMHTMLOptionElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLOptionElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLOutputElement, nsIDOMHTMLOutputElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLOutputElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLParagraphElement, nsIDOMHTMLParagraphElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLParagraphElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLParamElement, nsIDOMHTMLParamElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLParamElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLPreElement, nsIDOMHTMLPreElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLPreElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLProgressElement, nsIDOMHTMLProgressElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLProgressElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLQuoteElement, nsIDOMHTMLQuoteElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLQuoteElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLScriptElement, nsIDOMHTMLScriptElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLScriptElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLSelectElement, nsIDOMHTMLSelectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLSelectElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(HTMLSpanElement, nsIDOMHTMLElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLStyleElement, nsIDOMHTMLStyleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLStyleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLinkStyle)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLTableCaptionElement,
                          nsIDOMHTMLTableCaptionElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLTableCaptionElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLTableCellElement, nsIDOMHTMLTableCellElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLTableCellElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLTableColElement, nsIDOMHTMLTableColElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLTableColElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLTableElement, nsIDOMHTMLTableElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLTableElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLTableRowElement, nsIDOMHTMLTableRowElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLTableRowElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLTableSectionElement,
                          nsIDOMHTMLTableSectionElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLTableSectionElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLTextAreaElement, nsIDOMHTMLTextAreaElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLTextAreaElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLTitleElement, nsIDOMHTMLTitleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLTitleElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLUListElement, nsIDOMHTMLUListElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLUListElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLUnknownElement, nsIDOMHTMLUnknownElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLUnknownElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(ValidityState, nsIDOMValidityState)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMValidityState)
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

  DOM_CLASSINFO_MAP_BEGIN(CSSStyleDeclaration, nsIDOMCSSStyleDeclaration)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSStyleDeclaration)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSS2Properties)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ROCSSPrimitiveValue,
                                      nsIDOMCSSPrimitiveValue)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSPrimitiveValue)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSValueList, nsIDOMCSSValueList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSValueList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(CSSRect, nsIDOMRect)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMRect)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(CSSRGBColor, nsIDOMRGBColor)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMRGBColor)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSRGBAColor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Range, nsIDOMRange)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMRange)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(NodeIterator, nsIDOMNodeIterator)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeIterator)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(TreeWalker, nsIDOMTreeWalker)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMTreeWalker)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Selection, nsISelection)
    DOM_CLASSINFO_MAP_ENTRY(nsISelection)
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_XUL
  DOM_CLASSINFO_MAP_BEGIN(XULDocument, nsIDOMXULDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULDocument)
    DOM_CLASSINFO_DOCUMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XULElement, nsIDOMXULElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElementCSSInlineStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeSelector)
    DOM_CLASSINFO_MAP_ENTRY(nsIInlineEventHandlers)
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsITouchEventReceiver,
                                        nsDOMTouchEvent::PrefEnabled())
  DOM_CLASSINFO_MAP_END

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

  DOM_CLASSINFO_MAP_BEGIN(Crypto, nsIDOMCrypto)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCrypto)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CRMFObject, nsIDOMCRMFObject)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCRMFObject)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XMLStylesheetProcessingInstruction, nsIDOMProcessingInstruction)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMProcessingInstruction)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLinkStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ChromeWindow, nsIDOMWindow)
    DOM_CLASSINFO_WINDOW_MAP_ENTRIES(true)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMChromeWindow)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ContentList, nsIDOMHTMLCollection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLCollection)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(ImageDocument, nsIImageDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIImageDocument)
    DOM_CLASSINFO_DOCUMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_XUL
  DOM_CLASSINFO_MAP_BEGIN(XULTemplateBuilder, nsIXULTemplateBuilder)
    DOM_CLASSINFO_MAP_ENTRY(nsIXULTemplateBuilder)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XULTreeBuilder, nsIXULTreeBuilder)
    DOM_CLASSINFO_MAP_ENTRY(nsIXULTreeBuilder)
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

  DOM_CLASSINFO_MAP_BEGIN(TreeColumns, nsITreeColumns)
    DOM_CLASSINFO_MAP_ENTRY(nsITreeColumns)
  DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN(CSSMozDocumentRule, nsIDOMCSSMozDocumentRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSMozDocumentRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(BeforeUnloadEvent, nsIDOMBeforeUnloadEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMBeforeUnloadEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

#define DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES                           \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)                          \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGElement)                           \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeSelector)                         \
    DOM_CLASSINFO_MAP_ENTRY(nsIInlineEventHandlers)                     \
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsITouchEventReceiver,          \
                                        nsDOMTouchEvent::PrefEnabled())

#define DOM_CLASSINFO_SVG_TEXT_CONTENT_ELEMENT_MAP_ENTRIES \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextContentElement)   \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)                \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)             \
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES

#define DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLocatable)       \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTransformable)   \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)        \
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES

  // XXX - the proto chain stuff is sort of hackish, because of the MI in
  // the SVG interfaces. I doubt that extending the proto on one interface
  // works properly on an element which inherits off multiple interfaces.
  // Tough luck. - bbaetz

  // The SVG document

  DOM_CLASSINFO_MAP_BEGIN(SVGDocument, nsIDOMSVGDocument)
    // Order is significant.  nsIDOMDocument.title shadows
    // nsIDOMSVGDocument.title, which is readonly.
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGDocument)
    DOM_CLASSINFO_DOCUMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  // SVG element classes

  DOM_CLASSINFO_MAP_BEGIN(SVGAElement, nsIDOMSVGAElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAltGlyphElement, nsIDOMSVGAltGlyphElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextPositioningElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_SVG_TEXT_CONTENT_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimateElement, nsIDOMSVGAnimateElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimationElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimateElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElementTimeControl)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimateTransformElement,
                          nsIDOMSVGAnimateTransformElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimationElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimateTransformElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElementTimeControl)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimateMotionElement,
                          nsIDOMSVGAnimateMotionElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimationElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimateMotionElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElementTimeControl)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGSetElement,
                          nsIDOMSVGSetElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimationElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGSetElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElementTimeControl)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGMpathElement, nsIDOMSVGMpathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(TimeEvent, nsIDOMTimeEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMTimeEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGCircleElement, nsIDOMSVGCircleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGCircleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGClipPathElement, nsIDOMSVGClipPathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGClipPathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGUnitTypes)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGDefsElement, nsIDOMSVGDefsElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGDefsElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGDescElement, nsIDOMSVGDescElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGDescElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGEllipseElement, nsIDOMSVGEllipseElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGEllipseElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEBlendElement, nsIDOMSVGFEBlendElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEBlendElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEColorMatrixElement, nsIDOMSVGFEColorMatrixElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEColorMatrixElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEComponentTransferElement, nsIDOMSVGFEComponentTransferElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEComponentTransferElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFECompositeElement, nsIDOMSVGFECompositeElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFECompositeElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEConvolveMatrixElement, nsIDOMSVGFEConvolveMatrixElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEConvolveMatrixElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEDiffuseLightingElement, nsIDOMSVGFEDiffuseLightingElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEDiffuseLightingElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEDisplacementMapElement, nsIDOMSVGFEDisplacementMapElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEDisplacementMapElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEDistantLightElement, nsIDOMSVGFEDistantLightElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEDistantLightElement)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEFloodElement, nsIDOMSVGFEFloodElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEFloodElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEFuncAElement, nsIDOMSVGFEFuncAElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEFuncAElement)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEFuncBElement, nsIDOMSVGFEFuncBElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEFuncBElement)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEFuncGElement, nsIDOMSVGFEFuncGElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEFuncGElement)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEFuncRElement, nsIDOMSVGFEFuncRElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEFuncRElement)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEGaussianBlurElement, nsIDOMSVGFEGaussianBlurElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEGaussianBlurElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEImageElement, nsIDOMSVGFEImageElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEImageElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEMergeElement, nsIDOMSVGFEMergeElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEMergeElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEMorphologyElement, nsIDOMSVGFEMorphologyElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEMorphologyElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEMergeNodeElement, nsIDOMSVGFEMergeNodeElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEMergeNodeElement)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEOffsetElement, nsIDOMSVGFEOffsetElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEOffsetElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEPointLightElement, nsIDOMSVGFEPointLightElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEPointLightElement)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFESpecularLightingElement, nsIDOMSVGFESpecularLightingElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFESpecularLightingElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFESpotLightElement, nsIDOMSVGFESpotLightElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFESpotLightElement)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFETileElement, nsIDOMSVGFETileElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFETileElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFETurbulenceElement, nsIDOMSVGFETurbulenceElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFETurbulenceElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFilterElement, nsIDOMSVGFilterElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGUnitTypes)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGForeignObjectElement, nsIDOMSVGForeignObjectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGForeignObjectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGGElement, nsIDOMSVGGElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGGElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGImageElement, nsIDOMSVGImageElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGImageElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGLinearGradientElement, nsIDOMSVGLinearGradientElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGGradientElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLinearGradientElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGUnitTypes)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGLineElement, nsIDOMSVGLineElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLineElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGMarkerElement, nsIDOMSVGMarkerElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGMarkerElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFitToViewBox)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGMaskElement, nsIDOMSVGMaskElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGMaskElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGUnitTypes)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGMetadataElement, nsIDOMSVGMetadataElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGMetadataElement)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathElement, nsIDOMSVGPathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedPathData)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPatternElement, nsIDOMSVGPatternElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPatternElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFitToViewBox)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGUnitTypes)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPolygonElement, nsIDOMSVGPolygonElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPolygonElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedPoints)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPolylineElement, nsIDOMSVGPolylineElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPolylineElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedPoints)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGRadialGradientElement, nsIDOMSVGRadialGradientElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGGradientElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGRadialGradientElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGUnitTypes)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGRectElement, nsIDOMSVGRectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGRectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGScriptElement, nsIDOMSVGScriptElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGScriptElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGStopElement, nsIDOMSVGStopElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStopElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END
  
  DOM_CLASSINFO_MAP_BEGIN(SVGStyleElement, nsIDOMSVGStyleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStyleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLinkStyle)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGSVGElement, nsIDOMSVGSVGElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGSVGElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFitToViewBox)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLocatable)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGZoomAndPan)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGSwitchElement, nsIDOMSVGSwitchElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGSwitchElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGSymbolElement, nsIDOMSVGSymbolElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGSymbolElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFitToViewBox)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTextElement, nsIDOMSVGTextElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextPositioningElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextContentElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTextPathElement, nsIDOMSVGTextPathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_SVG_TEXT_CONTENT_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTitleElement, nsIDOMSVGTitleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTitleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTSpanElement, nsIDOMSVGTSpanElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextPositioningElement)
    DOM_CLASSINFO_SVG_TEXT_CONTENT_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGUnknownElement, nsIDOMSVGElement)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGUseElement, nsIDOMSVGUseElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGUseElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTests)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  // other SVG classes

  DOM_CLASSINFO_MAP_BEGIN(SVGAngle, nsIDOMSVGAngle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAngle)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedAngle, nsIDOMSVGAnimatedAngle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedAngle)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedBoolean, nsIDOMSVGAnimatedBoolean)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedBoolean)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedEnumeration, nsIDOMSVGAnimatedEnumeration)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedEnumeration)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedInteger, nsIDOMSVGAnimatedInteger)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedInteger)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedLength, nsIDOMSVGAnimatedLength)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedLength)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedLengthList, nsIDOMSVGAnimatedLengthList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedLengthList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedNumber, nsIDOMSVGAnimatedNumber)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedNumber)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedNumberList, nsIDOMSVGAnimatedNumberList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedNumberList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedPreserveAspectRatio, nsIDOMSVGAnimatedPreserveAspectRatio)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedPreserveAspectRatio)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedRect, nsIDOMSVGAnimatedRect)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedRect)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedString, nsIDOMSVGAnimatedString)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedString)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedTransformList, nsIDOMSVGAnimatedTransformList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedTransformList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGEvent, nsIDOMSVGEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGException, nsIDOMSVGException)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGException)
    DOM_CLASSINFO_MAP_ENTRY(nsIException)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGLength, nsIDOMSVGLength)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLength)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGLengthList, nsIDOMSVGLengthList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLengthList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGMatrix, nsIDOMSVGMatrix)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGMatrix)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGNumber, nsIDOMSVGNumber)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGNumber)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGNumberList, nsIDOMSVGNumberList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGNumberList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegArcAbs, nsIDOMSVGPathSegArcAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegArcAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegArcRel, nsIDOMSVGPathSegArcRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegArcRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegClosePath, nsIDOMSVGPathSegClosePath)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegClosePath)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoCubicAbs, nsIDOMSVGPathSegCurvetoCubicAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoCubicAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoCubicRel, nsIDOMSVGPathSegCurvetoCubicRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoCubicRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoCubicSmoothAbs, nsIDOMSVGPathSegCurvetoCubicSmoothAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoCubicSmoothAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoCubicSmoothRel, nsIDOMSVGPathSegCurvetoCubicSmoothRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoCubicSmoothRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoQuadraticAbs, nsIDOMSVGPathSegCurvetoQuadraticAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoQuadraticAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoQuadraticRel, nsIDOMSVGPathSegCurvetoQuadraticRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoQuadraticRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoQuadraticSmoothAbs, nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoQuadraticSmoothRel, nsIDOMSVGPathSegCurvetoQuadraticSmoothRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoQuadraticSmoothRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegLinetoAbs, nsIDOMSVGPathSegLinetoAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegLinetoAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegLinetoHorizontalAbs, nsIDOMSVGPathSegLinetoHorizontalAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegLinetoHorizontalAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegLinetoHorizontalRel, nsIDOMSVGPathSegLinetoHorizontalRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegLinetoHorizontalRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegLinetoRel, nsIDOMSVGPathSegLinetoRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegLinetoRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegLinetoVerticalAbs, nsIDOMSVGPathSegLinetoVerticalAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegLinetoVerticalAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegLinetoVerticalRel, nsIDOMSVGPathSegLinetoVerticalRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegLinetoVerticalRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegList, nsIDOMSVGPathSegList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegMovetoAbs, nsIDOMSVGPathSegMovetoAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegMovetoAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegMovetoRel, nsIDOMSVGPathSegMovetoRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegMovetoRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSeg)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPoint, nsIDOMSVGPoint)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPoint)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPointList, nsIDOMSVGPointList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPointList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPreserveAspectRatio, nsIDOMSVGPreserveAspectRatio)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPreserveAspectRatio)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGRect, nsIDOMSVGRect)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGRect)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGStringList, nsIDOMSVGStringList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStringList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTransform, nsIDOMSVGTransform)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTransform)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTransformList, nsIDOMSVGTransformList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTransformList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGZoomEvent, nsIDOMSVGZoomEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGZoomEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLCanvasElement, nsIDOMHTMLCanvasElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLCanvasElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CanvasRenderingContext2D, nsIDOMCanvasRenderingContext2D)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCanvasRenderingContext2D)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CanvasGradient, nsIDOMCanvasGradient)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCanvasGradient)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CanvasPattern, nsIDOMCanvasPattern)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCanvasPattern)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(TextMetrics, nsIDOMTextMetrics)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMTextMetrics)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(ImageData, nsIDOMImageData)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMImageData)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XSLTProcessor, nsIXSLTProcessor)
    DOM_CLASSINFO_MAP_ENTRY(nsIXSLTProcessor)
    DOM_CLASSINFO_MAP_ENTRY(nsIXSLTProcessorPrivate)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XPathEvaluator, nsIDOMXPathEvaluator)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXPathEvaluator)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XPathException, nsIDOMXPathException)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXPathException)
    DOM_CLASSINFO_MAP_ENTRY(nsIException)
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

  DOM_CLASSINFO_MAP_BEGIN(StorageObsolete, nsIDOMStorageObsolete)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorageObsolete)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Storage, nsIDOMStorage)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorage)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(StorageItem, nsIDOMStorageItem)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorageItem)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMToString)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(StorageEvent, nsIDOMStorageEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorageEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(StorageEventObsolete, nsIDOMStorageEventObsolete)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorageEventObsolete)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(DOMParser, nsIDOMParser)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMParser)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMParserJS)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XMLSerializer, nsIDOMSerializer)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSerializer)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XMLHttpRequest, nsIXMLHttpRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIXMLHttpRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIJSXMLHttpRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIXMLHttpRequestEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIInterfaceRequestor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XMLHttpProgressEvent, nsIDOMEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLSProgressEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMProgressEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(EventSource, nsIEventSource)
    DOM_CLASSINFO_MAP_ENTRY(nsIEventSource)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XULCommandEvent, nsIDOMXULCommandEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULCommandEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CommandEvent, nsIDOMCommandEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCommandEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(OfflineResourceList, nsIDOMOfflineResourceList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMOfflineResourceList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(ClientRect, nsIDOMClientRect)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMClientRect)
   DOM_CLASSINFO_MAP_END
 
  DOM_CLASSINFO_MAP_BEGIN(ClientRectList, nsIDOMClientRectList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMClientRectList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(FileList, nsIDOMFileList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMFileList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Blob, nsIDOMBlob)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMBlob)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(File, nsIDOMFile)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMBlob)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMFile)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(FileReader, nsIDOMFileReader)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMFileReader)
    DOM_CLASSINFO_MAP_ENTRY(nsIInterfaceRequestor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozURLProperty, nsIDOMMozURLProperty)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozURLProperty)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozBlobBuilder, nsIDOMMozBlobBuilder)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozBlobBuilder)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMStringMap, nsIDOMDOMStringMap)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMStringMap)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ModalContentWindow, nsIDOMWindow)
    DOM_CLASSINFO_WINDOW_MAP_ENTRIES(nsGlobalWindow::HasIndexedDBSupport())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMModalContentWindow)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DataContainerEvent, nsIDOMDataContainerEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDataContainerEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MessageEvent, nsIDOMMessageEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMessageEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(GeoGeolocation, nsIDOMGeoGeolocation)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMGeoGeolocation)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(GeoPosition, nsIDOMGeoPosition)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMGeoPosition)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(GeoPositionCoords, nsIDOMGeoPositionCoords)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMGeoPositionCoords)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(GeoPositionError, nsIDOMGeoPositionError)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMGeoPositionError)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozBatteryManager, nsIDOMMozBatteryManager)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozBatteryManager)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
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

  DOM_CLASSINFO_MAP_BEGIN(MozSmsMessage, nsIDOMMozSmsMessage)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSmsMessage)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozSmsEvent, nsIDOMMozSmsEvent)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSmsEvent)
     DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozSmsRequest, nsIDOMMozSmsRequest)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSmsRequest)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozSmsFilter, nsIDOMMozSmsFilter)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSmsFilter)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozSmsCursor, nsIDOMMozSmsCursor)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSmsCursor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozConnection, nsIDOMMozConnection)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozConnection)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozMobileConnection, nsIDOMMozMobileConnection)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozMobileConnection)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSFontFaceRule, nsIDOMCSSFontFaceRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSFontFaceRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(CSSFontFaceStyleDecl,
                                      nsIDOMCSSStyleDeclaration)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSStyleDeclaration)
  DOM_CLASSINFO_MAP_END

#if defined (MOZ_MEDIA)
  DOM_CLASSINFO_MAP_BEGIN(HTMLVideoElement, nsIDOMHTMLVideoElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLVideoElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLSourceElement, nsIDOMHTMLSourceElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLSourceElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MediaError, nsIDOMMediaError)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMediaError)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLAudioElement, nsIDOMHTMLAudioElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLAudioElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(TimeRanges, nsIDOMTimeRanges)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMTimeRanges)
  DOM_CLASSINFO_MAP_END  

  DOM_CLASSINFO_MAP_BEGIN(MediaStream, nsIDOMMediaStream)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMediaStream)
  DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN(ProgressEvent, nsIDOMProgressEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMProgressEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XMLHttpRequestUpload, nsIXMLHttpRequestUpload)
    DOM_CLASSINFO_MAP_ENTRY(nsIXMLHttpRequestEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIXMLHttpRequestUpload)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DataTransfer, nsIDOMDataTransfer)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDataTransfer)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(NotifyPaintEvent, nsIDOMNotifyPaintEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNotifyPaintEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(NotifyAudioAvailableEvent, nsIDOMNotifyAudioAvailableEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNotifyAudioAvailableEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SimpleGestureEvent, nsIDOMSimpleGestureEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSimpleGestureEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMouseEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozTouchEvent, nsIDOMMozTouchEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozTouchEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMouseEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(MathMLElement, nsIDOMElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeSelector)
    DOM_CLASSINFO_MAP_ENTRY(nsIInlineEventHandlers)
    DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsITouchEventReceiver,
                                        nsDOMTouchEvent::PrefEnabled())
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebGLRenderingContext, nsIDOMWebGLRenderingContext)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMWebGLRenderingContext)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebGLBuffer, nsIWebGLBuffer)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebGLBuffer)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebGLTexture, nsIWebGLTexture)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebGLTexture)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebGLProgram, nsIWebGLProgram)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebGLProgram)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebGLShader, nsIWebGLShader)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebGLShader)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebGLFramebuffer, nsIWebGLFramebuffer)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebGLFramebuffer)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebGLRenderbuffer, nsIWebGLRenderbuffer)
     DOM_CLASSINFO_MAP_ENTRY(nsIWebGLRenderbuffer)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebGLShaderPrecisionFormat, nsIWebGLShaderPrecisionFormat)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebGLShaderPrecisionFormat)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebGLUniformLocation, nsIWebGLUniformLocation)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebGLUniformLocation)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebGLActiveInfo, nsIWebGLActiveInfo)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebGLActiveInfo)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebGLExtension, nsIWebGLExtension)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebGLExtension)
  DOM_CLASSINFO_MAP_END
  
  DOM_CLASSINFO_MAP_BEGIN(WebGLExtensionStandardDerivatives, nsIWebGLExtensionStandardDerivatives)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebGLExtensionStandardDerivatives)
  DOM_CLASSINFO_MAP_END
  
  DOM_CLASSINFO_MAP_BEGIN(WebGLExtensionTextureFilterAnisotropic, nsIWebGLExtensionTextureFilterAnisotropic)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebGLExtensionTextureFilterAnisotropic)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebGLExtensionLoseContext, nsIWebGLExtensionLoseContext)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebGLExtensionLoseContext)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(PaintRequest, nsIDOMPaintRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPaintRequest)
   DOM_CLASSINFO_MAP_END
 
  DOM_CLASSINFO_MAP_BEGIN(PaintRequestList, nsIDOMPaintRequestList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPaintRequestList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(ScrollAreaEvent, nsIDOMScrollAreaEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMScrollAreaEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(EventListenerInfo, nsIEventListenerInfo)
    DOM_CLASSINFO_MAP_ENTRY(nsIEventListenerInfo)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(TransitionEvent, nsIDOMTransitionEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMTransitionEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(AnimationEvent, nsIDOMAnimationEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMAnimationEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ContentFrameMessageManager, nsIContentFrameMessageManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIFrameMessageManager)
    DOM_CLASSINFO_MAP_ENTRY(nsISyncMessageSender)
    DOM_CLASSINFO_MAP_ENTRY(nsIContentFrameMessageManager)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(FormData, nsIDOMFormData)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMFormData)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DesktopNotification, nsIDOMDesktopNotification)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDesktopNotification)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DesktopNotificationCenter, nsIDOMDesktopNotificationCenter)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDesktopNotificationCenter)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WebSocket, nsIWebSocket)
    DOM_CLASSINFO_MAP_ENTRY(nsIWebSocket)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CloseEvent, nsIDOMCloseEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCloseEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(IDBFactory, nsIIDBFactory)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBFactory)
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

  DOM_CLASSINFO_MAP_BEGIN(IDBVersionChangeEvent, nsIIDBVersionChangeEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBVersionChangeEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEvent)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(IDBOpenDBRequest, nsIIDBOpenDBRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBOpenDBRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIIDBRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_MAYBE_DISABLE(Touch, nsIDOMTouch,
                                        !nsDOMTouchEvent::PrefEnabled())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMTouch)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_MAYBE_DISABLE(TouchList, nsIDOMTouchList,
                                        !nsDOMTouchEvent::PrefEnabled())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMTouchList)
  DOM_CLASSINFO_MAP_END
  
  DOM_CLASSINFO_MAP_BEGIN_MAYBE_DISABLE(TouchEvent, nsIDOMTouchEvent,
                                        !nsDOMTouchEvent::PrefEnabled())
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMTouchEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozCSSKeyframeRule, nsIDOMMozCSSKeyframeRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozCSSKeyframeRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozCSSKeyframesRule, nsIDOMMozCSSKeyframesRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozCSSKeyframesRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MediaQueryList, nsIDOMMediaQueryList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMediaQueryList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CustomEvent, nsIDOMCustomEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCustomEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozMutationObserver, nsIDOMMozMutationObserver)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozMutationObserver)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MutationRecord, nsIDOMMutationRecord)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMutationRecord)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozSettingsEvent, nsIDOMMozSettingsEvent)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSettingsEvent)
     DOM_CLASSINFO_EVENT_MAP_ENTRIES
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

  DOM_CLASSINFO_MAP_BEGIN(CallEvent, nsIDOMCallEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCallEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEvent)
  DOM_CLASSINFO_MAP_END
#endif

#ifdef MOZ_B2G_BT
  DOM_CLASSINFO_MAP_BEGIN(BluetoothAdapter, nsIDOMBluetoothAdapter)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMBluetoothAdapter)
  DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN(DOMError, nsIDOMDOMError)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMError)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMRequest, nsIDOMDOMRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMRequest)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

#ifdef NS_DEBUG
  {
    PRUint32 i = ArrayLength(sClassInfoData);

    if (i != eDOMClassInfoIDCount) {
      NS_ERROR("The number of items in sClassInfoData doesn't match the "
               "number of nsIDOMClassInfo ID's, this is bad! Fix it!");

      return NS_ERROR_NOT_INITIALIZED;
    }

    for (i = 0; i < eDOMClassInfoIDCount; i++) {
      if (!sClassInfoData[i].u.mConstructorFptr ||
          sClassInfoData[i].mDebugID != i) {
        NS_ERROR("Class info data out of sync, you forgot to update "
                 "nsDOMClassInfo.h and nsDOMClassInfo.cpp! Fix this, "
                 "mozilla will not work without this fixed!");

        return NS_ERROR_NOT_INITIALIZED;
      }
    }

    for (i = 0; i < eDOMClassInfoIDCount; i++) {
      if (!sClassInfoData[i].mInterfaces) {
        NS_ERROR("Class info data without an interface list! Fix this, "
                 "mozilla will not work without this fixed!");

        return NS_ERROR_NOT_INITIALIZED;
      }
    }
  }
#endif

  // Initialize static JSString's
  DefineStaticJSVals(cx);

  PRInt32 i;

  for (i = 0; i < eDOMClassInfoIDCount; ++i) {
    RegisterClassName(i);
  }

  for (i = 0; i < eDOMClassInfoIDCount; ++i) {
    RegisterClassProtos(i);
  }

  RegisterExternalClasses();

  sDisableDocumentAllSupport =
    Preferences::GetBool("browser.dom.document.all.disabled");

  sDisableGlobalScopePollutionSupport =
    Preferences::GetBool("browser.dom.global_scope_pollution.disabled");

  // Proxy bindings
  mozilla::dom::binding::Register(nameSpaceManager);

  // Non-proxy bindings
  mozilla::dom::Register(nameSpaceManager);

  sIsInitialized = true;

  return NS_OK;
}

// static
PRInt32
nsDOMClassInfo::GetArrayIndexFromId(JSContext *cx, jsid id, bool *aIsNumber)
{
  if (aIsNumber) {
    *aIsNumber = false;
  }

  int i;
  if (JSID_IS_INT(id)) {
      i = JSID_TO_INT(id);
  } else {
      JSAutoRequest ar(cx);

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
nsDOMClassInfo::GetInterfaces(PRUint32 *aCount, nsIID ***aArray)
{
  PRUint32 count = 0;

  while (mData->mInterfaces[count]) {
    count++;
  }

  *aCount = count;

  if (!count) {
    *aArray = nsnull;

    return NS_OK;
  }

  *aArray = static_cast<nsIID **>(nsMemory::Alloc(count * sizeof(nsIID *)));
  NS_ENSURE_TRUE(*aArray, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 i;
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
nsDOMClassInfo::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
{
  if (language == nsIProgrammingLanguage::JAVASCRIPT) {
    *_retval = static_cast<nsIXPCScriptable *>(this);

    NS_ADDREF(*_retval);
  } else {
    *_retval = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetContractID(char **aContractID)
{
  *aContractID = nsnull;

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
  *aClassID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetClassIDNoAlloc(nsCID *aClassID)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMClassInfo::GetImplementationLanguage(PRUint32 *aImplLanguage)
{
  *aImplLanguage = nsIProgrammingLanguage::CPLUSPLUS;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetFlags(PRUint32 *aFlags)
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

NS_IMETHODIMP
nsDOMClassInfo::GetScriptableFlags(PRUint32 *aFlags)
{
  *aFlags = mData->mScriptableFlags;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::PreCreate(nsISupports *nativeObj, JSContext *cx,
                          JSObject *globalObj, JSObject **parentObj)
{
  *parentObj = globalObj;

  nsCOMPtr<nsPIDOMWindow> piwin = do_QueryWrapper(cx, globalObj);

  if (!piwin) {
    return NS_OK;
  }

  if (piwin->IsOuterWindow()) {
    nsGlobalWindow *win = ((nsGlobalWindow *)piwin.get())->
                            GetCurrentInnerWindowInternal();
    return SetParentToWindow(win, parentObj);
  }

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
nsDOMClassInfo::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, jsval *vp,
                            bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::AddProperty Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, jsval *vp,
                            bool *_retval)
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
                             JSContext *cx, JSObject *obj, PRUint32 enum_op,
                             jsval *statep, jsid *idp, bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::NewEnumerate Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

nsresult
nsDOMClassInfo::ResolveConstructor(JSContext *cx, JSObject *obj,
                                   JSObject **objp)
{
  JSObject *global = ::JS_GetGlobalForObject(cx, obj);

  jsval val;
  JSAutoRequest ar(cx);
  if (!::JS_LookupProperty(cx, global, mData->mName, &val)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!JSVAL_IS_PRIMITIVE(val)) {
    // If val is not an (non-null) object there either is no
    // constructor for this class, or someone messed with
    // window.classname, just fall through and let the JS engine
    // return the Object constructor.

    if (!::JS_DefinePropertyById(cx, obj, sConstructor_id, val, nsnull, nsnull,
                                 JSPROP_ENUMERATE)) {
      return NS_ERROR_UNEXPECTED;
    }

    *objp = obj;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                           JSObject *obj, jsid id, PRUint32 flags,
                           JSObject **objp, bool *_retval)
{
  if (id == sConstructor_id && !(flags & JSRESOLVE_ASSIGNING)) {
    return ResolveConstructor(cx, obj, objp);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::Convert(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, PRUint32 type, jsval *vp,
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
                            JSObject *obj, jsid id, PRUint32 mode,
                            jsval *vp, bool *_retval)
{
  PRUint32 mode_type = mode & JSACC_TYPEMASK;

  if ((mode_type == JSACC_WATCH ||
       mode_type == JSACC_PROTO ||
       mode_type == JSACC_PARENT) &&
      sSecMan) {

    nsresult rv;
    JSObject *real_obj;
    if (wrapper) {
      rv = wrapper->GetJSObject(&real_obj);
      NS_ENSURE_SUCCESS(rv, rv);
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
                     JSObject *obj, PRUint32 argc, jsval *argv, jsval *vp,
                     bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::Call Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, PRUint32 argc, jsval *argv,
                          jsval *vp, bool *_retval)
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
nsDOMClassInfo::Equality(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, const jsval &val, bool *bp)
{
  NS_WARNING("nsDOMClassInfo::Equality Don't call me!");

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

    *aResult = nsnull;
  }

  return NS_OK;
}


static nsresult
ResolvePrototype(nsIXPConnect *aXPConnect, nsGlobalWindow *aWin, JSContext *cx,
                 JSObject *obj, const PRUnichar *name,
                 const nsDOMClassInfoData *ci_data,
                 const nsGlobalNameStruct *name_struct,
                 nsScriptNameSpaceManager *nameSpaceManager,
                 JSObject *dot_prototype, bool install, bool *did_resolve);


NS_IMETHODIMP
nsDOMClassInfo::PostCreatePrototype(JSContext * cx, JSObject * proto)
{
  PRUint32 flags = (mData->mScriptableFlags & DONT_ENUM_STATIC_PROPS)
                   ? 0
                   : JSPROP_ENUMERATE;

  PRUint32 count = 0;
  while (mData->mInterfaces[count]) {
    count++;
  }

  if (!xpc::DOM_DefineQuickStubs(cx, proto, flags, count, mData->mInterfaces)) {
    JS_ClearPendingException(cx);
  }

  // This is called before any other location that requires
  // sObjectClass, so compute it here. We assume that nobody has had a
  // chance to monkey around with proto's prototype chain before this.
  if (!sObjectClass) {
    FindObjectClass(proto);
    NS_ASSERTION(sObjectClass && !strcmp(sObjectClass->name, "Object"),
                 "Incorrect object class!");
  }

  NS_ASSERTION(::JS_GetPrototype(proto) &&
               JS_GetClass(::JS_GetPrototype(proto)) == sObjectClass,
               "Hmm, somebody did something evil?");

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
  JSObject *global = ::JS_GetGlobalForObject(cx, proto);

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
                                    nsCRT::strlen(mData->mNameUTF16), &found)) {
    return NS_ERROR_FAILURE;
  }

  nsScriptNameSpaceManager *nameSpaceManager =
    nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_OK);

  bool unused;
  return ResolvePrototype(sXPConnect, win, cx, global, mData->mNameUTF16,
                          mData, nsnull, nameSpaceManager, proto, !found,
                          &unused);
}

// static
nsIClassInfo *
NS_GetDOMClassInfoInstance(nsDOMClassInfoID aID)
{
  if (aID >= eDOMClassInfoIDCount) {
    NS_ERROR("Bad ID!");

    return nsnull;
  }

  if (!nsDOMClassInfo::sIsInitialized) {
    nsresult rv = nsDOMClassInfo::Init();

    NS_ENSURE_SUCCESS(rv, nsnull);
  }

  if (!sClassInfoData[aID].mCachedClassInfo) {
    nsDOMClassInfoData& data = sClassInfoData[aID];

    data.mCachedClassInfo = data.u.mConstructorFptr(&data);
    NS_ENSURE_TRUE(data.mCachedClassInfo, nsnull);

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
    NS_ENSURE_TRUE(aData->mCachedClassInfo, nsnull);

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
    PRUint32 i;

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
  sDialogArguments_id = JSID_VOID;
  sControllers_id     = JSID_VOID;
  sLength_id          = JSID_VOID;
  sInnerHeight_id     = JSID_VOID;
  sInnerWidth_id      = JSID_VOID;
  sOuterHeight_id     = JSID_VOID;
  sOuterWidth_id      = JSID_VOID;
  sScreenX_id         = JSID_VOID;
  sScreenY_id         = JSID_VOID;
  sStatus_id          = JSID_VOID;
  sName_id            = JSID_VOID;
  sScrollX_id         = JSID_VOID;
  sScrollY_id         = JSID_VOID;
  sScrollMaxX_id      = JSID_VOID;
  sScrollMaxY_id      = JSID_VOID;
  sItem_id            = JSID_VOID;
  sEnumerate_id       = JSID_VOID;
  sNavigator_id       = JSID_VOID;
  sDocument_id        = JSID_VOID;
  sFrames_id          = JSID_VOID;
  sSelf_id            = JSID_VOID;
  sOpener_id          = JSID_VOID;
  sAll_id             = JSID_VOID;
  sTags_id            = JSID_VOID;
  sAddEventListener_id= JSID_VOID;
  sBaseURIObject_id   = JSID_VOID;
  sNodePrincipal_id   = JSID_VOID;
  sDocumentURIObject_id=JSID_VOID;
  sWrappedJSObject_id = JSID_VOID;
  sKeyPath_id         = JSID_VOID;
  sAutoIncrement_id   = JSID_VOID;
  sUnique_id          = JSID_VOID;
  sMultiEntry_id      = JSID_VOID;
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

// This JS class piggybacks on nsHTMLDocumentSH::ReleaseDocument()...

static JSClass sGlobalScopePolluterClass = {
  "Global Scope Polluter",
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_NEW_RESOLVE,
  nsWindowSH::SecurityCheckOnAddDelProp,
  nsWindowSH::SecurityCheckOnAddDelProp,
  nsWindowSH::GlobalScopePolluterGetProperty,
  nsWindowSH::SecurityCheckOnSetProp,
  JS_EnumerateStub,
  (JSResolveOp)nsWindowSH::GlobalScopePolluterNewResolve,
  JS_ConvertStub,
  nsHTMLDocumentSH::ReleaseDocument
};


// static
JSBool
nsWindowSH::GlobalScopePolluterGetProperty(JSContext *cx, JSObject *obj,
                                           jsid id, jsval *vp)
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

  // Print a warning on the console so developers have a chance to
  // catch and fix these mistakes.
  PrintWarningOnConsole(cx, "GlobalScopeElementReference");

  return JS_TRUE;
}

// static
JSBool
nsWindowSH::SecurityCheckOnAddDelProp(JSContext *cx, JSObject *obj, jsid id,
                                      jsval *vp)
{
  // Someone is accessing a element by referencing its name/id in the
  // global scope, do a security check to make sure that's ok.

  nsresult rv =
    sSecMan->CheckPropertyAccess(cx, ::JS_GetGlobalForObject(cx, obj),
                                 "Window", id,
                                 nsIXPCSecurityManager::ACCESS_SET_PROPERTY);

  // If !NS_SUCCEEDED(rv) the security check failed. The security
  // manager set a JS exception for us.
  return NS_SUCCEEDED(rv);
}

// static
JSBool
nsWindowSH::SecurityCheckOnSetProp(JSContext *cx, JSObject *obj, jsid id, JSBool strict,
                                   jsval *vp)
{
  return SecurityCheckOnAddDelProp(cx, obj, id, vp);
}

static nsHTMLDocument*
GetDocument(JSObject *obj)
{
  return static_cast<nsHTMLDocument*>(
    static_cast<nsIHTMLDocument*>(::JS_GetPrivate(obj)));
}

// static
JSBool
nsWindowSH::GlobalScopePolluterNewResolve(JSContext *cx, JSObject *obj,
                                          jsid id, unsigned flags,
                                          JSObject **objp)
{
  if (flags & (JSRESOLVE_ASSIGNING | JSRESOLVE_DECLARING |
               JSRESOLVE_CLASSNAME | JSRESOLVE_QUALIFIED) ||
      !JSID_IS_STRING(id)) {
    // Nothing to do here if we're either assigning or declaring,
    // resolving a class name, doing a qualified resolve, or
    // resolving a number.

    return JS_TRUE;
  }

  nsHTMLDocument *document = GetDocument(obj);

  if (!document) {
    // If we don't have a document, return early.

    return JS_TRUE;
  }

  JSObject *proto = ::JS_GetPrototype(obj);
  JSBool hasProp;

  if (!proto || !::JS_HasPropertyById(cx, proto, id, &hasProp) ||
      hasProp) {
    // No prototype, or the property exists on the prototype. Do
    // nothing.

    return JS_TRUE;
  }

  nsDependentJSString str(id);
  nsCOMPtr<nsISupports> result;
  nsWrapperCache *cache;
  {
    Element *element = document->GetElementById(str);
    result = element;
    cache = element;
  }

  if (!result) {
    document->ResolveName(str, nsnull, getter_AddRefs(result), &cache);
  }

  if (result) {
    jsval v;
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult rv = WrapNative(cx, obj, result, cache, true, &v,
                             getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, JS_FALSE);

    if (!JS_WrapValue(cx, &v) ||
        !JS_DefinePropertyById(cx, obj, id, v, nsnull, nsnull, 0)) {
      return JS_FALSE;
    }

    *objp = obj;
  }

  return JS_TRUE;
}

// static
void
nsWindowSH::InvalidateGlobalScopePolluter(JSContext *cx, JSObject *obj)
{
  JSObject *proto;

  JSAutoRequest ar(cx);

  while ((proto = ::JS_GetPrototype(obj))) {
    if (JS_GetClass(proto) == &sGlobalScopePolluterClass) {
      nsIHTMLDocument *doc = (nsIHTMLDocument *)::JS_GetPrivate(proto);

      NS_IF_RELEASE(doc);

      ::JS_SetPrivate(proto, nsnull);

      // Pull the global scope polluter out of the prototype chain so
      // that it can be freed.
      ::JS_SplicePrototype(cx, obj, ::JS_GetPrototype(proto));

      break;
    }

    obj = proto;
  }
}

// static
nsresult
nsWindowSH::InstallGlobalScopePolluter(JSContext *cx, JSObject *obj,
                                       nsIHTMLDocument *doc)
{
  // If global scope pollution is disabled, or if our document is not
  // a HTML document, do nothing
  if (sDisableGlobalScopePollutionSupport || !doc) {
    return NS_OK;
  }

  JSAutoRequest ar(cx);

  JSObject *gsp = ::JS_NewObjectWithUniqueType(cx, &sGlobalScopePolluterClass, nsnull, obj);
  if (!gsp) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  JSObject *o = obj, *proto;

  // Find the place in the prototype chain where we want this global
  // scope polluter (right before Object.prototype).

  while ((proto = ::JS_GetPrototype(o))) {
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

  ::JS_SetPrivate(gsp, doc);

  // The global scope polluter will release doc on destruction (or
  // invalidation).
  NS_ADDREF(doc);

  return NS_OK;
}

static
already_AddRefed<nsIDOMWindow>
GetChildFrame(nsGlobalWindow *win, PRUint32 index)
{
  nsCOMPtr<nsIDOMWindowCollection> frames;
  win->GetFrames(getter_AddRefs(frames));

  nsIDOMWindow *frame = nsnull;

  if (frames) {
    frames->Item(index, &frame);
  }

  return frame;
}

NS_IMETHODIMP
nsWindowSH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, jsval *vp, bool *_retval)
{
  nsGlobalWindow *win = nsGlobalWindow::FromWrapper(wrapper);

  JSAutoRequest ar(cx);

#ifdef DEBUG_SH_FORWARDING
  {
    JSString *jsstr = ::JS_ValueToString(cx, id);
    if (jsstr) {
      nsDependentJSString str(jsstr);

      if (win->IsInnerWindow()) {
#ifdef DEBUG_PRINT_INNER
        printf("Property '%s' get on inner window %p\n",
              NS_ConvertUTF16toUTF8(str).get(), (void *)win);
#endif
      } else {
        printf("Property '%s' get on outer window %p\n",
              NS_ConvertUTF16toUTF8(str).get(), (void *)win);
      }
    }
  }
#endif

  // The order in which things are done in this method are a bit
  // whacky, that's because this method is *extremely* performace
  // critical. Don't touch this unless you know what you're doing.

  if (JSID_IS_INT(id) && JSID_TO_INT(id) >= 0) {
    // If we're accessing a numeric property we'll treat that as if
    // window.frames[n] is accessed (since window.frames === window),
    // if window.frames[n] is a child frame, wrap the frame and return
    // it without doing a security check.
    PRUint32 index = PRUint32(JSID_TO_INT(id));
    nsresult rv = NS_OK;
    if (nsCOMPtr<nsIDOMWindow> frame = GetChildFrame(win, index)) {
      // A numeric property accessed and the numeric property is a
      // child frame, wrap the child frame without doing a security
      // check and return.

      nsGlobalWindow *frameWin = (nsGlobalWindow *)frame.get();
      NS_ASSERTION(frameWin->IsOuterWindow(), "GetChildFrame gave us an inner?");

      frameWin->EnsureInnerWindow();

      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      jsval v;
      rv = WrapNative(cx, frameWin->GetGlobalJSObject(), frame,
                      &NS_GET_IID(nsIDOMWindow), true, &v,
                      getter_AddRefs(holder));
      NS_ENSURE_SUCCESS(rv, rv);

      if (!JS_WrapValue(cx, &v)) {
        return NS_ERROR_FAILURE;
      }

      *vp = v;
    }

    return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
  }

  if (JSID_IS_STRING(id) && !JSVAL_IS_PRIMITIVE(*vp) &&
      ::JS_TypeOfValue(cx, *vp) != JSTYPE_FUNCTION) {
    // A named property accessed which could have been resolved to a
    // child frame in nsWindowSH::NewResolve() (*vp will tell us if
    // that's the case). If *vp is a window object (i.e. a child
    // frame), return without doing a security check.
    //
    // Calling GetWrappedNativeOfJSObject() is not all that cheap, so
    // only do that if the JSClass name is one that is likely to be a
    // window object.

    const char *name = JS_GetClass(JSVAL_TO_OBJECT(*vp))->name;

    // The list of Window class names here need to be kept in sync
    // with the actual class names! The class name
    // XPCCrossOriginWrapper needs to be handled here too as XOWs
    // define child frame names with a XOW as the value, and thus
    // we'll need to get through here with XOWs class name too.
    if ((*name == 'W' && strcmp(name, "Window") == 0) ||
        (*name == 'C' && strcmp(name, "ChromeWindow") == 0) ||
        (*name == 'M' && strcmp(name, "ModalContentWindow") == 0) ||
        (*name == 'I' &&
         (strcmp(name, "InnerWindow") == 0 ||
          strcmp(name, "InnerChromeWindow") == 0 ||
          strcmp(name, "InnerModalContentWindow") == 0)) ||
        (*name == 'X' && strcmp(name, "XPCCrossOriginWrapper") == 0)) {
      nsCOMPtr<nsIDOMWindow> window = do_QueryWrapper(cx, JSVAL_TO_OBJECT(*vp));

      if (window) {
        // Yup, *vp is a window object, return early (*vp is already
        // the window, so no need to wrap it again).

        return NS_SUCCESS_I_DID_SOMETHING;
      }
    }
  }

  if (id == sWrappedJSObject_id &&
      xpc::AccessCheck::isChrome(js::GetContextCompartment(cx))) {
    obj = JS_ObjectToOuterObject(cx, obj);
    *vp = OBJECT_TO_JSVAL(obj);
    return NS_SUCCESS_I_DID_SOMETHING;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                      JSObject *obj, bool *_retval)
{
  if (!ObjectIsNativeWrapper(cx, obj)) {
    *_retval = JS_EnumerateStandardClasses(cx, obj);
  }

  return NS_OK;
}

static const char*
FindConstructorContractID(const nsDOMClassInfoData *aDOMClassInfoData)
{
  PRUint32 i;
  for (i = 0; i < ArrayLength(kConstructorMap); ++i) {
    if (&sClassInfoData[kConstructorMap[i].mDOMClassInfoID] ==
        aDOMClassInfoData) {
      return kConstructorMap[i].mContractID;
    }
  }
  return nsnull;
}

static nsDOMConstructorFunc
FindConstructorFunc(const nsDOMClassInfoData *aDOMClassInfoData)
{
  for (PRUint32 i = 0; i < ArrayLength(kConstructorFuncMap); ++i) {
    if (&sClassInfoData[kConstructorFuncMap[i].mDOMClassInfoID] ==
        aDOMClassInfoData) {
      return kConstructorFuncMap[i].mConstructorFunc;
    }
  }
  return nsnull;
}

static nsresult
BaseStubConstructor(nsIWeakReference* aWeakOwner,
                    const nsGlobalNameStruct *name_struct, JSContext *cx,
                    JSObject *obj, unsigned argc, jsval *argv, jsval *rval)
{
  nsresult rv;
  nsCOMPtr<nsISupports> native;
  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    const nsDOMClassInfoData* ci_data =
      &sClassInfoData[name_struct->mDOMClassInfoID];
    const char *contractid = FindConstructorContractID(ci_data);
    if (contractid) {
      native = do_CreateInstance(contractid, &rv);
    }
    else {
      nsDOMConstructorFunc func = FindConstructorFunc(ci_data);
      if (func) {
        rv = func(getter_AddRefs(native));
      }
      else {
        rv = NS_ERROR_NOT_AVAILABLE;
      }
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
  if (initializer) {
    // Initialize object using the current inner window, but only if
    // the caller can access it.
    nsCOMPtr<nsPIDOMWindow> owner = do_QueryReferent(aWeakOwner);
    nsPIDOMWindow* outerWindow = owner ? owner->GetOuterWindow() : nsnull;
    nsPIDOMWindow* currentInner =
      outerWindow ? outerWindow->GetCurrentInnerWindow() : nsnull;
    if (!currentInner ||
        (owner != currentInner &&
         !nsContentUtils::CanCallerAccess(currentInner))) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    rv = initializer->Initialize(currentInner, cx, obj, argc, argv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(native));
  if (owner) {
    nsIScriptContext *context = nsJSUtils::GetStaticScriptContext(cx, obj);
    if (!context) {
      return NS_ERROR_UNEXPECTED;
    }

    JSObject* new_obj;
    rv = owner->GetScriptObject(context, (void**)&new_obj);

    if (NS_SUCCEEDED(rv)) {
      *rval = OBJECT_TO_JSVAL(new_obj);
    }

    return rv;
  }

  return WrapNative(cx, obj, native, true, rval);
}

static nsresult
DefineInterfaceConstants(JSContext *cx, JSObject *obj, const nsIID *aIID)
{
  nsCOMPtr<nsIInterfaceInfoManager>
    iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
  NS_ENSURE_TRUE(iim, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIInterfaceInfo> if_info;

  nsresult rv = iim->GetInfoForIID(aIID, getter_AddRefs(if_info));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && if_info, rv);

  PRUint16 constant_count;

  if_info->GetConstantCount(&constant_count);

  if (!constant_count) {
    return NS_OK;
  }

  nsCOMPtr<nsIInterfaceInfo> parent_if_info;

  rv = if_info->GetParent(getter_AddRefs(parent_if_info));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && parent_if_info, rv);

  PRUint16 parent_constant_count, i;
  parent_if_info->GetConstantCount(&parent_constant_count);

  for (i = parent_constant_count; i < constant_count; i++) {
    const nsXPTConstant *c = nsnull;

    rv = if_info->GetConstant(i, &c);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && c, rv);

    PRUint16 type = c->GetType().TagPart();

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
        if (!JS_NewNumberValue(cx, c->GetValue()->val.i32, &v)) {
          return NS_ERROR_UNEXPECTED;
        }
        break;
      }
      case nsXPTType::T_U32:
      {
        if (!JS_NewNumberValue(cx, c->GetValue()->val.u32, &v)) {
          return NS_ERROR_UNEXPECTED;
        }
        break;
      }
      default:
      {
#ifdef NS_DEBUG
        NS_ERROR("Non-numeric constant found in interface.");
#endif
        continue;
      }
    }

    if (!::JS_DefineProperty(cx, obj, c->GetName(), v, nsnull, nsnull,
                             JSPROP_ENUMERATE)) {
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
IDBConstantGetter(JSContext *cx, JSObject *obj, jsid id, jsval* vp)
{
  MOZ_ASSERT(JSID_IS_INT(id));
  
  int8_t index = JSID_TO_INT(id);
  
  MOZ_ASSERT((uint8_t)index < mozilla::ArrayLength(sIDBConstants));

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

  PRUint64 windowID = 0;
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
    nsresult rv = errorObject->InitWithWindowID(warnText.get(),
                                                nsnull, // file name
                                                nsnull, // source line
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
  jsval value;
  if (!xpc::StringToJsval(cx, valStr, &value)) {
    return JS_FALSE;
  }
  if (!::JS_DefineProperty(cx, obj, c.name, value, nsnull, nsnull,
                           JSPROP_ENUMERATE)) {
    return JS_FALSE;
  }

  // Return value
  *vp = value;
  return JS_TRUE;
}

static nsresult
DefineIDBInterfaceConstants(JSContext *cx, JSObject *obj, const nsIID *aIID)
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

  for (int8_t i = 0; i < (int8_t)mozilla::ArrayLength(sIDBConstants); ++i) {
    const IDBConstant& c = sIDBConstants[i];
    if (c.interface != interface) {
      continue;
    }

    if (!::JS_DefinePropertyWithTinyId(cx, obj, c.name, i, JSVAL_VOID,
                                       IDBConstantGetter, nsnull,
                                       JSPROP_ENUMERATE)) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

class nsDOMConstructor : public nsIDOMDOMConstructor
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
                     JSObject *obj, PRUint32 argc, jsval *argv,
                     jsval *vp, bool *_retval);

  nsresult HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, const jsval &val, bool *bp,
                       bool *_retval);

  nsresult Install(JSContext *cx, JSObject *target, jsval thisAsVal)
  {
    // The 'attrs' argument used to be JSPROP_PERMANENT. See bug 628612.
    JSBool ok = JS_WrapValue(cx, &thisAsVal) &&
      ::JS_DefineUCProperty(cx, target,
                            reinterpret_cast<const jschar *>(mClassName),
                            nsCRT::strlen(mClassName), thisAsVal, nsnull,
                            nsnull, 0);

    return ok ? NS_OK : NS_ERROR_UNEXPECTED;
  }

  nsresult ResolveInterfaceConstants(JSContext *cx, JSObject *obj);

private:
  const nsGlobalNameStruct *GetNameStruct()
  {
    if (!mClassName) {
      NS_ERROR("Can't get name");
      return nsnull;
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
    *aNameStruct = nsnull;

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
      return data->mConstructorCID != nsnull;
    }

    return FindConstructorContractID(aData) || FindConstructorFunc(aData);
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
  *aResult = nsnull;
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
      *aInstancePtr = nsnull;
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
                            JSObject * obj, PRUint32 argc, jsval * argv,
                            jsval * vp, bool *_retval)
{
  JSObject* class_obj = JSVAL_TO_OBJECT(argv[-2]);
  if (!class_obj) {
    NS_ERROR("nsDOMConstructor::Construct couldn't get constructor object.");
    return NS_ERROR_UNEXPECTED;
  }

  const nsGlobalNameStruct *name_struct = GetNameStruct();
  NS_ENSURE_TRUE(name_struct, NS_ERROR_FAILURE);

  if (!IsConstructable(name_struct)) {
    // ignore return value, we return JS_FALSE anyway
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  return BaseStubConstructor(mWeakOwner, name_struct, cx, obj, argc, argv, vp);
}

nsresult
nsDOMConstructor::HasInstance(nsIXPConnectWrappedNative *wrapper,
                              JSContext * cx, JSObject * obj,
                              const jsval &v, bool *bp, bool *_retval)

{
  // No need to look these up in the hash.
  *bp = false;
  if (JSVAL_IS_PRIMITIVE(v)) {
    return NS_OK;
  }

  JSObject *dom_obj = JSVAL_TO_OBJECT(v);
  NS_ASSERTION(dom_obj, "nsDOMConstructor::HasInstance couldn't get object");

  // This might not be the right object, if XPCNativeWrapping
  // happened.  Get the wrapped native for this object, then get its
  // JS object.
  JSObject *wrapped_obj;
  nsresult rv = nsContentUtils::XPConnect()->GetJSObjectOfWrapper(cx, dom_obj,
                                                                  &wrapped_obj);
  if (NS_SUCCEEDED(rv)) {
    dom_obj = wrapped_obj;
  }

  JSClass *dom_class = JS_GetClass(dom_obj);
  if (!dom_class) {
    NS_ERROR("nsDOMConstructor::HasInstance can't get class.");
    return NS_ERROR_UNEXPECTED;
  }

  const nsGlobalNameStruct *name_struct;
  rv = GetNameStruct(NS_ConvertASCIItoUTF16(dom_class->name), &name_struct);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!name_struct) {
    // This isn't a normal DOM object, see if this constructor lives on its
    // prototype chain.
    jsval val;
    if (!JS_GetProperty(cx, obj, "prototype", &val)) {
      return NS_ERROR_UNEXPECTED;
    }

    if (JSVAL_IS_PRIMITIVE(val)) {
      return NS_OK;
    }

    JSObject *dot_prototype = JSVAL_TO_OBJECT(val);

    JSObject *proto = JS_GetPrototype(dom_obj);
    for ( ; proto; proto = JS_GetPrototype(proto)) {
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

  const nsDOMClassInfoData *ci_data = nsnull;
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
  PRUint32 count = 0;
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
nsDOMConstructor::ResolveInterfaceConstants(JSContext *cx, JSObject *obj)
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

  // Special case for |Event|, Event needs constants from NSEvent
  // too for backwards compatibility.
  if (class_iid->Equals(NS_GET_IID(nsIDOMEvent))) {
    rv = DefineInterfaceConstants(cx, obj,
                                  &NS_GET_IID(nsIDOMNSEvent));
    NS_ENSURE_SUCCESS(rv, rv);
  }

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
    PRInt32 id = aNameStruct->mDOMClassInfoID;
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

  JSObject *proto_obj;
  (*aProto)->GetJSObject(&proto_obj);
  if (!JS_WrapObject(cx, &proto_obj)) {
    return NS_ERROR_FAILURE;
  }

  NS_IF_RELEASE(*aProto);
  return aXPConnect->HoldObject(cx, proto_obj, aProto);
}

// Either ci_data must be non-null or name_struct must be non-null and of type
// eTypeClassProto.
static nsresult
ResolvePrototype(nsIXPConnect *aXPConnect, nsGlobalWindow *aWin, JSContext *cx,
                 JSObject *obj, const PRUnichar *name,
                 const nsDOMClassInfoData *ci_data,
                 const nsGlobalNameStruct *name_struct,
                 nsScriptNameSpaceManager *nameSpaceManager,
                 JSObject *dot_prototype, bool install, bool *did_resolve)
{
  NS_ASSERTION(ci_data ||
               (name_struct &&
                name_struct->mType == nsGlobalNameStruct::eTypeClassProto),
               "Wrong type or missing ci_data!");

  nsRefPtr<nsDOMConstructor> constructor;
  nsresult rv = nsDOMConstructor::Create(name, ci_data, name_struct, aWin,
                                         getter_AddRefs(constructor));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  jsval v;

  rv = WrapNative(cx, obj, constructor, &NS_GET_IID(nsIDOMDOMConstructor),
                  false, &v, getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(rv, rv);

  if (install) {
    rv = constructor->Install(cx, obj, v);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  JSObject *class_obj;
  holder->GetJSObject(&class_obj);
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
  const char *class_parent_name = nsnull;

  if (!primary_iid->Equals(NS_GET_IID(nsISupports))) {
    JSAutoEnterCompartment ac;

    if (!ac.enter(cx, class_obj)) {
      return NS_ERROR_FAILURE;
    }

    rv = DefineInterfaceConstants(cx, class_obj, primary_iid);
    NS_ENSURE_SUCCESS(rv, rv);

    // Special case for |Event|, Event needs constants from NSEvent
    // too for backwards compatibility.
    if (primary_iid->Equals(NS_GET_IID(nsIDOMEvent))) {
      rv = DefineInterfaceConstants(cx, class_obj,
                                    &NS_GET_IID(nsIDOMNSEvent));
      NS_ENSURE_SUCCESS(rv, rv);
    }

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

    const nsIID *iid = nsnull;

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
    JSObject *winobj = aWin->FastGetGlobalJSObject();

    JSObject *proto = nsnull;

    if (class_parent_name) {
      jsval val;

      JSAutoEnterCompartment ac;
      if (!ac.enter(cx, winobj)) {
        return NS_ERROR_UNEXPECTED;
      }

      if (!::JS_LookupProperty(cx, winobj, CutPrefix(class_parent_name), &val)) {
        return NS_ERROR_UNEXPECTED;
      }

      JSObject *tmp = JSVAL_IS_OBJECT(val) ? JSVAL_TO_OBJECT(val) : nsnull;

      if (tmp) {
        if (!::JS_LookupProperty(cx, tmp, "prototype", &val)) {
          return NS_ERROR_UNEXPECTED;
        }

        if (JSVAL_IS_OBJECT(val)) {
          proto = JSVAL_TO_OBJECT(val);
        }
      }
    }

    if (dot_prototype) {
      JSAutoEnterCompartment ac;
      if (!ac.enter(cx, dot_prototype)) {
        return NS_ERROR_UNEXPECTED;
      }

      JSObject *xpc_proto_proto = ::JS_GetPrototype(dot_prototype);

      if (proto &&
          (!xpc_proto_proto ||
           JS_GetClass(xpc_proto_proto) == sObjectClass)) {
        if (!JS_WrapObject(cx, &proto) ||
            !JS_SetPrototype(cx, dot_prototype, proto)) {
          return NS_ERROR_UNEXPECTED;
        }
      }
    } else {
      JSAutoEnterCompartment ac;
      if (!ac.enter(cx, winobj)) {
        return NS_ERROR_UNEXPECTED;
      }

      dot_prototype = ::JS_NewObject(cx, &sDOMConstructorProtoClass, proto,
                                     winobj);
      NS_ENSURE_TRUE(dot_prototype, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  v = OBJECT_TO_JSVAL(dot_prototype);

  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, class_obj)) {
    return NS_ERROR_UNEXPECTED;
  }

  // Per ECMA, the prototype property is {DontEnum, DontDelete, ReadOnly}
  if (!JS_WrapValue(cx, &v) ||
      !JS_DefineProperty(cx, class_obj, "prototype", v, nsnull, nsnull,
                         JSPROP_PERMANENT | JSPROP_READONLY)) {
    return NS_ERROR_UNEXPECTED;
  }

  *did_resolve = true;

  return NS_OK;
}


// static
nsresult
nsWindowSH::GlobalResolve(nsGlobalWindow *aWin, JSContext *cx,
                          JSObject *obj, jsid id, bool *did_resolve)
{
  *did_resolve = false;

  nsScriptNameSpaceManager *nameSpaceManager =
    nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nsDependentJSString name(id);

  const PRUnichar *class_name = nsnull;
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

  if (name_struct->mType == nsGlobalNameStruct::eTypeInterface) {
    // We're resolving a name of a DOM interface for which there is no
    // direct DOM class, create a constructor object...

    // Lookup new DOM bindings.
    mozilla::dom::binding::DefineInterface define =
      name_struct->mDefineDOMInterface;
    if (define && mozilla::dom::binding::DefineConstructor(cx, obj, define, &rv)) {
      *did_resolve = NS_SUCCEEDED(rv);

      return rv;
    }

    nsRefPtr<nsDOMConstructor> constructor;
    rv = nsDOMConstructor::Create(class_name,
                                  nsnull,
                                  name_struct,
                                  static_cast<nsPIDOMWindow*>(aWin),
                                  getter_AddRefs(constructor));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    jsval v;
    rv = WrapNative(cx, obj, constructor, &NS_GET_IID(nsIDOMDOMConstructor),
                    false, &v, getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = constructor->Install(cx, obj, v);
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject *class_obj;
    holder->GetJSObject(&class_obj);
    NS_ASSERTION(class_obj, "The return value lied");

    // ... and define the constants from the DOM interface on that
    // constructor object.

    rv = DefineInterfaceConstants(cx, class_obj, &name_struct->mIID);
    NS_ENSURE_SUCCESS(rv, rv);

    *did_resolve = true;

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor ||
      name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    // Don't expose chrome only constructors to content windows.
    if (name_struct->mChromeOnly &&
        !nsContentUtils::IsSystemPrincipal(aWin->GetPrincipal())) {
      return NS_OK;
    }

    // For now don't expose web sockets unless user has explicitly enabled them
    if (name_struct->mDOMClassInfoID == eDOMClassInfo_WebSocket_id) {
      if (!nsWebSocket::PrefEnabled()) {
        return NS_OK;
      }
    }

    // For now don't expose server events unless user has explicitly enabled them
    if (name_struct->mDOMClassInfoID == eDOMClassInfo_EventSource_id) {
      if (!nsEventSource::PrefEnabled()) {
        return NS_OK;
      }
    }

    // Lookup new DOM bindings.
    if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
      mozilla::dom::binding::DefineInterface define =
        name_struct->mDefineDOMInterface;
      if (define && mozilla::dom::binding::DefineConstructor(cx, obj, define, &rv)) {
        *did_resolve = NS_SUCCEEDED(rv);

        return rv;
      }
    }

    // Create the XPConnect prototype for our classinfo, PostCreateProto will
    // set up the prototype chain.
    nsCOMPtr<nsIXPConnectJSObjectHolder> proto_holder;
    rv = GetXPCProto(sXPConnect, cx, aWin, name_struct,
                     getter_AddRefs(proto_holder));

    if (NS_SUCCEEDED(rv) && obj != aWin->GetGlobalJSObject()) {
      JSObject* dot_prototype;
      rv = proto_holder->GetJSObject(&dot_prototype);
      NS_ENSURE_SUCCESS(rv, rv);

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

    // Lookup new DOM bindings.
    mozilla::dom::binding::DefineInterface define =
      name_struct->mDefineDOMInterface;
    if (define && mozilla::dom::binding::DefineConstructor(cx, obj, define, &rv)) {
      *did_resolve = NS_SUCCEEDED(rv);

      return rv;
    }

    return ResolvePrototype(sXPConnect, aWin, cx, obj, class_name, nsnull,
                            name_struct, nameSpaceManager, nsnull, true,
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

    JSObject* dot_prototype;
    rv = proto_holder->GetJSObject(&dot_prototype);
    NS_ENSURE_SUCCESS(rv, rv);

    const nsDOMClassInfoData *ci_data;
    if (alias_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
      ci_data = &sClassInfoData[alias_struct->mDOMClassInfoID];
    } else if (alias_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
      ci_data = alias_struct->mData;
    } else {
      return NS_ERROR_UNEXPECTED;
    }

    return ResolvePrototype(sXPConnect, aWin, cx, obj, class_name, ci_data,
                            name_struct, nameSpaceManager, nsnull, true,
                            did_resolve);
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    nsRefPtr<nsDOMConstructor> constructor;
    rv = nsDOMConstructor::Create(class_name, nsnull, name_struct,
                                  static_cast<nsPIDOMWindow*>(aWin),
                                  getter_AddRefs(constructor));
    NS_ENSURE_SUCCESS(rv, rv);

    jsval val;
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, obj, constructor, &NS_GET_IID(nsIDOMDOMConstructor),
                    false, &val, getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = constructor->Install(cx, obj, val);
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject* class_obj;
    holder->GetJSObject(&class_obj);
    NS_ASSERTION(class_obj, "Why didn't we get a JSObject?");

    *did_resolve = true;

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeProperty) {
    if (name_struct->mChromeOnly && !nsContentUtils::IsCallerChrome())
      return NS_OK;

    nsCOMPtr<nsISupports> native(do_CreateInstance(name_struct->mCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    jsval prop_val = JSVAL_VOID; // Property value.

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(native));
    if (owner) {
      nsIScriptContext *context = nsJSUtils::GetStaticScriptContext(cx, obj);
      NS_ENSURE_TRUE(context, NS_ERROR_UNEXPECTED);

      JSObject *prop_obj = nsnull;
      rv = owner->GetScriptObject(context, (void**)&prop_obj);
      NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && prop_obj, NS_ERROR_UNEXPECTED);

      prop_val = OBJECT_TO_JSVAL(prop_obj);
    } else {
      nsCOMPtr<nsIDOMGlobalPropertyInitializer> gpi(do_QueryInterface(native));

      if (gpi) {
        rv = gpi->Init(aWin, &prop_val);
        NS_ENSURE_SUCCESS(rv, rv);
      }
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

      rv = WrapNative(cx, scope, native, true, &prop_val,
                      getter_AddRefs(holder));
    }

    NS_ENSURE_SUCCESS(rv, rv);

    if (!JS_WrapValue(cx, &prop_val)) {
      return NS_ERROR_UNEXPECTED;
    }

    JSBool ok = ::JS_DefinePropertyById(cx, obj, id, prop_val, nsnull, nsnull,
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

static JSNewResolveOp sOtherResolveFuncs[] = {
  mozilla::dom::workers::ResolveWorkerClasses
};

template<class Interface>
static nsresult
LocationSetterGuts(JSContext *cx, JSObject *obj, jsval *vp)
{
  // This function duplicates some of the logic in XPC_WN_HelperSetProperty
  XPCWrappedNative *wrapper =
    XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);

  // The error checks duplicate code in THROW_AND_RETURN_IF_BAD_WRAPPER
  NS_ENSURE_TRUE(!wrapper || wrapper->IsValid(), NS_ERROR_XPC_HAS_BEEN_SHUTDOWN);

  nsCOMPtr<Interface> xpcomObj = do_QueryWrappedNative(wrapper, obj);
  NS_ENSURE_TRUE(xpcomObj, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMLocation> location;
  nsresult rv = xpcomObj->GetLocation(getter_AddRefs(location));
  NS_ENSURE_SUCCESS(rv, rv);

  // Grab the value we're being set to before we stomp on |vp|
  JSString *val = ::JS_ValueToString(cx, *vp);
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
LocationSetter(JSContext *cx, JSObject *obj, jsid id, JSBool strict,
               jsval *vp)
{
  nsresult rv = LocationSetterGuts<Interface>(cx, obj, vp);
  if (NS_FAILED(rv)) {
    if (!::JS_IsExceptionPending(cx)) {
      nsDOMClassInfo::ThrowJSException(cx, rv);
    }
    return JS_FALSE;
  }

  return JS_TRUE;
}

static JSBool
LocationSetterUnwrapper(JSContext *cx, JSObject *obj, jsid id, JSBool strict,
                        jsval *vp)
{
  JSObject *wrapped = XPCWrapper::UnsafeUnwrapSecurityWrapper(obj);
  if (wrapped) {
    obj = wrapped;
  }

  return LocationSetter<nsIDOMWindow>(cx, obj, id, strict, vp);
}

NS_IMETHODIMP
nsWindowSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, jsid id, PRUint32 flags,
                       JSObject **objp, bool *_retval)
{
  nsGlobalWindow *win = nsGlobalWindow::FromWrapper(wrapper);

  if (!JSID_IS_STRING(id)) {
    if (JSID_IS_INT(id) && JSID_TO_INT(id) >= 0 && !(flags & JSRESOLVE_ASSIGNING)) {
      // If we're resolving a numeric property, treat that as if
      // window.frames[n] is resolved (since window.frames ===
      // window), if window.frames[n] is a child frame, define a
      // property for this index.
      PRUint32 index = PRUint32(JSID_TO_INT(id));
      if (nsCOMPtr<nsIDOMWindow> frame = GetChildFrame(win, index)) {
        // A numeric property accessed and the numeric property is a
        // child frame. Define a property for this index.

        *_retval = ::JS_DefineElement(cx, obj, index, JSVAL_VOID,
                                      nsnull, nsnull, JSPROP_SHARED);

        if (*_retval) {
          *objp = obj;
        }
      }
    }

    return NS_OK;
  }

  nsIScriptContext *my_context = win->GetContextInternal();

  nsresult rv = NS_OK;

  // Resolve standard classes on my_context's JSContext (or on cx,
  // if we don't have a my_context yet), in case the two contexts
  // have different origins.  We want lazy standard class
  // initialization to behave as if it were done eagerly, on each
  // window's own context (not on some other window-caller's
  // context).

  JSBool did_resolve = JS_FALSE;
  JSContext *my_cx;

  JSBool ok = JS_TRUE;
  jsval exn = JSVAL_VOID;
  if (!ObjectIsNativeWrapper(cx, obj)) {
    JSAutoEnterCompartment ac;

    if (!my_context) {
      my_cx = cx;
    } else {
      my_cx = my_context->GetNativeContext();

      if (my_cx != cx) {
        if (!ac.enter(my_cx, obj)) {
          return NS_ERROR_UNEXPECTED;
        }
      }
    }

    JSAutoRequest transfer(my_cx);

    // Don't resolve standard classes on XPCNativeWrapper etc, only
    // resolve them if we're resolving on the real global object.
    ok = JS_ResolveStandardClass(my_cx, obj, id, &did_resolve);

    if (!ok) {
      // Trust the JS engine (or the script security manager) to set
      // the exception in the JS engine.

      if (!JS_GetPendingException(my_cx, &exn)) {
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

  if (!(flags & JSRESOLVE_ASSIGNING)) {
    // We want this code to be before the child frame lookup code
    // below so that a child frame named 'constructor' doesn't
    // shadow the window's constructor property.
    if (id == sConstructor_id) {
      return ResolveConstructor(cx, obj, objp);
    }
  }

  if (!my_context || !my_context->IsContextInitialized()) {
    // The context is not yet initialized so there's nothing we can do
    // here yet.

    return NS_OK;
  }

  if (id == sLocation_id) {
    // This must be done even if we're just getting the value of
    // window.location (i.e. no checking flags & JSRESOLVE_ASSIGNING
    // here) since we must define window.location to prevent the
    // getter from being overriden (for security reasons).

    // Note: Because we explicitly don't forward to the inner window
    // above, we have to ensure here that our window has a current
    // inner window so that the location object we return will work.

    if (win->IsOuterWindow()) {
      win->EnsureInnerWindow();
    }

    nsCOMPtr<nsIDOMLocation> location;
    rv = win->GetLocation(getter_AddRefs(location));
    NS_ENSURE_SUCCESS(rv, rv);

    // Make sure we wrap the location object in the inner window's
    // scope if we've got an inner window.
    JSObject *scope = nsnull;
    if (win->IsOuterWindow()) {
      nsGlobalWindow *innerWin = win->GetCurrentInnerWindowInternal();

      if (innerWin) {
        scope = innerWin->GetGlobalJSObject();
      }
    }

    if (!scope) {
      wrapper->GetJSObject(&scope);
    }

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    jsval v;
    rv = WrapNative(cx, scope, location, &NS_GET_IID(nsIDOMLocation), true,
                    &v, getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    JSBool ok = JS_WrapValue(cx, &v) &&
                JS_DefinePropertyById(cx, obj, id, v, nsnull,
                                      LocationSetterUnwrapper,
                                      JSPROP_PERMANENT | JSPROP_ENUMERATE);

    if (!ok) {
      return NS_ERROR_FAILURE;
    }

    *objp = obj;

    return NS_OK;
  }

  // Hmm, we do an awful lot of QIs here; maybe we should add a
  // method on an interface that would let us just call into the
  // window code directly...

  if (!ObjectIsNativeWrapper(cx, obj)) {
    nsCOMPtr<nsIDocShellTreeNode> dsn(do_QueryInterface(win->GetDocShell()));

    PRInt32 count = 0;

    if (dsn) {
      dsn->GetChildCount(&count);
    }

    if (count > 0) {
      nsCOMPtr<nsIDocShellTreeItem> child;

      const jschar *chars = ::JS_GetInternedStringChars(JSID_TO_STRING(id));

      dsn->FindChildWithName(reinterpret_cast<const PRUnichar*>(chars),
                             false, true, nsnull, nsnull,
                             getter_AddRefs(child));

      nsCOMPtr<nsIDOMWindow> child_win(do_GetInterface(child));

      if (child_win) {
        // We found a subframe of the right name, define the property
        // on the wrapper so that ::NewResolve() doesn't get called
        // again for this property name.

        JSObject *wrapperObj;
        wrapper->GetJSObject(&wrapperObj);

        jsval v;
        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        rv = WrapNative(cx, wrapperObj, child_win,
                        &NS_GET_IID(nsIDOMWindow), true, &v,
                        getter_AddRefs(holder));
        NS_ENSURE_SUCCESS(rv, rv);

        JSAutoRequest ar(cx);

        bool ok = JS_WrapValue(cx, &v) &&
                    JS_DefinePropertyById(cx, obj, id, v, nsnull, nsnull, 0);
        if (!ok) {
          return NS_ERROR_FAILURE;
        }

        *objp = obj;

        return NS_OK;
      }
    }
  }

  // It is not worth calling GlobalResolve() if we are resolving
  // for assignment, since only read-write properties get dealt
  // with there.
  if (!(flags & JSRESOLVE_ASSIGNING)) {
    JSAutoRequest ar(cx);

    // Resolve special classes.
    for (PRUint32 i = 0; i < ArrayLength(sOtherResolveFuncs); i++) {
      if (!sOtherResolveFuncs[i](cx, obj, id, flags, objp)) {
        return NS_ERROR_FAILURE;
      }
      if (*objp) {
        return NS_OK;
      }
    }

    // Call GlobalResolve() after we call FindChildWithName() so
    // that named child frames will override external properties
    // which have been registered with the script namespace manager.

    bool did_resolve = false;
    rv = GlobalResolve(win, cx, obj, id, &did_resolve);
    NS_ENSURE_SUCCESS(rv, rv);

    if (did_resolve) {
      // GlobalResolve() resolved something, so we're done here.
      *objp = obj;

      return NS_OK;
    }
  }

  if (id == s_content_id) {
    // Map window._content to window.content for backwards
    // compatibility, this should spit out an message on the JS
    // console.

    JSObject *windowObj = win->GetGlobalJSObject();

    JSAutoRequest ar(cx);

    JSFunction *fun = ::JS_NewFunction(cx, ContentWindowGetter, 0, 0,
                                       windowObj, "_content");
    if (!fun) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    JSObject *funObj = ::JS_GetFunctionObject(fun);

    if (!::JS_DefinePropertyById(cx, windowObj, id, JSVAL_VOID,
                                 JS_DATA_TO_FUNC_PTR(JSPropertyOp, funObj),
                                 nsnull,
                                 JSPROP_ENUMERATE | JSPROP_GETTER |
                                 JSPROP_SHARED)) {
      return NS_ERROR_FAILURE;
    }

    *objp = obj;

    return NS_OK;
  }

  if (flags & JSRESOLVE_ASSIGNING) {
    if (IsReadonlyReplaceable(id) ||
        (!(flags & JSRESOLVE_QUALIFIED) && IsWritableReplaceable(id))) {
      // A readonly "replaceable" property is being set, or a
      // readwrite "replaceable" property is being set w/o being
      // fully qualified. Define the property on obj with the value
      // undefined to override the predefined property. This is done
      // for compatibility with other browsers.
      JSAutoRequest ar(cx);

      if (!::JS_DefinePropertyById(cx, obj, id, JSVAL_VOID, JS_PropertyStub,
                                   JS_StrictPropertyStub, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
      *objp = obj;

      return NS_OK;
    }
  } else {
    if (id == sNavigator_id) {
      nsCOMPtr<nsIDOMNavigator> navigator;
      rv = win->GetNavigator(getter_AddRefs(navigator));
      NS_ENSURE_SUCCESS(rv, rv);

      jsval v;
      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      rv = WrapNative(cx, obj, navigator, &NS_GET_IID(nsIDOMNavigator), true,
                      &v, getter_AddRefs(holder));
      NS_ENSURE_SUCCESS(rv, rv);

      // Hold on to the navigator object as a global property so we
      // don't need to worry about losing expando properties etc.
      if (!::JS_DefinePropertyById(cx, obj, id, v, nsnull, nsnull,
                                   JSPROP_READONLY | JSPROP_PERMANENT |
                                   JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
      *objp = obj;

      return NS_OK;
    }

    if (id == sDocument_id) {
      nsCOMPtr<nsIDOMDocument> document;
      rv = win->GetDocument(getter_AddRefs(document));
      NS_ENSURE_SUCCESS(rv, rv);

      // FIXME Ideally we'd have an nsIDocument here and get nsWrapperCache
      //       from it.
      jsval v;
      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      rv = WrapNative(cx, JS_GetGlobalForScopeChain(cx), document,
                      &NS_GET_IID(nsIDOMDocument), false, &v,
                      getter_AddRefs(holder));
      NS_ENSURE_SUCCESS(rv, rv);

      // The PostCreate hook for the document will handle defining the
      // property
      *objp = obj;

      // NB: We need to do this for any Xray wrapper.
      if (xpc::WrapperFactory::IsXrayWrapper(obj)) {
        // Unless our object is a native wrapper, in which case we have to
        // define it ourselves.

        *_retval = JS_WrapValue(cx, &v) &&
                   JS_DefineProperty(cx, obj, "document", v, NULL, NULL,
                                     JSPROP_READONLY | JSPROP_ENUMERATE);
        if (!*_retval) {
          return NS_ERROR_UNEXPECTED;
        }
      }

      return NS_OK;
    }

    if (id == sDialogArguments_id && win->IsModalContentWindow()) {
      nsCOMPtr<nsIArray> args;
      ((nsGlobalModalWindow *)win)->GetDialogArguments(getter_AddRefs(args));

      nsIScriptContext *script_cx = win->GetContext();
      if (script_cx) {
        JSAutoSuspendRequest asr(cx);

        // Make nsJSContext::SetProperty()'s magic argument array
        // handling happen.
        rv = script_cx->SetProperty(obj, "dialogArguments", args);
        NS_ENSURE_SUCCESS(rv, rv);

        *objp = obj;
      }

      return NS_OK;
    }
  }

  JSObject *oldobj = *objp;
  rv = nsDOMGenericSH::NewResolve(wrapper, cx, obj, id, flags, objp,
                                  _retval);

  if (NS_FAILED(rv) || *objp != oldobj) {
    // Something went wrong, or the property got resolved. Return.
    return rv;
  }

  // Make a fast expando if we're assigning to (not declaring or
  // binding a name) a new undefined property that's not already
  // defined on our prototype chain. This way we can access this
  // expando w/o ever getting back into XPConnect.
  if ((flags & JSRESOLVE_ASSIGNING) && !(flags & JSRESOLVE_WITH)) {
    JSObject *realObj;
    wrapper->GetJSObject(&realObj);

    if (obj == realObj) {
      JSObject *proto = js::GetObjectProto(obj);
      if (proto) {
        JSObject *pobj = NULL;
        jsval val;

        if (!::JS_LookupPropertyWithFlagsById(cx, proto, id, flags,
                                              &pobj, &val)) {
          *_retval = JS_FALSE;

          return NS_OK;
        }

        if (pobj) {
          // A property was found on the prototype chain.
          *objp = pobj;
          return NS_OK;
        }
      }

      // Define a fast expando, the key here is to use JS_PropertyStub
      // as the getter/setter, which makes us stay out of XPConnect
      // when using this property.
      //
      // We don't need to worry about property attributes here as we
      // know here we're dealing with an undefined property set, so
      // we're not declaring readonly or permanent properties.
      //
      // Since we always create the undeclared property here without given a
      // chance for the interpreter to report applicable strict mode warnings,
      // we must take care to check those warnings here.
      JSString *str = JSID_TO_STRING(id);
      if ((!(flags & JSRESOLVE_QUALIFIED) &&
           !js::CheckUndeclaredVarAssignment(cx, str)) ||
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
    *_retval = nsnull;
    return NS_ERROR_UNEXPECTED;
  }

  JSObject *winObj = win->FastGetGlobalJSObject();
  if (!winObj) {
    NS_ASSERTION(origWin->IsOuterWindow(), "What window is this?");
    *_retval = obj;
    return NS_OK;
  }

  if (!JS_WrapObject(cx, &winObj)) {
    *_retval = nsnull;
    return NS_ERROR_UNEXPECTED;
  }

  *_retval = winObj;
  return NS_OK;
}

// DOM Location helper

NS_IMETHODIMP
nsLocationSH::CheckAccess(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, jsid id, PRUint32 mode,
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

// DOM Navigator helper

NS_IMETHODIMP
nsNavigatorSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, jsid id, PRUint32 flags,
                          JSObject **objp, bool *_retval)
{
  if (!JSID_IS_STRING(id) || (flags & JSRESOLVE_ASSIGNING)) {
    return NS_OK;
  }

  nsScriptNameSpaceManager *nameSpaceManager =
    nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nsDependentJSString name(id);

  const nsGlobalNameStruct *name_struct = nsnull;

  nameSpaceManager->LookupNavigatorName(name, &name_struct);

  if (!name_struct) {
    return NS_OK;
  }
  NS_ASSERTION(name_struct->mType == nsGlobalNameStruct::eTypeNavigatorProperty,
               "unexpected type");

  nsresult rv = NS_OK;

  nsCOMPtr<nsISupports> native(do_CreateInstance(name_struct->mCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  jsval prop_val = JSVAL_VOID; // Property value.

  nsCOMPtr<nsIDOMGlobalPropertyInitializer> gpi(do_QueryInterface(native));

  if (gpi) {
    JSObject *global = JS_GetGlobalForObject(cx, obj);

    nsISupports *globalNative = XPConnect()->GetNativeOfWrapper(cx, global);
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(globalNative);

    if (!window) {
      return NS_ERROR_UNEXPECTED;
    }

    rv = gpi->Init(window, &prop_val);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (JSVAL_IS_PRIMITIVE(prop_val) && !JSVAL_IS_NULL(prop_val)) {
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, obj, native, true, &prop_val,
                    getter_AddRefs(holder));

    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!JS_WrapValue(cx, &prop_val)) {
    return NS_ERROR_UNEXPECTED;
  }

  JSBool ok = ::JS_DefinePropertyById(cx, obj, id, prop_val, nsnull, nsnull,
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

template<nsresult (*func)(JSContext *cx, JSObject *obj, jsval *vp)>
static JSBool
GetterShim(JSContext *cx, JSObject *obj, jsid /* unused */, jsval *vp)
{
  nsresult rv = (*func)(cx, obj, vp);
  if (NS_FAILED(rv)) {
    if (!::JS_IsExceptionPending(cx)) {
      nsDOMClassInfo::ThrowJSException(cx, rv);
    }
    return JS_FALSE;
  }

  return JS_TRUE;  
}

// Can't be static so GetterShim will compile
nsresult
BaseURIObjectGetter(JSContext *cx, JSObject *obj, jsval *vp)
{
  // This function duplicates some of the logic in XPC_WN_HelperGetProperty
  XPCWrappedNative *wrapper =
    XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);

  // The error checks duplicate code in THROW_AND_RETURN_IF_BAD_WRAPPER
  NS_ENSURE_TRUE(!wrapper || wrapper->IsValid(), NS_ERROR_XPC_HAS_BEEN_SHUTDOWN);

  nsCOMPtr<nsINode> node = do_QueryWrappedNative(wrapper, obj);
  NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIURI> uri = node->GetBaseURI();
  return WrapNative(cx, JS_GetGlobalForScopeChain(cx), uri,
                    &NS_GET_IID(nsIURI), true, vp);
}

// Can't be static so GetterShim will compile
nsresult
NodePrincipalGetter(JSContext *cx, JSObject *obj, jsval *vp)
{
  // This function duplicates some of the logic in XPC_WN_HelperGetProperty
  XPCWrappedNative *wrapper =
    XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);

  // The error checks duplicate code in THROW_AND_RETURN_IF_BAD_WRAPPER
  NS_ENSURE_TRUE(!wrapper || wrapper->IsValid(), NS_ERROR_XPC_HAS_BEEN_SHUTDOWN);

  nsCOMPtr<nsINode> node = do_QueryWrappedNative(wrapper, obj);
  NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);

  return WrapNative(cx, JS_GetGlobalForScopeChain(cx), node->NodePrincipal(),
                    &NS_GET_IID(nsIPrincipal), true, vp);
}

NS_IMETHODIMP
nsNodeSH::PostCreatePrototype(JSContext * cx, JSObject * proto)
{
  // set up our proto first
  nsresult rv = nsDOMGenericSH::PostCreatePrototype(cx, proto);

  if (xpc::AccessCheck::isChrome(js::GetObjectCompartment(proto))) {
    // Stick nodePrincipal and baseURIObject  properties on there
    JS_DefinePropertyById(cx, proto, sNodePrincipal_id,
                          JSVAL_VOID, GetterShim<NodePrincipalGetter>,
                          nsnull,
                          JSPROP_READONLY | JSPROP_SHARED);
    JS_DefinePropertyById(cx, proto, sBaseURIObject_id,
                          JSVAL_VOID, GetterShim<BaseURIObjectGetter>,
                          nsnull,
                          JSPROP_READONLY | JSPROP_SHARED);
  }

  return rv;
}

bool
nsNodeSH::IsCapabilityEnabled(const char* aCapability)
{
  bool enabled;
  return sSecMan &&
    NS_SUCCEEDED(sSecMan->IsCapabilityEnabled(aCapability, &enabled)) &&
    enabled;
}

NS_IMETHODIMP
nsNodeSH::PreCreate(nsISupports *nativeObj, JSContext *cx, JSObject *globalObj,
                    JSObject **parentObj)
{
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

  // If we have a document, make sure one of these is true
  // (1) it has a script handling object,
  // (2) has had one, or has been marked to have had one,
  // (3) we are running a privileged script.
  // Event handling is possible only if (1). If (2) event handling is prevented.
  // If document has never had a script handling object,
  // untrusted scripts (3) shouldn't touch it!
  bool hasHadScriptHandlingObject = false;
  NS_ENSURE_STATE(doc->GetScriptHandlingObject(hasHadScriptHandlingObject) ||
                  hasHadScriptHandlingObject ||
                  IsPrivilegedScript());

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
        nsHTMLLegendElement *legend =
          nsHTMLLegendElement::FromContent(node->AsElement());
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
    // document's global object, if there is one

    // Get the scope object from the document.
    nsISupports *scope = doc->GetScopeObject();

    if (scope) {
        jsval v;
        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        nsresult rv = WrapNative(cx, globalObj, scope, false, &v,
                                 getter_AddRefs(holder));
        NS_ENSURE_SUCCESS(rv, rv);

        holder->GetJSObject(parentObj);
    }
    else {
      // No global object reachable from this document, use the
      // global object that was passed to this method.

      *parentObj = globalObj;
    }

    // No slim wrappers for a document's scope object.
    return node->IsInNativeAnonymousSubtree() ?
      NS_SUCCESS_CHROME_ACCESS_ONLY : NS_OK;
  }

  // XXXjst: Maybe we need to find the global to use from the
  // nsIScriptGlobalObject that's reachable from the node we're about
  // to wrap here? But that's not always reachable, let's use
  // globalObj for now...

  nsresult rv = WrapNativeParent(cx, globalObj, native_parent, native_parent,
                                 parentObj);
  NS_ENSURE_SUCCESS(rv, rv);

  return node->IsInNativeAnonymousSubtree() ?
    NS_SUCCESS_CHROME_ACCESS_ONLY : NS_SUCCESS_ALLOW_SLIM_WRAPPERS;
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
                     JSObject *obj, jsid id, PRUint32 flags,
                     JSObject **objp, bool *_retval)
{
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
nsNodeSH::GetFlags(PRUint32 *aFlags)
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
                           JSObject *globalObj, JSObject **parentObj)
{
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

// IDBEventTarget helper

NS_IMETHODIMP
IDBEventTargetSH::PreCreate(nsISupports *aNativeObj, JSContext *aCx,
                            JSObject *aGlobalObj, JSObject **aParentObj)
{
  IDBWrapperCache *target = IDBWrapperCache::FromSupports(aNativeObj);
  JSObject *parent = target->GetParentObject();
  *aParentObj = parent ? parent : aGlobalObj;
  return NS_OK;
}

// Element helper

static bool
GetBindingURL(Element *aElement, nsIDocument *aDocument,
              nsCSSValue::URL **aResult)
{
  // If we have a frame the frame has already loaded the binding.  And
  // otherwise, don't do anything else here unless we're dealing with
  // XUL.
  nsIPresShell *shell = aDocument->GetShell();
  if (!shell || aElement->GetPrimaryFrame() || !aElement->IsXUL()) {
    *aResult = nsnull;

    return true;
  }

  // Get the computed -moz-binding directly from the style context
  nsPresContext *pctx = shell->GetPresContext();
  NS_ENSURE_TRUE(pctx, false);

  nsRefPtr<nsStyleContext> sc = pctx->StyleSet()->ResolveStyleFor(aElement,
                                                                  nsnull);
  NS_ENSURE_TRUE(sc, false);

  *aResult = sc->GetStyleDisplay()->mBinding;

  return true;
}

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
    // Don't allow slim wrappers.
    return rv == NS_SUCCESS_ALLOW_SLIM_WRAPPERS ? NS_OK : rv;
  }

  nsCSSValue::URL *bindingURL;
  bool ok = GetBindingURL(element, doc, &bindingURL);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  // Only allow slim wrappers if there's no binding.
  if (!bindingURL) {
    return rv;
  }

  element->SetFlags(NODE_ATTACH_BINDING_ON_POSTCREATE);

  return rv == NS_SUCCESS_ALLOW_SLIM_WRAPPERS ? NS_OK : rv;
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
  nsCSSValue::URL *bindingURL;
  bool ok = GetBindingURL(element, doc, &bindingURL);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  if (!bindingURL) {
    // No binding, nothing left to do here.
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri = bindingURL->GetURI();
  nsCOMPtr<nsIPrincipal> principal = bindingURL->mOriginPrincipal;

  // We have a binding that must be installed.
  bool dummy;

  nsCOMPtr<nsIXBLService> xblService(do_GetService("@mozilla.org/xbl;1"));
  NS_ENSURE_TRUE(xblService, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<nsXBLBinding> binding;
  xblService->LoadBindings(element, uri, principal, false,
                           getter_AddRefs(binding), &dummy);
  
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
nsElementSH::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, bool *_retval)
{
  // Make sure to not call the superclass here!
  nsCOMPtr<nsIContent> content(do_QueryWrappedNative(wrapper, obj));
  NS_ENSURE_TRUE(content, NS_ERROR_UNEXPECTED);

  nsIDocument* doc = content->OwnerDoc();

  nsRefPtr<nsXBLBinding> binding = doc->BindingManager()->GetBinding(content);
  if (!binding) {
    // Nothing else to do here
    return NS_OK;
  }

  *_retval = binding->ResolveAllFields(cx, obj);
  
  return NS_OK;
}
  

// Generic array scriptable helper.

NS_IMETHODIMP
nsGenericArraySH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, jsid id, PRUint32 flags,
                             JSObject **objp, bool *_retval)
{
  if (id == sLength_id) {
    // Bail early; this isn't something we're interested in
    return NS_OK;
  }
  
  bool is_number = false;
  PRInt32 n = GetArrayIndexFromId(cx, id, &is_number);

  if (is_number && n >= 0) {
    // XXX The following is a cheap optimization to avoid hitting xpconnect to
    // get the length. We may want to consider asking our concrete
    // implementation for the length, and falling back onto the GetProperty if
    // it doesn't provide one.

    PRUint32 length;
    nsresult rv = GetLength(wrapper, cx, obj, &length);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 index = PRUint32(n);
    if (index < length) {
      *_retval = ::JS_DefineElement(cx, obj, index, JSVAL_VOID, nsnull, nsnull,
                                    JSPROP_ENUMERATE | JSPROP_SHARED);
      *objp = obj;
    }
  }

  return NS_OK;
}

nsresult
nsGenericArraySH::GetLength(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, PRUint32 *length)
{
  *length = 0;

  jsval lenval;
  if (!JS_GetProperty(cx, obj, "length", &lenval)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!JSVAL_IS_INT(lenval)) {
    // This can apparently happen with some sparse array impls falling back
    // onto this code.
    return NS_OK;
  }

  PRInt32 slen = JSVAL_TO_INT(lenval);
  if (slen < 0) {
    return NS_OK;
  }

  *length = (PRUint32)slen;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericArraySH::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, bool *_retval)
{
  // Recursion protection in case someone tries to be smart and call
  // the enumerate hook from a user defined .length getter, or
  // somesuch.

  static bool sCurrentlyEnumerating;

  if (sCurrentlyEnumerating) {
    // Don't recurse to death.
    return NS_OK;
  }

  sCurrentlyEnumerating = true;

  jsval len_val;
  JSAutoRequest ar(cx);
  JSBool ok = ::JS_GetProperty(cx, obj, "length", &len_val);

  if (ok && JSVAL_IS_INT(len_val)) {
    PRInt32 length = JSVAL_TO_INT(len_val);

    for (PRInt32 i = 0; ok && i < length; ++i) {
      ok = ::JS_DefineElement(cx, obj, i, JSVAL_VOID, nsnull, nsnull,
                              JSPROP_ENUMERATE | JSPROP_SHARED);
    }
  }

  sCurrentlyEnumerating = false;

  return ok ? NS_OK : NS_ERROR_UNEXPECTED;
}

// Array scriptable helper

NS_IMETHODIMP
nsArraySH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, jsid id, jsval *vp, bool *_retval)
{
  bool is_number = false;
  PRInt32 n = GetArrayIndexFromId(cx, id, &is_number);

  nsresult rv = NS_OK;

  if (is_number) {
    if (n < 0) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    // Make sure rv == NS_OK here, so GetItemAt implementations that never fail
    // don't have to set rv.
    rv = NS_OK;
    nsWrapperCache *cache = nsnull;
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
nsStringListSH::GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                            nsAString& aResult)
{
  nsCOMPtr<nsIDOMDOMStringList> list(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(list, NS_ERROR_UNEXPECTED);

  nsresult rv = list->Item(aIndex, aResult);
#ifdef DEBUG
  if (DOMStringIsNull(aResult)) {
    PRUint32 length = 0;
    list->GetLength(&length);
    NS_ASSERTION(PRUint32(aIndex) >= length, "Item should only return null for out-of-bounds access");
  }
#endif
  return rv;
}


// DOMTokenList scriptable helper

nsresult
nsDOMTokenListSH::GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                              nsAString& aResult)
{
  nsCOMPtr<nsIDOMDOMTokenList> list(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(list, NS_ERROR_UNEXPECTED);

  nsresult rv = list->Item(aIndex, aResult);
#ifdef DEBUG
  if (DOMStringIsNull(aResult)) {
    PRUint32 length = 0;
    list->GetLength(&length);
    NS_ASSERTION(PRUint32(aIndex) >= length, "Item should only return null for out-of-bounds access");
  }
#endif
  return rv;
}


// Named Array helper

NS_IMETHODIMP
nsNamedArraySH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                           JSObject *obj, jsid id, PRUint32 flags,
                           JSObject **objp, bool *_retval)
{
  if ((!(JSRESOLVE_ASSIGNING & flags)) && JSID_IS_STRING(id) &&
      !ObjectIsNativeWrapper(cx, obj)) {

    {
      JSObject *realObj;

      if (wrapper) {
        wrapper->GetJSObject(&realObj);
      } else {
        realObj = obj;
      }

      JSAutoEnterCompartment ac;

      if (!ac.enter(cx, realObj)) {
        *_retval = false;
        return NS_ERROR_FAILURE;
      }

      JSObject *proto = ::JS_GetPrototype(realObj);

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
      *_retval = ::JS_DefinePropertyById(cx, obj, id, JSVAL_VOID, nsnull,
                                         nsnull, JSPROP_ENUMERATE | JSPROP_SHARED);

      *objp = obj;

      return *_retval ? NS_OK : NS_ERROR_FAILURE;
    }
  }

  return nsArraySH::NewResolve(wrapper, cx, obj, id, flags, objp, _retval);
}

NS_IMETHODIMP
nsNamedArraySH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, jsval *vp,
                            bool *_retval)
{
  if (JSID_IS_STRING(id) && !ObjectIsNativeWrapper(cx, obj)) {
    nsresult rv = NS_OK;
    nsWrapperCache *cache = nsnull;
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


// NamedNodeMap helper

nsISupports*
nsNamedNodeMapSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                            nsWrapperCache **aCache, nsresult *aResult)
{
  nsDOMAttributeMap* map = nsDOMAttributeMap::FromSupports(aNative);

  nsINode *attr;
  *aCache = attr = map->GetItemAt(aIndex, aResult);
  return attr;
}

nsISupports*
nsNamedNodeMapSH::GetNamedItem(nsISupports *aNative, const nsAString& aName,
                               nsWrapperCache **aCache, nsresult *aResult)
{
  nsDOMAttributeMap* map = nsDOMAttributeMap::FromSupports(aNative);

  nsINode *attr;
  *aCache = attr = map->GetNamedItem(aName, aResult);
  return attr;
}


NS_IMETHODIMP
nsDOMStringMapSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, jsid id, PRUint32 flags,
                             JSObject **objp, bool *_retval)
{
  nsCOMPtr<nsIDOMDOMStringMap> dataset(do_QueryWrappedNative(wrapper, obj));
  NS_ENSURE_TRUE(dataset, NS_ERROR_UNEXPECTED);

  nsAutoString prop;
  NS_ENSURE_TRUE(JSIDToProp(id, prop), NS_ERROR_UNEXPECTED);

  if (dataset->HasDataAttr(prop)) {
    *_retval = JS_DefinePropertyById(cx, obj, id, JSVAL_VOID,
                                     nsnull, nsnull,
                                     JSPROP_ENUMERATE | JSPROP_SHARED); 
    *objp = obj;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStringMapSH::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, bool *_retval)
{
  nsCOMPtr<nsIDOMDOMStringMap> dataset(do_QueryWrappedNative(wrapper, obj));
  NS_ENSURE_TRUE(dataset, NS_ERROR_UNEXPECTED);

  nsDOMStringMap* stringMap = static_cast<nsDOMStringMap*>(dataset.get());
  nsTArray<nsString> properties;
  nsresult rv = stringMap->GetDataPropList(properties);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < properties.Length(); ++i) {
    nsString& prop(properties[i]);
    *_retval = JS_DefineUCProperty(cx, obj, prop.get(), prop.Length(),
                                   JSVAL_VOID, nsnull, nsnull,
                                   JSPROP_ENUMERATE | JSPROP_SHARED);
    NS_ENSURE_TRUE(*_retval, NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStringMapSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                            JSObject *globalObj, JSObject **parentObj)
{
  *parentObj = globalObj;

  nsDOMStringMap* dataset = static_cast<nsDOMStringMap*>(nativeObj);

  nsIDocument* document = dataset->GetElement()->OwnerDoc();

  nsCOMPtr<nsIScriptGlobalObject> sgo =
      do_GetInterface(document->GetScopeObject());

  if (sgo) {
    JSObject *global = sgo->GetGlobalJSObject();

    if (global) {
      *parentObj = global;
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStringMapSH::DelProperty(nsIXPConnectWrappedNative *wrapper,
                              JSContext *cx, JSObject *obj, jsid id,
                              jsval *vp, bool *_retval)
{
  nsCOMPtr<nsIDOMDOMStringMap> dataset(do_QueryWrappedNative(wrapper, obj));
  NS_ENSURE_TRUE(dataset, NS_ERROR_UNEXPECTED);

  nsAutoString prop;
  NS_ENSURE_TRUE(JSIDToProp(id, prop), NS_ERROR_UNEXPECTED);

  dataset->RemoveDataAttr(prop);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStringMapSH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                              JSObject *obj, jsid id, jsval *vp,
                              bool *_retval)
{
  nsCOMPtr<nsIDOMDOMStringMap> dataset(do_QueryWrappedNative(wrapper, obj));
  NS_ENSURE_TRUE(dataset, NS_ERROR_UNEXPECTED);

  nsAutoString propName;
  NS_ENSURE_TRUE(JSIDToProp(id, propName), NS_ERROR_UNEXPECTED);

  nsAutoString propVal;
  nsresult rv = dataset->GetDataAttr(propName, propVal);
  NS_ENSURE_SUCCESS(rv, rv);

  if (propVal.IsVoid()) {
    *vp = JSVAL_VOID;
    return NS_SUCCESS_I_DID_SOMETHING;
  }

  nsStringBuffer* valBuf;
  *vp = XPCStringConvert::ReadableToJSVal(cx, propVal, &valBuf);
  if (valBuf) {
    propVal.ForgetSharedBuffer();
  }

  return NS_SUCCESS_I_DID_SOMETHING;
}

NS_IMETHODIMP
nsDOMStringMapSH::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                              JSObject *obj, jsid id, jsval *vp,
                              bool *_retval)
{
  nsCOMPtr<nsIDOMDOMStringMap> dataset(do_QueryWrappedNative(wrapper, obj));
  NS_ENSURE_TRUE(dataset, NS_ERROR_UNEXPECTED);

  nsAutoString propName;
  NS_ENSURE_TRUE(JSIDToProp(id, propName), NS_ERROR_UNEXPECTED);

  JSString *val = JS_ValueToString(cx, *vp);
  NS_ENSURE_TRUE(val, NS_ERROR_UNEXPECTED);

  nsDependentJSString propVal;
  NS_ENSURE_TRUE(propVal.init(cx, val), NS_ERROR_UNEXPECTED);

  nsresult rv = dataset->SetDataAttr(propName, propVal);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_SUCCESS_I_DID_SOMETHING;
}

bool
nsDOMStringMapSH::JSIDToProp(const jsid& aId, nsAString& aResult)
{
  if (JSID_IS_INT(aId)) {
    aResult.AppendInt(JSID_TO_INT(aId));
  } else if (JSID_IS_STRING(aId)) {
    aResult = nsDependentJSString(aId);
  } else {
    return false;
  }

  return true;
}

// Can't be static so GetterShim will compile
nsresult
DocumentURIObjectGetter(JSContext *cx, JSObject *obj, jsval *vp)
{
  // This function duplicates some of the logic in XPC_WN_HelperGetProperty
  XPCWrappedNative *wrapper =
    XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);

  // The error checks duplicate code in THROW_AND_RETURN_IF_BAD_WRAPPER
  NS_ENSURE_TRUE(!wrapper || wrapper->IsValid(), NS_ERROR_XPC_HAS_BEEN_SHUTDOWN);

  nsCOMPtr<nsIDocument> doc = do_QueryWrappedNative(wrapper, obj);
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  return WrapNative(cx, JS_GetGlobalForScopeChain(cx), doc->GetDocumentURI(),
                    &NS_GET_IID(nsIURI), true, vp);
}

NS_IMETHODIMP
nsDocumentSH::PostCreatePrototype(JSContext * cx, JSObject * proto)
{
  // set up our proto first
  nsresult rv = nsNodeSH::PostCreatePrototype(cx, proto);

  if (xpc::AccessCheck::isChrome(js::GetObjectCompartment(proto))) {
    // Stick a documentURIObject property on there
    JS_DefinePropertyById(cx, proto, sDocumentURIObject_id,
                          JSVAL_VOID, GetterShim<DocumentURIObjectGetter>,
                          nsnull,
                          JSPROP_READONLY | JSPROP_SHARED);
  }

  return rv;
}

NS_IMETHODIMP
nsDocumentSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, PRUint32 flags,
                         JSObject **objp, bool *_retval)
{
  nsresult rv;

  if (id == sLocation_id) {
    // Define the location property on the document object itself so
    // that we can intercept getting and setting of document.location.

    nsCOMPtr<nsIDOMDocument> doc = do_QueryWrappedNative(wrapper, obj);
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMLocation> location;
    rv = doc->GetLocation(getter_AddRefs(location));
    NS_ENSURE_SUCCESS(rv, rv);

    jsval v;

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, JS_GetGlobalForScopeChain(cx), location,
                    &NS_GET_IID(nsIDOMLocation), true, &v,
                    getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    JSBool ok = ::JS_DefinePropertyById(cx, obj, id, v, nsnull,
                                        LocationSetter<nsIDOMDocument>,
                                        JSPROP_PERMANENT | JSPROP_ENUMERATE);

    if (!ok) {
      return NS_ERROR_FAILURE;
    }

    *objp = obj;

    return NS_OK;
  }

  return nsNodeSH::NewResolve(wrapper, cx, obj, id, flags, objp, _retval);
}

NS_IMETHODIMP
nsDocumentSH::GetFlags(PRUint32* aFlags)
{
  *aFlags = DOMCLASSINFO_STANDARD_FLAGS;

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentSH::PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj)
{
  // If this is the current document for the window that's the script global
  // object of this document, then define this document object on the window.
  // That will make sure that the document is referenced (via window.document)
  // and prevent it from going away in GC.
  nsCOMPtr<nsIDocument> doc = do_QueryWrappedNative(wrapper);
  if (!doc) {
    return NS_ERROR_UNEXPECTED;
  }

  nsIScriptGlobalObject *sgo = doc->GetScriptGlobalObject();
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(sgo);
  if (!win) {
    // No window, nothing else to do here
    return NS_OK;
  }

  nsIDOMDocument* currentDoc = win->GetExtantDocument();

  if (SameCOMIdentity(doc, currentDoc)) {
    jsval winVal;

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult rv = WrapNative(cx, obj, win, &NS_GET_IID(nsIDOMWindow), false,
                             &winVal, getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_NAMED_LITERAL_STRING(doc_str, "document");

    if (!::JS_DefineUCProperty(cx, JSVAL_TO_OBJECT(winVal),
                               reinterpret_cast<const jschar *>
                                               (doc_str.get()),
                               doc_str.Length(), OBJECT_TO_JSVAL(obj),
                               JS_PropertyStub, JS_StrictPropertyStub,
                               JSPROP_READONLY | JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

// HTMLDocument helper

static nsresult
ResolveImpl(JSContext *cx, nsIXPConnectWrappedNative *wrapper, jsid id,
            nsISupports **result, nsWrapperCache **aCache)
{
  nsHTMLDocument *doc =
    static_cast<nsHTMLDocument*>(static_cast<nsINode*>(wrapper->Native()));

  // 'id' is not always a string, it can be a number since document.1
  // should map to <input name="1">. Thus we can't use
  // JSVAL_TO_STRING() here.
  JSString *str = IdToString(cx, id);
  NS_ENSURE_TRUE(str, NS_ERROR_UNEXPECTED);

  nsDependentJSString depStr;
  NS_ENSURE_TRUE(depStr.init(cx, str), NS_ERROR_UNEXPECTED);

  return doc->ResolveName(depStr, nsnull, result, aCache);
}


static JSClass sHTMLDocumentAllClass = {
  "HTML document.all class",
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_NEW_RESOLVE |
  JSCLASS_HAS_RESERVED_SLOTS(1),
  JS_PropertyStub, JS_PropertyStub, nsHTMLDocumentSH::DocumentAllGetProperty,
  JS_StrictPropertyStub, JS_EnumerateStub,
  (JSResolveOp)nsHTMLDocumentSH::DocumentAllNewResolve, JS_ConvertStub,
  nsHTMLDocumentSH::ReleaseDocument, nsnull, nsnull,
  nsHTMLDocumentSH::CallToGetPropMapper
};


static JSClass sHTMLDocumentAllHelperClass = {
  "HTML document.all helper class", JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
  JS_PropertyStub, JS_PropertyStub,
  nsHTMLDocumentSH::DocumentAllHelperGetProperty,
  JS_StrictPropertyStub, JS_EnumerateStub,
  (JSResolveOp)nsHTMLDocumentSH::DocumentAllHelperNewResolve, JS_ConvertStub,
  nsnull
};


static JSClass sHTMLDocumentAllTagsClass = {
  "HTML document.all.tags class",
  JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, (JSResolveOp)nsHTMLDocumentSH::DocumentAllTagsNewResolve,
  JS_ConvertStub, nsHTMLDocumentSH::ReleaseDocument, nsnull, nsnull,
  nsHTMLDocumentSH::CallToGetPropMapper
};

// static
JSBool
nsHTMLDocumentSH::GetDocumentAllNodeList(JSContext *cx, JSObject *obj,
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

  jsval collection = JS_GetReservedSlot(obj, 0);

  if (!JSVAL_IS_PRIMITIVE(collection)) {
    // We already have a node list in our reserved slot, use it.
    JSObject *obj = JSVAL_TO_OBJECT(collection);
    if (mozilla::dom::binding::HTMLCollection::objIsWrapper(obj)) {
      nsIHTMLCollection *native =
        mozilla::dom::binding::HTMLCollection::getNative(obj);
      NS_ADDREF(*nodeList = static_cast<nsContentList*>(native));
    }
    else {
      nsISupports *native = sXPConnect->GetNativeOfWrapper(cx, obj);
      if (native) {
        NS_ADDREF(*nodeList = nsContentList::FromSupports(native));
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
      rv |= NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv |= WrapNative(cx, JS_GetGlobalForScopeChain(cx),
                     static_cast<nsINodeList*>(list), list, false,
                     &collection, getter_AddRefs(holder));

    list.forget(nodeList);

    // ... and store it in our reserved slot.
    JS_SetReservedSlot(obj, 0, collection);
  }

  if (NS_FAILED(rv)) {
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_FAILURE);

    return JS_FALSE;
  }

  return *nodeList != nsnull;
}

JSBool
nsHTMLDocumentSH::DocumentAllGetProperty(JSContext *cx, JSObject *obj,
                                         jsid id, jsval *vp)
{
  // document.all.item and .namedItem get their value in the
  // newResolve hook, so nothing to do for those properties here. And
  // we need to return early to prevent <div id="item"> from shadowing
  // document.all.item(), etc.
  if (id == sItem_id || id == sNamedItem_id) {
    return JS_TRUE;
  }

  while (js::GetObjectJSClass(obj) != &sHTMLDocumentAllClass) {
    obj = js::GetObjectProto(obj);

    if (!obj) {
      NS_ERROR("The JS engine lies!");

      return JS_TRUE;
    }
  }

  nsHTMLDocument *doc = GetDocument(obj);
  nsISupports *result;
  nsWrapperCache *cache;
  nsresult rv = NS_OK;

  if (JSID_IS_STRING(id)) {
    if (id == sLength_id) {
      // Map document.all.length to the length of the collection
      // document.getElementsByTagName("*"), and make sure <div
      // id="length"> doesn't shadow document.all.length.

      nsRefPtr<nsContentList> nodeList;
      if (!GetDocumentAllNodeList(cx, obj, doc, getter_AddRefs(nodeList))) {
        return JS_FALSE;
      }

      PRUint32 length;
      rv = nodeList->GetLength(&length);

      if (NS_FAILED(rv)) {
        nsDOMClassInfo::ThrowJSException(cx, rv);

        return JS_FALSE;
      }

      *vp = INT_TO_JSVAL(length);

      return JS_TRUE;
    } else if (id != sTags_id) {
      // For all other strings, look for an element by id or name.

      nsDependentJSString str(id);

      result = doc->GetDocumentAllResult(str, &cache, &rv);

      if (NS_FAILED(rv)) {
        nsDOMClassInfo::ThrowJSException(cx, rv);

        return JS_FALSE;
      }
    }
    else {
      result = nsnull;
    }
  } else if (JSID_IS_INT(id) && JSID_TO_INT(id) >= 0) {
    // Map document.all[n] (where n is a number) to the n:th item in
    // the document.all node list.

    nsRefPtr<nsContentList> nodeList;
    if (!GetDocumentAllNodeList(cx, obj, doc, getter_AddRefs(nodeList))) {
      return JS_FALSE;
    }

    nsIContent *node = nodeList->GetNodeAt(JSID_TO_INT(id));

    result = node;
    cache = node;
  } else {
    result = nsnull;
  }

  if (result) {
    rv = WrapNative(cx, JS_GetGlobalForScopeChain(cx), result, cache, true, vp);
    if (NS_FAILED(rv)) {
      nsDOMClassInfo::ThrowJSException(cx, rv);

      return JS_FALSE;
    }
  } else {
    *vp = JSVAL_VOID;
  }

  return JS_TRUE;
}

JSBool
nsHTMLDocumentSH::DocumentAllNewResolve(JSContext *cx, JSObject *obj, jsid id,
                                        unsigned flags, JSObject **objp)
{
  if (flags & JSRESOLVE_ASSIGNING) {
    // Nothing to do here if we're assigning

    return JS_TRUE;
  }

  jsval v = JSVAL_VOID;

  if (id == sItem_id || id == sNamedItem_id) {
    // Define the item() or namedItem() method.

    JSFunction *fnc = ::JS_DefineFunctionById(cx, obj, id, CallToGetPropMapper,
                                              0, JSPROP_ENUMERATE);
    *objp = obj;

    return fnc != nsnull;
  }

  if (id == sLength_id) {
    // document.all.length. Any jsval other than undefined would do
    // here, all we need is to get into the code below that defines
    // this propery on obj, the rest happens in
    // DocumentAllGetProperty().

    v = JSVAL_ONE;
  } else if (id == sTags_id) {
    nsHTMLDocument *doc = GetDocument(obj);

    JSObject *tags = ::JS_NewObject(cx, &sHTMLDocumentAllTagsClass, nsnull,
                                    ::JS_GetGlobalForObject(cx, obj));
    if (!tags) {
      return JS_FALSE;
    }

    ::JS_SetPrivate(tags, doc);

    // The "tags" JSObject now also owns doc.
    NS_ADDREF(doc);

    v = OBJECT_TO_JSVAL(tags);
  } else {
    if (!DocumentAllGetProperty(cx, obj, id, &v)) {
      return JS_FALSE;
    }
  }

  JSBool ok = JS_TRUE;

  if (v != JSVAL_VOID) {
    ok = ::JS_DefinePropertyById(cx, obj, id, v, nsnull, nsnull, 0);
    *objp = obj;
  }

  return ok;
}

// Finalize hook used by document related JS objects, but also by
// sGlobalScopePolluterClass!

void
nsHTMLDocumentSH::ReleaseDocument(JSFreeOp *fop, JSObject *obj)
{
  nsIHTMLDocument *doc = (nsIHTMLDocument *)::JS_GetPrivate(obj);

  NS_IF_RELEASE(doc);
}

JSBool
nsHTMLDocumentSH::CallToGetPropMapper(JSContext *cx, unsigned argc, jsval *vp)
{
  // Handle document.all("foo") style access to document.all.

  if (argc != 1) {
    // XXX: Should throw NS_ERROR_XPC_NOT_ENOUGH_ARGS for argc < 1,
    // and create a new NS_ERROR_XPC_TOO_MANY_ARGS for argc > 1? IE
    // accepts nothing other than one arg.
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_INVALID_ARG);

    return JS_FALSE;
  }

  // Convert all types to string.
  JSString *str = ::JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
  if (!str) {
    return JS_FALSE;
  }

  // If we are called via document.all(id) instead of document.all.item(i) or
  // another method, use the document.all callee object as self.
  JSObject *self;
  if (JSVAL_IS_OBJECT(JS_CALLEE(cx, vp)) &&
      ::JS_GetClass(JSVAL_TO_OBJECT(JS_CALLEE(cx, vp))) == &sHTMLDocumentAllClass) {
    self = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
  } else {
    self = JS_THIS_OBJECT(cx, vp);
    if (!self)
      return JS_FALSE;
  }

  size_t length;
  const jschar *chars = ::JS_GetStringCharsAndLength(cx, str, &length);
  if (!chars) {
    return JS_FALSE;
  }

  return ::JS_GetUCProperty(cx, self, chars, length, vp);
}


static inline JSObject *
GetDocumentAllHelper(JSObject *obj)
{
  while (obj && JS_GetClass(obj) != &sHTMLDocumentAllHelperClass) {
    obj = ::JS_GetPrototype(obj);
  }

  return obj;
}

static inline void *
FlagsToPrivate(PRUint32 flags)
{
  JS_ASSERT((flags & (1 << 31)) == 0);
  return (void *)(flags << 1);
}

static inline PRUint32
PrivateToFlags(void *priv)
{
  JS_ASSERT(size_t(priv) <= PR_UINT32_MAX && (size_t(priv) & 1) == 0);
  return (PRUint32)(size_t(priv) >> 1);
}

JSBool
nsHTMLDocumentSH::DocumentAllHelperGetProperty(JSContext *cx, JSObject *obj,
                                               jsid id, jsval *vp)
{
  if (id != nsDOMClassInfo::sAll_id) {
    return JS_TRUE;
  }

  JSObject *helper = GetDocumentAllHelper(obj);

  if (!helper) {
    NS_ERROR("Uh, how'd we get here?");

    // Let scripts continue, if we somehow did get here...

    return JS_TRUE;
  }

  PRUint32 flags = PrivateToFlags(::JS_GetPrivate(helper));

  if (flags & JSRESOLVE_DETECTING || !(flags & JSRESOLVE_QUALIFIED)) {
    // document.all is either being detected, e.g. if (document.all),
    // or it was not being resolved with a qualified name. Claim that
    // document.all is undefined.

    *vp = JSVAL_VOID;
  } else {
    // document.all is not being detected, and it resolved with a
    // qualified name. Expose the document.all collection.

    if (!JSVAL_IS_OBJECT(*vp)) {
      // First time through, create the collection, and set the
      // document as its private nsISupports data.
      nsresult rv;
      nsCOMPtr<nsIHTMLDocument> doc = do_QueryWrapper(cx, obj, &rv);
      if (NS_FAILED(rv)) {
        nsDOMClassInfo::ThrowJSException(cx, rv);

        return JS_FALSE;
      }

      JSObject *all = ::JS_NewObject(cx, &sHTMLDocumentAllClass, nsnull,
                                     ::JS_GetGlobalForObject(cx, obj));
      if (!all) {
        return JS_FALSE;
      }

      // Let the JSObject take over ownership of doc.
      ::JS_SetPrivate(all, doc);

      doc.forget();

      *vp = OBJECT_TO_JSVAL(all);
    }
  }

  return JS_TRUE;
}

JSBool
nsHTMLDocumentSH::DocumentAllHelperNewResolve(JSContext *cx, JSObject *obj,
                                              jsid id, unsigned flags,
                                              JSObject **objp)
{
  if (id == nsDOMClassInfo::sAll_id) {
    // document.all is resolved for the first time. Define it.
    JSObject *helper = GetDocumentAllHelper(obj);

    if (helper) {
      if (!::JS_DefineProperty(cx, helper, "all", JSVAL_VOID, nsnull, nsnull,
                               JSPROP_ENUMERATE)) {
        return JS_FALSE;
      }

      *objp = helper;
    }
  }

  return JS_TRUE;
}


JSBool
nsHTMLDocumentSH::DocumentAllTagsNewResolve(JSContext *cx, JSObject *obj,
                                            jsid id, unsigned flags,
                                            JSObject **objp)
{
  if (JSID_IS_STRING(id)) {
    nsDocument *doc = GetDocument(obj);

    JSObject *proto = ::JS_GetPrototype(obj);
    if (NS_UNLIKELY(!proto)) {
      return JS_TRUE;
    }

    JSBool found;
    if (!::JS_HasPropertyById(cx, proto, id, &found)) {
      return JS_FALSE;
    }

    if (found) {
      return JS_TRUE;
    }

    nsRefPtr<nsContentList> tags =
      doc->GetElementsByTagName(nsDependentJSString(id));

    if (tags) {
      jsval v;
      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      nsresult rv = WrapNative(cx, JS_GetGlobalForScopeChain(cx),
                               static_cast<nsINodeList*>(tags), tags, true,
                               &v, getter_AddRefs(holder));
      if (NS_FAILED(rv)) {
        nsDOMClassInfo::ThrowJSException(cx, rv);

        return JS_FALSE;
      }

      if (!::JS_DefinePropertyById(cx, obj, id, v, nsnull, nsnull, 0)) {
        return JS_FALSE;
      }

      *objp = obj;
    }
  }

  return JS_TRUE;
}


NS_IMETHODIMP
nsHTMLDocumentSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, jsid id, PRUint32 flags,
                             JSObject **objp, bool *_retval)
{
  // nsDocumentSH::NewResolve() does a security check that we'd kinda
  // want to do here too before doing anything else. But given that we
  // only define dynamic properties here before the call to
  // nsDocumentSH::NewResolve() we're ok, since once those properties
  // are accessed, we'll do the necessary security check.

  if (!(flags & JSRESOLVE_ASSIGNING)) {
    // For native wrappers, do not resolve random names on document

    JSAutoRequest ar(cx);

    if (!ObjectIsNativeWrapper(cx, obj)) {
      nsCOMPtr<nsISupports> result;
      nsWrapperCache *cache;
      nsresult rv = ResolveImpl(cx, wrapper, id, getter_AddRefs(result),
                                &cache);
      NS_ENSURE_SUCCESS(rv, rv);

      if (result) {
        JSBool ok = *_retval =
          ::JS_DefinePropertyById(cx, obj, id, JSVAL_VOID, nsnull, nsnull, 0);
        *objp = obj;

        return ok ? NS_OK : NS_ERROR_FAILURE;
      }
    }

    if (id == sAll_id && !sDisableDocumentAllSupport &&
        !ObjectIsNativeWrapper(cx, obj)) {
      nsIDocument *doc = static_cast<nsIDocument*>(wrapper->Native());

      if (doc->GetCompatibilityMode() == eCompatibility_NavQuirks) {
        JSObject *helper = GetDocumentAllHelper(::JS_GetPrototype(obj));

        JSObject *proto = ::JS_GetPrototype(helper ? helper : obj);

        // Check if the property all is defined on obj's (or helper's
        // if obj doesn't exist) prototype, if it is, don't expose our
        // document.all helper.

        JSBool hasAll = JS_FALSE;
        if (proto && !JS_HasProperty(cx, proto, "all", &hasAll)) {
          return NS_ERROR_UNEXPECTED;
        }

        if (hasAll && helper) {
          // Our helper's prototype now has an "all" property, remove
          // the helper out of the prototype chain to prevent
          // shadowing of the now defined "all" property.
          JSObject *tmp = obj, *tmpProto;

          while ((tmpProto = ::JS_GetPrototype(tmp)) != helper) {
            tmp = tmpProto;
          }

          ::JS_SetPrototype(cx, tmp, proto);
        }

        // If we don't already have a helper, and we're resolving
        // document.all qualified, and we're *not* detecting
        // document.all, e.g. if (document.all), and "all" isn't
        // already defined on our prototype, create a helper.
        if (!helper && flags & JSRESOLVE_QUALIFIED &&
            !(flags & JSRESOLVE_DETECTING) && !hasAll) {
          // Print a warning so developers can stop using document.all
          PrintWarningOnConsole(cx, "DocumentAllUsed");

          helper = ::JS_NewObject(cx, &sHTMLDocumentAllHelperClass,
                                  ::JS_GetPrototype(obj),
                                  ::JS_GetGlobalForObject(cx, obj));

          if (!helper) {
            return NS_ERROR_OUT_OF_MEMORY;
          }

          // Insert the helper into our prototype chain. helper's prototype
          // is already obj's current prototype.
          if (!::JS_SetPrototype(cx, obj, helper)) {
            nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

            return NS_ERROR_UNEXPECTED;
          }
        }

        // If we have (or just created) a helper, pass the resolve flags
        // to the helper as its private data.
        if (helper) {
          ::JS_SetPrivate(helper, FlagsToPrivate(flags));
        }
      }

      return NS_OK;
    }
  }

  return nsDocumentSH::NewResolve(wrapper, cx, obj, id, flags, objp, _retval);
}

NS_IMETHODIMP
nsHTMLDocumentSH::GetProperty(nsIXPConnectWrappedNative *wrapper,
                              JSContext *cx, JSObject *obj, jsid id,
                              jsval *vp, bool *_retval)
{
  // For native wrappers, do not get random names on document
  if (!ObjectIsNativeWrapper(cx, obj)) {
    nsCOMPtr<nsISupports> result;

    JSAutoRequest ar(cx);

    nsWrapperCache *cache;
    nsresult rv = ResolveImpl(cx, wrapper, id, getter_AddRefs(result), &cache);
    NS_ENSURE_SUCCESS(rv, rv);

    if (result) {
      rv = WrapNative(cx, obj, result, cache, true, vp);
      if (NS_SUCCEEDED(rv)) {
        rv = NS_SUCCESS_I_DID_SOMETHING;
      }
      return rv;
    }
  }

  return NS_OK;
}

// HTMLFormElement helper

// static
nsresult
nsHTMLFormElementSH::FindNamedItem(nsIForm *aForm, jsid id,
                                   nsISupports **aResult,
                                   nsWrapperCache **aCache)
{
  nsDependentJSString name(id);

  *aResult = aForm->ResolveName(name).get();
  // FIXME Get the wrapper cache from nsIForm::ResolveName
  *aCache = nsnull;

  if (!*aResult) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aForm));

    nsCOMPtr<nsIHTMLDocument> html_doc =
      do_QueryInterface(content->GetDocument());

    if (html_doc && content) {
      html_doc->ResolveName(name, content, aResult, aCache);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElementSH::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                JSContext *cx, JSObject *obj, jsid id,
                                PRUint32 flags, JSObject **objp,
                                bool *_retval)
{
  // For native wrappers, do not resolve random names on form
  if ((!(JSRESOLVE_ASSIGNING & flags)) && JSID_IS_STRING(id) &&
      !ObjectIsNativeWrapper(cx, obj)) {
    nsCOMPtr<nsIForm> form(do_QueryWrappedNative(wrapper, obj));
    nsCOMPtr<nsISupports> result;
    nsWrapperCache *cache;

    FindNamedItem(form, id, getter_AddRefs(result), &cache);

    if (result) {
      JSAutoRequest ar(cx);
      *_retval = ::JS_DefinePropertyById(cx, obj, id, JSVAL_VOID, nsnull,
                                         nsnull, JSPROP_ENUMERATE);

      *objp = obj;

      return *_retval ? NS_OK : NS_ERROR_FAILURE;
    }
  }

  return nsElementSH::NewResolve(wrapper, cx, obj, id, flags, objp, _retval);
}


NS_IMETHODIMP
nsHTMLFormElementSH::GetProperty(nsIXPConnectWrappedNative *wrapper,
                                 JSContext *cx, JSObject *obj, jsid id,
                                 jsval *vp, bool *_retval)
{
  nsCOMPtr<nsIForm> form(do_QueryWrappedNative(wrapper, obj));

  if (JSID_IS_STRING(id)) {
    // For native wrappers, do not get random names on form
    if (!ObjectIsNativeWrapper(cx, obj)) {
      nsCOMPtr<nsISupports> result;
      nsWrapperCache *cache;

      FindNamedItem(form, id, getter_AddRefs(result), &cache);

      if (result) {
        // Wrap result, result can be either an element or a list of
        // elements
        nsresult rv = WrapNative(cx, obj, result, cache, true, vp);
        return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
      }
    }
  } else {
    PRInt32 n = GetArrayIndexFromId(cx, id);

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
                                  PRUint32 enum_op, jsval *statep,
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
        PRUint32 count = form->GetElementCount();

        *idp = INT_TO_JSID(count);
      }

      break;
    }
  case JSENUMERATE_NEXT:
    {
      nsCOMPtr<nsIForm> form(do_QueryWrappedNative(wrapper, obj));
      NS_ENSURE_TRUE(form, NS_ERROR_FAILURE);

      PRInt32 index = (PRInt32)JSVAL_TO_INT(*statep);

      PRUint32 count = form->GetElementCount();

      if ((PRUint32)index < count) {
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

        JSAutoRequest ar(cx);

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


// HTMLSelectElement helper

NS_IMETHODIMP
nsHTMLSelectElementSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                                  JSObject *obj, jsid id, PRUint32 flags,
                                  JSObject **objp, bool *_retval)
{
  PRInt32 n = GetArrayIndexFromId(cx, id);
  if (n >= 0) {
    nsHTMLSelectElement *s =
      nsHTMLSelectElement::FromSupports(GetNative(wrapper, obj));

    nsHTMLOptionCollection *options = s->GetOptions();
    if (options) {
      nsISupports *node = options->GetNodeAt(n);
      if (node) {
        *objp = obj;
        *_retval = JS_DefineElement(cx, obj, PRUint32(n), JSVAL_VOID, nsnull, nsnull,
                                    JSPROP_ENUMERATE | JSPROP_SHARED);

        return NS_OK;
      }
    }
  }

  return nsElementSH::NewResolve(wrapper, cx, obj, id, flags, objp, _retval);
}

NS_IMETHODIMP
nsHTMLSelectElementSH::GetProperty(nsIXPConnectWrappedNative *wrapper,
                                   JSContext *cx, JSObject *obj, jsid id,
                                   jsval *vp, bool *_retval)
{
  PRInt32 n = GetArrayIndexFromId(cx, id);

  nsresult rv = NS_OK;
  if (n >= 0) {
    nsHTMLSelectElement *s =
      nsHTMLSelectElement::FromSupports(GetNative(wrapper, obj));

    nsHTMLOptionCollection *options = s->GetOptions();

    if (options) {
      nsISupports *node = options->GetNodeAt(n);

      rv = WrapNative(cx, JS_GetGlobalForScopeChain(cx), node,
                      &NS_GET_IID(nsIDOMNode), true, vp);
      if (NS_SUCCEEDED(rv)) {
        rv = NS_SUCCESS_I_DID_SOMETHING;
      }
      return rv;
    }
  }

  return NS_OK;
}

// static
nsresult
nsHTMLSelectElementSH::SetOption(JSContext *cx, jsval *vp, PRUint32 aIndex,
                                 nsIDOMHTMLOptionsCollection *aOptCollection)
{
  JSAutoRequest ar(cx);

  // vp must refer to an object
  if (!JSVAL_IS_OBJECT(*vp) && !::JS_ConvertValue(cx, *vp, JSTYPE_OBJECT, vp)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIDOMHTMLOptionElement> new_option;

  if (!JSVAL_IS_NULL(*vp)) {
    new_option = do_QueryWrapper(cx, JSVAL_TO_OBJECT(*vp));
    if (!new_option) {
      // Someone is trying to set an option to a non-option object.

      return NS_ERROR_UNEXPECTED;
    }
  }

  return aOptCollection->SetOption(aIndex, new_option);
}

NS_IMETHODIMP
nsHTMLSelectElementSH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                                   JSContext *cx, JSObject *obj, jsid id,
                                   jsval *vp, bool *_retval)
{
  PRInt32 n = GetArrayIndexFromId(cx, id);

  if (n >= 0) {
    nsCOMPtr<nsIDOMHTMLSelectElement> select =
      do_QueryWrappedNative(wrapper, obj);
    NS_ENSURE_TRUE(select, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
    select->GetOptions(getter_AddRefs(options));

    nsresult rv = SetOption(cx, vp, n, options);
    return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
  }

  return NS_OK;
}


// HTMLObject/EmbedElement helper
// Keep in mind that it is OK for this to fail to return an instance. Don't return a
// failure result unless something truly exceptional has happened.
// static
nsresult
nsHTMLPluginObjElementSH::GetPluginInstanceIfSafe(nsIXPConnectWrappedNative *wrapper,
                                                  JSObject *obj,
                                                  nsNPAPIPluginInstance **_result)
{
  *_result = nsnull;

  nsCOMPtr<nsIContent> content(do_QueryWrappedNative(wrapper, obj));
  NS_ENSURE_TRUE(content, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIObjectLoadingContent> objlc(do_QueryInterface(content));
  NS_ASSERTION(objlc, "Object nodes must implement nsIObjectLoadingContent");

  nsresult rv = objlc->GetPluginInstance(_result);
  if (NS_SUCCEEDED(rv) && *_result) {
    return rv;
  }

  // If it's not safe to run script we'll only return the instance if it exists.
  if (!nsContentUtils::IsSafeToRunScript()) {
    return rv;
  }

  // We don't care if this actually starts the plugin or not, we just want to
  // try to start it now if possible.
  objlc->SyncStartPluginInstance();

  return objlc->GetPluginInstance(_result);
}

class nsPluginProtoChainInstallRunner : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  nsPluginProtoChainInstallRunner(nsIXPConnectWrappedNative* wrapper,
                                  nsIScriptContext* scriptContext)
    : mWrapper(wrapper),
      mContext(scriptContext)
  {
  }

  NS_IMETHOD Run()
  {
    JSContext* cx = nsnull;
    if (mContext) {
      cx = mContext->GetNativeContext();
    } else {
      nsCOMPtr<nsIThreadJSContextStack> stack =
        do_GetService("@mozilla.org/js/xpc/ContextStack;1");
      NS_ENSURE_TRUE(stack, NS_OK);

      stack->GetSafeJSContext(&cx);
      NS_ENSURE_TRUE(cx, NS_OK);
    }

    JSObject* obj = nsnull;
    mWrapper->GetJSObject(&obj);
    NS_ASSERTION(obj, "Should never be null");
    nsHTMLPluginObjElementSH::SetupProtoChain(mWrapper, cx, obj);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIXPConnectWrappedNative> mWrapper;
  nsCOMPtr<nsIScriptContext> mContext;
};

NS_IMPL_ISUPPORTS1(nsPluginProtoChainInstallRunner, nsIRunnable)

// static
nsresult
nsHTMLPluginObjElementSH::SetupProtoChain(nsIXPConnectWrappedNative *wrapper,
                                          JSContext *cx,
                                          JSObject *obj)
{
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Shouldn't have gotten in here");

  nsCxPusher cxPusher;
  if (!cxPusher.Push(cx)) {
    return NS_OK;
  }

  JSAutoRequest ar(cx);

  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, obj)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<nsNPAPIPluginInstance> pi;
  nsresult rv = GetPluginInstanceIfSafe(wrapper, obj, getter_AddRefs(pi));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!pi) {
    // No plugin around for this object.

    return NS_OK;
  }

  JSObject *pi_obj = nsnull; // XPConnect-wrapped peer object, when we get it.
  JSObject *pi_proto = nsnull; // 'pi.__proto__'

  rv = GetPluginJSObject(cx, obj, pi, &pi_obj, &pi_proto);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!pi_obj) {
    // Didn't get a plugin instance JSObject, nothing we can do then.
    return NS_OK;
  }

  // If we got an xpconnect-wrapped plugin object, set obj's
  // prototype's prototype to the scriptable plugin.

  JSObject *my_proto = nsnull;

  // Get 'this.__proto__'
  rv = wrapper->GetJSObjectPrototype(&my_proto);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set 'this.__proto__' to pi
  if (!::JS_SetPrototype(cx, obj, pi_obj)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (pi_proto && JS_GetClass(pi_proto) != sObjectClass) {
    // The plugin wrapper has a proto that's not Object.prototype, set
    // 'pi.__proto__.__proto__' to the original 'this.__proto__'
    if (pi_proto != my_proto && !::JS_SetPrototype(cx, pi_proto, my_proto)) {
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    // 'pi' didn't have a prototype, or pi's proto was
    // 'Object.prototype' (i.e. pi is an NPRuntime wrapped JS object)
    // set 'pi.__proto__' to the original 'this.__proto__'
    if (!::JS_SetPrototype(cx, pi_obj, my_proto)) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  // Before this proto dance the objects involved looked like this:
  //
  // this.__proto__.__proto__
  //   ^      ^         ^
  //   |      |         |__ Object.prototype
  //   |      |
  //   |      |__ xpc embed wrapper proto (shared)
  //   |
  //   |__ xpc wrapped native embed node
  //
  // pi.__proto__
  // ^      ^
  // |      |__ Object.prototype
  // |
  // |__ Plugin NPRuntime JS object wrapper
  //
  // Now, after the above prototype setup the prototype chain should
  // look like this:
  //
  // this.__proto__.__proto__.__proto__
  //   ^      ^         ^         ^
  //   |      |         |         |__ Object.prototype
  //   |      |         |
  //   |      |         |__ xpc embed wrapper proto (shared)
  //   |      |
  //   |      |__ Plugin NPRuntime JS object wrapper
  //   |
  //   |__ xpc wrapped native embed node
  //

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLPluginObjElementSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                                    JSObject *globalObj, JSObject **parentObj)
{
  nsresult rv = nsElementSH::PreCreate(nativeObj, cx, globalObj, parentObj);

  // For now we don't support slim wrappers for plugins.
  return rv == NS_SUCCESS_ALLOW_SLIM_WRAPPERS ? NS_OK : rv;
}

NS_IMETHODIMP
nsHTMLPluginObjElementSH::PostCreate(nsIXPConnectWrappedNative *wrapper,
                                     JSContext *cx, JSObject *obj)
{
  if (nsContentUtils::IsSafeToRunScript()) {
#ifdef DEBUG
    nsresult rv =
#endif
      SetupProtoChain(wrapper, cx, obj);

    // If SetupProtoChain failed then we're in real trouble. We're about to fail
    // PostCreate but it's more than likely that we handed our (now invalid)
    // wrapper to someone already. Bug 429442 is an example of the kind of crash
    // that can result from such a situation. We'll return NS_OK for the time
    // being and hope for the best.
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "SetupProtoChain failed!");
    return NS_OK;
  }

  // This may be null if the JS context is not a DOM context. That's ok, we'll
  // use the safe context from XPConnect in the runnable.
  nsCOMPtr<nsIScriptContext> scriptContext = GetScriptContextFromJSContext(cx);

  nsRefPtr<nsPluginProtoChainInstallRunner> runner =
    new nsPluginProtoChainInstallRunner(wrapper, scriptContext);
  nsContentUtils::AddScriptRunner(runner);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLPluginObjElementSH::GetProperty(nsIXPConnectWrappedNative *wrapper,
                                      JSContext *cx, JSObject *obj, jsid id,
                                      jsval *vp, bool *_retval)
{
  JSAutoRequest ar(cx);

  JSObject *pi_obj = ::JS_GetPrototype(obj);
  if (NS_UNLIKELY(!pi_obj)) {
    return NS_OK;
  }

  JSBool found = false;

  if (!ObjectIsNativeWrapper(cx, obj)) {
    *_retval = ::JS_HasPropertyById(cx, pi_obj, id, &found);
    if (!*_retval) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (found) {
    *_retval = ::JS_GetPropertyById(cx, pi_obj, id, vp);
    return *_retval ? NS_SUCCESS_I_DID_SOMETHING : NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLPluginObjElementSH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                                      JSContext *cx, JSObject *obj, jsid id,
                                      jsval *vp, bool *_retval)
{
  JSAutoRequest ar(cx);

  JSObject *pi_obj = ::JS_GetPrototype(obj);
  if (NS_UNLIKELY(!pi_obj)) {
    return NS_OK;
  }

  JSBool found = false;

  if (!ObjectIsNativeWrapper(cx, obj)) {
    *_retval = ::JS_HasPropertyById(cx, pi_obj, id, &found);
    if (!*_retval) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (found) {
    *_retval = ::JS_SetPropertyById(cx, pi_obj, id, vp);
    return *_retval ? NS_SUCCESS_I_DID_SOMETHING : NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLPluginObjElementSH::Call(nsIXPConnectWrappedNative *wrapper,
                               JSContext *cx, JSObject *obj, PRUint32 argc,
                               jsval *argv, jsval *vp, bool *_retval)
{
  nsRefPtr<nsNPAPIPluginInstance> pi;
  nsresult rv = GetPluginInstanceIfSafe(wrapper, obj, getter_AddRefs(pi));
  NS_ENSURE_SUCCESS(rv, rv);

  // If obj is a native wrapper, or if there's no plugin around for
  // this object, throw.
  if (ObjectIsNativeWrapper(cx, obj) || !pi) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  JSObject *pi_obj = nsnull;
  JSObject *pi_proto = nsnull;

  rv = GetPluginJSObject(cx, obj, pi, &pi_obj, &pi_proto);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!pi) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // XPConnect passes us the XPConnect wrapper JSObject as obj, and
  // not the 'this' parameter that the JS engine passes in. Pass in
  // the real this parameter from JS (argv[-1]) here.
  JSAutoRequest ar(cx);
  *_retval = ::JS::Call(cx, argv[-1], pi_obj, argc, argv, vp);

  return NS_OK;
}


nsresult
nsHTMLPluginObjElementSH::GetPluginJSObject(JSContext *cx, JSObject *obj,
                                            nsNPAPIPluginInstance *plugin_inst,
                                            JSObject **plugin_obj,
                                            JSObject **plugin_proto)
{
  *plugin_obj = nsnull;
  *plugin_proto = nsnull;

  JSAutoRequest ar(cx);

  // NB: We need an AutoEnterCompartment because we can be called from
  // nsObjectFrame when the plugin loads after the JS object for our content
  // node has been created.
  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, obj)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (plugin_inst) {
    plugin_inst->GetJSObject(cx, plugin_obj);
    if (*plugin_obj) {
      *plugin_proto = ::JS_GetPrototype(*plugin_obj);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLPluginObjElementSH::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                     JSContext *cx, JSObject *obj, jsid id,
                                     PRUint32 flags, JSObject **objp,
                                     bool *_retval)
{
  // Make sure the plugin instance is loaded and instantiated, if
  // possible.

  nsRefPtr<nsNPAPIPluginInstance> pi;
  nsresult rv = GetPluginInstanceIfSafe(wrapper, obj, getter_AddRefs(pi));
  NS_ENSURE_SUCCESS(rv, rv);

  return nsElementSH::NewResolve(wrapper, cx, obj, id, flags, objp,
                                 _retval);
}
 
// Plugin helper

nsISupports*
nsPluginSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
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
nsPluginArraySH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
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
nsMimeTypeArraySH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
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
                             JSObject *obj, jsid id, jsval *vp,
                             bool *_retval)
{
  bool is_number = false;
  PRInt32 n = GetArrayIndexFromId(cx, id, &is_number);

  if (!is_number) {
    return NS_OK;
  }

  nsAutoString val;

  nsresult rv = GetStringAt(GetNative(wrapper, obj), n, val);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(cx);

  if (DOMStringIsNull(val)) {
    *vp = JSVAL_VOID;
  } else {
    nsStringBuffer* sharedBuffer = nsnull;
    *vp = XPCStringConvert::ReadableToJSVal(cx, val, &sharedBuffer);
    if (sharedBuffer) {
      val.ForgetSharedBuffer();
    }
  }

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
                         JSObject *obj, jsid id, jsval *vp, bool *_retval)
{
  bool is_number = false;
  GetArrayIndexFromId(cx, id, &is_number);

  if (!is_number) {
    return NS_OK;
  }

  return nsStringArraySH::GetProperty(wrapper, cx, obj, id, vp, _retval);
}

nsresult
nsHistorySH::GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                         nsAString& aResult)
{
  if (aIndex < 0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMHistory> history(do_QueryInterface(aNative));

  nsresult rv = history->Item(aIndex, aResult);
#ifdef DEBUG
  if (DOMStringIsNull(aResult)) {
    PRInt32 length = 0;
    history->GetLength(&length);
    NS_ASSERTION(aIndex >= length, "Item should only return null for out-of-bounds access");
  }
#endif
  return rv;
}


// MediaList helper

nsresult
nsMediaListSH::GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                           nsAString& aResult)
{
  if (aIndex < 0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMMediaList> media_list(do_QueryInterface(aNative));

  nsresult rv = media_list->Item(PRUint32(aIndex), aResult);
#ifdef DEBUG
  if (DOMStringIsNull(aResult)) {
    PRUint32 length = 0;
    media_list->GetLength(&length);
    NS_ASSERTION(PRUint32(aIndex) >= length, "Item should only return null for out-of-bounds access");
  }
#endif
  return rv;
}


// StyleSheetList helper

nsISupports*
nsStyleSheetListSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                              nsWrapperCache **aCache, nsresult *rv)
{
  nsDOMStyleSheetList* list = nsDOMStyleSheetList::FromSupports(aNative);

  return list->GetItemAt(aIndex);
}


// CSSValueList helper

nsISupports*
nsCSSValueListSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                            nsWrapperCache **aCache, nsresult *aResult)
{
  nsDOMCSSValueList* list = nsDOMCSSValueList::FromSupports(aNative);

  return list->GetItemAt(aIndex);
}


// CSSStyleDeclaration helper

NS_IMETHODIMP
nsCSSStyleDeclSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                            JSObject *globalObj, JSObject **parentObj)
{
  nsWrapperCache* cache = nsnull;
  CallQueryInterface(nativeObj, &cache);
  if (!cache) {
    return nsDOMClassInfo::PreCreate(nativeObj, cx, globalObj, parentObj);
  }

  nsICSSDeclaration *declaration = static_cast<nsICSSDeclaration*>(nativeObj);
  nsINode *native_parent = declaration->GetParentObject();
  if (!native_parent) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv =
    WrapNativeParent(cx, globalObj, native_parent, native_parent, parentObj);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_SUCCESS_ALLOW_SLIM_WRAPPERS;
}

nsresult
nsCSSStyleDeclSH::GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                              nsAString& aResult)
{
  if (aIndex < 0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMCSSStyleDeclaration> style_decl(do_QueryInterface(aNative));

  nsresult rv = style_decl->Item(PRUint32(aIndex), aResult);
#ifdef DEBUG
  if (DOMStringIsNull(aResult)) {
    PRUint32 length = 0;
    style_decl->GetLength(&length);
    NS_ASSERTION(PRUint32(aIndex) >= length, "Item should only return null for out-of-bounds access");
  }
#endif
  return rv;
}


// CSSRuleList scriptable helper

nsISupports*
nsCSSRuleListSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
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

// ClientRectList scriptable helper

nsISupports*
nsClientRectListSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                              nsWrapperCache **aCache, nsresult *aResult)
{
  nsClientRectList* list = nsClientRectList::FromSupports(aNative);

  return list->GetItemAt(aIndex);
}

// PaintRequestList scriptable helper

nsISupports*
nsPaintRequestListSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                nsWrapperCache **aCache, nsresult *aResult)
{
  nsPaintRequestList* list = nsPaintRequestList::FromSupports(aNative);

  return list->GetItemAt(aIndex);
}

nsISupports*
nsDOMTouchListSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                            nsWrapperCache **aCache, nsresult *aResult)
{
  nsDOMTouchList* list = static_cast<nsDOMTouchList*>(aNative);
  return list->GetItemAt(aIndex);
}

#ifdef MOZ_XUL
// TreeColumns helper

nsISupports*
nsTreeColumnsSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                           nsWrapperCache **aCache, nsresult *aResult)
{
  nsTreeColumns* columns = nsTreeColumns::FromSupports(aNative);

  return columns->GetColumnAt(aIndex);
}

nsISupports*
nsTreeColumnsSH::GetNamedItem(nsISupports *aNative,
                              const nsAString& aName,
                              nsWrapperCache **aCache,
                              nsresult *aResult)
{
  nsTreeColumns* columns = nsTreeColumns::FromSupports(aNative);

  return columns->GetNamedColumn(aName);
}
#endif


// Storage scriptable helper

// One reason we need a newResolve hook is that in order for
// enumeration of storage object keys to work the keys we're
// enumerating need to exist on the storage object for the JS engine
// to find them.

NS_IMETHODIMP
nsStorageSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval)
{
  if (ObjectIsNativeWrapper(cx, obj)) {
    return NS_OK;
  }

  JSObject *realObj;
  wrapper->GetJSObject(&realObj);

  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, realObj)) {
    *_retval = false;
    return NS_ERROR_FAILURE;
  }

  // First check to see if the property is defined on our prototype.

  JSObject *proto = ::JS_GetPrototype(realObj);
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

  nsCOMPtr<nsIDOMStorageObsolete> storage(do_QueryWrappedNative(wrapper));

  JSString *jsstr = IdToString(cx, id);
  if (!jsstr)
    return JS_FALSE;

  nsDependentJSString depStr;
  if (!depStr.init(cx, jsstr))
    return JS_FALSE;

  // GetItem() will return null if the caller can't access the session
  // storage item.
  nsCOMPtr<nsIDOMStorageItem> item;
  nsresult rv = storage->GetItem(depStr, getter_AddRefs(item));
  NS_ENSURE_SUCCESS(rv, rv);

  if (item) {
    if (!::JS_DefinePropertyById(cx, realObj, id, JSVAL_VOID, nsnull, nsnull,
                                 JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    *objp = realObj;
  }

  return NS_OK;
}

nsISupports*
nsStorageSH::GetNamedItem(nsISupports *aNative, const nsAString& aName,
                          nsWrapperCache **aCache, nsresult *aResult)
{
  nsDOMStorage* storage = nsDOMStorage::FromSupports(aNative);

  return storage->GetNamedItem(aName, aResult);
}

NS_IMETHODIMP
nsStorageSH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                         JSContext *cx, JSObject *obj, jsid id,
                         jsval *vp, bool *_retval)
{
  nsCOMPtr<nsIDOMStorageObsolete> storage(do_QueryWrappedNative(wrapper));
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
nsStorageSH::DelProperty(nsIXPConnectWrappedNative *wrapper,
                         JSContext *cx, JSObject *obj, jsid id,
                         jsval *vp, bool *_retval)
{
  nsCOMPtr<nsIDOMStorageObsolete> storage(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);

  JSString *key = IdToString(cx, id);
  NS_ENSURE_TRUE(key, NS_ERROR_UNEXPECTED);

  nsDependentJSString keyStr;
  NS_ENSURE_TRUE(keyStr.init(cx, key), NS_ERROR_UNEXPECTED);

  nsresult rv = storage->RemoveItem(keyStr);
  if (NS_SUCCEEDED(rv)) {
    rv = NS_SUCCESS_I_DID_SOMETHING;
  }

  return rv;
}


NS_IMETHODIMP
nsStorageSH::NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, PRUint32 enum_op, jsval *statep,
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


// Storage2SH

// One reason we need a newResolve hook is that in order for
// enumeration of storage object keys to work the keys we're
// enumerating need to exist on the storage object for the JS engine
// to find them.

NS_IMETHODIMP
nsStorage2SH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, PRUint32 flags,
                         JSObject **objp, bool *_retval)
{
  if (ObjectIsNativeWrapper(cx, obj)) {
    return NS_OK;
  }

  JSObject *realObj;
  wrapper->GetJSObject(&realObj);

  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, realObj)) {
    *_retval = false;
    return NS_ERROR_FAILURE;
  }

  // First check to see if the property is defined on our prototype,
  // after converting id to a string if it's an integer.

  JSString *jsstr = IdToString(cx, id);
  if (!jsstr) {
    return JS_FALSE;
  }

  JSObject *proto = ::JS_GetPrototype(realObj);
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
    if (!::JS_DefinePropertyById(cx, realObj, id, JSVAL_VOID, nsnull,
                                 nsnull, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    *objp = realObj;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsStorage2SH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, jsid id, jsval *vp, bool *_retval)
{
  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);

  nsAutoString val;
  nsresult rv = NS_OK;

  if (JSID_IS_STRING(id)) {
    // For native wrappers, do not get random names on storage objects.
    if (ObjectIsNativeWrapper(cx, obj)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    rv = storage->GetItem(nsDependentJSString(id), val);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    PRInt32 n = GetArrayIndexFromId(cx, id);
    NS_ENSURE_TRUE(n >= 0, NS_ERROR_NOT_AVAILABLE);

    rv = storage->Key(n, val);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  JSAutoRequest ar(cx);

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
                          JSContext *cx, JSObject *obj, jsid id,
                          jsval *vp, bool *_retval)
{
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
                          JSContext *cx, JSObject *obj, jsid id,
                          jsval *vp, bool *_retval)
{
  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);

  JSString *key = IdToString(cx, id);
  NS_ENSURE_TRUE(key, NS_ERROR_UNEXPECTED);

  nsDependentJSString keyStr;
  NS_ENSURE_TRUE(keyStr.init(cx, key), NS_ERROR_UNEXPECTED);

  nsresult rv = storage->RemoveItem(keyStr);
  if (NS_SUCCEEDED(rv)) {
    rv = NS_SUCCESS_I_DID_SOMETHING;
  }

  return rv;
}


NS_IMETHODIMP
nsStorage2SH::NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                           JSObject *obj, PRUint32 enum_op, jsval *statep,
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
                                             nsIInterfaceInfo *aInterfaceInfo,
                                             PRUint16 aMethodIndex,
                                             bool *aHideFirstParamFromJS,
                                             nsIID * *aIIDOfResult,
                                             nsISupports **_retval)
{
  *aHideFirstParamFromJS = false;
  *aIIDOfResult = nsnull;

  nsCOMPtr<nsIDOMEvent> event(do_QueryInterface(aInitialThis));
  NS_ENSURE_TRUE(event, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMEventTarget> target;
  event->GetCurrentTarget(getter_AddRefs(target));

  *_retval = target.forget().get();

  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsMutationCallbackThisTranslator)
  NS_INTERFACE_MAP_ENTRY(nsIXPCFunctionThisTranslator)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsMutationCallbackThisTranslator)
NS_IMPL_RELEASE(nsMutationCallbackThisTranslator)

NS_IMETHODIMP
nsMutationCallbackThisTranslator::TranslateThis(nsISupports *aInitialThis,
                                                nsIInterfaceInfo *aInterfaceInfo,
                                                PRUint16 aMethodIndex,
                                                bool *aHideFirstParamFromJS,
                                                nsIID * *aIIDOfResult,
                                                nsISupports **_retval)
{
  *aHideFirstParamFromJS = false;
  *aIIDOfResult = nsnull;
  NS_IF_ADDREF(*_retval = nsDOMMutationObserver::CurrentObserver());
  return NS_OK;
}

NS_IMETHODIMP
nsDOMConstructorSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                              JSObject *globalObj, JSObject **parentObj)
{
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
                               JSObject *obj, jsid id, PRUint32 flags,
                               JSObject **objp, bool *_retval)
{
  // For regular DOM constructors, we have our interface constants defined on
  // us by nsWindowSH::GlobalResolve. However, XrayWrappers can't see these
  // interface constants (as they look like expando properties) so we have to
  // specially resolve those constants here, but only for Xray wrappers.
  if (!ObjectIsNativeWrapper(cx, obj)) {
    return NS_OK;
  }

  JSObject *nativePropsObj = xpc::XrayUtils::GetNativePropertiesObject(cx, obj);
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
                         JSObject *obj, PRUint32 argc, jsval *argv, jsval *vp,
                         bool *_retval)
{
  nsDOMConstructor *wrapped =
    static_cast<nsDOMConstructor *>(wrapper->Native());

#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMDOMConstructor> is_constructor =
      do_QueryWrappedNative(wrapper);
    NS_ASSERTION(is_constructor, "How did we not get a constructor?");
  }
#endif

  return wrapped->Construct(wrapper, cx, obj, argc, argv, vp, _retval);
}

NS_IMETHODIMP
nsDOMConstructorSH::Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                              JSObject *obj, PRUint32 argc, jsval *argv,
                              jsval *vp, bool *_retval)
{
  nsDOMConstructor *wrapped =
    static_cast<nsDOMConstructor *>(wrapper->Native());

#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMDOMConstructor> is_constructor =
      do_QueryWrappedNative(wrapper);
    NS_ASSERTION(is_constructor, "How did we not get a constructor?");
  }
#endif

  return wrapped->Construct(wrapper, cx, obj, argc, argv, vp, _retval);
}

NS_IMETHODIMP
nsDOMConstructorSH::HasInstance(nsIXPConnectWrappedNative *wrapper,
                                JSContext *cx, JSObject *obj, const jsval &val,
                                bool *bp, bool *_retval)
{
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
nsNonDOMObjectSH::GetFlags(PRUint32 *aFlags)
{
  // This is NOT a DOM Object.  Use this helper class for cases when you need
  // to do something like implement nsISecurityCheckedComponent in a meaningful
  // way.
  *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
  return NS_OK;
}

NS_IMETHODIMP
nsAttributeSH::GetFlags(PRUint32 *aFlags)
{
  // Just like nsNodeSH, but without CONTENT_NODE
  *aFlags = DOMCLASSINFO_STANDARD_FLAGS;

  return NS_OK;
}

// nsOfflineResourceListSH
nsresult
nsOfflineResourceListSH::GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                                     nsAString& aResult)
{
  nsCOMPtr<nsIDOMOfflineResourceList> list(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(list, NS_ERROR_UNEXPECTED);

  nsresult rv = list->MozItem(aIndex, aResult);
#ifdef DEBUG
  if (DOMStringIsNull(aResult)) {
    PRUint32 length = 0;
    list->GetMozLength(&length);
    NS_ASSERTION(PRUint32(aIndex) >= length, "MozItem should only return null for out-of-bounds access");
  }
#endif
  return rv;
}

// nsFileListSH
nsISupports*
nsFileListSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                        nsWrapperCache **aCache, nsresult *aResult)
{
  nsDOMFileList* list = nsDOMFileList::FromSupports(aNative);

  return list->GetItemAt(aIndex);
}

// Template for SVGXXXList helpers
template<class ListInterfaceType, class ListType> nsISupports*
nsSVGListSH<ListInterfaceType, ListType>::GetItemAt(nsISupports *aNative,
                                                    PRUint32 aIndex,
                                                    nsWrapperCache **aCache,
                                                    nsresult *aResult)
{
  ListType* list = static_cast<ListType*>(static_cast<ListInterfaceType*>(aNative));
#ifdef DEBUG
  {
    nsCOMPtr<ListInterfaceType> list_qi = do_QueryInterface(aNative);

    // If this assertion fires the QI implementation for the object in
    // question doesn't use the nsIDOMSVGXXXList pointer as the nsISupports
    // pointer. That must be fixed, or we'll crash...
    NS_ABORT_IF_FALSE(list_qi == list, "Uh, fix QI!");
  }
#endif

  return list->GetItemAt(aIndex);
}


// SVGStringList helper

nsresult
nsSVGStringListSH::GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                               nsAString& aResult)
{
  if (aIndex < 0) {
    SetDOMStringToNull(aResult);
    return NS_OK;
  }

  DOMSVGStringList* list = static_cast<DOMSVGStringList*>(
                             static_cast<nsIDOMSVGStringList*>(aNative));
#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMSVGStringList> list_qi = do_QueryInterface(aNative);
    
    // If this assertion fires the QI implementation for the object in
    // question doesn't use the nsIDOMDOMSVGStringList pointer as the
    // nsISupports pointer. That must be fixed, or we'll crash...
    NS_ABORT_IF_FALSE(list_qi == list, "Uh, fix QI!");
  }
#endif

  nsresult rv = list->GetItem(aIndex, aResult);
#ifdef DEBUG
  if (DOMStringIsNull(aResult)) {
    PRUint32 length = 0;
    list->GetLength(&length);
    NS_ASSERTION(PRUint32(aIndex) >= length, "Item should only return null for out-of-bounds access");
  }
#endif
  if (rv == NS_ERROR_DOM_INDEX_SIZE_ERR) {
    SetDOMStringToNull(aResult);
    rv = NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
WebGLExtensionSH::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                              JSObject *obj, jsid id, jsval *vp, bool *_retval)
{
  WebGLExtensionSH::PreserveWrapper(GetNative(wrapper, obj));

  return NS_OK;
}

void
WebGLExtensionSH::PreserveWrapper(nsISupports *aNative)
{
  WebGLExtension* ext = static_cast<WebGLExtension*>(aNative);
  nsContentUtils::PreserveWrapper(aNative, ext);
}

nsresult
WebGLExtensionSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                            JSObject *globalObj, JSObject **parentObj)
{
  *parentObj = globalObj;

  WebGLExtension *ext = static_cast<WebGLExtension*>(nativeObj);
  WebGLContext *webgl = ext->Context();
  nsHTMLCanvasElement *canvas = webgl->HTMLCanvasElement();
  nsINode *node = static_cast<nsINode*>(canvas);

  return WrapNativeParent(cx, globalObj, node, node, parentObj);
}
