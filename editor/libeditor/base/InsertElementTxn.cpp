/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InsertElementTxn.h"
#include "nsISelection.h"
#include "nsIContent.h"
#include "nsIDOMNodeList.h"
#include "nsReadableUtils.h"

#ifdef NS_DEBUG
static bool gNoisy = false;
#endif


InsertElementTxn::InsertElementTxn()
  : EditTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(InsertElementTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(InsertElementTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(InsertElementTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(InsertElementTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(InsertElementTxn, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InsertElementTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP InsertElementTxn::Init(nsIDOMNode *aNode,
                                     nsIDOMNode *aParent,
                                     PRInt32     aOffset,
                                     nsIEditor  *aEditor)
{
  NS_ASSERTION(aNode && aParent && aEditor, "bad arg");
  NS_ENSURE_TRUE(aNode && aParent && aEditor, NS_ERROR_NULL_POINTER);

  mNode = do_QueryInterface(aNode);
  mParent = do_QueryInterface(aParent);
  mOffset = aOffset;
  mEditor = aEditor;
  NS_ENSURE_TRUE(mNode && mParent && mEditor, NS_ERROR_INVALID_ARG);
  return NS_OK;
}


NS_IMETHODIMP InsertElementTxn::DoTransaction(void)
{
#ifdef NS_DEBUG
  if (gNoisy) 
  { 
    nsCOMPtr<nsIContent>nodeAsContent = do_QueryInterface(mNode);
    nsCOMPtr<nsIContent>parentAsContent = do_QueryInterface(mParent);
    nsString namestr;
    mNode->GetNodeName(namestr);
    char* nodename = ToNewCString(namestr);
    printf("%p Do Insert Element of %p <%s> into parent %p at offset %d\n", 
           static_cast<void*>(this),
           static_cast<void*>(nodeAsContent.get()),
           nodename,
           static_cast<void*>(parentAsContent.get()),
           mOffset); 
    nsMemory::Free(nodename);
  }
#endif

  NS_ENSURE_TRUE(mNode && mParent, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDOMNodeList> childNodes;
  nsresult result = mParent->GetChildNodes(getter_AddRefs(childNodes));
  NS_ENSURE_SUCCESS(result, result);
  nsCOMPtr<nsIDOMNode>refNode;
  if (childNodes)
  {
    PRUint32 count;
    childNodes->GetLength(&count);
    if (mOffset>(PRInt32)count) mOffset = count;
    // -1 is sentinel value meaning "append at end"
    if (mOffset == -1) mOffset = count;
    result = childNodes->Item(mOffset, getter_AddRefs(refNode));
    NS_ENSURE_SUCCESS(result, result); 
    // note, it's ok for mRefNode to be null.  that means append
  }

  mEditor->MarkNodeDirty(mNode);

  nsCOMPtr<nsIDOMNode> resultNode;
  result = mParent->InsertBefore(mNode, refNode, getter_AddRefs(resultNode));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(resultNode, NS_ERROR_NULL_POINTER);

  // only set selection to insertion point if editor gives permission
  bool bAdjustSelection;
  mEditor->ShouldTxnSetSelection(&bAdjustSelection);
  if (bAdjustSelection)
  {
    nsCOMPtr<nsISelection> selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(result, result);
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    // place the selection just after the inserted element
    selection->Collapse(mParent, mOffset+1);
  }
  else
  {
    // do nothing - dom range gravity will adjust selection
  }
  return result;
}

NS_IMETHODIMP InsertElementTxn::UndoTransaction(void)
{
#ifdef NS_DEBUG
  if (gNoisy)
  {
    printf("%p Undo Insert Element of %p into parent %p at offset %d\n",
           static_cast<void*>(this),
           static_cast<void*>(mNode.get()),
           static_cast<void*>(mParent.get()),
           mOffset);
  }
#endif

  NS_ENSURE_TRUE(mNode && mParent, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDOMNode> resultNode;
  return mParent->RemoveChild(mNode, getter_AddRefs(resultNode));
}

NS_IMETHODIMP InsertElementTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("InsertElementTxn");
  return NS_OK;
}
