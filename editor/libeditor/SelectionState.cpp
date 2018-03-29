/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SelectionState.h"

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc.
#include "mozilla/EditorUtils.h"        // for EditorUtils
#include "mozilla/dom/RangeBinding.h"
#include "mozilla/dom/Selection.h"      // for Selection
#include "nsAString.h"                  // for nsAString::Length
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc.
#include "nsError.h"                    // for NS_OK, etc.
#include "nsIContent.h"                 // for nsIContent
#include "nsISupportsImpl.h"            // for nsRange::Release
#include "nsRange.h"                    // for nsRange

namespace mozilla {

using namespace dom;

/******************************************************************************
 * mozilla::SelectionState
 *
 * Class for recording selection info.  Stores selection as collection of
 * { {startnode, startoffset} , {endnode, endoffset} } tuples.  Can't store
 * ranges since dom gravity will possibly change the ranges.
 ******************************************************************************/

template nsresult
RangeUpdater::SelAdjCreateNode(const EditorDOMPoint& aPoint);
template nsresult
RangeUpdater::SelAdjCreateNode(const EditorRawDOMPoint& aPoint);
template nsresult
RangeUpdater::SelAdjInsertNode(const EditorDOMPoint& aPoint);
template nsresult
RangeUpdater::SelAdjInsertNode(const EditorRawDOMPoint& aPoint);

SelectionState::SelectionState()
{
}

SelectionState::~SelectionState()
{
  MakeEmpty();
}

void
SelectionState::SaveSelection(Selection* aSel)
{
  MOZ_ASSERT(aSel);
  int32_t arrayCount = mArray.Length();
  int32_t rangeCount = aSel->RangeCount();

  // if we need more items in the array, new them
  if (arrayCount < rangeCount) {
    for (int32_t i = arrayCount; i < rangeCount; i++) {
      mArray.AppendElement();
      mArray[i] = new RangeItem();
    }
  } else if (arrayCount > rangeCount) {
    // else if we have too many, delete them
    for (int32_t i = arrayCount - 1; i >= rangeCount; i--) {
      mArray.RemoveElementAt(i);
    }
  }

  // now store the selection ranges
  for (int32_t i = 0; i < rangeCount; i++) {
    mArray[i]->StoreRange(aSel->GetRangeAt(i));
  }
}

nsresult
SelectionState::RestoreSelection(Selection* aSel)
{
  NS_ENSURE_TRUE(aSel, NS_ERROR_NULL_POINTER);

  // clear out selection
  aSel->RemoveAllRanges(IgnoreErrors());

  // set the selection ranges anew
  size_t arrayCount = mArray.Length();
  for (size_t i = 0; i < arrayCount; i++) {
    RefPtr<nsRange> range = mArray[i]->GetRange();
    NS_ENSURE_TRUE(range, NS_ERROR_UNEXPECTED);

    ErrorResult rv;
    aSel->AddRange(*range, rv);
    if (rv.Failed()) {
      return rv.StealNSResult();
    }
  }
  return NS_OK;
}

bool
SelectionState::IsCollapsed()
{
  if (mArray.Length() != 1) {
    return false;
  }
  RefPtr<nsRange> range = mArray[0]->GetRange();
  NS_ENSURE_TRUE(range, false);
  return range->Collapsed();
}

bool
SelectionState::IsEqual(SelectionState* aSelState)
{
  NS_ENSURE_TRUE(aSelState, false);
  size_t myCount = mArray.Length(), itsCount = aSelState->mArray.Length();
  if (myCount != itsCount) {
    return false;
  }
  if (!myCount) {
    return false;
  }

  for (size_t i = 0; i < myCount; i++) {
    RefPtr<nsRange> myRange = mArray[i]->GetRange();
    RefPtr<nsRange> itsRange = aSelState->mArray[i]->GetRange();
    NS_ENSURE_TRUE(myRange && itsRange, false);

    IgnoredErrorResult rv;
    int16_t compResult =
      myRange->CompareBoundaryPoints(RangeBinding::START_TO_START, *itsRange, rv);
    if (rv.Failed() || compResult) {
      return false;
    }
    compResult =
      myRange->CompareBoundaryPoints(RangeBinding::END_TO_END, *itsRange, rv);
    if (rv.Failed() || compResult) {
      return false;
    }
  }
  // if we got here, they are equal
  return true;
}

void
SelectionState::MakeEmpty()
{
  // free any items in the array
  mArray.Clear();
}

bool
SelectionState::IsEmpty()
{
  return mArray.IsEmpty();
}

/******************************************************************************
 * mozilla::RangeUpdater
 *
 * Class for updating nsRanges in response to editor actions.
 ******************************************************************************/

RangeUpdater::RangeUpdater()
  : mLock(false)
{
}

RangeUpdater::~RangeUpdater()
{
  // nothing to do, we don't own the items in our array.
}

void
RangeUpdater::RegisterRangeItem(RangeItem* aRangeItem)
{
  if (!aRangeItem) {
    return;
  }
  if (mArray.Contains(aRangeItem)) {
    NS_ERROR("tried to register an already registered range");
    return;  // don't register it again.  It would get doubly adjusted.
  }
  mArray.AppendElement(aRangeItem);
}

void
RangeUpdater::DropRangeItem(RangeItem* aRangeItem)
{
  if (!aRangeItem) {
    return;
  }
  mArray.RemoveElement(aRangeItem);
}

nsresult
RangeUpdater::RegisterSelectionState(SelectionState& aSelState)
{
  size_t theCount = aSelState.mArray.Length();
  if (theCount < 1) {
    return NS_ERROR_FAILURE;
  }

  for (size_t i = 0; i < theCount; i++) {
    RegisterRangeItem(aSelState.mArray[i]);
  }

  return NS_OK;
}

nsresult
RangeUpdater::DropSelectionState(SelectionState& aSelState)
{
  size_t theCount = aSelState.mArray.Length();
  if (theCount < 1) {
    return NS_ERROR_FAILURE;
  }

  for (size_t i = 0; i < theCount; i++) {
    DropRangeItem(aSelState.mArray[i]);
  }

  return NS_OK;
}

// gravity methods:

template<typename PT, typename CT>
nsresult
RangeUpdater::SelAdjCreateNode(const EditorDOMPointBase<PT, CT>& aPoint)
{
  if (mLock) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }
  size_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  if (NS_WARN_IF(!aPoint.IsSetAndValid())) {
    return NS_ERROR_FAILURE;
  }

  for (size_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->mStartContainer == aPoint.GetContainer() &&
        item->mStartOffset > static_cast<int32_t>(aPoint.Offset())) {
      item->mStartOffset++;
    }
    if (item->mEndContainer == aPoint.GetContainer() &&
        item->mEndOffset > static_cast<int32_t>(aPoint.Offset())) {
      item->mEndOffset++;
    }
  }
  return NS_OK;
}

template<typename PT, typename CT>
nsresult
RangeUpdater::SelAdjInsertNode(const EditorDOMPointBase<PT, CT>& aPoint)
{
  return SelAdjCreateNode(aPoint);
}

void
RangeUpdater::SelAdjDeleteNode(nsINode* aNode)
{
  if (mLock) {
    // lock set by Will/DidReplaceParent, etc...
    return;
  }
  MOZ_ASSERT(aNode);
  size_t count = mArray.Length();
  if (!count) {
    return;
  }

  nsCOMPtr<nsINode> parent = aNode->GetParentNode();
  int32_t offset = parent ? parent->ComputeIndexOf(aNode) : -1;

  // check for range endpoints that are after aNode and in the same parent
  for (size_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    MOZ_ASSERT(item);

    if (item->mStartContainer == parent && item->mStartOffset > offset) {
      item->mStartOffset--;
    }
    if (item->mEndContainer == parent && item->mEndOffset > offset) {
      item->mEndOffset--;
    }

    // check for range endpoints that are in aNode
    if (item->mStartContainer == aNode) {
      item->mStartContainer = parent;
      item->mStartOffset = offset;
    }
    if (item->mEndContainer == aNode) {
      item->mEndContainer = parent;
      item->mEndOffset = offset;
    }

    // check for range endpoints that are in descendants of aNode
    nsCOMPtr<nsINode> oldStart;
    if (EditorUtils::IsDescendantOf(*item->mStartContainer, *aNode)) {
      oldStart = item->mStartContainer;  // save for efficiency hack below.
      item->mStartContainer = parent;
      item->mStartOffset = offset;
    }

    // avoid having to call IsDescendantOf() for common case of range startnode == range endnode.
    if (item->mEndContainer == oldStart ||
        EditorUtils::IsDescendantOf(*item->mEndContainer, *aNode)) {
      item->mEndContainer = parent;
      item->mEndOffset = offset;
    }
  }
}

nsresult
RangeUpdater::SelAdjSplitNode(nsIContent& aRightNode,
                              nsIContent* aNewLeftNode)
{
  if (mLock) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }
  if (NS_WARN_IF(!aNewLeftNode)) {
    return NS_ERROR_FAILURE;
  }

  size_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  EditorRawDOMPoint atLeftNode(aNewLeftNode);
  nsresult rv = SelAdjInsertNode(atLeftNode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If point in the ranges is in left node, change its container to the left
  // node.  If point in the ranges is in right node, subtract numbers of
  // children moved to left node from the offset.
  int32_t lengthOfLeftNode = aNewLeftNode->Length();
  for (RefPtr<RangeItem>& item : mArray) {
    if (NS_WARN_IF(!item)) {
      return NS_ERROR_FAILURE;
    }

    if (item->mStartContainer == &aRightNode) {
      if (item->mStartOffset > lengthOfLeftNode) {
        item->mStartOffset -= lengthOfLeftNode;
      } else {
        item->mStartContainer = aNewLeftNode;
      }
    }
    if (item->mEndContainer == &aRightNode) {
      if (item->mEndOffset > lengthOfLeftNode) {
        item->mEndOffset -= lengthOfLeftNode;
      } else {
        item->mEndContainer = aNewLeftNode;
      }
    }
  }
  return NS_OK;
}

nsresult
RangeUpdater::SelAdjJoinNodes(nsINode& aLeftNode,
                              nsINode& aRightNode,
                              nsINode& aParent,
                              int32_t aOffset,
                              int32_t aOldLeftNodeLength)
{
  if (mLock) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }
  size_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  for (size_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->mStartContainer == &aParent) {
      // adjust start point in aParent
      if (item->mStartOffset > aOffset) {
        item->mStartOffset--;
      } else if (item->mStartOffset == aOffset) {
        // join keeps right hand node
        item->mStartContainer = &aRightNode;
        item->mStartOffset = aOldLeftNodeLength;
      }
    } else if (item->mStartContainer == &aRightNode) {
      // adjust start point in aRightNode
      item->mStartOffset += aOldLeftNodeLength;
    } else if (item->mStartContainer == &aLeftNode) {
      // adjust start point in aLeftNode
      item->mStartContainer = &aRightNode;
    }

    if (item->mEndContainer == &aParent) {
      // adjust end point in aParent
      if (item->mEndOffset > aOffset) {
        item->mEndOffset--;
      } else if (item->mEndOffset == aOffset) {
        // join keeps right hand node
        item->mEndContainer = &aRightNode;
        item->mEndOffset = aOldLeftNodeLength;
      }
    } else if (item->mEndContainer == &aRightNode) {
      // adjust end point in aRightNode
       item->mEndOffset += aOldLeftNodeLength;
    } else if (item->mEndContainer == &aLeftNode) {
      // adjust end point in aLeftNode
      item->mEndContainer = &aRightNode;
    }
  }

  return NS_OK;
}

void
RangeUpdater::SelAdjInsertText(Text& aTextNode,
                               int32_t aOffset,
                               const nsAString& aString)
{
  if (mLock) {
    // lock set by Will/DidReplaceParent, etc...
    return;
  }

  size_t count = mArray.Length();
  if (!count) {
    return;
  }

  size_t len = aString.Length();
  for (size_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    MOZ_ASSERT(item);

    if (item->mStartContainer == &aTextNode && item->mStartOffset > aOffset) {
      item->mStartOffset += len;
    }
    if (item->mEndContainer == &aTextNode && item->mEndOffset > aOffset) {
      item->mEndOffset += len;
    }
  }
}

nsresult
RangeUpdater::SelAdjDeleteText(nsIContent* aTextNode,
                               int32_t aOffset,
                               int32_t aLength)
{
  if (mLock) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }

  size_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }
  NS_ENSURE_TRUE(aTextNode, NS_ERROR_NULL_POINTER);

  for (size_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->mStartContainer == aTextNode && item->mStartOffset > aOffset) {
      item->mStartOffset -= aLength;
      if (item->mStartOffset < 0) {
        item->mStartOffset = 0;
      }
    }
    if (item->mEndContainer == aTextNode && item->mEndOffset > aOffset) {
      item->mEndOffset -= aLength;
      if (item->mEndOffset < 0) {
        item->mEndOffset = 0;
      }
    }
  }
  return NS_OK;
}

nsresult
RangeUpdater::WillReplaceContainer()
{
  if (mLock) {
    return NS_ERROR_UNEXPECTED;
  }
  mLock = true;
  return NS_OK;
}

nsresult
RangeUpdater::DidReplaceContainer(Element* aOriginalNode,
                                  Element* aNewNode)
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_UNEXPECTED);
  mLock = false;

  NS_ENSURE_TRUE(aOriginalNode && aNewNode, NS_ERROR_NULL_POINTER);
  size_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  for (size_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->mStartContainer == aOriginalNode) {
      item->mStartContainer = aNewNode;
    }
    if (item->mEndContainer == aOriginalNode) {
      item->mEndContainer = aNewNode;
    }
  }
  return NS_OK;
}

nsresult
RangeUpdater::WillRemoveContainer()
{
  if (mLock) {
    return NS_ERROR_UNEXPECTED;
  }
  mLock = true;
  return NS_OK;
}

nsresult
RangeUpdater::DidRemoveContainer(nsINode* aNode,
                                 nsINode* aParent,
                                 int32_t aOffset,
                                 uint32_t aNodeOrigLen)
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_UNEXPECTED);
  mLock = false;

  NS_ENSURE_TRUE(aNode && aParent, NS_ERROR_NULL_POINTER);
  size_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  for (size_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->mStartContainer == aNode) {
      item->mStartContainer = aParent;
      item->mStartOffset += aOffset;
    } else if (item->mStartContainer == aParent &&
               item->mStartOffset > aOffset) {
      item->mStartOffset += (int32_t)aNodeOrigLen - 1;
    }

    if (item->mEndContainer == aNode) {
      item->mEndContainer = aParent;
      item->mEndOffset += aOffset;
    } else if (item->mEndContainer == aParent && item->mEndOffset > aOffset) {
      item->mEndOffset += (int32_t)aNodeOrigLen - 1;
    }
  }
  return NS_OK;
}

nsresult
RangeUpdater::WillInsertContainer()
{
  if (mLock) {
    return NS_ERROR_UNEXPECTED;
  }
  mLock = true;
  return NS_OK;
}

nsresult
RangeUpdater::DidInsertContainer()
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_UNEXPECTED);
  mLock = false;
  return NS_OK;
}

void
RangeUpdater::WillMoveNode()
{
  mLock = true;
}

void
RangeUpdater::DidMoveNode(nsINode* aOldParent, int32_t aOldOffset,
                            nsINode* aNewParent, int32_t aNewOffset)
{
  MOZ_ASSERT(aOldParent);
  MOZ_ASSERT(aNewParent);
  NS_ENSURE_TRUE_VOID(mLock);
  mLock = false;

  for (size_t i = 0, count = mArray.Length(); i < count; ++i) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE_VOID(item);

    // like a delete in aOldParent
    if (item->mStartContainer == aOldParent &&
        item->mStartOffset > aOldOffset) {
      item->mStartOffset--;
    }
    if (item->mEndContainer == aOldParent && item->mEndOffset > aOldOffset) {
      item->mEndOffset--;
    }

    // and like an insert in aNewParent
    if (item->mStartContainer == aNewParent &&
        item->mStartOffset > aNewOffset) {
      item->mStartOffset++;
    }
    if (item->mEndContainer == aNewParent && item->mEndOffset > aNewOffset) {
      item->mEndOffset++;
    }
  }
}

/******************************************************************************
 * mozilla::RangeItem
 *
 * Helper struct for SelectionState.  This stores range endpoints.
 ******************************************************************************/

RangeItem::RangeItem()
{
}

RangeItem::~RangeItem()
{
}

NS_IMPL_CYCLE_COLLECTION(RangeItem, mStartContainer, mEndContainer)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(RangeItem, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(RangeItem, Release)

void
RangeItem::StoreRange(nsRange* aRange)
{
  MOZ_ASSERT(aRange);
  mStartContainer = aRange->GetStartContainer();
  mStartOffset = aRange->StartOffset();
  mEndContainer = aRange->GetEndContainer();
  mEndOffset = aRange->EndOffset();
}

already_AddRefed<nsRange>
RangeItem::GetRange()
{
  RefPtr<nsRange> range = new nsRange(mStartContainer);
  if (NS_FAILED(range->SetStartAndEnd(mStartContainer, mStartOffset,
                                      mEndContainer, mEndOffset))) {
    return nullptr;
  }
  return range.forget();
}

} // namespace mozilla
