/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>                      // for printf

#include "InsertElementTxn.h"
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsEditor.h"                   // for nsEditor
#include "nsError.h"                    // for NS_ERROR_NULL_POINTER, etc
#include "nsIContent.h"                 // for nsIContent
#include "nsINode.h"                    // for nsINode
#include "nsISelection.h"               // for nsISelection
#include "nsMemory.h"                   // for nsMemory
#include "nsReadableUtils.h"            // for ToNewCString
#include "nsString.h"                   // for nsString

using namespace mozilla;

InsertElementTxn::InsertElementTxn()
  : EditTxn()
{
}

InsertElementTxn::~InsertElementTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(InsertElementTxn, EditTxn,
                                   mNode,
                                   mParent)

NS_IMPL_ADDREF_INHERITED(InsertElementTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(InsertElementTxn, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InsertElementTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP InsertElementTxn::Init(nsIDOMNode *aNode,
                                     nsIDOMNode *aParent,
                                     int32_t     aOffset,
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
  NS_ENSURE_TRUE(mNode && mParent, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsINode> parent = do_QueryInterface(mParent);
  NS_ENSURE_STATE(parent);

  uint32_t count = parent->GetChildCount();
  if (mOffset > int32_t(count) || mOffset == -1) {
    // -1 is sentinel value meaning "append at end"
    mOffset = count;
  }

  // note, it's ok for refContent to be null.  that means append
  nsCOMPtr<nsIContent> refContent = parent->GetChildAt(mOffset);
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
  NS_ENSURE_TRUE(mNode && mParent, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDOMNode> resultNode;
  return mParent->RemoveChild(mNode, getter_AddRefs(resultNode));
}

NS_IMETHODIMP InsertElementTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("InsertElementTxn");
  return NS_OK;
}
