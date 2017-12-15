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
#include "mozilla/RangeBoundary.h"
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
  const RangeBoundary& startRef = rangeToDelete->StartRef();
  const RangeBoundary& endRef = rangeToDelete->EndRef();
  MOZ_ASSERT(startRef.IsSetAndValid());
  MOZ_ASSERT(endRef.IsSetAndValid());

  if (startRef.Container() == endRef.Container()) {
    // the selection begins and ends in the same node
    nsresult rv = CreateTxnsToDeleteBetween(startRef.AsRaw(), endRef.AsRaw());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    // the selection ends in a different node from where it started.  delete
    // the relevant content in the start node
    nsresult rv = CreateTxnsToDeleteContent(startRef.AsRaw(), nsIEditor::eNext);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // delete the intervening nodes
    rv = CreateTxnsToDeleteNodesBetween(rangeToDelete);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // delete the relevant content in the end node
    rv = CreateTxnsToDeleteContent(endRef.AsRaw(), nsIEditor::ePrevious);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // if we've successfully built this aggregate transaction, then do it.
  nsresult rv = EditAggregateTransaction::DoTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // only set selection to deletion point if editor gives permission
  bool bAdjustSelection;
  mEditorBase->ShouldTxnSetSelection(&bAdjustSelection);
  if (bAdjustSelection) {
    RefPtr<Selection> selection = mEditorBase->GetSelection();
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_NULL_POINTER;
    }
    rv = selection->Collapse(startRef.AsRaw());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
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
DeleteRangeTransaction::CreateTxnsToDeleteBetween(
                          const RawRangeBoundary& aStart,
                          const RawRangeBoundary& aEnd)
{
  if (NS_WARN_IF(!aStart.IsSetAndValid()) ||
      NS_WARN_IF(!aEnd.IsSetAndValid()) ||
      NS_WARN_IF(aStart.Container() != aEnd.Container())) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // see what kind of node we have
  if (aStart.Container()->IsNodeOfType(nsINode::eDATA_NODE)) {
    // if the node is a chardata node, then delete chardata content
    int32_t numToDel;
    if (aStart == aEnd) {
      numToDel = 1;
    } else {
      numToDel = aEnd.Offset() - aStart.Offset();
      MOZ_DIAGNOSTIC_ASSERT(numToDel > 0);
    }

    RefPtr<nsGenericDOMDataNode> charDataNode =
      static_cast<nsGenericDOMDataNode*>(aStart.Container());

    RefPtr<DeleteTextTransaction> deleteTextTransaction =
      DeleteTextTransaction::MaybeCreate(*mEditorBase, *charDataNode,
                                         aStart.Offset(), numToDel);
    // If the text node isn't editable, it should be never undone/redone.
    // So, the transaction shouldn't be recorded.
    if (NS_WARN_IF(!deleteTextTransaction)) {
      return NS_ERROR_FAILURE;
    }
    AppendChild(deleteTextTransaction);
    return NS_OK;
  }

  // Even if we detect invalid range, we should ignore it for removing
  // specified range's nodes as far as possible.
  for (nsIContent* child = aStart.GetChildAtOffset();
       child && child != aEnd.GetChildAtOffset();
       child = child->GetNextSibling()) {
    RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
      DeleteNodeTransaction::MaybeCreate(*mEditorBase, *child);
    // XXX This is odd handling.  Even if some children are not editable,
    //     editor should append transactions because they could be editable
    //     at undoing/redoing.  Additionally, if the transaction needs to
    //     delete/restore all nodes, it should at undoing/redoing.
    if (deleteNodeTransaction) {
      AppendChild(deleteNodeTransaction);
    }
  }

  return NS_OK;
}

nsresult
DeleteRangeTransaction::CreateTxnsToDeleteContent(
                          const RawRangeBoundary& aPoint,
                          nsIEditor::EDirection aAction)
{
  if (NS_WARN_IF(!aPoint.IsSetAndValid())) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!aPoint.Container()->IsNodeOfType(nsINode::eDATA_NODE)) {
    return NS_OK;
  }

  // If the node is a chardata node, then delete chardata content
  uint32_t startOffset, numToDelete;
  if (nsIEditor::eNext == aAction) {
    startOffset = aPoint.Offset();
    numToDelete = aPoint.Container()->Length() - aPoint.Offset();
  } else {
    startOffset = 0;
    numToDelete = aPoint.Offset();
  }

  if (!numToDelete) {
    return NS_OK;
  }

  RefPtr<nsGenericDOMDataNode> dataNode =
    static_cast<nsGenericDOMDataNode*>(aPoint.Container());
  RefPtr<DeleteTextTransaction> deleteTextTransaction =
    DeleteTextTransaction::MaybeCreate(*mEditorBase, *dataNode,
                                       startOffset, numToDelete);
  // If the text node isn't editable, it should be never undone/redone.
  // So, the transaction shouldn't be recorded.
  if (NS_WARN_IF(!deleteTextTransaction)) {
    return NS_ERROR_FAILURE;
  }
  AppendChild(deleteTextTransaction);

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
      DeleteNodeTransaction::MaybeCreate(*mEditorBase, *node);
    // XXX This is odd handling.  Even if some nodes in the range are not
    //     editable, editor should append transactions because they could
    //     at undoing/redoing.  Additionally, if the transaction needs to
    //     delete/restore all nodes, it should at undoing/redoing.
    if (NS_WARN_IF(!deleteNodeTransaction)) {
      return NS_ERROR_FAILURE;
    }
    AppendChild(deleteNodeTransaction);

    iter->Next();
  }
  return NS_OK;
}

} // namespace mozilla
