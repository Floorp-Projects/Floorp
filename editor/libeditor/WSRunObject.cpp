/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WSRunObject.h"

#include "HTMLEditUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/mozalloc.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/RangeUtils.h"
#include "mozilla/SelectionState.h"
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

const char16_t kNBSP = 160;

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

nsresult WhiteSpaceVisibilityKeeper::PrepareToSplitAcrossBlocks(
    HTMLEditor& aHTMLEditor, nsCOMPtr<nsINode>* aSplitNode,
    uint32_t* aSplitOffset) {
  if (NS_WARN_IF(!aSplitNode) || NS_WARN_IF(!*aSplitNode) ||
      NS_WARN_IF(!aSplitOffset)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(), aSplitNode,
                            aSplitOffset);

  nsresult rv = WhiteSpaceVisibilityKeeper::
      MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit(
          aHTMLEditor, EditorDOMPoint(*aSplitNode, *aSplitOffset));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "WhiteSpaceVisibilityKeeper::"
                       "MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit() "
                       "failed");
  return rv;
}

// static
EditActionResult WhiteSpaceVisibilityKeeper::
    MergeFirstLineOfRightBlockElementIntoDescendantLeftBlockElement(
        HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
        Element& aRightBlockElement, const EditorDOMPoint& aAtRightBlockChild,
        const Maybe<nsAtom*>& aListElementTagName,
        const HTMLBRElement* aPrecedingInvisibleBRElement) {
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
          aHTMLEditor.GetActiveEditingHost(),
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
    // XXX Why do we ignore the result of MoveOneHardLineContents()?
    NS_ASSERTION(rightBlockElement == afterRightBlockChild.GetContainer(),
                 "The relation is not guaranteed but assumed");
#ifdef DEBUG
    Result<bool, nsresult> firstLineHasContent =
        aHTMLEditor.CanMoveOrDeleteSomethingInHardLine(EditorRawDOMPoint(
            rightBlockElement, afterRightBlockChild.Offset()));
#endif  // #ifdef DEBUG
    MoveNodeResult moveNodeResult = aHTMLEditor.MoveOneHardLineContents(
        EditorDOMPoint(rightBlockElement, afterRightBlockChild.Offset()),
        EditorDOMPoint(&aLeftBlockElement, 0),
        HTMLEditor::MoveToEndOfContainer::Yes);
    if (NS_WARN_IF(moveNodeResult.EditorDestroyed())) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(moveNodeResult.Succeeded(),
                         "HTMLEditor::MoveOneHardLineContents("
                         "MoveToEndOfContainer::Yes) failed, but ignored");
    if (moveNodeResult.Succeeded()) {
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
      ret |= moveNodeResult;
    }
    // Now, all children of rightBlockElement were moved to leftBlockElement.
    // So, afterRightBlockChild is now invalid.
    afterRightBlockChild.Clear();
  }

  if (!invisibleBRElementAtEndOfLeftBlockElement) {
    return ret;
  }

  rv = aHTMLEditor.DeleteNodeWithTransaction(
      *invisibleBRElementAtEndOfLeftBlockElement);
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed, but ignored");
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
        const HTMLBRElement* aPrecedingInvisibleBRElement) {
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
    // XXX AutoTrackDOMPoint instance, tracker, hasn't been destroyed here.
    //     Do we really need to do update aRightBlockElement here??
    // XXX And atLeftBlockChild.GetContainerAsElement() always returns
    //     an element pointer so that probably here should not use
    //     accessors of EditorDOMPoint, should use DOM API directly instead.
    if (atLeftBlockChild.GetContainerAsElement()) {
      leftBlockElement = *atLeftBlockChild.GetContainerAsElement();
    } else if (NS_WARN_IF(!atLeftBlockChild.GetContainerParentAsElement())) {
      return EditActionResult(NS_ERROR_UNEXPECTED);
    } else {
      leftBlockElement = *atLeftBlockChild.GetContainerParentAsElement();
    }
  }

  // Do br adjustment.
  RefPtr<HTMLBRElement> invisibleBRElementBeforeLeftBlockElement =
      WSRunScanner::GetPrecedingBRElementUnlessVisibleContentFound(
          aHTMLEditor.GetActiveEditingHost(), atLeftBlockChild);
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
        moveNodeResult.Succeeded(),
        "HTMLEditor::MoveChildrenWithTransaction() failed, but ignored");
    if (moveNodeResult.Succeeded()) {
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
      // We are working with valid HTML, aLeftContentInBlock is a block node,
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

    MOZ_ASSERT(atPreviousContent.IsSet());

    // Because we don't want the moving content to receive the style of the
    // previous content, we split the previous content's style.

#ifdef DEBUG
    Result<bool, nsresult> firstLineHasContent =
        aHTMLEditor.CanMoveOrDeleteSomethingInHardLine(
            EditorRawDOMPoint(&aRightBlockElement, 0));
#endif  // #ifdef DEBUG

    Element* editingHost = aHTMLEditor.GetActiveEditingHost();
    // XXX It's odd to continue handling this edit action if there is no
    //     editing host.
    if (!editingHost || &aLeftContentInBlock != editingHost) {
      SplitNodeResult splitResult =
          aHTMLEditor.SplitAncestorStyledInlineElementsAt(atPreviousContent,
                                                          nullptr, nullptr);
      if (splitResult.Failed()) {
        NS_WARNING("HTMLEditor::SplitAncestorStyledInlineElementsAt() failed");
        return EditActionResult(splitResult.Rv());
      }

      if (splitResult.Handled()) {
        if (splitResult.GetNextNode()) {
          atPreviousContent.Set(splitResult.GetNextNode());
          if (!atPreviousContent.IsSet()) {
            NS_WARNING("Next node of split point was orphaned");
            return EditActionResult(NS_ERROR_NULL_POINTER);
          }
        } else {
          atPreviousContent = splitResult.SplitPoint();
          if (!atPreviousContent.IsSet()) {
            NS_WARNING("Split node was orphaned");
            return EditActionResult(NS_ERROR_NULL_POINTER);
          }
        }
      }
    }

    MoveNodeResult moveNodeResult = aHTMLEditor.MoveOneHardLineContents(
        EditorDOMPoint(&aRightBlockElement, 0), atPreviousContent);
    if (moveNodeResult.Failed()) {
      NS_WARNING("HTMLEditor::MoveOneHardLineContents() failed");
      return EditActionResult(moveNodeResult.Rv());
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

    ret |= moveNodeResult;
  }

  if (!invisibleBRElementBeforeLeftBlockElement) {
    return ret;
  }

  rv = aHTMLEditor.DeleteNodeWithTransaction(
      *invisibleBRElementBeforeLeftBlockElement);
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed, but ignored");
    return EditActionResult(rv);
  }
  return EditActionHandled();
}

// static
EditActionResult WhiteSpaceVisibilityKeeper::
    MergeFirstLineOfRightBlockElementIntoLeftBlockElement(
        HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
        Element& aRightBlockElement, const Maybe<nsAtom*>& aListElementTagName,
        const HTMLBRElement* aPrecedingInvisibleBRElement) {
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
          aHTMLEditor.GetActiveEditingHost(),
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
          convertListTypeResult.Succeeded(),
          "HTMLEditor::ChangeListElementType() failed, but ignored");
    }
    ret.MarkAsHandled();
  } else {
#ifdef DEBUG
    Result<bool, nsresult> firstLineHasContent =
        aHTMLEditor.CanMoveOrDeleteSomethingInHardLine(
            EditorRawDOMPoint(&aRightBlockElement, 0));
#endif  // #ifdef DEBUG

    // Nodes are dissimilar types.
    MoveNodeResult moveNodeResult = aHTMLEditor.MoveOneHardLineContents(
        EditorDOMPoint(&aRightBlockElement, 0),
        EditorDOMPoint(&aLeftBlockElement, 0),
        HTMLEditor::MoveToEndOfContainer::Yes);
    if (moveNodeResult.Failed()) {
      NS_WARNING(
          "HTMLEditor::MoveOneHardLineContents(MoveToEndOfContainer::Yes) "
          "failed");
      return EditActionResult(moveNodeResult.Rv());
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
    ret |= moveNodeResult;
  }

  if (!invisibleBRElementAtEndOfLeftBlockElement) {
    return ret.MarkAsHandled();
  }

  rv = aHTMLEditor.DeleteNodeWithTransaction(
      *invisibleBRElementAtEndOfLeftBlockElement);
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    return ret.SetResult(NS_ERROR_EDITOR_DESTROYED);
  }
  // XXX In other top level if blocks, the result of
  //     DeleteNodeWithTransaction() is ignored.  Why does only this result
  //     is respected?
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
    return EditActionResult(rv);
  }
  return EditActionHandled();
}

// static
Result<RefPtr<Element>, nsresult> WhiteSpaceVisibilityKeeper::InsertBRElement(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToInsert) {
  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return Err(NS_ERROR_INVALID_ARG);
  }

  // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
  // meanwhile, the pre case is handled in HandleInsertText() in
  // HTMLEditSubActionHandler.cpp

  Element* editingHost = aHTMLEditor.GetActiveEditingHost();
  TextFragmentData textFragmentDataAtInsertionPoint(aPointToInsert,
                                                    editingHost);
  if (NS_WARN_IF(!textFragmentDataAtInsertionPoint.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  const EditorDOMRange invisibleLeadingWhiteSpaceRangeOfNewLine =
      textFragmentDataAtInsertionPoint
          .GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(aPointToInsert);
  const EditorDOMRange invisibleTrailingWhiteSpaceRangeOfCurrentLine =
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
  {
    // Some scoping for AutoTrackDOMPoint.  This will track our insertion
    // point while we tweak any surrounding white-space
    AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(), &pointToInsert);

    if (invisibleTrailingWhiteSpaceRangeOfCurrentLine.IsPositioned()) {
      if (!invisibleTrailingWhiteSpaceRangeOfCurrentLine.Collapsed()) {
        // XXX Why don't we remove all of the invisible white-spaces?
        MOZ_ASSERT(invisibleTrailingWhiteSpaceRangeOfCurrentLine.StartRef() ==
                   pointToInsert);
        nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            invisibleTrailingWhiteSpaceRangeOfCurrentLine.StartRef(),
            invisibleTrailingWhiteSpaceRangeOfCurrentLine.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return Err(rv);
        }
      }
    }
    // If new line will start with visible white-spaces, it needs to be start
    // with an NBSP.
    else if (pointPositionWithVisibleWhiteSpaces ==
                 PointPosition::StartOfFragment ||
             pointPositionWithVisibleWhiteSpaces ==
                 PointPosition::MiddleOfFragment) {
      EditorRawDOMPointInText atNextCharOfInsertionPoint =
          textFragmentDataAtInsertionPoint.GetInclusiveNextEditableCharPoint(
              pointToInsert);
      if (atNextCharOfInsertionPoint.IsSet() &&
          !atNextCharOfInsertionPoint.IsEndOfContainer() &&
          atNextCharOfInsertionPoint.IsCharASCIISpace() &&
          !EditorUtils::IsContentPreformatted(
              *atNextCharOfInsertionPoint.ContainerAsText())) {
        EditorRawDOMPointInText atPreviousCharOfNextCharOfInsertionPoint =
            textFragmentDataAtInsertionPoint.GetPreviousEditableCharPoint(
                atNextCharOfInsertionPoint);
        if (!atPreviousCharOfNextCharOfInsertionPoint.IsSet() ||
            atPreviousCharOfNextCharOfInsertionPoint.IsEndOfContainer() ||
            !atPreviousCharOfNextCharOfInsertionPoint.IsCharASCIISpace()) {
          // We are at start of non-nbsps.  Convert to a single nbsp.
          EditorRawDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
              textFragmentDataAtInsertionPoint
                  .GetEndOfCollapsibleASCIIWhiteSpaces(
                      atNextCharOfInsertionPoint);
          nsresult rv =
              WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
                  aHTMLEditor,
                  EditorDOMRangeInTexts(atNextCharOfInsertionPoint,
                                        endOfCollapsibleASCIIWhiteSpaces),
                  nsDependentSubstring(&kNBSP, 1));
          if (NS_FAILED(rv)) {
            NS_WARNING(
                "WhiteSpaceVisibilityKeeper::"
                "ReplaceTextAndRemoveEmptyTextNodes() failed");
            return Err(rv);
          }
        }
      }
    }

    if (invisibleLeadingWhiteSpaceRangeOfNewLine.IsPositioned()) {
      if (!invisibleLeadingWhiteSpaceRangeOfNewLine.Collapsed()) {
        // XXX Why don't we remove all of the invisible white-spaces?
        MOZ_ASSERT(invisibleLeadingWhiteSpaceRangeOfNewLine.EndRef() ==
                   pointToInsert);
        // XXX If the DOM tree has been changed above,
        //     invisibleLeadingWhiteSpaceRangeOfNewLine may be invalid now.
        //     So, we may do something wrong here.
        nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            invisibleLeadingWhiteSpaceRangeOfNewLine.StartRef(),
            invisibleLeadingWhiteSpaceRangeOfNewLine.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "WhiteSpaceVisibilityKeeper::"
              "DeleteTextAndTextNodesWithTransaction() failed");
          return Err(rv);
        }
      }
    }
    // If the `<br>` element is put immediately after an NBSP, it should be
    // replaced with an ASCII white-space.
    else if (pointPositionWithVisibleWhiteSpaces ==
                 PointPosition::MiddleOfFragment ||
             pointPositionWithVisibleWhiteSpaces ==
                 PointPosition::EndOfFragment) {
      // XXX If the DOM tree has been changed above, pointToInsert` and/or
      //     `visibleWhiteSpaces` may be invalid.  So, we may do
      //     something wrong here.
      EditorDOMPointInText atNBSPReplacedWithASCIIWhiteSpace =
          textFragmentDataAtInsertionPoint
              .GetPreviousNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
                  pointToInsert);
      if (atNBSPReplacedWithASCIIWhiteSpace.IsSet()) {
        AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
        nsresult rv = aHTMLEditor.ReplaceTextWithTransaction(
            MOZ_KnownLive(*atNBSPReplacedWithASCIIWhiteSpace.ContainerAsText()),
            atNBSPReplacedWithASCIIWhiteSpace.Offset(), 1, u" "_ns);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed failed");
          return Err(rv);
        }
      }
    }
  }

  Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
      aHTMLEditor.InsertBRElementWithTransaction(pointToInsert,
                                                 nsIEditor::eNone);
  NS_WARNING_ASSERTION(
      resultOfInsertingBRElement.isOk(),
      "HTMLEditor::InsertBRElementWithTransaction(eNone) failed");
  MOZ_ASSERT_IF(resultOfInsertingBRElement.isOk(),
                resultOfInsertingBRElement.inspect());
  return resultOfInsertingBRElement;
}

// static
nsresult WhiteSpaceVisibilityKeeper::ReplaceText(
    HTMLEditor& aHTMLEditor, const nsAString& aStringToInsert,
    const EditorDOMRange& aRangeToBeReplaced,
    EditorRawDOMPoint* aPointAfterInsertedString /* = nullptr */) {
  // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
  // meanwhile, the pre case is handled in HandleInsertText() in
  // HTMLEditSubActionHandler.cpp

  // MOOSE: for now, just getting the ws logic straight.  This implementation
  // is very slow.  Will need to replace edit rules impl with a more efficient
  // text sink here that does the minimal amount of searching/replacing/copying

  if (aStringToInsert.IsEmpty()) {
    MOZ_ASSERT(aRangeToBeReplaced.Collapsed());
    if (aPointAfterInsertedString) {
      *aPointAfterInsertedString = aRangeToBeReplaced.StartRef();
    }
    return NS_OK;
  }

  RefPtr<Element> editingHost = aHTMLEditor.GetActiveEditingHost();
  TextFragmentData textFragmentDataAtStart(aRangeToBeReplaced.StartRef(),
                                           editingHost);
  if (NS_WARN_IF(!textFragmentDataAtStart.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }
  const bool isInsertionPointEqualsOrIsBeforeStartOfText =
      aRangeToBeReplaced.StartRef().EqualsOrIsBefore(
          textFragmentDataAtStart.StartRef());
  TextFragmentData textFragmentDataAtEnd =
      aRangeToBeReplaced.Collapsed()
          ? textFragmentDataAtStart
          : TextFragmentData(aRangeToBeReplaced.EndRef(), editingHost);
  if (NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }
  const bool isInsertionPointEqualsOrAfterEndOfText =
      textFragmentDataAtEnd.EndRef().EqualsOrIsBefore(
          aRangeToBeReplaced.EndRef());

  const EditorDOMRange invisibleLeadingWhiteSpaceRangeAtStart =
      textFragmentDataAtStart
          .GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(
              aRangeToBeReplaced.StartRef());
  const EditorDOMRange invisibleTrailingWhiteSpaceRangeAtEnd =
      textFragmentDataAtEnd.GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt(
          aRangeToBeReplaced.EndRef());
  const Maybe<const VisibleWhiteSpacesData> visibleWhiteSpacesAtStart =
      !invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned()
          ? Some(textFragmentDataAtStart.VisibleWhiteSpacesDataRef())
          : Nothing();
  const PointPosition pointPositionWithVisibleWhiteSpacesAtStart =
      visibleWhiteSpacesAtStart.isSome() &&
              visibleWhiteSpacesAtStart.ref().IsInitialized()
          ? visibleWhiteSpacesAtStart.ref().ComparePoint(
                aRangeToBeReplaced.StartRef())
          : PointPosition::NotInSameDOMTree;
  const Maybe<const VisibleWhiteSpacesData> visibleWhiteSpacesAtEnd =
      !invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned()
          ? Some(textFragmentDataAtEnd.VisibleWhiteSpacesDataRef())
          : Nothing();
  const PointPosition pointPositionWithVisibleWhiteSpacesAtEnd =
      visibleWhiteSpacesAtEnd.isSome() &&
              visibleWhiteSpacesAtEnd.ref().IsInitialized()
          ? visibleWhiteSpacesAtEnd.ref().ComparePoint(
                aRangeToBeReplaced.EndRef())
          : PointPosition::NotInSameDOMTree;

  EditorDOMPoint pointToInsert(aRangeToBeReplaced.StartRef());
  nsAutoString theString(aStringToInsert);
  {
    // Some scoping for AutoTrackDOMPoint.  This will track our insertion
    // point while we tweak any surrounding white-space
    AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(), &pointToInsert);

    if (invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned()) {
      if (!invisibleTrailingWhiteSpaceRangeAtEnd.Collapsed()) {
        // XXX Why don't we remove all of the invisible white-spaces?
        MOZ_ASSERT(invisibleTrailingWhiteSpaceRangeAtEnd.StartRef() ==
                   pointToInsert);
        nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            invisibleTrailingWhiteSpaceRangeAtEnd.StartRef(),
            invisibleTrailingWhiteSpaceRangeAtEnd.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return rv;
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
        AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
        nsresult rv = aHTMLEditor.ReplaceTextWithTransaction(
            MOZ_KnownLive(*atNBSPReplacedWithASCIIWhiteSpace.ContainerAsText()),
            atNBSPReplacedWithASCIIWhiteSpace.Offset(), 1, u" "_ns);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
          return rv;
        }
      }
    }

    if (invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned()) {
      if (!invisibleLeadingWhiteSpaceRangeAtStart.Collapsed()) {
        // XXX Why don't we remove all of the invisible white-spaces?
        MOZ_ASSERT(invisibleLeadingWhiteSpaceRangeAtStart.EndRef() ==
                   pointToInsert);
        // XXX If the DOM tree has been changed above,
        //     invisibleLeadingWhiteSpaceRangeAtStart may be invalid now.
        //     So, we may do something wrong here.
        nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            invisibleLeadingWhiteSpaceRangeAtStart.StartRef(),
            invisibleLeadingWhiteSpaceRangeAtStart.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return rv;
        }
      }
    }
    // Replace an NBSP at previous character of insertion point to an ASCII
    // white-space if inserting into a visible white-space sequence.
    // XXX With modifying the inserting string later, this creates a line break
    //     opportunity before the inserting string, but this causes
    //     inconsistent result with inserting order.  E.g., type white-space
    //     n times with various order.
    else if (pointPositionWithVisibleWhiteSpacesAtStart ==
                 PointPosition::MiddleOfFragment ||
             pointPositionWithVisibleWhiteSpacesAtStart ==
                 PointPosition::EndOfFragment) {
      // XXX If the DOM tree has been changed above, pointToInsert` and/or
      //     `visibleWhiteSpaces` may be invalid.  So, we may do
      //     something wrong here.
      EditorDOMPointInText atNBSPReplacedWithASCIIWhiteSpace =
          textFragmentDataAtStart
              .GetPreviousNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
                  pointToInsert);
      if (atNBSPReplacedWithASCIIWhiteSpace.IsSet()) {
        AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
        nsresult rv = aHTMLEditor.ReplaceTextWithTransaction(
            MOZ_KnownLive(*atNBSPReplacedWithASCIIWhiteSpace.ContainerAsText()),
            atNBSPReplacedWithASCIIWhiteSpace.Offset(), 1, u" "_ns);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed failed");
          return rv;
        }
      }
    }

    // After this block, pointToInsert is modified by AutoTrackDOMPoint.
  }

  // Next up, tweak head and tail of string as needed.  First the head: there
  // are a variety of circumstances that would require us to convert a leading
  // ws char into an nbsp:

  if (nsCRT::IsAsciiSpace(theString[0])) {
    // If inserting string will follow some invisible leading white-spaces, the
    // string needs to start with an NBSP.
    if (invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned()) {
      theString.SetCharAt(kNBSP, 0);
    }
    // If inserting around visible white-spaces, check whether the previous
    // character of insertion point is an NBSP or an ASCII white-space.
    else if (pointPositionWithVisibleWhiteSpacesAtStart ==
                 PointPosition::MiddleOfFragment ||
             pointPositionWithVisibleWhiteSpacesAtStart ==
                 PointPosition::EndOfFragment) {
      EditorDOMPointInText atPreviousChar =
          textFragmentDataAtStart.GetPreviousEditableCharPoint(pointToInsert);
      if (atPreviousChar.IsSet() && !atPreviousChar.IsEndOfContainer() &&
          atPreviousChar.IsCharASCIISpace()) {
        theString.SetCharAt(kNBSP, 0);
      }
    }
    // If the insertion point is (was) before the start of text and it's
    // immediately after a hard line break, the first ASCII white-space should
    // be replaced with an NBSP for making it visible.
    else if (textFragmentDataAtStart.StartsFromHardLineBreak() &&
             isInsertionPointEqualsOrIsBeforeStartOfText) {
      theString.SetCharAt(kNBSP, 0);
    }
  }

  // Then the tail
  uint32_t lastCharIndex = theString.Length() - 1;

  if (nsCRT::IsAsciiSpace(theString[lastCharIndex])) {
    // If inserting string will be followed by some invisible trailing
    // white-spaces, the string needs to end with an NBSP.
    if (invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned()) {
      theString.SetCharAt(kNBSP, lastCharIndex);
    }
    // If inserting around visible white-spaces, check whether the inclusive
    // next character of end of replaced range is an NBSP or an ASCII
    // white-space.
    if (pointPositionWithVisibleWhiteSpacesAtEnd ==
            PointPosition::StartOfFragment ||
        pointPositionWithVisibleWhiteSpacesAtEnd ==
            PointPosition::MiddleOfFragment) {
      EditorDOMPointInText atNextChar =
          textFragmentDataAtEnd.GetInclusiveNextEditableCharPoint(
              pointToInsert);
      if (atNextChar.IsSet() && !atNextChar.IsEndOfContainer() &&
          atNextChar.IsCharASCIISpace()) {
        theString.SetCharAt(kNBSP, lastCharIndex);
      }
    }
    // If the end of replacing range is (was) after the end of text and it's
    // immediately before block boundary, the last ASCII white-space should
    // be replaced with an NBSP for making it visible.
    else if (textFragmentDataAtEnd.EndsByBlockBoundary() &&
             isInsertionPointEqualsOrAfterEndOfText) {
      theString.SetCharAt(kNBSP, lastCharIndex);
    }
  }

  // Next, scan string for adjacent ws and convert to nbsp/space combos
  // MOOSE: don't need to convert tabs here since that is done by
  // WillInsertText() before we are called.  Eventually, all that logic will be
  // pushed down into here and made more efficient.
  bool prevWS = false;
  for (uint32_t i = 0; i <= lastCharIndex; i++) {
    if (nsCRT::IsAsciiSpace(theString[i])) {
      if (prevWS) {
        // i - 1 can't be negative because prevWS starts out false
        theString.SetCharAt(kNBSP, i - 1);
      } else {
        prevWS = true;
      }
    } else {
      prevWS = false;
    }
  }

  // XXX If the point is not editable, InsertTextWithTransaction() returns
  //     error, but we keep handling it.  But I think that it wastes the
  //     runtime cost.  So, perhaps, we should return error code which couldn't
  //     modify it and make each caller of this method decide whether it should
  //     keep or stop handling the edit action.
  if (!aHTMLEditor.GetDocument()) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::ReplaceText() lost proper document");
    return NS_ERROR_UNEXPECTED;
  }
  OwningNonNull<Document> document = *aHTMLEditor.GetDocument();
  nsresult rv = aHTMLEditor.InsertTextWithTransaction(
      document, theString, pointToInsert, aPointAfterInsertedString);
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_SUCCEEDED(rv)) {
    return NS_OK;
  }

  NS_WARNING("HTMLEditor::InsertTextWithTransaction() failed, but ignored");

  // XXX Temporarily, set new insertion point to the original point.
  if (aPointAfterInsertedString) {
    *aPointAfterInsertedString = pointToInsert;
  }
  return NS_OK;
}

// static
nsresult WhiteSpaceVisibilityKeeper::DeletePreviousWhiteSpace(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint) {
  Element* editingHost = aHTMLEditor.GetActiveEditingHost();
  TextFragmentData textFragmentDataAtDeletion(aPoint, editingHost);
  if (NS_WARN_IF(!textFragmentDataAtDeletion.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }
  EditorDOMPointInText atPreviousCharOfStart =
      textFragmentDataAtDeletion.GetPreviousEditableCharPoint(aPoint);
  if (!atPreviousCharOfStart.IsSet() ||
      atPreviousCharOfStart.IsEndOfContainer()) {
    return NS_OK;
  }

  // Easy case, preformatted ws.
  if (EditorUtils::IsContentPreformatted(
          *atPreviousCharOfStart.ContainerAsText())) {
    if (!atPreviousCharOfStart.IsCharASCIISpace() &&
        !atPreviousCharOfStart.IsCharNBSP()) {
      return NS_OK;
    }
    nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        atPreviousCharOfStart, atPreviousCharOfStart.NextPoint(),
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  // Caller's job to ensure that previous char is really ws.  If it is normal
  // ws, we need to delete the whole run.
  if (atPreviousCharOfStart.IsCharASCIISpace()) {
    EditorDOMPoint startToDelete =
        textFragmentDataAtDeletion.GetFirstASCIIWhiteSpacePointCollapsedTo(
            atPreviousCharOfStart);
    EditorDOMPoint endToDelete =
        textFragmentDataAtDeletion.GetEndOfCollapsibleASCIIWhiteSpaces(
            atPreviousCharOfStart);
    nsresult rv =
        WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
            aHTMLEditor, &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
          "failed");
      return rv;
    }

    // finally, delete that ws
    rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        startToDelete, endToDelete,
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  if (atPreviousCharOfStart.IsCharNBSP()) {
    EditorDOMPoint startToDelete(atPreviousCharOfStart);
    EditorDOMPoint endToDelete(startToDelete.NextPoint());
    nsresult rv =
        WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
            aHTMLEditor, &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
          "failed");
      return rv;
    }

    // finally, delete that ws
    rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        startToDelete, endToDelete,
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  return NS_OK;
}

// static
nsresult WhiteSpaceVisibilityKeeper::DeleteInclusiveNextWhiteSpace(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint) {
  Element* editingHost = aHTMLEditor.GetActiveEditingHost();
  TextFragmentData textFragmentDataAtDeletion(aPoint, editingHost);
  if (NS_WARN_IF(!textFragmentDataAtDeletion.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }
  EditorDOMPointInText atNextCharOfStart =
      textFragmentDataAtDeletion.GetInclusiveNextEditableCharPoint(aPoint);
  if (!atNextCharOfStart.IsSet() || atNextCharOfStart.IsEndOfContainer()) {
    return NS_OK;
  }

  // Easy case, preformatted ws.
  if (EditorUtils::IsContentPreformatted(
          *atNextCharOfStart.ContainerAsText())) {
    if (!atNextCharOfStart.IsCharASCIISpace() &&
        !atNextCharOfStart.IsCharNBSP()) {
      return NS_OK;
    }
    nsresult rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        atNextCharOfStart, atNextCharOfStart.NextPoint(),
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  // Caller's job to ensure that next char is really ws.  If it is normal ws,
  // we need to delete the whole run.
  if (atNextCharOfStart.IsCharASCIISpace()) {
    EditorDOMPoint startToDelete =
        textFragmentDataAtDeletion.GetFirstASCIIWhiteSpacePointCollapsedTo(
            atNextCharOfStart);
    EditorDOMPoint endToDelete =
        textFragmentDataAtDeletion.GetEndOfCollapsibleASCIIWhiteSpaces(
            atNextCharOfStart);
    nsresult rv =
        WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
            aHTMLEditor, &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
          "failed");
      return rv;
    }

    // Finally, delete that ws
    rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        startToDelete, endToDelete,
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  if (atNextCharOfStart.IsCharNBSP()) {
    EditorDOMPoint startToDelete(atNextCharOfStart);
    EditorDOMPoint endToDelete(startToDelete.NextPoint());
    nsresult rv =
        WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
            aHTMLEditor, &startToDelete, &endToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
          "failed");
      return rv;
    }

    // Finally, delete that ws
    rv = aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
        startToDelete, endToDelete,
        HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return rv;
  }

  return NS_OK;
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
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
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
                       "HTMLEditor::CollapseSelectionTo() failed");
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
    EditorDOMPointInText atPreviousChar = GetPreviousEditableCharPoint(aPoint);
    // When it's a non-empty text node, return it.
    if (atPreviousChar.IsSet() && !atPreviousChar.IsContainerEmpty()) {
      MOZ_ASSERT(!atPreviousChar.IsEndOfContainer());
      return WSScanResult(atPreviousChar.NextPoint(),
                          atPreviousChar.IsCharASCIISpaceOrNBSP()
                              ? WSType::NormalWhiteSpaces
                              : WSType::NormalText);
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
    EditorDOMPointInText atNextChar = GetInclusiveNextEditableCharPoint(aPoint);
    // When it's a non-empty text node, return it.
    if (atNextChar.IsSet() && !atNextChar.IsContainerEmpty()) {
      return WSScanResult(
          atNextChar,
          !atNextChar.IsEndOfContainer() && atNextChar.IsCharASCIISpaceOrNBSP()
              ? WSType::NormalWhiteSpaces
              : WSType::NormalText);
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
    : mEditingHost(aEditingHost), mIsPreformatted(false) {
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

  mScanStartPoint = aPoint;
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
  mEnd = BoundaryData::ScanCollapsibleWhiteSpaceEndFrom(
      mScanStartPoint, *editableBlockElementOrInlineEditingHost, mEditingHost,
      &mNBSPData);
  // If scan start point is start/end of preformatted text node, only
  // mEnd/mStart crosses a preformatted character so that when one of
  // them crosses a preformatted character, this fragment's range is
  // preformatted.
  // Additionally, if the scan start point is preformatted, and there is
  // no text node around it, the range is also preformatted.
  mIsPreformatted = mStart.AcrossPreformattedCharacter() ||
                    mEnd.AcrossPreformattedCharacter() ||
                    (EditorUtils::IsContentPreformatted(
                         *mScanStartPoint.ContainerAsContent()) &&
                     !mStart.IsNormalText() && !mEnd.IsNormalText());
}

// static
template <typename EditorDOMPointType>
Maybe<WSRunScanner::TextFragmentData::BoundaryData> WSRunScanner::
    TextFragmentData::BoundaryData::ScanCollapsibleWhiteSpaceStartInTextNode(
        const EditorDOMPointType& aPoint, NoBreakingSpaceData* aNBSPData) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_DIAGNOSTIC_ASSERT(aPoint.IsInTextNode());
  MOZ_DIAGNOSTIC_ASSERT(
      !EditorUtils::IsContentPreformatted(*aPoint.ContainerAsText()));

  const nsTextFragment& textFragment = aPoint.ContainerAsText()->TextFragment();
  for (uint32_t i = std::min(aPoint.Offset(), textFragment.GetLength()); i;
       i--) {
    char16_t ch = textFragment.CharAt(i - 1);
    if (nsCRT::IsAsciiSpace(ch)) {
      continue;
    }

    if (ch == HTMLEditUtils::kNBSP) {
      if (aNBSPData) {
        aNBSPData->NotifyNBSP(
            EditorDOMPointInText(aPoint.ContainerAsText(), i - 1),
            NoBreakingSpaceData::Scanning::Backward);
      }
      continue;
    }

    return Some(BoundaryData(EditorDOMPoint(aPoint.ContainerAsText(), i),
                             *aPoint.ContainerAsText(), WSType::NormalText,
                             Preformatted::No));
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
    // If the point is in a text node which is preformatted, we should return
    // the point as a visible character point.
    if (EditorUtils::IsContentPreformatted(*aPoint.ContainerAsText())) {
      return BoundaryData(aPoint, *aPoint.ContainerAsText(), WSType::NormalText,
                          Preformatted::Yes);
    }
    // If the text node is not preformatted, we should look for its preceding
    // characters.
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
                        WSType::CurrentBlockBoundary, Preformatted::No);
  }

  if (HTMLEditUtils::IsBlockElement(*previousLeafContentOrBlock)) {
    return BoundaryData(aPoint, *previousLeafContentOrBlock,
                        WSType::OtherBlockBoundary, Preformatted::No);
  }

  if (!previousLeafContentOrBlock->IsText() ||
      !previousLeafContentOrBlock->IsEditable()) {
    // it's a break or a special node, like <img>, that is not a block and
    // not a break but still serves as a terminator to ws runs.
    return BoundaryData(aPoint, *previousLeafContentOrBlock,
                        previousLeafContentOrBlock->IsHTMLElement(nsGkAtoms::br)
                            ? WSType::BRElement
                            : WSType::SpecialContent,
                        Preformatted::No);
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

  if (EditorUtils::IsContentPreformatted(*previousLeafContentOrBlock)) {
    // If the previous text node is preformatted and not empty, we should return
    // its end as found a visible character.  Note that we stop scanning
    // collapsible white-spaces due to reaching preformatted non-empty text
    // node.  I.e., the following text node might be not preformatted.
    return BoundaryData(EditorDOMPoint::AtEndOf(*previousLeafContentOrBlock),
                        *previousLeafContentOrBlock, WSType::NormalText,
                        Preformatted::No);
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
  MOZ_DIAGNOSTIC_ASSERT(
      !EditorUtils::IsContentPreformatted(*aPoint.ContainerAsText()));

  const nsTextFragment& textFragment = aPoint.ContainerAsText()->TextFragment();
  for (uint32_t i = aPoint.Offset(); i < textFragment.GetLength(); i++) {
    char16_t ch = textFragment.CharAt(i);
    if (nsCRT::IsAsciiSpace(ch)) {
      continue;
    }

    if (ch == HTMLEditUtils::kNBSP) {
      if (aNBSPData) {
        aNBSPData->NotifyNBSP(EditorDOMPointInText(aPoint.ContainerAsText(), i),
                              NoBreakingSpaceData::Scanning::Forward);
      }
      continue;
    }

    return Some(BoundaryData(EditorDOMPoint(aPoint.ContainerAsText(), i),
                             *aPoint.ContainerAsText(), WSType::NormalText,
                             Preformatted::No));
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
    // If the point is in a text node which is preformatted, we should return
    // the point as a visible character point.
    if (EditorUtils::IsContentPreformatted(*aPoint.ContainerAsText())) {
      return BoundaryData(aPoint, *aPoint.ContainerAsText(), WSType::NormalText,
                          Preformatted::Yes);
    }
    // If the text node is not preformatted, we should look for inclusive
    // next characters.
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
    return BoundaryData(aPoint,
                        const_cast<Element&>(
                            aEditableBlockParentOrTopmostEditableInlineElement),
                        WSType::CurrentBlockBoundary, Preformatted::No);
  }

  if (HTMLEditUtils::IsBlockElement(*nextLeafContentOrBlock)) {
    // we encountered a new block.  therefore no more ws.
    return BoundaryData(aPoint, *nextLeafContentOrBlock,
                        WSType::OtherBlockBoundary, Preformatted::No);
  }

  if (!nextLeafContentOrBlock->IsText() ||
      !nextLeafContentOrBlock->IsEditable()) {
    // we encountered a break or a special node, like <img>,
    // that is not a block and not a break but still
    // serves as a terminator to ws runs.
    return BoundaryData(aPoint, *nextLeafContentOrBlock,
                        nextLeafContentOrBlock->IsHTMLElement(nsGkAtoms::br)
                            ? WSType::BRElement
                            : WSType::SpecialContent,
                        Preformatted::No);
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

  if (EditorUtils::IsContentPreformatted(*nextLeafContentOrBlock)) {
    // If the next text node is preformatted and not empty, we should return
    // its start as found a visible character.  Note that we stop scanning
    // collapsible white-spaces due to reaching preformatted non-empty text
    // node.  I.e., the following text node might be not preformatted.
    return BoundaryData(EditorDOMPoint(nextLeafContentOrBlock, 0),
                        *nextLeafContentOrBlock, WSType::NormalText,
                        Preformatted::No);
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

  // If it's preformatted or not start of line, the range is not invisible
  // leading white-spaces.
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

  // If it's preformatted or not immediately before block boundary, the range is
  // not invisible trailing white-spaces.  Note that collapsible white-spaces
  // before a `<br>` element is visible.
  if (!EndsByBlockBoundary()) {
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

  EditorDOMPointInText firstPoint =
      aRange.StartRef().IsInTextNode()
          ? aRange.StartRef().AsInText()
          : GetInclusiveNextEditableCharPoint(aRange.StartRef());
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

  if (IsPreformattedOrSurrondedByVisibleContent()) {
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

  RefPtr<Element> editingHost = aHTMLEditor.GetActiveEditingHost();
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
      if (editingHost != aHTMLEditor.GetActiveEditingHost()) {
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

  if (IsPreformatted()) {
    return ReplaceRangeData();
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
  EditorRawDOMPointInText nextCharOfStartOfEnd =
      GetInclusiveNextEditableCharPoint(endToDelete);
  if (!nextCharOfStartOfEnd.IsSet() ||
      nextCharOfStartOfEnd.IsEndOfContainer() ||
      !nextCharOfStartOfEnd.IsCharASCIISpace() ||
      EditorUtils::IsContentPreformatted(
          *nextCharOfStartOfEnd.ContainerAsText())) {
    return ReplaceRangeData();
  }
  if (nextCharOfStartOfEnd.IsStartOfContainer() ||
      nextCharOfStartOfEnd.IsPreviousCharASCIISpace()) {
    nextCharOfStartOfEnd =
        aTextFragmentDataAtStartToDelete
            .GetFirstASCIIWhiteSpacePointCollapsedTo(nextCharOfStartOfEnd);
  }
  EditorRawDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
      aTextFragmentDataAtStartToDelete.GetEndOfCollapsibleASCIIWhiteSpaces(
          nextCharOfStartOfEnd);
  return ReplaceRangeData(nextCharOfStartOfEnd,
                          endOfCollapsibleASCIIWhiteSpaces,
                          nsDependentSubstring(&kNBSP, 1));
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

  if (IsPreformatted()) {
    return ReplaceRangeData();
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
  EditorRawDOMPointInText atPreviousCharOfStart =
      GetPreviousEditableCharPoint(startToDelete);
  if (!atPreviousCharOfStart.IsSet() ||
      atPreviousCharOfStart.IsEndOfContainer() ||
      !atPreviousCharOfStart.IsCharASCIISpace() ||
      EditorUtils::IsContentPreformatted(
          *atPreviousCharOfStart.ContainerAsText())) {
    return ReplaceRangeData();
  }
  if (atPreviousCharOfStart.IsStartOfContainer() ||
      atPreviousCharOfStart.IsPreviousCharASCIISpace()) {
    atPreviousCharOfStart =
        GetFirstASCIIWhiteSpacePointCollapsedTo(atPreviousCharOfStart);
  }
  EditorRawDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
      GetEndOfCollapsibleASCIIWhiteSpaces(atPreviousCharOfStart);
  return ReplaceRangeData(atPreviousCharOfStart,
                          endOfCollapsibleASCIIWhiteSpaces,
                          nsDependentSubstring(&kNBSP, 1));
}

// static
nsresult
WhiteSpaceVisibilityKeeper::MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToSplit) {
  TextFragmentData textFragmentDataAtSplitPoint(
      aPointToSplit, aHTMLEditor.GetActiveEditingHost());
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
    EditorRawDOMPointInText atNextCharOfStart =
        textFragmentDataAtSplitPoint.GetInclusiveNextEditableCharPoint(
            pointToSplit);
    if (atNextCharOfStart.IsSet() && !atNextCharOfStart.IsEndOfContainer() &&
        atNextCharOfStart.IsCharASCIISpace() &&
        !EditorUtils::IsContentPreformatted(
            *atNextCharOfStart.ContainerAsText())) {
      // pointToSplit will be referred bellow so that we need to keep
      // it a valid point.
      AutoEditorDOMPointChildInvalidator forgetChild(pointToSplit);
      if (atNextCharOfStart.IsStartOfContainer() ||
          atNextCharOfStart.IsPreviousCharASCIISpace()) {
        atNextCharOfStart =
            textFragmentDataAtSplitPoint
                .GetFirstASCIIWhiteSpacePointCollapsedTo(atNextCharOfStart);
      }
      EditorRawDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
          textFragmentDataAtSplitPoint.GetEndOfCollapsibleASCIIWhiteSpaces(
              atNextCharOfStart);
      nsresult rv =
          WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
              aHTMLEditor,
              EditorDOMRangeInTexts(atNextCharOfStart,
                                    endOfCollapsibleASCIIWhiteSpaces),
              nsDependentSubstring(&kNBSP, 1));
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
    EditorRawDOMPointInText atPreviousCharOfStart =
        textFragmentDataAtSplitPoint.GetPreviousEditableCharPoint(pointToSplit);
    if (atPreviousCharOfStart.IsSet() &&
        !atPreviousCharOfStart.IsEndOfContainer() &&
        atPreviousCharOfStart.IsCharASCIISpace() &&
        !EditorUtils::IsContentPreformatted(
            *atPreviousCharOfStart.ContainerAsText())) {
      if (atPreviousCharOfStart.IsStartOfContainer() ||
          atPreviousCharOfStart.IsPreviousCharASCIISpace()) {
        atPreviousCharOfStart =
            textFragmentDataAtSplitPoint
                .GetFirstASCIIWhiteSpacePointCollapsedTo(atPreviousCharOfStart);
      }
      EditorRawDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
          textFragmentDataAtSplitPoint.GetEndOfCollapsibleASCIIWhiteSpaces(
              atPreviousCharOfStart);
      nsresult rv =
          WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
              aHTMLEditor,
              EditorDOMRangeInTexts(atPreviousCharOfStart,
                                    endOfCollapsibleASCIIWhiteSpaces),
              nsDependentSubstring(&kNBSP, 1));
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

template <typename PT, typename CT>
EditorDOMPointInText
WSRunScanner::TextFragmentData::GetInclusiveNextEditableCharPoint(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (NS_WARN_IF(!aPoint.IsInContentNode()) ||
      NS_WARN_IF(!mScanStartPoint.IsInContentNode())) {
    return EditorDOMPointInText();
  }

  EditorRawDOMPoint point;
  if (nsIContent* child =
          aPoint.CanContainerHaveChildren() ? aPoint.GetChild() : nullptr) {
    nsIContent* leafContent = child->HasChildren()
                                  ? HTMLEditUtils::GetFirstLeafContent(
                                        *child, {LeafNodeType::OnlyLeafNode})
                                  : child;
    if (NS_WARN_IF(!leafContent)) {
      return EditorDOMPointInText();
    }
    point.Set(leafContent, 0);
  } else {
    point = aPoint;
  }

  // If it points a character in a text node, return it.
  // XXX For the performance, this does not check whether the container
  //     is outside of our range.
  if (point.IsInTextNode() && point.GetContainer()->IsEditable() &&
      !point.IsEndOfContainer()) {
    return EditorDOMPointInText(point.ContainerAsText(), point.Offset());
  }

  if (point.GetContainer() == GetEndReasonContent()) {
    return EditorDOMPointInText();
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
    return EditorDOMPointInText(nextContent->AsText(), 0);
  }
  return EditorDOMPointInText();
}

template <typename PT, typename CT>
EditorDOMPointInText
WSRunScanner::TextFragmentData::GetPreviousEditableCharPoint(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (NS_WARN_IF(!aPoint.IsInContentNode()) ||
      NS_WARN_IF(!mScanStartPoint.IsInContentNode())) {
    return EditorDOMPointInText();
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
      return EditorDOMPointInText();
    }
    point.SetToEndOf(leafContent);
  } else {
    point = aPoint;
  }

  // If it points a character in a text node and it's not first character
  // in it, return its previous point.
  // XXX For the performance, this does not check whether the container
  //     is outside of our range.
  if (point.IsInTextNode() && point.GetContainer()->IsEditable() &&
      !point.IsStartOfContainer()) {
    return EditorDOMPointInText(point.ContainerAsText(), point.Offset() - 1);
  }

  if (point.GetContainer() == GetStartReasonContent()) {
    return EditorDOMPointInText();
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
    return EditorDOMPointInText(
        previousContent->AsText(),
        previousContent->AsText()->TextLength()
            ? previousContent->AsText()->TextLength() - 1
            : 0);
  }
  return EditorDOMPointInText();
}

// static
template <typename EditorDOMPointType>
EditorDOMPointType WSRunScanner::GetAfterLastVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter) {
  if (EditorUtils::IsContentPreformatted(aTextNode)) {
    return EditorDOMPointType::AtEndOf(aTextNode);
  }
  TextFragmentData textFragmentData(
      EditorDOMPoint(&aTextNode,
                     aTextNode.Length() ? aTextNode.Length() - 1 : 0),
      aAncestorLimiter);
  if (NS_WARN_IF(!textFragmentData.IsInitialized())) {
    return EditorDOMPointType();  // TODO: Make here return error with Err.
  }
  const EditorDOMRange& invisibleWhiteSpaceRange =
      textFragmentData.InvisibleTrailingWhiteSpaceRangeRef();
  if (!invisibleWhiteSpaceRange.IsPositioned() ||
      invisibleWhiteSpaceRange.Collapsed()) {
    return EditorDOMPointType::AtEndOf(aTextNode);
  }
  return EditorDOMPointType(invisibleWhiteSpaceRange.StartRef());
}

// static
template <typename EditorDOMPointType>
EditorDOMPointType WSRunScanner::GetFirstVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter) {
  if (EditorUtils::IsContentPreformatted(aTextNode)) {
    return EditorDOMPointType(&aTextNode, 0);
  }
  TextFragmentData textFragmentData(EditorDOMPoint(&aTextNode, 0),
                                    aAncestorLimiter);
  if (NS_WARN_IF(!textFragmentData.IsInitialized())) {
    return EditorDOMPointType();  // TODO: Make here return error with Err.
  }
  const EditorDOMRange& invisibleWhiteSpaceRange =
      textFragmentData.InvisibleLeadingWhiteSpaceRangeRef();
  if (!invisibleWhiteSpaceRange.IsPositioned() ||
      invisibleWhiteSpaceRange.Collapsed()) {
    return EditorDOMPointType(&aTextNode, 0);
  }
  return EditorDOMPointType(invisibleWhiteSpaceRange.EndRef());
}

EditorDOMPointInText
WSRunScanner::TextFragmentData::GetEndOfCollapsibleASCIIWhiteSpaces(
    const EditorDOMPointInText& aPointAtASCIIWhiteSpace) const {
  MOZ_ASSERT(aPointAtASCIIWhiteSpace.IsSet());
  MOZ_ASSERT(!aPointAtASCIIWhiteSpace.IsEndOfContainer());
  MOZ_ASSERT(aPointAtASCIIWhiteSpace.IsCharASCIISpace());
  NS_ASSERTION(!EditorUtils::IsContentPreformatted(
                   *aPointAtASCIIWhiteSpace.ContainerAsText()),
               "aPointAtASCIIWhiteSpace should be in a formatted text node");

  // If it's not the last character in the text node, let's scan following
  // characters in it.
  if (!aPointAtASCIIWhiteSpace.IsAtLastContent()) {
    Maybe<uint32_t> nextVisibleCharOffset =
        HTMLEditUtils::GetNextCharOffsetExceptASCIIWhiteSpaces(
            aPointAtASCIIWhiteSpace);
    if (nextVisibleCharOffset.isSome()) {
      // There is non-white-space character in it.
      return EditorDOMPointInText(aPointAtASCIIWhiteSpace.ContainerAsText(),
                                  nextVisibleCharOffset.value());
    }
  }

  // Otherwise, i.e., the text node ends with ASCII white-space, keep scanning
  // the following text nodes.
  // XXX Perhaps, we should stop scanning if there is non-editable and visible
  //     content.
  EditorDOMPointInText afterLastWhiteSpace =
      EditorDOMPointInText::AtEndOf(*aPointAtASCIIWhiteSpace.ContainerAsText());
  for (EditorDOMPointInText atEndOfPreviousTextNode = afterLastWhiteSpace;;) {
    EditorDOMPointInText atStartOfNextTextNode =
        GetInclusiveNextEditableCharPoint(atEndOfPreviousTextNode);
    if (!atStartOfNextTextNode.IsSet()) {
      // There is no more text nodes.  Return end of the previous text node.
      return afterLastWhiteSpace;
    }

    // We can ignore empty text nodes (even if it's preformatted).
    if (atStartOfNextTextNode.IsContainerEmpty()) {
      atEndOfPreviousTextNode = atStartOfNextTextNode;
      continue;
    }

    // If next node starts with non-white-space character or next node is
    // preformatted, return end of previous text node.
    if (!atStartOfNextTextNode.IsCharASCIISpace() ||
        EditorUtils::IsContentPreformatted(
            *atStartOfNextTextNode.ContainerAsText())) {
      return afterLastWhiteSpace;
    }

    // Otherwise, scan the text node.
    Maybe<uint32_t> nextVisibleCharOffset =
        HTMLEditUtils::GetNextCharOffsetExceptASCIIWhiteSpaces(
            atStartOfNextTextNode);
    if (nextVisibleCharOffset.isSome()) {
      return EditorDOMPointInText(atStartOfNextTextNode.ContainerAsText(),
                                  nextVisibleCharOffset.value());
    }

    // The next text nodes ends with white-space too.  Try next one.
    afterLastWhiteSpace = atEndOfPreviousTextNode =
        EditorDOMPointInText::AtEndOf(*atStartOfNextTextNode.ContainerAsText());
  }
}

EditorDOMPointInText
WSRunScanner::TextFragmentData::GetFirstASCIIWhiteSpacePointCollapsedTo(
    const EditorDOMPointInText& aPointAtASCIIWhiteSpace) const {
  MOZ_ASSERT(aPointAtASCIIWhiteSpace.IsSet());
  MOZ_ASSERT(!aPointAtASCIIWhiteSpace.IsEndOfContainer());
  MOZ_ASSERT(aPointAtASCIIWhiteSpace.IsCharASCIISpace());
  NS_ASSERTION(!EditorUtils::IsContentPreformatted(
                   *aPointAtASCIIWhiteSpace.ContainerAsText()),
               "aPointAtASCIIWhiteSpace should be in a formatted text node");

  // If there is some characters before it, scan it in the text node first.
  if (!aPointAtASCIIWhiteSpace.IsStartOfContainer()) {
    uint32_t firstASCIIWhiteSpaceOffset =
        HTMLEditUtils::GetFirstASCIIWhiteSpaceOffsetCollapsedWith(
            aPointAtASCIIWhiteSpace);
    if (firstASCIIWhiteSpaceOffset) {
      // There is a non-white-space character in it.
      return EditorDOMPointInText(aPointAtASCIIWhiteSpace.ContainerAsText(),
                                  firstASCIIWhiteSpaceOffset);
    }
  }

  // Otherwise, i.e., the text node starts with ASCII white-space, keep scanning
  // the preceding text nodes.
  // XXX Perhaps, we should stop scanning if there is non-editable and visible
  //     content.
  EditorDOMPointInText atLastWhiteSpace =
      EditorDOMPointInText(aPointAtASCIIWhiteSpace.ContainerAsText(), 0);
  for (EditorDOMPointInText atStartOfPreviousTextNode = atLastWhiteSpace;;) {
    EditorDOMPointInText atLastCharOfNextTextNode =
        GetPreviousEditableCharPoint(atStartOfPreviousTextNode);
    if (!atLastCharOfNextTextNode.IsSet()) {
      // There is no more text nodes.  Return end of last text node.
      return atLastWhiteSpace;
    }

    // We can ignore empty text nodes (even if it's preformatted).
    if (atLastCharOfNextTextNode.IsContainerEmpty()) {
      atStartOfPreviousTextNode = atLastCharOfNextTextNode;
      continue;
    }

    // If next node ends with non-white-space character or next node is
    // preformatted, return start of previous text node.
    if (!atLastCharOfNextTextNode.IsCharASCIISpace() ||
        EditorUtils::IsContentPreformatted(
            *atLastCharOfNextTextNode.ContainerAsText())) {
      return atLastWhiteSpace;
    }

    // Otherwise, scan the text node.
    uint32_t firstASCIIWhiteSpaceOffset =
        HTMLEditUtils::GetFirstASCIIWhiteSpaceOffsetCollapsedWith(
            atLastCharOfNextTextNode);
    if (firstASCIIWhiteSpaceOffset) {
      return EditorDOMPointInText(atLastCharOfNextTextNode.ContainerAsText(),
                                  firstASCIIWhiteSpaceOffset);
    }

    // The next text nodes starts with white-space too.  Try next one.
    atLastWhiteSpace = atStartOfPreviousTextNode =
        EditorDOMPointInText(atLastCharOfNextTextNode.ContainerAsText(), 0);
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
  Element* editingHost = aHTMLEditor.GetActiveEditingHost();
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
        !atPreviousCharOfEndOfVisibleWhiteSpaces.IsCharNBSP()) {
      return NS_OK;
    }

    // now check that what is to the left of it is compatible with replacing
    // nbsp with space
    EditorDOMPointInText atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces =
        textFragmentData.GetPreviousEditableCharPoint(
            atPreviousCharOfEndOfVisibleWhiteSpaces);
    bool isPreviousCharASCIIWhiteSpace =
        atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
        !atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
             .IsEndOfContainer() &&
        atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
            .IsCharASCIISpace();
    bool maybeNBSPFollowingVisibleContent =
        (atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
         !isPreviousCharASCIIWhiteSpace) ||
        (!atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
         (visibleWhiteSpaces.StartsFromNormalText() ||
          visibleWhiteSpaces.StartsFromSpecialContent()));
    bool followedByVisibleContentOrBRElement = false;

    // If the NBSP follows a visible content or an ASCII white-space, i.e.,
    // unless NBSP is first character and start of a block, we may need to
    // insert <br> element and restore the NBSP to an ASCII white-space.
    if (maybeNBSPFollowingVisibleContent || isPreviousCharASCIIWhiteSpace) {
      followedByVisibleContentOrBRElement =
          visibleWhiteSpaces.EndsByNormalText() ||
          visibleWhiteSpaces.EndsBySpecialContent() ||
          visibleWhiteSpaces.EndsByBRElement();
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

          Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
              aHTMLEditor.InsertBRElementWithTransaction(
                  atEndOfVisibleWhiteSpaces);
          if (resultOfInsertingBRElement.isErr()) {
            NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
            return resultOfInsertingBRElement.unwrapErr();
          }
          MOZ_ASSERT(resultOfInsertingBRElement.inspect());

          atPreviousCharOfEndOfVisibleWhiteSpaces =
              textFragmentData.GetPreviousEditableCharPoint(
                  atEndOfVisibleWhiteSpaces);
          atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces =
              textFragmentData.GetPreviousEditableCharPoint(
                  atPreviousCharOfEndOfVisibleWhiteSpaces);
          isPreviousCharASCIIWhiteSpace =
              atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
              !atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                   .IsEndOfContainer() &&
              atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                  .IsCharASCIISpace();
          followedByVisibleContentOrBRElement = true;
        }
      }

      // Next, replace the NBSP with an ASCII white-space if it's surrounded
      // by visible contents (or immediately before a <br> element).
      if (maybeNBSPFollowingVisibleContent &&
          followedByVisibleContentOrBRElement) {
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
    // element and following (maybe multiple) ASCII spaces, remove the NBSP,
    // but inserts a NBSP before the spaces.  This makes a line break
    // opportunity to wrap the line.
    // XXX This is different behavior from Blink.  Blink generates pairs of
    //     an NBSP and an ASCII white-space, but put NBSP at the end of the
    //     sequence.  We should follow the behavior for web-compat.
    if (maybeNBSPFollowingVisibleContent || !isPreviousCharASCIIWhiteSpace ||
        !followedByVisibleContentOrBRElement ||
        EditorUtils::IsContentPreformatted(
            *atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                 .GetContainerAsText())) {
      return NS_OK;
    }

    // Currently, we're at an NBSP following an ASCII space, and we need to
    // replace them with `"&nbsp; "` for avoiding collapsing white-spaces.
    MOZ_ASSERT(!atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                    .IsEndOfContainer());
    EditorDOMPointInText atFirstASCIIWhiteSpace =
        textFragmentData.GetFirstASCIIWhiteSpacePointCollapsedTo(
            atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces);
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
        u"\x00A0 "_ns);
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
  EditorDOMPointInText atPreviousCharOfEndOfVisibleWhiteSpaces =
      textFragmentData.GetPreviousEditableCharPoint(atEndOfVisibleWhiteSpaces);
  if (!atPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() ||
      atPreviousCharOfEndOfVisibleWhiteSpaces.IsEndOfContainer() ||
      !atPreviousCharOfEndOfVisibleWhiteSpaces.IsCharNBSP()) {
    return NS_OK;
  }

  // Next, consider the range to collapse ASCII white-spaces before there.
  EditorDOMPointInText startToDelete, endToDelete;

  EditorDOMPointInText atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces =
      textFragmentData.GetPreviousEditableCharPoint(
          atPreviousCharOfEndOfVisibleWhiteSpaces);
  // If there are some preceding ASCII white-spaces, we need to treat them
  // as one white-space.  I.e., we need to collapse them.
  if (atPreviousCharOfEndOfVisibleWhiteSpaces.IsCharNBSP() &&
      atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
      atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
          .IsCharASCIISpace()) {
    startToDelete = textFragmentData.GetFirstASCIIWhiteSpacePointCollapsedTo(
        atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces);
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
  EditorDOMPointInText atPreviousChar =
      GetPreviousEditableCharPoint(aPointToInsert);
  if (!atPreviousChar.IsSet() || atPreviousChar.IsEndOfContainer() ||
      !atPreviousChar.IsCharNBSP() ||
      EditorUtils::IsContentPreformatted(*atPreviousChar.ContainerAsText())) {
    return EditorDOMPointInText();
  }

  EditorDOMPointInText atPreviousCharOfPreviousChar =
      GetPreviousEditableCharPoint(atPreviousChar);
  if (atPreviousCharOfPreviousChar.IsSet()) {
    // If the previous char is in different text node and it's preformatted,
    // we shouldn't touch it.
    if (atPreviousChar.ContainerAsText() !=
            atPreviousCharOfPreviousChar.ContainerAsText() &&
        EditorUtils::IsContentPreformatted(
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
  if (!visibleWhiteSpaces.StartsFromNormalText() &&
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
  EditorDOMPointInText atNextChar =
      GetInclusiveNextEditableCharPoint(aPointToInsert);
  if (!atNextChar.IsSet() || NS_WARN_IF(atNextChar.IsEndOfContainer()) ||
      !atNextChar.IsCharNBSP() ||
      EditorUtils::IsContentPreformatted(*atNextChar.ContainerAsText())) {
    return EditorDOMPointInText();
  }

  EditorDOMPointInText atNextCharOfNextCharOfNBSP =
      GetInclusiveNextEditableCharPoint(atNextChar.NextPoint());
  if (atNextCharOfNextCharOfNBSP.IsSet()) {
    // If the next char is in different text node and it's preformatted,
    // we shouldn't touch it.
    if (atNextChar.ContainerAsText() !=
            atNextCharOfNextCharOfNBSP.ContainerAsText() &&
        EditorUtils::IsContentPreformatted(
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
  if (!visibleWhiteSpaces.EndsByNormalText() &&
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
  Element* editingHost = aHTMLEditor.GetActiveEditingHost();
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
      const EditorDOMPointInText atFirstInvisibleWhiteSpace =
          hasInvisibleLeadingWhiteSpaces
              ? aStart.GetInclusiveNextEditableCharPoint(
                    aroundFirstInvisibleWhiteSpace)
              : aEnd.GetInclusiveNextEditableCharPoint(
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
  const EditorDOMPointInText atLastInvisibleWhiteSpace =
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

  // If the text node is preformatted, just remove the previous character.
  if (textFragmentDataAtCaret.IsPreformatted()) {
    return EditorDOMRangeInTexts(atPreviousChar, atNextChar);
  }

  // If previous char is an ASCII white-spaces, delete all adjcent ASCII
  // whitespaces.
  EditorDOMRangeInTexts rangeToDelete;
  if (atPreviousChar.IsCharASCIISpace()) {
    EditorDOMPointInText startToDelete =
        textFragmentDataAtCaret.GetFirstASCIIWhiteSpacePointCollapsedTo(
            atPreviousChar);
    if (!startToDelete.IsSet()) {
      NS_WARNING(
          "WSRunScanner::GetFirstASCIIWhiteSpacePointCollapsedTo() failed");
      return Err(NS_ERROR_FAILURE);
    }
    EditorDOMPointInText endToDelete =
        textFragmentDataAtCaret.GetEndOfCollapsibleASCIIWhiteSpaces(
            atPreviousChar);
    if (!endToDelete.IsSet()) {
      NS_WARNING("WSRunScanner::GetEndOfCollapsibleASCIIWhiteSpaces() failed");
      return Err(NS_ERROR_FAILURE);
    }
    rangeToDelete = EditorDOMRangeInTexts(startToDelete, endToDelete);
  }
  // if previous char is not an ASCII white-space, remove it.
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
  EditorDOMPointInText atCaret =
      textFragmentDataAtCaret.GetInclusiveNextEditableCharPoint(aPoint);
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

  // If the text node is preformatted, just remove the previous character.
  if (textFragmentDataAtCaret.IsPreformatted()) {
    return EditorDOMRangeInTexts(atCaret, atNextChar);
  }

  // If next char is an ASCII whitespaces, delete all adjcent ASCII
  // whitespaces.
  EditorDOMRangeInTexts rangeToDelete;
  if (atCaret.IsCharASCIISpace()) {
    EditorDOMPointInText startToDelete =
        textFragmentDataAtCaret.GetFirstASCIIWhiteSpacePointCollapsedTo(
            atCaret);
    if (!startToDelete.IsSet()) {
      NS_WARNING(
          "WSRunScanner::GetFirstASCIIWhiteSpacePointCollapsedTo() failed");
      return Err(NS_ERROR_FAILURE);
    }
    EditorDOMPointInText endToDelete =
        textFragmentDataAtCaret.GetEndOfCollapsibleASCIIWhiteSpaces(atCaret);
    if (!endToDelete.IsSet()) {
      NS_WARNING("WSRunScanner::GetEndOfCollapsibleASCIIWhiteSpaces() failed");
      return Err(NS_ERROR_FAILURE);
    }
    rangeToDelete = EditorDOMRangeInTexts(startToDelete, endToDelete);
  }
  // if next char is not an ASCII white-space, remove it.
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

  const Element* editingHost = aHTMLEditor.GetActiveEditingHost();

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
          : EditorDOMPoint(const_cast<Element*>(&aRightBlockElement), 0),
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
