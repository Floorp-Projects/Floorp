/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AutoRangeArray_h
#define AutoRangeArray_h

#include "EditAction.h"      // for EditSubAction
#include "EditorBase.h"      // for EditorBase
#include "EditorDOMPoint.h"  // for EditorDOMPoint, EditorDOMRange, etc
#include "EditorForwards.h"
#include "HTMLEditHelpers.h"  // for BlockInlineCheck
#include "SelectionState.h"   // for SelectionState

#include "mozilla/ErrorResult.h"        // for ErrorResult
#include "mozilla/IntegerRange.h"       // for IntegerRange
#include "mozilla/Maybe.h"              // for Maybe
#include "mozilla/RangeBoundary.h"      // for RangeBoundary
#include "mozilla/Result.h"             // for Result<>
#include "mozilla/dom/Element.h"        // for dom::Element
#include "mozilla/dom/HTMLBRElement.h"  // for dom::HTMLBRElement
#include "mozilla/dom/Selection.h"      // for dom::Selection
#include "mozilla/dom/Text.h"           // for dom::Text

#include "nsDebug.h"      // for NS_WARNING, etc
#include "nsDirection.h"  // for nsDirection
#include "nsError.h"      // for NS_SUCCESS_* and NS_ERROR_*
#include "nsRange.h"      // for nsRange

namespace mozilla {

/******************************************************************************
 * AutoRangeArray stores ranges which do no belong any `Selection`.
 * So, different from `AutoSelectionRangeArray`, this can be used for
 * ranges which may need to be modified before touching the DOM tree,
 * but does not want to modify `Selection` for the performance.
 *****************************************************************************/
class MOZ_STACK_CLASS AutoRangeArray final {
 public:
  explicit AutoRangeArray(const dom::Selection& aSelection);
  template <typename PointType>
  explicit AutoRangeArray(const EditorDOMRangeBase<PointType>& aRange);
  template <typename PT, typename CT>
  explicit AutoRangeArray(const EditorDOMPointBase<PT, CT>& aPoint);
  // The copy constructor copies everything except saved ranges.
  explicit AutoRangeArray(const AutoRangeArray& aOther);

  ~AutoRangeArray();

  void Initialize(const dom::Selection& aSelection) {
    ClearSavedRanges();
    mDirection = aSelection.GetDirection();
    mRanges.Clear();
    for (const uint32_t i : IntegerRange(aSelection.RangeCount())) {
      MOZ_ASSERT(aSelection.GetRangeAt(i));
      mRanges.AppendElement(aSelection.GetRangeAt(i)->CloneRange());
      if (aSelection.GetRangeAt(i) == aSelection.GetAnchorFocusRange()) {
        mAnchorFocusRange = mRanges.LastElement();
      }
    }
  }

  /**
   * Check whether all ranges in content nodes or not.  If the ranges is empty,
   * this returns false.
   */
  [[nodiscard]] bool IsInContent() const {
    if (mRanges.IsEmpty()) {
      return false;
    }
    for (const OwningNonNull<nsRange>& range : mRanges) {
      if (MOZ_UNLIKELY(!range->IsPositioned() || !range->GetStartContainer() ||
                       !range->GetStartContainer()->IsContent() ||
                       !range->GetEndContainer() ||
                       !range->GetEndContainer()->IsContent())) {
        return false;
      }
    }
    return true;
  }

  /**
   * EnsureOnlyEditableRanges() removes ranges which cannot modify.
   * Note that this is designed only for `HTMLEditor` because this must not
   * be required by `TextEditor`.
   */
  void EnsureOnlyEditableRanges(const dom::Element& aEditingHost);

  /**
   * EnsureRangesInTextNode() is designed for TextEditor to guarantee that
   * all ranges are in its text node which is first child of the anonymous <div>
   * element and is first child.
   */
  void EnsureRangesInTextNode(const dom::Text& aTextNode);

  /**
   * Extend ranges to wrap lines to handle block level edit actions such as
   * updating the block parent or indent/outdent around the selection.
   */
  void ExtendRangesToWrapLinesToHandleBlockLevelEditAction(
      EditSubAction aEditSubAction, const dom::Element& aEditingHost);

  /**
   * Check whether the range is in aEditingHost and both containers of start and
   * end boundaries of the range are editable.
   */
  [[nodiscard]] static bool IsEditableRange(const dom::AbstractRange& aRange,
                                            const dom::Element& aEditingHost);

  /**
   * Check whether the first range is in aEditingHost and both containers of
   * start and end boundaries of the first range are editable.
   */
  [[nodiscard]] bool IsFirstRangeEditable(
      const dom::Element& aEditingHost) const {
    return IsEditableRange(FirstRangeRef(), aEditingHost);
  }

  /**
   * IsAtLeastOneContainerOfRangeBoundariesInclusiveDescendantOf() returns true
   * if at least one of the containers of the range boundaries is an inclusive
   * descendant of aContent.
   */
  [[nodiscard]] bool
  IsAtLeastOneContainerOfRangeBoundariesInclusiveDescendantOf(
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

  [[nodiscard]] auto& Ranges() { return mRanges; }
  [[nodiscard]] const auto& Ranges() const { return mRanges; }
  [[nodiscard]] OwningNonNull<nsRange>& FirstRangeRef() { return mRanges[0]; }
  [[nodiscard]] const OwningNonNull<nsRange>& FirstRangeRef() const {
    return mRanges[0];
  }

  template <template <typename> typename StrongPtrType>
  [[nodiscard]] AutoTArray<StrongPtrType<nsRange>, 8> CloneRanges() const {
    AutoTArray<StrongPtrType<nsRange>, 8> ranges;
    for (const auto& range : mRanges) {
      ranges.AppendElement(range->CloneRange());
    }
    return ranges;
  }

  template <typename EditorDOMPointType>
  [[nodiscard]] EditorDOMPointType GetFirstRangeStartPoint() const {
    if (mRanges.IsEmpty() || !mRanges[0]->IsPositioned()) {
      return EditorDOMPointType();
    }
    return EditorDOMPointType(mRanges[0]->StartRef());
  }
  template <typename EditorDOMPointType>
  [[nodiscard]] EditorDOMPointType GetFirstRangeEndPoint() const {
    if (mRanges.IsEmpty() || !mRanges[0]->IsPositioned()) {
      return EditorDOMPointType();
    }
    return EditorDOMPointType(mRanges[0]->EndRef());
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
  [[nodiscard]] bool IsCollapsed() const {
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
  template <typename SPT, typename SCT, typename EPT, typename ECT>
  nsresult SetBaseAndExtent(const EditorDOMPointBase<SPT, SCT>& aAnchor,
                            const EditorDOMPointBase<EPT, ECT>& aFocus) {
    if (MOZ_UNLIKELY(!aAnchor.IsSet()) || MOZ_UNLIKELY(!aFocus.IsSet())) {
      mRanges.Clear();
      mAnchorFocusRange = nullptr;
      return NS_ERROR_INVALID_ARG;
    }
    return aAnchor.EqualsOrIsBefore(aFocus) ? SetStartAndEnd(aAnchor, aFocus)
                                            : SetStartAndEnd(aFocus, aAnchor);
  }
  [[nodiscard]] const nsRange* GetAnchorFocusRange() const {
    return mAnchorFocusRange;
  }
  [[nodiscard]] nsDirection GetDirection() const { return mDirection; }

  void SetDirection(nsDirection aDirection) { mDirection = aDirection; }

  [[nodiscard]] const RangeBoundary& AnchorRef() const {
    if (!mAnchorFocusRange) {
      static RangeBoundary sEmptyRangeBoundary;
      return sEmptyRangeBoundary;
    }
    return mDirection == nsDirection::eDirNext ? mAnchorFocusRange->StartRef()
                                               : mAnchorFocusRange->EndRef();
  }
  [[nodiscard]] nsINode* GetAnchorNode() const {
    return AnchorRef().IsSet() ? AnchorRef().Container() : nullptr;
  }
  [[nodiscard]] uint32_t GetAnchorOffset() const {
    return AnchorRef().IsSet()
               ? AnchorRef()
                     .Offset(RangeBoundary::OffsetFilter::kValidOffsets)
                     .valueOr(0)
               : 0;
  }
  [[nodiscard]] nsIContent* GetChildAtAnchorOffset() const {
    return AnchorRef().IsSet() ? AnchorRef().GetChildAtOffset() : nullptr;
  }

  [[nodiscard]] const RangeBoundary& FocusRef() const {
    if (!mAnchorFocusRange) {
      static RangeBoundary sEmptyRangeBoundary;
      return sEmptyRangeBoundary;
    }
    return mDirection == nsDirection::eDirNext ? mAnchorFocusRange->EndRef()
                                               : mAnchorFocusRange->StartRef();
  }
  [[nodiscard]] nsINode* GetFocusNode() const {
    return FocusRef().IsSet() ? FocusRef().Container() : nullptr;
  }
  [[nodiscard]] uint32_t FocusOffset() const {
    return FocusRef().IsSet()
               ? FocusRef()
                     .Offset(RangeBoundary::OffsetFilter::kValidOffsets)
                     .valueOr(0)
               : 0;
  }
  [[nodiscard]] nsIContent* GetChildAtFocusOffset() const {
    return FocusRef().IsSet() ? FocusRef().GetChildAtOffset() : nullptr;
  }

  void RemoveAllRanges() {
    mRanges.Clear();
    mAnchorFocusRange = nullptr;
    mDirection = nsDirection::eDirNext;
  }

  /**
   * APIs to store ranges with only container node and offset in it, and track
   * them with RangeUpdater.
   */
  [[nodiscard]] bool SaveAndTrackRanges(HTMLEditor& aHTMLEditor);
  [[nodiscard]] bool HasSavedRanges() const { return mSavedRanges.isSome(); }
  void ClearSavedRanges();
  void RestoreFromSavedRanges() {
    MOZ_DIAGNOSTIC_ASSERT(mSavedRanges.isSome());
    if (mSavedRanges.isNothing()) {
      return;
    }
    mSavedRanges->ApplyTo(*this);
    ClearSavedRanges();
  }

  /**
   * Apply mRanges and mDirection to aSelection.
   */
  MOZ_CAN_RUN_SCRIPT nsresult ApplyTo(dom::Selection& aSelection) {
    dom::SelectionBatcher selectionBatcher(aSelection, __FUNCTION__);
    aSelection.RemoveAllRanges(IgnoreErrors());
    MOZ_ASSERT(!aSelection.RangeCount());
    aSelection.SetDirection(mDirection);
    IgnoredErrorResult error;
    for (const OwningNonNull<nsRange>& range : mRanges) {
      // MOZ_KnownLive(range) due to bug 1622253
      aSelection.AddRangeAndSelectFramesAndNotifyListeners(MOZ_KnownLive(range),
                                                           error);
      if (error.Failed()) {
        return error.StealNSResult();
      }
    }
    return NS_OK;
  }

  /**
   * If the points are same (i.e., mean a collapsed range) and in an empty block
   * element except the padding <br> element, this makes aStartPoint and
   * aEndPoint contain the padding <br> element.
   */
  static void UpdatePointsToSelectAllChildrenIfCollapsedInEmptyBlockElement(
      EditorDOMPoint& aStartPoint, EditorDOMPoint& aEndPoint,
      const dom::Element& aEditingHost);

  /**
   * CreateRangeExtendedToHardLineStartAndEnd() creates an nsRange instance
   * which may be expanded to start/end of hard line at both edges of the given
   * range.  If this fails handling something, returns nullptr.
   */
  static already_AddRefed<nsRange>
  CreateRangeWrappingStartAndEndLinesContainingBoundaries(
      const EditorDOMRange& aRange, EditSubAction aEditSubAction,
      const dom::Element& aEditingHost) {
    if (!aRange.IsPositioned()) {
      return nullptr;
    }
    return CreateRangeWrappingStartAndEndLinesContainingBoundaries(
        aRange.StartRef(), aRange.EndRef(), aEditSubAction, aEditingHost);
  }
  static already_AddRefed<nsRange>
  CreateRangeWrappingStartAndEndLinesContainingBoundaries(
      const EditorDOMPoint& aStartPoint, const EditorDOMPoint& aEndPoint,
      EditSubAction aEditSubAction, const dom::Element& aEditingHost) {
    RefPtr<nsRange> range =
        nsRange::Create(aStartPoint.ToRawRangeBoundary(),
                        aEndPoint.ToRawRangeBoundary(), IgnoreErrors());
    if (MOZ_UNLIKELY(!range)) {
      return nullptr;
    }
    if (NS_FAILED(ExtendRangeToWrapStartAndEndLinesContainingBoundaries(
            *range, aEditSubAction, aEditingHost)) ||
        MOZ_UNLIKELY(!range->IsPositioned())) {
      return nullptr;
    }
    return range.forget();
  }

  /**
   * Splits text nodes if each range end is in middle of a text node, then,
   * calls HTMLEditor::SplitParentInlineElementsAtRangeBoundaries() for each
   * range.  Finally, updates ranges to keep edit target ranges as expected.
   *
   * @param aHTMLEditor         The HTMLEditor which will handle the splittings.
   * @param aBlockInlineCheck   Considering block vs inline with whether the
   *                            computed style or the HTML default style.
   * @param aElement            The editing host.
   * @param aAncestorLimiter    A content node which you don't want this to
   *                            split it.
   * @return                    A suggest point to put caret if succeeded, but
   *                            it may be unset.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<EditorDOMPoint, nsresult>
  SplitTextAtEndBoundariesAndInlineAncestorsAtBothBoundaries(
      HTMLEditor& aHTMLEditor, BlockInlineCheck aBlockInlineCheck,
      const dom::Element& aEditingHost,
      const nsIContent* aAncestorLimiter = nullptr);

  /**
   * CollectEditTargetNodes() collects edit target nodes the ranges.
   * First, this collects all nodes in given ranges, then, modifies the
   * result for specific edit sub-actions.
   */
  enum class CollectNonEditableNodes { No, Yes };
  nsresult CollectEditTargetNodes(
      const HTMLEditor& aHTMLEditor,
      nsTArray<OwningNonNull<nsIContent>>& aOutArrayOfContents,
      EditSubAction aEditSubAction,
      CollectNonEditableNodes aCollectNonEditableNodes) const;

  /**
   * Retrieve a closest ancestor list element of a common ancestor of _A_ range
   * of the ranges.  This tries to retrieve it from the first range to the last
   * range.
   */
  dom::Element* GetClosestAncestorAnyListElementOfRange() const;

 private:
  static nsresult ExtendRangeToWrapStartAndEndLinesContainingBoundaries(
      nsRange& aRange, EditSubAction aEditSubAction,
      const dom::Element& aEditingHost);

  AutoTArray<mozilla::OwningNonNull<nsRange>, 8> mRanges;
  RefPtr<nsRange> mAnchorFocusRange;
  nsDirection mDirection = nsDirection::eDirNext;
  Maybe<SelectionState> mSavedRanges;
  RefPtr<HTMLEditor> mTrackingHTMLEditor;
};

}  // namespace mozilla

#endif  // #ifndef AutoRangeArray_h
