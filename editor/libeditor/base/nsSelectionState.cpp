/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSelectionState.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMNode.h"
#include "nsIDOMRange.h"
#include "nsISelection.h"
#include "nsEditor.h"
#include "nsEditorUtils.h"


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
  for (PRUint32 i = 0, iEnd = mArray.Length(); i < iEnd; ++i)
  {
    nsRangeStore &item = mArray[i];
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                       "selection state mArray[i].startNode");
    cb.NoteXPCOMChild(item.startNode);
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                       "selection state mArray[i].endNode");
    cb.NoteXPCOMChild(item.endNode);
  }
}

nsresult  
nsSelectionState::SaveSelection(nsISelection *aSel)
{
  NS_ENSURE_TRUE(aSel, NS_ERROR_NULL_POINTER);
  PRInt32 i,rangeCount, arrayCount = mArray.Length();
  aSel->GetRangeCount(&rangeCount);
  
  // if we need more items in the array, new them
  if (arrayCount<rangeCount)
  {
    PRInt32 count = rangeCount-arrayCount;
    for (i=0; i<count; i++)
    {
      mArray.AppendElement();
    }
  }
  
  // else if we have too many, delete them
  else if (arrayCount>rangeCount)
  {
    for (i = arrayCount-1; i >= rangeCount; i--)
    {
      mArray.RemoveElementAt(i);
    }
  }
  
  // now store the selection ranges
  nsresult res = NS_OK;
  for (i=0; i<rangeCount; i++)
  {
    nsCOMPtr<nsIDOMRange> range;
    res = aSel->GetRangeAt(i, getter_AddRefs(range));
    mArray[i].StoreRange(range);
  }
  
  return res;
}

nsresult  
nsSelectionState::RestoreSelection(nsISelection *aSel)
{
  NS_ENSURE_TRUE(aSel, NS_ERROR_NULL_POINTER);
  nsresult res;
  PRUint32 i, arrayCount = mArray.Length();

  // clear out selection
  aSel->RemoveAllRanges();
  
  // set the selection ranges anew
  for (i=0; i<arrayCount; i++)
  {
    nsCOMPtr<nsIDOMRange> range;
    mArray[i].GetRange(address_of(range));
    NS_ENSURE_TRUE(range, NS_ERROR_UNEXPECTED);
   
    res = aSel->AddRange(range);
    if(NS_FAILED(res)) return res;

  }
  return NS_OK;
}

bool
nsSelectionState::IsCollapsed()
{
  if (1 != mArray.Length()) return PR_FALSE;
  nsCOMPtr<nsIDOMRange> range;
  mArray[0].GetRange(address_of(range));
  NS_ENSURE_TRUE(range, PR_FALSE);
  bool bIsCollapsed = false;
  range->GetCollapsed(&bIsCollapsed);
  return bIsCollapsed;
}

bool
nsSelectionState::IsEqual(nsSelectionState *aSelState)
{
  NS_ENSURE_TRUE(aSelState, PR_FALSE);
  PRUint32 i, myCount = mArray.Length(), itsCount = aSelState->mArray.Length();
  if (myCount != itsCount) return PR_FALSE;
  if (myCount < 1) return PR_FALSE;

  for (i=0; i<myCount; i++)
  {
    nsCOMPtr<nsIDOMRange> myRange, itsRange;
    mArray[i].GetRange(address_of(myRange));
    aSelState->mArray[i].GetRange(address_of(itsRange));
    NS_ENSURE_TRUE(myRange && itsRange, PR_FALSE);
  
    PRInt16 compResult;
    nsresult rv;
    rv = myRange->CompareBoundaryPoints(nsIDOMRange::START_TO_START, itsRange, &compResult);
    if (NS_FAILED(rv) || compResult) return PR_FALSE;
    rv = myRange->CompareBoundaryPoints(nsIDOMRange::END_TO_END, itsRange, &compResult);
    if (NS_FAILED(rv) || compResult) return PR_FALSE;
  }
  // if we got here, they are equal
  return PR_TRUE;
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

nsRangeUpdater::nsRangeUpdater() : mArray(), mLock(PR_FALSE) {}

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
  PRUint32 i, theCount = aSelState.mArray.Length();
  if (theCount < 1) return NS_ERROR_FAILURE;

  for (i=0; i<theCount; i++)
  {
    RegisterRangeItem(&aSelState.mArray[i]);
  }

  return NS_OK;
}

nsresult 
nsRangeUpdater::DropSelectionState(nsSelectionState &aSelState)
{
  PRUint32 i, theCount = aSelState.mArray.Length();
  if (theCount < 1) return NS_ERROR_FAILURE;

  for (i=0; i<theCount; i++)
  {
    DropRangeItem(&aSelState.mArray[i]);
  }

  return NS_OK;
}

// gravity methods:

nsresult
nsRangeUpdater::SelAdjCreateNode(nsIDOMNode *aParent, PRInt32 aPosition)
{
  if (mLock) return NS_OK;  // lock set by Will/DidReplaceParent, etc...
  NS_ENSURE_TRUE(aParent, NS_ERROR_NULL_POINTER);
  PRUint32 i, count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  nsRangeStore *item;
  
  for (i=0; i<count; i++)
  {
    item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);
    
    if ((item->startNode.get() == aParent) && (item->startOffset > aPosition))
      item->startOffset++;
    if ((item->endNode.get() == aParent) && (item->endOffset > aPosition))
      item->endOffset++;
  }
  return NS_OK;
}

nsresult
nsRangeUpdater::SelAdjInsertNode(nsIDOMNode *aParent, PRInt32 aPosition)
{
  return SelAdjCreateNode(aParent, aPosition);
}


nsresult
nsRangeUpdater::SelAdjDeleteNode(nsIDOMNode *aNode)
{
  if (mLock) return NS_OK;  // lock set by Will/DidReplaceParent, etc...
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  PRUint32 i, count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset = 0;
  
  nsresult res = nsEditor::GetNodeLocation(aNode, address_of(parent), &offset);
  NS_ENSURE_SUCCESS(res, res);
  
  // check for range endpoints that are after aNode and in the same parent
  nsRangeStore *item;
  for (i=0; i<count; i++)
  {
    item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);
    
    if ((item->startNode.get() == parent) && (item->startOffset > offset))
      item->startOffset--;
    if ((item->endNode.get() == parent) && (item->endOffset > offset))
      item->endOffset--;
      
    // check for range endpoints that are in aNode
    if (item->startNode == aNode)
    {
      item->startNode   = parent;
      item->startOffset = offset;
    }
    if (item->endNode == aNode)
    {
      item->endNode   = parent;
      item->endOffset = offset;
    }

    // check for range endpoints that are in descendants of aNode
    nsCOMPtr<nsIDOMNode> oldStart;
    if (nsEditorUtils::IsDescendantOf(item->startNode, aNode))
    {
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
  return NS_OK;
}


nsresult
nsRangeUpdater::SelAdjSplitNode(nsIDOMNode *aOldRightNode, PRInt32 aOffset, nsIDOMNode *aNewLeftNode)
{
  if (mLock) return NS_OK;  // lock set by Will/DidReplaceParent, etc...
  NS_ENSURE_TRUE(aOldRightNode && aNewLeftNode, NS_ERROR_NULL_POINTER);
  PRUint32 i, count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  nsresult result = nsEditor::GetNodeLocation(aOldRightNode, address_of(parent), &offset);
  NS_ENSURE_SUCCESS(result, result);
  
  // first part is same as inserting aNewLeftnode
  result = SelAdjInsertNode(parent,offset-1);
  NS_ENSURE_SUCCESS(result, result);

  // next step is to check for range enpoints inside aOldRightNode
  nsRangeStore *item;
  
  for (i=0; i<count; i++)
  {
    item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);
    
    if (item->startNode.get() == aOldRightNode)
    {
      if (item->startOffset > aOffset)
      {
        item->startOffset -= aOffset;
      }
      else
      {
        item->startNode = aNewLeftNode;
      }
    }
    if (item->endNode.get() == aOldRightNode)
    {
      if (item->endOffset > aOffset)
      {
        item->endOffset -= aOffset;
      }
      else
      {
        item->endNode = aNewLeftNode;
      }
    }
  }
  return NS_OK;
}


nsresult
nsRangeUpdater::SelAdjJoinNodes(nsIDOMNode *aLeftNode, 
                                  nsIDOMNode *aRightNode, 
                                  nsIDOMNode *aParent, 
                                  PRInt32 aOffset,
                                  PRInt32 aOldLeftNodeLength)
{
  if (mLock) return NS_OK;  // lock set by Will/DidReplaceParent, etc...
  NS_ENSURE_TRUE(aLeftNode && aRightNode && aParent, NS_ERROR_NULL_POINTER);
  PRUint32 i, count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  nsRangeStore *item;

  for (i=0; i<count; i++)
  {
    item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);
    
    if (item->startNode.get() == aParent)
    {
      // adjust start point in aParent
      if (item->startOffset > aOffset)
      {
        item->startOffset--;
      }
      else if (item->startOffset == aOffset)
      {
        // join keeps right hand node
        item->startNode = aRightNode;
        item->startOffset = aOldLeftNodeLength;
      }
    }
    else if (item->startNode.get() == aRightNode)
    {
      // adjust start point in aRightNode
      item->startOffset += aOldLeftNodeLength;
    }
    else if (item->startNode.get() == aLeftNode)
    {
      // adjust start point in aLeftNode
      item->startNode = aRightNode;
    }

    if (item->endNode.get() == aParent)
    {
      // adjust end point in aParent
      if (item->endOffset > aOffset)
      {
        item->endOffset--;
      }
      else if (item->endOffset == aOffset)
      {
        // join keeps right hand node
        item->endNode = aRightNode;
        item->endOffset = aOldLeftNodeLength;
      }
    }
    else if (item->endNode.get() == aRightNode)
    {
      // adjust end point in aRightNode
       item->endOffset += aOldLeftNodeLength;
    }
    else if (item->endNode.get() == aLeftNode)
    {
      // adjust end point in aLeftNode
      item->endNode = aRightNode;
    }
  }
  
  return NS_OK;
}


nsresult
nsRangeUpdater::SelAdjInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAString &aString)
{
  if (mLock) return NS_OK;  // lock set by Will/DidReplaceParent, etc...

  PRUint32 count = mArray.Length();
  if (!count) {
    return NS_OK;
  }
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aTextNode));
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  
  PRUint32 len=aString.Length(), i;
  nsRangeStore *item;
  for (i=0; i<count; i++)
  {
    item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);
    
    if ((item->startNode.get() == node) && (item->startOffset > aOffset))
      item->startOffset += len;
    if ((item->endNode.get() == node) && (item->endOffset > aOffset))
      item->endOffset += len;
  }
  return NS_OK;
}


nsresult
nsRangeUpdater::SelAdjDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength)
{
  if (mLock) return NS_OK;  // lock set by Will/DidReplaceParent, etc...

  PRUint32 i, count = mArray.Length();
  if (!count) {
    return NS_OK;
  }
  nsRangeStore *item;
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aTextNode));
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  
  for (i=0; i<count; i++)
  {
    item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);
    
    if ((item->startNode.get() == node) && (item->startOffset > aOffset))
    {
      item->startOffset -= aLength;
      if (item->startOffset < 0) item->startOffset = 0;
    }
    if ((item->endNode.get() == node) && (item->endOffset > aOffset))
    {
      item->endOffset -= aLength;
      if (item->endOffset < 0) item->endOffset = 0;
    }
  }
  return NS_OK;
}


nsresult
nsRangeUpdater::WillReplaceContainer()
{
  if (mLock) return NS_ERROR_UNEXPECTED;  
  mLock = PR_TRUE;
  return NS_OK;
}


nsresult
nsRangeUpdater::DidReplaceContainer(nsIDOMNode *aOriginalNode, nsIDOMNode *aNewNode)
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_UNEXPECTED);  
  mLock = PR_FALSE;

  NS_ENSURE_TRUE(aOriginalNode && aNewNode, NS_ERROR_NULL_POINTER);
  PRUint32 i, count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  nsRangeStore *item;
  
  for (i=0; i<count; i++)
  {
    item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);
    
    if (item->startNode.get() == aOriginalNode)
      item->startNode = aNewNode;
    if (item->endNode.get() == aOriginalNode)
      item->endNode = aNewNode;
  }
  return NS_OK;
}


nsresult
nsRangeUpdater::WillRemoveContainer()
{
  if (mLock) return NS_ERROR_UNEXPECTED;  
  mLock = PR_TRUE;
  return NS_OK;
}


nsresult
nsRangeUpdater::DidRemoveContainer(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aOffset, PRUint32 aNodeOrigLen)
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_UNEXPECTED);  
  mLock = PR_FALSE;

  NS_ENSURE_TRUE(aNode && aParent, NS_ERROR_NULL_POINTER);
  PRUint32 i, count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  nsRangeStore *item;
  
  for (i=0; i<count; i++)
  {
    item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);
    
    if (item->startNode.get() == aNode)
    {
      item->startNode = aParent;
      item->startOffset += aOffset;
    }
    else if ((item->startNode.get() == aParent) && (item->startOffset > aOffset))
      item->startOffset += (PRInt32)aNodeOrigLen-1;
      
    if (item->endNode.get() == aNode)
    {
      item->endNode = aParent;
      item->endOffset += aOffset;
    }
    else if ((item->endNode.get() == aParent) && (item->endOffset > aOffset))
      item->endOffset += (PRInt32)aNodeOrigLen-1;
  }
  return NS_OK;
}


nsresult
nsRangeUpdater::WillInsertContainer()
{
  if (mLock) return NS_ERROR_UNEXPECTED;  
  mLock = PR_TRUE;
  return NS_OK;
}


nsresult
nsRangeUpdater::DidInsertContainer()
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_UNEXPECTED);  
  mLock = PR_FALSE;
  return NS_OK;
}


nsresult
nsRangeUpdater::WillMoveNode()
{
  if (mLock) return NS_ERROR_UNEXPECTED;  
  mLock = PR_TRUE;
  return NS_OK;
}


nsresult
nsRangeUpdater::DidMoveNode(nsIDOMNode *aOldParent, PRInt32 aOldOffset, nsIDOMNode *aNewParent, PRInt32 aNewOffset)
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_UNEXPECTED);  
  mLock = PR_FALSE;

  NS_ENSURE_TRUE(aOldParent && aNewParent, NS_ERROR_NULL_POINTER);
  PRUint32 i, count = mArray.Length();
  if (!count) {
    return NS_OK;
  }

  nsRangeStore *item;
  
  for (i=0; i<count; i++)
  {
    item = mArray[i];
    NS_ENSURE_TRUE(item, NS_ERROR_NULL_POINTER);
    
    // like a delete in aOldParent
    if ((item->startNode.get() == aOldParent) && (item->startOffset > aOldOffset))
      item->startOffset--;
    if ((item->endNode.get() == aOldParent) && (item->endOffset > aOldOffset))
      item->endOffset--;
      
    // and like an insert in aNewParent
    if ((item->startNode.get() == aNewParent) && (item->startOffset > aNewOffset))
      item->startOffset++;
    if ((item->endNode.get() == aNewParent) && (item->endOffset > aNewOffset))
      item->endOffset++;
  }
  return NS_OK;
}



/***************************************************************************
 * helper class for nsSelectionState.  nsRangeStore stores range endpoints.
 */

  // DEBUG: PRInt32 nsRangeStore::n = 0;

nsRangeStore::nsRangeStore() 
{ 
  // DEBUG: n++;  printf("range store alloc count=%d\n", n); 
}
nsRangeStore::~nsRangeStore()
{
  // DEBUG: n--;  printf("range store alloc count=%d\n", n); 
}

nsresult nsRangeStore::StoreRange(nsIDOMRange *aRange)
{
  NS_ENSURE_TRUE(aRange, NS_ERROR_NULL_POINTER);
  aRange->GetStartContainer(getter_AddRefs(startNode));
  aRange->GetEndContainer(getter_AddRefs(endNode));
  aRange->GetStartOffset(&startOffset);
  aRange->GetEndOffset(&endOffset);
  return NS_OK;
}

nsresult nsRangeStore::GetRange(nsCOMPtr<nsIDOMRange> *outRange)
{
  NS_ENSURE_TRUE(outRange, NS_ERROR_NULL_POINTER);
  nsresult res;
  *outRange = do_CreateInstance("@mozilla.org/content/range;1", &res);
  if(NS_FAILED(res)) return res;

  res = (*outRange)->SetStart(startNode, startOffset);
  if(NS_FAILED(res)) return res;

  res = (*outRange)->SetEnd(endNode, endOffset);
  return res;
}
