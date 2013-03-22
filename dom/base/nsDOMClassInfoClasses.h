/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

DOMCI_CLASS(Window)
DOMCI_CLASS(Location)
DOMCI_CLASS(Navigator)
DOMCI_CLASS(Plugin)
DOMCI_CLASS(PluginArray)
DOMCI_CLASS(MimeType)
DOMCI_CLASS(MimeTypeArray)
DOMCI_CLASS(BarProp)
DOMCI_CLASS(History)
DOMCI_CLASS(DOMPrototype)
DOMCI_CLASS(DOMConstructor)

// Core classes
DOMCI_CLASS(XMLDocument)
DOMCI_CLASS(DOMException)
DOMCI_CLASS(Element)
DOMCI_CLASS(Attr)
DOMCI_CLASS(MozNamedAttrMap)

// Event classes
DOMCI_CLASS(Event)
DOMCI_CLASS(MutationEvent)
DOMCI_CLASS(UIEvent)
DOMCI_CLASS(MouseEvent)
DOMCI_CLASS(MouseScrollEvent)
DOMCI_CLASS(WheelEvent)
DOMCI_CLASS(DragEvent)
DOMCI_CLASS(ClipboardEvent)
DOMCI_CLASS(KeyboardEvent)
DOMCI_CLASS(CompositionEvent)
#define MOZ_GENERATED_EVENT_LIST
#define MOZ_GENERATED_EVENT(_event_interface) DOMCI_CLASS(_event_interface)
#include "GeneratedEvents.h"
#undef MOZ_GENERATED_EVENT_LIST
DOMCI_CLASS(DeviceMotionEvent)
DOMCI_CLASS(DeviceAcceleration)
DOMCI_CLASS(DeviceRotationRate)

// HTML classes
DOMCI_CLASS(HTMLDocument)

// HTML element classes
DOMCI_CLASS(HTMLAppletElement)
DOMCI_CLASS(HTMLEmbedElement)
DOMCI_CLASS(HTMLFormElement)
DOMCI_CLASS(HTMLIFrameElement)
DOMCI_CLASS(HTMLInputElement)
DOMCI_CLASS(HTMLObjectElement)
DOMCI_CLASS(HTMLSelectElement)
DOMCI_CLASS(ValidityState)

// CSS classes
DOMCI_CLASS(CSSStyleRule)
DOMCI_CLASS(CSSCharsetRule)
DOMCI_CLASS(CSSImportRule)
DOMCI_CLASS(CSSMediaRule)
DOMCI_CLASS(CSSNameSpaceRule)
DOMCI_CLASS(CSSRuleList)
DOMCI_CLASS(CSSGroupRuleRuleList)
DOMCI_CLASS(MediaList)
DOMCI_CLASS(StyleSheetList)
DOMCI_CLASS(CSSStyleSheet)

// Range classes
DOMCI_CLASS(Range)
DOMCI_CLASS(Selection)

// XUL classes
#ifdef MOZ_XUL
DOMCI_CLASS(XULDocument)
DOMCI_CLASS(XULCommandDispatcher)
#endif
DOMCI_CLASS(XULControllers)
DOMCI_CLASS(BoxObject)
#ifdef MOZ_XUL
DOMCI_CLASS(TreeSelection)
DOMCI_CLASS(TreeContentView)
#endif

// Crypto classes
#ifndef MOZ_DISABLE_CRYPTOLEGACY
DOMCI_CLASS(CRMFObject)
#endif
DOMCI_CLASS(Crypto)

// DOM Traversal classes
DOMCI_CLASS(TreeWalker)

// Rect object used by getComputedStyle
DOMCI_CLASS(CSSRect)

// DOM Chrome Window class, almost identical to Window
DOMCI_CLASS(ChromeWindow)

DOMCI_CLASS(ImageDocument)

#ifdef MOZ_XUL
DOMCI_CLASS(XULTemplateBuilder)
DOMCI_CLASS(XULTreeBuilder)
#endif

// DOMStringList object
DOMCI_CLASS(DOMStringList)

#ifdef MOZ_XUL
DOMCI_CLASS(TreeColumn)
DOMCI_CLASS(TreeColumns)
#endif

DOMCI_CLASS(CSSMozDocumentRule)
DOMCI_CLASS(CSSSupportsRule)

DOMCI_CLASS(BeforeUnloadEvent)

// The SVG document
DOMCI_CLASS(SVGDocument)

// SVG element classes
DOMCI_CLASS(TimeEvent)
DOMCI_CLASS(SVGFEConvolveMatrixElement)
DOMCI_CLASS(SVGFEDiffuseLightingElement)
DOMCI_CLASS(SVGFEDisplacementMapElement)
DOMCI_CLASS(SVGFEMorphologyElement)
DOMCI_CLASS(SVGFEOffsetElement)
DOMCI_CLASS(SVGFESpecularLightingElement)
DOMCI_CLASS(SVGFESpotLightElement)
DOMCI_CLASS(SVGFETurbulenceElement)
DOMCI_CLASS(SVGUnknownElement)

// other SVG classes
DOMCI_CLASS(SVGAnimatedEnumeration)
DOMCI_CLASS(SVGAnimatedInteger)
DOMCI_CLASS(SVGAnimatedNumber)
DOMCI_CLASS(SVGAnimatedRect)
DOMCI_CLASS(SVGAnimatedString)
DOMCI_CLASS(SVGLength)
DOMCI_CLASS(SVGNumber)
DOMCI_CLASS(SVGRect)
DOMCI_CLASS(SVGStringList)
DOMCI_CLASS(SVGZoomEvent)

// Canvas
DOMCI_CLASS(HTMLCanvasElement)
DOMCI_CLASS(CanvasGradient)
DOMCI_CLASS(CanvasPattern)
DOMCI_CLASS(TextMetrics)
DOMCI_CLASS(MozCanvasPrintState)

// WindowUtils
DOMCI_CLASS(WindowUtils)

// XSLTProcessor
DOMCI_CLASS(XSLTProcessor)

// DOM Level 3 XPath objects
DOMCI_CLASS(XPathExpression)
DOMCI_CLASS(XPathNSResolver)
DOMCI_CLASS(XPathResult)

// WhatWG WebApps Objects
DOMCI_CLASS(StorageObsolete)
DOMCI_CLASS(Storage)
DOMCI_CLASS(StorageItem)

DOMCI_CLASS(XULCommandEvent)
DOMCI_CLASS(CommandEvent)
DOMCI_CLASS(OfflineResourceList)

DOMCI_CLASS(Blob)
DOMCI_CLASS(File)
DOMCI_CLASS(FileReader)

// DOM modal content window class, almost identical to Window
DOMCI_CLASS(ModalContentWindow)

// Data Events
DOMCI_CLASS(DataContainerEvent)

// event used for cross-domain message-passing and for server-sent events in
// HTML5
DOMCI_CLASS(MessageEvent)

DOMCI_CLASS(DeviceStorage)

// Geolocation
DOMCI_CLASS(GeoGeolocation)
DOMCI_CLASS(GeoPosition)
DOMCI_CLASS(GeoPositionCoords)
DOMCI_CLASS(GeoPositionError)

DOMCI_CLASS(MozPowerManager)
DOMCI_CLASS(MozWakeLock)

DOMCI_CLASS(MozSmsManager)
DOMCI_CLASS(MozMobileMessageManager)
DOMCI_CLASS(MozSmsMessage)
DOMCI_CLASS(MozMmsMessage)
DOMCI_CLASS(MozSmsRequest)
DOMCI_CLASS(MozSmsFilter)
DOMCI_CLASS(MozSmsCursor)
DOMCI_CLASS(MozSmsSegmentInfo)

DOMCI_CLASS(MozConnection)
#ifdef MOZ_B2G_RIL
DOMCI_CLASS(MozMobileConnection)
DOMCI_CLASS(MozCellBroadcast)
#endif

// @font-face in CSS
DOMCI_CLASS(CSSFontFaceRule)

#if defined(MOZ_MEDIA)
// WhatWG Video Element
DOMCI_CLASS(HTMLVideoElement)
DOMCI_CLASS(HTMLAudioElement)
DOMCI_CLASS(TimeRanges)
#endif

// DOM Traversal NodeIterator class
DOMCI_CLASS(NodeIterator)

DOMCI_CLASS(DataTransfer)

DOMCI_CLASS(NotifyPaintEvent)

DOMCI_CLASS(NotifyAudioAvailableEvent)

DOMCI_CLASS(SimpleGestureEvent)

DOMCI_CLASS(ScrollAreaEvent)

DOMCI_CLASS(EventListenerInfo)

DOMCI_CLASS(TransitionEvent)
DOMCI_CLASS(AnimationEvent)

DOMCI_CLASS(ContentFrameMessageManager)
DOMCI_CLASS(ChromeMessageBroadcaster)
DOMCI_CLASS(ChromeMessageSender)

DOMCI_CLASS(DesktopNotification)
DOMCI_CLASS(DesktopNotificationCenter)

DOMCI_CLASS(IDBFactory)
DOMCI_CLASS(IDBFileHandle)
DOMCI_CLASS(IDBRequest)
DOMCI_CLASS(IDBDatabase)
DOMCI_CLASS(IDBObjectStore)
DOMCI_CLASS(IDBTransaction)
DOMCI_CLASS(IDBCursor)
DOMCI_CLASS(IDBCursorWithValue)
DOMCI_CLASS(IDBKeyRange)
DOMCI_CLASS(IDBIndex)
DOMCI_CLASS(IDBVersionChangeEvent)
DOMCI_CLASS(IDBOpenDBRequest)

DOMCI_CLASS(Touch)
DOMCI_CLASS(TouchList)
DOMCI_CLASS(TouchEvent)

#ifdef MOZ_GAMEPAD
DOMCI_CLASS(Gamepad)
#endif

DOMCI_CLASS(MozCSSKeyframeRule)
DOMCI_CLASS(MozCSSKeyframesRule)

DOMCI_CLASS(CSSPageRule)

DOMCI_CLASS(MediaQueryList)

#ifdef MOZ_B2G_RIL
DOMCI_CLASS(Telephony)
DOMCI_CLASS(TelephonyCall)
DOMCI_CLASS(MozVoicemail)
DOMCI_CLASS(MozIccManager)
DOMCI_CLASS(MozStkCommandEvent)
#endif

#ifdef MOZ_B2G_FM
DOMCI_CLASS(FMRadio)
#endif

#ifdef MOZ_B2G_BT
DOMCI_CLASS(BluetoothManager)
DOMCI_CLASS(BluetoothAdapter)
DOMCI_CLASS(BluetoothDevice)
#endif

DOMCI_CLASS(CameraManager)
DOMCI_CLASS(CameraControl)
DOMCI_CLASS(CameraCapabilities)

DOMCI_CLASS(DOMError)
DOMCI_CLASS(DOMRequest)
DOMCI_CLASS(DOMCursor)
DOMCI_CLASS(OpenWindowEventDetail)
DOMCI_CLASS(AsyncScrollEventDetail)

DOMCI_CLASS(LockedFile)

#ifdef MOZ_TIME_MANAGER
DOMCI_CLASS(MozTimeManager)
#endif

#ifdef MOZ_WEBRTC
DOMCI_CLASS(DataChannel)
DOMCI_CLASS(RTCPeerConnection)
#endif
