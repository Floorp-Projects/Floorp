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
#ifndef nsHTMLGenericContent_h___
#define nsHTMLGenericContent_h___

#include "nsIDOMHTMLElement.h"
#include "nsIContent.h"
#include "nsHTMLValue.h"
#include "nsVoidArray.h"

extern const nsIID kIDOMNodeIID;
extern const nsIID kIDOMElementIID;
extern const nsIID kIDOMHTMLElementIID;
extern const nsIID kIDOMEventReceiverIID;
extern const nsIID kIScriptObjectOwnerIID;
extern const nsIID kISupportsIID;
extern const nsIID kIContentIID;
extern const nsIID kIHTMLContentIID;

class nsIDOMAttribute;
class nsIDOMEventListener;
class nsIDOMNodeList;
class nsIEventListenerManager;
class nsIFrame;
class nsIHTMLAttributes;
class nsIHTMLContent;
class nsIStyleContext;
class nsIStyleRule;
class nsISupportsArray;

struct nsHTMLGenericContent {
  nsHTMLGenericContent();
  ~nsHTMLGenericContent();

  void Init(nsIHTMLContent* aOuterContentObject, nsIAtom* aTag);

  // Implementation for nsIDOMNode
  nsresult    GetNodeName(nsString& aNodeName);
  nsresult    GetNodeValue(nsString& aNodeValue);
  nsresult    SetNodeValue(const nsString& aNodeValue);
  nsresult    GetNodeType(PRInt32* aNodeType);
  nsresult    GetParentNode(nsIDOMNode** aParentNode);
  nsresult    GetAttributes(nsIDOMNamedNodeMap** aAttributes);
  nsresult    GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  nsresult    GetNextSibling(nsIDOMNode** aNextSibling);

  // Implementation for nsIDOMElement
  nsresult    GetTagName(nsString& aTagName);
  nsresult    GetDOMAttribute(const nsString& aName, nsString& aReturn);
  nsresult    SetDOMAttribute(const nsString& aName, const nsString& aValue);
  nsresult    RemoveAttribute(const nsString& aName);
  nsresult    GetAttributeNode(const nsString& aName,
                               nsIDOMAttribute** aReturn);
  nsresult    SetAttributeNode(nsIDOMAttribute* aNewAttr);
  nsresult    RemoveAttributeNode(nsIDOMAttribute* aOldAttr);
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

  // nsIDOMEventReceiver interface
  nsresult AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID);
  nsresult RemoveEventListener(nsIDOMEventListener* aListener,
                               const nsIID& aIID);
  nsresult GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
  nsresult GetNewListenerManager(nsIEventListenerManager** aInstancePtrResult);

  // nsIScriptObjectOwner interface
  nsresult GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  nsresult ResetScriptObject();

  // Implementation for nsIContent
  nsresult GetDocument(nsIDocument*& aResult) const;
  void SetDocument(nsIDocument* aDocument);
  nsIContent* GetParent() const;
  void SetParent(nsIContent* aParent);
  nsresult IsSynthetic(PRBool& aResult);
  nsIAtom* GetTag() const;
  void SetAttribute(const nsString& aName, const nsString& aValue);
  nsContentAttr GetAttribute(const nsString& aName, nsString& aResult) const;
  nsIContentDelegate* GetDelegate(nsIPresContext* aCX);
  void List(FILE* out, PRInt32 aIndent) const;
  nsresult HandleDOMEvent(nsIPresContext& aPresContext,
                          nsEvent* aEvent,
                          nsIDOMEvent** aDOMEvent,
                          PRUint32 aFlags,
                          nsEventStatus& aEventStatus);

  // Implementation for nsIHTMLContent
  void Compact();
  void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);
  void SetAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue);
  void UnsetAttribute(nsIAtom* aAttribute);
  nsContentAttr GetAttribute(nsIAtom *aAttribute,
                             nsString &aResult) const;
  nsContentAttr GetAttribute(nsIAtom* aAttribute,
                             nsHTMLValue& aValue) const;
  PRInt32 GetAllAttributeNames(nsISupportsArray* aArray) const;
  PRInt32 GetAttributeCount(void) const;
  void SetID(nsIAtom* aID);
  nsIAtom* GetID(void) const;
  void SetClass(nsIAtom* aClass);
  nsIAtom* GetClass(void) const;
  nsIStyleRule* GetStyleRule(void);
  void MapAttributesInto(nsIStyleContext* aStyleContext,
                         nsIPresContext* aPresContext);
  void ToHTMLString(nsString& aResult) const;
  void ToHTML(FILE* out) const;
  nsresult CreateFrame(nsIPresContext*  aPresContext,
                       nsIFrame*        aParentFrame,
                       nsIStyleContext* aStyleContext,
                       nsIFrame*&       aResult);

  //----------------------------------------

  nsresult AddScriptEventListener(nsIAtom* aAttribute,
                                  nsHTMLValue& aValue,
                                  REFNSIID aIID);

  nsContentAttr AttributeToString(nsIAtom* aAttribute,
                                  nsHTMLValue& aValue,
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

  // Up pointer to the real content object that we are
  // supporting. Sometimes there is work that we just can't do
  // ourselves, so this is needed to ask the real object to do the
  // work.
  nsIHTMLContent* mContent;

  nsIDocument* mDocument;
  nsIContent* mParent;
  nsIHTMLAttributes* mAttributes;
  nsIAtom* mTag;
  void* mScriptObject;
  nsIEventListenerManager* mListenerManager;
};

//----------------------------------------------------------------------

struct nsHTMLGenericLeafContent : public nsHTMLGenericContent {
  nsHTMLGenericLeafContent();
  ~nsHTMLGenericLeafContent();

  nsresult CopyInnerTo(nsIHTMLContent* aSrcContent,
                       nsHTMLGenericLeafContent* aDest);

  // Remainder of nsIDOMHTMLElement (and nsIDOMNode)
  nsresult    Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn);
  nsresult    GetChildNodes(nsIDOMNodeList** aChildNodes) {
    *aChildNodes = nsnull;
    return NS_OK;
  }
  nsresult    GetHasChildNodes(PRBool* aHasChildNodes) {
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
  void BeginConvertToXIF(nsXIFConverter& aConverter) const;
  void ConvertContentToXIF(nsXIFConverter& aConverter) const;
  void FinishConvertToXIF(nsXIFConverter& aConverter) const;
  void Compact() {
  }
  PRBool CanContainChildren() const {
    return PR_FALSE;
  }
  PRInt32 ChildCount() const {
    return 0;
  }
  nsIContent* ChildAt(PRInt32 aIndex) const {
    return nsnull;
  }
  PRInt32 IndexOf(nsIContent* aPossibleChild) const {
    return -1;
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

struct nsHTMLGenericContainerContent : public nsHTMLGenericContent {
  nsHTMLGenericContainerContent();
  ~nsHTMLGenericContainerContent();

  nsresult CopyInnerTo(nsIHTMLContent* aSrcContent,
                       nsHTMLGenericLeafContent* aDest);

  // Remainder of nsIDOMHTMLElement (and nsIDOMNode)
  nsresult    Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn);
  nsresult    GetChildNodes(nsIDOMNodeList** aChildNodes);
  nsresult    GetHasChildNodes(PRBool* aHasChildNodes);
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
  void BeginConvertToXIF(nsXIFConverter& aConverter) const;
  void ConvertContentToXIF(nsXIFConverter& aConverter) const;
  void FinishConvertToXIF(nsXIFConverter& aConverter) const;
  void Compact();
  PRBool CanContainChildren() const;
  PRInt32 ChildCount() const;
  nsIContent* ChildAt(PRInt32 aIndex) const;
  PRInt32 IndexOf(nsIContent* aPossibleChild) const;
  nsresult InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  nsresult ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify);
  nsresult RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

  nsVoidArray mChildren;
};

//----------------------------------------------------------------------

/**
 * Mostly implement the nsIDOMNode API by forwarding the methods to a
 * generic content object (either nsHTMLGenericLeafContent or
 * nsHTMLGenericContainerContent)
 *
 * Note that classes using this macro will need to implement:
 *       NS_IMETHOD CloneNode(nsIDOMNode** aReturn);
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
  NS_IMETHOD GetNodeType(PRInt32* aNodeType) {                          \
    return _g.GetNodeType(aNodeType);                                   \
  }                                                                     \
  NS_IMETHOD GetParentNode(nsIDOMNode** aParentNode) {                  \
    return _g.GetParentNode(aParentNode);                               \
  }                                                                     \
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes) {              \
    return _g.GetChildNodes(aChildNodes);                               \
  }                                                                     \
  NS_IMETHOD GetHasChildNodes(PRBool* aHasChildNodes) {                 \
    return _g.GetHasChildNodes(aHasChildNodes);                         \
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
  NS_IMETHOD Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn) { \
    return _g.Equals(aNode, aDeep, aReturn);                            \
  }                                                                     \
  NS_IMETHOD CloneNode(nsIDOMNode** aReturn);

/**
 * Implement the nsIDOMElement API by forwarding the methods to a
 * generic content object (either nsHTMLGenericLeafContent or
 * nsHTMLGenericContainerContent)
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
                              nsIDOMAttribute** aReturn) {                    \
    return _g.GetAttributeNode(aName, aReturn);                               \
  }                                                                           \
  NS_IMETHOD SetAttributeNode(nsIDOMAttribute* aNewAttr) {                    \
    return _g.SetAttributeNode(aNewAttr);                                     \
  }                                                                           \
  NS_IMETHOD RemoveAttributeNode(nsIDOMAttribute* aOldAttr) {                 \
    return _g.RemoveAttributeNode(aOldAttr);                                  \
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
 * generic content object (either nsHTMLGenericLeafContent or
 * nsHTMLGenericContainerContent)
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
  }

/**
 * Implement the nsIDOMEventReceiver API by forwarding the methods to a
 * generic content object (either nsHTMLGenericLeafContent or
 * nsHTMLGenericContainerContent)
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
 * generic content object (either nsHTMLGenericLeafContent or
 * nsHTMLGenericContainerContent)
 */
#define NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(_g)     \
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, \
                             void** aScriptObject) {     \
    return _g.GetScriptObject(aContext, aScriptObject);  \
  }                                                      \
  NS_IMETHOD ResetScriptObject() {                       \
    return _g.ResetScriptObject();                       \
  }

#define NS_IMPL_ICONTENT_USING_GENERIC(_g)                                   \
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {                      \
    return _g.GetDocument(aResult);                                          \
  }                                                                          \
  virtual void SetDocument(nsIDocument* aDocument) {                         \
    _g.SetDocument(aDocument);                                               \
  }                                                                          \
  virtual nsIContent* GetParent() const {                                    \
    return _g.GetParent();                                                   \
  }                                                                          \
  virtual void SetParent(nsIContent* aParent) {                              \
    _g.SetParent(aParent);                                                   \
  }                                                                          \
  virtual PRBool CanContainChildren() const {                                \
    return _g.CanContainChildren();                                          \
  }                                                                          \
  virtual PRInt32 ChildCount() const {                                       \
    return _g.ChildCount();                                                  \
  }                                                                          \
  virtual nsIContent* ChildAt(PRInt32 aIndex) const {                        \
    return _g.ChildAt(aIndex);                                               \
  }                                                                          \
  virtual PRInt32 IndexOf(nsIContent* aPossibleChild) const {                \
    return _g.IndexOf(aPossibleChild);                                       \
  }                                                                          \
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,                 \
                           PRBool aNotify) {                                 \
    return _g.InsertChildAt(aKid, aIndex, aNotify);                          \
  }                                                                          \
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,                \
                            PRBool aNotify) {                                \
    return _g.ReplaceChildAt(aKid, aIndex, aNotify);                         \
  }                                                                          \
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify) {               \
    return _g.AppendChildTo(aKid, aNotify);                                  \
  }                                                                          \
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {                 \
    return _g.RemoveChildAt(aIndex, aNotify);                                \
  }                                                                          \
  NS_IMETHOD IsSynthetic(PRBool& aResult) {                                  \
    return _g.IsSynthetic(aResult);                                          \
  }                                                                          \
  virtual nsIAtom* GetTag() const {                                          \
    return _g.GetTag();                                                      \
  }                                                                          \
  virtual void SetAttribute(const nsString& aName, const nsString& aValue) { \
    _g.SetAttribute(aName, aValue);                                          \
  }                                                                          \
  virtual nsContentAttr GetAttribute(const nsString& aName,                  \
                                     nsString& aResult) const {              \
    return _g.GetAttribute(aName, aResult);                                  \
  }                                                                          \
  virtual nsIContentDelegate* GetDelegate(nsIPresContext* aCX) {             \
    return _g.GetDelegate(aCX);                                              \
  }                                                                          \
  virtual void List(FILE* out, PRInt32 aIndent) const {                      \
    _g.List(out, aIndent);                                                   \
  }                                                                          \
  virtual void BeginConvertToXIF(nsXIFConverter& aConverter) const {         \
    _g.BeginConvertToXIF(aConverter);                                        \
  }                                                                          \
  virtual void ConvertContentToXIF(nsXIFConverter& aConverter) const {       \
    _g.ConvertContentToXIF(aConverter);                                      \
  }                                                                          \
  virtual void FinishConvertToXIF(nsXIFConverter& aConverter) const {        \
    _g.FinishConvertToXIF(aConverter);                                       \
  }                                                                          \
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const {                      \
    return _g.SizeOf(aHandler);                                              \
  }                                                                          \
  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,                    \
                            nsEvent* aEvent,                                 \
                            nsIDOMEvent** aDOMEvent,                         \
                            PRUint32 aFlags,                                 \
                            nsEventStatus& aEventStatus) {                   \
    return _g.HandleDOMEvent(aPresContext, aEvent, aDOMEvent, aFlags,        \
                             aEventStatus);                                  \
  }

#define NS_IMPL_IHTMLCONTENT_USING_GENERIC(_g)                             \
  virtual void Compact() {                                                 \
    _g.Compact();                                                          \
  }                                                                        \
  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue) { \
    _g.SetAttribute(aAttribute, aValue);                                   \
  }                                                                        \
  virtual void SetAttribute(nsIAtom* aAttribute,                           \
                            const nsHTMLValue& aValue) {                   \
    _g.SetAttribute(aAttribute, aValue);                                   \
  }                                                                        \
  virtual void UnsetAttribute(nsIAtom* aAttribute) {                       \
    _g.UnsetAttribute(aAttribute);                                         \
  }                                                                        \
  virtual nsContentAttr GetAttribute(nsIAtom *aAttribute,                  \
                                     nsString &aResult) const {            \
    return _g.GetAttribute(aAttribute, aResult);                           \
  }                                                                        \
  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,                  \
                                     nsHTMLValue& aValue) const {          \
    return _g.GetAttribute(aAttribute, aValue);                            \
  }                                                                        \
  virtual PRInt32 GetAllAttributeNames(nsISupportsArray* aArray) const {   \
    return _g.GetAllAttributeNames(aArray);                                \
  }                                                                        \
  virtual PRInt32 GetAttributeCount(void) const {                          \
    return _g.GetAttributeCount();                                         \
  }                                                                        \
  virtual void SetID(nsIAtom* aID) {                                       \
    _g.SetID(aID);                                                         \
  }                                                                        \
  virtual nsIAtom* GetID() const {                                         \
    return _g.GetID();                                                     \
  }                                                                        \
  virtual void SetClass(nsIAtom* aClass) {                                 \
    _g.SetClass(aClass);                                                   \
  }                                                                        \
  virtual nsIAtom* GetClass() const {                                      \
    return _g.GetClass();                                                  \
  }                                                                        \
  virtual nsIStyleRule* GetStyleRule() {                                   \
    return _g.GetStyleRule();                                              \
  }                                                                        \
  virtual void ToHTMLString(nsString& aResult) const {                     \
    _g.ToHTMLString(aResult);                                              \
  }                                                                        \
  virtual void ToHTML(FILE* out) const {                                   \
    _g.ToHTML(out);                                                        \
  }                                                                        \
  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,              \
                               nsIFrame*        aParentFrame,              \
                               nsIStyleContext* aStyleContext,             \
                               nsIFrame*&       aResult) {                 \
    return _g.CreateFrame(aPresContext, aParentFrame, aStyleContext,       \
                          aResult);                                        \
  }                                                                        \
  virtual nsContentAttr StringToAttribute(nsIAtom* aAttribute,             \
                                          const nsString& aValue,          \
                                          nsHTMLValue& aResult);           \
  virtual nsContentAttr AttributeToString(nsIAtom* aAttribute,             \
                                          nsHTMLValue& aValue,             \
                                          nsString& aResult) const;        \
  virtual void MapAttributesInto(nsIStyleContext* aContext,                \
                                 nsIPresContext* aPresContext);

/**
 * This macro implements the portion of query interface that is
 * generic to all html content objects.
 */
#define NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(_id, _iptr, _this) \
  if (_id.Equals(kISupportsIID)) {                              \
    nsIHTMLContent* tmp = _this;                                \
    nsISupports* tmp2 = tmp;                                    \
    *_iptr = (void*) tmp2;                                      \
    mRefCnt++;                                                  \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIDOMNodeIID)) {                               \
    nsIDOMNode* tmp = _this;                                    \
    *_iptr = (void*) tmp;                                       \
    mRefCnt++;                                                  \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIDOMElementIID)) {                            \
    nsIDOMElement* tmp = _this;                                 \
    *_iptr = (void*) tmp;                                       \
    mRefCnt++;                                                  \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIDOMHTMLElementIID)) {                        \
    nsIDOMHTMLElement* tmp = _this;                             \
    *_iptr = (void*) tmp;                                       \
    mRefCnt++;                                                  \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIDOMEventReceiverIID)) {                      \
    nsIDOMEventReceiver* tmp = _this;                           \
    *_iptr = (void*) tmp;                                       \
    mRefCnt++;                                                  \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIScriptObjectOwnerIID)) {                     \
    nsIScriptObjectOwner* tmp = _this;                          \
    *_iptr = (void*) tmp;                                       \
    mRefCnt++;                                                  \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIContentIID)) {                               \
    nsIContent* tmp = _this;                                    \
    *_iptr = (void*) tmp;                                       \
    mRefCnt++;                                                  \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(kIHTMLContentIID)) {                           \
    nsIHTMLContent* tmp = _this;                                \
    *_iptr = (void*) tmp;                                       \
    mRefCnt++;                                                  \
    return NS_OK;                                               \
  }

#endif /* nsHTMLLeafContent_h___ */
