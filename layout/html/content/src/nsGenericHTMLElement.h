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

#include "nsIDOMHTMLElement.h"
#include "nsIContent.h"
#include "nsHTMLValue.h"
#include "nsVoidArray.h"
#include "nsIJSScriptObject.h"

extern const nsIID kIDOMNodeIID;
extern const nsIID kIDOMElementIID;
extern const nsIID kIDOMHTMLElementIID;
extern const nsIID kIDOMEventReceiverIID;
extern const nsIID kIScriptObjectOwnerIID;
extern const nsIID kIJSScriptObjectIID;
extern const nsIID kISupportsIID;
extern const nsIID kIContentIID;
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

// There are a set of DOM- and scripting-specific instance variables
// that may only be instantiated when a content object is accessed
// through the DOM. Rather than burn actual slots in the content
// objects for each of these instance variables, we put them off
// in a side structure that's only allocated when the content is
// accessed through the DOM.
typedef struct {
  void *mScriptObject;
  nsChildContentList *mChildNodes;
  nsDOMCSSDeclaration *mStyle;
} nsDOMSlots;

class nsGenericHTMLElement : public nsIJSScriptObject {
public:
  nsGenericHTMLElement();
  ~nsGenericHTMLElement();

  void Init(nsIHTMLContent* aOuterContentObject, nsIAtom* aTag);

  // Implementation for nsIDOMNode
  nsresult    GetNodeName(nsString& aNodeName);
  nsresult    GetNodeValue(nsString& aNodeValue);
  nsresult    SetNodeValue(const nsString& aNodeValue);
  nsresult    GetNodeType(PRUint16* aNodeType);
  nsresult    GetParentNode(nsIDOMNode** aParentNode);
  nsresult    GetAttributes(nsIDOMNamedNodeMap** aAttributes);
  nsresult    GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  nsresult    GetNextSibling(nsIDOMNode** aNextSibling);
  nsresult    GetOwnerDocument(nsIDOMDocument** aOwnerDocument);

  // Implementation for nsIDOMElement
  nsresult    GetTagName(nsString& aTagName);
  nsresult    GetDOMAttribute(const nsString& aName, nsString& aReturn);
  nsresult    SetDOMAttribute(const nsString& aName, const nsString& aValue);
  nsresult    RemoveAttribute(const nsString& aName);
  nsresult    GetAttributeNode(const nsString& aName,
                               nsIDOMAttr** aReturn);
  nsresult    SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn);
  nsresult    RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn);
  nsresult    GetElementsByTagName(const nsString& aTagname,
                                   nsIDOMNodeList** aReturn);
  nsresult    Normalize();

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

  // nsIDOMEventReceiver interface
  nsresult AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID);
  nsresult RemoveEventListener(nsIDOMEventListener* aListener,
                               const nsIID& aIID);
  nsresult GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
  nsresult GetNewListenerManager(nsIEventListenerManager** aInstancePtrResult);

  // nsIScriptObjectOwner interface
  nsresult GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  nsresult SetScriptObject(void *aScriptObject);

  // Implementation for nsIContent
  nsresult GetDocument(nsIDocument*& aResult) const;
  nsresult SetDocument(nsIDocument* aDocument, PRBool aDeep);
  nsresult GetParent(nsIContent*& aResult) const;
  nsresult SetParent(nsIContent* aParent);
  nsresult IsSynthetic(PRBool& aResult) {
    aResult = PR_FALSE;
    return NS_OK;
  }
  nsresult GetTag(nsIAtom*& aResult) const;
  nsresult SetAttribute(const nsString& aName, const nsString& aValue,
                        PRBool aNotify);
  nsresult GetAttribute(const nsString& aName, nsString& aResult) const;
  nsresult List(FILE* out, PRInt32 aIndent) const;
  nsresult HandleDOMEvent(nsIPresContext& aPresContext,
                          nsEvent* aEvent,
                          nsIDOMEvent** aDOMEvent,
                          PRUint32 aFlags,
                          nsEventStatus& aEventStatus);

  // Implementation for nsIHTMLContent
  nsresult Compact();
  nsresult SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                        PRBool aNotify);
  nsresult SetAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue,
                        PRBool aNotify);
  nsresult UnsetAttribute(nsIAtom* aAttribute, PRBool aNotify);
  nsresult GetAttribute(nsIAtom *aAttribute, nsString &aResult) const;
  nsresult GetAttribute(nsIAtom* aAttribute, nsHTMLValue& aValue) const;
  nsresult GetAllAttributeNames(nsISupportsArray* aArray,
                                PRInt32& aCount) const;
  nsresult GetAttributeCount(PRInt32& aResult) const;
  nsresult SetID(nsIAtom* aID);
  nsresult GetID(nsIAtom*& aResult) const;
  nsresult SetClass(nsIAtom* aClass);
  nsresult GetClass(nsIAtom*& aResult) const;
  nsresult GetStyleRule(nsIStyleRule*& aResult);
  nsresult ToHTMLString(nsString& aResult) const;
  nsresult ToHTML(FILE* out) const;

  // Implementation for nsIJSScriptObject
  virtual PRBool    AddProperty(JSContext *aContext, jsval aID, jsval *aVp);
  virtual PRBool    DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp);
  virtual PRBool    GetProperty(JSContext *aContext, jsval aID, jsval *aVp);
  virtual PRBool    SetProperty(JSContext *aContext, jsval aID, jsval *aVp);
  virtual PRBool    EnumerateProperty(JSContext *aContext);
  virtual PRBool    Resolve(JSContext *aContext, jsval aID);
  virtual PRBool    Convert(JSContext *aContext, jsval aID);
  virtual void      Finalize(JSContext *aContext);

  // Implementation for nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID,
                            void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  //----------------------------------------

  nsresult RenderFrame();

  nsresult AddScriptEventListener(nsIAtom* aAttribute,
                                  const nsString& aValue,
                                  REFNSIID aIID);

  nsresult AttributeToString(nsIAtom* aAttribute,
                             nsHTMLValue& aValue,
                             nsString& aResult) const;

  void ListAttributes(FILE* out) const;

  void TriggerLink(nsIPresContext& aPresContext,
                   const nsString& aBase,
                   const nsString& aURLSpec,
                   const nsString& aTargetSpec,
                   PRBool aClick);

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

  static PRBool ParseValue(const nsString& aString, PRInt32 aMin,
                           nsHTMLValue& aResult, nsHTMLUnit aValueUnit);

  static PRBool ParseValue(const nsString& aString, PRInt32 aMin, PRInt32 aMax,
                           nsHTMLValue& aResult, nsHTMLUnit aValueUnit);

  static PRBool ParseColor(const nsString& aString, nsHTMLValue& aResult);

  static PRBool ColorToString(const nsHTMLValue& aValue,
                              nsString& aResult);

  static PRBool ParseAlignValue(const nsString& aString, nsHTMLValue& aResult);

  static PRBool ParseFormAlignValue(const nsString& aString,
                                    nsHTMLValue& aResult);

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

  static PRBool FormAlignValueToString(const nsHTMLValue& aValue,
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

  static nsresult GetScriptObjectFactory(nsIDOMScriptObjectFactory **aFactory);

  static nsIDOMScriptObjectFactory *gScriptObjectFactory;

  nsDOMSlots *GetDOMSlots();

  // Up pointer to the real content object that we are
  // supporting. Sometimes there is work that we just can't do
  // ourselves, so this is needed to ask the real object to do the
  // work.
  nsIHTMLContent* mContent;

  nsIDocument* mDocument;
  nsIContent* mParent;
  nsIHTMLAttributes* mAttributes;
  nsIAtom* mTag;
  nsIEventListenerManager* mListenerManager;
  nsDOMSlots *mDOMSlots;
};

//----------------------------------------------------------------------

class nsGenericHTMLLeafElement : public nsGenericHTMLElement {
public:
  nsGenericHTMLLeafElement();
  ~nsGenericHTMLLeafElement();

  nsresult CopyInnerTo(nsIHTMLContent* aSrcContent,
                       nsGenericHTMLLeafElement* aDest);

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
  nsresult SizeOf(nsISizeOfHandler* aHandler) const;
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

  nsresult CopyInnerTo(nsIHTMLContent* aSrcContent,
                       nsGenericHTMLContainerElement* aDest);

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
  nsresult SizeOf(nsISizeOfHandler* aHandler) const;
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
 * Mostly implement the nsIDOMNode API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 *
 * Note that classes using this macro will need to implement:
 *       NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);
 */
#define NS_IMPL_IDOMNODE_USING_GENERIC(_g)                              \
  NS_IMETHOD GetNodeName(nsString& aNodeName) {                         \
    return _g.GetNodeName(aNodeName);                                   \
  }                                                                     \
  NS_IMETHOD GetNodeValue(nsString& aNodeValue) {                       \
    return _g.GetNodeValue(aNodeValue);                                 \
  }                                                                     \
  NS_IMETHOD SetNodeValue(const nsString& aNodeValue) {                 \
    return _g.SetNodeValue(aNodeValue);                                 \
  }                                                                     \
  NS_IMETHOD GetNodeType(PRUint16* aNodeType) {                         \
    return _g.GetNodeType(aNodeType);                                   \
  }                                                                     \
  NS_IMETHOD GetParentNode(nsIDOMNode** aParentNode) {                  \
    return _g.GetParentNode(aParentNode);                               \
  }                                                                     \
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes) {              \
    return _g.GetChildNodes(aChildNodes);                               \
  }                                                                     \
  NS_IMETHOD HasChildNodes(PRBool* aHasChildNodes) {                    \
    return _g.HasChildNodes(aHasChildNodes);                            \
  }                                                                     \
  NS_IMETHOD GetFirstChild(nsIDOMNode** aFirstChild) {                  \
    return _g.GetFirstChild(aFirstChild);                               \
  }                                                                     \
  NS_IMETHOD GetLastChild(nsIDOMNode** aLastChild) {                    \
    return _g.GetLastChild(aLastChild);                                 \
  }                                                                     \
  NS_IMETHOD GetPreviousSibling(nsIDOMNode** aPreviousSibling) {        \
    return _g.GetPreviousSibling(aPreviousSibling);                     \
  }                                                                     \
  NS_IMETHOD GetNextSibling(nsIDOMNode** aNextSibling) {                \
    return _g.GetNextSibling(aNextSibling);                             \
  }                                                                     \
  NS_IMETHOD GetAttributes(nsIDOMNamedNodeMap** aAttributes) {          \
    return _g.GetAttributes(aAttributes);                               \
  }                                                                     \
  NS_IMETHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, \
                             nsIDOMNode** aReturn) {                    \
    return _g.InsertBefore(aNewChild, aRefChild, aReturn);              \
  }                                                                     \
  NS_IMETHOD AppendChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) { \
    return _g.AppendChild(aOldChild, aReturn);                          \
  }                                                                     \
  NS_IMETHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, \
                             nsIDOMNode** aReturn) {                    \
    return _g.ReplaceChild(aNewChild, aOldChild, aReturn);              \
  }                                                                     \
  NS_IMETHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) { \
    return _g.RemoveChild(aOldChild, aReturn);                          \
  }                                                                     \
  NS_IMETHOD GetOwnerDocument(nsIDOMDocument** aOwnerDocument) {        \
    return _g.GetOwnerDocument(aOwnerDocument);                         \
  }                                                                     \
  NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

/**
 * Implement the nsIDOMElement API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 */
#define NS_IMPL_IDOMELEMENT_USING_GENERIC(_g)                                 \
  NS_IMETHOD GetTagName(nsString& aTagName) {                                 \
    return _g.GetTagName(aTagName);                                           \
  }                                                                           \
  NS_IMETHOD GetDOMAttribute(const nsString& aName, nsString& aReturn) {      \
    return _g.GetDOMAttribute(aName, aReturn);                                \
  }                                                                           \
  NS_IMETHOD SetDOMAttribute(const nsString& aName, const nsString& aValue) { \
    return _g.SetDOMAttribute(aName, aValue);                                 \
  }                                                                           \
  NS_IMETHOD RemoveAttribute(const nsString& aName) {                         \
    return _g.RemoveAttribute(aName);                                         \
  }                                                                           \
  NS_IMETHOD GetAttributeNode(const nsString& aName,                          \
                              nsIDOMAttr** aReturn) {                         \
    return _g.GetAttributeNode(aName, aReturn);                               \
  }                                                                           \
  NS_IMETHOD SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn) {   \
    return _g.SetAttributeNode(aNewAttr, aReturn);                            \
  }                                                                           \
  NS_IMETHOD RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn) {\
    return _g.RemoveAttributeNode(aOldAttr, aReturn);                         \
  }                                                                           \
  NS_IMETHOD GetElementsByTagName(const nsString& aTagname,                   \
                                  nsIDOMNodeList** aReturn) {                 \
    return _g.GetElementsByTagName(aTagname, aReturn);                        \
  }                                                                           \
  NS_IMETHOD Normalize() {                                                    \
    return _g.Normalize();                                                    \
  }

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

/**
 * Implement the nsIDOMEventReceiver API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 */
#define NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(_g)                     \
  NS_IMETHOD AddEventListener(nsIDOMEventListener *aListener,           \
                              const nsIID& aIID) {                      \
    return _g.AddEventListener(aListener, aIID);                        \
  }                                                                     \
  NS_IMETHOD RemoveEventListener(nsIDOMEventListener *aListener,        \
                                 const nsIID& aIID) {                   \
    return _g.RemoveEventListener(aListener, aIID);                     \
  }                                                                     \
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aResult) {    \
    return _g.GetListenerManager(aResult);                              \
  }                                                                     \
  NS_IMETHOD GetNewListenerManager(nsIEventListenerManager** aResult) { \
    return _g.GetNewListenerManager(aResult);                           \
  }

/**
 * Implement the nsIScriptObjectOwner API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 */
#define NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(_g)     \
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, \
                             void** aScriptObject) {     \
    return _g.GetScriptObject(aContext, aScriptObject);  \
  }                                                      \
  NS_IMETHOD SetScriptObject(void *aScriptObject) {      \
    return _g.SetScriptObject(aScriptObject);            \
  }

#define NS_IMPL_ICONTENT_USING_GENERIC(_g)                                 \
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {                    \
    return _g.GetDocument(aResult);                                        \
  }                                                                        \
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep) {           \
    return _g.SetDocument(aDocument, aDeep);                               \
  }                                                                        \
  NS_IMETHOD GetParent(nsIContent*& aResult) const {                       \
    return _g.GetParent(aResult);                                          \
  }                                                                        \
  NS_IMETHOD SetParent(nsIContent* aParent) {                              \
    return _g.SetParent(aParent);                                          \
  }                                                                        \
  NS_IMETHOD CanContainChildren(PRBool& aResult) const {                   \
    return _g.CanContainChildren(aResult);                                 \
  }                                                                        \
  NS_IMETHOD ChildCount(PRInt32& aResult) const {                          \
    return _g.ChildCount(aResult);                                         \
  }                                                                        \
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const {         \
    return _g.ChildAt(aIndex, aResult);                                    \
  }                                                                        \
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const { \
    return _g.IndexOf(aPossibleChild, aResult);                            \
  }                                                                        \
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,               \
                           PRBool aNotify) {                               \
    return _g.InsertChildAt(aKid, aIndex, aNotify);                        \
  }                                                                        \
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,              \
                            PRBool aNotify) {                              \
    return _g.ReplaceChildAt(aKid, aIndex, aNotify);                       \
  }                                                                        \
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify) {             \
    return _g.AppendChildTo(aKid, aNotify);                                \
  }                                                                        \
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {               \
    return _g.RemoveChildAt(aIndex, aNotify);                              \
  }                                                                        \
  NS_IMETHOD IsSynthetic(PRBool& aResult) {                                \
    return _g.IsSynthetic(aResult);                                        \
  }                                                                        \
  NS_IMETHOD GetTag(nsIAtom*& aResult) const {                             \
    return _g.GetTag(aResult);                                             \
  }                                                                        \
  NS_IMETHOD SetAttribute(const nsString& aName, const nsString& aValue,   \
                          PRBool aNotify) {                                \
    return _g.SetAttribute(aName, aValue, aNotify);                        \
  }                                                                        \
  NS_IMETHOD GetAttribute(const nsString& aName,                           \
                          nsString& aResult) const {                       \
    return _g.GetAttribute(aName, aResult);                                \
  }                                                                        \
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const {                      \
    return _g.List(out, aIndent);                                          \
  }                                                                        \
  NS_IMETHOD BeginConvertToXIF(nsXIFConverter& aConverter) const {         \
    return _g.BeginConvertToXIF(aConverter);                               \
  }                                                                        \
  NS_IMETHOD ConvertContentToXIF(nsXIFConverter& aConverter) const {       \
    return _g.ConvertContentToXIF(aConverter);                             \
  }                                                                        \
  NS_IMETHOD FinishConvertToXIF(nsXIFConverter& aConverter) const {        \
    return _g.FinishConvertToXIF(aConverter);                              \
  }                                                                        \
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const {                    \
    return _g.SizeOf(aHandler);                                            \
  }                                                                        \
  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,                  \
                            nsEvent* aEvent,                               \
                            nsIDOMEvent** aDOMEvent,                       \
                            PRUint32 aFlags,                               \
                            nsEventStatus& aEventStatus);

#define NS_IMPL_IHTMLCONTENT_USING_GENERIC(_g)                         \
  NS_IMETHOD Compact() {                                               \
    return _g.Compact();                                               \
  }                                                                    \
  NS_IMETHOD SetAttribute(nsIAtom* aAttribute, const nsString& aValue, \
                          PRBool aNotify) {                            \
    return _g.SetAttribute(aAttribute, aValue, aNotify);               \
  }                                                                    \
  NS_IMETHOD SetAttribute(nsIAtom* aAttribute,                         \
                          const nsHTMLValue& aValue, PRBool aNotify) { \
    return _g.SetAttribute(aAttribute, aValue, aNotify);               \
  }                                                                    \
  NS_IMETHOD UnsetAttribute(nsIAtom* aAttribute, PRBool aNotify) {     \
    return _g.UnsetAttribute(aAttribute, aNotify);                     \
  }                                                                    \
  NS_IMETHOD GetAttribute(nsIAtom *aAttribute,                         \
                          nsString &aResult) const {                   \
    return _g.GetAttribute(aAttribute, aResult);                       \
  }                                                                    \
  NS_IMETHOD GetAttribute(nsIAtom* aAttribute,                         \
                          nsHTMLValue& aValue) const {                 \
    return _g.GetAttribute(aAttribute, aValue);                        \
  }                                                                    \
  NS_IMETHOD GetAllAttributeNames(nsISupportsArray* aArray,            \
                                  PRInt32& aResult) const {            \
    return _g.GetAllAttributeNames(aArray, aResult);                   \
  }                                                                    \
  NS_IMETHOD GetAttributeCount(PRInt32& aResult) const {               \
    return _g.GetAttributeCount(aResult);                              \
  }                                                                    \
  NS_IMETHOD  SetID(nsIAtom* aID) {                                    \
    return _g.SetID(aID);                                              \
  }                                                                    \
  NS_IMETHOD GetID(nsIAtom*& aResult) const {                          \
    return _g.GetID(aResult);                                          \
  }                                                                    \
  NS_IMETHOD SetClass(nsIAtom* aClass) {                               \
    return _g.SetClass(aClass);                                        \
  }                                                                    \
  NS_IMETHOD GetClass(nsIAtom*& aResult) const {                       \
    return _g.GetClass(aResult);                                       \
  }                                                                    \
  NS_IMETHOD GetStyleRule(nsIStyleRule*& aResult) {                    \
    return _g.GetStyleRule(aResult);                                   \
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
                               nsHTMLValue& aValue,                    \
                               nsString& aResult) const;               \
  NS_IMETHOD GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const;

#define NS_IMPL_IHTMLCONTENT_USING_GENERIC2(_g)                        \
  NS_IMETHOD Compact() {                                               \
    return _g.Compact();                                               \
  }                                                                    \
  NS_IMETHOD SetAttribute(nsIAtom* aAttribute, const nsString& aValue, \
                          PRBool aNotify) {                            \
    return _g.SetAttribute(aAttribute, aValue, aNotify);               \
  }                                                                    \
  NS_IMETHOD SetAttribute(nsIAtom* aAttribute,                         \
                          const nsHTMLValue& aValue, PRBool aNotify) { \
    return _g.SetAttribute(aAttribute, aValue, aNotify);               \
  }                                                                    \
  NS_IMETHOD UnsetAttribute(nsIAtom* aAttribute, PRBool aNotify) {     \
    return _g.UnsetAttribute(aAttribute, aNotify);                     \
  }                                                                    \
  NS_IMETHOD GetAttribute(nsIAtom *aAttribute,                         \
                          nsString &aResult) const {                   \
    return _g.GetAttribute(aAttribute, aResult);                       \
  }                                                                    \
  NS_IMETHOD GetAttribute(nsIAtom* aAttribute,                         \
                          nsHTMLValue& aValue) const {                 \
    return _g.GetAttribute(aAttribute, aValue);                        \
  }                                                                    \
  NS_IMETHOD GetAllAttributeNames(nsISupportsArray* aArray,            \
                                  PRInt32& aResult) const {            \
    return _g.GetAllAttributeNames(aArray, aResult);                   \
  }                                                                    \
  NS_IMETHOD GetAttributeCount(PRInt32& aResult) const {               \
    return _g.GetAttributeCount(aResult);                              \
  }                                                                    \
  NS_IMETHOD  SetID(nsIAtom* aID) {                                    \
    return _g.SetID(aID);                                              \
  }                                                                    \
  NS_IMETHOD GetID(nsIAtom*& aResult) const {                          \
    return _g.GetID(aResult);                                          \
  }                                                                    \
  NS_IMETHOD SetClass(nsIAtom* aClass) {                               \
    return _g.SetClass(aClass);                                        \
  }                                                                    \
  NS_IMETHOD GetClass(nsIAtom*& aResult) const {                       \
    return _g.GetClass(aResult);                                       \
  }                                                                    \
  NS_IMETHOD GetStyleRule(nsIStyleRule*& aResult);                     \
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
                               nsHTMLValue& aValue,                    \
                               nsString& aResult) const;               \
  NS_IMETHOD GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const;

/**
 * This macro implements the portion of query interface that is
 * generic to all html content objects.
 */
#define NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(_id, _iptr, _this) \
  if (_id.Equals(kISupportsIID)) {                              \
    nsIHTMLContent* tmp = _this;                                \
    nsISupports* tmp2 = tmp;                                    \
    *_iptr = (void*) tmp2;                                      \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIDOMNodeIID)) {                               \
    nsIDOMNode* tmp = _this;                                    \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIDOMElementIID)) {                            \
    nsIDOMElement* tmp = _this;                                 \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIDOMHTMLElementIID)) {                        \
    nsIDOMHTMLElement* tmp = _this;                             \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIDOMEventReceiverIID)) {                      \
    nsIDOMEventReceiver* tmp = _this;                           \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIScriptObjectOwnerIID)) {                     \
    nsIScriptObjectOwner* tmp = _this;                          \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIContentIID)) {                               \
    nsIContent* tmp = _this;                                    \
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
  if (_id.Equals(kIJSScriptObjectIID)) {                        \
    nsIJSScriptObject* tmp = (nsIJSScriptObject*)&mInner;       \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             

/**
 * A macro to implement the getter and setter for a given string
 * valued content property. The method uses the generic SetAttr and
 * GetAttribute methods.
 */
#define NS_IMPL_STRING_ATTR(_class, _method, _atom, _notify)         \
  NS_IMETHODIMP                                                      \
  _class::Get##_method(nsString& aValue)                             \
  {                                                                  \
    mInner.GetAttribute(nsHTMLAtoms::_atom, aValue);                 \
    return NS_OK;                                                    \
  }                                                                  \
  NS_IMETHODIMP                                                      \
  _class::Set##_method(const nsString& aValue)                       \
  {                                                                  \
    return mInner.SetAttribute(nsHTMLAtoms::_atom, aValue, PR_TRUE); \
  }

/**
 * A macro to implement the getter and setter for a given boolean
 * valued content property. The method uses the generic SetAttr and
 * GetAttribute methods.
 */
#define NS_IMPL_BOOL_ATTR(_class, _method, _atom, _notify)            \
  NS_IMETHODIMP                                                       \
  _class::Get##_method(PRBool* aValue)                                \
  {                                                                   \
    nsHTMLValue val;                                                  \
    nsresult rv = mInner.GetAttribute(nsHTMLAtoms::_atom, val);       \
    *aValue = NS_CONTENT_ATTR_NOT_THERE != rv;                        \
    return NS_OK;                                                     \
  }                                                                   \
  NS_IMETHODIMP                                                       \
  _class::Set##_method(PRBool aValue)                                 \
  {                                                                   \
    nsAutoString empty;                                               \
    if (aValue) {                                                     \
      return mInner.SetAttribute(nsHTMLAtoms::_atom, empty, PR_TRUE); \
    }                                                                 \
    else {                                                            \
      mInner.UnsetAttribute(nsHTMLAtoms::_atom, PR_TRUE);             \
      return NS_OK;                                                   \
    }                                                                 \
  }

/**
 * A macro to implement the getter and setter for a given integer
 * valued content property. The method uses the generic SetAttr and
 * GetAttribute methods.
 */
#define NS_IMPL_INT_ATTR(_class, _method, _atom, _notify)           \
  NS_IMETHODIMP                                                     \
  _class::Get##_method(PRInt32* aValue)                             \
  {                                                                 \
    nsHTMLValue value;                                              \
    *aValue = -1;                                                   \
    if (NS_CONTENT_ATTR_HAS_VALUE ==                                \
        mInner.GetAttribute(nsHTMLAtoms::_atom, value)) {           \
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
    return mInner.SetAttribute(nsHTMLAtoms::_atom, value, PR_TRUE); \
  }

#endif /* nsGenericHTMLElement_h___ */
