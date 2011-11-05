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

#include "nsIContent.h"

#include "nsTextFragment.h"
#include "nsDOMError.h"
#include "nsEventListenerManager.h"
#include "nsGenericElement.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMMemoryReporter.h"

#include "nsISMILAttr.h"

// This bit is set to indicate that if the text node changes to
// non-whitespace, we may need to create a frame for it. This bit must
// not be set on nodes that already have a frame.
#define NS_CREATE_FRAME_IF_NON_WHITESPACE (1 << NODE_TYPE_SPECIFIC_BITS_OFFSET)

// This bit is set to indicate that if the text node changes to
// whitespace, we may need to reframe it (or its ancestors).
#define NS_REFRAME_IF_WHITESPACE (1 << (NODE_TYPE_SPECIFIC_BITS_OFFSET + 1))

// This bit is set to indicate that the text may be part of a selection.
#define NS_TEXT_IN_SELECTION (1 << (NODE_TYPE_SPECIFIC_BITS_OFFSET + 2))

// Make sure we have enough space for those bits
PR_STATIC_ASSERT(NODE_TYPE_SPECIFIC_BITS_OFFSET + 2 < 32);

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

  NS_DECL_DOM_MEMORY_REPORTER_SIZEOF

  nsGenericDOMDataNode(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsGenericDOMDataNode();

  // Implementation for nsIDOMNode
  nsresult GetNodeName(nsAString& aNodeName)
  {
    aNodeName = NodeName();
    return NS_OK;
  }
  nsresult GetNodeType(PRUint16* aNodeType)
  {
    *aNodeType = NodeType();
    return NS_OK;
  }
  nsresult GetNodeValue(nsAString& aNodeValue);
  nsresult SetNodeValue(const nsAString& aNodeValue);
  nsresult GetAttributes(nsIDOMNamedNodeMap** aAttributes)
  {
    NS_ENSURE_ARG_POINTER(aAttributes);
    *aAttributes = nsnull;
    return NS_OK;
  }
  nsresult HasChildNodes(bool* aHasChildNodes)
  {
    NS_ENSURE_ARG_POINTER(aHasChildNodes);
    *aHasChildNodes = false;
    return NS_OK;
  }
  nsresult HasAttributes(bool* aHasAttributes)
  {
    NS_ENSURE_ARG_POINTER(aHasAttributes);
    *aHasAttributes = false;
    return NS_OK;
  }
  nsresult InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                        nsIDOMNode** aReturn)
  {
    return ReplaceOrInsertBefore(false, aNewChild, aRefChild, aReturn);
  }
  nsresult ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                        nsIDOMNode** aReturn)
  {
    return ReplaceOrInsertBefore(true, aNewChild, aOldChild, aReturn);
  }
  nsresult RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
  {
    return nsINode::RemoveChild(aOldChild, aReturn);
  }
  nsresult AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
  {
    return InsertBefore(aNewChild, nsnull, aReturn);
  }
  nsresult GetNamespaceURI(nsAString& aNamespaceURI);
  nsresult GetLocalName(nsAString& aLocalName)
  {
    aLocalName = LocalName();
    return NS_OK;
  }
  nsresult GetPrefix(nsAString& aPrefix);
  nsresult IsSupported(const nsAString& aFeature,
                       const nsAString& aVersion,
                       bool* aReturn);
  nsresult CloneNode(bool aDeep, nsIDOMNode** aReturn)
  {
    return nsNodeUtils::CloneNodeImpl(this, aDeep, true, aReturn);
  }

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
  virtual nsIContent * const * GetChildArray(PRUint32* aChildCount) const;
  virtual PRInt32 IndexOf(nsINode* aPossibleChild) const;
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 bool aNotify);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, bool aNotify);
  NS_IMETHOD GetTextContent(nsAString &aTextContent)
  {
    nsresult rv = GetNodeValue(aTextContent);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetNodeValue() failed?");
    return rv;
  }
  NS_IMETHOD SetTextContent(const nsAString& aTextContent)
  {
    // Batch possible DOMSubtreeModified events.
    mozAutoSubtreeModified subtree(OwnerDoc(), nsnull);
    return SetNodeValue(aTextContent);
  }

  // Implementation for nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);

  virtual already_AddRefed<nsINodeList> GetChildren(PRUint32 aFilter);

  virtual nsIAtom *GetIDAttributeName() const;
  virtual already_AddRefed<nsINodeInfo> GetExistingAttrNameFromQName(const nsAString& aStr) const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);
  virtual bool GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute,
                         nsAString& aResult) const;
  virtual bool HasAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute) const;
  virtual const nsAttrName* GetAttrNameAt(PRUint32 aIndex) const;
  virtual PRUint32 GetAttrCount() const;
  virtual const nsTextFragment *GetText();
  virtual PRUint32 TextLength();
  virtual nsresult SetText(const PRUnichar* aBuffer, PRUint32 aLength,
                           bool aNotify);
  // Need to implement this here too to avoid hiding.
  nsresult SetText(const nsAString& aStr, bool aNotify)
  {
    return SetText(aStr.BeginReading(), aStr.Length(), aNotify);
  }
  virtual nsresult AppendText(const PRUnichar* aBuffer, PRUint32 aLength,
                              bool aNotify);
  virtual bool TextIsOnlyWhitespace();
  virtual void AppendTextTo(nsAString& aResult);
  virtual void DestroyContent();
  virtual void SaveSubtreeState();

  virtual nsISMILAttr* GetAnimatedAttr(PRInt32 /*aNamespaceID*/, nsIAtom* /*aName*/)
  {
    return nsnull;
  }
  virtual nsIDOMCSSStyleDeclaration* GetSMILOverrideStyle();
  virtual mozilla::css::StyleRule* GetSMILOverrideStyleRule();
  virtual nsresult SetSMILOverrideStyleRule(mozilla::css::StyleRule* aStyleRule,
                                            bool aNotify);

#ifdef DEBUG
  virtual void List(FILE* out, PRInt32 aIndent) const;
  virtual void DumpContent(FILE* out, PRInt32 aIndent, bool aDumpAll) const;
#endif

  virtual nsIContent *GetBindingParent() const;
  virtual bool IsNodeOfType(PRUint32 aFlags) const;
  virtual bool IsLink(nsIURI** aURI) const;

  virtual nsIAtom* DoGetID() const;
  virtual const nsAttrValue* DoGetClasses() const;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  virtual mozilla::css::StyleRule* GetInlineStyleRule();
  NS_IMETHOD SetInlineStyleRule(mozilla::css::StyleRule* aStyleRule, bool aNotify);
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;
  virtual nsIAtom *GetClassAttributeName() const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
  {
    *aResult = CloneDataNode(aNodeInfo, true);
    if (!*aResult) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*aResult);

    return NS_OK;
  }

  nsresult SplitData(PRUint32 aOffset, nsIContent** aReturn,
                     bool aCloneAfterOriginal = true);

  //----------------------------------------

#ifdef DEBUG
  void ToCString(nsAString& aBuf, PRInt32 aOffset, PRInt32 aLen) const;
#endif

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsGenericDOMDataNode)

protected:
  virtual mozilla::dom::Element* GetNameSpaceElement()
  {
    nsINode *parent = GetNodeParent();

    return parent && parent->IsElement() ? parent->AsElement() : nsnull;
  }

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
    nsDataSlots()
      : nsINode::nsSlots(),
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
    return static_cast<nsDataSlots*>(GetSlots());
  }

  nsDataSlots *GetExistingDataSlots() const
  {
    return static_cast<nsDataSlots*>(GetExistingSlots());
  }

  nsresult SplitText(PRUint32 aOffset, nsIDOMText** aReturn);

  nsresult GetWholeText(nsAString& aWholeText);

  static PRInt32 FirstLogicallyAdjacentTextNode(nsIContent* aParent,
                                                PRInt32 aIndex);

  static PRInt32 LastLogicallyAdjacentTextNode(nsIContent* aParent,
                                               PRInt32 aIndex,
                                               PRUint32 aCount);

  nsresult SetTextInternal(PRUint32 aOffset, PRUint32 aCount,
                           const PRUnichar* aBuffer, PRUint32 aLength,
                           bool aNotify,
                           CharacterDataChangeInfo::Details* aDetails = nsnull);

  /**
   * Method to clone this node. This needs to be overriden by all derived
   * classes. If aCloneText is true the text content will be cloned too.
   *
   * @param aOwnerDocument the ownerDocument of the clone
   * @param aCloneText if true the text content will be cloned too
   * @return the clone
   */
  virtual nsGenericDOMDataNode *CloneDataNode(nsINodeInfo *aNodeInfo,
                                              bool aCloneText) const = 0;

  nsTextFragment mText;

private:
  already_AddRefed<nsIAtom> GetCurrentValueAtom();
};

#endif /* nsGenericDOMDataNode_h___ */
