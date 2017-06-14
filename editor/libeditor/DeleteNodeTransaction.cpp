/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteNodeTransaction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/SelectionState.h" // RangeUpdater
#include "nsDebug.h"
#include "nsError.h"
#include "nsAString.h"

namespace mozilla {

DeleteNodeTransaction::DeleteNodeTransaction(EditorBase& aEditorBase,
                                             nsINode& aNodeToDelete,
                                             RangeUpdater* aRangeUpdater)
  : mEditorBase(&aEditorBase)
  , mNodeToDelete(&aNodeToDelete)
  , mParentNode(aNodeToDelete.GetParentNode())
  , mRangeUpdater(aRangeUpdater)
{
  // XXX We're not sure if this is really necessary.
  if (!CanDoIt()) {
    mRangeUpdater = nullptr;
  }
}

DeleteNodeTransaction::~DeleteNodeTransaction()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeleteNodeTransaction, EditTransactionBase,
                                   mEditorBase,
                                   mNodeToDelete,
                                   mParentNode,
                                   mRefNode)

NS_IMPL_ADDREF_INHERITED(DeleteNodeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(DeleteNodeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

bool
DeleteNodeTransaction::CanDoIt() const
{
  if (NS_WARN_IF(!mNodeToDelete) || NS_WARN_IF(!mEditorBase) ||
      !mParentNode || !mEditorBase->IsModifiableNode(mParentNode)) {
    return false;
  }
  return true;
}

NS_IMETHODIMP
DeleteNodeTransaction::DoTransaction()
{
  if (NS_WARN_IF(!CanDoIt())) {
    return NS_OK;
  }

  // Remember which child mNodeToDelete was (by remembering which child was
  // next).  Note that mRefNode can be nullptr.
  mRefNode = mNodeToDelete->GetNextSibling();

  // give range updater a chance.  SelAdjDeleteNode() needs to be called
  // *before* we do the action, unlike some of the other RangeItem update
  // methods.
  if (mRangeUpdater) {
    mRangeUpdater->SelAdjDeleteNode(mNodeToDelete);
  }

  ErrorResult error;
  mParentNode->RemoveChild(*mNodeToDelete, error);
  return error.StealNSResult();
}

NS_IMETHODIMP
DeleteNodeTransaction::UndoTransaction()
{
  if (NS_WARN_IF(!CanDoIt())) {
    // This is a legal state, the transaction is a no-op.
    return NS_OK;
  }
  ErrorResult error;
  nsCOMPtr<nsIContent> refNode = mRefNode;
  mParentNode->InsertBefore(*mNodeToDelete, refNode, error);
  return error.StealNSResult();
}

NS_IMETHODIMP
DeleteNodeTransaction::RedoTransaction()
{
  if (NS_WARN_IF(!CanDoIt())) {
    // This is a legal state, the transaction is a no-op.
    return NS_OK;
  }

  if (mRangeUpdater) {
    mRangeUpdater->SelAdjDeleteNode(mNodeToDelete);
  }

  ErrorResult error;
  mParentNode->RemoveChild(*mNodeToDelete, error);
  return error.StealNSResult();
}

NS_IMETHODIMP
DeleteNodeTransaction::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("DeleteNodeTransaction");
  return NS_OK;
}

} // namespace mozilla
