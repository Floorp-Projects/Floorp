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
#ifndef nsGenericDOMDataNode_h___
#define nsGenericDOMDataNode_h___

#include "nsCOMPtr.h"
#include "nsIDOMCharacterData.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsTextFragment.h"
#include "nsVoidArray.h"
#include "nsINameSpaceManager.h"
#include "nsITextContent.h"
#include "nsDOMError.h"
#include "nsIEventListenerManager.h"

extern const nsIID kIDOMCharacterDataIID;
extern const nsIID kIDOMNodeIID;
extern const nsIID kIDOMEventReceiverIID;
extern const nsIID kIDOMEventTargetIID;
extern const nsIID kIScriptObjectOwnerIID;
extern const nsIID kISupportsIID;
extern const nsIID kIContentIID;

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIDOMNodeList;
class nsIFrame;
class nsIStyleContext;
class nsIStyleRule;
class nsISupportsArray;
class nsIDOMText;
class nsINodeInfo;

struct nsGenericDOMDataNode {
  nsGenericDOMDataNode();
  virtual ~nsGenericDOMDataNode();

  // Implementation for nsIDOMNode
  nsresult    GetNodeValue(nsAWritableString& aNodeValue);
  nsresult    SetNodeValue(nsIContent *aOuterContent,
                           const nsAReadableString& aNodeValue);
  nsresult    GetParentNode(nsIDOMNode** aParentNode);
  nsresult    GetAttributes(nsIDOMNamedNodeMap** aAttributes) {
    NS_ENSURE_ARG_POINTER(aAttributes);
    *aAttributes = nsnull;
    return NS_OK;
  }
  nsresult    GetPreviousSibling(nsIContent *aOuterContent,
                                 nsIDOMNode** aPreviousSibling);
  nsresult    GetNextSibling(nsIContent *aOuterContent,
                             nsIDOMNode** aNextSibling);
  nsresult    GetChildNodes(nsIDOMNodeList** aChildNodes);
  nsresult    HasChildNodes(PRBool* aHasChildNodes) {
    NS_ENSURE_ARG_POINTER(aHasChildNodes);
    *aHasChildNodes = PR_FALSE;
    return NS_OK;
  }
  nsresult    GetFirstChild(nsIDOMNode** aFirstChild) {
    NS_ENSURE_ARG_POINTER(aFirstChild);
    *aFirstChild = nsnull;
    return NS_OK;
  }
  nsresult    GetLastChild(nsIDOMNode** aLastChild) {
    NS_ENSURE_ARG_POINTER(aLastChild);
    *aLastChild = nsnull;
    return NS_OK;
  }
  nsresult    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                           nsIDOMNode** aReturn) {
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = nsnull;
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }
  nsresult    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                           nsIDOMNode** aReturn) {
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = nsnull;

    /*
     * Data nodes can't have children, i.e. aOldChild can't be a child of
     * this node.
     */
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }
  nsresult    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) {
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = nsnull;

    /*
     * Data nodes can't have children, i.e. aOldChild can't be a child of
     * this node.
     */
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }
  nsresult    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn) {
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = nsnull;
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }
  nsresult    GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
  nsresult    GetNamespaceURI(nsAWritableString& aNamespaceURI);
  nsresult    GetPrefix(nsAWritableString& aPrefix);
  nsresult    SetPrefix(const nsAReadableString& aPrefix);
  nsresult    Normalize();
  nsresult    Supports(const nsAReadableString& aFeature, const nsAReadableString& aVersion,
                       PRBool* aReturn);

  // Implementation for nsIDOMCharacterData
  nsresult    GetData(nsAWritableString& aData);
  nsresult    SetData(nsIContent *aOuterContent, const nsAReadableString& aData);
  nsresult    GetLength(PRUint32* aLength);
  nsresult    SubstringData(PRUint32 aOffset, PRUint32 aCount, nsAWritableString& aReturn);
  nsresult    AppendData(nsIContent *aOuterContent, const nsAReadableString& aArg);
  nsresult    InsertData(nsIContent *aOuterContent, PRUint32 aOffset,
                         const nsAReadableString& aArg);
  nsresult    DeleteData(nsIContent *aOuterContent, PRUint32 aOffset,
                         PRUint32 aCount);
  nsresult    ReplaceData(nsIContent *aOuterContent, PRUint32 aOffset,
                          PRUint32 aCount, const nsAReadableString& aArg);


  // nsIScriptObjectOwner interface
  nsresult GetScriptObject(nsIContent *aOuterContent,
                           nsIScriptContext* aContext, void** aScriptObject);
  nsresult SetScriptObject(void *aScriptObject);

  // Implementation for nsIContent
  nsresult GetDocument(nsIDocument*& aResult) const;
  nsresult SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers);
  nsresult GetParent(nsIContent*& aResult) const;
  nsresult SetParent(nsIContent* aParent);
  nsresult IsSynthetic(PRBool& aResult) {
    aResult = PR_FALSE;
    return NS_OK;
  }
  nsresult GetNameSpaceID(PRInt32& aID) const {
    aID = kNameSpaceID_None;
    return NS_OK;
  }
  nsresult ParseAttributeString(const nsAReadableString& aStr, 
                                nsIAtom*& aName,
                                PRInt32& aNameSpaceID) { 
    aName = nsnull;
    aNameSpaceID = kNameSpaceID_None;
    return NS_OK; 
  }
  NS_IMETHOD GetNameSpacePrefixFromId(PRInt32 aNameSpaceID,
                                      nsIAtom*& aPrefix) {
    aPrefix = nsnull;
    return NS_OK;
  }
  nsresult SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute, const nsAReadableString& aValue,
                        PRBool aNotify) {
    return NS_OK;
  }
  nsresult SetAttribute(nsINodeInfo *aNodeInfo, const nsAReadableString& aValue,
                        PRBool aNotify) {
    return NS_OK;
  }
  nsresult UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRBool aNotify) {
    return NS_OK;
  }
  nsresult GetAttribute(PRInt32 aNameSpaceID, nsIAtom *aAttribute, nsAWritableString& aResult) const {
    return NS_CONTENT_ATTR_NOT_THERE;
  }
  nsresult GetAttribute(PRInt32 aNameSpaceID, nsIAtom *aAttribute, nsIAtom*& aPrefix, nsAWritableString& aResult) const {
    aPrefix = nsnull;
    return NS_CONTENT_ATTR_NOT_THERE;
  }
  nsresult GetAttributeNameAt(PRInt32 aIndex, PRInt32& aNameSpaceID, 
                              nsIAtom*& aName, nsIAtom*& aPrefix) const {
    aNameSpaceID = kNameSpaceID_None;
    aName = nsnull;
    aPrefix = nsnull;
    return NS_ERROR_ILLEGAL_VALUE;
  }
  nsresult GetAttributeCount(PRInt32& aResult) const {
    aResult = 0;
    return NS_OK;
  }
  nsresult List(FILE* out, PRInt32 aIndent) const;
  nsresult DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const;
  nsresult HandleDOMEvent(nsIPresContext* aPresContext,
                          nsEvent* aEvent,
                          nsIDOMEvent** aDOMEvent,
                          PRUint32 aFlags,
                          nsEventStatus* aEventStatus);
  nsresult RangeAdd(nsIDOMRange& aRange);
  nsresult RangeRemove(nsIDOMRange& aRange);
  nsresult GetRangeList(nsVoidArray*& aResult) const;
  nsresult SetFocus(nsIPresContext *aPresContext);
  nsresult RemoveFocus(nsIPresContext *aPresContext);

  nsresult GetBindingParent(nsIContent** aContent);
  nsresult SetBindingParent(nsIContent* aParent);

  nsresult SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult,
                  size_t aInstanceSize) const;

  // Implementation for nsIContent
  nsresult BeginConvertToXIF(nsIXIFConverter * aConverter) const;
  nsresult ConvertContentToXIF(const nsIContent *aOuterContent,
                               nsIXIFConverter * aConverter) const;
  nsresult FinishConvertToXIF(nsIXIFConverter * aConverter) const;
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

  nsresult SplitText(nsIContent *aOuterContent, PRUint32 aOffset,
                     nsIDOMText** aReturn);

  nsresult GetText(const nsTextFragment** aFragmentsResult);
  nsresult GetTextLength(PRInt32* aLengthResult);
  nsresult CopyText(nsAWritableString& aResult);
  nsresult SetText(nsIContent *aOuterContent,
                   const PRUnichar* aBuffer,
                   PRInt32 aLength,
                   PRBool aNotify);
  nsresult SetText(nsIContent *aOuterContent,
                   const nsAReadableString& aStr,
                   PRBool aNotify);
  nsresult SetText(nsIContent *aOuterContent,
                   const char* aBuffer,
                   PRInt32 aLength,
                   PRBool aNotify);
  nsresult IsOnlyWhitespace(PRBool* aResult);

  //----------------------------------------

  nsresult GetListenerManager(nsIContent* aOuterContent, nsIEventListenerManager** aInstancePtrResult);

  void ToCString(nsAWritableString& aBuf, PRInt32 aOffset, PRInt32 aLen) const;

  nsIDocument* mDocument;
  nsIContent* mParent;
  void* mScriptObject;
  nsIEventListenerManager* mListenerManager;
  nsIContent* mCapturer;

  nsTextFragment mText;
  nsVoidArray *mRangeList;
};

//----------------------------------------------------------------------

/**
 * Mostly implement the nsIDOMNode API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 *
 * Note that classes using this macro will need to implement:
 *       NS_IMETHOD GetNodeType(PRUint16* aNodeType);
 *       NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);
 */
#define NS_IMPL_IDOMNODE_USING_GENERIC_DOM_DATA(_g)                     \
  NS_IMETHOD GetNodeName(nsAWritableString& aNodeName);                          \
  NS_IMETHOD GetLocalName(nsAWritableString& aLocalName) {                       \
    return GetNodeName(aLocalName);                                     \
  }                                                                     \
  NS_IMETHOD GetNodeValue(nsAWritableString& aNodeValue) {                       \
    return _g.GetNodeValue(aNodeValue);                                 \
  }                                                                     \
  NS_IMETHOD SetNodeValue(const nsAReadableString& aNodeValue) {                 \
    return _g.SetNodeValue(this, aNodeValue);                           \
  }                                                                     \
  NS_IMETHOD GetNodeType(PRUint16* aNodeType);                          \
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
    return _g.GetPreviousSibling(this, aPreviousSibling);               \
  }                                                                     \
  NS_IMETHOD GetNextSibling(nsIDOMNode** aNextSibling) {                \
    return _g.GetNextSibling(this, aNextSibling);                       \
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
  NS_IMETHOD GetNamespaceURI(nsAWritableString& aNamespaceURI) {                 \
    return _g.GetNamespaceURI(aNamespaceURI);                           \
  }                                                                     \
  NS_IMETHOD GetPrefix(nsAWritableString& aPrefix) {                             \
    return _g.GetPrefix(aPrefix);                                       \
  }                                                                     \
  NS_IMETHOD SetPrefix(const nsAReadableString& aPrefix) {                       \
    return _g.SetPrefix(aPrefix);                                       \
  }                                                                     \
  NS_IMETHOD Normalize() {                                              \
    return NS_OK;                                                       \
  }                                                                     \
  NS_IMETHOD Supports(const nsAReadableString& aFeature, const nsAReadableString& aVersion,\
                      PRBool* aReturn) {                                \
    return _g.Supports(aFeature, aVersion, aReturn);                    \
  }                                                                     \
  NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

#define NS_IMPL_IDOMCHARACTERDATA_USING_GENERIC_DOM_DATA(_g)                         \
  NS_IMETHOD GetData(nsAWritableString& aData) {                                     \
    return _g.GetData(aData);                                               \
  }                                                                         \
  NS_IMETHOD SetData(const nsAReadableString& aData) {                               \
    return _g.SetData(this, aData);                                         \
  }                                                                         \
  NS_IMETHOD GetLength(PRUint32* aLength) {                                 \
    return _g.GetLength(aLength);                                           \
  }                                                                         \
  NS_IMETHOD SubstringData(PRUint32 aStart, PRUint32 aEnd, nsAWritableString& aReturn) { \
    return _g.SubstringData(aStart, aEnd, aReturn);                         \
  }                                                                         \
  NS_IMETHOD AppendData(const nsAReadableString& aData) {                            \
    return _g.AppendData(this, aData);                                      \
  }                                                                         \
  NS_IMETHOD InsertData(PRUint32 aOffset, const nsAReadableString& aData) {          \
    return _g.InsertData(this, aOffset, aData);                             \
  }                                                                         \
  NS_IMETHOD DeleteData(PRUint32 aOffset, PRUint32 aCount) {                \
    return _g.DeleteData(this, aOffset, aCount);                            \
  }                                                                         \
  NS_IMETHOD ReplaceData(PRUint32 aOffset, PRUint32 aCount,                 \
                     const nsAReadableString& aData) {                               \
    return _g.ReplaceData(this, aOffset, aCount, aData);                    \
  }


/**
 * Implement the nsIDOMEventReceiver API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 */
#define NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC_DOM_DATA(_g)                    \
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,              \
                                   const nsIID& aIID) {                         \
    return _g.AddEventListenerByIID(aListener, aIID);                           \
  }                                                                             \
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,           \
                                      const nsIID& aIID) {                      \
    return _g.RemoveEventListenerByIID(aListener, aIID);                        \
  }                                                                             \
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aResult) {            \
    return _g.GetListenerManager(aResult);                                      \
  }                                                                             \
  NS_IMETHOD GetNewListenerManager(nsIEventListenerManager** aResult) {         \
    return _g.GetNewListenerManager(aResult);                                   \
  }                                                                             \
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent) {                                 \
    return _g.HandleEvent(aEvent);                                              \
  }                                                                             \
  NS_IMETHOD AddEventListener(const nsAReadableString& aType,                            \
                              nsIDOMEventListener* aListener,                   \
                              PRBool aUseCapture) {                             \
    return _g.AddEventListener(aType, aListener, aUseCapture);    \
  }                                                                             \
  NS_IMETHOD RemoveEventListener(const nsAReadableString& aType,                         \
                                 nsIDOMEventListener* aListener,                \
                                 PRBool aUseCapture) {                          \
    return _g.RemoveEventListener(aType, aListener, aUseCapture); \
  }                                                                     

/**
 * Implement the nsIScriptObjectOwner API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 */
#define NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC_DOM_DATA(_g) \
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext,      \
                             void** aScriptObject) {          \
    return _g.GetScriptObject(this, aContext, aScriptObject); \
  }                                                           \
  NS_IMETHOD SetScriptObject(void *aScriptObject) {           \
    return _g.SetScriptObject(aScriptObject);                 \
  }

#define NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(_g)        \
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {                    \
    return _g.GetDocument(aResult);                                        \
  }                                                                        \
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers) {           \
    return _g.SetDocument(aDocument, aDeep, aCompileEventHandlers);                               \
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
  NS_IMETHOD GetNameSpaceID(PRInt32& aID) const {                          \
    return _g.GetNameSpaceID(aID);                                         \
  }                                                                        \
  NS_IMETHOD GetTag(nsIAtom*& aResult) const;                              \
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const;                     \
  NS_IMETHOD ParseAttributeString(const nsAReadableString& aStr,                    \
                                  nsIAtom*& aName,                         \
                                  PRInt32& aNameSpaceID) {                 \
    return _g.ParseAttributeString(aStr, aName, aNameSpaceID);             \
  }                                                                        \
  NS_IMETHOD GetNameSpacePrefixFromId(PRInt32 aNameSpaceID,                \
                                nsIAtom*& aPrefix) {                       \
    return _g.GetNameSpacePrefixFromId(aNameSpaceID, aPrefix);             \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom *aAttribute,       \
                          nsAWritableString& aResult) const {                       \
    return _g.GetAttribute(aNameSpaceID, aAttribute, aResult);             \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom *aAttribute,       \
                          nsIAtom*& aPrefix, nsAWritableString& aResult) const {    \
    return _g.GetAttribute(aNameSpaceID, aAttribute, aPrefix, aResult);    \
  }                                                                        \
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute,       \
                          const nsAReadableString& aValue, PRBool aNotify) {        \
    return _g.SetAttribute(aNameSpaceID, aAttribute, aValue, aNotify);     \
  }                                                                        \
  NS_IMETHOD SetAttribute(nsINodeInfo* aNodeInfo,                          \
                          const nsAReadableString& aValue, PRBool aNotify) {        \
    return _g.SetAttribute(aNodeInfo, aValue, aNotify);                    \
  }                                                                        \
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute,     \
                            PRBool aNotify) {                              \
    return _g.UnsetAttribute(aNameSpaceID, aAttribute, aNotify);           \
  }                                                                        \
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,                            \
                                PRInt32& aNameSpaceID,                     \
                                nsIAtom*& aName,                           \
                                nsIAtom*& aPrefix) const {                 \
    return _g.GetAttributeNameAt(aIndex, aNameSpaceID, aName, aPrefix);    \
  }                                                                        \
  NS_IMETHOD GetAttributeCount(PRInt32& aResult) const {                   \
    return _g.GetAttributeCount(aResult);                                  \
  }                                                                        \
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;                       \
  NS_IMETHOD DumpContent(FILE* out,                                        \
                         PRInt32 aIndent,                                  \
                         PRBool aDumpAll) const;                           \
  NS_IMETHOD BeginConvertToXIF(nsIXIFConverter * aConverter) const {       \
    return _g.BeginConvertToXIF(aConverter);                               \
  }                                                                        \
  NS_IMETHOD ConvertContentToXIF(nsIXIFConverter * aConverter) const {       \
    return _g.ConvertContentToXIF(this, aConverter);                       \
  }                                                                        \
  NS_IMETHOD FinishConvertToXIF(nsIXIFConverter * aConverter) const {        \
    return _g.FinishConvertToXIF(aConverter);                              \
  }                                                                        \
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,                  \
                            nsEvent* aEvent,                               \
                            nsIDOMEvent** aDOMEvent,                       \
                            PRUint32 aFlags,                               \
                            nsEventStatus* aEventStatus);                  \
  NS_IMETHOD GetContentID(PRUint32* aID);                                  \
  NS_IMETHOD SetContentID(PRUint32 aID);                                   \
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange){                                \
    return _g.RangeAdd(aRange);                                            \
  }                                                                        \
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange){                             \
    return _g.RangeRemove(aRange);                                         \
  }                                                                        \
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const {                   \
    return _g.GetRangeList(aResult);                                       \
  }                                                                        \
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext) {                      \
    return _g.SetFocus(aPresContext);                                      \
  }                                                                        \
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext) {                   \
    return _g.RemoveFocus(aPresContext);                                   \
  }                                                                        \
  NS_IMETHOD GetBindingParent(nsIContent** aContent) {                     \
    return _g.GetBindingParent(aContent);                              \
  }                                                                        \
  NS_IMETHOD SetBindingParent(nsIContent* aParent) { \
    return _g.SetBindingParent(aParent); \
  }        

/**
 * Implement the nsIDOMText API by forwarding the methods to a
 * generic character data content object.
 */
#define NS_IMPL_IDOMTEXT_USING_GENERIC_DOM_DATA(_g) \
  NS_IMETHOD SplitText(PRUint32 aOffset, nsIDOMText** aReturn){            \
    return _g.SplitText(this, aOffset, aReturn);                           \
  }

/**
 * Implement the nsITextContent API by forwarding the methods to a
 * generic character data content object.
 */
#define NS_IMPL_ITEXTCONTENT_USING_GENERIC_DOM_DATA(_g)           \
  NS_IMETHOD GetText(const nsTextFragment** aFragmentsResult) {   \
    return mInner.GetText(aFragmentsResult);                      \
  }                                                               \
  NS_IMETHOD GetTextLength(PRInt32* aLengthResult) {              \
    return mInner.GetTextLength(aLengthResult);                   \
  }                                                               \
  NS_IMETHOD CopyText(nsAWritableString& aResult) {                        \
    return mInner.CopyText(aResult);                              \
  }                                                               \
  NS_IMETHOD SetText(const PRUnichar* aBuffer,                    \
                     PRInt32 aLength,                             \
                     PRBool aNotify){                             \
    return mInner.SetText(this, aBuffer, aLength, aNotify);       \
  }                                                               \
  NS_IMETHOD SetText(const nsAReadableString& aStr,               \
                     PRBool aNotify){                             \
    return mInner.SetText(this, aStr, aNotify);                   \
  }                                                               \
  NS_IMETHOD SetText(const char* aBuffer,                         \
                     PRInt32 aLength,                             \
                     PRBool aNotify){                             \
    return mInner.SetText(this, aBuffer, aLength, aNotify);       \
  }                                                               \
  NS_IMETHOD IsOnlyWhitespace(PRBool* aResult){                   \
    return mInner.IsOnlyWhitespace(aResult);                      \
  }                                                               \
  NS_IMETHOD CloneContent(PRBool aCloneText, nsITextContent** aClone); 

/**
 * This macro implements the portion of query interface that is
 * generic to all html content objects.
 */
#define NS_IMPL_DOM_DATA_QUERY_INTERFACE(_id, _iptr, _this) \
  if (_id.Equals(kISupportsIID)) {                          \
    nsIContent* tmp = _this;                                \
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
  if (_id.Equals(kIDOMCharacterDataIID)) {                  \
    nsIDOMCharacterData* tmp = _this;                       \
    *_iptr = (void*) tmp;                                   \
    NS_ADDREF_THIS();                                       \
    return NS_OK;                                           \
  }                                                         \
  if (_id.Equals(kIDOMEventReceiverIID)) {                  \
    nsCOMPtr<nsIEventListenerManager> man;                  \
    if (NS_SUCCEEDED(mInner.GetListenerManager(this, getter_AddRefs(man)))){ \
      return man->QueryInterface(kIDOMEventReceiverIID, (void**)_iptr); \
    }                                                       \
    return NS_NOINTERFACE;                                  \
  }                                                         \
  if (_id.Equals(kIDOMEventTargetIID)) {                    \
    nsCOMPtr<nsIEventListenerManager> man;                  \
    if (NS_SUCCEEDED(mInner.GetListenerManager(this, getter_AddRefs(man)))){ \
      return man->QueryInterface(kIDOMEventTargetIID, (void**)_iptr); \
    }                                                       \
    return NS_NOINTERFACE;                                  \
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
  }

#endif /* nsGenericDOMDataNode_h___ */
