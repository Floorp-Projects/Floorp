/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteRangeTransaction.h"

#include "DeleteNodeTransaction.h"
#include "DeleteTextTransaction.h"
#include "mozilla/Assertions.h"
#include "mozilla/EditorBase.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/mozalloc.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsINode.h"
#include "nsAString.h"

namespace mozilla {

using namespace dom;

// note that aEditorBase is not refcounted
DeleteRangeTransaction::DeleteRangeTransaction(EditorBase& aEditorBase,
                                               nsRange& aRangeToDelete,
                                               RangeUpdater* aRangeUpdater)
  : mEditorBase(&aEditorBase)
  , mRangeToDelete(aRangeToDelete.CloneRange())
  , mRangeUpdater(aRangeUpdater)
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeleteRangeTransaction,
                                   EditAggregateTransaction,
                                   mEditorBase,
                                   mRangeToDelete)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteRangeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditAggregateTransaction)

NS_IMETHODIMP
DeleteRangeTransaction::DoTransaction()
{
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mRangeToDelete)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Swap mRangeToDelete out into a stack variable, so we make sure to null it
  // out on return from this function.  Once this function returns, we no longer
  // need mRangeToDelete, and keeping it alive in the long term slows down all
  // DOM mutations because it's observing them.
  RefPtr<nsRange> rangeToDelete;
  rangeToDelete.swap(mRangeToDelete);

  // build the child transactions
  nsCOMPtr<nsINode> startContainer = rangeToDelete->GetStartContainer();
  int32_t startOffset = rangeToDelete->StartOffset();
  nsCOMPtr<nsINode> endContainer = rangeToDelete->GetEndContainer();
  int32_t endOffset = rangeToDelete->EndOffset();
  MOZ_ASSERT(startContainer && endContainer);

  if (startContainer == endContainer) {
    // the selection begins and ends in the same node
    nsresult rv =
      CreateTxnsToDeleteBetween(startContainer, startOffset, endOffset);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // the selection ends in a different node from where it started.  delete
    // the relevant content in the start node
    nsresult rv =
      CreateTxnsToDeleteContent(startContainer, startOffset, nsIEditor::eNext);
    NS_ENSURE_SUCCESS(rv, rv);
    // delete the intervening nodes
    rv = CreateTxnsToDeleteNodesBetween(rangeToDelete);
    NS_ENSURE_SUCCESS(rv, rv);
    // delete the relevant content in the end node
    rv = CreateTxnsToDeleteContent(endContainer, endOffset,
                                   nsIEditor::ePrevious);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // if we've successfully built this aggregate transaction, then do it.
  nsresult rv = EditAggregateTransaction::DoTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  // only set selection to deletion point if editor gives permission
  bool bAdjustSelection;
  mEditorBase->ShouldTxnSetSelection(&bAdjustSelection);
  if (bAdjustSelection) {
    RefPtr<Selection> selection = mEditorBase->GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    rv = selection->Collapse(startContainer, startOffset);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // else do nothing - dom range gravity will adjust selection

  return NS_OK;
}

NS_IMETHODIMP
DeleteRangeTransaction::UndoTransaction()
{
  return EditAggregateTransaction::UndoTransaction();
}

NS_IMETHODIMP
DeleteRangeTransaction::RedoTransaction()
{
  return EditAggregateTransaction::RedoTransaction();
}

NS_IMETHODIMP
DeleteRangeTransaction::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("DeleteRangeTransaction");
  return NS_OK;
}

nsresult
DeleteRangeTransaction::CreateTxnsToDeleteBetween(nsINode* aNode,
                                                  int32_t aStartOffset,
                                                  int32_t aEndOffset)
{
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // see what kind of node we have
  if (aNode->IsNodeOfType(nsINode::eDATA_NODE)) {
    // if the node is a chardata node, then delete chardata content
    int32_t numToDel;
    if (aStartOffset == aEndOffset) {
      numToDel = 1;
    } else {
      numToDel = aEndOffset - aStartOffset;
    }

    RefPtr<nsGenericDOMDataNode> charDataNode =
      static_cast<nsGenericDOMDataNode*>(aNode);

    RefPtr<DeleteTextTransaction> deleteTextTransaction =
      new DeleteTextTransaction(*mEditorBase, *charDataNode, aStartOffset,
                                numToDel, mRangeUpdater);
    // If the text node isn't editable, it should be never undone/redone.
    // So, the transaction shouldn't be recorded.
    if (NS_WARN_IF(!deleteTextTransaction->CanDoIt())) {
      return NS_ERROR_FAILURE;
    }
    AppendChild(deleteTextTransaction);
    return NS_OK;
  }

  nsCOMPtr<nsIContent> child = aNode->GetChildAt(aStartOffset);
  for (int32_t i = aStartOffset; i < aEndOffset; ++i) {
    // Even if we detect invalid range, we should ignore it for removing
    // specified range's nodes as far as possible.
    if (NS_WARN_IF(!child)) {
      break;
    }
    RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
      new DeleteNodeTransaction(*mEditorBase, *child, mRangeUpdater);
    // XXX This is odd handling.  Even if some children are not editable,
    //     editor should append transactions because they could be editable
    //     at undoing/redoing.  Additionally, if the transaction needs to
    //     delete/restore all nodes, it should at undoing/redoing.
    if (deleteNodeTransaction->CanDoIt()) {
      AppendChild(deleteNodeTransaction);
    }
    child = child->GetNextSibling();
  }

  return NS_OK;
}

nsresult
DeleteRangeTransaction::CreateTxnsToDeleteContent(nsINode* aNode,
                                                  int32_t aOffset,
                                                  nsIEditor::EDirection aAction)
{
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // see what kind of node we have
  if (aNode->IsNodeOfType(nsINode::eDATA_NODE)) {
    // if the node is a chardata node, then delete chardata content
    uint32_t start, numToDelete;
    if (nsIEditor::eNext == aAction) {
      start = aOffset;
      numToDelete = aNode->Length() - aOffset;
    } else {
      start = 0;
      numToDelete = aOffset;
    }

    if (numToDelete) {
      RefPtr<nsGenericDOMDataNode> dataNode =
        static_cast<nsGenericDOMDataNode*>(aNode);
      RefPtr<DeleteTextTransaction> deleteTextTransaction =
        new DeleteTextTransaction(*mEditorBase, *dataNode, start, numToDelete,
                                  mRangeUpdater);
      // If the text node isn't editable, it should be never undone/redone.
      // So, the transaction shouldn't be recorded.
      if (NS_WARN_IF(!deleteTextTransaction->CanDoIt())) {
        return NS_ERROR_FAILURE;
      }
      AppendChild(deleteTextTransaction);
    }
  }

  return NS_OK;
}

nsresult
DeleteRangeTransaction::CreateTxnsToDeleteNodesBetween(nsRange* aRangeToDelete)
{
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIContentIterator> iter = NS_NewContentSubtreeIterator();

  nsresult rv = iter->Init(aRangeToDelete);
  NS_ENSURE_SUCCESS(rv, rv);

  while (!iter->IsDone()) {
    nsCOMPtr<nsINode> node = iter->GetCurrentNode();
    if (NS_WARN_IF(!node)) {
      return NS_ERROR_NULL_POINTER;
    }

    RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
      new DeleteNodeTransaction(*mEditorBase, *node, mRangeUpdater);
    // XXX This is odd handling.  Even if some nodes in the range are not
    //     editable, editor should append transactions because they could
    //     at undoing/redoing.  Additionally, if the transaction needs to
    //     delete/restore all nodes, it should at undoing/redoing.
    if (NS_WARN_IF(!deleteNodeTransaction->CanDoIt())) {
      return NS_ERROR_FAILURE;
    }
    AppendChild(deleteNodeTransaction);

    iter->Next();
  }
  return NS_OK;
}

} // namespace mozilla
