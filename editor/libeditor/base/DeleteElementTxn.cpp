/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsReadableUtils.h"
#include "nsCRT.h"

#include "DeleteElementTxn.h"
#include "nsSelectionState.h"
#ifdef DEBUG
#include "nsIDOMElement.h"
#endif

#ifdef DEBUG
static bool gNoisy = false;
#endif


DeleteElementTxn::DeleteElementTxn()
: EditTxn()
,mElement()
,mParent()
,mRefNode()
,mRangeUpdater(nsnull)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(DeleteElementTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DeleteElementTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRefNode)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DeleteElementTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRefNode)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(DeleteElementTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(DeleteElementTxn, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteElementTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP DeleteElementTxn::Init(nsIEditor *aEditor,
                                     nsIDOMNode *aElement,
                                     nsRangeUpdater *aRangeUpdater)
{
  NS_ENSURE_TRUE(aEditor && aElement, NS_ERROR_NULL_POINTER);
  mEditor = aEditor;
  mElement = do_QueryInterface(aElement);
  nsresult result = mElement->GetParentNode(getter_AddRefs(mParent));
  if (NS_FAILED(result)) { return result; }

  // do nothing if the parent is read-only
  if (mParent && !mEditor->IsModifiableNode(mParent)) {
    return NS_ERROR_FAILURE;
  }

  mRangeUpdater = aRangeUpdater;
  return NS_OK;
}


NS_IMETHODIMP DeleteElementTxn::DoTransaction(void)
{
#ifdef DEBUG
  if (gNoisy)
  {
    printf("%p Do Delete Element element = %p\n",
           static_cast<void*>(this),
           static_cast<void*>(mElement.get()));
  }
#endif

  NS_ENSURE_TRUE(mElement, NS_ERROR_NOT_INITIALIZED);

  if (!mParent) { return NS_OK; }  // this is a no-op, there's no parent to delete mElement from

#ifdef DEBUG
  // begin debug output
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mElement);
  nsAutoString elementTag(NS_LITERAL_STRING("text node"));
  if (element)
    element->GetTagName(elementTag);
  nsCOMPtr<nsIDOMElement> parentElement = do_QueryInterface(mParent);
  nsAutoString parentElementTag(NS_LITERAL_STRING("text node"));
  if (parentElement)
    parentElement->GetTagName(parentElementTag);
  char *c, *p;
  c = ToNewCString(elementTag);
  p = ToNewCString(parentElementTag);
  if (c&&p)
  {
    if (gNoisy)
      printf("  DeleteElementTxn:  deleting child %s from parent %s\n", c, p); 

    NS_Free(c);
    NS_Free(p);
  }
  // end debug output
#endif

  // remember which child mElement was (by remembering which child was next)
  mElement->GetNextSibling(getter_AddRefs(mRefNode));  // can return null mRefNode

  // give range updater a chance.  SelAdjDeleteNode() needs to be called *before*
  // we do the action, unlike some of the other nsRangeStore update methods.
  if (mRangeUpdater) 
    mRangeUpdater->SelAdjDeleteNode(mElement);

  nsCOMPtr<nsIDOMNode> resultNode;
  return mParent->RemoveChild(mElement, getter_AddRefs(resultNode));
}

NS_IMETHODIMP DeleteElementTxn::UndoTransaction(void)
{
#ifdef DEBUG
  if (gNoisy)
  {
    printf("%p Undo Delete Element element = %p, parent = %p\n",
           static_cast<void*>(this),
           static_cast<void*>(mElement.get()),
           static_cast<void*>(mParent.get()));
  }
#endif

  if (!mParent) { return NS_OK; } // this is a legal state, the txn is a no-op
  if (!mElement) { return NS_ERROR_NULL_POINTER; }

#ifdef DEBUG
  // begin debug output
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mElement);
  nsAutoString elementTag(NS_LITERAL_STRING("text node"));
  if (element)
    element->GetTagName(elementTag);
  nsCOMPtr<nsIDOMElement> parentElement = do_QueryInterface(mParent);
  nsAutoString parentElementTag(NS_LITERAL_STRING("text node"));
  if (parentElement)
    parentElement->GetTagName(parentElementTag);
  char *c, *p;
  c = ToNewCString(elementTag);
  p = ToNewCString(parentElementTag);
  if (c&&p)
  {
    if (gNoisy)
      printf("  DeleteElementTxn:  inserting child %s back into parent %s\n", c, p); 

    NS_Free(c);
    NS_Free(p);
  }
  // end debug output
#endif

  nsCOMPtr<nsIDOMNode> resultNode;
  return mParent->InsertBefore(mElement, mRefNode, getter_AddRefs(resultNode));
}

NS_IMETHODIMP DeleteElementTxn::RedoTransaction(void)
{
#ifdef DEBUG
  if (gNoisy)
  {
    printf("%p Redo Delete Element element = %p, parent = %p\n",
           static_cast<void*>(this),
           static_cast<void*>(mElement.get()),
           static_cast<void*>(mParent.get()));
  }
#endif

  if (!mParent) { return NS_OK; } // this is a legal state, the txn is a no-op
  if (!mElement) { return NS_ERROR_NULL_POINTER; }

  if (mRangeUpdater) 
    mRangeUpdater->SelAdjDeleteNode(mElement);

  nsCOMPtr<nsIDOMNode> resultNode;
  return mParent->RemoveChild(mElement, getter_AddRefs(resultNode));
}

NS_IMETHODIMP DeleteElementTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("DeleteElementTxn");
  return NS_OK;
}
