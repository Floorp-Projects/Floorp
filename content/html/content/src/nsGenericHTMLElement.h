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
class nsIScrollableView;
struct nsRect;


class nsGenericHTMLElement : public nsGenericElement
{
public:
  nsGenericHTMLElement();
  virtual ~nsGenericHTMLElement();

#ifdef GATHER_ELEMENT_USEAGE_STATISTICS
  nsresult Init(nsINodeInfo *aNodeInfo);
#endif

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
  nsresult GetId(nsAWritableString& aId);
  nsresult SetId(const nsAReadableString& aId);
  nsresult GetTitle(nsAWritableString& aTitle);
  nsresult SetTitle(const nsAReadableString& aTitle);
  nsresult GetLang(nsAWritableString& aLang);
  nsresult SetLang(const nsAReadableString& aLang);
  nsresult GetDir(nsAWritableString& aDir);
  nsresult SetDir(const nsAReadableString& aDir);
  nsresult GetClassName(nsAWritableString& aClassName);
  nsresult SetClassName(const nsAReadableString& aClassName);
  nsresult GetStyle(nsIDOMCSSStyleDeclaration** aStyle);
  nsresult GetOffsetTop(PRInt32* aOffsetTop);
  nsresult GetOffsetLeft(PRInt32* aOffsetLeft);
  nsresult GetOffsetWidth(PRInt32* aOffsetWidth);
  nsresult GetOffsetHeight(PRInt32* aOffsetHeight);
  nsresult GetOffsetParent(nsIDOMElement** aOffsetParent);
  nsresult GetInnerHTML(nsAWritableString& aInnerHTML);
  nsresult SetInnerHTML(const nsAReadableString& aInnerHTML);
  nsresult GetScrollTop(PRInt32* aScrollTop);
  nsresult SetScrollTop(PRInt32 aScrollTop);
  nsresult GetScrollLeft(PRInt32* aScrollLeft);
  nsresult SetScrollLeft(PRInt32 aScrollLeft);
  nsresult GetScrollHeight(PRInt32* aScrollHeight);
  nsresult GetScrollWidth(PRInt32* aScrollWidth);
  nsresult GetClientHeight(PRInt32* aClientHeight);
  nsresult GetClientWidth(PRInt32* aClientWidth);
  nsresult ScrollIntoView(PRBool aTop);

  nsresult GetOffsetRect(nsRect& aRect, 
                         nsIAtom* aOffsetParentTag,
                         nsIContent** aOffsetParent);
  nsresult GetScrollInfo(nsIScrollableView **aScrollableView, float *aP2T,
                         float *aT2P);

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
#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const;
#endif
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
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  NS_IMETHOD WalkInlineStyleRules(nsRuleWalker* aRuleWalker);
  NS_IMETHOD GetBaseURL(nsIURI*& aBaseURL) const;
  NS_IMETHOD GetBaseTarget(nsAWritableString& aBaseTarget) const;

  //----------------------------------------
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;

#ifdef DEBUG
  void ListAttributes(FILE* out) const;

  PRUint32 BaseSizeOf(nsISizeOfHandler* aSizer) const;
#endif



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

#define NS_HTML_CONTENT_INTERFACE_MAP_AMBIGOUS_BEGIN(_class, _base, _base_if) \
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
    rv = DOMQueryInterface(NS_STATIC_CAST(_base_if *, this), aIID,            \
                           aInstancePtr);                                     \
                                                                              \
    if (NS_SUCCEEDED(rv))                                                     \
      return rv;                                                              \
                                                                              \
    nsISupports *foundInterface = nsnull;


#define NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(_class, _base)                    \
  NS_HTML_CONTENT_INTERFACE_MAP_AMBIGOUS_BEGIN(_class, _base,                 \
                                               nsIDOMHTMLElement)


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


#define NS_INTERFACE_MAP_ENTRY_IF_TAG(_interface, _tag)                       \
  if (mNodeInfo->Equals(nsHTMLAtoms::_tag) &&                                 \
      aIID.Equals(NS_GET_IID(_interface)))                                    \
    foundInterface = NS_STATIC_CAST(_interface *, this);                      \
  else


#define NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(_class, _tag)         \
  if (mNodeInfo->Equals(nsHTMLAtoms::_tag) &&                                 \
      aIID.Equals(NS_GET_IID(nsIClassInfo))) {                                \
    foundInterface =                                                          \
      nsContentUtils::GetClassInfoInstance(eDOMClassInfo_##_class##_id);      \
    NS_ENSURE_TRUE(foundInterface, NS_ERROR_OUT_OF_MEMORY);                   \
                                                                              \
    *aInstancePtr = foundInterface;                                           \
                                                                              \
    return NS_OK;                                                             \
  } else



// Element class factory methods

nsresult
NS_NewHTMLSharedLeafElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLAnchorElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLAppletElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLAreaElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLBRElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLBaseElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLSharedLeafElement(aResult, aNodeInfo);
}

nsresult
NS_NewHTMLBaseFontElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLBodyElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLButtonElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLDListElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLDelElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLDirectoryElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLDivElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLEmbedElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLSharedLeafElement(aResult, aNodeInfo);
}

nsresult
NS_NewHTMLFieldSetElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLFontElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLFormElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLFrameElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLFrameSetElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLHRElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLHeadElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLHeadingElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLHtmlElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLIFrameElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLImageElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLInputElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLInsElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLIsIndexElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLSharedLeafElement(aResult, aNodeInfo);
}

nsresult
NS_NewHTMLLIElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLLabelElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLLegendElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLLinkElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLMapElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLMenuElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLMetaElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLModElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLOListElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLObjectElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLOptGroupElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLOptionElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLParagraphElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLParamElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLSharedLeafElement(aResult, aNodeInfo);
}

nsresult
NS_NewHTMLPreElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLQuoteElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLScriptElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLSelectElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLSpacerElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLSharedLeafElement(aResult, aNodeInfo);
}

nsresult
NS_NewHTMLSpanElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLStyleElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTableCaptionElement(nsIHTMLContent** aResult,nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTableCellElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTableColElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLTableColGroupElement(nsIHTMLContent** aResult,
                               nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLTableColElement(aResult, aNodeInfo);
}

nsresult
NS_NewHTMLTableElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTableRowElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTableSectionElement(nsIHTMLContent** aResult,nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTbodyElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTextAreaElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTfootElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTheadElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTitleElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLUListElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLUnknownElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLWBRElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLSharedLeafElement(aResult, aNodeInfo);
}


#endif /* nsGenericHTMLElement_h___ */
