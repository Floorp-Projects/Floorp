/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/dom/Selection.h"      // for Selection
#include "nsAString.h"                  // for nsAString_internal::Length
#include "nsAutoPtr.h"                  // for nsRefPtr, getter_AddRefs, etc
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsEditor.h"                   // for nsEditor
#include "nsEditorUtils.h"              // for nsEditorUtils
#include "nsError.h"                    // for NS_OK, etc
#include "nsIContent.h"                 // for nsIContent
#include "nsIDOMCharacterData.h"        // for nsIDOMCharacterData
#include "nsIDOMNode.h"                 // for nsIDOMNode
#include "nsIDOMRange.h"                // for nsIDOMRange, etc
#include "nsISelection.h"               // for nsISelection
#include "nsISupportsImpl.h"            // for nsRange::Release
#include "nsRange.h"                    // for nsRange
#include "nsSelectionState.h"

using namespace mozilla;
using namespace mozilla::dom;

/***************************************************************************
 * class for recording selection info.  stores selection as collection of
 * { {startnode, startoffset} , {endnode, endoffset} } tuples.  Can't store
 * ranges since dom gravity will possibly change the ranges.
 */
nsSelectionState::nsSelectionState() : mArray(){}

nsSelectionState::~nsSelectionState() 
{
  MakeEmpty();
}

void
nsSelectionState::DoTraverse(nsCycleCollectionTraversalCallback &cb)
{
  for (uint32_t i = 0, iEnd = mArray.Length(); i < iEnd; ++i)
  {
    nsRangeStore* item = mArray[i];
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                       "selection state mArray[i].startNode");
    cb.NoteXPCOMChild(item->startNode);
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                       "selection state mArray[i].endNode");
    cb.NoteXPCOMChild(item->endNode);
  }
}

void
nsSelectionState::SaveSelection(Selection* aSel)
{
  MOZ_ASSERT(aSel);
  int32_t arrayCount = mArray.Length();
  int32_t rangeCount = aSel->GetRangeCount();

  // if we need more items in the array, new them
  if (arrayCount < rangeCount) {
    for (int32_t i = arrayCount; i < rangeCount; i++) {
      mArray.AppendElement();
      mArray[i] = new nsRangeStore();
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
nsSelectionState::RestoreSelection(nsISelection *aSel)
{
  NS_ENSURE_TRUE(aSel, NS_ERROR_NULL_POINTER);
  nsresult res;
  uint32_t i, arrayCount = mArray.Length();

  // clear out selection
  aSel->RemoveAllRanges();
  
  // set the selection ranges anew
  for (i=0; i<arrayCount; i++)
  {
    nsRefPtr<nsRange> range = mArray[i]->GetRange();
    NS_ENSURE_TRUE(range, NS_ERROR_UNEXPECTED);
   
    res = aSel->AddRange(range);
    if(NS_FAILED(res)) return res;

  }
  return NS_OK;
}

bool
nsSelectionState::IsCollapsed()
{
  if (1 != mArray.Length()) return false;
  nsRefPtr<nsRange> range = mArray[0]->GetRange();
  NS_ENSURE_TRUE(range, false);
  bool bIsCollapsed = false;
  range->GetCollapsed(&bIsCollapsed);
  return bIsCollapsed;
}

bool
nsSelectionState::IsEqual(nsSelectionState *aSelState)
{
  NS_ENSURE_TRUE(aSelState, false);
  uint32_t i, myCount = mArray.Length(), itsCount = aSelState->mArray.Length();
  if (myCount != itsCount) return false;
  if (myCount < 1) return false;

  for (i=0; i<myCount; i++)
  {
    nsRefPtr<nsRange> myRange = mArray[i]->GetRange();
    nsRefPtr<nsRange> itsRange = aSelState->mArray[i]->GetRange();
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
nsSelectionState::MakeEmpty()
{
  // free any items in the array
  mArray.Clear();
}

bool     
nsSelectionState::IsEmpty()
{
  return mArray.IsEmpty();
}

/***************************************************************************
 * nsRangeUpdater:  class for updating nsIDOMRanges in response to editor actions.
 */

nsRangeUpdater::nsRangeUpdater() : mArray(), mLock(false) {}

nsRangeUpdater::~nsRangeUpdater()
{
  // nothing to do, we don't own the items in our array.
}
  
void 
nsRangeUpdater::RegisterRangeItem(nsRangeStore *aRangeItem)
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
nsRangeUpdater::DropRangeItem(nsRangeStore *aRangeItem)
{
  if (!aRangeItem) return;
  mArray.RemoveElement(aRangeItem);
}

nsresult 
nsRangeUpdater::RegisterSelectionState(nsSelectionState &aSelState)
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
nsRangeUpdater::DropSelectionState(nsSelectionState &aSelState)
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
nsRangeUpdater::SelAdjCreateNode(nsINode* aParent, int32_t aPosition)
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
    nsRangeStore* item = mArray[i];
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
nsRangeUpdater::SelAdjCreateNode(nsIDOMNode* aParent, int32_t aPosition)
{
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  return SelAdjCreateNode(parent, aPosition);
}

nsresult
nsRangeUpdater::SelAdjInsertNode(nsINode* aParent, int32_t aPosition)
{
  return SelAdjCreateNode(aParent, aPosition);
}

nsresult
nsRangeUpdater::SelAdjInsertNode(nsIDOMNode *aParent, int32_t aPosition)
{
  return SelAdjCreateNode(aParent, aPosition);
}

void
nsRangeUpdater::SelAdjDeleteNode(nsINode* aNode)
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
    nsRangeStore* item = mArray[i];
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
    if (nsEditorUtils::IsDescendantOf(item->startNode, aNode)) {
      oldStart = item->startNode;  // save for efficiency hack below.
      item->startNode   = parent;
      item->startOffset = offset;
    }

    // avoid having to call IsDescendantOf() for common case of range startnode == range endnode.
    if ((item->endNode == oldStart) || nsEditorUtils::IsDescendantOf(item->endNode, aNode))
    {
      item->endNode   = parent;
      item->endOffset = offset;
    }
  }
}

void
nsRangeUpdater::SelAdjDeleteNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, );
  return SelAdjDeleteNode(node);
}


nsresult
nsRangeUpdater::SelAdjSplitNode(nsINode* aOldRightNode, int32_t aOffset,
                                nsINode* aNewLeftNode)
{
  if (mLock) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }
  NS_ENSURE_TRUE(aOldRightNode && aNewLeftNode, NS_ERROR_NULL_POINTER);
  uint32_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> parent = aOldRightNode->GetParentNode();
  int32_t offset = parent ? parent->IndexOf(aOldRightNode) : -1;

  // first part is same as inserting aNewLeftnode
  nsresult result = SelAdjInsertNode(parent, offset - 1);
  NS_ENSURE_SUCCESS(result, result);

  // next step is to check for range enpoints inside aOldRightNode
  for (uint32_t i = 0; i < count; i++) {
    nsRangeStore* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->startNode == aOldRightNode) {
      if (item->startOffset > aOffset) {
        item->startOffset -= aOffset;
      } else {
        item->startNode = aNewLeftNode;
      }
    }
    if (item->endNode == aOldRightNode) {
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
nsRangeUpdater::SelAdjSplitNode(nsIDOMNode* aOldRightNode, int32_t aOffset,
                                nsIDOMNode* aNewLeftNode)
{
  nsCOMPtr<nsINode> oldRightNode = do_QueryInterface(aOldRightNode);
  nsCOMPtr<nsINode> newLeftNode = do_QueryInterface(aNewLeftNode);
  return SelAdjSplitNode(oldRightNode, aOffset, newLeftNode);
}


nsresult
nsRangeUpdater::SelAdjJoinNodes(nsINode* aLeftNode,
                                nsINode* aRightNode,
                                nsINode* aParent,
                                int32_t aOffset,
                                int32_t aOldLeftNodeLength)
{
  if (mLock) {
    // lock set by Will/DidReplaceParent, etc...
    return NS_OK;
  }
  NS_ENSURE_TRUE(aLeftNode && aRightNode && aParent, NS_ERROR_NULL_POINTER);
  uint32_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < count; i++) {
    nsRangeStore* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->startNode == aParent) {
      // adjust start point in aParent
      if (item->startOffset > aOffset) {
        item->startOffset--;
      } else if (item->startOffset == aOffset) {
        // join keeps right hand node
        item->startNode = aRightNode;
        item->startOffset = aOldLeftNodeLength;
      }
    } else if (item->startNode == aRightNode) {
      // adjust start point in aRightNode
      item->startOffset += aOldLeftNodeLength;
    } else if (item->startNode == aLeftNode) {
      // adjust start point in aLeftNode
      item->startNode = aRightNode;
    }

    if (item->endNode == aParent) {
      // adjust end point in aParent
      if (item->endOffset > aOffset) {
        item->endOffset--;
      } else if (item->endOffset == aOffset) {
        // join keeps right hand node
        item->endNode = aRightNode;
        item->endOffset = aOldLeftNodeLength;
      }
    } else if (item->endNode == aRightNode) {
      // adjust end point in aRightNode
       item->endOffset += aOldLeftNodeLength;
    } else if (item->endNode == aLeftNode) {
      // adjust end point in aLeftNode
      item->endNode = aRightNode;
    }
  }

  return NS_OK;
}

nsresult
nsRangeUpdater::SelAdjJoinNodes(nsIDOMNode* aLeftNode,
                                nsIDOMNode* aRightNode,
                                nsIDOMNode* aParent,
                                int32_t aOffset,
                                int32_t aOldLeftNodeLength)
{
  nsCOMPtr<nsINode> leftNode = do_QueryInterface(aLeftNode);
  nsCOMPtr<nsINode> rightNode = do_QueryInterface(aRightNode);
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  return SelAdjJoinNodes(leftNode, rightNode, parent, aOffset, aOldLeftNodeLength);
}


nsresult
nsRangeUpdater::SelAdjInsertText(nsIContent* aTextNode, int32_t aOffset,
                                 const nsAString &aString)
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

  uint32_t len = aString.Length();
  for (uint32_t i = 0; i < count; i++) {
    nsRangeStore* item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);

    if (item->startNode == aTextNode && item->startOffset > aOffset) {
      item->startOffset += len;
    }
    if (item->endNode == aTextNode && item->endOffset > aOffset) {
      item->endOffset += len;
    }
  }
  return NS_OK;
}

nsresult
nsRangeUpdater::SelAdjInsertText(nsIDOMCharacterData* aTextNode,
                                 int32_t aOffset, const nsAString &aString)
{
  nsCOMPtr<nsIContent> textNode = do_QueryInterface(aTextNode);
  return SelAdjInsertText(textNode, aOffset, aString);
}


nsresult
nsRangeUpdater::SelAdjDeleteText(nsIContent* aTextNode, int32_t aOffset,
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
    nsRangeStore* item = mArray[i];
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
nsRangeUpdater::SelAdjDeleteText(nsIDOMCharacterData* aTextNode,
                                 int32_t aOffset, int32_t aLength)
{
  nsCOMPtr<nsIContent> textNode = do_QueryInterface(aTextNode);
  return SelAdjDeleteText(textNode, aOffset, aLength);
}


nsresult
nsRangeUpdater::WillReplaceContainer()
{
  if (mLock) return NS_ERROR_UNEXPECTED;  
  mLock = true;
  return NS_OK;
}


nsresult
nsRangeUpdater::DidReplaceContainer(nsINode* aOriginalNode, nsINode* aNewNode)
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_UNEXPECTED);
  mLock = false;

  NS_ENSURE_TRUE(aOriginalNode && aNewNode, NS_ERROR_NULL_POINTER);
  uint32_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < count; i++) {
    nsRangeStore* item = mArray[i];
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
nsRangeUpdater::DidReplaceContainer(nsIDOMNode* aOriginalNode,
                                    nsIDOMNode* aNewNode)
{
  nsCOMPtr<nsINode> originalNode = do_QueryInterface(aOriginalNode);
  nsCOMPtr<nsINode> newNode = do_QueryInterface(aNewNode);
  return DidReplaceContainer(originalNode, newNode);
}


nsresult
nsRangeUpdater::WillRemoveContainer()
{
  if (mLock) return NS_ERROR_UNEXPECTED;  
  mLock = true;
  return NS_OK;
}


nsresult
nsRangeUpdater::DidRemoveContainer(nsINode* aNode, nsINode* aParent,
                                   int32_t aOffset, uint32_t aNodeOrigLen)
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_UNEXPECTED);
  mLock = false;

  NS_ENSURE_TRUE(aNode && aParent, NS_ERROR_NULL_POINTER);
  uint32_t count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < count; i++) {
    nsRangeStore* item = mArray[i];
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
nsRangeUpdater::DidRemoveContainer(nsIDOMNode* aNode, nsIDOMNode* aParent,
                                   int32_t aOffset, uint32_t aNodeOrigLen)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  return DidRemoveContainer(node, parent, aOffset, aNodeOrigLen);
}


nsresult
nsRangeUpdater::WillInsertContainer()
{
  if (mLock) return NS_ERROR_UNEXPECTED;  
  mLock = true;
  return NS_OK;
}


nsresult
nsRangeUpdater::DidInsertContainer()
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_UNEXPECTED);  
  mLock = false;
  return NS_OK;
}


void
nsRangeUpdater::WillMoveNode()
{
  mLock = true;
}


void
nsRangeUpdater::DidMoveNode(nsINode* aOldParent, int32_t aOldOffset,
                            nsINode* aNewParent, int32_t aNewOffset)
{
  MOZ_ASSERT(aOldParent);
  MOZ_ASSERT(aNewParent);
  NS_ENSURE_TRUE_VOID(mLock);
  mLock = false;

  for (uint32_t i = 0, count = mArray.Length(); i < count; ++i) {
    nsRangeStore* item = mArray[i];
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



/***************************************************************************
 * helper class for nsSelectionState.  nsRangeStore stores range endpoints.
 */

nsRangeStore::nsRangeStore() 
{ 
}
nsRangeStore::~nsRangeStore()
{
}

void
nsRangeStore::StoreRange(nsRange* aRange)
{
  MOZ_ASSERT(aRange);
  startNode = aRange->GetStartParent();
  startOffset = aRange->StartOffset();
  endNode = aRange->GetEndParent();
  endOffset = aRange->EndOffset();
}

already_AddRefed<nsRange>
nsRangeStore::GetRange()
{
  nsRefPtr<nsRange> range = new nsRange(startNode);
  nsresult res = range->Set(startNode, startOffset, endNode, endOffset);
  NS_ENSURE_SUCCESS(res, nullptr);
  return range.forget();
}
