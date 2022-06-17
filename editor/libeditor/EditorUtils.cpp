/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditorUtils.h"

#include "EditorDOMPoint.h"
#include "HTMLEditHelpers.h"  // for MoveNodeResult
#include "HTMLEditUtils.h"    // for HTMLEditUtils
#include "TextEditor.h"
#include "WSRunObject.h"

#include "gfxFontUtils.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Text.h"
#include "nsContentUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsComputedDOMStyle.h"
#include "nsError.h"
#include "nsFrameSelection.h"
#include "nsIContent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
#include "nsINode.h"
#include "nsITransferable.h"
#include "nsRange.h"
#include "nsStyleStruct.h"
#include "nsTextFragment.h"

class nsISupports;

namespace mozilla {

using namespace dom;

/******************************************************************************
 * mozilla::EditActionResult
 *****************************************************************************/

EditActionResult& EditActionResult::operator|=(
    const MoveNodeResult& aMoveNodeResult) {
  mHandled |= aMoveNodeResult.Handled();
  // When both result are same, keep the result.
  if (mRv == aMoveNodeResult.inspectErr()) {
    return *this;
  }
  // If one of the result is NS_ERROR_EDITOR_DESTROYED, use it since it's
  // the most important error code for editor.
  if (EditorDestroyed() || aMoveNodeResult.EditorDestroyed()) {
    mRv = NS_ERROR_EDITOR_DESTROYED;
    return *this;
  }
  // If aMoveNodeResult hasn't been set explicit nsresult value, keep current
  // result.
  if (aMoveNodeResult.NotInitialized()) {
    return *this;
  }
  // If one of the results is error, use NS_ERROR_FAILURE.
  if (Failed() || aMoveNodeResult.isErr()) {
    mRv = NS_ERROR_FAILURE;
    return *this;
  }
  // Otherwise, use generic success code, NS_OK.
  mRv = NS_OK;
  return *this;
}

/******************************************************************************
 * mozilla::AutoRangeArray
 *****************************************************************************/

// static
bool AutoRangeArray::IsEditableRange(const dom::AbstractRange& aRange,
                                     const Element& aEditingHost) {
  // TODO: Perhaps, we should check whether the start/end boundaries are
  //       first/last point of non-editable element.
  //       See https://github.com/w3c/editing/issues/283#issuecomment-788654850
  EditorRawDOMPoint atStart(aRange.StartRef());
  const bool isStartEditable =
      atStart.IsInContentNode() &&
      EditorUtils::IsEditableContent(*atStart.ContainerAsContent(),
                                     EditorUtils::EditorType::HTML) &&
      !HTMLEditUtils::IsNonEditableReplacedContent(
          *atStart.ContainerAsContent());
  if (!isStartEditable) {
    return false;
  }

  if (aRange.GetStartContainer() != aRange.GetEndContainer()) {
    EditorRawDOMPoint atEnd(aRange.EndRef());
    const bool isEndEditable =
        atEnd.IsInContentNode() &&
        EditorUtils::IsEditableContent(*atEnd.ContainerAsContent(),
                                       EditorUtils::EditorType::HTML) &&
        !HTMLEditUtils::IsNonEditableReplacedContent(
            *atEnd.ContainerAsContent());
    if (!isEndEditable) {
      return false;
    }

    // Now, both start and end points are editable, but if they are in
    // different editing host, we cannot edit the range.
    if (atStart.ContainerAsContent() != atEnd.ContainerAsContent() &&
        atStart.ContainerAsContent()->GetEditingHost() !=
            atEnd.ContainerAsContent()->GetEditingHost()) {
      return false;
    }
  }

  // HTMLEditor does not support modifying outside `<body>` element for now.
  nsINode* commonAncestor = aRange.GetClosestCommonInclusiveAncestor();
  return commonAncestor && commonAncestor->IsContent() &&
         commonAncestor->IsInclusiveDescendantOf(&aEditingHost);
}

void AutoRangeArray::EnsureOnlyEditableRanges(const Element& aEditingHost) {
  for (size_t i = mRanges.Length(); i > 0; i--) {
    const OwningNonNull<nsRange>& range = mRanges[i - 1];
    if (!AutoRangeArray::IsEditableRange(range, aEditingHost)) {
      mRanges.RemoveElementAt(i - 1);
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
  for (uint32_t i : IntegerRange(mRanges.Length())) {
    const OwningNonNull<nsRange>& range = mRanges[i];
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
    for (uint32_t i : Reversed(IntegerRange(mRanges.Length() - 1u))) {
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

  // At this point, the anchor-focus ranges must match for bidi information.
  // See `EditorBase::AutoCaretBidiLevelManager`.
  MOZ_ASSERT(aEditorBase.SelectionRef().GetAnchorFocusRange()->StartRef() ==
             mAnchorFocusRange->StartRef());
  MOZ_ASSERT(aEditorBase.SelectionRef().GetAnchorFocusRange()->EndRef() ==
             mAnchorFocusRange->EndRef());

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
          aEditorBase.FindBetterInsertionPoint(atStartOfSelection);
      if (MOZ_UNLIKELY(!insertionPoint.IsSet())) {
        NS_WARNING(
            "EditorBase::FindBetterInsertionPoint() failed, but ignored");
        return aDirectionAndAmount;
      }

      if (!insertionPoint.IsInTextNode()) {
        return aDirectionAndAmount;
      }

      const nsTextFragment* data =
          &insertionPoint.GetContainerAsText()->TextFragment();
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
  for (auto& range : mRanges) {
    MOZ_ASSERT(!range->IsInSelection(),
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
          *aStartPoint.ContainerAsContent(),
          HTMLEditUtils::ClosestBlockElement);
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
GetPointAtFirstContentOfLineOrParentBlockIfFirstContentOfBlock(
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
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, &aEditingHost);
       previousEditableContent && previousEditableContent->GetParentNode() &&
       !HTMLEditUtils::IsVisibleBRElement(*previousEditableContent) &&
       !HTMLEditUtils::IsBlockElement(*previousEditableContent);
       previousEditableContent = HTMLEditUtils::GetPreviousContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, &aEditingHost)) {
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
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, &aEditingHost);
       !nearContent && !point.IsContainerHTMLElement(nsGkAtoms::body) &&
       point.GetContainerParent();
       nearContent = HTMLEditUtils::GetPreviousContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, &aEditingHost)) {
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
        aEditSubAction == EditSubAction::eCreateOrRemoveBlock;
    // XXX So, does this check whether the container is removable or not? It
    //     seems that here can be rewritten as obviously what here tries to
    //     check.
    if (!point.GetContainerParent()->IsInclusiveDescendantOf(&aEditingHost) &&
        (blockLevelAction ||
         !point.GetContainer()->IsInclusiveDescendantOf(&aEditingHost))) {
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
static EditorDOMPoint GetPointAfterFollowingLineBreakOrAtFollowingBlock(
    const EditorDOMPoint& aPointInLine, const Element& aEditingHost) {
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
      // case, we should retrun the block boundary.
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
        if (atNextPreformattedNewLine.ContainerAsText()
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
  //     EditSubAction::eCreateOrRemoveBlock because it might be better to wrap
  //     existing inline elements even if it's non-editable.  For example,
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
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, &aEditingHost);
       nextEditableContent &&
       !HTMLEditUtils::IsBlockElement(*nextEditableContent) &&
       nextEditableContent->GetParent();
       nextEditableContent = HTMLEditUtils::GetNextContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, &aEditingHost)) {
    EditorDOMPoint atFirstPreformattedNewLine =
        HTMLEditUtils::GetInclusiveNextPreformattedNewLineInTextNode<
            EditorDOMPoint>(EditorRawDOMPoint(nextEditableContent, 0));
    if (atFirstPreformattedNewLine.IsSet()) {
      // If the linefeed is last character of the text node, it may be
      // invisible if it's immediately before a block boundary.  In such
      // case, we should retrun the block boundary.
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
        if (atFirstPreformattedNewLine.ContainerAsText()
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
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, &aEditingHost);
       !nearContent && !point.IsContainerHTMLElement(nsGkAtoms::body) &&
       point.GetContainerParent();
       nearContent = HTMLEditUtils::GetNextContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, &aEditingHost)) {
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

    point.SetAfter(point.GetContainer());
    if (NS_WARN_IF(!point.IsSet())) {
      break;
    }
  }
  return point;
}

// static
already_AddRefed<nsRange>
AutoRangeArray::CreateRangeWrappingStartAndEndLinesContainingBoundaries(
    const EditorDOMRange& aRange, EditSubAction aEditSubAction,
    const Element& aEditingHost) {
  if (!aRange.IsPositioned()) {
    return nullptr;
  }
  return CreateRangeWrappingStartAndEndLinesContainingBoundaries(
      aRange.StartRef(), aRange.EndRef(), aEditSubAction, aEditingHost);
}

// static
already_AddRefed<nsRange>
AutoRangeArray::CreateRangeWrappingStartAndEndLinesContainingBoundaries(
    const EditorDOMPoint& aStartPoint, const EditorDOMPoint& aEndPoint,
    EditSubAction aEditSubAction, const Element& aEditingHost) {
  MOZ_DIAGNOSTIC_ASSERT(!aStartPoint.IsInNativeAnonymousSubtree());
  MOZ_DIAGNOSTIC_ASSERT(!aEndPoint.IsInNativeAnonymousSubtree());

  if (NS_WARN_IF(!aStartPoint.IsSet()) || NS_WARN_IF(!aEndPoint.IsSet())) {
    return nullptr;
  }

  EditorDOMPoint startPoint(aStartPoint), endPoint(aEndPoint);
  AutoRangeArray::UpdatePointsToSelectAllChildrenIfCollapsedInEmptyBlockElement(
      startPoint, endPoint, aEditingHost);

  // Make a new adjusted range to represent the appropriate block content.
  // This is tricky.  The basic idea is to push out the range endpoints to
  // truly enclose the blocks that we will affect.

  // Make sure that the new range ends up to be in the editable section.
  // XXX Looks like that this check wastes the time.  Perhaps, we should
  //     implement a method which checks both two DOM points in the editor
  //     root.

  startPoint = GetPointAtFirstContentOfLineOrParentBlockIfFirstContentOfBlock(
      startPoint, aEditSubAction, aEditingHost);
  // XXX GetPointAtFirstContentOfLineOrParentBlockIfFirstContentOfBlock() may
  //     return point of editing host.  Perhaps, we should change it and stop
  //     checking it here since this check may be expensive.
  // XXX If the container is an element in the editing host but it points end of
  //     the container, this returns nullptr.  Is it intentional?
  if (!startPoint.GetChildOrContainerIfDataNode() ||
      !startPoint.GetChildOrContainerIfDataNode()->IsInclusiveDescendantOf(
          &aEditingHost)) {
    return nullptr;
  }
  endPoint =
      GetPointAfterFollowingLineBreakOrAtFollowingBlock(endPoint, aEditingHost);
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
    return nullptr;
  }

  RefPtr<nsRange> range =
      nsRange::Create(startPoint.ToRawRangeBoundary(),
                      endPoint.ToRawRangeBoundary(), IgnoreErrors());
  NS_WARNING_ASSERTION(range, "nsRange::Create() failed");
  return range.forget();
}

/******************************************************************************
 * some general purpose editor utils
 *****************************************************************************/

bool EditorUtils::IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                                 EditorRawDOMPoint* aOutPoint /* = nullptr */) {
  if (aOutPoint) {
    aOutPoint->Clear();
  }

  if (&aNode == &aParent) {
    return false;
  }

  for (const nsINode* node = &aNode; node; node = node->GetParentNode()) {
    if (node->GetParentNode() == &aParent) {
      if (aOutPoint) {
        MOZ_ASSERT(node->IsContent());
        aOutPoint->Set(node->AsContent());
      }
      return true;
    }
  }

  return false;
}

bool EditorUtils::IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                                 EditorDOMPoint* aOutPoint) {
  MOZ_ASSERT(aOutPoint);
  aOutPoint->Clear();
  if (&aNode == &aParent) {
    return false;
  }

  for (const nsINode* node = &aNode; node; node = node->GetParentNode()) {
    if (node->GetParentNode() == &aParent) {
      MOZ_ASSERT(node->IsContent());
      aOutPoint->Set(node->AsContent());
      return true;
    }
  }

  return false;
}

// static
void EditorUtils::MaskString(nsString& aString, const Text& aTextNode,
                             uint32_t aStartOffsetInString,
                             uint32_t aStartOffsetInText) {
  MOZ_ASSERT(aTextNode.HasFlag(NS_MAYBE_MASKED));
  MOZ_ASSERT(aStartOffsetInString == 0 || aStartOffsetInText == 0);

  uint32_t unmaskStart = UINT32_MAX, unmaskLength = 0;
  TextEditor* textEditor =
      nsContentUtils::GetTextEditorFromAnonymousNodeWithoutCreation(&aTextNode);
  if (textEditor && textEditor->UnmaskedLength() > 0) {
    unmaskStart = textEditor->UnmaskedStart();
    unmaskLength = textEditor->UnmaskedLength();
    // If text is copied from after unmasked range, we can treat this case
    // as mask all.
    if (aStartOffsetInText >= unmaskStart + unmaskLength) {
      unmaskLength = 0;
      unmaskStart = UINT32_MAX;
    } else {
      // If text is copied from middle of unmasked range, reduce the length
      // and adjust start offset.
      if (aStartOffsetInText > unmaskStart) {
        unmaskLength = unmaskStart + unmaskLength - aStartOffsetInText;
        unmaskStart = 0;
      }
      // If text is copied from before start of unmasked range, just adjust
      // the start offset.
      else {
        unmaskStart -= aStartOffsetInText;
      }
      // Make the range is in the string.
      unmaskStart += aStartOffsetInString;
    }
  }

  const char16_t kPasswordMask = TextEditor::PasswordMask();
  for (uint32_t i = aStartOffsetInString; i < aString.Length(); ++i) {
    bool isSurrogatePair = NS_IS_HIGH_SURROGATE(aString.CharAt(i)) &&
                           i < aString.Length() - 1 &&
                           NS_IS_LOW_SURROGATE(aString.CharAt(i + 1));
    if (i < unmaskStart || i >= unmaskStart + unmaskLength) {
      if (isSurrogatePair) {
        aString.SetCharAt(kPasswordMask, i);
        aString.SetCharAt(kPasswordMask, i + 1);
      } else {
        aString.SetCharAt(kPasswordMask, i);
      }
    }

    // Skip the following low surrogate.
    if (isSurrogatePair) {
      ++i;
    }
  }
}

// static
bool EditorUtils::IsWhiteSpacePreformatted(const nsIContent& aContent) {
  // Look at the node (and its parent if it's not an element), and grab its
  // ComputedStyle.
  Element* element = aContent.GetAsElementOrParentElement();
  if (!element) {
    return false;
  }

  RefPtr<const ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(element);
  if (!elementStyle) {
    // Consider nodes without a ComputedStyle to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no ComputedStyle).
    return false;
  }

  return elementStyle->StyleText()->WhiteSpaceIsSignificant();
}

// static
bool EditorUtils::IsNewLinePreformatted(const nsIContent& aContent) {
  // Look at the node (and its parent if it's not an element), and grab its
  // ComputedStyle.
  Element* element = aContent.GetAsElementOrParentElement();
  if (!element) {
    return false;
  }

  RefPtr<const ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(element);
  if (!elementStyle) {
    // Consider nodes without a ComputedStyle to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no ComputedStyle).
    return false;
  }

  return elementStyle->StyleText()->NewlineIsSignificantStyle();
}

// static
bool EditorUtils::IsOnlyNewLinePreformatted(const nsIContent& aContent) {
  // Look at the node (and its parent if it's not an element), and grab its
  // ComputedStyle.
  Element* element = aContent.GetAsElementOrParentElement();
  if (!element) {
    return false;
  }

  RefPtr<const ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(element);
  if (!elementStyle) {
    // Consider nodes without a ComputedStyle to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no ComputedStyle).
    return false;
  }

  return elementStyle->StyleText()->mWhiteSpace == StyleWhiteSpace::PreLine;
}

bool EditorUtils::IsPointInSelection(const Selection& aSelection,
                                     const nsINode& aParentNode,
                                     uint32_t aOffset) {
  if (aSelection.IsCollapsed()) {
    return false;
  }

  const uint32_t rangeCount = aSelection.RangeCount();
  for (const uint32_t i : IntegerRange(rangeCount)) {
    MOZ_ASSERT(aSelection.RangeCount() == rangeCount);
    RefPtr<const nsRange> range = aSelection.GetRangeAt(i);
    if (MOZ_UNLIKELY(NS_WARN_IF(!range))) {
      // Don't bail yet, iterate through them all
      continue;
    }

    IgnoredErrorResult ignoredError;
    bool nodeIsInSelection =
        range->IsPointInRange(aParentNode, aOffset, ignoredError) &&
        !ignoredError.Failed();
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "nsRange::IsPointInRange() failed");

    // Done when we find a range that we are in
    if (nodeIsInSelection) {
      return true;
    }
  }

  return false;
}

// static
Result<nsCOMPtr<nsITransferable>, nsresult>
EditorUtils::CreateTransferableForPlainText(const Document& aDocument) {
  // Create generic Transferable for getting the data
  nsresult rv;
  nsCOMPtr<nsITransferable> transferable =
      do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("do_CreateInstance() failed to create nsITransferable instance");
    return Err(rv);
  }

  if (!transferable) {
    NS_WARNING("do_CreateInstance() returned nullptr, but ignored");
    return nsCOMPtr<nsITransferable>();
  }

  DebugOnly<nsresult> rvIgnored =
      transferable->Init(aDocument.GetLoadContext());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "nsITransferable::Init() failed, but ignored");

  rvIgnored = transferable->AddDataFlavor(kUnicodeMime);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsITransferable::AddDataFlavor(kUnicodeMime) failed, but ignored");
  rvIgnored = transferable->AddDataFlavor(kMozTextInternal);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsITransferable::AddDataFlavor(kMozTextInternal) failed, but ignored");
  return transferable;
}

/******************************************************************************
 * mozilla::EditorDOMPointBase
 *****************************************************************************/

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool, IsCharCollapsibleASCIISpace);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsCharCollapsibleASCIISpace() const {
  if (IsCharNewLine()) {
    return !EditorUtils::IsNewLinePreformatted(*ContainerAsText());
  }
  return IsCharASCIISpace() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool, IsCharCollapsibleNBSP);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsCharCollapsibleNBSP() const {
  // TODO: Perhaps, we should return false if neither previous char nor
  //       next char is collapsible white-space or NBSP.
  return IsCharNBSP() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool,
                                             IsCharCollapsibleASCIISpaceOrNBSP);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsCharCollapsibleASCIISpaceOrNBSP() const {
  if (IsCharNewLine()) {
    return !EditorUtils::IsNewLinePreformatted(*ContainerAsText());
  }
  return IsCharASCIISpaceOrNBSP() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(
    bool, IsPreviousCharCollapsibleASCIISpace);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsPreviousCharCollapsibleASCIISpace() const {
  if (IsPreviousCharNewLine()) {
    return !EditorUtils::IsNewLinePreformatted(*ContainerAsText());
  }
  return IsPreviousCharASCIISpace() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool,
                                             IsPreviousCharCollapsibleNBSP);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsPreviousCharCollapsibleNBSP() const {
  return IsPreviousCharNBSP() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(
    bool, IsPreviousCharCollapsibleASCIISpaceOrNBSP);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsPreviousCharCollapsibleASCIISpaceOrNBSP()
    const {
  if (IsPreviousCharNewLine()) {
    return !EditorUtils::IsNewLinePreformatted(*ContainerAsText());
  }
  return IsPreviousCharASCIISpaceOrNBSP() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool,
                                             IsNextCharCollapsibleASCIISpace);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsNextCharCollapsibleASCIISpace() const {
  if (IsNextCharNewLine()) {
    return !EditorUtils::IsNewLinePreformatted(*ContainerAsText());
  }
  return IsNextCharASCIISpace() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool, IsNextCharCollapsibleNBSP);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsNextCharCollapsibleNBSP() const {
  return IsNextCharNBSP() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(
    bool, IsNextCharCollapsibleASCIISpaceOrNBSP);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsNextCharCollapsibleASCIISpaceOrNBSP() const {
  if (IsNextCharNewLine()) {
    return !EditorUtils::IsNewLinePreformatted(*ContainerAsText());
  }
  return IsNextCharASCIISpaceOrNBSP() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool, IsCharPreformattedNewLine);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsCharPreformattedNewLine() const {
  return IsCharNewLine() &&
         EditorUtils::IsNewLinePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(
    bool, IsCharPreformattedNewLineCollapsedWithWhiteSpaces);

template <typename PT, typename CT>
bool EditorDOMPointBase<
    PT, CT>::IsCharPreformattedNewLineCollapsedWithWhiteSpaces() const {
  return IsCharNewLine() &&
         EditorUtils::IsOnlyNewLinePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool,
                                             IsPreviousCharPreformattedNewLine);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsPreviousCharPreformattedNewLine() const {
  return IsPreviousCharNewLine() &&
         EditorUtils::IsNewLinePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(
    bool, IsPreviousCharPreformattedNewLineCollapsedWithWhiteSpaces);

template <typename PT, typename CT>
bool EditorDOMPointBase<
    PT, CT>::IsPreviousCharPreformattedNewLineCollapsedWithWhiteSpaces() const {
  return IsPreviousCharNewLine() &&
         EditorUtils::IsOnlyNewLinePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool,
                                             IsNextCharPreformattedNewLine);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsNextCharPreformattedNewLine() const {
  return IsNextCharNewLine() &&
         EditorUtils::IsNewLinePreformatted(*ContainerAsText());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(
    bool, IsNextCharPreformattedNewLineCollapsedWithWhiteSpaces);

template <typename PT, typename CT>
bool EditorDOMPointBase<
    PT, CT>::IsNextCharPreformattedNewLineCollapsedWithWhiteSpaces() const {
  return IsNextCharNewLine() &&
         EditorUtils::IsOnlyNewLinePreformatted(*ContainerAsText());
}

/******************************************************************************
 * mozilla::CreateNodeResultBase
 *****************************************************************************/

NS_INSTANTIATE_CREATE_NODE_RESULT_CONST_METHOD(
    nsresult, SuggestCaretPointTo, const EditorBase& aEditorBase,
    const SuggestCaretOptions& aOptions)

template <typename NodeType>
nsresult CreateNodeResultBase<NodeType>::SuggestCaretPointTo(
    const EditorBase& aEditorBase, const SuggestCaretOptions& aOptions) const {
  mHandledCaretPoint = true;
  if (!mCaretPoint.IsSet()) {
    if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion)) {
      return NS_OK;
    }
    NS_WARNING("There was no suggestion to put caret");
    return NS_ERROR_FAILURE;
  }
  if (aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt) &&
      !aEditorBase.AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }
  nsresult rv = aEditorBase.CollapseSelectionTo(mCaretPoint);
  if (MOZ_UNLIKELY(rv == NS_ERROR_EDITOR_DESTROYED)) {
    NS_WARNING(
        "EditorBase::CollapseSelectionTo() caused destroying the editor");
    return NS_ERROR_EDITOR_DESTROYED;
  }
  return aOptions.contains(SuggestCaret::AndIgnoreTrivialError) && NS_FAILED(rv)
             ? NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR
             : rv;
}

NS_INSTANTIATE_CREATE_NODE_RESULT_METHOD(bool, MoveCaretPointTo,
                                         EditorDOMPoint& aPointToPutCaret,
                                         const EditorBase& aEditorBase,
                                         const SuggestCaretOptions& aOptions)

template <typename NodeType>
bool CreateNodeResultBase<NodeType>::MoveCaretPointTo(
    EditorDOMPoint& aPointToPutCaret, const EditorBase& aEditorBase,
    const SuggestCaretOptions& aOptions) {
  MOZ_ASSERT(!aOptions.contains(SuggestCaret::AndIgnoreTrivialError));
  mHandledCaretPoint = true;
  if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion) &&
      !mCaretPoint.IsSet()) {
    return false;
  }
  if (aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt) &&
      !aEditorBase.AllowsTransactionsToChangeSelection()) {
    return false;
  }
  aPointToPutCaret = UnwrapCaretPoint();
  return true;
}

}  // namespace mozilla
