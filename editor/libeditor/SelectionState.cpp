/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SelectionState.h"

#include "AutoRangeArray.h"   // for AutoRangeArray
#include "EditorUtils.h"      // for EditorUtils, AutoRangeArray
#include "HTMLEditHelpers.h"  // for JoinNodesDirection, SplitNodeDirection

#include "mozilla/Assertions.h"    // for MOZ_ASSERT, etc.
#include "mozilla/IntegerRange.h"  // for IntegerRange
#include "mozilla/Likely.h"        // For MOZ_LIKELY and MOZ_UNLIKELY
#include "mozilla/RangeUtils.h"    // for RangeUtils
#include "mozilla/dom/RangeBinding.h"
#include "mozilla/dom/Selection.h"  // for Selection
#include "nsAString.h"              // for nsAString::Length
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"          // for NS_WARNING, etc.
#include "nsError.h"          // for NS_OK, etc.
#include "nsIContent.h"       // for nsIContent
#include "nsISupportsImpl.h"  // for nsRange::Release
#include "nsRange.h"          // for nsRange

namespace mozilla {

using namespace dom;

/*****************************************************************************
 * mozilla::RangeItem
 *****************************************************************************/

nsINode* RangeItem::GetRoot() const {
  if (MOZ_UNLIKELY(!IsPositioned())) {
    return nullptr;
  }
  nsINode* rootNode = RangeUtils::ComputeRootNode(mStartContainer);
  if (mStartContainer == mEndContainer) {
    return rootNode;
  }
  return MOZ_LIKELY(rootNode == RangeUtils::ComputeRootNode(mEndContainer))
             ? rootNode
             : nullptr;
}

/******************************************************************************
 * mozilla::SelectionState
 *
 * Class for recording selection info.  Stores selection as collection of
 * { {startnode, startoffset} , {endnode, endoffset} } tuples.  Can't store
 * ranges since dom gravity will possibly change the ranges.
 ******************************************************************************/

template nsresult RangeUpdater::SelAdjCreateNode(const EditorDOMPoint& aPoint);
template nsresult RangeUpdater::SelAdjCreateNode(
    const EditorRawDOMPoint& aPoint);
template nsresult RangeUpdater::SelAdjInsertNode(const EditorDOMPoint& aPoint);
template nsresult RangeUpdater::SelAdjInsertNode(
    const EditorRawDOMPoint& aPoint);

SelectionState::SelectionState(const AutoRangeArray& aRanges)
    : mDirection(aRanges.GetDirection()) {
  mArray.SetCapacity(aRanges.Ranges().Length());
  for (const OwningNonNull<nsRange>& range : aRanges.Ranges()) {
    RefPtr<RangeItem> rangeItem = new RangeItem();
    rangeItem->StoreRange(range);
    mArray.AppendElement(std::move(rangeItem));
  }
}

void SelectionState::SaveSelection(Selection& aSelection) {
  // if we need more items in the array, new them
  if (mArray.Length() < aSelection.RangeCount()) {
    for (uint32_t i = mArray.Length(); i < aSelection.RangeCount(); i++) {
      mArray.AppendElement();
      mArray[i] = new RangeItem();
    }
  } else if (mArray.Length() > aSelection.RangeCount()) {
    // else if we have too many, delete them
    mArray.TruncateLength(aSelection.RangeCount());
  }

  // now store the selection ranges
  const uint32_t rangeCount = aSelection.RangeCount();
  for (const uint32_t i : IntegerRange(rangeCount)) {
    MOZ_ASSERT(aSelection.RangeCount() == rangeCount);
    const nsRange* range = aSelection.GetRangeAt(i);
    MOZ_ASSERT(range);
    if (MOZ_UNLIKELY(NS_WARN_IF(!range))) {
      continue;
    }
    mArray[i]->StoreRange(*range);
  }

  mDirection = aSelection.GetDirection();
}

nsresult SelectionState::RestoreSelection(Selection& aSelection) {
  // clear out selection
  IgnoredErrorResult ignoredError;
  aSelection.RemoveAllRanges(ignoredError);
  NS_WARNING_ASSERTION(!ignoredError.Failed(),
                       "Selection::RemoveAllRanges() failed, but ignored");

  aSelection.SetDirection(mDirection);

  ErrorResult error;
  const CopyableAutoTArray<RefPtr<RangeItem>, 10> rangeItems(mArray);
  for (const RefPtr<RangeItem>& rangeItem : rangeItems) {
    RefPtr<nsRange> range = rangeItem->GetRange();
    if (!range) {
      NS_WARNING("RangeItem::GetRange() failed");
      return NS_ERROR_FAILURE;
    }
    aSelection.AddRangeAndSelectFramesAndNotifyListeners(*range, error);
    if (error.Failed()) {
      NS_WARNING(
          "Selection::AddRangeAndSelectFramesAndNotifyListeners() failed");
      return error.StealNSResult();
    }
  }
  return NS_OK;
}

void SelectionState::ApplyTo(AutoRangeArray& aRanges) {
  aRanges.RemoveAllRanges();
  aRanges.SetDirection(mDirection);
  for (const RefPtr<RangeItem>& rangeItem : mArray) {
    RefPtr<nsRange> range = rangeItem->GetRange();
    if (MOZ_UNLIKELY(!range)) {
      continue;
    }
    aRanges.Ranges().AppendElement(std::move(range));
  }
}

bool SelectionState::Equals(const SelectionState& aOther) const {
  if (mArray.Length() != aOther.mArray.Length()) {
    return false;
  }
  if (mArray.IsEmpty()) {
    return false;  // XXX Why?
  }
  if (mDirection != aOther.mDirection) {
    return false;
  }

  for (uint32_t i : IntegerRange(mArray.Length())) {
    if (NS_WARN_IF(!mArray[i]) || NS_WARN_IF(!aOther.mArray[i]) ||
        !mArray[i]->Equals(*aOther.mArray[i])) {
      return false;
    }
  }
  // if we got here, they are equal
  return true;
}

/******************************************************************************
 * mozilla::RangeUpdater
 *
 * Class for updating nsRanges in response to editor actions.
 ******************************************************************************/

RangeUpdater::RangeUpdater() : mLocked(false) {}

void RangeUpdater::RegisterRangeItem(RangeItem& aRangeItem) {
  if (mArray.Contains(&aRangeItem)) {
    NS_ERROR("tried to register an already registered range");
    return;  // don't register it again.  It would get doubly adjusted.
  }
  mArray.AppendElement(&aRangeItem);
}

void RangeUpdater::DropRangeItem(RangeItem& aRangeItem) {
  NS_WARNING_ASSERTION(
      mArray.Contains(&aRangeItem),
      "aRangeItem is not in the range, but tried to removed from it");
  mArray.RemoveElement(&aRangeItem);
}

void RangeUpdater::RegisterSelectionState(SelectionState& aSelectionState) {
  for (RefPtr<RangeItem>& rangeItem : aSelectionState.mArray) {
    if (NS_WARN_IF(!rangeItem)) {
      continue;
    }
    RegisterRangeItem(*rangeItem);
  }
}

void RangeUpdater::DropSelectionState(SelectionState& aSelectionState) {
  for (RefPtr<RangeItem>& rangeItem : aSelectionState.mArray) {
    if (NS_WARN_IF(!rangeItem)) {
      continue;
    }
    DropRangeItem(*rangeItem);
  }
}

// gravity methods:

template <typename PT, typename CT>
nsresult RangeUpdater::SelAdjCreateNode(
    const EditorDOMPointBase<PT, CT>& aPoint) {
  if (mLocked) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }
  if (mArray.IsEmpty()) {
    return NS_OK;
  }

  if (NS_WARN_IF(!aPoint.IsSetAndValid())) {
    return NS_ERROR_INVALID_ARG;
  }

  for (RefPtr<RangeItem>& rangeItem : mArray) {
    if (NS_WARN_IF(!rangeItem)) {
      return NS_ERROR_FAILURE;
    }
    if (rangeItem->mStartContainer == aPoint.GetContainer() &&
        rangeItem->mStartOffset > aPoint.Offset()) {
      rangeItem->mStartOffset++;
    }
    if (rangeItem->mEndContainer == aPoint.GetContainer() &&
        rangeItem->mEndOffset > aPoint.Offset()) {
      rangeItem->mEndOffset++;
    }
  }
  return NS_OK;
}

template <typename PT, typename CT>
nsresult RangeUpdater::SelAdjInsertNode(
    const EditorDOMPointBase<PT, CT>& aPoint) {
  nsresult rv = SelAdjCreateNode(aPoint);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "RangeUpdater::SelAdjCreateNode() failed");
  return rv;
}

void RangeUpdater::SelAdjDeleteNode(nsINode& aNodeToDelete) {
  if (mLocked) {
    // lock set by Will/DidReplaceParent, etc...
    return;
  }

  if (mArray.IsEmpty()) {
    return;
  }

  EditorRawDOMPoint atNodeToDelete(&aNodeToDelete);
  NS_ASSERTION(atNodeToDelete.IsSetAndValid(),
               "aNodeToDelete must be an orphan node or this is called "
               "during mutation");
  // check for range endpoints that are after aNodeToDelete and in the same
  // parent
  for (RefPtr<RangeItem>& rangeItem : mArray) {
    MOZ_ASSERT(rangeItem);

    if (rangeItem->mStartContainer == atNodeToDelete.GetContainer() &&
        rangeItem->mStartOffset > atNodeToDelete.Offset()) {
      rangeItem->mStartOffset--;
    }
    if (rangeItem->mEndContainer == atNodeToDelete.GetContainer() &&
        rangeItem->mEndOffset > atNodeToDelete.Offset()) {
      rangeItem->mEndOffset--;
    }

    // check for range endpoints that are in aNodeToDelete
    if (rangeItem->mStartContainer == &aNodeToDelete) {
      rangeItem->mStartContainer = atNodeToDelete.GetContainer();
      rangeItem->mStartOffset = atNodeToDelete.Offset();
    }
    if (rangeItem->mEndContainer == &aNodeToDelete) {
      rangeItem->mEndContainer = atNodeToDelete.GetContainer();
      rangeItem->mEndOffset = atNodeToDelete.Offset();
    }

    // check for range endpoints that are in descendants of aNodeToDelete
    bool updateEndBoundaryToo = false;
    if (EditorUtils::IsDescendantOf(*rangeItem->mStartContainer,
                                    aNodeToDelete)) {
      updateEndBoundaryToo =
          rangeItem->mStartContainer == rangeItem->mEndContainer;
      rangeItem->mStartContainer = atNodeToDelete.GetContainer();
      rangeItem->mStartOffset = atNodeToDelete.Offset();
    }

    // avoid having to call IsDescendantOf() for common case of range startnode
    // == range endnode.
    if (updateEndBoundaryToo ||
        EditorUtils::IsDescendantOf(*rangeItem->mEndContainer, aNodeToDelete)) {
      rangeItem->mEndContainer = atNodeToDelete.GetContainer();
      rangeItem->mEndOffset = atNodeToDelete.Offset();
    }
  }
}

nsresult RangeUpdater::SelAdjSplitNode(nsIContent& aOriginalContent,
                                       uint32_t aSplitOffset,
                                       nsIContent& aNewContent,
                                       SplitNodeDirection aSplitNodeDirection) {
  if (mLocked) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }

  if (mArray.IsEmpty()) {
    return NS_OK;
  }

  EditorRawDOMPoint atNewNode(&aNewContent);
  nsresult rv = SelAdjInsertNode(atNewNode);
  if (NS_FAILED(rv)) {
    NS_WARNING("RangeUpdater::SelAdjInsertNode() failed");
    return rv;
  }

  // If point is in the range which are moved from aOriginalContent to
  // aNewContent, we need to change its container to aNewContent and may need to
  // adjust the offset. If point is in the range which are not moved from
  // aOriginalContent, we may need to adjust the offset.
  auto AdjustDOMPoint = [&](nsCOMPtr<nsINode>& aContainer,
                            uint32_t& aOffset) -> void {
    if (aContainer != &aOriginalContent) {
      return;
    }
    if (aSplitNodeDirection == SplitNodeDirection::LeftNodeIsNewOne) {
      if (aOffset > aSplitOffset) {
        aOffset -= aSplitOffset;
      } else {
        aContainer = &aNewContent;
      }
    } else if (aOffset >= aSplitOffset) {
      aContainer = &aNewContent;
      aOffset = aSplitOffset - aOffset;
    }
  };

  for (RefPtr<RangeItem>& rangeItem : mArray) {
    if (NS_WARN_IF(!rangeItem)) {
      return NS_ERROR_FAILURE;
    }
    AdjustDOMPoint(rangeItem->mStartContainer, rangeItem->mStartOffset);
    AdjustDOMPoint(rangeItem->mEndContainer, rangeItem->mEndOffset);
  }
  return NS_OK;
}

nsresult RangeUpdater::SelAdjJoinNodes(
    const EditorRawDOMPoint& aStartOfRightContent,
    const nsIContent& aRemovedContent, uint32_t aOffsetOfJoinedContent,
    JoinNodesDirection aJoinNodesDirection) {
  MOZ_ASSERT(aStartOfRightContent.IsSetAndValid());

  if (mLocked) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }

  if (mArray.IsEmpty()) {
    return NS_OK;
  }

  auto AdjustDOMPoint = [&](nsCOMPtr<nsINode>& aContainer,
                            uint32_t& aOffset) -> void {
    if (aContainer == aStartOfRightContent.GetContainerParent()) {
      // If the point is in common parent of joined content nodes and it pointed
      // after the right content node, decrease the offset.
      if (aOffset > aOffsetOfJoinedContent) {
        aOffset--;
      }
      // If it pointed the right content node, adjust it to point ex-first
      // content of the right node.
      else if (aOffset == aOffsetOfJoinedContent) {
        aContainer = aStartOfRightContent.GetContainer();
        aOffset = aStartOfRightContent.Offset();
      }
    } else if (aContainer == aStartOfRightContent.GetContainer()) {
      // If the point is in joined node, and removed content is moved to
      // start of the joined node, we need to adjust the offset.
      if (aJoinNodesDirection == JoinNodesDirection::LeftNodeIntoRightNode) {
        aOffset += aStartOfRightContent.Offset();
      }
    } else if (aContainer == &aRemovedContent) {
      // If the point is in the removed content, move the point to the new
      // point in the joined node.  If left node content is moved into
      // right node, the offset should be same.  Otherwise, we need to advance
      // the offset to length of the removed content.
      aContainer = aStartOfRightContent.GetContainer();
      if (aJoinNodesDirection == JoinNodesDirection::RightNodeIntoLeftNode) {
        aOffset += aStartOfRightContent.Offset();
      }
    }
  };

  for (RefPtr<RangeItem>& rangeItem : mArray) {
    if (NS_WARN_IF(!rangeItem)) {
      return NS_ERROR_FAILURE;
    }
    AdjustDOMPoint(rangeItem->mStartContainer, rangeItem->mStartOffset);
    AdjustDOMPoint(rangeItem->mEndContainer, rangeItem->mEndOffset);
  }

  return NS_OK;
}

void RangeUpdater::SelAdjReplaceText(const Text& aTextNode, uint32_t aOffset,
                                     uint32_t aReplacedLength,
                                     uint32_t aInsertedLength) {
  if (mLocked) {
    // lock set by Will/DidReplaceParent, etc...
    return;
  }

  // First, adjust selection for insertion because when offset is in the
  // replaced range, it's adjusted to aOffset and never modified by the
  // insertion if we adjust selection for deletion first.
  SelAdjInsertText(aTextNode, aOffset, aInsertedLength);

  // Then, adjust selection for deletion.
  SelAdjDeleteText(aTextNode, aOffset, aReplacedLength);
}

void RangeUpdater::SelAdjInsertText(const Text& aTextNode, uint32_t aOffset,
                                    uint32_t aInsertedLength) {
  if (mLocked) {
    // lock set by Will/DidReplaceParent, etc...
    return;
  }

  for (RefPtr<RangeItem>& rangeItem : mArray) {
    MOZ_ASSERT(rangeItem);

    if (rangeItem->mStartContainer == &aTextNode &&
        rangeItem->mStartOffset > aOffset) {
      rangeItem->mStartOffset += aInsertedLength;
    }
    if (rangeItem->mEndContainer == &aTextNode &&
        rangeItem->mEndOffset > aOffset) {
      rangeItem->mEndOffset += aInsertedLength;
    }
  }
}

void RangeUpdater::SelAdjDeleteText(const Text& aTextNode, uint32_t aOffset,
                                    uint32_t aDeletedLength) {
  if (mLocked) {
    // lock set by Will/DidReplaceParent, etc...
    return;
  }

  for (RefPtr<RangeItem>& rangeItem : mArray) {
    MOZ_ASSERT(rangeItem);

    if (rangeItem->mStartContainer == &aTextNode &&
        rangeItem->mStartOffset > aOffset) {
      if (rangeItem->mStartOffset >= aDeletedLength) {
        rangeItem->mStartOffset -= aDeletedLength;
      } else {
        rangeItem->mStartOffset = 0;
      }
    }
    if (rangeItem->mEndContainer == &aTextNode &&
        rangeItem->mEndOffset > aOffset) {
      if (rangeItem->mEndOffset >= aDeletedLength) {
        rangeItem->mEndOffset -= aDeletedLength;
      } else {
        rangeItem->mEndOffset = 0;
      }
    }
  }
}

void RangeUpdater::DidReplaceContainer(const Element& aRemovedElement,
                                       Element& aInsertedElement) {
  if (NS_WARN_IF(!mLocked)) {
    return;
  }
  mLocked = false;

  for (RefPtr<RangeItem>& rangeItem : mArray) {
    if (NS_WARN_IF(!rangeItem)) {
      return;
    }

    if (rangeItem->mStartContainer == &aRemovedElement) {
      rangeItem->mStartContainer = &aInsertedElement;
    }
    if (rangeItem->mEndContainer == &aRemovedElement) {
      rangeItem->mEndContainer = &aInsertedElement;
    }
  }
}

void RangeUpdater::DidRemoveContainer(const Element& aRemovedElement,
                                      nsINode& aRemovedElementContainerNode,
                                      uint32_t aOldOffsetOfRemovedElement,
                                      uint32_t aOldChildCountOfRemovedElement) {
  if (NS_WARN_IF(!mLocked)) {
    return;
  }
  mLocked = false;

  for (RefPtr<RangeItem>& rangeItem : mArray) {
    if (NS_WARN_IF(!rangeItem)) {
      return;
    }

    if (rangeItem->mStartContainer == &aRemovedElement) {
      rangeItem->mStartContainer = &aRemovedElementContainerNode;
      rangeItem->mStartOffset += aOldOffsetOfRemovedElement;
    } else if (rangeItem->mStartContainer == &aRemovedElementContainerNode &&
               rangeItem->mStartOffset > aOldOffsetOfRemovedElement) {
      rangeItem->mStartOffset += aOldChildCountOfRemovedElement - 1;
    }

    if (rangeItem->mEndContainer == &aRemovedElement) {
      rangeItem->mEndContainer = &aRemovedElementContainerNode;
      rangeItem->mEndOffset += aOldOffsetOfRemovedElement;
    } else if (rangeItem->mEndContainer == &aRemovedElementContainerNode &&
               rangeItem->mEndOffset > aOldOffsetOfRemovedElement) {
      rangeItem->mEndOffset += aOldChildCountOfRemovedElement - 1;
    }
  }
}

void RangeUpdater::DidMoveNode(const nsINode& aOldParent, uint32_t aOldOffset,
                               const nsINode& aNewParent, uint32_t aNewOffset) {
  if (mLocked) {
    // Do nothing if moving nodes is occurred while changing the container.
    return;
  }
  auto AdjustDOMPoint = [&](nsCOMPtr<nsINode>& aNode, uint32_t& aOffset) {
    if (aNode == &aOldParent) {
      // If previously pointed the moved content, it should keep pointing it.
      if (aOffset == aOldOffset) {
        aNode = const_cast<nsINode*>(&aNewParent);
        aOffset = aNewOffset;
      } else if (aOffset > aOldOffset) {
        aOffset--;
      }
      return;
    }
    if (aNode == &aNewParent) {
      if (aOffset > aNewOffset) {
        aOffset++;
      }
    }
  };
  for (RefPtr<RangeItem>& rangeItem : mArray) {
    if (NS_WARN_IF(!rangeItem)) {
      return;
    }

    AdjustDOMPoint(rangeItem->mStartContainer, rangeItem->mStartOffset);
    AdjustDOMPoint(rangeItem->mEndContainer, rangeItem->mEndOffset);
  }
}

/******************************************************************************
 * mozilla::RangeItem
 *
 * Helper struct for SelectionState.  This stores range endpoints.
 ******************************************************************************/

NS_IMPL_CYCLE_COLLECTION(RangeItem, mStartContainer, mEndContainer)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(RangeItem, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(RangeItem, Release)

void RangeItem::StoreRange(const nsRange& aRange) {
  mStartContainer = aRange.GetStartContainer();
  mStartOffset = aRange.StartOffset();
  mEndContainer = aRange.GetEndContainer();
  mEndOffset = aRange.EndOffset();
}

already_AddRefed<nsRange> RangeItem::GetRange() const {
  RefPtr<nsRange> range = nsRange::Create(
      mStartContainer, mStartOffset, mEndContainer, mEndOffset, IgnoreErrors());
  NS_WARNING_ASSERTION(range, "nsRange::Create() failed");
  return range.forget();
}

}  // namespace mozilla
