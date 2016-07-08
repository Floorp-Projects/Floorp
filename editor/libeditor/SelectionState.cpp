/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SelectionState.h"

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc.
#include "mozilla/EditorUtils.h"        // for EditorUtils
#include "mozilla/dom/Selection.h"      // for Selection
#include "nsAString.h"                  // for nsAString_internal::Length
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc.
#include "nsError.h"                    // for NS_OK, etc.
#include "nsIContent.h"                 // for nsIContent
#include "nsIDOMCharacterData.h"        // for nsIDOMCharacterData
#include "nsIDOMNode.h"                 // for nsIDOMNode
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
  nsresult res;
  uint32_t i, arrayCount = mArray.Length();

  // clear out selection
  aSel->RemoveAllRanges();

  // set the selection ranges anew
  for (i=0; i<arrayCount; i++)
  {
    RefPtr<nsRange> range = mArray[i]->GetRange();
    NS_ENSURE_TRUE(range, NS_ERROR_UNEXPECTED);

    res = aSel->AddRange(range);
    if(NS_FAILED(res)) return res;

  }
  return NS_OK;
}

bool
SelectionState::IsCollapsed()
{
  if (1 != mArray.Length()) return false;
  RefPtr<nsRange> range = mArray[0]->GetRange();
  NS_ENSURE_TRUE(range, false);
  bool bIsCollapsed = false;
  range->GetCollapsed(&bIsCollapsed);
  return bIsCollapsed;
}

bool
SelectionState::IsEqual(SelectionState* aSelState)
{
  NS_ENSURE_TRUE(aSelState, false);
  uint32_t i, myCount = mArray.Length(), itsCount = aSelState->mArray.Length();
  if (myCount != itsCount) return false;
  if (myCount < 1) return false;

  for (i=0; i<myCount; i++)
  {
    RefPtr<nsRange> myRange = mArray[i]->GetRange();
    RefPtr<nsRange> itsRange = aSelState->mArray[i]->GetRange();
    NS_ENSURE_TRUE(myRange && itsRange, false);

    int16_t compResult;
    nsresult rv;
    rv = myRange->CompareBoundaryPoints(nsIDOMRange::START_TO_START, itsRange, &compResult);
    if (NS_FAILED(rv) || compResult) return false;
    rv = myRange->CompareBoundaryPoints(nsIDOMRange::END_TO_END, itsRange, &compResult);
    if (NS_FAILED(rv) || compResult) return false;
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
  if (!aRangeItem) return;
  if (mArray.Contains(aRangeItem))
  {
    NS_ERROR("tried to register an already registered range");
    return;  // don't register it again.  It would get doubly adjusted.
  }
  mArray.AppendElement(aRangeItem);
}

void
RangeUpdater::DropRangeItem(RangeItem* aRangeItem)
{
  if (!aRangeItem) return;
  mArray.RemoveElement(aRangeItem);
}

nsresult
RangeUpdater::RegisterSelectionState(SelectionState& aSelState)
{
  uint32_t i, theCount = aSelState.mArray.Length();
  if (theCount < 1) return NS_ERROR_FAILURE;

  for (i=0; i<theCount; i++)
  {
    RegisterRangeItem(aSelState.mArray[i]);
  }

  return NS_OK;
}

nsresult
RangeUpdater::DropSelectionState(SelectionState& aSelState)
{
  uint32_t i, theCount = aSelState.mArray.Length();
  if (theCount < 1) return NS_ERROR_FAILURE;

  for (i=0; i<theCount; i++)
  {
    DropRangeItem(aSelState.mArray[i]);
  }

  return NS_OK;
}

// gravity methods:

nsresult
RangeUpdater::SelAdjCreateNode(nsINode* aParent,
                               int32_t aPosition)
{
  if (mLock) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }
  NS_ENSURE_TRUE(aParent, NS_ERROR_NULL_POINTER);
  uint32_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->startNode == aParent && item->startOffset > aPosition) {
      item->startOffset++;
    }
    if (item->endNode == aParent && item->endOffset > aPosition) {
      item->endOffset++;
    }
  }
  return NS_OK;
}

nsresult
RangeUpdater::SelAdjCreateNode(nsIDOMNode* aParent,
                               int32_t aPosition)
{
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  return SelAdjCreateNode(parent, aPosition);
}

nsresult
RangeUpdater::SelAdjInsertNode(nsINode* aParent,
                               int32_t aPosition)
{
  return SelAdjCreateNode(aParent, aPosition);
}

nsresult
RangeUpdater::SelAdjInsertNode(nsIDOMNode* aParent,
                               int32_t aPosition)
{
  return SelAdjCreateNode(aParent, aPosition);
}

void
RangeUpdater::SelAdjDeleteNode(nsINode* aNode)
{
  if (mLock) {
    // lock set by Will/DidReplaceParent, etc...
    return;
  }
  MOZ_ASSERT(aNode);
  uint32_t count = mArray.Length();
  if (!count) {
    return;
  }

  nsCOMPtr<nsINode> parent = aNode->GetParentNode();
  int32_t offset = parent ? parent->IndexOf(aNode) : -1;

  // check for range endpoints that are after aNode and in the same parent
  for (uint32_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    MOZ_ASSERT(item);

    if (item->startNode == parent && item->startOffset > offset) {
      item->startOffset--;
    }
    if (item->endNode == parent && item->endOffset > offset) {
      item->endOffset--;
    }

    // check for range endpoints that are in aNode
    if (item->startNode == aNode) {
      item->startNode   = parent;
      item->startOffset = offset;
    }
    if (item->endNode == aNode) {
      item->endNode   = parent;
      item->endOffset = offset;
    }

    // check for range endpoints that are in descendants of aNode
    nsCOMPtr<nsINode> oldStart;
    if (EditorUtils::IsDescendantOf(item->startNode, aNode)) {
      oldStart = item->startNode;  // save for efficiency hack below.
      item->startNode   = parent;
      item->startOffset = offset;
    }

    // avoid having to call IsDescendantOf() for common case of range startnode == range endnode.
    if (item->endNode == oldStart ||
        EditorUtils::IsDescendantOf(item->endNode, aNode))
    {
      item->endNode   = parent;
      item->endOffset = offset;
    }
  }
}

void
RangeUpdater::SelAdjDeleteNode(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, );
  return SelAdjDeleteNode(node);
}

nsresult
RangeUpdater::SelAdjSplitNode(nsIContent& aOldRightNode,
                              int32_t aOffset,
                              nsIContent* aNewLeftNode)
{
  if (mLock) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }
  NS_ENSURE_TRUE(aNewLeftNode, NS_ERROR_NULL_POINTER);
  uint32_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> parent = aOldRightNode.GetParentNode();
  int32_t offset = parent ? parent->IndexOf(&aOldRightNode) : -1;

  // first part is same as inserting aNewLeftnode
  nsresult result = SelAdjInsertNode(parent, offset - 1);
  NS_ENSURE_SUCCESS(result, result);

  // next step is to check for range enpoints inside aOldRightNode
  for (uint32_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->startNode == &aOldRightNode) {
      if (item->startOffset > aOffset) {
        item->startOffset -= aOffset;
      } else {
        item->startNode = aNewLeftNode;
      }
    }
    if (item->endNode == &aOldRightNode) {
      if (item->endOffset > aOffset) {
        item->endOffset -= aOffset;
      } else {
        item->endNode = aNewLeftNode;
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
  uint32_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->startNode == &aParent) {
      // adjust start point in aParent
      if (item->startOffset > aOffset) {
        item->startOffset--;
      } else if (item->startOffset == aOffset) {
        // join keeps right hand node
        item->startNode = &aRightNode;
        item->startOffset = aOldLeftNodeLength;
      }
    } else if (item->startNode == &aRightNode) {
      // adjust start point in aRightNode
      item->startOffset += aOldLeftNodeLength;
    } else if (item->startNode == &aLeftNode) {
      // adjust start point in aLeftNode
      item->startNode = &aRightNode;
    }

    if (item->endNode == &aParent) {
      // adjust end point in aParent
      if (item->endOffset > aOffset) {
        item->endOffset--;
      } else if (item->endOffset == aOffset) {
        // join keeps right hand node
        item->endNode = &aRightNode;
        item->endOffset = aOldLeftNodeLength;
      }
    } else if (item->endNode == &aRightNode) {
      // adjust end point in aRightNode
       item->endOffset += aOldLeftNodeLength;
    } else if (item->endNode == &aLeftNode) {
      // adjust end point in aLeftNode
      item->endNode = &aRightNode;
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

  uint32_t count = mArray.Length();
  if (!count) {
    return;
  }

  uint32_t len = aString.Length();
  for (uint32_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    MOZ_ASSERT(item);

    if (item->startNode == &aTextNode && item->startOffset > aOffset) {
      item->startOffset += len;
    }
    if (item->endNode == &aTextNode && item->endOffset > aOffset) {
      item->endOffset += len;
    }
  }
  return;
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

  uint32_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }
  NS_ENSURE_TRUE(aTextNode, NS_ERROR_NULL_POINTER);

  for (uint32_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->startNode == aTextNode && item->startOffset > aOffset) {
      item->startOffset -= aLength;
      if (item->startOffset < 0) {
        item->startOffset = 0;
      }
    }
    if (item->endNode == aTextNode && item->endOffset > aOffset) {
      item->endOffset -= aLength;
      if (item->endOffset < 0) {
        item->endOffset = 0;
      }
    }
  }
  return NS_OK;
}

nsresult
RangeUpdater::SelAdjDeleteText(nsIDOMCharacterData* aTextNode,
                               int32_t aOffset,
                               int32_t aLength)
{
  nsCOMPtr<nsIContent> textNode = do_QueryInterface(aTextNode);
  return SelAdjDeleteText(textNode, aOffset, aLength);
}

nsresult
RangeUpdater::WillReplaceContainer()
{
  if (mLock) return NS_ERROR_UNEXPECTED;
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
  uint32_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->startNode == aOriginalNode) {
      item->startNode = aNewNode;
    }
    if (item->endNode == aOriginalNode) {
      item->endNode = aNewNode;
    }
  }
  return NS_OK;
}

nsresult
RangeUpdater::WillRemoveContainer()
{
  if (mLock) return NS_ERROR_UNEXPECTED;
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
  uint32_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < count; i++) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->startNode == aNode) {
      item->startNode = aParent;
      item->startOffset += aOffset;
    } else if (item->startNode == aParent && item->startOffset > aOffset) {
      item->startOffset += (int32_t)aNodeOrigLen - 1;
    }

    if (item->endNode == aNode) {
      item->endNode = aParent;
      item->endOffset += aOffset;
    } else if (item->endNode == aParent && item->endOffset > aOffset) {
      item->endOffset += (int32_t)aNodeOrigLen - 1;
    }
  }
  return NS_OK;
}

nsresult
RangeUpdater::DidRemoveContainer(nsIDOMNode* aNode,
                                 nsIDOMNode* aParent,
                                 int32_t aOffset,
                                 uint32_t aNodeOrigLen)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  return DidRemoveContainer(node, parent, aOffset, aNodeOrigLen);
}

nsresult
RangeUpdater::WillInsertContainer()
{
  if (mLock) return NS_ERROR_UNEXPECTED;
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

  for (uint32_t i = 0, count = mArray.Length(); i < count; ++i) {
    RangeItem* item = mArray[i];
    NS_ENSURE_TRUE_VOID(item);

    // like a delete in aOldParent
    if (item->startNode == aOldParent && item->startOffset > aOldOffset) {
      item->startOffset--;
    }
    if (item->endNode == aOldParent && item->endOffset > aOldOffset) {
      item->endOffset--;
    }

    // and like an insert in aNewParent
    if (item->startNode == aNewParent && item->startOffset > aNewOffset) {
      item->startOffset++;
    }
    if (item->endNode == aNewParent && item->endOffset > aNewOffset) {
      item->endOffset++;
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

NS_IMPL_CYCLE_COLLECTION(RangeItem, startNode, endNode)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(RangeItem, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(RangeItem, Release)

void
RangeItem::StoreRange(nsRange* aRange)
{
  MOZ_ASSERT(aRange);
  startNode = aRange->GetStartParent();
  startOffset = aRange->StartOffset();
  endNode = aRange->GetEndParent();
  endOffset = aRange->EndOffset();
}

already_AddRefed<nsRange>
RangeItem::GetRange()
{
  RefPtr<nsRange> range = new nsRange(startNode);
  nsresult res = range->Set(startNode, startOffset, endNode, endOffset);
  NS_ENSURE_SUCCESS(res, nullptr);
  return range.forget();
}

} // namespace mozilla
