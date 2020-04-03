/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteNodeTransaction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/SelectionState.h"  // RangeUpdater
#include "nsDebug.h"
#include "nsError.h"
#include "nsAString.h"

namespace mozilla {

// static
already_AddRefed<DeleteNodeTransaction> DeleteNodeTransaction::MaybeCreate(
    EditorBase& aEditorBase, nsIContent& aContentToDelete) {
  RefPtr<DeleteNodeTransaction> transaction =
      new DeleteNodeTransaction(aEditorBase, aContentToDelete);
  if (NS_WARN_IF(!transaction->CanDoIt())) {
    return nullptr;
  }
  return transaction.forget();
}

DeleteNodeTransaction::DeleteNodeTransaction(EditorBase& aEditorBase,
                                             nsIContent& aContentToDelete)
    : mEditorBase(&aEditorBase),
      mContentToDelete(&aContentToDelete),
      mParentNode(aContentToDelete.GetParentNode()) {}

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeleteNodeTransaction, EditTransactionBase,
                                   mEditorBase, mContentToDelete, mParentNode,
                                   mRefContent)

NS_IMPL_ADDREF_INHERITED(DeleteNodeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(DeleteNodeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

bool DeleteNodeTransaction::CanDoIt() const {
  if (NS_WARN_IF(!mContentToDelete) || NS_WARN_IF(!mEditorBase) ||
      !mParentNode || !mEditorBase->IsModifiableNode(*mParentNode)) {
    return false;
  }
  return true;
}

NS_IMETHODIMP DeleteNodeTransaction::DoTransaction() {
  if (NS_WARN_IF(!CanDoIt())) {
    return NS_OK;
  }

  if (!mEditorBase->AsHTMLEditor() && mContentToDelete->IsText()) {
    uint32_t length = mContentToDelete->AsText()->TextLength();
    if (length > 0) {
      mEditorBase->AsTextEditor()->WillDeleteText(length, 0, length);
    }
  }

  // Remember which child mContentToDelete was (by remembering which child was
  // next).  Note that mRefContent can be nullptr.
  mRefContent = mContentToDelete->GetNextSibling();

  // give range updater a chance.  SelAdjDeleteNode() needs to be called
  // *before* we do the action, unlike some of the other RangeItem update
  // methods.
  mEditorBase->RangeUpdaterRef().SelAdjDeleteNode(*mContentToDelete);

  OwningNonNull<nsINode> parentNode = *mParentNode;
  OwningNonNull<nsIContent> contentToDelete = *mContentToDelete;
  ErrorResult error;
  parentNode->RemoveChild(contentToDelete, error);
  NS_WARNING_ASSERTION(!error.Failed(), "nsINode::RemoveChild() failed");
  return error.StealNSResult();
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
DeleteNodeTransaction::UndoTransaction() {
  if (NS_WARN_IF(!CanDoIt())) {
    // This is a legal state, the transaction is a no-op.
    return NS_OK;
  }
  ErrorResult error;
  OwningNonNull<EditorBase> editorBase = *mEditorBase;
  OwningNonNull<nsINode> parentNode = *mParentNode;
  OwningNonNull<nsIContent> contentToDelete = *mContentToDelete;
  nsCOMPtr<nsIContent> refContent = mRefContent;
  // XXX Perhaps, we should check `refContent` is a child of `parentNode`,
  //     and if it's not, we should stop undoing or something.
  parentNode->InsertBefore(contentToDelete, refContent, error);
  if (error.Failed()) {
    NS_WARNING("nsINode::InsertBefore() failed");
    return error.StealNSResult();
  }
  if (!editorBase->AsHTMLEditor() && contentToDelete->IsText()) {
    uint32_t length = contentToDelete->AsText()->TextLength();
    if (length > 0) {
      nsresult rv = MOZ_KnownLive(editorBase->AsTextEditor())
                        ->DidInsertText(length, 0, length);
      if (NS_FAILED(rv)) {
        NS_WARNING("TextEditor::DidInsertText() failed");
        return rv;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP DeleteNodeTransaction::RedoTransaction() {
  if (NS_WARN_IF(!CanDoIt())) {
    // This is a legal state, the transaction is a no-op.
    return NS_OK;
  }

  if (!mEditorBase->AsHTMLEditor() && mContentToDelete->IsText()) {
    uint32_t length = mContentToDelete->AsText()->TextLength();
    if (length > 0) {
      mEditorBase->AsTextEditor()->WillDeleteText(length, 0, length);
    }
  }

  mEditorBase->RangeUpdaterRef().SelAdjDeleteNode(*mContentToDelete);

  OwningNonNull<nsINode> parentNode = *mParentNode;
  OwningNonNull<nsIContent> contentToDelete = *mContentToDelete;
  ErrorResult error;
  parentNode->RemoveChild(contentToDelete, error);
  NS_WARNING_ASSERTION(!error.Failed(), "nsINode::RemoveChild() failed");
  return error.StealNSResult();
}

}  // namespace mozilla
