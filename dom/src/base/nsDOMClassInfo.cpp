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
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsIComponentRegistrar.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIXPConnect.h"
#include "nsIJSContextStack.h"
#include "nsIScriptContext.h"
#include "nsIXPCSecurityManager.h"
#include "nsIStringBundle.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "xptcall.h"
#include "prprf.h"

// JavaScript includes
#include "jsapi.h"
#include "jsnum.h"
#include "jsdbgapi.h"

// General helper includes
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOM3Document.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMEventListener.h"
#include "nsINodeInfo.h"
#include "nsContentUtils.h"

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

// DOM core includes
#include "nsDOMError.h"
#include "nsIDOMDOMException.h"
#include "nsIDOMNode.h"
#include "nsIDOM3Node.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMDOMStringList.h"
#include "nsIDOMNameList.h"

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
#include "nsIObjectFrame.h"
#include "nsINPRuntimePlugin.h"
#include "nsIScriptablePlugin.h"
#include "nsIPluginHost.h"
#include "nsPIPluginHost.h"

#ifdef OJI
// HTMLAppletElement helper includes
#include "nsIJVMManager.h"

// Oh, did I mention that I hate Microsoft for doing this to me?
#undef GetClassName

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
#include "nsIContentList.h"

// Event related includes
#include "nsIEventListenerManager.h"
#include "nsIDOMEventReceiver.h"

// CSS related includes
#include "nsIDOMStyleSheet.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSSRule.h"
#include "nsIDOMCSSRuleList.h"
#include "nsIDOMRect.h"
#include "nsIDOMRGBColor.h"

// XBL related includes.
#include "nsIXBLService.h"
#include "nsIXBLBinding.h"
#include "nsIBindingManager.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMElement.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIScriptGlobalObject.h"
#include "nsStyleSet.h"
#include "nsStyleContext.h"
#include "nsAutoPtr.h"

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
#include "nsIDOMEntity.h"
#include "nsIDOMEntityReference.h"
#include "nsIDOMNotation.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMPopupBlockedEvent.h"
#include "nsIDOMMutationEvent.h"
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
#include "nsIDOMNSHTMLAnchorElement.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMNSHTMLAreaElement.h"
#include "nsIDOMHTMLBRElement.h"
#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMHTMLBaseFontElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMNSHTMLButtonElement.h"
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
#include "nsIDOMXPathEvaluator.h"

#ifdef MOZ_SVG
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGAnimatedLengthList.h"
#include "nsIDOMSVGAnimatedPathData.h"
#include "nsIDOMSVGAnimatedPoints.h"
#include "nsIDOMSVGAnimPresAspRatio.h"
#include "nsIDOMSVGAnimatedRect.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsIDOMSVGCircleElement.h"
#include "nsIDOMSVGDefsElement.h"
#include "nsIDOMSVGDocument.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGEllipseElement.h"
#include "nsIDOMSVGException.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsIDOMSVGForeignObjectElem.h"
#include "nsIDOMSVGGElement.h"
#include "nsIDOMSVGImageElement.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGLengthList.h"
#include "nsIDOMSVGLineElement.h"
#include "nsIDOMSVGLocatable.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGNumber.h"
#include "nsIDOMSVGPathElement.h"
#include "nsIDOMSVGPathSeg.h"
#include "nsIDOMSVGPathSegList.h"
#include "nsIDOMSVGPoint.h"
#include "nsIDOMSVGPointList.h"
#include "nsIDOMSVGPolygonElement.h"
#include "nsIDOMSVGPolylineElement.h"
#include "nsIDOMSVGPresAspectRatio.h"
#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGRectElement.h"
#include "nsIDOMSVGScriptElement.h"
#include "nsIDOMSVGStylable.h"
#include "nsIDOMSVGStyleElement.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGTextElement.h"
#include "nsIDOMSVGTransform.h"
#include "nsIDOMSVGTransformable.h"
#include "nsIDOMSVGTransformList.h"
#include "nsIDOMSVGTSpanElement.h"
#include "nsIDOMSVGURIReference.h"
#endif // MOZ_SVG

#include "nsIImageDocument.h"

static NS_DEFINE_CID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_CID(kDOMSOF_CID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

static const char kDOMStringBundleURL[] =
  "chrome://communicator/locale/dom/dom.properties";

// NOTE: DEFAULT_SCRIPTABLE_FLAGS and DOM_DEFAULT_SCRIPTABLE_FLAGS
//       are defined in nsIDOMClassInfo.h.

#define NODE_SCRIPTABLE_FLAGS                                                 \
 ((DOM_DEFAULT_SCRIPTABLE_FLAGS |                                             \
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

#define EXTERNAL_OBJ_SCRIPTABLE_FLAGS                                         \
  (ELEMENT_SCRIPTABLE_FLAGS & ~nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY | \
   nsIXPCScriptable::WANT_SETPROPERTY |                                       \
   nsIXPCScriptable::WANT_CALL)

#define DOCUMENT_SCRIPTABLE_FLAGS                                             \
  (NODE_SCRIPTABLE_FLAGS |                                                    \
   nsIXPCScriptable::WANT_ADDPROPERTY |                                       \
   nsIXPCScriptable::WANT_DELPROPERTY |                                       \
   nsIXPCScriptable::WANT_GETPROPERTY)

#define ARRAY_SCRIPTABLE_FLAGS                                                \
  (DOM_DEFAULT_SCRIPTABLE_FLAGS |                                             \
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
  // that are implemented by GlobalWindowImpl can securely be exposed
  // to JS.


  NS_DEFINE_CLASSINFO_DATA(Window, nsWindowSH,
                           DEFAULT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_GETPROPERTY |
                           nsIXPCScriptable::WANT_SETPROPERTY |
                           nsIXPCScriptable::WANT_PRECREATE |
                           nsIXPCScriptable::WANT_FINALIZE |
                           nsIXPCScriptable::WANT_ADDPROPERTY |
                           nsIXPCScriptable::WANT_DELPROPERTY |
                           nsIXPCScriptable::WANT_ENUMERATE |
                           nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE)

  // Don't allow modifications to Location.prototype
  NS_DEFINE_CLASSINFO_DATA(Location, nsLocationSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS &
                           ~nsIXPCScriptable::ALLOW_PROP_MODS_TO_PROTOTYPE)

  NS_DEFINE_CLASSINFO_DATA(Navigator, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
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
  NS_DEFINE_CLASSINFO_DATA(Attr, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Text, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Comment, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CDATASection, nsNodeSH, NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(ProcessingInstruction, nsNodeSH,
                           NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(Entity, nsNodeSH, NODE_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(EntityReference, nsNodeSH, NODE_SCRIPTABLE_FLAGS)
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
  NS_DEFINE_CLASSINFO_DATA(HTMLCollection, nsHTMLCollectionSH,
                           ARRAY_SCRIPTABLE_FLAGS)
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
  NS_DEFINE_CLASSINFO_DATA(HTMLFrameElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
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
  NS_DEFINE_CLASSINFO_DATA(HTMLIFrameElement, nsHTMLElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
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
  NS_DEFINE_CLASSINFO_DATA(HTMLModElement, nsHTMLElementSH,
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
  NS_DEFINE_CLASSINFO_DATA(HTMLTableColGroupElement, nsHTMLElementSH,
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
  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(XULNodeList, NodeList, nsArraySH,
                                     ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(XULNamedNodeMap, NamedNodeMap,
                                     nsNamedNodeMapSH, ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA_WITH_NAME(XULAttr, Attr, nsDOMGenericSH,
                                     DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif
  NS_DEFINE_CLASSINFO_DATA(XULControllers, nsDOMGenericSH,
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
                           nsIXPCScriptable::WANT_GETPROPERTY |
                           nsIXPCScriptable::WANT_SETPROPERTY |
                           nsIXPCScriptable::WANT_NEWRESOLVE |
                           nsIXPCScriptable::WANT_PRECREATE |
                           nsIXPCScriptable::WANT_FINALIZE |
                           nsIXPCScriptable::WANT_ADDPROPERTY |
                           nsIXPCScriptable::WANT_DELPROPERTY |
                           nsIXPCScriptable::WANT_ENUMERATE |
                           nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE)

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

#ifdef MOZ_SVG
  // SVG document
  NS_DEFINE_CLASSINFO_DATA(SVGDocument, nsDocumentSH,
                           DOCUMENT_SCRIPTABLE_FLAGS)

  // SVG element classes
  NS_DEFINE_CLASSINFO_DATA(SVGCircleElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGDefsElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGEllipseElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGForeignObjectElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGGElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGImageElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGLineElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPathElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPolygonElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGPolylineElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGRectElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGScriptElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGStyleElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGSVGElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGTextElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGTSpanElement, nsElementSH,
                           ELEMENT_SCRIPTABLE_FLAGS)

  // other SVG classes
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedLength, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedLengthList, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedPoints, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedPreserveAspectRatio, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedRect, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedString, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGAnimatedTransformList, nsDOMGenericSH,
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
#endif // MOZ_SVG
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
jsval nsDOMClassInfo::sComponents_id      = JSVAL_VOID;
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

const JSClass *nsDOMClassInfo::sObjectClass = nsnull;

PRBool nsDOMClassInfo::sDoSecurityCheckInAddProperty = PR_TRUE;


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

  SET_JSVAL_TO_STRING(sTop_id,             cx, "top");
  SET_JSVAL_TO_STRING(sParent_id,          cx, "parent");
  SET_JSVAL_TO_STRING(sScrollbars_id,      cx, "scrollbars");
  SET_JSVAL_TO_STRING(sLocation_id,        cx, "location");
  SET_JSVAL_TO_STRING(sComponents_id,      cx, "Components");
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

  return NS_OK;
}

// static
nsresult
nsDOMClassInfo::WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
                           const nsIID& aIID, jsval *vp)
{
  if (!native) {
    *vp = JSVAL_NULL;

    return NS_OK;
  }

  NS_ENSURE_TRUE(sXPConnect, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

  nsresult rv = sXPConnect->WrapNative(cx, scope, native, aIID,
                                       getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject* obj = nsnull;
  rv = holder->GetJSObject(&obj);
  NS_ENSURE_SUCCESS(rv, rv);

  *vp = OBJECT_TO_JSVAL(obj);

  return rv;
}

// static
nsresult
nsDOMClassInfo::ThrowJSException(JSContext *cx, nsresult aResult)
{
  nsCOMPtr<nsIExceptionService> xs =
    do_GetService(NS_EXCEPTIONSERVICE_CONTRACTID);
  if (!xs)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIExceptionManager> xm;
  nsresult rv = xs->GetCurrentExceptionManager(getter_AddRefs(xm));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIException> exception;
  rv = xm->GetExceptionFromProvider(aResult, 0, getter_AddRefs(exception));

  jsval jv;
  rv = WrapNative(cx, ::JS_GetGlobalObject(cx), exception,
                  NS_GET_IID(nsIException), &jv);
  NS_ENSURE_SUCCESS(rv, rv);
  JS_SetPendingException(cx, jv);

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

  nsCOMPtr<nsIInterfaceInfoManager> iim =
    dont_AddRef(XPTI_GetInterfaceInfoManager());
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

  nsresult rv = NS_OK;
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

    nsCID cid;
    rv = nsComponentManager::ContractIDToClassID(contractId, &cid);

    if (NS_FAILED(rv)) {
      NS_WARNING("Bad contract id registered with the script namespace manager");
      continue;
    }

    rv = gNameSpaceManager->RegisterExternalClassName(categoryEntry.get(), cid);
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

#define DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES                                \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLElement)                              \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMElementCSSInlineStyle)                      \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)                                \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)

#define DOM_CLASSINFO_EVENT_MAP_ENTRIES                                       \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEvent)                                      \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSEvent)                                    \

#define DOM_CLASSINFO_UI_EVENT_MAP_ENTRIES                                    \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMUIEvent)                                    \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSUIEvent)                                  \
    DOM_CLASSINFO_EVENT_MAP_ENTRIES

#define DOM_CLASSINFO_MAP_END_WITH_XPATH                                      \
    xpathEvaluatorIID,                                                        \
  DOM_CLASSINFO_MAP_END

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

  nsCOMPtr<nsIComponentRegistrar> cr;
  NS_GetComponentRegistrar(getter_AddRefs(cr));

  const nsIID* xpathEvaluatorIID = nsnull;
  if (cr) {
    PRBool haveXPathDOM;
    cr->IsContractIDRegistered(NS_XPATH_EVALUATOR_CONTRACTID,
                               &haveXPathDOM);
    if (haveXPathDOM) {
      xpathEvaluatorIID = &NS_GET_IID(nsIDOMXPathEvaluator);
    }
  }

  DOM_CLASSINFO_MAP_BEGIN(Window, nsIDOMWindow)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindow)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMJSWindow)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowInternal)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventReceiver)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMViewCSS)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMAbstractView)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Location, nsIDOMLocation)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLocation)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSLocation)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Navigator, nsIDOMNavigator)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNavigator)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMJSNavigator)
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

  DOM_CLASSINFO_MAP_BEGIN(XMLDocument, nsIDOMXMLDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXMLDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSDocumentStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentView)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentRange)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentTraversal)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentXBL)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Document)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END_WITH_XPATH

  DOM_CLASSINFO_MAP_BEGIN(DocumentType, nsIDOMDocumentType)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentType)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Attr, nsIDOMAttr)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMAttr)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
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

  DOM_CLASSINFO_MAP_BEGIN(Entity, nsIDOMEntity)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEntity)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(EntityReference, nsIDOMEntityReference)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEntityReference)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSDocumentStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentView)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentRange)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentTraversal)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentXBL)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Document)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END_WITH_XPATH

  DOM_CLASSINFO_MAP_BEGIN(HTMLCollection, nsIDOMHTMLCollection)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLCollection)
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
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLAppletElement, nsIDOMHTMLAppletElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLAppletElement)
    DOM_CLASSINFO_GENERIC_HTML_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(HTMLAreaElement, nsIDOMHTMLAreaElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMHTMLAreaElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSHTMLAreaElement)
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


  // This should be removed, there are no users of this class any more.

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(HTMLTableColGroupElement,
                                      nsIDOMHTMLTableColElement)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentView)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentXBL)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSDocumentStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentRange)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentTraversal)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Document)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END_WITH_XPATH

  DOM_CLASSINFO_MAP_BEGIN(XULElement, nsIDOMXULElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XULCommandDispatcher, nsIDOMXULCommandDispatcher)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULCommandDispatcher)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XULNodeList, nsIDOMNodeList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNodeList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XULNamedNodeMap, nsIDOMNamedNodeMap)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNamedNodeMap)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XULAttr, nsIDOMAttr)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMAttr)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventReceiver)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSDocumentStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentView)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentRange)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentTraversal)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentXBL)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Document)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
  DOM_CLASSINFO_MAP_END_WITH_XPATH

#ifdef MOZ_XUL
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

#ifdef MOZ_SVG
#define DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES \
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGElement) \
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
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSDocument)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNode)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Node)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOM3Document)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentEvent)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentView)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentXBL)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSDocumentStyle)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDocumentRange)
  DOM_CLASSINFO_MAP_END_WITH_XPATH

  // SVG element classes

  DOM_CLASSINFO_MAP_BEGIN(SVGCircleElement, nsIDOMSVGCircleElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGCircleElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGDefsElement, nsIDOMSVGDefsElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGDefsElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGEllipseElement, nsIDOMSVGEllipseElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGEllipseElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGForeignObjectElement, nsIDOMSVGForeignObjectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGForeignObjectElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
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

  DOM_CLASSINFO_MAP_BEGIN(SVGLineElement, nsIDOMSVGLineElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLineElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathElement, nsIDOMSVGPathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedPathData)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
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

  DOM_CLASSINFO_MAP_BEGIN(SVGRectElement, nsIDOMSVGRectElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGRectElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGScriptElement, nsIDOMSVGScriptElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGScriptElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGURIReference)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
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
    DOM_CLASSINFO_SVG_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTextElement, nsIDOMSVGTextElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextPositioningElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextContentElement)
    DOM_CLASSINFO_SVG_GRAPHIC_ELEMENT_MAP_ENTRIES
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGTSpanElement, nsIDOMSVGTSpanElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextPositioningElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGTextContentElement)
  DOM_CLASSINFO_MAP_END

  // other SVG classes

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedLength, nsIDOMSVGAnimatedLength)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedLength)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedLengthList, nsIDOMSVGAnimatedLengthList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedLengthList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGAnimatedPoints, nsIDOMSVGAnimatedPoints)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGAnimatedPoints)
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

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegArcAbs, nsIDOMSVGPathSegArcAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegArcAbs)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegArcRel, nsIDOMSVGPathSegArcRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegArcRel)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegClosePath, nsIDOMSVGPathSegClosePath)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegClosePath)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoCubicAbs, nsIDOMSVGPathSegCurvetoCubicAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoCubicAbs)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoCubicRel, nsIDOMSVGPathSegCurvetoCubicRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoCubicRel)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoCubicSmoothAbs, nsIDOMSVGPathSegCurvetoCubicSmoothAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoCubicSmoothAbs)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoCubicSmoothRel, nsIDOMSVGPathSegCurvetoCubicSmoothRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoCubicSmoothRel)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoQuadraticAbs, nsIDOMSVGPathSegCurvetoQuadraticAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoQuadraticAbs)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoQuadraticRel, nsIDOMSVGPathSegCurvetoQuadraticRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoQuadraticRel)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoQuadraticSmoothAbs, nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegCurvetoQuadraticSmoothRel, nsIDOMSVGPathSegCurvetoQuadraticSmoothRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegCurvetoQuadraticSmoothRel)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegLinetoAbs, nsIDOMSVGPathSegLinetoAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegLinetoAbs)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegLinetoHorizontalAbs, nsIDOMSVGPathSegLinetoHorizontalAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegLinetoHorizontalAbs)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegLinetoHorizontalRel, nsIDOMSVGPathSegLinetoHorizontalRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegLinetoHorizontalRel)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegLinetoRel, nsIDOMSVGPathSegLinetoRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegLinetoRel)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegLinetoVerticalAbs, nsIDOMSVGPathSegLinetoVerticalAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegLinetoVerticalAbs)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegLinetoVerticalRel, nsIDOMSVGPathSegLinetoVerticalRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegLinetoVerticalRel)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegList, nsIDOMSVGPathSegList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegMovetoAbs, nsIDOMSVGPathSegMovetoAbs)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegMovetoAbs)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGPathSegMovetoRel, nsIDOMSVGPathSegMovetoRel)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGPathSegMovetoRel)
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

#endif // MOZ_SVG

#ifdef NS_DEBUG
  {
    PRUint32 i = sizeof(sClassInfoData) / sizeof(sClassInfoData[0]);

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
  *aClassName = nsCRT::strdup(mData->mName);

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
  NS_ERROR("Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Create(nsIXPConnectWrappedNative *wrapper,
                       JSContext *cx, JSObject *obj)
{
  NS_ERROR("Don't call me!");

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

  JSObject *proto = nsnull;

  wrapper->GetJSObjectPrototype(&proto);

  JSObject *proto_proto = ::JS_GetPrototype(cx, proto);

  JSClass *proto_proto_class = JS_GET_CLASS(cx, proto_proto);

  if (proto_proto_class != sObjectClass) {
    // We've just wrapped an object of a type that has been wrapped on
    // this scope already so the prototype of the xpcwrapped native's
    // prototype is already set up.

    return NS_OK;
  }

#ifdef DEBUG
  if (mData->mHasClassInterface) {
    nsCOMPtr<nsIInterfaceInfoManager> iim =
      dont_AddRef(XPTI_GetInterfaceInfoManager());

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

  JSObject *global = GetGlobalJSObject(cx, obj);

  jsval val;
  if (!::JS_GetProperty(cx, global, mData->mName, &val)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval id, jsval *vp,
                            PRBool *_retval)
{
  NS_ERROR("Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval id, jsval *vp,
                            PRBool *_retval)
{
  NS_ERROR("Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval id, jsval *vp,
                            PRBool *_retval)
{
  NS_ERROR("Don't call me!");

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval id, jsval *vp,
                            PRBool *_retval)
{
  NS_ERROR("Don't call me!");

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
  NS_ERROR("Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

nsresult
nsDOMClassInfo::ResolveConstructor(JSContext *cx, JSObject *obj,
                                   JSObject **objp)
{
  JSObject *global = GetGlobalJSObject(cx, obj);

  jsval val;
  if (!::JS_GetProperty(cx, global, mData->mName, &val)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!JSVAL_IS_PRIMITIVE(val)) {
    // If val is not an (non-null) object there either is no
    // constructor for this class, or someone messed with
    // window.classname, just fall through and let the JS engine
    // return the Object constructor.

    JSString *str = JSVAL_TO_STRING(sConstructor_id);
    if (!::JS_SetUCProperty(cx, obj, ::JS_GetStringChars(str),
                            ::JS_GetStringLength(str), &val)) {
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
  NS_ERROR("Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Finalize(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj)
{
  NS_ERROR("Don't call me!");

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
       mode_type == JSACC_PARENT)
      && sSecMan) {

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
  NS_ERROR("Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, PRUint32 argc, jsval *argv,
                          jsval *vp, PRBool *_retval)
{
  NS_ERROR("Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsval val, PRBool *bp,
                            PRBool *_retval)
{
  NS_ERROR("Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Mark(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JSObject *obj, void *arg, PRUint32 *_retval)
{
  NS_ERROR("Don't call me!");

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
  sComponents_id      = JSVAL_VOID;
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
  
  nsCOMPtr<nsISupports> native;
  wrapper->GetNative(getter_AddRefs(native));

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(native));

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

  do {
    fp = ::JS_FrameIterator(cx, &fp);

    if(!fp) {
      break;
    }

    fp_obj = ::JS_GetFrameFunctionObject(cx, fp);
  } while (!fp_obj);

  if (fp_obj) {
    JSObject *global = GetGlobalJSObject(cx, fp_obj);

    JSObject *wrapper_obj = nsnull;
    wrapper->GetJSObject(&wrapper_obj);

    if (global != wrapper_obj) {
      return PR_TRUE;
    }
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

  // Don't check when getting the Components property, since we check
  // its properties anyway. This will help performance.
  if (id == sComponents_id &&
      accessMode == nsIXPCSecurityManager::ACCESS_GET_PROPERTY && isWindow) {
    return NS_OK;
  }

  nsCOMPtr<nsISupports> native;
  wrapper->GetNative(getter_AddRefs(native));

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
      // document was created is not accessable and we can't do a
      // security check, but the document must remain
      // scriptable. Documents loaded through these methods have
      // already been vetted by the security manager before they were
      // loaded, so allowing access here w/o doing a security check
      // here is probably safe anyway.

      return NS_OK;
    }
  }

  nsIScriptContext *scx = sgo->GetContext();

  if (!scx || !scx->IsContextInitialized()) {
    return NS_OK;
  }

  JSObject *global = sgo->GetGlobalJSObject();

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
  NS_WARN_IF_FALSE(sgo, "nativeObj not a node!");

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
      JSVAL_IS_INT(id)) {
    // Nothing to do here if we're either assigning or declaring,
    // resolving a class name, doing a qualified resolve, or
    // resolving a number.

    return JS_TRUE;
  }

  nsIHTMLDocument *doc = (nsIHTMLDocument *)::JS_GetPrivate(cx, obj);

  if (!doc || doc->GetCompatibilityMode() != eCompatibility_NavQuirks) {
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
    nsresult rv = WrapNative(cx, GetGlobalJSObject(cx, obj), result,
                             NS_GET_IID(nsISupports), &v);
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
JSObject *
nsWindowSH::GetInvalidatedGlobalScopePolluter(JSContext *cx, JSObject *obj)
{
  JSObject *proto;

  while ((proto = ::JS_GetPrototype(cx, obj))) {
    if (JS_GET_CLASS(cx, proto) == &sGlobalScopePolluterClass) {
      nsIHTMLDocument *doc = (nsIHTMLDocument *)::JS_GetPrivate(cx, proto);

      NS_IF_RELEASE(doc);

      ::JS_SetPrivate(cx, proto, nsnull);

      // Pull the global scope polluter out of the prototype chain.
      ::JS_SetPrototype(cx, obj, ::JS_GetPrototype(cx, proto));

      ::JS_ClearScope(cx, proto);

      break;
    }

    obj = proto;
  }

  return proto;
}

// static
nsresult
nsWindowSH::InstallGlobalScopePolluter(JSContext *cx, JSObject *obj,
                                       JSObject *oldPolluter,
                                       nsIHTMLDocument *doc)
{
  // If global scope pollution is disabled, do nothing
  if (sDisableGlobalScopePollutionSupport) {
    return NS_OK;
  }

  JSObject *gsp = oldPolluter;

  if (!gsp) {
    gsp = ::JS_NewObject(cx, &sGlobalScopePolluterClass, nsnull, obj);
    if (!gsp) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
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
  // reinitialzation).
  NS_IF_ADDREF(doc);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, jsval *vp, PRBool *_retval)
{
  // The order in which things are done in this method are a bit
  // whacky, that's because this method is *extremely* performace
  // critical. Don't touch this unless you know what you're doing.

  if (JSVAL_IS_NUMBER(id)) {
    // If we're accessing a numeric property we'll treat that as if
    // window.frames.n is accessed (since window.frames === window),
    // if window.frames.n is a child frame, wrap the frame and return
    // it without doing a security check.

    nsCOMPtr<nsISupports> native;
    wrapper->GetNative(getter_AddRefs(native));

    nsCOMPtr<nsIDOMWindowInternal> win(do_QueryInterface(native));

    nsCOMPtr<nsIDOMWindowCollection> frames;
    win->GetFrames(getter_AddRefs(frames));

    if (frames) {
      nsCOMPtr<nsIDOMWindow> frame;
      frames->Item(JSVAL_TO_INT(id), getter_AddRefs(frame));

      if (frame) {
        // A numeric property accessed and the numeric proerty is a
        // child frame, wrap the child frame without doing a security
        // check and return.

        return WrapNative(cx, ::JS_GetGlobalObject(cx), frame,
                          NS_GET_IID(nsIDOMWindow), vp);
      }
    }
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

      nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
      sXPConnect->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(*vp),
                                             getter_AddRefs(wrapper));

      if (wrapper) {
        nsCOMPtr<nsISupports> native;
        wrapper->GetNative(getter_AddRefs(native));

        nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(native));

        if (window) {
          // Yup, *vp is a window object, return early (*vp is already
          // the window, so no need to wrap it again).

          return NS_OK;
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
    JSString *val = ::JS_ValueToString(cx, *vp);
    NS_ENSURE_TRUE(val, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsISupports> native;
    wrapper->GetNative(getter_AddRefs(native));

    nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(native));
    NS_ENSURE_TRUE(window, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMLocation> location;
    nsresult rv = window->GetLocation(getter_AddRefs(location));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = location->SetHref(nsDependentJSString(val));
    NS_ENSURE_SUCCESS(rv, rv);

    return WrapNative(cx, obj, location, NS_GET_IID(nsIDOMLocation), vp);
  }

  return nsEventReceiverSH::SetProperty(wrapper, cx, obj, id, vp, _retval);
}

NS_IMETHODIMP
nsWindowSH::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, jsval *vp,
                        PRBool *_retval)
{
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

static JSBool
BaseStubConstructor(const nsGlobalNameStruct *name_struct, JSContext *cx,
                    JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult rv;
  nsCOMPtr<nsISupports> native;
  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    native = do_CreateInstance(name_struct->mCID, &rv);
  } else if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    native = do_CreateInstance(name_struct->mAlias->mCID, &rv);
  } else {
    native = do_CreateInstance(*name_struct->mData->mConstructorCID, &rv);
  }
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to create the object");
    nsDOMClassInfo::ThrowJSException(cx, rv);

    return JS_FALSE;
  }

  nsCOMPtr<nsIJSNativeInitializer> initializer(do_QueryInterface(native));
  if (initializer) {
    rv = initializer->Initialize(cx, obj, argc, argv);
    if (NS_FAILED(rv)) {
      nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_NOT_INITIALIZED);

      return JS_FALSE;
    }
  }

  nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(native));
  if (owner) {
    nsIScriptContext *context = nsJSUtils::GetStaticScriptContext(cx, obj);
    if (!context) {
      nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

      return JS_FALSE;
    }

    JSObject* new_obj;
    rv = owner->GetScriptObject(context, (void**)&new_obj);

    if (NS_SUCCEEDED(rv)) {
      *rval = OBJECT_TO_JSVAL(new_obj);
    }

    return rv;
  }

  rv = nsDOMGenericSH::WrapNative(cx, ::JS_GetGlobalObject(cx), native,
                                  NS_GET_IID(nsISupports), rval);

  return NS_SUCCEEDED(rv) ? JS_TRUE : JS_FALSE;
}

static nsresult
DefineInterfaceConstants(JSContext *cx, JSObject *obj, const nsIID *aIID)
{
  nsCOMPtr<nsIInterfaceInfoManager> iim =
    dont_AddRef(XPTI_GetInterfaceInfoManager());
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

JS_STATIC_DLL_CALLBACK(JSBool)
DOMJSClass_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                     jsval *rval)
{
  JSObject* class_obj = JSVAL_TO_OBJECT(argv[-2]);
  if (!class_obj) {
    NS_ERROR("DOMJSClass_Construct couldn't get constructor object.");
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

    return JS_FALSE;
  }

  const PRUnichar* class_name =
      NS_CONST_CAST(const PRUnichar *,
                    NS_STATIC_CAST(PRUnichar *,
                                   ::JS_GetPrivate(cx, class_obj)));

  extern nsScriptNameSpaceManager *gNameSpaceManager;
  if (!class_name || !gNameSpaceManager) {
    NS_ERROR("DOMJSClass_Construct can't get name or namespace manager.");
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

    return JS_FALSE;
  }

  const nsGlobalNameStruct *name_struct = nsnull;
  gNameSpaceManager->LookupName(nsDependentString(class_name),
                                &name_struct);
  if (!name_struct) {
    NS_ERROR("Name isn't in hash.");
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

    return JS_FALSE;
  }

  if ((name_struct->mType != nsGlobalNameStruct::eTypeExternalClassInfo ||
       !name_struct->mData->mConstructorCID) &&
      name_struct->mType != nsGlobalNameStruct::eTypeExternalConstructor &&
      name_struct->mType != nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    // ignore return value, we return JS_FALSE anyway
    NS_ERROR("object instantiated without constructor");
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

    return JS_FALSE;
  }

  return BaseStubConstructor(name_struct, cx, obj, argc, argv, rval);
}

JS_STATIC_DLL_CALLBACK(JSBool)
DOMJSClass_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
  // No need to look these up in the hash.
  if (JSVAL_IS_PRIMITIVE(v)) {
    return JS_TRUE;
  }

  JSObject *dom_obj = JSVAL_TO_OBJECT(v);
  NS_ASSERTION(dom_obj, "DOMJSClass_HasInstance couldn't get object");

  JSClass *dom_class = JS_GET_CLASS(cx, dom_obj);
  if (!dom_class) {
    NS_ERROR("DOMJSClass_HasInstance can't get class.");
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

    return JS_FALSE;
  }

  const nsGlobalNameStruct *name_struct = nsnull;

  extern nsScriptNameSpaceManager *gNameSpaceManager;
  if (!gNameSpaceManager) {
    NS_ERROR("DOMJSClass_HasInstance can't get namespace manager.");
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

    return JS_FALSE;
  }

  gNameSpaceManager->LookupName(NS_ConvertASCIItoUCS2(dom_class->name),
                                &name_struct);
  if (!name_struct) {
    // Name isn't in hash, not a DOM object.
    return JS_TRUE;
  }

  NS_ASSERTION(name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor ||
               name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo ||
               name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias,
               "The constructor was set up with a struct of the wrong type.");

  const PRUnichar* class_name =
      NS_CONST_CAST(const PRUnichar *,
                    NS_STATIC_CAST(PRUnichar *,
                                   ::JS_GetPrivate(cx, obj)));
  if (!class_name) {
    NS_ERROR("DOMJSClass_HasInstance can't get name.");
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

    return JS_FALSE;
  }

  const nsGlobalNameStruct *class_name_struct = nsnull;
  gNameSpaceManager->LookupName(nsDependentString(class_name),
                                &class_name_struct);
  if (!class_name_struct) {
    NS_ERROR("Name isn't in hash.");
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

    return JS_FALSE;
  }

  if (name_struct == class_name_struct) {
    *bp = JS_TRUE;

    return JS_TRUE;
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
      nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

      return JS_FALSE;
    }

    if (alias_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
      class_iid =
        sClassInfoData[alias_struct->mDOMClassInfoID].mProtoChainInterface;
    } else if (alias_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
      class_iid = alias_struct->mData->mProtoChainInterface;
    } else {
      NS_ERROR("Expected eTypeClassConstructor or eTypeExternalClassInfo.");
      nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

      return JS_FALSE;
    }
  } else {
    *bp = JS_FALSE;

    return JS_TRUE;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    name_struct = gNameSpaceManager->GetConstructorProto(name_struct);
    if (!name_struct) {
      NS_ERROR("Couldn't get constructor prototype.");
      nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

      return JS_FALSE;
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

  nsCOMPtr<nsIInterfaceInfoManager> iim =
    dont_AddRef(XPTI_GetInterfaceInfoManager());
  if (!iim) {
    NS_ERROR("DOMJSClass_HasInstance can't get interface info mgr.");
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

    return JS_FALSE;
  }

  nsCOMPtr<nsIInterfaceInfo> if_info;
  PRUint32 count = 0;
  const nsIID* class_interface;
  while ((class_interface = ci_data->mInterfaces[count++])) {
    if (class_iid->Equals(*class_interface)) {
      *bp = JS_TRUE;

      return JS_TRUE;
    }

    iim->GetInfoForIID(class_interface, getter_AddRefs(if_info));
    if (!if_info) {
      NS_ERROR("DOMJSClass_HasInstance can't get interface info.");
      nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

      return JS_FALSE;
    }

    if_info->HasAncestor(class_iid, bp);

    if (*bp) {
      return JS_TRUE;
    }
  }

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
DOMJSClass_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                    jsval *rval)
{
  const PRUnichar* class_name =
      NS_CONST_CAST(const PRUnichar *,
                    NS_STATIC_CAST(PRUnichar *,
                                   ::JS_GetPrivate(cx, obj)));
  if (!class_name) {
    NS_ERROR("DOMJSClass_HasInstance can't get name.");
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);

    return JS_FALSE;
  }

  nsAutoString resultString('[');
  resultString.Append(class_name);
  resultString.Append(PRUnichar(']'));

  JSString* str = ::JS_NewUCStringCopyN(cx,
                                        NS_REINTERPRET_CAST(const jschar *,
                                                            resultString.get()),
                                        resultString.Length());
  NS_ENSURE_TRUE(str, JS_FALSE);

  *rval = STRING_TO_JSVAL(str);

  return JS_TRUE;
}

JSClass nsDOMClassInfo::sDOMJSClass = {
  "DOM Class", JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  nsnull, nsnull, nsnull, DOMJSClass_Construct,
  nsnull, DOMJSClass_HasInstance
};

JSFunctionSpec nsDOMClassInfo::sDOMJSClass_methods[] = {
  {"toString", DOMJSClass_toString, 0, 0, 0},
  {0, 0, 0, 0, 0}
};

// static
nsresult
nsDOMClassInfo::InitDOMJSClass(JSContext *cx, JSObject *obj)
{
  JSObject *proto = ::JS_InitClass(cx, obj, nsnull, &sDOMJSClass, nsnull, 0,
                                   nsnull, sDOMJSClass_methods, nsnull,
                                   nsnull);
  NS_ASSERTION(proto, "Can't initialize DOM class proto.");
  if (!proto) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

// static
nsresult
nsWindowSH::GlobalResolve(nsISupports *native, JSContext *cx, JSObject *obj,
                          JSString *str, PRUint32 flags, PRBool *did_resolve)
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

    rv = creator->RegisterDOMCI(NS_ConvertUCS2toUTF8(name).get(), sof);
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

    sDoSecurityCheckInAddProperty = PR_FALSE;

    JSObject* class_obj = ::JS_DefineObject(cx, obj, ::JS_GetStringBytes(str),
                                            &sDOMJSClass, 0, 0);

    sDoSecurityCheckInAddProperty = PR_TRUE;

    if (!class_obj) {
      return NS_ERROR_UNEXPECTED;
    }

    if (!::JS_SetPrivate(cx, class_obj,
                         NS_CONST_CAST(void *,
                                       NS_STATIC_CAST(const void *,
                                                      class_name)))) {
      return NS_ERROR_UNEXPECTED;
    }

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

    sDoSecurityCheckInAddProperty = PR_FALSE;

    JSObject* class_obj = ::JS_DefineObject(cx, obj, ::JS_GetStringBytes(str),
                                            &sDOMJSClass, 0, 0);

    sDoSecurityCheckInAddProperty = PR_TRUE;

    if (!class_obj) {
      return NS_ERROR_UNEXPECTED;
    }

    if (!::JS_SetPrivate(cx, class_obj,
                         NS_CONST_CAST(void *,
                                       NS_STATIC_CAST(const void *,
                                                      class_name)))) {
      return NS_ERROR_UNEXPECTED;
    }

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

      nsCOMPtr<nsIInterfaceInfoManager> iim =
        dont_AddRef(XPTI_GetInterfaceInfoManager());
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

      nsresult rv =
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
    } else if (name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
      nsCOMPtr<nsIClassInfo> ci = GetClassInfoInstance(name_struct->mData);
      NS_ENSURE_TRUE(ci, NS_ERROR_UNEXPECTED);

      nsCOMPtr<nsIXPConnectJSObjectHolder> proto_holder;

      nsresult rv =
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

    jsval v = OBJECT_TO_JSVAL(dot_prototype);

    if (!::JS_SetProperty(cx, class_obj, "prototype", &v)) {
      return NS_ERROR_UNEXPECTED;
    }

    *did_resolve = PR_TRUE;

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    // If there was a JS_DefineUCObject() we could use it here...
    JSObject* class_obj = ::JS_DefineObject(cx, obj, ::JS_GetStringBytes(str),
                                            &sDOMJSClass, 0, 0);
    if (!class_obj) {
      return NS_ERROR_UNEXPECTED;
    }

    if (!::JS_SetPrivate(cx, class_obj,
                         NS_CONST_CAST(void *,
                                       NS_STATIC_CAST(const void *,
                                                      class_name)))) {
      return NS_ERROR_UNEXPECTED;
    }

    *did_resolve = PR_TRUE;

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeProperty) {
    nsCOMPtr<nsISupports> native(do_CreateInstance(name_struct->mCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    jsval prop_val; // Property value.

    nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(native));
    if (owner) {
      nsIScriptContext *context = nsJSUtils::GetStaticScriptContext(cx, obj);
      NS_ENSURE_TRUE(context, NS_ERROR_UNEXPECTED);

      JSObject *prop_obj = nsnull;
      rv = owner->GetScriptObject(context, (void**)&prop_obj);
      NS_ENSURE_TRUE(prop_obj, NS_ERROR_UNEXPECTED);

      prop_val = OBJECT_TO_JSVAL(prop_obj);
    } else {
      rv = WrapNative(cx, ::JS_GetGlobalObject(cx), native,
                      NS_GET_IID(nsISupports), &prop_val);
    }

    NS_ENSURE_SUCCESS(rv, rv);

    PRBool retval = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                          ::JS_GetStringLength(str),
                                          prop_val, nsnull, nsnull,
                                          JSPROP_ENUMERATE);
    *did_resolve = PR_TRUE;

    return retval ? NS_OK : NS_ERROR_FAILURE;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeDynamicNameSet) {
    nsCOMPtr<nsIScriptExternalNameSet> nameset =
      do_CreateInstance(name_struct->mCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(native));
    NS_ENSURE_TRUE(sgo, NS_ERROR_UNEXPECTED);

    nsIScriptContext *context = sgo->GetContext();
    NS_ENSURE_TRUE(context, NS_ERROR_UNEXPECTED);

    rv = nameset->InitializeNameSet(context);

    *did_resolve = PR_TRUE;
  }

  return rv;
}

// static
nsresult
nsWindowSH::OnDocumentChanged(JSContext *cx, JSObject *obj,
                              nsIDOMWindow *window)
{
  nsCOMPtr<nsIDOMDocument> document;
  nsresult rv = window->GetDocument(getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);

  jsval v;
  rv = WrapNative(cx, obj, document, NS_GET_IID(nsIDOMDocument), &v);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_NAMED_LITERAL_STRING(doc_str, "document");

  if (!::JS_DefineUCProperty(cx, obj, NS_REINTERPRET_CAST(const jschar *,
                                                          doc_str.get()),
                             doc_str.Length(), v, nsnull,
                             nsnull, JSPROP_READONLY | JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, jsval id, PRUint32 flags,
                       JSObject **objp, PRBool *_retval)
{
  if (JSVAL_IS_STRING(id)) {
    nsCOMPtr<nsISupports> native;
    wrapper->GetNative(getter_AddRefs(native));

    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(native));
    NS_ENSURE_TRUE(sgo, NS_ERROR_UNEXPECTED);

    nsIScriptContext *my_context = sgo->GetContext();

    if (!my_context || !my_context->IsContextInitialized()) {
      // The context is not yet initialized so there's nothing we can do
      // here yet.

      return NS_OK;
    }

    nsresult rv = NS_OK;

    // It is not worth calling JS_ResolveStandardClass() if we are
    // resolving for assignment, since only read-write properties
    // get dealt with there.
    if (!(flags & JSRESOLVE_ASSIGNING)) {
      JSContext *my_cx = (JSContext *) my_context->GetNativeContext();
      JSBool did_resolve = JS_FALSE;

      // Resolve standard classes on my_context's JSContext, not on
      // cx, in case the two contexts have different origins.  We want
      // lazy standard class initialization to behave as if it were
      // done eagerly, on each window's own context (not on some other
      // window-caller's context).

      if (!::JS_ResolveStandardClass(my_cx, obj, id, &did_resolve)) {
        *_retval = JS_FALSE;

        return NS_ERROR_UNEXPECTED;
      }

      if (did_resolve) {
        *objp = obj;

        return NS_OK;
      }

      // We want this code to be before the child frame lookup code
      // below so that a child frame named 'constructor' doesn't
      // shadow the window's constructor property.
      if (id == sConstructor_id) {
        return ResolveConstructor(cx, obj, objp);
      }
    }

    // Hmm, we do an awful lot of QIs here; maybe we should add a
    // method on an interface that would let us just call into the
    // window code directly...

    JSString *str = JSVAL_TO_STRING(id);

    nsCOMPtr<nsIDocShellTreeNode> dsn(do_QueryInterface(sgo->GetDocShell()));

    PRInt32 count = 0;

    if (dsn) {
      dsn->GetChildCount(&count);
    }

    if (count > 0) {
      nsCOMPtr<nsIDocShellTreeItem> child;

      const jschar *chars = ::JS_GetStringChars(str);

      dsn->FindChildWithName(NS_REINTERPRET_CAST(const PRUnichar*, chars),
                             PR_FALSE, PR_TRUE, nsnull,
                             getter_AddRefs(child));

      nsCOMPtr<nsIDOMWindow> child_win(do_GetInterface(child));

      if (child_win) {
        // We found a subframe of the right name, define the property
        // on the wrapper so that ::NewResolve() doesn't get called
        // again for this property name.

        jsval v;
        rv = WrapNative(cx, ::JS_GetGlobalObject(cx), child_win,
                        NS_GET_IID(nsIDOMWindowInternal), &v);
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

        sDoSecurityCheckInAddProperty = PR_FALSE;

        PRBool ok = ::JS_DefineUCProperty(cx, obj, chars,
                                          ::JS_GetStringLength(str), v, nsnull,
                                          nsnull, 0);

        sDoSecurityCheckInAddProperty = PR_TRUE;

        if (!ok) {
          return NS_ERROR_FAILURE;
        }

        *objp = obj;

        return NS_OK;
      }
    }

    // It is not worth calling GlobalResolve() if we are resolving
    // for assignment, since only read-write properties get dealt
    // with there.
    if (!(flags & JSRESOLVE_ASSIGNING)) {
      // Call GlobalResolve() after we call FindChildWithName() so
      // that named child frames will override external properties
      // which have been registered with the script namespace manager.

      JSBool did_resolve = JS_FALSE;
      rv = GlobalResolve(native, cx, obj, str, flags, &did_resolve);
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
      JSObject* getterObj;
      rv = my_context->CompileFunction(obj,
                                       nsCAutoString("_content"),
                                       0,
                                       nsnull,
                                       NS_LITERAL_STRING("return this.content;"),
                                       "javascript:this.content; // See " __FILE__,
                                       1, // lineno
                                       PR_FALSE,
                                       (void **) &getterObj);
      NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && getterObj, NS_ERROR_FAILURE);

      if (!::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                 ::JS_GetStringLength(str), JSVAL_VOID,
                                 (JSPropertyOp)getterObj, nsnull,
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

      nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(native));
      NS_ENSURE_TRUE(window, NS_ERROR_UNEXPECTED);

      nsCOMPtr<nsIDOMLocation> location;
      rv = window->GetLocation(getter_AddRefs(location));
      NS_ENSURE_SUCCESS(rv, rv);

      jsval v;
      rv = WrapNative(cx, obj, location, NS_GET_IID(nsIDOMLocation), &v);
      NS_ENSURE_SUCCESS(rv, rv);

      sDoSecurityCheckInAddProperty = PR_FALSE;

      JSBool ok = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                        ::JS_GetStringLength(str), v, nsnull,
                                        nsnull, JSPROP_ENUMERATE);

      sDoSecurityCheckInAddProperty = PR_TRUE;

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
        nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(native));
        NS_ENSURE_TRUE(window, NS_ERROR_UNEXPECTED);

        nsCOMPtr<nsIDOMNavigator> navigator;
        rv = window->GetNavigator(getter_AddRefs(navigator));
        NS_ENSURE_SUCCESS(rv, rv);

        jsval v;
        rv = WrapNative(cx, obj, navigator, NS_GET_IID(nsIDOMNavigator), &v);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                   ::JS_GetStringLength(str), v, nsnull,
                                   nsnull, JSPROP_ENUMERATE)) {
          return NS_ERROR_FAILURE;
        }

        *objp = obj;

        return NS_OK;
      }

      if (id == sDocument_id) {
        nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(native));
        NS_ENSURE_TRUE(window, NS_ERROR_UNEXPECTED);

        rv = OnDocumentChanged(cx, obj, window);
        NS_ENSURE_SUCCESS(rv, rv);

        *objp = obj;

        return NS_OK;
      }

      if (id == sWindow_id) {
        jsval v;
        rv = WrapNative(cx, obj, native, NS_GET_IID(nsIDOMWindow), &v);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                   ::JS_GetStringLength(str), v, nsnull,
                                   nsnull,
                                   JSPROP_READONLY | JSPROP_ENUMERATE)) {
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

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::Finalize(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JSObject *obj)
{
  nsCOMPtr<nsISupports> native;
  wrapper->GetNative(getter_AddRefs(native));
  NS_ENSURE_TRUE(native, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(native));
  NS_ENSURE_TRUE(sgo, NS_ERROR_UNEXPECTED);

  sgo->OnFinalize(obj);

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


// DOM Node helper

NS_IMETHODIMP
nsNodeSH::PreCreate(nsISupports *nativeObj, JSContext *cx, JSObject *globalObj,
                    JSObject **parentObj)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(nativeObj));
  nsCOMPtr<nsIDocument> doc;

  if (content) {
    // Make sure that we get the owner document of the content node, in case
    // we're in document teardown.  If we are, it's important to *not* use
    // globalObj as the nodes parent since that would give the node the
    // principal of globalObj (i.e. the principal of the document that's being
    // loaded) and not the principal of the document that's being unloaded.
    // See http://bugzilla.mozilla.org/show_bug.cgi?id=227417
    doc = content->GetOwnerDoc();
  }

  if (!doc) {
    doc = do_QueryInterface(nativeObj);

    if (!doc) {
      // No document reachable from nativeObj, use the global object
      // that was passed to this method.

      *parentObj = globalObj;

      return NS_OK;
    }
  }

  nsISupports *native_parent;

  if (content) {
    if (content->IsContentOfType(nsIContent::eXUL)) {
      // For XUL elements, use the parent, if any.
      native_parent = content->GetParent();

      if (!native_parent) {
        native_parent = doc;
      }
    } else {
      // For non-XUL elements, use the document as scope parent.
      native_parent = doc;

      // But for HTML form controls, use the form as scope parent.
      if (content->IsContentOfType(nsIContent::eELEMENT |
                                   nsIContent::eHTML |
                                   nsIContent::eHTML_FORM_CONTROL)) {
        nsCOMPtr<nsIFormControl> form_control(do_QueryInterface(content));

        if (form_control) {
          nsCOMPtr<nsIDOMHTMLFormElement> form;
          form_control->GetForm(getter_AddRefs(form));

          if (form) {
            // Found a form, use it.
            native_parent = form;
          }
        }
      }
    }
  } else {
    // We're called for a document object (since content is null),
    // set the parent to be the document's global object, if there
    // is one

    // Get the script global object from the document.

    native_parent = doc->GetScriptGlobalObject();

    if (!native_parent) {
      // No global object reachable from this document, use the
      // global object that was passed to this method.

      *parentObj = globalObj;

      return NS_OK;
    }
  }

  jsval v;
  nsresult rv = WrapNative(cx, ::JS_GetGlobalObject(cx), native_parent,
                           NS_GET_IID(nsISupports), &v);

  *parentObj = JSVAL_TO_OBJECT(v);

  return rv;
}

NS_IMETHODIMP
nsNodeSH::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                      JSObject *obj, jsval id, jsval *vp, PRBool *_retval)
{
  nsCOMPtr<nsISupports> native;
  wrapper->GetNative(getter_AddRefs(native));

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
    doc->AddReference(content, wrapper);
  }

  return nsEventReceiverSH::AddProperty(wrapper, cx, obj, id, vp, _retval);
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
    return id == sOnpaint_id;
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

nsresult
nsEventReceiverSH::RegisterCompileHandler(nsIXPConnectWrappedNative *wrapper,
                                          JSContext *cx, JSObject *obj,
                                          jsval id, PRBool compile,
                                          PRBool *did_compile)
{
  *did_compile = PR_FALSE;

  if (!IsEventName(id)) {
    return NS_OK;
  }

  nsIScriptContext *script_cx = nsJSUtils::GetStaticScriptContext(cx, obj);
  NS_ENSURE_TRUE(script_cx, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISupports> native;
  wrapper->GetNative(getter_AddRefs(native));
  NS_ABORT_IF_FALSE(native, "No native!");

  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(native));
  NS_ENSURE_TRUE(receiver, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIEventListenerManager> manager;

  receiver->GetListenerManager(getter_AddRefs(manager));
  NS_ENSURE_TRUE(manager, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIAtom> atom(do_GetAtom(nsDependentJSString(id)));
  NS_ENSURE_TRUE(atom, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;

  if (compile) {
    rv = manager->CompileScriptEventListener(script_cx, native, atom,
                                             did_compile);
  } else {
    rv = manager->RegisterScriptEventListener(script_cx, native, atom);
  }

  return rv;
}

NS_IMETHODIMP
nsEventReceiverSH::NewResolve(nsIXPConnectWrappedNative *wrapper,
                              JSContext *cx, JSObject *obj, jsval id,
                              PRUint32 flags, JSObject **objp, PRBool *_retval)
{
  // If we're assigning to an on* property, we'll register the handler
  // in our ::SetProperty() hook, so no need to do it here too.
  if (!JSVAL_IS_STRING(id) || (flags & JSRESOLVE_ASSIGNING)) {
    return NS_OK;
  }

  PRBool did_compile = PR_FALSE;

  nsresult rv = RegisterCompileHandler(wrapper, cx, obj, id, PR_TRUE,
                                       &did_compile);
  NS_ENSURE_SUCCESS(rv, rv);

  if (did_compile) {
    *objp = obj;
  }

  return nsDOMClassInfo::NewResolve(wrapper, cx, obj, id, flags, objp,
                                    _retval);
}

NS_IMETHODIMP
nsEventReceiverSH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                               JSContext *cx, JSObject *obj, jsval id,
                               jsval *vp, PRBool *_retval)
{
  if (::JS_TypeOfValue(cx, *vp) != JSTYPE_FUNCTION || !JSVAL_IS_STRING(id)) {
    return NS_OK;
  }

  PRBool did_compile; // Ignored here.

  return RegisterCompileHandler(wrapper, cx, obj, id, PR_FALSE, &did_compile);
}

NS_IMETHODIMP
nsEventReceiverSH::AddProperty(nsIXPConnectWrappedNative *wrapper,
                               JSContext *cx, JSObject *obj, jsval id,
                               jsval *vp, PRBool *_retval)
{
  return nsEventReceiverSH::SetProperty(wrapper, cx, obj, id, vp, _retval);
}

/*
NS_IMETHODIMP
nsEventReceiverSH::OnFinalize(...)
{
  clear event handlers in mListener...
}
*/


// Element helper

NS_IMETHODIMP
nsElementSH::PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj)
{
  nsresult rv = nsNodeSH::PostCreate(wrapper, cx, obj);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> native;

  wrapper->GetNative(getter_AddRefs(native));

  nsCOMPtr<nsIContent> content(do_QueryInterface(native));
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

  nsIFrame* frame = nsnull;
  shell->GetPrimaryFrameFor(content, &frame);

  if (frame) {
    // If we have a frame the frame has already loaded the binding.

    return NS_OK;
  }

  // We must ensure that the XBL Binding is installed before we hand
  // back this object.

  nsIBindingManager *bindingManager = doc->GetBindingManager();
  NS_ENSURE_TRUE(bindingManager, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIXBLBinding> binding;
  bindingManager->GetBinding(content, getter_AddRefs(binding));

  if (binding) {
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

  xblService->LoadBindings(content, bindingURL, PR_FALSE,
                           getter_AddRefs(binding), &dummy);

  if (binding) {
    binding->ExecuteAttachedHandler();
  }

  return NS_OK;
}

// Generic array scriptable helper.

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
  JSBool ok = ::JS_GetProperty(cx, obj, "length", &len_val);

  if (ok && JSVAL_IS_INT(len_val)) {
    PRInt32 length = JSVAL_TO_INT(len_val);
    char buf[11];

    for (PRInt32 i = 0; ok && i < length; ++i) {
      PR_snprintf(buf, sizeof(buf), "%d", i);

      ok = ::JS_DefineProperty(cx, obj, buf, JSVAL_VOID, nsnull, nsnull,
                               JSPROP_ENUMERATE);
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

  if (is_number) {
    if (n < 0) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    nsCOMPtr<nsISupports> native;
    wrapper->GetNative(getter_AddRefs(native));

    nsCOMPtr<nsISupports> array_item;

    nsresult rv = GetItemAt(native, n, getter_AddRefs(array_item));
    NS_ENSURE_SUCCESS(rv, rv);

    if (array_item) {
      rv = WrapNative(cx, ::JS_GetGlobalObject(cx), array_item,
                      NS_GET_IID(nsISupports), vp);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
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
  if (JSVAL_IS_STRING(id)) {
    nsCOMPtr<nsISupports> native;
    wrapper->GetNative(getter_AddRefs(native));

    nsCOMPtr<nsISupports> item;
    nsresult rv = GetNamedItem(native, nsDependentJSString(id),
                               getter_AddRefs(item));
    NS_ENSURE_SUCCESS(rv, rv);

    if (item) {
      rv = WrapNative(cx, ::JS_GetGlobalObject(cx), item,
                      NS_GET_IID(nsISupports), vp);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK; // Don't fall through to nsArraySH::GetProperty() here
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
  nsCOMPtr<nsIContentList> contentList(do_QueryInterface(nativeObj));
  NS_ASSERTION(contentList, "Why does something not implementing nsIContentList use nsContentListSH??");

  nsISupports *native_parent = contentList->GetParentObject();

  if (!native_parent) {
    *parentObj = globalObj;
    return NS_OK;
  }

  jsval v;
  nsresult rv = WrapNative(cx, ::JS_GetGlobalObject(cx), native_parent,
                           NS_GET_IID(nsISupports), &v);

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
  cached_doc_needs_check = PR_TRUE;
  
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

  if (wrapper_global != ::JS_GetGlobalObject(cx)) {
    // cx is not the context in the global object in wrapper, force
    // a security check.

    return PR_TRUE;
  }

  // Check if the calling function comes from the same scope that the
  // wrapper comes from. If that's the case, or if there's no JS
  // running at the moment (i.e. someone is using the JS engine API
  // directly to access a property on a JS object) there's no need to
  // do a security check.

  JSObject *function_obj = nsnull;
  JSStackFrame *fp = nsnull;

  do {
    fp = ::JS_FrameIterator(cx, &fp);

    if (!fp) {
      if (!function_obj) {
        // No JS is running (there's no frame on the JS stack with a
        // function object), someone is just accessing properties on a
        // JS object using the JS API, no need to do security checks
        // then.

        // Since the scope chain is immutable, it's OK to keep
        // skipping the check
        cached_doc_needs_check = PR_FALSE;
        return PR_FALSE;
      }

      break;
    }

    function_obj = ::JS_GetFrameFunctionObject(cx, fp);
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

    JSString *str = JSVAL_TO_STRING(id);
    nsCOMPtr<nsISupports> native;
    wrapper->GetNative(getter_AddRefs(native));
    NS_ENSURE_TRUE(native, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMNSDocument> doc(do_QueryInterface(native));
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMLocation> location;
    rv = doc->GetLocation(getter_AddRefs(location));
    NS_ENSURE_SUCCESS(rv, rv);

    jsval v;

    rv = WrapNative(cx, obj, location, NS_GET_IID(nsIDOMLocation), &v);
    NS_ENSURE_SUCCESS(rv, rv);

    sDoSecurityCheckInAddProperty = PR_FALSE;

    JSBool ok = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                      ::JS_GetStringLength(str), v, nsnull,
                                      nsnull, JSPROP_ENUMERATE);

    sDoSecurityCheckInAddProperty = PR_TRUE;

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

  return NS_OK;
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
    nsCOMPtr<nsISupports> native;
    wrapper->GetNative(getter_AddRefs(native));
    NS_ENSURE_TRUE(native, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMNSDocument> doc(do_QueryInterface(native));
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMLocation> location;

    nsresult rv = doc->GetLocation(getter_AddRefs(location));
    NS_ENSURE_SUCCESS(rv, rv);

    if (location) {
      JSString *val = ::JS_ValueToString(cx, *vp);
      NS_ENSURE_TRUE(val, NS_ERROR_UNEXPECTED);

      nsresult rv = location->SetHref(nsDependentJSString(val));
      NS_ENSURE_SUCCESS(rv, rv);

      return WrapNative(cx, obj, location, NS_GET_IID(nsIDOMLocation), vp);
    }
  }

  return nsNodeSH::SetProperty(wrapper, cx, obj, id, vp, _retval);
}

NS_IMETHODIMP
nsDocumentSH::GetFlags(PRUint32* aFlags)
{
  *aFlags = DOMCLASSINFO_STANDARD_FLAGS;

  return NS_OK;
}


// HTMLDocument helper

// static
nsresult
nsHTMLDocumentSH::ResolveImpl(JSContext *cx,
                              nsIXPConnectWrappedNative *wrapper, jsval id,
                              nsISupports **result)
{
  nsCOMPtr<nsISupports> native;

  wrapper->GetNative(getter_AddRefs(native));
  NS_ABORT_IF_FALSE(native, "No native!");

  nsCOMPtr<nsIHTMLDocument> doc(do_QueryInterface(native));
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

  nsCOMPtr<nsISupports> native;
  rv = wrapper->GetNative(getter_AddRefs(native));
  NS_ENSURE_SUCCESS(rv, JS_FALSE);

  nsCOMPtr<nsIDOMNSHTMLDocument> doc(do_QueryInterface(native));
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
    if (!type.EqualsLiteral("text/html")) {
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

  rv = WrapNative(cx, ::JS_GetGlobalObject(cx), retval,
                  NS_GET_IID(nsIDOMDocument), rval);
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
      nsCOMPtr<nsISupports> native;
      rv |= wrapper->GetNative(getter_AddRefs(native));

      if (native) {
        CallQueryInterface(native, nodeList);
      }
    }
  } else {
    // No node list for this document.all yet, create one...

    rv |= domdoc->GetElementsByTagName(NS_LITERAL_STRING("*"), nodeList);

    rv |= nsDOMClassInfo::WrapNative(cx, obj, *nodeList,
                                     NS_GET_IID(nsISupports), &collection);

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
    rv = nsDOMClassInfo::WrapNative(cx, obj, result, NS_GET_IID(nsISupports),
                                    vp);
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

  if (JS_GET_CLASS(cx, obj) == &sHTMLDocumentAllClass) {
    // If obj is our document.all object, we're called through
    // document.all.item(), or something similar. In such a case, self
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

    // Print a warning so developers can stop using document.all
    PrintWarningOnConsole(cx, "DocumentAllUsed");

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

      nsCOMPtr<nsISupports> native;
      rv = wrapper->GetNative(getter_AddRefs(native));
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
      CallQueryInterface(native, &doc);

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
      jsval v = JSVAL_VOID;
      ::JS_SetProperty(cx, helper, "all", &v);

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
      nsresult rv = nsDOMClassInfo::WrapNative(cx, obj, tags,
                                               NS_GET_IID(nsISupports), &v);
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

    if (id == sOpen_id) {
      JSString *str = JSVAL_TO_STRING(id);
      JSFunction *fnc =
        ::JS_DefineFunction(cx, obj, ::JS_GetStringBytes(str),
                            DocumentOpen, 0, JSPROP_ENUMERATE);

      *objp = obj;

      return fnc ? NS_OK : NS_ERROR_UNEXPECTED;
    }

    if (id == sAll_id && !sDisableDocumentAllSupport) {
      wrapper->GetNative(getter_AddRefs(result));
      nsCOMPtr<nsIHTMLDocument> doc(do_QueryInterface(result));

      if (doc->GetCompatibilityMode() == eCompatibility_NavQuirks) {
        JSObject *helper =
          GetDocumentAllHelper(cx, ::JS_GetPrototype(cx, obj));

        JSObject *proto = ::JS_GetPrototype(cx, helper ? helper : obj);

        // Check if the property all is defined on obj's (or helper's
        // if obj doesn't exist) prototype, it it is, don't expose our
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
          helper = JS_NewObject(cx, &sHTMLDocumentAllHelperClass,
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

  nsCOMPtr<nsISupports> result;

  rv = ResolveImpl(cx, wrapper, id, getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);

  if (result) {
    return WrapNative(cx, ::JS_GetGlobalObject(cx), result,
                      NS_GET_IID(nsISupports), vp);
  }

  return NS_OK;
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

  nsCOMPtr<nsISupports> native;
  rv = wrapper->GetNative(getter_AddRefs(native));
  NS_ENSURE_SUCCESS(rv, JS_FALSE);

  nsCOMPtr<nsIDOMNSHTMLElement> element(do_QueryInterface(native));
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
    JSFunction *cfnc =
      ::JS_DefineFunction(cx, obj, ::JS_GetStringBytes(str), ScrollIntoView,
                          0, 0);

    *objp = obj;

    return cfnc ? NS_OK : NS_ERROR_UNEXPECTED;
  }

  return nsElementSH::NewResolve(wrapper, cx, obj, id, flags, objp, _retval);
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
  if ((!(JSRESOLVE_ASSIGNING & flags)) && JSVAL_IS_STRING(id)) {
    nsCOMPtr<nsISupports> native;

    wrapper->GetNative(getter_AddRefs(native));
    NS_ABORT_IF_FALSE(native, "No native!");

    nsCOMPtr<nsIForm> form(do_QueryInterface(native));
    nsCOMPtr<nsISupports> result;

    JSString *str = JSVAL_TO_STRING(id);
    FindNamedItem(form, str, getter_AddRefs(result));

    if (result) {
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
  nsCOMPtr<nsISupports> native;

  wrapper->GetNative(getter_AddRefs(native));
  NS_ABORT_IF_FALSE(native, "No native!");

  nsCOMPtr<nsIForm> form(do_QueryInterface(native));

  if (JSVAL_IS_STRING(id)) {
    nsCOMPtr<nsISupports> result;

    JSString *str = JSVAL_TO_STRING(id);
    FindNamedItem(form, str, getter_AddRefs(result));

    if (result) {
      // Wrap result, result can be either an element or a list of
      // elements
      return WrapNative(cx, ::JS_GetGlobalObject(cx), result,
                        NS_GET_IID(nsISupports), vp);
    }

    return NS_OK; // Don't fall through
  }

  PRInt32 n = GetArrayIndexFromId(cx, id);

  if (n >= 0) {
    nsCOMPtr<nsIFormControl> control;
    form->GetElementAt(n, getter_AddRefs(control));

    if (control) {
      return WrapNative(cx, ::JS_GetGlobalObject(cx), control,
                        NS_GET_IID(nsISupports), vp);
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
      nsCOMPtr<nsISupports> native;
      wrapper->GetNative(getter_AddRefs(native));
      NS_ABORT_IF_FALSE(native, "No native!");

      nsCOMPtr<nsIForm> form(do_QueryInterface(native));

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
      nsCOMPtr<nsISupports> native;
      wrapper->GetNative(getter_AddRefs(native));
      NS_ABORT_IF_FALSE(native, "No native!");

      nsCOMPtr<nsIForm> form(do_QueryInterface(native));
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

  if (n >= 0) {
    nsCOMPtr<nsISupports> native;

    wrapper->GetNative(getter_AddRefs(native));
    NS_ABORT_IF_FALSE(native, "No native!");

    nsCOMPtr<nsIDOMHTMLSelectElement> s(do_QueryInterface(native));

    nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
    s->GetOptions(getter_AddRefs(options));

    if (options) {
      nsCOMPtr<nsIDOMNode> node;

      options->Item(n, getter_AddRefs(node));

      return WrapNative(cx, ::JS_GetGlobalObject(cx), node,
                        NS_GET_IID(nsIDOMNode), vp);
    }
  }

  return NS_OK;
}

// static
nsresult
nsHTMLSelectElementSH::SetOption(JSContext *cx, jsval *vp, PRUint32 aIndex,
                                 nsIDOMNSHTMLOptionCollection *aOptCollection)
{
  // vp must refer to an object
  if (!JSVAL_IS_OBJECT(*vp) && !::JS_ConvertValue(cx, *vp, JSTYPE_OBJECT,
                                                  vp)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIDOMHTMLOptionElement> new_option;

  if (!JSVAL_IS_NULL(*vp)) {
    nsCOMPtr<nsIXPConnectWrappedNative> new_wrapper;
    nsresult rv;

    rv = sXPConnect->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(*vp),
                                                getter_AddRefs(new_wrapper));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupports> native;
    new_wrapper->GetNative(getter_AddRefs(native));

    new_option = do_QueryInterface(native);

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
    nsCOMPtr<nsISupports> native;
    wrapper->GetNative(getter_AddRefs(native));

    nsCOMPtr<nsIDOMHTMLSelectElement> select(do_QueryInterface(native));
    NS_ENSURE_TRUE(select, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
    select->GetOptions(getter_AddRefs(options));

    nsCOMPtr<nsIDOMNSHTMLOptionCollection> oc(do_QueryInterface(options));
    NS_ENSURE_TRUE(oc, NS_ERROR_UNEXPECTED);

    return SetOption(cx, vp, n, oc);
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

  nsCOMPtr<nsISupports> native;

  wrapper->GetNative(getter_AddRefs(native));

  nsCOMPtr<nsIContent> content(do_QueryInterface(native));
  NS_ENSURE_TRUE(content, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDocument> doc = content->GetDocument();

  if (!doc) {
    // No document, no plugin.

    return NS_OK;
  }

  // Have to flush layout, since plugin instance instantiation
  // currently happens in reflow!  That's just uncool.
  doc->FlushPendingNotifications(Flush_Layout);

  // See if we have a frame.
  nsIPresShell *shell = doc->GetShellAt(0);

  if (!shell) {
    return NS_OK;
  }

  nsIFrame* frame = nsnull;
  shell->GetPrimaryFrameFor(content, &frame);

  if (!frame) {
    // No frame, no plugin

    return NS_OK;
  }

  nsIObjectFrame* objectFrame = nsnull;
  CallQueryInterface(frame, &objectFrame);
  if (!objectFrame) {
    return NS_OK;
  }

  return objectFrame->GetPluginInstance(*_result);
}

// Check if proto is already in obj's prototype chain.

static PRBool
IsObjInProtoChain(JSContext *cx, JSObject *obj, JSObject *proto)
{
  JSObject *o = obj;

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
    // (through the FlushPendingNotifications() call in
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
nsHTMLExternalObjSH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                                 JSContext *cx, JSObject *obj, jsval id,
                                 jsval *vp, PRBool *_retval)
{
  JSString *id_str = ::JS_ValueToString(cx, id);
  if (!id_str) {
    *_retval = JS_FALSE;
    return NS_ERROR_FAILURE;
  }

  JSObject *pi_obj = ::JS_GetPrototype(cx, obj);
  const jschar *id_chars = ::JS_GetStringChars(id_str);
  size_t id_length = ::JS_GetStringLength(id_str);

  JSBool found;
  if (!::JS_HasUCProperty(cx, pi_obj, id_chars, id_length, &found)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (found) {
    *_retval = ::JS_SetUCProperty(cx, pi_obj, id_chars, id_length, vp);
    return *_retval ? NS_OK : NS_ERROR_FAILURE;
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

  nsCOMPtr<nsINPRuntimePlugin> npruntime_plugin =
    do_QueryInterface(plugin_inst);

  if (npruntime_plugin) {
    *plugin_obj = npruntime_plugin->GetJSObject(cx);

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
  NS_ENSURE_SUCCESS(rv, rv);

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

    nsCOMPtr<nsIInterfaceInfoManager> iim =
      dont_AddRef(XPTI_GetInterfaceInfoManager());
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
        rv = sXPConnect->WrapNative(cx, ::JS_GetGlobalObject(cx), pi, *iid,
                                    getter_AddRefs(holder));

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

  nsCOMPtr<nsISupports> native;
  wrapper->GetNative(getter_AddRefs(native));

  nsCOMPtr<nsIDOMNSHTMLOptionCollection> oc(do_QueryInterface(native));
  NS_ENSURE_TRUE(oc, NS_ERROR_UNEXPECTED);

  return nsHTMLSelectElementSH::SetOption(cx, vp, n, oc);
}

NS_IMETHODIMP
nsHTMLOptionsCollectionSH::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                      JSContext *cx, JSObject *obj, 
                                      jsval id, PRUint32 flags, 
                                      JSObject **objp, PRBool *_retval)
{
  if (id == sAdd_id) {
    JSString *str = JSVAL_TO_STRING(id);
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

  nsCOMPtr<nsISupports> native;
  rv = wrapper->GetNative(getter_AddRefs(native));
  NS_ENSURE_SUCCESS(rv, JS_FALSE);

  nsCOMPtr<nsIDOMHTMLOptionsCollection> options(do_QueryInterface(native));
  NS_ASSERTION(options, "native should have been an options collection");

  if (argc < 1 || !JSVAL_IS_OBJECT(argv[0])) {
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
    return JS_FALSE;
  }

  rv = sXPConnect->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(argv[0]),
                                              getter_AddRefs(wrapper));
  
  if (NS_FAILED(rv)) {
    nsDOMClassInfo::ThrowJSException(cx, rv);
    return JS_FALSE;
  }
  
  rv = wrapper->GetNative(getter_AddRefs(native));
  NS_ENSURE_SUCCESS(rv, JS_FALSE);
    
  nsCOMPtr<nsIDOMHTMLOptionElement> newOption(do_QueryInterface(native));
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

  nsCOMPtr<nsISupports> native;
  wrapper->GetNative(getter_AddRefs(native));

  nsAutoString val;

  nsresult rv = GetStringAt(native, n, val);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX: Null strings?

  JSString *str =
    ::JS_NewUCStringCopyN(cx, NS_REINTERPRET_CAST(const jschar *, val.get()),
                          val.Length());
  NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

  *vp = STRING_TO_JSVAL(str);

  return NS_OK;
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
