/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Johnny Stenback <jst@netscape.com> (original author)
 */
#ifndef nsIDOMClassInfo_h___
#define nsIDOMClassInfo_h___

#include "nsIClassInfo.h"
#include "nsVoidArray.h"

typedef void (*GetDOMClassIIDsFnc)(nsVoidArray& aArray);

class nsIDOMClassInfo : public nsIClassInfo
{
public:
  enum nsDOMClassInfoID {

    // Core classes
    eDocument_id,
    eDocumentType_id,
    eDOMImplementation_id,
    eDocumentFragment_id,
    eElement_id,
    eAttr_id,
    eText_id,
    eComment_id,
    eCDATASection_id,
    eProcessingInstruction_id,
    eEntity_id,
    eEntityReference_id,
    eNotation_id,
    eNodeList_id,
    eNamedNodeMap_id,

    // Event classes
    eEvent_id,

    // HTML classes
    eHTMLDocument_id,
    eHTMLCollection_id,
    eHTMLOptionCollection_id,
    eHTMLFormControlCollection_id,

    // HTML element classes
    eHTMLAnchorElement_id,
    eHTMLAppletElement_id,
    eHTMLAreaElement_id,
    eHTMLBRElement_id,
    eHTMLBaseElement_id,
    eHTMLBaseFontElement_id,
    eHTMLBodyElement_id,
    eHTMLButtonElement_id,
    eHTMLDListElement_id,
    eHTMLDelElement_id,
    eHTMLDirectoryElement_id,
    eHTMLDivElement_id,
    eHTMLEmbedElement_id,
    eHTMLFieldSetElement_id,
    eHTMLFontElement_id,
    eHTMLFormElement_id,
    eHTMLFrameElement_id,
    eHTMLFrameSetElement_id,
    eHTMLHRElement_id,
    eHTMLHeadElement_id,
    eHTMLHeadingElement_id,
    eHTMLHtmlElement_id,
    eHTMLIFrameElement_id,
    eHTMLImageElement_id,
    eHTMLInputElement_id,
    eHTMLInsElement_id,
    eHTMLIsIndexElement_id,
    eHTMLLIElement_id,
    eHTMLLabelElement_id,
    eHTMLLegendElement_id,
    eHTMLLinkElement_id,
    eHTMLMapElement_id,
    eHTMLMenuElement_id,
    eHTMLMetaElement_id,
    eHTMLModElement_id,
    eHTMLOListElement_id,
    eHTMLObjectElement_id,
    eHTMLOptGroupElement_id,
    eHTMLOptionElement_id,
    eHTMLParagraphElement_id,
    eHTMLParamElement_id,
    eHTMLPreElement_id,
    eHTMLQuoteElement_id,
    eHTMLScriptElement_id,
    eHTMLSelectElement_id,
    eHTMLSpacerElement_id,
    eHTMLSpanElement_id,
    eHTMLStyleElement_id,
    eHTMLTableCaptionElement_id,
    eHTMLTableCellElement_id,
    eHTMLTableColElement_id,
    eHTMLTableColGroupElement_id,
    eHTMLTableElement_id,
    eHTMLTableRowElement_id,
    eHTMLTableSectionElement_id,
    eHTMLTextAreaElement_id,
    eHTMLTitleElement_id,
    eHTMLUListElement_id,
    eHTMLUnknownElement_id,
    eHTMLWBRElement_id,

    // CSS classes
    eCSSStyleRule_id,
    eCSSRuleList_id,
    eMediaList_id,
    eStyleSheetList_id,
    eCSSStyleSheet_id,
    eCSSStyleDeclaration_id,
    eComputedCSSStyleDeclaration_id,
    eROCSSPrimitiveValue_id,

    // Range classes
    eRange_id,
    eSelection_id,

    // XUL classes
    eXULDocument_id,
    eXULElement_id,
    eXULTreeElement_id,
    eXULCommandDispatcher_id,
    eXULNodeList_id,
    eXULNamedNodeMap_id,
    eXULAttr_id,
    eXULPDGlobalObject_id,

    eDOMClassInfoIDCount // This one better be the last one in this list
  };

  nsISupports* GetClassInfoInstance(nsDOMClassInfoID aID,
                                    GetDOMClassIIDsFnc aGetIIDsFptr,
                                    const char *aName);
};


/**
 * nsIClassInfo helper macros
 */

#define NS_CLASSINFO_MAP_BEGIN(_class)                                        \
static void Get##_class##IIDs(nsVoidArray& aArray)                            \
{

#define NS_CLASSINFO_MAP_BEGIN_EXPORTED(_class)                               \
void Get##_class##IIDs(nsVoidArray& aArray)                                   \
{

#define NS_CLASSINFO_MAP_ENTRY(_interface)                                    \
  aArray.AppendElement((void *)&NS_GET_IID(_interface));

#define NS_CLASSINFO_MAP_ENTRY_FUNCTION(_function)                            \
  _function(aArray);

#define NS_CLASSINFO_MAP_END                                                  \
}

#endif /* nsIDOMClassInfo_h___ */
