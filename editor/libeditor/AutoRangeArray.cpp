/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoRangeArray.h"

#include "EditAction.h"
#include "EditorDOMPoint.h"   // for EditorDOMPoint, EditorDOMRange, etc
#include "EditorForwards.h"   // for CollectChildrenOptions
#include "HTMLEditUtils.h"    // for HTMLEditUtils
#include "HTMLEditHelpers.h"  // for SplitNodeResult
#include "TextEditor.h"       // for TextEditor
#include "WSRunObject.h"      // for WSRunScanner

#include "mozilla/IntegerRange.h"       // for IntegerRange
#include "mozilla/OwningNonNull.h"      // for OwningNonNull
#include "mozilla/dom/Document.h"       // for dom::Document
#include "mozilla/dom/HTMLBRElement.h"  // for dom HTMLBRElement
#include "mozilla/dom/Selection.h"      // for dom::Selection
#include "mozilla/dom/Text.h"           // for dom::Text

#include "gfxFontUtils.h"      // for gfxFontUtils
#include "nsError.h"           // for NS_SUCCESS_* and NS_ERROR_*
#include "nsFrameSelection.h"  // for nsFrameSelection
#include "nsIContent.h"        // for nsIContent
#include "nsINode.h"           // for nsINode
#include "nsRange.h"           // for nsRange
#include "nsTextFragment.h"    // for nsTextFragment

namespace mozilla {

using namespace dom;

/******************************************************************************
 * mozilla::AutoRangeArray
 *****************************************************************************/

template AutoRangeArray::AutoRangeArray(const EditorDOMRange& aRange);
template AutoRangeArray::AutoRangeArray(const EditorRawDOMRange& aRange);
template AutoRangeArray::AutoRangeArray(const EditorDOMPoint& aRange);
template AutoRangeArray::AutoRangeArray(const EditorRawDOMPoint& aRange);

AutoRangeArray::AutoRangeArray(const dom::Selection& aSelection) {
  Initialize(aSelection);
}

AutoRangeArray::AutoRangeArray(const AutoRangeArray& aOther)
    : mAnchorFocusRange(aOther.mAnchorFocusRange),
      mDirection(aOther.mDirection) {
  mRanges.SetCapacity(aOther.mRanges.Length());
  for (const OwningNonNull<nsRange>& range : aOther.mRanges) {
    RefPtr<nsRange> clonedRange = range->CloneRange();
    mRanges.AppendElement(std::move(clonedRange));
  }
  mAnchorFocusRange = aOther.mAnchorFocusRange;
}

template <typename PointType>
AutoRangeArray::AutoRangeArray(const EditorDOMRangeBase<PointType>& aRange) {
  MOZ_ASSERT(aRange.IsPositionedAndValid());
  RefPtr<nsRange> range = aRange.CreateRange(IgnoreErrors());
  if (NS_WARN_IF(!range) || NS_WARN_IF(!range->IsPositioned())) {
    return;
  }
  mRanges.AppendElement(*range);
  mAnchorFocusRange = std::move(range);
}

template <typename PT, typename CT>
AutoRangeArray::AutoRangeArray(const EditorDOMPointBase<PT, CT>& aPoint) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  RefPtr<nsRange> range = aPoint.CreateCollapsedRange(IgnoreErrors());
  if (NS_WARN_IF(!range) || NS_WARN_IF(!range->IsPositioned())) {
    return;
  }
  mRanges.AppendElement(*range);
  mAnchorFocusRange = std::move(range);
}

AutoRangeArray::~AutoRangeArray() {
  if (mSavedRanges.isSome()) {
    ClearSavedRanges();
  }
}

// static
bool AutoRangeArray::IsEditableRange(const dom::AbstractRange& aRange,
                                     const Element& aEditingHost) {
  // TODO: Perhaps, we should check whether the start/end boundaries are
  //       first/last point of non-editable element.
  //       See https://github.com/w3c/editing/issues/283#issuecomment-788654850
  EditorRawDOMPoint atStart(aRange.StartRef());
  const bool isStartEditable =
      atStart.IsInContentNode() &&
      EditorUtils::IsEditableContent(*atStart.ContainerAs<nsIContent>(),
                                     EditorUtils::EditorType::HTML) &&
      !HTMLEditUtils::IsNonEditableReplacedContent(
          *atStart.ContainerAs<nsIContent>());
  if (!isStartEditable) {
    return false;
  }

  if (aRange.GetStartContainer() != aRange.GetEndContainer()) {
    EditorRawDOMPoint atEnd(aRange.EndRef());
    const bool isEndEditable =
        atEnd.IsInContentNode() &&
        EditorUtils::IsEditableContent(*atEnd.ContainerAs<nsIContent>(),
                                       EditorUtils::EditorType::HTML) &&
        !HTMLEditUtils::IsNonEditableReplacedContent(
            *atEnd.ContainerAs<nsIContent>());
    if (!isEndEditable) {
      return false;
    }

    // Now, both start and end points are editable, but if they are in
    // different editing host, we cannot edit the range.
    if (atStart.ContainerAs<nsIContent>() != atEnd.ContainerAs<nsIContent>() &&
        atStart.ContainerAs<nsIContent>()->GetEditingHost() !=
            atEnd.ContainerAs<nsIContent>()->GetEditingHost()) {
      return false;
    }
  }

  // HTMLEditor does not support modifying outside `<body>` element for now.
  nsINode* commonAncestor = aRange.GetClosestCommonInclusiveAncestor();
  return commonAncestor && commonAncestor->IsContent() &&
         commonAncestor->IsInclusiveDescendantOf(&aEditingHost);
}

void AutoRangeArray::EnsureOnlyEditableRanges(const Element& aEditingHost) {
  for (const size_t index : Reversed(IntegerRange(mRanges.Length()))) {
    const OwningNonNull<nsRange>& range = mRanges[index];
    if (!AutoRangeArray::IsEditableRange(range, aEditingHost)) {
      mRanges.RemoveElementAt(index);
      continue;
    }
    // Special handling for `inert` attribute. If anchor node is inert, the
    // range should be treated as not editable.
    nsIContent* anchorContent =
        mDirection == eDirNext
            ? nsIContent::FromNode(range->GetStartContainer())
            : nsIContent::FromNode(range->GetEndContainer());
    if (anchorContent && HTMLEditUtils::ContentIsInert(*anchorContent)) {
      mRanges.RemoveElementAt(index);
      continue;
    }
    // Additionally, if focus node is inert, the range should be collapsed to
    // anchor node.
    nsIContent* focusContent =
        mDirection == eDirNext
            ? nsIContent::FromNode(range->GetEndContainer())
            : nsIContent::FromNode(range->GetStartContainer());
    if (focusContent && focusContent != anchorContent &&
        HTMLEditUtils::ContentIsInert(*focusContent)) {
      range->Collapse(mDirection == eDirNext);
    }
  }
  mAnchorFocusRange = mRanges.IsEmpty() ? nullptr : mRanges.LastElement().get();
}

void AutoRangeArray::EnsureRangesInTextNode(const Text& aTextNode) {
  auto GetOffsetInTextNode = [&aTextNode](const nsINode* aNode,
                                          uint32_t aOffset) -> uint32_t {
    MOZ_DIAGNOSTIC_ASSERT(aNode);
    if (aNode == &aTextNode) {
      return aOffset;
    }
    const nsIContent* anonymousDivElement = aTextNode.GetParent();
    MOZ_DIAGNOSTIC_ASSERT(anonymousDivElement);
    MOZ_DIAGNOSTIC_ASSERT(anonymousDivElement->IsHTMLElement(nsGkAtoms::div));
    MOZ_DIAGNOSTIC_ASSERT(anonymousDivElement->GetFirstChild() == &aTextNode);
    if (aNode == anonymousDivElement && aOffset == 0u) {
      return 0u;  // Point before the text node so that use start of the text.
    }
    MOZ_DIAGNOSTIC_ASSERT(aNode->IsInclusiveDescendantOf(anonymousDivElement));
    // Point after the text node so that use end of the text.
    return aTextNode.TextDataLength();
  };
  for (const OwningNonNull<nsRange>& range : mRanges) {
    if (MOZ_LIKELY(range->GetStartContainer() == &aTextNode &&
                   range->GetEndContainer() == &aTextNode)) {
      continue;
    }
    range->SetStartAndEnd(
        const_cast<Text*>(&aTextNode),
        GetOffsetInTextNode(range->GetStartContainer(), range->StartOffset()),
        const_cast<Text*>(&aTextNode),
        GetOffsetInTextNode(range->GetEndContainer(), range->EndOffset()));
  }

  if (MOZ_UNLIKELY(mRanges.Length() >= 2)) {
    // For avoiding to handle same things in same range, we should drop and
    // merge unnecessary ranges.  Note that the ranges never overlap
    // because selection ranges are not allowed it so that we need to check only
    // end offset vs start offset of next one.
    for (const size_t i : Reversed(IntegerRange(mRanges.Length() - 1u))) {
      MOZ_ASSERT(mRanges[i]->EndOffset() < mRanges[i + 1]->StartOffset());
      // XXX Should we delete collapsed range unless the index is 0?  Without
      //     Selection API, such situation cannot happen so that `TextEditor`
      //     may behave unexpectedly.
      if (MOZ_UNLIKELY(mRanges[i]->EndOffset() >=
                       mRanges[i + 1]->StartOffset())) {
        const uint32_t newEndOffset = mRanges[i + 1]->EndOffset();
        mRanges.RemoveElementAt(i + 1);
        if (MOZ_UNLIKELY(NS_WARN_IF(newEndOffset > mRanges[i]->EndOffset()))) {
          // So, this case shouldn't happen.
          mRanges[i]->SetStartAndEnd(
              const_cast<Text*>(&aTextNode), mRanges[i]->StartOffset(),
              const_cast<Text*>(&aTextNode), newEndOffset);
        }
      }
    }
  }
}

Result<nsIEditor::EDirection, nsresult>
AutoRangeArray::ExtendAnchorFocusRangeFor(
    const EditorBase& aEditorBase, nsIEditor::EDirection aDirectionAndAmount) {
  MOZ_ASSERT(aEditorBase.IsEditActionDataAvailable());
  MOZ_ASSERT(mAnchorFocusRange);
  MOZ_ASSERT(mAnchorFocusRange->IsPositioned());
  MOZ_ASSERT(mAnchorFocusRange->StartRef().IsSet());
  MOZ_ASSERT(mAnchorFocusRange->EndRef().IsSet());

  if (!EditorUtils::IsFrameSelectionRequiredToExtendSelection(
          aDirectionAndAmount, *this)) {
    return aDirectionAndAmount;
  }

  if (NS_WARN_IF(!aEditorBase.SelectionRef().RangeCount())) {
    return Err(NS_ERROR_FAILURE);
  }

  // By a preceding call of EnsureOnlyEditableRanges(), anchor/focus range may
  // have been changed.  In that case, we cannot use nsFrameSelection anymore.
  // FIXME: We should make `nsFrameSelection::CreateRangeExtendedToSomewhere`
  //        work without `Selection` instance.
  if (MOZ_UNLIKELY(
          aEditorBase.SelectionRef().GetAnchorFocusRange()->StartRef() !=
              mAnchorFocusRange->StartRef() ||
          aEditorBase.SelectionRef().GetAnchorFocusRange()->EndRef() !=
              mAnchorFocusRange->EndRef())) {
    return aDirectionAndAmount;
  }

  RefPtr<nsFrameSelection> frameSelection =
      aEditorBase.SelectionRef().GetFrameSelection();
  if (NS_WARN_IF(!frameSelection)) {
    return Err(NS_ERROR_NOT_INITIALIZED);
  }

  RefPtr<Element> editingHost;
  if (aEditorBase.IsHTMLEditor()) {
    editingHost = aEditorBase.AsHTMLEditor()->ComputeEditingHost();
    if (!editingHost) {
      return Err(NS_ERROR_FAILURE);
    }
  }

  Result<RefPtr<nsRange>, nsresult> result(NS_ERROR_UNEXPECTED);
  nsIEditor::EDirection directionAndAmountResult = aDirectionAndAmount;
  switch (aDirectionAndAmount) {
    case nsIEditor::eNextWord:
      result = frameSelection->CreateRangeExtendedToNextWordBoundary<nsRange>();
      if (NS_WARN_IF(aEditorBase.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          result.isOk(),
          "nsFrameSelection::CreateRangeExtendedToNextWordBoundary() failed");
      // DeleteSelectionWithTransaction() doesn't handle these actions
      // because it's inside batching, so don't confuse it:
      directionAndAmountResult = nsIEditor::eNone;
      break;
    case nsIEditor::ePreviousWord:
      result =
          frameSelection->CreateRangeExtendedToPreviousWordBoundary<nsRange>();
      if (NS_WARN_IF(aEditorBase.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          result.isOk(),
          "nsFrameSelection::CreateRangeExtendedToPreviousWordBoundary() "
          "failed");
      // DeleteSelectionWithTransaction() doesn't handle these actions
      // because it's inside batching, so don't confuse it:
      directionAndAmountResult = nsIEditor::eNone;
      break;
    case nsIEditor::eNext:
      result =
          frameSelection
              ->CreateRangeExtendedToNextGraphemeClusterBoundary<nsRange>();
      if (NS_WARN_IF(aEditorBase.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(result.isOk(),
                           "nsFrameSelection::"
                           "CreateRangeExtendedToNextGraphemeClusterBoundary() "
                           "failed");
      // Don't set directionAndAmount to eNone (see Bug 502259)
      break;
    case nsIEditor::ePrevious: {
      // Only extend the selection where the selection is after a UTF-16
      // surrogate pair or a variation selector.
      // For other cases we don't want to do that, in order
      // to make sure that pressing backspace will only delete the last
      // typed character.
      // XXX This is odd if the previous one is a sequence for a grapheme
      //     cluster.
      const auto atStartOfSelection = GetFirstRangeStartPoint<EditorDOMPoint>();
      if (MOZ_UNLIKELY(NS_WARN_IF(!atStartOfSelection.IsSet()))) {
        return Err(NS_ERROR_FAILURE);
      }

      // node might be anonymous DIV, so we find better text node
      const EditorDOMPoint insertionPoint =
          aEditorBase.IsTextEditor()
              ? aEditorBase.AsTextEditor()->FindBetterInsertionPoint(
                    atStartOfSelection)
              : atStartOfSelection.GetPointInTextNodeIfPointingAroundTextNode<
                    EditorDOMPoint>();
      if (MOZ_UNLIKELY(!insertionPoint.IsSet())) {
        NS_WARNING(
            "EditorBase::FindBetterInsertionPoint() failed, but ignored");
        return aDirectionAndAmount;
      }

      if (!insertionPoint.IsInTextNode()) {
        return aDirectionAndAmount;
      }

      const nsTextFragment* data =
          &insertionPoint.ContainerAs<Text>()->TextFragment();
      uint32_t offset = insertionPoint.Offset();
      if (!(offset > 1 &&
            data->IsLowSurrogateFollowingHighSurrogateAt(offset - 1)) &&
          !(offset > 0 &&
            gfxFontUtils::IsVarSelector(data->CharAt(offset - 1)))) {
        return aDirectionAndAmount;
      }
      // Different from the `eNext` case, we look for character boundary.
      // I'm not sure whether this inconsistency between "Delete" and
      // "Backspace" is intentional or not.
      result = frameSelection
                   ->CreateRangeExtendedToPreviousCharacterBoundary<nsRange>();
      if (NS_WARN_IF(aEditorBase.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          result.isOk(),
          "nsFrameSelection::"
          "CreateRangeExtendedToPreviousGraphemeClusterBoundary() failed");
      break;
    }
    case nsIEditor::eToBeginningOfLine:
      result =
          frameSelection->CreateRangeExtendedToPreviousHardLineBreak<nsRange>();
      if (NS_WARN_IF(aEditorBase.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          result.isOk(),
          "nsFrameSelection::CreateRangeExtendedToPreviousHardLineBreak() "
          "failed");
      directionAndAmountResult = nsIEditor::eNone;
      break;
    case nsIEditor::eToEndOfLine:
      result =
          frameSelection->CreateRangeExtendedToNextHardLineBreak<nsRange>();
      if (NS_WARN_IF(aEditorBase.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          result.isOk(),
          "nsFrameSelection::CreateRangeExtendedToNextHardLineBreak() failed");
      directionAndAmountResult = nsIEditor::eNext;
      break;
    default:
      return aDirectionAndAmount;
  }

  if (result.isErr()) {
    return Err(result.inspectErr());
  }
  RefPtr<nsRange> extendedRange(result.unwrap().forget());
  if (!extendedRange || NS_WARN_IF(!extendedRange->IsPositioned())) {
    NS_WARNING("Failed to extend the range, but ignored");
    return directionAndAmountResult;
  }

  // If the new range isn't editable, keep using the original range.
  if (aEditorBase.IsHTMLEditor() &&
      !AutoRangeArray::IsEditableRange(*extendedRange, *editingHost)) {
    return aDirectionAndAmount;
  }

  if (NS_WARN_IF(!frameSelection->IsValidSelectionPoint(
          extendedRange->GetStartContainer())) ||
      NS_WARN_IF(!frameSelection->IsValidSelectionPoint(
          extendedRange->GetEndContainer()))) {
    NS_WARNING("A range was extended to outer of selection limiter");
    return Err(NS_ERROR_FAILURE);
  }

  // Swap focus/anchor range with the extended range.
  DebugOnly<bool> found = false;
  for (OwningNonNull<nsRange>& range : mRanges) {
    if (range == mAnchorFocusRange) {
      range = *extendedRange;
      found = true;
      break;
    }
  }
  MOZ_ASSERT(found);
  mAnchorFocusRange.swap(extendedRange);
  return directionAndAmountResult;
}

Result<bool, nsresult>
AutoRangeArray::ShrinkRangesIfStartFromOrEndAfterAtomicContent(
    const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    IfSelectingOnlyOneAtomicContent aIfSelectingOnlyOneAtomicContent,
    const Element* aEditingHost) {
  if (IsCollapsed()) {
    return false;
  }

  switch (aDirectionAndAmount) {
    case nsIEditor::eNext:
    case nsIEditor::eNextWord:
    case nsIEditor::ePrevious:
    case nsIEditor::ePreviousWord:
      break;
    default:
      return false;
  }

  bool changed = false;
  for (const OwningNonNull<nsRange>& range : mRanges) {
    MOZ_ASSERT(!range->IsInAnySelection(),
               "Changing range in selection may cause running script");
    Result<bool, nsresult> result =
        WSRunScanner::ShrinkRangeIfStartsFromOrEndsAfterAtomicContent(
            aHTMLEditor, range, aEditingHost);
    if (result.isErr()) {
      NS_WARNING(
          "WSRunScanner::ShrinkRangeIfStartsFromOrEndsAfterAtomicContent() "
          "failed");
      return Err(result.inspectErr());
    }
    changed |= result.inspect();
  }

  if (mRanges.Length() == 1 && aIfSelectingOnlyOneAtomicContent ==
                                   IfSelectingOnlyOneAtomicContent::Collapse) {
    MOZ_ASSERT(mRanges[0].get() == mAnchorFocusRange.get());
    if (mAnchorFocusRange->GetStartContainer() ==
            mAnchorFocusRange->GetEndContainer() &&
        mAnchorFocusRange->GetChildAtStartOffset() &&
        mAnchorFocusRange->StartRef().GetNextSiblingOfChildAtOffset() ==
            mAnchorFocusRange->GetChildAtEndOffset()) {
      mAnchorFocusRange->Collapse(aDirectionAndAmount == nsIEditor::eNext ||
                                  aDirectionAndAmount == nsIEditor::eNextWord);
      changed = true;
    }
  }

  return changed;
}

bool AutoRangeArray::SaveAndTrackRanges(HTMLEditor& aHTMLEditor) {
  if (mSavedRanges.isSome()) {
    return false;
  }
  mSavedRanges.emplace(*this);
  aHTMLEditor.RangeUpdaterRef().RegisterSelectionState(mSavedRanges.ref());
  mTrackingHTMLEditor = &aHTMLEditor;
  return true;
}

void AutoRangeArray::ClearSavedRanges() {
  if (mSavedRanges.isNothing()) {
    return;
  }
  OwningNonNull<HTMLEditor> htmlEditor(std::move(mTrackingHTMLEditor));
  MOZ_ASSERT(!mTrackingHTMLEditor);
  htmlEditor->RangeUpdaterRef().DropSelectionState(mSavedRanges.ref());
  mSavedRanges.reset();
}

// static
void AutoRangeArray::
    UpdatePointsToSelectAllChildrenIfCollapsedInEmptyBlockElement(
        EditorDOMPoint& aStartPoint, EditorDOMPoint& aEndPoint,
        const Element& aEditingHost) {
  // FYI: This was moved from
  // https://searchfox.org/mozilla-central/rev/3419858c997f422e3e70020a46baae7f0ec6dacc/editor/libeditor/HTMLEditSubActionHandler.cpp#6743

  // MOOSE major hack:
  // The GetPointAtFirstContentOfLineOrParentBlockIfFirstContentOfBlock() and
  // GetPointAfterFollowingLineBreakOrAtFollowingBlock() don't really do the
  // right thing for collapsed ranges inside block elements that contain nothing
  // but a solo <br>.  It's easier/ to put a workaround here than to revamp
  // them.  :-(
  if (aStartPoint != aEndPoint) {
    return;
  }

  if (!aStartPoint.IsInContentNode()) {
    return;
  }

  // XXX Perhaps, this should be more careful.  This may not select only one
  //     node because this just check whether the block is empty or not,
  //     and may not select in non-editable block.  However, for inline
  //     editing host case, it's right to look for block element without
  //     editable state check.  Now, this method is used for preparation for
  //     other things.  So, cannot write test for this method behavior.
  //     So, perhaps, we should get rid of this method and each caller should
  //     handle its job better.
  Element* const maybeNonEditableBlockElement =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *aStartPoint.ContainerAs<nsIContent>(),
          HTMLEditUtils::ClosestBlockElement,
          BlockInlineCheck::UseComputedDisplayStyle);
  if (!maybeNonEditableBlockElement) {
    return;
  }

  // Make sure we don't go higher than our root element in the content tree
  if (aEditingHost.IsInclusiveDescendantOf(maybeNonEditableBlockElement)) {
    return;
  }

  if (HTMLEditUtils::IsEmptyNode(*maybeNonEditableBlockElement)) {
    aStartPoint.Set(maybeNonEditableBlockElement, 0u);
    aEndPoint.SetToEndOf(maybeNonEditableBlockElement);
  }
}

/**
 * Get the point before the line containing aPointInLine.
 *
 * @return              If the line starts after a `<br>` element, returns next
 *                      sibling of the `<br>` element.
 *                      If the line is first line of a block, returns point of
 *                      the block.
 * NOTE: The result may be point of editing host.  I.e., the container may be
 *       outside of editing host.
 */
static EditorDOMPoint
GetPointAtFirstContentOfLineOrParentHTMLBlockIfFirstContentOfBlock(
    const EditorDOMPoint& aPointInLine, EditSubAction aEditSubAction,
    const Element& aEditingHost) {
  // FYI: This was moved from
  // https://searchfox.org/mozilla-central/rev/3419858c997f422e3e70020a46baae7f0ec6dacc/editor/libeditor/HTMLEditSubActionHandler.cpp#6447

  if (NS_WARN_IF(!aPointInLine.IsSet())) {
    return EditorDOMPoint();
  }

  EditorDOMPoint point(aPointInLine);
  // Start scanning from the container node if aPoint is in a text node.
  // XXX Perhaps, IsInDataNode() must be expected.
  if (point.IsInTextNode()) {
    if (!point.GetContainer()->GetParentNode()) {
      // Okay, can't promote any further
      // XXX Why don't we return start of the text node?
      return point;
    }
    // If there is a preformatted linefeed in the text node, let's return
    // the point after it.
    EditorDOMPoint atLastPreformattedNewLine =
        HTMLEditUtils::GetPreviousPreformattedNewLineInTextNode<EditorDOMPoint>(
            point);
    if (atLastPreformattedNewLine.IsSet()) {
      return atLastPreformattedNewLine.NextPoint();
    }
    point.Set(point.GetContainer());
  }

  // Look back through any further inline nodes that aren't across a <br>
  // from us, and that are enclosed in the same block.
  // I.e., looking for start of current hard line.
  constexpr HTMLEditUtils::WalkTreeOptions
      ignoreNonEditableNodeAndStopAtBlockBoundary{
          HTMLEditUtils::WalkTreeOption::IgnoreNonEditableNode,
          HTMLEditUtils::WalkTreeOption::StopAtBlockBoundary};
  for (nsIContent* previousEditableContent = HTMLEditUtils::GetPreviousContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary,
           BlockInlineCheck::UseHTMLDefaultStyle, &aEditingHost);
       previousEditableContent && previousEditableContent->GetParentNode() &&
       !HTMLEditUtils::IsVisibleBRElement(*previousEditableContent) &&
       !HTMLEditUtils::IsBlockElement(*previousEditableContent,
                                      BlockInlineCheck::UseHTMLDefaultStyle);
       previousEditableContent = HTMLEditUtils::GetPreviousContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary,
           BlockInlineCheck::UseHTMLDefaultStyle, &aEditingHost)) {
    EditorDOMPoint atLastPreformattedNewLine =
        HTMLEditUtils::GetPreviousPreformattedNewLineInTextNode<EditorDOMPoint>(
            EditorRawDOMPoint::AtEndOf(*previousEditableContent));
    if (atLastPreformattedNewLine.IsSet()) {
      return atLastPreformattedNewLine.NextPoint();
    }
    point.Set(previousEditableContent);
  }

  // Finding the real start for this point unless current line starts after
  // <br> element.  Look up the tree for as long as we are the first node in
  // the container (typically, start of nearest block ancestor), and as long
  // as we haven't hit the body node.
  for (nsIContent* nearContent = HTMLEditUtils::GetPreviousContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary,
           BlockInlineCheck::UseHTMLDefaultStyle, &aEditingHost);
       !nearContent && !point.IsContainerHTMLElement(nsGkAtoms::body) &&
       point.GetContainerParent();
       nearContent = HTMLEditUtils::GetPreviousContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary,
           BlockInlineCheck::UseHTMLDefaultStyle, &aEditingHost)) {
    // Don't keep looking up if we have found a blockquote element to act on
    // when we handle outdent.
    // XXX Sounds like this is hacky.  If possible, it should be check in
    //     outdent handler for consistency between edit sub-actions.
    //     We should check Chromium's behavior of outdent when Selection
    //     starts from `<blockquote>` and starts from first child of
    //     `<blockquote>`.
    if (aEditSubAction == EditSubAction::eOutdent &&
        point.IsContainerHTMLElement(nsGkAtoms::blockquote)) {
      break;
    }

    // Don't walk past the editable section. Note that we need to check
    // before walking up to a parent because we need to return the parent
    // object, so the parent itself might not be in the editable area, but
    // it's OK if we're not performing a block-level action.
    bool blockLevelAction =
        aEditSubAction == EditSubAction::eIndent ||
        aEditSubAction == EditSubAction::eOutdent ||
        aEditSubAction == EditSubAction::eSetOrClearAlignment ||
        aEditSubAction == EditSubAction::eCreateOrRemoveBlock ||
        aEditSubAction == EditSubAction::eFormatBlockForHTMLCommand;
    // XXX So, does this check whether the container is removable or not? It
    //     seems that here can be rewritten as obviously what here tries to
    //     check.
    if (!point.GetContainerParent()->IsInclusiveDescendantOf(&aEditingHost) &&
        (blockLevelAction ||
         !point.GetContainer()->IsInclusiveDescendantOf(&aEditingHost))) {
      break;
    }

    // If we're formatting a block, we should reformat first ancestor format
    // block.
    if (aEditSubAction == EditSubAction::eFormatBlockForHTMLCommand &&
        HTMLEditUtils::IsFormatElementForFormatBlockCommand(
            *point.ContainerAs<Element>())) {
      point.Set(point.GetContainer());
      break;
    }

    point.Set(point.GetContainer());
  }
  return point;
}

/**
 * Get the point after the following line break or the block which breaks the
 * line containing aPointInLine.
 *
 * @return              If the line ends with a visible `<br>` element, returns
 *                      the point after the `<br>` element.
 *                      If the line ends with a preformatted linefeed, returns
 *                      the point after the linefeed unless it's an invisible
 *                      line break immediately before a block boundary.
 *                      If the line ends with a block boundary, returns the
 *                      point of the block.
 */
static EditorDOMPoint GetPointAfterFollowingLineBreakOrAtFollowingHTMLBlock(
    const EditorDOMPoint& aPointInLine, EditSubAction aEditSubAction,
    const Element& aEditingHost) {
  // FYI: This was moved from
  // https://searchfox.org/mozilla-central/rev/3419858c997f422e3e70020a46baae7f0ec6dacc/editor/libeditor/HTMLEditSubActionHandler.cpp#6541

  if (NS_WARN_IF(!aPointInLine.IsSet())) {
    return EditorDOMPoint();
  }

  EditorDOMPoint point(aPointInLine);
  // Start scanning from the container node if aPoint is in a text node.
  // XXX Perhaps, IsInDataNode() must be expected.
  if (point.IsInTextNode()) {
    if (NS_WARN_IF(!point.GetContainer()->GetParentNode())) {
      // Okay, can't promote any further
      // XXX Why don't we return end of the text node?
      return point;
    }
    EditorDOMPoint atNextPreformattedNewLine =
        HTMLEditUtils::GetInclusiveNextPreformattedNewLineInTextNode<
            EditorDOMPoint>(point);
    if (atNextPreformattedNewLine.IsSet()) {
      // If the linefeed is last character of the text node, it may be
      // invisible if it's immediately before a block boundary.  In such
      // case, we should return the block boundary.
      Element* maybeNonEditableBlockElement = nullptr;
      if (HTMLEditUtils::IsInvisiblePreformattedNewLine(
              atNextPreformattedNewLine, &maybeNonEditableBlockElement) &&
          maybeNonEditableBlockElement) {
        // If the block is a parent of the editing host, let's return end
        // of editing host.
        if (maybeNonEditableBlockElement == &aEditingHost ||
            !maybeNonEditableBlockElement->IsInclusiveDescendantOf(
                &aEditingHost)) {
          return EditorDOMPoint::AtEndOf(*maybeNonEditableBlockElement);
        }
        // If it's invisible because of parent block boundary, return end
        // of the block.  Otherwise, i.e., it's followed by a child block,
        // returns the point of the child block.
        if (atNextPreformattedNewLine.ContainerAs<Text>()
                ->IsInclusiveDescendantOf(maybeNonEditableBlockElement)) {
          return EditorDOMPoint::AtEndOf(*maybeNonEditableBlockElement);
        }
        return EditorDOMPoint(maybeNonEditableBlockElement);
      }
      // Otherwise, return the point after the preformatted linefeed.
      return atNextPreformattedNewLine.NextPoint();
    }
    // want to be after the text node
    point.SetAfter(point.GetContainer());
    NS_WARNING_ASSERTION(point.IsSet(), "Failed to set to after the text node");
  }

  // Look ahead through any further inline nodes that aren't across a <br> from
  // us, and that are enclosed in the same block.
  // XXX Currently, we stop block-extending when finding visible <br> element.
  //     This might be different from "block-extend" of execCommand spec.
  //     However, the spec is really unclear.
  // XXX Probably, scanning only editable nodes is wrong for
  //     EditSubAction::eCreateOrRemoveBlock and
  //     EditSubAction::eFormatBlockForHTMLCommand because it might be better to
  //     wrap existing inline elements even if it's non-editable.  For example,
  //     following examples with insertParagraph causes different result:
  //     * <div contenteditable>foo[]<b contenteditable="false">bar</b></div>
  //     * <div contenteditable>foo[]<b>bar</b></div>
  //     * <div contenteditable>foo[]<b contenteditable="false">bar</b>baz</div>
  //     Only in the first case, after the caret position isn't wrapped with
  //     new <div> element.
  constexpr HTMLEditUtils::WalkTreeOptions
      ignoreNonEditableNodeAndStopAtBlockBoundary{
          HTMLEditUtils::WalkTreeOption::IgnoreNonEditableNode,
          HTMLEditUtils::WalkTreeOption::StopAtBlockBoundary};
  for (nsIContent* nextEditableContent = HTMLEditUtils::GetNextContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary,
           BlockInlineCheck::UseHTMLDefaultStyle, &aEditingHost);
       nextEditableContent &&
       !HTMLEditUtils::IsBlockElement(*nextEditableContent,
                                      BlockInlineCheck::UseHTMLDefaultStyle) &&
       nextEditableContent->GetParent();
       nextEditableContent = HTMLEditUtils::GetNextContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary,
           BlockInlineCheck::UseHTMLDefaultStyle, &aEditingHost)) {
    EditorDOMPoint atFirstPreformattedNewLine =
        HTMLEditUtils::GetInclusiveNextPreformattedNewLineInTextNode<
            EditorDOMPoint>(EditorRawDOMPoint(nextEditableContent, 0));
    if (atFirstPreformattedNewLine.IsSet()) {
      // If the linefeed is last character of the text node, it may be
      // invisible if it's immediately before a block boundary.  In such
      // case, we should return the block boundary.
      Element* maybeNonEditableBlockElement = nullptr;
      if (HTMLEditUtils::IsInvisiblePreformattedNewLine(
              atFirstPreformattedNewLine, &maybeNonEditableBlockElement) &&
          maybeNonEditableBlockElement) {
        // If the block is a parent of the editing host, let's return end
        // of editing host.
        if (maybeNonEditableBlockElement == &aEditingHost ||
            !maybeNonEditableBlockElement->IsInclusiveDescendantOf(
                &aEditingHost)) {
          return EditorDOMPoint::AtEndOf(*maybeNonEditableBlockElement);
        }
        // If it's invisible because of parent block boundary, return end
        // of the block.  Otherwise, i.e., it's followed by a child block,
        // returns the point of the child block.
        if (atFirstPreformattedNewLine.ContainerAs<Text>()
                ->IsInclusiveDescendantOf(maybeNonEditableBlockElement)) {
          return EditorDOMPoint::AtEndOf(*maybeNonEditableBlockElement);
        }
        return EditorDOMPoint(maybeNonEditableBlockElement);
      }
      // Otherwise, return the point after the preformatted linefeed.
      return atFirstPreformattedNewLine.NextPoint();
    }
    point.SetAfter(nextEditableContent);
    if (NS_WARN_IF(!point.IsSet())) {
      break;
    }
    if (HTMLEditUtils::IsVisibleBRElement(*nextEditableContent)) {
      break;
    }
  }

  // Finding the real end for this point unless current line ends with a <br>
  // element.  Look up the tree for as long as we are the last node in the
  // container (typically, block node), and as long as we haven't hit the body
  // node.
  for (nsIContent* nearContent = HTMLEditUtils::GetNextContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary,
           BlockInlineCheck::UseHTMLDefaultStyle, &aEditingHost);
       !nearContent && !point.IsContainerHTMLElement(nsGkAtoms::body) &&
       point.GetContainerParent();
       nearContent = HTMLEditUtils::GetNextContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary,
           BlockInlineCheck::UseHTMLDefaultStyle, &aEditingHost)) {
    // Don't walk past the editable section. Note that we need to check before
    // walking up to a parent because we need to return the parent object, so
    // the parent itself might not be in the editable area, but it's OK.
    // XXX Maybe returning parent of editing host is really error prone since
    //     everybody need to check whether the end point is in editing host
    //     when they touch there.
    if (!point.GetContainer()->IsInclusiveDescendantOf(&aEditingHost) &&
        !point.GetContainerParent()->IsInclusiveDescendantOf(&aEditingHost)) {
      break;
    }

    // If we're formatting a block, we should reformat first ancestor format
    // block.
    if (aEditSubAction == EditSubAction::eFormatBlockForHTMLCommand &&
        HTMLEditUtils::IsFormatElementForFormatBlockCommand(
            *point.ContainerAs<Element>())) {
      point.SetAfter(point.GetContainer());
      break;
    }

    point.SetAfter(point.GetContainer());
    if (NS_WARN_IF(!point.IsSet())) {
      break;
    }
  }
  return point;
}

void AutoRangeArray::ExtendRangesToWrapLinesToHandleBlockLevelEditAction(
    EditSubAction aEditSubAction, const Element& aEditingHost) {
  // FYI: This is originated in
  // https://searchfox.org/mozilla-central/rev/1739f1301d658c9bff544a0a095ab11fca2e549d/editor/libeditor/HTMLEditSubActionHandler.cpp#6712

  bool removeSomeRanges = false;
  for (const OwningNonNull<nsRange>& range : mRanges) {
    // Remove non-positioned ranges.
    if (MOZ_UNLIKELY(!range->IsPositioned())) {
      removeSomeRanges = true;
      continue;
    }
    // If the range is native anonymous subtrees, we must meet a bug of
    // `Selection` so that we need to hack here.
    if (MOZ_UNLIKELY(range->GetStartContainer()->IsInNativeAnonymousSubtree() ||
                     range->GetEndContainer()->IsInNativeAnonymousSubtree())) {
      EditorRawDOMRange rawRange(range);
      if (!rawRange.EnsureNotInNativeAnonymousSubtree()) {
        range->Reset();
        removeSomeRanges = true;
        continue;
      }
      if (NS_FAILED(
              range->SetStartAndEnd(rawRange.StartRef().ToRawRangeBoundary(),
                                    rawRange.EndRef().ToRawRangeBoundary())) ||
          MOZ_UNLIKELY(!range->IsPositioned())) {
        range->Reset();
        removeSomeRanges = true;
        continue;
      }
    }
    // Finally, extend the range.
    if (NS_FAILED(ExtendRangeToWrapStartAndEndLinesContainingBoundaries(
            range, aEditSubAction, aEditingHost))) {
      // If we failed to extend the range, we should use the original range
      // as-is unless the range is broken at setting the range.
      if (NS_WARN_IF(!range->IsPositioned())) {
        removeSomeRanges = true;
      }
    }
  }
  if (removeSomeRanges) {
    for (const size_t i : Reversed(IntegerRange(mRanges.Length()))) {
      if (!mRanges[i]->IsPositioned()) {
        mRanges.RemoveElementAt(i);
      }
    }
    if (!mAnchorFocusRange || !mAnchorFocusRange->IsPositioned()) {
      if (mRanges.IsEmpty()) {
        mAnchorFocusRange = nullptr;
      } else {
        mAnchorFocusRange = mRanges.LastElement();
      }
    }
  }
}

// static
nsresult AutoRangeArray::ExtendRangeToWrapStartAndEndLinesContainingBoundaries(
    nsRange& aRange, EditSubAction aEditSubAction,
    const Element& aEditingHost) {
  MOZ_DIAGNOSTIC_ASSERT(
      !EditorRawDOMPoint(aRange.StartRef()).IsInNativeAnonymousSubtree());
  MOZ_DIAGNOSTIC_ASSERT(
      !EditorRawDOMPoint(aRange.EndRef()).IsInNativeAnonymousSubtree());

  if (NS_WARN_IF(!aRange.IsPositioned())) {
    return NS_ERROR_INVALID_ARG;
  }

  EditorDOMPoint startPoint(aRange.StartRef()), endPoint(aRange.EndRef());
  AutoRangeArray::UpdatePointsToSelectAllChildrenIfCollapsedInEmptyBlockElement(
      startPoint, endPoint, aEditingHost);

  // Make a new adjusted range to represent the appropriate block content.
  // This is tricky.  The basic idea is to push out the range endpoints to
  // truly enclose the blocks that we will affect.

  // Make sure that the new range ends up to be in the editable section.
  // XXX Looks like that this check wastes the time.  Perhaps, we should
  //     implement a method which checks both two DOM points in the editor
  //     root.

  startPoint =
      GetPointAtFirstContentOfLineOrParentHTMLBlockIfFirstContentOfBlock(
          startPoint, aEditSubAction, aEditingHost);
  // XXX GetPointAtFirstContentOfLineOrParentBlockIfFirstContentOfBlock() may
  //     return point of editing host.  Perhaps, we should change it and stop
  //     checking it here since this check may be expensive.
  // XXX If the container is an element in the editing host but it points end of
  //     the container, this returns nullptr.  Is it intentional?
  if (!startPoint.GetChildOrContainerIfDataNode() ||
      !startPoint.GetChildOrContainerIfDataNode()->IsInclusiveDescendantOf(
          &aEditingHost)) {
    return NS_ERROR_FAILURE;
  }
  endPoint = GetPointAfterFollowingLineBreakOrAtFollowingHTMLBlock(
      endPoint, aEditSubAction, aEditingHost);
  const EditorDOMPoint lastRawPoint =
      endPoint.IsStartOfContainer() ? endPoint : endPoint.PreviousPoint();
  // XXX GetPointAfterFollowingLineBreakOrAtFollowingBlock() may return point of
  //     editing host.  Perhaps, we should change it and stop checking it here
  //     since this check may be expensive.
  // XXX If the container is an element in the editing host but it points end of
  //     the container, this returns nullptr.  Is it intentional?
  if (!lastRawPoint.GetChildOrContainerIfDataNode() ||
      !lastRawPoint.GetChildOrContainerIfDataNode()->IsInclusiveDescendantOf(
          &aEditingHost)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = aRange.SetStartAndEnd(startPoint.ToRawRangeBoundary(),
                                      endPoint.ToRawRangeBoundary());
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

Result<EditorDOMPoint, nsresult>
AutoRangeArray::SplitTextAtEndBoundariesAndInlineAncestorsAtBothBoundaries(
    HTMLEditor& aHTMLEditor, BlockInlineCheck aBlockInlineCheck,
    const Element& aEditingHost,
    const nsIContent* aAncestorLimiter /* = nullptr */) {
  // FYI: The following code is originated in
  // https://searchfox.org/mozilla-central/rev/c8e15e17bc6fd28f558c395c948a6251b38774ff/editor/libeditor/HTMLEditSubActionHandler.cpp#6971

  // Split text nodes. This is necessary, since given ranges may end in text
  // nodes in case where part of a pre-formatted elements needs to be moved.
  EditorDOMPoint pointToPutCaret;
  IgnoredErrorResult ignoredError;
  for (const OwningNonNull<nsRange>& range : mRanges) {
    EditorDOMPoint atEnd(range->EndRef());
    if (NS_WARN_IF(!atEnd.IsSet()) || !atEnd.IsInTextNode() ||
        atEnd.GetContainer() == aAncestorLimiter) {
      continue;
    }

    if (!atEnd.IsStartOfContainer() && !atEnd.IsEndOfContainer()) {
      // Split the text node.
      Result<SplitNodeResult, nsresult> splitAtEndResult =
          aHTMLEditor.SplitNodeWithTransaction(atEnd);
      if (MOZ_UNLIKELY(splitAtEndResult.isErr())) {
        NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
        return splitAtEndResult.propagateErr();
      }
      SplitNodeResult unwrappedSplitAtEndResult = splitAtEndResult.unwrap();
      unwrappedSplitAtEndResult.MoveCaretPointTo(
          pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});

      // Correct the range.
      // The new end parent becomes the parent node of the text.
      MOZ_ASSERT(!range->IsInAnySelection());
      range->SetEnd(unwrappedSplitAtEndResult.AtNextContent<EditorRawDOMPoint>()
                        .ToRawRangeBoundary(),
                    ignoredError);
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "nsRange::SetEnd() failed, but ignored");
      ignoredError.SuppressException();
    }
  }

  // FYI: The following code is originated in
  // https://searchfox.org/mozilla-central/rev/c8e15e17bc6fd28f558c395c948a6251b38774ff/editor/libeditor/HTMLEditSubActionHandler.cpp#7023
  AutoTArray<OwningNonNull<RangeItem>, 8> rangeItemArray;
  rangeItemArray.AppendElements(mRanges.Length());

  // First register ranges for special editor gravity
  Maybe<size_t> anchorFocusRangeIndex;
  for (const size_t index : IntegerRange(rangeItemArray.Length())) {
    rangeItemArray[index] = new RangeItem();
    rangeItemArray[index]->StoreRange(*mRanges[index]);
    aHTMLEditor.RangeUpdaterRef().RegisterRangeItem(*rangeItemArray[index]);
    if (mRanges[index] == mAnchorFocusRange) {
      anchorFocusRangeIndex = Some(index);
    }
  }
  // TODO: We should keep the array, and just update the ranges.
  mRanges.Clear();
  mAnchorFocusRange = nullptr;
  // Now bust up inlines.
  nsresult rv = NS_OK;
  for (const OwningNonNull<RangeItem>& item : Reversed(rangeItemArray)) {
    // MOZ_KnownLive because 'rangeItemArray' is guaranteed to keep it alive.
    Result<EditorDOMPoint, nsresult> splitParentsResult =
        aHTMLEditor.SplitInlineAncestorsAtRangeBoundaries(
            MOZ_KnownLive(*item), aBlockInlineCheck, aEditingHost,
            aAncestorLimiter);
    if (MOZ_UNLIKELY(splitParentsResult.isErr())) {
      NS_WARNING("HTMLEditor::SplitInlineAncestorsAtRangeBoundaries() failed");
      rv = splitParentsResult.unwrapErr();
      break;
    }
    if (splitParentsResult.inspect().IsSet()) {
      pointToPutCaret = splitParentsResult.unwrap();
    }
  }
  // Then unregister the ranges
  for (const size_t index : IntegerRange(rangeItemArray.Length())) {
    aHTMLEditor.RangeUpdaterRef().DropRangeItem(rangeItemArray[index]);
    RefPtr<nsRange> range = rangeItemArray[index]->GetRange();
    if (range && range->IsPositioned()) {
      if (anchorFocusRangeIndex.isSome() && index == *anchorFocusRangeIndex) {
        mAnchorFocusRange = range;
      }
      mRanges.AppendElement(std::move(range));
    }
  }
  if (!mAnchorFocusRange && !mRanges.IsEmpty()) {
    mAnchorFocusRange = mRanges.LastElement();
  }

  // XXX Why do we ignore the other errors here??
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return Err(NS_ERROR_EDITOR_DESTROYED);
  }
  return pointToPutCaret;
}

nsresult AutoRangeArray::CollectEditTargetNodes(
    const HTMLEditor& aHTMLEditor,
    nsTArray<OwningNonNull<nsIContent>>& aOutArrayOfContents,
    EditSubAction aEditSubAction,
    CollectNonEditableNodes aCollectNonEditableNodes) const {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());

  // FYI: This was moved from
  // https://searchfox.org/mozilla-central/rev/4bce7d85ba4796dd03c5dcc7cfe8eee0e4c07b3b/editor/libeditor/HTMLEditSubActionHandler.cpp#7060

  // Gather up a list of all the nodes
  for (const OwningNonNull<nsRange>& range : mRanges) {
    DOMSubtreeIterator iter;
    nsresult rv = iter.Init(*range);
    if (NS_FAILED(rv)) {
      NS_WARNING("DOMSubtreeIterator::Init() failed");
      return rv;
    }
    if (aOutArrayOfContents.IsEmpty()) {
      iter.AppendAllNodesToArray(aOutArrayOfContents);
    } else {
      AutoTArray<OwningNonNull<nsIContent>, 24> arrayOfTopChildren;
      iter.AppendNodesToArray(
          +[](nsINode& aNode, void* aArray) -> bool {
            MOZ_ASSERT(aArray);
            return !static_cast<nsTArray<OwningNonNull<nsIContent>>*>(aArray)
                        ->Contains(&aNode);
          },
          arrayOfTopChildren, &aOutArrayOfContents);
      aOutArrayOfContents.AppendElements(std::move(arrayOfTopChildren));
    }
    if (aCollectNonEditableNodes == CollectNonEditableNodes::No) {
      for (const size_t i :
           Reversed(IntegerRange(aOutArrayOfContents.Length()))) {
        if (!EditorUtils::IsEditableContent(aOutArrayOfContents[i],
                                            EditorUtils::EditorType::HTML)) {
          aOutArrayOfContents.RemoveElementAt(i);
        }
      }
    }
  }

  switch (aEditSubAction) {
    case EditSubAction::eCreateOrRemoveBlock:
    case EditSubAction::eFormatBlockForHTMLCommand: {
      // Certain operations should not act on li's and td's, but rather inside
      // them.  Alter the list as needed.
      CollectChildrenOptions options = {
          CollectChildrenOption::CollectListChildren,
          CollectChildrenOption::CollectTableChildren};
      if (aCollectNonEditableNodes == CollectNonEditableNodes::No) {
        options += CollectChildrenOption::IgnoreNonEditableChildren;
      }
      if (aEditSubAction == EditSubAction::eCreateOrRemoveBlock) {
        for (const size_t index :
             Reversed(IntegerRange(aOutArrayOfContents.Length()))) {
          OwningNonNull<nsIContent> content = aOutArrayOfContents[index];
          if (HTMLEditUtils::IsListItem(content)) {
            aOutArrayOfContents.RemoveElementAt(index);
            HTMLEditUtils::CollectChildren(*content, aOutArrayOfContents, index,
                                           options);
          }
        }
      } else {
        // <dd> and <dt> are format blocks.  Therefore, we should not handle
        // their children directly.  They should be replaced with new format
        // block.
        MOZ_ASSERT(
            HTMLEditUtils::IsFormatTagForFormatBlockCommand(*nsGkAtoms::dt));
        MOZ_ASSERT(
            HTMLEditUtils::IsFormatTagForFormatBlockCommand(*nsGkAtoms::dd));
        for (const size_t index :
             Reversed(IntegerRange(aOutArrayOfContents.Length()))) {
          OwningNonNull<nsIContent> content = aOutArrayOfContents[index];
          MOZ_ASSERT_IF(HTMLEditUtils::IsListItem(content),
                        content->IsAnyOfHTMLElements(
                            nsGkAtoms::dd, nsGkAtoms::dt, nsGkAtoms::li));
          if (content->IsHTMLElement(nsGkAtoms::li)) {
            aOutArrayOfContents.RemoveElementAt(index);
            HTMLEditUtils::CollectChildren(*content, aOutArrayOfContents, index,
                                           options);
          }
        }
      }
      // Empty text node shouldn't be selected if unnecessary
      for (const size_t index :
           Reversed(IntegerRange(aOutArrayOfContents.Length()))) {
        if (const Text* text = aOutArrayOfContents[index]->GetAsText()) {
          // Don't select empty text except to empty block
          if (!HTMLEditUtils::IsVisibleTextNode(*text)) {
            aOutArrayOfContents.RemoveElementAt(index);
          }
        }
      }
      break;
    }
    case EditSubAction::eCreateOrChangeList: {
      // XXX aCollectNonEditableNodes is ignored here.  Maybe a bug.
      CollectChildrenOptions options = {
          CollectChildrenOption::CollectTableChildren};
      for (const size_t index :
           Reversed(IntegerRange(aOutArrayOfContents.Length()))) {
        // Scan for table elements.  If we find table elements other than
        // table, replace it with a list of any editable non-table content
        // because if a selection range starts from end in a table-cell and
        // ends at or starts from outside the `<table>`, we need to make
        // lists in each selected table-cells.
        OwningNonNull<nsIContent> content = aOutArrayOfContents[index];
        if (HTMLEditUtils::IsAnyTableElementButNotTable(content)) {
          aOutArrayOfContents.RemoveElementAt(index);
          HTMLEditUtils::CollectChildren(content, aOutArrayOfContents, index,
                                         options);
        }
      }
      // If there is only one node in the array, and it is a `<div>`,
      // `<blockquote>` or a list element, then look inside of it until we
      // find inner list or content.
      if (aOutArrayOfContents.Length() != 1) {
        break;
      }
      Element* deepestDivBlockquoteOrListElement =
          HTMLEditUtils::GetInclusiveDeepestFirstChildWhichHasOneChild(
              aOutArrayOfContents[0],
              {HTMLEditUtils::WalkTreeOption::IgnoreNonEditableNode},
              BlockInlineCheck::Unused, nsGkAtoms::div, nsGkAtoms::blockquote,
              nsGkAtoms::ul, nsGkAtoms::ol, nsGkAtoms::dl);
      if (!deepestDivBlockquoteOrListElement) {
        break;
      }
      if (deepestDivBlockquoteOrListElement->IsAnyOfHTMLElements(
              nsGkAtoms::div, nsGkAtoms::blockquote)) {
        aOutArrayOfContents.Clear();
        // XXX Before we're called, non-editable nodes are ignored.  However,
        //     we may append non-editable nodes here.
        HTMLEditUtils::CollectChildren(*deepestDivBlockquoteOrListElement,
                                       aOutArrayOfContents, 0, {});
        break;
      }
      aOutArrayOfContents.ReplaceElementAt(
          0, OwningNonNull<nsIContent>(*deepestDivBlockquoteOrListElement));
      break;
    }
    case EditSubAction::eOutdent:
    case EditSubAction::eIndent:
    case EditSubAction::eSetPositionToAbsolute: {
      // Indent/outdent already do something special for list items, but we
      // still need to make sure we don't act on table elements
      CollectChildrenOptions options = {
          CollectChildrenOption::CollectListChildren,
          CollectChildrenOption::CollectTableChildren};
      if (aCollectNonEditableNodes == CollectNonEditableNodes::No) {
        options += CollectChildrenOption::IgnoreNonEditableChildren;
      }
      for (const size_t index :
           Reversed(IntegerRange(aOutArrayOfContents.Length()))) {
        OwningNonNull<nsIContent> content = aOutArrayOfContents[index];
        if (HTMLEditUtils::IsAnyTableElementButNotTable(content)) {
          aOutArrayOfContents.RemoveElementAt(index);
          HTMLEditUtils::CollectChildren(*content, aOutArrayOfContents, index,
                                         options);
        }
      }
      break;
    }
    default:
      break;
  }

  // Outdent should look inside of divs.
  if (aEditSubAction == EditSubAction::eOutdent &&
      !aHTMLEditor.IsCSSEnabled()) {
    CollectChildrenOptions options = {};
    if (aCollectNonEditableNodes == CollectNonEditableNodes::No) {
      options += CollectChildrenOption::IgnoreNonEditableChildren;
    }
    for (const size_t index :
         Reversed(IntegerRange(aOutArrayOfContents.Length()))) {
      OwningNonNull<nsIContent> content = aOutArrayOfContents[index];
      if (content->IsHTMLElement(nsGkAtoms::div)) {
        aOutArrayOfContents.RemoveElementAt(index);
        HTMLEditUtils::CollectChildren(*content, aOutArrayOfContents, index,
                                       options);
      }
    }
  }

  return NS_OK;
}

Element* AutoRangeArray::GetClosestAncestorAnyListElementOfRange() const {
  for (const OwningNonNull<nsRange>& range : mRanges) {
    nsINode* commonAncestorNode = range->GetClosestCommonInclusiveAncestor();
    if (MOZ_UNLIKELY(!commonAncestorNode)) {
      continue;
    }
    for (Element* const element :
         commonAncestorNode->InclusiveAncestorsOfType<Element>()) {
      if (HTMLEditUtils::IsAnyListElement(element)) {
        return element;
      }
    }
  }
  return nullptr;
}

}  // namespace mozilla
