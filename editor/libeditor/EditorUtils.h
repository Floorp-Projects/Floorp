/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorUtils_h
#define mozilla_EditorUtils_h

#include "mozilla/EditorBase.h"      // for EditorBase
#include "mozilla/EditorDOMPoint.h"  // for EditorDOMPoint, EditorDOMRange, etc
#include "mozilla/EditorForwards.h"
#include "mozilla/IntegerRange.h"       // for IntegerRange
#include "mozilla/Maybe.h"              // for Maybe
#include "mozilla/Result.h"             // for Result<>
#include "mozilla/dom/Element.h"        // for dom::Element
#include "mozilla/dom/HTMLBRElement.h"  // for dom::HTMLBRElement
#include "mozilla/dom/Selection.h"      // for dom::Selection
#include "mozilla/dom/Text.h"           // for dom::Text

#include "nsAtom.h"          // for nsStaticAtom
#include "nsCOMPtr.h"        // for nsCOMPtr
#include "nsContentUtils.h"  // for nsContentUtils
#include "nsDebug.h"         // for NS_WARNING, etc
#include "nsError.h"         // for NS_SUCCESS_* and NS_ERROR_*
#include "nsRange.h"         // for nsRange
#include "nsString.h"        // for nsAString, nsString, etc

class nsITransferable;

namespace mozilla {

enum class StyleWhiteSpace : uint8_t;

enum class SuggestCaret {
  // If specified, the method returns NS_OK when there is no recommended caret
  // position.
  OnlyIfHasSuggestion,
  // If specified and if EditorBase::AllowsTransactionsToChangeSelection
  // returns false, the method does nothing and returns NS_OK.
  OnlyIfTransactionsAllowedToDoIt,
  // If specified, the method returns
  // NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR even if
  // EditorBase::CollapseSelectionTo returns an error except when
  // NS_ERROR_EDITOR_DESTROYED.
  AndIgnoreTrivialError,
};

/******************************************************************************
 * CaretPoint is a wrapper of EditorDOMPoint and provides a helper method to
 * collapse Selection there, or move it to a local variable.  This is typically
 * used as the ok type of Result or a base class of DoSomethingResult classes.
 ******************************************************************************/
class MOZ_STACK_CLASS CaretPoint {
 public:
  explicit CaretPoint(const EditorDOMPoint& aPointToPutCaret)
      : mCaretPoint(aPointToPutCaret) {}
  explicit CaretPoint(EditorDOMPoint&& aPointToPutCaret)
      : mCaretPoint(std::move(aPointToPutCaret)) {}

  CaretPoint(const CaretPoint&) = delete;
  CaretPoint& operator=(const CaretPoint&) = delete;
  CaretPoint(CaretPoint&&) = default;
  CaretPoint& operator=(CaretPoint&&) = default;

  /**
   * Suggest caret position to aEditorBase.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult SuggestCaretPointTo(
      EditorBase& aEditorBase, const SuggestCaretOptions& aOptions) const;

  /**
   * IgnoreCaretPointSuggestion() should be called if the method does not want
   * to use caret position recommended by this instance.
   */
  void IgnoreCaretPointSuggestion() const { mHandledCaretPoint = true; }

  /**
   * When propagating the result, it may not want to the caller modify
   * selection.  In such case, this can clear the caret point.  Use
   * IgnoreCaretPointSuggestion() in the caller side instead.
   */
  void ForgetCaretPointSuggestion() { mCaretPoint.Clear(); }

  bool HasCaretPointSuggestion() const { return mCaretPoint.IsSet(); }
  constexpr const EditorDOMPoint& CaretPointRef() const { return mCaretPoint; }
  constexpr EditorDOMPoint&& UnwrapCaretPoint() {
    mHandledCaretPoint = true;
    return std::move(mCaretPoint);
  }
  bool CopyCaretPointTo(EditorDOMPoint& aPointToPutCaret,
                        const SuggestCaretOptions& aOptions) const {
    MOZ_ASSERT(!aOptions.contains(SuggestCaret::AndIgnoreTrivialError));
    MOZ_ASSERT(
        !aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt));
    mHandledCaretPoint = true;
    if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion) &&
        !mCaretPoint.IsSet()) {
      return false;
    }
    aPointToPutCaret = mCaretPoint;
    return true;
  }
  bool MoveCaretPointTo(EditorDOMPoint& aPointToPutCaret,
                        const SuggestCaretOptions& aOptions) {
    MOZ_ASSERT(!aOptions.contains(SuggestCaret::AndIgnoreTrivialError));
    MOZ_ASSERT(
        !aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt));
    if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion) &&
        !mCaretPoint.IsSet()) {
      return false;
    }
    aPointToPutCaret = UnwrapCaretPoint();
    return true;
  }
  bool CopyCaretPointTo(EditorDOMPoint& aPointToPutCaret,
                        const EditorBase& aEditorBase,
                        const SuggestCaretOptions& aOptions) const;
  bool MoveCaretPointTo(EditorDOMPoint& aPointToPutCaret,
                        const EditorBase& aEditorBase,
                        const SuggestCaretOptions& aOptions);

 protected:
  constexpr bool CaretPointHandled() const { return mHandledCaretPoint; }

  void SetCaretPoint(const EditorDOMPoint& aCaretPoint) {
    mHandledCaretPoint = false;
    mCaretPoint = aCaretPoint;
  }
  void SetCaretPoint(EditorDOMPoint&& aCaretPoint) {
    mHandledCaretPoint = false;
    mCaretPoint = std::move(aCaretPoint);
  }

  void UnmarkAsHandledCaretPoint() { mHandledCaretPoint = true; }

  CaretPoint() = default;

 private:
  EditorDOMPoint mCaretPoint;
  bool mutable mHandledCaretPoint = false;
};

/***************************************************************************
 * EditActionResult is useful to return the handling state of edit sub actions
 * without out params.
 */
class MOZ_STACK_CLASS EditActionResult final {
 public:
  bool Canceled() const { return mCanceled; }
  bool Handled() const { return mHandled; }
  bool Ignored() const { return !mCanceled && !mHandled; }

  void MarkAsCanceled() { mCanceled = true; }
  void MarkAsHandled() { mHandled = true; }

  EditActionResult& operator|=(const EditActionResult& aOther) {
    mCanceled |= aOther.mCanceled;
    mHandled |= aOther.mHandled;
    return *this;
  }

  EditActionResult& operator|=(const MoveNodeResult& aMoveNodeResult);

  static EditActionResult IgnoredResult() {
    return EditActionResult(false, false);
  }
  static EditActionResult HandledResult() {
    return EditActionResult(false, true);
  }
  static EditActionResult CanceledResult() {
    return EditActionResult(true, true);
  }

  EditActionResult(const EditActionResult&) = delete;
  EditActionResult& operator=(const EditActionResult&) = delete;
  EditActionResult(EditActionResult&&) = default;
  EditActionResult& operator=(EditActionResult&&) = default;

 private:
  bool mCanceled = false;
  bool mHandled = false;

  EditActionResult(bool aCanceled, bool aHandled)
      : mCanceled(aCanceled), mHandled(aHandled) {}

  EditActionResult() : mCanceled(false), mHandled(false) {}
};

/***************************************************************************
 * CreateNodeResultBase is a simple class for CreateSomething() methods
 * which want to return new node.
 */
template <typename NodeType>
class MOZ_STACK_CLASS CreateNodeResultBase final : public CaretPoint {
  using SelfType = CreateNodeResultBase<NodeType>;

 public:
  bool Handled() const { return mNode; }
  NodeType* GetNewNode() const { return mNode; }
  RefPtr<NodeType> UnwrapNewNode() { return std::move(mNode); }

  CreateNodeResultBase() = delete;
  explicit CreateNodeResultBase(NodeType& aNode) : mNode(&aNode) {}
  explicit CreateNodeResultBase(NodeType& aNode,
                                const EditorDOMPoint& aCandidateCaretPoint)
      : CaretPoint(aCandidateCaretPoint), mNode(&aNode) {}
  explicit CreateNodeResultBase(NodeType& aNode,
                                EditorDOMPoint&& aCandidateCaretPoint)
      : CaretPoint(std::move(aCandidateCaretPoint)), mNode(&aNode) {}

  explicit CreateNodeResultBase(RefPtr<NodeType>&& aNode)
      : mNode(std::move(aNode)) {}
  explicit CreateNodeResultBase(RefPtr<NodeType>&& aNode,
                                const EditorDOMPoint& aCandidateCaretPoint)
      : CaretPoint(aCandidateCaretPoint), mNode(std::move(aNode)) {
    MOZ_ASSERT(mNode);
  }
  explicit CreateNodeResultBase(RefPtr<NodeType>&& aNode,
                                EditorDOMPoint&& aCandidateCaretPoint)
      : CaretPoint(std::move(aCandidateCaretPoint)), mNode(std::move(aNode)) {
    MOZ_ASSERT(mNode);
  }

  [[nodiscard]] static SelfType NotHandled() {
    return SelfType(EditorDOMPoint());
  }
  [[nodiscard]] static SelfType NotHandled(
      const EditorDOMPoint& aPointToPutCaret) {
    SelfType result(aPointToPutCaret);
    return result;
  }
  [[nodiscard]] static SelfType NotHandled(EditorDOMPoint&& aPointToPutCaret) {
    SelfType result(std::move(aPointToPutCaret));
    return result;
  }

#ifdef DEBUG
  ~CreateNodeResultBase() {
    MOZ_ASSERT(!HasCaretPointSuggestion() || CaretPointHandled());
  }
#endif

  CreateNodeResultBase(const SelfType& aOther) = delete;
  SelfType& operator=(const SelfType& aOther) = delete;
  CreateNodeResultBase(SelfType&& aOther) = default;
  SelfType& operator=(SelfType&& aOther) = default;

 private:
  explicit CreateNodeResultBase(const EditorDOMPoint& aCandidateCaretPoint)
      : CaretPoint(aCandidateCaretPoint) {}
  explicit CreateNodeResultBase(EditorDOMPoint&& aCandidateCaretPoint)
      : CaretPoint(std::move(aCandidateCaretPoint)) {}

  RefPtr<NodeType> mNode;
};

/**
 * This is a result of inserting text.  If the text inserted as a part of
 * composition, this does not return CaretPoint.  Otherwise, must return
 * CaretPoint which is typically same as end of inserted text.
 */
class MOZ_STACK_CLASS InsertTextResult final : public CaretPoint {
 public:
  InsertTextResult() : CaretPoint(EditorDOMPoint()) {}
  template <typename EditorDOMPointType>
  explicit InsertTextResult(const EditorDOMPointType& aEndOfInsertedText)
      : CaretPoint(EditorDOMPoint()),
        mEndOfInsertedText(aEndOfInsertedText.template To<EditorDOMPoint>()) {}
  explicit InsertTextResult(EditorDOMPointInText&& aEndOfInsertedText)
      : CaretPoint(EditorDOMPoint()),
        mEndOfInsertedText(std::move(aEndOfInsertedText)) {}
  template <typename PT, typename CT>
  InsertTextResult(EditorDOMPointInText&& aEndOfInsertedText,
                   const EditorDOMPointBase<PT, CT>& aCaretPoint)
      : CaretPoint(aCaretPoint.template To<EditorDOMPoint>()),
        mEndOfInsertedText(std::move(aEndOfInsertedText)) {}
  InsertTextResult(EditorDOMPointInText&& aEndOfInsertedText,
                   CaretPoint&& aCaretPoint)
      : CaretPoint(std::move(aCaretPoint)),
        mEndOfInsertedText(std::move(aEndOfInsertedText)) {
    UnmarkAsHandledCaretPoint();
  }
  InsertTextResult(InsertTextResult&& aOther, EditorDOMPoint&& aCaretPoint)
      : CaretPoint(std::move(aCaretPoint)),
        mEndOfInsertedText(std::move(aOther.mEndOfInsertedText)) {}

  [[nodiscard]] bool Handled() const { return mEndOfInsertedText.IsSet(); }
  const EditorDOMPointInText& EndOfInsertedTextRef() const {
    return mEndOfInsertedText;
  }

 private:
  EditorDOMPointInText mEndOfInsertedText;
};

/***************************************************************************
 * stack based helper class for calling EditorBase::EndTransaction() after
 * EditorBase::BeginTransaction().  This shouldn't be used in editor classes
 * or helper classes while an edit action is being handled.  Use
 * AutoTransactionBatch in such cases since it uses non-virtual internal
 * methods.
 ***************************************************************************/
class MOZ_RAII AutoTransactionBatchExternal final {
 public:
  MOZ_CAN_RUN_SCRIPT explicit AutoTransactionBatchExternal(
      EditorBase& aEditorBase)
      : mEditorBase(aEditorBase) {
    MOZ_KnownLive(mEditorBase).BeginTransaction();
  }

  MOZ_CAN_RUN_SCRIPT ~AutoTransactionBatchExternal() {
    MOZ_KnownLive(mEditorBase).EndTransaction();
  }

 private:
  EditorBase& mEditorBase;
};

/******************************************************************************
 * AutoSelectionRangeArray stores all ranges in `aSelection`.
 * Note that modifying the ranges means modifing the selection ranges.
 *****************************************************************************/
class MOZ_STACK_CLASS AutoSelectionRangeArray final {
 public:
  explicit AutoSelectionRangeArray(dom::Selection& aSelection) {
    for (const uint32_t i : IntegerRange(aSelection.RangeCount())) {
      MOZ_ASSERT(aSelection.GetRangeAt(i));
      mRanges.AppendElement(*aSelection.GetRangeAt(i));
    }
  }

  AutoTArray<mozilla::OwningNonNull<nsRange>, 8> mRanges;
};

class EditorUtils final {
 public:
  using EditorType = EditorBase::EditorType;
  using Selection = dom::Selection;

  /**
   * IsDescendantOf() checks if aNode is a child or a descendant of aParent.
   * aOutPoint is set to the child of aParent.
   *
   * @return            true if aNode is a child or a descendant of aParent.
   */
  static bool IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                             EditorRawDOMPoint* aOutPoint = nullptr);
  static bool IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                             EditorDOMPoint* aOutPoint);

  /**
   * Returns true if aContent is a <br> element and it's marked as padding for
   * empty editor.
   */
  static bool IsPaddingBRElementForEmptyEditor(const nsIContent& aContent) {
    const dom::HTMLBRElement* brElement =
        dom::HTMLBRElement::FromNode(&aContent);
    return brElement && brElement->IsPaddingForEmptyEditor();
  }

  /**
   * Returns true if aContent is a <br> element and it's marked as padding for
   * empty last line.
   */
  static bool IsPaddingBRElementForEmptyLastLine(const nsIContent& aContent) {
    const dom::HTMLBRElement* brElement =
        dom::HTMLBRElement::FromNode(&aContent);
    return brElement && brElement->IsPaddingForEmptyLastLine();
  }

  /**
   * IsEditableContent() returns true if aContent's data or children is ediable
   * for the given editor type.  Be aware, returning true does NOT mean the
   * node can be removed from its parent node, and returning false does NOT
   * mean the node cannot be removed from the parent node.
   * XXX May be the anonymous nodes in TextEditor not editable?  If it's not
   *     so, we can get rid of aEditorType.
   */
  static bool IsEditableContent(const nsIContent& aContent,
                                EditorType aEditorType) {
    if (aEditorType == EditorType::HTML &&
        (!aContent.IsEditable() || !aContent.IsInComposedDoc())) {
      // FIXME(emilio): Why only for HTML editors? All content from the root
      // content in text editors is also editable, so afaict we can remove the
      // special-case.
      return false;
    }
    return IsElementOrText(aContent);
  }

  /**
   * Returns true if aContent is a usual element node (not padding <br> element
   * for empty editor) or a text node.  In other words, returns true if
   * aContent is a usual element node or visible data node.
   */
  static bool IsElementOrText(const nsIContent& aContent) {
    if (aContent.IsText()) {
      return true;
    }
    return aContent.IsElement() && !IsPaddingBRElementForEmptyEditor(aContent);
  }

  /**
   * Get computed white-space style of aContent.
   */
  static Maybe<StyleWhiteSpace> GetComputedWhiteSpaceStyle(
      const nsIContent& aContent);

  /**
   * IsWhiteSpacePreformatted() checks the style info for the node for the
   * preformatted text style.  This does NOT flush layout.
   */
  static bool IsWhiteSpacePreformatted(const nsIContent& aContent);

  /**
   * IsNewLinePreformatted() checks whether the linefeed characters are
   * preformatted or collapsible white-spaces.  This does NOT flush layout.
   */
  static bool IsNewLinePreformatted(const nsIContent& aContent);

  /**
   * IsOnlyNewLinePreformatted() checks whether the linefeed characters are
   * preformated but white-spaces are collapsed, or otherwise.  I.e., this
   * returns true only when `white-space:pre-line`.
   */
  static bool IsOnlyNewLinePreformatted(const nsIContent& aContent);

  static nsStaticAtom* GetTagNameAtom(const nsAString& aTagName) {
    if (aTagName.IsEmpty()) {
      return nullptr;
    }
    nsAutoString lowerTagName;
    nsContentUtils::ASCIIToLower(aTagName, lowerTagName);
    return NS_GetStaticAtom(lowerTagName);
  }

  static nsStaticAtom* GetAttributeAtom(const nsAString& aAttribute) {
    if (aAttribute.IsEmpty()) {
      return nullptr;  // Don't use nsGkAtoms::_empty for attribute.
    }
    return NS_GetStaticAtom(aAttribute);
  }

  /**
   * Helper method for deletion.  When this returns true, Selection will be
   * computed with nsFrameSelection that also requires flushed layout
   * information.
   */
  template <typename SelectionOrAutoRangeArray>
  static bool IsFrameSelectionRequiredToExtendSelection(
      nsIEditor::EDirection aDirectionAndAmount,
      SelectionOrAutoRangeArray& aSelectionOrAutoRangeArray) {
    switch (aDirectionAndAmount) {
      case nsIEditor::eNextWord:
      case nsIEditor::ePreviousWord:
      case nsIEditor::eToBeginningOfLine:
      case nsIEditor::eToEndOfLine:
        return true;
      case nsIEditor::ePrevious:
      case nsIEditor::eNext:
        return aSelectionOrAutoRangeArray.IsCollapsed();
      default:
        return false;
    }
  }

  /**
   * Create an nsITransferable instance which has kTextMime and
   * kMozTextInternal flavors.
   */
  static Result<nsCOMPtr<nsITransferable>, nsresult>
  CreateTransferableForPlainText(const dom::Document& aDocument);
};

}  // namespace mozilla

#endif  // #ifndef mozilla_EditorUtils_h
