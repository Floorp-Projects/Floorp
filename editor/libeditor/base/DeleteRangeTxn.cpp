/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteRangeTxn.h"
#include "nsIDOMRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMNodeList.h"
#include "nsISelection.h"
#include "DeleteTextTxn.h"
#include "DeleteElementTxn.h"
#include "nsIContentIterator.h"
#include "nsIContent.h"
#include "nsComponentManagerUtils.h"

#ifdef NS_DEBUG
static bool gNoisy = false;
#endif

// note that aEditor is not refcounted
DeleteRangeTxn::DeleteRangeTxn()
: EditAggregateTxn()
,mRange()
,mStartParent()
,mStartOffset(0)
,mEndParent()
,mCommonParent()
,mEndOffset(0)
,mEditor(nsnull)
,mRangeUpdater(nsnull)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(DeleteRangeTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DeleteRangeTxn,
                                                EditAggregateTxn)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRange)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mStartParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mEndParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCommonParent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DeleteRangeTxn,
                                                  EditAggregateTxn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mStartParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mEndParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCommonParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteRangeTxn)
NS_INTERFACE_MAP_END_INHERITING(EditAggregateTxn)

NS_IMETHODIMP DeleteRangeTxn::Init(nsIEditor *aEditor, 
                                   nsIDOMRange *aRange,
                                   nsRangeUpdater *aRangeUpdater)
{
  NS_ASSERTION(aEditor && aRange, "bad state");
  if (!aEditor || !aRange) { return NS_ERROR_NOT_INITIALIZED; }

  mEditor = aEditor;
  mRange  = do_QueryInterface(aRange);
  mRangeUpdater = aRangeUpdater;
  
  nsresult result = aRange->GetStartContainer(getter_AddRefs(mStartParent));
  NS_ASSERTION((NS_SUCCEEDED(result)), "GetStartParent failed.");
  result = aRange->GetEndContainer(getter_AddRefs(mEndParent));
  NS_ASSERTION((NS_SUCCEEDED(result)), "GetEndParent failed.");
  result = aRange->GetStartOffset(&mStartOffset);
  NS_ASSERTION((NS_SUCCEEDED(result)), "GetStartOffset failed.");
  result = aRange->GetEndOffset(&mEndOffset);
  NS_ASSERTION((NS_SUCCEEDED(result)), "GetEndOffset failed.");
  result = aRange->GetCommonAncestorContainer(getter_AddRefs(mCommonParent));
  NS_ASSERTION((NS_SUCCEEDED(result)), "GetCommonParent failed.");

  if (!mEditor->IsModifiableNode(mStartParent)) {
    return NS_ERROR_FAILURE;
  }

  if (mStartParent!=mEndParent &&
      (!mEditor->IsModifiableNode(mEndParent) ||
       !mEditor->IsModifiableNode(mCommonParent)))
  {
      return NS_ERROR_FAILURE;
  }

#ifdef NS_DEBUG
  {
    PRUint32 count;
    nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(mStartParent);
    if (textNode)
      textNode->GetLength(&count);
    else
    {
      nsCOMPtr<nsIDOMNodeList> children;
      result = mStartParent->GetChildNodes(getter_AddRefs(children));
      NS_ASSERTION(((NS_SUCCEEDED(result)) && children), "bad start child list");
      children->GetLength(&count);
    }
    NS_ASSERTION(mStartOffset<=(PRInt32)count, "bad start offset");

    textNode = do_QueryInterface(mEndParent);
    if (textNode)
      textNode->GetLength(&count);
    else
    {
      nsCOMPtr<nsIDOMNodeList> children;
      result = mEndParent->GetChildNodes(getter_AddRefs(children));
      NS_ASSERTION(((NS_SUCCEEDED(result)) && children), "bad end child list");
      children->GetLength(&count);
    }
    NS_ASSERTION(mEndOffset<=(PRInt32)count, "bad end offset");

#ifdef NS_DEBUG
    if (gNoisy)
    {
      printf ("DeleteRange: %d of %p to %d of %p\n", 
               mStartOffset, (void *)mStartParent, mEndOffset, (void *)mEndParent);
    }         
#endif
  }
#endif
  return result;

}

NS_IMETHODIMP DeleteRangeTxn::DoTransaction(void)
{
#ifdef NS_DEBUG
  if (gNoisy) { printf("Do Delete Range\n"); }
#endif

  NS_ENSURE_TRUE(mStartParent && mEndParent && mCommonParent && mEditor, NS_ERROR_NOT_INITIALIZED);

  nsresult result; 
  // build the child transactions

  if (mStartParent==mEndParent)
  { // the selection begins and ends in the same node
    result = CreateTxnsToDeleteBetween(mStartParent, mStartOffset, mEndOffset);
  }
  else
  { // the selection ends in a different node from where it started
    // delete the relevant content in the start node
    result = CreateTxnsToDeleteContent(mStartParent, mStartOffset, nsIEditor::eNext);
    if (NS_SUCCEEDED(result))
    {
      // delete the intervening nodes
      result = CreateTxnsToDeleteNodesBetween();
      if (NS_SUCCEEDED(result))
      {
        // delete the relevant content in the end node
        result = CreateTxnsToDeleteContent(mEndParent, mEndOffset, nsIEditor::ePrevious);
      }
    }
  }

  // if we've successfully built this aggregate transaction, then do it.
  if (NS_SUCCEEDED(result)) {
    result = EditAggregateTxn::DoTransaction();
  }

  NS_ENSURE_SUCCESS(result, result);
  
  // only set selection to deletion point if editor gives permission
  bool bAdjustSelection;
  mEditor->ShouldTxnSetSelection(&bAdjustSelection);
  if (bAdjustSelection)
  {
    nsCOMPtr<nsISelection> selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    // At this point, it is possible that the frame for our root element
    // might have been destroyed, in which case, the above call returns
    // an error.  We eat that error here intentionally.  See bug 574558
    // for a sample case where this happens.
    NS_ENSURE_SUCCESS(result, NS_OK);
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    result = selection->Collapse(mStartParent, mStartOffset);
  }
  else
  {
    // do nothing - dom range gravity will adjust selection
  }

  return result;
}

NS_IMETHODIMP DeleteRangeTxn::UndoTransaction(void)
{
#ifdef NS_DEBUG
  if (gNoisy) { printf("Undo Delete Range\n"); }
#endif

  NS_ENSURE_TRUE(mStartParent && mEndParent && mCommonParent && mEditor, NS_ERROR_NOT_INITIALIZED);

  return EditAggregateTxn::UndoTransaction();
}

NS_IMETHODIMP DeleteRangeTxn::RedoTransaction(void)
{
#ifdef NS_DEBUG
  if (gNoisy) { printf("Redo Delete Range\n"); }
#endif

  NS_ENSURE_TRUE(mStartParent && mEndParent && mCommonParent && mEditor, NS_ERROR_NOT_INITIALIZED);

  return EditAggregateTxn::RedoTransaction();
}

NS_IMETHODIMP DeleteRangeTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("DeleteRangeTxn");
  return NS_OK;
}

NS_IMETHODIMP 
DeleteRangeTxn::CreateTxnsToDeleteBetween(nsIDOMNode *aStartParent, 
                                          PRUint32    aStartOffset, 
                                          PRUint32    aEndOffset)
{
  nsresult result = NS_OK;
  // see what kind of node we have
  nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(aStartParent);
  if (textNode)
  { // if the node is a text node, then delete text content
    nsRefPtr<DeleteTextTxn> txn = new DeleteTextTxn();
    NS_ENSURE_TRUE(txn, NS_ERROR_OUT_OF_MEMORY);

    PRInt32 numToDel;
    if (aStartOffset==aEndOffset)
      numToDel = 1;
    else
      numToDel = aEndOffset-aStartOffset;
    result = txn->Init(mEditor, textNode, aStartOffset, numToDel, mRangeUpdater);
    if (NS_SUCCEEDED(result))
      AppendChild(txn);
  }
  else
  {
    nsCOMPtr<nsIDOMNodeList> children;
    result = aStartParent->GetChildNodes(getter_AddRefs(children));
    NS_ENSURE_SUCCESS(result, result);
    NS_ENSURE_TRUE(children, NS_ERROR_NULL_POINTER);

#ifdef DEBUG
    PRUint32 childCount;
    children->GetLength(&childCount);
    NS_ASSERTION(aEndOffset<=childCount, "bad aEndOffset");
#endif
    PRUint32 i;
    for (i=aStartOffset; i<aEndOffset; i++)
    {
      nsCOMPtr<nsIDOMNode> child;
      result = children->Item(i, getter_AddRefs(child));
      NS_ENSURE_SUCCESS(result, result);
      NS_ENSURE_TRUE(child, NS_ERROR_NULL_POINTER);

      nsRefPtr<DeleteElementTxn> txn = new DeleteElementTxn();
      NS_ENSURE_TRUE(txn, NS_ERROR_OUT_OF_MEMORY);

      result = txn->Init(mEditor, child, mRangeUpdater);
      if (NS_SUCCEEDED(result))
        AppendChild(txn);
    }
  }
  return result;
}

NS_IMETHODIMP DeleteRangeTxn::CreateTxnsToDeleteContent(nsIDOMNode *aParent, 
                                                        PRUint32    aOffset, 
                                                        nsIEditor::EDirection aAction)
{
  nsresult result = NS_OK;
  // see what kind of node we have
  nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(aParent);
  if (textNode)
  { // if the node is a text node, then delete text content
    PRUint32 start, numToDelete;
    if (nsIEditor::eNext == aAction)
    {
      start=aOffset;
      textNode->GetLength(&numToDelete);
      numToDelete -= aOffset;
    }
    else
    {
      start=0;
      numToDelete=aOffset;
    }
    
    if (numToDelete)
    {
      nsRefPtr<DeleteTextTxn> txn = new DeleteTextTxn();
      NS_ENSURE_TRUE(txn, NS_ERROR_OUT_OF_MEMORY);

      result = txn->Init(mEditor, textNode, start, numToDelete, mRangeUpdater);
      if (NS_SUCCEEDED(result))
        AppendChild(txn);
    }
  }

  return result;
}

NS_IMETHODIMP DeleteRangeTxn::CreateTxnsToDeleteNodesBetween()
{
  nsCOMPtr<nsIContentIterator> iter = do_CreateInstance("@mozilla.org/content/subtree-content-iterator;1");
  NS_ENSURE_TRUE(iter, NS_ERROR_NULL_POINTER);

  nsresult result = iter->Init(mRange);
  NS_ENSURE_SUCCESS(result, result);

  while (!iter->IsDone() && NS_SUCCEEDED(result))
  {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(iter->GetCurrentNode());
    NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

    nsRefPtr<DeleteElementTxn> txn = new DeleteElementTxn();
    NS_ENSURE_TRUE(txn, NS_ERROR_OUT_OF_MEMORY);

    result = txn->Init(mEditor, node, mRangeUpdater);
    if (NS_SUCCEEDED(result))
      AppendChild(txn);
    iter->Next();
  }
  return result;
}

