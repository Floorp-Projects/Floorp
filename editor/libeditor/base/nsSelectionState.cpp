/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSelectionState.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMNode.h"
#include "nsIDOMRange.h"
#include "nsISelection.h"
#include "nsEditor.h"
#include "nsLayoutCID.h"

static NS_DEFINE_CID(kCRangeCID, NS_RANGE_CID);


/***************************************************************************
 * class for recording selection info.  stores selection as collection of
 * { {startnode, startoffset} , {endnode, endoffset} } tuples.  Cant store
 * ranges since dom gravity will possibly change the ranges.
 */
nsSelectionState::nsSelectionState() : mArray(){}

nsSelectionState::~nsSelectionState() 
{
  MakeEmpty();
}

nsresult  
nsSelectionState::SaveSelection(nsISelection *aSel)
{
  if (!aSel) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  PRInt32 i,rangeCount, arrayCount = mArray.Count();
  nsRangeStore *item;
  aSel->GetRangeCount(&rangeCount);
  
  // if we need more items in the array, new them
  if (arrayCount<rangeCount)
  {
    PRInt32 count = rangeCount-arrayCount;
    for (i=0; i<count; i++)
    {
      item = new nsRangeStore;
      mArray.AppendElement(item);
    }
  }
  
  // else if we have too many, delete them
  else if (rangeCount>arrayCount)
  {
    while ((item = (nsRangeStore*)mArray.ElementAt(rangeCount)))
    {
      delete item;
      mArray.RemoveElementAt(rangeCount);
    }
  }
  
  // now store the selection ranges
  for (i=0; i<rangeCount; i++)
  {
    item = (nsRangeStore*)mArray.ElementAt(i);
    if (!item) return NS_ERROR_UNEXPECTED;
    nsCOMPtr<nsIDOMRange> range;
    res = aSel->GetRangeAt(i, getter_AddRefs(range));
    item->StoreRange(range);
  }
  
  return res;
}

nsresult  
nsSelectionState::RestoreSelection(nsISelection *aSel)
{
  if (!aSel) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  PRInt32 i, arrayCount = mArray.Count();
  nsRangeStore *item;

  // clear out selection
  aSel->RemoveAllRanges();
  
  // set the selection ranges anew
  for (i=0; i<arrayCount; i++)
  {
    item = (nsRangeStore*)mArray.ElementAt(i);
    if (!item) return NS_ERROR_UNEXPECTED;
    nsCOMPtr<nsIDOMRange> range;
    item->GetRange(address_of(range));
    if (!range) return NS_ERROR_UNEXPECTED;
   
    res = aSel->AddRange(range);
    if(NS_FAILED(res)) return res;

  }
  return NS_OK;
}

PRBool
nsSelectionState::IsCollapsed()
{
  if (1 != mArray.Count()) return PR_FALSE;
  nsRangeStore *item;
  item = (nsRangeStore*)mArray.ElementAt(0);
  if (!item) return PR_FALSE;
  nsCOMPtr<nsIDOMRange> range;
  item->GetRange(address_of(range));
  if (!range) return PR_FALSE;
  PRBool bIsCollapsed;
  range->GetCollapsed(&bIsCollapsed);
  return bIsCollapsed;
}

PRBool
nsSelectionState::IsEqual(nsSelectionState *aSelState)
{
  if (!aSelState) return NS_ERROR_NULL_POINTER;
  PRInt32 i, myCount = mArray.Count(), itsCount = aSelState->mArray.Count();
  if (myCount != itsCount) return PR_FALSE;
  if (myCount < 1) return PR_FALSE;

  nsRangeStore *myItem, *itsItem;
  
  for (i=0; i<myCount; i++)
  {
    myItem = (nsRangeStore*)mArray.ElementAt(i);
    itsItem = (nsRangeStore*)(aSelState->mArray.ElementAt(i));
    if (!myItem || !itsItem) return PR_FALSE;
    
    nsCOMPtr<nsIDOMRange> myRange, itsRange;
    myItem->GetRange(address_of(myRange));
    itsItem->GetRange(address_of(itsRange));
    if (!myRange || !itsRange) return PR_FALSE;
  
    PRInt32 compResult;
    myRange->CompareBoundaryPoints(nsIDOMRange::START_TO_START, itsRange, &compResult);
    if (compResult) return PR_FALSE;
    myRange->CompareBoundaryPoints(nsIDOMRange::END_TO_END, itsRange, &compResult);
    if (compResult) return PR_FALSE;
  }
  // if we got here, they are equal
  return PR_TRUE;
}

void     
nsSelectionState::MakeEmpty()
{
  // free any items in the array
  nsRangeStore *item;
  while ((item = (nsRangeStore*)mArray.ElementAt(0)))
  {
    delete item;
    mArray.RemoveElementAt(0);
  }
}

PRBool   
nsSelectionState::IsEmpty()
{
  return (mArray.Count() == 0);
}

/***************************************************************************
 * nsRangeUpdater:  class for updating nsIDOMRanges in response to editor actions.
 */

nsRangeUpdater::nsRangeUpdater() : mArray(), mLock(PR_FALSE) {}

nsRangeUpdater::~nsRangeUpdater()
{
  // free any items in the array
  nsRangeStore *item;
  while ((item = (nsRangeStore*)mArray.ElementAt(0)))
  {
    delete item;
    mArray.RemoveElementAt(0);
  }
}
  
void* 
nsRangeUpdater::RegisterRange(nsIDOMRange *aRange)
{
  nsRangeStore *item = new nsRangeStore;
  if (!item) return nsnull;
  item->StoreRange(aRange);
  mArray.AppendElement(item);
  return item;
}

nsCOMPtr<nsIDOMRange> 
nsRangeUpdater::ReclaimRange(void *aCookie)
{
  nsRangeStore *item = NS_STATIC_CAST(nsRangeStore*,aCookie);
  if (!item) return nsnull;
  nsCOMPtr<nsIDOMRange> outRange;
  item->GetRange(address_of(outRange));
  mArray.RemoveElement(aCookie);
  delete item;
  return outRange;
}
    
void 
nsRangeUpdater::DropRange(void *aCookie)
{
  nsRangeStore *item = NS_STATIC_CAST(nsRangeStore*,aCookie);
  if (!item) return;
  mArray.RemoveElement(aCookie);
  delete item;
}

void 
nsRangeUpdater::RegisterRangeItem(nsRangeStore *aRangeItem)
{
  if (!aRangeItem) return;
  mArray.AppendElement(aRangeItem);
  return;
}

void 
nsRangeUpdater::DropRangeItem(nsRangeStore *aRangeItem)
{
  if (!aRangeItem) return;
  mArray.RemoveElement(aRangeItem);
  return;
}

nsresult 
nsRangeUpdater::RegisterSelectionState(nsSelectionState &aSelState)
{
  PRInt32 i, theCount = aSelState.mArray.Count();
  if (theCount < 1) return NS_ERROR_FAILURE;

  nsRangeStore *item;
  
  for (i=0; i<theCount; i++)
  {
    item = (nsRangeStore*)aSelState.mArray.ElementAt(i);
    RegisterRangeItem(item);
  }

  return NS_OK;;
}

nsresult 
nsRangeUpdater::DropSelectionState(nsSelectionState &aSelState)
{
  PRInt32 i, theCount = aSelState.mArray.Count();
  if (theCount < 1) return NS_ERROR_FAILURE;

  nsRangeStore *item;
  
  for (i=0; i<theCount; i++)
  {
    item = (nsRangeStore*)aSelState.mArray.ElementAt(i);
    DropRangeItem(item);
  }

  return NS_OK;;
}

// gravity methods:

nsresult
nsRangeUpdater::SelAdjCreateNode(nsIDOMNode *aParent, PRInt32 aPosition)
{
  if (mLock) return NS_OK;  // lock set by Will/DidReplaceParent, etc...
  if (!aParent) return NS_ERROR_NULL_POINTER;
  PRInt32 i, count = mArray.Count();
  if (!count) return NS_OK;

  nsRangeStore *item;
  
  for (i=0; i<count; i++)
  {
    item = (nsRangeStore*)mArray.ElementAt(i);
    if (!item) return NS_ERROR_NULL_POINTER;
    
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
nsRangeUpdater::SelAdjDeleteNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aOffset)
{
  if (mLock) return NS_OK;  // lock set by Will/DidReplaceParent, etc...
  if (!aNode) return NS_ERROR_NULL_POINTER;
  PRInt32 i, count = mArray.Count();
  if (!count) return NS_OK;

  nsRangeStore *item;
  
  for (i=0; i<count; i++)
  {
    item = (nsRangeStore*)mArray.ElementAt(i);
    if (!item) return NS_ERROR_NULL_POINTER;
    
    if ((item->startNode.get() == aParent) && (item->startOffset > aOffset))
      item->startOffset--;
    if ((item->endNode.get() == aParent) && (item->endOffset > aOffset))
      item->endOffset--;
  }
  // MOOSE: also check inside of aNode, expensive.  But in theory, we shouldn't
  // actually hit this case in the usage i forsee for this.
  return NS_OK;
}


nsresult
nsRangeUpdater::SelAdjSplitNode(nsIDOMNode *aOldRightNode, PRInt32 aOffset, nsIDOMNode *aNewLeftNode)
{
  if (mLock) return NS_OK;  // lock set by Will/DidReplaceParent, etc...
  if (!aOldRightNode || !aNewLeftNode) return NS_ERROR_NULL_POINTER;
  PRInt32 i, count = mArray.Count();
  if (!count) return NS_OK;

  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  nsresult result = nsEditor::GetNodeLocation(aOldRightNode, address_of(parent), &offset);
  if (NS_FAILED(result)) return result;
  
  // first part is same as inserting aNewLeftnode
  result = SelAdjInsertNode(parent,offset-1);
  if (NS_FAILED(result)) return result;

  // next step is to check for range enpoints inside aOldRightNode
  nsRangeStore *item;
  
  for (i=0; i<count; i++)
  {
    item = (nsRangeStore*)mArray.ElementAt(i);
    if (!item) return NS_ERROR_NULL_POINTER;
    
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
  if (!aLeftNode || !aRightNode || !aParent) return NS_ERROR_NULL_POINTER;
  PRInt32 i, count = mArray.Count();
  if (!count) return NS_OK;

  nsRangeStore *item;

  for (i=0; i<count; i++)
  {
    item = (nsRangeStore*)mArray.ElementAt(i);
    if (!item) return NS_ERROR_NULL_POINTER;
    
    // adjust endpoints in aParent
    if (item->startNode.get() == aParent)
    {
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
    if (item->endNode.get() == aParent)
    {
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
    // adjust endpoints in aRightNode
    if (item->startNode.get() == aRightNode)
      item->startOffset += aOldLeftNodeLength;
    if (item->endNode.get() == aRightNode)
      item->endOffset += aOldLeftNodeLength;
  }
  
  return NS_OK;
}


nsresult
nsRangeUpdater::SelAdjInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsString &aString)
{
  if (mLock) return NS_OK;  // lock set by Will/DidReplaceParent, etc...
  return NS_OK;
}


nsresult
nsRangeUpdater::SelAdjDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength)
{
  if (mLock) return NS_OK;  // lock set by Will/DidReplaceParent, etc...
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
  if (!mLock) return NS_ERROR_UNEXPECTED;  
  mLock = PR_FALSE;

  if (!aOriginalNode || !aNewNode) return NS_ERROR_NULL_POINTER;
  PRInt32 i, count = mArray.Count();
  if (!count) return NS_OK;

  nsRangeStore *item;
  
  for (i=0; i<count; i++)
  {
    item = (nsRangeStore*)mArray.ElementAt(i);
    if (!item) return NS_ERROR_NULL_POINTER;
    
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
  if (!mLock) return NS_ERROR_UNEXPECTED;  
  mLock = PR_FALSE;

  if (!aNode || !aParent) return NS_ERROR_NULL_POINTER;
  PRInt32 i, count = mArray.Count();
  if (!count) return NS_OK;

  nsRangeStore *item;
  
  for (i=0; i<count; i++)
  {
    item = (nsRangeStore*)mArray.ElementAt(i);
    if (!item) return NS_ERROR_NULL_POINTER;
    
    if (item->startNode.get() == aNode)
    {
      item->startNode = aParent;
      item->startOffset += aOffset;
    }
    if (item->endNode.get() == aNode)
    {
      item->endNode = aParent;
      item->endOffset += aOffset;
    }
    if ((item->startNode.get() == aParent) && (item->startOffset > aOffset))
      item->startOffset += (PRInt32)aNodeOrigLen-1;
    if ((item->endNode.get() == aParent) && (item->endOffset > aOffset))
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
  if (!mLock) return NS_ERROR_UNEXPECTED;  
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
  if (!mLock) return NS_ERROR_UNEXPECTED;  
  mLock = PR_FALSE;

  if (!aOldParent || !aNewParent) return NS_ERROR_NULL_POINTER;
  PRInt32 i, count = mArray.Count();
  if (!count) return NS_OK;

  nsRangeStore *item;
  
  for (i=0; i<count; i++)
  {
    item = (nsRangeStore*)mArray.ElementAt(i);
    if (!item) return NS_ERROR_NULL_POINTER;
    
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
  if (!aRange) return NS_ERROR_NULL_POINTER;
  aRange->GetStartContainer(getter_AddRefs(startNode));
  aRange->GetEndContainer(getter_AddRefs(endNode));
  aRange->GetStartOffset(&startOffset);
  aRange->GetEndOffset(&endOffset);
  return NS_OK;
}

nsresult nsRangeStore::GetRange(nsCOMPtr<nsIDOMRange> *outRange)
{
  if (!outRange) return NS_ERROR_NULL_POINTER;
  nsresult res = nsComponentManager::CreateInstance(kCRangeCID,
                             nsnull,
                             NS_GET_IID(nsIDOMRange),
                             getter_AddRefs(*outRange));
  if(NS_FAILED(res)) return res;

  res = (*outRange)->SetStart(startNode, startOffset);
  if(NS_FAILED(res)) return res;

  res = (*outRange)->SetEnd(endNode, endOffset);
  return res;
}
