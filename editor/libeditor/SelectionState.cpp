/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SelectionState.h"

#include "mozilla/Assertions.h"   // for MOZ_ASSERT, etc.
#include "mozilla/EditorUtils.h"  // for EditorUtils
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

SelectionState::SelectionState() : mDirection(eDirNext) {}

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
  for (uint32_t i = 0; i < aSelection.RangeCount(); i++) {
    const nsRange* range = aSelection.GetRangeAt(i);
    if (NS_WARN_IF(!range)) {
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

bool SelectionState::IsCollapsed() const {
  if (mArray.Length() != 1) {
    return false;
  }
  RefPtr<nsRange> range = mArray[0]->GetRange();
  if (!range) {
    NS_WARNING("RangeItem::GetRange() failed");
    return false;
  }
  return range->Collapsed();
}

bool SelectionState::Equals(SelectionState& aOther) const {
  if (mArray.Length() != aOther.mArray.Length()) {
    return false;
  }
  if (mArray.IsEmpty()) {
    return false;  // XXX Why?
  }
  if (mDirection != aOther.mDirection) {
    return false;
  }

  // XXX Creating nsRanges are really expensive.  Why cannot we just check
  //     the container and offsets??
  IgnoredErrorResult ignoredError;
  for (size_t i = 0; i < mArray.Length(); i++) {
    RefPtr<nsRange> range = mArray[i]->GetRange();
    if (!range) {
      NS_WARNING("Failed to create a range from the range item");
      return false;
    }
    RefPtr<nsRange> otherRange = aOther.mArray[i]->GetRange();
    if (!otherRange) {
      NS_WARNING("Failed to create a range from the other's range item");
      return false;
    }

    int16_t compResult = range->CompareBoundaryPoints(
        Range_Binding::START_TO_START, *otherRange, ignoredError);
    if (ignoredError.Failed()) {
      NS_WARNING(
          "nsRange::CompareBoundaryPoints(Range_Binding::START_TO_START) "
          "failed");
      return false;
    }
    if (compResult) {
      return false;
    }
    compResult = range->CompareBoundaryPoints(Range_Binding::END_TO_END,
                                              *otherRange, ignoredError);
    if (ignoredError.Failed()) {
      NS_WARNING(
          "nsRange::CompareBoundaryPoints(Range_Binding::END_TO_END) failed");
      return false;
    }
    if (compResult) {
      return false;
    }
  }
  // if we got here, they are equal
  return true;
}

void SelectionState::Clear() {
  // free any items in the array
  mArray.Clear();
  mDirection = eDirNext;
}

bool SelectionState::IsEmpty() const { return mArray.IsEmpty(); }

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

nsresult RangeUpdater::SelAdjSplitNode(nsIContent& aRightNode,
                                       nsIContent& aNewLeftNode) {
  if (mLocked) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }

  if (mArray.IsEmpty()) {
    return NS_OK;
  }

  EditorRawDOMPoint atLeftNode(&aNewLeftNode);
  nsresult rv = SelAdjInsertNode(atLeftNode);
  if (NS_FAILED(rv)) {
    NS_WARNING("RangeUpdater::SelAdjInsertNode() failed");
    return rv;
  }

  // If point in the ranges is in left node, change its container to the left
  // node.  If point in the ranges is in right node, subtract numbers of
  // children moved to left node from the offset.
  uint32_t lengthOfLeftNode = aNewLeftNode.Length();
  for (RefPtr<RangeItem>& rangeItem : mArray) {
    if (NS_WARN_IF(!rangeItem)) {
      return NS_ERROR_FAILURE;
    }

    if (rangeItem->mStartContainer == &aRightNode) {
      if (rangeItem->mStartOffset > lengthOfLeftNode) {
        rangeItem->mStartOffset -= lengthOfLeftNode;
      } else {
        rangeItem->mStartContainer = &aNewLeftNode;
      }
    }
    if (rangeItem->mEndContainer == &aRightNode) {
      if (rangeItem->mEndOffset > lengthOfLeftNode) {
        rangeItem->mEndOffset -= lengthOfLeftNode;
      } else {
        rangeItem->mEndContainer = &aNewLeftNode;
      }
    }
  }
  return NS_OK;
}

nsresult RangeUpdater::SelAdjJoinNodes(nsINode& aLeftNode, nsINode& aRightNode,
                                       nsINode& aParent, uint32_t aOffset,
                                       uint32_t aOldLeftNodeLength) {
  if (mLocked) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }

  if (mArray.IsEmpty()) {
    return NS_OK;
  }

  for (RefPtr<RangeItem>& rangeItem : mArray) {
    if (NS_WARN_IF(!rangeItem)) {
      return NS_ERROR_FAILURE;
    }

    if (rangeItem->mStartContainer == &aParent) {
      // adjust start point in aParent
      if (rangeItem->mStartOffset > aOffset) {
        rangeItem->mStartOffset--;
      } else if (rangeItem->mStartOffset == aOffset) {
        // join keeps right hand node
        rangeItem->mStartContainer = &aRightNode;
        rangeItem->mStartOffset = aOldLeftNodeLength;
      }
    } else if (rangeItem->mStartContainer == &aRightNode) {
      // adjust start point in aRightNode
      rangeItem->mStartOffset += aOldLeftNodeLength;
    } else if (rangeItem->mStartContainer == &aLeftNode) {
      // adjust start point in aLeftNode
      rangeItem->mStartContainer = &aRightNode;
    }

    if (rangeItem->mEndContainer == &aParent) {
      // adjust end point in aParent
      if (rangeItem->mEndOffset > aOffset) {
        rangeItem->mEndOffset--;
      } else if (rangeItem->mEndOffset == aOffset) {
        // join keeps right hand node
        rangeItem->mEndContainer = &aRightNode;
        rangeItem->mEndOffset = aOldLeftNodeLength;
      }
    } else if (rangeItem->mEndContainer == &aRightNode) {
      // adjust end point in aRightNode
      rangeItem->mEndOffset += aOldLeftNodeLength;
    } else if (rangeItem->mEndContainer == &aLeftNode) {
      // adjust end point in aLeftNode
      rangeItem->mEndContainer = &aRightNode;
    }
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
  if (NS_WARN_IF(!mLocked)) {
    return;
  }
  mLocked = false;

  for (RefPtr<RangeItem>& rangeItem : mArray) {
    if (NS_WARN_IF(!rangeItem)) {
      return;
    }

    // like a delete in aOldParent
    if (rangeItem->mStartContainer == &aOldParent &&
        rangeItem->mStartOffset > aOldOffset) {
      rangeItem->mStartOffset--;
    }
    if (rangeItem->mEndContainer == &aOldParent &&
        rangeItem->mEndOffset > aOldOffset) {
      rangeItem->mEndOffset--;
    }

    // and like an insert in aNewParent
    if (rangeItem->mStartContainer == &aNewParent &&
        rangeItem->mStartOffset > aNewOffset) {
      rangeItem->mStartOffset++;
    }
    if (rangeItem->mEndContainer == &aNewParent &&
        rangeItem->mEndOffset > aNewOffset) {
      rangeItem->mEndOffset++;
    }
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

already_AddRefed<nsRange> RangeItem::GetRange() {
  RefPtr<nsRange> range = nsRange::Create(
      mStartContainer, mStartOffset, mEndContainer, mEndOffset, IgnoreErrors());
  NS_WARNING_ASSERTION(range, "nsRange::Create() failed");
  return range.forget();
}

}  // namespace mozilla
