/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for DOM Core's nsIDOMComment, nsIDOMDocumentType, nsIDOMText,
 * nsIDOMCDATASection, and nsIDOMProcessingInstruction nodes.
 */

#ifndef nsGenericDOMDataNode_h___
#define nsGenericDOMDataNode_h___

#include "mozilla/Attributes.h"
#include "nsIContent.h"

#include "nsTextFragment.h"
#include "nsError.h"
#include "mozilla/dom/Element.h"
#include "nsCycleCollectionParticipant.h"

#include "nsISMILAttr.h"
#include "nsIDocument.h"
#include "mozilla/dom/ShadowRoot.h"

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIDOMNodeList;
class nsIFrame;
class nsIDOMText;
class nsURI;

#define DATA_NODE_FLAG_BIT(n_) NODE_FLAG_BIT(NODE_TYPE_SPECIFIC_BITS_OFFSET + (n_))

// Data node specific flags
enum {
  // This bit is set to indicate that if the text node changes to
  // non-whitespace, we may need to create a frame for it. This bit must
  // not be set on nodes that already have a frame.
  NS_CREATE_FRAME_IF_NON_WHITESPACE =     DATA_NODE_FLAG_BIT(0),

  // This bit is set to indicate that if the text node changes to
  // whitespace, we may need to reframe it (or its ancestors).
  NS_REFRAME_IF_WHITESPACE =              DATA_NODE_FLAG_BIT(1),

  // This bit is set to indicate that we have a cached
  // TextIsOnlyWhitespace value
  NS_CACHED_TEXT_IS_ONLY_WHITESPACE =     DATA_NODE_FLAG_BIT(2),

  // This bit is only meaningful if the NS_CACHED_TEXT_IS_ONLY_WHITESPACE
  // bit is set, and if so it indicates whether we're only whitespace or
  // not.
  NS_TEXT_IS_ONLY_WHITESPACE =            DATA_NODE_FLAG_BIT(3),
};

// Make sure we have enough space for those bits
ASSERT_NODE_FLAGS_SPACE(NODE_TYPE_SPECIFIC_BITS_OFFSET + 4);

#undef DATA_NODE_FLAG_BIT

class nsGenericDOMDataNode : public nsIContent
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_SIZEOF_EXCLUDING_THIS

  nsGenericDOMDataNode(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  nsGenericDOMDataNode(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  virtual void GetNodeValueInternal(nsAString& aNodeValue) MOZ_OVERRIDE;
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    mozilla::ErrorResult& aError) MOZ_OVERRIDE;

  // Implementation for nsIDOMCharacterData
  nsresult GetData(nsAString& aData) const;
  nsresult SetData(const nsAString& aData);
  nsresult GetLength(uint32_t* aLength);
  nsresult SubstringData(uint32_t aOffset, uint32_t aCount,
                         nsAString& aReturn);
  nsresult AppendData(const nsAString& aArg);
  nsresult InsertData(uint32_t aOffset, const nsAString& aArg);
  nsresult DeleteData(uint32_t aOffset, uint32_t aCount);
  nsresult ReplaceData(uint32_t aOffset, uint32_t aCount,
                       const nsAString& aArg);
  NS_IMETHOD MozRemove();

  // nsINode methods
  virtual uint32_t GetChildCount() const MOZ_OVERRIDE;
  virtual nsIContent *GetChildAt(uint32_t aIndex) const MOZ_OVERRIDE;
  virtual nsIContent * const * GetChildArray(uint32_t* aChildCount) const MOZ_OVERRIDE;
  virtual int32_t IndexOf(const nsINode* aPossibleChild) const MOZ_OVERRIDE;
  virtual nsresult InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                                 bool aNotify) MOZ_OVERRIDE;
  virtual void RemoveChildAt(uint32_t aIndex, bool aNotify) MOZ_OVERRIDE;
  virtual void GetTextContentInternal(nsAString& aTextContent,
                                      mozilla::ErrorResult& aError) MOZ_OVERRIDE
  {
    GetNodeValue(aTextContent);
  }
  virtual void SetTextContentInternal(const nsAString& aTextContent,
                                      mozilla::ErrorResult& aError) MOZ_OVERRIDE
  {
    // Batch possible DOMSubtreeModified events.
    mozAutoSubtreeModified subtree(OwnerDoc(), nullptr);
    return SetNodeValue(aTextContent, aError);
  }

  // Implementation for nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;

  virtual already_AddRefed<nsINodeList> GetChildren(uint32_t aFilter) MOZ_OVERRIDE;


  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) MOZ_OVERRIDE;
  virtual const nsAttrName* GetAttrNameAt(uint32_t aIndex) const MOZ_OVERRIDE;
  virtual uint32_t GetAttrCount() const MOZ_OVERRIDE;
  virtual const nsTextFragment *GetText() MOZ_OVERRIDE;
  virtual uint32_t TextLength() const MOZ_OVERRIDE;
  virtual nsresult SetText(const char16_t* aBuffer, uint32_t aLength,
                           bool aNotify) MOZ_OVERRIDE;
  // Need to implement this here too to avoid hiding.
  nsresult SetText(const nsAString& aStr, bool aNotify)
  {
    return SetText(aStr.BeginReading(), aStr.Length(), aNotify);
  }
  virtual nsresult AppendText(const char16_t* aBuffer, uint32_t aLength,
                              bool aNotify) MOZ_OVERRIDE;
  virtual bool TextIsOnlyWhitespace() MOZ_OVERRIDE;
  virtual bool HasTextForTranslation() MOZ_OVERRIDE;
  virtual void AppendTextTo(nsAString& aResult) MOZ_OVERRIDE;
  virtual bool AppendTextTo(nsAString& aResult,
                            const mozilla::fallible_t&) MOZ_OVERRIDE NS_WARN_UNUSED_RESULT;
  virtual void DestroyContent() MOZ_OVERRIDE;
  virtual void SaveSubtreeState() MOZ_OVERRIDE;

#ifdef DEBUG
  virtual void List(FILE* out, int32_t aIndent) const MOZ_OVERRIDE;
  virtual void DumpContent(FILE* out, int32_t aIndent, bool aDumpAll) const MOZ_OVERRIDE;
#endif

  virtual nsIContent *GetBindingParent() const MOZ_OVERRIDE;
  virtual nsXBLBinding *GetXBLBinding() const MOZ_OVERRIDE;
  virtual void SetXBLBinding(nsXBLBinding* aBinding,
                             nsBindingManager* aOldBindingManager = nullptr) MOZ_OVERRIDE;
  virtual mozilla::dom::ShadowRoot *GetContainingShadow() const MOZ_OVERRIDE;
  virtual mozilla::dom::ShadowRoot *GetShadowRoot() const MOZ_OVERRIDE;
  virtual nsTArray<nsIContent*> &DestInsertionPoints() MOZ_OVERRIDE;
  virtual nsTArray<nsIContent*> *GetExistingDestInsertionPoints() const MOZ_OVERRIDE;
  virtual void SetShadowRoot(mozilla::dom::ShadowRoot* aShadowRoot) MOZ_OVERRIDE;
  virtual nsIContent *GetXBLInsertionParent() const MOZ_OVERRIDE;
  virtual void SetXBLInsertionParent(nsIContent* aContent) MOZ_OVERRIDE;
  virtual bool IsNodeOfType(uint32_t aFlags) const MOZ_OVERRIDE;
  virtual bool IsLink(nsIURI** aURI) const MOZ_OVERRIDE;

  virtual mozilla::dom::CustomElementData* GetCustomElementData() const MOZ_OVERRIDE;
  virtual void SetCustomElementData(mozilla::dom::CustomElementData* aData) MOZ_OVERRIDE;

  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              int32_t aModType) const;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE
  {
    *aResult = CloneDataNode(aNodeInfo, true);
    if (!*aResult) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*aResult);

    return NS_OK;
  }

  nsresult SplitData(uint32_t aOffset, nsIContent** aReturn,
                     bool aCloneAfterOriginal = true);

  // WebIDL API
  // Our XPCOM GetData is just fine for WebIDL
  virtual void SetData(const nsAString& aData, mozilla::ErrorResult& rv)
  {
    rv = SetData(aData);
  }
  // nsINode::Length() returns the right thing for our length attribute
  void SubstringData(uint32_t aStart, uint32_t aCount, nsAString& aReturn,
                     mozilla::ErrorResult& rv);
  void AppendData(const nsAString& aData, mozilla::ErrorResult& rv)
  {
    rv = AppendData(aData);
  }
  void InsertData(uint32_t aOffset, const nsAString& aData,
                  mozilla::ErrorResult& rv)
  {
    rv = InsertData(aOffset, aData);
  }
  void DeleteData(uint32_t aOffset, uint32_t aCount, mozilla::ErrorResult& rv)
  {
    rv = DeleteData(aOffset, aCount);
  }
  void ReplaceData(uint32_t aOffset, uint32_t aCount, const nsAString& aData,
                   mozilla::ErrorResult& rv)
  {
    rv = ReplaceData(aOffset, aCount, aData);
  }

  //----------------------------------------

#ifdef DEBUG
  void ToCString(nsAString& aBuf, int32_t aOffset, int32_t aLen) const;
#endif

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(nsGenericDOMDataNode)

protected:
  virtual ~nsGenericDOMDataNode();

  virtual mozilla::dom::Element* GetNameSpaceElement()
  {
    nsINode *parent = GetParentNode();

    return parent && parent->IsElement() ? parent->AsElement() : nullptr;
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
    nsDataSlots();

    void Traverse(nsCycleCollectionTraversalCallback &cb);
    void Unlink();

    /**
     * The nearest enclosing content node with a binding that created us.
     * @see nsIContent::GetBindingParent
     */
    nsIContent* mBindingParent;  // [Weak]

    /**
     * @see nsIContent::GetXBLInsertionParent
     */
    nsCOMPtr<nsIContent> mXBLInsertionParent;

    /**
     * @see nsIContent::GetContainingShadow
     */
    nsRefPtr<mozilla::dom::ShadowRoot> mContainingShadow;

    /**
     * @see nsIContent::GetDestInsertionPoints
     */
    nsTArray<nsIContent*> mDestInsertionPoints;
  };

  // Override from nsINode
  virtual nsINode::nsSlots* CreateSlots() MOZ_OVERRIDE;

  nsDataSlots* DataSlots()
  {
    return static_cast<nsDataSlots*>(Slots());
  }

  nsDataSlots *GetExistingDataSlots() const
  {
    return static_cast<nsDataSlots*>(GetExistingSlots());
  }

  nsresult SplitText(uint32_t aOffset, nsIDOMText** aReturn);

  nsresult GetWholeText(nsAString& aWholeText);

  static int32_t FirstLogicallyAdjacentTextNode(nsIContent* aParent,
                                                int32_t aIndex);

  static int32_t LastLogicallyAdjacentTextNode(nsIContent* aParent,
                                               int32_t aIndex,
                                               uint32_t aCount);

  nsresult SetTextInternal(uint32_t aOffset, uint32_t aCount,
                           const char16_t* aBuffer, uint32_t aLength,
                           bool aNotify,
                           CharacterDataChangeInfo::Details* aDetails = nullptr);

  /**
   * Method to clone this node. This needs to be overriden by all derived
   * classes. If aCloneText is true the text content will be cloned too.
   *
   * @param aOwnerDocument the ownerDocument of the clone
   * @param aCloneText if true the text content will be cloned too
   * @return the clone
   */
  virtual nsGenericDOMDataNode *CloneDataNode(mozilla::dom::NodeInfo *aNodeInfo,
                                              bool aCloneText) const = 0;

  nsTextFragment mText;

public:
  virtual bool OwnedOnlyByTheDOMTree() MOZ_OVERRIDE
  {
    return GetParent() && mRefCnt.get() == 1;
  }

  virtual bool IsPurple() MOZ_OVERRIDE
  {
    return mRefCnt.IsPurple();
  }
  virtual void RemovePurple() MOZ_OVERRIDE
  {
    mRefCnt.RemovePurple();
  }
  
private:
  already_AddRefed<nsIAtom> GetCurrentValueAtom();
};

#endif /* nsGenericDOMDataNode_h___ */
