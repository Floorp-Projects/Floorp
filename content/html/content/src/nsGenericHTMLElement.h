/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
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

extern const nsIID kIDOMHTMLElementIID;
extern const nsIID kIHTMLContentIID;

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIDOMNodeList;
class nsIEventListenerManager;
class nsIFrame;
class nsIHTMLAttributes;
class nsIHTMLContent;
class nsIStyleContext;
class nsIStyleRule;
class nsISupportsArray;
class nsIDOMScriptObjectFactory;
class nsChildContentList;
class nsDOMCSSDeclaration;
class nsIDOMCSSStyleDeclaration;
class nsIURL;
class nsIFormControlFrame;

class nsGenericHTMLElement : public nsGenericElement {
public:
  nsGenericHTMLElement();
  ~nsGenericHTMLElement();

  nsresult CopyInnerTo(nsIContent* aSrcContent,
                       nsGenericHTMLElement* aDest,
                       PRBool aDeep);

  // Implementation for nsIDOMElement
  nsresult    GetAttribute(const nsString& aName, nsString& aReturn) 
  {
    return nsGenericElement::GetAttribute(aName, aReturn);
  }
  nsresult    SetAttribute(const nsString& aName, const nsString& aValue)
  {
    return nsGenericElement::SetAttribute(aName, aValue);
  }
  nsresult GetTagName(nsString& aTagName);

  // Implementation for nsIDOMHTMLElement
  nsresult    GetId(nsString& aId);
  nsresult    SetId(const nsString& aId);
  nsresult    GetTitle(nsString& aTitle);
  nsresult    SetTitle(const nsString& aTitle);
  nsresult    GetLang(nsString& aLang);
  nsresult    SetLang(const nsString& aLang);
  nsresult    GetDir(nsString& aDir);
  nsresult    SetDir(const nsString& aDir);
  nsresult    GetClassName(nsString& aClassName);
  nsresult    SetClassName(const nsString& aClassName);
  nsresult    GetStyle(nsIDOMCSSStyleDeclaration** aStyle);

  // Implementation for nsIContent
  nsresult GetNameSpaceID(PRInt32& aNameSpaceID) const;
  nsresult SetDocument(nsIDocument* aDocument, PRBool aDeep);
  nsresult ParseAttributeString(const nsString& aStr, 
                                nsIAtom*& aName,
                                PRInt32& aNameSpaceID);
  nsresult GetNameSpacePrefix(PRInt32 aNameSpaceID,
                              nsIAtom*& aPrefix);
  nsresult SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsString& aValue,
                        PRBool aNotify);
  nsresult GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, nsString& aResult) const;
  nsresult UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify);
  nsresult GetAttributeNameAt(PRInt32 aIndex,
                              PRInt32& aNameSpaceID, 
                              nsIAtom*& aName) const;
  nsresult GetAttributeCount(PRInt32& aResult) const;
  nsresult List(FILE* out, PRInt32 aIndent) const;

  // Implementation for nsIHTMLContent
  nsresult Compact();
  nsresult SetHTMLAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue,
                            PRBool aNotify);
  nsresult GetHTMLAttribute(nsIAtom* aAttribute, nsHTMLValue& aValue) const;
  nsresult GetID(nsIAtom*& aResult) const;
  nsresult GetClasses(nsVoidArray& aArray) const;
  nsresult HasClass(nsIAtom* aClass) const;
  nsresult GetContentStyleRule(nsIStyleRule*& aResult);
  nsresult GetInlineStyleRule(nsIStyleRule*& aResult);
  nsresult GetBaseURL(nsIURL*& aBaseURL) const;
  nsresult GetBaseTarget(nsString& aBaseTarget) const;
  nsresult ToHTMLString(nsString& aResult) const;
  nsresult ToHTML(FILE* out) const;

  //----------------------------------------
  nsresult AttributeToString(nsIAtom* aAttribute,
                             const nsHTMLValue& aValue,
                             nsString& aResult) const;

  void ListAttributes(FILE* out) const;


  //----------------------------------------

  // Attribute parsing utilities

  struct EnumTable {
    const char* tag;
    PRInt32 value;
  };

  static PRBool ParseEnumValue(const nsString& aValue,
                               EnumTable* aTable,
                               nsHTMLValue& aResult);

  static PRBool ParseCaseSensitiveEnumValue(const nsString& aValue,
                                            EnumTable* aTable,
                                            nsHTMLValue& aResult);

  static PRBool EnumValueToString(const nsHTMLValue& aValue,
                                  EnumTable* aTable,
                                  nsString& aResult);

  static PRBool ParseValueOrPercent(const nsString& aString,
                                    nsHTMLValue& aResult,
                                    nsHTMLUnit aValueUnit);

  static void ParseValueOrPercentOrProportional(const nsString& aString,
                                                nsHTMLValue& aResult, 
                                                nsHTMLUnit aValueUnit);

  static PRBool ValueOrPercentToString(const nsHTMLValue& aValue,
                                       nsString& aResult);

  static PRBool ValueOrPercentOrProportionalToString(const nsHTMLValue& aValue,
                                                     nsString& aResult);

  static PRBool ParseValue(const nsString& aString, PRInt32 aMin,
                           nsHTMLValue& aResult, nsHTMLUnit aValueUnit);

  static PRBool ParseValue(const nsString& aString, PRInt32 aMin, PRInt32 aMax,
                           nsHTMLValue& aResult, nsHTMLUnit aValueUnit);

  static PRBool ParseColor(const nsString& aString, nsHTMLValue& aResult);

  static PRBool ColorToString(const nsHTMLValue& aValue,
                              nsString& aResult);

  static PRBool ParseAlignValue(const nsString& aString, nsHTMLValue& aResult);

  static PRBool ParseDivAlignValue(const nsString& aString,
                                   nsHTMLValue& aResult);

  static PRBool ParseTableHAlignValue(const nsString& aString,
                                      nsHTMLValue& aResult);

  static PRBool ParseTableVAlignValue(const nsString& aString,
                                      nsHTMLValue& aResult);

  static PRBool TableHAlignValueToString(const nsHTMLValue& aValue,
                                         nsString& aResult);

  static PRBool TableVAlignValueToString(const nsHTMLValue& aValue,
                                         nsString& aResult);

  static PRBool AlignValueToString(const nsHTMLValue& aValue,
                                   nsString& aResult);

  static PRBool DivAlignValueToString(const nsHTMLValue& aValue,
                                      nsString& aResult);

  static PRBool ParseImageAttribute(nsIAtom* aAttribute,
                                    const nsString& aString,
                                    nsHTMLValue& aResult);

  static PRBool ImageAttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsString& aResult);

  static PRBool ParseFrameborderValue(PRBool aStandardMode,
                                      const nsString& aString,
                                      nsHTMLValue& aResult);

  static PRBool FrameborderValueToString(PRBool aStandardMode,
                                         const nsHTMLValue& aValue,
                                         nsString& aResult);
  static void MapCommonAttributesInto(nsIHTMLAttributes* aAttributes, 
                                      nsIStyleContext* aStyleContext,
                                      nsIPresContext* aPresContext);

  static void MapImageAttributesInto(nsIHTMLAttributes* aAttributes, 
                                     nsIStyleContext* aContext,
                                     nsIPresContext* aPresContext);

  static void MapImageAlignAttributeInto(nsIHTMLAttributes* aAttributes, 
                                         nsIStyleContext* aContext,
                                         nsIPresContext* aPresContext);

  static void MapImageBorderAttributesInto(nsIHTMLAttributes* aAttributes, 
                                           nsIStyleContext* aContext,
                                           nsIPresContext* aPresContext,
                                           nscolor aBorderColors[4]);

  static void MapBackgroundAttributesInto(nsIHTMLAttributes* aAttributes, 
                                          nsIStyleContext* aContext,
                                          nsIPresContext* aPresContext);

  static PRBool ParseScrollingValue(PRBool aStandardMode,
                                    const nsString& aString,
                                    nsHTMLValue& aResult);

  static PRBool ScrollingValueToString(PRBool aStandardMode,
                                       const nsHTMLValue& aValue,
                                       nsString& aResult);

  static PRBool GetStyleHintForCommonAttributes(const nsIContent* aNode,
                                                const nsIAtom* aAttribute,
                                                PRInt32* aHint);

  //XXX This creates a dependency between content and frames 
  static nsresult GetPrimaryFrame(nsIHTMLContent* aContent,
                                  nsIFormControlFrame *&aFormControlFrame);

  static nsresult GetBaseURL(nsIHTMLAttributes* aAttributes,
                             nsIDocument* aDocument,
                             nsIURL** aResult);

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
  nsresult BeginConvertToXIF(nsXIFConverter& aConverter) const;
  nsresult ConvertContentToXIF(nsXIFConverter& aConverter) const;
  nsresult FinishConvertToXIF(nsXIFConverter& aConverter) const;
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
                           nsIDOMNode** aReturn);
  nsresult    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                           nsIDOMNode** aReturn);
  nsresult    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  nsresult    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn);

  // Remainder of nsIHTMLContent (and nsIContent)
  nsresult BeginConvertToXIF(nsXIFConverter& aConverter) const;
  nsresult ConvertContentToXIF(nsXIFConverter& aConverter) const;
  nsresult FinishConvertToXIF(nsXIFConverter& aConverter) const;
  nsresult Compact();
  nsresult CanContainChildren(PRBool& aResult) const;
  nsresult ChildCount(PRInt32& aResult) const;
  nsresult ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
  nsresult IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const;
  nsresult InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  nsresult ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify);
  nsresult RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

  nsVoidArray mChildren;
};

//----------------------------------------------------------------------

/**
 * Implement the nsIDOMHTMLElement API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 */
#define NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(_g)       \
  NS_IMETHOD GetId(nsString& aId) {                     \
    return _g.GetId(aId);                               \
  }                                                     \
  NS_IMETHOD SetId(const nsString& aId) {               \
    return _g.SetId(aId);                               \
  }                                                     \
  NS_IMETHOD GetTitle(nsString& aTitle) {               \
    return _g.GetTitle(aTitle);                         \
  }                                                     \
  NS_IMETHOD SetTitle(const nsString& aTitle) {         \
    return _g.SetTitle(aTitle);                         \
  }                                                     \
  NS_IMETHOD GetLang(nsString& aLang) {                 \
    return _g.GetLang(aLang);                           \
  }                                                     \
  NS_IMETHOD SetLang(const nsString& aLang) {           \
    return _g.SetLang(aLang);                           \
  }                                                     \
  NS_IMETHOD GetDir(nsString& aDir) {                   \
    return _g.GetDir(aDir);                             \
  }                                                     \
  NS_IMETHOD SetDir(const nsString& aDir) {             \
    return _g.SetDir(aDir);                             \
  }                                                     \
  NS_IMETHOD GetClassName(nsString& aClassName) {       \
    return _g.GetClassName(aClassName);                 \
  }                                                     \
  NS_IMETHOD SetClassName(const nsString& aClassName) { \
    return _g.SetClassName(aClassName);                 \
  }                                                     \
  NS_IMETHOD GetStyle(nsIDOMCSSStyleDeclaration** aStyle) { \
    return _g.GetStyle(aStyle);                         \
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
  NS_IMETHOD GetContentStyleRule(nsIStyleRule*& aResult) {             \
    return _g.GetContentStyleRule(aResult);                            \
  }                                                                    \
  NS_IMETHOD GetInlineStyleRule(nsIStyleRule*& aResult) {              \
    return _g.GetInlineStyleRule(aResult);                             \
  }                                                                    \
  NS_IMETHOD GetBaseURL(nsIURL*& aBaseURL) const {                     \
    return _g.GetBaseURL(aBaseURL);                                    \
  }                                                                    \
  NS_IMETHOD GetBaseTarget(nsString& aBaseTarget) const {              \
    return _g.GetBaseTarget(aBaseTarget);                              \
  }                                                                    \
  NS_IMETHOD ToHTMLString(nsString& aResult) const {                   \
    return _g.ToHTMLString(aResult);                                   \
  }                                                                    \
  NS_IMETHOD ToHTML(FILE* out) const {                                 \
    return _g.ToHTML(out);                                             \
  }                                                                    \
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,                    \
                               const nsString& aValue,                 \
                               nsHTMLValue& aResult);                  \
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,                    \
                               const nsHTMLValue& aValue,              \
                               nsString& aResult) const;               \
  NS_IMETHOD GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const;  \
  NS_IMETHOD GetStyleHintForAttributeChange(const nsIAtom* aAttribute, \
                                            PRInt32 *aHint) const;                                             
  
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
  NS_IMETHOD GetContentStyleRule(nsIStyleRule*& aResult);              \
  NS_IMETHOD GetInlineStyleRule(nsIStyleRule*& aResult);               \
  NS_IMETHOD GetBaseURL(nsIURL*& aBaseURL) const {                     \
    return _g.GetBaseURL(aBaseURL);                                    \
  }                                                                    \
  NS_IMETHOD GetBaseTarget(nsString& aBaseTarget) const {              \
    return _g.GetBaseTarget(aBaseTarget);                              \
  }                                                                    \
  NS_IMETHOD ToHTMLString(nsString& aResult) const {                   \
    return _g.ToHTMLString(aResult);                                   \
  }                                                                    \
  NS_IMETHOD ToHTML(FILE* out) const {                                 \
    return _g.ToHTML(out);                                             \
  }                                                                    \
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,                    \
                               const nsString& aValue,                 \
                               nsHTMLValue& aResult);                  \
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,                    \
                               const nsHTMLValue& aValue,              \
                               nsString& aResult) const;               \
  NS_IMETHOD GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const;  \
  NS_IMETHOD GetStyleHintForAttributeChange(const nsIAtom* aAttribute, \
                                            PRInt32 *aHint) const;

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
  _class::Get##_method(nsString& aValue)                             \
  {                                                                  \
    mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::_atom, aValue); \
    return NS_OK;                                                    \
  }                                                                  \
  NS_IMETHODIMP                                                      \
  _class::Set##_method(const nsString& aValue)                       \
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
