/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WSRunObject.h"

#include "EditorDOMPoint.h"
#include "EditorUtils.h"
#include "HTMLEditor.h"
#include "HTMLEditUtils.h"
#include "SelectionState.h"

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/mozalloc.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/RangeUtils.h"
#include "mozilla/StaticPrefs_dom.h"     // for StaticPrefs::dom_*
#include "mozilla/StaticPrefs_editor.h"  // for StaticPrefs::editor_*
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/dom/AncestorIterator.h"

#include "nsAString.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsISupportsImpl.h"
#include "nsRange.h"
#include "nsString.h"
#include "nsTextFragment.h"

namespace mozilla {

using namespace dom;

using LeafNodeType = HTMLEditUtils::LeafNodeType;
using LeafNodeTypes = HTMLEditUtils::LeafNodeTypes;
using WalkTreeOption = HTMLEditUtils::WalkTreeOption;

template WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPoint& aPoint) const;
template WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorRawDOMPoint& aPoint) const;
template WSScanResult WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPoint& aPoint) const;
template WSScanResult WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom(
    const EditorRawDOMPoint& aPoint) const;
template EditorDOMPoint WSRunScanner::GetAfterLastVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter);
template EditorRawDOMPoint WSRunScanner::GetAfterLastVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter);
template EditorDOMPoint WSRunScanner::GetFirstVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter);
template EditorRawDOMPoint WSRunScanner::GetFirstVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter);

template nsresult WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aScanStartPoint);
template nsresult WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt(
    HTMLEditor& aHTMLEditor, const EditorRawDOMPoint& aScanStartPoint);
template nsresult WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt(
    HTMLEditor& aHTMLEditor, const EditorDOMPointInText& aScanStartPoint);

template WSRunScanner::TextFragmentData::TextFragmentData(
    const EditorDOMPoint& aPoint, const Element* aEditingHost);
template WSRunScanner::TextFragmentData::TextFragmentData(
    const EditorRawDOMPoint& aPoint, const Element* aEditingHost);
template WSRunScanner::TextFragmentData::TextFragmentData(
    const EditorDOMPointInText& aPoint, const Element* aEditingHost);

NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetInclusiveNextEditableCharPoint,
    const EditorDOMPoint& aPoint);
NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetInclusiveNextEditableCharPoint,
    const EditorRawDOMPoint& aPoint);
NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetInclusiveNextEditableCharPoint,
    const EditorDOMPointInText& aPoint);
NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetInclusiveNextEditableCharPoint,
    const EditorRawDOMPointInText& aPoint);

NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetPreviousEditableCharPoint,
    const EditorDOMPoint& aPoint);
NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetPreviousEditableCharPoint,
    const EditorRawDOMPoint& aPoint);
NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetPreviousEditableCharPoint,
    const EditorDOMPointInText& aPoint);
NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetPreviousEditableCharPoint,
    const EditorRawDOMPointInText& aPoint);

Result<EditorDOMPoint, nsresult>
WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToSplit,
    const Element& aSplittingBlockElement) {
  if (NS_WARN_IF(!aPointToSplit.IsInContentNode()) ||
      NS_WARN_IF(!HTMLEditUtils::IsSplittableNode(aSplittingBlockElement)) ||
      NS_WARN_IF(!EditorUtils::IsEditableContent(
          *aPointToSplit.ContainerAsContent(), EditorType::HTML))) {
    return Err(NS_ERROR_FAILURE);
  }

  // The container of aPointToSplit may be not splittable, e.g., selection
  // may be collapsed **in** a `<br>` element or a comment node.  So, look
  // for splittable point with climbing the tree up.
  EditorDOMPoint pointToSplit(aPointToSplit);
  for (nsIContent* content : aPointToSplit.ContainerAsContent()
                                 ->InclusiveAncestorsOfType<nsIContent>()) {
    if (content == &aSplittingBlockElement) {
      break;
    }
    if (HTMLEditUtils::IsSplittableNode(*content)) {
      break;
    }
    pointToSplit.Set(content);
  }

  {
    AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(), &pointToSplit);

    nsresult rv = WhiteSpaceVisibilityKeeper::
        MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit(aHTMLEditor,
                                                          pointToSplit);
    if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
      return Err(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::"
          "MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit() failed");
      return Err(rv);
    }
  }

  if (NS_WARN_IF(!pointToSplit.IsInContentNode()) ||
      NS_WARN_IF(!pointToSplit.ContainerAsContent()->IsInclusiveDescendantOf(
          &aSplittingBlockElement)) ||
      NS_WARN_IF(!HTMLEditUtils::IsSplittableNode(aSplittingBlockElement)) ||
      NS_WARN_IF(!HTMLEditUtils::IsSplittableNode(
          *pointToSplit.ContainerAsContent()))) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  return pointToSplit;
}

// static
EditActionResult WhiteSpaceVisibilityKeeper::
    MergeFirstLineOfRightBlockElementIntoDescendantLeftBlockElement(
        HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
        Element& aRightBlockElement, const EditorDOMPoint& aAtRightBlockChild,
        const Maybe<nsAtom*>& aListElementTagName,
        const HTMLBRElement* aPrecedingInvisibleBRElement,
        const Element& aEditingHost) {
  MOZ_ASSERT(
      EditorUtils::IsDescendantOf(aLeftBlockElement, aRightBlockElement));
  MOZ_ASSERT(&aRightBlockElement == aAtRightBlockChild.GetContainer());

  // NOTE: This method may extend deletion range:
  // - to delete invisible white-spaces at end of aLeftBlockElement
  // - to delete invisible white-spaces at start of
  //   afterRightBlockChild.GetChild()
  // - to delete invisible white-spaces before afterRightBlockChild.GetChild()
  // - to delete invisible `<br>` element at end of aLeftBlockElement

  AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);

  nsresult rv = WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces(
      aHTMLEditor, EditorDOMPoint::AtEndOf(aLeftBlockElement));
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces() "
        "failed at left block");
    return EditActionResult(rv);
  }

  // Check whether aLeftBlockElement is a descendant of aRightBlockElement.
  if (aHTMLEditor.MayHaveMutationEventListeners()) {
    EditorDOMPoint leftBlockContainingPointInRightBlockElement;
    if (aHTMLEditor.MayHaveMutationEventListeners() &&
        !EditorUtils::IsDescendantOf(
            aLeftBlockElement, aRightBlockElement,
            &leftBlockContainingPointInRightBlockElement)) {
      NS_WARNING(
          "Deleting invisible whitespace at end of left block element caused "
          "moving the left block element outside the right block element");
      return EditActionResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }

    if (leftBlockContainingPointInRightBlockElement != aAtRightBlockChild) {
      NS_WARNING(
          "Deleting invisible whitespace at end of left block element caused "
          "changing the left block element in the right block element");
      return EditActionResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }

    if (!EditorUtils::IsEditableContent(aRightBlockElement, EditorType::HTML)) {
      NS_WARNING(
          "Deleting invisible whitespace at end of left block element caused "
          "making the right block element non-editable");
      return EditActionResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }

    if (!EditorUtils::IsEditableContent(aLeftBlockElement, EditorType::HTML)) {
      NS_WARNING(
          "Deleting invisible whitespace at end of left block element caused "
          "making the left block element non-editable");
      return EditActionResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }

  OwningNonNull<Element> rightBlockElement = aRightBlockElement;
  EditorDOMPoint afterRightBlockChild = aAtRightBlockChild.NextPoint();
  {
    // We can't just track rightBlockElement because it's an Element.
    AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(),
                              &afterRightBlockChild);
    nsresult rv = WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces(
        aHTMLEditor, afterRightBlockChild);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces() "
          "failed at right block child");
      return EditActionResult(rv);
    }

    // XXX AutoTrackDOMPoint instance, tracker, hasn't been destroyed here.
    //     Do we really need to do update rightBlockElement here??
    // XXX And afterRightBlockChild.GetContainerAsElement() always returns
    //     an element pointer so that probably here should not use
    //     accessors of EditorDOMPoint, should use DOM API directly instead.
    if (afterRightBlockChild.GetContainerAsElement()) {
      rightBlockElement = *afterRightBlockChild.GetContainerAsElement();
    } else if (NS_WARN_IF(
                   !afterRightBlockChild.GetContainerParentAsElement())) {
      return EditActionResult(NS_ERROR_UNEXPECTED);
    } else {
      rightBlockElement = *afterRightBlockChild.GetContainerParentAsElement();
    }
  }

  // Do br adjustment.
  RefPtr<HTMLBRElement> invisibleBRElementAtEndOfLeftBlockElement =
      WSRunScanner::GetPrecedingBRElementUnlessVisibleContentFound(
          aHTMLEditor.ComputeEditingHost(),
          EditorDOMPoint::AtEndOf(aLeftBlockElement));
  NS_ASSERTION(
      aPrecedingInvisibleBRElement == invisibleBRElementAtEndOfLeftBlockElement,
      "The preceding invisible BR element computation was different");
  EditActionResult ret(NS_OK);
  // NOTE: Keep syncing with CanMergeLeftAndRightBlockElements() of
  //       AutoInclusiveAncestorBlockElementsJoiner.
  if (NS_WARN_IF(aListElementTagName.isSome())) {
    // Since 2002, here was the following comment:
    // > The idea here is to take all children in rightListElement that are
    // > past offset, and pull them into leftlistElement.
    // However, this has never been performed because we are here only when
    // neither left list nor right list is a descendant of the other but
    // in such case, getting a list item in the right list node almost
    // always failed since a variable for offset of
    // rightListElement->GetChildAt() was not initialized.  So, it might be
    // a bug, but we should keep this traditional behavior for now.  If you
    // find when we get here, please remove this comment if we don't need to
    // do it.  Otherwise, please move children of the right list node to the
    // end of the left list node.

    // XXX Although, we do nothing here, but for keeping traditional
    //     behavior, we should mark as handled.
    ret.MarkAsHandled();
  } else {
    // XXX Why do we ignore the result of
    //     MoveOneHardLineContentsWithTransaction()?
    NS_ASSERTION(rightBlockElement == afterRightBlockChild.GetContainer(),
                 "The relation is not guaranteed but assumed");
#ifdef DEBUG
    Result<bool, nsresult> firstLineHasContent =
        aHTMLEditor.CanMoveOrDeleteSomethingInHardLine(
            EditorDOMPoint(rightBlockElement, afterRightBlockChild.Offset()),
            aEditingHost);
#endif  // #ifdef DEBUG
    MoveNodeResult moveNodeResult =
        aHTMLEditor.MoveOneHardLineContentsWithTransaction(
            EditorDOMPoint(rightBlockElement, afterRightBlockChild.Offset()),
            EditorDOMPoint(&aLeftBlockElement, 0u), aEditingHost,
            HTMLEditor::MoveToEndOfContainer::Yes);
    if (moveNodeResult.isErr()) {
      NS_WARNING(
          "HTMLEditor::MoveOneHardLineContentsWithTransaction("
          "MoveToEndOfContainer::Yes) failed");
      return EditActionResult(moveNodeResult.unwrapErr());
    }

#ifdef DEBUG
    MOZ_ASSERT(!firstLineHasContent.isErr());
    if (firstLineHasContent.inspect()) {
      NS_ASSERTION(moveNodeResult.Handled(),
                   "Failed to consider whether moving or not something");
    } else {
      NS_ASSERTION(moveNodeResult.Ignored(),
                   "Failed to consider whether moving or not something");
    }
#endif  // #ifdef DEBUG

    // We don't need to update selection here because of dontChangeMySelection
    // above.
    moveNodeResult.IgnoreCaretPointSuggestion();
    ret |= moveNodeResult;
    // Now, all children of rightBlockElement were moved to leftBlockElement.
    // So, afterRightBlockChild is now invalid.
    afterRightBlockChild.Clear();
  }

  if (!invisibleBRElementAtEndOfLeftBlockElement) {
    return ret;
  }

  rv = aHTMLEditor.DeleteNodeWithTransaction(
      *invisibleBRElementAtEndOfLeftBlockElement);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed, but ignored");
    return EditActionResult(rv);
  }
  return EditActionHandled();
}

// static
EditActionResult WhiteSpaceVisibilityKeeper::
    MergeFirstLineOfRightBlockElementIntoAncestorLeftBlockElement(
        HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
        Element& aRightBlockElement, const EditorDOMPoint& aAtLeftBlockChild,
        nsIContent& aLeftContentInBlock,
        const Maybe<nsAtom*>& aListElementTagName,
        const HTMLBRElement* aPrecedingInvisibleBRElement,
        const Element& aEditingHost) {
  MOZ_ASSERT(
      EditorUtils::IsDescendantOf(aRightBlockElement, aLeftBlockElement));
  MOZ_ASSERT(
      &aLeftBlockElement == &aLeftContentInBlock ||
      EditorUtils::IsDescendantOf(aLeftContentInBlock, aLeftBlockElement));
  MOZ_ASSERT(&aLeftBlockElement == aAtLeftBlockChild.GetContainer());

  // NOTE: This method may extend deletion range:
  // - to delete invisible white-spaces at start of aRightBlockElement
  // - to delete invisible white-spaces before aRightBlockElement
  // - to delete invisible white-spaces at start of aAtLeftBlockChild.GetChild()
  // - to delete invisible white-spaces before aAtLeftBlockChild.GetChild()
  // - to delete invisible `<br>` element before aAtLeftBlockChild.GetChild()

  AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);

  nsresult rv = WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces(
      aHTMLEditor, EditorDOMPoint(&aRightBlockElement, 0));
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces() failed "
        "at right block");
    return EditActionResult(rv);
  }

  // Check whether aRightBlockElement is a descendant of aLeftBlockElement.
  if (aHTMLEditor.MayHaveMutationEventListeners()) {
    EditorDOMPoint rightBlockContainingPointInLeftBlockElement;
    if (aHTMLEditor.MayHaveMutationEventListeners() &&
        !EditorUtils::IsDescendantOf(
            aRightBlockElement, aLeftBlockElement,
            &rightBlockContainingPointInLeftBlockElement)) {
      NS_WARNING(
          "Deleting invisible whitespace at start of right block element "
          "caused moving the right block element outside the left block "
          "element");
      return EditActionResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }

    if (rightBlockContainingPointInLeftBlockElement != aAtLeftBlockChild) {
      NS_WARNING(
          "Deleting invisible whitespace at start of right block element "
          "caused changing the right block element position in the left block "
          "element");
      return EditActionResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }

    if (!EditorUtils::IsEditableContent(aLeftBlockElement, EditorType::HTML)) {
      NS_WARNING(
          "Deleting invisible whitespace at start of right block element "
          "caused making the left block element non-editable");
      return EditActionResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }

    if (!EditorUtils::IsEditableContent(aRightBlockElement, EditorType::HTML)) {
      NS_WARNING(
          "Deleting invisible whitespace at start of right block element "
          "caused making the right block element non-editable");
      return EditActionResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }

  OwningNonNull<Element> originalLeftBlockElement = aLeftBlockElement;
  OwningNonNull<Element> leftBlockElement = aLeftBlockElement;
  EditorDOMPoint atLeftBlockChild(aAtLeftBlockChild);
  {
    // We can't just track leftBlockElement because it's an Element, so track
    // something else.
    AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(), &atLeftBlockChild);
    nsresult rv = WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces(
        aHTMLEditor, EditorDOMPoint(atLeftBlockChild.GetContainer(),
                                    atLeftBlockChild.Offset()));
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces() "
          "failed at left block child");
      return EditActionResult(rv);
    }
  }
  if (!atLeftBlockChild.IsSetAndValid()) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces() caused "
        "unexpected DOM tree");
    return EditActionResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  // XXX atLeftBlockChild.GetContainerAsElement() should always return
  //     an element pointer so that probably here should not use
  //     accessors of EditorDOMPoint, should use DOM API directly instead.
  if (Element* nearestAncestor =
          atLeftBlockChild.GetContainerOrContainerParentElement()) {
    leftBlockElement = *nearestAncestor;
  } else {
    return EditActionResult(NS_ERROR_UNEXPECTED);
  }

  // Do br adjustment.
  RefPtr<HTMLBRElement> invisibleBRElementBeforeLeftBlockElement =
      WSRunScanner::GetPrecedingBRElementUnlessVisibleContentFound(
          aHTMLEditor.ComputeEditingHost(), atLeftBlockChild);
  NS_ASSERTION(
      aPrecedingInvisibleBRElement == invisibleBRElementBeforeLeftBlockElement,
      "The preceding invisible BR element computation was different");
  EditActionResult ret(NS_OK);
  // NOTE: Keep syncing with CanMergeLeftAndRightBlockElements() of
  //       AutoInclusiveAncestorBlockElementsJoiner.
  if (aListElementTagName.isSome()) {
    // XXX Why do we ignore the error from MoveChildrenWithTransaction()?
    MOZ_ASSERT(originalLeftBlockElement == atLeftBlockChild.GetContainer(),
               "This is not guaranteed, but assumed");
#ifdef DEBUG
    Result<bool, nsresult> rightBlockHasContent =
        aHTMLEditor.CanMoveChildren(aRightBlockElement, aLeftBlockElement);
#endif  // #ifdef DEBUG
    MoveNodeResult moveNodeResult = aHTMLEditor.MoveChildrenWithTransaction(
        aRightBlockElement, EditorDOMPoint(atLeftBlockChild.GetContainer(),
                                           atLeftBlockChild.Offset()));
    if (NS_WARN_IF(moveNodeResult.EditorDestroyed())) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(
        moveNodeResult.isOk(),
        "HTMLEditor::MoveChildrenWithTransaction() failed, but ignored");
    if (moveNodeResult.isOk()) {
      // We don't need to update selection here because of dontChangeMySelection
      // above.
      moveNodeResult.IgnoreCaretPointSuggestion();
      ret |= moveNodeResult;

#ifdef DEBUG
      MOZ_ASSERT(!rightBlockHasContent.isErr());
      if (rightBlockHasContent.inspect()) {
        NS_ASSERTION(moveNodeResult.Handled(),
                     "Failed to consider whether moving or not children");
      } else {
        NS_ASSERTION(moveNodeResult.Ignored(),
                     "Failed to consider whether moving or not children");
      }
#endif  // #ifdef DEBUG
    }
    // atLeftBlockChild was moved to rightListElement.  So, it's invalid now.
    atLeftBlockChild.Clear();
  } else {
    // Left block is a parent of right block, and the parent of the previous
    // visible content.  Right block is a child and contains the contents we
    // want to move.

    EditorDOMPoint atPreviousContent;
    if (&aLeftContentInBlock == leftBlockElement) {
      // We are working with valid HTML, aLeftContentInBlock is a block element,
      // and is therefore allowed to contain aRightBlockElement.  This is the
      // simple case, we will simply move the content in aRightBlockElement
      // out of its block.
      atPreviousContent = atLeftBlockChild;
    } else {
      // We try to work as well as possible with HTML that's already invalid.
      // Although "right block" is a block, and a block must not be contained
      // in inline elements, reality is that broken documents do exist.  The
      // DIRECT parent of "left NODE" might be an inline element.  Previous
      // versions of this code skipped inline parents until the first block
      // parent was found (and used "left block" as the destination).
      // However, in some situations this strategy moves the content to an
      // unexpected position.  (see bug 200416) The new idea is to make the
      // moving content a sibling, next to the previous visible content.
      atPreviousContent.Set(&aLeftContentInBlock);

      // We want to move our content just after the previous visible node.
      atPreviousContent.AdvanceOffset();
    }

    MOZ_ASSERT(atPreviousContent.IsSetAndValid());

    // Because we don't want the moving content to receive the style of the
    // previous content, we split the previous content's style.

#ifdef DEBUG
    Result<bool, nsresult> firstLineHasContent =
        aHTMLEditor.CanMoveOrDeleteSomethingInHardLine(
            EditorDOMPoint(&aRightBlockElement, 0u), aEditingHost);
#endif  // #ifdef DEBUG

    Element* editingHost =
        aHTMLEditor.ComputeEditingHost(HTMLEditor::LimitInBodyElement::No);
    if (MOZ_UNLIKELY(NS_WARN_IF(!editingHost))) {
      return EditActionResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
    if (&aLeftContentInBlock != editingHost) {
      SplitNodeResult splitResult =
          aHTMLEditor.SplitAncestorStyledInlineElementsAt(atPreviousContent,
                                                          nullptr, nullptr);
      if (splitResult.isErr()) {
        NS_WARNING("HTMLEditor::SplitAncestorStyledInlineElementsAt() failed");
        return EditActionResult(splitResult.unwrapErr());
      }
      // When adding caret suggestion to SplitNodeResult, here didn't change
      // selection so that just ignore it.
      splitResult.IgnoreCaretPointSuggestion();
      if (splitResult.Handled()) {
        if (nsIContent* nextContentAtSplitPoint =
                splitResult.GetNextContent()) {
          atPreviousContent.Set(nextContentAtSplitPoint);
          if (!atPreviousContent.IsSet()) {
            NS_WARNING("Next node of split point was orphaned");
            return EditActionResult(NS_ERROR_NULL_POINTER);
          }
        } else {
          atPreviousContent = splitResult.AtSplitPoint<EditorDOMPoint>();
          if (!atPreviousContent.IsSet()) {
            NS_WARNING("Split node was orphaned");
            return EditActionResult(NS_ERROR_NULL_POINTER);
          }
        }
      }
      MOZ_DIAGNOSTIC_ASSERT(atPreviousContent.IsSetAndValid());
    }

    MoveNodeResult moveNodeResult =
        aHTMLEditor.MoveOneHardLineContentsWithTransaction(
            EditorDOMPoint(&aRightBlockElement, 0u), atPreviousContent,
            aEditingHost);
    if (moveNodeResult.isErr()) {
      NS_WARNING("HTMLEditor::MoveOneHardLineContentsWithTransaction() failed");
      return EditActionResult(moveNodeResult.unwrapErr());
    }

#ifdef DEBUG
    MOZ_ASSERT(!firstLineHasContent.isErr());
    if (firstLineHasContent.inspect()) {
      NS_ASSERTION(moveNodeResult.Handled(),
                   "Failed to consider whether moving or not something");
    } else {
      NS_ASSERTION(moveNodeResult.Ignored(),
                   "Failed to consider whether moving or not something");
    }
#endif  // #ifdef DEBUG

    // We don't need to update selection here because of dontChangeMySelection
    // above.
    moveNodeResult.IgnoreCaretPointSuggestion();
    ret |= moveNodeResult;
  }

  if (!invisibleBRElementBeforeLeftBlockElement) {
    return ret;
  }

  rv = aHTMLEditor.DeleteNodeWithTransaction(
      *invisibleBRElementBeforeLeftBlockElement);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed, but ignored");
    return EditActionResult(rv);
  }
  return EditActionHandled();
}

// static
EditActionResult WhiteSpaceVisibilityKeeper::
    MergeFirstLineOfRightBlockElementIntoLeftBlockElement(
        HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
        Element& aRightBlockElement, const Maybe<nsAtom*>& aListElementTagName,
        const HTMLBRElement* aPrecedingInvisibleBRElement,
        const Element& aEditingHost) {
  MOZ_ASSERT(
      !EditorUtils::IsDescendantOf(aLeftBlockElement, aRightBlockElement));
  MOZ_ASSERT(
      !EditorUtils::IsDescendantOf(aRightBlockElement, aLeftBlockElement));

  // NOTE: This method may extend deletion range:
  // - to delete invisible white-spaces at end of aLeftBlockElement
  // - to delete invisible white-spaces at start of aRightBlockElement
  // - to delete invisible `<br>` element at end of aLeftBlockElement

  AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);

  // Adjust white-space at block boundaries
  nsresult rv = WhiteSpaceVisibilityKeeper::
      MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange(
          aHTMLEditor,
          EditorDOMRange(EditorDOMPoint::AtEndOf(aLeftBlockElement),
                         EditorDOMPoint(&aRightBlockElement, 0)));
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::"
        "MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange() failed");
    return EditActionResult(rv);
  }
  // Do br adjustment.
  RefPtr<HTMLBRElement> invisibleBRElementAtEndOfLeftBlockElement =
      WSRunScanner::GetPrecedingBRElementUnlessVisibleContentFound(
          aHTMLEditor.ComputeEditingHost(),
          EditorDOMPoint::AtEndOf(aLeftBlockElement));
  NS_ASSERTION(
      aPrecedingInvisibleBRElement == invisibleBRElementAtEndOfLeftBlockElement,
      "The preceding invisible BR element computation was different");
  EditActionResult ret(NS_OK);
  if (aListElementTagName.isSome() ||
      aLeftBlockElement.NodeInfo()->NameAtom() ==
          aRightBlockElement.NodeInfo()->NameAtom()) {
    // Nodes are same type.  merge them.
    EditorDOMPoint atFirstChildOfRightNode;
    nsresult rv = aHTMLEditor.JoinNearestEditableNodesWithTransaction(
        aLeftBlockElement, aRightBlockElement, &atFirstChildOfRightNode);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::JoinNearestEditableNodesWithTransaction()"
                         " failed, but ignored");
    if (aListElementTagName.isSome() && atFirstChildOfRightNode.IsSet()) {
      CreateElementResult convertListTypeResult =
          aHTMLEditor.ChangeListElementType(
              aRightBlockElement, MOZ_KnownLive(*aListElementTagName.ref()),
              *nsGkAtoms::li);
      if (NS_WARN_IF(convertListTypeResult.EditorDestroyed())) {
        return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          convertListTypeResult.isOk(),
          "HTMLEditor::ChangeListElementType() failed, but ignored");
    }
    ret.MarkAsHandled();
  } else {
#ifdef DEBUG
    Result<bool, nsresult> firstLineHasContent =
        aHTMLEditor.CanMoveOrDeleteSomethingInHardLine(
            EditorDOMPoint(&aRightBlockElement, 0u), aEditingHost);
#endif  // #ifdef DEBUG

    // Nodes are dissimilar types.
    MoveNodeResult moveNodeResult =
        aHTMLEditor.MoveOneHardLineContentsWithTransaction(
            EditorDOMPoint(&aRightBlockElement, 0u),
            EditorDOMPoint(&aLeftBlockElement, 0u), aEditingHost,
            HTMLEditor::MoveToEndOfContainer::Yes);
    if (moveNodeResult.isErr()) {
      NS_WARNING(
          "HTMLEditor::MoveOneHardLineContentsWithTransaction("
          "MoveToEndOfContainer::Yes) failed");
      return EditActionResult(moveNodeResult.unwrapErr());
    }

#ifdef DEBUG
    MOZ_ASSERT(!firstLineHasContent.isErr());
    if (firstLineHasContent.inspect()) {
      NS_ASSERTION(moveNodeResult.Handled(),
                   "Failed to consider whether moving or not something");
    } else {
      NS_ASSERTION(moveNodeResult.Ignored(),
                   "Failed to consider whether moving or not something");
    }
#endif  // #ifdef DEBUG

    // We don't need to update selection here because of dontChangeMySelection
    // above.
    moveNodeResult.IgnoreCaretPointSuggestion();
    ret |= moveNodeResult;
  }

  if (!invisibleBRElementAtEndOfLeftBlockElement) {
    return ret.MarkAsHandled();
  }

  rv = aHTMLEditor.DeleteNodeWithTransaction(
      *invisibleBRElementAtEndOfLeftBlockElement);
  // XXX In other top level if blocks, the result of
  //     DeleteNodeWithTransaction() is ignored.  Why does only this result
  //     is respected?
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
    return EditActionResult(rv);
  }
  return EditActionHandled();
}

// static
CreateElementResult WhiteSpaceVisibilityKeeper::InsertBRElement(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToInsert,
    const Element& aEditingHost) {
  if (MOZ_UNLIKELY(NS_WARN_IF(!aPointToInsert.IsSet()))) {
    return CreateElementResult(NS_ERROR_INVALID_ARG);
  }

  // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
  // meanwhile, the pre case is handled in HandleInsertText() in
  // HTMLEditSubActionHandler.cpp

  TextFragmentData textFragmentDataAtInsertionPoint(aPointToInsert,
                                                    &aEditingHost);
  if (MOZ_UNLIKELY(
          NS_WARN_IF(!textFragmentDataAtInsertionPoint.IsInitialized()))) {
    return CreateElementResult(NS_ERROR_FAILURE);
  }
  EditorDOMRange invisibleLeadingWhiteSpaceRangeOfNewLine =
      textFragmentDataAtInsertionPoint
          .GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(aPointToInsert);
  EditorDOMRange invisibleTrailingWhiteSpaceRangeOfCurrentLine =
      textFragmentDataAtInsertionPoint
          .GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt(aPointToInsert);
  const Maybe<const VisibleWhiteSpacesData> visibleWhiteSpaces =
      !invisibleLeadingWhiteSpaceRangeOfNewLine.IsPositioned() ||
              !invisibleTrailingWhiteSpaceRangeOfCurrentLine.IsPositioned()
          ? Some(textFragmentDataAtInsertionPoint.VisibleWhiteSpacesDataRef())
          : Nothing();
  const PointPosition pointPositionWithVisibleWhiteSpaces =
      visibleWhiteSpaces.isSome() && visibleWhiteSpaces.ref().IsInitialized()
          ? visibleWhiteSpaces.ref().ComparePoint(aPointToInsert)
          : PointPosition::NotInSameDOMTree;

  EditorDOMPoint pointToInsert(aPointToInsert);
  EditorDOMPoint atNBSPReplacableWithSP;
  if (!invisibleLeadingWhiteSpaceRangeOfNewLine.IsPositioned() &&
      (pointPositionWithVisibleWhiteSpaces == PointPosition::MiddleOfFragment ||
       pointPositionWithVisibleWhiteSpaces == PointPosition::EndOfFragment)) {
    atNBSPReplacableWithSP =
        textFragmentDataAtInsertionPoint
            .GetPreviousNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
                pointToInsert)
            .To<EditorDOMPoint>();
  }

  {
    if (invisibleTrailingWhiteSpaceRangeOfCurrentLine.IsPositioned()) {
      if (!invisibleTrailingWhiteSpaceRangeOfCurrentLine.Collapsed()) {
        // XXX Why don't we remove all of the invisible white-spaces?
        MOZ_ASSERT(invisibleTrailingWhiteSpaceRangeOfCurrentLine.StartRef() ==
                   pointToInsert);
        AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                             &pointToInsert);
        AutoTrackDOMPoint trackEndOfLineNBSP(aHTMLEditor.RangeUpdaterRef(),
                                             &atNBSPReplacableWithSP);
        AutoTrackDOMRange trackLeadingWhiteSpaceRange(
            aHTMLEditor.RangeUpdaterRef(),
            &invisibleLeadingWhiteSpaceRangeOfNewLine);
        nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            invisibleTrailingWhiteSpaceRangeOfCurrentLine.StartRef(),
            invisibleTrailingWhiteSpaceRangeOfCurrentLine.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
        if (MOZ_UNLIKELY(NS_FAILED(rv))) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return CreateElementResult(rv);
        }
        // Don't refer the following variables anymore unless tracking the
        // change.
        invisibleTrailingWhiteSpaceRangeOfCurrentLine.Clear();
      }
    }
    // If new line will start with visible white-spaces, it needs to be start
    // with an NBSP.
    else if (pointPositionWithVisibleWhiteSpaces ==
                 PointPosition::StartOfFragment ||
             pointPositionWithVisibleWhiteSpaces ==
                 PointPosition::MiddleOfFragment) {
      auto atNextCharOfInsertionPoint =
          textFragmentDataAtInsertionPoint
              .GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(
                  pointToInsert);
      if (atNextCharOfInsertionPoint.IsSet() &&
          !atNextCharOfInsertionPoint.IsEndOfContainer() &&
          atNextCharOfInsertionPoint.IsCharCollapsibleASCIISpace()) {
        const EditorDOMPointInText atPreviousCharOfNextCharOfInsertionPoint =
            textFragmentDataAtInsertionPoint.GetPreviousEditableCharPoint(
                atNextCharOfInsertionPoint);
        if (!atPreviousCharOfNextCharOfInsertionPoint.IsSet() ||
            atPreviousCharOfNextCharOfInsertionPoint.IsEndOfContainer() ||
            !atPreviousCharOfNextCharOfInsertionPoint.IsCharASCIISpace()) {
          AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                               &pointToInsert);
          AutoTrackDOMPoint trackEndOfLineNBSP(aHTMLEditor.RangeUpdaterRef(),
                                               &atNBSPReplacableWithSP);
          AutoTrackDOMRange trackLeadingWhiteSpaceRange(
              aHTMLEditor.RangeUpdaterRef(),
              &invisibleLeadingWhiteSpaceRangeOfNewLine);
          // We are at start of non-nbsps.  Convert to a single nbsp.
          const EditorDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
              textFragmentDataAtInsertionPoint
                  .GetEndOfCollapsibleASCIIWhiteSpaces(
                      atNextCharOfInsertionPoint, nsIEditor::eNone);
          nsresult rv =
              WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
                  aHTMLEditor,
                  EditorDOMRangeInTexts(atNextCharOfInsertionPoint,
                                        endOfCollapsibleASCIIWhiteSpaces),
                  nsDependentSubstring(&HTMLEditUtils::kNBSP, 1));
          if (MOZ_UNLIKELY(NS_FAILED(rv))) {
            NS_WARNING(
                "WhiteSpaceVisibilityKeeper::"
                "ReplaceTextAndRemoveEmptyTextNodes() failed");
            return CreateElementResult(rv);
          }
          // Don't refer the following variables anymore unless tracking the
          // change.
          invisibleTrailingWhiteSpaceRangeOfCurrentLine.Clear();
        }
      }
    }

    if (invisibleLeadingWhiteSpaceRangeOfNewLine.IsPositioned()) {
      if (!invisibleLeadingWhiteSpaceRangeOfNewLine.Collapsed()) {
        AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                             &pointToInsert);
        // XXX Why don't we remove all of the invisible white-spaces?
        MOZ_ASSERT(invisibleLeadingWhiteSpaceRangeOfNewLine.EndRef() ==
                   pointToInsert);
        nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            invisibleLeadingWhiteSpaceRangeOfNewLine.StartRef(),
            invisibleLeadingWhiteSpaceRangeOfNewLine.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
        if (MOZ_UNLIKELY(NS_FAILED(rv))) {
          NS_WARNING(
              "WhiteSpaceVisibilityKeeper::"
              "DeleteTextAndTextNodesWithTransaction() failed");
          return CreateElementResult(rv);
        }
        // Don't refer the following variables anymore unless tracking the
        // change.
        atNBSPReplacableWithSP.Clear();
        invisibleLeadingWhiteSpaceRangeOfNewLine.Clear();
        invisibleTrailingWhiteSpaceRangeOfCurrentLine.Clear();
      }
    }
    // If the `<br>` element is put immediately after an NBSP, it should be
    // replaced with an ASCII white-space.
    else if (atNBSPReplacableWithSP.IsInTextNode()) {
      const EditorDOMPointInText atNBSPReplacedWithASCIIWhiteSpace =
          atNBSPReplacableWithSP.AsInText();
      if (!atNBSPReplacedWithASCIIWhiteSpace.IsEndOfContainer() &&
          atNBSPReplacedWithASCIIWhiteSpace.IsCharNBSP()) {
        AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                             &pointToInsert);
        AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
        nsresult rv = aHTMLEditor.ReplaceTextWithTransaction(
            MOZ_KnownLive(*atNBSPReplacedWithASCIIWhiteSpace.ContainerAsText()),
            atNBSPReplacedWithASCIIWhiteSpace.Offset(), 1, u" "_ns);
        if (MOZ_UNLIKELY(NS_FAILED(rv))) {
          NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed failed");
          return CreateElementResult(rv);
        }
        // Don't refer the following variables anymore unless tracking the
        // change.
        atNBSPReplacableWithSP.Clear();
        invisibleLeadingWhiteSpaceRangeOfNewLine.Clear();
        invisibleTrailingWhiteSpaceRangeOfCurrentLine.Clear();
      }
    }
  }

  CreateElementResult insertBRElementResult = aHTMLEditor.InsertBRElement(
      HTMLEditor::WithTransaction::Yes, pointToInsert);
  NS_WARNING_ASSERTION(
      insertBRElementResult.isOk(),
      "HTMLEditor::InsertBRElement(WithTransaction::Yes, eNone) failed");
  return insertBRElementResult;
}

// static
Result<EditorDOMPoint, nsresult> WhiteSpaceVisibilityKeeper::ReplaceText(
    HTMLEditor& aHTMLEditor, const nsAString& aStringToInsert,
    const EditorDOMRange& aRangeToBeReplaced) {
  // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
  // meanwhile, the pre case is handled in HandleInsertText() in
  // HTMLEditSubActionHandler.cpp

  // MOOSE: for now, just getting the ws logic straight.  This implementation
  // is very slow.  Will need to replace edit rules impl with a more efficient
  // text sink here that does the minimal amount of searching/replacing/copying

  if (aStringToInsert.IsEmpty()) {
    MOZ_ASSERT(aRangeToBeReplaced.Collapsed());
    return EditorDOMPoint(aRangeToBeReplaced.StartRef());
  }

  RefPtr<Element> editingHost = aHTMLEditor.ComputeEditingHost();
  TextFragmentData textFragmentDataAtStart(aRangeToBeReplaced.StartRef(),
                                           editingHost);
  if (MOZ_UNLIKELY(NS_WARN_IF(!textFragmentDataAtStart.IsInitialized()))) {
    return Err(NS_ERROR_FAILURE);
  }
  const bool isInsertionPointEqualsOrIsBeforeStartOfText =
      aRangeToBeReplaced.StartRef().EqualsOrIsBefore(
          textFragmentDataAtStart.StartRef());
  TextFragmentData textFragmentDataAtEnd =
      aRangeToBeReplaced.Collapsed()
          ? textFragmentDataAtStart
          : TextFragmentData(aRangeToBeReplaced.EndRef(), editingHost);
  if (MOZ_UNLIKELY(NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized()))) {
    return Err(NS_ERROR_FAILURE);
  }
  const bool isInsertionPointEqualsOrAfterEndOfText =
      textFragmentDataAtEnd.EndRef().EqualsOrIsBefore(
          aRangeToBeReplaced.EndRef());

  EditorDOMRange invisibleLeadingWhiteSpaceRangeAtStart =
      textFragmentDataAtStart
          .GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(
              aRangeToBeReplaced.StartRef());
  const bool isInvisibleLeadingWhiteSpaceRangeAtStartPositioned =
      invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned();
  EditorDOMRange invisibleTrailingWhiteSpaceRangeAtEnd =
      textFragmentDataAtEnd.GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt(
          aRangeToBeReplaced.EndRef());
  const bool isInvisibleTrailingWhiteSpaceRangeAtEndPositioned =
      invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned();
  const Maybe<const VisibleWhiteSpacesData> visibleWhiteSpacesAtStart =
      !isInvisibleLeadingWhiteSpaceRangeAtStartPositioned
          ? Some(textFragmentDataAtStart.VisibleWhiteSpacesDataRef())
          : Nothing();
  const PointPosition pointPositionWithVisibleWhiteSpacesAtStart =
      visibleWhiteSpacesAtStart.isSome() &&
              visibleWhiteSpacesAtStart.ref().IsInitialized()
          ? visibleWhiteSpacesAtStart.ref().ComparePoint(
                aRangeToBeReplaced.StartRef())
          : PointPosition::NotInSameDOMTree;
  const Maybe<const VisibleWhiteSpacesData> visibleWhiteSpacesAtEnd =
      !isInvisibleTrailingWhiteSpaceRangeAtEndPositioned
          ? Some(textFragmentDataAtEnd.VisibleWhiteSpacesDataRef())
          : Nothing();
  const PointPosition pointPositionWithVisibleWhiteSpacesAtEnd =
      visibleWhiteSpacesAtEnd.isSome() &&
              visibleWhiteSpacesAtEnd.ref().IsInitialized()
          ? visibleWhiteSpacesAtEnd.ref().ComparePoint(
                aRangeToBeReplaced.EndRef())
          : PointPosition::NotInSameDOMTree;

  EditorDOMPoint pointToInsert(aRangeToBeReplaced.StartRef());
  EditorDOMPoint atNBSPReplaceableWithSP;
  if (!invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned() &&
      (pointPositionWithVisibleWhiteSpacesAtStart ==
           PointPosition::MiddleOfFragment ||
       pointPositionWithVisibleWhiteSpacesAtStart ==
           PointPosition::EndOfFragment)) {
    atNBSPReplaceableWithSP =
        textFragmentDataAtStart
            .GetPreviousNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
                pointToInsert)
            .To<EditorDOMPoint>();
  }
  nsAutoString theString(aStringToInsert);
  {
    if (invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned()) {
      if (!invisibleTrailingWhiteSpaceRangeAtEnd.Collapsed()) {
        AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                             &pointToInsert);
        AutoTrackDOMPoint trackPrecedingNBSP(aHTMLEditor.RangeUpdaterRef(),
                                             &atNBSPReplaceableWithSP);
        AutoTrackDOMRange trackInvisibleLeadingWhiteSpaceRange(
            aHTMLEditor.RangeUpdaterRef(),
            &invisibleLeadingWhiteSpaceRangeAtStart);
        AutoTrackDOMRange trackInvisibleTrailingWhiteSpaceRange(
            aHTMLEditor.RangeUpdaterRef(),
            &invisibleTrailingWhiteSpaceRangeAtEnd);
        // XXX Why don't we remove all of the invisible white-spaces?
        MOZ_ASSERT(invisibleTrailingWhiteSpaceRangeAtEnd.StartRef() ==
                   pointToInsert);
        nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            invisibleTrailingWhiteSpaceRangeAtEnd.StartRef(),
            invisibleTrailingWhiteSpaceRangeAtEnd.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
        if (MOZ_UNLIKELY(NS_FAILED(rv))) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return Err(rv);
        }
      }
    }
    // Replace an NBSP at inclusive next character of replacing range to an
    // ASCII white-space if inserting into a visible white-space sequence.
    // XXX With modifying the inserting string later, this creates a line break
    //     opportunity after the inserting string, but this causes
    //     inconsistent result with inserting order.  E.g., type white-space
    //     n times with various order.
    else if (pointPositionWithVisibleWhiteSpacesAtEnd ==
                 PointPosition::StartOfFragment ||
             pointPositionWithVisibleWhiteSpacesAtEnd ==
                 PointPosition::MiddleOfFragment) {
      EditorDOMPointInText atNBSPReplacedWithASCIIWhiteSpace =
          textFragmentDataAtEnd
              .GetInclusiveNextNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
                  aRangeToBeReplaced.EndRef());
      if (atNBSPReplacedWithASCIIWhiteSpace.IsSet()) {
        AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                             &pointToInsert);
        AutoTrackDOMPoint trackPrecedingNBSP(aHTMLEditor.RangeUpdaterRef(),
                                             &atNBSPReplaceableWithSP);
        AutoTrackDOMRange trackInvisibleLeadingWhiteSpaceRange(
            aHTMLEditor.RangeUpdaterRef(),
            &invisibleLeadingWhiteSpaceRangeAtStart);
        AutoTrackDOMRange trackInvisibleTrailingWhiteSpaceRange(
            aHTMLEditor.RangeUpdaterRef(),
            &invisibleTrailingWhiteSpaceRangeAtEnd);
        AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
        nsresult rv = aHTMLEditor.ReplaceTextWithTransaction(
            MOZ_KnownLive(*atNBSPReplacedWithASCIIWhiteSpace.ContainerAsText()),
            atNBSPReplacedWithASCIIWhiteSpace.Offset(), 1, u" "_ns);
        if (MOZ_UNLIKELY(NS_FAILED(rv))) {
          NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
          return Err(rv);
        }
      }
    }

    if (invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned()) {
      if (!invisibleLeadingWhiteSpaceRangeAtStart.Collapsed()) {
        AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                             &pointToInsert);
        AutoTrackDOMRange trackInvisibleTrailingWhiteSpaceRange(
            aHTMLEditor.RangeUpdaterRef(),
            &invisibleTrailingWhiteSpaceRangeAtEnd);
        // XXX Why don't we remove all of the invisible white-spaces?
        MOZ_ASSERT(invisibleLeadingWhiteSpaceRangeAtStart.EndRef() ==
                   pointToInsert);
        nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            invisibleLeadingWhiteSpaceRangeAtStart.StartRef(),
            invisibleLeadingWhiteSpaceRangeAtStart.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
        if (MOZ_UNLIKELY(NS_FAILED(rv))) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return Err(rv);
        }
        // Don't refer the following variables anymore unless tracking the
        // change.
        atNBSPReplaceableWithSP.Clear();
        invisibleLeadingWhiteSpaceRangeAtStart.Clear();
      }
    }
    // Replace an NBSP at previous character of insertion point to an ASCII
    // white-space if inserting into a visible white-space sequence.
    // XXX With modifying the inserting string later, this creates a line break
    //     opportunity before the inserting string, but this causes
    //     inconsistent result with inserting order.  E.g., type white-space
    //     n times with various order.
    else if (atNBSPReplaceableWithSP.IsInTextNode()) {
      EditorDOMPointInText atNBSPReplacedWithASCIIWhiteSpace =
          atNBSPReplaceableWithSP.AsInText();
      if (!atNBSPReplacedWithASCIIWhiteSpace.IsEndOfContainer() &&
          atNBSPReplacedWithASCIIWhiteSpace.IsCharNBSP()) {
        AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                             &pointToInsert);
        AutoTrackDOMRange trackInvisibleTrailingWhiteSpaceRange(
            aHTMLEditor.RangeUpdaterRef(),
            &invisibleTrailingWhiteSpaceRangeAtEnd);
        AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
        nsresult rv = aHTMLEditor.ReplaceTextWithTransaction(
            MOZ_KnownLive(*atNBSPReplacedWithASCIIWhiteSpace.ContainerAsText()),
            atNBSPReplacedWithASCIIWhiteSpace.Offset(), 1, u" "_ns);
        if (MOZ_UNLIKELY(NS_FAILED(rv))) {
          NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed failed");
          return Err(rv);
        }
        // Don't refer the following variables anymore unless tracking the
        // change.
        atNBSPReplaceableWithSP.Clear();
        invisibleLeadingWhiteSpaceRangeAtStart.Clear();
      }
    }
  }

  // If white-space and/or linefeed characters are collapsible, and inserting
  // string starts and/or ends with a collapsible characters, we need to
  // replace them with NBSP for making sure the collapsible characters visible.
  // FYI: There is no case only linefeeds are collapsible.  So, we need to
  //      do the things only when white-spaces are collapsible.
  MOZ_DIAGNOSTIC_ASSERT(!theString.IsEmpty());
  if (NS_WARN_IF(!pointToInsert.IsInContentNode()) ||
      !EditorUtils::IsWhiteSpacePreformatted(
          *pointToInsert.ContainerAsContent())) {
    const bool isNewLineCollapsible = !pointToInsert.IsInContentNode() ||
                                      !EditorUtils::IsNewLinePreformatted(
                                          *pointToInsert.ContainerAsContent());
    auto isCollapsibleChar = [&isNewLineCollapsible](char16_t aChar) -> bool {
      return nsCRT::IsAsciiSpace(aChar) &&
             (isNewLineCollapsible || aChar != HTMLEditUtils::kNewLine);
    };
    if (isCollapsibleChar(theString[0])) {
      // If inserting string will follow some invisible leading white-spaces,
      // the string needs to start with an NBSP.
      if (isInvisibleLeadingWhiteSpaceRangeAtStartPositioned) {
        theString.SetCharAt(HTMLEditUtils::kNBSP, 0);
      }
      // If inserting around visible white-spaces, check whether the previous
      // character of insertion point is an NBSP or an ASCII white-space.
      else if (pointPositionWithVisibleWhiteSpacesAtStart ==
                   PointPosition::MiddleOfFragment ||
               pointPositionWithVisibleWhiteSpacesAtStart ==
                   PointPosition::EndOfFragment) {
        const auto atPreviousChar =
            textFragmentDataAtStart
                .GetPreviousEditableCharPoint<EditorRawDOMPointInText>(
                    pointToInsert);
        if (atPreviousChar.IsSet() && !atPreviousChar.IsEndOfContainer() &&
            atPreviousChar.IsCharASCIISpace()) {
          theString.SetCharAt(HTMLEditUtils::kNBSP, 0);
        }
      }
      // If the insertion point is (was) before the start of text and it's
      // immediately after a hard line break, the first ASCII white-space should
      // be replaced with an NBSP for making it visible.
      else if (textFragmentDataAtStart.StartsFromHardLineBreak() &&
               isInsertionPointEqualsOrIsBeforeStartOfText) {
        theString.SetCharAt(HTMLEditUtils::kNBSP, 0);
      }
    }

    // Then the tail.  Note that it may be the first character.
    const uint32_t lastCharIndex = theString.Length() - 1;
    if (isCollapsibleChar(theString[lastCharIndex])) {
      // If inserting string will be followed by some invisible trailing
      // white-spaces, the string needs to end with an NBSP.
      if (isInvisibleTrailingWhiteSpaceRangeAtEndPositioned) {
        theString.SetCharAt(HTMLEditUtils::kNBSP, lastCharIndex);
      }
      // If inserting around visible white-spaces, check whether the inclusive
      // next character of end of replaced range is an NBSP or an ASCII
      // white-space.
      if (pointPositionWithVisibleWhiteSpacesAtEnd ==
              PointPosition::StartOfFragment ||
          pointPositionWithVisibleWhiteSpacesAtEnd ==
              PointPosition::MiddleOfFragment) {
        const auto atNextChar =
            textFragmentDataAtEnd
                .GetInclusiveNextEditableCharPoint<EditorRawDOMPointInText>(
                    pointToInsert);
        if (atNextChar.IsSet() && !atNextChar.IsEndOfContainer() &&
            atNextChar.IsCharASCIISpace()) {
          theString.SetCharAt(HTMLEditUtils::kNBSP, lastCharIndex);
        }
      }
      // If the end of replacing range is (was) after the end of text and it's
      // immediately before block boundary, the last ASCII white-space should
      // be replaced with an NBSP for making it visible.
      else if (textFragmentDataAtEnd.EndsByBlockBoundary() &&
               isInsertionPointEqualsOrAfterEndOfText) {
        theString.SetCharAt(HTMLEditUtils::kNBSP, lastCharIndex);
      }
    }

    // Next, scan string for adjacent ws and convert to nbsp/space combos
    // MOOSE: don't need to convert tabs here since that is done by
    // WillInsertText() before we are called.  Eventually, all that logic will
    // be pushed down into here and made more efficient.
    enum class PreviousChar {
      NonCollapsibleChar,
      CollapsibleChar,
      PreformattedNewLine,
    };
    PreviousChar previousChar = PreviousChar::NonCollapsibleChar;
    for (uint32_t i = 0; i <= lastCharIndex; i++) {
      if (isCollapsibleChar(theString[i])) {
        // If current char is collapsible and 2nd or latter character of
        // collapsible characters, we need to make the previous character an
        // NBSP for avoiding current character to be collapsed to it.
        if (previousChar == PreviousChar::CollapsibleChar) {
          MOZ_ASSERT(i > 0);
          theString.SetCharAt(HTMLEditUtils::kNBSP, i - 1);
          // Keep previousChar as PreviousChar::CollapsibleChar.
          continue;
        }

        // If current character is a collapsbile white-space and the previous
        // character is a preformatted linefeed, we need to replace the current
        // character with an NBSP for avoiding collapsed with the previous
        // linefeed.
        if (previousChar == PreviousChar::PreformattedNewLine) {
          MOZ_ASSERT(i > 0);
          theString.SetCharAt(HTMLEditUtils::kNBSP, i);
          previousChar = PreviousChar::NonCollapsibleChar;
          continue;
        }

        previousChar = PreviousChar::CollapsibleChar;
        continue;
      }

      if (theString[i] != HTMLEditUtils::kNewLine) {
        previousChar = PreviousChar::NonCollapsibleChar;
        continue;
      }

      // If current character is a preformatted linefeed and the previous
      // character is collapbile white-space, the previous character will be
      // collapsed into current linefeed.  Therefore, we need to replace the
      // previous character with an NBSP.
      MOZ_ASSERT(!isNewLineCollapsible);
      if (previousChar == PreviousChar::CollapsibleChar) {
        MOZ_ASSERT(i > 0);
        theString.SetCharAt(HTMLEditUtils::kNBSP, i - 1);
      }
      previousChar = PreviousChar::PreformattedNewLine;
    }
  }

  // XXX If the point is not editable, InsertTextWithTransaction() returns
  //     error, but we keep handling it.  But I think that it wastes the
  //     runtime cost.  So, perhaps, we should return error code which couldn't
  //     modify it and make each caller of this method decide whether it should
  //     keep or stop handling the edit action.
  if (MOZ_UNLIKELY(!aHTMLEditor.GetDocument())) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::ReplaceText() lost proper document");
    return Err(NS_ERROR_UNEXPECTED);
  }
  OwningNonNull<Document> document = *aHTMLEditor.GetDocument();
  Result<EditorDOMPoint, nsresult> insertTextResult =
      aHTMLEditor.InsertTextWithTransaction(document, theString, pointToInsert);
  if (MOZ_UNLIKELY(insertTextResult.isErr() && insertTextResult.inspectErr() ==
                                                   NS_ERROR_EDITOR_DESTROYED)) {
    NS_WARNING(
        "HTMLEditor::InsertTextWithTransaction() caused destroying the editor");
    return Err(NS_ERROR_EDITOR_DESTROYED);
  }
  if (insertTextResult.isOk()) {
    return insertTextResult.unwrap();
  }

  NS_WARNING("HTMLEditor::InsertTextWithTransaction() failed, but ignored");

  // XXX Temporarily, set new insertion point to the original point.
  return pointToInsert;
}

// static
nsresult WhiteSpaceVisibilityKeeper::DeletePreviousWhiteSpace(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint) {
  Element* editingHost = aHTMLEditor.ComputeEditingHost();
  TextFragmentData textFragmentDataAtDeletion(aPoint, editingHost);
  if (NS_WARN_IF(!textFragmentDataAtDeletion.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }
  const EditorDOMPointInText atPreviousCharOfStart =
      textFragmentDataAtDeletion.GetPreviousEditableCharPoint(aPoint);
  if (!atPreviousCharOfStart.IsSet() ||
      atPreviousCharOfStart.IsEndOfContainer()) {
    return NS_OK;
  }

  // If the char is a collapsible white-space or a non-collapsible new line
  // but it can collapse adjacent white-spaces, we need to extend the range
  // to delete all invisible white-spaces.
  if (atPreviousCharOfStart.IsCharCollapsibleASCIISpace() ||
      atPreviousCharOfStart
          .IsCharPreformattedNewLineCollapsedWithWhiteSpaces()) {
    auto startToDelete =
        textFragmentDataAtDeletion
            .GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPoint>(
                atPreviousCharOfStart, nsIEditor::ePrevious);
    auto endToDelete = textFragmentDataAtDeletion
                           .GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPoint>(
                               atPreviousCharOfStart, nsIEditor::ePrevious);
    nsresult rv =
        WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
            aHTMLEditor, &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
          "failed");
      return rv;
    }

    rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        startToDelete, endToDelete,
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  if (atPreviousCharOfStart.IsCharCollapsibleNBSP()) {
    auto startToDelete = atPreviousCharOfStart.To<EditorDOMPoint>();
    auto endToDelete = startToDelete.NextPoint<EditorDOMPoint>();
    nsresult rv =
        WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
            aHTMLEditor, &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
          "failed");
      return rv;
    }

    rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        startToDelete, endToDelete,
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
      atPreviousCharOfStart, atPreviousCharOfStart.NextPoint(),
      HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
  return rv;
}

// static
nsresult WhiteSpaceVisibilityKeeper::DeleteInclusiveNextWhiteSpace(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint) {
  Element* editingHost = aHTMLEditor.ComputeEditingHost();
  TextFragmentData textFragmentDataAtDeletion(aPoint, editingHost);
  if (NS_WARN_IF(!textFragmentDataAtDeletion.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }
  auto atNextCharOfStart =
      textFragmentDataAtDeletion
          .GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(aPoint);
  if (!atNextCharOfStart.IsSet() || atNextCharOfStart.IsEndOfContainer()) {
    return NS_OK;
  }

  // If the char is a collapsible white-space or a non-collapsible new line
  // but it can collapse adjacent white-spaces, we need to extend the range
  // to delete all invisible white-spaces.
  if (atNextCharOfStart.IsCharCollapsibleASCIISpace() ||
      atNextCharOfStart.IsCharPreformattedNewLineCollapsedWithWhiteSpaces()) {
    auto startToDelete =
        textFragmentDataAtDeletion
            .GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPoint>(
                atNextCharOfStart, nsIEditor::eNext);
    auto endToDelete = textFragmentDataAtDeletion
                           .GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPoint>(
                               atNextCharOfStart, nsIEditor::eNext);
    nsresult rv =
        WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
            aHTMLEditor, &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
          "failed");
      return rv;
    }

    rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        startToDelete, endToDelete,
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  if (atNextCharOfStart.IsCharCollapsibleNBSP()) {
    auto startToDelete = atNextCharOfStart.To<EditorDOMPoint>();
    auto endToDelete = startToDelete.NextPoint<EditorDOMPoint>();
    nsresult rv =
        WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
            aHTMLEditor, &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
          "failed");
      return rv;
    }

    rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        startToDelete, endToDelete,
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
      atNextCharOfStart, atNextCharOfStart.NextPoint(),
      HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
  return rv;
}

// static
nsresult WhiteSpaceVisibilityKeeper::DeleteContentNodeAndJoinTextNodesAroundIt(
    HTMLEditor& aHTMLEditor, nsIContent& aContentToDelete,
    const EditorDOMPoint& aCaretPoint) {
  EditorDOMPoint atContent(&aContentToDelete);
  if (!atContent.IsSet()) {
    NS_WARNING("Deleting content node was an orphan node");
    return NS_ERROR_FAILURE;
  }
  if (!HTMLEditUtils::IsRemovableNode(aContentToDelete)) {
    NS_WARNING("Deleting content node wasn't removable");
    return NS_ERROR_FAILURE;
  }
  nsresult rv = WhiteSpaceVisibilityKeeper::
      MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange(
          aHTMLEditor, EditorDOMRange(atContent, atContent.NextPoint()));
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::"
        "MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange() failed");
    return rv;
  }

  nsCOMPtr<nsIContent> previousEditableSibling =
      HTMLEditUtils::GetPreviousSibling(
          aContentToDelete, {WalkTreeOption::IgnoreNonEditableNode});
  // Delete the node, and join like nodes if appropriate
  rv = aHTMLEditor.DeleteNodeWithTransaction(aContentToDelete);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
    return rv;
  }
  // Are they both text nodes?  If so, join them!
  // XXX This may cause odd behavior if there is non-editable nodes
  //     around the atomic content.
  if (!aCaretPoint.IsInTextNode() || !previousEditableSibling ||
      !previousEditableSibling->IsText()) {
    return NS_OK;
  }

  nsIContent* nextEditableSibling = HTMLEditUtils::GetNextSibling(
      *previousEditableSibling, {WalkTreeOption::IgnoreNonEditableNode});
  if (aCaretPoint.GetContainer() != nextEditableSibling) {
    return NS_OK;
  }
  EditorDOMPoint atFirstChildOfRightNode;
  rv = aHTMLEditor.JoinNearestEditableNodesWithTransaction(
      *previousEditableSibling,
      MOZ_KnownLive(*aCaretPoint.GetContainerAsText()),
      &atFirstChildOfRightNode);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::JoinNearestEditableNodesWithTransaction() failed");
    return rv;
  }
  if (!atFirstChildOfRightNode.IsSet()) {
    NS_WARNING(
        "HTMLEditor::JoinNearestEditableNodesWithTransaction() didn't return "
        "right node position");
    return NS_ERROR_FAILURE;
  }
  // Fix up selection
  rv = aHTMLEditor.CollapseSelectionTo(atFirstChildOfRightNode);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::CollapseSelectionTo() failed");
  return rv;
}

template <typename PT, typename CT>
WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSet());

  if (!TextFragmentDataAtStartRef().IsInitialized()) {
    return WSScanResult(nullptr, WSType::UnexpectedError);
  }

  // If the range has visible text and start of the visible text is before
  // aPoint, return previous character in the text.
  const VisibleWhiteSpacesData& visibleWhiteSpaces =
      TextFragmentDataAtStartRef().VisibleWhiteSpacesDataRef();
  if (visibleWhiteSpaces.IsInitialized() &&
      visibleWhiteSpaces.StartRef().IsBefore(aPoint)) {
    // If the visible things are not editable, we shouldn't scan "editable"
    // things now.  Whether keep scanning editable things or not should be
    // considered by the caller.
    if (aPoint.GetChild() && !aPoint.GetChild()->IsEditable()) {
      return WSScanResult(aPoint.GetChild(), WSType::SpecialContent);
    }
    const auto atPreviousChar =
        GetPreviousEditableCharPoint<EditorRawDOMPointInText>(aPoint);
    // When it's a non-empty text node, return it.
    if (atPreviousChar.IsSet() && !atPreviousChar.IsContainerEmpty()) {
      MOZ_ASSERT(!atPreviousChar.IsEndOfContainer());
      return WSScanResult(atPreviousChar.template NextPoint<EditorDOMPoint>(),
                          atPreviousChar.IsCharCollapsibleASCIISpaceOrNBSP()
                              ? WSType::CollapsibleWhiteSpaces
                              : WSType::NonCollapsibleCharacters);
    }
  }

  // Otherwise, return the start of the range.
  if (TextFragmentDataAtStartRef().GetStartReasonContent() !=
      TextFragmentDataAtStartRef().StartRef().GetContainer()) {
    // In this case, TextFragmentDataAtStartRef().StartRef().Offset() is not
    // meaningful.
    return WSScanResult(TextFragmentDataAtStartRef().GetStartReasonContent(),
                        TextFragmentDataAtStartRef().StartRawReason());
  }
  return WSScanResult(TextFragmentDataAtStartRef().StartRef(),
                      TextFragmentDataAtStartRef().StartRawReason());
}

template <typename PT, typename CT>
WSScanResult WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSet());

  if (!TextFragmentDataAtStartRef().IsInitialized()) {
    return WSScanResult(nullptr, WSType::UnexpectedError);
  }

  // If the range has visible text and aPoint equals or is before the end of the
  // visible text, return inclusive next character in the text.
  const VisibleWhiteSpacesData& visibleWhiteSpaces =
      TextFragmentDataAtStartRef().VisibleWhiteSpacesDataRef();
  if (visibleWhiteSpaces.IsInitialized() &&
      aPoint.EqualsOrIsBefore(visibleWhiteSpaces.EndRef())) {
    // If the visible things are not editable, we shouldn't scan "editable"
    // things now.  Whether keep scanning editable things or not should be
    // considered by the caller.
    if (aPoint.GetChild() && !aPoint.GetChild()->IsEditable()) {
      return WSScanResult(aPoint.GetChild(), WSType::SpecialContent);
    }
    const auto atNextChar =
        GetInclusiveNextEditableCharPoint<EditorDOMPoint>(aPoint);
    // When it's a non-empty text node, return it.
    if (atNextChar.IsSet() && !atNextChar.IsContainerEmpty()) {
      return WSScanResult(atNextChar,
                          !atNextChar.IsEndOfContainer() &&
                                  atNextChar.IsCharCollapsibleASCIISpaceOrNBSP()
                              ? WSType::CollapsibleWhiteSpaces
                              : WSType::NonCollapsibleCharacters);
    }
  }

  // Otherwise, return the end of the range.
  if (TextFragmentDataAtStartRef().GetEndReasonContent() !=
      TextFragmentDataAtStartRef().EndRef().GetContainer()) {
    // In this case, TextFragmentDataAtStartRef().EndRef().Offset() is not
    // meaningful.
    return WSScanResult(TextFragmentDataAtStartRef().GetEndReasonContent(),
                        TextFragmentDataAtStartRef().EndRawReason());
  }
  return WSScanResult(TextFragmentDataAtStartRef().EndRef(),
                      TextFragmentDataAtStartRef().EndRawReason());
}

template <typename EditorDOMPointType>
WSRunScanner::TextFragmentData::TextFragmentData(
    const EditorDOMPointType& aPoint, const Element* aEditingHost)
    : mEditingHost(aEditingHost) {
  if (!aPoint.IsSetAndValid()) {
    NS_WARNING("aPoint was invalid");
    return;
  }
  if (!aPoint.IsInContentNode()) {
    NS_WARNING("aPoint was in Document or DocumentFragment");
    // I.e., we're try to modify outside of root element.  We don't need to
    // support such odd case because web apps cannot append text nodes as
    // direct child of Document node.
    return;
  }

  mScanStartPoint = aPoint.template To<EditorDOMPoint>();
  NS_ASSERTION(EditorUtils::IsEditableContent(
                   *mScanStartPoint.ContainerAsContent(), EditorType::HTML),
               "Given content is not editable");
  NS_ASSERTION(
      mScanStartPoint.ContainerAsContent()->GetAsElementOrParentElement(),
      "Given content is not an element and an orphan node");
  if (NS_WARN_IF(!EditorUtils::IsEditableContent(
          *mScanStartPoint.ContainerAsContent(), EditorType::HTML))) {
    return;
  }
  const Element* editableBlockElementOrInlineEditingHost =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *mScanStartPoint.ContainerAsContent(),
          HTMLEditUtils::ClosestEditableBlockElementOrInlineEditingHost);
  if (!editableBlockElementOrInlineEditingHost) {
    NS_WARNING(
        "HTMLEditUtils::GetInclusiveAncestorElement(HTMLEditUtils::"
        "ClosestEditableBlockElementOrInlineEditingHost) couldn't find "
        "editing host");
    return;
  }

  mStart = BoundaryData::ScanCollapsibleWhiteSpaceStartFrom(
      mScanStartPoint, *editableBlockElementOrInlineEditingHost, mEditingHost,
      &mNBSPData);
  MOZ_ASSERT_IF(mStart.IsNonCollapsibleCharacters(),
                !mStart.PointRef().IsPreviousCharPreformattedNewLine());
  MOZ_ASSERT_IF(mStart.IsPreformattedLineBreak(),
                mStart.PointRef().IsPreviousCharPreformattedNewLine());
  mEnd = BoundaryData::ScanCollapsibleWhiteSpaceEndFrom(
      mScanStartPoint, *editableBlockElementOrInlineEditingHost, mEditingHost,
      &mNBSPData);
  MOZ_ASSERT_IF(mEnd.IsNonCollapsibleCharacters(),
                !mEnd.PointRef().IsCharPreformattedNewLine());
  MOZ_ASSERT_IF(mEnd.IsPreformattedLineBreak(),
                mEnd.PointRef().IsCharPreformattedNewLine());
}

// static
template <typename EditorDOMPointType>
Maybe<WSRunScanner::TextFragmentData::BoundaryData> WSRunScanner::
    TextFragmentData::BoundaryData::ScanCollapsibleWhiteSpaceStartInTextNode(
        const EditorDOMPointType& aPoint, NoBreakingSpaceData* aNBSPData) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_DIAGNOSTIC_ASSERT(aPoint.IsInTextNode());

  const bool isWhiteSpaceCollapsible =
      !EditorUtils::IsWhiteSpacePreformatted(*aPoint.ContainerAsText());
  const bool isNewLineCollapsible =
      !EditorUtils::IsNewLinePreformatted(*aPoint.ContainerAsText());
  const nsTextFragment& textFragment = aPoint.ContainerAsText()->TextFragment();
  for (uint32_t i = std::min(aPoint.Offset(), textFragment.GetLength()); i;
       i--) {
    WSType wsTypeOfNonCollapsibleChar;
    switch (textFragment.CharAt(i - 1)) {
      case HTMLEditUtils::kSpace:
      case HTMLEditUtils::kCarriageReturn:
      case HTMLEditUtils::kTab:
        if (isWhiteSpaceCollapsible) {
          continue;  // collapsible white-space or invisible white-space.
        }
        // preformatted white-space.
        wsTypeOfNonCollapsibleChar = WSType::NonCollapsibleCharacters;
        break;
      case HTMLEditUtils::kNewLine:
        if (isNewLineCollapsible) {
          continue;  // collapsible linefeed.
        }
        // preformatted linefeed.
        wsTypeOfNonCollapsibleChar = WSType::PreformattedLineBreak;
        break;
      case HTMLEditUtils::kNBSP:
        if (isWhiteSpaceCollapsible) {
          if (aNBSPData) {
            aNBSPData->NotifyNBSP(
                EditorDOMPointInText(aPoint.ContainerAsText(), i - 1),
                NoBreakingSpaceData::Scanning::Backward);
          }
          continue;
        }
        // NBSP is never converted from collapsible white-space/linefeed.
        wsTypeOfNonCollapsibleChar = WSType::NonCollapsibleCharacters;
        break;
      default:
        MOZ_ASSERT(!nsCRT::IsAsciiSpace(textFragment.CharAt(i - 1)));
        wsTypeOfNonCollapsibleChar = WSType::NonCollapsibleCharacters;
        break;
    }

    return Some(BoundaryData(EditorDOMPoint(aPoint.ContainerAsText(), i),
                             *aPoint.ContainerAsText(),
                             wsTypeOfNonCollapsibleChar));
  }

  return Nothing();
}

// static
template <typename EditorDOMPointType>
WSRunScanner::TextFragmentData::BoundaryData WSRunScanner::TextFragmentData::
    BoundaryData::ScanCollapsibleWhiteSpaceStartFrom(
        const EditorDOMPointType& aPoint,
        const Element& aEditableBlockParentOrTopmostEditableInlineContent,
        const Element* aEditingHost, NoBreakingSpaceData* aNBSPData) {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (aPoint.IsInTextNode() && !aPoint.IsStartOfContainer()) {
    Maybe<BoundaryData> startInTextNode =
        BoundaryData::ScanCollapsibleWhiteSpaceStartInTextNode(aPoint,
                                                               aNBSPData);
    if (startInTextNode.isSome()) {
      return startInTextNode.ref();
    }
    // The text node does not have visible character, let's keep scanning
    // preceding nodes.
    return BoundaryData::ScanCollapsibleWhiteSpaceStartFrom(
        EditorDOMPoint(aPoint.ContainerAsText(), 0),
        aEditableBlockParentOrTopmostEditableInlineContent, aEditingHost,
        aNBSPData);
  }

  // Then, we need to check previous leaf node.
  nsIContent* previousLeafContentOrBlock =
      HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
          aPoint, aEditableBlockParentOrTopmostEditableInlineContent,
          {LeafNodeType::LeafNodeOrNonEditableNode}, aEditingHost);
  if (!previousLeafContentOrBlock) {
    // no prior node means we exhausted
    // aEditableBlockParentOrTopmostEditableInlineContent
    // mReasonContent can be either a block element or any non-editable
    // content in this case.
    return BoundaryData(aPoint,
                        const_cast<Element&>(
                            aEditableBlockParentOrTopmostEditableInlineContent),
                        WSType::CurrentBlockBoundary);
  }

  if (HTMLEditUtils::IsBlockElement(*previousLeafContentOrBlock)) {
    return BoundaryData(aPoint, *previousLeafContentOrBlock,
                        WSType::OtherBlockBoundary);
  }

  if (!previousLeafContentOrBlock->IsText() ||
      !previousLeafContentOrBlock->IsEditable()) {
    // it's a break or a special node, like <img>, that is not a block and
    // not a break but still serves as a terminator to ws runs.
    return BoundaryData(aPoint, *previousLeafContentOrBlock,
                        previousLeafContentOrBlock->IsHTMLElement(nsGkAtoms::br)
                            ? WSType::BRElement
                            : WSType::SpecialContent);
  }

  if (!previousLeafContentOrBlock->AsText()->TextLength()) {
    // If it's an empty text node, keep looking for its previous leaf content.
    // Note that even if the empty text node is preformatted, we should keep
    // looking for the previous one.
    return BoundaryData::ScanCollapsibleWhiteSpaceStartFrom(
        EditorDOMPointInText(previousLeafContentOrBlock->AsText(), 0),
        aEditableBlockParentOrTopmostEditableInlineContent, aEditingHost,
        aNBSPData);
  }

  Maybe<BoundaryData> startInTextNode =
      BoundaryData::ScanCollapsibleWhiteSpaceStartInTextNode(
          EditorDOMPointInText::AtEndOf(*previousLeafContentOrBlock->AsText()),
          aNBSPData);
  if (startInTextNode.isSome()) {
    return startInTextNode.ref();
  }

  // The text node does not have visible character, let's keep scanning
  // preceding nodes.
  return BoundaryData::ScanCollapsibleWhiteSpaceStartFrom(
      EditorDOMPointInText(previousLeafContentOrBlock->AsText(), 0),
      aEditableBlockParentOrTopmostEditableInlineContent, aEditingHost,
      aNBSPData);
}

// static
template <typename EditorDOMPointType>
Maybe<WSRunScanner::TextFragmentData::BoundaryData> WSRunScanner::
    TextFragmentData::BoundaryData::ScanCollapsibleWhiteSpaceEndInTextNode(
        const EditorDOMPointType& aPoint, NoBreakingSpaceData* aNBSPData) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_DIAGNOSTIC_ASSERT(aPoint.IsInTextNode());

  const bool isWhiteSpaceCollapsible =
      !EditorUtils::IsWhiteSpacePreformatted(*aPoint.ContainerAsText());
  const bool isNewLineCollapsible =
      !EditorUtils::IsNewLinePreformatted(*aPoint.ContainerAsText());
  const nsTextFragment& textFragment = aPoint.ContainerAsText()->TextFragment();
  for (uint32_t i = aPoint.Offset(); i < textFragment.GetLength(); i++) {
    WSType wsTypeOfNonCollapsibleChar;
    switch (textFragment.CharAt(i)) {
      case HTMLEditUtils::kSpace:
      case HTMLEditUtils::kCarriageReturn:
      case HTMLEditUtils::kTab:
        if (isWhiteSpaceCollapsible) {
          continue;  // collapsible white-space or invisible white-space.
        }
        // preformatted white-space.
        wsTypeOfNonCollapsibleChar = WSType::NonCollapsibleCharacters;
        break;
      case HTMLEditUtils::kNewLine:
        if (isNewLineCollapsible) {
          continue;  // collapsible linefeed.
        }
        // preformatted linefeed.
        wsTypeOfNonCollapsibleChar = WSType::PreformattedLineBreak;
        break;
      case HTMLEditUtils::kNBSP:
        if (isWhiteSpaceCollapsible) {
          if (aNBSPData) {
            aNBSPData->NotifyNBSP(
                EditorDOMPointInText(aPoint.ContainerAsText(), i),
                NoBreakingSpaceData::Scanning::Forward);
          }
          continue;
        }
        // NBSP is never converted from collapsible white-space/linefeed.
        wsTypeOfNonCollapsibleChar = WSType::NonCollapsibleCharacters;
        break;
      default:
        MOZ_ASSERT(!nsCRT::IsAsciiSpace(textFragment.CharAt(i)));
        wsTypeOfNonCollapsibleChar = WSType::NonCollapsibleCharacters;
        break;
    }

    return Some(BoundaryData(EditorDOMPoint(aPoint.ContainerAsText(), i),
                             *aPoint.ContainerAsText(),
                             wsTypeOfNonCollapsibleChar));
  }

  return Nothing();
}

// static
template <typename EditorDOMPointType>
WSRunScanner::TextFragmentData::BoundaryData
WSRunScanner::TextFragmentData::BoundaryData::ScanCollapsibleWhiteSpaceEndFrom(
    const EditorDOMPointType& aPoint,
    const Element& aEditableBlockParentOrTopmostEditableInlineElement,
    const Element* aEditingHost, NoBreakingSpaceData* aNBSPData) {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (aPoint.IsInTextNode() && !aPoint.IsEndOfContainer()) {
    Maybe<BoundaryData> endInTextNode =
        BoundaryData::ScanCollapsibleWhiteSpaceEndInTextNode(aPoint, aNBSPData);
    if (endInTextNode.isSome()) {
      return endInTextNode.ref();
    }
    // The text node does not have visible character, let's keep scanning
    // following nodes.
    return BoundaryData::ScanCollapsibleWhiteSpaceEndFrom(
        EditorDOMPointInText::AtEndOf(*aPoint.ContainerAsText()),
        aEditableBlockParentOrTopmostEditableInlineElement, aEditingHost,
        aNBSPData);
  }

  // Then, we need to check next leaf node.
  nsIContent* nextLeafContentOrBlock =
      HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          aPoint, aEditableBlockParentOrTopmostEditableInlineElement,
          {LeafNodeType::LeafNodeOrNonEditableNode}, aEditingHost);
  if (!nextLeafContentOrBlock) {
    // no next node means we exhausted
    // aEditableBlockParentOrTopmostEditableInlineElement
    // mReasonContent can be either a block element or any non-editable
    // content in this case.
    return BoundaryData(aPoint.template To<EditorDOMPoint>(),
                        const_cast<Element&>(
                            aEditableBlockParentOrTopmostEditableInlineElement),
                        WSType::CurrentBlockBoundary);
  }

  if (HTMLEditUtils::IsBlockElement(*nextLeafContentOrBlock)) {
    // we encountered a new block.  therefore no more ws.
    return BoundaryData(aPoint, *nextLeafContentOrBlock,
                        WSType::OtherBlockBoundary);
  }

  if (!nextLeafContentOrBlock->IsText() ||
      !nextLeafContentOrBlock->IsEditable()) {
    // we encountered a break or a special node, like <img>,
    // that is not a block and not a break but still
    // serves as a terminator to ws runs.
    return BoundaryData(aPoint, *nextLeafContentOrBlock,
                        nextLeafContentOrBlock->IsHTMLElement(nsGkAtoms::br)
                            ? WSType::BRElement
                            : WSType::SpecialContent);
  }

  if (!nextLeafContentOrBlock->AsText()->TextFragment().GetLength()) {
    // If it's an empty text node, keep looking for its next leaf content.
    // Note that even if the empty text node is preformatted, we should keep
    // looking for the next one.
    return BoundaryData::ScanCollapsibleWhiteSpaceEndFrom(
        EditorDOMPointInText(nextLeafContentOrBlock->AsText(), 0),
        aEditableBlockParentOrTopmostEditableInlineElement, aEditingHost,
        aNBSPData);
  }

  Maybe<BoundaryData> endInTextNode =
      BoundaryData::ScanCollapsibleWhiteSpaceEndInTextNode(
          EditorDOMPointInText(nextLeafContentOrBlock->AsText(), 0), aNBSPData);
  if (endInTextNode.isSome()) {
    return endInTextNode.ref();
  }

  // The text node does not have visible character, let's keep scanning
  // following nodes.
  return BoundaryData::ScanCollapsibleWhiteSpaceEndFrom(
      EditorDOMPointInText::AtEndOf(*nextLeafContentOrBlock->AsText()),
      aEditableBlockParentOrTopmostEditableInlineElement, aEditingHost,
      aNBSPData);
}

const EditorDOMRange&
WSRunScanner::TextFragmentData::InvisibleLeadingWhiteSpaceRangeRef() const {
  if (mLeadingWhiteSpaceRange.isSome()) {
    return mLeadingWhiteSpaceRange.ref();
  }

  // If it's start of line, there is no invisible leading white-spaces.
  if (!StartsFromHardLineBreak()) {
    mLeadingWhiteSpaceRange.emplace();
    return mLeadingWhiteSpaceRange.ref();
  }

  // If there is no NBSP, all of the given range is leading white-spaces.
  // Note that this result may be collapsed if there is no leading white-spaces.
  if (!mNBSPData.FoundNBSP()) {
    MOZ_ASSERT(mStart.PointRef().IsSet() || mEnd.PointRef().IsSet());
    mLeadingWhiteSpaceRange.emplace(mStart.PointRef(), mEnd.PointRef());
    return mLeadingWhiteSpaceRange.ref();
  }

  MOZ_ASSERT(mNBSPData.LastPointRef().IsSetAndValid());

  // Even if the first NBSP is the start, i.e., there is no invisible leading
  // white-space, return collapsed range.
  mLeadingWhiteSpaceRange.emplace(mStart.PointRef(), mNBSPData.FirstPointRef());
  return mLeadingWhiteSpaceRange.ref();
}

const EditorDOMRange&
WSRunScanner::TextFragmentData::InvisibleTrailingWhiteSpaceRangeRef() const {
  if (mTrailingWhiteSpaceRange.isSome()) {
    return mTrailingWhiteSpaceRange.ref();
  }

  // If it's not immediately before a block boundary nor an invisible
  // preformatted linefeed, there is no invisible trailing white-spaces.  Note
  // that collapsible white-spaces before a `<br>` element is visible.
  if (!EndsByBlockBoundary() && !EndsByInvisiblePreformattedLineBreak()) {
    mTrailingWhiteSpaceRange.emplace();
    return mTrailingWhiteSpaceRange.ref();
  }

  // If there is no NBSP, all of the given range is trailing white-spaces.
  // Note that this result may be collapsed if there is no trailing white-
  // spaces.
  if (!mNBSPData.FoundNBSP()) {
    MOZ_ASSERT(mStart.PointRef().IsSet() || mEnd.PointRef().IsSet());
    mTrailingWhiteSpaceRange.emplace(mStart.PointRef(), mEnd.PointRef());
    return mTrailingWhiteSpaceRange.ref();
  }

  MOZ_ASSERT(mNBSPData.LastPointRef().IsSetAndValid());

  // If last NBSP is immediately before the end, there is no trailing white-
  // spaces.
  if (mEnd.PointRef().IsSet() &&
      mNBSPData.LastPointRef().GetContainer() ==
          mEnd.PointRef().GetContainer() &&
      mNBSPData.LastPointRef().Offset() == mEnd.PointRef().Offset() - 1) {
    mTrailingWhiteSpaceRange.emplace();
    return mTrailingWhiteSpaceRange.ref();
  }

  // Otherwise, the may be some trailing white-spaces.
  MOZ_ASSERT(!mNBSPData.LastPointRef().IsEndOfContainer());
  mTrailingWhiteSpaceRange.emplace(mNBSPData.LastPointRef().NextPoint(),
                                   mEnd.PointRef());
  return mTrailingWhiteSpaceRange.ref();
}

EditorDOMRangeInTexts
WSRunScanner::TextFragmentData::GetNonCollapsedRangeInTexts(
    const EditorDOMRange& aRange) const {
  if (!aRange.IsPositioned()) {
    return EditorDOMRangeInTexts();
  }
  if (aRange.Collapsed()) {
    // If collapsed, we can do nothing.
    return EditorDOMRangeInTexts();
  }
  if (aRange.IsInTextNodes()) {
    // Note that this may return a range which don't include any invisible
    // white-spaces due to empty text nodes.
    return aRange.GetAsInTexts();
  }

  const auto firstPoint =
      aRange.StartRef().IsInTextNode()
          ? aRange.StartRef().AsInText()
          : GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(
                aRange.StartRef());
  if (!firstPoint.IsSet()) {
    return EditorDOMRangeInTexts();
  }
  EditorDOMPointInText endPoint;
  if (aRange.EndRef().IsInTextNode()) {
    endPoint = aRange.EndRef().AsInText();
  } else {
    // FYI: GetPreviousEditableCharPoint() returns last character's point
    //      of preceding text node if it's not empty, but we need end of
    //      the text node here.
    endPoint = GetPreviousEditableCharPoint(aRange.EndRef());
    if (endPoint.IsSet() && endPoint.IsAtLastContent()) {
      MOZ_ALWAYS_TRUE(endPoint.AdvanceOffset());
    }
  }
  if (!endPoint.IsSet() || firstPoint == endPoint) {
    return EditorDOMRangeInTexts();
  }
  return EditorDOMRangeInTexts(firstPoint, endPoint);
}

const WSRunScanner::VisibleWhiteSpacesData&
WSRunScanner::TextFragmentData::VisibleWhiteSpacesDataRef() const {
  if (mVisibleWhiteSpacesData.isSome()) {
    return mVisibleWhiteSpacesData.ref();
  }

  {
    // If all things are obviously visible, we can return range for all of the
    // things quickly.
    const bool mayHaveInvisibleLeadingSpace =
        !StartsFromNonCollapsibleCharacters() && !StartsFromSpecialContent();
    const bool mayHaveInvisibleTrailingWhiteSpace =
        !EndsByNonCollapsibleCharacters() && !EndsBySpecialContent() &&
        !EndsByBRElement() && !EndsByInvisiblePreformattedLineBreak();

    if (!mayHaveInvisibleLeadingSpace && !mayHaveInvisibleTrailingWhiteSpace) {
      VisibleWhiteSpacesData visibleWhiteSpaces;
      if (mStart.PointRef().IsSet()) {
        visibleWhiteSpaces.SetStartPoint(mStart.PointRef());
      }
      visibleWhiteSpaces.SetStartFrom(mStart.RawReason());
      if (mEnd.PointRef().IsSet()) {
        visibleWhiteSpaces.SetEndPoint(mEnd.PointRef());
      }
      visibleWhiteSpaces.SetEndBy(mEnd.RawReason());
      mVisibleWhiteSpacesData.emplace(visibleWhiteSpaces);
      return mVisibleWhiteSpacesData.ref();
    }
  }

  // If all of the range is invisible leading or trailing white-spaces,
  // there is no visible content.
  const EditorDOMRange& leadingWhiteSpaceRange =
      InvisibleLeadingWhiteSpaceRangeRef();
  const bool maybeHaveLeadingWhiteSpaces =
      leadingWhiteSpaceRange.StartRef().IsSet() ||
      leadingWhiteSpaceRange.EndRef().IsSet();
  if (maybeHaveLeadingWhiteSpaces &&
      leadingWhiteSpaceRange.StartRef() == mStart.PointRef() &&
      leadingWhiteSpaceRange.EndRef() == mEnd.PointRef()) {
    mVisibleWhiteSpacesData.emplace(VisibleWhiteSpacesData());
    return mVisibleWhiteSpacesData.ref();
  }
  const EditorDOMRange& trailingWhiteSpaceRange =
      InvisibleTrailingWhiteSpaceRangeRef();
  const bool maybeHaveTrailingWhiteSpaces =
      trailingWhiteSpaceRange.StartRef().IsSet() ||
      trailingWhiteSpaceRange.EndRef().IsSet();
  if (maybeHaveTrailingWhiteSpaces &&
      trailingWhiteSpaceRange.StartRef() == mStart.PointRef() &&
      trailingWhiteSpaceRange.EndRef() == mEnd.PointRef()) {
    mVisibleWhiteSpacesData.emplace(VisibleWhiteSpacesData());
    return mVisibleWhiteSpacesData.ref();
  }

  if (!StartsFromHardLineBreak()) {
    VisibleWhiteSpacesData visibleWhiteSpaces;
    if (mStart.PointRef().IsSet()) {
      visibleWhiteSpaces.SetStartPoint(mStart.PointRef());
    }
    visibleWhiteSpaces.SetStartFrom(mStart.RawReason());
    if (!maybeHaveTrailingWhiteSpaces) {
      visibleWhiteSpaces.SetEndPoint(mEnd.PointRef());
      visibleWhiteSpaces.SetEndBy(mEnd.RawReason());
      mVisibleWhiteSpacesData = Some(visibleWhiteSpaces);
      return mVisibleWhiteSpacesData.ref();
    }
    if (trailingWhiteSpaceRange.StartRef().IsSet()) {
      visibleWhiteSpaces.SetEndPoint(trailingWhiteSpaceRange.StartRef());
    }
    visibleWhiteSpaces.SetEndByTrailingWhiteSpaces();
    mVisibleWhiteSpacesData.emplace(visibleWhiteSpaces);
    return mVisibleWhiteSpacesData.ref();
  }

  MOZ_ASSERT(StartsFromHardLineBreak());
  MOZ_ASSERT(maybeHaveLeadingWhiteSpaces);

  VisibleWhiteSpacesData visibleWhiteSpaces;
  if (leadingWhiteSpaceRange.EndRef().IsSet()) {
    visibleWhiteSpaces.SetStartPoint(leadingWhiteSpaceRange.EndRef());
  }
  visibleWhiteSpaces.SetStartFromLeadingWhiteSpaces();
  if (!EndsByBlockBoundary()) {
    // then no trailing ws.  this normal run ends the overall ws run.
    if (mEnd.PointRef().IsSet()) {
      visibleWhiteSpaces.SetEndPoint(mEnd.PointRef());
    }
    visibleWhiteSpaces.SetEndBy(mEnd.RawReason());
    mVisibleWhiteSpacesData.emplace(visibleWhiteSpaces);
    return mVisibleWhiteSpacesData.ref();
  }

  MOZ_ASSERT(EndsByBlockBoundary());

  if (!maybeHaveTrailingWhiteSpaces) {
    // normal ws runs right up to adjacent block (nbsp next to block)
    visibleWhiteSpaces.SetEndPoint(mEnd.PointRef());
    visibleWhiteSpaces.SetEndBy(mEnd.RawReason());
    mVisibleWhiteSpacesData.emplace(visibleWhiteSpaces);
    return mVisibleWhiteSpacesData.ref();
  }

  if (trailingWhiteSpaceRange.StartRef().IsSet()) {
    visibleWhiteSpaces.SetEndPoint(trailingWhiteSpaceRange.StartRef());
  }
  visibleWhiteSpaces.SetEndByTrailingWhiteSpaces();
  mVisibleWhiteSpacesData.emplace(visibleWhiteSpaces);
  return mVisibleWhiteSpacesData.ref();
}

// static
nsresult WhiteSpaceVisibilityKeeper::
    MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange(
        HTMLEditor& aHTMLEditor, const EditorDOMRange& aRangeToDelete) {
  if (NS_WARN_IF(!aRangeToDelete.IsPositionedAndValid()) ||
      NS_WARN_IF(!aRangeToDelete.IsInContentNodes())) {
    return NS_ERROR_INVALID_ARG;
  }

  EditorDOMRange rangeToDelete(aRangeToDelete);
  bool mayBecomeUnexpectedDOMTree = aHTMLEditor.MayHaveMutationEventListeners(
      NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED |
      NS_EVENT_BITS_MUTATION_NODEREMOVED |
      NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT |
      NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED);

  RefPtr<Element> editingHost = aHTMLEditor.ComputeEditingHost();
  TextFragmentData textFragmentDataAtStart(rangeToDelete.StartRef(),
                                           editingHost);
  if (NS_WARN_IF(!textFragmentDataAtStart.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }
  TextFragmentData textFragmentDataAtEnd(rangeToDelete.EndRef(), editingHost);
  if (NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }
  ReplaceRangeData replaceRangeDataAtEnd =
      textFragmentDataAtEnd.GetReplaceRangeDataAtEndOfDeletionRange(
          textFragmentDataAtStart);
  if (replaceRangeDataAtEnd.IsSet() && !replaceRangeDataAtEnd.Collapsed()) {
    MOZ_ASSERT(rangeToDelete.EndRef().EqualsOrIsBefore(
        replaceRangeDataAtEnd.EndRef()));
    // If there is some text after deleting range, replacing range start must
    // equal or be before end of the deleting range.
    MOZ_ASSERT_IF(rangeToDelete.EndRef().IsInTextNode() &&
                      !rangeToDelete.EndRef().IsEndOfContainer(),
                  replaceRangeDataAtEnd.StartRef().EqualsOrIsBefore(
                      rangeToDelete.EndRef()));
    // If the deleting range end is end of a text node, the replacing range
    // starts with another node if the following text node starts with white-
    // spaces.
    MOZ_ASSERT_IF(rangeToDelete.EndRef().IsInTextNode() &&
                      rangeToDelete.EndRef().IsEndOfContainer(),
                  rangeToDelete.EndRef() == replaceRangeDataAtEnd.StartRef() ||
                      replaceRangeDataAtEnd.StartRef().IsStartOfContainer());
    MOZ_ASSERT(rangeToDelete.StartRef().EqualsOrIsBefore(
        replaceRangeDataAtEnd.StartRef()));
    if (!replaceRangeDataAtEnd.HasReplaceString()) {
      EditorDOMPoint startToDelete(aRangeToDelete.StartRef());
      EditorDOMPoint endToDelete(replaceRangeDataAtEnd.StartRef());
      {
        AutoEditorDOMPointChildInvalidator lockOffsetOfStart(startToDelete);
        AutoEditorDOMPointChildInvalidator lockOffsetOfEnd(endToDelete);
        AutoTrackDOMPoint trackStartToDelete(aHTMLEditor.RangeUpdaterRef(),
                                             &startToDelete);
        AutoTrackDOMPoint trackEndToDelete(aHTMLEditor.RangeUpdaterRef(),
                                           &endToDelete);
        nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            replaceRangeDataAtEnd.StartRef(), replaceRangeDataAtEnd.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return rv;
        }
      }
      if (mayBecomeUnexpectedDOMTree &&
          (NS_WARN_IF(!startToDelete.IsSetAndValid()) ||
           NS_WARN_IF(!endToDelete.IsSetAndValid()) ||
           NS_WARN_IF(!startToDelete.EqualsOrIsBefore(endToDelete)))) {
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      MOZ_ASSERT(startToDelete.EqualsOrIsBefore(endToDelete));
      rangeToDelete.SetStartAndEnd(startToDelete, endToDelete);
    } else {
      MOZ_ASSERT(replaceRangeDataAtEnd.RangeRef().IsInTextNodes());
      EditorDOMPoint startToDelete(aRangeToDelete.StartRef());
      EditorDOMPoint endToDelete(replaceRangeDataAtEnd.StartRef());
      {
        AutoTrackDOMPoint trackStartToDelete(aHTMLEditor.RangeUpdaterRef(),
                                             &startToDelete);
        AutoTrackDOMPoint trackEndToDelete(aHTMLEditor.RangeUpdaterRef(),
                                           &endToDelete);
        nsresult rv =
            WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
                aHTMLEditor, replaceRangeDataAtEnd.RangeRef().AsInTexts(),
                replaceRangeDataAtEnd.ReplaceStringRef());
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "WhiteSpaceVisibilityKeeper::"
              "MakeSureToKeepVisibleStateOfWhiteSpacesAtEndOfDeletingRange() "
              "failed");
          return rv;
        }
      }
      if (mayBecomeUnexpectedDOMTree &&
          (NS_WARN_IF(!startToDelete.IsSetAndValid()) ||
           NS_WARN_IF(!endToDelete.IsSetAndValid()) ||
           NS_WARN_IF(!startToDelete.EqualsOrIsBefore(endToDelete)))) {
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      MOZ_ASSERT(startToDelete.EqualsOrIsBefore(endToDelete));
      rangeToDelete.SetStartAndEnd(startToDelete, endToDelete);
    }

    if (mayBecomeUnexpectedDOMTree) {
      // If focus is changed by mutation event listeners, we should stop
      // handling this edit action.
      if (editingHost != aHTMLEditor.ComputeEditingHost()) {
        NS_WARNING("Active editing host was changed");
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      if (!rangeToDelete.IsInContentNodes()) {
        NS_WARNING("The modified range was not in content");
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      // If the DOM tree might be changed by mutation event listeners, we
      // should retrieve the latest data for avoiding to delete/replace
      // unexpected range.
      textFragmentDataAtStart =
          TextFragmentData(rangeToDelete.StartRef(), editingHost);
      textFragmentDataAtEnd =
          TextFragmentData(rangeToDelete.EndRef(), editingHost);
    }
  }
  ReplaceRangeData replaceRangeDataAtStart =
      textFragmentDataAtStart.GetReplaceRangeDataAtStartOfDeletionRange(
          textFragmentDataAtEnd);
  if (!replaceRangeDataAtStart.IsSet() || replaceRangeDataAtStart.Collapsed()) {
    return NS_OK;
  }
  if (!replaceRangeDataAtStart.HasReplaceString()) {
    nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        replaceRangeDataAtStart.StartRef(), replaceRangeDataAtStart.EndRef(),
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    // XXX Should we validate the range for making this return
    //     NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE in this case?
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }
  MOZ_ASSERT(replaceRangeDataAtStart.RangeRef().IsInTextNodes());
  nsresult rv = WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
      aHTMLEditor, replaceRangeDataAtStart.RangeRef().AsInTexts(),
      replaceRangeDataAtStart.ReplaceStringRef());
  // XXX Should we validate the range for making this return
  //     NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE in this case?
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "WhiteSpaceVisibilityKeeper::"
      "MakeSureToKeepVisibleStateOfWhiteSpacesAtStartOfDeletingRange() failed");
  return rv;
}

ReplaceRangeData
WSRunScanner::TextFragmentData::GetReplaceRangeDataAtEndOfDeletionRange(
    const TextFragmentData& aTextFragmentDataAtStartToDelete) const {
  const EditorDOMPoint& startToDelete =
      aTextFragmentDataAtStartToDelete.ScanStartRef();
  const EditorDOMPoint& endToDelete = mScanStartPoint;

  MOZ_ASSERT(startToDelete.IsSetAndValid());
  MOZ_ASSERT(endToDelete.IsSetAndValid());
  MOZ_ASSERT(startToDelete.EqualsOrIsBefore(endToDelete));

  if (EndRef().EqualsOrIsBefore(endToDelete)) {
    return ReplaceRangeData();
  }

  // If deleting range is followed by invisible trailing white-spaces, we need
  // to remove it for making them not visible.
  const EditorDOMRange invisibleTrailingWhiteSpaceRangeAtEnd =
      GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt(endToDelete);
  if (invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned()) {
    if (invisibleTrailingWhiteSpaceRangeAtEnd.Collapsed()) {
      return ReplaceRangeData();
    }
    // XXX Why don't we remove all invisible white-spaces?
    MOZ_ASSERT(invisibleTrailingWhiteSpaceRangeAtEnd.StartRef() == endToDelete);
    return ReplaceRangeData(invisibleTrailingWhiteSpaceRangeAtEnd, u""_ns);
  }

  // If end of the deleting range is followed by visible white-spaces which
  // is not preformatted, we might need to replace the following ASCII
  // white-spaces with an NBSP.
  const VisibleWhiteSpacesData& nonPreformattedVisibleWhiteSpacesAtEnd =
      VisibleWhiteSpacesDataRef();
  if (!nonPreformattedVisibleWhiteSpacesAtEnd.IsInitialized()) {
    return ReplaceRangeData();
  }
  const PointPosition pointPositionWithNonPreformattedVisibleWhiteSpacesAtEnd =
      nonPreformattedVisibleWhiteSpacesAtEnd.ComparePoint(endToDelete);
  if (pointPositionWithNonPreformattedVisibleWhiteSpacesAtEnd !=
          PointPosition::StartOfFragment &&
      pointPositionWithNonPreformattedVisibleWhiteSpacesAtEnd !=
          PointPosition::MiddleOfFragment) {
    return ReplaceRangeData();
  }
  // If start of deleting range follows white-spaces or end of delete
  // will be start of a line, the following text cannot start with an
  // ASCII white-space for keeping it visible.
  if (!aTextFragmentDataAtStartToDelete
           .FollowingContentMayBecomeFirstVisibleContent(startToDelete)) {
    return ReplaceRangeData();
  }
  auto nextCharOfStartOfEnd =
      GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(endToDelete);
  if (!nextCharOfStartOfEnd.IsSet() ||
      nextCharOfStartOfEnd.IsEndOfContainer() ||
      !nextCharOfStartOfEnd.IsCharCollapsibleASCIISpace()) {
    return ReplaceRangeData();
  }
  if (nextCharOfStartOfEnd.IsStartOfContainer() ||
      nextCharOfStartOfEnd.IsPreviousCharCollapsibleASCIISpace()) {
    nextCharOfStartOfEnd = aTextFragmentDataAtStartToDelete
                               .GetFirstASCIIWhiteSpacePointCollapsedTo(
                                   nextCharOfStartOfEnd, nsIEditor::eNone);
  }
  const EditorDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
      aTextFragmentDataAtStartToDelete.GetEndOfCollapsibleASCIIWhiteSpaces(
          nextCharOfStartOfEnd, nsIEditor::eNone);
  return ReplaceRangeData(nextCharOfStartOfEnd,
                          endOfCollapsibleASCIIWhiteSpaces,
                          nsDependentSubstring(&HTMLEditUtils::kNBSP, 1));
}

ReplaceRangeData
WSRunScanner::TextFragmentData::GetReplaceRangeDataAtStartOfDeletionRange(
    const TextFragmentData& aTextFragmentDataAtEndToDelete) const {
  const EditorDOMPoint& startToDelete = mScanStartPoint;
  const EditorDOMPoint& endToDelete =
      aTextFragmentDataAtEndToDelete.ScanStartRef();

  MOZ_ASSERT(startToDelete.IsSetAndValid());
  MOZ_ASSERT(endToDelete.IsSetAndValid());
  MOZ_ASSERT(startToDelete.EqualsOrIsBefore(endToDelete));

  if (startToDelete.EqualsOrIsBefore(StartRef())) {
    return ReplaceRangeData();
  }

  const EditorDOMRange invisibleLeadingWhiteSpaceRangeAtStart =
      GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(startToDelete);

  // If deleting range follows invisible leading white-spaces, we need to
  // remove them for making them not visible.
  if (invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned()) {
    if (invisibleLeadingWhiteSpaceRangeAtStart.Collapsed()) {
      return ReplaceRangeData();
    }

    // XXX Why don't we remove all leading white-spaces?
    return ReplaceRangeData(invisibleLeadingWhiteSpaceRangeAtStart, u""_ns);
  }

  // If start of the deleting range follows visible white-spaces which is not
  // preformatted, we might need to replace previous ASCII white-spaces with
  // an NBSP.
  const VisibleWhiteSpacesData& nonPreformattedVisibleWhiteSpacesAtStart =
      VisibleWhiteSpacesDataRef();
  if (!nonPreformattedVisibleWhiteSpacesAtStart.IsInitialized()) {
    return ReplaceRangeData();
  }
  const PointPosition
      pointPositionWithNonPreformattedVisibleWhiteSpacesAtStart =
          nonPreformattedVisibleWhiteSpacesAtStart.ComparePoint(startToDelete);
  if (pointPositionWithNonPreformattedVisibleWhiteSpacesAtStart !=
          PointPosition::MiddleOfFragment &&
      pointPositionWithNonPreformattedVisibleWhiteSpacesAtStart !=
          PointPosition::EndOfFragment) {
    return ReplaceRangeData();
  }
  // If end of the deleting range is (was) followed by white-spaces or
  // previous character of start of deleting range will be immediately
  // before a block boundary, the text cannot ends with an ASCII white-space
  // for keeping it visible.
  if (!aTextFragmentDataAtEndToDelete.PrecedingContentMayBecomeInvisible(
          endToDelete)) {
    return ReplaceRangeData();
  }
  EditorDOMPointInText atPreviousCharOfStart =
      GetPreviousEditableCharPoint(startToDelete);
  if (!atPreviousCharOfStart.IsSet() ||
      atPreviousCharOfStart.IsEndOfContainer() ||
      !atPreviousCharOfStart.IsCharCollapsibleASCIISpace()) {
    return ReplaceRangeData();
  }
  if (atPreviousCharOfStart.IsStartOfContainer() ||
      atPreviousCharOfStart.IsPreviousCharASCIISpace()) {
    atPreviousCharOfStart = GetFirstASCIIWhiteSpacePointCollapsedTo(
        atPreviousCharOfStart, nsIEditor::eNone);
  }
  const EditorDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
      GetEndOfCollapsibleASCIIWhiteSpaces(atPreviousCharOfStart,
                                          nsIEditor::eNone);
  return ReplaceRangeData(atPreviousCharOfStart,
                          endOfCollapsibleASCIIWhiteSpaces,
                          nsDependentSubstring(&HTMLEditUtils::kNBSP, 1));
}

// static
nsresult
WhiteSpaceVisibilityKeeper::MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToSplit) {
  TextFragmentData textFragmentDataAtSplitPoint(
      aPointToSplit, aHTMLEditor.ComputeEditingHost());
  if (NS_WARN_IF(!textFragmentDataAtSplitPoint.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }

  // used to prepare white-space sequence to be split across two blocks.
  // The main issue here is make sure white-spaces around the split point
  // doesn't end up becoming non-significant leading or trailing ws after
  // the split.
  const VisibleWhiteSpacesData& visibleWhiteSpaces =
      textFragmentDataAtSplitPoint.VisibleWhiteSpacesDataRef();
  if (!visibleWhiteSpaces.IsInitialized()) {
    return NS_OK;  // No visible white-space sequence.
  }

  PointPosition pointPositionWithVisibleWhiteSpaces =
      visibleWhiteSpaces.ComparePoint(aPointToSplit);

  // XXX If we split white-space sequence, the following code modify the DOM
  //     tree twice.  This is not reasonable and the latter change may touch
  //     wrong position.  We should do this once.

  // If we insert block boundary to start or middle of the white-space sequence,
  // the character at the insertion point needs to be an NBSP.
  EditorDOMPoint pointToSplit(aPointToSplit);
  if (pointPositionWithVisibleWhiteSpaces == PointPosition::StartOfFragment ||
      pointPositionWithVisibleWhiteSpaces == PointPosition::MiddleOfFragment) {
    EditorDOMPointInText atNextCharOfStart =
        textFragmentDataAtSplitPoint.GetInclusiveNextEditableCharPoint(
            pointToSplit);
    if (atNextCharOfStart.IsSet() && !atNextCharOfStart.IsEndOfContainer() &&
        atNextCharOfStart.IsCharCollapsibleASCIISpace()) {
      // pointToSplit will be referred bellow so that we need to keep
      // it a valid point.
      AutoEditorDOMPointChildInvalidator forgetChild(pointToSplit);
      AutoTrackDOMPoint trackSplitPoint(aHTMLEditor.RangeUpdaterRef(),
                                        &pointToSplit);
      if (atNextCharOfStart.IsStartOfContainer() ||
          atNextCharOfStart.IsPreviousCharASCIISpace()) {
        atNextCharOfStart = textFragmentDataAtSplitPoint
                                .GetFirstASCIIWhiteSpacePointCollapsedTo(
                                    atNextCharOfStart, nsIEditor::eNone);
      }
      const EditorDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
          textFragmentDataAtSplitPoint.GetEndOfCollapsibleASCIIWhiteSpaces(
              atNextCharOfStart, nsIEditor::eNone);
      nsresult rv =
          WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
              aHTMLEditor,
              EditorDOMRangeInTexts(atNextCharOfStart,
                                    endOfCollapsibleASCIIWhiteSpaces),
              nsDependentSubstring(&HTMLEditUtils::kNBSP, 1));
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes() "
            "failed");
        return rv;
      }
    }
  }

  // If we insert block boundary to middle of or end of the white-space
  // sequence, the previous character at the insertion point needs to be an
  // NBSP.
  if (pointPositionWithVisibleWhiteSpaces == PointPosition::MiddleOfFragment ||
      pointPositionWithVisibleWhiteSpaces == PointPosition::EndOfFragment) {
    EditorDOMPointInText atPreviousCharOfStart =
        textFragmentDataAtSplitPoint.GetPreviousEditableCharPoint(pointToSplit);
    if (atPreviousCharOfStart.IsSet() &&
        !atPreviousCharOfStart.IsEndOfContainer() &&
        atPreviousCharOfStart.IsCharCollapsibleASCIISpace()) {
      if (atPreviousCharOfStart.IsStartOfContainer() ||
          atPreviousCharOfStart.IsPreviousCharASCIISpace()) {
        atPreviousCharOfStart =
            textFragmentDataAtSplitPoint
                .GetFirstASCIIWhiteSpacePointCollapsedTo(atPreviousCharOfStart,
                                                         nsIEditor::eNone);
      }
      const EditorDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
          textFragmentDataAtSplitPoint.GetEndOfCollapsibleASCIIWhiteSpaces(
              atPreviousCharOfStart, nsIEditor::eNone);
      nsresult rv =
          WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
              aHTMLEditor,
              EditorDOMRangeInTexts(atPreviousCharOfStart,
                                    endOfCollapsibleASCIIWhiteSpaces),
              nsDependentSubstring(&HTMLEditUtils::kNBSP, 1));
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes() "
            "failed");
        return rv;
      }
    }
  }
  return NS_OK;
}

template <typename EditorDOMPointType, typename PT, typename CT>
EditorDOMPointType
WSRunScanner::TextFragmentData::GetInclusiveNextEditableCharPoint(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (NS_WARN_IF(!aPoint.IsInContentNode()) ||
      NS_WARN_IF(!mScanStartPoint.IsInContentNode())) {
    return EditorDOMPointType();
  }

  EditorRawDOMPoint point;
  if (nsIContent* child =
          aPoint.CanContainerHaveChildren() ? aPoint.GetChild() : nullptr) {
    nsIContent* leafContent = child->HasChildren()
                                  ? HTMLEditUtils::GetFirstLeafContent(
                                        *child, {LeafNodeType::OnlyLeafNode})
                                  : child;
    if (NS_WARN_IF(!leafContent)) {
      return EditorDOMPointType();
    }
    point.Set(leafContent, 0);
  } else {
    point = aPoint.template To<EditorRawDOMPoint>();
  }

  // If it points a character in a text node, return it.
  // XXX For the performance, this does not check whether the container
  //     is outside of our range.
  if (point.IsInTextNode() && point.GetContainer()->IsEditable() &&
      !point.IsEndOfContainer()) {
    return EditorDOMPointType(point.ContainerAsText(), point.Offset());
  }

  if (point.GetContainer() == GetEndReasonContent()) {
    return EditorDOMPointType();
  }

  NS_ASSERTION(EditorUtils::IsEditableContent(
                   *mScanStartPoint.ContainerAsContent(), EditorType::HTML),
               "Given content is not editable");
  NS_ASSERTION(
      mScanStartPoint.ContainerAsContent()->GetAsElementOrParentElement(),
      "Given content is not an element and an orphan node");
  nsIContent* editableBlockElementOrInlineEditingHost =
      mScanStartPoint.ContainerAsContent() &&
              EditorUtils::IsEditableContent(
                  *mScanStartPoint.ContainerAsContent(), EditorType::HTML)
          ? HTMLEditUtils::GetInclusiveAncestorElement(
                *mScanStartPoint.ContainerAsContent(),
                HTMLEditUtils::ClosestEditableBlockElementOrInlineEditingHost)
          : nullptr;
  if (NS_WARN_IF(!editableBlockElementOrInlineEditingHost)) {
    // Meaning that the container of `mScanStartPoint` is not editable.
    editableBlockElementOrInlineEditingHost =
        mScanStartPoint.ContainerAsContent();
  }

  for (nsIContent* nextContent =
           HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
               *point.ContainerAsContent(),
               *editableBlockElementOrInlineEditingHost,
               {LeafNodeType::LeafNodeOrNonEditableNode}, mEditingHost);
       nextContent;
       nextContent = HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
           *nextContent, *editableBlockElementOrInlineEditingHost,
           {LeafNodeType::LeafNodeOrNonEditableNode}, mEditingHost)) {
    if (!nextContent->IsText() || !nextContent->IsEditable()) {
      if (nextContent == GetEndReasonContent()) {
        break;  // Reached end of current runs.
      }
      continue;
    }
    return EditorDOMPointType(nextContent->AsText(), 0);
  }
  return EditorDOMPointType();
}

template <typename EditorDOMPointType, typename PT, typename CT>
EditorDOMPointType WSRunScanner::TextFragmentData::GetPreviousEditableCharPoint(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (NS_WARN_IF(!aPoint.IsInContentNode()) ||
      NS_WARN_IF(!mScanStartPoint.IsInContentNode())) {
    return EditorDOMPointType();
  }

  EditorRawDOMPoint point;
  if (nsIContent* previousChild = aPoint.CanContainerHaveChildren()
                                      ? aPoint.GetPreviousSiblingOfChild()
                                      : nullptr) {
    nsIContent* leafContent =
        previousChild->HasChildren()
            ? HTMLEditUtils::GetLastLeafContent(*previousChild,
                                                {LeafNodeType::OnlyLeafNode})
            : previousChild;
    if (NS_WARN_IF(!leafContent)) {
      return EditorDOMPointType();
    }
    point.SetToEndOf(leafContent);
  } else {
    point = aPoint.template To<EditorRawDOMPoint>();
  }

  // If it points a character in a text node and it's not first character
  // in it, return its previous point.
  // XXX For the performance, this does not check whether the container
  //     is outside of our range.
  if (point.IsInTextNode() && point.GetContainer()->IsEditable() &&
      !point.IsStartOfContainer()) {
    return EditorDOMPointType(point.ContainerAsText(), point.Offset() - 1);
  }

  if (point.GetContainer() == GetStartReasonContent()) {
    return EditorDOMPointType();
  }

  NS_ASSERTION(EditorUtils::IsEditableContent(
                   *mScanStartPoint.ContainerAsContent(), EditorType::HTML),
               "Given content is not editable");
  NS_ASSERTION(
      mScanStartPoint.ContainerAsContent()->GetAsElementOrParentElement(),
      "Given content is not an element and an orphan node");
  nsIContent* editableBlockElementOrInlineEditingHost =
      mScanStartPoint.ContainerAsContent() &&
              EditorUtils::IsEditableContent(
                  *mScanStartPoint.ContainerAsContent(), EditorType::HTML)
          ? HTMLEditUtils::GetInclusiveAncestorElement(
                *mScanStartPoint.ContainerAsContent(),
                HTMLEditUtils::ClosestEditableBlockElementOrInlineEditingHost)
          : nullptr;
  if (NS_WARN_IF(!editableBlockElementOrInlineEditingHost)) {
    // Meaning that the container of `mScanStartPoint` is not editable.
    editableBlockElementOrInlineEditingHost =
        mScanStartPoint.ContainerAsContent();
  }

  for (nsIContent* previousContent =
           HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
               *point.ContainerAsContent(),
               *editableBlockElementOrInlineEditingHost,
               {LeafNodeType::LeafNodeOrNonEditableNode}, mEditingHost);
       previousContent;
       previousContent =
           HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
               *previousContent, *editableBlockElementOrInlineEditingHost,
               {LeafNodeType::LeafNodeOrNonEditableNode}, mEditingHost)) {
    if (!previousContent->IsText() || !previousContent->IsEditable()) {
      if (previousContent == GetStartReasonContent()) {
        break;  // Reached start of current runs.
      }
      continue;
    }
    return EditorDOMPointType(previousContent->AsText(),
                              previousContent->AsText()->TextLength()
                                  ? previousContent->AsText()->TextLength() - 1
                                  : 0);
  }
  return EditorDOMPointType();
}

// static
template <typename EditorDOMPointType>
EditorDOMPointType WSRunScanner::GetAfterLastVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter) {
  EditorDOMPoint atLastCharOfTextNode(
      &aTextNode, AssertedCast<uint32_t>(std::max<int64_t>(
                      static_cast<int64_t>(aTextNode.Length()) - 1, 0)));
  if (!atLastCharOfTextNode.IsContainerEmpty() &&
      !atLastCharOfTextNode.IsCharCollapsibleASCIISpace()) {
    return EditorDOMPointType::AtEndOf(aTextNode);
  }
  TextFragmentData textFragmentData(atLastCharOfTextNode, aAncestorLimiter);
  if (NS_WARN_IF(!textFragmentData.IsInitialized())) {
    return EditorDOMPointType();  // TODO: Make here return error with Err.
  }
  const EditorDOMRange& invisibleWhiteSpaceRange =
      textFragmentData.InvisibleTrailingWhiteSpaceRangeRef();
  if (!invisibleWhiteSpaceRange.IsPositioned() ||
      invisibleWhiteSpaceRange.Collapsed()) {
    return EditorDOMPointType::AtEndOf(aTextNode);
  }
  return invisibleWhiteSpaceRange.StartRef().To<EditorDOMPointType>();
}

// static
template <typename EditorDOMPointType>
EditorDOMPointType WSRunScanner::GetFirstVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter) {
  EditorDOMPoint atStartOfTextNode(&aTextNode, 0);
  if (!atStartOfTextNode.IsContainerEmpty() &&
      atStartOfTextNode.IsCharCollapsibleASCIISpace()) {
    return atStartOfTextNode.To<EditorDOMPointType>();
  }
  TextFragmentData textFragmentData(atStartOfTextNode, aAncestorLimiter);
  if (NS_WARN_IF(!textFragmentData.IsInitialized())) {
    return EditorDOMPointType();  // TODO: Make here return error with Err.
  }
  const EditorDOMRange& invisibleWhiteSpaceRange =
      textFragmentData.InvisibleLeadingWhiteSpaceRangeRef();
  if (!invisibleWhiteSpaceRange.IsPositioned() ||
      invisibleWhiteSpaceRange.Collapsed()) {
    return atStartOfTextNode.To<EditorDOMPointType>();
  }
  return invisibleWhiteSpaceRange.EndRef().To<EditorDOMPointType>();
}

template <typename EditorDOMPointType>
EditorDOMPointType
WSRunScanner::TextFragmentData::GetEndOfCollapsibleASCIIWhiteSpaces(
    const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
    nsIEditor::EDirection aDirectionToDelete) const {
  MOZ_ASSERT(aDirectionToDelete == nsIEditor::eNone ||
             aDirectionToDelete == nsIEditor::eNext ||
             aDirectionToDelete == nsIEditor::ePrevious);
  MOZ_ASSERT(aPointAtASCIIWhiteSpace.IsSet());
  MOZ_ASSERT(!aPointAtASCIIWhiteSpace.IsEndOfContainer());
  MOZ_ASSERT_IF(!EditorUtils::IsNewLinePreformatted(
                    *aPointAtASCIIWhiteSpace.ContainerAsContent()),
                aPointAtASCIIWhiteSpace.IsCharCollapsibleASCIISpace());
  MOZ_ASSERT_IF(EditorUtils::IsNewLinePreformatted(
                    *aPointAtASCIIWhiteSpace.ContainerAsContent()),
                aPointAtASCIIWhiteSpace.IsCharASCIISpace());

  // If we're deleting text forward and the next visible character is first
  // preformatted new line but white-spaces can be collapsed, we need to
  // delete its following collapsible white-spaces too.
  bool hasSeenPreformattedNewLine =
      aPointAtASCIIWhiteSpace.IsCharPreformattedNewLine();
  auto NeedToScanFollowingWhiteSpaces =
      [&hasSeenPreformattedNewLine, &aDirectionToDelete](
          const EditorDOMPointInText& aAtNextVisibleCharacter) -> bool {
    MOZ_ASSERT(!aAtNextVisibleCharacter.IsEndOfContainer());
    return !hasSeenPreformattedNewLine &&
           aDirectionToDelete == nsIEditor::eNext &&
           aAtNextVisibleCharacter
               .IsCharPreformattedNewLineCollapsedWithWhiteSpaces();
  };
  auto ScanNextNonCollapsibleChar =
      [&hasSeenPreformattedNewLine, &NeedToScanFollowingWhiteSpaces](
          const EditorDOMPointInText& aPoint) -> EditorDOMPointInText {
    Maybe<uint32_t> nextVisibleCharOffset =
        HTMLEditUtils::GetNextNonCollapsibleCharOffset(aPoint);
    if (!nextVisibleCharOffset.isSome()) {
      return EditorDOMPointInText();  // Keep scanning following text nodes
    }
    EditorDOMPointInText atNextVisibleChar(aPoint.ContainerAsText(),
                                           nextVisibleCharOffset.value());
    if (!NeedToScanFollowingWhiteSpaces(atNextVisibleChar)) {
      return atNextVisibleChar;
    }
    hasSeenPreformattedNewLine |= atNextVisibleChar.IsCharPreformattedNewLine();
    nextVisibleCharOffset =
        HTMLEditUtils::GetNextNonCollapsibleCharOffset(atNextVisibleChar);
    if (nextVisibleCharOffset.isSome()) {
      MOZ_ASSERT(aPoint.ContainerAsText() ==
                 atNextVisibleChar.ContainerAsText());
      return EditorDOMPointInText(atNextVisibleChar.ContainerAsText(),
                                  nextVisibleCharOffset.value());
    }
    return EditorDOMPointInText();  // Keep scanning following text nodes
  };

  // If it's not the last character in the text node, let's scan following
  // characters in it.
  if (!aPointAtASCIIWhiteSpace.IsAtLastContent()) {
    const EditorDOMPointInText atNextVisibleChar(
        ScanNextNonCollapsibleChar(aPointAtASCIIWhiteSpace));
    if (atNextVisibleChar.IsSet()) {
      return atNextVisibleChar.To<EditorDOMPointType>();
    }
  }

  // Otherwise, i.e., the text node ends with ASCII white-space, keep scanning
  // the following text nodes.
  // XXX Perhaps, we should stop scanning if there is non-editable and visible
  //     content.
  EditorDOMPointInText afterLastWhiteSpace =
      EditorDOMPointInText::AtEndOf(*aPointAtASCIIWhiteSpace.ContainerAsText());
  for (EditorDOMPointInText atEndOfPreviousTextNode = afterLastWhiteSpace;;) {
    const auto atStartOfNextTextNode =
        GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(
            atEndOfPreviousTextNode);
    if (!atStartOfNextTextNode.IsSet()) {
      // There is no more text nodes.  Return end of the previous text node.
      return afterLastWhiteSpace.To<EditorDOMPointType>();
    }

    // We can ignore empty text nodes (even if it's preformatted).
    if (atStartOfNextTextNode.IsContainerEmpty()) {
      atEndOfPreviousTextNode = atStartOfNextTextNode;
      continue;
    }

    // If next node starts with non-white-space character or next node is
    // preformatted, return end of previous text node.  However, if it
    // starts with a preformatted linefeed but white-spaces are collapsible,
    // we need to scan following collapsible white-spaces when we're deleting
    // text forward.
    if (!atStartOfNextTextNode.IsCharCollapsibleASCIISpace() &&
        !NeedToScanFollowingWhiteSpaces(atStartOfNextTextNode)) {
      return afterLastWhiteSpace.To<EditorDOMPointType>();
    }

    // Otherwise, scan the text node.
    const EditorDOMPointInText atNextVisibleChar(
        ScanNextNonCollapsibleChar(atStartOfNextTextNode));
    if (atNextVisibleChar.IsSet()) {
      return atNextVisibleChar.To<EditorDOMPointType>();
    }

    // The next text nodes ends with white-space too.  Try next one.
    afterLastWhiteSpace = atEndOfPreviousTextNode =
        EditorDOMPointInText::AtEndOf(*atStartOfNextTextNode.ContainerAsText());
  }
}

template <typename EditorDOMPointType>
EditorDOMPointType
WSRunScanner::TextFragmentData::GetFirstASCIIWhiteSpacePointCollapsedTo(
    const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
    nsIEditor::EDirection aDirectionToDelete) const {
  MOZ_ASSERT(aDirectionToDelete == nsIEditor::eNone ||
             aDirectionToDelete == nsIEditor::eNext ||
             aDirectionToDelete == nsIEditor::ePrevious);
  MOZ_ASSERT(aPointAtASCIIWhiteSpace.IsSet());
  MOZ_ASSERT(!aPointAtASCIIWhiteSpace.IsEndOfContainer());
  MOZ_ASSERT_IF(!EditorUtils::IsNewLinePreformatted(
                    *aPointAtASCIIWhiteSpace.ContainerAsContent()),
                aPointAtASCIIWhiteSpace.IsCharCollapsibleASCIISpace());
  MOZ_ASSERT_IF(EditorUtils::IsNewLinePreformatted(
                    *aPointAtASCIIWhiteSpace.ContainerAsContent()),
                aPointAtASCIIWhiteSpace.IsCharASCIISpace());

  // If we're deleting text backward and the previous visible character is first
  // preformatted new line but white-spaces can be collapsed, we need to delete
  // its preceding collapsible white-spaces too.
  bool hasSeenPreformattedNewLine =
      aPointAtASCIIWhiteSpace.IsCharPreformattedNewLine();
  auto NeedToScanPrecedingWhiteSpaces =
      [&hasSeenPreformattedNewLine, &aDirectionToDelete](
          const EditorDOMPointInText& aAtPreviousVisibleCharacter) -> bool {
    MOZ_ASSERT(!aAtPreviousVisibleCharacter.IsEndOfContainer());
    return !hasSeenPreformattedNewLine &&
           aDirectionToDelete == nsIEditor::ePrevious &&
           aAtPreviousVisibleCharacter
               .IsCharPreformattedNewLineCollapsedWithWhiteSpaces();
  };
  auto ScanPreviousNonCollapsibleChar =
      [&hasSeenPreformattedNewLine, &NeedToScanPrecedingWhiteSpaces](
          const EditorDOMPointInText& aPoint) -> EditorDOMPointInText {
    Maybe<uint32_t> previousVisibleCharOffset =
        HTMLEditUtils::GetPreviousNonCollapsibleCharOffset(aPoint);
    if (previousVisibleCharOffset.isNothing()) {
      return EditorDOMPointInText();  // Keep scanning preceding text nodes
    }
    EditorDOMPointInText atPreviousVisibleCharacter(
        aPoint.ContainerAsText(), previousVisibleCharOffset.value());
    if (!NeedToScanPrecedingWhiteSpaces(atPreviousVisibleCharacter)) {
      return atPreviousVisibleCharacter.NextPoint();
    }
    hasSeenPreformattedNewLine |=
        atPreviousVisibleCharacter.IsCharPreformattedNewLine();
    previousVisibleCharOffset =
        HTMLEditUtils::GetPreviousNonCollapsibleCharOffset(
            atPreviousVisibleCharacter);
    if (previousVisibleCharOffset.isSome()) {
      MOZ_ASSERT(aPoint.ContainerAsText() ==
                 atPreviousVisibleCharacter.ContainerAsText());
      return EditorDOMPointInText(atPreviousVisibleCharacter.ContainerAsText(),
                                  previousVisibleCharOffset.value() + 1);
    }
    return EditorDOMPointInText();  // Keep scanning preceding text nodes
  };

  // If there is some characters before it, scan it in the text node first.
  if (!aPointAtASCIIWhiteSpace.IsStartOfContainer()) {
    EditorDOMPointInText atFirstASCIIWhiteSpace(
        ScanPreviousNonCollapsibleChar(aPointAtASCIIWhiteSpace));
    if (atFirstASCIIWhiteSpace.IsSet()) {
      return atFirstASCIIWhiteSpace.To<EditorDOMPointType>();
    }
  }

  // Otherwise, i.e., the text node starts with ASCII white-space, keep scanning
  // the preceding text nodes.
  // XXX Perhaps, we should stop scanning if there is non-editable and visible
  //     content.
  EditorDOMPointInText atLastWhiteSpace =
      EditorDOMPointInText(aPointAtASCIIWhiteSpace.ContainerAsText(), 0u);
  for (EditorDOMPointInText atStartOfPreviousTextNode = atLastWhiteSpace;;) {
    const EditorDOMPointInText atLastCharOfPreviousTextNode =
        GetPreviousEditableCharPoint(atStartOfPreviousTextNode);
    if (!atLastCharOfPreviousTextNode.IsSet()) {
      // There is no more text nodes.  Return end of last text node.
      return atLastWhiteSpace.To<EditorDOMPointType>();
    }

    // We can ignore empty text nodes (even if it's preformatted).
    if (atLastCharOfPreviousTextNode.IsContainerEmpty()) {
      atStartOfPreviousTextNode = atLastCharOfPreviousTextNode;
      continue;
    }

    // If next node ends with non-white-space character or next node is
    // preformatted, return start of previous text node.
    if (!atLastCharOfPreviousTextNode.IsCharCollapsibleASCIISpace() &&
        !NeedToScanPrecedingWhiteSpaces(atLastCharOfPreviousTextNode)) {
      return atLastWhiteSpace.To<EditorDOMPointType>();
    }

    // Otherwise, scan the text node.
    const EditorDOMPointInText atFirstASCIIWhiteSpace(
        ScanPreviousNonCollapsibleChar(atLastCharOfPreviousTextNode));
    if (atFirstASCIIWhiteSpace.IsSet()) {
      return atFirstASCIIWhiteSpace.To<EditorDOMPointType>();
    }

    // The next text nodes starts with white-space too.  Try next one.
    atLastWhiteSpace = atStartOfPreviousTextNode = EditorDOMPointInText(
        atLastCharOfPreviousTextNode.ContainerAsText(), 0u);
  }
}

// static
nsresult WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
    HTMLEditor& aHTMLEditor, const EditorDOMRangeInTexts& aRangeToReplace,
    const nsAString& aReplaceString) {
  MOZ_ASSERT(aRangeToReplace.IsPositioned());
  MOZ_ASSERT(aRangeToReplace.StartRef().IsSetAndValid());
  MOZ_ASSERT(aRangeToReplace.EndRef().IsSetAndValid());
  MOZ_ASSERT(aRangeToReplace.StartRef().IsBefore(aRangeToReplace.EndRef()));

  AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
  nsresult rv = aHTMLEditor.ReplaceTextWithTransaction(
      MOZ_KnownLive(*aRangeToReplace.StartRef().ContainerAsText()),
      aRangeToReplace.StartRef().Offset(),
      aRangeToReplace.InSameContainer()
          ? aRangeToReplace.EndRef().Offset() -
                aRangeToReplace.StartRef().Offset()
          : aRangeToReplace.StartRef().ContainerAsText()->TextLength() -
                aRangeToReplace.StartRef().Offset(),
      aReplaceString);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
    return rv;
  }

  if (aRangeToReplace.InSameContainer()) {
    return NS_OK;
  }

  rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
      EditorDOMPointInText::AtEndOf(
          *aRangeToReplace.StartRef().ContainerAsText()),
      aRangeToReplace.EndRef(),
      HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
  return rv;
}

char16_t WSRunScanner::GetCharAt(Text* aTextNode, uint32_t aOffset) const {
  // return 0 if we can't get a char, for whatever reason
  if (NS_WARN_IF(!aTextNode) ||
      NS_WARN_IF(aOffset >= aTextNode->TextDataLength())) {
    return 0;
  }
  return aTextNode->TextFragment().CharAt(aOffset);
}

// static
template <typename EditorDOMPointType>
nsresult WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt(
    HTMLEditor& aHTMLEditor, const EditorDOMPointType& aPoint) {
  MOZ_ASSERT(aPoint.IsInContentNode());
  MOZ_ASSERT(EditorUtils::IsEditableContent(*aPoint.ContainerAsContent(),
                                            EditorType::HTML));
  Element* editingHost = aHTMLEditor.ComputeEditingHost();
  TextFragmentData textFragmentData(aPoint, editingHost);
  if (NS_WARN_IF(!textFragmentData.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }

  // this routine examines a run of ws and tries to get rid of some unneeded
  // nbsp's, replacing them with regular ascii space if possible.  Keeping
  // things simple for now and just trying to fix up the trailing ws in the run.
  if (!textFragmentData.FoundNoBreakingWhiteSpaces()) {
    // nothing to do!
    return NS_OK;
  }
  const VisibleWhiteSpacesData& visibleWhiteSpaces =
      textFragmentData.VisibleWhiteSpacesDataRef();
  if (!visibleWhiteSpaces.IsInitialized()) {
    return NS_OK;
  }

  // Remove this block if we ship Blink-compat white-space normalization.
  if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    // now check that what is to the left of it is compatible with replacing
    // nbsp with space
    const EditorDOMPoint& atEndOfVisibleWhiteSpaces =
        visibleWhiteSpaces.EndRef();
    EditorDOMPointInText atPreviousCharOfEndOfVisibleWhiteSpaces =
        textFragmentData.GetPreviousEditableCharPoint(
            atEndOfVisibleWhiteSpaces);
    if (!atPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() ||
        atPreviousCharOfEndOfVisibleWhiteSpaces.IsEndOfContainer() ||
        // If the NBSP is never replaced from an ASCII white-space, we cannod
        // replace it with an ASCII white-space.
        !atPreviousCharOfEndOfVisibleWhiteSpaces.IsCharCollapsibleNBSP()) {
      return NS_OK;
    }

    // now check that what is to the left of it is compatible with replacing
    // nbsp with space
    EditorDOMPointInText atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces =
        textFragmentData.GetPreviousEditableCharPoint(
            atPreviousCharOfEndOfVisibleWhiteSpaces);
    bool isPreviousCharCollapsibleASCIIWhiteSpace =
        atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
        !atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
             .IsEndOfContainer() &&
        atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
            .IsCharCollapsibleASCIISpace();
    const bool maybeNBSPFollowsVisibleContent =
        (atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
         !isPreviousCharCollapsibleASCIIWhiteSpace) ||
        (!atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
         (visibleWhiteSpaces.StartsFromNonCollapsibleCharacters() ||
          visibleWhiteSpaces.StartsFromSpecialContent()));
    bool followedByVisibleContent =
        visibleWhiteSpaces.EndsByNonCollapsibleCharacters() ||
        visibleWhiteSpaces.EndsBySpecialContent();
    bool followedByBRElement = visibleWhiteSpaces.EndsByBRElement();
    bool followedByPreformattedLineBreak =
        visibleWhiteSpaces.EndsByPreformattedLineBreak();

    // If the NBSP follows a visible content or a collapsible ASCII white-space,
    // i.e., unless NBSP is first character and start of a block, we may need to
    // insert <br> element and restore the NBSP to an ASCII white-space.
    if (maybeNBSPFollowsVisibleContent ||
        isPreviousCharCollapsibleASCIIWhiteSpace) {
      // First, try to insert <br> element if NBSP is at end of a block.
      // XXX We should stop this if there is a visible content.
      if (visibleWhiteSpaces.EndsByBlockBoundary() &&
          aPoint.IsInContentNode()) {
        bool insertBRElement =
            HTMLEditUtils::IsBlockElement(*aPoint.ContainerAsContent());
        if (!insertBRElement) {
          NS_ASSERTION(EditorUtils::IsEditableContent(
                           *aPoint.ContainerAsContent(), EditorType::HTML),
                       "Given content is not editable");
          NS_ASSERTION(
              aPoint.ContainerAsContent()->GetAsElementOrParentElement(),
              "Given content is not an element and an orphan node");
          const Element* editableBlockElement =
              EditorUtils::IsEditableContent(*aPoint.ContainerAsContent(),
                                             EditorType::HTML)
                  ? HTMLEditUtils::GetInclusiveAncestorElement(
                        *aPoint.ContainerAsContent(),
                        HTMLEditUtils::ClosestEditableBlockElement)
                  : nullptr;
          insertBRElement = !!editableBlockElement;
        }
        if (insertBRElement) {
          // We are at a block boundary.  Insert a <br>.  Why?  Well, first note
          // that the br will have no visible effect since it is up against a
          // block boundary.  |foo<br><p>bar| renders like |foo<p>bar| and
          // similarly |<p>foo<br></p>bar| renders like |<p>foo</p>bar|.  What
          // this <br> addition gets us is the ability to convert a trailing
          // nbsp to a space.  Consider: |<body>foo. '</body>|, where '
          // represents selection.  User types space attempting to put 2 spaces
          // after the end of their sentence.  We used to do this as:
          // |<body>foo. &nbsp</body>|  This caused problems with soft wrapping:
          // the nbsp would wrap to the next line, which looked attrocious.  If
          // you try to do: |<body>foo.&nbsp </body>| instead, the trailing
          // space is invisible because it is against a block boundary.  If you
          // do:
          // |<body>foo.&nbsp&nbsp</body>| then you get an even uglier soft
          // wrapping problem, where foo is on one line until you type the final
          // space, and then "foo  " jumps down to the next line.  Ugh.  The
          // best way I can find out of this is to throw in a harmless <br>
          // here, which allows us to do: |<body>foo.&nbsp <br></body>|, which
          // doesn't cause foo to jump lines, doesn't cause spaces to show up at
          // the beginning of soft wrapped lines, and lets the user see 2 spaces
          // when they type 2 spaces.

          const CreateElementResult insertBRElementResult =
              aHTMLEditor.InsertBRElement(HTMLEditor::WithTransaction::Yes,
                                          atEndOfVisibleWhiteSpaces);
          if (insertBRElementResult.isErr()) {
            NS_WARNING(
                "HTMLEditor::InsertBRElement(WithTransaction::Yes) failed");
            return insertBRElementResult.unwrapErr();
          }
          // XXX Is this intentional selection change?
          nsresult rv = insertBRElementResult.SuggestCaretPointTo(
              aHTMLEditor, {SuggestCaret::OnlyIfHasSuggestion,
                            SuggestCaret::OnlyIfTransactionsAllowedToDoIt,
                            SuggestCaret::AndIgnoreTrivialError});
          if (NS_FAILED(rv)) {
            NS_WARNING("CreateElementResult::SuggestCaretPointTo() failed");
            return rv;
          }
          NS_WARNING_ASSERTION(
              rv != NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR,
              "CreateElementResult::SuggestCaretPointTo() failed, but ignored");
          MOZ_ASSERT(insertBRElementResult.GetNewNode());

          atPreviousCharOfEndOfVisibleWhiteSpaces =
              textFragmentData.GetPreviousEditableCharPoint(
                  atEndOfVisibleWhiteSpaces);
          if (MOZ_UNLIKELY(NS_WARN_IF(
                  !atPreviousCharOfEndOfVisibleWhiteSpaces.IsSet()))) {
            return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
          }
          atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces =
              textFragmentData.GetPreviousEditableCharPoint(
                  atPreviousCharOfEndOfVisibleWhiteSpaces);
          isPreviousCharCollapsibleASCIIWhiteSpace =
              atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
              !atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                   .IsEndOfContainer() &&
              atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                  .IsCharCollapsibleASCIISpace();
          followedByBRElement = true;
          followedByVisibleContent = followedByPreformattedLineBreak = false;
        }
      }

      // Once insert a <br>, the remaining work is only for normalizing
      // white-space sequence in white-space collapsible text node.
      // So, if the the text node's white-spaces are preformatted, we need
      // to do nothing anymore.
      if (EditorUtils::IsWhiteSpacePreformatted(
              *atPreviousCharOfEndOfVisibleWhiteSpaces.ContainerAsText())) {
        return NS_OK;
      }

      // Next, replace the NBSP with an ASCII white-space if it's surrounded
      // by visible contents (or immediately before a <br> element).
      // However, if it follows or is followed by a preformatted linefeed,
      // we shouldn't do this because an ASCII white-space will be collapsed
      // **into** the linefeed.
      if (maybeNBSPFollowsVisibleContent &&
          (followedByVisibleContent || followedByBRElement) &&
          !visibleWhiteSpaces.StartsFromPreformattedLineBreak()) {
        MOZ_ASSERT(!followedByPreformattedLineBreak);
        AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
        nsresult rv = aHTMLEditor.ReplaceTextWithTransaction(
            MOZ_KnownLive(
                *atPreviousCharOfEndOfVisibleWhiteSpaces.ContainerAsText()),
            atPreviousCharOfEndOfVisibleWhiteSpaces.Offset(), 1, u" "_ns);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "HTMLEditor::ReplaceTextWithTransaction() failed");
        return rv;
      }
    }
    // If the text node is not preformatted, and the NBSP is followed by a <br>
    // element and following (maybe multiple) collapsible ASCII white-spaces,
    // remove the NBSP, but inserts a NBSP before the spaces.  This makes a line
    // break opportunity to wrap the line.
    // XXX This is different behavior from Blink.  Blink generates pairs of
    //     an NBSP and an ASCII white-space, but put NBSP at the end of the
    //     sequence.  We should follow the behavior for web-compat.
    if (maybeNBSPFollowsVisibleContent ||
        !isPreviousCharCollapsibleASCIIWhiteSpace ||
        !(followedByVisibleContent || followedByBRElement) ||
        EditorUtils::IsWhiteSpacePreformatted(
            *atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                 .GetContainerAsText())) {
      return NS_OK;
    }

    // Currently, we're at an NBSP following an ASCII space, and we need to
    // replace them with `"&nbsp; "` for avoiding collapsing white-spaces.
    MOZ_ASSERT(!atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                    .IsEndOfContainer());
    const EditorDOMPointInText atFirstASCIIWhiteSpace =
        textFragmentData.GetFirstASCIIWhiteSpacePointCollapsedTo(
            atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces,
            nsIEditor::eNone);
    AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
    uint32_t numberOfASCIIWhiteSpacesInStartNode =
        atFirstASCIIWhiteSpace.ContainerAsText() ==
                atPreviousCharOfEndOfVisibleWhiteSpaces.ContainerAsText()
            ? atPreviousCharOfEndOfVisibleWhiteSpaces.Offset() -
                  atFirstASCIIWhiteSpace.Offset()
            : atFirstASCIIWhiteSpace.ContainerAsText()->Length() -
                  atFirstASCIIWhiteSpace.Offset();
    // Replace all preceding ASCII white-spaces **and** the NBSP.
    uint32_t replaceLengthInStartNode =
        numberOfASCIIWhiteSpacesInStartNode +
        (atFirstASCIIWhiteSpace.ContainerAsText() ==
                 atPreviousCharOfEndOfVisibleWhiteSpaces.ContainerAsText()
             ? 1
             : 0);
    nsresult rv = aHTMLEditor.ReplaceTextWithTransaction(
        MOZ_KnownLive(*atFirstASCIIWhiteSpace.ContainerAsText()),
        atFirstASCIIWhiteSpace.Offset(), replaceLengthInStartNode,
        textFragmentData.StartsFromPreformattedLineBreak() &&
                textFragmentData.EndsByPreformattedLineBreak()
            ? u"\x00A0\x00A0"_ns
            : (textFragmentData.EndsByPreformattedLineBreak() ? u" \x00A0"_ns
                                                              : u"\x00A0 "_ns));
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
      return rv;
    }

    if (atFirstASCIIWhiteSpace.GetContainer() ==
        atPreviousCharOfEndOfVisibleWhiteSpaces.GetContainer()) {
      return NS_OK;
    }

    // We need to remove the following unnecessary ASCII white-spaces and
    // NBSP at atPreviousCharOfEndOfVisibleWhiteSpaces because we collapsed them
    // into the start node.
    rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        EditorDOMPointInText::AtEndOf(
            *atFirstASCIIWhiteSpace.ContainerAsText()),
        atPreviousCharOfEndOfVisibleWhiteSpaces.NextPoint(),
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  // XXX This is called when top-level edit sub-action handling ends for
  //     3 points at most.  However, this is not compatible with Blink.
  //     Blink touches white-space sequence which includes new character
  //     or following white-space sequence of new <br> element or, if and
  //     only if deleting range is followed by white-space sequence (i.e.,
  //     not touched previous white-space sequence of deleting range).
  //     This should be done when we change to make each edit action
  //     handler directly normalize white-space sequence rather than
  //     OnEndHandlingTopLevelEditSucAction().

  // First, check if the last character is an NBSP.  Otherwise, we don't need
  // to do nothing here.
  const EditorDOMPoint& atEndOfVisibleWhiteSpaces = visibleWhiteSpaces.EndRef();
  const EditorDOMPointInText atPreviousCharOfEndOfVisibleWhiteSpaces =
      textFragmentData.GetPreviousEditableCharPoint(atEndOfVisibleWhiteSpaces);
  if (!atPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() ||
      atPreviousCharOfEndOfVisibleWhiteSpaces.IsEndOfContainer() ||
      !atPreviousCharOfEndOfVisibleWhiteSpaces.IsCharCollapsibleNBSP() ||
      // If the next character of the NBSP is a preformatted linefeed, we
      // shouldn't replace it with an ASCII white-space for avoiding collapsed
      // into the linefeed.
      visibleWhiteSpaces.EndsByPreformattedLineBreak()) {
    return NS_OK;
  }

  // Next, consider the range to collapse ASCII white-spaces before there.
  EditorDOMPointInText startToDelete, endToDelete;

  const EditorDOMPointInText
      atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces =
          textFragmentData.GetPreviousEditableCharPoint(
              atPreviousCharOfEndOfVisibleWhiteSpaces);
  // If there are some preceding ASCII white-spaces, we need to treat them
  // as one white-space.  I.e., we need to collapse them.
  if (atPreviousCharOfEndOfVisibleWhiteSpaces.IsCharNBSP() &&
      atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
      atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
          .IsCharCollapsibleASCIISpace()) {
    startToDelete = textFragmentData.GetFirstASCIIWhiteSpacePointCollapsedTo(
        atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces,
        nsIEditor::eNone);
    endToDelete = atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces;
  }
  // Otherwise, we don't need to remove any white-spaces, but we may need
  // to normalize the white-space sequence containing the previous NBSP.
  else {
    startToDelete = endToDelete =
        atPreviousCharOfEndOfVisibleWhiteSpaces.NextPoint();
  }

  AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
  Result<EditorDOMPoint, nsresult> result =
      aHTMLEditor.DeleteTextAndNormalizeSurroundingWhiteSpaces(
          startToDelete, endToDelete,
          HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries,
          HTMLEditor::DeleteDirection::Forward);
  NS_WARNING_ASSERTION(
      !result.isOk(),
      "HTMLEditor::DeleteTextAndNormalizeSurroundingWhiteSpaces() failed");
  return result.isErr() ? result.unwrapErr() : NS_OK;
}

EditorDOMPointInText WSRunScanner::TextFragmentData::
    GetPreviousNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
        const EditorDOMPoint& aPointToInsert) const {
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());
  MOZ_ASSERT(VisibleWhiteSpacesDataRef().IsInitialized());
  NS_ASSERTION(VisibleWhiteSpacesDataRef().ComparePoint(aPointToInsert) ==
                       PointPosition::MiddleOfFragment ||
                   VisibleWhiteSpacesDataRef().ComparePoint(aPointToInsert) ==
                       PointPosition::EndOfFragment,
               "Previous char of aPoint should be in the visible white-spaces");

  // Try to change an NBSP to a space, if possible, just to prevent NBSP
  // proliferation.  This routine is called when we are about to make this
  // point in the ws abut an inserted break or text, so we don't have to worry
  // about what is after it.  What is after it now will end up after the
  // inserted object.
  const EditorDOMPointInText atPreviousChar =
      GetPreviousEditableCharPoint(aPointToInsert);
  if (!atPreviousChar.IsSet() || atPreviousChar.IsEndOfContainer() ||
      !atPreviousChar.IsCharNBSP() ||
      EditorUtils::IsWhiteSpacePreformatted(
          *atPreviousChar.ContainerAsText())) {
    return EditorDOMPointInText();
  }

  const EditorDOMPointInText atPreviousCharOfPreviousChar =
      GetPreviousEditableCharPoint(atPreviousChar);
  if (atPreviousCharOfPreviousChar.IsSet()) {
    // If the previous char is in different text node and it's preformatted,
    // we shouldn't touch it.
    if (atPreviousChar.ContainerAsText() !=
            atPreviousCharOfPreviousChar.ContainerAsText() &&
        EditorUtils::IsWhiteSpacePreformatted(
            *atPreviousCharOfPreviousChar.ContainerAsText())) {
      return EditorDOMPointInText();
    }
    // If the previous char of the NBSP at previous position of aPointToInsert
    // is an ASCII white-space, we don't need to replace it with same character.
    if (!atPreviousCharOfPreviousChar.IsEndOfContainer() &&
        atPreviousCharOfPreviousChar.IsCharASCIISpace()) {
      return EditorDOMPointInText();
    }
    return atPreviousChar;
  }

  // If previous content of the NBSP is block boundary, we cannot replace the
  // NBSP with an ASCII white-space to keep it rendered.
  const VisibleWhiteSpacesData& visibleWhiteSpaces =
      VisibleWhiteSpacesDataRef();
  if (!visibleWhiteSpaces.StartsFromNonCollapsibleCharacters() &&
      !visibleWhiteSpaces.StartsFromSpecialContent()) {
    return EditorDOMPointInText();
  }
  return atPreviousChar;
}

EditorDOMPointInText WSRunScanner::TextFragmentData::
    GetInclusiveNextNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
        const EditorDOMPoint& aPointToInsert) const {
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());
  MOZ_ASSERT(VisibleWhiteSpacesDataRef().IsInitialized());
  NS_ASSERTION(VisibleWhiteSpacesDataRef().ComparePoint(aPointToInsert) ==
                       PointPosition::StartOfFragment ||
                   VisibleWhiteSpacesDataRef().ComparePoint(aPointToInsert) ==
                       PointPosition::MiddleOfFragment,
               "Inclusive next char of aPointToInsert should be in the visible "
               "white-spaces");

  // Try to change an nbsp to a space, if possible, just to prevent nbsp
  // proliferation This routine is called when we are about to make this point
  // in the ws abut an inserted text, so we don't have to worry about what is
  // before it.  What is before it now will end up before the inserted text.
  auto atNextChar =
      GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(aPointToInsert);
  if (!atNextChar.IsSet() || NS_WARN_IF(atNextChar.IsEndOfContainer()) ||
      !atNextChar.IsCharNBSP() ||
      EditorUtils::IsWhiteSpacePreformatted(*atNextChar.ContainerAsText())) {
    return EditorDOMPointInText();
  }

  const auto atNextCharOfNextCharOfNBSP =
      GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(
          atNextChar.NextPoint<EditorRawDOMPointInText>());
  if (atNextCharOfNextCharOfNBSP.IsSet()) {
    // If the next char is in different text node and it's preformatted,
    // we shouldn't touch it.
    if (atNextChar.ContainerAsText() !=
            atNextCharOfNextCharOfNBSP.ContainerAsText() &&
        EditorUtils::IsWhiteSpacePreformatted(
            *atNextCharOfNextCharOfNBSP.ContainerAsText())) {
      return EditorDOMPointInText();
    }
    // If following character of an NBSP is an ASCII white-space, we don't
    // need to replace it with same character.
    if (!atNextCharOfNextCharOfNBSP.IsEndOfContainer() &&
        atNextCharOfNextCharOfNBSP.IsCharASCIISpace()) {
      return EditorDOMPointInText();
    }
    return atNextChar;
  }

  // If the NBSP is last character in the hard line, we don't need to
  // replace it because it's required to render multiple white-spaces.
  const VisibleWhiteSpacesData& visibleWhiteSpaces =
      VisibleWhiteSpacesDataRef();
  if (!visibleWhiteSpaces.EndsByNonCollapsibleCharacters() &&
      !visibleWhiteSpaces.EndsBySpecialContent() &&
      !visibleWhiteSpaces.EndsByBRElement()) {
    return EditorDOMPointInText();
  }

  return atNextChar;
}

// static
nsresult WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint) {
  MOZ_ASSERT(aPoint.IsSet());
  Element* editingHost = aHTMLEditor.ComputeEditingHost();
  TextFragmentData textFragmentData(aPoint, editingHost);
  if (NS_WARN_IF(!textFragmentData.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }
  const EditorDOMRange& leadingWhiteSpaceRange =
      textFragmentData.InvisibleLeadingWhiteSpaceRangeRef();
  // XXX Getting trailing white-space range now must be wrong because
  //     mutation event listener may invalidate it.
  const EditorDOMRange& trailingWhiteSpaceRange =
      textFragmentData.InvisibleTrailingWhiteSpaceRangeRef();
  DebugOnly<bool> leadingWhiteSpacesDeleted = false;
  if (leadingWhiteSpaceRange.IsPositioned() &&
      !leadingWhiteSpaceRange.Collapsed()) {
    nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        leadingWhiteSpaceRange.StartRef(), leadingWhiteSpaceRange.EndRef(),
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed to "
          "delete leading white-spaces");
      return rv;
    }
    leadingWhiteSpacesDeleted = true;
  }
  if (trailingWhiteSpaceRange.IsPositioned() &&
      !trailingWhiteSpaceRange.Collapsed() &&
      leadingWhiteSpaceRange != trailingWhiteSpaceRange) {
    NS_ASSERTION(!leadingWhiteSpacesDeleted,
                 "We're trying to remove trailing white-spaces with maybe "
                 "outdated range");
    nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        trailingWhiteSpaceRange.StartRef(), trailingWhiteSpaceRange.EndRef(),
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed to "
          "delete trailing white-spaces");
      return rv;
    }
  }
  return NS_OK;
}

/*****************************************************************************
 * Implementation for new white-space normalizer
 *****************************************************************************/

// static
EditorDOMRangeInTexts
WSRunScanner::ComputeRangeInTextNodesContainingInvisibleWhiteSpaces(
    const TextFragmentData& aStart, const TextFragmentData& aEnd) {
  // Corresponding to handling invisible white-spaces part of
  // `TextFragmentData::GetReplaceRangeDataAtEndOfDeletionRange()` and
  // `TextFragmentData::GetReplaceRangeDataAtStartOfDeletionRange()`

  MOZ_ASSERT(aStart.ScanStartRef().IsSetAndValid());
  MOZ_ASSERT(aEnd.ScanStartRef().IsSetAndValid());
  MOZ_ASSERT(aStart.ScanStartRef().EqualsOrIsBefore(aEnd.ScanStartRef()));
  MOZ_ASSERT(aStart.ScanStartRef().IsInTextNode());
  MOZ_ASSERT(aEnd.ScanStartRef().IsInTextNode());

  // XXX `GetReplaceRangeDataAtEndOfDeletionRange()` and
  //     `GetReplaceRangeDataAtStartOfDeletionRange()` use
  //     `GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt()` and
  //     `GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt()`.
  //     However, they are really odd as mentioned with "XXX" comments
  //     in them.  For the new white-space normalizer, we need to treat
  //     invisible white-spaces stricter because the legacy path handles
  //     white-spaces multiple times (e.g., calling `HTMLEditor::
  //     DeleteNodeIfInvisibleAndEditableTextNode()` later) and that hides
  //     the bug, but in the new path, we should stop doing same things
  //     multiple times for both performance and footprint.  Therefore,
  //     even though the result might be different in some edge cases,
  //     we should use clean path for now.  Perhaps, we should fix the odd
  //     cases before shipping `beforeinput` event in release channel.

  const EditorDOMRange& invisibleLeadingWhiteSpaceRange =
      aStart.InvisibleLeadingWhiteSpaceRangeRef();
  const EditorDOMRange& invisibleTrailingWhiteSpaceRange =
      aEnd.InvisibleTrailingWhiteSpaceRangeRef();
  const bool hasInvisibleLeadingWhiteSpaces =
      invisibleLeadingWhiteSpaceRange.IsPositioned() &&
      !invisibleLeadingWhiteSpaceRange.Collapsed();
  const bool hasInvisibleTrailingWhiteSpaces =
      invisibleLeadingWhiteSpaceRange != invisibleTrailingWhiteSpaceRange &&
      invisibleTrailingWhiteSpaceRange.IsPositioned() &&
      !invisibleTrailingWhiteSpaceRange.Collapsed();

  EditorDOMRangeInTexts result(aStart.ScanStartRef().AsInText(),
                               aEnd.ScanStartRef().AsInText());
  MOZ_ASSERT(result.IsPositionedAndValid());
  if (!hasInvisibleLeadingWhiteSpaces && !hasInvisibleTrailingWhiteSpaces) {
    return result;
  }

  MOZ_ASSERT_IF(
      hasInvisibleLeadingWhiteSpaces && hasInvisibleTrailingWhiteSpaces,
      invisibleLeadingWhiteSpaceRange.StartRef().IsBefore(
          invisibleTrailingWhiteSpaceRange.StartRef()));
  const EditorDOMPoint& aroundFirstInvisibleWhiteSpace =
      hasInvisibleLeadingWhiteSpaces
          ? invisibleLeadingWhiteSpaceRange.StartRef()
          : invisibleTrailingWhiteSpaceRange.StartRef();
  if (aroundFirstInvisibleWhiteSpace.IsBefore(result.StartRef())) {
    if (aroundFirstInvisibleWhiteSpace.IsInTextNode()) {
      result.SetStart(aroundFirstInvisibleWhiteSpace.AsInText());
      MOZ_ASSERT(result.IsPositionedAndValid());
    } else {
      const auto atFirstInvisibleWhiteSpace =
          hasInvisibleLeadingWhiteSpaces
              ? aStart.GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(
                    aroundFirstInvisibleWhiteSpace)
              : aEnd.GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(
                    aroundFirstInvisibleWhiteSpace);
      MOZ_ASSERT(atFirstInvisibleWhiteSpace.IsSet());
      MOZ_ASSERT(
          atFirstInvisibleWhiteSpace.EqualsOrIsBefore(result.StartRef()));
      result.SetStart(atFirstInvisibleWhiteSpace);
      MOZ_ASSERT(result.IsPositionedAndValid());
    }
  }
  MOZ_ASSERT_IF(
      hasInvisibleLeadingWhiteSpaces && hasInvisibleTrailingWhiteSpaces,
      invisibleLeadingWhiteSpaceRange.EndRef().IsBefore(
          invisibleTrailingWhiteSpaceRange.EndRef()));
  const EditorDOMPoint& afterLastInvisibleWhiteSpace =
      hasInvisibleTrailingWhiteSpaces
          ? invisibleTrailingWhiteSpaceRange.EndRef()
          : invisibleLeadingWhiteSpaceRange.EndRef();
  if (afterLastInvisibleWhiteSpace.EqualsOrIsBefore(result.EndRef())) {
    MOZ_ASSERT(result.IsPositionedAndValid());
    return result;
  }
  if (afterLastInvisibleWhiteSpace.IsInTextNode()) {
    result.SetEnd(afterLastInvisibleWhiteSpace.AsInText());
    MOZ_ASSERT(result.IsPositionedAndValid());
    return result;
  }
  const auto atLastInvisibleWhiteSpace =
      hasInvisibleTrailingWhiteSpaces
          ? aEnd.GetPreviousEditableCharPoint(afterLastInvisibleWhiteSpace)
          : aStart.GetPreviousEditableCharPoint(afterLastInvisibleWhiteSpace);
  MOZ_ASSERT(atLastInvisibleWhiteSpace.IsSet());
  MOZ_ASSERT(atLastInvisibleWhiteSpace.IsContainerEmpty() ||
             atLastInvisibleWhiteSpace.IsAtLastContent());
  MOZ_ASSERT(result.EndRef().EqualsOrIsBefore(atLastInvisibleWhiteSpace));
  result.SetEnd(atLastInvisibleWhiteSpace.IsEndOfContainer()
                    ? atLastInvisibleWhiteSpace
                    : atLastInvisibleWhiteSpace.NextPoint());
  MOZ_ASSERT(result.IsPositionedAndValid());
  return result;
}

// static
Result<EditorDOMRangeInTexts, nsresult>
WSRunScanner::GetRangeInTextNodesToBackspaceFrom(Element* aEditingHost,
                                                 const EditorDOMPoint& aPoint) {
  // Corresponding to computing delete range part of
  // `WhiteSpaceVisibilityKeeper::DeletePreviousWhiteSpace()`
  MOZ_ASSERT(aPoint.IsSetAndValid());

  TextFragmentData textFragmentDataAtCaret(aPoint, aEditingHost);
  if (NS_WARN_IF(!textFragmentDataAtCaret.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  EditorDOMPointInText atPreviousChar =
      textFragmentDataAtCaret.GetPreviousEditableCharPoint(aPoint);
  if (!atPreviousChar.IsSet()) {
    return EditorDOMRangeInTexts();  // There is no content in the block.
  }

  // XXX When previous char point is in an empty text node, we do nothing,
  //     but this must look odd from point of user view.  We should delete
  //     something before aPoint.
  if (atPreviousChar.IsEndOfContainer()) {
    return EditorDOMRangeInTexts();
  }

  // Extend delete range if previous char is a low surrogate following
  // a high surrogate.
  EditorDOMPointInText atNextChar = atPreviousChar.NextPoint();
  if (!atPreviousChar.IsStartOfContainer()) {
    if (atPreviousChar.IsCharLowSurrogateFollowingHighSurrogate()) {
      atPreviousChar = atPreviousChar.PreviousPoint();
    }
    // If caret is in middle of a surrogate pair, delete the surrogate pair
    // (blink-compat).
    else if (atPreviousChar.IsCharHighSurrogateFollowedByLowSurrogate()) {
      atNextChar = atNextChar.NextPoint();
    }
  }

  // If previous char is an collapsible white-spaces, delete all adjcent
  // white-spaces which are collapsed together.
  EditorDOMRangeInTexts rangeToDelete;
  if (atPreviousChar.IsCharCollapsibleASCIISpace() ||
      atPreviousChar.IsCharPreformattedNewLineCollapsedWithWhiteSpaces()) {
    const EditorDOMPointInText startToDelete =
        textFragmentDataAtCaret.GetFirstASCIIWhiteSpacePointCollapsedTo(
            atPreviousChar, nsIEditor::ePrevious);
    if (!startToDelete.IsSet()) {
      NS_WARNING(
          "WSRunScanner::GetFirstASCIIWhiteSpacePointCollapsedTo() failed");
      return Err(NS_ERROR_FAILURE);
    }
    const EditorDOMPointInText endToDelete =
        textFragmentDataAtCaret.GetEndOfCollapsibleASCIIWhiteSpaces(
            atPreviousChar, nsIEditor::ePrevious);
    if (!endToDelete.IsSet()) {
      NS_WARNING("WSRunScanner::GetEndOfCollapsibleASCIIWhiteSpaces() failed");
      return Err(NS_ERROR_FAILURE);
    }
    rangeToDelete = EditorDOMRangeInTexts(startToDelete, endToDelete);
  }
  // if previous char is not a collapsible white-space, remove it.
  else {
    rangeToDelete = EditorDOMRangeInTexts(atPreviousChar, atNextChar);
  }

  // If there is no removable and visible content, we should do nothing.
  if (rangeToDelete.Collapsed()) {
    return EditorDOMRangeInTexts();
  }

  // And also delete invisible white-spaces if they become visible.
  TextFragmentData textFragmentDataAtStart =
      rangeToDelete.StartRef() != aPoint
          ? TextFragmentData(rangeToDelete.StartRef(), aEditingHost)
          : textFragmentDataAtCaret;
  TextFragmentData textFragmentDataAtEnd =
      rangeToDelete.EndRef() != aPoint
          ? TextFragmentData(rangeToDelete.EndRef(), aEditingHost)
          : textFragmentDataAtCaret;
  if (NS_WARN_IF(!textFragmentDataAtStart.IsInitialized()) ||
      NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  EditorDOMRangeInTexts extendedRangeToDelete =
      WSRunScanner::ComputeRangeInTextNodesContainingInvisibleWhiteSpaces(
          textFragmentDataAtStart, textFragmentDataAtEnd);
  MOZ_ASSERT(extendedRangeToDelete.IsPositionedAndValid());
  return extendedRangeToDelete.IsPositioned() ? extendedRangeToDelete
                                              : rangeToDelete;
}

// static
Result<EditorDOMRangeInTexts, nsresult>
WSRunScanner::GetRangeInTextNodesToForwardDeleteFrom(
    Element* aEditingHost, const EditorDOMPoint& aPoint) {
  // Corresponding to computing delete range part of
  // `WhiteSpaceVisibilityKeeper::DeleteInclusiveNextWhiteSpace()`
  MOZ_ASSERT(aPoint.IsSetAndValid());

  TextFragmentData textFragmentDataAtCaret(aPoint, aEditingHost);
  if (NS_WARN_IF(!textFragmentDataAtCaret.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  auto atCaret =
      textFragmentDataAtCaret
          .GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(aPoint);
  if (!atCaret.IsSet()) {
    return EditorDOMRangeInTexts();  // There is no content in the block.
  }
  // If caret is in middle of a surrogate pair, we should remove next
  // character (blink-compat).
  if (!atCaret.IsEndOfContainer() &&
      atCaret.IsCharLowSurrogateFollowingHighSurrogate()) {
    atCaret = atCaret.NextPoint();
  }

  // XXX When next char point is in an empty text node, we do nothing,
  //     but this must look odd from point of user view.  We should delete
  //     something after aPoint.
  if (atCaret.IsEndOfContainer()) {
    return EditorDOMRangeInTexts();
  }

  // Extend delete range if previous char is a low surrogate following
  // a high surrogate.
  EditorDOMPointInText atNextChar = atCaret.NextPoint();
  if (atCaret.IsCharHighSurrogateFollowedByLowSurrogate()) {
    atNextChar = atNextChar.NextPoint();
  }

  // If next char is a collapsible white-space, delete all adjcent white-spaces
  // which are collapsed together.
  EditorDOMRangeInTexts rangeToDelete;
  if (atCaret.IsCharCollapsibleASCIISpace() ||
      atCaret.IsCharPreformattedNewLineCollapsedWithWhiteSpaces()) {
    const EditorDOMPointInText startToDelete =
        textFragmentDataAtCaret.GetFirstASCIIWhiteSpacePointCollapsedTo(
            atCaret, nsIEditor::eNext);
    if (!startToDelete.IsSet()) {
      NS_WARNING(
          "WSRunScanner::GetFirstASCIIWhiteSpacePointCollapsedTo() failed");
      return Err(NS_ERROR_FAILURE);
    }
    const EditorDOMPointInText endToDelete =
        textFragmentDataAtCaret.GetEndOfCollapsibleASCIIWhiteSpaces(
            atCaret, nsIEditor::eNext);
    if (!endToDelete.IsSet()) {
      NS_WARNING("WSRunScanner::GetEndOfCollapsibleASCIIWhiteSpaces() failed");
      return Err(NS_ERROR_FAILURE);
    }
    rangeToDelete = EditorDOMRangeInTexts(startToDelete, endToDelete);
  }
  // if next char is not a collapsible white-space, remove it.
  else {
    rangeToDelete = EditorDOMRangeInTexts(atCaret, atNextChar);
  }

  // If there is no removable and visible content, we should do nothing.
  if (rangeToDelete.Collapsed()) {
    return EditorDOMRangeInTexts();
  }

  // And also delete invisible white-spaces if they become visible.
  TextFragmentData textFragmentDataAtStart =
      rangeToDelete.StartRef() != aPoint
          ? TextFragmentData(rangeToDelete.StartRef(), aEditingHost)
          : textFragmentDataAtCaret;
  TextFragmentData textFragmentDataAtEnd =
      rangeToDelete.EndRef() != aPoint
          ? TextFragmentData(rangeToDelete.EndRef(), aEditingHost)
          : textFragmentDataAtCaret;
  if (NS_WARN_IF(!textFragmentDataAtStart.IsInitialized()) ||
      NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  EditorDOMRangeInTexts extendedRangeToDelete =
      WSRunScanner::ComputeRangeInTextNodesContainingInvisibleWhiteSpaces(
          textFragmentDataAtStart, textFragmentDataAtEnd);
  MOZ_ASSERT(extendedRangeToDelete.IsPositionedAndValid());
  return extendedRangeToDelete.IsPositioned() ? extendedRangeToDelete
                                              : rangeToDelete;
}

// static
EditorDOMRange WSRunScanner::GetRangesForDeletingAtomicContent(
    Element* aEditingHost, const nsIContent& aAtomicContent) {
  if (aAtomicContent.IsHTMLElement(nsGkAtoms::br)) {
    // Preceding white-spaces should be preserved, but the following
    // white-spaces should be invisible around `<br>` element.
    TextFragmentData textFragmentDataAfterBRElement(
        EditorDOMPoint::After(aAtomicContent), aEditingHost);
    if (NS_WARN_IF(!textFragmentDataAfterBRElement.IsInitialized())) {
      return EditorDOMRange();  // TODO: Make here return error with Err.
    }
    const EditorDOMRangeInTexts followingInvisibleWhiteSpaces =
        textFragmentDataAfterBRElement.GetNonCollapsedRangeInTexts(
            textFragmentDataAfterBRElement
                .InvisibleLeadingWhiteSpaceRangeRef());
    return followingInvisibleWhiteSpaces.IsPositioned() &&
                   !followingInvisibleWhiteSpaces.Collapsed()
               ? EditorDOMRange(
                     EditorDOMPoint(const_cast<nsIContent*>(&aAtomicContent)),
                     followingInvisibleWhiteSpaces.EndRef())
               : EditorDOMRange(
                     EditorDOMPoint(const_cast<nsIContent*>(&aAtomicContent)),
                     EditorDOMPoint::After(aAtomicContent));
  }

  if (!HTMLEditUtils::IsBlockElement(aAtomicContent)) {
    // Both preceding and following white-spaces around it should be preserved
    // around inline elements like `<img>`.
    return EditorDOMRange(
        EditorDOMPoint(const_cast<nsIContent*>(&aAtomicContent)),
        EditorDOMPoint::After(aAtomicContent));
  }

  // Both preceding and following white-spaces can be invisible around a
  // block element.
  TextFragmentData textFragmentDataBeforeAtomicContent(
      EditorDOMPoint(const_cast<nsIContent*>(&aAtomicContent)), aEditingHost);
  if (NS_WARN_IF(!textFragmentDataBeforeAtomicContent.IsInitialized())) {
    return EditorDOMRange();  // TODO: Make here return error with Err.
  }
  const EditorDOMRangeInTexts precedingInvisibleWhiteSpaces =
      textFragmentDataBeforeAtomicContent.GetNonCollapsedRangeInTexts(
          textFragmentDataBeforeAtomicContent
              .InvisibleTrailingWhiteSpaceRangeRef());
  TextFragmentData textFragmentDataAfterAtomicContent(
      EditorDOMPoint::After(aAtomicContent), aEditingHost);
  if (NS_WARN_IF(!textFragmentDataAfterAtomicContent.IsInitialized())) {
    return EditorDOMRange();  // TODO: Make here return error with Err.
  }
  const EditorDOMRangeInTexts followingInvisibleWhiteSpaces =
      textFragmentDataAfterAtomicContent.GetNonCollapsedRangeInTexts(
          textFragmentDataAfterAtomicContent
              .InvisibleLeadingWhiteSpaceRangeRef());
  if (precedingInvisibleWhiteSpaces.StartRef().IsSet() &&
      followingInvisibleWhiteSpaces.EndRef().IsSet()) {
    return EditorDOMRange(precedingInvisibleWhiteSpaces.StartRef(),
                          followingInvisibleWhiteSpaces.EndRef());
  }
  if (precedingInvisibleWhiteSpaces.StartRef().IsSet()) {
    return EditorDOMRange(precedingInvisibleWhiteSpaces.StartRef(),
                          EditorDOMPoint::After(aAtomicContent));
  }
  if (followingInvisibleWhiteSpaces.EndRef().IsSet()) {
    return EditorDOMRange(
        EditorDOMPoint(const_cast<nsIContent*>(&aAtomicContent)),
        followingInvisibleWhiteSpaces.EndRef());
  }
  return EditorDOMRange(
      EditorDOMPoint(const_cast<nsIContent*>(&aAtomicContent)),
      EditorDOMPoint::After(aAtomicContent));
}

// static
EditorDOMRange WSRunScanner::GetRangeForDeletingBlockElementBoundaries(
    const HTMLEditor& aHTMLEditor, const Element& aLeftBlockElement,
    const Element& aRightBlockElement,
    const EditorDOMPoint& aPointContainingTheOtherBlock) {
  MOZ_ASSERT(&aLeftBlockElement != &aRightBlockElement);
  MOZ_ASSERT_IF(
      aPointContainingTheOtherBlock.IsSet(),
      aPointContainingTheOtherBlock.GetContainer() == &aLeftBlockElement ||
          aPointContainingTheOtherBlock.GetContainer() == &aRightBlockElement);
  MOZ_ASSERT_IF(
      aPointContainingTheOtherBlock.GetContainer() == &aLeftBlockElement,
      aRightBlockElement.IsInclusiveDescendantOf(
          aPointContainingTheOtherBlock.GetChild()));
  MOZ_ASSERT_IF(
      aPointContainingTheOtherBlock.GetContainer() == &aRightBlockElement,
      aLeftBlockElement.IsInclusiveDescendantOf(
          aPointContainingTheOtherBlock.GetChild()));
  MOZ_ASSERT_IF(
      !aPointContainingTheOtherBlock.IsSet(),
      !aRightBlockElement.IsInclusiveDescendantOf(&aLeftBlockElement));
  MOZ_ASSERT_IF(
      !aPointContainingTheOtherBlock.IsSet(),
      !aLeftBlockElement.IsInclusiveDescendantOf(&aRightBlockElement));
  MOZ_ASSERT_IF(!aPointContainingTheOtherBlock.IsSet(),
                EditorRawDOMPoint(const_cast<Element*>(&aLeftBlockElement))
                    .IsBefore(EditorRawDOMPoint(
                        const_cast<Element*>(&aRightBlockElement))));

  const Element* editingHost = aHTMLEditor.ComputeEditingHost();

  EditorDOMRange range;
  // Include trailing invisible white-spaces in aLeftBlockElement.
  TextFragmentData textFragmentDataAtEndOfLeftBlockElement(
      aPointContainingTheOtherBlock.GetContainer() == &aLeftBlockElement
          ? aPointContainingTheOtherBlock
          : EditorDOMPoint::AtEndOf(const_cast<Element&>(aLeftBlockElement)),
      editingHost);
  if (NS_WARN_IF(!textFragmentDataAtEndOfLeftBlockElement.IsInitialized())) {
    return EditorDOMRange();  // TODO: Make here return error with Err.
  }
  if (textFragmentDataAtEndOfLeftBlockElement.StartsFromInvisibleBRElement()) {
    // If the left block element ends with an invisible `<br>` element,
    // it'll be deleted (and it means there is no invisible trailing
    // white-spaces).  Therefore, the range should start from the invisible
    // `<br>` element.
    range.SetStart(EditorDOMPoint(
        textFragmentDataAtEndOfLeftBlockElement.StartReasonBRElementPtr()));
  } else {
    const EditorDOMRange& trailingWhiteSpaceRange =
        textFragmentDataAtEndOfLeftBlockElement
            .InvisibleTrailingWhiteSpaceRangeRef();
    if (trailingWhiteSpaceRange.StartRef().IsSet()) {
      range.SetStart(trailingWhiteSpaceRange.StartRef());
    } else {
      range.SetStart(textFragmentDataAtEndOfLeftBlockElement.ScanStartRef());
    }
  }
  // Include leading invisible white-spaces in aRightBlockElement.
  TextFragmentData textFragmentDataAtStartOfRightBlockElement(
      aPointContainingTheOtherBlock.GetContainer() == &aRightBlockElement &&
              !aPointContainingTheOtherBlock.IsEndOfContainer()
          ? aPointContainingTheOtherBlock.NextPoint()
          : EditorDOMPoint(const_cast<Element*>(&aRightBlockElement), 0u),
      editingHost);
  if (NS_WARN_IF(!textFragmentDataAtStartOfRightBlockElement.IsInitialized())) {
    return EditorDOMRange();  // TODO: Make here return error with Err.
  }
  const EditorDOMRange& leadingWhiteSpaceRange =
      textFragmentDataAtStartOfRightBlockElement
          .InvisibleLeadingWhiteSpaceRangeRef();
  if (leadingWhiteSpaceRange.EndRef().IsSet()) {
    range.SetEnd(leadingWhiteSpaceRange.EndRef());
  } else {
    range.SetEnd(textFragmentDataAtStartOfRightBlockElement.ScanStartRef());
  }
  return range;
}

// static
EditorDOMRange
WSRunScanner::GetRangeContainingInvisibleWhiteSpacesAtRangeBoundaries(
    Element* aEditingHost, const EditorDOMRange& aRange) {
  MOZ_ASSERT(aRange.IsPositionedAndValid());
  MOZ_ASSERT(aRange.EndRef().IsSetAndValid());
  MOZ_ASSERT(aRange.StartRef().IsSetAndValid());

  EditorDOMRange result;
  TextFragmentData textFragmentDataAtStart(aRange.StartRef(), aEditingHost);
  if (NS_WARN_IF(!textFragmentDataAtStart.IsInitialized())) {
    return EditorDOMRange();  // TODO: Make here return error with Err.
  }
  const EditorDOMRangeInTexts invisibleLeadingWhiteSpacesAtStart =
      textFragmentDataAtStart.GetNonCollapsedRangeInTexts(
          textFragmentDataAtStart.InvisibleLeadingWhiteSpaceRangeRef());
  if (invisibleLeadingWhiteSpacesAtStart.IsPositioned() &&
      !invisibleLeadingWhiteSpacesAtStart.Collapsed()) {
    result.SetStart(invisibleLeadingWhiteSpacesAtStart.StartRef());
  } else {
    const EditorDOMRangeInTexts invisibleTrailingWhiteSpacesAtStart =
        textFragmentDataAtStart.GetNonCollapsedRangeInTexts(
            textFragmentDataAtStart.InvisibleTrailingWhiteSpaceRangeRef());
    if (invisibleTrailingWhiteSpacesAtStart.IsPositioned() &&
        !invisibleTrailingWhiteSpacesAtStart.Collapsed()) {
      MOZ_ASSERT(
          invisibleTrailingWhiteSpacesAtStart.StartRef().EqualsOrIsBefore(
              aRange.StartRef()));
      result.SetStart(invisibleTrailingWhiteSpacesAtStart.StartRef());
    }
    // If there is no invisible white-space and the line starts with a
    // text node, shrink the range to start of the text node.
    else if (!aRange.StartRef().IsInTextNode() &&
             textFragmentDataAtStart.StartsFromBlockBoundary() &&
             textFragmentDataAtStart.EndRef().IsInTextNode()) {
      result.SetStart(textFragmentDataAtStart.EndRef());
    }
  }
  if (!result.StartRef().IsSet()) {
    result.SetStart(aRange.StartRef());
  }

  TextFragmentData textFragmentDataAtEnd(aRange.EndRef(), aEditingHost);
  if (NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized())) {
    return EditorDOMRange();  // TODO: Make here return error with Err.
  }
  const EditorDOMRangeInTexts invisibleLeadingWhiteSpacesAtEnd =
      textFragmentDataAtEnd.GetNonCollapsedRangeInTexts(
          textFragmentDataAtEnd.InvisibleTrailingWhiteSpaceRangeRef());
  if (invisibleLeadingWhiteSpacesAtEnd.IsPositioned() &&
      !invisibleLeadingWhiteSpacesAtEnd.Collapsed()) {
    result.SetEnd(invisibleLeadingWhiteSpacesAtEnd.EndRef());
  } else {
    const EditorDOMRangeInTexts invisibleLeadingWhiteSpacesAtEnd =
        textFragmentDataAtEnd.GetNonCollapsedRangeInTexts(
            textFragmentDataAtEnd.InvisibleLeadingWhiteSpaceRangeRef());
    if (invisibleLeadingWhiteSpacesAtEnd.IsPositioned() &&
        !invisibleLeadingWhiteSpacesAtEnd.Collapsed()) {
      MOZ_ASSERT(aRange.EndRef().EqualsOrIsBefore(
          invisibleLeadingWhiteSpacesAtEnd.EndRef()));
      result.SetEnd(invisibleLeadingWhiteSpacesAtEnd.EndRef());
    }
    // If there is no invisible white-space and the line ends with a text
    // node, shrink the range to end of the text node.
    else if (!aRange.EndRef().IsInTextNode() &&
             textFragmentDataAtEnd.EndsByBlockBoundary() &&
             textFragmentDataAtEnd.StartRef().IsInTextNode()) {
      result.SetEnd(EditorDOMPoint::AtEndOf(
          *textFragmentDataAtEnd.StartRef().ContainerAsText()));
    }
  }
  if (!result.EndRef().IsSet()) {
    result.SetEnd(aRange.EndRef());
  }
  MOZ_ASSERT(result.IsPositionedAndValid());
  return result;
}

/******************************************************************************
 * Utilities for other things.
 ******************************************************************************/

// static
Result<bool, nsresult>
WSRunScanner::ShrinkRangeIfStartsFromOrEndsAfterAtomicContent(
    const HTMLEditor& aHTMLEditor, nsRange& aRange,
    const Element* aEditingHost) {
  MOZ_ASSERT(aRange.IsPositioned());
  MOZ_ASSERT(!aRange.IsInSelection(),
             "Changing range in selection may cause running script");

  if (NS_WARN_IF(!aRange.GetStartContainer()) ||
      NS_WARN_IF(!aRange.GetEndContainer())) {
    return Err(NS_ERROR_FAILURE);
  }

  if (!aRange.GetStartContainer()->IsContent() ||
      !aRange.GetEndContainer()->IsContent()) {
    return false;
  }

  // If the range crosses a block boundary, we should do nothing for now
  // because it hits a bug of inserting a padding `<br>` element after
  // joining the blocks.
  if (HTMLEditUtils::GetInclusiveAncestorElement(
          *aRange.GetStartContainer()->AsContent(),
          HTMLEditUtils::ClosestEditableBlockElementExceptHRElement) !=
      HTMLEditUtils::GetInclusiveAncestorElement(
          *aRange.GetEndContainer()->AsContent(),
          HTMLEditUtils::ClosestEditableBlockElementExceptHRElement)) {
    return false;
  }

  nsIContent* startContent = nullptr;
  if (aRange.GetStartContainer() && aRange.GetStartContainer()->IsText() &&
      aRange.GetStartContainer()->AsText()->Length() == aRange.StartOffset()) {
    // If next content is a visible `<br>` element, special inline content
    // (e.g., `<img>`, non-editable text node, etc) or a block level void
    // element like `<hr>`, the range should start with it.
    TextFragmentData textFragmentDataAtStart(
        EditorRawDOMPoint(aRange.StartRef()), aEditingHost);
    if (NS_WARN_IF(!textFragmentDataAtStart.IsInitialized())) {
      return Err(NS_ERROR_FAILURE);
    }
    if (textFragmentDataAtStart.EndsByVisibleBRElement()) {
      startContent = textFragmentDataAtStart.EndReasonBRElementPtr();
    } else if (textFragmentDataAtStart.EndsBySpecialContent() ||
               (textFragmentDataAtStart.EndsByOtherBlockElement() &&
                !HTMLEditUtils::IsContainerNode(
                    *textFragmentDataAtStart
                         .EndReasonOtherBlockElementPtr()))) {
      startContent = textFragmentDataAtStart.GetEndReasonContent();
    }
  }

  nsIContent* endContent = nullptr;
  if (aRange.GetEndContainer() && aRange.GetEndContainer()->IsText() &&
      !aRange.EndOffset()) {
    // If previous content is a visible `<br>` element, special inline content
    // (e.g., `<img>`, non-editable text node, etc) or a block level void
    // element like `<hr>`, the range should end after it.
    TextFragmentData textFragmentDataAtEnd(EditorRawDOMPoint(aRange.EndRef()),
                                           aEditingHost);
    if (NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized())) {
      return Err(NS_ERROR_FAILURE);
    }
    if (textFragmentDataAtEnd.StartsFromVisibleBRElement()) {
      endContent = textFragmentDataAtEnd.StartReasonBRElementPtr();
    } else if (textFragmentDataAtEnd.StartsFromSpecialContent() ||
               (textFragmentDataAtEnd.StartsFromOtherBlockElement() &&
                !HTMLEditUtils::IsContainerNode(
                    *textFragmentDataAtEnd
                         .StartReasonOtherBlockElementPtr()))) {
      endContent = textFragmentDataAtEnd.GetStartReasonContent();
    }
  }

  if (!startContent && !endContent) {
    return false;
  }

  nsresult rv = aRange.SetStartAndEnd(
      startContent ? RangeBoundary(
                         startContent->GetParentNode(),
                         startContent->GetPreviousSibling())  // at startContent
                   : aRange.StartRef(),
      endContent ? RangeBoundary(endContent->GetParentNode(),
                                 endContent)  // after endContent
                 : aRange.EndRef());
  if (NS_FAILED(rv)) {
    NS_WARNING("nsRange::SetStartAndEnd() failed");
    return Err(rv);
  }
  return true;
}

}  // namespace mozilla
