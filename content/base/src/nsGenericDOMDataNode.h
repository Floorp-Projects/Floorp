/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Base class for DOM Core's nsIDOMComment, nsIDOMDocumentType, nsIDOMText,
 * nsIDOMCDATASection, and nsIDOMProcessingInstruction nodes.
 */

#ifndef nsGenericDOMDataNode_h___
#define nsGenericDOMDataNode_h___

#include "nsIDOMCharacterData.h"
#include "nsIDOMEventReceiver.h"
#include "nsTextFragment.h"
#include "nsVoidArray.h"
#include "nsDOMError.h"
#include "nsIEventListenerManager.h"
#include "nsGenericElement.h"
#include "nsCycleCollectionParticipant.h"

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIDOMNodeList;
class nsIFrame;
class nsIDOMText;
class nsINodeInfo;
class nsURI;

class nsGenericDOMDataNode : public nsIContent
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  nsGenericDOMDataNode(nsINodeInfo *aNodeInfo);
  virtual ~nsGenericDOMDataNode();

  // Implementation for nsIDOMNode
  nsresult GetNodeValue(nsAString& aNodeValue);
  nsresult SetNodeValue(const nsAString& aNodeValue);
  nsresult GetParentNode(nsIDOMNode** aParentNode);
  nsresult GetAttributes(nsIDOMNamedNodeMap** aAttributes)
  {
    NS_ENSURE_ARG_POINTER(aAttributes);
    *aAttributes = nsnull;
    return NS_OK;
  }
  nsresult GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  nsresult GetNextSibling(nsIDOMNode** aNextSibling);
  nsresult GetChildNodes(nsIDOMNodeList** aChildNodes);
  nsresult HasChildNodes(PRBool* aHasChildNodes)
  {
    NS_ENSURE_ARG_POINTER(aHasChildNodes);
    *aHasChildNodes = PR_FALSE;
    return NS_OK;
  }
  nsresult HasAttributes(PRBool* aHasAttributes)
  {
    NS_ENSURE_ARG_POINTER(aHasAttributes);
    *aHasAttributes = PR_FALSE;
    return NS_OK;
  }
  nsresult GetFirstChild(nsIDOMNode** aFirstChild)
  {
    NS_ENSURE_ARG_POINTER(aFirstChild);
    *aFirstChild = nsnull;
    return NS_OK;
  }
  nsresult GetLastChild(nsIDOMNode** aLastChild)
  {
    NS_ENSURE_ARG_POINTER(aLastChild);
    *aLastChild = nsnull;
    return NS_OK;
  }
  nsresult InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                        nsIDOMNode** aReturn)
  {
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = nsnull;
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }
  nsresult ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                        nsIDOMNode** aReturn)
  {
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = nsnull;

    /*
     * Data nodes can't have children.
     */
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }
  nsresult RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
  {
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = nsnull;

    /*
     * Data nodes can't have children, i.e. aOldChild can't be a child of
     * this node.
     */
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }
  nsresult AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
  {
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = nsnull;
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }
  nsresult GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
  nsresult GetNamespaceURI(nsAString& aNamespaceURI);
  nsresult GetLocalName(nsAString& aLocalName);
  nsresult GetPrefix(nsAString& aPrefix);
  nsresult SetPrefix(const nsAString& aPrefix);
  nsresult Normalize();
  nsresult IsSupported(const nsAString& aFeature,
                       const nsAString& aVersion,
                       PRBool* aReturn);
  nsresult GetBaseURI(nsAString& aURI);

  nsresult LookupPrefix(const nsAString& aNamespaceURI,
                        nsAString& aPrefix);
  nsresult LookupNamespaceURI(const nsAString& aNamespacePrefix,
                              nsAString& aNamespaceURI);

  // Implementation for nsIDOMCharacterData
  nsresult GetData(nsAString& aData) const;
  nsresult SetData(const nsAString& aData);
  nsresult GetLength(PRUint32* aLength);
  nsresult SubstringData(PRUint32 aOffset, PRUint32 aCount,
                         nsAString& aReturn);
  nsresult AppendData(const nsAString& aArg);
  nsresult InsertData(PRUint32 aOffset, const nsAString& aArg);
  nsresult DeleteData(PRUint32 aOffset, PRUint32 aCount);
  nsresult ReplaceData(PRUint32 aOffset, PRUint32 aCount,
                       const nsAString& aArg);

  // nsINode methods
  virtual PRUint32 GetChildCount() const;
  virtual nsIContent *GetChildAt(PRUint32 aIndex) const;
  virtual PRInt32 IndexOf(nsINode* aPossibleChild) const;
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual nsresult DispatchDOMEvent(nsEvent* aEvent, nsIDOMEvent* aDOMEvent,
                                    nsPresContext* aPresContext,
                                    nsEventStatus* aEventStatus);
  virtual nsresult GetListenerManager(PRBool aCreateIfNotFound,
                                      nsIEventListenerManager** aResult);

  // Implementation for nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);

  virtual nsIAtom *GetIDAttributeName() const;
  virtual already_AddRefed<nsINodeInfo> GetExistingAttrNameFromQName(const nsAString& aStr) const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);
  virtual PRBool GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute,
                         nsAString& aResult) const;
  virtual PRBool HasAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute) const;
  virtual const nsAttrName* GetAttrNameAt(PRUint32 aIndex) const;
  virtual PRUint32 GetAttrCount() const;
  virtual const nsTextFragment *GetText();
  virtual PRUint32 TextLength();
  virtual nsresult SetText(const PRUnichar* aBuffer, PRUint32 aLength,
                           PRBool aNotify);
  // Need to implement this here too to avoid hiding.
  nsresult SetText(const nsAString& aStr, PRBool aNotify)
  {
    return SetText(aStr.BeginReading(), aStr.Length(), aNotify);
  }
  virtual nsresult AppendText(const PRUnichar* aBuffer, PRUint32 aLength,
                              PRBool aNotify);
  virtual PRBool TextIsOnlyWhitespace();
  virtual void AppendTextTo(nsAString& aResult);
#ifdef DEBUG
  virtual void List(FILE* out, PRInt32 aIndent) const;
  virtual void DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const;
#endif

  virtual nsIContent *GetBindingParent() const;
  virtual PRBool IsNodeOfType(PRUint32 aFlags) const;

  virtual already_AddRefed<nsIURI> GetBaseURI() const;
  virtual PRBool IsLink(nsIURI** aURI) const;

  virtual PRBool MayHaveFrame() const;

  virtual nsIAtom* GetID() const;
  virtual const nsAttrValue* GetClasses() const;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  virtual nsICSSStyleRule* GetInlineStyleRule();
  NS_IMETHOD SetInlineStyleRule(nsICSSStyleRule* aStyleRule, PRBool aNotify);
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;
  virtual nsIAtom *GetClassAttributeName() const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
  {
    *aResult = CloneDataNode(aNodeInfo, PR_TRUE);
    if (!*aResult) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*aResult);

    return NS_OK;
  }

  //----------------------------------------

#ifdef DEBUG
  void ToCString(nsAString& aBuf, PRInt32 aOffset, PRInt32 aLen) const;
#endif

  NS_DECL_CYCLE_COLLECTION_CLASS(nsGenericDOMDataNode)

protected:
  /**
   * There are a set of DOM- and scripting-specific instance variables
   * that may only be instantiated when a content object is accessed
   * through the DOM. Rather than burn actual slots in the content
   * objects for each of these instance variables, we put them off
   * in a side structure that's only allocated when the content is
   * accessed through the DOM.
   */
  class nsDataSlots : public nsINode::nsSlots
  {
  public:
    nsDataSlots(PtrBits aFlags)
      : nsINode::nsSlots(aFlags),
        mBindingParent(nsnull)
    {
    }

    /**
     * The nearest enclosing content node with a binding that created us.
     * @see nsIContent::GetBindingParent
     */
    nsIContent* mBindingParent;  // [Weak]
  };

  // Override from nsINode
  virtual nsINode::nsSlots* CreateSlots();

  nsDataSlots *GetDataSlots()
  {
    return NS_STATIC_CAST(nsDataSlots*, GetSlots());
  }

  nsDataSlots *GetExistingDataSlots() const
  {
    return NS_STATIC_CAST(nsDataSlots*, GetExistingSlots());
  }

  nsresult SplitText(PRUint32 aOffset, nsIDOMText** aReturn);

  nsresult SetTextInternal(PRUint32 aOffset, PRUint32 aCount,
                           const PRUnichar* aBuffer, PRUint32 aLength,
                           PRBool aNotify);

  /**
   * Method to clone this node. This needs to be overriden by all derived
   * classes. If aCloneText is true the text content will be cloned too.
   *
   * @param aOwnerDocument the ownerDocument of the clone
   * @param aCloneText if true the text content will be cloned too
   * @return the clone
   */
  virtual nsGenericDOMDataNode *CloneDataNode(nsINodeInfo *aNodeInfo,
                                              PRBool aCloneText) const = 0;

  nsTextFragment mText;

private:
  void SetBidiStatus();

  already_AddRefed<nsIAtom> GetCurrentValueAtom();
};

//----------------------------------------------------------------------

/**
 * Mostly implement the nsIDOMNode API by forwarding the methods to
 * nsGenericDOMDataNode
 *
 * Note that classes using this macro will need to implement:
 *       NS_IMETHOD GetNodeType(PRUint16* aNodeType);
 *       nsGenericDOMDataNode *CloneDataNode(nsINodeInfo *aNodeInfo,
 *                                           PRBool aCloneText) const;
 */
#define NS_IMPL_NSIDOMNODE_USING_GENERIC_DOM_DATA                           \
  NS_IMETHOD GetNodeName(nsAString& aNodeName);                             \
  NS_IMETHOD GetLocalName(nsAString& aLocalName) {                          \
    return nsGenericDOMDataNode::GetLocalName(aLocalName);                  \
  }                                                                         \
  NS_IMETHOD GetNodeValue(nsAString& aNodeValue);                           \
  NS_IMETHOD SetNodeValue(const nsAString& aNodeValue);                     \
  NS_IMETHOD GetNodeType(PRUint16* aNodeType);                              \
  NS_IMETHOD GetParentNode(nsIDOMNode** aParentNode) {                      \
    return nsGenericDOMDataNode::GetParentNode(aParentNode);                \
  }                                                                         \
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes) {                  \
    return nsGenericDOMDataNode::GetChildNodes(aChildNodes);                \
  }                                                                         \
  NS_IMETHOD HasChildNodes(PRBool* aHasChildNodes) {                        \
    return nsGenericDOMDataNode::HasChildNodes(aHasChildNodes);             \
  }                                                                         \
  NS_IMETHOD HasAttributes(PRBool* aHasAttributes) {                        \
    return nsGenericDOMDataNode::HasAttributes(aHasAttributes);             \
  }                                                                         \
  NS_IMETHOD GetFirstChild(nsIDOMNode** aFirstChild) {                      \
    return nsGenericDOMDataNode::GetFirstChild(aFirstChild);                \
  }                                                                         \
  NS_IMETHOD GetLastChild(nsIDOMNode** aLastChild) {                        \
    return nsGenericDOMDataNode::GetLastChild(aLastChild);                  \
  }                                                                         \
  NS_IMETHOD GetPreviousSibling(nsIDOMNode** aPreviousSibling) {            \
    return nsGenericDOMDataNode::GetPreviousSibling(aPreviousSibling);      \
  }                                                                         \
  NS_IMETHOD GetNextSibling(nsIDOMNode** aNextSibling) {                    \
    return nsGenericDOMDataNode::GetNextSibling(aNextSibling);              \
  }                                                                         \
  NS_IMETHOD GetAttributes(nsIDOMNamedNodeMap** aAttributes) {              \
    return nsGenericDOMDataNode::GetAttributes(aAttributes);                \
  }                                                                         \
  NS_IMETHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,     \
                             nsIDOMNode** aReturn) {                        \
    return nsGenericDOMDataNode::InsertBefore(aNewChild, aRefChild,         \
                                              aReturn);                     \
  }                                                                         \
  NS_IMETHOD AppendChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) {     \
    return nsGenericDOMDataNode::AppendChild(aOldChild, aReturn);           \
  }                                                                         \
  NS_IMETHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,     \
                             nsIDOMNode** aReturn) {                        \
    return nsGenericDOMDataNode::ReplaceChild(aNewChild, aOldChild,         \
                                              aReturn);                     \
  }                                                                         \
  NS_IMETHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) {     \
    return nsGenericDOMDataNode::RemoveChild(aOldChild, aReturn);           \
  }                                                                         \
  NS_IMETHOD GetOwnerDocument(nsIDOMDocument** aOwnerDocument) {            \
    return nsGenericDOMDataNode::GetOwnerDocument(aOwnerDocument);          \
  }                                                                         \
  NS_IMETHOD GetNamespaceURI(nsAString& aNamespaceURI) {                    \
    return nsGenericDOMDataNode::GetNamespaceURI(aNamespaceURI);            \
  }                                                                         \
  NS_IMETHOD GetPrefix(nsAString& aPrefix) {                                \
    return nsGenericDOMDataNode::GetPrefix(aPrefix);                        \
  }                                                                         \
  NS_IMETHOD SetPrefix(const nsAString& aPrefix) {                          \
    return nsGenericDOMDataNode::SetPrefix(aPrefix);                        \
  }                                                                         \
  NS_IMETHOD Normalize() {                                                  \
    return NS_OK;                                                           \
  }                                                                         \
  NS_IMETHOD IsSupported(const nsAString& aFeature,                         \
                      const nsAString& aVersion,                            \
                      PRBool* aReturn) {                                    \
    return nsGenericDOMDataNode::IsSupported(aFeature, aVersion, aReturn);  \
  }                                                                         \
  NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn) {                \
    return nsNodeUtils::CloneNodeImpl(this, aDeep, aReturn);                \
  }                                                                         \
  virtual nsGenericDOMDataNode *CloneDataNode(nsINodeInfo *aNodeInfo,       \
                                              PRBool aCloneText) const;

#endif /* nsGenericDOMDataNode_h___ */
