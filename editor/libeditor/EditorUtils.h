/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorUtils_h
#define mozilla_EditorUtils_h

#include "mozilla/EditAction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/Result.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Text.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsDirection.h"
#include "nsError.h"
#include "nsRange.h"
#include "nsString.h"

class nsITransferable;

namespace mozilla {
class MoveNodeResult;
template <class T>
class OwningNonNull;

/***************************************************************************
 * EditActionResult is useful to return multiple results of an editor
 * action handler without out params.
 * Note that when you return an anonymous instance from a method, you should
 * use EditActionIgnored(), EditActionHandled() or EditActionCanceled() for
 * easier to read.  In other words, EditActionResult should be used when
 * declaring return type of a method, being an argument or defined as a local
 * variable.
 */
class MOZ_STACK_CLASS EditActionResult final {
 public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }
  bool Canceled() const { return mCanceled; }
  bool Handled() const { return mHandled; }
  bool Ignored() const { return !mCanceled && !mHandled; }
  bool EditorDestroyed() const { return mRv == NS_ERROR_EDITOR_DESTROYED; }

  EditActionResult SetResult(nsresult aRv) {
    mRv = aRv;
    return *this;
  }
  EditActionResult MarkAsCanceled() {
    mCanceled = true;
    return *this;
  }
  EditActionResult MarkAsHandled() {
    mHandled = true;
    return *this;
  }

  explicit EditActionResult(nsresult aRv)
      : mRv(aRv), mCanceled(false), mHandled(false) {}

  EditActionResult& operator|=(const EditActionResult& aOther) {
    mCanceled |= aOther.mCanceled;
    mHandled |= aOther.mHandled;
    // When both result are same, keep the result.
    if (mRv == aOther.mRv) {
      return *this;
    }
    // If one of the result is NS_ERROR_EDITOR_DESTROYED, use it since it's
    // the most important error code for editor.
    if (EditorDestroyed() || aOther.EditorDestroyed()) {
      mRv = NS_ERROR_EDITOR_DESTROYED;
    }
    // If one of the results is error, use NS_ERROR_FAILURE.
    else if (Failed() || aOther.Failed()) {
      mRv = NS_ERROR_FAILURE;
    } else {
      // Otherwise, use generic success code, NS_OK.
      mRv = NS_OK;
    }
    return *this;
  }

  EditActionResult& operator|=(const MoveNodeResult& aMoveNodeResult);

 private:
  nsresult mRv;
  bool mCanceled;
  bool mHandled;

  EditActionResult(nsresult aRv, bool aCanceled, bool aHandled)
      : mRv(aRv), mCanceled(aCanceled), mHandled(aHandled) {}

  EditActionResult()
      : mRv(NS_ERROR_NOT_INITIALIZED), mCanceled(false), mHandled(false) {}

  friend EditActionResult EditActionIgnored(nsresult aRv);
  friend EditActionResult EditActionHandled(nsresult aRv);
  friend EditActionResult EditActionCanceled(nsresult aRv);
};

/***************************************************************************
 * When an edit action handler (or its helper) does nothing,
 * EditActionIgnored should be returned.
 */
inline EditActionResult EditActionIgnored(nsresult aRv = NS_OK) {
  return EditActionResult(aRv, false, false);
}

/***************************************************************************
 * When an edit action handler (or its helper) handled and not canceled,
 * EditActionHandled should be returned.
 */
inline EditActionResult EditActionHandled(nsresult aRv = NS_OK) {
  return EditActionResult(aRv, false, true);
}

/***************************************************************************
 * When an edit action handler (or its helper) handled and canceled,
 * EditActionHandled should be returned.
 */
inline EditActionResult EditActionCanceled(nsresult aRv = NS_OK) {
  return EditActionResult(aRv, true, true);
}

/***************************************************************************
 * CreateNodeResultBase is a simple class for CreateSomething() methods
 * which want to return new node.
 */
template <typename NodeType>
class CreateNodeResultBase;

typedef CreateNodeResultBase<dom::Element> CreateElementResult;

template <typename NodeType>
class MOZ_STACK_CLASS CreateNodeResultBase final {
  typedef CreateNodeResultBase<NodeType> SelfType;

 public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }
  bool EditorDestroyed() const { return mRv == NS_ERROR_EDITOR_DESTROYED; }
  NodeType* GetNewNode() const { return mNode; }

  CreateNodeResultBase() = delete;

  explicit CreateNodeResultBase(nsresult aRv) : mRv(aRv) {
    MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(mRv));
  }

  explicit CreateNodeResultBase(NodeType* aNode)
      : mNode(aNode), mRv(aNode ? NS_OK : NS_ERROR_FAILURE) {}

  explicit CreateNodeResultBase(RefPtr<NodeType>&& aNode)
      : mNode(std::move(aNode)), mRv(mNode.get() ? NS_OK : NS_ERROR_FAILURE) {}

  CreateNodeResultBase(const SelfType& aOther) = delete;
  SelfType& operator=(const SelfType& aOther) = delete;
  CreateNodeResultBase(SelfType&& aOther) = default;
  SelfType& operator=(SelfType&& aOther) = default;

  already_AddRefed<NodeType> forget() {
    mRv = NS_ERROR_NOT_INITIALIZED;
    return mNode.forget();
  }

 private:
  RefPtr<NodeType> mNode;
  nsresult mRv;
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
    uint32_t rangeCount = aSelection.RangeCount();
    for (uint32_t i = 0; i < rangeCount; i++) {
      mRanges.AppendElement(*aSelection.GetRangeAt(i));
    }
  }

  AutoTArray<mozilla::OwningNonNull<nsRange>, 8> mRanges;
};

/******************************************************************************
 * AutoRangeArray stores ranges which do no belong any `Selection`.
 * So, different from `AutoSelectionRangeArray`, this can be used for
 * ranges which may need to be modified before touching the DOM tree,
 * but does not want to modify `Selection` for the performance.
 *****************************************************************************/
class MOZ_STACK_CLASS AutoRangeArray final {
 public:
  explicit AutoRangeArray(const dom::Selection& aSelection) {
    Initialize(aSelection);
  }

  void Initialize(const dom::Selection& aSelection) {
    mDirection = aSelection.GetDirection();
    mRanges.Clear();
    for (uint32_t i = 0; i < aSelection.RangeCount(); i++) {
      mRanges.AppendElement(aSelection.GetRangeAt(i)->CloneRange());
      if (aSelection.GetRangeAt(i) == aSelection.GetAnchorFocusRange()) {
        mAnchorFocusRange = mRanges.LastElement();
      }
    }
  }

  /**
   * EnsureOnlyEditableRanges() removes ranges which cannot modify.
   * Note that this is designed only for `HTMLEditor` because this must not
   * be required by `TextEditor`.
   */
  void EnsureOnlyEditableRanges(const dom::Element& aEditingHost);
  static bool IsEditableRange(const dom::AbstractRange& aRange,
                              const dom::Element& aEditingHost);

  /**
   * IsAtLeastOneContainerOfRangeBoundariesInclusiveDescendantOf() returns true
   * if at least one of the containers of the range boundaries is an inclusive
   * descendant of aContent.
   */
  bool IsAtLeastOneContainerOfRangeBoundariesInclusiveDescendantOf(
      const nsIContent& aContent) const {
    for (const OwningNonNull<nsRange>& range : mRanges) {
      nsINode* startContainer = range->GetStartContainer();
      if (startContainer &&
          startContainer->IsInclusiveDescendantOf(&aContent)) {
        return true;
      }
      nsINode* endContainer = range->GetEndContainer();
      if (startContainer == endContainer) {
        continue;
      }
      if (endContainer && endContainer->IsInclusiveDescendantOf(&aContent)) {
        return true;
      }
    }
    return false;
  }

  auto& Ranges() { return mRanges; }
  const auto& Ranges() const { return mRanges; }
  auto& FirstRangeRef() { return mRanges[0]; }
  const auto& FirstRangeRef() const { return mRanges[0]; }

  template <template <typename> typename StrongPtrType>
  AutoTArray<StrongPtrType<nsRange>, 8> CloneRanges() const {
    AutoTArray<StrongPtrType<nsRange>, 8> ranges;
    for (const auto& range : mRanges) {
      ranges.AppendElement(range->CloneRange());
    }
    return ranges;
  }

  EditorDOMPoint GetStartPointOfFirstRange() const {
    if (mRanges.IsEmpty() || !mRanges[0]->IsPositioned()) {
      return EditorDOMPoint();
    }
    return EditorDOMPoint(mRanges[0]->StartRef());
  }
  EditorDOMPoint GetEndPointOfFirstRange() const {
    if (mRanges.IsEmpty() || !mRanges[0]->IsPositioned()) {
      return EditorDOMPoint();
    }
    return EditorDOMPoint(mRanges[0]->EndRef());
  }

  nsresult SelectNode(nsINode& aNode) {
    mRanges.Clear();
    if (!mAnchorFocusRange) {
      mAnchorFocusRange = nsRange::Create(&aNode);
      if (!mAnchorFocusRange) {
        return NS_ERROR_FAILURE;
      }
    }
    ErrorResult error;
    mAnchorFocusRange->SelectNode(aNode, error);
    if (error.Failed()) {
      mAnchorFocusRange = nullptr;
      return error.StealNSResult();
    }
    mRanges.AppendElement(*mAnchorFocusRange);
    return NS_OK;
  }

  /**
   * ExtendAnchorFocusRangeFor() extends the anchor-focus range for deleting
   * content for aDirectionAndAmount.  The range won't be extended to outer of
   * selection limiter.  Note that if a range is extened, the range is
   * recreated.  Therefore, caller cannot cache pointer of any ranges before
   * calling this.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<nsIEditor::EDirection, nsresult>
  ExtendAnchorFocusRangeFor(const EditorBase& aEditorBase,
                            nsIEditor::EDirection aDirectionAndAmount);

  /**
   * For compatiblity with the other browsers, we should shrink ranges to
   * start from an atomic content and/or end after one instead of start
   * from end of a preceding text node and end by start of a follwing text
   * node.  Returns true if this modifies a range.
   */
  enum class IfSelectingOnlyOneAtomicContent {
    Collapse,  // Collapse to the range selecting only one atomic content to
               // start or after of it.  Whether to collapse start or after
               // it depends on aDirectionAndAmount.  This is ignored if
               // there are multiple ranges.
    KeepSelecting,  // Won't collapse the range.
  };
  Result<bool, nsresult> ShrinkRangesIfStartFromOrEndAfterAtomicContent(
      const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      IfSelectingOnlyOneAtomicContent aIfSelectingOnlyOneAtomicContent,
      const dom::Element* aEditingHost);

  /**
   * The following methods are same as `Selection`'s methods.
   */
  bool IsCollapsed() const {
    return mRanges.IsEmpty() ||
           (mRanges.Length() == 1 && mRanges[0]->Collapsed());
  }
  template <typename PT, typename CT>
  nsresult Collapse(const EditorDOMPointBase<PT, CT>& aPoint) {
    mRanges.Clear();
    if (!mAnchorFocusRange) {
      ErrorResult error;
      mAnchorFocusRange = nsRange::Create(aPoint.ToRawRangeBoundary(),
                                          aPoint.ToRawRangeBoundary(), error);
      if (error.Failed()) {
        mAnchorFocusRange = nullptr;
        return error.StealNSResult();
      }
    } else {
      nsresult rv = mAnchorFocusRange->CollapseTo(aPoint.ToRawRangeBoundary());
      if (NS_FAILED(rv)) {
        mAnchorFocusRange = nullptr;
        return rv;
      }
    }
    mRanges.AppendElement(*mAnchorFocusRange);
    return NS_OK;
  }
  template <typename SPT, typename SCT, typename EPT, typename ECT>
  nsresult SetStartAndEnd(const EditorDOMPointBase<SPT, SCT>& aStart,
                          const EditorDOMPointBase<EPT, ECT>& aEnd) {
    mRanges.Clear();
    if (!mAnchorFocusRange) {
      ErrorResult error;
      mAnchorFocusRange = nsRange::Create(aStart.ToRawRangeBoundary(),
                                          aEnd.ToRawRangeBoundary(), error);
      if (error.Failed()) {
        mAnchorFocusRange = nullptr;
        return error.StealNSResult();
      }
    } else {
      nsresult rv = mAnchorFocusRange->SetStartAndEnd(
          aStart.ToRawRangeBoundary(), aEnd.ToRawRangeBoundary());
      if (NS_FAILED(rv)) {
        mAnchorFocusRange = nullptr;
        return rv;
      }
    }
    mRanges.AppendElement(*mAnchorFocusRange);
    return NS_OK;
  }
  const nsRange* GetAnchorFocusRange() const { return mAnchorFocusRange; }
  nsDirection GetDirection() const { return mDirection; }

  const RangeBoundary& AnchorRef() const {
    if (!mAnchorFocusRange) {
      static RangeBoundary sEmptyRangeBoundary;
      return sEmptyRangeBoundary;
    }
    return mDirection == nsDirection::eDirNext ? mAnchorFocusRange->StartRef()
                                               : mAnchorFocusRange->EndRef();
  }
  nsINode* GetAnchorNode() const {
    return AnchorRef().IsSet() ? AnchorRef().Container() : nullptr;
  }
  uint32_t GetAnchorOffset() const {
    return AnchorRef().IsSet()
               ? AnchorRef()
                     .Offset(RangeBoundary::OffsetFilter::kValidOffsets)
                     .valueOr(0)
               : 0;
  }
  nsIContent* GetChildAtAnchorOffset() const {
    return AnchorRef().IsSet() ? AnchorRef().GetChildAtOffset() : nullptr;
  }

  const RangeBoundary& FocusRef() const {
    if (!mAnchorFocusRange) {
      static RangeBoundary sEmptyRangeBoundary;
      return sEmptyRangeBoundary;
    }
    return mDirection == nsDirection::eDirNext ? mAnchorFocusRange->EndRef()
                                               : mAnchorFocusRange->StartRef();
  }
  nsINode* GetFocusNode() const {
    return FocusRef().IsSet() ? FocusRef().Container() : nullptr;
  }
  uint32_t FocusOffset() const {
    return FocusRef().IsSet()
               ? FocusRef()
                     .Offset(RangeBoundary::OffsetFilter::kValidOffsets)
                     .valueOr(0)
               : 0;
  }
  nsIContent* GetChildAtFocusOffset() const {
    return FocusRef().IsSet() ? FocusRef().GetChildAtOffset() : nullptr;
  }

  void RemoveAllRanges() {
    mRanges.Clear();
    mAnchorFocusRange = nullptr;
    mDirection = nsDirection::eDirNext;
  }

 private:
  AutoTArray<mozilla::OwningNonNull<nsRange>, 8> mRanges;
  RefPtr<nsRange> mAnchorFocusRange;
  nsDirection mDirection = nsDirection::eDirNext;
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

  /**
   * Helper method for `AppendString()` and `AppendSubString()`.  This should
   * be called only when `aText` is in a password field.  This method masks
   * A part of or all of `aText` (`aStartOffsetInText` and later) should've
   * been copied (apppended) to `aString`.  `aStartOffsetInString` is where
   * the password was appended into `aString`.
   */
  static void MaskString(nsString& aString, const dom::Text& aTextNode,
                         uint32_t aStartOffsetInString,
                         uint32_t aStartOffsetInText);

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
   * Returns true if aSelection includes the point in aParentContent.
   */
  static bool IsPointInSelection(const Selection& aSelection,
                                 const nsINode& aParentNode, uint32_t aOffset);

  /**
   * Create an nsITransferable instance which has kUnicodeMime and
   * kMozTextInternal flavors.
   */
  static Result<nsCOMPtr<nsITransferable>, nsresult>
  CreateTransferableForPlainText(const dom::Document& aDocument);
};

}  // namespace mozilla

#endif  // #ifndef mozilla_EditorUtils_h
