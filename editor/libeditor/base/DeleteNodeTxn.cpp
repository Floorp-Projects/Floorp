/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteNodeTxn.h"
#include "nsSelectionState.h" // nsRangeUpdater
#include "nsEditor.h"


DeleteNodeTxn::DeleteNodeTxn()
  : EditTxn(), mNode(), mParent(), mRefNode(), mRangeUpdater(nsnull)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(DeleteNodeTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DeleteNodeTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRefNode)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DeleteNodeTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRefNode)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(DeleteNodeTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(DeleteNodeTxn, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteNodeTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

nsresult
DeleteNodeTxn::Init(nsEditor* aEditor, nsINode* aNode,
                    nsRangeUpdater* aRangeUpdater)
{
  NS_ENSURE_TRUE(aEditor && aNode, NS_ERROR_NULL_POINTER);
  mEditor = aEditor;
  mNode = aNode;
  mParent = aNode->GetNodeParent();

  // do nothing if the node has a parent and it's read-only
  NS_ENSURE_TRUE(!mParent || mEditor->IsModifiableNode(mParent),
                 NS_ERROR_FAILURE);

  mRangeUpdater = aRangeUpdater;
  return NS_OK;
}


NS_IMETHODIMP
DeleteNodeTxn::DoTransaction()
{
  NS_ENSURE_TRUE(mNode, NS_ERROR_NOT_INITIALIZED);

  if (!mParent) {
    // this is a no-op, there's no parent to delete mNode from
    return NS_OK;
  }

  // remember which child mNode was (by remembering which child was next);
  // mRefNode can be null
  mRefNode = mNode->GetNextSibling();

  // give range updater a chance.  SelAdjDeleteNode() needs to be called
  // *before* we do the action, unlike some of the other nsRangeStore update
  // methods.
  if (mRangeUpdater) {
    mRangeUpdater->SelAdjDeleteNode(mNode->AsDOMNode());
  }

  return mParent->RemoveChild(mNode);
}

NS_IMETHODIMP
DeleteNodeTxn::UndoTransaction()
{
  if (!mParent) {
    // this is a legal state, the txn is a no-op
    return NS_OK;
  }
  if (!mNode) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult res;
  mParent->InsertBefore(mNode, mRefNode, &res);
  return res;
}

NS_IMETHODIMP
DeleteNodeTxn::RedoTransaction()
{
  if (!mParent) {
    // this is a legal state, the txn is a no-op
    return NS_OK;
  }
  if (!mNode) {
    return NS_ERROR_NULL_POINTER;
  }

  if (mRangeUpdater) {
    mRangeUpdater->SelAdjDeleteNode(mNode->AsDOMNode());
  }

  return mParent->RemoveChild(mNode);
}

NS_IMETHODIMP
DeleteNodeTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("DeleteNodeTxn");
  return NS_OK;
}
