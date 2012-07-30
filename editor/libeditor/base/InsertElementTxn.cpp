/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>                      // for printf

#include "InsertElementTxn.h"
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsError.h"                    // for NS_ERROR_NULL_POINTER, etc
#include "nsIContent.h"                 // for nsIContent
#include "nsIEditor.h"                  // for nsIEditor
#include "nsINode.h"                    // for nsINode
#include "nsISelection.h"               // for nsISelection
#include "nsMemory.h"                   // for nsMemory
#include "nsReadableUtils.h"            // for ToNewCString
#include "nsString.h"                   // for nsString

#ifdef DEBUG
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
#ifdef DEBUG
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

  nsCOMPtr<nsINode> parent = do_QueryInterface(mParent);
  NS_ENSURE_STATE(parent);

  PRUint32 count = parent->GetChildCount();
  if (mOffset > PRInt32(count) || mOffset == -1) {
    // -1 is sentinel value meaning "append at end"
    mOffset = count;
  }

  nsIContent* refContent = parent->GetChildAt(mOffset);
  // note, it's ok for refNode to be null.  that means append
  nsCOMPtr<nsIDOMNode> refNode = refContent ? refContent->AsDOMNode() : nullptr;

  mEditor->MarkNodeDirty(mNode);

  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->InsertBefore(mNode, refNode, getter_AddRefs(resultNode));
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
#ifdef DEBUG
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
