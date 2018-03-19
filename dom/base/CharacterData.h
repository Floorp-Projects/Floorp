/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for DOM Core's Comment, DocumentType, Text,
 * CDATASection, and ProcessingInstruction nodes.
 */

#ifndef mozilla_dom_CharacterData_h
#define mozilla_dom_CharacterData_h

#include "mozilla/Attributes.h"
#include "nsIContent.h"

#include "nsTextFragment.h"
#include "nsError.h"
#include "mozilla/dom/Element.h"
#include "nsCycleCollectionParticipant.h"

#include "nsISMILAttr.h"
#include "mozilla/dom/ShadowRoot.h"

class nsIDocument;

namespace mozilla {
namespace dom {
class HTMLSlotElement;
} // namespace dom
} // namespace mozilla

#define CHARACTER_DATA_FLAG_BIT(n_) NODE_FLAG_BIT(NODE_TYPE_SPECIFIC_BITS_OFFSET + (n_))

// Data node specific flags
enum {
  // This bit is set to indicate that if the text node changes to
  // non-whitespace, we may need to create a frame for it. This bit must
  // not be set on nodes that already have a frame.
  NS_CREATE_FRAME_IF_NON_WHITESPACE =     CHARACTER_DATA_FLAG_BIT(0),

  // This bit is set to indicate that if the text node changes to
  // whitespace, we may need to reframe it (or its ancestors).
  NS_REFRAME_IF_WHITESPACE =              CHARACTER_DATA_FLAG_BIT(1),

  // This bit is set to indicate that we have a cached
  // TextIsOnlyWhitespace value
  NS_CACHED_TEXT_IS_ONLY_WHITESPACE =     CHARACTER_DATA_FLAG_BIT(2),

  // This bit is only meaningful if the NS_CACHED_TEXT_IS_ONLY_WHITESPACE
  // bit is set, and if so it indicates whether we're only whitespace or
  // not.
  NS_TEXT_IS_ONLY_WHITESPACE =            CHARACTER_DATA_FLAG_BIT(3),

  // This bit is set if there is a NewlineProperty attached to the node
  // (used by nsTextFrame).
  NS_HAS_NEWLINE_PROPERTY =               CHARACTER_DATA_FLAG_BIT(4),

  // This bit is set if there is a FlowLengthProperty attached to the node
  // (used by nsTextFrame).
  NS_HAS_FLOWLENGTH_PROPERTY =            CHARACTER_DATA_FLAG_BIT(5),

  // This bit is set if the node may be modified frequently.  This is typically
  // specified if the instance is in <input> or <textarea>.
  NS_MAYBE_MODIFIED_FREQUENTLY =          CHARACTER_DATA_FLAG_BIT(6),
};

// Make sure we have enough space for those bits
ASSERT_NODE_FLAGS_SPACE(NODE_TYPE_SPECIFIC_BITS_OFFSET + 7);

#undef CHARACTER_DATA_FLAG_BIT

namespace mozilla {
namespace dom {

class CharacterData : public nsIContent
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_ADDSIZEOFEXCLUDINGTHIS

  explicit CharacterData(already_AddRefed<dom::NodeInfo>& aNodeInfo);
  explicit CharacterData(already_AddRefed<dom::NodeInfo>&& aNodeInfo);

  void MarkAsMaybeModifiedFrequently()
  {
    SetFlags(NS_MAYBE_MODIFIED_FREQUENTLY);
  }

  NS_IMPL_FROMCONTENT_HELPER(CharacterData, IsCharacterData())

  virtual void GetNodeValueInternal(nsAString& aNodeValue) override;
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    ErrorResult& aError) override;

  // Implementation for nsIDOMCharacterData
  nsresult GetLength(uint32_t* aLength);
  nsresult AppendData(const nsAString& aArg);
  nsresult InsertData(uint32_t aOffset, const nsAString& aArg);
  nsresult DeleteData(uint32_t aOffset, uint32_t aCount);
  nsresult ReplaceData(uint32_t aOffset, uint32_t aCount,
                       const nsAString& aArg);

  // nsINode methods
  virtual uint32_t GetChildCount() const override;
  virtual nsIContent *GetChildAt_Deprecated(uint32_t aIndex) const override;
  virtual int32_t ComputeIndexOf(const nsINode* aPossibleChild) const override;
  virtual nsresult InsertChildBefore(nsIContent* aKid, nsIContent* aBeforeThis,
                                     bool aNotify) override;
  virtual nsresult InsertChildAt_Deprecated(nsIContent* aKid, uint32_t aIndex,
                                            bool aNotify) override;
  virtual void RemoveChildAt_Deprecated(uint32_t aIndex, bool aNotify) override;
  virtual void RemoveChildNode(nsIContent* aKid, bool aNotify) override;
  virtual void GetTextContentInternal(nsAString& aTextContent,
                                      OOMReporter& aError) override
  {
    GetNodeValue(aTextContent);
  }
  virtual void SetTextContentInternal(const nsAString& aTextContent,
                                      nsIPrincipal* aSubjectPrincipal,
                                      ErrorResult& aError) override
  {
    // Batch possible DOMSubtreeModified events.
    mozAutoSubtreeModified subtree(OwnerDoc(), nullptr);
    return SetNodeValue(aTextContent, aError);
  }

  // Implementation for nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

  virtual already_AddRefed<nsINodeList> GetChildren(uint32_t aFilter) override;

  virtual const nsTextFragment *GetText() override;
  virtual uint32_t TextLength() const override;
  virtual nsresult SetText(const char16_t* aBuffer, uint32_t aLength,
                           bool aNotify) override;
  // Need to implement this here too to avoid hiding.
  nsresult SetText(const nsAString& aStr, bool aNotify)
  {
    return SetText(aStr.BeginReading(), aStr.Length(), aNotify);
  }
  virtual nsresult AppendText(const char16_t* aBuffer, uint32_t aLength,
                              bool aNotify) override;
  virtual bool TextIsOnlyWhitespace() override;
  bool ThreadSafeTextIsOnlyWhitespace() const final;
  virtual bool HasTextForTranslation() override;
  virtual void AppendTextTo(nsAString& aResult) override;
  MOZ_MUST_USE
  virtual bool AppendTextTo(nsAString& aResult,
                            const fallible_t&) override;
  virtual void SaveSubtreeState() override;

#ifdef DEBUG
  virtual void List(FILE* out, int32_t aIndent) const override;
  virtual void DumpContent(FILE* out, int32_t aIndent, bool aDumpAll) const override;
#endif

  virtual nsXBLBinding* DoGetXBLBinding() const override;
  virtual bool IsNodeOfType(uint32_t aFlags) const override;
  virtual bool IsLink(nsIURI** aURI) const override;

  virtual nsresult Clone(dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override
  {
    nsCOMPtr<nsINode> result = CloneDataNode(aNodeInfo, true);
    result.forget(aResult);

    if (!*aResult) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
  }

  // WebIDL API
  void GetData(nsAString& aData) const;
  virtual void SetData(const nsAString& aData, ErrorResult& rv);
  // nsINode::Length() returns the right thing for our length attribute
  void SubstringData(uint32_t aStart, uint32_t aCount, nsAString& aReturn,
                     ErrorResult& rv);
  void AppendData(const nsAString& aData, ErrorResult& rv)
  {
    rv = AppendData(aData);
  }
  void InsertData(uint32_t aOffset, const nsAString& aData, ErrorResult& rv)
  {
    rv = InsertData(aOffset, aData);
  }
  void DeleteData(uint32_t aOffset, uint32_t aCount, ErrorResult& rv)
  {
    rv = DeleteData(aOffset, aCount);
  }
  void ReplaceData(uint32_t aOffset, uint32_t aCount, const nsAString& aData,
                   ErrorResult& rv)
  {
    rv = ReplaceData(aOffset, aCount, aData);
  }

  uint32_t TextDataLength() const
  {
    return mText.GetLength();
  }

  //----------------------------------------

#ifdef DEBUG
  void ToCString(nsAString& aBuf, int32_t aOffset, int32_t aLen) const;
#endif

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(CharacterData)

protected:
  virtual ~CharacterData();

  virtual Element* GetNameSpaceElement() override
  {
    nsINode *parent = GetParentNode();

    return parent && parent->IsElement() ? parent->AsElement() : nullptr;
  }

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
  virtual CharacterData *CloneDataNode(dom::NodeInfo *aNodeInfo,
                                       bool aCloneText) const = 0;

  nsTextFragment mText;

public:
  virtual bool OwnedOnlyByTheDOMTree() override
  {
    return GetParent() && mRefCnt.get() == 1;
  }

  virtual bool IsPurple() override
  {
    return mRefCnt.IsPurple();
  }
  virtual void RemovePurple() override
  {
    mRefCnt.RemovePurple();
  }

private:
  already_AddRefed<nsAtom> GetCurrentValueAtom();
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_CharacterData_h */
