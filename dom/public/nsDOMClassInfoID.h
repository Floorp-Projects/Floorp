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

/**
 * This file defines enum values for all of the DOM objects which have
 * an entry in nsDOMClassInfo.
 */

#ifndef nsDOMClassInfoID_h__
#define nsDOMClassInfoID_h__

enum nsDOMClassInfoID {
  // Base classes
  eDOMClassInfo_Window_id,
  eDOMClassInfo_Location_id,
  eDOMClassInfo_Navigator_id,
  eDOMClassInfo_Plugin_id,
  eDOMClassInfo_PluginArray_id,
  eDOMClassInfo_MimeType_id,
  eDOMClassInfo_MimeTypeArray_id,
  eDOMClassInfo_BarProp_id,
  eDOMClassInfo_History_id,
  eDOMClassInfo_Screen_id,
  eDOMClassInfo_Constructor_id,

  // Core classes
  eDOMClassInfo_XMLDocument_id,
  eDOMClassInfo_DocumentType_id,
  eDOMClassInfo_DOMImplementation_id,
  eDOMClassInfo_DOMException_id,
  eDOMClassInfo_DocumentFragment_id,
  eDOMClassInfo_Element_id,
  eDOMClassInfo_Attr_id,
  eDOMClassInfo_Text_id,
  eDOMClassInfo_Comment_id,
  eDOMClassInfo_CDATASection_id,
  eDOMClassInfo_ProcessingInstruction_id,
  eDOMClassInfo_Notation_id,
  eDOMClassInfo_NodeList_id,
  eDOMClassInfo_NamedNodeMap_id,

  // StyleSheet classes
  eDOMClassInfo_DocumentStyleSheetList_id,

  // Event classes
  eDOMClassInfo_Event_id,
  eDOMClassInfo_MutationEvent_id,
  eDOMClassInfo_UIEvent_id,
  eDOMClassInfo_MouseEvent_id,
  eDOMClassInfo_KeyboardEvent_id,
  eDOMClassInfo_PopupBlockedEvent_id,

  // HTML classes
  eDOMClassInfo_HTMLDocument_id,
  eDOMClassInfo_HTMLOptionsCollection_id,
  eDOMClassInfo_HTMLFormControlCollection_id,
  eDOMClassInfo_HTMLGenericCollection_id,

  // HTML element classes
  eDOMClassInfo_HTMLAnchorElement_id,
  eDOMClassInfo_HTMLAppletElement_id,
  eDOMClassInfo_HTMLAreaElement_id,
  eDOMClassInfo_HTMLBRElement_id,
  eDOMClassInfo_HTMLBaseElement_id,
  eDOMClassInfo_HTMLBaseFontElement_id,
  eDOMClassInfo_HTMLBodyElement_id,
  eDOMClassInfo_HTMLButtonElement_id,
  eDOMClassInfo_HTMLDListElement_id,
  eDOMClassInfo_HTMLDelElement_id,
  eDOMClassInfo_HTMLDirectoryElement_id,
  eDOMClassInfo_HTMLDivElement_id,
  eDOMClassInfo_HTMLEmbedElement_id,
  eDOMClassInfo_HTMLFieldSetElement_id,
  eDOMClassInfo_HTMLFontElement_id,
  eDOMClassInfo_HTMLFormElement_id,
  eDOMClassInfo_HTMLFrameElement_id,
  eDOMClassInfo_HTMLFrameSetElement_id,
  eDOMClassInfo_HTMLHRElement_id,
  eDOMClassInfo_HTMLHeadElement_id,
  eDOMClassInfo_HTMLHeadingElement_id,
  eDOMClassInfo_HTMLHtmlElement_id,
  eDOMClassInfo_HTMLIFrameElement_id,
  eDOMClassInfo_HTMLImageElement_id,
  eDOMClassInfo_HTMLInputElement_id,
  eDOMClassInfo_HTMLInsElement_id,
  eDOMClassInfo_HTMLIsIndexElement_id,
  eDOMClassInfo_HTMLLIElement_id,
  eDOMClassInfo_HTMLLabelElement_id,
  eDOMClassInfo_HTMLLegendElement_id,
  eDOMClassInfo_HTMLLinkElement_id,
  eDOMClassInfo_HTMLMapElement_id,
  eDOMClassInfo_HTMLMenuElement_id,
  eDOMClassInfo_HTMLMetaElement_id,
  eDOMClassInfo_HTMLOListElement_id,
  eDOMClassInfo_HTMLObjectElement_id,
  eDOMClassInfo_HTMLOptGroupElement_id,
  eDOMClassInfo_HTMLOptionElement_id,
  eDOMClassInfo_HTMLParagraphElement_id,
  eDOMClassInfo_HTMLParamElement_id,
  eDOMClassInfo_HTMLPreElement_id,
  eDOMClassInfo_HTMLQuoteElement_id,
  eDOMClassInfo_HTMLScriptElement_id,
  eDOMClassInfo_HTMLSelectElement_id,
  eDOMClassInfo_HTMLSpacerElement_id,
  eDOMClassInfo_HTMLSpanElement_id,
  eDOMClassInfo_HTMLStyleElement_id,
  eDOMClassInfo_HTMLTableCaptionElement_id,
  eDOMClassInfo_HTMLTableCellElement_id,
  eDOMClassInfo_HTMLTableColElement_id,
  eDOMClassInfo_HTMLTableElement_id,
  eDOMClassInfo_HTMLTableRowElement_id,
  eDOMClassInfo_HTMLTableSectionElement_id,
  eDOMClassInfo_HTMLTextAreaElement_id,
  eDOMClassInfo_HTMLTitleElement_id,
  eDOMClassInfo_HTMLUListElement_id,
  eDOMClassInfo_HTMLUnknownElement_id,
  eDOMClassInfo_HTMLWBRElement_id,

  // CSS classes
  eDOMClassInfo_CSSStyleRule_id,
  eDOMClassInfo_CSSCharsetRule_id,
  eDOMClassInfo_CSSImportRule_id,
  eDOMClassInfo_CSSMediaRule_id,
  eDOMClassInfo_CSSNameSpaceRule_id,
  eDOMClassInfo_CSSRuleList_id,
  eDOMClassInfo_CSSGroupRuleRuleList_id,
  eDOMClassInfo_MediaList_id,
  eDOMClassInfo_StyleSheetList_id,
  eDOMClassInfo_CSSStyleSheet_id,
  eDOMClassInfo_CSSStyleDeclaration_id,
  eDOMClassInfo_ComputedCSSStyleDeclaration_id,
  eDOMClassInfo_ROCSSPrimitiveValue_id,

  // Range classes
  eDOMClassInfo_Range_id,
  eDOMClassInfo_Selection_id,

  // XUL classes
#ifdef MOZ_XUL
  eDOMClassInfo_XULDocument_id,
  eDOMClassInfo_XULElement_id,
  eDOMClassInfo_XULCommandDispatcher_id,
#endif
  eDOMClassInfo_XULControllers_id,
#ifdef MOZ_XUL
  eDOMClassInfo_BoxObject_id,
  eDOMClassInfo_TreeSelection_id,
  eDOMClassInfo_TreeContentView_id,
#endif

  // Crypto classes
  eDOMClassInfo_Crypto_id,
  eDOMClassInfo_CRMFObject_id,
  eDOMClassInfo_Pkcs11_id,
  
  // DOM Traversal classes
  eDOMClassInfo_TreeWalker_id,

  // Rect object used by getComputedStyle
  eDOMClassInfo_CSSRect_id,

  // DOM Chrome Window class, almost identical to Window
  eDOMClassInfo_ChromeWindow_id,

  // RGBColor object used by getComputedStyle
  eDOMClassInfo_CSSRGBColor_id,

  eDOMClassInfo_RangeException_id,

  // CSSValueList object that represents an nsIDOMCSSValueList, used
  // by DOM CSS
  eDOMClassInfo_CSSValueList_id,

  // ContentList object used for various live NodeLists
  eDOMClassInfo_ContentList_id,
  
  // Processing-instruction with target "xml-stylesheet"
  eDOMClassInfo_XMLStylesheetProcessingInstruction_id,
  
  eDOMClassInfo_ImageDocument_id,

#ifdef MOZ_XUL
  eDOMClassInfo_XULTemplateBuilder_id,
  eDOMClassInfo_XULTreeBuilder_id,
#endif

  // DOMStringList object
  eDOMClassInfo_DOMStringList_id,

  // NameList object used by the DOM
  eDOMClassInfo_NameList_id,

#ifdef MOZ_XUL
  eDOMClassInfo_TreeColumn_id,
  eDOMClassInfo_TreeColumns_id,
#endif

  eDOMClassInfo_CSSMozDocumentRule_id,

  eDOMClassInfo_BeforeUnloadEvent_id,

#ifdef MOZ_SVG
  // The SVG document
  eDOMClassInfo_SVGDocument_id,

  // SVG element classes
  eDOMClassInfo_SVGAElement_id,
  eDOMClassInfo_SVGCircleElement_id,
  eDOMClassInfo_SVGClipPathElement_id,
  eDOMClassInfo_SVGDefsElement_id,
  eDOMClassInfo_SVGDescElement_id,
  eDOMClassInfo_SVGEllipseElement_id,
  eDOMClassInfo_SVGFEBlendElement_id,
  eDOMClassInfo_SVGFEColorMatrixElement_id,
  eDOMClassInfo_SVGFEComponentTransferElement_id,
  eDOMClassInfo_SVGFECompositeElement_id,
  eDOMClassInfo_SVGFEFloodElement_id,
  eDOMClassInfo_SVGFEFuncAElement_id,
  eDOMClassInfo_SVGFEFuncBElement_id,
  eDOMClassInfo_SVGFEFuncGElement_id,
  eDOMClassInfo_SVGFEFuncRElement_id,
  eDOMClassInfo_SVGFEGaussianBlurElement_id,
  eDOMClassInfo_SVGFEMergeElement_id,
  eDOMClassInfo_SVGFEMergeNodeElement_id,
  eDOMClassInfo_SVGFEMorphologyElement_id,
  eDOMClassInfo_SVGFEOffsetElement_id,
  eDOMClassInfo_SVGFETurbulenceElement_id,
  eDOMClassInfo_SVGFEUnimplementedMOZElement_id,
  eDOMClassInfo_SVGFilterElement_id,
  eDOMClassInfo_SVGGElement_id,
  eDOMClassInfo_SVGImageElement_id,
  eDOMClassInfo_SVGLinearGradientElement_id,
  eDOMClassInfo_SVGLineElement_id,
  eDOMClassInfo_SVGMarkerElement_id,
  eDOMClassInfo_SVGMaskElement_id,
  eDOMClassInfo_SVGMetadataElement_id,
  eDOMClassInfo_SVGPathElement_id,
  eDOMClassInfo_SVGPatternElement_id,
  eDOMClassInfo_SVGPolygonElement_id,
  eDOMClassInfo_SVGPolylineElement_id,
  eDOMClassInfo_SVGRadialGradientElement_id,
  eDOMClassInfo_SVGRectElement_id,
  eDOMClassInfo_SVGScriptElement_id,
  eDOMClassInfo_SVGStopElement_id,
  eDOMClassInfo_SVGStyleElement_id,
  eDOMClassInfo_SVGSVGElement_id,
  eDOMClassInfo_SVGSwitchElement_id,
  eDOMClassInfo_SVGSymbolElement_id,
  eDOMClassInfo_SVGTextElement_id,
  eDOMClassInfo_SVGTextPathElement_id,
  eDOMClassInfo_SVGTitleElement_id,
  eDOMClassInfo_SVGTSpanElement_id,
  eDOMClassInfo_SVGUseElement_id,

  // other SVG classes
  eDOMClassInfo_SVGAngle_id,
  eDOMClassInfo_SVGAnimatedAngle_id,
  eDOMClassInfo_SVGAnimatedEnumeration_id,
  eDOMClassInfo_SVGAnimatedInteger_id,
  eDOMClassInfo_SVGAnimatedLength_id,
  eDOMClassInfo_SVGAnimatedLengthList_id,
  eDOMClassInfo_SVGAnimatedNumber_id,
  eDOMClassInfo_SVGAnimatedNumberList_id,
  eDOMClassInfo_SVGAnimatedPreserveAspectRatio_id,
  eDOMClassInfo_SVGAnimatedRect_id,
  eDOMClassInfo_SVGAnimatedString_id,
  eDOMClassInfo_SVGAnimatedTransformList_id,
  eDOMClassInfo_SVGEvent_id,
  eDOMClassInfo_SVGException_id,
  eDOMClassInfo_SVGLength_id,
  eDOMClassInfo_SVGLengthList_id,
  eDOMClassInfo_SVGMatrix_id,
  eDOMClassInfo_SVGNumber_id,
  eDOMClassInfo_SVGNumberList_id,
  eDOMClassInfo_SVGPathSegArcAbs_id,
  eDOMClassInfo_SVGPathSegArcRel_id,
  eDOMClassInfo_SVGPathSegClosePath_id,
  eDOMClassInfo_SVGPathSegCurvetoCubicAbs_id,
  eDOMClassInfo_SVGPathSegCurvetoCubicRel_id,
  eDOMClassInfo_SVGPathSegCurvetoCubicSmoothAbs_id,
  eDOMClassInfo_SVGPathSegCurvetoCubicSmoothRel_id,
  eDOMClassInfo_SVGPathSegCurvetoQuadraticAbs_id,
  eDOMClassInfo_SVGPathSegCurvetoQuadraticRel_id,
  eDOMClassInfo_SVGPathSegCurvetoQuadraticSmoothAbs_id,
  eDOMClassInfo_SVGPathSegCurvetoQuadraticSmoothRel_id,
  eDOMClassInfo_SVGPathSegLinetoAbs_id,
  eDOMClassInfo_SVGPathSegLinetoHorizontalAbs_id,
  eDOMClassInfo_SVGPathSegLinetoHorizontalRel_id,
  eDOMClassInfo_SVGPathSegLinetoRel_id,
  eDOMClassInfo_SVGPathSegLinetoVerticalAbs_id,
  eDOMClassInfo_SVGPathSegLinetoVerticalRel_id,
  eDOMClassInfo_SVGPathSegList_id,
  eDOMClassInfo_SVGPathSegMovetoAbs_id,
  eDOMClassInfo_SVGPathSegMovetoRel_id,
  eDOMClassInfo_SVGPoint_id,
  eDOMClassInfo_SVGPointList_id,
  eDOMClassInfo_SVGPreserveAspectRatio_id,
  eDOMClassInfo_SVGRect_id,
  eDOMClassInfo_SVGTransform_id,
  eDOMClassInfo_SVGTransformList_id,
  eDOMClassInfo_SVGZoomEvent_id,
#endif // MOZ_SVG

  // Canvas
  eDOMClassInfo_HTMLCanvasElement_id,
#ifdef MOZ_ENABLE_CANVAS
  eDOMClassInfo_CanvasRenderingContext2D_id,
  eDOMClassInfo_CanvasGradient_id,
  eDOMClassInfo_CanvasPattern_id,
#endif
  
  // SmartCard Events
  eDOMClassInfo_SmartCardEvent_id,
  
  // PageTransition Events
  eDOMClassInfo_PageTransitionEvent_id,

  // WindowUtils
  eDOMClassInfo_WindowUtils_id,

  // XSLTProcessor
  eDOMClassInfo_XSLTProcessor_id,

  // DOM Level 3 XPath objects
  eDOMClassInfo_XPathEvaluator_id,
  eDOMClassInfo_XPathException_id,
  eDOMClassInfo_XPathExpression_id,
  eDOMClassInfo_XPathNSResolver_id,
  eDOMClassInfo_XPathResult_id,

  // WhatWG WebApps Objects
  eDOMClassInfo_Storage_id,
  eDOMClassInfo_StorageList_id,
  eDOMClassInfo_StorageItem_id,
  eDOMClassInfo_StorageEvent_id,

  eDOMClassInfo_WindowRoot_id,

  // DOMParser, XMLSerializer
  eDOMClassInfo_DOMParser_id,
  eDOMClassInfo_XMLSerializer_id,

  // XMLHttpRequest
  eDOMClassInfo_XMLHttpProgressEvent_id,
  eDOMClassInfo_XMLHttpRequest_id,

  // We are now trying to preserve binary compat in classinfo.  No more
  // putting things in those categories up there.  New entries are to be
  // added here, which is the end of the things that are currently on by
  // default.

#if defined(MOZ_SVG) && defined(MOZ_SVG_FOREIGNOBJECT)
  eDOMClassInfo_SVGForeignObjectElement_id,
#endif

  eDOMClassInfo_XULCommandEvent_id,
  eDOMClassInfo_CommandEvent_id,

  // This one better be the last one in this list
  eDOMClassInfoIDCount
};

#endif // nsDOMClassInfoID_h__
