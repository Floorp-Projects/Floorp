/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "DeleteRangeTxn.h"
#include "nsIDOMRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMSelection.h"
#include "DeleteTextTxn.h"
#include "DeleteElementTxn.h"
#include "TransactionFactory.h"
#include "nsIContentIterator.h"
#include "nsIContent.h"
#include "nsLayoutCID.h"

#ifdef NS_DEBUG
#include "nsIDOMElement.h"
#endif

static NS_DEFINE_IID(kDeleteTextTxnIID,     DELETE_TEXT_TXN_IID);
static NS_DEFINE_IID(kDeleteElementTxnIID,  DELETE_ELEMENT_TXN_IID);

#ifdef NS_DEBUG
static PRBool gNoisy = PR_TRUE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

// note that aEditor is not refcounted
DeleteRangeTxn::DeleteRangeTxn()
  : EditAggregateTxn()
{
}

NS_IMETHODIMP DeleteRangeTxn::Init(nsIEditor *aEditor, nsIDOMRange *aRange)
{
  if (aEditor && aRange)
  {
    mEditor = do_QueryInterface(aEditor);
    mRange  = do_QueryInterface(aRange);
    
    nsresult result = aRange->GetStartParent(getter_AddRefs(mStartParent));
    NS_ASSERTION((NS_SUCCEEDED(result)), "GetStartParent failed.");
    result = aRange->GetEndParent(getter_AddRefs(mEndParent));
    NS_ASSERTION((NS_SUCCEEDED(result)), "GetEndParent failed.");
    result = aRange->GetStartOffset(&mStartOffset);
    NS_ASSERTION((NS_SUCCEEDED(result)), "GetStartOffset failed.");
    result = aRange->GetEndOffset(&mEndOffset);
    NS_ASSERTION((NS_SUCCEEDED(result)), "GetEndOffset failed.");
    result = aRange->GetCommonParent(getter_AddRefs(mCommonParent));
    NS_ASSERTION((NS_SUCCEEDED(result)), "GetCommonParent failed.");

#ifdef NS_DEBUG
  {
    PRUint32 count;
    nsCOMPtr<nsIDOMCharacterData> textNode;
    textNode = do_QueryInterface(mStartParent);
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
    if (gNoisy)
    {
      printf ("DeleteRange: %d of %p to %d of %p\n", 
               mStartOffset, (void *)mStartParent, mEndOffset, (void *)mEndParent);
    }         
  }
#endif
    return result;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

DeleteRangeTxn::~DeleteRangeTxn()
{
}

NS_IMETHODIMP DeleteRangeTxn::Do(void)
{
  if (gNoisy) { printf("Do Delete Range\n"); }
  if (!mStartParent || !mEndParent || !mCommonParent)
    return NS_ERROR_NULL_POINTER;

  nsresult result; 

  // build the child transactions

  if (mStartParent==mEndParent)
  { // the selection begins and ends in the same node
    result = CreateTxnsToDeleteBetween(mStartParent, mStartOffset, mEndOffset);
  }
  else
  { // the selection ends in a different node from where it started
    // delete the relevant content in the start node
    result = CreateTxnsToDeleteContent(mStartParent, mStartOffset, nsIEditor::eLTR);
    if (NS_SUCCEEDED(result))
    {
      // delete the intervening nodes
      result = CreateTxnsToDeleteNodesBetween();
      if (NS_SUCCEEDED(result))
      {
        // delete the relevant content in the end node
        result = CreateTxnsToDeleteContent(mEndParent, mEndOffset, nsIEditor::eRTL);
      }
    }
  }

  // if we've successfully built this aggregate transaction, then do it.
  if (NS_SUCCEEDED(result)) {
    result = EditAggregateTxn::Do();
  }

  if (NS_SUCCEEDED(result)) {
    // set the resulting selection
    nsCOMPtr<nsIDOMSelection> selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(result)) {
      result = selection->Collapse(mStartParent, mStartOffset);
    }
  }

  return result;
}

NS_IMETHODIMP DeleteRangeTxn::Undo(void)
{
  if (gNoisy) { printf("Undo Delete Range\n"); }
  if (!mStartParent || !mEndParent || !mCommonParent)
    return NS_ERROR_NULL_POINTER;

  nsresult result = EditAggregateTxn::Undo();

  if (NS_SUCCEEDED(result)) {
    // set the resulting selection
    nsCOMPtr<nsIDOMSelection> selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(result)) {
      selection->Collapse(mStartParent, mStartOffset);
      selection->Extend(mEndParent, mEndOffset);
    }
  }

  return result;
}

NS_IMETHODIMP DeleteRangeTxn::Redo(void)
{
  if (gNoisy) { printf("Redo Delete Range\n"); }
  if (!mStartParent || !mEndParent || !mCommonParent)
    return NS_ERROR_NULL_POINTER;

  nsresult result = EditAggregateTxn::Redo();

  if (NS_SUCCEEDED(result)) {
    // set the resulting selection
    nsCOMPtr<nsIDOMSelection> selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(result)) {
      result = selection->Collapse(mStartParent, mStartOffset);
    }
  }

  return result;
}

NS_IMETHODIMP DeleteRangeTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP DeleteRangeTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP DeleteRangeTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Insert Range: ";
  }
  return NS_OK;
}

NS_IMETHODIMP DeleteRangeTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove Range: ";
  }
  return NS_OK;
}

NS_IMETHODIMP DeleteRangeTxn::CreateTxnsToDeleteBetween(nsIDOMNode *aStartParent, 
                                                   PRUint32    aStartOffset, 
                                                   PRUint32    aEndOffset)
{
  nsresult result;
  // see what kind of node we have
  nsCOMPtr<nsIDOMCharacterData> textNode;
  textNode =  do_QueryInterface(aStartParent);
  if (textNode)
  { // if the node is a text node, then delete text content
    DeleteTextTxn *txn;
    result = TransactionFactory::GetNewTransaction(kDeleteTextTxnIID, (EditTxn **)&txn);
    if (nsnull!=txn)
    {
      PRInt32 numToDel;
      if (aStartOffset==aEndOffset)
        numToDel = 1;
      else
        numToDel = aEndOffset-aStartOffset;
      txn->Init(mEditor, textNode, aStartOffset, numToDel);
      AppendChild(txn);
    }
  }
  else
  {
    PRUint32 childCount;
    nsCOMPtr<nsIDOMNodeList> children;
    result = aStartParent->GetChildNodes(getter_AddRefs(children));
    if ((NS_SUCCEEDED(result)) && children)
    {
      children->GetLength(&childCount);
      NS_ASSERTION(aEndOffset<=childCount, "bad aEndOffset");
      PRUint32 i;
      for (i=aStartOffset; i<=aEndOffset; i++)
      {
        nsCOMPtr<nsIDOMNode> child;
        result = children->Item(i, getter_AddRefs(child));
        if ((NS_SUCCEEDED(result)) && child)
        {
          DeleteElementTxn *txn;
          result = TransactionFactory::GetNewTransaction(kDeleteElementTxnIID, (EditTxn **)&txn);
          if (nsnull!=txn)
          {
            txn->Init(child);
            AppendChild(txn);
          }
          else
            return NS_ERROR_NULL_POINTER;
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP DeleteRangeTxn::CreateTxnsToDeleteContent(nsIDOMNode *aParent, 
                                                        PRUint32    aOffset, 
                                                        nsIEditor::Direction aDir)
{
  nsresult result;
  // see what kind of node we have
  nsCOMPtr<nsIDOMCharacterData> textNode;
  textNode = do_QueryInterface(aParent);
  if (textNode)
  { // if the node is a text node, then delete text content
    PRUint32 start, numToDelete;
    if (nsIEditor::eLTR==aDir)
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
      DeleteTextTxn *txn;
      result = TransactionFactory::GetNewTransaction(kDeleteTextTxnIID, (EditTxn **)&txn);
      if (nsnull!=txn)
      {
        txn->Init(mEditor, textNode, start, numToDelete);
        AppendChild(txn);
      }
      else
        return NS_ERROR_NULL_POINTER;
    }
  }

  return result;
}

static NS_DEFINE_IID(kSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);
static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

NS_IMETHODIMP DeleteRangeTxn::CreateTxnsToDeleteNodesBetween()
{
  nsresult result;

  nsCOMPtr<nsIContentIterator> iter;
  
  result = nsComponentManager::CreateInstance(kSubtreeIteratorCID,
                                        nsnull,
                                        nsIContentIterator::GetIID(), 
                                        getter_AddRefs(iter));
  if (!NS_SUCCEEDED(result))
    return result;
  result = iter->Init(mRange);
  if (!NS_SUCCEEDED(result))
    return result;
    
  while (NS_COMFALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsIContent> content;
    result = iter->CurrentNode(getter_AddRefs(content));
    node = do_QueryInterface(content);
    if ((NS_SUCCEEDED(result)) && node)
    {
      DeleteElementTxn *txn;
      result = TransactionFactory::GetNewTransaction(kDeleteElementTxnIID, (EditTxn **)&txn);
      if (nsnull!=txn)
      {
        txn->Init(node);
        AppendChild(txn);
      }
      else
        return NS_ERROR_NULL_POINTER;
    }
    iter->Next();
  }
  return result;
}

