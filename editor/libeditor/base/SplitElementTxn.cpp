/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>                      // for printf

#include "SplitElementTxn.h"
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsEditor.h"                   // for nsEditor
#include "nsError.h"                    // for NS_ERROR_NOT_INITIALIZED, etc
#include "nsIDOMCharacterData.h"        // for nsIDOMCharacterData
#include "nsIDOMNode.h"                 // for nsIDOMNode
#include "nsIEditor.h"                  // for nsEditor::DebugDumpContent, etc
#include "nsISelection.h"               // for nsISelection
#include "nsISupportsUtils.h"           // for NS_ADDREF

#ifdef DEBUG
static bool gNoisy = false;
#endif


// note that aEditor is not refcounted
SplitElementTxn::SplitElementTxn()
  : EditTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(SplitElementTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SplitElementTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNewLeftNode)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SplitElementTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mNewLeftNode)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(SplitElementTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(SplitElementTxn, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SplitElementTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP SplitElementTxn::Init(nsEditor   *aEditor,
                                    nsIDOMNode *aNode,
                                    PRInt32     aOffset)
{
  NS_ASSERTION(aEditor && aNode, "bad args");
  if (!aEditor || !aNode) { return NS_ERROR_NOT_INITIALIZED; }

  mEditor = aEditor;
  mExistingRightNode = do_QueryInterface(aNode);
  mOffset = aOffset;
  return NS_OK;
}

NS_IMETHODIMP SplitElementTxn::DoTransaction(void)
{
#ifdef DEBUG
  if (gNoisy)
  {
    printf("%p Do Split of node %p offset %d\n",
           static_cast<void*>(this),
           static_cast<void*>(mExistingRightNode.get()),
           mOffset);
  }
#endif

  NS_ASSERTION(mExistingRightNode && mEditor, "bad state");
  if (!mExistingRightNode || !mEditor) { return NS_ERROR_NOT_INITIALIZED; }

  // create a new node
  nsresult result = mExistingRightNode->CloneNode(false, 1, getter_AddRefs(mNewLeftNode));
  NS_ASSERTION(((NS_SUCCEEDED(result)) && (mNewLeftNode)), "could not create element.");
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(mNewLeftNode, NS_ERROR_NULL_POINTER);
  mEditor->MarkNodeDirty(mExistingRightNode);

#ifdef DEBUG
  if (gNoisy)
  {
    printf("  created left node = %p\n",
           static_cast<void*>(mNewLeftNode.get()));
  }
#endif

  // get the parent node
  result = mExistingRightNode->GetParentNode(getter_AddRefs(mParent));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(mParent, NS_ERROR_NULL_POINTER);

  // insert the new node
  result = mEditor->SplitNodeImpl(mExistingRightNode, mOffset, mNewLeftNode, mParent);
  if (mNewLeftNode) {
    bool bAdjustSelection;
    mEditor->ShouldTxnSetSelection(&bAdjustSelection);
    if (bAdjustSelection)
    {
      nsCOMPtr<nsISelection> selection;
      result = mEditor->GetSelection(getter_AddRefs(selection));
      NS_ENSURE_SUCCESS(result, result);
      NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
      result = selection->Collapse(mNewLeftNode, mOffset);
    }
    else
    {
      // do nothing - dom range gravity will adjust selection
    }
  }
  return result;
}

NS_IMETHODIMP SplitElementTxn::UndoTransaction(void)
{
#ifdef DEBUG
  if (gNoisy) { 
    printf("%p Undo Split of existing node %p and new node %p offset %d\n",
           static_cast<void*>(this),
           static_cast<void*>(mExistingRightNode.get()),
           static_cast<void*>(mNewLeftNode.get()),
           mOffset);
  }
#endif

  NS_ASSERTION(mEditor && mExistingRightNode && mNewLeftNode && mParent, "bad state");
  if (!mEditor || !mExistingRightNode || !mNewLeftNode || !mParent) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // this assumes Do inserted the new node in front of the prior existing node
  nsresult result = mEditor->JoinNodesImpl(mExistingRightNode, mNewLeftNode, mParent, false);
#ifdef DEBUG
  if (gNoisy) 
  { 
    printf("** after join left child node %p into right node %p\n",
           static_cast<void*>(mNewLeftNode.get()),
           static_cast<void*>(mExistingRightNode.get()));
    if (gNoisy) {mEditor->DebugDumpContent(); } // DEBUG
  }
  if (NS_SUCCEEDED(result))
  {
    if (gNoisy)
    {
      printf("  left node = %p removed\n",
             static_cast<void*>(mNewLeftNode.get()));
    }
  }
#endif

  return result;
}

/* redo cannot simply resplit the right node, because subsequent transactions
 * on the redo stack may depend on the left node existing in its previous state.
 */
NS_IMETHODIMP SplitElementTxn::RedoTransaction(void)
{
  NS_ASSERTION(mEditor && mExistingRightNode && mNewLeftNode && mParent, "bad state");
  if (!mEditor || !mExistingRightNode || !mNewLeftNode || !mParent) {
    return NS_ERROR_NOT_INITIALIZED;
  }

#ifdef DEBUG
  if (gNoisy) { 
    printf("%p Redo Split of existing node %p and new node %p offset %d\n",
           static_cast<void*>(this),
           static_cast<void*>(mExistingRightNode.get()),
           static_cast<void*>(mNewLeftNode.get()),
           mOffset);
    if (gNoisy) {mEditor->DebugDumpContent(); } // DEBUG
  }
#endif

  nsresult result;
  nsCOMPtr<nsIDOMNode>resultNode;
  // first, massage the existing node so it is in its post-split state
  nsCOMPtr<nsIDOMCharacterData>rightNodeAsText = do_QueryInterface(mExistingRightNode);
  if (rightNodeAsText)
  {
    result = rightNodeAsText->DeleteData(0, mOffset);
#ifdef DEBUG
    if (gNoisy) 
    { 
      printf("** after delete of text in right text node %p offset %d\n",
             static_cast<void*>(rightNodeAsText.get()),
             mOffset);
      mEditor->DebugDumpContent();  // DEBUG
    }
#endif
  }
  else
  {
    nsCOMPtr<nsIDOMNode>child;
    nsCOMPtr<nsIDOMNode>nextSibling;
    result = mExistingRightNode->GetFirstChild(getter_AddRefs(child));
    PRInt32 i;
    for (i=0; i<mOffset; i++)
    {
      if (NS_FAILED(result)) {return result;}
      if (!child) {return NS_ERROR_NULL_POINTER;}
      child->GetNextSibling(getter_AddRefs(nextSibling));
      result = mExistingRightNode->RemoveChild(child, getter_AddRefs(resultNode));
      if (NS_SUCCEEDED(result)) 
      {
        result = mNewLeftNode->AppendChild(child, getter_AddRefs(resultNode));
#ifdef DEBUG
        if (gNoisy) 
        { 
          printf("** move child node %p from right node %p to left node %p\n",
                 static_cast<void*>(child.get()),
                 static_cast<void*>(mExistingRightNode.get()),
                 static_cast<void*>(mNewLeftNode.get()));
          if (gNoisy) {mEditor->DebugDumpContent(); } // DEBUG
        }
#endif
      }
      child = do_QueryInterface(nextSibling);
    }
  }
  // second, re-insert the left node into the tree 
  result = mParent->InsertBefore(mNewLeftNode, mExistingRightNode, getter_AddRefs(resultNode));
#ifdef DEBUG
  if (gNoisy) 
  { 
    printf("** reinsert left child node %p before right node %p\n",
           static_cast<void*>(mNewLeftNode.get()),
           static_cast<void*>(mExistingRightNode.get()));
    if (gNoisy) {mEditor->DebugDumpContent(); } // DEBUG
  }
#endif
  return result;
}


NS_IMETHODIMP SplitElementTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("SplitElementTxn");
  return NS_OK;
}

NS_IMETHODIMP SplitElementTxn::GetNewNode(nsIDOMNode **aNewNode)
{
  NS_ENSURE_TRUE(aNewNode, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(mNewLeftNode, NS_ERROR_NOT_INITIALIZED);
  *aNewNode = mNewLeftNode;
  NS_ADDREF(*aNewNode);
  return NS_OK;
}
