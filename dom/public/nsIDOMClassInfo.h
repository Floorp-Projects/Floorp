/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
  eDOMClassInfo_HTMLOptionCollection_id,
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
  eDOMClassInfo_XULDocument_id,
  eDOMClassInfo_XULElement_id,
  eDOMClassInfo_XULTreeElement_id,
  eDOMClassInfo_XULCommandDispatcher_id,
  eDOMClassInfo_XULNodeList_id,
  eDOMClassInfo_XULNamedNodeMap_id,
  eDOMClassInfo_XULAttr_id,
  eDOMClassInfo_XULControllers_id,
  eDOMClassInfo_BoxObject_id,
  eDOMClassInfo_OutlinerSelection_id,

  // Crypto classes
  eDOMClassInfo_Crypto_id,
  eDOMClassInfo_CRMFObject_id,
  eDOMClassInfo_Pkcs11_id,

  // XML extras classes
  eDOMClassInfo_XMLHttpRequest_id,
  eDOMClassInfo_DOMSerializer_id,
  eDOMClassInfo_DOMParser_id,

  // Transformiix classes
  eDOMClassInfo_XSLTProcessor_id,
  eDOMClassInfo_XPathProcessor_id,
  eDOMClassInfo_NodeSet_id,

  // DOM Traversal classes
  eDOMClassInfo_TreeWalker_id,

  // This one better be the last one in this list
  eDOMClassInfoIDCount
};


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

#define NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(_class)                          \
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {                                \
    static NS_DEFINE_CID(kDOMSOF_CID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);      \
                                                                              \
    nsCOMPtr<nsIDOMScriptObjectFactory> sof(do_GetService(kDOMSOF_CID));      \
    if (sof) {                                                                \
      foundInterface =                                                        \
        sof->GetClassInfoInstance(eDOMClassInfo_##_class##_id);               \
                                                                              \
      if (foundInterface) {                                                   \
        *aInstancePtr = foundInterface;                                       \
                                                                              \
        return NS_OK;                                                         \
      }                                                                       \
    }                                                                         \
  } else

#endif /* nsIDOMClassInfo_h___ */
