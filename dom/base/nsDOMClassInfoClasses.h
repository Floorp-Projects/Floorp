/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

DOMCI_CLASS(Window)
DOMCI_CLASS(Location)
DOMCI_CLASS(History)
DOMCI_CLASS(DOMPrototype)
DOMCI_CLASS(DOMConstructor)

// Core classes
DOMCI_CLASS(DOMException)

DOMCI_CLASS(DeviceAcceleration)
DOMCI_CLASS(DeviceRotationRate)

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
DOMCI_CLASS(Selection)

// XUL classes
#ifdef MOZ_XUL
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

// DOM Chrome Window class, almost identical to Window
DOMCI_CLASS(ChromeWindow)

#ifdef MOZ_XUL
DOMCI_CLASS(XULTemplateBuilder)
DOMCI_CLASS(XULTreeBuilder)
#endif

// DOMStringList object
DOMCI_CLASS(DOMStringList)

#ifdef MOZ_XUL
DOMCI_CLASS(TreeColumn)
#endif

DOMCI_CLASS(CSSMozDocumentRule)
DOMCI_CLASS(CSSSupportsRule)

// other SVG classes
DOMCI_CLASS(SVGLength)
DOMCI_CLASS(SVGNumber)

// Canvas
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
DOMCI_CLASS(Storage)

DOMCI_CLASS(Blob)
DOMCI_CLASS(File)

// DOM modal content window class, almost identical to Window
DOMCI_CLASS(ModalContentWindow)

DOMCI_CLASS(MozPowerManager)
DOMCI_CLASS(MozWakeLock)

DOMCI_CLASS(MozMobileMessageManager)
DOMCI_CLASS(MozSmsMessage)
DOMCI_CLASS(MozMmsMessage)
DOMCI_CLASS(MozSmsFilter)
DOMCI_CLASS(MozSmsSegmentInfo)
DOMCI_CLASS(MozMobileMessageThread)

DOMCI_CLASS(MozConnection)
#ifdef MOZ_B2G_RIL
DOMCI_CLASS(MozMobileConnection)
DOMCI_CLASS(MozCellBroadcast)
#endif

// @font-face in CSS
DOMCI_CLASS(CSSFontFaceRule)

DOMCI_CLASS(DataTransfer)

DOMCI_CLASS(EventListenerInfo)

DOMCI_CLASS(ContentFrameMessageManager)
DOMCI_CLASS(ChromeMessageBroadcaster)
DOMCI_CLASS(ChromeMessageSender)

DOMCI_CLASS(MozCSSKeyframeRule)
DOMCI_CLASS(MozCSSKeyframesRule)

DOMCI_CLASS(CSSPageRule)

DOMCI_CLASS(MediaQueryList)

#ifdef MOZ_B2G_RIL
DOMCI_CLASS(Telephony)
DOMCI_CLASS(TelephonyCall)
DOMCI_CLASS(MozVoicemail)
DOMCI_CLASS(MozIccManager)
#endif

#ifdef MOZ_B2G_FM
DOMCI_CLASS(FMRadio)
#endif

#ifdef MOZ_B2G_BT
DOMCI_CLASS(BluetoothManager)
DOMCI_CLASS(BluetoothAdapter)
DOMCI_CLASS(BluetoothDevice)
#endif

DOMCI_CLASS(CameraControl)
DOMCI_CLASS(CameraCapabilities)

DOMCI_CLASS(OpenWindowEventDetail)
DOMCI_CLASS(AsyncScrollEventDetail)

DOMCI_CLASS(LockedFile)

DOMCI_CLASS(CSSFontFeatureValuesRule)

DOMCI_CLASS(UserDataHandler)
DOMCI_CLASS(GeoPositionError)
DOMCI_CLASS(LoadStatus)
DOMCI_CLASS(XPathNamespace)
DOMCI_CLASS(XULButtonElement)
DOMCI_CLASS(XULCheckboxElement)
DOMCI_CLASS(XULPopupElement)
