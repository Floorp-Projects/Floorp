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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsGenericHTMLElement_h___
#define nsGenericHTMLElement_h___

#include "nsGenericElement.h"
#include "nsHTMLParts.h"
#include "nsIDOMHTMLElement.h"
#include "nsIContent.h"
#include "nsHTMLValue.h"
#include "nsVoidArray.h"
#include "nsIJSScriptObject.h"
#include "nsINameSpaceManager.h"  // for kNameSpaceID_HTML

#include "nsIStatefulFrame.h"

extern const nsIID kIDOMHTMLElementIID;
extern const nsIID kIHTMLContentIID;

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIDOMNodeList;
class nsIEventListenerManager;
class nsIFrame;
class nsIHTMLAttributes;
class nsIHTMLMappedAttributes;
class nsIHTMLContent;
class nsIMutableStyleContext;
class nsIStyleRule;
class nsISupportsArray;
class nsIDOMScriptObjectFactory;
class nsChildContentList;
class nsDOMCSSDeclaration;
class nsIDOMCSSStyleDeclaration;
class nsIURI;
class nsIFormControlFrame;
class nsIFormControl;
class nsIForm;
class nsIPresState;

class nsGenericHTMLElement : public nsGenericElement {
public:
  nsGenericHTMLElement();
  ~nsGenericHTMLElement();

  nsresult CopyInnerTo(nsIContent* aSrcContent,
                       nsGenericHTMLElement* aDest,
                       PRBool aDeep);

  // Implementation for nsIDOMNode
  nsresult    GetNodeName(nsAWritableString& aNodeName);
  nsresult    GetLocalName(nsAWritableString& aLocalName);

  // Implementation for nsIDOMElement
  nsresult    GetAttribute(const nsAReadableString& aName, nsAWritableString& aReturn) 
  {
    return nsGenericElement::GetAttribute(aName, aReturn);
  }
  nsresult    SetAttribute(const nsAReadableString& aName, const nsAReadableString& aValue)
  {
    return nsGenericElement::SetAttribute(aName, aValue);
  }
  nsresult GetTagName(nsAWritableString& aTagName);

  // Implementation for nsIDOMHTMLElement
  nsresult    GetId(nsAWritableString& aId);
  nsresult    SetId(const nsAReadableString& aId);
  nsresult    GetTitle(nsAWritableString& aTitle);
  nsresult    SetTitle(const nsAReadableString& aTitle);
  nsresult    GetLang(nsAWritableString& aLang);
  nsresult    SetLang(const nsAReadableString& aLang);
  nsresult    GetDir(nsAWritableString& aDir);
  nsresult    SetDir(const nsAReadableString& aDir);
  nsresult    GetClassName(nsAWritableString& aClassName);
  nsresult    SetClassName(const nsAReadableString& aClassName);
  nsresult    GetStyle(nsIDOMCSSStyleDeclaration** aStyle);
  nsresult    GetOffsetTop(PRInt32* aOffsetTop);
  nsresult    GetOffsetLeft(PRInt32* aOffsetLeft);
  nsresult    GetOffsetWidth(PRInt32* aOffsetWidth);
  nsresult    GetOffsetHeight(PRInt32* aOffsetHeight);
  nsresult    GetOffsetParent(nsIDOMElement** aOffsetParent);
  nsresult    GetInnerHTML(nsAWritableString& aInnerHTML);
  nsresult    SetInnerHTML(const nsAReadableString& aInnerHTML);
  nsresult    GetOffsetRect(nsRect& aRect, 
                            nsIAtom* aOffsetParentTag,
                            nsIContent** aOffsetParent);


  // Implementation for nsIContent
  nsresult GetNameSpaceID(PRInt32& aNameSpaceID) const;
  nsresult SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers);
  nsresult ParseAttributeString(const nsAReadableString& aStr, 
                                nsIAtom*& aName,
                                PRInt32& aNameSpaceID);
  nsresult GetNameSpacePrefixFromId(PRInt32 aNameSpaceID,
                                    nsIAtom*& aPrefix);
  nsresult SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsAReadableString& aValue,
                        PRBool aNotify);
  nsresult SetAttribute(nsINodeInfo* aNodeInfo, const nsAReadableString& aValue,
                        PRBool aNotify);
  nsresult GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, nsAWritableString& aResult) const;
  nsresult GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, nsIAtom*& aPrefix, nsAWritableString& aResult) const;
  nsresult UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify);
  nsresult GetAttributeNameAt(PRInt32 aIndex,
                              PRInt32& aNameSpaceID, 
                              nsIAtom*& aName,
                              nsIAtom*& aPrefix) const;
  nsresult GetAttributeCount(PRInt32& aResult) const;
  nsresult List(FILE* out, PRInt32 aIndent) const;
  nsresult DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const;
  nsresult SetParentForFormControls(nsIContent* aParent,
                                    nsIFormControl* aControl,
                                    nsIForm* aForm);
  nsresult SetDocumentForFormControls(nsIDocument* aDocument,
                                      PRBool aDeep,
                                      PRBool aCompileEventHandlers,
                                      nsIFormControl* aControl,
                                      nsIForm* aForm);
  nsresult HandleDOMEventForAnchors(nsIContent* aOuter,
                                    nsIPresContext* aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus* aEventStatus);

  // Implementation for nsIHTMLContent
  nsresult Compact();
  nsresult SetHTMLAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue,
                            PRBool aNotify);
  nsresult GetHTMLAttribute(nsIAtom* aAttribute, nsHTMLValue& aValue) const;
  nsresult GetID(nsIAtom*& aResult) const;
  nsresult GetClasses(nsVoidArray& aArray) const;
  nsresult HasClass(nsIAtom* aClass) const;
  nsresult GetContentStyleRules(nsISupportsArray* aRules);
  nsresult GetInlineStyleRules(nsISupportsArray* aRules);
  nsresult GetBaseURL(nsIURI*& aBaseURL) const;
  nsresult GetBaseTarget(nsAWritableString& aBaseTarget) const;
  nsresult ToHTMLString(nsAWritableString& aResult) const;
  nsresult ToHTML(FILE* out) const;
  nsresult SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult,
                  size_t aInstanceSize) const;

  //----------------------------------------
  nsresult AttributeToString(nsIAtom* aAttribute,
                             const nsHTMLValue& aValue,
                             nsAWritableString& aResult) const;

  void ListAttributes(FILE* out) const;


  //----------------------------------------

  // Attribute parsing utilities

  struct EnumTable {
    const char* tag;
    PRInt32 value;
  };

  static PRBool ParseEnumValue(const nsAReadableString& aValue,
                               EnumTable* aTable,
                               nsHTMLValue& aResult);

  static PRBool ParseCaseSensitiveEnumValue(const nsAReadableString& aValue,
                                            EnumTable* aTable,
                                            nsHTMLValue& aResult);

  static PRBool EnumValueToString(const nsHTMLValue& aValue,
                                  EnumTable* aTable,
                                  nsAWritableString& aResult,
                                  PRBool aFoldCase=PR_TRUE);

  static PRBool ParseValueOrPercent(const nsAReadableString& aString,
                                    nsHTMLValue& aResult,
                                    nsHTMLUnit aValueUnit);

  static PRBool ParseValueOrPercentOrProportional(const nsAReadableString& aString,
                                                  nsHTMLValue& aResult, 
                                                  nsHTMLUnit aValueUnit);

  static PRBool ValueOrPercentToString(const nsHTMLValue& aValue,
                                       nsAWritableString& aResult);

  static PRBool ValueOrPercentOrProportionalToString(const nsHTMLValue& aValue,
                                                     nsAWritableString& aResult);

  static PRBool ParseValue(const nsAReadableString& aString, PRInt32 aMin,
                           nsHTMLValue& aResult, nsHTMLUnit aValueUnit);

  static PRBool ParseValue(const nsAReadableString& aString, PRInt32 aMin, PRInt32 aMax,
                           nsHTMLValue& aResult, nsHTMLUnit aValueUnit);

  static PRBool ParseColor(const nsAReadableString& aString, nsIDocument* aDocument,
                           nsHTMLValue& aResult);

  static PRBool ColorToString(const nsHTMLValue& aValue,
                              nsAWritableString& aResult);

  static PRBool ParseCommonAttribute(nsIAtom* aAttribute, 
                                     const nsAReadableString& aValue, 
                                     nsHTMLValue& aResult);
  static PRBool ParseAlignValue(const nsAReadableString& aString, nsHTMLValue& aResult);

  PRBool ParseDivAlignValue(const nsAReadableString& aString,
                            nsHTMLValue& aResult) const;
  PRBool DivAlignValueToString(const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;

  PRBool ParseTableHAlignValue(const nsAReadableString& aString,
                               nsHTMLValue& aResult) const;
  PRBool TableHAlignValueToString(const nsHTMLValue& aValue,
                                  nsAWritableString& aResult) const;

  PRBool ParseTableCellHAlignValue(const nsAReadableString& aString,
                                   nsHTMLValue& aResult) const;
  PRBool TableCellHAlignValueToString(const nsHTMLValue& aValue,
                                      nsAWritableString& aResult) const;

  static PRBool ParseTableVAlignValue(const nsAReadableString& aString,
                                      nsHTMLValue& aResult);

  static PRBool TableVAlignValueToString(const nsHTMLValue& aValue,
                                         nsAWritableString& aResult);

  static PRBool AlignValueToString(const nsHTMLValue& aValue,
                                   nsAWritableString& aResult);

  static PRBool ParseImageAttribute(nsIAtom* aAttribute,
                                    const nsAReadableString& aString,
                                    nsHTMLValue& aResult);

  static PRBool ImageAttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsAWritableString& aResult);

  static PRBool ParseFrameborderValue(PRBool aStandardMode,
                                      const nsAReadableString& aString,
                                      nsHTMLValue& aResult);

  static PRBool FrameborderValueToString(PRBool aStandardMode,
                                         const nsHTMLValue& aValue,
                                         nsAWritableString& aResult);

  static PRBool ParseScrollingValue(PRBool aStandardMode,
                                    const nsAReadableString& aString,
                                    nsHTMLValue& aResult);

  static PRBool ScrollingValueToString(PRBool aStandardMode,
                                       const nsHTMLValue& aValue,
                                       nsAWritableString& aResult);

  nsresult  ReparseStyleAttribute(void);
  nsresult  ParseStyleAttribute(const nsAReadableString& aValue, nsHTMLValue& aResult);

  /** Attribute Mapping Helpers
   *
   * All attributes that are mapped into style contexts must have a 
   * matched set of mapping function and impact getter
   */

  static void MapCommonAttributesInto(const nsIHTMLMappedAttributes* aAttributes, 
                                      nsIMutableStyleContext* aStyleContext,
                                      nsIPresContext* aPresContext);
  static PRBool GetCommonMappedAttributesImpact(const nsIAtom* aAttribute,
                                                PRInt32& aHint);

  static void MapImageAttributesInto(const nsIHTMLMappedAttributes* aAttributes, 
                                     nsIMutableStyleContext* aContext,
                                     nsIPresContext* aPresContext);
  static PRBool GetImageMappedAttributesImpact(const nsIAtom* aAttribute,
                                               PRInt32& aHint);

  static void MapImageAlignAttributeInto(const nsIHTMLMappedAttributes* aAttributes, 
                                         nsIMutableStyleContext* aContext,
                                         nsIPresContext* aPresContext);
  static PRBool GetImageAlignAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint);

  static void MapImageBorderAttributeInto(const nsIHTMLMappedAttributes* aAttributes, 
                                          nsIMutableStyleContext* aContext,
                                          nsIPresContext* aPresContext,
                                          nscolor aBorderColors[4]);
  static PRBool GetImageBorderAttributeImpact(const nsIAtom* aAttribute,
                                              PRInt32& aHint);

  static void MapBackgroundAttributesInto(const nsIHTMLMappedAttributes* aAttributes, 
                                          nsIMutableStyleContext* aContext,
                                          nsIPresContext* aPresContext);
  static PRBool GetBackgroundAttributesImpact(const nsIAtom* aAttribute,
                                              PRInt32& aHint);

  //XXX These three create a dependency between content and frames 
  static nsresult GetPrimaryFrame(nsIHTMLContent* aContent,
                                  nsIFormControlFrame *&aFormControlFrame,
                                  PRBool aFlushNotifications=PR_TRUE);
  static nsresult GetPrimaryPresState(nsIHTMLContent* aContent,
                                          nsIStatefulFrame::StateType aStateType,
                                          nsIPresState** aPresState);
  static nsresult GetPresContext(nsIHTMLContent* aContent, nsIPresContext** aPresContext);

  static nsresult GetBaseURL(const nsHTMLValue& aBaseHref,
                             nsIDocument* aDocument,
                             nsIURI** aResult);

  // See if the content object is in a document that has nav-quirks
  // mode enabled.
  static PRBool InNavQuirksMode(nsIDocument* aDoc);

  nsIHTMLAttributes* mAttributes;
};

//----------------------------------------------------------------------

class nsGenericHTMLLeafElement : public nsGenericHTMLElement {
public:
  nsGenericHTMLLeafElement();
  ~nsGenericHTMLLeafElement();

  nsresult CopyInnerTo(nsIContent* aSrcContent,
                       nsGenericHTMLLeafElement* aDest,
                       PRBool aDeep);

  // Remainder of nsIDOMHTMLElement (and nsIDOMNode)
  nsresult    GetChildNodes(nsIDOMNodeList** aChildNodes);
  nsresult    HasChildNodes(PRBool* aHasChildNodes) {
    *aHasChildNodes = PR_FALSE;
    return NS_OK;
  }
  nsresult    GetFirstChild(nsIDOMNode** aFirstChild) {
    *aFirstChild = nsnull;
    return NS_OK;
  }
  nsresult    GetLastChild(nsIDOMNode** aLastChild) {
    *aLastChild = nsnull;
    return NS_OK;
  }
  nsresult    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                           nsIDOMNode** aReturn) {
    return NS_ERROR_FAILURE;
  }
  nsresult    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                           nsIDOMNode** aReturn) {
    return NS_ERROR_FAILURE;
  }
  nsresult    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) {
    return NS_ERROR_FAILURE;
  }
  nsresult    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn) {
    return NS_ERROR_FAILURE;
  }

  // Remainder of nsIHTMLContent (and nsIContent)
  nsresult BeginConvertToXIF(nsIXIFConverter* aConverter) const;
  nsresult ConvertContentToXIF(nsIXIFConverter* aConverter) const;
  nsresult FinishConvertToXIF(nsIXIFConverter* aConverter) const;
  nsresult Compact() {
    return NS_OK;
  }
  nsresult CanContainChildren(PRBool& aResult) const {
    aResult = PR_FALSE;
    return NS_OK;
  }
  nsresult ChildCount(PRInt32& aResult) const {
    aResult = 0;
    return NS_OK;
  }
  nsresult ChildAt(PRInt32 aIndex, nsIContent*& aResult) const {
    aResult = nsnull;
    return NS_OK;
  }
  nsresult IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const {
    aResult = -1;
    return NS_OK;
  }
  nsresult InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify) {
    return NS_OK;
  }
  nsresult ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify) {
    return NS_OK;
  }
  nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify) {
    return NS_OK;
  }
  nsresult RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {
    return NS_OK;
  }
};

//----------------------------------------------------------------------

class nsGenericHTMLContainerElement : public nsGenericHTMLElement {
public:
  nsGenericHTMLContainerElement();
  ~nsGenericHTMLContainerElement();

  nsresult CopyInnerTo(nsIContent* aSrcContent,
                       nsGenericHTMLContainerElement* aDest,
                       PRBool aDeep);

  // Remainder of nsIDOMHTMLElement (and nsIDOMNode)
  nsresult    GetChildNodes(nsIDOMNodeList** aChildNodes);
  nsresult    HasChildNodes(PRBool* aHasChildNodes);
  nsresult    GetFirstChild(nsIDOMNode** aFirstChild);
  nsresult    GetLastChild(nsIDOMNode** aLastChild);
  
  nsresult    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                           nsIDOMNode** aReturn)
  {
    return nsGenericElement::doInsertBefore(aNewChild, aRefChild, aReturn);
  }
  nsresult    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                           nsIDOMNode** aReturn)
  {
    return nsGenericElement::doReplaceChild(aNewChild, aOldChild, aReturn);
  }
  nsresult    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
  {
    return nsGenericElement::doRemoveChild(aOldChild, aReturn);
  }
  nsresult    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
  {
    return nsGenericElement::doInsertBefore(aNewChild, nsnull, aReturn);
  }

  // Remainder of nsIHTMLContent (and nsIContent)
  nsresult BeginConvertToXIF(nsIXIFConverter* aConverter) const;
  nsresult ConvertContentToXIF(nsIXIFConverter* aConverter) const;
  nsresult FinishConvertToXIF(nsIXIFConverter* aConverter) const;
  nsresult Compact();
  nsresult CanContainChildren(PRBool& aResult) const;
  nsresult ChildCount(PRInt32& aResult) const;
  nsresult ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
  nsresult IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const;
  nsresult InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  nsresult ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify);
  nsresult RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

  nsCheapVoidArray mChildren;
};

//----------------------------------------------------------------------

class nsGenericHTMLContainerFormElement : public nsGenericHTMLContainerElement {
public:
  nsGenericHTMLContainerFormElement();
  ~nsGenericHTMLContainerFormElement();

  nsresult SetForm(nsIForm* aForm);
  nsresult SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsAReadableString& aValue,
                          PRBool aNotify);
  nsresult SetAttribute(nsINodeInfo* aNodeInfo, const nsAReadableString& aValue,
                          PRBool aNotify);
  nsresult SetAttribute(const nsAReadableString& aName, const nsAReadableString& aValue)
  {
    return nsGenericHTMLElement::SetAttribute(aName, aValue);
  }
  nsresult GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);

  nsIForm* mForm;
};

//----------------------------------------------------------------------

class nsGenericHTMLLeafFormElement : public nsGenericHTMLLeafElement {
public:
  nsGenericHTMLLeafFormElement();
  ~nsGenericHTMLLeafFormElement();

  nsresult SetForm(nsIForm* aForm);
  nsresult SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsAReadableString& aValue,
                          PRBool aNotify);
  nsresult SetAttribute(nsINodeInfo* aNodeInfo, const nsAReadableString& aValue,
                          PRBool aNotify);
  nsresult SetAttribute(const nsAReadableString& aName, const nsAReadableString& aValue)
  {
    return nsGenericHTMLElement::SetAttribute(aName, aValue);
  }
  nsresult GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);

  nsIForm* mForm;
};


//----------------------------------------------------------------------
/**
 * Implement the nsIDOMHTMLElement API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 */
#define NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(_g)       \
  NS_IMETHOD GetId(nsAWritableString& aId) {                     \
    return _g.GetId(aId);                               \
  }                                                     \
  NS_IMETHOD SetId(const nsAReadableString& aId) {               \
    return _g.SetId(aId);                               \
  }                                                     \
  NS_IMETHOD GetTitle(nsAWritableString& aTitle) {               \
    return _g.GetTitle(aTitle);                         \
  }                                                     \
  NS_IMETHOD SetTitle(const nsAReadableString& aTitle) {         \
    return _g.SetTitle(aTitle);                         \
  }                                                     \
  NS_IMETHOD GetLang(nsAWritableString& aLang) {                 \
    return _g.GetLang(aLang);                           \
  }                                                     \
  NS_IMETHOD SetLang(const nsAReadableString& aLang) {           \
    return _g.SetLang(aLang);                           \
  }                                                     \
  NS_IMETHOD GetDir(nsAWritableString& aDir) {                   \
    return _g.GetDir(aDir);                             \
  }                                                     \
  NS_IMETHOD SetDir(const nsAReadableString& aDir) {             \
    return _g.SetDir(aDir);                             \
  }                                                     \
  NS_IMETHOD GetClassName(nsAWritableString& aClassName) {       \
    return _g.GetClassName(aClassName);                 \
  }                                                     \
  NS_IMETHOD SetClassName(const nsAReadableString& aClassName) { \
    return _g.SetClassName(aClassName);                 \
  }                                                     \
  NS_IMETHOD GetStyle(nsIDOMCSSStyleDeclaration** aStyle) { \
    return _g.GetStyle(aStyle);                         \
  }                                                     \
  NS_IMETHOD GetOffsetTop(PRInt32* aOffsetTop) {        \
    return _g.GetOffsetTop(aOffsetTop);                 \
  }                                                     \
  NS_IMETHOD GetOffsetLeft(PRInt32* aOffsetLeft) {      \
    return _g.GetOffsetLeft(aOffsetLeft);               \
  }                                                     \
  NS_IMETHOD GetOffsetWidth(PRInt32* aOffsetWidth) {    \
    return _g.GetOffsetWidth(aOffsetWidth);             \
  }                                                     \
  NS_IMETHOD GetOffsetHeight(PRInt32* aOffsetHeight) {  \
    return _g.GetOffsetHeight(aOffsetHeight);           \
  }                                                     \
  NS_IMETHOD GetOffsetParent(nsIDOMElement** aOffsetParent) { \
    return _g.GetOffsetParent(aOffsetParent);           \
  }                                                     \
  NS_IMETHOD GetInnerHTML(nsAWritableString& aInnerHTML) {       \
    return _g.GetInnerHTML(aInnerHTML);                 \
  }                                                     \
  NS_IMETHOD SetInnerHTML(const nsAReadableString& aInnerHTML) { \
    return _g.SetInnerHTML(aInnerHTML);                 \
  }


#define NS_IMPL_IHTMLCONTENT_USING_GENERIC(_g)                         \
  NS_IMETHOD Compact() {                                               \
    return _g.Compact();                                               \
  }                                                                    \
  NS_IMETHOD SetHTMLAttribute(nsIAtom* aAttribute,                     \
                              const nsHTMLValue& aValue, PRBool aNotify) { \
    return _g.SetHTMLAttribute(aAttribute, aValue, aNotify);           \
  }                                                                    \
  NS_IMETHOD GetHTMLAttribute(nsIAtom* aAttribute,                     \
                              nsHTMLValue& aValue) const {             \
    return _g.GetHTMLAttribute(aAttribute, aValue);                    \
  }                                                                    \
  NS_IMETHOD GetID(nsIAtom*& aResult) const {                          \
    return _g.GetID(aResult);                                          \
  }                                                                    \
  NS_IMETHOD GetClasses(nsVoidArray& aArray) const {                   \
    return _g.GetClasses(aArray);                                      \
  }                                                                    \
  NS_IMETHOD HasClass(nsIAtom* aClass) const {                         \
    return _g.HasClass(aClass);                                        \
  }                                                                    \
  NS_IMETHOD GetContentStyleRules(nsISupportsArray* aRules) {          \
    return _g.GetContentStyleRules(aRules);                            \
  }                                                                    \
  NS_IMETHOD GetInlineStyleRules(nsISupportsArray* aRules) {           \
    return _g.GetInlineStyleRules(aRules);                             \
  }                                                                    \
  NS_IMETHOD GetBaseURL(nsIURI*& aBaseURL) const {                     \
    return _g.GetBaseURL(aBaseURL);                                    \
  }                                                                    \
  NS_IMETHOD GetBaseTarget(nsAWritableString& aBaseTarget) const {              \
    return _g.GetBaseTarget(aBaseTarget);                              \
  }                                                                    \
  NS_IMETHOD ToHTMLString(nsAWritableString& aResult) const {                   \
    return _g.ToHTMLString(aResult);                                   \
  }                                                                    \
  NS_IMETHOD ToHTML(FILE* out) const {                                 \
    return _g.ToHTML(out);                                             \
  }                                                                    \
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,                    \
                               const nsAReadableString& aValue,                 \
                               nsHTMLValue& aResult);                  \
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,                    \
                               const nsHTMLValue& aValue,              \
                               nsAWritableString& aResult) const;               \
  NS_IMETHOD GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,  \
                                          nsMapAttributesFunc& aMapFunc) const;  \
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,       \
                                      PRInt32& aHint) const;
  
#define NS_IMPL_IHTMLCONTENT_USING_GENERIC2(_g)                        \
  NS_IMETHOD Compact() {                                               \
    return _g.Compact();                                               \
  }                                                                    \
  NS_IMETHOD SetHTMLAttribute(nsIAtom* aAttribute,                     \
                              const nsHTMLValue& aValue, PRBool aNotify) { \
    return _g.SetHTMLAttribute(aAttribute, aValue, aNotify);           \
  }                                                                    \
  NS_IMETHOD GetHTMLAttribute(nsIAtom* aAttribute,                     \
                              nsHTMLValue& aValue) const {             \
    return _g.GetHTMLAttribute(aAttribute, aValue);                    \
  }                                                                    \
  NS_IMETHOD GetID(nsIAtom*& aResult) const {                          \
    return _g.GetID(aResult);                                          \
  }                                                                    \
  NS_IMETHOD GetClasses(nsVoidArray& aArray) const {                   \
    return _g.GetClasses(aArray);                                      \
  }                                                                    \
  NS_IMETHOD HasClass(nsIAtom* aClass) const {                         \
    return _g.HasClass(aClass);                                        \
  }                                                                    \
  NS_IMETHOD GetContentStyleRules(nsISupportsArray* aRules);           \
  NS_IMETHOD GetInlineStyleRules(nsISupportsArray* aRules);            \
  NS_IMETHOD GetBaseURL(nsIURI*& aBaseURL) const {                     \
    return _g.GetBaseURL(aBaseURL);                                    \
  }                                                                    \
  NS_IMETHOD GetBaseTarget(nsAWritableString& aBaseTarget) const {              \
    return _g.GetBaseTarget(aBaseTarget);                              \
  }                                                                    \
  NS_IMETHOD ToHTMLString(nsAWritableString& aResult) const {                   \
    return _g.ToHTMLString(aResult);                                   \
  }                                                                    \
  NS_IMETHOD ToHTML(FILE* out) const {                                 \
    return _g.ToHTML(out);                                             \
  }                                                                    \
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,                    \
                               const nsAReadableString& aValue,                 \
                               nsHTMLValue& aResult);                  \
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,                    \
                               const nsHTMLValue& aValue,              \
                               nsAWritableString& aResult) const;               \
  NS_IMETHOD GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc, \
                                          nsMapAttributesFunc& aMapFunc) const;  \
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,       \
                                      PRInt32& aHint) const;

/**
 * This macro implements the portion of query interface that is
 * generic to all html content objects.
 */
#define NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(_id, _iptr, _this) \
  NS_IMPL_CONTENT_QUERY_INTERFACE(_id, _iptr, _this, nsIHTMLContent) \
  if (_id.Equals(kIDOMHTMLElementIID)) {                        \
    nsIDOMHTMLElement* tmp = _this;                             \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIHTMLContentIID)) {                           \
    nsIHTMLContent* tmp = _this;                                \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIStyledContentIID)) {                         \
    nsIStyledContent* tmp = _this;                              \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }

/**
 * A macro to implement the getter and setter for a given string
 * valued content property. The method uses the generic SetAttr and
 * GetAttribute methods.
 */
#define NS_IMPL_STRING_ATTR(_class, _method, _atom)                  \
  NS_IMETHODIMP                                                      \
  _class::Get##_method(nsAWritableString& aValue)                    \
  {                                                                  \
    mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::_atom, aValue); \
    return NS_OK;                                                    \
  }                                                                  \
  NS_IMETHODIMP                                                      \
  _class::Set##_method(const nsAReadableString& aValue)              \
  {                                                                  \
    return mInner.SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::_atom, aValue, PR_TRUE); \
  }

/**
 * A macro to implement the getter and setter for a given boolean
 * valued content property. The method uses the generic SetAttr and
 * GetAttribute methods.
 */
#define NS_IMPL_BOOL_ATTR(_class, _method, _atom)                     \
  NS_IMETHODIMP                                                       \
  _class::Get##_method(PRBool* aValue)                                \
  {                                                                   \
    nsHTMLValue val;                                                  \
    nsresult rv = mInner.GetHTMLAttribute(nsHTMLAtoms::_atom, val);   \
    *aValue = NS_CONTENT_ATTR_NOT_THERE != rv;                        \
    return NS_OK;                                                     \
  }                                                                   \
  NS_IMETHODIMP                                                       \
  _class::Set##_method(PRBool aValue)                                 \
  {                                                                   \
    nsHTMLValue empty(eHTMLUnit_Empty);                               \
    if (aValue) {                                                     \
      return mInner.SetHTMLAttribute(nsHTMLAtoms::_atom, empty, PR_TRUE); \
    }                                                                 \
    else {                                                            \
      mInner.UnsetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::_atom, PR_TRUE);  \
      return NS_OK;                                                   \
    }                                                                 \
  }

/**
 * A macro to implement the getter and setter for a given integer
 * valued content property. The method uses the generic SetAttr and
 * GetAttribute methods.
 */
#define NS_IMPL_INT_ATTR(_class, _method, _atom)                    \
  NS_IMETHODIMP                                                     \
  _class::Get##_method(PRInt32* aValue)                             \
  {                                                                 \
    nsHTMLValue value;                                              \
    *aValue = -1;                                                   \
    if (NS_CONTENT_ATTR_HAS_VALUE ==                                \
        mInner.GetHTMLAttribute(nsHTMLAtoms::_atom, value)) {       \
      if (value.GetUnit() == eHTMLUnit_Integer) {                   \
        *aValue = value.GetIntValue();                              \
      }                                                             \
    }                                                               \
    return NS_OK;                                                   \
  }                                                                 \
  NS_IMETHODIMP                                                     \
  _class::Set##_method(PRInt32 aValue)                              \
  {                                                                 \
    nsHTMLValue value(aValue, eHTMLUnit_Integer);                   \
    return mInner.SetHTMLAttribute(nsHTMLAtoms::_atom, value, PR_TRUE); \
  }

#endif /* nsGenericHTMLElement_h___ */
