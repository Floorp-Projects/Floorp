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

#include "nsIDOMCharacterData.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsTextFragment.h"
#include "nsVoidArray.h"
#include "nsINameSpaceManager.h"
#include "nsITextContent.h"

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
class nsIEventListenerManager;
class nsIFrame;
class nsIStyleContext;
class nsIStyleRule;
class nsISupportsArray;
class nsIDOMText;

struct nsGenericDOMDataNode {
  nsGenericDOMDataNode();
  ~nsGenericDOMDataNode();

  void Init(nsIContent* aOuterContentObject);

  // Implementation for nsIDOMNode
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
  nsresult    GetOwnerDocument(nsIDOMDocument** aOwnerDocument);

  // Implementation for nsIDOMCharacterData
  nsresult    GetData(nsString& aData);
  nsresult    SetData(const nsString& aData);
  nsresult    GetLength(PRUint32* aLength);
  nsresult    SubstringData(PRUint32 aOffset, PRUint32 aCount, nsString& aReturn);
  nsresult    AppendData(const nsString& aArg);
  nsresult    InsertData(PRUint32 aOffset, const nsString& aArg);
  nsresult    DeleteData(PRUint32 aOffset, PRUint32 aCount);
  nsresult    ReplaceData(PRUint32 aOffset, PRUint32 aCount, const nsString& aArg);

  // nsIDOMEventReceiver interface
  nsresult AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
  nsresult RemoveEventListenerByIID(nsIDOMEventListener* aListener,
                               const nsIID& aIID);
  nsresult GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
  nsresult GetNewListenerManager(nsIEventListenerManager** aInstancePtrResult);

  // nsIDOMEventTarget interface
  nsresult AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                            PRBool aPostProcess, PRBool aUseCapture);
  nsresult RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                               PRBool aPostProcess, PRBool aUseCapture);

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
  nsresult GetNameSpaceID(PRInt32& aID) const {
    aID = kNameSpaceID_None;
    return NS_OK;
  }
  nsresult ParseAttributeString(const nsString& aStr, 
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
  nsresult SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute, const nsString& aValue,
                        PRBool aNotify) {
    return NS_OK;
  }
  nsresult UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRBool aNotify) {
    return NS_OK;
  }
  nsresult GetAttribute(PRInt32 aNameSpaceID, nsIAtom *aAttribute, nsString &aResult) const {
    return NS_CONTENT_ATTR_NOT_THERE;
  }
  nsresult GetAttributeNameAt(PRInt32 aIndex, PRInt32& aNameSpaceID, 
                              nsIAtom*& aName) const {
    aName = nsnull;
    return NS_ERROR_ILLEGAL_VALUE;
  }
  nsresult GetAttributeCount(PRInt32& aResult) const {
    aResult = 0;
    return NS_OK;
  }
  nsresult List(FILE* out, PRInt32 aIndent) const;
  nsresult HandleDOMEvent(nsIPresContext& aPresContext,
                          nsEvent* aEvent,
                          nsIDOMEvent** aDOMEvent,
                          PRUint32 aFlags,
                          nsEventStatus& aEventStatus);
  nsresult RangeAdd(nsIDOMRange& aRange);
  nsresult RangeRemove(nsIDOMRange& aRange);
  nsresult GetRangeList(nsVoidArray*& aResult) const;

  // Implementation for nsIContent
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

  nsresult SplitText(PRUint32 aOffset, nsIDOMText** aReturn);

  nsresult GetText(const nsTextFragment*& aFragmentsResult,
                   PRInt32& aNumFragmentsResult);
  nsresult SetText(const PRUnichar* aBuffer,
                   PRInt32 aLength,
                   PRBool aNotify);
  nsresult SetText(const char* aBuffer,
                   PRInt32 aLength,
                   PRBool aNotify);
  nsresult IsOnlyWhitespace(PRBool* aResult);

  //----------------------------------------

  void ToCString(nsString& aBuf, PRInt32 aOffset, PRInt32 aLen) const;

  // Up pointer to the real content object that we are
  // supporting. Sometimes there is work that we just can't do
  // ourselves, so this is needed to ask the real object to do the
  // work.
  nsIContent* mContent;

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
  NS_IMETHOD GetNodeName(nsString& aNodeName);                          \
  NS_IMETHOD GetNodeValue(nsString& aNodeValue) {                       \
    return _g.GetNodeValue(aNodeValue);                                 \
  }                                                                     \
  NS_IMETHOD SetNodeValue(const nsString& aNodeValue) {                 \
    return _g.SetNodeValue(aNodeValue);                                 \
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

#define NS_IMPL_IDOMCHARACTERDATA_USING_GENERIC_DOM_DATA(_g)                         \
  NS_IMETHOD GetData(nsString& aData) {                                     \
    return _g.GetData(aData);                                               \
  }                                                                         \
  NS_IMETHOD SetData(const nsString& aData) {                               \
    return _g.SetData(aData);                                               \
  }                                                                         \
  NS_IMETHOD GetLength(PRUint32* aLength) {                                 \
    return _g.GetLength(aLength);                                           \
  }                                                                         \
  NS_IMETHOD SubstringData(PRUint32 aStart, PRUint32 aEnd, nsString& aReturn) { \
    return _g.SubstringData(aStart, aEnd, aReturn);                         \
  }                                                                         \
  NS_IMETHOD AppendData(const nsString& aData) {                            \
    return _g.AppendData(aData);                                            \
  }                                                                         \
  NS_IMETHOD InsertData(PRUint32 aOffset, const nsString& aData) {          \
    return _g.InsertData(aOffset, aData);                                   \
  }                                                                         \
  NS_IMETHOD DeleteData(PRUint32 aOffset, PRUint32 aCount) {                \
    return _g.DeleteData(aOffset, aCount);                                  \
  }                                                                         \
  NS_IMETHOD ReplaceData(PRUint32 aOffset, PRUint32 aCount,                 \
                     const nsString& aData) {                               \
    return _g.ReplaceData(aOffset, aCount, aData);                          \
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
  NS_IMETHOD AddEventListener(const nsString& aType,                            \
                              nsIDOMEventListener* aListener,                   \
                              PRBool aPostProcess,                              \
                              PRBool aUseCapture) {                             \
    return _g.AddEventListener(aType, aListener, aPostProcess, aUseCapture);    \
  }                                                                             \
  NS_IMETHOD RemoveEventListener(const nsString& aType,                         \
                                 nsIDOMEventListener* aListener,                \
                                 PRBool aPostProcess,                           \
                                 PRBool aUseCapture) {                          \
    return _g.RemoveEventListener(aType, aListener, aPostProcess, aUseCapture); \
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
  NS_IMETHOD GetNameSpaceID(PRInt32& aID) const {                          \
    return _g.GetNameSpaceID(aID);                                         \
  }                                                                        \
  NS_IMETHOD GetTag(nsIAtom*& aResult) const;                              \
  NS_IMETHOD ParseAttributeString(const nsString& aStr,                    \
                                  nsIAtom*& aName,                         \
                                  PRInt32& aNameSpaceID) {                 \
    return _g.ParseAttributeString(aStr, aName, aNameSpaceID);             \
  }                                                                        \
  NS_IMETHOD GetNameSpacePrefixFromId(PRInt32 aNameSpaceID,                \
                                nsIAtom*& aPrefix) {                       \
    return _g.GetNameSpacePrefixFromId(aNameSpaceID, aPrefix);             \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom *aAttribute,       \
                          nsString &aResult) const {                       \
    return _g.GetAttribute(aNameSpaceID, aAttribute, aResult);             \
  }                                                                        \
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute,       \
                          const nsString& aValue, PRBool aNotify) {        \
    return _g.SetAttribute(aNameSpaceID, aAttribute, aValue, aNotify);     \
  }                                                                        \
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute,     \
                            PRBool aNotify) {                              \
    return _g.UnsetAttribute(aNameSpaceID, aAttribute, aNotify);           \
  }                                                                        \
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,                            \
                                PRInt32& aNameSpaceID,                     \
                                nsIAtom*& aName) const {                   \
    return _g.GetAttributeNameAt(aIndex, aNameSpaceID, aName);             \
  }                                                                        \
  NS_IMETHOD GetAttributeCount(PRInt32& aResult) const {                   \
    return _g.GetAttributeCount(aResult);                                  \
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
  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,                  \
                            nsEvent* aEvent,                               \
                            nsIDOMEvent** aDOMEvent,                       \
                            PRUint32 aFlags,                               \
                            nsEventStatus& aEventStatus);                  \
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange){                                \
    return _g.RangeAdd(aRange);                                            \
  }                                                                        \
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange){                             \
    return _g.RangeRemove(aRange);                                         \
  }                                                                        \
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const {                   \
    return _g.GetRangeList(aResult);                                       \
  }                                                                        

/**
 * Implement the nsIDOMText API by forwarding the methods to a
 * generic character data content object.
 */
#define NS_IMPL_IDOMTEXT_USING_GENERIC_DOM_DATA(_g) \
  NS_IMETHOD SplitText(PRUint32 aOffset, nsIDOMText** aReturn){            \
    return _g.SplitText(aOffset, aReturn);                                 \
  }

/**
 * Implement the nsITextContent API by forwarding the methods to a
 * generic character data content object.
 */
#define NS_IMPL_ITEXTCONTENT_USING_GENERIC_DOM_DATA(_g) \
  NS_IMETHOD GetText(const nsTextFragment*& aFragmentsResult,              \
                     PRInt32& aNumFragmentsResult){                        \
    return mInner.GetText(aFragmentsResult, aNumFragmentsResult);          \
  }                                                                        \
  NS_IMETHOD SetText(const PRUnichar* aBuffer,                             \
                     PRInt32 aLength,                                      \
                     PRBool aNotify){                                      \
    return mInner.SetText(aBuffer, aLength, aNotify);                      \
  }                                                                        \
  NS_IMETHOD SetText(const char* aBuffer,                                  \
                     PRInt32 aLength,                                      \
                     PRBool aNotify){                                      \
    return mInner.SetText(aBuffer, aLength, aNotify);                      \
  }                                                                        \
  NS_IMETHOD IsOnlyWhitespace(PRBool* aResult){                            \
    return mInner.IsOnlyWhitespace(aResult);                               \
  }

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
    nsIDOMEventReceiver* tmp = _this;                       \
    *_iptr = (void*) tmp;                                   \
    NS_ADDREF_THIS();                                       \
    return NS_OK;                                           \
  }                                                         \
  if (_id.Equals(kIDOMEventTargetIID)) {                    \
    nsIDOMEventTarget* tmp = _this;                         \
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
  }

#endif /* nsGenericDOMDataNode_h___ */
