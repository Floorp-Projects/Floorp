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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsGenericHTMLElement_h___
#define nsGenericHTMLElement_h___

#include "nsGenericElement.h"
#include "nsHTMLParts.h"
#include "nsIDOMHTMLElement.h"
#include "nsIContent.h"
#include "nsHTMLValue.h"
#include "nsVoidArray.h"
#include "nsINameSpaceManager.h"  // for kNameSpaceID_HTML
#include "nsIFormControl.h"

#include "nsIStatefulFrame.h"

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIDOMNodeList;
class nsIEventListenerManager;
class nsIFrame;
class nsIHTMLAttributes;
class nsIHTMLMappedAttributes;
class nsIHTMLContent;
class nsIStyleRule;
class nsISupportsArray;
class nsChildContentList;
class nsDOMCSSDeclaration;
class nsIDOMCSSStyleDeclaration;
class nsIURI;
class nsIFormControlFrame;
class nsIForm;
class nsIPresState;
struct nsRect;

extern void GetGenericHTMLElementIIDs(nsVoidArray& aArray);

class nsGenericHTMLElement : public nsGenericElement {
public:
  nsGenericHTMLElement();
  virtual ~nsGenericHTMLElement();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  nsresult DOMQueryInterface(nsIDOMHTMLElement *aElement, REFNSIID aIID,
                             void **aInstancePtr);

  NS_METHOD CopyInnerTo(nsIContent* aSrcContent,
                        nsGenericHTMLElement* aDest,
                        PRBool aDeep);

  // Implementation for nsIDOMNode
  NS_METHOD GetNodeName(nsAWritableString& aNodeName);
  NS_METHOD GetLocalName(nsAWritableString& aLocalName);

  // Implementation for nsIDOMElement
  NS_METHOD GetAttribute(const nsAReadableString& aName,
                         nsAWritableString& aReturn) 
  {
    return nsGenericElement::GetAttribute(aName, aReturn);
  }
  NS_METHOD SetAttribute(const nsAReadableString& aName,
                         const nsAReadableString& aValue)
  {
    return nsGenericElement::SetAttribute(aName, aValue);
  }
  NS_METHOD GetTagName(nsAWritableString& aTagName);
  NS_METHOD GetElementsByTagName(const nsAReadableString& aTagname,
                                 nsIDOMNodeList** aReturn);

  // Implementation for nsIDOMHTMLElement
  NS_METHOD GetId(nsAWritableString& aId);
  NS_METHOD SetId(const nsAReadableString& aId);
  NS_METHOD GetTitle(nsAWritableString& aTitle);
  NS_METHOD SetTitle(const nsAReadableString& aTitle);
  NS_METHOD GetLang(nsAWritableString& aLang);
  NS_METHOD SetLang(const nsAReadableString& aLang);
  NS_METHOD GetDir(nsAWritableString& aDir);
  NS_METHOD SetDir(const nsAReadableString& aDir);
  NS_METHOD GetClassName(nsAWritableString& aClassName);
  NS_METHOD SetClassName(const nsAReadableString& aClassName);
  NS_METHOD GetStyle(nsIDOMCSSStyleDeclaration** aStyle);
  NS_METHOD GetOffsetTop(PRInt32* aOffsetTop);
  NS_METHOD GetOffsetLeft(PRInt32* aOffsetLeft);
  NS_METHOD GetOffsetWidth(PRInt32* aOffsetWidth);
  NS_METHOD GetOffsetHeight(PRInt32* aOffsetHeight);
  NS_METHOD GetOffsetParent(nsIDOMElement** aOffsetParent);
  NS_METHOD GetInnerHTML(nsAWritableString& aInnerHTML);
  NS_METHOD SetInnerHTML(const nsAReadableString& aInnerHTML);
  NS_METHOD GetOffsetRect(nsRect& aRect, 
                         nsIAtom* aOffsetParentTag,
                         nsIContent** aOffsetParent);

  // Implementation for nsIContent
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);
  NS_IMETHOD GetNameSpaceID(PRInt32& aID) const;
  NS_IMETHOD NormalizeAttrString(const nsAReadableString& aStr,
                                 nsINodeInfo*& aNodeInfo);
  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAReadableString& aValue,
                          PRBool aNotify);
  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
                     const nsAReadableString& aValue,
                     PRBool aNotify);
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     nsAWritableString& aResult) const;
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     nsIAtom*& aPrefix, nsAWritableString& aResult) const;
  NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                       PRBool aNotify);
  NS_IMETHOD GetAttrNameAt(PRInt32 aIndex,
                           PRInt32& aNameSpaceID, 
                           nsIAtom*& aName,
                           nsIAtom*& aPrefix) const;
  NS_IMETHOD GetAttrCount(PRInt32& aResult) const;
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const;
  NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags);

  nsresult HandleDOMEventForAnchors(nsIContent* aOuter,
                                    nsIPresContext* aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus* aEventStatus);

  // Implementation for nsIHTMLContent
  NS_IMETHOD SetHTMLAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue,
                              PRBool aNotify);
  NS_IMETHOD GetHTMLAttribute(nsIAtom* aAttribute, nsHTMLValue& aValue) const;
  NS_IMETHOD GetID(nsIAtom*& aResult) const;
  NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
  NS_IMETHOD HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const;
  NS_IMETHOD WalkContentStyleRules(nsIRuleWalker* aRuleWalker);
  NS_IMETHOD WalkInlineStyleRules(nsIRuleWalker* aRuleWalker);
  NS_IMETHOD GetBaseURL(nsIURI*& aBaseURL) const;
  NS_IMETHOD GetBaseTarget(nsAWritableString& aBaseTarget) const;

  //----------------------------------------
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;

  void ListAttributes(FILE* out) const;

  PRUint32 BaseSizeOf(nsISizeOfHandler* aSizer) const;



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

  static PRBool ParseValue(const nsAReadableString& aString, PRInt32 aMin,
                           PRInt32 aMax, nsHTMLValue& aResult,
                           nsHTMLUnit aValueUnit);

  static PRBool ParseColor(const nsAReadableString& aString,
                           nsIDocument* aDocument, nsHTMLValue& aResult);

  static PRBool ColorToString(const nsHTMLValue& aValue,
                              nsAWritableString& aResult);

  static PRBool ParseCommonAttribute(nsIAtom* aAttribute, 
                                     const nsAReadableString& aValue, 
                                     nsHTMLValue& aResult);
  static PRBool ParseAlignValue(const nsAReadableString& aString,
                                nsHTMLValue& aResult);

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
  nsresult  ParseStyleAttribute(const nsAReadableString& aValue,
                                nsHTMLValue& aResult);

  /** Attribute Mapping Helpers
   *
   * All attributes that are mapped into style contexts must have a 
   * matched set of mapping function and impact getter
   */

  static void MapCommonAttributesInto(const nsIHTMLMappedAttributes* aAttributes, 
                                      nsRuleData* aRuleData);
  static PRBool GetCommonMappedAttributesImpact(const nsIAtom* aAttribute,
                                                PRInt32& aHint);

  static PRBool GetImageMappedAttributesImpact(const nsIAtom* aAttribute,
                                               PRInt32& aHint);
  static PRBool GetImageAlignAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint);

  static void MapAlignAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                    nsRuleData* aData);
  static void MapImageBorderAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                          nsRuleData* aData);
  static void MapImageMarginAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                          nsRuleData* aData);
  static void MapImagePositionAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                            nsRuleData* aData);
  static PRBool GetImageBorderAttributeImpact(const nsIAtom* aAttribute,
                                              PRInt32& aHint);

  static void MapBackgroundAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                                          nsRuleData* aData);
  static PRBool GetBackgroundAttributesImpact(const nsIAtom* aAttribute,
                                              PRInt32& aHint);

  //XXX These three create a dependency between content and frames 
  static nsresult GetPrimaryFrame(nsIHTMLContent* aContent,
                                  nsIFormControlFrame *&aFormControlFrame,
                                  PRBool aFlushNotifications=PR_TRUE);
  static nsresult GetPrimaryPresState(nsIHTMLContent* aContent,
                                      nsIPresState** aPresState);
  static nsresult GetPresContext(nsIHTMLContent* aContent,
                                 nsIPresContext** aPresContext);

  static nsresult GetBaseURL(const nsHTMLValue& aBaseHref,
                             nsIDocument* aDocument,
                             nsIURI** aResult);

  // Form Helper Routines
  nsresult FindForm(nsIDOMHTMLFormElement **aForm);

  nsresult FindAndSetForm(nsIFormControl *aFormControl);

  // See if the content object is in a document that has nav-quirks
  // mode enabled.
  static PRBool InNavQuirksMode(nsIDocument* aDoc);

  nsresult SetFormControlAttribute(nsIForm* aForm, PRInt32 aNameSpaceID,
                                   nsIAtom* aName,
                                   const nsAReadableString& aValue,
                                   PRBool aNotify);

  nsIHTMLAttributes* mAttributes;

protected:
  nsresult SetElementFocus(PRBool aDoFocus);

  nsresult HandleFrameOnloadEvent(nsIDOMEvent* aEvent);

  PRBool IsEventName(nsIAtom* aName);
};


//----------------------------------------------------------------------

class nsGenericHTMLLeafElement : public nsGenericHTMLElement {
public:
  nsGenericHTMLLeafElement();
  virtual ~nsGenericHTMLLeafElement();

  NS_METHOD CopyInnerTo(nsIContent* aSrcContent,
                        nsGenericHTMLLeafElement* aDest,
                        PRBool aDeep);

  // Remainder of nsIDOMHTMLElement (and nsIDOMNode)
  NS_METHOD GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_METHOD HasChildNodes(PRBool* aHasChildNodes) {
    *aHasChildNodes = PR_FALSE;
    return NS_OK;
  }
  NS_METHOD GetFirstChild(nsIDOMNode** aFirstChild) {
    *aFirstChild = nsnull;
    return NS_OK;
  }
  NS_METHOD GetLastChild(nsIDOMNode** aLastChild) {
    *aLastChild = nsnull;
    return NS_OK;
  }
  NS_METHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                        nsIDOMNode** aReturn) {
    return NS_ERROR_FAILURE;
  }
  NS_METHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                        nsIDOMNode** aReturn) {
    return NS_ERROR_FAILURE;
  }
  NS_METHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) {
    return NS_ERROR_FAILURE;
  }
  NS_METHOD AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn) {
    return NS_ERROR_FAILURE;
  }

  // Remainder of nsIHTMLContent (and nsIContent)
  NS_IMETHOD Compact() {
    return NS_OK;
  }
  NS_IMETHOD CanContainChildren(PRBool& aResult) const {
    aResult = PR_FALSE;
    return NS_OK;
  }
  NS_IMETHOD ChildCount(PRInt32& aResult) const {
    aResult = 0;
    return NS_OK;
  }
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const {
    aResult = nsnull;
    return NS_OK;
  }
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const {
    aResult = -1;
    return NS_OK;
  }
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                           PRBool aDeepSetDocument) {
    return NS_OK;
  }
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                            PRBool aDeepSetDocument) {
    return NS_OK;
  }
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                           PRBool aDeepSetDocument) {
    return NS_OK;
  }
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {
    return NS_OK;
  }
};

//----------------------------------------------------------------------

class nsGenericHTMLContainerElement : public nsGenericHTMLElement
{
public:
  nsGenericHTMLContainerElement();
  virtual ~nsGenericHTMLContainerElement();

  NS_METHOD CopyInnerTo(nsIContent* aSrcContent,
                       nsGenericHTMLContainerElement* aDest,
                       PRBool aDeep);

  // Remainder of nsIDOMHTMLElement (and nsIDOMNode)
  NS_METHOD GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_METHOD HasChildNodes(PRBool* aHasChildNodes);
  NS_METHOD GetFirstChild(nsIDOMNode** aFirstChild);
  NS_METHOD GetLastChild(nsIDOMNode** aLastChild);
  
  NS_METHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                         nsIDOMNode** aReturn)
  {
    return nsGenericElement::doInsertBefore(aNewChild, aRefChild, aReturn);
  }
  NS_METHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                         nsIDOMNode** aReturn)
  {
    return nsGenericElement::doReplaceChild(aNewChild, aOldChild, aReturn);
  }
  NS_METHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
  {
    return nsGenericElement::doRemoveChild(aOldChild, aReturn);
  }
  NS_METHOD AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
  {
    return nsGenericElement::doInsertBefore(aNewChild, nsnull, aReturn);
  }

  // Remainder of nsIHTMLContent (and nsIContent)
  NS_IMETHOD Compact();
  NS_IMETHOD CanContainChildren(PRBool& aResult) const;
  NS_IMETHOD ChildCount(PRInt32& aResult) const;
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const;
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                           PRBool aDeepSetDocument);
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                            PRBool aDeepSetDocument);
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                           PRBool aDeepSetDocument);
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

  nsCheapVoidArray mChildren;
};

//----------------------------------------------------------------------

class nsGenericHTMLContainerFormElement : public nsGenericHTMLContainerElement,
                                          public nsIFormControl
{
public:
  nsGenericHTMLContainerFormElement();
  virtual ~nsGenericHTMLContainerFormElement();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags);

  // nsIFormControl
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm,
                     PRBool aRemoveFromForm = PR_TRUE);
  NS_IMETHOD Init();

  // nsIContent
  NS_IMETHOD SetParent(nsIContent *aParent);
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);

  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAReadableString& aValue, PRBool aNotify);

  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
                     const nsAReadableString& aValue,
                     PRBool aNotify);


  NS_METHOD SetAttribute(const nsAReadableString& aName,
                         const nsAReadableString& aValue)
  {
    return nsGenericHTMLElement::SetAttribute(aName, aValue);
  }

protected:
  nsIForm* mForm;
};

//----------------------------------------------------------------------

class nsGenericHTMLLeafFormElement : public nsGenericHTMLLeafElement,
                                     public nsIFormControl
{
public:
  nsGenericHTMLLeafFormElement();
  ~nsGenericHTMLLeafFormElement();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags);

  // nsIFormControl
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm,
                     PRBool aRemoveFromForm = PR_TRUE);
  NS_IMETHOD Init();

  // nsIContent
  NS_IMETHOD SetParent(nsIContent *aParent);
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);

  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAReadableString& aValue, PRBool aNotify);

  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
                     const nsAReadableString& aValue,
                     PRBool aNotify);


  NS_METHOD SetAttribute(const nsAReadableString& aName,
                         const nsAReadableString& aValue)
  {
    return nsGenericHTMLElement::SetAttribute(aName, aValue);
  }

protected:
  nsIForm* mForm;
};


//----------------------------------------------------------------------

/**
 * A macro to implement the getter and setter for a given string
 * valued content property. The method uses the generic SetAttr and
 * GetAttribute methods.
 */
#define NS_IMPL_STRING_ATTR(_class, _method, _atom)                  \
  NS_IMETHODIMP                                                      \
  _class::Get##_method(nsAWritableString& aValue)                    \
  {                                                                  \
    GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::_atom, aValue);          \
    return NS_OK;                                                    \
  }                                                                  \
  NS_IMETHODIMP                                                      \
  _class::Set##_method(const nsAReadableString& aValue)              \
  {                                                                  \
    return SetAttr(kNameSpaceID_HTML, nsHTMLAtoms::_atom, aValue,    \
                   PR_TRUE);                                         \
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
    nsresult rv;                                                      \
    rv = GetHTMLAttribute(nsHTMLAtoms::_atom, val);                   \
    *aValue = NS_CONTENT_ATTR_NOT_THERE != rv;                        \
    return NS_OK;                                                     \
  }                                                                   \
  NS_IMETHODIMP                                                       \
  _class::Set##_method(PRBool aValue)                                 \
  {                                                                   \
    nsHTMLValue empty(eHTMLUnit_Empty);                               \
    if (aValue) {                                                     \
      return SetHTMLAttribute(nsHTMLAtoms::_atom, empty, PR_TRUE);    \
    }                                                                 \
    else {                                                            \
      UnsetAttr(kNameSpaceID_HTML, nsHTMLAtoms::_atom, PR_TRUE);      \
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
        GetHTMLAttribute(nsHTMLAtoms::_atom, value)) {              \
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
    return SetHTMLAttribute(nsHTMLAtoms::_atom, value, PR_TRUE);    \
  }


/**
 * QueryInterface() implementation helper macros
 */

#define NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(_class, _base)                    \
  NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)    \
  {                                                                           \
    NS_ENSURE_ARG_POINTER(aInstancePtr);                                      \
                                                                              \
    *aInstancePtr = nsnull;                                                   \
                                                                              \
    nsresult rv;                                                              \
                                                                              \
    rv = _base::QueryInterface(aIID, aInstancePtr);                           \
                                                                              \
    if (NS_SUCCEEDED(rv))                                                     \
      return rv;                                                              \
                                                                              \
    rv = DOMQueryInterface(this, aIID, aInstancePtr);                         \
                                                                              \
    if (NS_SUCCEEDED(rv))                                                     \
      return rv;                                                              \
                                                                              \
    nsISupports *foundInterface = nsnull;


#define NS_HTML_CONTENT_INTERFACE_MAP_END                                     \
    {                                                                         \
      return NS_NOINTERFACE;                                                  \
    }                                                                         \
                                                                              \
    NS_ADDREF(foundInterface);                                                \
                                                                              \
    *aInstancePtr = foundInterface;                                           \
                                                                              \
    return NS_OK;                                                             \
  }



#endif /* nsGenericHTMLElement_h___ */
