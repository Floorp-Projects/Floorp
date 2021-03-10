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
#include "nsCycleCollectionParticipant.h"

#include "mozilla/dom/ShadowRoot.h"

namespace mozilla {
namespace dom {
class Element;
class HTMLSlotElement;
}  // namespace dom
}  // namespace mozilla

#define CHARACTER_DATA_FLAG_BIT(n_) \
  NODE_FLAG_BIT(NODE_TYPE_SPECIFIC_BITS_OFFSET + (n_))

// Data node specific flags
enum {
  // This bit is set to indicate that if the text node changes to
  // non-whitespace, we may need to create a frame for it. This bit must
  // not be set on nodes that already have a frame.
  NS_CREATE_FRAME_IF_NON_WHITESPACE = CHARACTER_DATA_FLAG_BIT(0),

  // This bit is set to indicate that if the text node changes to
  // whitespace, we may need to reframe it (or its ancestors).
  NS_REFRAME_IF_WHITESPACE = CHARACTER_DATA_FLAG_BIT(1),

  // This bit is set to indicate that we have a cached
  // TextIsOnlyWhitespace value
  NS_CACHED_TEXT_IS_ONLY_WHITESPACE = CHARACTER_DATA_FLAG_BIT(2),

  // This bit is only meaningful if the NS_CACHED_TEXT_IS_ONLY_WHITESPACE
  // bit is set, and if so it indicates whether we're only whitespace or
  // not.
  NS_TEXT_IS_ONLY_WHITESPACE = CHARACTER_DATA_FLAG_BIT(3),

  // This bit is set if there is a NewlineProperty attached to the node
  // (used by nsTextFrame).
  NS_HAS_NEWLINE_PROPERTY = CHARACTER_DATA_FLAG_BIT(4),

  // This bit is set if there is a FlowLengthProperty attached to the node
  // (used by nsTextFrame).
  NS_HAS_FLOWLENGTH_PROPERTY = CHARACTER_DATA_FLAG_BIT(5),

  // This bit is set if the node may be modified frequently.  This is typically
  // specified if the instance is in <input> or <textarea>.
  NS_MAYBE_MODIFIED_FREQUENTLY = CHARACTER_DATA_FLAG_BIT(6),

  // This bit is set if the node may be masked because of being in a password
  // field.
  NS_MAYBE_MASKED = CHARACTER_DATA_FLAG_BIT(7),
};

// Make sure we have enough space for those bits
ASSERT_NODE_FLAGS_SPACE(NODE_TYPE_SPECIFIC_BITS_OFFSET + 8);

#undef CHARACTER_DATA_FLAG_BIT

namespace mozilla {
namespace dom {

class CharacterData : public nsIContent {
 public:
  // We want to avoid the overhead of extra function calls for
  // refcounting when we're not doing refcount logging, so we can't
  // NS_DECL_ISUPPORTS_INHERITED.
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  NS_INLINE_DECL_REFCOUNTING_INHERITED(CharacterData, nsIContent);

  NS_DECL_ADDSIZEOFEXCLUDINGTHIS

  explicit CharacterData(already_AddRefed<dom::NodeInfo>&& aNodeInfo);

  void MarkAsMaybeModifiedFrequently() {
    SetFlags(NS_MAYBE_MODIFIED_FREQUENTLY);
  }
  void MarkAsMaybeMasked() { SetFlags(NS_MAYBE_MASKED); }

  NS_IMPL_FROMNODE_HELPER(CharacterData, IsCharacterData())

  virtual void GetNodeValueInternal(nsAString& aNodeValue) override;
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    ErrorResult& aError) override;

  void GetTextContentInternal(nsAString& aTextContent, OOMReporter&) final {
    GetNodeValue(aTextContent);
  }

  void SetTextContentInternal(const nsAString& aTextContent,
                              nsIPrincipal* aSubjectPrincipal,
                              ErrorResult& aError) final;

  // Implementation for nsIContent
  nsresult BindToTree(BindContext&, nsINode& aParent) override;

  void UnbindFromTree(bool aNullParent = true) override;

  already_AddRefed<nsINodeList> GetChildren(uint32_t aFilter) final {
    return nullptr;
  }

  const nsTextFragment* GetText() override { return &mText; }
  uint32_t TextLength() const final { return TextDataLength(); }

  const nsTextFragment& TextFragment() const { return mText; }
  uint32_t TextDataLength() const { return mText.GetLength(); }

  /**
   * Set the text to the given value. If aNotify is true then
   * the document is notified of the content change.
   */
  nsresult SetText(const char16_t* aBuffer, uint32_t aLength, bool aNotify);
  /**
   * Append the given value to the current text. If aNotify is true then
   * the document is notified of the content change.
   */
  nsresult SetText(const nsAString& aStr, bool aNotify) {
    return SetText(aStr.BeginReading(), aStr.Length(), aNotify);
  }

  /**
   * Append the given value to the current text. If aNotify is true then
   * the document is notified of the content change.
   */
  nsresult AppendText(const char16_t* aBuffer, uint32_t aLength, bool aNotify);

  bool TextIsOnlyWhitespace() final;
  bool ThreadSafeTextIsOnlyWhitespace() const final;

  /**
   * Append the text content to aResult.
   */
  void AppendTextTo(nsAString& aResult) const { mText.AppendTo(aResult); }

  /**
   * Append the text content to aResult.
   */
  [[nodiscard]] bool AppendTextTo(nsAString& aResult,
                                  const fallible_t& aFallible) const {
    return mText.AppendTo(aResult, aFallible);
  }

  void SaveSubtreeState() final {}

#ifdef MOZ_DOM_LIST
  void ToCString(nsAString& aBuf, int32_t aOffset, int32_t aLen) const;

  void List(FILE* out, int32_t aIndent) const override {}

  void DumpContent(FILE* out, int32_t aIndent, bool aDumpAll) const override {}
#endif

  bool IsNodeOfType(uint32_t aFlags) const override { return false; }

  bool IsLink(nsIURI** aURI) const final {
    *aURI = nullptr;
    return false;
  }

  nsresult Clone(dom::NodeInfo* aNodeInfo, nsINode** aResult) const override {
    RefPtr<CharacterData> result = CloneDataNode(aNodeInfo, true);
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
  void AppendData(const nsAString& aData, ErrorResult& rv);
  void InsertData(uint32_t aOffset, const nsAString& aData, ErrorResult& rv);
  void DeleteData(uint32_t aOffset, uint32_t aCount, ErrorResult& rv);
  void ReplaceData(uint32_t aOffset, uint32_t aCount, const nsAString& aData,
                   ErrorResult& rv);

  //----------------------------------------

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_INHERITED(
      CharacterData, nsIContent)

  /**
   * Compare two CharacterData nodes for text equality.
   */
  [[nodiscard]] bool TextEquals(const CharacterData* aOther) const {
    return mText.TextEquals(aOther->mText);
  }

 protected:
  virtual ~CharacterData();

  Element* GetNameSpaceElement() final;

  nsresult SetTextInternal(
      uint32_t aOffset, uint32_t aCount, const char16_t* aBuffer,
      uint32_t aLength, bool aNotify,
      CharacterDataChangeInfo::Details* aDetails = nullptr);

  /**
   * Method to clone this node. This needs to be overriden by all derived
   * classes. If aCloneText is true the text content will be cloned too.
   *
   * @param aOwnerDocument the ownerDocument of the clone
   * @param aCloneText if true the text content will be cloned too
   * @return the clone
   */
  virtual already_AddRefed<CharacterData> CloneDataNode(
      dom::NodeInfo* aNodeInfo, bool aCloneText) const = 0;

  nsTextFragment mText;

 private:
  already_AddRefed<nsAtom> GetCurrentValueAtom();
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_CharacterData_h */
