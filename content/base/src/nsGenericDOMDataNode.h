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
#ifndef nsGenericDOMDataNode_h___
#define nsGenericDOMDataNode_h___

#include "nsCOMPtr.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsTextFragment.h"
#include "nsVoidArray.h"
#include "nsINameSpaceManager.h"
#include "nsITextContent.h"
#include "nsDOMError.h"
#include "nsIEventListenerManager.h"
#include "nsGenericElement.h"


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
  nsresult    HasAttributes(PRBool* aHasAttributes) {
    NS_ENSURE_ARG_POINTER(aHasAttributes);
    *aHasAttributes = PR_FALSE;
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
  nsresult    GetLocalName(nsAWritableString& aLocalName);
  nsresult    GetPrefix(nsAWritableString& aPrefix);
  nsresult    SetPrefix(const nsAReadableString& aPrefix);
  nsresult    Normalize();
  nsresult    IsSupported(const nsAReadableString& aFeature,
                          const nsAReadableString& aVersion,
                          PRBool* aReturn);
  nsresult    GetBaseURI(nsAWritableString& aURI);
  nsresult    LookupNamespacePrefix(const nsAReadableString& aNamespaceURI,
                                    nsAWritableString& aPrefix);
  nsresult    LookupNamespaceURI(const nsAReadableString& aNamespacePrefix,
                                 nsAWritableString& aNamespaceURI);

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


  // Implementation for nsIContent
  nsresult GetDocument(nsIDocument*& aResult) const;
  nsresult SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers);
  nsresult GetParent(nsIContent*& aResult) const;
  nsresult SetParent(nsIContent* aParent);
  nsresult GetNameSpaceID(PRInt32& aID) const {
    aID = kNameSpaceID_None;
    return NS_OK;
  }
  nsresult NormalizeAttributeString(const nsAReadableString& aStr, 
                                    nsINodeInfo*& aNodeInfo) { 
    aNodeInfo = nsnull;
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
#ifdef DEBUG
  nsresult List(FILE* out, PRInt32 aIndent) const;
  nsresult DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const;
#endif
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

  nsresult GetListenerManager(nsIContent* aOuterContent,
                              nsIEventListenerManager** aInstancePtrResult);

#ifdef DEBUG
  nsresult SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult,
                  size_t aInstanceSize) const;
#endif

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
  nsresult InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                         PRBool aDeepSetDocument) {
    return NS_OK;
  }
  nsresult ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                          PRBool aDeepSetDocument) {
    return NS_OK;
  }
  nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify,
                         PRBool aDeepSetDocument) {
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

  void ToCString(nsAWritableString& aBuf, PRInt32 aOffset, PRInt32 aLen) const;

  nsIDocument* mDocument;
  nsIContent* mParent;
  nsIEventListenerManager* mListenerManager;

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
#define NS_IMPL_NSIDOMNODE_USING_GENERIC_DOM_DATA(_g)                   \
  NS_IMETHOD GetNodeName(nsAWritableString& aNodeName);                 \
  NS_IMETHOD GetLocalName(nsAWritableString& aLocalName) {              \
    return _g.GetLocalName(aLocalName);                                 \
  }                                                                     \
  NS_IMETHOD GetNodeValue(nsAWritableString& aNodeValue) {              \
    return _g.GetNodeValue(aNodeValue);                                 \
  }                                                                     \
  NS_IMETHOD SetNodeValue(const nsAReadableString& aNodeValue) {        \
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
  NS_IMETHOD HasAttributes(PRBool* aHasAttributes) {                    \
    return _g.HasAttributes(aHasAttributes);                            \
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
  NS_IMETHOD GetNamespaceURI(nsAWritableString& aNamespaceURI) {        \
    return _g.GetNamespaceURI(aNamespaceURI);                           \
  }                                                                     \
  NS_IMETHOD GetPrefix(nsAWritableString& aPrefix) {                    \
    return _g.GetPrefix(aPrefix);                                       \
  }                                                                     \
  NS_IMETHOD SetPrefix(const nsAReadableString& aPrefix) {              \
    return _g.SetPrefix(aPrefix);                                       \
  }                                                                     \
  NS_IMETHOD Normalize() {                                              \
    return NS_OK;                                                       \
  }                                                                     \
  NS_IMETHOD IsSupported(const nsAReadableString& aFeature,             \
                      const nsAReadableString& aVersion,                \
                      PRBool* aReturn) {                                \
    return _g.IsSupported(aFeature, aVersion, aReturn);                 \
  }                                                                     \
  NS_IMETHOD GetBaseURI(nsAWritableString& aURI) {                      \
    return _g.GetBaseURI(aURI);                                         \
  }                                                                     \
  NS_IMETHOD LookupNamespacePrefix(const nsAReadableString& aNamespaceURI, \
                                   nsAWritableString& aPrefix) {           \
    return _g.LookupNamespacePrefix(aNamespaceURI, aPrefix);               \
  }                                                                        \
  NS_IMETHOD LookupNamespaceURI(const nsAReadableString& aNamespacePrefix, \
                                nsAWritableString& aNamespaceURI) {        \
    return _g.LookupNamespaceURI(aNamespacePrefix, aNamespaceURI);         \
  }                                                                        \
  NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

#define NS_IMPL_NSIDOMCHARACTERDATA_USING_GENERIC_DOM_DATA(_g)              \
  NS_IMETHOD GetData(nsAWritableString& aData) {                            \
    return _g.GetData(aData);                                               \
  }                                                                         \
  NS_IMETHOD SetData(const nsAReadableString& aData) {                      \
    return _g.SetData(this, aData);                                         \
  }                                                                         \
  NS_IMETHOD GetLength(PRUint32* aLength) {                                 \
    return _g.GetLength(aLength);                                           \
  }                                                                         \
  NS_IMETHOD SubstringData(PRUint32 aStart, PRUint32 aEnd, nsAWritableString& aReturn) { \
    return _g.SubstringData(aStart, aEnd, aReturn);                         \
  }                                                                         \
  NS_IMETHOD AppendData(const nsAReadableString& aData) {                   \
    return _g.AppendData(this, aData);                                      \
  }                                                                         \
  NS_IMETHOD InsertData(PRUint32 aOffset, const nsAReadableString& aData) { \
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
#define NS_IMPL_NSIDOMEVENTRECEIVER_USING_GENERIC_DOM_DATA(_g)                  \
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

#ifdef DEBUG
#define DEBUG_METHODS                                                      \
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;                       \
  NS_IMETHOD DumpContent(FILE* out,                                        \
                         PRInt32 aIndent,                                  \
                         PRBool aDumpAll) const;
#else
#define DEBUG_METHODS
#endif

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
                           PRBool aNotify, PRBool aDeepSetDocument) {      \
    return _g.InsertChildAt(aKid, aIndex, aNotify, aDeepSetDocument);      \
  }                                                                        \
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,              \
                            PRBool aNotify, PRBool aDeepSetDocument) {     \
    return _g.ReplaceChildAt(aKid, aIndex, aNotify, aDeepSetDocument);     \
  }                                                                        \
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,               \
                           PRBool aDeepSetDocument) {                      \
    return _g.AppendChildTo(aKid, aNotify, aDeepSetDocument);              \
  }                                                                        \
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {               \
    return _g.RemoveChildAt(aIndex, aNotify);                              \
  }                                                                        \
  NS_IMETHOD GetNameSpaceID(PRInt32& aID) const {                          \
    return _g.GetNameSpaceID(aID);                                         \
  }                                                                        \
  NS_IMETHOD GetTag(nsIAtom*& aResult) const;                              \
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const;                     \
  NS_IMETHOD NormalizeAttrString(const nsAReadableString& aStr,            \
                                 nsINodeInfo*& aNodeInfo) {                \
    return _g.NormalizeAttributeString(aStr, aNodeInfo);                   \
  }                                                                        \
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute,            \
                     nsAWritableString& aResult) const {                   \
    return _g.GetAttribute(aNameSpaceID, aAttribute, aResult);             \
  }                                                                        \
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute,            \
                     nsIAtom*& aPrefix, nsAWritableString& aResult) const {    \
    return _g.GetAttribute(aNameSpaceID, aAttribute, aPrefix, aResult);    \
  }                                                                        \
  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,            \
                     const nsAReadableString& aValue, PRBool aNotify) {    \
    return _g.SetAttribute(aNameSpaceID, aAttribute, aValue, aNotify);     \
  }                                                                        \
  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,                               \
                     const nsAReadableString& aValue, PRBool aNotify) {    \
    return _g.SetAttribute(aNodeInfo, aValue, aNotify);                    \
  }                                                                        \
  NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,          \
                       PRBool aNotify) {                                   \
    return _g.UnsetAttribute(aNameSpaceID, aAttribute, aNotify);           \
  }                                                                        \
  NS_IMETHOD GetAttrNameAt(PRInt32 aIndex,                                 \
                           PRInt32& aNameSpaceID,                          \
                           nsIAtom*& aName,                                \
                           nsIAtom*& aPrefix) const {                      \
    return _g.GetAttributeNameAt(aIndex, aNameSpaceID, aName, aPrefix);    \
  }                                                                        \
  NS_IMETHOD GetAttrCount(PRInt32& aResult) const {                        \
    return _g.GetAttributeCount(aResult);                                  \
  }                                                                        \
  DEBUG_METHODS                                                            \
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
    return _g.GetBindingParent(aContent);                                  \
  }                                                                        \
  NS_IMETHOD SetBindingParent(nsIContent* aParent) {                       \
    return _g.SetBindingParent(aParent);                                   \
  }                                                                        \
  NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags);                    \
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aResult) {       \
    return _g.GetListenerManager(this, aResult);                           \
  }


/**
 * Implement the nsIDOMText API by forwarding the methods to a
 * generic character data content object.
 */
#define NS_IMPL_NSIDOMTEXT_USING_GENERIC_DOM_DATA(_g) \
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

#define NS_INTERFACE_MAP_ENTRY_DOM_DATA()                                     \
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)                   \
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)                                          \
  if (aIID.Equals(NS_GET_IID(nsIDOMEventReceiver)) ||                         \
      aIID.Equals(NS_GET_IID(nsIDOMEventTarget))) {                           \
    foundInterface = NS_STATIC_CAST(nsIDOMEventReceiver *,                    \
                                    nsDOMEventRTTearoff::Create(this));       \
    NS_ENSURE_TRUE(foundInterface, NS_ERROR_OUT_OF_MEMORY);                   \
  } else                                                                      \
  NS_INTERFACE_MAP_ENTRY(nsIContent)                                          \
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3Node, nsNode3Tearoff(this))

#endif /* nsGenericDOMDataNode_h___ */
