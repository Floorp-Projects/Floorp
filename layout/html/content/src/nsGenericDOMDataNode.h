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
#ifndef nsGenericDOMDataNode_h___
#define nsGenericDOMDataNode_h___

#include "nsIDOMData.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsTextFragment.h"
#include "nsHTMLValue.h"
#include "nsVoidArray.h"

extern const nsIID kIDOMDataIID;
extern const nsIID kIDOMNodeIID;
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

struct nsGenericDOMDataNode {
  nsGenericDOMDataNode();
  ~nsGenericDOMDataNode();

  void Init(nsIHTMLContent* aOuterContentObject);

  // Implementation for nsIDOMNode
  nsresult    GetNodeName(nsString& aNodeName) {
    aNodeName.Truncate();
    return NS_OK;
  }
  nsresult    GetNodeValue(nsString& aNodeValue);
  nsresult    SetNodeValue(const nsString& aNodeValue);
  nsresult    GetParentNode(nsIDOMNode** aParentNode);
  nsresult    GetAttributes(nsIDOMNamedNodeMap** aAttributes) {
    *aAttributes = nsnull;
    return NS_OK;
  }
  nsresult    GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  nsresult    GetNextSibling(nsIDOMNode** aNextSibling);
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

  // Implementation for nsIDOMData
  nsresult GetData(nsString& aData);
  nsresult SetData(const nsString& aData);
  nsresult GetSize(PRUint32* aSize);
  nsresult Substring(PRUint32 aStart, PRUint32 aEnd, nsString& aReturn);
  nsresult Append(const nsString& aData);
  nsresult Insert(PRUint32 aOffset, const nsString& aData);
  nsresult Remove(PRUint32 aOffset, PRUint32 aCount);
  nsresult Replace(PRUint32 aOffset, PRUint32 aCount, const nsString& aData);

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
  nsresult SetDocument(nsIDocument* aDocument);
  nsresult GetParent(nsIContent*& aResult) const;
  nsresult SetParent(nsIContent* aParent);
  nsresult IsSynthetic(PRBool& aResult) {
    aResult = PR_FALSE;
    return NS_OK;
  }
  nsresult GetTag(nsIAtom*& aResult) const {
    aResult = nsnull;
    return NS_OK;
  }
  nsresult SetAttribute(const nsString& aName, const nsString& aValue,
                        PRBool aNotify) {
    return NS_OK;
  }
  nsresult GetAttribute(const nsString& aName, nsString& aResult) const {
    return NS_CONTENT_ATTR_NOT_THERE;
  }
  nsresult List(FILE* out, PRInt32 aIndent) const;
  nsresult HandleDOMEvent(nsIPresContext& aPresContext,
                          nsEvent* aEvent,
                          nsIDOMEvent** aDOMEvent,
                          PRUint32 aFlags,
                          nsEventStatus& aEventStatus);

  // Implementation for nsIHTMLContent
  nsresult Compact();
  nsresult SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                        PRBool aNotify) {
    return NS_OK;
  }
  nsresult SetAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue,
                        PRBool aNotify) {
    return NS_OK;
  }
  nsresult UnsetAttribute(nsIAtom* aAttribute, PRBool aNotify) {
    return NS_OK;
  }
  nsresult GetAttribute(nsIAtom *aAttribute, nsString &aResult) const {
    return NS_CONTENT_ATTR_NOT_THERE;
  }
  nsresult GetAttribute(nsIAtom* aAttribute, nsHTMLValue& aValue) const {
    return NS_CONTENT_ATTR_NOT_THERE;
  }
  nsresult GetAllAttributeNames(nsISupportsArray* aArray,
                                PRInt32& aCount) const {
    aCount = 0;
    return NS_OK;
  }
  nsresult GetAttributeCount(PRInt32& aResult) const {
    aResult = 0;
    return NS_OK;
  }
  nsresult SetID(nsIAtom* aID) {
    return NS_OK;
  }
  nsresult GetID(nsIAtom*& aResult) const {
    aResult = nsnull;
    return NS_OK;
  }
  nsresult SetClass(nsIAtom* aClass) {
    return NS_OK;
  }
  nsresult GetClass(nsIAtom*& aResult) const {
    aResult = nsnull;
    return NS_OK;
  }
  nsresult GetStyleRule(nsIStyleRule*& aResult) {
    aResult = nsnull;
    return NS_OK;
  }
  nsresult MapAttributesInto(nsIStyleContext* aStyleContext,
                             nsIPresContext* aPresContext) {
    return NS_OK;
  }
  static void MapNoAttributes(nsIHTMLAttributes* aAttributes,
                              nsIStyleContext* aContext,
                              nsIPresContext* aPresContext) {
  }
  nsresult SizeOf(nsISizeOfHandler* aHandler) const;
  nsresult BeginConvertToXIF(nsXIFConverter& aConverter) const;
  nsresult ConvertContentToXIF(nsXIFConverter& aConverter) const;
  nsresult FinishConvertToXIF(nsXIFConverter& aConverter) const;
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

  //----------------------------------------

  void ToCString(nsString& aBuf, PRInt32 aOffset, PRInt32 aLen) const;

  // Up pointer to the real content object that we are
  // supporting. Sometimes there is work that we just can't do
  // ourselves, so this is needed to ask the real object to do the
  // work.
  nsIHTMLContent* mContent;

  nsIDocument* mDocument;
  nsIContent* mParent;
  void* mScriptObject;
  nsIEventListenerManager* mListenerManager;

  nsTextFragment mText;
};

//----------------------------------------------------------------------

/**
 * Mostly implement the nsIDOMNode API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 *
 * Note that classes using this macro will need to implement:
 *       NS_IMETHOD GetNodeType(PRInt32* aNodeType);
 *       NS_IMETHOD CloneNode(nsIDOMNode** aReturn);
 */
#define NS_IMPL_IDOMNODE_USING_GENERIC_DOM_DATA(_g)                     \
  NS_IMETHOD GetNodeName(nsString& aNodeName) {                         \
    return _g.GetNodeName(aNodeName);                                   \
  }                                                                     \
  NS_IMETHOD GetNodeValue(nsString& aNodeValue) {                       \
    return _g.GetNodeValue(aNodeValue);                                 \
  }                                                                     \
  NS_IMETHOD SetNodeValue(const nsString& aNodeValue) {                 \
    return _g.SetNodeValue(aNodeValue);                                 \
  }                                                                     \
  NS_IMETHOD GetNodeType(PRInt32* aNodeType);                           \
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
  NS_IMETHOD Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn);  \
  NS_IMETHOD CloneNode(nsIDOMNode** aReturn);

#define NS_IMPL_IDOMDATA_USING_GENERIC_DOM_DATA(_g)                         \
  NS_IMETHOD GetData(nsString& aData) {                                     \
    return _g.GetData(aData);                                               \
  }                                                                         \
  NS_IMETHOD SetData(const nsString& aData) {                               \
    return _g.SetData(aData);                                               \
  }                                                                         \
  NS_IMETHOD GetSize(PRUint32* aSize) {                                     \
    return _g.GetSize(aSize);                                               \
  }                                                                         \
  NS_IMETHOD Substring(PRUint32 aStart, PRUint32 aEnd, nsString& aReturn) { \
    return _g.Substring(aStart, aEnd, aReturn);                             \
  }                                                                         \
  NS_IMETHOD Append(const nsString& aData) {                                \
    return _g.Append(aData);                                                \
  }                                                                         \
  NS_IMETHOD Insert(PRUint32 aOffset, const nsString& aData) {              \
    return _g.Insert(aOffset, aData);                                       \
  }                                                                         \
  NS_IMETHOD Remove(PRUint32 aOffset, PRUint32 aCount) {                    \
    return _g.Remove(aOffset, aCount);                                      \
  }                                                                         \
  NS_IMETHOD Replace(PRUint32 aOffset, PRUint32 aCount,                     \
                     const nsString& aData) {                               \
    return _g.Replace(aOffset, aCount, aData);                              \
  }


/**
 * Implement the nsIDOMEventReceiver API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 */
#define NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC_DOM_DATA(_g)            \
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
#define NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC_DOM_DATA(_g) \
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext,      \
                             void** aScriptObject) {          \
    return _g.GetScriptObject(aContext, aScriptObject);       \
  }                                                           \
  NS_IMETHOD SetScriptObject(void *aScriptObject) {           \
    return _g.SetScriptObject(aScriptObject);                 \
  }

#define NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(_g)                        \
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {                    \
    return _g.GetDocument(aResult);                                        \
  }                                                                        \
  NS_IMETHOD SetDocument(nsIDocument* aDocument) {                         \
    return _g.SetDocument(aDocument);                                      \
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
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;                       \
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

#define NS_IMPL_IHTMLCONTENT_USING_GENERIC_DOM_DATA(_g)                \
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
  NS_IMETHOD ToHTMLString(nsString& aResult) const;                    \
  NS_IMETHOD ToHTML(FILE* out) const;                                  \
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,                    \
                               const nsString& aValue,                 \
                               nsHTMLValue& aResult) {                 \
    return NS_CONTENT_ATTR_NOT_THERE;                                  \
  }                                                                    \
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,                    \
                               nsHTMLValue& aValue,                    \
                               nsString& aResult) const {              \
    return NS_CONTENT_ATTR_NOT_THERE;                                  \
  }                                                                    \
  NS_IMETHOD                                                           \
    GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const { \
    aMapFunc = nsGenericDOMDataNode::MapNoAttributes;                  \
    return NS_OK;                                                      \
  }

/**
 * This macro implements the portion of query interface that is
 * generic to all html content objects.
 */
#define NS_IMPL_DOM_DATA_QUERY_INTERFACE(_id, _iptr, _this) \
  if (_id.Equals(kISupportsIID)) {                          \
    nsIHTMLContent* tmp = _this;                            \
    nsISupports* tmp2 = tmp;                                \
    *_iptr = (void*) tmp2;                                  \
    NS_ADDREF_THIS();                                       \
    return NS_OK;                                           \
  }                                                         \
  if (_id.Equals(kIDOMNodeIID)) {                           \
    nsIDOMNode* tmp = _this;                                \
    *_iptr = (void*) tmp;                                   \
    NS_ADDREF_THIS();                                       \
    return NS_OK;                                           \
  }                                                         \
  if (_id.Equals(kIDOMDataIID)) {                           \
    nsIDOMData* tmp = _this;                                \
    *_iptr = (void*) tmp;                                   \
    NS_ADDREF_THIS();                                       \
    return NS_OK;                                           \
  }                                                         \
  if (_id.Equals(kIDOMEventReceiverIID)) {                  \
    nsIDOMEventReceiver* tmp = _this;                       \
    *_iptr = (void*) tmp;                                   \
    NS_ADDREF_THIS();                                       \
    return NS_OK;                                           \
  }                                                         \
  if (_id.Equals(kIScriptObjectOwnerIID)) {                 \
    nsIScriptObjectOwner* tmp = _this;                      \
    *_iptr = (void*) tmp;                                   \
    NS_ADDREF_THIS();                                       \
    return NS_OK;                                           \
  }                                                         \
  if (_id.Equals(kIContentIID)) {                           \
    nsIContent* tmp = _this;                                \
    *_iptr = (void*) tmp;                                   \
    NS_ADDREF_THIS();                                       \
    return NS_OK;                                           \
  }                                                         \
  if (_id.Equals(kIHTMLContentIID)) {                       \
    nsIHTMLContent* tmp = _this;                            \
    *_iptr = (void*) tmp;                                   \
    NS_ADDREF_THIS();                                       \
    return NS_OK;                                           \
  }

#endif /* nsGenericDOMDataNode_h___ */
