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

// JavaScript includes
#include "jsapi.h"
#include "jsnum.h"
#include "jsdbgapi.h"
#include "jscntxt.h"

// General helper includes
#include "nsGlobalWindow.h"
#include "nsIContent.h"
#include "nsIAttribute.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOM3Document.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMEventListener.h"
#include "nsContentUtils.h"
#include "nsDOMWindowUtils.h"

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

// DOM base includes
#include "nsIDOMPluginArray.h"
#include "nsIDOMPlugin.h"
#include "nsIDOMMimeTypeArray.h"
#include "nsIDOMMimeType.h"
#include "nsIDOMNSLocation.h"
#include "nsIDOMLocation.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMJSWindow.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMHistory.h"
#include "nsIDOMNSHistory.h"
#include "nsIDOMMediaList.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMConstructor.h"

// DOM core includes
#include "nsDOMError.h"
#include "nsIDOMDOMException.h"
#include "nsIDOMNode.h"
#include "nsIDOM3Node.h"
#include "nsIDOM3Attr.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMDOMStringList.h"
#include "nsIDOMNameList.h"
#include "nsIDOMNSElement.h"

// HTMLFormElement helper includes
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNSHTMLFormControlList.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIHTMLDocument.h"

// HTMLSelectElement helper includes
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMNSXBLFormControl.h"

// HTMLEmbed/ObjectElement helper includes
#include "nsIPluginInstance.h"
#include "nsIPluginInstanceInternal.h"
#include "nsIObjectFrame.h"
#include "nsIObjectLoadingContent.h"
#include "nsIScriptablePlugin.h"
#include "nsIPluginHost.h"
#include "nsPIPluginHost.h"

#ifdef OJI
// HTMLAppletElement helper includes
#include "nsIJVMManager.h"

// Oh, did I mention that I hate Microsoft for doing this to me?
#ifndef WINCE
#undef GetClassName
#endif

#include "nsILiveConnectManager.h"
#include "nsIJVMPluginInstance.h"
#endif

// HTMLOptionsCollection includes
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMNSHTMLOptionElement.h"
#include "nsIDOMHTMLOptionsCollection.h"
#include "nsIDOMNSHTMLOptionCollectn.h"
#include "nsIDOMHTMLOptionsCollection.h"

// ContentList includes
#include "nsContentList.h"

// Event related includes
#include "nsIEventListenerManager.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNSEventTarget.h"

// CSS related includes
#include "nsIDOMStyleSheet.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSSRule.h"
#include "nsIDOMCSSRuleList.h"
#include "nsIDOMRect.h"
#include "nsIDOMRGBColor.h"
#include "nsIDOMNSRGBAColor.h"

// XBL related includes.
#include "nsIXBLService.h"
#include "nsXBLBinding.h"
#include "nsBindingManager.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMElement.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIScriptGlobalObject.h"
#include "nsStyleSet.h"
#include "nsStyleContext.h"
#include "nsAutoPtr.h"
#include "nsMemory.h"

// Tranformiix
#include "nsIDOMXPathEvaluator.h"
#include "nsIXSLTProcessor.h"
#include "nsIXSLTProcessorObsolete.h"
#include "nsIXSLTProcessorPrivate.h"

#include "nsIDOMLSProgressEvent.h"
#include "nsIDOMParser.h"
#include "nsIDOMSerializer.h"
#include "nsIXMLHttpRequest.h"

// includes needed for the prototype chain interfaces
#include "nsIDOMNavigator.h"
#include "nsIDOMBarProp.h"
#include "nsIDOMScreen.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMAttr.h"
#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMNotation.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMCommandEvent.h"
#include "nsIDOMPopupBlockedEvent.h"
#include "nsIDOMBeforeUnloadEvent.h"
#include "nsIDOMMutationEvent.h"
#include "nsIDOMSmartCardEvent.h"
#include "nsIDOMXULCommandEvent.h"
#include "nsIDOMPageTransitionEvent.h"
#include "nsIDOMNSDocumentStyle.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMLinkStyle.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMNSHTMLAnchorElement2.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMNSHTMLAreaElement2.h"
#include "nsIDOMHTMLBRElement.h"
#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMHTMLBaseFontElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMNSHTMLButtonElement.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsIDOMHTMLDListElement.h"
#include "nsIDOMHTMLDirectoryElement.h"
#include "nsIDOMHTMLDivElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMHTMLFieldSetElement.h"
#include "nsIDOMHTMLFontElement.h"
#include "nsIDOMNSHTMLFormElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLFrameSetElement.h"
#include "nsIDOMNSHTMLFrameElement.h"
#include "nsIDOMHTMLHRElement.h"
#include "nsIDOMNSHTMLHRElement.h"
#include "nsIDOMHTMLHeadElement.h"
#include "nsIDOMHTMLHeadingElement.h"
#include "nsIDOMHTMLHtmlElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMNSHTMLImageElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMHTMLIsIndexElement.h"
#include "nsIDOMHTMLLIElement.h"
#include "nsIDOMHTMLLabelElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLMenuElement.h"
#include "nsIDOMHTMLMetaElement.h"
#include "nsIDOMHTMLModElement.h"
#include "nsIDOMHTMLOListElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLParagraphElement.h"
#include "nsIDOMHTMLParamElement.h"
#include "nsIDOMHTMLPreElement.h"
#include "nsIDOMHTMLQuoteElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMNSHTMLSelectElement.h"
#include "nsIDOMHTMLStyleElement.h"
#include "nsIDOMHTMLTableCaptionElem.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLTableColElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMHTMLTitleElement.h"
#include "nsIDOMHTMLUListElement.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMCSS2Properties.h"
#include "nsIDOMCSSCharsetRule.h"
#include "nsIDOMCSSImportRule.h"
#include "nsIDOMCSSMediaRule.h"
#include "nsIDOMCSSMozDocumentRule.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIDOMCSSStyleRule.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMCSSValueList.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsIDOMRangeException.h"
#include "nsIDOMTreeWalker.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMCrypto.h"
#include "nsIDOMCRMFObject.h"
#include "nsIDOMPkcs11.h"
#include "nsIControllers.h"
#include "nsISelection.h"
#include "nsIBoxObject.h"
#ifdef MOZ_XUL
#include "nsITreeSelection.h"
#include "nsITreeContentView.h"
#include "nsITreeView.h"
#include "nsIXULTemplateBuilder.h"
#include "nsITreeColumns.h"
#endif
#include "nsIDOMXPathException.h"
#include "nsIDOMXPathExpression.h"
#include "nsIDOMNSXPathExpression.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIDOMXPathResult.h"

#ifdef MOZ_SVG
#include "nsIDOMGetSVGDocument.h"
#include "nsIDOMSVGAElement.h"
#include "nsIDOMSVGAngle.h"
#include "nsIDOMSVGAnimatedAngle.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsIDOMSVGAnimatedInteger.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGAnimatedLengthList.h"
#include "nsIDOMSVGAnimatedNumber.h"
#include "nsIDOMSVGAnimatedNumberList.h"
#include "nsIDOMSVGAnimatedPathData.h"
#include "nsIDOMSVGAnimatedPoints.h"
#include "nsIDOMSVGAnimPresAspRatio.h"
#include "nsIDOMSVGAnimatedRect.h"
#include "nsIDOMSVGAnimatedString.h"
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
#ifdef MOZ_SVG_FOREIGNOBJECT
#include "nsIDOMSVGForeignObjectElem.h"
#endif
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
#include "nsIDOMSVGTextElement.h"
#include "nsIDOMSVGTextPathElement.h"
#include "nsIDOMSVGTitleElement.h"
#include "nsIDOMSVGTransform.h"
#include "nsIDOMSVGTransformable.h"
#include "nsIDOMSVGTransformList.h"
#include "nsIDOMSVGTSpanElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsIDOMSVGUseElement.h"
#include "nsIDOMSVGZoomAndPan.h"
#include "nsIDOMSVGZoomEvent.h"
#endif // MOZ_SVG

#ifdef MOZ_ENABLE_CANVAS
#include "nsIDOMCanvasRenderingContext2D.h"
#endif

#include "nsIImageDocument.h"

// Storage includes
#include "nsIDOMStorage.h"
#include "nsPIDOMStorage.h"
#include "nsIDOMStorageList.h"
#include "nsIDOMStorageItem.h"
#include "nsIDOMStorageEvent.h"
#include "nsIDOMToString.h"

static NS_DEFINE_CID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_CID(kDOMSOF_CID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

static const char kDOMStringBundleURL[] =
  "chrome://global/locale/dom/dom.properties";

// NOTE: DEFAULT_SCRIPTABLE_FLAGS and DOM_DEFAULT_SCRIPTABLE_FLAGS
//       are defined in nsIDOMClassInfo.h.

#define WINDOW_SCRIPTABLE_FLAGS                                               \
 (nsIXPCScriptable::WANT_GETPROPERTY |                                        \
  nsIXPCScriptable::WANT_SETPROPERTY |                                        \
  nsIXPCScriptable::WANT_PRECREATE |                                          \
  nsIXPCScriptable::WANT_ADDPROPERTY |                                        \
  nsIXPCScriptable::WANT_DELPROPERTY |                                        \
  nsIXPCScriptable::WANT_NEWENUMERATE |                                       \
  nsIXPCScriptable::WANT_EQUALITY |                                           \
  nsIXPCScriptable::WANT_OUTER_OBJECT |                                       \
  nsIXPCScriptable::WANT_INNER_OBJECT |                                       \
  nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE)

#define NODE_SCRIPTABLE_FLAGS                                                 \
 ((DOM_DEFAULT_SCRIPTABLE_FLAGS |                                             \
   nsIXPCScriptable::WANT_GETPROPERTY |                                       \
   nsIXPCScriptable::WANT_PRECREATE |                                         \
   nsIXPCScriptable::WANT_ADDPROPERTY |                                       \
   nsIXPCScriptable::WANT_SETPROPERTY) &                                      \
  ~nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY)

// We need to let JavaScript QI elements to interfaces that are not in
// the classinfo since XBL can be used to dynamically implement new
// unknown interfaces on elements, accessibility relies on this being
// possible.

#define ELEMENT_SCRIPTABLE_FLAGS                                              \
  (NODE_SCRIPTABLE_FLAGS & ~nsIXPCScriptable::CLASSINFO_INTERFACES_ONLY)

#define FRAME_ELEMENT_SCRIPTABLE_FLAGS                                        \
  (ELEMENT_SCRIPTABLE_FLAGS |                                                 \
   nsIXPCScriptable::WANT_DELPROPERTY)

#define EXTERNAL_OBJ_SCRIPTABLE_FLAGS                                         \
  (ELEMENT_SCRIPTABLE_FLAGS & ~nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY | \
   nsIXPCScriptable::WANT_GETPROPERTY |                                       \
   nsIXPCScriptable::WANT_SETPROPERTY |                                       \
   nsIXPCScriptable::WANT_CALL)

#define DOCUMENT_SCRIPTABLE_FLAGS                                             \
  (NODE_SCRIPTABLE_FLAGS |                                                    \
   nsIXPCScriptable::WANT_ADDPROPERTY |                                       \
   nsIXPCScriptable::WANT_DELPROPERTY |                                       \
   nsIXPCScriptable::WANT_GETPROPERTY |                                       \
   nsIXPCScriptable::WANT_POSTCREATE)

#define ARRAY_SCRIPTABLE_FLAGS                                                \
  (DOM_DEFAULT_SCRIPTABLE_FLAGS       |                                       \
   nsIXPCScriptable::WANT_GETPROPERTY |                                       \
   nsIXPCScriptable::WANT_ENUMERATE)

#define DOMCLASSINFO_STANDARD_FLAGS                                           \
  (nsIClassInfo::MAIN_THREAD_ONLY | nsIClassInfo::DOM_OBJECT)


#ifdef NS_DEBUG
#define NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                \
    eDOMClassInfo_##_class##_id,
#else
#define NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                \
  // nothing
#endif


#define NS_DEFINE_CLASSINFO_DATA_WITH_NAME(_class, _name, _helper,            \
                                           _flags)                            \
  { #_name,                                                                   \
    { _helper::doCreate },                                                    \
    nsnull,                                                                   \
    nsnull,                                                                   \
    nsnull,                                                                   \
    _flags,                                                                   \
    PR_TRUE,                                                                  \
    NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                    \
  },

#define NS_DEFINE_CLASSINFO_DATA(_class, _helper, _flags)                     \
  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(_class, _class, _helper, _flags)


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

  // Don't allow modifications to Location.prototype
  NS_DEFINE_CLASSINFO_DATA(Location, nsLocationSH,
                           (DOM_DEFAULT_SCRIPTABLE_FLAGS |
                            nsIXPCScriptable::WANT_PRECREATE) &
                           ~nsIXPCScriptable::ALLOW_PROP_MODS_TO_PROTOTYPE)

  NS_DEFINE_CLASSINFO_DATA(Navigator, nsNavigatorSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_PRECREATE)
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
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Screen, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Constructor, nsDOMConstructorSH,
                           (DEFAULT_SCRIPTABLE_FLAGS |
                            nsIXPCScriptable::WANT_CONSTRUCT |
                            nsIXPCScriptable::WANT_HASINSTANCE) &
                           ~(nsIXPCScriptable::WANT_PRECREATE |
                             nsIXPCScriptable::WANT_POSTCREATE |
                             nsIXPCScriptable::WANT_CHECKACCESS |
                             nsIXPCScriptable::WANT_NEWRESOLVE))

  // Core classes
  NS_DEFINE_CLASSINFO_DATA(XMLDocument, nsDocumentSH,
                           DOCUMENT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_ENUMERATE)
  NS_DEFINE_CLASSINFO_DATA(DocumentType, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DOMImplementation, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DOMException, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(DocumentFragment, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Element, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Attr, nsAttributeSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Text, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Comment, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CDATASection, nsNodeSH, NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(ProcessingInstruction, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Notation, nsNodeSH, NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(NodeList, nsArraySH, ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(NamedNodeMap, nsNamedNodeMapSH,
                           ARRAY_SCRIPTABLE_FLAGS)

  // Misc Core related classes

  // StyleSheet classes
  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(DocumentStyleSheetList, StyleSheetList,
                                     nsStyleSheetListSH,
                                     ARRAY_SCRIPTABLE_FLAGS)

  // Event
  NS_DEFINE_CLASSINFO_DATA(Event, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MutationEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(UIEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MouseEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(KeyboardEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(PopupBlockedEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // Misc HTML classes
  NS_DEFINE_CLASSINFO_DATA(HTMLDocument, nsHTMLDocumentSH,
                           DOCUMENT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_ENUMERATE)
  NS_DEFINE_CLASSINFO_DATA(HTMLOptionsCollection,
                           nsHTMLOptionsCollectionSH,
                           ARRAY_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_SETPROPERTY)
  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(HTMLFormControlCollection, HTMLCollection,
                                     nsFormControlListSH,
                                     ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(HTMLGenericCollection, HTMLCollection,
                                     nsHTMLCollectionSH,
                                     ARRAY_SCRIPTABLE_FLAGS)

  // HTML element classes
  NS_DEFINE_CLASSINFO_DATA(HTMLAnchorElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLAppletElement, nsHTMLAppletElementSH,
                           EXTERNAL_OBJ_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLAreaElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLBRElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLBaseElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLBaseFontElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLBodyElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLButtonElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLDListElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLDelElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLDirectoryElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLDivElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLEmbedElement, nsHTMLPluginObjElementSH,
                           EXTERNAL_OBJ_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLFieldSetElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLFontElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLFormElement, nsHTMLFormElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_GETPROPERTY |
                           nsIXPCScriptable::WANT_NEWENUMERATE)
  NS_DEFINE_CLASSINFO_DATA(HTMLFrameElement, nsHTMLFrameElementSH,
                           FRAME_ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLFrameSetElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLHRElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLHeadElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLHeadingElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLHtmlElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLIFrameElement, nsHTMLFrameElementSH,
                           FRAME_ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLImageElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLInputElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLInsElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLIsIndexElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLLIElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLLabelElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLLegendElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLLinkElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLMapElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLMenuElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLMetaElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLOListElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLObjectElement, nsHTMLPluginObjElementSH,
                           EXTERNAL_OBJ_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLOptGroupElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLOptionElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLParagraphElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLParamElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLPreElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLQuoteElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLScriptElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLSelectElement, nsHTMLSelectElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_GETPROPERTY)
  NS_DEFINE_CLASSINFO_DATA(HTMLSpacerElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLSpanElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLStyleElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTableCaptionElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTableCellElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTableColElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTableElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTableRowElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTableSectionElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTextAreaElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLTitleElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLUListElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLUnknownElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(HTMLWBRElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)

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
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(StyleSheetList, nsDOMGenericSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSStyleSheet, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSStyleDeclaration, nsCSSStyleDeclSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(ComputedCSSStyleDeclaration, nsCSSStyleDeclSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
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
                           DOCUMENT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_ENUMERATE)
  NS_DEFINE_CLASSINFO_DATA(XULElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(XULCommandDispatcher, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif
  NS_DEFINE_CLASSINFO_DATA(XULControllers, nsNonDOMObjectSH,
                           DEFAULT_SCRIPTABLE_FLAGS)
#ifdef MOZ_XUL
  NS_DEFINE_CLASSINFO_DATA(BoxObject, nsDOMGenericSH,
                           DEFAULT_SCRIPTABLE_FLAGS)
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
  NS_DEFINE_CLASSINFO_DATA(Pkcs11, nsDOMGenericSH,
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

  NS_DEFINE_CLASSINFO_DATA(RangeException, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(CSSValueList, nsCSSValueListSH,
                           ARRAY_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(ContentList, HTMLCollection,
                                     nsContentListSH,
                                     ARRAY_SCRIPTABLE_FLAGS |
                                     nsIXPCScriptable::WANT_PRECREATE)

  NS_DEFINE_CLASSINFO_DATA(XMLStylesheetProcessingInstruction, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(ImageDocument, nsHTMLDocumentSH,
                           DOCUMENT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_ENUMERATE)

#ifdef MOZ_XUL
  NS_DEFINE_CLASSINFO_DATA(XULTemplateBuilder, nsDOMGenericSH,
                           DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XULTreeBuilder, nsDOMGenericSH,
                           DEFAULT_SCRIPTABLE_FLAGS)
#endif

  NS_DEFINE_CLASSINFO_DATA(DOMStringList, nsStringListSH,
                           ARRAY_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(NameList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

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

#ifdef MOZ_SVG
  // SVG document
  NS_DEFINE_CLASSINFO_DATA(SVGDocument, nsDocumentSH,
                           DOCUMENT_SCRIPTABLE_FLAGS)

  // SVG element classes
  NS_DEFINE_CLASSINFO_DATA(SVGAElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
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
  NS_DEFINE_CLASSINFO_DATA(SVGFEMergeElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEMergeNodeElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEMorphologyElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEOffsetElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFETurbulenceElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGFEUnimplementedMOZElement, nsElementSH,
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
  NS_DEFINE_CLASSINFO_DATA(SVGUseElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)

  // other SVG classes
  NS_DEFINE_CLASSINFO_DATA(SVGAngle, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedAngle, nsDOMGenericSH,
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
  NS_DEFINE_CLASSINFO_DATA(SVGLengthList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGMatrix, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGNumber, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGNumberList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)    
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
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegMovetoAbs, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathSegMovetoRel, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPoint, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPointList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPreserveAspectRatio, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGRect, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGTransform, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGTransformList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGZoomEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif // MOZ_SVG

  NS_DEFINE_CLASSINFO_DATA(HTMLCanvasElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
#ifdef MOZ_ENABLE_CANVAS
  NS_DEFINE_CLASSINFO_DATA(CanvasRenderingContext2D, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CanvasGradient, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CanvasPattern, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif // MOZ_ENABLE_CANVAS

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
  NS_DEFINE_CLASSINFO_DATA(Storage, nsStorageSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_NEWRESOLVE |
                           nsIXPCScriptable::WANT_GETPROPERTY |
                           nsIXPCScriptable::WANT_SETPROPERTY |
                           nsIXPCScriptable::WANT_DELPROPERTY |
                           nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |
                           nsIXPCScriptable::WANT_NEWENUMERATE)
  NS_DEFINE_CLASSINFO_DATA(StorageList, nsStorageListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(StorageItem, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(StorageEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // We just want this to have classinfo so it gets mark callbacks for marking
  // event listeners.
  // We really don't want any of the default flags!
  NS_DEFINE_CLASSINFO_DATA(WindowRoot, nsEventReceiverSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(DOMParser, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(XMLSerializer, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XMLHttpProgressEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(XMLHttpRequest, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // Define MOZ_SVG_FOREIGNOBJECT here so that when it gets switched on,
  // we preserve binary compatibility. New classes should be added
  // at the end.
#if defined(MOZ_SVG) && defined(MOZ_SVG_FOREIGNOBJECT)
  NS_DEFINE_CLASSINFO_DATA(SVGForeignObjectElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
#endif

  NS_DEFINE_CLASSINFO_DATA(XULCommandEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(CommandEvent, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
};

// Objects that shuld be constructable through |new Name();|
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
  NS_DEFINE_CONSTRUCTOR_DATA(XMLSerializer, NS_XMLSERIALIZER_CONTRACTID)
  NS_DEFINE_CONSTRUCTOR_DATA(XMLHttpRequest, NS_XMLHTTPREQUEST_CONTRACTID)
  NS_DEFINE_CONSTRUCTOR_DATA(XPathEvaluator, NS_XPATH_EVALUATOR_CONTRACTID)
  NS_DEFINE_CONSTRUCTOR_DATA(XSLTProcessor,
                             "@mozilla.org/document-transformer;1?type=xslt")
};

nsIXPConnect *nsDOMClassInfo::sXPConnect = nsnull;
nsIScriptSecurityManager *nsDOMClassInfo::sSecMan = nsnull;
PRBool nsDOMClassInfo::sIsInitialized = PR_FALSE;
PRBool nsDOMClassInfo::sDisableDocumentAllSupport = PR_FALSE;
PRBool nsDOMClassInfo::sDisableGlobalScopePollutionSupport = PR_FALSE;


jsval nsDOMClassInfo::sTop_id             = JSVAL_VOID;
jsval nsDOMClassInfo::sParent_id          = JSVAL_VOID;
jsval nsDOMClassInfo::sScrollbars_id      = JSVAL_VOID;
jsval nsDOMClassInfo::sLocation_id        = JSVAL_VOID;
jsval nsDOMClassInfo::sConstructor_id     = JSVAL_VOID;
jsval nsDOMClassInfo::s_content_id        = JSVAL_VOID;
jsval nsDOMClassInfo::sContent_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sMenubar_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sToolbar_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sLocationbar_id     = JSVAL_VOID;
jsval nsDOMClassInfo::sPersonalbar_id     = JSVAL_VOID;
jsval nsDOMClassInfo::sStatusbar_id       = JSVAL_VOID;
jsval nsDOMClassInfo::sDirectories_id     = JSVAL_VOID;
jsval nsDOMClassInfo::sControllers_id     = JSVAL_VOID;
jsval nsDOMClassInfo::sLength_id          = JSVAL_VOID;
jsval nsDOMClassInfo::sInnerHeight_id     = JSVAL_VOID;
jsval nsDOMClassInfo::sInnerWidth_id      = JSVAL_VOID;
jsval nsDOMClassInfo::sOuterHeight_id     = JSVAL_VOID;
jsval nsDOMClassInfo::sOuterWidth_id      = JSVAL_VOID;
jsval nsDOMClassInfo::sScreenX_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sScreenY_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sStatus_id          = JSVAL_VOID;
jsval nsDOMClassInfo::sName_id            = JSVAL_VOID;
jsval nsDOMClassInfo::sOnmousedown_id     = JSVAL_VOID;
jsval nsDOMClassInfo::sOnmouseup_id       = JSVAL_VOID;
jsval nsDOMClassInfo::sOnclick_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sOndblclick_id      = JSVAL_VOID;
jsval nsDOMClassInfo::sOncontextmenu_id   = JSVAL_VOID;
jsval nsDOMClassInfo::sOnmouseover_id     = JSVAL_VOID;
jsval nsDOMClassInfo::sOnmouseout_id      = JSVAL_VOID;
jsval nsDOMClassInfo::sOnkeydown_id       = JSVAL_VOID;
jsval nsDOMClassInfo::sOnkeyup_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sOnkeypress_id      = JSVAL_VOID;
jsval nsDOMClassInfo::sOnmousemove_id     = JSVAL_VOID;
jsval nsDOMClassInfo::sOnfocus_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sOnblur_id          = JSVAL_VOID;
jsval nsDOMClassInfo::sOnsubmit_id        = JSVAL_VOID;
jsval nsDOMClassInfo::sOnreset_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sOnchange_id        = JSVAL_VOID;
jsval nsDOMClassInfo::sOnselect_id        = JSVAL_VOID;
jsval nsDOMClassInfo::sOnload_id          = JSVAL_VOID;
jsval nsDOMClassInfo::sOnbeforeunload_id  = JSVAL_VOID;
jsval nsDOMClassInfo::sOnunload_id        = JSVAL_VOID;
jsval nsDOMClassInfo::sOnpageshow_id      = JSVAL_VOID;
jsval nsDOMClassInfo::sOnpagehide_id      = JSVAL_VOID;
jsval nsDOMClassInfo::sOnabort_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sOnerror_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sOnpaint_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sOnresize_id        = JSVAL_VOID;
jsval nsDOMClassInfo::sOnscroll_id        = JSVAL_VOID;
jsval nsDOMClassInfo::sScrollIntoView_id  = JSVAL_VOID;
jsval nsDOMClassInfo::sScrollX_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sScrollY_id         = JSVAL_VOID;
jsval nsDOMClassInfo::sScrollMaxX_id      = JSVAL_VOID;
jsval nsDOMClassInfo::sScrollMaxY_id      = JSVAL_VOID;
jsval nsDOMClassInfo::sOpen_id            = JSVAL_VOID;
jsval nsDOMClassInfo::sItem_id            = JSVAL_VOID;
jsval nsDOMClassInfo::sNamedItem_id       = JSVAL_VOID;
jsval nsDOMClassInfo::sEnumerate_id       = JSVAL_VOID;
jsval nsDOMClassInfo::sNavigator_id       = JSVAL_VOID;
jsval nsDOMClassInfo::sDocument_id        = JSVAL_VOID;
jsval nsDOMClassInfo::sWindow_id          = JSVAL_VOID;
jsval nsDOMClassInfo::sFrames_id          = JSVAL_VOID;
jsval nsDOMClassInfo::sSelf_id            = JSVAL_VOID;
jsval nsDOMClassInfo::sOpener_id          = JSVAL_VOID;
jsval nsDOMClassInfo::sAdd_id             = JSVAL_VOID;
jsval nsDOMClassInfo::sAll_id             = JSVAL_VOID;
jsval nsDOMClassInfo::sTags_id            = JSVAL_VOID;
jsval nsDOMClassInfo::sAddEventListener_id= JSVAL_VOID;
jsval nsDOMClassInfo::sBaseURIObject_id   = JSVAL_VOID;
jsval nsDOMClassInfo::sNodePrincipal_id   = JSVAL_VOID;
jsval nsDOMClassInfo::sDocumentURIObject_id=JSVAL_VOID;

const JSClass *nsDOMClassInfo::sObjectClass = nsnull;
const JSClass *nsDOMClassInfo::sXPCNativeWrapperClass = nsnull;

PRBool nsDOMClassInfo::sDoSecurityCheckInAddProperty = PR_TRUE;

const JSClass*
NS_DOMClassInfo_GetXPCNativeWrapperClass()
{
  return nsDOMClassInfo::GetXPCNativeWrapperClass();
}

void
NS_DOMClassInfo_SetXPCNativeWrapperClass(JSClass* aClass)
{
  nsDOMClassInfo::SetXPCNativeWrapperClass(aClass);
}

static void
PrintWarningOnConsole(JSContext *cx, const char *stringBundleProperty)
{
  nsCOMPtr<nsIStringBundleService>
    stringService(do_GetService(NS_STRINGBUNDLE_CONTRACTID));
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
    (do_GetService("@mozilla.org/consoleservice;1"));
  if (!consoleService) {
    return;
  }

  nsCOMPtr<nsIScriptError> scriptError =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  if (!scriptError) {
    return;
  }

  JSStackFrame *fp, *iterator = nsnull;
  fp = ::JS_FrameIterator(cx, &iterator);
  PRUint32 lineno = 0;
  nsAutoString sourcefile;
  if (fp) {
    JSScript* script = ::JS_GetFrameScript(cx, fp);
    if (script) {
      const char* filename = ::JS_GetScriptFilename(cx, script);
      if (filename) {
        CopyUTF8toUTF16(nsDependentCString(filename), sourcefile);
      }
      jsbytecode* pc = ::JS_GetFramePC(cx, fp);
      if (pc) {
        lineno = ::JS_PCToLineNumber(cx, script, pc);
      }
    }
  }
  nsresult rv = scriptError->Init(msg.get(),
                                  sourcefile.get(),
                                  EmptyString().get(),
                                  lineno,
                                  0, // column for error is not available
                                  nsIScriptError::warningFlag,
                                  "DOM:HTML");
  if (NS_SUCCEEDED(rv)){
    consoleService->LogMessage(scriptError);
  }
}

static inline JSObject *
GetGlobalJSObject(JSContext *cx, JSObject *obj)
{
  JSObject *tmp;

  while ((tmp = ::JS_GetParent(cx, obj))) {
    obj = tmp;
  }

  return obj;
}

static jsval
GetInternedJSVal(JSContext *cx, const char *str)
{
  JSString *s = ::JS_InternString(cx, str);

  if (!s) {
    return JSVAL_VOID;
  }

  return STRING_TO_JSVAL(s);
}

// static
nsresult
nsDOMClassInfo::DefineStaticJSVals(JSContext *cx)
{
#define SET_JSVAL_TO_STRING(_val, _cx, _str)                                  \
  _val = GetInternedJSVal(_cx, _str);                                         \
  if (!JSVAL_IS_STRING(_val)) {                                               \
    return NS_ERROR_OUT_OF_MEMORY;                                            \
  }

  JSAutoRequest ar(cx);

  SET_JSVAL_TO_STRING(sTop_id,             cx, "top");
  SET_JSVAL_TO_STRING(sParent_id,          cx, "parent");
  SET_JSVAL_TO_STRING(sScrollbars_id,      cx, "scrollbars");
  SET_JSVAL_TO_STRING(sLocation_id,        cx, "location");
  SET_JSVAL_TO_STRING(sConstructor_id,     cx, "constructor");
  SET_JSVAL_TO_STRING(s_content_id,        cx, "_content");
  SET_JSVAL_TO_STRING(sContent_id,         cx, "content");
  SET_JSVAL_TO_STRING(sMenubar_id,         cx, "menubar");
  SET_JSVAL_TO_STRING(sToolbar_id,         cx, "toolbar");
  SET_JSVAL_TO_STRING(sLocationbar_id,     cx, "locationbar");
  SET_JSVAL_TO_STRING(sPersonalbar_id,     cx, "personalbar");
  SET_JSVAL_TO_STRING(sStatusbar_id,       cx, "statusbar");
  SET_JSVAL_TO_STRING(sDirectories_id,     cx, "directories");
  SET_JSVAL_TO_STRING(sControllers_id,     cx, "controllers");
  SET_JSVAL_TO_STRING(sLength_id,          cx, "length");
  SET_JSVAL_TO_STRING(sInnerHeight_id,     cx, "innerHeight");
  SET_JSVAL_TO_STRING(sInnerWidth_id,      cx, "innerWidth");
  SET_JSVAL_TO_STRING(sOuterHeight_id,     cx, "outerHeight");
  SET_JSVAL_TO_STRING(sOuterWidth_id,      cx, "outerWidth");
  SET_JSVAL_TO_STRING(sScreenX_id,         cx, "screenX");
  SET_JSVAL_TO_STRING(sScreenY_id,         cx, "screenY");
  SET_JSVAL_TO_STRING(sStatus_id,          cx, "status");
  SET_JSVAL_TO_STRING(sName_id,            cx, "name");
  SET_JSVAL_TO_STRING(sOnmousedown_id,     cx, "onmousedown");
  SET_JSVAL_TO_STRING(sOnmouseup_id,       cx, "onmouseup");
  SET_JSVAL_TO_STRING(sOnclick_id,         cx, "onclick");
  SET_JSVAL_TO_STRING(sOndblclick_id,      cx, "ondblclick");
  SET_JSVAL_TO_STRING(sOncontextmenu_id,   cx, "oncontextmenu");
  SET_JSVAL_TO_STRING(sOnmouseover_id,     cx, "onmouseover");
  SET_JSVAL_TO_STRING(sOnmouseout_id,      cx, "onmouseout");
  SET_JSVAL_TO_STRING(sOnkeydown_id,       cx, "onkeydown");
  SET_JSVAL_TO_STRING(sOnkeyup_id,         cx, "onkeyup");
  SET_JSVAL_TO_STRING(sOnkeypress_id,      cx, "onkeypress");
  SET_JSVAL_TO_STRING(sOnmousemove_id,     cx, "onmousemove");
  SET_JSVAL_TO_STRING(sOnfocus_id,         cx, "onfocus");
  SET_JSVAL_TO_STRING(sOnblur_id,          cx, "onblur");
  SET_JSVAL_TO_STRING(sOnsubmit_id,        cx, "onsubmit");
  SET_JSVAL_TO_STRING(sOnreset_id,         cx, "onreset");
  SET_JSVAL_TO_STRING(sOnchange_id,        cx, "onchange");
  SET_JSVAL_TO_STRING(sOnselect_id,        cx, "onselect");
  SET_JSVAL_TO_STRING(sOnload_id,          cx, "onload");
  SET_JSVAL_TO_STRING(sOnbeforeunload_id,  cx, "onbeforeunload");
  SET_JSVAL_TO_STRING(sOnunload_id,        cx, "onunload");
  SET_JSVAL_TO_STRING(sOnpageshow_id,      cx, "onpageshow");
  SET_JSVAL_TO_STRING(sOnpagehide_id,      cx, "onpagehide");
  SET_JSVAL_TO_STRING(sOnabort_id,         cx, "onabort");
  SET_JSVAL_TO_STRING(sOnerror_id,         cx, "onerror");
  SET_JSVAL_TO_STRING(sOnpaint_id,         cx, "onpaint");
  SET_JSVAL_TO_STRING(sOnresize_id,        cx, "onresize");
  SET_JSVAL_TO_STRING(sOnscroll_id,        cx, "onscroll");
  SET_JSVAL_TO_STRING(sScrollIntoView_id,  cx, "scrollIntoView");
  SET_JSVAL_TO_STRING(sScrollX_id,         cx, "scrollX");
  SET_JSVAL_TO_STRING(sScrollY_id,         cx, "scrollY");
  SET_JSVAL_TO_STRING(sScrollMaxX_id,      cx, "scrollMaxX");
  SET_JSVAL_TO_STRING(sScrollMaxY_id,      cx, "scrollMaxY");
  SET_JSVAL_TO_STRING(sOpen_id,            cx, "open");
  SET_JSVAL_TO_STRING(sItem_id,            cx, "item");
  SET_JSVAL_TO_STRING(sNamedItem_id,       cx, "namedItem");
  SET_JSVAL_TO_STRING(sEnumerate_id,       cx, "enumerateProperties");
  SET_JSVAL_TO_STRING(sNavigator_id,       cx, "navigator");
  SET_JSVAL_TO_STRING(sDocument_id,        cx, "document");
  SET_JSVAL_TO_STRING(sWindow_id,          cx, "window");
  SET_JSVAL_TO_STRING(sFrames_id,          cx, "frames");
  SET_JSVAL_TO_STRING(sSelf_id,            cx, "self");
  SET_JSVAL_TO_STRING(sOpener_id,          cx, "opener");
  SET_JSVAL_TO_STRING(sAdd_id,             cx, "add");
  SET_JSVAL_TO_STRING(sAll_id,             cx, "all");
  SET_JSVAL_TO_STRING(sTags_id,            cx, "tags");
  SET_JSVAL_TO_STRING(sAddEventListener_id,cx, "addEventListener");
  SET_JSVAL_TO_STRING(sBaseURIObject_id,   cx, "baseURIObject");
  SET_JSVAL_TO_STRING(sNodePrincipal_id,   cx, "nodePrincipal");
  SET_JSVAL_TO_STRING(sDocumentURIObject_id,cx,"documentURIObject");

  return NS_OK;
}

// static
nsresult
nsDOMClassInfo::WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
                           const nsIID& aIID, jsval *vp,
                           nsIXPConnectJSObjectHolder **aHolder)
{
  *aHolder = nsnull;
  
  if (!native) {
    *vp = JSVAL_NULL;

    return NS_OK;
  }

  NS_ENSURE_TRUE(sXPConnect, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  nsresult rv = sXPConnect->WrapNative(cx, GetGlobalJSObject(cx, scope),
                                       native, aIID, getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject* obj = nsnull;
  rv = holder->GetJSObject(&obj);
  NS_ENSURE_SUCCESS(rv, rv);

  *vp = OBJECT_TO_JSVAL(obj);
  holder.swap(*aHolder);

  return rv;
}

// static
nsresult
nsDOMClassInfo::ThrowJSException(JSContext *cx, nsresult aResult)
{
  JSAutoRequest ar(cx);

  do {
    nsCOMPtr<nsIExceptionService> xs =
      do_GetService(NS_EXCEPTIONSERVICE_CONTRACTID);
    if (!xs) {
      break;
    }

    nsCOMPtr<nsIExceptionManager> xm;
    nsresult rv = xs->GetCurrentExceptionManager(getter_AddRefs(xm));
    if (NS_FAILED(rv)) {
      break;
    }

    nsCOMPtr<nsIException> exception;
    rv = xm->GetExceptionFromProvider(aResult, 0, getter_AddRefs(exception));
    if (NS_FAILED(rv) || !exception) {
      break;
    }

    jsval jv;
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, ::JS_GetGlobalObject(cx), exception,
                    NS_GET_IID(nsIException), &jv, getter_AddRefs(holder));
    if (NS_FAILED(rv) || JSVAL_IS_NULL(jv)) {
      break;
    }
    JS_SetPendingException(cx, jv);

    return NS_OK;
  } while (0);

  // XXX This probably wants to be localized, but that can fail in ways that
  // are hard to report correctly.
  JSString *str =
    JS_NewStringCopyZ(cx, "An error occured throwing an exception");
  if (!str) {
    // JS_NewStringCopyZ reported the error for us.
    return NS_OK; 
  }
  JS_SetPendingException(cx, STRING_TO_JSVAL(str));
  return NS_OK;
}

nsDOMClassInfo::nsDOMClassInfo(nsDOMClassInfoData* aData) : mData(aData)
{
}

nsDOMClassInfo::~nsDOMClassInfo()
{
  if (IS_EXTERNAL(mData->mCachedClassInfo)) {
    // Some compilers don't like delete'ing a const nsDOMClassInfo*
    nsDOMClassInfoData* data = NS_CONST_CAST(nsDOMClassInfoData*, mData);
    delete NS_STATIC_CAST(nsExternalDOMClassInfoData*, data);
  }
}

NS_IMPL_ADDREF(nsDOMClassInfo)
NS_IMPL_RELEASE(nsDOMClassInfo)

NS_INTERFACE_MAP_BEGIN(nsDOMClassInfo)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCScriptable)
NS_INTERFACE_MAP_END


JSClass nsDOMClassInfo::sDOMConstructorProtoClass = {
  "DOM Constructor.prototype", 0,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
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
  extern nsScriptNameSpaceManager *gNameSpaceManager;
  NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  gNameSpaceManager->RegisterClassName(sClassInfoData[aClassInfoID].mName,
                                       aClassInfoID);

  return NS_OK;
}

// static
nsresult
nsDOMClassInfo::RegisterClassProtos(PRInt32 aClassInfoID)
{
  extern nsScriptNameSpaceManager *gNameSpaceManager;
  NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_NOT_INITIALIZED);
  PRBool found_old;

  const nsIID *primary_iid = sClassInfoData[aClassInfoID].mProtoChainInterface;

  if (!primary_iid || primary_iid == &NS_GET_IID(nsISupports)) {
    return NS_OK;
  }

  nsCOMPtr<nsIInterfaceInfoManager>
    iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
  NS_ENSURE_TRUE(iim, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIInterfaceInfo> if_info;
  PRBool first = PR_TRUE;

  iim->GetInfoForIID(primary_iid, getter_AddRefs(if_info));

  while (if_info) {
    nsIID *iid = nsnull;

    if_info->GetInterfaceIID(&iid);
    NS_ENSURE_TRUE(iid, NS_ERROR_UNEXPECTED);

    if (iid->Equals(NS_GET_IID(nsISupports))) {
      nsMemory::Free(iid);

      break;
    }

    nsXPIDLCString name;
    if_info->GetName(getter_Copies(name));

    gNameSpaceManager->RegisterClassProto(CutPrefix(name), iid, &found_old);

    nsMemory::Free(iid);

    if (first) {
      first = PR_FALSE;
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
  extern nsScriptNameSpaceManager *gNameSpaceManager;
  NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  nsCOMPtr<nsIComponentRegistrar> registrar;
  rv = NS_GetComponentRegistrar(getter_AddRefs(registrar));
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

    rv = gNameSpaceManager->RegisterExternalClassName(categoryEntry.get(), *cid);
    nsMemory::Free(cid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return gNameSpaceManager->RegisterExternalInterfaces(PR_TRUE);
}

#define _DOM_CLASSINFO_MAP_BEGIN(_class, _ifptr, _has_class_if)               \
  {                                                                           \
    nsDOMClassInfoData &d = sClassInfoData[eDOMClassInfo_##_class##_id];      \
    d.mProtoChainInterface = _ifptr;                                          \
    d.mHasClassInterface = _has_class_if;                                     \
    static const nsIID *interface_list[] = {

#define DOM_CLASSINFO_MAP_BEGIN(_class, _interface)                           \
  _DOM_CLASSINFO_MAP_BEGIN(_class, &NS_GET_IID(_interface), PR_TRUE)

#define DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(_class, _interface)               \
  _DOM_CLASSINFO_MAP_BEGIN(_class, &NS_GET_IID(_interface), PR_FALSE)

#define DOM_CLASSINFO_MAP_ENTRY(_if)                                          \
      &NS_GET_IID(_if),

#define DOM_CLASSINFO_MAP_END                                                 \
      nsnull                                                                  \
    };                                                                        \
                                                                              \
    d.mInterfaces = interface_list;                                           \
  }

#define DOM_CLASSINFO_DOCUMENT_MAP_ENTRIES                                    \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSDocument)                                 \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentEvent)                              \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentStyle)                              \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSDocumentStyle)                            \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentView)                               \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentRange)                              \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentTraversal)                          \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentXBL)                                \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)                                \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Document)                                  \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)                                      \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXPathEvaluator)

#define DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES                                \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLElement)                              \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElementCSSInlineStyle)                      \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)                                \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)                                      \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSElement)

#define DOM_CLASSINFO_EVENT_MAP_ENTRIES                                       \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEvent)                                      \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSEvent)                                    \

#define DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES                                    \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMUIEvent)                                    \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSUIEvent)                                  \
    DOM_CLASSINFO_EVENT_MAP_ENTRIES

nsresult
nsDOMClassInfo::Init()
{
  /* Errors that can trigger early returns are done first,
     otherwise nsDOMClassInfo is left in a half inited state. */
  NS_ASSERTION(sizeof(PtrBits) == sizeof(void*),
               "BAD! You'll need to adjust the size of PtrBits to the size "
               "of a pointer on your platform.");

  NS_ENSURE_TRUE(!sIsInitialized, NS_ERROR_ALREADY_INITIALIZED);

  extern nsScriptNameSpaceManager *gNameSpaceManager;
  NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = CallGetService(nsIXPConnect::GetCID(), &sXPConnect);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPCFunctionThisTranslator> old;

  nsCOMPtr<nsIXPCFunctionThisTranslator> elt = new nsEventListenerThisTranslator();
  NS_ENSURE_TRUE(elt, NS_ERROR_OUT_OF_MEMORY);

  sXPConnect->SetFunctionThisTranslator(NS_GET_IID(nsIDOMEventListener),
                                        elt, getter_AddRefs(old));

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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindow)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMJSWindow)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowInternal)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMViewCSS)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMAbstractView)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorageWindow)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WindowUtils, nsIDOMWindowUtils)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowUtils)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Location, nsIDOMLocation)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLocation)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSLocation)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Navigator, nsIDOMNavigator)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigator)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMJSNavigator)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMClientInformation)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Plugin, nsIDOMPlugin)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPlugin)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(PluginArray, nsIDOMPluginArray)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPluginArray)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMJSPluginArray)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHistory)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Screen, nsIDOMScreen)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMScreen)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Constructor, nsIDOMConstructor)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMConstructor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XMLDocument, nsIDOMXMLDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXMLDocument)
    DOM_CLASSINFO_DOCUMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DocumentType, nsIDOMDocumentType)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentType)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMImplementation, nsIDOMDOMImplementation)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMImplementation)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMException, nsIDOMDOMException)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMException)
    DOM_CLASSINFO_MAP_ENTRY(nsIException)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DocumentFragment, nsIDOMDocumentFragment)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentFragment)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Element, nsIDOMElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Attr, nsIDOMAttr)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMAttr)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Attr)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Text, nsIDOMText)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMText)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Comment, nsIDOMComment)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMComment)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CDATASection, nsIDOMCDATASection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCDATASection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(ProcessingInstruction, nsIDOMProcessingInstruction)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMProcessingInstruction)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Notation, nsIDOMNotation)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNotation)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(NodeList, nsIDOMNodeList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(NamedNodeMap, nsIDOMNamedNodeMap)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNamedNodeMap)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(DocumentStyleSheetList,
                                      nsIDOMStyleSheetList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStyleSheetList)
  DOM_CLASSINFO_MAP_END
  
  DOM_CLASSINFO_MAP_BEGIN(Event, nsIDOMEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(PopupBlockedEvent, nsIDOMPopupBlockedEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPopupBlockedEvent)
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

  DOM_CLASSINFO_MAP_BEGIN(MouseEvent, nsIDOMMouseEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMouseEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLDocument, nsIDOMHTMLDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLDocument)
    DOM_CLASSINFO_DOCUMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLOptionsCollection, nsIDOMHTMLOptionsCollection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLOptionsCollection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLOptionCollection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLCollection)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(HTMLFormControlCollection,
                                      nsIDOMHTMLCollection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLCollection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLFormControlList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(HTMLGenericCollection,
                                      nsIDOMHTMLCollection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLCollection)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLAnchorElement, nsIDOMHTMLAnchorElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLAnchorElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLAnchorElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLAnchorElement2)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLAppletElement, nsIDOMHTMLAppletElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLAppletElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLAreaElement, nsIDOMHTMLAreaElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLAreaElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLAreaElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLAreaElement2)
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

  DOM_CLASSINFO_MAP_BEGIN(HTMLBaseFontElement, nsIDOMHTMLBaseFontElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLBaseFontElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLBodyElement, nsIDOMHTMLBodyElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLBodyElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLButtonElement, nsIDOMHTMLButtonElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLButtonElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLButtonElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLDListElement, nsIDOMHTMLDListElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLDListElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(HTMLDelElement, nsIDOMHTMLElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLModElement)
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
#ifdef MOZ_SVG
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMGetSVGDocument)
#endif
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLFormElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLFrameElement, nsIDOMHTMLFrameElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLFrameElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLFrameElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLFrameSetElement, nsIDOMHTMLFrameSetElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLFrameSetElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLHRElement, nsIDOMHTMLHRElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLHRElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLHRElement)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLFrameElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLImageElement, nsIDOMHTMLImageElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLImageElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLImageElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLInputElement, nsIDOMHTMLInputElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLInputElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLInputElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(HTMLInsElement, nsIDOMHTMLElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLModElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLIsIndexElement, nsIDOMHTMLIsIndexElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLIsIndexElement)
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

  DOM_CLASSINFO_MAP_BEGIN(HTMLMetaElement, nsIDOMHTMLMetaElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLMetaElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLOListElement, nsIDOMHTMLOListElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLOListElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLObjectElement, nsIDOMHTMLObjectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLObjectElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLOptGroupElement, nsIDOMHTMLOptGroupElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLOptGroupElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLOptionElement, nsIDOMHTMLOptionElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLOptionElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLOptionElement)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLSelectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSXBLFormControl)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(HTMLSpacerElement, nsIDOMHTMLElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLElement)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLTextAreaElement)
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

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(HTMLUnknownElement, nsIDOMHTMLElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(HTMLWBRElement, nsIDOMHTMLElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLElement)
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

  DOM_CLASSINFO_MAP_BEGIN(CSSStyleDeclaration, nsIDOMCSSStyleDeclaration)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSStyleDeclaration)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSS2Properties)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSCSS2Properties)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ComputedCSSStyleDeclaration,
                                      nsIDOMCSSStyleDeclaration)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSStyleDeclaration)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSS2Properties)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSCSS2Properties)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSRange)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElementCSSInlineStyle)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XULCommandDispatcher, nsIDOMXULCommandDispatcher)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULCommandDispatcher)
  DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XULControllers, nsIControllers)
    DOM_CLASSINFO_MAP_ENTRY(nsIControllers)
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_XUL
  DOM_CLASSINFO_MAP_BEGIN(BoxObject, nsIBoxObject)
    DOM_CLASSINFO_MAP_ENTRY(nsIBoxObject)
  DOM_CLASSINFO_MAP_END

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

  DOM_CLASSINFO_MAP_BEGIN(Pkcs11, nsIDOMPkcs11)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMPkcs11)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XMLStylesheetProcessingInstruction, nsIDOMProcessingInstruction)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMProcessingInstruction)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLinkStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ChromeWindow, nsIDOMWindow)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindow)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMJSWindow)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowInternal)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMChromeWindow)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    // XXXjst: Do we want this on chrome windows?
    // DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorageWindow)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMViewCSS)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMAbstractView)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(RangeException, nsIDOMRangeException)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMRangeException)
    DOM_CLASSINFO_MAP_ENTRY(nsIException)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ContentList, nsIDOMHTMLCollection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLCollection)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(ImageDocument, nsIImageDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIImageDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLDocument)
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

  DOM_CLASSINFO_MAP_BEGIN(NameList, nsIDOMNameList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNameList)
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

#ifdef MOZ_SVG
#define DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGElement) \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSElement)  \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)

#define DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)        \
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocument)
    DOM_CLASSINFO_DOCUMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  // SVG element classes

  DOM_CLASSINFO_MAP_BEGIN(SVGAElement, nsIDOMSVGAElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGCircleElement, nsIDOMSVGCircleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGCircleElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGClipPathElement, nsIDOMSVGClipPathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGClipPathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLocatable)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTransformable)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGDefsElement, nsIDOMSVGDefsElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGDefsElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGDescElement, nsIDOMSVGDescElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGDescElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGEllipseElement, nsIDOMSVGEllipseElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGEllipseElement)
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

  DOM_CLASSINFO_MAP_BEGIN(SVGFETurbulenceElement, nsIDOMSVGFETurbulenceElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFETurbulenceElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFEUnimplementedMOZElement, nsIDOMSVGFEUnimplementedMOZElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFEUnimplementedMOZElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGFilterElement, nsIDOMSVGFilterElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFilterElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGGElement, nsIDOMSVGGElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGGElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGImageElement, nsIDOMSVGImageElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGImageElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGLinearGradientElement, nsIDOMSVGLinearGradientElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGGradientElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLinearGradientElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGLineElement, nsIDOMSVGLineElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLineElement)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGMetadataElement, nsIDOMSVGMetadataElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGMetadataElement)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathElement, nsIDOMSVGPathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedPathData)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPatternElement, nsIDOMSVGPatternElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPatternElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFitToViewBox)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPolygonElement, nsIDOMSVGPolygonElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPolygonElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedPoints)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPolylineElement, nsIDOMSVGPolylineElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPolylineElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedPoints)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGRadialGradientElement, nsIDOMSVGRadialGradientElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGGradientElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGRadialGradientElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGRectElement, nsIDOMSVGRectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGRectElement)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGSVGElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFitToViewBox)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLocatable)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGZoomAndPan)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGSwitchElement, nsIDOMSVGSwitchElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGSwitchElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGSymbolElement, nsIDOMSVGSymbolElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGSymbolElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGFitToViewBox)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTextElement, nsIDOMSVGTextElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextPositioningElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextContentElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTextPathElement, nsIDOMSVGTextPathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextContentElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTitleElement, nsIDOMSVGTitleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTitleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTSpanElement, nsIDOMSVGTSpanElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextPositioningElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextContentElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGStylable)
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGUseElement, nsIDOMSVGUseElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGUseElement)
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

  DOM_CLASSINFO_MAP_BEGIN(SVGTransform, nsIDOMSVGTransform)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTransform)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTransformList, nsIDOMSVGTransformList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTransformList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGZoomEvent, nsIDOMSVGZoomEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGZoomEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END
#endif // MOZ_SVG

  DOM_CLASSINFO_MAP_BEGIN(HTMLCanvasElement, nsIDOMHTMLCanvasElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLCanvasElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_ENABLE_CANVAS
  DOM_CLASSINFO_MAP_BEGIN(CanvasRenderingContext2D, nsIDOMCanvasRenderingContext2D)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCanvasRenderingContext2D)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CanvasGradient, nsIDOMCanvasGradient)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCanvasGradient)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CanvasPattern, nsIDOMCanvasPattern)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCanvasPattern)
  DOM_CLASSINFO_MAP_END
#endif // MOZ_ENABLE_CANVAS

  DOM_CLASSINFO_MAP_BEGIN(XSLTProcessor, nsIXSLTProcessor)
    DOM_CLASSINFO_MAP_ENTRY(nsIXSLTProcessor)
    DOM_CLASSINFO_MAP_ENTRY(nsIXSLTProcessorObsolete) // XXX DEPRECATED
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

  DOM_CLASSINFO_MAP_BEGIN(Storage, nsIDOMStorage)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorage)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(StorageList, nsIDOMStorageList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorageList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(StorageItem, nsIDOMStorageItem)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorageItem)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMToString)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(StorageEvent, nsIDOMStorageEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorageEvent)
  DOM_CLASSINFO_MAP_END

  // We just want this to have classinfo so it gets mark callbacks for marking
  // event listeners.
  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(WindowRoot, nsISupports)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIInterfaceRequestor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XMLHttpProgressEvent, nsIDOMEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLSProgressEvent)
  DOM_CLASSINFO_MAP_END

#if defined(MOZ_SVG) && defined(MOZ_SVG_FOREIGNOBJECT)
  DOM_CLASSINFO_MAP_BEGIN(SVGForeignObjectElement, nsIDOMSVGForeignObjectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGForeignObjectElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN(XULCommandEvent, nsIDOMXULCommandEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULCommandEvent)
    DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CommandEvent, nsIDOMCommandEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCommandEvent)
    DOM_CLASSINFO_EVENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

#ifdef NS_DEBUG
  {
    PRUint32 i = NS_ARRAY_LENGTH(sClassInfoData);

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

  PRBool doSecurityCheckInAddProperty = sDoSecurityCheckInAddProperty;
  sDoSecurityCheckInAddProperty = PR_FALSE;
  RegisterExternalClasses();
  sDoSecurityCheckInAddProperty = doSecurityCheckInAddProperty;

  sDisableDocumentAllSupport =
    nsContentUtils::GetBoolPref("browser.dom.document.all.disabled");

  sDisableGlobalScopePollutionSupport =
    nsContentUtils::GetBoolPref("browser.dom.global_scope_pollution.disabled");

  sIsInitialized = PR_TRUE;

  return NS_OK;
}

// static
PRInt32
nsDOMClassInfo::GetArrayIndexFromId(JSContext *cx, jsval id, PRBool *aIsNumber)
{
  jsdouble array_index;

  if (aIsNumber) {
    *aIsNumber = PR_FALSE;
  }

  JSAutoRequest ar(cx);

  if (!::JS_ValueToNumber(cx, id, &array_index)) {
    return -1;
  }

  jsint i = -1;

  if (!JSDOUBLE_IS_INT(array_index, i)) {
    return -1;
  }

  if (aIsNumber) {
    *aIsNumber = PR_TRUE;
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

  *aArray = NS_STATIC_CAST(nsIID **, nsMemory::Alloc(count * sizeof(nsIID *)));
  NS_ENSURE_TRUE(*aArray, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 i;
  for (i = 0; i < count; i++) {
    nsIID *iid = NS_STATIC_CAST(nsIID *, nsMemory::Clone(mData->mInterfaces[i],
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
    *_retval = NS_STATIC_CAST(nsIXPCScriptable *, this);

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

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  nsresult rv =
    sXPConnect->GetWrappedNativeOfJSObject(cx, globalObj,
                                           getter_AddRefs(wrapper));
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> piwin = do_QueryWrappedNative(wrapper);

  if (!piwin) {
    return NS_OK;
  }

  if (piwin->IsOuterWindow()) {
    *parentObj = ((nsGlobalWindow *)piwin.get())->
      GetCurrentInnerWindowInternal()->GetGlobalJSObject();
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
  static const nsIID *sSupportsIID = &NS_GET_IID(nsISupports);

  // This is safe because...
  if (mData->mProtoChainInterface == sSupportsIID ||
      !mData->mProtoChainInterface) {
    return NS_OK;
  }

#ifdef DEBUG
  {
    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryWrappedNative(wrapper));

    NS_ASSERTION(!sgo || sgo->GetGlobalJSObject() == nsnull,
                 "Multiple wrappers created for global object!");
  }
#endif

  JSObject *proto = nsnull;

  wrapper->GetJSObjectPrototype(&proto);

  JSAutoRequest ar(cx);

  JSObject *proto_proto = ::JS_GetPrototype(cx, proto);
  if (!proto_proto) {
    // If our prototype doesn't have a proto, then we've probably already
    // wrapped this object and someone's done something evil, like set
    // our prototype's proto to null, so bail.

    return NS_OK;
  }

  JSClass *proto_proto_class = JS_GET_CLASS(cx, proto_proto);
  if (proto_proto_class != sObjectClass) {
    // We've just wrapped an object of a type that has been wrapped on
    // this scope already so the prototype of the xpcwrapped native's
    // prototype is already set up.

    return NS_OK;
  }

#ifdef DEBUG
  if (mData->mHasClassInterface) {
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

  // Look up the name of our constructor in the current global scope. We do
  // this because triggering this lookup can cause us to call
  // nsWindowSH::NewResolve, which will end up in nsWindowSH::GlobalResolve.
  // GlobalResolve does some prototype magic (which satisfies the if condition
  // above) in order to make sure that prototype delegation works correctly.
  // Consider if a site sets HTMLElement.prototype.foopy = function () { ... }
  // Now, calling document.body.foopy() needs to ensure that looking up foopy
  // on document.body's prototype will find the right function. This
  // LookupProperty accomplishes that.
  // XXX This shouldn't need to go through the JS engine. Instead, we should
  // be calling nsWindowSH::GlobalResolve directly.
  JSObject *global = GetGlobalJSObject(cx, obj);
  jsval val;
  if (!::JS_LookupProperty(cx, global, mData->mName, &val)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval id, jsval *vp,
                            PRBool *_retval)
{
  NS_WARNING("nsDOMClassInfo::AddProperty Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval id, jsval *vp,
                            PRBool *_retval)
{
  NS_WARNING("nsDOMClassInfo::DelProperty Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval id, jsval *vp,
                            PRBool *_retval)
{
  NS_WARNING("nsDOMClassInfo::GetProperty Don't call me!");

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval id, jsval *vp,
                            PRBool *_retval)
{
  NS_WARNING("nsDOMClassInfo::SetProperty Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, PRBool *_retval)
{
  if (!sSecMan)
    return NS_OK;

  // Ask the security manager if it's OK to enumerate
  nsresult rv =
    sSecMan->CheckPropertyAccess(cx, obj, mData->mName, sEnumerate_id,
                                 nsIXPCSecurityManager::ACCESS_GET_PROPERTY);

  if (NS_FAILED(rv)) {
    // Let XPConnect know that the access was not granted.
    *_retval = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                             JSContext *cx, JSObject *obj, PRUint32 enum_op,
                             jsval *statep, jsid *idp, PRBool *_retval)
{
  NS_WARNING("nsDOMClassInfo::NewEnumerate Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

nsresult
nsDOMClassInfo::ResolveConstructor(JSContext *cx, JSObject *obj,
                                   JSObject **objp)
{
  JSObject *global = GetGlobalJSObject(cx, obj);

  jsval val;
  JSAutoRequest ar(cx);
  if (!::JS_GetProperty(cx, global, mData->mName, &val)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!JSVAL_IS_PRIMITIVE(val)) {
    // If val is not an (non-null) object there either is no
    // constructor for this class, or someone messed with
    // window.classname, just fall through and let the JS engine
    // return the Object constructor.

    JSString *str = JSVAL_TO_STRING(sConstructor_id);
    if (!::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                               ::JS_GetStringLength(str), val, nsnull, nsnull,
                               JSPROP_ENUMERATE)) {
      return NS_ERROR_UNEXPECTED;
    }

    *objp = obj;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                           JSObject *obj, jsval id, PRUint32 flags,
                           JSObject **objp, PRBool *_retval)
{
  if (id == sConstructor_id && !(flags & JSRESOLVE_ASSIGNING)) {
    return ResolveConstructor(cx, obj, objp);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::Convert(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, PRUint32 type, jsval *vp,
                        PRBool *_retval)
{
  NS_WARNING("nsDOMClassInfo::Convert Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Finalize(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj)
{
  NS_WARNING("nsDOMClassInfo::Finalize Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::CheckAccess(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval id, PRUint32 mode,
                            jsval *vp, PRBool *_retval)
{
  PRUint32 mode_type = mode & JSACC_TYPEMASK;

  if ((mode_type == JSACC_WATCH ||
       mode_type == JSACC_PROTO ||
       mode_type == JSACC_PARENT) &&
      sSecMan) {

    JSObject *real_obj = nsnull;
    nsresult rv = wrapper->GetJSObject(&real_obj);
    NS_ENSURE_SUCCESS(rv, rv);

    rv =
      sSecMan->CheckPropertyAccess(cx, real_obj, mData->mName, id,
                                   nsIXPCSecurityManager::ACCESS_GET_PROPERTY);

    if (NS_FAILED(rv)) {
      // Let XPConnect know that the access was not granted.
      *_retval = PR_FALSE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JSObject *obj, PRUint32 argc, jsval *argv, jsval *vp,
                     PRBool *_retval)
{
  NS_WARNING("nsDOMClassInfo::Call Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, PRUint32 argc, jsval *argv,
                          jsval *vp, PRBool *_retval)
{
  NS_WARNING("nsDOMClassInfo::Construct Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval val, PRBool *bp,
                            PRBool *_retval)
{
  NS_WARNING("nsDOMClassInfo::HasInstance Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Mark(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JSObject *obj, void *arg, PRUint32 *_retval)
{
  NS_WARNING("nsDOMClassInfo::Mark Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Equality(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, jsval val, PRBool *bp)
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

NS_IMETHODIMP
nsDOMClassInfo::InnerObject(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                            JSObject * obj, JSObject * *_retval)
{
  NS_WARNING("nsDOMClassInfo::InnerObject Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

// static
nsIClassInfo *
nsDOMClassInfo::GetClassInfoInstance(nsDOMClassInfoID aID)
{
  if (aID >= eDOMClassInfoIDCount) {
    NS_ERROR("Bad ID!");

    return nsnull;
  }

  if (!sIsInitialized) {
    nsresult rv = Init();

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
nsresult
nsDOMClassInfo::PreserveNodeWrapper(nsIXPConnectWrappedNative *aWrapper)
{
   nsISupports *native = aWrapper->Native();
   nsCOMPtr<nsIDOMNode> node(do_QueryInterface(native));
   
   nsCOMPtr<nsIDocument> doc;
   if (node) {
     nsCOMPtr<nsIDOMDocument> domdoc;
     node->GetOwnerDocument(getter_AddRefs(domdoc));
     doc = do_QueryInterface(domdoc);
  }

   if (!doc) {
     doc = do_QueryInterface(native);
   }

   if (doc) {
     nsCOMPtr<nsIContent> content(do_QueryInterface(node));
     doc->AddReference(content, aWrapper);
   }
   return NS_OK;
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

  sTop_id             = JSVAL_VOID;
  sParent_id          = JSVAL_VOID;
  sScrollbars_id      = JSVAL_VOID;
  sLocation_id        = JSVAL_VOID;
  sConstructor_id     = JSVAL_VOID;
  s_content_id        = JSVAL_VOID;
  sContent_id         = JSVAL_VOID;
  sMenubar_id         = JSVAL_VOID;
  sToolbar_id         = JSVAL_VOID;
  sLocationbar_id     = JSVAL_VOID;
  sPersonalbar_id     = JSVAL_VOID;
  sStatusbar_id       = JSVAL_VOID;
  sDirectories_id     = JSVAL_VOID;
  sControllers_id     = JSVAL_VOID;
  sLength_id          = JSVAL_VOID;
  sInnerHeight_id     = JSVAL_VOID;
  sInnerWidth_id      = JSVAL_VOID;
  sOuterHeight_id     = JSVAL_VOID;
  sOuterWidth_id      = JSVAL_VOID;
  sScreenX_id         = JSVAL_VOID;
  sScreenY_id         = JSVAL_VOID;
  sStatus_id          = JSVAL_VOID;
  sName_id            = JSVAL_VOID;
  sOnmousedown_id     = JSVAL_VOID;
  sOnmouseup_id       = JSVAL_VOID;
  sOnclick_id         = JSVAL_VOID;
  sOndblclick_id      = JSVAL_VOID;
  sOncontextmenu_id   = JSVAL_VOID;
  sOnmouseover_id     = JSVAL_VOID;
  sOnmouseout_id      = JSVAL_VOID;
  sOnkeydown_id       = JSVAL_VOID;
  sOnkeyup_id         = JSVAL_VOID;
  sOnkeypress_id      = JSVAL_VOID;
  sOnmousemove_id     = JSVAL_VOID;
  sOnfocus_id         = JSVAL_VOID;
  sOnblur_id          = JSVAL_VOID;
  sOnsubmit_id        = JSVAL_VOID;
  sOnreset_id         = JSVAL_VOID;
  sOnchange_id        = JSVAL_VOID;
  sOnselect_id        = JSVAL_VOID;
  sOnload_id          = JSVAL_VOID;
  sOnbeforeunload_id  = JSVAL_VOID;
  sOnunload_id        = JSVAL_VOID;
  sOnpageshow_id      = JSVAL_VOID;
  sOnpagehide_id      = JSVAL_VOID;
  sOnabort_id         = JSVAL_VOID;
  sOnerror_id         = JSVAL_VOID;
  sOnpaint_id         = JSVAL_VOID;
  sOnresize_id        = JSVAL_VOID;
  sOnscroll_id        = JSVAL_VOID;
  sScrollIntoView_id  = JSVAL_VOID;
  sScrollX_id         = JSVAL_VOID;
  sScrollY_id         = JSVAL_VOID;
  sScrollMaxX_id      = JSVAL_VOID;
  sScrollMaxY_id      = JSVAL_VOID;
  sOpen_id            = JSVAL_VOID;
  sItem_id            = JSVAL_VOID;
  sEnumerate_id       = JSVAL_VOID;
  sNavigator_id       = JSVAL_VOID;
  sDocument_id        = JSVAL_VOID;
  sWindow_id          = JSVAL_VOID;
  sFrames_id          = JSVAL_VOID;
  sSelf_id            = JSVAL_VOID;
  sOpener_id          = JSVAL_VOID;
  sAdd_id             = JSVAL_VOID;
  sAll_id             = JSVAL_VOID;
  sTags_id            = JSVAL_VOID;
  sAddEventListener_id= JSVAL_VOID;
  sBaseURIObject_id   = JSVAL_VOID;
  sNodePrincipal_id   = JSVAL_VOID;
  sDocumentURIObject_id=JSVAL_VOID;

  NS_IF_RELEASE(sXPConnect);
  NS_IF_RELEASE(sSecMan);
  sIsInitialized = PR_FALSE;
}


static const nsIXPConnectWrappedNative *cached_win_wrapper;
static const JSContext *cached_win_cx;
static PRBool cached_win_needs_check = PR_TRUE;
static const nsIXPConnectWrappedNative *cached_doc_wrapper;
static const JSContext *cached_doc_cx;
static PRBool cached_doc_needs_check = PR_TRUE;


void InvalidateContextAndWrapperCache()
{
  cached_win_wrapper = nsnull;
  cached_doc_wrapper = nsnull;
  cached_win_cx = nsnull;
  cached_doc_cx = nsnull;
  cached_win_needs_check = PR_TRUE;
  cached_doc_needs_check = PR_TRUE;
}

// static helper that determines if a security manager check is needed
// by checking if the callee's context is the same as the caller's
// context
// Note: See documentNeedsSecurityCheck for information about the control
// flow in this function.

static inline PRBool
needsSecurityCheck(JSContext *cx, nsIXPConnectWrappedNative *wrapper)
{
  // We cache a pointer to a wrapper and a context that we've last vetted
  // and cache what the verdict was.

  // First, compare the context and wrapper with the cached ones
  if (cx == cached_win_cx && wrapper == cached_win_wrapper) {
    return cached_win_needs_check;
  }

  cached_win_cx = cx;
  cached_win_wrapper = wrapper;
  cached_win_needs_check = PR_TRUE;
  
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryWrappedNative(wrapper));

  if (!sgo) {
    NS_ERROR("Huh, global not a nsIScriptGlobalObject?");

    return PR_TRUE;
  }

  nsIScriptContext *otherScriptContext = sgo->GetContext();

  if (!otherScriptContext) {
    return PR_TRUE;
  }

  if (cx != (JSContext *)otherScriptContext->GetNativeContext()) {
    return PR_TRUE;
  }

  // Compare the current context and function object
  // to the ones in the next JS frame
  JSStackFrame *fp = nsnull;
  JSObject *fp_obj = nsnull;

  cached_win_needs_check = PR_FALSE;

  do {
    fp = ::JS_FrameIterator(cx, &fp);

    if (!fp) {
      cached_win_cx = nsnull;
      return cached_win_needs_check;
    }

    fp_obj = ::JS_GetFrameFunctionObject(cx, fp);
    cached_win_needs_check = PR_TRUE;
  } while (!fp_obj);

  JSObject *global = GetGlobalJSObject(cx, fp_obj);

  JSObject *wrapper_obj = nsnull;
  wrapper->GetJSObject(&wrapper_obj);

  if (global != wrapper_obj) {
    return PR_TRUE;
  }

  cached_win_needs_check = PR_FALSE;
  return PR_FALSE;
}


// Window helper

nsresult
nsDOMClassInfo::doCheckPropertyAccess(JSContext *cx, JSObject *obj, jsval id,
                                      nsIXPConnectWrappedNative *wrapper,
                                      PRUint32 accessMode, PRBool isWindow)
{
  if (!sSecMan) {
    return NS_OK;
  }

  nsISupports *native = wrapper->Native();
  nsCOMPtr<nsIScriptGlobalObject> sgo;

  if (isWindow) {
    sgo = do_QueryInterface(native);
    NS_ENSURE_TRUE(sgo, NS_ERROR_UNEXPECTED);
  } else {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(native));
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    sgo = doc->GetScriptGlobalObject();

    if (!sgo) {
      // There's no script global in the document. This means that
      // this document is a result from using XMLHttpRequest or it's a
      // document created through a DOMImplementation. In that case
      // there's nothing we can do since the context on which the
      // document was created is not accessible and we can't do a
      // security check, but the document must remain
      // scriptable. Documents loaded through these methods have
      // already been vetted by the security manager before they were
      // loaded, so allowing access here w/o doing a security check
      // here is probably safe anyway.

      return NS_OK;
    }
  }

  nsIScriptContext *scx = sgo->GetContext();
  JSObject *global;

  if (!scx || !scx->IsContextInitialized() ||
      !(global = sgo->GetGlobalJSObject())) {
    return NS_OK;
  }

  return sSecMan->CheckPropertyAccess(cx, global, mData->mName, id,
                                      accessMode);
}

NS_IMETHODIMP
nsWindowSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                      JSObject *globalObj, JSObject **parentObj)
{
  // Since this is one of the first calls we'll get from XPConnect,
  // grab the pointer to the Object class so we'll have it later on.

  if (!sObjectClass) {
    JSObject *obj, *proto = globalObj;
    JSAutoRequest ar(cx);

    do {
      obj = proto;
      proto = ::JS_GetPrototype(cx, obj);
    } while (proto);

    sObjectClass = JS_GET_CLASS(cx, obj);
  }

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

  if (sgo) {
    *parentObj = sgo->GetGlobalJSObject();

    if (*parentObj) {
      return NS_OK;
    }
  }

  // We're most likely being called when the global object is
  // created, at that point we won't get a nsIScriptContext but we
  // know we're called on the correct context so we return globalObj

  *parentObj = globalObj;

  return NS_OK;
}


// This JS class piggybacks on nsHTMLDocumentSH::ReleaseDocument()...

static JSClass sGlobalScopePolluterClass = {
  "Global Scope Polluter",
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_NEW_RESOLVE,
  nsWindowSH::SecurityCheckOnSetProp, nsWindowSH::SecurityCheckOnSetProp,
  nsWindowSH::GlobalScopePolluterGetProperty,
  nsWindowSH::SecurityCheckOnSetProp, JS_EnumerateStub,
  (JSResolveOp)nsWindowSH::GlobalScopePolluterNewResolve, JS_ConvertStub,
  nsHTMLDocumentSH::ReleaseDocument
};


// static
JSBool JS_DLL_CALLBACK
nsWindowSH::GlobalScopePolluterGetProperty(JSContext *cx, JSObject *obj,
                                           jsval id, jsval *vp)
{
  // Someone is accessing a element by referencing its name/id in the
  // global scope, do a security check to make sure that's ok.

  nsresult rv =
    sSecMan->CheckPropertyAccess(cx, GetGlobalJSObject(cx, obj), "Window", id,
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
JSBool JS_DLL_CALLBACK
nsWindowSH::SecurityCheckOnSetProp(JSContext *cx, JSObject *obj, jsval id,
                                   jsval *vp)
{
  // Someone is accessing a element by referencing its name/id in the
  // global scope, do a security check to make sure that's ok.

  nsresult rv =
    sSecMan->CheckPropertyAccess(cx, GetGlobalJSObject(cx, obj), "Window", id,
                                 nsIXPCSecurityManager::ACCESS_SET_PROPERTY);

  // If !NS_SUCCEEDED(rv) the security check failed. The security
  // manager set a JS exception for us.
  return NS_SUCCEEDED(rv);
}

// static
JSBool JS_DLL_CALLBACK
nsWindowSH::GlobalScopePolluterNewResolve(JSContext *cx, JSObject *obj,
                                          jsval id, uintN flags,
                                          JSObject **objp)
{
  if (flags & (JSRESOLVE_ASSIGNING | JSRESOLVE_DECLARING |
               JSRESOLVE_CLASSNAME | JSRESOLVE_QUALIFIED) ||
      !JSVAL_IS_STRING(id)) {
    // Nothing to do here if we're either assigning or declaring,
    // resolving a class name, doing a qualified resolve, or
    // resolving a number.

    return JS_TRUE;
  }

  nsIHTMLDocument *doc = (nsIHTMLDocument *)::JS_GetPrivate(cx, obj);
  nsCOMPtr<nsIDocument> document(do_QueryInterface(doc));

  if (!document ||
      document->GetCompatibilityMode() != eCompatibility_NavQuirks) {
    // If we don't have a document, or if the document is not in
    // quirks mode, return early.

    return JS_TRUE;
  }

  JSObject *proto = ::JS_GetPrototype(cx, obj);
  JSString *jsstr = JSVAL_TO_STRING(id);
  JSBool hasProp;

  if (!proto || !::JS_HasUCProperty(cx, proto, ::JS_GetStringChars(jsstr),
                                    ::JS_GetStringLength(jsstr), &hasProp) ||
      hasProp) {
    // No prototype, or the property exists on the prototype. Do
    // nothing.

    return JS_TRUE;
  }

  nsDependentJSString str(jsstr);
  nsCOMPtr<nsISupports> result;

  {
    nsCOMPtr<nsIDOMDocument> dom_doc(do_QueryInterface(doc));
    nsCOMPtr<nsIDOMElement> element;

    dom_doc->GetElementById(str, getter_AddRefs(element));

    result = element;
  }

  if (!result) {
    doc->ResolveName(str, nsnull, getter_AddRefs(result));
  }

  if (result) {
    jsval v;
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult rv = WrapNative(cx, obj, result, NS_GET_IID(nsISupports), &v,
                             getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(jsstr),
                               ::JS_GetStringLength(jsstr), v, nsnull, nsnull,
                               0)) {
      nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

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

  while ((proto = ::JS_GetPrototype(cx, obj))) {
    if (JS_GET_CLASS(cx, proto) == &sGlobalScopePolluterClass) {
      nsIHTMLDocument *doc = (nsIHTMLDocument *)::JS_GetPrivate(cx, proto);

      NS_IF_RELEASE(doc);

      ::JS_SetPrivate(cx, proto, nsnull);

      // Pull the global scope polluter out of the prototype chain so
      // that it can be freed.
      ::JS_SetPrototype(cx, obj, ::JS_GetPrototype(cx, proto));

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

  JSObject *gsp = ::JS_NewObject(cx, &sGlobalScopePolluterClass, nsnull, obj);
  if (!gsp) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  JSObject *o = obj, *proto;

  // Find the place in the prototype chain where we want this global
  // scope polluter (right before Object.prototype).

  while ((proto = ::JS_GetPrototype(cx, o))) {
    if (JS_GET_CLASS(cx, proto) == sObjectClass) {
      // Set the global scope polluters prototype to Object.prototype
      if (!::JS_SetPrototype(cx, gsp, proto)) {
        return NS_ERROR_UNEXPECTED;
      }

      break;
    }

    o = proto;
  }

  // And then set the prototype of the object whose prototype was
  // Object.prototype to be the global scope polluter.
  if (!::JS_SetPrototype(cx, o, gsp)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!::JS_SetPrivate(cx, gsp, doc)) {
    return NS_ERROR_UNEXPECTED;
  }

  // The global scope polluter will release doc on destruction (or
  // invalidation).
  NS_ADDREF(doc);

  return NS_OK;
}

static
already_AddRefed<nsIDOMWindow>
GetChildFrame(nsGlobalWindow *win, jsval id)
{
  nsCOMPtr<nsIDOMWindowCollection> frames;
  win->GetFrames(getter_AddRefs(frames));

  nsIDOMWindow *frame = nsnull;

  if (frames) {
    frames->Item(JSVAL_TO_INT(id), &frame);
  }

  return frame;
}

NS_IMETHODIMP
nsWindowSH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, jsval *vp, PRBool *_retval)
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

  if (win->IsOuterWindow() && !ObjectIsNativeWrapper(cx, obj)) {
    // XXXjst: Do security checks here when we remove the security
    // checks on the inner window.

    nsGlobalWindow *innerWin = win->GetCurrentInnerWindowInternal();

    JSObject *innerObj;
    if (innerWin && (innerObj = innerWin->GetGlobalJSObject())) {
#ifdef DEBUG_SH_FORWARDING
      printf(" --- Forwarding get to inner window %p\n", (void *)innerWin);
#endif

      // Forward the get to the inner object
      if (JSVAL_IS_STRING(id)) {
        JSString *str = JSVAL_TO_STRING(id);

        *_retval = ::JS_GetUCProperty(cx, innerObj, ::JS_GetStringChars(str),
                                      ::JS_GetStringLength(str), vp);
      } else if (JSVAL_IS_INT(id)) {
        *_retval = ::JS_GetElement(cx, innerObj, JSVAL_TO_INT(id), vp);
      } else {
        NS_ERROR("Write me!");

        return NS_ERROR_NOT_IMPLEMENTED;
      }

      return NS_OK;
    }
  }

  // The order in which things are done in this method are a bit
  // whacky, that's because this method is *extremely* performace
  // critical. Don't touch this unless you know what you're doing.

  if (JSVAL_IS_INT(id)) {
    // If we're accessing a numeric property we'll treat that as if
    // window.frames[n] is accessed (since window.frames === window),
    // if window.frames[n] is a child frame, wrap the frame and return
    // it without doing a security check.

    nsCOMPtr<nsIDOMWindow> frame = GetChildFrame(win, id);
    nsresult rv = NS_OK;

    if (frame) {
      // A numeric property accessed and the numeric property is a
      // child frame, wrap the child frame without doing a security
      // check and return.

      nsGlobalWindow *frameWin = (nsGlobalWindow *)frame.get();

      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      rv = WrapNative(cx, frameWin->GetGlobalJSObject(), frame,
                      NS_GET_IID(nsIDOMWindow), vp,
                      getter_AddRefs(holder));
    }

    return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
  }

  if (needsSecurityCheck(cx, wrapper)) {
    // Even if we'd need to do a security check for access to "normal"
    // properties on a window, we won't do a security check if we're
    // accessing a child frame.

    if (JSVAL_IS_STRING(id) && !JSVAL_IS_PRIMITIVE(*vp) &&
        ::JS_TypeOfValue(cx, *vp) != JSTYPE_FUNCTION) {
      // A named property accessed which could have been resolved to a
      // child frame in nsWindowSH::NewResolve() (*vp will tell us if
      // that's the case). If *vp is a window object (i.e. a child
      // frame), return without doing a security check.

      nsCOMPtr<nsIXPConnectWrappedNative> vpwrapper;
      sXPConnect->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(*vp),
                                             getter_AddRefs(vpwrapper));

      if (vpwrapper) {
        nsCOMPtr<nsIDOMWindow> window(do_QueryWrappedNative(vpwrapper));

        if (window) {
          // Yup, *vp is a window object, return early (*vp is already
          // the window, so no need to wrap it again).

          return NS_SUCCESS_I_DID_SOMETHING;
        }
      }
    }

    nsresult rv =
      doCheckPropertyAccess(cx, obj, id, wrapper,
                            nsIXPCSecurityManager::ACCESS_GET_PROPERTY,
                            PR_TRUE);

    if (NS_FAILED(rv)) {
      // Security check failed. The security manager set a JS
      // exception, we must make sure that exception is propagated.

      *_retval = PR_FALSE;

      *vp = JSVAL_NULL;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, jsval *vp, PRBool *_retval)
{
  nsGlobalWindow *win = nsGlobalWindow::FromWrapper(wrapper);

#ifdef DEBUG_SH_FORWARDING
  {
    nsDependentJSString str(::JS_ValueToString(cx, id));

    if (win->IsInnerWindow()) {
#ifdef DEBUG_PRINT_INNER
      printf("Property '%s' set on inner window %p\n",
             NS_ConvertUTF16toUTF8(str).get(), (void *)win);
#endif
    } else {
      printf("Property '%s' set on outer window %p\n",
             NS_ConvertUTF16toUTF8(str).get(), (void *)win);
    }
  }
#endif

  if (win->IsOuterWindow() && !ObjectIsNativeWrapper(cx, obj)) {
    // XXXjst: Do security checks here when we remove the security
    // checks on the inner window.

    nsGlobalWindow *innerWin = win->GetCurrentInnerWindowInternal();

    JSObject *innerObj;
    if (innerWin && (innerObj = innerWin->GetGlobalJSObject())) {
#ifdef DEBUG_SH_FORWARDING
      printf(" --- Forwarding set to inner window %p\n", (void *)innerWin);
#endif

      // Forward the set to the inner object
      if (JSVAL_IS_STRING(id)) {
        JSString *str = JSVAL_TO_STRING(id);

        *_retval = ::JS_SetUCProperty(cx, innerObj, ::JS_GetStringChars(str),
                                      ::JS_GetStringLength(str), vp);
      } else if (JSVAL_IS_INT(id)) {
        *_retval = ::JS_SetElement(cx, innerObj, JSVAL_TO_INT(id), vp);
      } else {
        NS_ERROR("Write me!");

        return NS_ERROR_NOT_IMPLEMENTED;
      }

      return NS_OK;
    }
  }

  if (needsSecurityCheck(cx, wrapper)) {
    nsresult rv =
      doCheckPropertyAccess(cx, obj, id, wrapper,
                            nsIXPCSecurityManager::ACCESS_SET_PROPERTY,
                            PR_TRUE);

    if (NS_FAILED(rv)) {
      // Security check failed. The security manager set a JS
      // exception, we must make sure that exception is propagated.

      *_retval = PR_FALSE;

      return NS_OK;
    }
  }

  if (id == sLocation_id) {
    JSAutoRequest ar(cx);

    JSString *val = ::JS_ValueToString(cx, *vp);
    NS_ENSURE_TRUE(val, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMWindowInternal> window(do_QueryWrappedNative(wrapper));
    NS_ENSURE_TRUE(window, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMLocation> location;
    nsresult rv = window->GetLocation(getter_AddRefs(location));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, obj, location, NS_GET_IID(nsIDOMLocation), vp,
                    getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = location->SetHref(nsDependentJSString(val));

    return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
  }

  return nsEventReceiverSH::SetProperty(wrapper, cx, obj, id, vp, _retval);
}

NS_IMETHODIMP
nsWindowSH::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, jsval *vp,
                        PRBool *_retval)
{
  nsGlobalWindow *win = nsGlobalWindow::FromWrapper(wrapper);

#ifdef DEBUG_SH_FORWARDING
  {
    nsDependentJSString str(::JS_ValueToString(cx, id));

    if (win->IsInnerWindow()) {
#ifdef DEBUG_PRINT_INNER
      printf("Property '%s' add on inner window %p\n",
             NS_ConvertUTF16toUTF8(str).get(), (void *)win);
#endif
    } else {
      printf("Property '%s' add on outer window %p\n",
             NS_ConvertUTF16toUTF8(str).get(), (void *)win);
    }
  }
#endif

  if (win->IsOuterWindow() && !ObjectIsNativeWrapper(cx, obj) ) {
    // XXXjst: Do security checks here when we remove the security
    // checks on the inner window.

    nsGlobalWindow *innerWin = win->GetCurrentInnerWindowInternal();

    JSObject *innerObj;
    if (innerWin && (innerObj = innerWin->GetGlobalJSObject())) {
#ifdef DEBUG_SH_FORWARDING
      printf(" --- Forwarding add to inner window %p\n", (void *)innerWin);
#endif

      // Forward the add to the inner object
      jsid interned_id;
      *_retval = (::JS_ValueToId(cx, id, &interned_id) &&
                  OBJ_DEFINE_PROPERTY(cx, innerObj, interned_id, *vp, nsnull,
                                      nsnull, JSPROP_ENUMERATE, nsnull));

      return NS_OK;
    }
  }

  // If we're in a state where we're not supposed to do a security
  // check, return early.
  if (!sDoSecurityCheckInAddProperty) {
    return NS_OK;
  }

  if (id == sLocation_id) {
    // Don't allow adding a window.location setter or getter, allowing
    // that could lead to security bugs (see bug 143369).

    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsresult rv =
    doCheckPropertyAccess(cx, obj, id, wrapper,
                          nsIXPCSecurityManager::ACCESS_SET_PROPERTY,
                          PR_TRUE);

  if (NS_FAILED(rv)) {
    // Security check failed. The security manager set a JS
    // exception, we must make sure that exception is propagated.

    *_retval = PR_FALSE;

    return NS_OK;
  }

  return nsEventReceiverSH::AddProperty(wrapper, cx, obj, id, vp, _retval);
}

NS_IMETHODIMP
nsWindowSH::DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, jsval *vp,
                        PRBool *_retval)
{
  nsGlobalWindow *win = nsGlobalWindow::FromWrapper(wrapper);

#ifdef DEBUG_SH_FORWARDING
  {
    nsDependentJSString str(::JS_ValueToString(cx, id));

    if (win->IsInnerWindow()) {
#ifdef DEBUG_PRINT_INNER
      printf("Property '%s' del on inner window %p\n",
             NS_ConvertUTF16toUTF8(str).get(), (void *)win);
#endif
    } else {
      printf("Property '%s' del on outer window %p\n",
             NS_ConvertUTF16toUTF8(str).get(), (void *)win);
    }
  }
#endif

  if (win->IsOuterWindow() && !ObjectIsNativeWrapper(cx, obj)) {
    // XXXjst: Do security checks here when we remove the security
    // checks on the inner window.

    nsGlobalWindow *innerWin = win->GetCurrentInnerWindowInternal();

    JSObject *innerObj;
    if (innerWin && (innerObj = innerWin->GetGlobalJSObject())) {
#ifdef DEBUG_SH_FORWARDING
      printf(" --- Forwarding del to inner window %p\n", (void *)innerWin);
#endif

      // Forward the del to the inner object
      jsid interned_id;
      *_retval = (::JS_ValueToId(cx, id, &interned_id) &&
                  OBJ_DELETE_PROPERTY(cx, innerObj, interned_id, vp));

      return NS_OK;
    }
  }

  if (id == sLocation_id) {
    // Don't allow deleting window.location, allowing that could lead
    // to security bugs (see bug 143369).

    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsresult rv =
    doCheckPropertyAccess(cx, obj, id, wrapper,
                          nsIXPCSecurityManager::ACCESS_SET_PROPERTY,
                          PR_TRUE);

  if (NS_FAILED(rv)) {
    // Security check failed. The security manager set a JS
    // exception, we must make sure that exception is propagated.

    *_retval = PR_FALSE;
  }

  return NS_OK;
}

static const char*
FindConstructorContractID(PRInt32 aDOMClassInfoID)
{
  PRUint32 i;
  for (i = 0; i < NS_ARRAY_LENGTH(kConstructorMap); ++i) {
    if (kConstructorMap[i].mDOMClassInfoID == aDOMClassInfoID) {
      return kConstructorMap[i].mContractID;
    }
  }
  return nsnull;
}

static nsresult
BaseStubConstructor(const nsGlobalNameStruct *name_struct, JSContext *cx,
                    JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult rv;
  nsCOMPtr<nsISupports> native;
  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    const char *contractid =
      FindConstructorContractID(name_struct->mDOMClassInfoID);
    native = do_CreateInstance(contractid, &rv);
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
    rv = initializer->Initialize(cx, obj, argc, argv);
    if (NS_FAILED(rv)) {
      return NS_ERROR_NOT_INITIALIZED;
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

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  rv = nsDOMGenericSH::WrapNative(cx, obj, native, NS_GET_IID(nsISupports),
                                  rval, getter_AddRefs(holder));

  return rv;
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
      case nsXPTType::T_U32:
      {
        v = INT_TO_JSVAL(c->GetValue()->val.u32);
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

class nsDOMConstructor : public nsIDOMConstructor
{
public:
  nsDOMConstructor(const PRUnichar *aName)
    : mClassName(aName)
  {
  }

  virtual ~nsDOMConstructor();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMCONSTRUCTOR

  nsresult Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JSObject *obj, PRUint32 argc, jsval *argv,
                     jsval *vp, PRBool *_retval);

  nsresult HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, jsval val, PRBool *bp,
                       PRBool *_retval);

  nsresult Install(JSContext *cx, JSObject *target, jsval thisAsVal)
  {
    PRBool doSecurityCheckInAddProperty =
      nsDOMClassInfo::sDoSecurityCheckInAddProperty;
    nsDOMClassInfo::sDoSecurityCheckInAddProperty = PR_FALSE;

    JSBool ok =
      ::JS_DefineUCProperty(cx, target,
                            NS_REINTERPRET_CAST(const jschar *, mClassName),
                            nsCRT::strlen(mClassName), thisAsVal, nsnull,
                            nsnull, 0);

    nsDOMClassInfo::sDoSecurityCheckInAddProperty =
      doSecurityCheckInAddProperty;
    return ok ? NS_OK : NS_ERROR_UNEXPECTED;
  }

private:
  const PRUnichar *mClassName;
};

NS_IMPL_ADDREF(nsDOMConstructor)
NS_IMPL_RELEASE(nsDOMConstructor)
NS_INTERFACE_MAP_BEGIN(nsDOMConstructor)
  NS_INTERFACE_MAP_ENTRY(nsIDOMConstructor)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Constructor)
NS_INTERFACE_MAP_END

nsDOMConstructor::~nsDOMConstructor()
{
}

nsresult
nsDOMConstructor::Construct(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                            JSObject * obj, PRUint32 argc, jsval * argv,
                            jsval * vp, PRBool *_retval)
{
  JSObject* class_obj = JSVAL_TO_OBJECT(argv[-2]);
  if (!class_obj) {
    NS_ERROR("nsDOMConstructor::Construct couldn't get constructor object.");
    return NS_ERROR_UNEXPECTED;
  }

  extern nsScriptNameSpaceManager *gNameSpaceManager;
  if (!mClassName || !gNameSpaceManager) {
    NS_ERROR("nsDOMConstructor::Construct can't get name or namespace manager");
    return NS_ERROR_UNEXPECTED;
  }

  const nsGlobalNameStruct *name_struct = nsnull;
  gNameSpaceManager->LookupName(nsDependentString(mClassName), &name_struct);
  if (!name_struct) {
    NS_ERROR("Name isn't in hash.");
    return NS_ERROR_UNEXPECTED;
  }

  if ((name_struct->mType != nsGlobalNameStruct::eTypeClassConstructor ||
       !FindConstructorContractID(name_struct->mDOMClassInfoID)) &&
      (name_struct->mType != nsGlobalNameStruct::eTypeExternalClassInfo ||
       !name_struct->mData->mConstructorCID) &&
      name_struct->mType != nsGlobalNameStruct::eTypeExternalConstructor &&
      name_struct->mType != nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    // ignore return value, we return JS_FALSE anyway
    NS_ERROR("object instantiated without constructor");
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  return BaseStubConstructor(name_struct, cx, obj, argc, argv, vp);
}

nsresult
nsDOMConstructor::HasInstance(nsIXPConnectWrappedNative *wrapper,
                              JSContext * cx, JSObject * obj,
                              jsval v, PRBool *bp, PRBool *_retval)

{
  // No need to look these up in the hash.
  if (JSVAL_IS_PRIMITIVE(v)) {
    return NS_OK;
  }

  JSObject *dom_obj = JSVAL_TO_OBJECT(v);
  NS_ASSERTION(dom_obj, "nsDOMConstructor::HasInstance couldn't get object");

  // This might not be the right object, if XPCNativeWrapping
  // happened.  Get the wrapped native for this object, then get its
  // JS object.
  nsCOMPtr<nsIXPConnectWrappedNative> wrapped_native;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(cx, dom_obj, getter_AddRefs(wrapped_native));
  if (wrapped_native) {
    wrapped_native->GetJSObject(&dom_obj);
  }

  JSClass *dom_class = JS_GET_CLASS(cx, dom_obj);
  if (!dom_class) {
    NS_ERROR("nsDOMConstructor::HasInstance can't get class.");
    return NS_ERROR_UNEXPECTED;
  }

  const nsGlobalNameStruct *name_struct = nsnull;

  extern nsScriptNameSpaceManager *gNameSpaceManager;
  if (!gNameSpaceManager) {
    NS_ERROR("nsDOMConstructor::HasInstance can't get namespace manager.");
    return NS_ERROR_UNEXPECTED;
  }

  gNameSpaceManager->LookupName(NS_ConvertASCIItoUTF16(dom_class->name),
                                &name_struct);
  if (!name_struct) {
    // Name isn't in hash, not a DOM object.
    return NS_OK;
  }

  if (name_struct->mType != nsGlobalNameStruct::eTypeClassConstructor &&
      name_struct->mType != nsGlobalNameStruct::eTypeExternalClassInfo &&
      name_struct->mType != nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    // Doesn't have DOM interfaces.
    return NS_OK;
  }

  if (!mClassName) {
    NS_ERROR("nsDOMConstructor::HasInstance can't get name.");
    return NS_ERROR_UNEXPECTED;
  }

  const nsGlobalNameStruct *class_name_struct = nsnull;
  gNameSpaceManager->LookupName(nsDependentString(mClassName),
                                &class_name_struct);
  if (!class_name_struct) {
    NS_ERROR("Name isn't in hash.");
    return NS_ERROR_UNEXPECTED;
  }

  if (name_struct == class_name_struct) {
    *bp = JS_TRUE;

    return NS_OK;
  }

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
      gNameSpaceManager->GetConstructorProto(class_name_struct);
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
    name_struct = gNameSpaceManager->GetConstructorProto(name_struct);
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

NS_IMETHODIMP
nsDOMConstructor::ToString(nsAString &aResult)
{
  aResult.AssignLiteral("[object ");
  aResult.Append(mClassName);
  aResult.Append(PRUnichar(']'));

  return NS_OK;
}

// static
nsresult
nsWindowSH::GlobalResolve(nsGlobalWindow *aWin, JSContext *cx,
                          JSObject *obj, JSString *str, PRUint32 flags,
                          PRBool *did_resolve)
{
  *did_resolve = PR_FALSE;

  extern nsScriptNameSpaceManager *gNameSpaceManager;

  NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nsDependentJSString name(str);

  const nsGlobalNameStruct *name_struct = nsnull;
  const PRUnichar *class_name = nsnull;

  gNameSpaceManager->LookupName(name, &name_struct, &class_name);

  if (!name_struct) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(class_name, NS_ERROR_UNEXPECTED);

  nsresult rv = NS_OK;

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfoCreator) {
    nsCOMPtr<nsIDOMCIExtension> creator(do_CreateInstance(name_struct->mCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMScriptObjectFactory> sof(do_GetService(kDOMSOF_CID));
    NS_ENSURE_TRUE(sof, NS_ERROR_FAILURE);

    rv = creator->RegisterDOMCI(NS_ConvertUTF16toUTF8(name).get(), sof);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = gNameSpaceManager->LookupName(name, &name_struct);
    if (NS_FAILED(rv) || !name_struct ||
        name_struct->mType != nsGlobalNameStruct::eTypeExternalClassInfo) {
      NS_ERROR("Couldn't get the DOM ClassInfo data.");

      return NS_OK;
    }
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeInterface) {
    // We're resolving a name of a DOM interface for which there is no
    // direct DOM class, create a constructor object...

    nsRefPtr<nsDOMConstructor> constructor =
      new nsDOMConstructor(NS_REINTERPRET_CAST(PRUnichar *,
                                               ::JS_GetStringChars(str)));
    if (!constructor) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    PRBool doSecurityCheckInAddProperty = sDoSecurityCheckInAddProperty;
    sDoSecurityCheckInAddProperty = PR_FALSE;

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    jsval v;

    rv = WrapNative(cx, obj, constructor, NS_GET_IID(nsIDOMConstructor), &v,
                    getter_AddRefs(holder));

    sDoSecurityCheckInAddProperty = doSecurityCheckInAddProperty;

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

    *did_resolve = PR_TRUE;

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor ||
      name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo ||
      name_struct->mType == nsGlobalNameStruct::eTypeClassProto ||
      name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    const nsDOMClassInfoData *ci_data = nsnull;
    const nsGlobalNameStruct* alias_struct = nsnull;

    if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor &&
        name_struct->mDOMClassInfoID >= 0) {
      ci_data = &sClassInfoData[name_struct->mDOMClassInfoID];
    } else if (name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
      ci_data = name_struct->mData;
    } else if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
      alias_struct = gNameSpaceManager->GetConstructorProto(name_struct);
      NS_ENSURE_TRUE(alias_struct, NS_ERROR_UNEXPECTED);

      if (alias_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
        ci_data = &sClassInfoData[alias_struct->mDOMClassInfoID];
      } else if (alias_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
        ci_data = alias_struct->mData;
      }
    }

    const PRUnichar *name = NS_REINTERPRET_CAST(PRUnichar *,
                                                ::JS_GetStringChars(str));
    nsRefPtr<nsDOMConstructor> constructor = new nsDOMConstructor(name);
    if (!constructor) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    PRBool doSecurityCheckInAddProperty = sDoSecurityCheckInAddProperty;
    sDoSecurityCheckInAddProperty = PR_FALSE;

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    jsval v;

    rv = WrapNative(cx, obj, constructor, NS_GET_IID(nsIDOMConstructor), &v,
                    getter_AddRefs(holder));

    sDoSecurityCheckInAddProperty = doSecurityCheckInAddProperty;

    NS_ENSURE_SUCCESS(rv, rv);

    rv = constructor->Install(cx, obj, v);
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject *class_obj;
    holder->GetJSObject(&class_obj);
    NS_ASSERTION(class_obj, "The return value lied");

    const nsIID *primary_iid = &NS_GET_IID(nsISupports);

    if (name_struct->mType == nsGlobalNameStruct::eTypeClassProto) {
      primary_iid = &name_struct->mIID;
    } else if (ci_data && ci_data->mProtoChainInterface) {
      primary_iid = ci_data->mProtoChainInterface;
    }

    nsXPIDLCString class_parent_name;

    if (!primary_iid->Equals(NS_GET_IID(nsISupports))) {
      rv = DefineInterfaceConstants(cx, class_obj, primary_iid);
      NS_ENSURE_SUCCESS(rv, rv);

      // Special case for |Node|, which needs constants from Node3
      // too for forwards compatibility.
      if (primary_iid->Equals(NS_GET_IID(nsIDOMNode))) {
        rv = DefineInterfaceConstants(cx, class_obj,
                                      &NS_GET_IID(nsIDOM3Node));
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Special case for |Event|, Event needs constants from NSEvent
      // too for backwards compatibility.
      if (primary_iid->Equals(NS_GET_IID(nsIDOMEvent))) {
        rv = DefineInterfaceConstants(cx, class_obj,
                                      &NS_GET_IID(nsIDOMNSEvent));
        NS_ENSURE_SUCCESS(rv, rv);
      }

      nsCOMPtr<nsIInterfaceInfoManager>
        iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
      NS_ENSURE_TRUE(iim, NS_ERROR_NOT_AVAILABLE);

      nsCOMPtr<nsIInterfaceInfo> if_info;
      iim->GetInfoForIID(primary_iid, getter_AddRefs(if_info));
      NS_ENSURE_TRUE(if_info, NS_ERROR_UNEXPECTED);

      nsCOMPtr<nsIInterfaceInfo> parent;
      nsIID *iid = nsnull;

      if (ci_data && !ci_data->mHasClassInterface) {
        if_info->GetInterfaceIID(&iid);
      } else {
        if_info->GetParent(getter_AddRefs(parent));
        NS_ENSURE_TRUE(parent, NS_ERROR_UNEXPECTED);

        parent->GetInterfaceIID(&iid);
      }

      if (iid) {
        if (!iid->Equals(NS_GET_IID(nsISupports))) {
          if (ci_data && !ci_data->mHasClassInterface) {
            // If the class doesn't have a class interface the primary
            // interface is the interface that should be
            // constructor.prototype.__proto__.

            if_info->GetName(getter_Copies(class_parent_name));
          } else {
            // If the class does have a class interface (or there's no
            // real class for this name) then the parent of the
            // primary interface is what we want on
            // constructor.prototype.__proto__.

            NS_ASSERTION(parent, "Whoa, this is bad, null parent here!");

            parent->GetName(getter_Copies(class_parent_name));
          }
        }

        nsMemory::Free(iid);
      }
    }

    JSObject *proto = nsnull;

    if (class_parent_name) {
      jsval val;

      if (!::JS_GetProperty(cx, obj, CutPrefix(class_parent_name), &val)) {
        return NS_ERROR_UNEXPECTED;
      }

      JSObject *tmp = JSVAL_IS_OBJECT(val) ? JSVAL_TO_OBJECT(val) : nsnull;

      if (tmp) {
        if (!::JS_GetProperty(cx, tmp, "prototype", &val)) {
          return NS_ERROR_UNEXPECTED;
        }

        if (JSVAL_IS_OBJECT(val)) {
          proto = JSVAL_TO_OBJECT(val);
        }
      }
    }

    JSObject *dot_prototype = nsnull;

    if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
      name_struct = alias_struct;
    }

    if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
      PRInt32 id = name_struct->mDOMClassInfoID;
      NS_ABORT_IF_FALSE(id >= 0, "Negative DOM classinfo?!?");

      nsDOMClassInfoID ci_id = (nsDOMClassInfoID)id;

      nsCOMPtr<nsIClassInfo> ci(GetClassInfoInstance(ci_id));
      NS_ENSURE_TRUE(ci, NS_ERROR_UNEXPECTED);

      nsCOMPtr<nsIXPConnectJSObjectHolder> proto_holder;

      // In most cases we want to find the wrapped native prototype in
      // aWin's scope and use that prototype for
      // ClassName.prototype. But in the case where we're setting up
      // "Window.prototype" or "ChromeWindow.prototype" we want to do
      // the look up in aWin's outer window's scope since the inner
      // window's wrapped native prototype comes from the outer
      // window's scope.
      nsGlobalWindow *scopeWindow;

      if (ci_id == eDOMClassInfo_Window_id ||
          ci_id == eDOMClassInfo_ChromeWindow_id) {
        scopeWindow = aWin->GetOuterWindowInternal();

        if (!scopeWindow) {
          scopeWindow = aWin;
        }
      } else {
        scopeWindow = aWin;
      }

      rv =
        sXPConnect->GetWrappedNativePrototype(cx,
                                              scopeWindow->GetGlobalJSObject(),
                                              ci,
                                              getter_AddRefs(proto_holder));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

      rv = proto_holder->GetJSObject(&dot_prototype);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

      JSObject *xpc_proto_proto = ::JS_GetPrototype(cx, dot_prototype);

      if (proto && JS_GET_CLASS(cx, xpc_proto_proto) == sObjectClass) {
        if (!::JS_SetPrototype(cx, dot_prototype, proto)) {
          return NS_ERROR_UNEXPECTED;
        }
      }
    } else if (name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
      nsCOMPtr<nsIClassInfo> ci = GetClassInfoInstance(name_struct->mData);
      NS_ENSURE_TRUE(ci, NS_ERROR_UNEXPECTED);

      nsCOMPtr<nsIXPConnectJSObjectHolder> proto_holder;

      rv =
        sXPConnect->GetWrappedNativePrototype(cx, obj, ci,
                                              getter_AddRefs(proto_holder));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

      rv = proto_holder->GetJSObject(&dot_prototype);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

      JSObject *xpc_proto_proto = ::JS_GetPrototype(cx, dot_prototype);

      if (proto && JS_GET_CLASS(cx, xpc_proto_proto) == sObjectClass) {
        if (!::JS_SetPrototype(cx, dot_prototype, proto)) {
          return NS_ERROR_UNEXPECTED;
        }
      }
    } else {
      dot_prototype = ::JS_NewObject(cx, &sDOMConstructorProtoClass, proto,
                                     obj);
      NS_ENSURE_TRUE(dot_prototype, NS_ERROR_OUT_OF_MEMORY);
    }

    v = OBJECT_TO_JSVAL(dot_prototype);

    // Per ECMA, the prototype property is {DontEnum, DontDelete, ReadOnly}
    if (!::JS_DefineProperty(cx, class_obj, "prototype", v, nsnull, nsnull,
                             JSPROP_PERMANENT | JSPROP_READONLY)) {
      return NS_ERROR_UNEXPECTED;
    }

    *did_resolve = PR_TRUE;

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    nsRefPtr<nsDOMConstructor> constructor = new nsDOMConstructor(class_name);
    if (!constructor) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    jsval val;
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, obj, constructor, NS_GET_IID(nsIDOMConstructor), &val,
                    getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = constructor->Install(cx, obj, val);
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject* class_obj;
    holder->GetJSObject(&class_obj);
    NS_ASSERTION(class_obj, "Why didn't we get a JSObject?");

    *did_resolve = PR_TRUE;

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeProperty) {
    nsCOMPtr<nsISupports> native(do_CreateInstance(name_struct->mCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    jsval prop_val; // Property value.

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(native));
    if (owner) {
      nsIScriptContext *context = nsJSUtils::GetStaticScriptContext(cx, obj);
      NS_ENSURE_TRUE(context, NS_ERROR_UNEXPECTED);

      JSObject *prop_obj = nsnull;
      rv = owner->GetScriptObject(context, (void**)&prop_obj);
      NS_ENSURE_TRUE(prop_obj, NS_ERROR_UNEXPECTED);

      prop_val = OBJECT_TO_JSVAL(prop_obj);
    } else {
      JSObject *scope;

      if (aWin->IsOuterWindow()) {
        nsGlobalWindow *inner = aWin->GetCurrentInnerWindowInternal();
        NS_ENSURE_TRUE(inner, NS_ERROR_UNEXPECTED);

        scope = inner->GetGlobalJSObject();
      } else {
        scope = aWin->GetGlobalJSObject();
      }

      rv = WrapNative(cx, scope, native, NS_GET_IID(nsISupports), &prop_val,
                      getter_AddRefs(holder));
    }

    NS_ENSURE_SUCCESS(rv, rv);

    PRBool doSecurityCheckInAddProperty = sDoSecurityCheckInAddProperty;
    sDoSecurityCheckInAddProperty = PR_FALSE;

    JSBool ok = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                      ::JS_GetStringLength(str),
                                      prop_val, nsnull, nsnull,
                                      JSPROP_ENUMERATE);

    sDoSecurityCheckInAddProperty = doSecurityCheckInAddProperty;
    *did_resolve = PR_TRUE;

    return ok ? NS_OK : NS_ERROR_FAILURE;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeDynamicNameSet) {
    nsCOMPtr<nsIScriptExternalNameSet> nameset =
      do_CreateInstance(name_struct->mCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIScriptContext *context = aWin->GetContext();
    NS_ENSURE_TRUE(context, NS_ERROR_UNEXPECTED);

    rv = nameset->InitializeNameSet(context);

    *did_resolve = PR_TRUE;
  }

  return rv;
}

// Native code for window._content getter, this simply maps
// window._content to window.content for backwards compatibility only.
static JSBool JS_DLL_CALLBACK
ContentWindowGetter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                    jsval *rval)
{
  return ::JS_GetProperty(cx, obj, "content", rval);
}

NS_IMETHODIMP
nsWindowSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, jsval id, PRUint32 flags,
                       JSObject **objp, PRBool *_retval)
{
  nsGlobalWindow *win = nsGlobalWindow::FromWrapper(wrapper);

#ifdef DEBUG_SH_FORWARDING
  {
    nsDependentJSString str(::JS_ValueToString(cx, id));

    if (win->IsInnerWindow()) {
#ifdef DEBUG_PRINT_INNER
      printf("Property '%s' resolve on inner window %p\n",
             NS_ConvertUTF16toUTF8(str).get(), (void *)win);
#endif
    } else {
      printf("Property '%s' resolve on outer window %p\n",
             NS_ConvertUTF16toUTF8(str).get(), (void *)win);
    }
  }
#endif

  // Note, we won't forward resolve of the location property to the
  // inner window, we need to deal with that one for the outer too
  // since we've got special security protection code for that
  // property.  Also note that we want to enter this block even for
  // native wrappers, so that we'll ensure an inner window to wrap
  // against for the result of whatever we're getting.
  if (win->IsOuterWindow() && id != sLocation_id) {
    // XXXjst: Do security checks here when we remove the security
    // checks on the inner window.

    nsGlobalWindow *innerWin = win->GetCurrentInnerWindowInternal();

    if ((!innerWin || !innerWin->GetExtantDocument()) &&
        !win->IsCreatingInnerWindow()) {
      // We're resolving a property on an outer window for which there
      // is no inner window yet, and we're not in the midst of
      // creating the inner window or in the middle of initializing
      // XPConnect classes on it. If the context is already
      // initialized, force creation of a new inner window. This will
      // create a synthetic about:blank document, and an inner window
      // which may be reused by the actual document being loaded into
      // this outer window. This way properties defined on the window
      // before the document load started will be visible to the
      // document once it's loaded, assuming same origin etc.
      nsIScriptContext *scx = win->GetContextInternal();

      if (scx && scx->IsContextInitialized()) {
        // Grab the new inner window.
        innerWin = win->EnsureInnerWindowInternal();

        if (!innerWin) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }

    JSObject *innerObj;
    if (!ObjectIsNativeWrapper(cx, obj) &&
        innerWin && (innerObj = innerWin->GetGlobalJSObject())) {
#ifdef DEBUG_SH_FORWARDING
      printf(" --- Forwarding resolve to inner window %p\n", (void *)innerWin);
#endif

      jsid interned_id;
      JSObject *pobj;
      JSProperty *prop = nsnull;

      *_retval = (::JS_ValueToId(cx, id, &interned_id) &&
                  OBJ_LOOKUP_PROPERTY(cx, innerObj, interned_id, &pobj,
                                      &prop));

      if (*_retval && prop) {
#ifdef DEBUG_SH_FORWARDING
        printf(" --- Resolve on inner window found property.\n");
#endif

        OBJ_DROP_PROPERTY(cx, pobj, prop);

        *objp = pobj;
      }

      return NS_OK;
    }
  }

  if (!JSVAL_IS_STRING(id)) {
    if (JSVAL_IS_INT(id) && !(flags & JSRESOLVE_ASSIGNING)) {
      // If we're resolving a numeric property, treat that as if
      // window.frames[n] is resolved (since window.frames ===
      // window), if window.frames[n] is a child frame, define a
      // property for this index.

      nsCOMPtr<nsIDOMWindow> frame = GetChildFrame(win, id);

      if (frame) {
        // A numeric property accessed and the numeric property is a
        // child frame. Define a property for this index.

        PRBool doSecurityCheckInAddProperty = sDoSecurityCheckInAddProperty;
        sDoSecurityCheckInAddProperty = PR_FALSE;

        *_retval = ::JS_DefineElement(cx, obj, JSVAL_TO_INT(id), JSVAL_VOID,
                                      nsnull, nsnull, 0);

        sDoSecurityCheckInAddProperty = doSecurityCheckInAddProperty;

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

  JSContext *my_cx;

  if (!my_context) {
    my_cx = cx;
  } else {
    my_cx = (JSContext *)my_context->GetNativeContext();
  }

  // Resolving a standard class won't do any evil, and it's possible
  // for caps to get the answer wrong, so disable the security check
  // for this case.

  JSBool did_resolve = JS_FALSE;
  PRBool doSecurityCheckInAddProperty = sDoSecurityCheckInAddProperty;
  sDoSecurityCheckInAddProperty = PR_FALSE;

  JSAutoRequest ar(my_cx);

  // Don't resolve standard classes on XPCNativeWrapper.
  JSBool ok = !ObjectIsNativeWrapper(cx, obj) ?
              ::JS_ResolveStandardClass(my_cx, obj, id, &did_resolve) :
              JS_TRUE;

  sDoSecurityCheckInAddProperty = doSecurityCheckInAddProperty;

  if (!ok) {
    // Trust the JS engine (or the script security manager) to set
    // the exception in the JS engine.

    jsval exn;
    if (!JS_GetPendingException(my_cx, &exn)) {
      return NS_ERROR_UNEXPECTED;
    }

    // Return NS_OK to avoid stomping over the exception that was passed
    // down from the ResolveStandardClass call.
    // Note that the order of the JS_ClearPendingException and
    // JS_SetPendingException is important in the case that my_cx == cx.

    JS_ClearPendingException(my_cx);
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


  // Hmm, we do an awful lot of QIs here; maybe we should add a
  // method on an interface that would let us just call into the
  // window code directly...

  JSString *str = JSVAL_TO_STRING(id);

  // Don't resolve named frames on native wrappers
  if (!ObjectIsNativeWrapper(cx, obj)) {
    nsCOMPtr<nsIDocShellTreeNode> dsn(do_QueryInterface(win->GetDocShell()));

    PRInt32 count = 0;

    if (dsn) {
      dsn->GetChildCount(&count);
    }

    if (count > 0) {
      nsCOMPtr<nsIDocShellTreeItem> child;

      const jschar *chars = ::JS_GetStringChars(str);

      dsn->FindChildWithName(NS_REINTERPRET_CAST(const PRUnichar*, chars),
                             PR_FALSE, PR_TRUE, nsnull, nsnull,
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
                        NS_GET_IID(nsIDOMWindowInternal), &v,
                        getter_AddRefs(holder));
        NS_ENSURE_SUCCESS(rv, rv);

        // Script is accessing a child frame and this access can
        // potentially come from a context from a different domain.
        // ::JS_DefineUCProperty() will call
        // nsWindowSH::AddProperty(), and that method will do a
        // security check and that security check will fail since
        // other domains can't add properties to a global object in
        // this domain. Set the sDoSecurityCheckInAddProperty flag to
        // false (and set it to true immediagtely when we're done) to
        // tell nsWindowSH::AddProperty() that defining this new
        // property is 'ok' in this case, even if the call comes from
        // a different context.

        PRBool doSecurityCheckInAddProperty = sDoSecurityCheckInAddProperty;
        sDoSecurityCheckInAddProperty = PR_FALSE;

        JSAutoRequest ar(cx);

        PRBool ok = ::JS_DefineUCProperty(cx, obj, chars, ::JS_GetStringLength(str),
                                    v, nsnull, nsnull, 0);

        sDoSecurityCheckInAddProperty = doSecurityCheckInAddProperty;

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

    // Call GlobalResolve() after we call FindChildWithName() so
    // that named child frames will override external properties
    // which have been registered with the script namespace manager.

    JSBool did_resolve = JS_FALSE;
    rv = GlobalResolve(win, cx, obj, str, flags, &did_resolve);
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

    nsAutoGCRoot root(&funObj, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!::JS_DefineUCProperty(cx, windowObj, ::JS_GetStringChars(str),
                               ::JS_GetStringLength(str), JSVAL_VOID,
                               (JSPropertyOp)funObj, nsnull,
                               JSPROP_ENUMERATE | JSPROP_GETTER |
                               JSPROP_SHARED)) {
      return NS_ERROR_FAILURE;
    }

    *objp = obj;

    return NS_OK;
  }

  if (id == sLocation_id) {
    // This must be done even if we're just getting the value of
    // window.location (i.e. no checking flags & JSRESOLVE_ASSIGNING
    // here) since we must define window.location to prevent the
    // getter from being overriden (for security reasons).

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

    jsval v;
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, scope, location, NS_GET_IID(nsIDOMLocation), &v,
                    getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool doSecurityCheckInAddProperty = sDoSecurityCheckInAddProperty;
    sDoSecurityCheckInAddProperty = PR_FALSE;

    JSAutoRequest ar(cx);

    JSBool ok = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                 ::JS_GetStringLength(str), v, nsnull,
                                 nsnull, JSPROP_ENUMERATE);

    sDoSecurityCheckInAddProperty = doSecurityCheckInAddProperty;

    if (!ok) {
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

      if (!::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                ::JS_GetStringLength(str),
                                JSVAL_VOID, nsnull, nsnull,
                                JSPROP_ENUMERATE)) {
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
      rv = WrapNative(cx, obj, navigator, NS_GET_IID(nsIDOMNavigator), &v,
                      getter_AddRefs(holder));
      NS_ENSURE_SUCCESS(rv, rv);

      JSAutoRequest ar(cx);

      if (!::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                 ::JS_GetStringLength(str), v, nsnull, nsnull,
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

      jsval v;
      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      rv = WrapNative(cx, obj, document, NS_GET_IID(nsIDOMDocument), &v,
                      getter_AddRefs(holder));
      NS_ENSURE_SUCCESS(rv, rv);

      // The PostCreate hook for the document will handle defining the
      // property
      *objp = obj;

      return NS_OK;
    }

    if (id == sWindow_id) {
      // window should *always* be the outer window object.
      win = win->GetOuterWindowInternal();
      NS_ENSURE_TRUE(win, NS_ERROR_NOT_AVAILABLE);

      JSAutoRequest ar(cx);

      PRBool doSecurityCheckInAddProperty = sDoSecurityCheckInAddProperty;
      sDoSecurityCheckInAddProperty = PR_FALSE;

      PRBool ok =
        ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                              ::JS_GetStringLength(str),
                              OBJECT_TO_JSVAL(win->GetGlobalJSObject()),
                              nsnull, nsnull,
                              JSPROP_READONLY | JSPROP_ENUMERATE);

      sDoSecurityCheckInAddProperty = doSecurityCheckInAddProperty;

      if (!ok) {
        return NS_ERROR_FAILURE;
      }
      *objp = obj;

      return NS_OK;
    }

    // Do a security check when resolving heretofore unknown string
    // properties on window objects to prevent detection of a
    // property's existence across origins. We only do this when
    // resolving for a GET, no need to do it for set since we'll do
    // a security check in nsWindowSH::SetProperty() in that case.
    rv =
      doCheckPropertyAccess(cx, obj, id, wrapper,
                            nsIXPCSecurityManager::ACCESS_GET_PROPERTY,
                            PR_TRUE);
    if (NS_FAILED(rv)) {
      // Security check failed. The security manager set a JS
      // exception, we must make sure that exception is propagated, so
      // return NS_OK here.

      *_retval = PR_FALSE;

      return NS_OK;
    }
  }

  return nsEventReceiverSH::NewResolve(wrapper, cx, obj, id, flags, objp,
                                       _retval);
}

NS_IMETHODIMP
nsWindowSH::NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, PRUint32 enum_op, jsval *statep,
                         jsid *idp, PRBool *_retval)
{
  switch ((JSIterateOp)enum_op) {
    case JSENUMERATE_INIT:
    {
      // First, do the security check that nsDOMClassInfo does to see
      // if we need to do any work at all.
      nsDOMClassInfo::Enumerate(wrapper, cx, obj, _retval);
      if (!*_retval) {
        return NS_OK;
      }

      // The security check passed, let's see if we need to get the inner
      // window's JS object or if we can just start enumerating.
      nsGlobalWindow *win = nsGlobalWindow::FromWrapper(wrapper);
      JSObject *enumobj = win->GetGlobalJSObject();
      if (win->IsOuterWindow()) {
        nsGlobalWindow *inner = win->GetCurrentInnerWindowInternal();
        if (inner) {
          enumobj = inner->GetGlobalJSObject();
        }
      }

      // Great, we have the js object, now let's enumerate it.
      JSObject *iterator = JS_NewPropertyIterator(cx, enumobj);
      if (!iterator) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      *statep = OBJECT_TO_JSVAL(iterator);
      if (idp) {
        // Note: With these property iterators, we can't tell ahead of time how
        // many properties we're going to be iterating over.
        *idp = JSVAL_ZERO;
      }
      break;
    }
    case JSENUMERATE_NEXT:
    {
      JSObject *iterator = (JSObject*)JSVAL_TO_OBJECT(*statep);
      if (!JS_NextProperty(cx, iterator, idp)) {
        return NS_ERROR_UNEXPECTED;
      }

      if (*idp != JSVAL_VOID) {
        break;
      }

      // Fall through.
    }
    case JSENUMERATE_DESTROY:
      // Let GC at our iterator object.
      *statep = JSVAL_NULL;
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::Finalize(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JSObject *obj)
{
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(sgo, NS_ERROR_UNEXPECTED);

  sgo->OnFinalize(nsIProgrammingLanguage::JAVASCRIPT, obj);

  return nsEventReceiverSH::Finalize(wrapper, cx, obj);
}

NS_IMETHODIMP
nsWindowSH::Equality(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                     JSObject * obj, jsval val, PRBool *bp)
{
  *bp = PR_FALSE;

  if (JSVAL_IS_PRIMITIVE(val)) {
    return NS_OK;
  }

  nsCOMPtr<nsIXPConnectWrappedNative> other_wrapper;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(val),
                               getter_AddRefs(other_wrapper));
  if (!other_wrapper) {
    // Not equal.

    return NS_OK;
  }

  nsGlobalWindow *win = nsGlobalWindow::FromWrapper(wrapper);

  NS_ASSERTION(win->IsOuterWindow(),
               "Inner window detected in Equality hook!");

  nsCOMPtr<nsPIDOMWindow> other = do_QueryWrappedNative(other_wrapper);

  if (other) {
    NS_ASSERTION(other->IsOuterWindow(),
                 "Inner window detected in Equality hook!");

    *bp = win->GetOuterWindow() == other->GetOuterWindow();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::OuterObject(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                        JSObject * obj, JSObject * *_retval)
{
  nsGlobalWindow *win =
    nsGlobalWindow::FromWrapper(wrapper)->GetOuterWindowInternal();

  if (!win) {
    // If we no longer have an outer window. No code should ever be
    // running on a window w/o an outer, which means this hook should
    // never be called when we have no outer. But just in case, return
    // null to prevent leaking an inner window to code in a different
    // window.

    *_retval = nsnull;

    return NS_ERROR_UNEXPECTED;
  }

  // Return the outer window.

  *_retval = win->GetGlobalJSObject();

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::InnerObject(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                        JSObject * obj, JSObject * *_retval)
{
  nsGlobalWindow *win = nsGlobalWindow::FromWrapper(wrapper);

  if (win->IsInnerWindow() || win->IsFrozen()) {
    // Return the inner window, or the outer if we're dealing with a
    // frozen outer.

    *_retval = obj;
  } else {
    // Try to find the current inner window.

    nsGlobalWindow *inner = win->GetCurrentInnerWindowInternal();
    if (!inner) {
      // Yikes! No inner window! Instead of leaking the outer window into the
      // scope chain, let's return an error.

      *_retval = nsnull;

      return NS_ERROR_UNEXPECTED;
    }

    *_retval = inner->GetGlobalJSObject();
  }

  return NS_OK;
}

// DOM Location helper

NS_IMETHODIMP
nsLocationSH::CheckAccess(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, jsval id, PRUint32 mode,
                          jsval *vp, PRBool *_retval)
{
  if ((mode & JSACC_TYPEMASK) == JSACC_PROTO && (mode & JSACC_WRITE)) {
    // No setting location.__proto__, ever!

    // Let XPConnect know that the access was not granted.
    *_retval = PR_FALSE;

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

  if (sgo) {
    JSObject *global = sgo->GetGlobalJSObject();

    if (global) {
      *parentObj = global;
    }
  }

  return NS_OK;
}

// DOM Navigator helper
nsresult
nsNavigatorSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                         JSObject *globalObj, JSObject **parentObj)
{
  // window.navigator is persisted across document transitions if
  // we're loading a page from the same origin. Because of that we
  // need to parent the navigator wrapper at the outer window to avoid
  // holding on to the inner window where the navigator was initially
  // created too long.
  *parentObj = globalObj;

  nsCOMPtr<nsIDOMNavigator> safeNav(do_QueryInterface(nativeObj));
  if (!safeNav) {
    // Oops, this wasn't really a navigator object. This can happen if someone
    // tries to use our scriptable helper as a real object and tries to wrap
    // it, see bug 319296.
    return NS_OK;
  }

  nsNavigator *nav = (nsNavigator *)safeNav.get();
  nsIDocShell *ds = nav->GetDocShell();
  if (!ds) {
    NS_WARNING("Refusing to create a navigator in the wrong scope");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_GetInterface(ds);

  if (sgo) {
    JSObject *global = sgo->GetGlobalJSObject();

    if (global) {
      *parentObj = global;
    }
  }

  return NS_OK;
}

// DOM Node helper

PRBool
nsNodeSH::IsCapabilityEnabled(const char* aCapability)
{
  PRBool enabled;
  return sSecMan &&
    NS_SUCCEEDED(sSecMan->IsCapabilityEnabled(aCapability, &enabled)) &&
    enabled;
}

nsresult
nsNodeSH::DefineVoidProp(JSContext* cx, JSObject* obj, jsval id,
                         JSObject** objp)
{
  NS_ASSERTION(JSVAL_IS_STRING(id), "id must be a string");

  JSString* str = JSVAL_TO_STRING(id);

  // We might have a document here.
  PRBool doSecurityCheckInAddProperty = sDoSecurityCheckInAddProperty;
  sDoSecurityCheckInAddProperty = PR_FALSE;

  // We want this to be as invisible to content script as possible.  So
  // don't enumerate this, and set is as JSPROP_SHARED so it won't get
  // cached on the object.
  JSBool ok = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                    ::JS_GetStringLength(str), JSVAL_VOID,
                                    nsnull, nsnull, JSPROP_SHARED);

  sDoSecurityCheckInAddProperty = doSecurityCheckInAddProperty;

  if (!ok) {
    return NS_ERROR_FAILURE;
  }

  *objp = obj;
  return NS_OK;
}

NS_IMETHODIMP
nsNodeSH::PreCreate(nsISupports *nativeObj, JSContext *cx, JSObject *globalObj,
                    JSObject **parentObj)
{
  nsCOMPtr<nsINode> node(do_QueryInterface(nativeObj));
  NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);
  
  // Make sure that we get the owner document of the content node, in case
  // we're in document teardown.  If we are, it's important to *not* use
  // globalObj as the nodes parent since that would give the node the
  // principal of globalObj (i.e. the principal of the document that's being
  // loaded) and not the principal of the document that's being unloaded.
  // See http://bugzilla.mozilla.org/show_bug.cgi?id=227417
  nsIDocument* doc = node->GetOwnerDoc();

  if (!doc) {
    // No document reachable from nativeObj, use the global object
    // that was passed to this method.

    *parentObj = globalObj;

    return NS_OK;
  }

  nsISupports *native_parent;

  if (node->IsNodeOfType(nsINode::eELEMENT | nsINode::eXUL)) {
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
    if (node->IsNodeOfType(nsINode::eELEMENT |
                           nsIContent::eHTML |
                           nsIContent::eHTML_FORM_CONTROL)) {
      nsCOMPtr<nsIFormControl> form_control(do_QueryInterface(node));

      if (form_control) {
        nsCOMPtr<nsIDOMHTMLFormElement> form;
        form_control->GetForm(getter_AddRefs(form));

        if (form) {
          // Found a form, use it.
          native_parent = form;
        }
      }
    }
  } else {
    // We're called for a document object; set the parent to be the
    // document's global object, if there is one

    // Get the scope object from the document.
    native_parent = doc->GetScopeObject();

    if (!native_parent) {
      // No global object reachable from this document, use the
      // global object that was passed to this method.

      *parentObj = globalObj;

      return NS_OK;
    }
  }

  // XXXjst: Maybe we need to find the global to use from the
  // nsIScriptGlobalObject that's reachable from the node we're about
  // to wrap here? But that's not always reachable, let's use
  // globalObj for now...

  jsval v;
  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  nsresult rv = WrapNative(cx, globalObj, native_parent,
                           NS_GET_IID(nsISupports), &v,
                           getter_AddRefs(holder));

  *parentObj = JSVAL_TO_OBJECT(v);

  return rv;
}

NS_IMETHODIMP
nsNodeSH::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                      JSObject *obj, jsval id, jsval *vp, PRBool *_retval)
{
  nsDOMClassInfo::PreserveNodeWrapper(wrapper);
  return nsEventReceiverSH::AddProperty(wrapper, cx, obj, id, vp, _retval);
}

NS_IMETHODIMP
nsNodeSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JSObject *obj, jsval id, PRUint32 flags,
                     JSObject **objp, PRBool *_retval)
{
  if ((id == sBaseURIObject_id || id == sNodePrincipal_id) &&
      IsPrivilegedScript()) {
    return DefineVoidProp(cx, obj, id, objp);
  }

  return nsEventReceiverSH::NewResolve(wrapper, cx, obj, id, flags, objp,
                                       _retval);
}

NS_IMETHODIMP
nsNodeSH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                      JSObject *obj, jsval id, jsval *vp, PRBool *_retval)
{
  if (id == sBaseURIObject_id && IsPrivilegedScript()) {
    // I wish GetBaseURI lived on nsINode
    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIContent> content = do_QueryWrappedNative(wrapper);
    if (content) {
      uri = content->GetBaseURI();
      NS_ENSURE_TRUE(uri, NS_ERROR_OUT_OF_MEMORY);
    } else {
      nsCOMPtr<nsIDocument> doc = do_QueryWrappedNative(wrapper);
      NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

      uri = doc->GetBaseURI();
      NS_ENSURE_TRUE(uri, NS_ERROR_NOT_AVAILABLE);
    }

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult rv = WrapNative(cx, obj, uri, NS_GET_IID(nsIURI), vp,
                             getter_AddRefs(holder));
    return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
  }

  if (id == sNodePrincipal_id && IsPrivilegedScript()) {
    nsCOMPtr<nsINode> node = do_QueryWrappedNative(wrapper);
    NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult rv = WrapNative(cx, obj, node->NodePrincipal(),
                             NS_GET_IID(nsIPrincipal), vp,
                             getter_AddRefs(holder));
    return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
  }    

  // Note: none of our ancestors want GetProperty
  return NS_OK;
}

NS_IMETHODIMP
nsNodeSH::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                      JSObject *obj, jsval id, jsval *vp, PRBool *_retval)
{
  if ((id == sBaseURIObject_id || id == sNodePrincipal_id) &&
      IsPrivilegedScript()) {
    // Is there a better error we could use here?  We don't want privileged
    // script that can read these properties to set them, but _do_ want to
    // allow everyone else to.
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  return nsEventReceiverSH::SetProperty(wrapper, cx, obj, id, vp,_retval);
}

NS_IMETHODIMP
nsNodeSH::GetFlags(PRUint32 *aFlags)
{
  *aFlags = DOMCLASSINFO_STANDARD_FLAGS | nsIClassInfo::CONTENT_NODE;

  return NS_OK;
}

// EventReceiver helper

// static
PRBool
nsEventReceiverSH::ReallyIsEventName(jsval id, jschar aFirstChar)
{
  // I wonder if this is faster than using a hash...

  switch (aFirstChar) {
  case 'a' :
    return id == sOnabort_id;
  case 'b' :
    return (id == sOnbeforeunload_id ||
            id == sOnblur_id);
  case 'e' :
    return id == sOnerror_id;
  case 'f' :
    return id == sOnfocus_id;
  case 'c' :
    return (id == sOnchange_id       ||
            id == sOnclick_id        ||
            id == sOncontextmenu_id);
  case 'd' :
    return id == sOndblclick_id;
  case 'l' :
    return id == sOnload_id;
  case 'p' :
    return (id == sOnpaint_id        ||
            id == sOnpageshow_id     ||
            id == sOnpagehide_id);
  case 'k' :
    return (id == sOnkeydown_id      ||
            id == sOnkeypress_id     ||
            id == sOnkeyup_id);
  case 'u' :
    return id == sOnunload_id;
  case 'm' :
    return (id == sOnmousemove_id    ||
            id == sOnmouseout_id     ||
            id == sOnmouseover_id    ||
            id == sOnmouseup_id      ||
            id == sOnmousedown_id);
  case 'r' :
    return (id == sOnreset_id        ||
            id == sOnresize_id);
  case 's' :
    return (id == sOnscroll_id       ||
            id == sOnselect_id       ||
            id == sOnsubmit_id);
  }

  return PR_FALSE;
}

// static
JSBool JS_DLL_CALLBACK
nsEventReceiverSH::AddEventListenerHelper(JSContext *cx, JSObject *obj,
                                          uintN argc, jsval *argv, jsval *rval)
{
  if (argc < 3 || argc > 4) {
    ThrowJSException(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    return JS_FALSE;
  }

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  nsresult rv =
    sXPConnect->GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wrapper));
  if (NS_FAILED(rv)) {
    nsDOMClassInfo::ThrowJSException(cx, rv);

    return JS_FALSE;
  }

  if (JSVAL_IS_PRIMITIVE(argv[1])) {
    // The second argument must be a function, or a
    // nsIDOMEventListener. Throw an error.
    ThrowJSException(cx, NS_ERROR_XPC_BAD_CONVERT_JS);

    return JS_FALSE;
  }

  JSString* jsstr = JS_ValueToString(cx, argv[0]);
  if (!jsstr) {
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_OUT_OF_MEMORY);

    return JS_FALSE;
  }

  nsDependentJSString type(jsstr);

  nsCOMPtr<nsIDOMEventListener> listener;

  {
    nsCOMPtr<nsISupports> tmp;
    sXPConnect->WrapJS(cx, JSVAL_TO_OBJECT(argv[1]),
                       NS_GET_IID(nsIDOMEventListener), getter_AddRefs(tmp));

    listener = do_QueryInterface(tmp, &rv);
    if (NS_FAILED(rv)) {
      ThrowJSException(cx, rv);

      return JS_FALSE;
    }
  }

  JSBool useCapture;
  if (!JS_ValueToBoolean(cx, argv[2], &useCapture)) {
    return JS_FALSE;
  }

  if (argc == 4) {
    JSBool wantsUntrusted;
    if (!JS_ValueToBoolean(cx, argv[3], &wantsUntrusted)) {
      return JS_FALSE;
    }

    nsCOMPtr<nsIDOMNSEventTarget> eventTarget(do_QueryWrappedNative(wrapper,
                                                                    &rv));
    if (NS_FAILED(rv)) {
      ThrowJSException(cx, rv);

      return JS_FALSE;
    }

    rv = eventTarget->AddEventListener(type, listener, useCapture,
                                       wantsUntrusted);
    if (NS_FAILED(rv)) {
      ThrowJSException(cx, rv);

      return JS_FALSE;
    }
  } else {
    nsCOMPtr<nsIDOMEventTarget> eventTarget(do_QueryWrappedNative(wrapper,
                                                                  &rv));
    if (NS_FAILED(rv)) {
      ThrowJSException(cx, rv);

      return JS_FALSE;
    }

    rv = eventTarget->AddEventListener(type, listener, useCapture);
    if (NS_FAILED(rv)) {
      ThrowJSException(cx, rv);

      return JS_FALSE;
    }
  }
  
  return JS_TRUE;
}

nsresult
nsEventReceiverSH::RegisterCompileHandler(nsIXPConnectWrappedNative *wrapper,
                                          JSContext *cx, JSObject *obj,
                                          jsval id, PRBool compile,
                                          PRBool remove,
                                          PRBool *did_define)
{
  NS_PRECONDITION(!compile || !remove,
                  "Can't both compile and remove at the same time");
  *did_define = PR_FALSE;

  if (!IsEventName(id)) {
    return NS_OK;
  }

  if (ObjectIsNativeWrapper(cx, obj)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsIScriptContext *script_cx = nsJSUtils::GetStaticScriptContext(cx, obj);
  NS_ENSURE_TRUE(script_cx, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryWrappedNative(wrapper));
  if (!receiver) {
    // Doesn't do events
#ifdef DEBUG
    nsCOMPtr<nsIAttribute> attr = do_QueryWrappedNative(wrapper);
    NS_WARN_IF_FALSE(attr, "Non-attr doesn't QI to nsIDOMEventReceiver?");
#endif
    return NS_OK;
  }
  
  nsCOMPtr<nsIEventListenerManager> manager;
  receiver->GetListenerManager(PR_TRUE, getter_AddRefs(manager));
  NS_ENSURE_TRUE(manager, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIAtom> atom(do_GetAtom(nsDependentJSString(id)));
  NS_ENSURE_TRUE(atom, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;

  JSObject *scope = GetGlobalJSObject(cx, obj);

  if (compile) {
    rv = manager->CompileScriptEventListener(script_cx, scope, receiver, atom,
                                             did_define);
  } else if (remove) {
    rv = manager->RemoveScriptEventListener(atom);
  } else {
    rv = manager->RegisterScriptEventListener(script_cx, scope, receiver,
                                              atom);
  }

  return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
}

NS_IMETHODIMP
nsEventReceiverSH::NewResolve(nsIXPConnectWrappedNative *wrapper,
                              JSContext *cx, JSObject *obj, jsval id,
                              PRUint32 flags, JSObject **objp, PRBool *_retval)
{
  if (id == sOnload_id || id == sOnerror_id) {    
    // Make sure that this node can't go away while waiting for a
    // network load that could fire an event handler.
    nsDOMClassInfo::PreserveNodeWrapper(wrapper);
  }

  if (!JSVAL_IS_STRING(id)) {
    return NS_OK;
  }

  if (flags & JSRESOLVE_ASSIGNING) {
    if (!IsEventName(id)) {
      // Bail out.  We don't care about this assignment.
      return NS_OK;
    }

    // If we're assigning to an on* property, just resolve to null for
    // now; the assignment will then set the right value.
    JSString* str = JSVAL_TO_STRING(id);
    JSAutoRequest ar(cx);
    // Make sure the flags here match those in
    // nsJSContext::BindCompiledEventHandler
    if (!::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                               ::JS_GetStringLength(str), JSVAL_NULL,
                               nsnull, nsnull,
                               JSPROP_ENUMERATE | JSPROP_PERMANENT)) {
      return NS_ERROR_FAILURE;
    }

    *objp = obj;
    return NS_OK;
  }

  if (id == sAddEventListener_id) {
    JSString *str = JSVAL_TO_STRING(id);
    // addEventListener always takes at least 3 arguments.
    JSFunction *fnc =
      ::JS_DefineFunction(cx, obj, ::JS_GetStringBytes(str),
                          AddEventListenerHelper, 3, JSPROP_ENUMERATE);

    *objp = obj;

    return fnc ? NS_OK : NS_ERROR_UNEXPECTED;
  }

  PRBool did_define = PR_FALSE;
  nsresult rv = RegisterCompileHandler(wrapper, cx, obj, id, PR_TRUE, PR_FALSE,
                                       &did_define);
  NS_ENSURE_SUCCESS(rv, rv);

  if (did_define) {
    *objp = obj;
  }

  return nsDOMGenericSH::NewResolve(wrapper, cx, obj, id, flags, objp,
                                    _retval);
}

NS_IMETHODIMP
nsEventReceiverSH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                               JSContext *cx, JSObject *obj, jsval id,
                               jsval *vp, PRBool *_retval)
{
  JSAutoRequest ar(cx);

  if ((::JS_TypeOfValue(cx, *vp) != JSTYPE_FUNCTION && !JSVAL_IS_NULL(*vp)) ||
      !JSVAL_IS_STRING(id) || id == sAddEventListener_id) {
    return NS_OK;
  }

  PRBool did_compile; // Ignored here.

  return RegisterCompileHandler(wrapper, cx, obj, id, PR_FALSE,
                                JSVAL_IS_NULL(*vp), &did_compile);
}

NS_IMETHODIMP
nsEventReceiverSH::AddProperty(nsIXPConnectWrappedNative *wrapper,
                               JSContext *cx, JSObject *obj, jsval id,
                               jsval *vp, PRBool *_retval)
{
  return nsEventReceiverSH::SetProperty(wrapper, cx, obj, id, vp, _retval);
}


// Element helper

NS_IMETHODIMP
nsElementSH::PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj)
{
  nsresult rv = nsNodeSH::PostCreate(wrapper, cx, obj);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> content(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(content, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDocument> doc = content->GetDocument();

  if (!doc) {
    // There's no baseclass that cares about this call so we just
    // return here.

    return NS_OK;
  }

  // See if we have a frame.
  nsIPresShell *shell = doc->GetShellAt(0);

  if (!shell) {
    return NS_OK;
  }

  nsIFrame* frame = shell->GetPrimaryFrameFor(content);

  if (frame) {
    // If we have a frame the frame has already loaded the binding.

    return NS_OK;
  }

  // We must ensure that the XBL Binding is installed before we hand
  // back this object.

  if (doc->BindingManager()->GetBinding(content)) {
    // There's already a binding for this element so nothing left to
    // be done here.

    return NS_OK;
  }

  // Get the computed -moz-binding directly from the style context
  nsPresContext *pctx = shell->GetPresContext();
  NS_ENSURE_TRUE(pctx, NS_ERROR_UNEXPECTED);

  nsRefPtr<nsStyleContext> sc = pctx->StyleSet()->ResolveStyleFor(content,
                                                                  nsnull);
  NS_ENSURE_TRUE(sc, NS_ERROR_FAILURE);

  nsIURI *bindingURL = sc->GetStyleDisplay()->mBinding;
  if (!bindingURL) {
    // No binding, nothing left to do here.
    return NS_OK;
  }

  // We have a binding that must be installed.
  PRBool dummy;

  nsCOMPtr<nsIXBLService> xblService(do_GetService("@mozilla.org/xbl;1"));
  NS_ENSURE_TRUE(xblService, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<nsXBLBinding> binding;
  xblService->LoadBindings(content, bindingURL, PR_FALSE,
                           getter_AddRefs(binding), &dummy);

  if (binding) {
    binding->ExecuteAttachedHandler();
  }

  return NS_OK;
}

// Generic array scriptable helper.

NS_IMETHODIMP
nsGenericArraySH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, jsval id, PRUint32 flags,
                             JSObject **objp, PRBool *_retval)
{
  PRBool is_number = PR_FALSE;
  PRInt32 n = GetArrayIndexFromId(cx, id, &is_number);

  if (is_number && n >= 0) {
    // XXX The following is a cheap optimization to avoid hitting xpconnect to
    // get the length. We may want to consider asking our concrete
    // implementation for the length, and falling back onto the GetProperty if
    // it doesn't provide one.

    PRUint32 length;

    nsCOMPtr<nsIDOMNodeList> map = do_QueryWrappedNative(wrapper);
    if (map) {
      // Fast path: Get the length from our map.

      map->GetLength(&length);
    } else {
      // Slow path: We don't know how to get the length in a fast way, ask our
      // implementation.

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

      length = (PRUint32)slen;
    }

    if ((PRUint32)n < length) {
      *_retval = ::JS_DefineElement(cx, obj, n, JSVAL_VOID, nsnull, nsnull,
                                    JSPROP_ENUMERATE | JSPROP_SHARED);
      *objp = obj;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericArraySH::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, PRBool *_retval)
{
  // Recursion protection in case someone tries to be smart and call
  // the enumerate hook from a user defined .length getter, or
  // somesuch.

  static PRBool sCurrentlyEnumerating;

  if (sCurrentlyEnumerating) {
    // Don't recurse to death.
    return NS_OK;
  }

  sCurrentlyEnumerating = PR_TRUE;

  jsval len_val;
  JSAutoRequest ar(cx);
  JSBool ok = ::JS_GetProperty(cx, obj, "length", &len_val);

  if (ok && JSVAL_IS_INT(len_val)) {
    PRInt32 length = JSVAL_TO_INT(len_val);
    char buf[11];

    for (PRInt32 i = 0; ok && i < length; ++i) {
      PR_snprintf(buf, sizeof(buf), "%d", i);

      ok = ::JS_DefineProperty(cx, obj, buf, JSVAL_VOID, nsnull, nsnull,
                               JSPROP_ENUMERATE | JSPROP_SHARED);
    }
  }

  sCurrentlyEnumerating = PR_FALSE;

  return ok ? NS_OK : NS_ERROR_UNEXPECTED;
}

// NodeList scriptable helper

nsresult
nsArraySH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                     nsISupports **aResult)
{
  nsCOMPtr<nsIDOMNodeList> list(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(list, NS_ERROR_UNEXPECTED);

  nsIDOMNode *node = nsnull; // Weak, transfer the ownership over to aResult
  nsresult rv = list->Item(aIndex, &node);

  *aResult = node;

  return rv;
}

NS_IMETHODIMP
nsArraySH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, jsval id, jsval *vp, PRBool *_retval)
{
  PRBool is_number = PR_FALSE;
  PRInt32 n = GetArrayIndexFromId(cx, id, &is_number);

  nsresult rv = NS_OK;

  if (is_number) {
    if (n < 0) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    nsCOMPtr<nsISupports> array_item;

    rv = GetItemAt(wrapper->Native(), n, getter_AddRefs(array_item));
    NS_ENSURE_SUCCESS(rv, rv);

    if (array_item) {
      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      rv = WrapNative(cx, obj, array_item, NS_GET_IID(nsISupports), vp,
                      getter_AddRefs(holder));
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

  return list->Item(aIndex, aResult);
}


// Named Array helper

NS_IMETHODIMP
nsNamedArraySH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval id, jsval *vp,
                            PRBool *_retval)
{
  if (JSVAL_IS_STRING(id) && !ObjectIsNativeWrapper(cx, obj)) {
    nsCOMPtr<nsISupports> item;
    nsresult rv = GetNamedItem(wrapper->Native(), nsDependentJSString(id),
                               getter_AddRefs(item));
    NS_ENSURE_SUCCESS(rv, rv);

    if (item) {
      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      rv = WrapNative(cx, obj, item, NS_GET_IID(nsISupports), vp,
                      getter_AddRefs(holder));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = NS_SUCCESS_I_DID_SOMETHING;
    }

    // Don't fall through to nsArraySH::GetProperty() here
    return rv;
  }

  return nsArraySH::GetProperty(wrapper, cx, obj, id, vp, _retval);
}


// NamedNodeMap helper

nsresult
nsNamedNodeMapSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                            nsISupports **aResult)
{
  nsCOMPtr<nsIDOMNamedNodeMap> map(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(map, NS_ERROR_UNEXPECTED);

  nsIDOMNode *node = nsnull; // Weak, transfer the ownership over to aResult
  nsresult rv = map->Item(aIndex, &node);

  *aResult = node;

  return rv;
}

nsresult
nsNamedNodeMapSH::GetNamedItem(nsISupports *aNative, const nsAString& aName,
                               nsISupports **aResult)
{
  nsCOMPtr<nsIDOMNamedNodeMap> map(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(map, NS_ERROR_UNEXPECTED);

  nsIDOMNode *node = nsnull; // Weak, transfer the ownership over to aResult
  nsresult rv = map->GetNamedItem(aName, &node);

  *aResult = node;

  return rv;
}


// HTMLCollection helper

nsresult
nsHTMLCollectionSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                              nsISupports **aResult)
{
  nsCOMPtr<nsIDOMHTMLCollection> collection(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(collection, NS_ERROR_UNEXPECTED);

  nsIDOMNode *node = nsnull; // Weak, transfer the ownership over to aResult
  nsresult rv = collection->Item(aIndex, &node);

  *aResult = node;

  return rv;
}

nsresult
nsHTMLCollectionSH::GetNamedItem(nsISupports *aNative,
                                 const nsAString& aName,
                                 nsISupports **aResult)
{
  nsCOMPtr<nsIDOMHTMLCollection> collection(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(collection, NS_ERROR_UNEXPECTED);

  nsIDOMNode *node = nsnull; // Weak, transfer the ownership over to aResult
  nsresult rv = collection->NamedItem(aName, &node);

  *aResult = node;

  return rv;
}


// ContentList helper
nsresult
nsContentListSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                           JSObject *globalObj, JSObject **parentObj)
{
  nsCOMPtr<nsIDOMNodeList> nodeList(do_QueryInterface(nativeObj));
  nsContentList *contentList =
    NS_STATIC_CAST(nsContentList*, NS_STATIC_CAST(nsIDOMNodeList*, nodeList));

  if (!contentList) {
    return NS_OK;
  }

  nsISupports *native_parent = contentList->GetParentObject();

  if (!native_parent) {
    *parentObj = globalObj;
    return NS_OK;
  }

  jsval v;
  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  nsresult rv = WrapNative(cx, globalObj, native_parent,
                           NS_GET_IID(nsISupports), &v,
                           getter_AddRefs(holder));

  *parentObj = JSVAL_TO_OBJECT(v);

  return rv;
}

// FormControlList helper

nsresult
nsFormControlListSH::GetNamedItem(nsISupports *aNative,
                                  const nsAString& aName,
                                  nsISupports **aResult)
{
  nsCOMPtr<nsIDOMNSHTMLFormControlList> list(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(list, NS_ERROR_UNEXPECTED);

  return list->NamedItem(aName, aResult);
}

// Document helper for document.location and document.on*

static inline PRBool
documentNeedsSecurityCheck(JSContext *cx, nsIXPConnectWrappedNative *wrapper)
{
  // We cache a pointer to a wrapper and a context that we've last vetted
  // and cache what the verdict was.

  if (cx == cached_doc_cx && wrapper == cached_doc_wrapper) {
    return cached_doc_needs_check;
  }

  cached_doc_cx = cx;
  cached_doc_wrapper = wrapper;
  
  // Get the JS object from the wrapper
  JSObject *wrapper_obj = nsnull;
  wrapper->GetJSObject(&wrapper_obj);

  JSObject *wrapper_global = GetGlobalJSObject(cx, wrapper_obj);

#ifdef DEBUG
  {
    JSClass *clazz = JS_GET_CLASS(cx, wrapper_obj);

    NS_ASSERTION(clazz && clazz->flags & JSCLASS_HAS_PRIVATE &&
                 clazz->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS,
                 "Bad class for Document object!");
  }
#endif

  // Check if the calling function comes from the same scope that the
  // wrapper comes from. If that's the case, or if there's no JS
  // running at the moment (i.e. someone is using the JS engine API
  // directly to access a property on a JS object) there's no need to
  // do a security check.

  JSObject *function_obj = nsnull;
  JSStackFrame *fp = nsnull;

  // Initialize to false to handle the case where there's no JS running
  // on the current context (e.g., we're getting here from a property
  // access from the JS API).  Since the scope chain is immutable, it's
  // OK to keep skipping the check.

  cached_doc_needs_check = PR_FALSE;

  do {
    fp = ::JS_FrameIterator(cx, &fp);
    if (!fp) {
      // Clear cached_doc_cx so that we don't really cache this return
      // value. If we hit this case, then we didn't really have enough
      // information about the currently running code to make any long-term
      // decisions.

      cached_doc_cx = nsnull;
      return cached_doc_needs_check;
    }

    function_obj = ::JS_GetFrameFunctionObject(cx, fp);

    // Since we're here, we know that there is some JS running. Now, we
    // need to default to being paranoid, and can only skip the security
    // check if we find that the currently-running function is from the
    // same scope.

    cached_doc_needs_check = PR_TRUE;
  } while (!function_obj);

  // Get the global object that the calling function comes from.
  JSObject *function_global = GetGlobalJSObject(cx, function_obj);

  if (function_global != wrapper_global) {
    // The global object we're trying to access a property on is not
    // from the scope that the calling function comes from. Do a
    // security check.

    return PR_TRUE;
  }

  // We're called from the same context as the context in the global
  // object in the scope that wrapper came from, no need to do a
  // security check now.
  cached_doc_needs_check = PR_FALSE;

  return PR_FALSE;
}

NS_IMETHODIMP
nsDocumentSH::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, jsval id, jsval *vp,
                          PRBool *_retval)
{
  // If we're in a state where we're not supposed to do a security
  // check, return early.
  if (!sDoSecurityCheckInAddProperty) {
    return NS_OK;
  }

  if (id == sLocation_id) {
    // Don't allow adding a document.location setter or getter, allowing
    // that could lead to security bugs (see bug 143369).

    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentSH::DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, jsval id, jsval *vp,
                          PRBool *_retval)
{
  if (id == sLocation_id) {
    // Don't allow deleting document.location, allowing that could lead
    // to security bugs (see bug 143369).

    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, PRUint32 flags,
                         JSObject **objp, PRBool *_retval)
{
  nsresult rv;

  if (id == sLocation_id) {
    // This must be done even if we're just getting the value of
    // document.location (i.e. no checking flags & JSRESOLVE_ASSIGNING
    // here) since we must define document.location to prevent the
    // getter from being overriden (for security reasons).

    nsCOMPtr<nsIDOMNSDocument> doc(do_QueryWrappedNative(wrapper));
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMLocation> location;
    rv = doc->GetLocation(getter_AddRefs(location));
    NS_ENSURE_SUCCESS(rv, rv);

    jsval v;

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, obj, location, NS_GET_IID(nsIDOMLocation), &v,
                    getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool doSecurityCheckInAddProperty = sDoSecurityCheckInAddProperty;
    sDoSecurityCheckInAddProperty = PR_FALSE;

    JSAutoRequest ar(cx);

    JSString *str = JSVAL_TO_STRING(id);
    JSBool ok = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                      ::JS_GetStringLength(str), v, nsnull,
                                      nsnull, JSPROP_ENUMERATE);

    sDoSecurityCheckInAddProperty = doSecurityCheckInAddProperty;

    if (!ok) {
      return NS_ERROR_FAILURE;
    }

    *objp = obj;

    return NS_OK;
  }

  if (documentNeedsSecurityCheck(cx, wrapper)) {
    PRUint32 accessType;

    if (flags & JSRESOLVE_ASSIGNING)
      accessType = nsIXPCSecurityManager::ACCESS_SET_PROPERTY;
    else
      accessType = nsIXPCSecurityManager::ACCESS_GET_PROPERTY;

    // Do a security check when resolving heretofore unknown string
    // properties on document objects to prevent detection of a
    // property's existence across origins.
    rv = doCheckPropertyAccess(cx, obj, id, wrapper, accessType, PR_FALSE);

    if (NS_FAILED(rv)) {
      // Security check failed. The security manager set a JS exception,
      // we must make sure that exception is propagated, so return NS_OK
      // here.

      *_retval = PR_FALSE;
      
      return NS_OK;
    }
  }

  if (id == sDocumentURIObject_id && IsPrivilegedScript()) {
    return DefineVoidProp(cx, obj, id, objp);
  } 

  return nsNodeSH::NewResolve(wrapper, cx, obj, id, flags, objp, _retval);
}

NS_IMETHODIMP
nsDocumentSH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, jsval id, jsval *vp, PRBool *_retval)
{
  if (documentNeedsSecurityCheck(cx, wrapper)) {
    nsresult rv =
      doCheckPropertyAccess(cx, obj, id, wrapper,
                            nsIXPCSecurityManager::ACCESS_GET_PROPERTY,
                            PR_FALSE);

    if (NS_FAILED(rv)) {
      // Security check failed. The security manager set a JS
      // exception, we must make sure that exception is propagated.

      *_retval = PR_FALSE;

      *vp = JSVAL_NULL;
    }
  }

  if (id == sDocumentURIObject_id && IsPrivilegedScript()) {
    nsCOMPtr<nsIDocument> doc = do_QueryWrappedNative(wrapper);
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    nsIURI* uri = doc->GetDocumentURI();
    NS_ENSURE_TRUE(uri, NS_ERROR_NOT_AVAILABLE);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult rv = WrapNative(cx, obj, uri, NS_GET_IID(nsIURI), vp,
                             getter_AddRefs(holder));

    return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
  }

  return nsNodeSH::GetProperty(wrapper, cx, obj, id, vp, _retval);
}

NS_IMETHODIMP
nsDocumentSH::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, jsval id, jsval *vp, PRBool *_retval)
{
  if (documentNeedsSecurityCheck(cx, wrapper)) {
    nsresult rv =
      doCheckPropertyAccess(cx, obj, id, wrapper,
                            nsIXPCSecurityManager::ACCESS_SET_PROPERTY,
                            PR_FALSE);

    if (NS_FAILED(rv)) {
      // Security check failed. The security manager set a JS
      // exception, we must make sure that exception is propagated.

      *_retval = PR_FALSE;

      return NS_OK;
    }
  }

  if (id == sLocation_id) {
    nsCOMPtr<nsIDOMNSDocument> doc(do_QueryWrappedNative(wrapper));
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMLocation> location;

    nsresult rv = doc->GetLocation(getter_AddRefs(location));
    NS_ENSURE_SUCCESS(rv, rv);

    if (location) {
      JSAutoRequest ar(cx);

      JSString *val = ::JS_ValueToString(cx, *vp);
      NS_ENSURE_TRUE(val, NS_ERROR_UNEXPECTED);

      rv = location->SetHref(nsDependentJSString(val));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      rv = WrapNative(cx, obj, location, NS_GET_IID(nsIDOMLocation), vp,
                      getter_AddRefs(holder));
      return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
    }
  }

  if (id == sDocumentURIObject_id && IsPrivilegedScript()) {
    // Is there a better error we could use here?  We don't want privileged
    // script that can read this property to set it, but _do_ want to allow
    // everyone else to.
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }
  
  return nsNodeSH::SetProperty(wrapper, cx, obj, id, vp, _retval);
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
  nsresult rv = nsNodeSH::PostCreate(wrapper, cx, obj);
  NS_ENSURE_SUCCESS(rv, rv);

  // If this is the current document for the window that's the script global
  // object of this document, then define this document object on the window.
  // That will make sure that the document is referenced (via window.document)
  // and prevent it from going away in GC.
  nsCOMPtr<nsIDocument> doc = do_QueryWrappedNative(wrapper);
  if (!doc) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsPIDOMWindow> win =
    do_QueryInterface(doc->GetScriptGlobalObject());
  if (!win) {
    // No window, nothing else to do here
    return NS_OK;
  }

  nsIDOMDocument* currentDoc = win->GetExtantDocument();

  if (SameCOMIdentity(doc, currentDoc)) {
    jsval winVal;

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, obj, win, NS_GET_IID(nsIDOMWindow), &winVal,
                    getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_NAMED_LITERAL_STRING(doc_str, "document");

    if (!::JS_DefineUCProperty(cx, JSVAL_TO_OBJECT(winVal),
                               NS_REINTERPRET_CAST(const jschar *,
                                                   doc_str.get()),
                               doc_str.Length(), OBJECT_TO_JSVAL(obj), nsnull,
                               nsnull, JSPROP_READONLY | JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }    
  }
  return NS_OK;
}

// HTMLDocument helper

// static
nsresult
nsHTMLDocumentSH::ResolveImpl(JSContext *cx,
                              nsIXPConnectWrappedNative *wrapper, jsval id,
                              nsISupports **result)
{
  nsCOMPtr<nsIHTMLDocument> doc(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  // 'id' is not always a string, it can be a number since document.1
  // should map to <input name="1">. Thus we can't use
  // JSVAL_TO_STRING() here.
  JSString *str = JS_ValueToString(cx, id);
  NS_ENSURE_TRUE(str, NS_ERROR_UNEXPECTED);

  return doc->ResolveName(nsDependentJSString(str), nsnull, result);
}

// static
JSBool JS_DLL_CALLBACK
nsHTMLDocumentSH::DocumentOpen(JSContext *cx, JSObject *obj, uintN argc,
                               jsval *argv, jsval *rval)
{
  if (argc > 2) {
    JSObject *global = GetGlobalJSObject(cx, obj);

    // DOM0 quirk that makes document.open() call window.open() if
    // called with 3 or more arguments.

    return ::JS_CallFunctionName(cx, global, "open", argc, argv, rval);
  }

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;

  nsresult rv =
    sXPConnect->GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wrapper));
  if (NS_FAILED(rv)) {
    nsDOMClassInfo::ThrowJSException(cx, rv);

    return JS_FALSE;
  }

  nsCOMPtr<nsIDOMNSHTMLDocument> doc(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(doc, JS_FALSE);

  nsCAutoString contentType("text/html");
  if (argc > 0) {
    JSString* jsstr = JS_ValueToString(cx, argv[0]);
    if (!jsstr) {
      nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_OUT_OF_MEMORY);
      return JS_FALSE;
    }
    nsAutoString type;
    type.Assign(nsDependentJSString(jsstr));
    ToLowerCase(type);
    nsCAutoString actualType, dummy;
    NS_ParseContentType(NS_ConvertUTF16toUTF8(type), actualType, dummy);
    if (!actualType.EqualsLiteral("text/html") &&
        !type.EqualsLiteral("replace")) {
      contentType = "text/plain";
    }
  }
  
  PRBool replace = PR_FALSE;
  if (argc > 1) {
    JSString* jsstr = JS_ValueToString(cx, argv[1]);
    if (!jsstr) {
      nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_OUT_OF_MEMORY);
      return JS_FALSE;
    }

    replace = NS_LITERAL_STRING("replace").
      Equals(NS_REINTERPRET_CAST(const PRUnichar*,
                                 ::JS_GetStringChars(jsstr)));
  }

  nsCOMPtr<nsIDOMDocument> retval;
  rv = doc->Open(contentType, replace, getter_AddRefs(retval));
  if (NS_FAILED(rv)) {
    nsDOMClassInfo::ThrowJSException(cx, rv);

    return JS_FALSE;
  }

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  rv = WrapNative(cx, obj, retval, NS_GET_IID(nsIDOMDocument), rval,
                  getter_AddRefs(holder));
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to wrap native!");

  return NS_SUCCEEDED(rv);
}


static JSClass sHTMLDocumentAllClass = {
  "HTML document.all class",
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_NEW_RESOLVE |
  JSCLASS_HAS_RESERVED_SLOTS(1),
  JS_PropertyStub, JS_PropertyStub, nsHTMLDocumentSH::DocumentAllGetProperty,
  JS_PropertyStub, JS_EnumerateStub,
  (JSResolveOp)nsHTMLDocumentSH::DocumentAllNewResolve, JS_ConvertStub,
  nsHTMLDocumentSH::ReleaseDocument, nsnull, nsnull,
  nsHTMLDocumentSH::CallToGetPropMapper
};


static JSClass sHTMLDocumentAllHelperClass = {
  "HTML document.all helper class", JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
  JS_PropertyStub, JS_PropertyStub,
  nsHTMLDocumentSH::DocumentAllHelperGetProperty,
  JS_PropertyStub, JS_EnumerateStub,
  (JSResolveOp)nsHTMLDocumentSH::DocumentAllHelperNewResolve, JS_ConvertStub,
  JS_FinalizeStub
};


static JSClass sHTMLDocumentAllTagsClass = {
  "HTML document.all.tags class",
  JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, (JSResolveOp)nsHTMLDocumentSH::DocumentAllTagsNewResolve,
  JS_ConvertStub, nsHTMLDocumentSH::ReleaseDocument, nsnull, nsnull,
  nsHTMLDocumentSH::CallToGetPropMapper
};

// static
JSBool
nsHTMLDocumentSH::GetDocumentAllNodeList(JSContext *cx, JSObject *obj,
                                         nsIDOMDocument *domdoc,
                                         nsIDOMNodeList **nodeList)
{
  // The document.all object is a mix of the node list returned by
  // document.getElementsByTagName("*") and a map of elements in the
  // document exposed by their id and/or name. To make access to the
  // node list part (i.e. access to elements by index) not walk the
  // document each time, we create a nsContentList and hold on to it
  // in a reserved slot (0) on the document.all JSObject.
  jsval collection;
  nsresult rv = NS_OK;

  if (!JS_GetReservedSlot(cx, obj, 0, &collection)) {
    return JS_FALSE;
  }

  if (!JSVAL_IS_PRIMITIVE(collection)) {
    // We already have a node list in our reserved slot, use it.

    nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
    rv |=
      sXPConnect->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(collection),
                                             getter_AddRefs(wrapper));

    if (wrapper) {
      CallQueryInterface(wrapper->Native(), nodeList);
    }
  } else {
    // No node list for this document.all yet, create one...

    rv |= domdoc->GetElementsByTagName(NS_LITERAL_STRING("*"), nodeList);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv |= nsDOMClassInfo::WrapNative(cx, obj, *nodeList,
                                     NS_GET_IID(nsISupports), &collection,
                                     getter_AddRefs(holder));

    // ... and store it in our reserved slot.
    if (!JS_SetReservedSlot(cx, obj, 0, collection)) {
      return JS_FALSE;
    }
  }

  if (NS_FAILED(rv)) {
    nsDOMClassInfo::ThrowJSException(cx, rv);

    return JS_FALSE;
  }

  return *nodeList != nsnull;
}

JSBool JS_DLL_CALLBACK
nsHTMLDocumentSH::DocumentAllGetProperty(JSContext *cx, JSObject *obj,
                                         jsval id, jsval *vp)
{
  // document.all.item and .namedItem get their value in the
  // newResolve hook, so nothing to do for those properties here. And
  // we need to return early to prevent <div id="item"> from shadowing
  // document.all.item(), etc.
  if (id == sItem_id || id == sNamedItem_id) {
    return JS_TRUE;
  }

  nsIHTMLDocument *doc = (nsIHTMLDocument *)::JS_GetPrivate(cx, obj);
  nsCOMPtr<nsIDOMHTMLDocument> domdoc(do_QueryInterface(doc));
  nsCOMPtr<nsISupports> result;
  nsresult rv = NS_OK;

  if (JSVAL_IS_STRING(id)) {
    if (id == sLength_id) {
      // Map document.all.length to the length of the collection
      // document.getElementsByTagName("*"), and make sure <div
      // id="length"> doesn't shadow document.all.length.

      nsCOMPtr<nsIDOMNodeList> nodeList;
      if (!GetDocumentAllNodeList(cx, obj, domdoc, getter_AddRefs(nodeList))) {
        return JS_FALSE;
      }

      PRUint32 length;
      rv = nodeList->GetLength(&length);

      if (NS_FAILED(rv)) {
        nsDOMClassInfo::ThrowJSException(cx, rv);

        return JS_FALSE;
      }

      *vp = INT_TO_JSVAL(length);
    } else if (id != sTags_id) {
      // For all other strings, look for an element by id or name.

      nsDependentJSString str(id);

      nsCOMPtr<nsIDOMElement> element;
      domdoc->GetElementById(str, getter_AddRefs(element));

      result = element;

      if (!result) {
        doc->ResolveName(str, nsnull, getter_AddRefs(result));
      }

      if (!result) {
        nsCOMPtr<nsIDOMNodeList> nodeList;
        rv = domdoc->GetElementsByName(str, getter_AddRefs(nodeList));

        if (nodeList) {
          // Get the second node in the list, if found, we know
          // there's more than one node (this is cheaper than getting
          // the length of the collection since that requires walking
          // the whole DOM tree in all cases, all we care about is if
          // there's more than one item in the collection, or if
          // there's only one, or no items at all).

          nsCOMPtr<nsIDOMNode> node;
          rv |= nodeList->Item(1, getter_AddRefs(node));

          if (node) {
            result = nodeList;
          } else {
            rv |= nodeList->Item(0, getter_AddRefs(node));

            result = node;
          }
        }

        if (NS_FAILED(rv)) {
          nsDOMClassInfo::ThrowJSException(cx, rv);

          return JS_FALSE;
        }
      }
    }
  } else if (JSVAL_TO_INT(id) >= 0) {
    // Map document.all[n] (where n is a number) to the n:th item in
    // the document.all node list.

    nsCOMPtr<nsIDOMNodeList> nodeList;
    if (!GetDocumentAllNodeList(cx, obj, domdoc, getter_AddRefs(nodeList))) {
      return JS_FALSE;
    }

    nsCOMPtr<nsIDOMNode> node;
    nodeList->Item(JSVAL_TO_INT(id), getter_AddRefs(node));

    result = node;
  }

  if (result) {
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = nsDOMClassInfo::WrapNative(cx, obj, result, NS_GET_IID(nsISupports),
                                    vp, getter_AddRefs(holder));
    if (NS_FAILED(rv)) {
      nsDOMClassInfo::ThrowJSException(cx, rv);

      return JS_FALSE;
    }
  }

  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK
nsHTMLDocumentSH::DocumentAllNewResolve(JSContext *cx, JSObject *obj, jsval id,
                                        uintN flags, JSObject **objp)
{
  if (flags & JSRESOLVE_ASSIGNING) {
    // Nothing to do here if we're assigning

    return JS_TRUE;
  }

  jsval v = JSVAL_VOID;

  if (id == sItem_id || id == sNamedItem_id) {
    // Define the item() or namedItem() method.

    JSFunction *fnc =
      ::JS_DefineFunction(cx, obj, ::JS_GetStringBytes(JSVAL_TO_STRING(id)),
                          CallToGetPropMapper, 0, JSPROP_ENUMERATE);

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
    nsIHTMLDocument *doc = (nsIHTMLDocument *)::JS_GetPrivate(cx, obj);

    JSObject *tags = ::JS_NewObject(cx, &sHTMLDocumentAllTagsClass, nsnull,
                                    GetGlobalJSObject(cx, obj));
    if (!tags) {
      return JS_FALSE;
    }

    if (!::JS_SetPrivate(cx, tags, doc)) {
      return JS_FALSE;
    }

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
    if (JSVAL_IS_STRING(id)) {
      JSString *str = JSVAL_TO_STRING(id);

      ok = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                 ::JS_GetStringLength(str), v, nsnull, nsnull,
                                 0);
    } else {
      ok = ::JS_DefineElement(cx, obj, JSVAL_TO_INT(id), v, nsnull, nsnull, 0);
    }

    *objp = obj;
  }

  return ok;
}

// Finalize hook used by document related JS objects, but also by
// sGlobalScopePolluterClass!

void JS_DLL_CALLBACK
nsHTMLDocumentSH::ReleaseDocument(JSContext *cx, JSObject *obj)
{
  nsIHTMLDocument *doc = (nsIHTMLDocument *)::JS_GetPrivate(cx, obj);

  NS_IF_RELEASE(doc);
}

JSBool JS_DLL_CALLBACK
nsHTMLDocumentSH::CallToGetPropMapper(JSContext *cx, JSObject *obj, uintN argc,
                                      jsval *argv, jsval *rval)
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
  JSString *str = ::JS_ValueToString(cx, argv[0]);
  if (!str) {
    return JS_FALSE;
  }

  JSObject *self;

  if (::JS_TypeOfValue(cx, argv[-2]) == JSTYPE_FUNCTION) {
    // If argv[-2] is a function, we're called through
    // document.all.item() or something similar. In such a case, self
    // is passed as obj.

    self = obj;
  } else {
    // In other cases (i.e. document.all("foo")), self is passed as
    // argv[-2].

    self = JSVAL_TO_OBJECT(argv[-2]);
  }

  return ::JS_GetUCProperty(cx, self, ::JS_GetStringChars(str),
                            ::JS_GetStringLength(str), rval);
}


static inline JSObject *
GetDocumentAllHelper(JSContext *cx, JSObject *obj)
{
  while (obj && JS_GET_CLASS(cx, obj) != &sHTMLDocumentAllHelperClass) {
    obj = ::JS_GetPrototype(cx, obj);
  }

  return obj;
}

JSBool JS_DLL_CALLBACK
nsHTMLDocumentSH::DocumentAllHelperGetProperty(JSContext *cx, JSObject *obj,
                                               jsval id, jsval *vp)
{
  if (id != nsDOMClassInfo::sAll_id) {
    return JS_TRUE;
  }

  JSObject *helper = GetDocumentAllHelper(cx, obj);

  if (!helper) {
    NS_ERROR("Uh, how'd we get here?");

    // Let scripts continue, if we somehow did get here...

    return JS_TRUE;
  }

  PRUint32 flags = JSVAL_TO_INT(PRIVATE_TO_JSVAL(::JS_GetPrivate(cx, helper)));

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
      nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
      nsresult rv =
        sXPConnect->GetWrappedNativeOfJSObject(cx, obj,
                                               getter_AddRefs(wrapper));
      if (NS_FAILED(rv)) {
        nsDOMClassInfo::ThrowJSException(cx, rv);

        return JS_FALSE;
      }

      JSObject *all = ::JS_NewObject(cx, &sHTMLDocumentAllClass, nsnull,
                                     GetGlobalJSObject(cx, obj));
      if (!all) {
        return JS_FALSE;
      }

      nsIHTMLDocument *doc;
      CallQueryInterface(wrapper->Native(), &doc);

      // Let the JSObject take over ownership of doc.
      if (!::JS_SetPrivate(cx, all, doc)) {
        NS_RELEASE(doc);

        return JS_FALSE;
      }

      *vp = OBJECT_TO_JSVAL(all);
    }
  }

  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK
nsHTMLDocumentSH::DocumentAllHelperNewResolve(JSContext *cx, JSObject *obj,
                                              jsval id, uintN flags,
                                              JSObject **objp)
{
  if (id == nsDOMClassInfo::sAll_id) {
    // document.all is resolved for the first time. Define it.
    JSObject *helper = GetDocumentAllHelper(cx, obj);

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


JSBool JS_DLL_CALLBACK
nsHTMLDocumentSH::DocumentAllTagsNewResolve(JSContext *cx, JSObject *obj,
                                            jsval id, uintN flags,
                                            JSObject **objp)
{
  if (JSVAL_IS_STRING(id)) {
    nsIHTMLDocument *doc = (nsIHTMLDocument *)::JS_GetPrivate(cx, obj);

    JSString *str = JSVAL_TO_STRING(id);

    JSBool found;
    if (!::JS_HasUCProperty(cx, ::JS_GetPrototype(cx, obj),
                            ::JS_GetStringChars(str),
                            ::JS_GetStringLength(str), &found)) {
      return JS_FALSE;
    }

    if (found) {
      return JS_TRUE;
    }

    nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(doc));

    nsCOMPtr<nsIDOMNodeList> tags;
    domdoc->GetElementsByTagName(nsDependentJSString(str),
                                 getter_AddRefs(tags));

    if (tags) {
      jsval v;
      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      nsresult rv = nsDOMClassInfo::WrapNative(cx, obj, tags,
                                               NS_GET_IID(nsISupports), &v,
                                               getter_AddRefs(holder));
      if (NS_FAILED(rv)) {
        nsDOMClassInfo::ThrowJSException(cx, rv);

        return JS_FALSE;
      }

      if (!::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                 ::JS_GetStringLength(str), v, nsnull, nsnull,
                                 0)) {
        return JS_FALSE;
      }

      *objp = obj;
    }
  }

  return JS_TRUE;
}


NS_IMETHODIMP
nsHTMLDocumentSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, jsval id, PRUint32 flags,
                             JSObject **objp, PRBool *_retval)
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

      nsresult rv = ResolveImpl(cx, wrapper, id, getter_AddRefs(result));
      NS_ENSURE_SUCCESS(rv, rv);

      if (result) {
        JSString *str = JS_ValueToString(cx, id);

        JSBool ok = *_retval =
          ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                ::JS_GetStringLength(str), JSVAL_VOID, nsnull,
                                nsnull, 0);
        *objp = obj;

        return ok ? NS_OK : NS_ERROR_FAILURE;
      }
    }

    if (id == sOpen_id) {
      JSString *str = JSVAL_TO_STRING(id);
      JSFunction *fnc =
        ::JS_DefineFunction(cx, obj, ::JS_GetStringBytes(str),
                            DocumentOpen, 0, JSPROP_ENUMERATE);

      *objp = obj;

      return fnc ? NS_OK : NS_ERROR_UNEXPECTED;
    }

    if (id == sAll_id && !sDisableDocumentAllSupport &&
        !ObjectIsNativeWrapper(cx, obj)) {
      nsCOMPtr<nsIDocument> doc(do_QueryWrappedNative(wrapper));

      if (doc->GetCompatibilityMode() == eCompatibility_NavQuirks) {
        JSObject *helper =
          GetDocumentAllHelper(cx, ::JS_GetPrototype(cx, obj));

        JSObject *proto = ::JS_GetPrototype(cx, helper ? helper : obj);

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

          while ((tmpProto = ::JS_GetPrototype(cx, tmp)) != helper) {
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
                                  ::JS_GetPrototype(cx, obj),
                                  GetGlobalJSObject(cx, obj));

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
        if (helper &&
            !::JS_SetPrivate(cx, helper,
                             JSVAL_TO_PRIVATE(INT_TO_JSVAL(flags)))) {
          nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

          return NS_ERROR_UNEXPECTED;
        }
      }

      return NS_OK;
    }
  }

  return nsDocumentSH::NewResolve(wrapper, cx, obj, id, flags, objp, _retval);
}

NS_IMETHODIMP
nsHTMLDocumentSH::GetProperty(nsIXPConnectWrappedNative *wrapper,
                              JSContext *cx, JSObject *obj, jsval id,
                              jsval *vp, PRBool *_retval)
{
  // nsDocumentSH::GetProperty() does a security check for us, call
  // that first...
  nsresult rv = nsDocumentSH::GetProperty(wrapper, cx, obj, id, vp, _retval);
  if (!*_retval) {
    return rv;
  }

  // For native wrappers, do not get random names on document
  if (ObjectIsNativeWrapper(cx, obj)) {
    return rv;
  }
  
  nsCOMPtr<nsISupports> result;

  JSAutoRequest ar(cx);

  rv = ResolveImpl(cx, wrapper, id, getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);

  if (result) {
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = WrapNative(cx, obj, result, NS_GET_IID(nsISupports), vp,
                    getter_AddRefs(holder));
    if (NS_SUCCEEDED(rv)) {
      rv = NS_SUCCESS_I_DID_SOMETHING;
    }
  }

  return rv;
}

// HTMLElement helper

// static
JSBool JS_DLL_CALLBACK
nsHTMLElementSH::ScrollIntoView(JSContext *cx, JSObject *obj, uintN argc,
                                jsval *argv, jsval *rval)
{
  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;

  nsresult rv =
    sXPConnect->GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wrapper));
  if (NS_FAILED(rv)) {
    nsDOMClassInfo::ThrowJSException(cx, rv);

    return JS_FALSE;
  }

  nsCOMPtr<nsIDOMNSHTMLElement> element(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(element, JS_FALSE);

  JSBool top = JS_TRUE;

  if (argc > 0) {
    ::JS_ValueToBoolean(cx, argv[0], &top);
  }

  rv = element->ScrollIntoView(top);

  *rval = JSVAL_VOID;

  return NS_SUCCEEDED(rv);
}

NS_IMETHODIMP
nsHTMLElementSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval id, PRUint32 flags,
                            JSObject **objp, PRBool *_retval)
{
  if (id == sScrollIntoView_id && !(JSRESOLVE_ASSIGNING & flags)) {
    JSString *str = JSVAL_TO_STRING(id);
    JSAutoRequest ar(cx);
    JSFunction *cfnc =
      ::JS_DefineFunction(cx, obj, ::JS_GetStringBytes(str), ScrollIntoView,
                          0, 0);

    *objp = obj;

    return cfnc ? NS_OK : NS_ERROR_UNEXPECTED;
  }

  return nsElementSH::NewResolve(wrapper, cx, obj, id, flags, objp, _retval);
}


// HTML[I]FrameElement helper

NS_IMETHODIMP
nsHTMLFrameElementSH::GetProperty(nsIXPConnectWrappedNative *wrapper,
                                  JSContext *cx, JSObject *obj, jsval id,
                                  jsval *vp, PRBool *_retval)
{
  nsresult rv =
    sSecMan->CheckPropertyAccess(cx, obj, mData->mName, id,
                                 nsIXPCSecurityManager::ACCESS_GET_PROPERTY);

  if (NS_FAILED(rv)) {
    // Let XPConnect know that the access was not granted.
    *_retval = PR_FALSE;
  }

  // None of our base classes "implement" GetProperty(), so simply
  // return NS_OK;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameElementSH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                                  JSContext *cx, JSObject *obj, jsval id,
                                  jsval *vp, PRBool *_retval)
{
  nsresult rv =
    sSecMan->CheckPropertyAccess(cx, obj, mData->mName, id,
                                 nsIXPCSecurityManager::ACCESS_SET_PROPERTY);

  if (NS_FAILED(rv)) {
    // Let XPConnect know that the access was not granted.
    *_retval = PR_FALSE;

    return NS_OK;
  }

  return nsHTMLElementSH::SetProperty(wrapper, cx, obj, id, vp, _retval);
}

NS_IMETHODIMP
nsHTMLFrameElementSH::AddProperty(nsIXPConnectWrappedNative *wrapper,
                                  JSContext *cx, JSObject *obj, jsval id,
                                  jsval *vp, PRBool *_retval)
{
  nsresult rv =
    sSecMan->CheckPropertyAccess(cx, obj, mData->mName, id,
                                 nsIXPCSecurityManager::ACCESS_SET_PROPERTY);

  if (NS_FAILED(rv)) {
    // Let XPConnect know that the access was not granted.
    *_retval = PR_FALSE;

    return NS_OK;
  }

  return nsHTMLElementSH::AddProperty(wrapper, cx, obj, id, vp, _retval);
}

NS_IMETHODIMP
nsHTMLFrameElementSH::DelProperty(nsIXPConnectWrappedNative *wrapper,
                                  JSContext *cx, JSObject *obj, jsval id,
                                  jsval *vp, PRBool *_retval)
{
  nsresult rv =
    sSecMan->CheckPropertyAccess(cx, obj, mData->mName, id,
                                 nsIXPCSecurityManager::ACCESS_SET_PROPERTY);

  if (NS_FAILED(rv)) {
    // Let XPConnect know that the access was not granted.
    *_retval = PR_FALSE;
  }

  // None of our base classes "implement" GetProperty(), so simply
  // return NS_OK;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameElementSH::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                 JSContext *cx, JSObject *obj, jsval id,
                                 PRUint32 flags, JSObject **objp,
                                 PRBool *_retval)
{
  nsresult rv =
    sSecMan->CheckPropertyAccess(cx, obj, mData->mName, id,
                                 nsIXPCSecurityManager::ACCESS_GET_PROPERTY);

  if (NS_FAILED(rv)) {
    // Let XPConnect know that the access was not granted.
    *_retval = PR_FALSE;

    return NS_OK;
  }

  return nsHTMLElementSH::NewResolve(wrapper, cx, obj, id, flags, objp,
                                     _retval);
}


// HTMLFormElement helper

// static
nsresult
nsHTMLFormElementSH::FindNamedItem(nsIForm *aForm, JSString *str,
                                   nsISupports **aResult)
{
  *aResult = nsnull;

  nsDependentJSString name(str);

  aForm->ResolveName(name, aResult);

  if (!*aResult) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aForm));
    nsCOMPtr<nsIDOMHTMLFormElement> form_element(do_QueryInterface(aForm));

    nsCOMPtr<nsIHTMLDocument> html_doc =
      do_QueryInterface(content->GetDocument());

    if (html_doc && form_element) {
      html_doc->ResolveName(name, form_element, aResult);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElementSH::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                JSContext *cx, JSObject *obj, jsval id,
                                PRUint32 flags, JSObject **objp,
                                PRBool *_retval)
{
  // For native wrappers, do not resolve random names on form
  if ((!(JSRESOLVE_ASSIGNING & flags)) && JSVAL_IS_STRING(id) &&
      !ObjectIsNativeWrapper(cx, obj)) {
    nsCOMPtr<nsIForm> form(do_QueryWrappedNative(wrapper));
    nsCOMPtr<nsISupports> result;

    JSString *str = JSVAL_TO_STRING(id);
    FindNamedItem(form, str, getter_AddRefs(result));

    if (result) {
      JSAutoRequest ar(cx);
      *_retval = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                       ::JS_GetStringLength(str),
                                       JSVAL_VOID, nsnull, nsnull, 0);

      *objp = obj;

      return *_retval ? NS_OK : NS_ERROR_FAILURE;
    }
  }

  return nsHTMLElementSH::NewResolve(wrapper, cx, obj, id, flags, objp,
                                     _retval);
}


NS_IMETHODIMP
nsHTMLFormElementSH::GetProperty(nsIXPConnectWrappedNative *wrapper,
                                 JSContext *cx, JSObject *obj, jsval id,
                                 jsval *vp, PRBool *_retval)
{
  nsCOMPtr<nsIForm> form(do_QueryWrappedNative(wrapper));

  if (JSVAL_IS_STRING(id)) {
    // For native wrappers, do not get random names on form
    if (!ObjectIsNativeWrapper(cx, obj)) {
      nsCOMPtr<nsISupports> result;

      JSString *str = JSVAL_TO_STRING(id);
      FindNamedItem(form, str, getter_AddRefs(result));

      if (result) {
        // Wrap result, result can be either an element or a list of
        // elements
        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        nsresult rv = WrapNative(cx, obj, result, NS_GET_IID(nsISupports), vp,
                                 getter_AddRefs(holder));
        return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
      }
    }

    return NS_OK; // Don't fall through
  }

  PRInt32 n = GetArrayIndexFromId(cx, id);

  if (n >= 0) {
    nsCOMPtr<nsIFormControl> control;
    form->GetElementAt(n, getter_AddRefs(control));

    if (control) {
      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      nsresult rv = WrapNative(cx, obj, control, NS_GET_IID(nsISupports), vp,
                               getter_AddRefs(holder));
      return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElementSH::NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                                  JSContext *cx, JSObject *obj,
                                  PRUint32 enum_op, jsval *statep,
                                  jsid *idp, PRBool *_retval)
{
  switch (enum_op) {
  case JSENUMERATE_INIT:
    {
      nsCOMPtr<nsIForm> form(do_QueryWrappedNative(wrapper));

      if (!form) {
        *statep = JSVAL_NULL;
        return NS_ERROR_UNEXPECTED;
      }

      *statep = INT_TO_JSVAL(0);

      if (idp) {
        PRUint32 count = 0;
        form->GetElementCount(&count);

        *idp = INT_TO_JSVAL(count);
      }

      break;
    }
  case JSENUMERATE_NEXT:
    {
      nsCOMPtr<nsIForm> form(do_QueryWrappedNative(wrapper));
      NS_ENSURE_TRUE(form, NS_ERROR_FAILURE);

      PRInt32 index = (PRInt32)JSVAL_TO_INT(*statep);

      PRUint32 count = 0;
      form->GetElementCount(&count);

      if ((PRUint32)index < count) {
        nsCOMPtr<nsIFormControl> controlNode;
        form->GetElementAt(index, getter_AddRefs(controlNode));
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
          JS_NewUCStringCopyN(cx, NS_REINTERPRET_CAST(const jschar *,
                                                      attr.get()),
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
nsHTMLSelectElementSH::GetProperty(nsIXPConnectWrappedNative *wrapper,
                                   JSContext *cx, JSObject *obj, jsval id,
                                   jsval *vp, PRBool *_retval)
{
  PRInt32 n = GetArrayIndexFromId(cx, id);

  nsresult rv = NS_OK;
  if (n >= 0) {
    nsCOMPtr<nsIDOMHTMLSelectElement> s(do_QueryWrappedNative(wrapper));

    nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
    s->GetOptions(getter_AddRefs(options));

    if (options) {
      nsCOMPtr<nsIDOMNode> node;

      options->Item(n, getter_AddRefs(node));

      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      rv = WrapNative(cx, obj, node, NS_GET_IID(nsIDOMNode), vp,
                      getter_AddRefs(holder));
      if (NS_SUCCEEDED(rv)) {
        rv = NS_SUCCESS_I_DID_SOMETHING;
      }
    }
  }

  return rv;
}

// static
nsresult
nsHTMLSelectElementSH::SetOption(JSContext *cx, jsval *vp, PRUint32 aIndex,
                                 nsIDOMNSHTMLOptionCollection *aOptCollection)
{
  JSAutoRequest ar(cx);

  // vp must refer to an object
  if (!JSVAL_IS_OBJECT(*vp) && !::JS_ConvertValue(cx, *vp, JSTYPE_OBJECT, vp)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIDOMHTMLOptionElement> new_option;

  if (!JSVAL_IS_NULL(*vp)) {
    nsCOMPtr<nsIXPConnectWrappedNative> new_wrapper;
    nsresult rv;

    rv = sXPConnect->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(*vp),
                                                getter_AddRefs(new_wrapper));
    NS_ENSURE_SUCCESS(rv, rv);

    new_option = do_QueryWrappedNative(new_wrapper);

    if (!new_option) {
      // Someone is trying to set an option to a non-option object.

      return NS_ERROR_UNEXPECTED;
    }
  }

  return aOptCollection->SetOption(aIndex, new_option);
}

NS_IMETHODIMP
nsHTMLSelectElementSH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                                   JSContext *cx, JSObject *obj, jsval id,
                                   jsval *vp, PRBool *_retval)
{
  PRInt32 n = GetArrayIndexFromId(cx, id);

  if (n >= 0) {
    nsCOMPtr<nsIDOMHTMLSelectElement> select(do_QueryWrappedNative(wrapper));
    NS_ENSURE_TRUE(select, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
    select->GetOptions(getter_AddRefs(options));

    nsCOMPtr<nsIDOMNSHTMLOptionCollection> oc(do_QueryInterface(options));
    NS_ENSURE_TRUE(oc, NS_ERROR_UNEXPECTED);

    nsresult rv = SetOption(cx, vp, n, oc);
    return NS_FAILED(rv) ? rv : NS_SUCCESS_I_DID_SOMETHING;
  }

  return nsElementSH::SetProperty(wrapper, cx, obj, id, vp, _retval);
}


// HTMLObject/EmbedElement helper

// This resolve hook makes embed.nsIFoo work as if
// QueryInterface(Components.interfaces.nsIFoo) was called on the
// plugin instance, the result of calling QI, assuming it's
// successful, will be defined on the embed element as a nsIFoo
// property.

nsresult
nsHTMLExternalObjSH::GetPluginInstance(nsIXPConnectWrappedNative *wrapper,
                                       nsIPluginInstance **_result)
{
  *_result = nsnull;

  nsCOMPtr<nsIContent> content(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(content, NS_ERROR_UNEXPECTED);

  // Make sure that there is a plugin
  nsCOMPtr<nsIObjectLoadingContent> objlc(do_QueryInterface(content));
  NS_ASSERTION(objlc, "Object nodes must implement nsIObjectLoadingContent");
  return objlc->EnsureInstantiation(_result);
}

// Check if proto is already in obj's prototype chain.

static PRBool
IsObjInProtoChain(JSContext *cx, JSObject *obj, JSObject *proto)
{
  JSObject *o = obj;

  JSAutoRequest ar(cx);

  while (o) {
    JSObject *p = ::JS_GetPrototype(cx, o);

    if (p == proto) {
      return PR_TRUE;
    }

    o = p;
  }

  return PR_FALSE;
}


// Note that not only XPConnect calls this PostCreate() method when
// it creates wrappers, nsObjectFrame also calls this method when a
// plugin is loaded if the embed/object element is already wrapped to
// get the scriptable plugin inserted into the embed/object's proto
// chain.

NS_IMETHODIMP
nsHTMLExternalObjSH::PostCreate(nsIXPConnectWrappedNative *wrapper,
                                JSContext *cx, JSObject *obj)
{
  nsresult rv = nsElementSH::PostCreate(wrapper, cx, obj);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPluginInstance> pi;
  rv = GetPluginInstance(wrapper, getter_AddRefs(pi));
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

  if (IsObjInProtoChain(cx, obj, pi_obj)) {
    // We must have re-entered ::PostCreate() from nsObjectFrame()
    // (through the EnsureInstantiation() call in
    // GetPluginInstance()), this means that we've already done what
    // we're about to do in this function so we can just return here.

    return NS_OK;
  }


  // If we got an xpconnect-wrapped plugin object, set obj's
  // prototype's prototype to the scriptable plugin.

  JSObject *my_proto = nsnull;

  // Get 'this.__proto__'
  rv = wrapper->GetJSObjectPrototype(&my_proto);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(cx);

  // Set 'this.__proto__' to pi
  if (!::JS_SetPrototype(cx, obj, pi_obj)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (pi_proto && JS_GET_CLASS(cx, pi_proto) != sObjectClass) {
    // The plugin wrapper has a proto that's not Object.prototype, set
    // 'pi.__proto__.__proto__' to the original 'this.__proto__'
    if (!::JS_SetPrototype(cx, pi_proto, my_proto)) {
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    // 'pi' didn't have a prototype, or pi's proto was 'Object.prototype'
    // (i.e. pi is an LiveConnect wrapped Java applet), set
    // 'pi.__proto__' to the original 'this.__proto__'
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
  // pi.__proto__.__proto__
  // ^      ^         ^
  // |      |         |__ Object.prototype
  // |      |
  // |      |__ plugin proto (not shared in the xpc wrapper case)
  // |
  // |__ xpc wrapped native pi (plugin instance)
  //
  // Now, after the above prototype setup the prototype chain should
  // look like this if the plugin had a proto (other than
  // Object.prototype):
  //
  // this.__proto__.__proto__.__proto__.__proto__
  //   ^      ^         ^         ^         ^
  //   |      |         |         |         |__ Object.prototype
  //   |      |         |         |
  //   |      |         |         |__ xpc embed wrapper proto (shared)
  //   |      |         |
  //   |      |         |__ plugin proto (not shared in the xpc wrapper case)
  //   |      |
  //   |      |__ xpc wrapped native pi (plugin instance)
  //   |
  //   |__ xpc wrapped native embed node
  //
  // If the plugin's proto was Object.prototype, the prototype chain
  // should look like this:
  //
  // this.__proto__.__proto__.__proto__
  //   ^      ^         ^         ^
  //   |      |         |         |__ Object.prototype
  //   |      |         |
  //   |      |         |__ xpc embed wrapper proto (shared)
  //   |      |
  //   |      |__ pi (plugin instance) wrapper, most likely wrapped
  //   |          by LiveConnect
  //   |
  //   |__ xpc wrapped native embed node
  //

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLExternalObjSH::GetProperty(nsIXPConnectWrappedNative *wrapper,
                                 JSContext *cx, JSObject *obj, jsval id,
                                 jsval *vp, PRBool *_retval)
{
  JSAutoRequest ar(cx);

  JSObject *pi_obj = ::JS_GetPrototype(cx, obj);

  const jschar *id_chars = nsnull;
  size_t id_length = 0;

  JSBool found = PR_FALSE;

  if (!ObjectIsNativeWrapper(cx, obj)) {
    if (JSVAL_IS_STRING(id)) {
      JSString *id_str = JSVAL_TO_STRING(id);

      id_chars = ::JS_GetStringChars(id_str);
      id_length = ::JS_GetStringLength(id_str);

      *_retval = ::JS_HasUCProperty(cx, pi_obj, id_chars, id_length, &found);
    } else {
      *_retval = JS_HasElement(cx, pi_obj, JSVAL_TO_INT(id), &found);
    }

    if (!*_retval) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (found) {
    if (JSVAL_IS_STRING(id)) {
      *_retval = ::JS_GetUCProperty(cx, pi_obj, id_chars, id_length, vp);
    } else {
      *_retval = ::JS_GetElement(cx, pi_obj, JSVAL_TO_INT(id), vp);
    }

    return *_retval ? NS_SUCCESS_I_DID_SOMETHING : NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLExternalObjSH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                                 JSContext *cx, JSObject *obj, jsval id,
                                 jsval *vp, PRBool *_retval)
{
  JSAutoRequest ar(cx);

  JSObject *pi_obj = ::JS_GetPrototype(cx, obj);

  const jschar *id_chars = nsnull;
  size_t id_length = 0;

  JSBool found = PR_FALSE;

  if (!ObjectIsNativeWrapper(cx, obj)) {
    if (JSVAL_IS_STRING(id)) {
      JSString *id_str = JSVAL_TO_STRING(id);

      id_chars = ::JS_GetStringChars(id_str);
      id_length = ::JS_GetStringLength(id_str);

      *_retval = ::JS_HasUCProperty(cx, pi_obj, id_chars, id_length, &found);
    } else {
      *_retval = JS_HasElement(cx, pi_obj, JSVAL_TO_INT(id), &found);
    }

    if (!*_retval) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (found) {
    if (JSVAL_IS_STRING(id)) {
      *_retval = ::JS_SetUCProperty(cx, pi_obj, id_chars, id_length, vp);
    } else {
      *_retval = ::JS_SetElement(cx, pi_obj, JSVAL_TO_INT(id), vp);
    }

    return *_retval ? NS_SUCCESS_I_DID_SOMETHING : NS_ERROR_FAILURE;
  }

  return nsElementSH::SetProperty(wrapper, cx, obj, id, vp, _retval);
}

NS_IMETHODIMP
nsHTMLExternalObjSH::Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, PRUint32 argc, jsval *argv, jsval *vp,
                          PRBool *_retval)
{
  nsCOMPtr<nsIPluginInstance> pi;
  nsresult rv = GetPluginInstance(wrapper, getter_AddRefs(pi));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!pi) {
    // No plugin around for this object.

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
  *_retval = ::JS_CallFunctionValue(cx, JSVAL_TO_OBJECT(argv[-1]),
                                    OBJECT_TO_JSVAL(pi_obj), argc, argv, vp);

  return NS_OK;
}


// HTMLAppletElement helper

nsresult
nsHTMLAppletElementSH::GetPluginJSObject(JSContext *cx, JSObject *obj,
                                         nsIPluginInstance *plugin_inst,
                                         JSObject **plugin_obj,
                                         JSObject **plugin_proto)
{
#ifdef OJI
  *plugin_obj = nsnull;
  *plugin_proto = nsnull;

  nsCOMPtr<nsIJVMManager> jvm(do_GetService(nsIJVMManager::GetCID()));

  if (!jvm) {
#endif
    return NS_OK;
#ifdef OJI
  }

  nsCOMPtr<nsIJVMPluginInstance> javaPluginInstance;

  javaPluginInstance = do_QueryInterface(plugin_inst);

  if (!javaPluginInstance) {
    return NS_OK;
  }

  jobject appletObject = nsnull;
  nsresult rv = javaPluginInstance->GetJavaObject(&appletObject);

  if (NS_FAILED(rv) || !appletObject) {
    return rv;
  }

  nsCOMPtr<nsILiveConnectManager> manager =
    do_GetService(nsIJVMManager::GetCID());

  if (!manager) {
    return NS_OK;
  }

  return manager->WrapJavaObject(cx, appletObject, plugin_obj);
#endif /* OJI */
}


// HTMLEmbed/ObjectElement helper

nsresult
nsHTMLPluginObjElementSH::GetPluginJSObject(JSContext *cx, JSObject *obj,
                                            nsIPluginInstance *plugin_inst,
                                            JSObject **plugin_obj,
                                            JSObject **plugin_proto)
{
  *plugin_obj = nsnull;
  *plugin_proto = nsnull;

  nsCOMPtr<nsIPluginInstanceInternal> plugin_internal =
    do_QueryInterface(plugin_inst);

  if (plugin_internal) {
    *plugin_obj = plugin_internal->GetJSObject(cx);

    if (*plugin_obj) {
      *plugin_proto = ::JS_GetPrototype(cx, *plugin_obj);

      return NS_OK;
    }
  }

  // Check if the plugin object has the nsIScriptablePlugin interface,
  // describing how to expose it to JavaScript. Given this interface,
  // use it to get the scriptable peer object (possibly the plugin
  // object itself) and the scriptable interface to expose it with.

  // default to nsISupports's IID
  nsIID scriptableIID = NS_GET_IID(nsISupports);
  nsCOMPtr<nsISupports> scriptable_peer;

  nsCOMPtr<nsIScriptablePlugin> spi(do_QueryInterface(plugin_inst));

  if (spi) {
    nsIID *scriptableInterfacePtr = nsnull;
    spi->GetScriptableInterface(&scriptableInterfacePtr);

    if (scriptableInterfacePtr) {
      spi->GetScriptablePeer(getter_AddRefs(scriptable_peer));

      scriptableIID = *scriptableInterfacePtr;

      nsMemory::Free(scriptableInterfacePtr);
    }
  }

  nsCOMPtr<nsIClassInfo> ci(do_QueryInterface(plugin_inst));

  if (!scriptable_peer) {
    if (!ci) {
      // This plugin doesn't support nsIScriptablePlugin, nor does it
      // have classinfo, this plugin is not scriptable using those
      // methods. It might however be a Java plugin running in an EMBED or
      // OBJECT so let's try that.

      return nsHTMLAppletElementSH::GetPluginJSObject(cx, obj, plugin_inst,
                                                      plugin_obj,
                                                      plugin_proto);
    }

    // The plugin instance has classinfo, use it as the scriptable
    // plugin
    scriptable_peer = plugin_inst;
  }

  // Check if the plugin can be safely scriptable, the plugin wrapper
  // must not have a shared prototype for this to work since we'll end
  // up setting it's prototype here, and we want this change to affect
  // this plugin object only.

  if (ci) {
    // If we have class info we must make sure that the "share my
    // proto" flag is *not* set

    PRUint32 flags;
    ci->GetFlags(&flags);

    if (!(flags & nsIClassInfo::PLUGIN_OBJECT)) {
      // The plugin classinfo doesn't claim it's a plugin object, this
      // means the plugin object's proto might be shared, can't do
      // this prototype setup then.

      return NS_OK;
    }
  }

  // notify the PluginManager that this one is scriptable --
  // it will need some special treatment later
  nsCOMPtr<nsIPluginHost> pluginManager =
    do_GetService(kCPluginManagerCID);

  nsCOMPtr<nsPIPluginHost> pluginHost(do_QueryInterface(pluginManager));

  if(pluginHost) {
    pluginHost->SetIsScriptableInstance(plugin_inst, PR_TRUE);
  }

  // Wrap it.

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  nsresult rv = sXPConnect->WrapNative(cx, ::JS_GetParent(cx, obj),
                                       scriptable_peer,
                                       scriptableIID, getter_AddRefs(holder));
  // Wrapping a plugin object can fail if the plugins XPT file can't
  // be found (i.e. is incorrectly installed). Return NS_OK in such a
  // case to avoid having this generate exceptions in JS and to let
  // the script still access the DOM node, even if the underlying
  // plugin won't be scriptable.
  NS_ENSURE_SUCCESS(rv, NS_OK);

  // QI holder to nsIXPConnectWrappedNative so that we can reliably
  // access it's prototype
  nsCOMPtr<nsIXPConnectWrappedNative> pi_wrapper(do_QueryInterface(holder));
  NS_ENSURE_TRUE(pi_wrapper, NS_ERROR_UNEXPECTED);

  rv = pi_wrapper->GetJSObject(plugin_obj);
  NS_ENSURE_SUCCESS(rv, rv);

  return pi_wrapper->GetJSObjectPrototype(plugin_proto);
}

NS_IMETHODIMP
nsHTMLPluginObjElementSH::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                     JSContext *cx, JSObject *obj, jsval id,
                                     PRUint32 flags, JSObject **objp,
                                     PRBool *_retval)
{
  if (JSVAL_IS_STRING(id)) {
    // This code resolves embed.nsIFoo to the nsIFoo wrapper of the
    // plugin/applet instance

    JSString *str = JSVAL_TO_STRING(id);
    char* cstring = ::JS_GetStringBytes(str);

    nsCOMPtr<nsIInterfaceInfoManager>
      iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
    NS_ENSURE_TRUE(iim, NS_ERROR_UNEXPECTED);

    nsIID* iid = nsnull;

    nsresult rv = iim->GetIIDForName(cstring, &iid);

    if (NS_SUCCEEDED(rv) && iid) {
      nsCOMPtr<nsIPluginInstance> pi;

      GetPluginInstance(wrapper, getter_AddRefs(pi));

      if (pi) {
        // notify the PluginManager that this one is scriptable --
        // it will need some special treatment later

        nsCOMPtr<nsIPluginHost> pluginManager =
          do_GetService(kCPluginManagerCID);

        nsCOMPtr<nsPIPluginHost> pluginHost(do_QueryInterface(pluginManager));

        if(pluginHost) {
          pluginHost->SetIsScriptableInstance(pi, PR_TRUE);
        }

        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        rv = sXPConnect->WrapNative(cx, obj, pi, *iid, getter_AddRefs(holder));

        if (NS_SUCCEEDED(rv)) {
          JSObject* ifaceObj;

          rv = holder->GetJSObject(&ifaceObj);

          if (NS_SUCCEEDED(rv)) {
            nsMemory::Free(iid);

            *_retval = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                             ::JS_GetStringLength(str),
                                             OBJECT_TO_JSVAL(ifaceObj), nsnull,
                                             nsnull, JSPROP_ENUMERATE);

            *objp = obj;

            return *_retval ? NS_OK : NS_ERROR_FAILURE;
          }
        }
      }

      nsMemory::Free(iid);
    }
  }

  return nsHTMLElementSH::NewResolve(wrapper, cx, obj, id, flags, objp,
                                     _retval);
}


// HTMLOptionsCollection helper

NS_IMETHODIMP
nsHTMLOptionsCollectionSH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                                       JSContext *cx, JSObject *obj, jsval id,
                                       jsval *vp, PRBool *_retval)
{
  PRInt32 n = GetArrayIndexFromId(cx, id);

  if (n < 0) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNSHTMLOptionCollection> oc(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(oc, NS_ERROR_UNEXPECTED);

  nsresult rv = nsHTMLSelectElementSH::SetOption(cx, vp, n, oc);
  if (NS_SUCCEEDED(rv)) {
    rv = NS_SUCCESS_I_DID_SOMETHING;
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLOptionsCollectionSH::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                      JSContext *cx, JSObject *obj, 
                                      jsval id, PRUint32 flags, 
                                      JSObject **objp, PRBool *_retval)
{
  if (id == sAdd_id) {
    JSString *str = JSVAL_TO_STRING(id);

    JSAutoRequest ar(cx);

    JSFunction *fnc =
      ::JS_DefineFunction(cx, obj, ::JS_GetStringBytes(str),
                          Add, 0, JSPROP_ENUMERATE);
    
    *objp = obj;
    
    return fnc ? NS_OK : NS_ERROR_UNEXPECTED;
  }

  return nsHTMLCollectionSH::NewResolve(wrapper, cx, obj, id, flags, objp, _retval);
}

JSBool JS_DLL_CALLBACK
nsHTMLOptionsCollectionSH::Add(JSContext *cx, JSObject *obj, uintN argc,
                               jsval *argv, jsval *rval)
{
  *rval = JSVAL_VOID;

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  nsresult rv =
    sXPConnect->GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wrapper));

  if (NS_FAILED(rv)) {
    nsDOMClassInfo::ThrowJSException(cx, rv);
    return JS_FALSE;
  }

  nsCOMPtr<nsIDOMHTMLOptionsCollection> options(do_QueryWrappedNative(wrapper));
  NS_ASSERTION(options, "native should have been an options collection");

  if (argc < 1) {
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
    return JS_FALSE;
  }

  if (JSVAL_IS_PRIMITIVE(argv[0])) {
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_DOM_WRONG_TYPE_ERR);
    return JS_FALSE;
  }

  rv = sXPConnect->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(argv[0]),
                                              getter_AddRefs(wrapper));
  
  if (NS_FAILED(rv)) {
    nsDOMClassInfo::ThrowJSException(cx, rv);
    return JS_FALSE;
  }
  
  nsCOMPtr<nsIDOMHTMLOptionElement> newOption(do_QueryWrappedNative(wrapper));
  if (!newOption) {
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_DOM_WRONG_TYPE_ERR);
    return JS_FALSE;
  }

  int32 index = -1;
  if (argc > 1) {
    if (!JS_ValueToInt32(cx, argv[1], &index)) {
      return JS_FALSE;
    }
  }

  if (index < -1) {
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_DOM_INDEX_SIZE_ERR);
    return JS_FALSE;
  }

  PRUint32 length;
  options->GetLength(&length);

  if (index == -1 || index > (int32)length) {
    // IE appends in these cases
    index = length;
  }

  nsCOMPtr<nsIDOMNode> beforeNode;
  options->Item(index, getter_AddRefs(beforeNode));
  
  nsCOMPtr<nsIDOMHTMLOptionElement> beforeElement(do_QueryInterface(beforeNode));

  nsCOMPtr<nsIDOMNSHTMLOptionCollection> nsoptions(do_QueryInterface(options));

  nsCOMPtr<nsIDOMHTMLSelectElement> select;
  nsoptions->GetSelect(getter_AddRefs(select));
                             
  rv = select->Add(newOption, beforeElement);

  if (NS_FAILED(rv)) {
    nsDOMClassInfo::ThrowJSException(cx, rv);
  }

  return NS_SUCCEEDED(rv);
}


// Plugin helper

nsresult
nsPluginSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                      nsISupports **aResult)
{
  nsCOMPtr<nsIDOMPlugin> plugin(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(plugin, NS_ERROR_UNEXPECTED);

  nsIDOMMimeType *mime_type = nsnull;
  nsresult rv = plugin->Item(aIndex, &mime_type);

  *aResult = mime_type;

  return rv;
}

nsresult
nsPluginSH::GetNamedItem(nsISupports *aNative, const nsAString& aName,
                         nsISupports **aResult)
{
  nsCOMPtr<nsIDOMPlugin> plugin(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(plugin, NS_ERROR_UNEXPECTED);

  nsIDOMMimeType *mime_type = nsnull;

  nsresult rv = plugin->NamedItem(aName, &mime_type);

  *aResult = mime_type;

  return rv;
}


// PluginArray helper

nsresult
nsPluginArraySH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                           nsISupports **aResult)
{
  nsCOMPtr<nsIDOMPluginArray> array(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(array, NS_ERROR_UNEXPECTED);

  nsIDOMPlugin *plugin = nsnull;
  nsresult rv = array->Item(aIndex, &plugin);

  *aResult = plugin;

  return rv;
}

nsresult
nsPluginArraySH::GetNamedItem(nsISupports *aNative, const nsAString& aName,
                              nsISupports **aResult)
{
  nsCOMPtr<nsIDOMPluginArray> array(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(array, NS_ERROR_UNEXPECTED);

  nsIDOMPlugin *plugin = nsnull;

  nsresult rv = array->NamedItem(aName, &plugin);

  *aResult = plugin;

  return rv;
}


// MimeTypeArray helper

nsresult
nsMimeTypeArraySH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                             nsISupports **aResult)
{
  nsCOMPtr<nsIDOMMimeTypeArray> array(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(array, NS_ERROR_UNEXPECTED);

  nsIDOMMimeType *mime_type = nsnull;
  nsresult rv = array->Item(aIndex, &mime_type);

  *aResult = mime_type;

  return rv;
}

nsresult
nsMimeTypeArraySH::GetNamedItem(nsISupports *aNative, const nsAString& aName,
                                nsISupports **aResult)
{
  nsCOMPtr<nsIDOMMimeTypeArray> array(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(array, NS_ERROR_UNEXPECTED);

  nsIDOMMimeType *mime_type = nsnull;

  nsresult rv = array->NamedItem(aName, &mime_type);

  *aResult = mime_type;

  return rv;
}


// StringArray helper

NS_IMETHODIMP
nsStringArraySH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, jsval id, jsval *vp,
                             PRBool *_retval)
{
  PRBool is_number = PR_FALSE;
  PRInt32 n = GetArrayIndexFromId(cx, id, &is_number);

  if (!is_number) {
    return NS_OK;
  }

  nsAutoString val;

  nsresult rv = GetStringAt(wrapper->Native(), n, val);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX: Null strings?

  JSAutoRequest ar(cx);

  JSString *str =
    ::JS_NewUCStringCopyN(cx, NS_REINTERPRET_CAST(const jschar *, val.get()),
                          val.Length());
  NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

  *vp = STRING_TO_JSVAL(str);

  return NS_SUCCESS_I_DID_SOMETHING;
}


// History helper

NS_IMETHODIMP
nsHistorySH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval)
{
  PRBool is_number = PR_FALSE;
  GetArrayIndexFromId(cx, id, &is_number);

  if (!is_number) {
    return NS_OK;
  }

  nsresult rv =
    sSecMan->CheckPropertyAccess(cx, obj, mData->mName, sItem_id,
                                 nsIXPCSecurityManager::ACCESS_CALL_METHOD);

  if (NS_FAILED(rv)) {
    // Let XPConnect know that the access was not granted.
    *_retval = PR_FALSE;

    return NS_OK;
  }

  // sec check

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

  return history->Item(aIndex, aResult);
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

  return media_list->Item(PRUint32(aIndex), aResult);
}


// StyleSheetList helper

nsresult
nsStyleSheetListSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                              nsISupports **aResult)
{
  nsCOMPtr<nsIDOMStyleSheetList> stylesheets(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(stylesheets, NS_ERROR_UNEXPECTED);

  nsIDOMStyleSheet *sheet = nsnull;
  nsresult rv = stylesheets->Item(aIndex, &sheet);

  *aResult = sheet;

  return rv;
}


// CSSValueList helper

nsresult
nsCSSValueListSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                            nsISupports **aResult)
{
  nsCOMPtr<nsIDOMCSSValueList> cssValueList(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(cssValueList, NS_ERROR_UNEXPECTED);

  nsIDOMCSSValue *cssValue = nsnull; // Weak, transfer the ownership over to aResult
  nsresult rv = cssValueList->Item(aIndex, &cssValue);

  *aResult = cssValue;

  return rv;
}


// CSSStyleDeclaration helper

nsresult
nsCSSStyleDeclSH::GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                              nsAString& aResult)
{
  if (aIndex < 0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMCSSStyleDeclaration> style_decl(do_QueryInterface(aNative));

  return style_decl->Item(PRUint32(aIndex), aResult);
}


// CSSRuleList scriptable helper

nsresult
nsCSSRuleListSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                           nsISupports **aResult)
{
  nsCOMPtr<nsIDOMCSSRuleList> list(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(list, NS_ERROR_UNEXPECTED);

  nsIDOMCSSRule *rule = nsnull; // Weak, transfer the ownership over to aResult
  nsresult rv = list->Item(aIndex, &rule);

  *aResult = rule;

  return rv;
}


#ifdef MOZ_XUL
// TreeColumns helper

nsresult
nsTreeColumnsSH::GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                           nsISupports **aResult)
{
  nsCOMPtr<nsITreeColumns> columns(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(columns, NS_ERROR_UNEXPECTED);

  nsITreeColumn* column = nsnull; // Weak, transfer the ownership over to aResult
  nsresult rv = columns->GetColumnAt(aIndex, &column);

  *aResult = column;

  return rv;
}

nsresult
nsTreeColumnsSH::GetNamedItem(nsISupports *aNative,
                              const nsAString& aName,
                              nsISupports **aResult)
{
  nsCOMPtr<nsITreeColumns> columns(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(columns, NS_ERROR_UNEXPECTED);

  nsITreeColumn* column = nsnull; // Weak, transfer the ownership over to aResult
  nsresult rv = columns->GetNamedColumn(aName, &column);

  *aResult = column;

  return rv;
}
#endif


// Storage scriptable helper

// One reason we need a newResolve hook is that in order for
// enumeration of storage object keys to work the keys we're
// enumerating need to exist on the storage object for the JS engine
// to find them.

NS_IMETHODIMP
nsStorageSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval)
{
  JSObject *realObj;
  wrapper->GetJSObject(&realObj);

  // First check to see if the property is defined on our prototype,
  // after converting id to a string if it's an integer.

  JSString *jsstr = JS_ValueToString(cx, id);
  if (!jsstr) {
    return JS_FALSE;
  }

  JSObject *proto = ::JS_GetPrototype(cx, realObj);
  JSBool hasProp;

  if (proto &&
      (::JS_HasUCProperty(cx, proto, ::JS_GetStringChars(jsstr),
                          ::JS_GetStringLength(jsstr), &hasProp) &&
       hasProp)) {
    // We found the property we're resolving on the prototype,
    // nothing left to do here then.

    return NS_OK;
  }

  // We're resolving property that doesn't exist on the prototype,
  // check if the key exists in the storage object.

  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));

  // GetItem() will return null if the caller can't access the session
  // storage item.
  nsCOMPtr<nsIDOMStorageItem> item;
  nsresult rv = storage->GetItem(nsDependentJSString(jsstr),
                                 getter_AddRefs(item));
  NS_ENSURE_SUCCESS(rv, rv);

  if (item) {
    if (!::JS_DefineUCProperty(cx, realObj, ::JS_GetStringChars(jsstr),
                               ::JS_GetStringLength(jsstr), JSVAL_VOID, nsnull,
                               nsnull, 0)) {
      return NS_ERROR_FAILURE;
    }

    *objp = realObj;
  }

  return NS_OK;
}

nsresult
nsStorageSH::GetNamedItem(nsISupports *aNative, const nsAString& aName,
                          nsISupports **aResult)
{
  nsCOMPtr<nsIDOMStorage> storage(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);

  // Weak, transfer the ownership over to aResult
  nsIDOMStorageItem* item = nsnull;
  nsresult rv = storage->GetItem(aName, &item);

  *aResult = item;

  return rv;
}

NS_IMETHODIMP
nsStorageSH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                         JSContext *cx, JSObject *obj, jsval id,
                         jsval *vp, PRBool *_retval)
{
  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);

  JSString *key = ::JS_ValueToString(cx, id);
  NS_ENSURE_TRUE(key, NS_ERROR_UNEXPECTED);

  JSString *value = ::JS_ValueToString(cx, *vp);
  NS_ENSURE_TRUE(value, NS_ERROR_UNEXPECTED);

  nsresult rv = storage->SetItem(nsDependentJSString(key),
                                 nsDependentJSString(value));
  if (NS_SUCCEEDED(rv)) {
    rv = NS_SUCCESS_I_DID_SOMETHING;
  }

  return rv;
}

NS_IMETHODIMP
nsStorageSH::DelProperty(nsIXPConnectWrappedNative *wrapper,
                         JSContext *cx, JSObject *obj, jsval id,
                         jsval *vp, PRBool *_retval)
{
  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);

  JSString *key = ::JS_ValueToString(cx, id);
  NS_ENSURE_TRUE(key, NS_ERROR_UNEXPECTED);

  nsresult rv = storage->RemoveItem(nsDependentJSString(key));
  if (NS_SUCCEEDED(rv)) {
    rv = NS_SUCCESS_I_DID_SOMETHING;
  }

  return rv;
}


NS_IMETHODIMP
nsStorageSH::NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, PRUint32 enum_op, jsval *statep,
                          jsid *idp, PRBool *_retval)
{
  nsTArray<nsString> *keys =
    (nsTArray<nsString> *)JSVAL_TO_PRIVATE(*statep);

  switch (enum_op) {
    case JSENUMERATE_INIT:
    {
      nsCOMPtr<nsPIDOMStorage> storage(do_QueryWrappedNative(wrapper));

      // XXXndeakin need to free the keys afterwards
      keys = storage->GetKeys();
      NS_ENSURE_TRUE(keys, NS_ERROR_OUT_OF_MEMORY);

      *statep = PRIVATE_TO_JSVAL(keys);

      if (idp) {
        *idp = INT_TO_JSVAL(keys->Length());
      }
      break;
    }
    case JSENUMERATE_NEXT:
      if (keys->Length() != 0) {
        nsString& key = keys->ElementAt(0);
        JSString *str =
          JS_NewUCStringCopyN(cx, NS_REINTERPRET_CAST(const jschar *,
                                                      key.get()),
                              key.Length());
        NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

        JS_ValueToId(cx, STRING_TO_JSVAL(str), idp);

        keys->RemoveElementAt(0);

        break;
      }

      // Fall through
    case JSENUMERATE_DESTROY:
      delete keys;

      *statep = JSVAL_NULL;

      break;
    default:
      NS_NOTREACHED("Bad call from the JS engine");

      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


// StorageList scriptable helper

nsresult
nsStorageListSH::GetNamedItem(nsISupports *aNative, const nsAString& aName,
                              nsISupports **aResult)
{
  nsCOMPtr<nsIDOMStorageList> storagelist(do_QueryInterface(aNative));
  NS_ENSURE_TRUE(storagelist, NS_ERROR_UNEXPECTED);

  // Weak, transfer the ownership over to aResult
  nsIDOMStorage* storage = nsnull;
  nsresult rv = storagelist->NamedItem(aName, &storage);

  *aResult = storage;

  return rv;
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
                                             PRBool *aHideFirstParamFromJS,
                                             nsIID * *aIIDOfResult,
                                             nsISupports **_retval)
{
  *aHideFirstParamFromJS = PR_FALSE;
  *aIIDOfResult = nsnull;

  nsCOMPtr<nsIDOMEvent> event(do_QueryInterface(aInitialThis));
  NS_ENSURE_TRUE(event, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMEventTarget> target;
  event->GetCurrentTarget(getter_AddRefs(target));

  *_retval = target;
  NS_IF_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMConstructorSH::Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                              JSObject *obj, PRUint32 argc, jsval *argv,
                              jsval *vp, PRBool *_retval)
{
  nsDOMConstructor *wrapped =
    NS_STATIC_CAST(nsDOMConstructor *, wrapper->Native());

#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMConstructor> is_constructor(do_QueryWrappedNative(wrapper));
    NS_ASSERTION(is_constructor, "How did we not get a constructor?");
  }
#endif

  return wrapped->Construct(wrapper, cx, obj, argc, argv, vp, _retval);
}

NS_IMETHODIMP
nsDOMConstructorSH::HasInstance(nsIXPConnectWrappedNative *wrapper,
                                JSContext *cx, JSObject *obj, jsval val,
                                PRBool *bp, PRBool *_retval)
{
  nsDOMConstructor *wrapped =
    NS_STATIC_CAST(nsDOMConstructor *, wrapper->Native());

#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMConstructor> is_constructor(do_QueryWrappedNative(wrapper));
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
