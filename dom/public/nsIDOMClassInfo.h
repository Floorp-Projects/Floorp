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

#ifndef nsIDOMClassInfo_h___
#define nsIDOMClassInfo_h___

#include "nsIClassInfo.h"
#include "nsVoidArray.h"

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
  eDOMClassInfo_Entity_id,
  eDOMClassInfo_EntityReference_id,
  eDOMClassInfo_Notation_id,
  eDOMClassInfo_NodeList_id,
  eDOMClassInfo_NamedNodeMap_id,

  // StyleSheet classes
  eDOMClassInfo_DocumentStyleSheetList_id,

  // Event classes
  eDOMClassInfo_Event_id,
  eDOMClassInfo_MutationEvent_id,

  // HTML classes
  eDOMClassInfo_HTMLDocument_id,
  eDOMClassInfo_HTMLCollection_id,
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
  eDOMClassInfo_HTMLModElement_id,
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
  eDOMClassInfo_HTMLTableColGroupElement_id,
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
  eDOMClassInfo_XULNodeList_id,
  eDOMClassInfo_XULNamedNodeMap_id,
  eDOMClassInfo_XULAttr_id,
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

  // We are now trying to preserve binary compat in classinfo.  No
  // more putting things in those categories up there.  New entries
  // are to be added right before eDOMClassInfoIDCount

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

#ifdef MOZ_SVG
  // The SVG document
  eDOMClassInfo_SVGDocument_id,

  // SVG element classes
  eDOMClassInfo_SVGSVGElement_id,
  eDOMClassInfo_SVGPolygonElement_id,
  eDOMClassInfo_SVGPolylineElement_id,
  eDOMClassInfo_SVGCircleElement_id,
  eDOMClassInfo_SVGEllipseElement_id,
  eDOMClassInfo_SVGLineElement_id,
  eDOMClassInfo_SVGRectElement_id,
  eDOMClassInfo_SVGGElement_id,
  eDOMClassInfo_SVGForeignObjectElement_id,
  eDOMClassInfo_SVGPathElement_id,
  eDOMClassInfo_SVGDefsElement_id,
  
  // other SVG classes
  eDOMClassInfo_SVGAnimatedLength_id,
  eDOMClassInfo_SVGLength_id,
  eDOMClassInfo_SVGAnimatedPoints_id,
  eDOMClassInfo_SVGPointList_id,
  eDOMClassInfo_SVGPoint_id,
  eDOMClassInfo_SVGAnimatedTransformList_id,
  eDOMClassInfo_SVGTransformList_id,
  eDOMClassInfo_SVGTransform_id,
  eDOMClassInfo_SVGMatrix_id,
  eDOMClassInfo_SVGPathSegList_id,
  eDOMClassInfo_SVGPathSegClosePath_id,
  eDOMClassInfo_SVGPathSegMovetoAbs_id,
  eDOMClassInfo_SVGPathSegMovetoRel_id,
  eDOMClassInfo_SVGPathSegLinetoAbs_id,
  eDOMClassInfo_SVGPathSegLinetoRel_id,
  eDOMClassInfo_SVGPathSegCurvetoCubicAbs_id,
  eDOMClassInfo_SVGPathSegCurvetoCubicRel_id,
  eDOMClassInfo_SVGPathSegCurvetoQuadraticAbs_id,
  eDOMClassInfo_SVGPathSegCurvetoQuadraticRel_id,
  eDOMClassInfo_SVGPathSegArcAbs_id,
  eDOMClassInfo_SVGPathSegArcRel_id,
  eDOMClassInfo_SVGPathSegLinetoHorizontalAbs_id,
  eDOMClassInfo_SVGPathSegLinetoHorizontalRel_id,
  eDOMClassInfo_SVGPathSegLinetoVerticalAbs_id,
  eDOMClassInfo_SVGPathSegLinetoVerticalRel_id,
  eDOMClassInfo_SVGPathSegCurvetoCubicSmoothAbs_id,
  eDOMClassInfo_SVGPathSegCurvetoCubicSmoothRel_id,
  eDOMClassInfo_SVGPathSegCurvetoQuadraticSmoothAbs_id,
  eDOMClassInfo_SVGPathSegCurvetoQuadraticSmoothRel_id,
  eDOMClassInfo_SVGRect_id,
  eDOMClassInfo_SVGAnimatedRect_id,
  eDOMClassInfo_SVGAnimatedLengthList_id,
  eDOMClassInfo_SVGLengthList_id,
  eDOMClassInfo_SVGNumber_id,
  eDOMClassInfo_SVGTextElement_id,
  eDOMClassInfo_SVGTSpanElement_id,
  eDOMClassInfo_SVGAnimatedString_id,
  eDOMClassInfo_SVGImageElement_id,
  eDOMClassInfo_SVGStyleElement_id,
  eDOMClassInfo_SVGAnimatedPreserveAspectRatio_id,
  eDOMClassInfo_SVGPreserveAspectRatio_id,
  eDOMClassInfo_SVGScriptElement_id,
#endif //MOZ_SVG
  
  // This one better be the last one in this list
  eDOMClassInfoIDCount
};

#include "nsIXPCScriptable.h"

#define DEFAULT_SCRIPTABLE_FLAGS                                           \
  (nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY |                          \
   nsIXPCScriptable::USE_JSSTUB_FOR_DELPROPERTY |                          \
   nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY |                          \
   nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE |                      \
   nsIXPCScriptable::ALLOW_PROP_MODS_TO_PROTOTYPE |                        \
   nsIXPCScriptable::DONT_ASK_INSTANCE_FOR_SCRIPTABLE |                    \
   nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES |                        \
   nsIXPCScriptable::WANT_NEWRESOLVE |                                     \
   nsIXPCScriptable::WANT_CHECKACCESS |                                    \
   nsIXPCScriptable::WANT_POSTCREATE)

#define DOM_DEFAULT_SCRIPTABLE_FLAGS                                       \
  (DEFAULT_SCRIPTABLE_FLAGS |                                              \
   nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE |                           \
   nsIXPCScriptable::CLASSINFO_INTERFACES_ONLY)


typedef nsIClassInfo* (*nsDOMClassInfoExternalConstructorFnc)
  (const char* aName);


/**
 * nsIClassInfo helper macros
 */

#define NS_CLASSINFO_MAP_BEGIN(_class)

#define NS_CLASSINFO_MAP_BEGIN_EXPORTED(_class)

#define NS_CLASSINFO_MAP_ENTRY(_interface)

#define NS_CLASSINFO_MAP_ENTRY_FUNCTION(_function)

#define NS_CLASSINFO_MAP_END


#include "nsIServiceManager.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsDOMCID.h"

#define NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(_class)                       \
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {                             \
    static NS_DEFINE_CID(kDOMSOF_CID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);   \
                                                                           \
    nsresult rv;                                                           \
    nsCOMPtr<nsIDOMScriptObjectFactory> sof(do_GetService(kDOMSOF_CID,     \
                                                          &rv));           \
    if (NS_FAILED(rv)) {                                                   \
      *aInstancePtr = nsnull;                                              \
      return rv;                                                           \
    }                                                                      \
                                                                           \
    foundInterface =                                                       \
      sof->GetClassInfoInstance(eDOMClassInfo_##_class##_id);              \
  } else

// Looks up the nsIClassInfo for a class name registered with the 
// nsScriptNamespaceManager. Remember to release NS_CLASSINFO_NAME(_class)
// (eg. when your module unloads).
#define NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(_class)              \
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {                             \
    extern nsISupports *NS_CLASSINFO_NAME(_class);                         \
    if (NS_CLASSINFO_NAME(_class)) {                                       \
      foundInterface = NS_CLASSINFO_NAME(_class);                          \
    } else {                                                               \
      static NS_DEFINE_CID(kDOMSOF_CID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID); \
                                                                           \
      nsresult rv;                                                         \
      nsCOMPtr<nsIDOMScriptObjectFactory> sof(do_GetService(kDOMSOF_CID,   \
                                                            &rv));         \
      if (NS_FAILED(rv)) {                                                 \
        *aInstancePtr = nsnull;                                            \
        return rv;                                                         \
      }                                                                    \
                                                                           \
      foundInterface =                                                     \
        sof->GetExternalClassInfoInstance(NS_LITERAL_STRING(#_class));     \
                                                                           \
      if (foundInterface) {                                                \
        NS_CLASSINFO_NAME(_class) = foundInterface;                        \
        NS_CLASSINFO_NAME(_class)->AddRef();                               \
      }                                                                    \
    }                                                                      \
  } else


#define NS_DECL_DOM_CLASSINFO(_class) \
  nsISupports *NS_CLASSINFO_NAME(_class) = nsnull;

// {891a7b01-1b61-11d6-a7f2-f690b638899c}
#define NS_IDOMCI_EXTENSION_IID  \
{ 0x891a7b01, 0x1b61, 0x11d6, \
{ 0xa7, 0xf2, 0xf6, 0x90, 0xb6, 0x38, 0x89, 0x9c } }

class nsIDOMScriptObjectFactory;

class nsIDOMCIExtension : public nsISupports {
public:  
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMCI_EXTENSION_IID)

  NS_IMETHOD RegisterDOMCI(const char* aName,
                           nsIDOMScriptObjectFactory* aDOMSOFactory) = 0;
};


#define NS_DOMCI_EXTENSION_NAME(_module) ns##_module##DOMCIExtension
#define NS_DOMCI_EXTENSION_CONSTRUCTOR(_module) \
  ns##_module##DOMCIExtensionConstructor
#define NS_DOMCI_EXTENSION_CONSTRUCTOR_IMP(_extension) \
  NS_GENERIC_FACTORY_CONSTRUCTOR(_extension)

#define NS_DOMCI_EXTENSION(_module)                                       \
class NS_DOMCI_EXTENSION_NAME(_module) : public nsIDOMCIExtension         \
{                                                                         \
public:                                                                   \
  NS_DOMCI_EXTENSION_NAME(_module)();                                     \
  virtual ~NS_DOMCI_EXTENSION_NAME(_module)();                            \
                                                                          \
  NS_DECL_ISUPPORTS                                                       \
                                                                          \
  NS_IMETHOD RegisterDOMCI(const char* aName,                             \
                           nsIDOMScriptObjectFactory* aDOMSOFactory);     \
};                                                                        \
                                                                          \
NS_DOMCI_EXTENSION_CONSTRUCTOR_IMP(NS_DOMCI_EXTENSION_NAME(_module))      \
                                                                          \
NS_DOMCI_EXTENSION_NAME(_module)::NS_DOMCI_EXTENSION_NAME(_module)()      \
{                                                                         \
}                                                                         \
                                                                          \
NS_DOMCI_EXTENSION_NAME(_module)::~NS_DOMCI_EXTENSION_NAME(_module)()     \
{                                                                         \
}                                                                         \
                                                                          \
NS_IMPL_ISUPPORTS1(NS_DOMCI_EXTENSION_NAME(_module), nsIDOMCIExtension)   \
                                                                          \
NS_IMETHODIMP                                                             \
NS_DOMCI_EXTENSION_NAME(_module)::RegisterDOMCI(const char* aName,        \
                                                nsIDOMScriptObjectFactory* aDOMSOFactory) \
{

#define NS_DOMCI_EXTENSION_ENTRY_BEGIN(_class)                            \
  if (nsCRT::strcmp(aName, #_class) == 0) {                               \
    static const nsIID* interfaces[] = {

#define NS_DOMCI_EXTENSION_ENTRY_INTERFACE(_interface)                    \
      &NS_GET_IID(_interface),

// Don't forget to register the primary interface (_proto) in the 
// JAVASCRIPT_DOM_INTERFACE category, or prototypes for this class
// won't work (except if the interface name starts with nsIDOM).
#define NS_DOMCI_EXTENSION_ENTRY_END_HELPER(_class, _proto, _hasclassif,  \
                                            _constructorcid)              \
      nsnull                                                              \
    };                                                                    \
    aDOMSOFactory->RegisterDOMClassInfo(#_class, nsnull, _proto,          \
                                        interfaces,                       \
                                        DOM_DEFAULT_SCRIPTABLE_FLAGS,     \
                                        _hasclassif, _constructorcid);    \
    return NS_OK;                                                         \
  }

#define NS_DOMCI_EXTENSION_ENTRY_END(_class, _proto, _hasclassif,         \
                                     _constructorcid)                     \
  NS_DOMCI_EXTENSION_ENTRY_END_HELPER(_class, &NS_GET_IID(_proto),        \
                                      _hasclassif, _constructorcid)

#define NS_DOMCI_EXTENSION_ENTRY_END_NO_PRIMARY_IF(_class, _hasclassif,   \
                                                   _constructorcid)       \
  NS_DOMCI_EXTENSION_ENTRY_END_HELPER(_class, nsnull, _hasclassif,        \
                                      _constructorcid)

#define NS_DOMCI_EXTENSION_END                                            \
  return NS_ERROR_FAILURE;                                                \
}


#endif /* nsIDOMClassInfo_h___ */
