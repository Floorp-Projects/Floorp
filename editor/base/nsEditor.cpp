/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "pratom.h"

#include "nsVoidArray.h"

#include "nsIDOMDocument.h"
#include "nsIPref.h"
#include "nsILocale.h"
#include "nsIEditProperty.h"  // to be removed  XXX

#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNode.h"
#include "nsIDOMComment.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsIDocument.h"
#include "nsIDiskDocument.h"
#include "nsIServiceManager.h"
#include "nsTransactionManagerCID.h"
#include "nsITransactionManager.h"
#include "nsIAbsorbingTransaction.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsIDOMSelection.h"
#include "nsIEnumerator.h"
#include "nsIAtom.h"
#include "nsISupportsArray.h"
#include "nsICaret.h"
#include "nsIStyleContext.h"
#include "nsIEditActionListener.h"
#include "nsIEditorObserver.h"
#include "nsIKBStateControl.h"
#include "nsIWidget.h"
#include "nsIScrollbar.h"

#include "nsIFrame.h"  // Needed by IME code

#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIStyleSet.h"
#include "nsIDocumentObserver.h"
#include "nsIDocumentStateListener.h"
#include "nsIStringStream.h"
#include "nsITextContent.h"

#include "nsNetUtil.h"

#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDocumentEncoder.h"
#include "nsLayoutCID.h"

// transactions the editor knows how to build
#include "TransactionFactory.h"
#include "EditAggregateTxn.h"
#include "PlaceholderTxn.h"
#include "ChangeAttributeTxn.h"
#include "CreateElementTxn.h"
#include "InsertElementTxn.h"
#include "DeleteElementTxn.h"
#include "InsertTextTxn.h"
#include "DeleteTextTxn.h"
#include "DeleteRangeTxn.h"
#include "SplitElementTxn.h"
#include "JoinElementTxn.h"
#include "nsStyleSheetTxns.h"
#include "IMETextTxn.h"
#include "nsIHTMLEditor.h"
// included for nsEditor::CreateHTMLContent
#include "nsIElementFactory.h"
#include "nsINodeInfo.h"
#include "nsINameSpaceManager.h"

// #define HACK_FORCE_REDRAW 1

#include "nsEditorCID.h"
#include "nsEditor.h"
#include "nsEditorUtils.h"

#ifdef HACK_FORCE_REDRAW
// INCLUDES FOR EVIL HACK TO FOR REDRAW
// BEGIN
#include "nsIViewManager.h"
#include "nsIView.h"
// END
#endif

static NS_DEFINE_CID(kCRangeCID,            NS_RANGE_CID);
static NS_DEFINE_CID(kCContentIteratorCID,  NS_CONTENTITERATOR_CID);
static NS_DEFINE_CID(kCDOMRangeCID,         NS_RANGE_CID);
static NS_DEFINE_CID(kPrefServiceCID,       NS_PREF_CID);

// transaction manager
static NS_DEFINE_CID(kCTransactionManagerCID, NS_TRANSACTIONMANAGER_CID);

#ifdef XP_PC
#define TRANSACTION_MANAGER_DLL "txmgr.dll"
#else
#ifdef XP_MAC
#define TRANSACTION_MANAGER_DLL "TRANSACTION_MANAGER_DLL"
#else // XP_UNIX || XP_BEOS
#define TRANSACTION_MANAGER_DLL "libtxmgr"MOZ_DLL_SUFFIX
#endif
#endif

#define NS_ERROR_EDITOR_NO_SELECTION NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR,1)
#define NS_ERROR_EDITOR_NO_TEXTNODE  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR,2)

const char* nsEditor::kMOZEditorBogusNodeAttr="_moz_editor_bogus_node";
const char* nsEditor::kMOZEditorBogusNodeValue="TRUE";

#ifdef NS_DEBUG_EDITOR
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif


const PRUnichar nbsp = 160;
PRInt32 nsEditor::gInstanceCount = 0;


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
nsSelectionState::SaveSelection(nsIDOMSelection *aSel)
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
nsSelectionState::RestoreSelection(nsIDOMSelection *aSel)
{
  if (!aSel) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  PRInt32 i, arrayCount = mArray.Count();
  nsRangeStore *item;

  // clear out selection
  aSel->ClearSelection();
  
  // set the selection ranges anew
  for (i=0; i<arrayCount; i++)
  {
    item = (nsRangeStore*)mArray.ElementAt(i);
    if (!item) return NS_ERROR_UNEXPECTED;
    nsCOMPtr<nsIDOMRange> range;
    item->GetRange(&range);
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
  item->GetRange(&range);
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
    myItem = (nsRangeStore*)mArray.ElementAt(0);
    itsItem = (nsRangeStore*)(aSelState->mArray.ElementAt(0));
    if (!myItem || !itsItem) return PR_FALSE;
    
    nsCOMPtr<nsIDOMRange> myRange, itsRange;
    myItem->GetRange(&myRange);
    itsItem->GetRange(&itsRange);
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
  item->GetRange(&outRange);
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
  nsresult result = nsEditor::GetNodeLocation(aOldRightNode, &parent, &offset);
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

/***************************************************************************
 * another helper class for nsSelectionState.  stack based class for doing
 * Will/DidReplaceContainer()
 */

class nsAutoReplaceContainerSelNotify
{
  private:
    nsRangeUpdater &mRU;
    nsIDOMNode *mOriginalNode;
    nsIDOMNode *mNewNode;

  public:
    nsAutoReplaceContainerSelNotify(nsRangeUpdater &aRangeUpdater, nsIDOMNode *aOriginalNode, nsIDOMNode *aNewNode) :
    mRU(aRangeUpdater)
    ,mOriginalNode(aOriginalNode)
    ,mNewNode(aNewNode)
    {
      mRU.WillReplaceContainer();
    }
    
    ~nsAutoReplaceContainerSelNotify()
    {
      mRU.DidReplaceContainer(mOriginalNode, mNewNode);
    }
};


/***************************************************************************
 * another helper class for nsSelectionState.  stack based class for doing
 * Will/DidRemoveContainer()
 */

class nsAutoRemoveContainerSelNotify
{
  private:
    nsRangeUpdater &mRU;
    nsIDOMNode *mNode;
    nsIDOMNode *mParent;
    PRInt32    mOffset;
    PRUint32   mNodeOrigLen;

  public:
    nsAutoRemoveContainerSelNotify(nsRangeUpdater &aRangeUpdater, 
                                   nsIDOMNode *aNode, 
                                   nsIDOMNode *aParent, 
                                   PRInt32 aOffset, 
                                   PRUint32 aNodeOrigLen) :
    mRU(aRangeUpdater)
    ,mNode(aNode)
    ,mParent(aParent)
    ,mOffset(aOffset)
    ,mNodeOrigLen(aNodeOrigLen)
    {
      mRU.WillRemoveContainer();
    }
    
    ~nsAutoRemoveContainerSelNotify()
    {
      mRU.DidRemoveContainer(mNode, mParent, mOffset, mNodeOrigLen);
    }
};

/***************************************************************************
 * another helper class for nsSelectionState.  stack based class for doing
 * Will/DidInsertContainer()
 */

class nsAutoInsertContainerSelNotify
{
  private:
    nsRangeUpdater &mRU;

  public:
    nsAutoInsertContainerSelNotify(nsRangeUpdater &aRangeUpdater) :
    mRU(aRangeUpdater)
    {
      mRU.WillInsertContainer();
    }
    
    ~nsAutoInsertContainerSelNotify()
    {
      mRU.DidInsertContainer();
    }
};


/***************************************************************************
 * another helper class for nsSelectionState.  stack based class for doing
 * Will/DidMoveNode()
 */

class nsAutoMoveNodeSelNotify
{
  private:
    nsRangeUpdater &mRU;
    nsIDOMNode *mOldParent;
    nsIDOMNode *mNewParent;
    PRInt32    mOldOffset;
    PRInt32    mNewOffset;

  public:
    nsAutoMoveNodeSelNotify(nsRangeUpdater &aRangeUpdater, 
                            nsIDOMNode *aOldParent, 
                            PRInt32 aOldOffset, 
                            nsIDOMNode *aNewParent, 
                            PRInt32 aNewOffset) :
    mRU(aRangeUpdater)
    ,mOldParent(aOldParent)
    ,mNewParent(aNewParent)
    ,mOldOffset(aOldOffset)
    ,mNewOffset(aNewOffset)
    {
      mRU.WillMoveNode();
    }
    
    ~nsAutoMoveNodeSelNotify()
    {
      mRU.DidMoveNode(mOldParent, mOldOffset, mNewParent, mNewOffset);
    }
};


//---------------------------------------------------------------------------
//
// nsEditor: base editor class implementation
//
//---------------------------------------------------------------------------


nsEditor::nsEditor()
:  mPresShellWeak(nsnull)
,  mViewManager(nsnull)
,  mUpdateCount(0)
,  mPlaceHolderTxn(nsnull)
,  mPlaceHolderName(nsnull)
,  mPlaceHolderBatch(0)
,  mSelState(nsnull)
,  mSavedSel()
,  mRangeUpdater()
,  mShouldTxnSetSelection(PR_TRUE)
,  mBodyElement(nsnull)
,  mInIMEMode(PR_FALSE)
,  mIMETextRangeList(nsnull)
,  mIMETextNode(nsnull)
,  mIMETextOffset(0)
,  mIMEBufferLength(0)
,  mActionListeners(nsnull)
,  mEditorObservers(nsnull)
,  mDocDirtyState(-1)
,  mDocWeak(nsnull)
,  mAction(nsnull)
,  mDirection(eNone)
{
  //initialize member variables here
  NS_INIT_REFCNT();

  PR_AtomicIncrement(&gInstanceCount);
}

nsEditor::~nsEditor()
{
  // not sure if this needs to be called earlier.
  NotifyDocumentListeners(eDocumentToBeDestroyed);

  delete mEditorObservers;   // no need to release observers; we didn't addref them
  mEditorObservers = 0;
  
  if (mActionListeners)
  {
    PRInt32 i;
    nsIEditActionListener *listener;

    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      NS_IF_RELEASE(listener);
    }

    delete mActionListeners;
    mActionListeners = 0;
  }

  /* shut down all classes that needed initialization */
  InsertTextTxn::ClassShutdown();
  IMETextTxn::ClassShutdown();
 
  PR_AtomicDecrement(&gInstanceCount);
}


// BEGIN nsEditor core implementation

NS_IMPL_ADDREF(nsEditor)
NS_IMPL_RELEASE(nsEditor)
NS_IMPL_QUERY_INTERFACE3(nsEditor, nsIEditor, nsIEditorIMESupport, nsISupportsWeakReference)

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditorMethods 
#pragma mark -
#endif


NS_IMETHODIMP
nsEditor::Init(nsIDOMDocument *aDoc, nsIPresShell* aPresShell, nsIContent *aRoot, nsISelectionController *aSelCon, PRUint32 aFlags)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  if ((nsnull==aDoc) || (nsnull==aPresShell))
    return NS_ERROR_NULL_POINTER;

  mFlags = aFlags;
  mDocWeak = getter_AddRefs( NS_GetWeakReference(aDoc) );  // weak reference to doc
  mPresShellWeak = getter_AddRefs( NS_GetWeakReference(aPresShell) );   // weak reference to pres shell
  mSelConWeak = getter_AddRefs( NS_GetWeakReference(aSelCon) );   // weak reference to selectioncontroller
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  
  //set up body element if we are passed one.  
  if (aRoot)
    mBodyElement = do_QueryInterface(aRoot);



  // Set up the DTD
  // XXX - in the long run we want to get this from the document, but there
  // is no way to do that right now.  So we leave it null here and set
  // up a nav html dtd in nsHTMLEditor::Init

  // Init mEditProperty
  nsresult result = NS_NewEditProperty(getter_AddRefs(mEditProperty));
  if (NS_FAILED(result)) { return result; }
  if (!mEditProperty) {return NS_ERROR_NULL_POINTER;}

  ps->GetViewManager(&mViewManager);
  if (!mViewManager) {return NS_ERROR_NULL_POINTER;}
  mViewManager->Release(); //we want a weak link

  mUpdateCount=0;
  InsertTextTxn::ClassInit();

  /* initalize IME stuff */
  IMETextTxn::ClassInit();
  mIMETextNode = do_QueryInterface(nsnull);
  mIMETextOffset = 0;
  mIMEBufferLength = 0;
  
  /* Show the caret */
  aSelCon->SetCaretReadOnly(PR_FALSE);
  aSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
  
  aSelCon->SetDisplayNonTextSelection(PR_TRUE);//we want to see all the selection reflected to user

  // Set the selection to the beginning:

//hack to get around this for now.
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mSelConWeak);
  if (shell)
    BeginningOfDocument();

  NS_POSTCONDITION(mDocWeak && mPresShellWeak, "bad state");

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::PostCreate()
{
  // nuke the modification count, so the doc appears unmodified
  // do this before we notify listeners
  ResetDocModCount();
  
  // update the UI with our state
  NotifyDocumentListeners(eDocumentCreated);
  NotifyDocumentListeners(eDocumentStateChanged);
  
  // Call ResetInputState() for initialization
  ForceCompositionEnd();

  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::GetDocument(nsIDOMDocument **aDoc)
{
  if (!aDoc)
    return NS_ERROR_NULL_POINTER;
  *aDoc = nsnull; // init out param
  NS_PRECONDITION(mDocWeak, "bad state, mDocWeak weak pointer not initialized");
  if (!mDocWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  NS_ADDREF(*aDoc = doc);
  return NS_OK;
}


nsresult 
nsEditor::GetPresShell(nsIPresShell **aPS)
{
  if (!aPS)
    return NS_ERROR_NULL_POINTER;
  *aPS = nsnull; // init out param
  NS_PRECONDITION(mPresShellWeak, "bad state, null mPresShellWeak");
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  NS_ADDREF(*aPS = ps);
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::GetSelectionController(nsISelectionController **aSel)
{
  if (!aSel)
    return NS_ERROR_NULL_POINTER;
  *aSel = nsnull; // init out param
  NS_PRECONDITION(mSelConWeak, "bad state, null mSelConWeak");
  if (!mSelConWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mSelConWeak);
  if (!selCon) return NS_ERROR_NOT_INITIALIZED;
  NS_ADDREF(*aSel = selCon);
  return NS_OK;
}



NS_IMETHODIMP
nsEditor::GetSelection(nsIDOMSelection **aSelection)
{
  if (!aSelection)
    return NS_ERROR_NULL_POINTER;
  *aSelection = nsnull;
  if (!mSelConWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsISelectionController> selcon = do_QueryReferent(mSelConWeak);
  if (!selcon) return NS_ERROR_NOT_INITIALIZED;
  nsresult result = selcon->GetSelection(nsISelectionController::SELECTION_NORMAL, aSelection);  // does an addref
  return result;
}

NS_IMETHODIMP 
nsEditor::Do(nsITransaction *aTxn)
{
  if (gNoisy) { printf("Editor::Do ----------\n"); }
  
  nsresult result = NS_OK;
  
  if (mPlaceHolderBatch && !mPlaceHolderTxn)
  {
    // it's pretty darn amazing how many different types of pointers
    // this transaction goes through here.  I bet this is a record.
    
    // We start off with an EditTxn since that's what the factory returns.
    EditTxn *editTxn;
    result = TransactionFactory::GetNewTransaction(PlaceholderTxn::GetCID(), &editTxn);
    if (NS_FAILED(result)) { return result; }
    if (!editTxn) { return NS_ERROR_NULL_POINTER; }

    // Then we QI to an nsIAbsorbingTransaction to get at placeholder functionality
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn;
    editTxn->QueryInterface(NS_GET_IID(nsIAbsorbingTransaction), getter_AddRefs(plcTxn));
    // have to use line above instead of "plcTxn = do_QueryInterface(editTxn);"
    // due to our broken interface model for transactions.

    // save off weak reference to placeholder txn
    mPlaceHolderTxn = getter_AddRefs( NS_GetWeakReference(plcTxn) );
    plcTxn->Init(mPlaceHolderName, mSelState, this);
    mSelState = nsnull;  // placeholder txn took ownership of this pointer

    // finally we QI to an nsITransaction since that's what Do() expects
    nsCOMPtr<nsITransaction> theTxn = do_QueryInterface(plcTxn);
    nsITransaction* txn = theTxn;
    Do(txn);  // we will recurse, but will not hit this case in the nested call
    
    // The transaction system (if any) has taken ownwership of txn
    NS_IF_RELEASE(txn);
  }

  if (aTxn)
  {  
    // get the selection and start a batch change
    nsCOMPtr<nsIDOMSelection>selection;
    result = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) { return result; }
    if (!selection) { return NS_ERROR_NULL_POINTER; }

    selection->StartBatchChanges();
    if (mTxnMgr) {
      result = mTxnMgr->Do(aTxn);
    }
    else {
      result = aTxn->Do();
    }
    if (NS_SUCCEEDED(result)) {
      result = DoAfterDoTransaction(aTxn);
    }
  
    selection->EndBatchChanges(); // no need to check result here, don't lose result of operation
  }
 
  NS_POSTCONDITION((NS_SUCCEEDED(result)), "transaction did not execute properly\n");

  return result;
}


NS_IMETHODIMP
nsEditor::EnableUndo(PRBool aEnable)
{
  nsresult result=NS_OK;

  if (PR_TRUE==aEnable)
  {
    if (!mTxnMgr)
    {
      result = nsComponentManager::CreateInstance(kCTransactionManagerCID,
                                        nsnull,
                                        NS_GET_IID(nsITransactionManager), getter_AddRefs(mTxnMgr));
      if (NS_FAILED(result) || !mTxnMgr) {
        printf("ERROR: Failed to get TransactionManager instance.\n");
        return NS_ERROR_NOT_AVAILABLE;
      }
    }
    mTxnMgr->SetMaxTransactionCount(-1);
  }
  else
  { // disable the transaction manager if it is enabled
    if (mTxnMgr)
    {
      mTxnMgr->Clear();
      mTxnMgr->SetMaxTransactionCount(0);
    }
  }

  return result;
}


NS_IMETHODIMP
nsEditor::GetTransactionManager(nsITransactionManager* *aTxnManager)
{
  NS_ENSURE_ARG_POINTER(aTxnManager);
  
  *aTxnManager = NULL;
  if (!mTxnMgr)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aTxnManager = mTxnMgr);
  return NS_OK;
}


NS_IMETHODIMP 
nsEditor::Undo(PRUint32 aCount)
{
  if (gNoisy) { printf("Editor::Undo ----------\n"); }
  nsresult result = NS_OK;
  ForceCompositionEnd();

  nsAutoRules beginRulesSniffing(this, kOpUndo, nsIEditor::eNone);

  if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
  {
    PRUint32 i=0;
    for ( ; i<aCount; i++)
    {
      result = mTxnMgr->Undo();

      if (NS_SUCCEEDED(result))
        result = DoAfterUndoTransaction();
          
      if (NS_FAILED(result))
        break;
    }
  }

  NotifyEditorObservers();  
  return result;
}


NS_IMETHODIMP nsEditor::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  aIsEnabled = ((PRBool)((nsITransactionManager *)0!=mTxnMgr.get()));
  if (aIsEnabled)
  {
    PRInt32 numTxns=0;
    mTxnMgr->GetNumberOfUndoItems(&numTxns);
    aCanUndo = ((PRBool)(0!=numTxns));
  }
  else {
    aCanUndo = PR_FALSE;
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsEditor::Redo(PRUint32 aCount)
{
  if (gNoisy) { printf("Editor::Redo ----------\n"); }
  nsresult result = NS_OK;

  nsAutoRules beginRulesSniffing(this, kOpRedo, nsIEditor::eNone);

  if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
  {
    PRUint32 i=0;
    for ( ; i<aCount; i++)
    {
      result = mTxnMgr->Redo();

      if (NS_SUCCEEDED(result))
        result = DoAfterRedoTransaction();

      if (NS_FAILED(result))
        break;
    }
  }

  NotifyEditorObservers();  
  return result;
}


NS_IMETHODIMP nsEditor::CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)
{
  aIsEnabled = ((PRBool)((nsITransactionManager *)0!=mTxnMgr.get()));
  if (aIsEnabled)
  {
    PRInt32 numTxns=0;
    mTxnMgr->GetNumberOfRedoItems(&numTxns);
    aCanRedo = ((PRBool)(0!=numTxns));
  }
  else {
    aCanRedo = PR_FALSE;
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsEditor::BeginTransaction()
{
  BeginUpdateViewBatch();

  if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
  {
    mTxnMgr->BeginBatch();
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::EndTransaction()
{
  if ((nsITransactionManager *)nsnull!=mTxnMgr.get())
  {
    mTxnMgr->EndBatch();
  }

  EndUpdateViewBatch();

  return NS_OK;
}


// These two routines are similar to the above, but do not use
// the transaction managers batching feature.  Instead we use
// a placeholder transaction to wrap up any further transaction
// while the batch is open.  The advantage of this is that
// placeholder transactions can later merge, if needed.  Merging
// is unavailable between transaction manager batches.

NS_IMETHODIMP 
nsEditor::BeginPlaceHolderTransaction(nsIAtom *aName)
{
  NS_PRECONDITION(mPlaceHolderBatch >= 0, "negative placeholder batch count!");
  if (!mPlaceHolderBatch)
  {
    // time to turn on the batch
    BeginUpdateViewBatch();
    mPlaceHolderTxn = nsnull;
    mPlaceHolderName = aName;
    nsCOMPtr<nsIDOMSelection> selection;
    nsresult res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    mSelState = new nsSelectionState();
    mSelState->SaveSelection(selection);
  }
  mPlaceHolderBatch++;

  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::EndPlaceHolderTransaction()
{
  NS_PRECONDITION(mPlaceHolderBatch > 0, "zero or negative placeholder batch count when ending batch!");
  if (mPlaceHolderBatch == 1)
  {
    // time to turn off the batch
    EndUpdateViewBatch();
    // make sure selection is in view
    nsCOMPtr<nsISelectionController> selCon;
    if (NS_SUCCEEDED(GetSelectionController(getter_AddRefs(selCon))) && selCon)
      selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL, nsISelectionController::SELECTION_FOCUS_REGION);

    if (mSelState)
    {
      // we saved the selection state, but never got to hand it to placeholder 
      // (else we ould have nulled out this pointer), so destroy it to prevent leaks.
      delete mSelState;
      mSelState = nsnull;
    }
    if (mPlaceHolderTxn)  // we might have never made a placeholder if no action took place
    {
      nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryReferent(mPlaceHolderTxn);
      if (plcTxn) 
      {
        plcTxn->EndPlaceHolderBatch();
      }
      else  
      {
        // in the future we will check to make sure undo is off here,
        // since that is the only known case where the placeholdertxn would disappear on us.
        // For now just removing the assert.
      }
      // notify editor observers of action unless it is uncommitted IME
      if (!mInIMEMode) NotifyEditorObservers();
    }
  }
  mPlaceHolderBatch--;
  
  return NS_OK;
}

NS_IMETHODIMP nsEditor::ShouldTxnSetSelection(PRBool *aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
  *aResult = mShouldTxnSetSelection;
  return NS_OK;
}


// XXX: the rule system should tell us which node to select all on (ie, the root, or the body)
NS_IMETHODIMP nsEditor::SelectAll()
{
  if (!mDocWeak || !mPresShellWeak) { return NS_ERROR_NOT_INITIALIZED; }
  ForceCompositionEnd();

  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mSelConWeak);
  if (!selCon) return NS_ERROR_NOT_INITIALIZED;
  nsresult result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection)
  {
    result = SelectEntireDocument(selection);
  }
  return result;
}

NS_IMETHODIMP nsEditor::BeginningOfDocument()
{
  if (!mDocWeak || !mPresShellWeak) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mSelConWeak);
  if (!selCon) return NS_ERROR_NOT_INITIALIZED;
  nsresult result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection)
  {
    nsCOMPtr<nsIDOMNodeList> nodeList;
    nsAutoString bodyTag; bodyTag.AssignWithConversion("body");
    nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
    if (!doc) return NS_ERROR_NOT_INITIALIZED;
    result = doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
    if ((NS_SUCCEEDED(result)) && nodeList)
    {
      PRUint32 count;
      nodeList->GetLength(&count);
      if (1!=count) { return NS_ERROR_UNEXPECTED; }
      nsCOMPtr<nsIDOMNode> bodyNode;
      result = nodeList->Item(0, getter_AddRefs(bodyNode));
      if ((NS_SUCCEEDED(result)) && bodyNode)
      {
        // Get the first child of the body node:
        nsCOMPtr<nsIDOMNode> firstNode;
        result = GetFirstEditableNode(bodyNode, &firstNode);
        if (firstNode)
        {
          // if firstNode is text, set selection to beginning of the text node
          if (IsTextNode(firstNode)) 
          {
            result = selection->Collapse(firstNode, 0);
          }
          else
          { // otherwise, it's a leaf node and we set the selection just in front of it
            nsCOMPtr<nsIDOMNode> parentNode;
            result = firstNode->GetParentNode(getter_AddRefs(parentNode));
            if (NS_FAILED(result)) { return result; }
            if (!parentNode) { return NS_ERROR_NULL_POINTER; }
            PRInt32 offsetInParent;
            result = nsEditor::GetChildOffset(firstNode, parentNode, offsetInParent);
            if (NS_FAILED(result)) return result;
            result = selection->Collapse(parentNode, offsetInParent);
          }
          ScrollIntoView(PR_TRUE);
        }
        else
        {
          // just the body node, set selection to inside the body
          result = selection->Collapse(bodyNode, 0);
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsEditor::EndOfDocument() 
{ 
  if (!mDocWeak || !mPresShellWeak) { return NS_ERROR_NOT_INITIALIZED; } 
  nsresult res; 
  
  // get selection 
  nsCOMPtr<nsIDOMSelection> selection; 
  res = GetSelection(getter_AddRefs(selection)); 
  if (NS_FAILED(res)) return res; 
  if (!selection)   return NS_ERROR_NULL_POINTER; 
  
  // get the root element 
  nsCOMPtr<nsIDOMElement> rootElement; 
  res = GetRootElement(getter_AddRefs(rootElement)); 
  if (NS_FAILED(res)) return res; 
  if (!rootElement)   return NS_ERROR_NULL_POINTER; 
  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(rootElement); 
  
  // get the length of the rot element 
  PRUint32 len; 
  res = GetLengthOfDOMNode(rootNode, len); 
  if (NS_FAILED(res)) return res; 
  
  // set the selection to after the last child of the root element 
  res = selection->Collapse(rootNode, (PRInt32)len); 
  return res; 
} 
  
NS_IMETHODIMP
nsEditor::GetDocumentModified(PRBool *outDocModified)
{
  if (!outDocModified)
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDOMDocument>  theDoc;
  nsresult  rv = GetDocument(getter_AddRefs(theDoc));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(theDoc, &rv);
  if (NS_FAILED(rv)) return rv;

  PRInt32  modCount = 0;
  diskDoc->GetModCount(&modCount);

  *outDocModified = (modCount != 0);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetDocumentCharacterSet(PRUnichar** characterSet)
{
  nsresult rv;
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIPresShell> presShell;
  nsAutoString  character_set;

  if (characterSet==nsnull) return NS_ERROR_NULL_POINTER;

  rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_SUCCEEDED(rv))
  {
    presShell->GetDocument(getter_AddRefs(doc));
    if (doc ) {
    rv = doc->GetDocumentCharacterSet(character_set);
    if (NS_SUCCEEDED(rv)) *characterSet=character_set.ToNewUnicode();
    return rv;
  }
  }

  return rv;

}

NS_IMETHODIMP
nsEditor::SetDocumentCharacterSet(const PRUnichar* characterSet)
{
  nsresult rv;
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIPresShell> presShell;
  nsAutoString character_set(characterSet);

  if (characterSet==nsnull) return NS_ERROR_NULL_POINTER;

  rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_SUCCEEDED(rv))
  {
    presShell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      return doc->SetDocumentCharacterSet(character_set);
    }
  }

  return rv;
}

NS_IMETHODIMP 
nsEditor::SaveFile(nsFileSpec *aFileSpec, PRBool aReplaceExisting,
                   PRBool aSaveCopy, const nsString& aFormat)
{
  if (!aFileSpec)
    return NS_ERROR_NULL_POINTER;
  
  ForceCompositionEnd();

  // get the document
  nsCOMPtr<nsIDOMDocument> doc;
  nsresult rv = GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(rv)) return rv;
  if (!doc) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(doc);
  if (!diskDoc)
    return NS_ERROR_NO_INTERFACE;

  // Should we prettyprint?
  PRUint32 flags = 0;
  NS_WITH_SERVICE(nsIPref, prefService, kPrefServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && prefService)
  {
    PRBool prettyprint = PR_FALSE;;
    rv = prefService->GetBoolPref("editor.prettyprint", &prettyprint);
    if (NS_SUCCEEDED(rv) && prettyprint)
      flags |= nsIDocumentEncoder::OutputFormatted;
  }

  nsAutoString useDocCharset;

  rv = diskDoc->SaveFile(aFileSpec, aReplaceExisting, aSaveCopy, 
                          aFormat, useDocCharset, flags);
  if (NS_SUCCEEDED(rv))
    DoAfterDocumentSave();

  return rv;
}

NS_IMETHODIMP 
nsEditor::SetAttribute(nsIDOMElement *aElement, const nsString& aAttribute, const nsString& aValue)
{
  ChangeAttributeTxn *txn;
  nsresult result = CreateTxnForSetAttribute(aElement, aAttribute, aValue, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);
  return result;
}


NS_IMETHODIMP 
nsEditor::GetAttributeValue(nsIDOMElement *aElement, 
                            const nsString& aAttribute, 
                            nsString& aResultValue, 
                            PRBool&   aResultIsSet)
{
  aResultIsSet=PR_FALSE;
  nsresult result=NS_OK;
  if (nsnull!=aElement)
  {
    nsCOMPtr<nsIDOMAttr> attNode;
    result = aElement->GetAttributeNode(aAttribute, getter_AddRefs(attNode));
    if ((NS_SUCCEEDED(result)) && attNode)
    {
      attNode->GetSpecified(&aResultIsSet);
      attNode->GetValue(aResultValue);
    }
  }
  return result;
}

NS_IMETHODIMP 
nsEditor::RemoveAttribute(nsIDOMElement *aElement, const nsString& aAttribute)
{
  ChangeAttributeTxn *txn;
  nsresult result = CreateTxnForRemoveAttribute(aElement, aAttribute, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);
  return result;
}


NS_IMETHODIMP
nsEditor::MarkNodeDirty(nsIDOMNode* aNode)
{  
  //  mark the node dirty.
  nsCOMPtr<nsIDOMElement> element (do_QueryInterface(aNode));
  if (element)
    element->SetAttribute(NS_ConvertASCIItoUCS2("_moz_dirty"), nsAutoString());
  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  main node manipulation routines 
#pragma mark -
#endif

NS_IMETHODIMP nsEditor::CreateNode(const nsString& aTag,
                                   nsIDOMNode *    aParent,
                                   PRInt32         aPosition,
                                   nsIDOMNode **   aNewNode)
{
  PRInt32 i;
  nsIEditActionListener *listener;

  nsAutoRules beginRulesSniffing(this, kOpCreateNode, nsIEditor::eNext);
  
  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->WillCreateNode(aTag, aParent, aPosition);
    }
  }

  CreateElementTxn *txn;
  nsresult result = CreateTxnForCreateElement(aTag, aParent, aPosition, &txn);
  if (NS_SUCCEEDED(result)) 
  {
    result = Do(txn);  
    if (NS_SUCCEEDED(result)) 
    {
      result = txn->GetNewNode(aNewNode);
      NS_ASSERTION((NS_SUCCEEDED(result)), "GetNewNode can't fail if txn::Do succeeded.");
    }
  }
  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);
  
  mRangeUpdater.SelAdjCreateNode(aParent, aPosition);
  
  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->DidCreateNode(aTag, *aNewNode, aParent, aPosition, result);
    }
  }

  return result;
}


NS_IMETHODIMP nsEditor::InsertNode(nsIDOMNode * aNode,
                                   nsIDOMNode * aParent,
                                   PRInt32      aPosition)
{
  PRInt32 i;
  nsIEditActionListener *listener;
  nsAutoRules beginRulesSniffing(this, kOpInsertNode, nsIEditor::eNext);

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->WillInsertNode(aNode, aParent, aPosition);
    }
  }

  InsertElementTxn *txn;
  nsresult result = CreateTxnForInsertElement(aNode, aParent, aPosition, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }
  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);

  mRangeUpdater.SelAdjInsertNode(aParent, aPosition);

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->DidInsertNode(aNode, aParent, aPosition, result);
    }
  }

  return result;
}


NS_IMETHODIMP 
nsEditor::SplitNode(nsIDOMNode * aNode,
                    PRInt32      aOffset,
                    nsIDOMNode **aNewLeftNode)
{
  PRInt32 i;
  nsIEditActionListener *listener;
  nsAutoRules beginRulesSniffing(this, kOpSplitNode, nsIEditor::eNext);

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->WillSplitNode(aNode, aOffset);
    }
  }

  SplitElementTxn *txn;
  nsresult result = CreateTxnForSplitNode(aNode, aOffset, &txn);
  if (NS_SUCCEEDED(result))  
  {
    result = Do(txn);
    if (NS_SUCCEEDED(result))
    {
      result = txn->GetNewNode(aNewLeftNode);
      NS_ASSERTION((NS_SUCCEEDED(result)), "result must succeeded for GetNewNode");
    }
  }
  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);

  mRangeUpdater.SelAdjSplitNode(aNode, aOffset, *aNewLeftNode);

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
      {
        nsIDOMNode *ptr = (aNewLeftNode) ? *aNewLeftNode : 0;
        listener->DidSplitNode(aNode, aOffset, ptr, result);
      }
    }
  }

  return result;
}



NS_IMETHODIMP
nsEditor::JoinNodes(nsIDOMNode * aLeftNode,
                    nsIDOMNode * aRightNode,
                    nsIDOMNode * aParent)
{
  PRInt32 i, offset;
  PRUint32 oldLeftNodeLen;
  nsIEditActionListener *listener;
  nsAutoRules beginRulesSniffing(this, kOpJoinNode, nsIEditor::ePrevious);

  // remember some values; later used for saved selection updating.
  // find the offset between the nodes to be joined.
  nsresult result = GetChildOffset(aRightNode, aParent, offset);
  if (NS_FAILED(result)) return result;
  // find the number of children of the lefthand node
  result = GetLengthOfDOMNode(aLeftNode, oldLeftNodeLen);
  if (NS_FAILED(result)) return result;

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->WillJoinNodes(aLeftNode, aRightNode, aParent);
    }
  }

  JoinElementTxn *txn;
  result = CreateTxnForJoinNode(aLeftNode, aRightNode, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }

  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);

  mRangeUpdater.SelAdjJoinNodes(aLeftNode, aRightNode, aParent, offset, (PRInt32)oldLeftNodeLen);
  
  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->DidJoinNodes(aLeftNode, aRightNode, aParent, result);
    }
  }

  return result;
}


NS_IMETHODIMP nsEditor::DeleteNode(nsIDOMNode * aElement)
{
  PRInt32 i, offset;
  nsCOMPtr<nsIDOMNode> parent;
  nsIEditActionListener *listener;
  nsAutoRules beginRulesSniffing(this, kOpCreateNode, nsIEditor::ePrevious);

  // save node location for selection updating code.
  nsresult result = GetNodeLocation(aElement, &parent, &offset);
  if (NS_FAILED(result)) return result;

  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->WillDeleteNode(aElement);
    }
  }

  DeleteElementTxn *txn;
  result = CreateTxnForDeleteElement(aElement, &txn);
  if (NS_SUCCEEDED(result))  {
    result = Do(txn);  
  }

  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);

  mRangeUpdater.SelAdjDeleteNode(aElement, parent, offset);
  
  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->DidDeleteNode(aElement, result);
    }
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////
// ReplaceContainer: replace inNode with a new node (outNode) which is contructed 
//                   to be of type aNodeType.  Put inNodes children into outNode.
//                   Callers responsibility to make sure inNode's children can 
//                   go in outNode.
nsresult
nsEditor::ReplaceContainer(nsIDOMNode *inNode, 
                           nsCOMPtr<nsIDOMNode> *outNode, 
                           const nsString &aNodeType,
                           const nsString *aAttribute,
                           const nsString *aValue,
                           PRBool aCloneAttributes)
{
  if (!inNode || !outNode)
    return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  res = GetNodeLocation(inNode, &parent, &offset);
  if (NS_FAILED(res)) return res;

  // get our doc
  nsCOMPtr<nsIDOMDocument>doc;
  res = GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(res)) return res;
  if (!doc) return NS_ERROR_NULL_POINTER;

  // create new container
  nsCOMPtr<nsIDOMElement> elem;
  nsCOMPtr<nsIContent> newContent;

  //new call to use instead to get proper HTML element, bug# 39919
  res = CreateHTMLContent(aNodeType, getter_AddRefs(newContent));
  elem = do_QueryInterface(newContent);
  if (NS_FAILED(res)) return res;
    *outNode = do_QueryInterface(elem);
  
  // set attribute if needed
  if (aAttribute && aValue && !aAttribute->IsEmpty())
  {
    res = elem->SetAttribute(*aAttribute, *aValue);
    if (NS_FAILED(res)) return res;
  }
  if (aCloneAttributes)
  {
    nsCOMPtr<nsIDOMNode>newNode = do_QueryInterface(elem);
    res = CloneAttributes(newNode, inNode);
    if (NS_FAILED(res)) return res;
  }
  
  // notify our internal selection state listener
  // (Note: A nsAutoSelectionReset object must be created
  //  before calling this to initialize mRangeUpdater)
  nsAutoReplaceContainerSelNotify selStateNotify(mRangeUpdater, inNode, *outNode);
  
  nsCOMPtr<nsIDOMNode> child;
  PRBool bHasMoreChildren;
  inNode->HasChildNodes(&bHasMoreChildren);
  while (bHasMoreChildren)
  {
    inNode->GetLastChild(getter_AddRefs(child));
    res = DeleteNode(child);
    if (NS_FAILED(res)) return res;
    res = InsertNode(child, *outNode, 0);
    if (NS_FAILED(res)) return res;
    inNode->HasChildNodes(&bHasMoreChildren);
  }

  // insert new container into tree
  res = InsertNode( *outNode, parent, offset);
  if (NS_FAILED(res)) return res;
  
  // delete old container
  return DeleteNode(inNode);
}

///////////////////////////////////////////////////////////////////////////
// RemoveContainer: remove inNode, reparenting it's children into their
//                  the parent of inNode
//
nsresult
nsEditor::RemoveContainer(nsIDOMNode *inNode)
{
  if (!inNode)
    return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  PRUint32 nodeOrigLen;
  
  res = GetNodeLocation(inNode, &parent, &offset);
  if (NS_FAILED(res)) return res;
  
  // loop through the child nodes of inNode and promote them
  // into inNode's parent.
  PRBool bHasMoreChildren;
  inNode->HasChildNodes(&bHasMoreChildren);
  nsCOMPtr<nsIDOMNodeList> nodeList;
  res = inNode->GetChildNodes(getter_AddRefs(nodeList));
  if (NS_FAILED(res)) return res;
  if (!nodeList) return NS_ERROR_NULL_POINTER;
  nodeList->GetLength(&nodeOrigLen);

  // notify our internal selection state listener
  nsAutoRemoveContainerSelNotify selNotify(mRangeUpdater, inNode, parent, offset, nodeOrigLen);
                                   
  nsCOMPtr<nsIDOMNode> child;
  while (bHasMoreChildren)
  {
    inNode->GetLastChild(getter_AddRefs(child));
    res = DeleteNode(child);
    if (NS_FAILED(res)) return res;
    res = InsertNode(child, parent, offset);
    if (NS_FAILED(res)) return res;
    inNode->HasChildNodes(&bHasMoreChildren);
  }
  res = DeleteNode(inNode);
  return res;
}


///////////////////////////////////////////////////////////////////////////
// InsertContainerAbove:  insert a new parent for inNode, returned in outNode,
//                   which is contructed to be of type aNodeType.  outNode becomes
//                   a child of inNode's earlier parent.
//                   Callers responsibility to make sure inNode's can be child
//                   of outNode, and outNode can be child of old parent.
nsresult
nsEditor::InsertContainerAbove( nsIDOMNode *inNode, 
                                nsCOMPtr<nsIDOMNode> *outNode, 
                                const nsString &aNodeType,
                                const nsString *aAttribute,
                                const nsString *aValue)
{
  if (!inNode || !outNode)
    return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  res = GetNodeLocation(inNode, &parent, &offset);
  if (NS_FAILED(res)) return res;
  
  // get our doc
  nsCOMPtr<nsIDOMDocument>doc;
  res = GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(res)) return res;
  if (!doc) return NS_ERROR_NULL_POINTER;

  // create new container
  nsCOMPtr<nsIDOMElement> elem;
  nsCOMPtr<nsIContent> newContent;

  //new call to use instead to get proper HTML element, bug# 39919
  res = CreateHTMLContent(aNodeType, getter_AddRefs(newContent));
  elem = do_QueryInterface(newContent);
  if (NS_FAILED(res)) return res;
  *outNode = do_QueryInterface(elem);
  
  // set attribute if needed
  if (aAttribute && aValue && !aAttribute->IsEmpty())
  {
    res = elem->SetAttribute(*aAttribute, *aValue);
    if (NS_FAILED(res)) return res;
  }
  
  // notify our internal selection state listener
  nsAutoInsertContainerSelNotify selNotify(mRangeUpdater);
  
  // put inNode in new parent, outNode
  res = DeleteNode(inNode);
  if (NS_FAILED(res)) return res;
  res = InsertNode(inNode, *outNode, 0);
  if (NS_FAILED(res)) return res;
  
  // put new parent in doc
  res = InsertNode(*outNode, parent, offset);
  return res;
}

///////////////////////////////////////////////////////////////////////////
// MoveNode:  move aNode to {aParent,aOffset}
nsresult
nsEditor::MoveNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aOffset)
{
  if (!aNode || !aParent)
    return NS_ERROR_NULL_POINTER;
  nsresult res;
  
  nsCOMPtr<nsIDOMNode> oldParent;
  PRInt32 oldOffset;
  res = GetNodeLocation(aNode, &oldParent, &oldOffset);
  
  if (aOffset == -1)
  {
    PRUint32 unsignedOffset;
    // magic value meaning "move to end of aParent"
    res = GetLengthOfDOMNode(aParent, unsignedOffset);
    if (NS_FAILED(res)) return res;
    aOffset = (PRInt32)unsignedOffset;
  }
  
  // dont do anything if it's already in right place
  if ((aParent == oldParent.get()) && (oldOffset == aOffset)) return NS_OK;
  
  // notify our internal selection state listener
  nsAutoMoveNodeSelNotify selNotify(mRangeUpdater, oldParent, oldOffset, aParent, aOffset);
  
  // need to adjust aOffset if we are moving aNode further along in it's current parent
  if ((aParent == oldParent.get()) && (oldOffset < aOffset)) 
  {
    aOffset--;  // this is because when we delete aNode, it will make the offsets after it off by one
  }

  // put aNode in new parent
  res = DeleteNode(aNode);
  if (NS_FAILED(res)) return res;
  res = InsertNode(aNode, aParent, aOffset);
  return res;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  editor observer maintainance
#pragma mark -
#endif

NS_IMETHODIMP
nsEditor::AddEditorObserver(nsIEditorObserver *aObserver)
{
  // we don't keep ownership of the observers.  They must
  // remove themselves as observers before they are destroyed.
  
  if (!aObserver)
    return NS_ERROR_NULL_POINTER;

  if (!mEditorObservers)
  {
    mEditorObservers = new nsVoidArray();

    if (!mEditorObservers)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  // Make sure the listener isn't already on the list
  if (mEditorObservers->IndexOf(aObserver) == -1) 
  {
    if (!mEditorObservers->AppendElement((void *)aObserver))
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveEditorObserver(nsIEditorObserver *aObserver)
{
  if (!aObserver || !mEditorObservers)
    return NS_ERROR_FAILURE;

  if (!mEditorObservers->RemoveElement((void *)aObserver))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

void nsEditor::NotifyEditorObservers(void)
{
  if (mEditorObservers)
  {
    PRInt32 i;
    nsIEditorObserver *observer;
    for (i = 0; i < mEditorObservers->Count(); i++)
    {
      observer = (nsIEditorObserver*)mEditorObservers->ElementAt(i);
      if (observer)
        observer->EditAction();
    }
  }
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  action listener maintainance
#pragma mark -
#endif

NS_IMETHODIMP
nsEditor::AddEditActionListener(nsIEditActionListener *aListener)
{
  if (!aListener)
    return NS_ERROR_NULL_POINTER;

  if (!mActionListeners)
  {
    mActionListeners = new nsVoidArray();

    if (!mActionListeners)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  // Make sure the listener isn't already on the list
  if (mActionListeners->IndexOf(aListener) == -1) 
  {
    if (!mActionListeners->AppendElement((void *)aListener))
      return NS_ERROR_FAILURE;
    else
      NS_ADDREF(aListener);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveEditActionListener(nsIEditActionListener *aListener)
{
  if (!aListener || !mActionListeners)
    return NS_ERROR_FAILURE;

  if (!mActionListeners->RemoveElement((void *)aListener))
    return NS_ERROR_FAILURE;

  NS_IF_RELEASE(aListener);

  if (mActionListeners->Count() < 1)
  {
    delete mActionListeners;
    mActionListeners = 0;
  }

  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  docstate listener maintainance
#pragma mark -
#endif


NS_IMETHODIMP
nsEditor::AddDocumentStateListener(nsIDocumentStateListener *aListener)
{
  if (!aListener)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  
  if (!mDocStateListeners)
  {
    rv = NS_NewISupportsArray(getter_AddRefs(mDocStateListeners));
    if (NS_FAILED(rv)) return rv;
  }
  
  nsCOMPtr<nsISupports> iSupports = do_QueryInterface(aListener, &rv);
  if (NS_FAILED(rv)) return rv;
    
  // is it already in the list?
  PRInt32 foundIndex;
  if (NS_SUCCEEDED(mDocStateListeners->GetIndexOf(iSupports, &foundIndex)) && foundIndex != -1)
    return NS_OK;

  return mDocStateListeners->AppendElement(iSupports);
}


NS_IMETHODIMP
nsEditor::RemoveDocumentStateListener(nsIDocumentStateListener *aListener)
{
  if (!aListener || !mDocStateListeners)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;
  nsCOMPtr<nsISupports> iSupports = do_QueryInterface(aListener, &rv);
  if (NS_FAILED(rv)) return rv;

  return mDocStateListeners->RemoveElement(iSupports);
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  misc 
#pragma mark -
#endif

NS_IMETHODIMP nsEditor::OutputToString(nsAWritableString& aOutputString,
                                       const nsAReadableString& aFormatType,
                                       PRUint32 aFlags)
{
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsEditor::OutputToStream(nsIOutputStream* aOutputStream,
                                       const nsString& aFormatType,
                                       const nsString* aCharsetOverride,
                                       PRUint32 aFlags)
{
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::DumpContentTree()
{
  nsCOMPtr<nsIContent> root = do_QueryInterface(mBodyElement);
  if (root)  root->List(stdout);
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DebugDumpContent() const
{
  nsCOMPtr<nsIContent>content;
  nsCOMPtr<nsIDOMNodeList>nodeList;
  nsAutoString bodyTag; bodyTag.AssignWithConversion("body");
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
  if (nodeList)
  {
    PRUint32 count;
    nodeList->GetLength(&count);
    NS_ASSERTION(1==count, "there is not exactly 1 body in the document!");
    nsCOMPtr<nsIDOMNode>bodyNode;
    nodeList->Item(0, getter_AddRefs(bodyNode));
    if (bodyNode) {
      content = do_QueryInterface(bodyNode);
    }
  }
  content->List();
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed)
{
  NS_NOTREACHED("This should never get called. Overridden by subclasses");
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  support for selection preservation
#pragma mark -
#endif

PRBool   
nsEditor::ArePreservingSelection()
{
  if (mSavedSel.IsEmpty()) return PR_FALSE;
  return PR_TRUE;
}

nsresult 
nsEditor::PreserveSelectionAcrossActions(nsIDOMSelection *aSel)
{
  mSavedSel.SaveSelection(aSel);
  mRangeUpdater.RegisterSelectionState(mSavedSel);
  return NS_OK;
}

nsresult 
nsEditor::RestorePreservedSelection(nsIDOMSelection *aSel)
{
  if (mSavedSel.IsEmpty()) return NS_ERROR_FAILURE;
  mSavedSel.RestoreSelection(aSel);
  StopPreservingSelection();
  return NS_OK;
}

void     
nsEditor::StopPreservingSelection()
{
  mRangeUpdater.DropSelectionState(mSavedSel);
  mSavedSel.MakeEmpty();
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditorIMESupport 
#pragma mark -
#endif

//
// The BeingComposition method is called from the Editor Composition event listeners.
//
NS_IMETHODIMP
nsEditor::QueryComposition(nsTextEventReply* aReply)
{
  nsresult result;
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsISelectionController> selcon = do_QueryReferent(mSelConWeak);
  if (selcon)
    selcon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));

  nsCOMPtr<nsIDOMCharacterData> nodeAsText;
  nsCOMPtr<nsICaret> caretP; 
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  result = ps->GetCaret(getter_AddRefs(caretP));
  
  if (NS_SUCCEEDED(result) && caretP) {
    if (aReply) {
      caretP->GetWindowRelativeCoordinates(
		aReply->mCursorPosition,
		aReply->mCursorIsCollapsed, selection);
    }
  }
  return result;
}
NS_IMETHODIMP
nsEditor::BeginComposition(nsTextEventReply* aReply)
{
#ifdef DEBUG_tague
  printf("nsEditor::StartComposition\n");
#endif
  nsresult ret = QueryComposition(aReply);
  mInIMEMode = PR_TRUE;
  return ret;
}

NS_IMETHODIMP
nsEditor::EndComposition(void)
{
  if (!mInIMEMode) return NS_OK; // nothing to do
  
  nsresult result = NS_OK;

  // commit the IME transaction..we can get at it via the transaction mgr.
  // Note that this means IME won't work without an undo stack!
  if (mTxnMgr) 
  {
    nsITransaction *txn;
    result = mTxnMgr->PeekUndoStack(&txn);  
    // PeekUndoStack does not addref
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryInterface(txn);
    if (plcTxn)
    {
      result = plcTxn->Commit();
    }
  }

  /* reset the data we need to construct a transaction */
  mIMETextNode = do_QueryInterface(nsnull);
  mIMETextOffset = 0;
  mIMEBufferLength = 0;
  mInIMEMode = PR_FALSE;

  // notify editor observers of action
  NotifyEditorObservers();

  return result;
}

NS_IMETHODIMP
nsEditor::SetCompositionString(const nsString& aCompositionString, nsIPrivateTextRangeList* aTextRangeList,nsTextEventReply* aReply)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult
GetEditorContentWindow(nsIPresShell *aPresShell, nsIDOMElement *aRoot, nsIWidget **aResult)
{
  if (!aPresShell || !aRoot || !aResult)
    return NS_ERROR_NULL_POINTER;

  *aResult = 0;

  nsresult result;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aRoot);

  if (!content)
    return NS_ERROR_FAILURE;

  nsIFrame *frame = 0; // Not ref counted

  result = aPresShell->GetPrimaryFrameFor(content, &frame);

  if (NS_FAILED(result))
    return result;

  if (!frame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> presContext;

  result = aPresShell->GetPresContext(getter_AddRefs(presContext));

  if (NS_FAILED(result))
    return result;

  if (!presContext)
    return NS_ERROR_FAILURE;

  // Check first to see if this frame contains a view with a native widget.

  nsIView *view = 0; // Not ref counted

  result = frame->GetView(presContext, &view);

  if (NS_FAILED(result))
    return result;

  if (view)
  {
    result = view->GetWidget(*aResult);

    if (NS_FAILED(result))
      return result;

    if (*aResult)
      return NS_OK;
  }

  // frame doesn't have a view with a widget, so call GetWindow()
  // which will traverse it's parent hierarchy till it finds a
  // view with a widget.

  result = frame->GetWindow(presContext, aResult);

  if (NS_FAILED(result))
    return result;

  if (!*aResult)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::ForceCompositionEnd()
{

// We can test mInIMEMode and do some optimization for Mac and Window
// Howerver, since UNIX support over-the-spot, we cannot rely on that 
// flag for Unix.
// We should use nsILookAndFeel to resolve this

#if defined(XP_MAC) || defined(XP_PC)
  if(! mInIMEMode)
    return NS_OK;
#endif

#ifdef XP_UNIX
  if(mFlags & nsIHTMLEditor::eEditorPasswordMask)
	return NS_OK;
#endif

  nsresult res = NS_OK;
  nsCOMPtr<nsIPresShell> shell;

  res = GetPresShell(getter_AddRefs(shell));

  if (NS_FAILED(res))
    return res;

  if (!shell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWidget> widget;

  res = GetEditorContentWindow(shell, mBodyElement, getter_AddRefs(widget));

  if (NS_FAILED(res))
    return res;

  if (!widget)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIKBStateControl> kb = do_QueryInterface(widget);

  if(kb)
    res = kb->ResetInputState();
  
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetReconversionString(nsReconversionEventReply* aReply)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  public nsEditor methods 
#pragma mark -
#endif
/* Non-interface, public methods */


// This seems like too much work! There should be a "nsDOMDocument::GetBody()"
// Have a look in nsHTMLDocument. Maybe add it to nsIHTMLDocument.
NS_IMETHODIMP 
nsEditor::GetRootElement(nsIDOMElement **aBodyElement)
{
  nsresult result = NS_OK;

  if (!aBodyElement)
    return NS_ERROR_NULL_POINTER;

  *aBodyElement = 0;
  
  if (mBodyElement)
  {
    // if we have cached the body element, use that
    *aBodyElement = mBodyElement;
    NS_ADDREF(*aBodyElement);
    return result;
  }
  
  NS_PRECONDITION(mDocWeak, "bad state, null mDocWeak");
  if (!mDocWeak)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMNodeList>nodeList; 
  nsAutoString bodyTag; bodyTag.AssignWithConversion("body"); 

  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  result = doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));

  if (NS_FAILED(result))
    return result;
  
  if (!nodeList)
    return NS_ERROR_NULL_POINTER;

  PRUint32 count; 
  nodeList->GetLength(&count);

  if (count < 1)
    return NS_ERROR_FAILURE;

  // Use the first body node in the list:
  nsCOMPtr<nsIDOMNode> node;
  result = nodeList->Item(0, getter_AddRefs(node)); 
  if (NS_SUCCEEDED(result) && node)
  {
    //return node->QueryInterface(NS_GET_IID(nsIDOMElement), (void **)aBodyElement);
    // Is above equivalent to this:
    nsCOMPtr<nsIDOMElement> bodyElement = do_QueryInterface(node);
    if (bodyElement)
    {
      mBodyElement = do_QueryInterface(bodyElement);
      *aBodyElement = bodyElement;
      // A "getter" method should always addref
      NS_ADDREF(*aBodyElement);
    }
  }
  return result;
}


/** All editor operations which alter the doc should be prefaced
 *  with a call to StartOperation, naming the action and direction */
NS_IMETHODIMP
nsEditor::StartOperation(PRInt32 opID, nsIEditor::EDirection aDirection)
{
  mAction = opID;
  mDirection = aDirection;
  return NS_OK;
}


/** All editor operations which alter the doc should be followed
 *  with a call to EndOperation */
NS_IMETHODIMP
nsEditor::EndOperation()
{
  mAction = nsnull;
  mDirection = eNone;
  return NS_OK;
}


// Objects must be DOM elements
NS_IMETHODIMP
nsEditor::CloneAttributes(nsIDOMNode *aDestNode, nsIDOMNode *aSourceNode)
{
  nsresult result=NS_OK;

  if (!aDestNode || !aSourceNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMElement> destElement = do_QueryInterface(aDestNode);
  nsCOMPtr<nsIDOMElement> sourceElement = do_QueryInterface(aSourceNode);
  if (!destElement || !sourceElement)
    return NS_ERROR_NO_INTERFACE;

  nsAutoString name;
  nsAutoString value;
  nsCOMPtr<nsIDOMNamedNodeMap> sourceAttributes;
  sourceElement->GetAttributes(getter_AddRefs(sourceAttributes));
  nsCOMPtr<nsIDOMNamedNodeMap> destAttributes;
  destElement->GetAttributes(getter_AddRefs(destAttributes));
  if (!sourceAttributes || !destAttributes)
    return NS_ERROR_FAILURE;

  nsAutoEditBatch beginBatching(this);

  // Use transaction system for undo only if destination
  //   is already in the document
  PRBool destInBody = PR_TRUE;
  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult res = GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement)   return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> bodyNode = do_QueryInterface(bodyElement);
  nsCOMPtr<nsIDOMNode> p = aDestNode;
  while (p && p!= bodyNode)
  {
    nsCOMPtr<nsIDOMNode> tmp;
    if (NS_FAILED(p->GetParentNode(getter_AddRefs(tmp))) || !tmp)
    {
      destInBody = PR_FALSE;
      break;
    }
    p = tmp;
  }

  PRUint32 sourceCount;
  sourceAttributes->GetLength(&sourceCount);
  PRUint32 i, destCount;
  destAttributes->GetLength(&destCount);
  nsIDOMNode* attrNode;

  // Clear existing attributes
  for (i = 0; i < destCount; i++)
  {
    // always remove item number 0 (first item in list)
    if( NS_SUCCEEDED(destAttributes->Item(0, &attrNode)) && attrNode)
    {
      nsCOMPtr<nsIDOMAttr> destAttribute = do_QueryInterface(attrNode);
      if (destAttribute)
      {
        nsAutoString str;
        if (NS_SUCCEEDED(destAttribute->GetName(str)))
        {
          if (destInBody)
            RemoveAttribute(destElement, str);
          else
            destElement->RemoveAttribute(str);
        }
      }
    }
  }
  // Set just the attributes that the source element has
  for (i = 0; i < sourceCount; i++)
  {
    if( NS_SUCCEEDED(sourceAttributes->Item(i, &attrNode)) && attrNode)
    {
      nsCOMPtr<nsIDOMAttr> sourceAttribute = do_QueryInterface(attrNode);
      if (sourceAttribute)
      {
        nsAutoString sourceAttrName;
        if (NS_SUCCEEDED(sourceAttribute->GetName(sourceAttrName)))
        {
          nsAutoString sourceAttrValue;
          if (NS_SUCCEEDED(sourceAttribute->GetValue(sourceAttrValue)) &&
              !sourceAttrValue.IsEmpty())
          {
            if (destInBody)
              SetAttribute(destElement, sourceAttrName, sourceAttrValue);
            else
              destElement->SetAttribute(sourceAttrName, sourceAttrValue);
          } else {
            // Do we ever get here?
#if DEBUG_cmanske
            printf("Attribute in sourceAttribute has empty value in nsEditor::CloneAttributes()\n");
#endif
          }
        }        
      }
    }
  }
  return result;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  Protected and static methods 
#pragma mark -
#endif

NS_IMETHODIMP nsEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/** static helper method */
nsresult nsEditor::GetTextNodeTag(nsString& aOutString)
{
  aOutString.SetLength(0);
  static nsString *gTextNodeTag=nsnull;
  if (!gTextNodeTag)
  {
    if ( (gTextNodeTag = new nsString) == 0 )
      return NS_ERROR_OUT_OF_MEMORY;
    gTextNodeTag->AssignWithConversion("special text node tag");
  }
  aOutString = *gTextNodeTag;
  return NS_OK;
}


NS_IMETHODIMP nsEditor::InsertTextImpl(const nsString& aStringToInsert, 
                                          nsCOMPtr<nsIDOMNode> *aInOutNode, 
                                          PRInt32 *aInOutOffset,
                                          nsIDOMDocument *aDoc)
{
  // NOTE: caller *must* have already used nsAutoTxnsConserveSelection stack-based
  // class to turn off txn selection updating.  Caller also turned on rules sniffing
  // if desired.
  
  if (!aInOutNode || !*aInOutNode || !aInOutOffset || !aDoc) return NS_ERROR_NULL_POINTER;
  if (!mInIMEMode && aStringToInsert.IsEmpty()) return NS_OK;
  nsCOMPtr<nsIDOMText> nodeAsText = do_QueryInterface(*aInOutNode);
  PRInt32 offset = *aInOutOffset;
  nsresult res;
  if (mInIMEMode)
  {
    if (!nodeAsText)
    {
      // create a text node
      nsCOMPtr<nsIDOMNode> newNode;
      res = aDoc->CreateTextNode(nsAutoString(), getter_AddRefs(nodeAsText));
      if (NS_FAILED(res)) return res;
      if (!nodeAsText) return NS_ERROR_NULL_POINTER;
      newNode = do_QueryInterface(nodeAsText);
      // then we insert it into the dom tree
      res = InsertNode(newNode, *aInOutNode, offset);
      if (NS_FAILED(res)) return res;
      offset = 0;
    }
    res = InsertTextIntoTextNodeImpl(aStringToInsert, nodeAsText, offset);
    if (NS_FAILED(res)) return res;
  }
  else
  {
    if (nodeAsText)
    {
      // we are inserting text into an existing text node.
      res = InsertTextIntoTextNodeImpl(aStringToInsert, nodeAsText, offset);
      if (NS_FAILED(res)) return res;
      *aInOutOffset += aStringToInsert.Length();
    }
    else
    {
      nsCOMPtr<nsIDOMNode> newNode;
      // we are inserting text into a non-text node
      // first we have to create a textnode (this also populates it with the text)
      res = aDoc->CreateTextNode(aStringToInsert, getter_AddRefs(nodeAsText));
      if (NS_FAILED(res)) return res;
      if (!nodeAsText) return NS_ERROR_NULL_POINTER;
      newNode = do_QueryInterface(nodeAsText);
      // then we insert it into the dom tree
      res = InsertNode(newNode, *aInOutNode, offset);
      if (NS_FAILED(res)) return res;
      *aInOutNode = newNode;
      *aInOutOffset = aStringToInsert.Length();
    }
  }
  return res;
}


NS_IMETHODIMP nsEditor::InsertTextIntoTextNodeImpl(const nsString& aStringToInsert, 
                                                     nsIDOMCharacterData *aTextNode, 
                                                     PRInt32 aOffset)
{
  EditTxn *txn;
  nsresult result;
  if (mInIMEMode)
  {
    if (!mIMETextNode)
    {
      mIMETextNode = aTextNode;
      mIMETextOffset = aOffset;
    }
    result = CreateTxnForIMEText(aStringToInsert, (IMETextTxn**)&txn);
  }
  else
  {
    result = CreateTxnForInsertText(aStringToInsert, aTextNode, aOffset, (InsertTextTxn**)&txn);
  }
  if (NS_FAILED(result)) return result;  // we potentially leak txn here?

  // let listeners know whats up
  PRInt32 i;
  nsIEditActionListener *listener;
  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->WillInsertText(aTextNode, aOffset, aStringToInsert);
    }
  }
    
  BeginUpdateViewBatch();
  result = Do(txn);
  EndUpdateViewBatch();

  // let listeners know what happened
  if (mActionListeners)
  {
    for (i = 0; i < mActionListeners->Count(); i++)
    {
      listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
      if (listener)
        listener->DidInsertText(aTextNode, aOffset, aStringToInsert, result);
    }
  }

  // Added some cruft here for bug 43366.  Layout was crashing because we left an 
  // empty text node lying around in the document.  So I delete empty text nodes
  // caused by IME.  I have to mark the IME transaction as "fixed", which means
  // that furure ime txns won't merge with it.  This is because we don't want
  // future ime txns trying to put their text into a node that is no longer in
  // the document.  This does not break undo/redo, because all these txns are 
  // wrapped in a parent PlaceHolder txn, and placeholder txns are already 
  // savvy to having multiple ime txns inside them.
  
  // delete empty ime text node if there is one
  if (mInIMEMode && mIMETextNode)
  {
    PRUint32 len;
    mIMETextNode->GetLength(&len);
    if (!len)
    {
      DeleteNode(mIMETextNode);
      mIMETextNode = nsnull;
      ((IMETextTxn*)txn)->MarkFixed();  // mark the ime txn "fixed"
    }
  }
  
  // The transaction system (if any) has taken ownwership of txns.
  // aggTxn released at end of routine.
  NS_IF_RELEASE(txn);
  return result;
}


NS_IMETHODIMP nsEditor::SelectEntireDocument(nsIDOMSelection *aSelection)
{
  nsresult result;
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIDOMElement>bodyElement;
  result = GetRootElement(getter_AddRefs(bodyElement));
  if ((NS_SUCCEEDED(result)) && bodyElement)
  {
    nsCOMPtr<nsIDOMNode>bodyNode = do_QueryInterface(bodyElement);
    if (bodyNode)
    {
      result = aSelection->SelectAllChildren(bodyNode);
    }
    else {
      return NS_ERROR_NO_INTERFACE;
    }
  }
  return result;
}


nsresult nsEditor::GetFirstEditableNode(nsIDOMNode *aRoot, nsCOMPtr<nsIDOMNode> *outFirstNode)
{
  if (!aRoot || !outFirstNode) return NS_ERROR_NULL_POINTER;

  *outFirstNode = nsnull;

  nsCOMPtr<nsIDOMNode> node,next;
  nsresult rv = GetLeftmostChild(aRoot, getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;

  if (node && !IsEditable(node))
  {
    rv = GetNextNode(node, PR_TRUE, getter_AddRefs(next));
    node = next;
  }
  
  if (node.get() != aRoot) *outFirstNode = node;

  return rv;
}


nsresult nsEditor::GetLastEditableNode(nsIDOMNode *aRoot, nsCOMPtr<nsIDOMNode> *outLastNode)
{
  if (!aRoot || !outLastNode) return NS_ERROR_NULL_POINTER;

  *outLastNode = nsnull;

  nsCOMPtr<nsIDOMNode> node,next;
  nsresult rv = GetRightmostChild(aRoot, getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;

  if (node && !IsEditable(node))
  {
    rv = GetPriorNode(node, PR_TRUE, getter_AddRefs(next));
    node = next;
  }
  
  if (node.get() != aRoot) *outLastNode = node;

  return rv;
}



NS_IMETHODIMP
nsEditor::NotifyDocumentListeners(TDocumentListenerNotification aNotificationType)
{
  if (!mDocStateListeners)
    return NS_OK;    // maybe there just aren't any.
 
  PRUint32 numListeners;
  nsresult rv = mDocStateListeners->Count(&numListeners);
  if (NS_FAILED(rv)) return rv;

  PRUint32 i;
  switch (aNotificationType)
  {
    case eDocumentCreated:
      for (i = 0; i < numListeners;i++)
      {
        nsCOMPtr<nsISupports> iSupports = getter_AddRefs(mDocStateListeners->ElementAt(i));
        nsCOMPtr<nsIDocumentStateListener> thisListener = do_QueryInterface(iSupports);
        if (thisListener)
        {
          rv = thisListener->NotifyDocumentCreated();
          if (NS_FAILED(rv))
            break;
        }
      }
      break;
      
    case eDocumentToBeDestroyed:
      for (i = 0; i < numListeners;i++)
      {
        nsCOMPtr<nsISupports> iSupports = getter_AddRefs(mDocStateListeners->ElementAt(i));
        nsCOMPtr<nsIDocumentStateListener> thisListener = do_QueryInterface(iSupports);
        if (thisListener)
        {
          rv = thisListener->NotifyDocumentWillBeDestroyed();
          if (NS_FAILED(rv))
            break;
        }
      }
      break;
  
    case eDocumentStateChanged:
      {
        PRBool docIsDirty;
        rv = GetDocumentModified(&docIsDirty);
        if (NS_FAILED(rv)) return rv;
        
        if (docIsDirty == mDocDirtyState)
          return NS_OK;
        
        mDocDirtyState = (PRInt8)docIsDirty;
        
        for (i = 0; i < numListeners;i++)
        {
          nsCOMPtr<nsISupports> iSupports = getter_AddRefs(mDocStateListeners->ElementAt(i));
          nsCOMPtr<nsIDocumentStateListener> thisListener = do_QueryInterface(iSupports);
          if (thisListener)
          {
            rv = thisListener->NotifyDocumentStateChanged(mDocDirtyState);
            if (NS_FAILED(rv))
              break;
          }
        }
      }
      break;
    
    default:
      NS_NOTREACHED("Unknown notification");
  }

  return rv;
}


NS_IMETHODIMP nsEditor::CreateTxnForInsertText(const nsString & aStringToInsert,
                                               nsIDOMCharacterData *aTextNode,
                                               PRInt32 aOffset,
                                               InsertTextTxn ** aTxn)
{
  if (!aTextNode || !aTxn) return NS_ERROR_NULL_POINTER;
  nsresult result;

  result = TransactionFactory::GetNewTransaction(InsertTextTxn::GetCID(), (EditTxn **)aTxn);
  if (NS_FAILED(result)) return result;
  if (!*aTxn) return NS_ERROR_OUT_OF_MEMORY;
  result = (*aTxn)->Init(aTextNode, aOffset, aStringToInsert, this);
  return result;
}


NS_IMETHODIMP nsEditor::DeleteText(nsIDOMCharacterData *aElement,
                              PRUint32             aOffset,
                              PRUint32             aLength)
{
  DeleteTextTxn *txn;
  nsresult result = CreateTxnForDeleteText(aElement, aOffset, aLength, &txn);
  nsAutoRules beginRulesSniffing(this, kOpDeleteText, nsIEditor::ePrevious);
  if (NS_SUCCEEDED(result))  
  {
    // let listeners know whats up
    PRInt32 i;
    nsIEditActionListener *listener;
    if (mActionListeners)
    {
      for (i = 0; i < mActionListeners->Count(); i++)
      {
        listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
        if (listener)
          listener->WillDeleteText(aElement, aOffset, aLength);
      }
    }
    
    result = Do(txn); 
    
    // let listeners know what happened
    if (mActionListeners)
    {
      for (i = 0; i < mActionListeners->Count(); i++)
      {
        listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
        if (listener)
          listener->DidDeleteText(aElement, aOffset, aLength, result);
      }
    }
  }
  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);
  return result;
}


NS_IMETHODIMP nsEditor::CreateTxnForDeleteText(nsIDOMCharacterData *aElement,
                                               PRUint32             aOffset,
                                               PRUint32             aLength,
                                               DeleteTextTxn      **aTxn)
{
  nsresult result=NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(DeleteTextTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aElement, aOffset, aLength);
    }
  }
  return result;
}




NS_IMETHODIMP nsEditor::CreateTxnForSplitNode(nsIDOMNode *aNode,
                                         PRUint32    aOffset,
                                         SplitElementTxn **aTxn)
{
  nsresult result=NS_ERROR_NULL_POINTER;
  if (nsnull != aNode)
  {
    result = TransactionFactory::GetNewTransaction(SplitElementTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aNode, aOffset);
    }
  }
  return result;
}

NS_IMETHODIMP nsEditor::CreateTxnForJoinNode(nsIDOMNode  *aLeftNode,
                                             nsIDOMNode  *aRightNode,
                                             JoinElementTxn **aTxn)
{
  nsresult result=NS_ERROR_NULL_POINTER;
  if ((nsnull != aLeftNode) && (nsnull != aRightNode))
  {
    result = TransactionFactory::GetNewTransaction(JoinElementTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aLeftNode, aRightNode);
    }
  }
  return result;
}


// END nsEditor core implementation

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsEditor public static helper methods 
#pragma mark -
#endif

// BEGIN nsEditor public helper methods

nsresult
nsEditor::SplitNodeImpl(nsIDOMNode * aExistingRightNode,
                        PRInt32      aOffset,
                        nsIDOMNode*  aNewLeftNode,
                        nsIDOMNode*  aParent)
{

  if (gNoisy) { printf("SplitNodeImpl: left=%p, right=%p, offset=%d\n", aNewLeftNode, aExistingRightNode, aOffset); }
  
  nsresult result;
  NS_ASSERTION(((nsnull!=aExistingRightNode) &&
                (nsnull!=aNewLeftNode) &&
                (nsnull!=aParent)),
                "null arg");
  if ((nsnull!=aExistingRightNode) &&
      (nsnull!=aNewLeftNode) &&
      (nsnull!=aParent))
  {
    // get selection
    nsCOMPtr<nsIDOMSelection> selection;
    result = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) return result;
    if (!selection) return NS_ERROR_NULL_POINTER;

    // remember some selection points
    nsCOMPtr<nsIDOMNode> selStartNode, selEndNode;
    PRInt32 selStartOffset, selEndOffset;
    result = GetStartNodeAndOffset(selection, &selStartNode, &selStartOffset);
    if (NS_FAILED(result)) selStartNode = nsnull;  // if selection is cleared, remember that
    result = GetEndNodeAndOffset(selection, &selEndNode, &selEndOffset);
    if (NS_FAILED(result)) selStartNode = nsnull;  // if selection is cleared, remember that

    nsCOMPtr<nsIDOMNode> resultNode;
    result = aParent->InsertBefore(aNewLeftNode, aExistingRightNode, getter_AddRefs(resultNode));
    //printf("  after insert\n"); content->List();  // DEBUG
    if (NS_SUCCEEDED(result))
    {
      // split the children between the 2 nodes
      // at this point, aExistingRightNode has all the children
      // move all the children whose index is < aOffset to aNewLeftNode
      if (0<=aOffset) // don't bother unless we're going to move at least one child
      {
        // if it's a text node, just shuffle around some text
        nsCOMPtr<nsIDOMCharacterData> rightNodeAsText( do_QueryInterface(aExistingRightNode) );
        nsCOMPtr<nsIDOMCharacterData> leftNodeAsText( do_QueryInterface(aNewLeftNode) );
        if (leftNodeAsText && rightNodeAsText)
        {
          // fix right node
          nsAutoString leftText;
          rightNodeAsText->SubstringData(0, aOffset, leftText);
          rightNodeAsText->DeleteData(0, aOffset);
          // fix left node
          leftNodeAsText->SetData(leftText);
          // moose          
        }
        else
        {  // otherwise it's an interior node, so shuffle around the children
           // go through list backwards so deletes don't interfere with the iteration
          nsCOMPtr<nsIDOMNodeList> childNodes;
          result = aExistingRightNode->GetChildNodes(getter_AddRefs(childNodes));
          if ((NS_SUCCEEDED(result)) && (childNodes))
          {
            PRInt32 i=aOffset-1;
            for ( ; ((NS_SUCCEEDED(result)) && (0<=i)); i--)
            {
              nsCOMPtr<nsIDOMNode> childNode;
              result = childNodes->Item(i, getter_AddRefs(childNode));
              if ((NS_SUCCEEDED(result)) && (childNode))
              {
                result = aExistingRightNode->RemoveChild(childNode, getter_AddRefs(resultNode));
                //printf("  after remove\n"); content->List();  // DEBUG
                if (NS_SUCCEEDED(result))
                {
                  nsCOMPtr<nsIDOMNode> firstChild;
                  aNewLeftNode->GetFirstChild(getter_AddRefs(firstChild));
                  result = aNewLeftNode->InsertBefore(childNode, firstChild, getter_AddRefs(resultNode));
                  //printf("  after append\n"); content->List();  // DEBUG
                }
              }
            }
          }        
        }
        // handle selection
        if (GetShouldTxnSetSelection())
        {
          // editor wants us to set selection at split point
          selection->Collapse(aNewLeftNode, aOffset);
        }
        else if (selStartNode)   
        {
          // else adjust the selection if needed.  if selStartNode is null, then there was no selection.
          // HACK: this is overly simplified - multi-range selections need more work than this
          if (selStartNode.get() == aExistingRightNode)
          {
            if (selStartOffset < aOffset)
            {
              selStartNode = aNewLeftNode;
            }
            else
            {
              selStartOffset -= aOffset;
            }
          }
          if (selEndNode.get() == aExistingRightNode)
          {
            if (selEndOffset < aOffset)
            {
              selEndNode = aNewLeftNode;
            }
            else
            {
              selEndOffset -= aOffset;
            }
          }
          selection->Collapse(selStartNode,selStartOffset);
          selection->Extend(selEndNode,selEndOffset);
        }
      }
    }
  }
  else
    result = NS_ERROR_INVALID_ARG;

  return result;
}

nsresult
nsEditor::JoinNodesImpl(nsIDOMNode * aNodeToKeep,
                        nsIDOMNode * aNodeToJoin,
                        nsIDOMNode * aParent,
                        PRBool       aNodeToKeepIsFirst)
{
  nsresult result = NS_OK;
  NS_ASSERTION(aNodeToKeep && aNodeToJoin && aParent, "null arg");
  if (aNodeToKeep && aNodeToJoin && aParent)
  {
    // get selection
    nsCOMPtr<nsIDOMSelection> selection;
    GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) return result;
    if (!selection) return NS_ERROR_NULL_POINTER;

    // remember some selection points
    nsCOMPtr<nsIDOMNode> selStartNode, selEndNode, parent;
    PRInt32 selStartOffset, selEndOffset, joinOffset, keepOffset;
    result = GetStartNodeAndOffset(selection, &selStartNode, &selStartOffset);
    if (NS_FAILED(result)) selStartNode = nsnull;
    result = GetEndNodeAndOffset(selection, &selEndNode, &selEndOffset);
    if (NS_FAILED(result)) selStartNode = nsnull;
    
    
    PRUint32 firstNodeLength;
    nsCOMPtr<nsIDOMNode> leftNode = aNodeToJoin;
    if (aNodeToKeepIsFirst) leftNode = aNodeToKeep;
    result = GetLengthOfDOMNode(leftNode, firstNodeLength);
    if (NS_FAILED(result)) return result;
    result = GetNodeLocation(aNodeToJoin, &parent, &joinOffset);
    if (NS_FAILED(result)) return result;
    result = GetNodeLocation(aNodeToKeep, &parent, &keepOffset);
    if (NS_FAILED(result)) return result;
    
    // if selection endpoint is between the nodes, remember it as being
    // in the one that is going away instead.  This simplifies later selection
    // adjustment logic at end of this method.
    if (selStartNode)
    {
      if (selStartNode == parent)
      {
        if (aNodeToKeepIsFirst)
        {
          if ((selStartOffset > keepOffset) && (selStartOffset <= joinOffset))
          {
            selStartNode = aNodeToJoin; 
            selStartOffset = 0;
          }
        }
        else
        {
          if ((selStartOffset > joinOffset) && (selStartOffset <= keepOffset))
          {
            selStartNode = aNodeToJoin; 
            selStartOffset = firstNodeLength;
          }
        }
      }
      if (selEndNode == parent)
      {
        if (aNodeToKeepIsFirst)
        {
          if ((selEndOffset > keepOffset) && (selEndOffset <= joinOffset))
          {
            selEndNode = aNodeToJoin; 
            selEndOffset = 0;
          }
        }
        else
        {
          if ((selEndOffset > joinOffset) && (selEndOffset <= keepOffset))
          {
            selEndNode = aNodeToJoin; 
            selEndOffset = firstNodeLength;
          }
        }
      }
    }
    // ok, ready to do join now.
    // if it's a text node, just shuffle around some text
    nsCOMPtr<nsIDOMCharacterData> keepNodeAsText( do_QueryInterface(aNodeToKeep) );
    nsCOMPtr<nsIDOMCharacterData> joinNodeAsText( do_QueryInterface(aNodeToJoin) );
    if (keepNodeAsText && joinNodeAsText)
    {
      nsAutoString rightText;
      nsAutoString leftText;
      if (aNodeToKeepIsFirst)
      {
        keepNodeAsText->GetData(leftText);
        joinNodeAsText->GetData(rightText);
      }
      else
      {
        keepNodeAsText->GetData(rightText);
        joinNodeAsText->GetData(leftText);
      }
      leftText += rightText;
      keepNodeAsText->SetData(leftText);          
    }
    else
    {  // otherwise it's an interior node, so shuffle around the children
      nsCOMPtr<nsIDOMNodeList> childNodes;
      result = aNodeToJoin->GetChildNodes(getter_AddRefs(childNodes));
      if ((NS_SUCCEEDED(result)) && (childNodes))
      {
        PRInt32 i;  // must be signed int!
        PRUint32 childCount=0;
        nsCOMPtr<nsIDOMNode> firstNode; //only used if aNodeToKeepIsFirst is false
        childNodes->GetLength(&childCount);
        if (!aNodeToKeepIsFirst)
        { // remember the first child in aNodeToKeep, we'll insert all the children of aNodeToJoin in front of it
          result = aNodeToKeep->GetFirstChild(getter_AddRefs(firstNode));  
          // GetFirstChild returns nsnull firstNode if aNodeToKeep has no children, that's ok.
        }
        nsCOMPtr<nsIDOMNode> resultNode;
        // have to go through the list backwards to keep deletes from interfering with iteration
        nsCOMPtr<nsIDOMNode> previousChild;
        for (i=childCount-1; ((NS_SUCCEEDED(result)) && (0<=i)); i--)
        {
          nsCOMPtr<nsIDOMNode> childNode;
          result = childNodes->Item(i, getter_AddRefs(childNode));
          if ((NS_SUCCEEDED(result)) && (childNode))
          {
            if (aNodeToKeepIsFirst)
            { // append children of aNodeToJoin
              //was result = aNodeToKeep->AppendChild(childNode, getter_AddRefs(resultNode));
              result = aNodeToKeep->InsertBefore(childNode, previousChild, getter_AddRefs(resultNode));
              previousChild = do_QueryInterface(childNode);
            }
            else
            { // prepend children of aNodeToJoin
              //was result = aNodeToKeep->InsertBefore(childNode, firstNode, getter_AddRefs(resultNode));
              result = aNodeToKeep->InsertBefore(childNode, firstNode, getter_AddRefs(resultNode));
              firstNode = do_QueryInterface(childNode);
            }
          }
        }
      }
      else if (!childNodes) {
        result = NS_ERROR_NULL_POINTER;
      }
    }
    if (NS_SUCCEEDED(result))
    { // delete the extra node
      nsCOMPtr<nsIDOMNode> resultNode;
      result = aParent->RemoveChild(aNodeToJoin, getter_AddRefs(resultNode));
      
      if (GetShouldTxnSetSelection())
      {
        // editor wants us to set selection at join point
        selection->Collapse(aNodeToKeep, firstNodeLength);
      }
      else if (selStartNode)
      {
        // and adjust the selection if needed
        // HACK: this is overly simplified - multi-range selections need more work than this
        PRBool bNeedToAdjust = PR_FALSE;
        if (selStartNode.get() == aNodeToJoin)
        {
          bNeedToAdjust = PR_TRUE;
          selStartNode = aNodeToKeep;
          if (aNodeToKeepIsFirst)
          {
            selStartOffset += firstNodeLength;
          }
        }
        else if ((selStartNode.get() == aNodeToKeep) && !aNodeToKeepIsFirst)
        {
          bNeedToAdjust = PR_TRUE;
          selStartOffset += firstNodeLength;
        }
        
        if (bNeedToAdjust)
          selection->Collapse(selStartNode,selStartOffset);

        bNeedToAdjust = PR_FALSE;
        
        if (selEndNode.get() == aNodeToJoin)
        {
          bNeedToAdjust = PR_TRUE;
          selEndNode = aNodeToKeep;
          if (aNodeToKeepIsFirst)
          {
            selEndOffset += firstNodeLength;
          }
        }
        else if ((selEndNode.get() == aNodeToKeep) && !aNodeToKeepIsFirst)
        {
          bNeedToAdjust = PR_TRUE;
          selEndOffset += firstNodeLength;
        }
        
        if (bNeedToAdjust)
          selection->Extend(selEndNode,selEndOffset);
      }
    }
  }
  else
    result = NS_ERROR_INVALID_ARG;

  return result;
}


nsresult 
nsEditor::GetChildOffset(nsIDOMNode *aChild, nsIDOMNode *aParent, PRInt32 &aOffset)
{
  NS_ASSERTION((aChild && aParent), "bad args");
  if (!aChild || !aParent) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aParent);
  nsCOMPtr<nsIContent> cChild = do_QueryInterface(aChild);
  if (!cChild || !content) return NS_ERROR_NULL_POINTER;
  nsresult res = content->IndexOf(cChild, aOffset);
  return res;
}

nsresult 
nsEditor::GetNodeLocation(nsIDOMNode *inChild, nsCOMPtr<nsIDOMNode> *outParent, PRInt32 *outOffset)
{
  NS_ASSERTION((inChild && outParent && outOffset), "bad args");
  nsresult result = NS_ERROR_NULL_POINTER;
  if (inChild && outParent && outOffset)
  {
    result = inChild->GetParentNode(getter_AddRefs(*outParent));
    if ((NS_SUCCEEDED(result)) && (*outParent))
    {
      result = GetChildOffset(inChild, *outParent, *outOffset);
    }
  }
  return result;
}

// returns the number of things inside aNode.  
// If aNode is text, returns number of characters. If not, returns number of children nodes.
nsresult
nsEditor::GetLengthOfDOMNode(nsIDOMNode *aNode, PRUint32 &aCount) 
{
  aCount = 0;
  if (!aNode) { return NS_ERROR_NULL_POINTER; }
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar = do_QueryInterface(aNode);
  if (nodeAsChar) {
    nodeAsChar->GetLength(&aCount);
  }
  else
  {
    PRBool hasChildNodes;
    aNode->HasChildNodes(&hasChildNodes);
    if (PR_TRUE==hasChildNodes)
    {
      nsCOMPtr<nsIDOMNodeList>nodeList;
      result = aNode->GetChildNodes(getter_AddRefs(nodeList));
      if (NS_SUCCEEDED(result) && nodeList) {
        nodeList->GetLength(&aCount);
      }
    }
  }
  return result;
}

// Non-static version for the nsIEditor interface and JavaScript
NS_IMETHODIMP 
nsEditor::NodeIsBlock(nsIDOMNode *aNode, PRBool &aIsBlock)
{
  if (!aNode) { return NS_ERROR_NULL_POINTER; }
  return IsNodeBlock(aNode, aIsBlock);
}

// The list of block nodes is shorter, so do the real work here...
nsresult
nsEditor::IsNodeBlock(nsIDOMNode *aNode, PRBool &aIsBlock)
{
  // this is a content-based implementation
  if (!aNode) { return NS_ERROR_NULL_POINTER; }

  nsresult result = NS_ERROR_FAILURE;
  aIsBlock = PR_FALSE;
  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString tagName;
    result = element->GetTagName(tagName);
    if (NS_SUCCEEDED(result))
    {
      tagName.ToLowerCase();
      nsIAtom *tagAtom = NS_NewAtom(tagName);
      if (!tagAtom) { return NS_ERROR_NULL_POINTER; }

      if (tagAtom==nsIEditProperty::p          ||
          tagAtom==nsIEditProperty::div        ||
          tagAtom==nsIEditProperty::blockquote ||
          tagAtom==nsIEditProperty::h1         ||
          tagAtom==nsIEditProperty::h2         ||
          tagAtom==nsIEditProperty::h3         ||
          tagAtom==nsIEditProperty::h4         ||
          tagAtom==nsIEditProperty::h5         ||
          tagAtom==nsIEditProperty::h6         ||
          tagAtom==nsIEditProperty::ul         ||
          tagAtom==nsIEditProperty::ol         ||
          tagAtom==nsIEditProperty::dl         ||
          tagAtom==nsIEditProperty::pre        ||
          tagAtom==nsIEditProperty::noscript   ||
          tagAtom==nsIEditProperty::form       ||
          tagAtom==nsIEditProperty::hr         ||
          tagAtom==nsIEditProperty::table      ||
          tagAtom==nsIEditProperty::fieldset   ||
          tagAtom==nsIEditProperty::address    ||
          tagAtom==nsIEditProperty::body       ||
          tagAtom==nsIEditProperty::tr         ||
          tagAtom==nsIEditProperty::td         ||
          tagAtom==nsIEditProperty::th         ||
          tagAtom==nsIEditProperty::caption    ||
          tagAtom==nsIEditProperty::col        ||
          tagAtom==nsIEditProperty::colgroup   ||
          tagAtom==nsIEditProperty::tbody      ||
          tagAtom==nsIEditProperty::thead      ||
          tagAtom==nsIEditProperty::tfoot      ||
          tagAtom==nsIEditProperty::li         ||
          tagAtom==nsIEditProperty::dt         ||
          tagAtom==nsIEditProperty::dd         ||
          tagAtom==nsIEditProperty::legend     )
      {
        aIsBlock = PR_TRUE;
      }
      NS_RELEASE(tagAtom);
      result = NS_OK;
    }
  } else {
    // We don't have an element -- probably a text node
    nsCOMPtr<nsIDOMCharacterData>nodeAsText = do_QueryInterface(aNode);
    if (nodeAsText)
    {
      aIsBlock = PR_FALSE;
      result = NS_OK;
    }
  }
  return result;
}

// ...and simply assume non-block element is inline
nsresult
nsEditor::IsNodeInline(nsIDOMNode *aNode, PRBool &aIsInline)
{
  // this is a content-based implementation
  if (!aNode) { return NS_ERROR_NULL_POINTER; }

  nsresult result;
  aIsInline = PR_FALSE;
  PRBool IsBlock = PR_FALSE;
  result = IsNodeBlock(aNode, IsBlock);
  aIsInline = !IsBlock;
  return result;
}


nsresult 
nsEditor::GetPriorNode(nsIDOMNode  *aParentNode, 
                       PRInt32      aOffset, 
                       PRBool       aEditableNode, 
                       nsIDOMNode **aResultNode)
{
  // just another version of GetPriorNode that takes a {parent, offset}
  // instead of a node
  nsresult result = NS_OK;
  if (!aParentNode || !aResultNode) { return NS_ERROR_NULL_POINTER; }
  
  // if we are at beginning of node, or it is a textnode, then just look before it
  if (!aOffset || IsTextNode(aParentNode))
  {
    return GetPriorNode(aParentNode, aEditableNode, aResultNode);
  }
  else
  {
    // else look before the child at 'aOffset'
    nsCOMPtr<nsIDOMNode> child = GetChildAt(aParentNode, aOffset);
    if (child)
      return GetPriorNode(child, aEditableNode, aResultNode);
    // unless there isn't one, in which case we are at the end of the node
    // and want the deep-right child.
    else
    {
      result = GetRightmostChild(aParentNode, aResultNode);
      if (NS_FAILED(result)) return result;
      if (!aEditableNode) return result;
      if (IsEditable(*aResultNode))  return result;
      else 
      { 
        // restart the search from the non-editable node we just found
        nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
        NS_IF_RELEASE(*aResultNode);
        return GetPriorNode(notEditableNode, aEditableNode, aResultNode);
      }
    }
  }
}



nsresult 
nsEditor::GetNextNode(nsIDOMNode   *aParentNode, 
                       PRInt32      aOffset, 
                       PRBool       aEditableNode, 
                       nsIDOMNode **aResultNode)
{
  // just another version of GetNextNode that takes a {parent, offset}
  // instead of a node
  nsresult result = NS_OK;
  if (!aParentNode || !aResultNode) { return NS_ERROR_NULL_POINTER; }
  
  *aResultNode = nsnull;
  
  // if aParentNode is a text node, use it's location instead
  if (IsTextNode(aParentNode))
  {
    nsCOMPtr<nsIDOMNode> parent;
    nsEditor::GetNodeLocation(aParentNode, &parent, &aOffset);
    aParentNode = parent;
    aOffset++;  // _after_ the text node
  }
  // look at the child at 'aOffset'
  nsCOMPtr<nsIDOMNode> child = GetChildAt(aParentNode, aOffset);
  if (child)
  {
    result = GetLeftmostChild(child, aResultNode);
    if (NS_FAILED(result)) return result;
    if (!IsDescendantOfBody(*aResultNode))
    {
      NS_RELEASE(*aResultNode); // assigns nsnull
      return result;
    }
    if (!aEditableNode) return result;
    if (IsEditable(*aResultNode)) return result;
    else 
    { 
      // restart the search from the non-editable node we just found
      nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
      NS_IF_RELEASE(*aResultNode);
      return GetNextNode(notEditableNode, aEditableNode, aResultNode);
    }
  }
    
  // unless there isn't one, in which case we are at the end of the node
  // and want the next one.
  else
  {
    return GetNextNode(aParentNode, aEditableNode, aResultNode);
  }
}



nsresult 
nsEditor::GetPriorNode(nsIDOMNode  *aCurrentNode, 
                       PRBool       aEditableNode, 
                       nsIDOMNode **aResultNode)
{
  nsresult result;
  if (!aCurrentNode || !aResultNode) { return NS_ERROR_NULL_POINTER; }
  
  *aResultNode = nsnull;  // init out-param

  // if aCurrentNode has a left sibling, return that sibling's rightmost child (or itself if it has no children)
  nsCOMPtr<nsIDOMNode> prevSibling;
  result = aCurrentNode->GetPreviousSibling(getter_AddRefs(prevSibling));
  if ((NS_SUCCEEDED(result)) && prevSibling)
  {
    result = GetRightmostChild(prevSibling, aResultNode);
    if (NS_FAILED(result)) { return result; }
    if (!IsDescendantOfBody(*aResultNode))
    {
      NS_RELEASE(*aResultNode); // assigns nsnull
      return result;
    }
    if (PR_FALSE==aEditableNode) {
      return result;
    }
    if (PR_TRUE==IsEditable(*aResultNode)) {
      return result;
    }
    else 
    { // restart the search from the non-editable node we just found
      nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
      NS_IF_RELEASE(*aResultNode);
      return GetPriorNode(notEditableNode, aEditableNode, aResultNode);
    }
  }
  
  // otherwise, walk up the parent change until there is a child that comes before 
  // the ancestor of aCurrentNode.  Then return that node's rightmost child
  nsCOMPtr<nsIDOMNode> parent = do_QueryInterface(aCurrentNode);
  do {
    nsCOMPtr<nsIDOMNode> node(parent);
    result = node->GetParentNode(getter_AddRefs(parent));
    if ((NS_SUCCEEDED(result)) && parent)
    {
      result = parent->GetPreviousSibling(getter_AddRefs(node));
      if ((NS_SUCCEEDED(result)) && node)
      {
        result = GetRightmostChild(node, aResultNode);
        if (NS_FAILED(result)) { return result; }
        if (!IsDescendantOfBody(*aResultNode))
        {
          NS_RELEASE(*aResultNode); // assigns nsnull
          return result;
        }
        if (PR_FALSE==aEditableNode) {
          return result;
        }
        if (PR_TRUE==IsEditable(*aResultNode)) {
          return result;
        }
        else 
        { // restart the search from the non-editable node we just found
          nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
          NS_IF_RELEASE(*aResultNode);
          return GetPriorNode(notEditableNode, aEditableNode, aResultNode);
        }
      }
    }
  } while ((NS_SUCCEEDED(result)) && parent);

  return result;
}

nsresult 
nsEditor::GetNextNode(nsIDOMNode  *aCurrentNode, 
                      PRBool       aEditableNode, 
                      nsIDOMNode **aResultNode)
{
  nsresult result;
  *aResultNode = nsnull;
  // if aCurrentNode has a right sibling, return that sibling's leftmost child (or itself if it has no children)
  nsCOMPtr<nsIDOMNode> nextSibling;
  result = aCurrentNode->GetNextSibling(getter_AddRefs(nextSibling));
  if ((NS_SUCCEEDED(result)) && nextSibling)
  {
    result = GetLeftmostChild(nextSibling, aResultNode);
    if (NS_FAILED(result)) { return result; }
    if (!IsDescendantOfBody(*aResultNode))
    {
      NS_RELEASE(*aResultNode); // assigns nsnull
      return result;
    }
    if (PR_FALSE==aEditableNode) {
      return result;
    }
    if (PR_TRUE==IsEditable(*aResultNode)) {
      return result;
    }
    else 
    { // restart the search from the non-editable node we just found
      nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
      NS_IF_RELEASE(*aResultNode);
      return GetNextNode(notEditableNode, aEditableNode, aResultNode);
    }
  }
  
  // otherwise, walk up the parent change until there is a child that comes after 
  // the ancestor of aCurrentNode.  Then return that node's leftmost child

  nsCOMPtr<nsIDOMNode> parent(do_QueryInterface(aCurrentNode));
  do {
    nsCOMPtr<nsIDOMNode> node(parent);
    result = node->GetParentNode(getter_AddRefs(parent));
    if ((NS_SUCCEEDED(result)) && parent)
    {
      result = parent->GetNextSibling(getter_AddRefs(node));
      if ((NS_SUCCEEDED(result)) && node)
      {
        result = GetLeftmostChild(node, aResultNode);
        if (NS_FAILED(result)) { return result; }
        if (!IsDescendantOfBody(*aResultNode))
        {
          NS_RELEASE(*aResultNode); // assigns nsnull
          return result;
        }
        if (PR_FALSE==aEditableNode) {
          return result;
        }
        if (PR_TRUE==IsEditable(*aResultNode)) {
          return result;
        }
        else 
        { // restart the search from the non-editable node we just found
          nsCOMPtr<nsIDOMNode> notEditableNode = do_QueryInterface(*aResultNode);
          NS_IF_RELEASE(*aResultNode);
          return GetNextNode(notEditableNode, aEditableNode, aResultNode);
        }
      }
    }
  } while ((NS_SUCCEEDED(result)) && parent);

  return result;
}

nsresult
nsEditor::GetRightmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> resultNode(do_QueryInterface(aCurrentNode));
  PRBool hasChildren;
  resultNode->HasChildNodes(&hasChildren);
  while ((NS_SUCCEEDED(result)) && (PR_TRUE==hasChildren))
  {
    nsCOMPtr<nsIDOMNode> temp(resultNode);
    temp->GetLastChild(getter_AddRefs(resultNode));
    resultNode->HasChildNodes(&hasChildren);
  }

  if (NS_SUCCEEDED(result)) {
    *aResultNode = resultNode;
    NS_ADDREF(*aResultNode);
  }
  return result;
}

nsresult
nsEditor::GetLeftmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> resultNode(do_QueryInterface(aCurrentNode));
  PRBool hasChildren;
  resultNode->HasChildNodes(&hasChildren);
  while ((NS_SUCCEEDED(result)) && (PR_TRUE==hasChildren))
  {
    nsCOMPtr<nsIDOMNode> temp(resultNode);
    temp->GetFirstChild(getter_AddRefs(resultNode));
    resultNode->HasChildNodes(&hasChildren);
  }

  if (NS_SUCCEEDED(result)) {
    *aResultNode = resultNode;
    NS_ADDREF(*aResultNode);
  }
  return result;
}

PRBool 
nsEditor::NodeIsType(nsIDOMNode *aNode, nsIAtom *aTag)
{
  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString tag;
    element->GetTagName(tag);
    const PRUnichar *unicodeString;
    aTag->GetUnicode(&unicodeString);
    if (tag.EqualsIgnoreCase(nsAutoString(unicodeString)))
    {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool 
nsEditor::NodeIsType(nsIDOMNode *aNode, const nsString &aTagStr)
{
  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString tag;
    element->GetTagName(tag);
    if (tag.EqualsIgnoreCase(aTagStr))
    {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool 
nsEditor::CanContainTag(nsIDOMNode* aParent, const nsString &aChildTag)
{
  nsAutoString parentStringTag;
  
  nsCOMPtr<nsIDOMElement> parentElement = do_QueryInterface(aParent);
  if (!parentElement) return PR_FALSE;
  
  parentElement->GetTagName(parentStringTag);
  return TagCanContainTag(parentStringTag, aChildTag);
}

PRBool 
nsEditor::TagCanContain(const nsString &aParentTag, nsIDOMNode* aChild)
{
  nsAutoString childStringTag;
  
  if (IsTextNode(aChild)) 
  {
    childStringTag.AssignWithConversion("__moz_text");
  }
  else
  {
    nsCOMPtr<nsIDOMElement> childElement = do_QueryInterface(aChild);
    if (!childElement) return PR_FALSE;
    childElement->GetTagName(childStringTag);
  }
  return TagCanContainTag(aParentTag, childStringTag);
}

PRBool 
nsEditor::TagCanContainTag(const nsString &aParentTag, const nsString &aChildTag)
{
  // if we don't have a dtd then assume we can insert whatever want
  if (!mDTD) return PR_TRUE;
  
  PRInt32 childTagEnum, parentTagEnum;
  nsAutoString non_const_childTag(aChildTag);
  nsAutoString non_const_parentTag(aParentTag);
  nsresult res = mDTD->StringTagToIntTag(non_const_childTag,&childTagEnum);
  if (NS_FAILED(res)) return PR_FALSE;
  res = mDTD->StringTagToIntTag(non_const_parentTag,&parentTagEnum);
  if (NS_FAILED(res)) return PR_FALSE;

  return mDTD->CanContain(parentTagEnum, childTagEnum);
}

PRBool 
nsEditor::IsDescendantOfBody(nsIDOMNode *inNode) 
{
  if (!inNode) return PR_FALSE;
  nsCOMPtr<nsIDOMElement> junk;
  if (!mBodyElement) GetRootElement(getter_AddRefs(junk));
  if (!mBodyElement) return PR_FALSE;
  nsCOMPtr<nsIDOMNode> root = do_QueryInterface(mBodyElement);
  
  if (inNode == root.get()) return PR_TRUE;
  
  nsCOMPtr<nsIDOMNode> parent, node = do_QueryInterface(inNode);
  
  do
  {
    node->GetParentNode(getter_AddRefs(parent));
    if (parent == root) return PR_TRUE;
    node = parent;
  } while (parent);
  
  return PR_FALSE;
}

PRBool 
nsEditor::IsContainer(nsIDOMNode *aNode)
{
  if (!aNode) return PR_FALSE;
  nsAutoString stringTag;
  PRInt32 tagEnum;
  nsresult res = aNode->GetNodeName(stringTag);
  if (NS_FAILED(res)) return PR_FALSE;
  res = mDTD->StringTagToIntTag(stringTag,&tagEnum);
  if (NS_FAILED(res)) return PR_FALSE;
  return mDTD->IsContainer(tagEnum);
}

PRBool 
nsEditor::IsEditable(nsIDOMNode *aNode)
{
  if (!aNode) return PR_FALSE;
  nsCOMPtr<nsIPresShell> shell;
  GetPresShell(getter_AddRefs(shell));
  if (!shell)  return PR_FALSE;

  if (IsMozEditorBogusNode(aNode)) return PR_FALSE;

  // it's not the bogus node, so see if it is an irrelevant text node
  if (PR_TRUE==IsTextNode(aNode))
  {
    nsCOMPtr<nsIDOMCharacterData> text = do_QueryInterface(aNode);    
    // nsCOMPtr<nsIDOMComment> commentNode = do_QueryInterface(aNode);
    if (text)
    {
      nsAutoString data;
      text->GetData(data);
      PRUint32 length = data.Length();
      if (0==length) {
        return PR_FALSE;
      }
      // if the node contains only newlines, it's not editable
      // you could use nsITextContent::IsOnlyWhitespace here
      PRUint32 i;
      for (i=0; i<length; i++)
      {
        PRUnichar character = data.CharAt(i);
        if ('\n'!=character) {
          return PR_TRUE;
        }
      }
      return PR_FALSE;
    }
  }
  
  // we got this far, so see if it has a frame.  If so, we'll edit it.
  nsIFrame *resultFrame;
  nsCOMPtr<nsIContent>content;
  content = do_QueryInterface(aNode);
  if (content)
  {
    nsresult result = shell->GetPrimaryFrameFor(content, &resultFrame);
    if (NS_FAILED(result) || !resultFrame) {  // if it has no frame, it is not editable
      return PR_FALSE;
    }
    else {                                    
      // it has a frame, so it might editable
      // but not if it's a formatting whitespace node
      if (IsEmptyTextContent(content)) return PR_FALSE;          
      return PR_TRUE;
    }
  }
  return PR_FALSE;  // it's not a content object (???) so it's not editable
}

PRBool
nsEditor::IsMozEditorBogusNode(nsIDOMNode *aNode)
{
  if (!aNode)
    return PR_FALSE;

  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString att; att.AssignWithConversion(kMOZEditorBogusNodeAttr);
    nsAutoString val;
    (void)element->GetAttribute(att, val);
    if (val.EqualsWithConversion(kMOZEditorBogusNodeValue)) {
      return PR_TRUE;
    }
  }
    
  return PR_FALSE;
}

PRBool
nsEditor::IsEmptyTextContent(nsIContent* aContent)
{
  PRBool result = PR_FALSE;
  nsCOMPtr<nsITextContent> tc(do_QueryInterface(aContent));
  if (tc) {
    tc->IsOnlyWhitespace(&result);
  }
  return result;
}

nsresult
nsEditor::CountEditableChildren(nsIDOMNode *aNode, PRUint32 &outCount) 
{
  outCount = 0;
  if (!aNode) { return NS_ERROR_NULL_POINTER; }
  nsresult res=NS_OK;
  PRBool hasChildNodes;
  aNode->HasChildNodes(&hasChildNodes);
  if (PR_TRUE==hasChildNodes)
  {
    nsCOMPtr<nsIDOMNodeList>nodeList;
    PRUint32 len;
    PRUint32 i;
    res = aNode->GetChildNodes(getter_AddRefs(nodeList));
    if (NS_SUCCEEDED(res) && nodeList) 
    {
      nodeList->GetLength(&len);
      for (i=0 ; i<len; i++)
      {
        nsCOMPtr<nsIDOMNode> child;
        res = nodeList->Item((PRInt32)i, getter_AddRefs(child));
        if ((NS_SUCCEEDED(res)) && (child))
        {
          if (IsEditable(child))
          {
            outCount++;
          }
        }
      }
    }
    else if (!nodeList)
      res = NS_ERROR_NULL_POINTER;
  }
  return res;
}

//END nsEditor static utility methods


NS_IMETHODIMP nsEditor::IncDocModCount(PRInt32 inNumMods)
{
  if (!mDocWeak) return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDiskDocument>  diskDoc = do_QueryInterface(doc);
  if (diskDoc)
    diskDoc->IncrementModCount(inNumMods);

  NotifyDocumentListeners(eDocumentStateChanged);
  return NS_OK;
}


NS_IMETHODIMP nsEditor::GetDocModCount(PRInt32 &outModCount)
{
  if (!mDocWeak) return NS_ERROR_NOT_INITIALIZED;

  outModCount = 0;
  
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDiskDocument>  diskDoc = do_QueryInterface(doc);
  if (diskDoc)
    diskDoc->GetModCount(&outModCount);

  return NS_OK;
}


NS_IMETHODIMP nsEditor::ResetDocModCount()
{
  if (!mDocWeak) return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDiskDocument>  diskDoc = do_QueryInterface(doc);
  if (diskDoc)
    diskDoc->ResetModCount();

  NotifyDocumentListeners(eDocumentStateChanged);
  return NS_OK;
}


void nsEditor::HACKForceRedraw()
{
#ifdef HACK_FORCE_REDRAW
// XXXX: Horrible hack! We are doing this because
// of an error in Gecko which is not rendering the
// document after a change via the DOM - gpk 2/11/99
  // BEGIN HACK!!!
  nsCOMPtr<nsIPresShell> shell;
  
   GetPresShell(getter_AddRefs(shell));
  if (shell) {
    nsCOMPtr<nsIViewManager> viewmgr;

    shell->GetViewManager(getter_AddRefs(viewmgr));
    if (viewmgr) {
      nsIView* view;
      viewmgr->GetRootView(view);      // views are not refCounted
      if (view) {
        viewmgr->UpdateView(view,NS_VMREFRESH_IMMEDIATE);
      }
    }
  }
  // END HACK
#endif
}

nsresult
nsEditor::GetFirstNodeOfType(nsIDOMNode     *aStartNode, 
                             const nsString &aTag, 
                             nsIDOMNode    **aResult)
{
  nsresult result=NS_OK;

  if (!aStartNode)
    return NS_ERROR_NULL_POINTER;
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMElement> element;
  *aResult = nsnull;
  nsCOMPtr<nsIDOMNode> childNode;
  result = aStartNode->GetFirstChild(getter_AddRefs(childNode));
  while (childNode)
  {
    result = childNode->QueryInterface(NS_GET_IID(nsIDOMNode),getter_AddRefs(element));
    nsAutoString tag;
    if (NS_SUCCEEDED(result) && (element))
    {    
      element->GetTagName(tag);
      if (PR_TRUE==aTag.EqualsIgnoreCase(tag))
      {
        return (childNode->QueryInterface(NS_GET_IID(nsIDOMNode),(void **) aResult)); // does the addref
      }
      else
      {
        result = GetFirstNodeOfType(childNode, aTag, aResult);
        if (nsnull!=*aResult)
          return result;
      }
    }
    nsCOMPtr<nsIDOMNode> temp = childNode;
    temp->GetNextSibling(getter_AddRefs(childNode));
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsEditor::GetFirstTextNode(nsIDOMNode *aNode, nsIDOMNode **aRetNode)
{
  if (!aNode || !aRetNode)
  {
    NS_NOTREACHED("GetFirstTextNode Failed");
    return NS_ERROR_NULL_POINTER;
  }

  PRUint16 mType;
  PRBool mCNodes;

  nsCOMPtr<nsIDOMNode> answer;
  
  aNode->GetNodeType(&mType);

  if (nsIDOMNode::ELEMENT_NODE == mType) {
    if (NS_SUCCEEDED(aNode->HasChildNodes(&mCNodes)) && PR_TRUE == mCNodes) 
    {
      nsCOMPtr<nsIDOMNode> node1;
      nsCOMPtr<nsIDOMNode> node2;

      if (!NS_SUCCEEDED(aNode->GetFirstChild(getter_AddRefs(node1))))
      {
        NS_NOTREACHED("GetFirstTextNode Failed");
      }
      while(!answer && node1) 
      {
        GetFirstTextNode(node1, getter_AddRefs(answer));
        node1->GetNextSibling(getter_AddRefs(node2));
        node1 = node2;
      }
    }
  }
  else if (nsIDOMNode::TEXT_NODE == mType) {
    answer = do_QueryInterface(aNode);
  }

    // OK, now return the answer, if any
  *aRetNode = answer;
  if (*aRetNode)
    NS_IF_ADDREF(*aRetNode);
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}


//END nsEditor Private methods



///////////////////////////////////////////////////////////////////////////
// GetTag: digs out the atom for the tag of this node
//                    
nsCOMPtr<nsIAtom> 
nsEditor::GetTag(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIAtom> atom;
  
  if (!aNode) 
  {
    NS_NOTREACHED("null node passed to nsEditor::GetTag()");
    return atom;
  }
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (content)
    content->GetTag(*getter_AddRefs(atom));

  return atom;
}


///////////////////////////////////////////////////////////////////////////
// GetTagString: digs out string for the tag of this node
//                    
nsresult 
nsEditor::GetTagString(nsIDOMNode *aNode, nsString& outString)
{
  nsCOMPtr<nsIAtom> atom;
  
  if (!aNode) 
  {
    NS_NOTREACHED("null node passed to nsEditor::GetTag()");
    return NS_ERROR_NULL_POINTER;
  }
  
  atom = GetTag(aNode);
  if (atom)
  {
    atom->ToString(outString);
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}


///////////////////////////////////////////////////////////////////////////
// NodesSameType: do these nodes have the same tag?
//                    
PRBool 
nsEditor::NodesSameType(nsIDOMNode *aNode1, nsIDOMNode *aNode2)
{
  if (!aNode1 || !aNode2) 
  {
    NS_NOTREACHED("null node passed to nsEditor::NodesSameType()");
    return PR_FALSE;
  }
  
  nsCOMPtr<nsIAtom> atom1 = GetTag(aNode1);
  nsCOMPtr<nsIAtom> atom2 = GetTag(aNode2);
  
  if (atom1.get() == atom2.get())
    return PR_TRUE;

  return PR_FALSE;
}



///////////////////////////////////////////////////////////////////////////
// IsBlockNode: true if this node is an html block node
//                    
PRBool
nsEditor::IsBlockNode(nsIDOMNode *aNode)
{
  return !IsInlineNode(aNode);
}


///////////////////////////////////////////////////////////////////////////
// IsInlineNode: true if this node is an html inline node
//                    
PRBool
nsEditor::IsInlineNode(nsIDOMNode *aNode)
{
  PRBool retVal = PR_FALSE;
  IsNodeInline(aNode, retVal);
  return retVal;
}


///////////////////////////////////////////////////////////////////////////
// GetBlockNodeParent: returns enclosing block level ancestor, if any
//
nsCOMPtr<nsIDOMNode>
nsEditor::GetBlockNodeParent(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> tmp;
  nsCOMPtr<nsIDOMNode> p;

  if (!aNode)
  {
    NS_NOTREACHED("null node passed to GetBlockNodeParent()");
    return PR_FALSE;
  }

  if (NS_FAILED(aNode->GetParentNode(getter_AddRefs(p))))  // no parent, ran off top of tree
    return tmp;

  while (p && !IsBlockNode(p))
  {
    if ( NS_FAILED(p->GetParentNode(getter_AddRefs(tmp))) || !tmp) // no parent, ran off top of tree
      return p;

    p = tmp;
  }
  return p;
}


///////////////////////////////////////////////////////////////////////////
// HasSameBlockNodeParent: true if nodes have same block level ancestor
//               
PRBool
nsEditor::HasSameBlockNodeParent(nsIDOMNode *aNode1, nsIDOMNode *aNode2)
{
  if (!aNode1 || !aNode2)
  {
    NS_NOTREACHED("null node passed to HasSameBlockNodeParent()");
    return PR_FALSE;
  }
  
  if (aNode1 == aNode2)
    return PR_TRUE;
    
  nsCOMPtr<nsIDOMNode> p1 = GetBlockNodeParent(aNode1);
  nsCOMPtr<nsIDOMNode> p2 = GetBlockNodeParent(aNode2);

  return (p1 == p2);
}


///////////////////////////////////////////////////////////////////////////
// GetBlockSection: return leftmost/rightmost nodes in aChild's block
//               
nsresult
nsEditor::GetBlockSection(nsIDOMNode *aChild,
                          nsIDOMNode **aLeftNode, 
                          nsIDOMNode **aRightNode) 
{
  nsresult result = NS_OK;
  if (!aChild || !aLeftNode || !aRightNode) {return NS_ERROR_NULL_POINTER;}
  *aLeftNode = aChild;
  *aRightNode = aChild;

  nsCOMPtr<nsIDOMNode>sibling;
  result = aChild->GetPreviousSibling(getter_AddRefs(sibling));
  while ((NS_SUCCEEDED(result)) && sibling)
  {
    PRBool isInline;
    IsNodeInline(sibling, isInline);
    if (PR_FALSE==isInline) 
    {
      nsCOMPtr<nsIDOMCharacterData>nodeAsText = do_QueryInterface(sibling);
      if (!nodeAsText) {
        break;
      }
      // XXX: needs some logic to work for other leaf nodes besides text!
    }
    *aLeftNode = sibling;
    result = (*aLeftNode)->GetPreviousSibling(getter_AddRefs(sibling)); 
  }
  NS_ADDREF((*aLeftNode));
  // now do the right side
  result = aChild->GetNextSibling(getter_AddRefs(sibling));
  while ((NS_SUCCEEDED(result)) && sibling)
  {
    PRBool isInline;
    IsNodeInline(sibling, isInline);
    if (PR_FALSE==isInline) 
    {
      nsCOMPtr<nsIDOMCharacterData>nodeAsText = do_QueryInterface(sibling);
      if (!nodeAsText) {
        break;
      }
    }
    *aRightNode = sibling;
    result = (*aRightNode)->GetNextSibling(getter_AddRefs(sibling)); 
  }
  NS_ADDREF((*aRightNode));
  if (gNoisy) { printf("GetBlockSection returning %p %p\n", 
                      (*aLeftNode), (*aRightNode)); }

  return result;
}


///////////////////////////////////////////////////////////////////////////
// GetBlockSectionsForRange: return list of block sections that intersect 
//                           this range
nsresult
nsEditor::GetBlockSectionsForRange(nsIDOMRange *aRange, nsISupportsArray *aSections) 
{
  if (!aRange || !aSections) {return NS_ERROR_NULL_POINTER;}

  nsresult result;
  nsCOMPtr<nsIContentIterator>iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              NS_GET_IID(nsIContentIterator), getter_AddRefs(iter));
  if ((NS_SUCCEEDED(result)) && iter)
  {
    nsCOMPtr<nsIDOMRange> lastRange;
    iter->Init(aRange);
    nsCOMPtr<nsIContent> currentContent;
    iter->CurrentNode(getter_AddRefs(currentContent));
    while (NS_ENUMERATOR_FALSE == iter->IsDone())
    {
      nsCOMPtr<nsIDOMNode>currentNode = do_QueryInterface(currentContent);
      if (currentNode)
      {
        nsCOMPtr<nsIAtom> currentContentTag;
        currentContent->GetTag(*getter_AddRefs(currentContentTag));
        // <BR> divides block content ranges.  We can achieve this by nulling out lastRange
        if (nsIEditProperty::br==currentContentTag.get())
        {
          lastRange = do_QueryInterface(nsnull);
        }
        else
        {
          PRBool isInlineOrText;
          result = IsNodeInline(currentNode, isInlineOrText);
          if (PR_FALSE==isInlineOrText)
          {
            PRUint16 nodeType;
            currentNode->GetNodeType(&nodeType);
            if (nsIDOMNode::TEXT_NODE == nodeType) {
              isInlineOrText = PR_TRUE;
            }
          }
          if (PR_TRUE==isInlineOrText)
          {
            nsCOMPtr<nsIDOMNode>leftNode;
            nsCOMPtr<nsIDOMNode>rightNode;
            result = GetBlockSection(currentNode,
                                     getter_AddRefs(leftNode),
                                     getter_AddRefs(rightNode));
            if (gNoisy) {printf("currentNode %p has block content (%p,%p)\n", currentNode.get(), leftNode.get(), rightNode.get());}
            if ((NS_SUCCEEDED(result)) && leftNode && rightNode)
            {
              // add range to the list if it doesn't overlap with the previous range
              PRBool addRange=PR_TRUE;
              if (lastRange)
              {
                nsCOMPtr<nsIDOMNode> lastStartNode;
                nsCOMPtr<nsIDOMElement> blockParentOfLastStartNode;
                lastRange->GetStartContainer(getter_AddRefs(lastStartNode));
                blockParentOfLastStartNode = do_QueryInterface(GetBlockNodeParent(lastStartNode));
                if (blockParentOfLastStartNode)
                {
                  if (gNoisy) {printf("lastStartNode %p has block parent %p\n", lastStartNode.get(), blockParentOfLastStartNode.get());}
                  nsCOMPtr<nsIDOMElement> blockParentOfLeftNode;
                  blockParentOfLeftNode = do_QueryInterface(GetBlockNodeParent(leftNode));
                  if (blockParentOfLeftNode)
                  {
                    if (gNoisy) {printf("leftNode %p has block parent %p\n", leftNode.get(), blockParentOfLeftNode.get());}
                    if (blockParentOfLastStartNode==blockParentOfLeftNode) {
                      addRange = PR_FALSE;
                    }
                  }
                }
              }
              if (PR_TRUE==addRange) 
              {
                if (gNoisy) {printf("adding range, setting lastRange with start node %p\n", leftNode.get());}
                nsCOMPtr<nsIDOMRange> range;
                result = nsComponentManager::CreateInstance(kCRangeCID, nsnull, 
                                                            NS_GET_IID(nsIDOMRange), getter_AddRefs(range));
                if ((NS_SUCCEEDED(result)) && range)
                { // initialize the range
                  range->SetStart(leftNode, 0);
                  range->SetEnd(rightNode, 0);
                  aSections->AppendElement(range);
                  lastRange = do_QueryInterface(range);
                }
              }        
            }
          }
        }
      }
      /* do not check result here, and especially do not return the result code.
       * we rely on iter->IsDone to tell us when the iteration is complete
       */
      iter->Next();
      iter->CurrentNode(getter_AddRefs(currentContent));
    }
  }
  return result;
}


///////////////////////////////////////////////////////////////////////////
// IsTextOrElementNode: true if node of dom type element or text
//               
PRBool
nsEditor::IsTextOrElementNode(nsIDOMNode *aNode)
{
  if (!aNode)
  {
    NS_NOTREACHED("null node passed to IsTextOrElementNode()");
    return PR_FALSE;
  }
  
  PRUint16 nodeType;
  aNode->GetNodeType(&nodeType);
  if ((nodeType == nsIDOMNode::ELEMENT_NODE) || (nodeType == nsIDOMNode::TEXT_NODE))
    return PR_TRUE;
    
  return PR_FALSE;
}



///////////////////////////////////////////////////////////////////////////
// IsTextNode: true if node of dom type text
//               
PRBool
nsEditor::IsTextNode(nsIDOMNode *aNode)
{
  if (!aNode)
  {
    NS_NOTREACHED("null node passed to IsTextNode()");
    return PR_FALSE;
  }
  
  PRUint16 nodeType;
  aNode->GetNodeType(&nodeType);
  if (nodeType == nsIDOMNode::TEXT_NODE)
    return PR_TRUE;
    
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// GetIndexOf: returns the position index of the node in the parent
//
PRInt32 
nsEditor::GetIndexOf(nsIDOMNode *parent, nsIDOMNode *child)
{
  PRInt32 idx = 0;
  
  NS_PRECONDITION(parent, "null parent passed to nsEditor::GetIndexOf");
  NS_PRECONDITION(parent, "null child passed to nsEditor::GetIndexOf");
  nsCOMPtr<nsIContent> content = do_QueryInterface(parent);
  nsCOMPtr<nsIContent> cChild = do_QueryInterface(child);
  NS_PRECONDITION(content, "null content in nsEditor::GetIndexOf");
  NS_PRECONDITION(cChild, "null content in nsEditor::GetIndexOf");
  
  nsresult res = content->IndexOf(cChild, idx);
  if (NS_FAILED(res)) 
  {
    NS_NOTREACHED("could not find child in parent - nsEditor::GetIndexOf");
  }
  return idx;
}
  

///////////////////////////////////////////////////////////////////////////
// GetChildAt: returns the node at this position index in the parent
//
nsCOMPtr<nsIDOMNode> 
nsEditor::GetChildAt(nsIDOMNode *aParent, PRInt32 aOffset)
{
  nsCOMPtr<nsIDOMNode> resultNode;
  
  if (!aParent) 
    return resultNode;
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aParent);
  nsCOMPtr<nsIContent> cChild;
  NS_PRECONDITION(content, "null content in nsEditor::GetChildAt");
  
  if (NS_FAILED(content->ChildAt(aOffset, *getter_AddRefs(cChild))))
    return resultNode;
  
  resultNode = do_QueryInterface(cChild);
  return resultNode;
}
  


///////////////////////////////////////////////////////////////////////////
// NextNodeInBlock: gets the next/prev node in the block, if any.  Next node
//                  must be an element or text node, others are ignored
nsCOMPtr<nsIDOMNode>
nsEditor::NextNodeInBlock(nsIDOMNode *aNode, IterDirection aDir)
{
  nsCOMPtr<nsIDOMNode> nullNode;
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> blockContent;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIDOMNode> blockParent;
  
  if (!aNode)  return nullNode;

  nsCOMPtr<nsIContentIterator> iter;
  if (NS_FAILED(nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(iter))))
    return nullNode;

  // much gnashing of teeth as we twit back and forth between content and domnode types
  content = do_QueryInterface(aNode);
  if (IsBlockNode(aNode))
  {
    blockParent = do_QueryInterface(aNode);
  }
  else
  {
    blockParent = GetBlockNodeParent(aNode);
  }
  if (!blockParent) return nullNode;
  blockContent = do_QueryInterface(blockParent);
  if (!blockContent) return nullNode;
  
  if (NS_FAILED(iter->Init(blockContent)))  return nullNode;
  if (NS_FAILED(iter->PositionAt(content)))  return nullNode;
  
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    if (NS_FAILED(iter->CurrentNode(getter_AddRefs(content)))) return nullNode;
    // ignore nodes that aren't elements or text, or that are the block parent 
    node = do_QueryInterface(content);
    if (node && IsTextOrElementNode(node) && (node != blockParent) && (node.get() != aNode))
      return node;
    
    if (aDir == kIterForward)
      iter->Next();
    else
      iter->Prev();
  }
  
  return nullNode;
}


///////////////////////////////////////////////////////////////////////////
// GetStartNodeAndOffset: returns whatever the start parent & offset is of 
//                        the first range in the selection.
nsresult 
nsEditor::GetStartNodeAndOffset(nsIDOMSelection *aSelection,
                                       nsCOMPtr<nsIDOMNode> *outStartNode,
                                       PRInt32 *outStartOffset)
{
  if (!outStartNode || !outStartOffset || !aSelection) 
    return NS_ERROR_NULL_POINTER;
    
  nsCOMPtr<nsIEnumerator> enumerator;
  nsresult result;
  result = aSelection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result) || !enumerator)
    return NS_ERROR_FAILURE;
    
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  if ((NS_FAILED(enumerator->CurrentItem(getter_AddRefs(currentItem)))) || !currentItem)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
  if (!range)
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetStartContainer(getter_AddRefs(*outStartNode))))
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetStartOffset(outStartOffset)))
    return NS_ERROR_FAILURE;
    
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetEndNodeAndOffset: returns whatever the end parent & offset is of 
//                        the first range in the selection.
nsresult 
nsEditor::GetEndNodeAndOffset(nsIDOMSelection *aSelection,
                                       nsCOMPtr<nsIDOMNode> *outEndNode,
                                       PRInt32 *outEndOffset)
{
  if (!outEndNode || !outEndOffset) 
    return NS_ERROR_NULL_POINTER;
    
  nsCOMPtr<nsIEnumerator> enumerator;
  nsresult result = aSelection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result) || !enumerator)
    return NS_ERROR_FAILURE;
    
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  if ((NS_FAILED(enumerator->CurrentItem(getter_AddRefs(currentItem)))) || !currentItem)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
  if (!range)
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetEndContainer(getter_AddRefs(*outEndNode))))
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetEndOffset(outEndOffset)))
    return NS_ERROR_FAILURE;
    
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsPreformatted: checks the style info for the node for the preformatted
//                 text style.
nsresult 
nsEditor::IsPreformatted(nsIDOMNode *aNode, PRBool *aResult)
{
  nsresult result;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  nsIFrame *frame;
  nsCOMPtr<nsIStyleContext> styleContext;
  const nsStyleText* styleText;
  PRBool bPreformatted;
  
  if (!aResult || !content) return NS_ERROR_NULL_POINTER;
  
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  
  result = ps->GetPrimaryFrameFor(content, &frame);
  if (NS_FAILED(result)) return result;
  
  result = ps->GetStyleContextFor(frame, getter_AddRefs(styleContext));
  if (NS_FAILED(result))
  {
    // Consider nodes without a style context to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no style context).
    *aResult = PR_FALSE;
    return NS_OK;
  }

  styleText = (const nsStyleText*)styleContext->GetStyleData(eStyleStruct_Text);

  bPreformatted = (NS_STYLE_WHITESPACE_PRE == styleText->mWhiteSpace) ||
    (NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace);

  *aResult = bPreformatted;
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsNextCharWhitespace: checks the adjacent content in the same block
//                       to see if following selection is whitespace or nbsp
nsresult 
nsEditor::IsNextCharWhitespace(nsIDOMNode *aParentNode, 
                                      PRInt32 aOffset,
                                      PRBool *outIsSpace,
                                      PRBool *outIsNBSP,
                                      nsCOMPtr<nsIDOMNode> *outNode,
                                      PRInt32 *outOffset)
{
  if (!outIsSpace || !outIsNBSP) return NS_ERROR_NULL_POINTER;
  *outIsSpace = PR_FALSE;
  *outIsNBSP = PR_FALSE;
  if (outNode) *outNode = nsnull;
  if (outOffset) *outOffset = -1;
  
  nsAutoString tempString;
  PRUint32 strLength;
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aParentNode);
  if (textNode)
  {
    textNode->GetLength(&strLength);
    if ((PRUint32)aOffset < strLength)
    {
      // easy case: next char is in same node
      textNode->SubstringData(aOffset,aOffset+1,tempString);
      *outIsSpace = nsCRT::IsAsciiSpace(tempString.First());
      *outIsNBSP = (tempString.First() == nbsp);
      if (outNode) *outNode = do_QueryInterface(aParentNode);
      if (outOffset) *outOffset = aOffset+1;  // yes, this is _past_ the character; 
      return NS_OK;
    }
  }
  
  // harder case: next char in next node.
  nsCOMPtr<nsIDOMNode> node = NextNodeInBlock(aParentNode, kIterForward);
  nsCOMPtr<nsIDOMNode> tmp;
  while (node) 
  {
    if (!IsInlineNode(node))  // skip over bold, italic, link, ect nodes
    {
      if (IsTextNode(node) && IsEditable(node))
      {
        textNode = do_QueryInterface(node);
        textNode->GetLength(&strLength);
        if (strLength)
        {
          // you could use nsITextContent::IsOnlyWhitespace here
          textNode->SubstringData(0,1,tempString);
          *outIsSpace = nsCRT::IsAsciiSpace(tempString.First());
          *outIsNBSP = (tempString.First() == nbsp);
          if (outNode) *outNode = do_QueryInterface(node);
          if (outOffset) *outOffset = 1;  // yes, this is _past_ the character; 
          return NS_OK;
        }
        // else it's an empty text node, or not editable; skip it.
      }
      else  // node is an image or some other thingy that doesn't count as whitespace
      {
        break;
      }
    }
    tmp = node;
    node = NextNodeInBlock(tmp, kIterForward);
  }
  
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsPrevCharWhitespace: checks the adjacent content in the same block
//                       to see if following selection is whitespace
nsresult 
nsEditor::IsPrevCharWhitespace(nsIDOMNode *aParentNode, 
                                      PRInt32 aOffset,
                                      PRBool *outIsSpace,
                                      PRBool *outIsNBSP,
                                      nsCOMPtr<nsIDOMNode> *outNode,
                                      PRInt32 *outOffset)
{
  if (!outIsSpace || !outIsNBSP) return NS_ERROR_NULL_POINTER;
  *outIsSpace = PR_FALSE;
  *outIsNBSP = PR_FALSE;
  if (outNode) *outNode = nsnull;
  if (outOffset) *outOffset = -1;
  
  nsAutoString tempString;
  PRUint32 strLength;
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aParentNode);
  if (textNode)
  {
    if (aOffset > 0)
    {
      // easy case: prev char is in same node
      textNode->SubstringData(aOffset-1,aOffset,tempString);
      *outIsSpace = nsCRT::IsAsciiSpace(tempString.First());
      *outIsNBSP = (tempString.First() == nbsp);
      if (outNode) *outNode = do_QueryInterface(aParentNode);
      if (outOffset) *outOffset = aOffset-1;  
      return NS_OK;
    }
  }
  
  // harder case: prev char in next node
  nsCOMPtr<nsIDOMNode> node = NextNodeInBlock(aParentNode, kIterBackward);
  nsCOMPtr<nsIDOMNode> tmp;
  while (node) 
  {
    if (!IsInlineNode(node))  // skip over bold, italic, link, ect nodes
    {
      if (IsTextNode(node) && IsEditable(node))
      {
        textNode = do_QueryInterface(node);
        textNode->GetLength(&strLength);
        if (strLength)
        {
          // you could use nsITextContent::IsOnlyWhitespace here
          textNode->SubstringData(strLength-1,strLength,tempString);
          *outIsSpace = nsCRT::IsAsciiSpace(tempString.First());
          *outIsNBSP = (tempString.First() == nbsp);
          if (outNode) *outNode = do_QueryInterface(aParentNode);
          if (outOffset) *outOffset = strLength-1;  
          return NS_OK;
        }
        // else it's an empty text node, or not editable; skip it.
      }
      else  // node is an image or some other thingy that doesn't count as whitespace
      {
        break;
      }
    }
    // otherwise we found a node we want to skip, keep going
    tmp = node;
    node = NextNodeInBlock(tmp, kIterBackward);
  }
  
  return NS_OK;
  
}


///////////////////////////////////////////////////////////////////////////
// SplitNodeDeep: this splits a node "deeply", splitting children as 
//                appropriate.  The place to split is represented by
//                a dom point at {splitPointParent, splitPointOffset}.
//                That dom point must be inside aNode, which is the node to 
//                split.  outOffset is set to the offset in the parent of aNode where
//                the split terminates - where you would want to insert 
//                a new element, for instance, if thats why you were splitting 
//                the node.
//
nsresult
nsEditor::SplitNodeDeep(nsIDOMNode *aNode, 
                        nsIDOMNode *aSplitPointParent, 
                        PRInt32 aSplitPointOffset,
                        PRInt32 *outOffset,
                        PRBool  aNoEmptyContainers,
                        nsCOMPtr<nsIDOMNode> *outLeftNode,
                        nsCOMPtr<nsIDOMNode> *outRightNode)
{
  if (!aNode || !aSplitPointParent || !outOffset) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> nodeToSplit = do_QueryInterface(aSplitPointParent);
  nsCOMPtr<nsIDOMNode> tempNode, parentNode;  
  PRInt32 offset = aSplitPointOffset;
  nsresult res;
  
  if (outLeftNode)  *outLeftNode  = nsnull;
  if (outRightNode) *outRightNode = nsnull;
  
  while (nodeToSplit)
  {
    // need to insert rules code call here to do things like
    // not split a list if you are after the last <li> or before the first, etc.
    // for now we just have some smarts about unneccessarily splitting
    // textnodes, which should be universal enough to put straight in
    // this nsEditor routine.
    
    nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(nodeToSplit);
    PRUint32 len;
    PRBool bDoSplit = PR_FALSE;
    res = GetLengthOfDOMNode(nodeToSplit, len);
    if (NS_FAILED(res)) return res;
    
    if (!(aNoEmptyContainers || nodeAsText) || (offset && (offset != (PRInt32)len)))
    {
      bDoSplit = PR_TRUE;
      res = SplitNode(nodeToSplit, offset, getter_AddRefs(tempNode));
      if (NS_FAILED(res)) return res;
      if (outRightNode) *outRightNode = nodeToSplit;
      if (outLeftNode)  *outLeftNode  = tempNode;
    }

    res = nodeToSplit->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(res)) return res;
    if (!parentNode) return NS_ERROR_FAILURE;
    
    if (!bDoSplit && offset)  // must be "end of text node" case, we didn't split it, just move past it
    {
      offset = GetIndexOf(parentNode, nodeToSplit) +1;
      if (outLeftNode)  *outLeftNode  = nodeToSplit;
    }
    else
    {
      offset = GetIndexOf(parentNode, nodeToSplit);
      if (outRightNode) *outRightNode = nodeToSplit;
    }
    
    if (nodeToSplit.get() == aNode)  // we split all the way up to (and including) aNode; we're done
      break;
      
    nodeToSplit = parentNode;
  }
  
  if (!nodeToSplit)
  {
    NS_NOTREACHED("null node obtained in nsEditor::SplitNodeDeep()");
    return NS_ERROR_FAILURE;
  }
  
  *outOffset = offset;
  
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// JoinNodeDeep:  this joins two like nodes "deeply", joining children as 
//                appropriate.  
nsresult
nsEditor::JoinNodeDeep(nsIDOMNode *aLeftNode, 
                       nsIDOMNode *aRightNode,
                       nsCOMPtr<nsIDOMNode> *aOutJoinNode, 
                       PRInt32 *outOffset)
{
  if (!aLeftNode || !aRightNode || !aOutJoinNode || !outOffset) return NS_ERROR_NULL_POINTER;

  // while the rightmost children and their descendants of the left node 
  // match the leftmost children and their descendants of the right node
  // join them up.  Can you say that three times fast?
  
  nsCOMPtr<nsIDOMNode> leftNodeToJoin = do_QueryInterface(aLeftNode);
  nsCOMPtr<nsIDOMNode> rightNodeToJoin = do_QueryInterface(aRightNode);
  nsCOMPtr<nsIDOMNode> parentNode,tmp;
  nsresult res = NS_OK;
  
  rightNodeToJoin->GetParentNode(getter_AddRefs(parentNode));
  
  while (leftNodeToJoin && rightNodeToJoin && parentNode &&
          NodesSameType(leftNodeToJoin, rightNodeToJoin))
  {
    // adjust out params
    PRUint32 length;
    if (IsTextNode(leftNodeToJoin))
    {
      nsCOMPtr<nsIDOMCharacterData>nodeAsText;
      nodeAsText = do_QueryInterface(leftNodeToJoin);
      nodeAsText->GetLength(&length);
    }
    else
    {
      res = GetLengthOfDOMNode(leftNodeToJoin, length);
      if (NS_FAILED(res)) return res;
    }
    
    *aOutJoinNode = rightNodeToJoin;
    *outOffset = length;
    
    // do the join
    res = JoinNodes(leftNodeToJoin, rightNodeToJoin, parentNode);
    if (NS_FAILED(res)) return res;
    
    if (IsTextNode(parentNode)) // we've joined all the way down to text nodes, we're done!
      return NS_OK;

    else
    {
      // get new left and right nodes, and begin anew
      parentNode = rightNodeToJoin;
      leftNodeToJoin = GetChildAt(parentNode, length-1);
      rightNodeToJoin = GetChildAt(parentNode, length);

      // skip over non-editable nodes
      while (leftNodeToJoin && !IsEditable(leftNodeToJoin))
      {
        leftNodeToJoin->GetPreviousSibling(getter_AddRefs(tmp));
        leftNodeToJoin = tmp;
      }
      if (!leftNodeToJoin) break;
    
      while (rightNodeToJoin && !IsEditable(rightNodeToJoin))
      {
        rightNodeToJoin->GetNextSibling(getter_AddRefs(tmp));
        rightNodeToJoin = tmp;
      }
      if (!rightNodeToJoin) break;
    }
  }
  
  return res;
}

nsresult nsEditor::BeginUpdateViewBatch()
{
  NS_PRECONDITION(mUpdateCount>=0, "bad state");

  nsCOMPtr<nsIDOMSelection>selection;
  nsresult rv = GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(rv) && selection) 
  {
    selection->StartBatchChanges();
  }

  if (nsnull!=mViewManager)
  {
    if (0==mUpdateCount)
    {
#ifdef HACK_FORCE_REDRAW
      mViewManager->DisableRefresh();
#else
      mViewManager->BeginUpdateViewBatch();
#endif
      nsCOMPtr<nsIPresShell> presShell;
      rv = GetPresShell(getter_AddRefs(presShell));
      if (NS_SUCCEEDED(rv) && presShell)
        presShell->BeginReflowBatching();
    }
    mUpdateCount++;
  }

  return NS_OK;
}


nsresult nsEditor::EndUpdateViewBatch()
{
  NS_PRECONDITION(mUpdateCount>0, "bad state");
  
  nsresult rv;
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mSelConWeak,&rv);
  if (NS_FAILED(rv) || !selCon)
    return rv?rv:NS_ERROR_FAILURE;
    
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  nsCOMPtr<nsICaret> caret;
  if (ps)
    rv = ps->GetCaret(getter_AddRefs(caret));
  if (NS_FAILED(rv) ||!caret)
    return rv?rv:NS_ERROR_FAILURE;
  StCaretHider caretHider(caret);
        
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult selectionResult = GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(selectionResult) && selection) {
    selection->EndBatchChanges();
  }

  if (mViewManager)
  {
    mUpdateCount--;
    if (0==mUpdateCount)
    {
      PRUint32 flags = 0;

      rv = GetFlags(&flags);

      if (NS_FAILED(rv))
        return rv;

      // Make sure we enable reflowing before we call
      // mViewManager->EndUpdateViewBatch().  This will make sure that any
      // new updates caused by a reflow, that may happen during the
      // EndReflowBatching(), get included if we force a refresh during
      // the mViewManager->EndUpdateViewBatch() call.

      PRBool forceReflow = PR_TRUE;

      if (flags & nsIHTMLEditor::eEditorDisableForcedReflowsMask)
        forceReflow = PR_FALSE;

      nsCOMPtr<nsIPresShell>    presShell;
      rv = GetPresShell(getter_AddRefs(presShell));
      if (NS_SUCCEEDED(rv) && presShell)
        presShell->EndReflowBatching(forceReflow);

      PRUint32 updateFlag = NS_VMREFRESH_IMMEDIATE;

      if (flags & nsIHTMLEditor::eEditorDisableForcedUpdatesMask)
        updateFlag = NS_VMREFRESH_NO_SYNC;

#ifdef HACK_FORCE_REDRAW
      mViewManager->EnableRefresh(updateFlag);
      HACKForceRedraw();
#else
      mViewManager->EndUpdateViewBatch(updateFlag);
#endif
    }
  }  

  return NS_OK;
}

PRBool 
nsEditor::GetShouldTxnSetSelection()
{
  return mShouldTxnSetSelection;
}


void   
nsEditor::SetShouldTxnSetSelection(PRBool aShould)
{
  mShouldTxnSetSelection = aShould;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  protected nsEditor methods 
#pragma mark -
#endif


NS_IMETHODIMP 
nsEditor::DeleteSelectionImpl(nsIEditor::EDirection aAction)
{
  nsresult res;

  EditAggregateTxn *txn;
  PRInt32 i;
  nsIEditActionListener *listener;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  res = CreateTxnForDeleteSelection(aAction, &txn);
  if (NS_FAILED(res)) return res;
  nsAutoRules beginRulesSniffing(this, kOpDeleteSelection, aAction);

  if (NS_SUCCEEDED(res))  
  {
    if (mActionListeners)
    {
      for (i = 0; i < mActionListeners->Count(); i++)
      {
        listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
        if (listener)
          listener->WillDeleteSelection(selection);
      }
    }

    res = Do(txn);  

    if (mActionListeners)
    {
      for (i = 0; i < mActionListeners->Count(); i++)
      {
        listener = (nsIEditActionListener *)mActionListeners->ElementAt(i);
        if (listener)
          listener->DidDeleteSelection(selection);
      }
    }
  }

  // The transaction system (if any) has taken ownwership of txn
  NS_IF_RELEASE(txn);

  return res;
}


/* Non-interface, protected methods */

NS_IMETHODIMP 
nsEditor::DoAfterDoTransaction(nsITransaction *aTxn)
{
  nsresult rv = NS_OK;
  
  PRBool  isTransientTransaction;
  rv = aTxn->GetIsTransient(&isTransientTransaction);
  if (NS_FAILED(rv))
    return rv;
  
  if (!isTransientTransaction)
  {
    // we need to deal here with the case where the user saved after some
    // edits, then undid one or more times. Then, the undo count is -ve,
    // but we can't let a do take it back to zero. So we flip it up to
    // a +ve number.
    PRInt32 modCount;
    GetDocModCount(modCount);
    if (modCount < 0)
      modCount = -modCount;
        
    rv = IncDocModCount(1);    // don't count transient transactions
  }
  
  return rv;
}


NS_IMETHODIMP 
nsEditor::DoAfterUndoTransaction()
{
  nsresult rv = NS_OK;

  rv = IncDocModCount(-1);    // all undoable transactions are non-transient

  return rv;
}

NS_IMETHODIMP 
nsEditor::DoAfterRedoTransaction()
{
  nsresult rv = NS_OK;

  rv = IncDocModCount(1);    // all redoable transactions are non-transient

  return rv;
}

NS_IMETHODIMP 
nsEditor::DoAfterDocumentSave()
{
  // the mod count is reset by nsIDiskDocument.
  NotifyDocumentListeners(eDocumentStateChanged);
  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::CreateTxnForSetAttribute(nsIDOMElement *aElement, 
                                   const nsString& aAttribute, 
                                   const nsString& aValue,
                                   ChangeAttributeTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(ChangeAttributeTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aElement, aAttribute, aValue, PR_FALSE);
    }
  }
  return result;
}


NS_IMETHODIMP 
nsEditor::CreateTxnForRemoveAttribute(nsIDOMElement *aElement, 
                                      const nsString& aAttribute,
                                      ChangeAttributeTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(ChangeAttributeTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  
    {
      nsAutoString value;
      result = (*aTxn)->Init(this, aElement, aAttribute, value, PR_TRUE);
    }
  }
  return result;
}


NS_IMETHODIMP nsEditor::CreateTxnForCreateElement(const nsString& aTag,
                                                  nsIDOMNode     *aParent,
                                                  PRInt32         aPosition,
                                                  CreateElementTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aParent)
  {
    result = TransactionFactory::GetNewTransaction(CreateElementTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result))  {
      result = (*aTxn)->Init(this, aTag, aParent, aPosition);
    }
  }
  return result;
}


NS_IMETHODIMP nsEditor::CreateTxnForInsertElement(nsIDOMNode * aNode,
                                                  nsIDOMNode * aParent,
                                                  PRInt32      aPosition,
                                                  InsertElementTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (aNode && aParent && aTxn)
  {
    result = TransactionFactory::GetNewTransaction(InsertElementTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result)) {
      result = (*aTxn)->Init(aNode, aParent, aPosition, this);
    }
  }
  return result;
}

NS_IMETHODIMP nsEditor::CreateTxnForDeleteElement(nsIDOMNode * aElement,
                                             DeleteElementTxn ** aTxn)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (nsnull != aElement)
  {
    result = TransactionFactory::GetNewTransaction(DeleteElementTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_SUCCEEDED(result)) {
      result = (*aTxn)->Init(aElement);
    }
  }
  return result;
}

/*NS_IMETHODIMP nsEditor::CreateAggregateTxnForDeleteSelection(nsIAtom *aTxnName, EditAggregateTxn **aAggTxn) 
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (aAggTxn)
  {
    *aAggTxn = nsnull;
    result = TransactionFactory::GetNewTransaction(EditAggregateTxn::GetCID(), (EditTxn**)aAggTxn); 

    if (NS_FAILED(result) || !*aAggTxn) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Set the name for the aggregate transaction  
    (*aAggTxn)->SetName(aTxnName);

    // Get current selection and setup txn to delete it,
    //  but only if selection exists (is not a collapsed "caret" state)
    nsCOMPtr<nsIDOMSelection> selection;
    if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
    if (!ps) return NS_ERROR_NOT_INITIALIZED;
    result = ps->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
    if (NS_SUCCEEDED(result) && selection)
    {
      PRBool collapsed;
      result = selection->GetIsCollapsed(&collapsed);
      if (NS_SUCCEEDED(result) && !collapsed) {
        EditAggregateTxn *delSelTxn;
        result = CreateTxnForDeleteSelection(eNone, &delSelTxn);
        if (NS_SUCCEEDED(result) && delSelTxn) {
          (*aAggTxn)->AppendChild(delSelTxn);
          NS_RELEASE(delSelTxn);
        }
      }
    }
  }
  return result;
}
*/

NS_IMETHODIMP 
nsEditor::CreateTxnForIMEText(const nsString & aStringToInsert,
                              IMETextTxn ** aTxn)
{
  NS_ASSERTION(aTxn, "illegal value- null ptr- aTxn");
  if(!aTxn) return NS_ERROR_NULL_POINTER;
     
  nsresult  result;

  result = TransactionFactory::GetNewTransaction(IMETextTxn::GetCID(), (EditTxn **)aTxn);
  if (nsnull!=*aTxn) {
    result = (*aTxn)->Init(mIMETextNode,mIMETextOffset,mIMEBufferLength,mIMETextRangeList,aStringToInsert,mSelConWeak);
  }
  else {
    result = NS_ERROR_OUT_OF_MEMORY;
  }
  return result;
}


NS_IMETHODIMP 
nsEditor::CreateTxnForAddStyleSheet(nsICSSStyleSheet* aSheet, AddStyleSheetTxn* *aTxn)
{
  nsresult rv = TransactionFactory::GetNewTransaction(AddStyleSheetTxn::GetCID(), (EditTxn **)aTxn);
  if (NS_FAILED(rv))
    return rv;
    
  if (! *aTxn)
    return NS_ERROR_OUT_OF_MEMORY;

  return (*aTxn)->Init(this, aSheet);
}



NS_IMETHODIMP 
nsEditor::CreateTxnForRemoveStyleSheet(nsICSSStyleSheet* aSheet, RemoveStyleSheetTxn* *aTxn)
{
  nsresult rv = TransactionFactory::GetNewTransaction(RemoveStyleSheetTxn::GetCID(), (EditTxn **)aTxn);
  if (NS_FAILED(rv))
    return rv;
    
  if (! *aTxn)
    return NS_ERROR_OUT_OF_MEMORY;

  return (*aTxn)->Init(this, aSheet);
}


NS_IMETHODIMP
nsEditor::CreateTxnForDeleteSelection(nsIEditor::EDirection aAction,
                                      EditAggregateTxn  ** aTxn)
{
  if (!aTxn)
    return NS_ERROR_NULL_POINTER;
  *aTxn = nsnull;

#ifdef DEBUG_akkana
  NS_ASSERTION(aAction != eNextWord && aAction != ePreviousWord && aAction != eToEndOfLine, "CreateTxnForDeleteSelection: unsupported action!");
#endif

  nsresult result;
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mSelConWeak);
  if (!selCon) return NS_ERROR_NOT_INITIALIZED;
  result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    // Check whether the selection is collapsed and we should do nothing:
    PRBool isCollapsed;
    result = (selection->GetIsCollapsed(&isCollapsed));
    if (NS_SUCCEEDED(result) && isCollapsed && aAction == eNone)
      return NS_OK;

    // allocate the out-param transaction
    result = TransactionFactory::GetNewTransaction(EditAggregateTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_FAILED(result)) {
      return result;
    }

    nsCOMPtr<nsIEnumerator> enumerator;
    result = selection->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_SUCCEEDED(result) && enumerator)
    {
      for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
      {
        nsCOMPtr<nsISupports> currentItem;
        result = enumerator->CurrentItem(getter_AddRefs(currentItem));
        if ((NS_SUCCEEDED(result)) && (currentItem))
        {
          nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
          range->GetCollapsed(&isCollapsed);
          if (PR_FALSE==isCollapsed)
          {
            DeleteRangeTxn *txn;
            result = TransactionFactory::GetNewTransaction(DeleteRangeTxn::GetCID(), (EditTxn **)&txn);
            if ((NS_SUCCEEDED(result)) && (nsnull!=txn))
            {
              txn->Init(this, range);
              (*aTxn)->AppendChild(txn);
              NS_RELEASE(txn);
            }
            else
              result = NS_ERROR_OUT_OF_MEMORY;
          }
          else
          { // we have an insertion point.  delete the thing in front of it or behind it, depending on aAction
            result = CreateTxnForDeleteInsertionPoint(range, aAction, *aTxn);
          }
        }
      }
    }
  }

  // if we didn't build the transaction correctly, destroy the out-param transaction so we don't leak it.
  if (NS_FAILED(result))
  {
    NS_IF_RELEASE(*aTxn);
  }

  return result;
}


//XXX: currently, this doesn't handle edge conditions because GetNext/GetPrior are not implemented
NS_IMETHODIMP
nsEditor::CreateTxnForDeleteInsertionPoint(nsIDOMRange         *aRange, 
                                           nsIEditor::EDirection
                                             aAction,
                                           EditAggregateTxn    *aTxn)
{
  nsCOMPtr<nsIDOMNode> node;
  PRBool isFirst;
  PRBool isLast;
  PRInt32 offset;

  // get the node and offset of the insertion point
  nsresult result = aRange->GetStartContainer(getter_AddRefs(node));
  if (NS_FAILED(result))
    return result;
  result = aRange->GetStartOffset(&offset);
  if (NS_FAILED(result))
    return result;

  // determine if the insertion point is at the beginning, middle, or end of the node
  nsCOMPtr<nsIDOMCharacterData> nodeAsText;
  nodeAsText = do_QueryInterface(node);

  if (nodeAsText)
  {
    PRUint32 count;
    nodeAsText->GetLength(&count);
    isFirst = PRBool(0==offset);
    isLast  = PRBool(count==(PRUint32)offset);
  }
  else
  { 
    // get the child list and count
    nsCOMPtr<nsIDOMNodeList>childList;
    PRUint32 count=0;
    result = node->GetChildNodes(getter_AddRefs(childList));
    if ((NS_SUCCEEDED(result)) && childList)
    {
      childList->GetLength(&count);
    }
    isFirst = PRBool(0==offset);
    isLast  = PRBool((count-1)==(PRUint32)offset);
  }
// XXX: if isFirst && isLast, then we'll need to delete the node 
  //    as well as the 1 child

  // build a transaction for deleting the appropriate data
  // XXX: this has to come from rule section
  if ((ePrevious==aAction) && (PR_TRUE==isFirst))
  { // we're backspacing from the beginning of the node.  Delete the first thing to our left
    nsCOMPtr<nsIDOMNode> priorNode;
    result = GetPriorNode(node, PR_TRUE, getter_AddRefs(priorNode));
    if ((NS_SUCCEEDED(result)) && priorNode)
    { // there is a priorNode, so delete it's last child (if text content, delete the last char.)
      // if it has no children, delete it
      nsCOMPtr<nsIDOMCharacterData> priorNodeAsText;
      priorNodeAsText = do_QueryInterface(priorNode);
      if (priorNodeAsText)
      {
        PRUint32 length=0;
        priorNodeAsText->GetLength(&length);
        if (0<length)
        {
          DeleteTextTxn *txn;
          result = CreateTxnForDeleteText(priorNodeAsText, length-1, 1, &txn);
          if (NS_SUCCEEDED(result)) {
            aTxn->AppendChild(txn);
            NS_RELEASE(txn);
          }
        }
        else
        { // XXX: can you have an empty text node?  If so, what do you do?
          printf("ERROR: found a text node with 0 characters\n");
          result = NS_ERROR_UNEXPECTED;
        }
      }
      else
      { // priorNode is not text, so tell it's parent to delete it
        DeleteElementTxn *txn;
        result = CreateTxnForDeleteElement(priorNode, &txn);
        if (NS_SUCCEEDED(result)) {
          aTxn->AppendChild(txn);
          NS_RELEASE(txn);
        }
      }
    }
  }
  else if ((nsIEditor::eNext==aAction) && (PR_TRUE==isLast))
  { // we're deleting from the end of the node.  Delete the first thing to our right
    nsCOMPtr<nsIDOMNode> nextNode;
    result = GetNextNode(node, PR_TRUE, getter_AddRefs(nextNode));
    if ((NS_SUCCEEDED(result)) && nextNode)
    { // there is a nextNode, so delete it's first child (if text content, delete the first char.)
      // if it has no children, delete it
      nsCOMPtr<nsIDOMCharacterData> nextNodeAsText;
      nextNodeAsText = do_QueryInterface(nextNode);
      if (nextNodeAsText)
      {
        PRUint32 length=0;
        nextNodeAsText->GetLength(&length);
        if (0<length)
        {
          DeleteTextTxn *txn;
          result = CreateTxnForDeleteText(nextNodeAsText, 0, 1, &txn);
          if (NS_SUCCEEDED(result)) {
            aTxn->AppendChild(txn);
            NS_RELEASE(txn);
          }
        }
        else
        { // XXX: can you have an empty text node?  If so, what do you do?
          printf("ERROR: found a text node with 0 characters\n");
          result = NS_ERROR_UNEXPECTED;
        }
      }
      else
      { // nextNode is not text, so tell it's parent to delete it
        DeleteElementTxn *txn;
        result = CreateTxnForDeleteElement(nextNode, &txn);
        if (NS_SUCCEEDED(result)) {
          aTxn->AppendChild(txn);
          NS_RELEASE(txn);
        }
      }
    }
  }
  else
  {
    if (nodeAsText)
    { // we have text, so delete a char at the proper offset
      if (nsIEditor::ePrevious==aAction) {
        offset --;
      }
      DeleteTextTxn *txn;
      result = CreateTxnForDeleteText(nodeAsText, offset, 1, &txn);
      if (NS_SUCCEEDED(result)) {
        aTxn->AppendChild(txn);
        NS_RELEASE(txn);
      }
    }
    else
    { // we're either deleting a node or some text, need to dig into the next/prev node to find out
      nsCOMPtr<nsIDOMNode> selectedNode;
      if (ePrevious==aAction)
      {
        result = GetPriorNode(node, offset, PR_TRUE, getter_AddRefs(selectedNode));
      }
      else if (eNext==aAction)
      {
        result = GetNextNode(node, offset, PR_TRUE, getter_AddRefs(selectedNode));
      }
      if (NS_FAILED(result)) { return result; }
      if (selectedNode) 
      {
        nsCOMPtr<nsIDOMCharacterData> selectedNodeAsText;
        selectedNodeAsText = do_QueryInterface(selectedNode);
        if (selectedNodeAsText)
        { // we are deleting from a text node, so do a text deletion
          PRInt32 begin = 0;    // default for forward delete
          if (ePrevious==aAction)
          {
            PRUint32 length=0;
            selectedNodeAsText->GetLength(&length);
            if (0<length)
              begin = length-1;
          }
          DeleteTextTxn *delTextTxn;
          result = CreateTxnForDeleteText(selectedNodeAsText, begin, 1, &delTextTxn);
          if (NS_FAILED(result))  { return result; }
          if (!delTextTxn) { return NS_ERROR_NULL_POINTER; }
          aTxn->AppendChild(delTextTxn);
          NS_RELEASE(delTextTxn);
        }
        else
        {
          DeleteElementTxn *delElementTxn;
          result = CreateTxnForDeleteElement(selectedNode, &delElementTxn);
          if (NS_FAILED(result))  { return result; }
          if (!delElementTxn) { return NS_ERROR_NULL_POINTER; }
          aTxn->AppendChild(delElementTxn);
          NS_RELEASE(delElementTxn);
        }
      }
    }
  }
  return result;
}

nsresult 
nsEditor::CreateRange(nsIDOMNode *aStartParent, PRInt32 aStartOffset,
                      nsIDOMNode *aEndParent, PRInt32 aEndOffset,
                      nsIDOMRange **aRange)
{
  nsresult result;
  result = nsComponentManager::CreateInstance(kCDOMRangeCID, nsnull,
                                              NS_GET_IID(nsIDOMRange), 
                                              (void **)aRange);

  if (NS_FAILED(result))
    return result;

  if (!*aRange)
    return NS_ERROR_NULL_POINTER;

  result = (*aRange)->SetStart(aStartParent, aStartOffset);

  if (NS_SUCCEEDED(result))
    result = (*aRange)->SetEnd(aEndParent, aEndOffset);

  if (NS_FAILED(result))
  {
    NS_RELEASE((*aRange));
    *aRange = 0;
  }
  return result;
}

nsresult 
nsEditor::GetFirstNodeInRange(nsIDOMRange *aRange, nsIDOMNode **aNode)
{
  // Note: this might return a node that is outside of the range.
  // Use caqrefully.
  if (!aRange || !aNode) return NS_ERROR_NULL_POINTER;

  *aNode = nsnull;

  nsCOMPtr<nsIDOMNode> startParent;
  nsresult res = aRange->GetStartContainer(getter_AddRefs(startParent));
  if (NS_FAILED(res)) return res;
  if (!startParent) return NS_ERROR_FAILURE;

  PRInt32 offset;
  res = aRange->GetStartOffset(&offset);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> child = GetChildAt(startParent, offset);
  if (!child) return NS_ERROR_FAILURE;

  *aNode = child.get();
  NS_ADDREF(*aNode);

  return res;
}

nsresult 
nsEditor::AppendNodeToSelectionAsRange(nsIDOMNode *aNode)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if(!selection) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> parentNode;
  res = aNode->GetParentNode(getter_AddRefs(parentNode));
  if (NS_FAILED(res)) return res;
  if (!parentNode) return NS_ERROR_NULL_POINTER;
  
  PRInt32 offset;
  res = GetChildOffset(aNode, parentNode, offset);
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIDOMRange> range;
  res = CreateRange(parentNode, offset, parentNode, offset+1, getter_AddRefs(range));
  if (NS_FAILED(res)) return res;
  if (!range) return NS_ERROR_NULL_POINTER;

  return selection->AddRange(range);
}

nsresult nsEditor::ClearSelection()
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_FAILURE;
  return selection->ClearSelection();  
}

nsresult
nsEditor::CreateHTMLContent(const nsString& aTag, nsIContent** aContent)
{
  nsresult rv;

  NS_WITH_SERVICE(nsIElementFactory, elementFactory,
    NS_ELEMENT_FACTORY_CONTRACTID_PREFIX"http://www.w3.org/1999/xhtml" , &rv);
  if (!elementFactory)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDocument> tempDoc;
  nsCOMPtr<nsIDocument> doc;

  rv = GetDocument(getter_AddRefs(tempDoc));
  if (NS_FAILED(rv) || !tempDoc)
    return rv?rv:NS_ERROR_FAILURE;

  doc = do_QueryInterface(tempDoc);

  nsCOMPtr<nsINodeInfoManager> nodeInfoManager;
  rv = doc->GetNodeInfoManager(*getter_AddRefs(nodeInfoManager));
  if (NS_FAILED(rv))
    return rv;

  NS_ENSURE_TRUE(nodeInfoManager, NS_ERROR_FAILURE);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = nodeInfoManager->GetNodeInfo(aTag, nsnull, kNameSpaceID_None,*getter_AddRefs(nodeInfo));

  if (NS_FAILED(rv) || !nodeInfo)
    return rv?rv:NS_ERROR_FAILURE;

  rv = elementFactory->CreateInstanceByTag(nodeInfo, aContent);

  if (NS_FAILED(rv) || !aContent)
    return rv?rv:NS_ERROR_FAILURE;
  else
    return NS_OK;
}

