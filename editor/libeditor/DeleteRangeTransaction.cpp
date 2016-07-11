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
#include "nsIDOMCharacterData.h"
#include "nsINode.h"
#include "nsAString.h"

namespace mozilla {

using namespace dom;

// note that aEditorBase is not refcounted
DeleteRangeTransaction::DeleteRangeTransaction()
  : mEditorBase(nullptr)
  , mRangeUpdater(nullptr)
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeleteRangeTransaction,
                                   EditAggregateTransaction,
                                   mRange)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteRangeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditAggregateTransaction)

nsresult
DeleteRangeTransaction::Init(EditorBase* aEditorBase,
                             nsRange* aRange,
                             RangeUpdater* aRangeUpdater)
{
  MOZ_ASSERT(aEditorBase && aRange);

  mEditorBase = aEditorBase;
  mRange = aRange->CloneRange();
  mRangeUpdater = aRangeUpdater;

  NS_ENSURE_TRUE(mEditorBase->IsModifiableNode(mRange->GetStartParent()),
                 NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mEditorBase->IsModifiableNode(mRange->GetEndParent()),
                 NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mEditorBase->IsModifiableNode(mRange->GetCommonAncestor()),
                 NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
DeleteRangeTransaction::DoTransaction()
{
  MOZ_ASSERT(mRange && mEditorBase);
  nsresult res;

  // build the child transactions
  nsCOMPtr<nsINode> startParent = mRange->GetStartParent();
  int32_t startOffset = mRange->StartOffset();
  nsCOMPtr<nsINode> endParent = mRange->GetEndParent();
  int32_t endOffset = mRange->EndOffset();
  MOZ_ASSERT(startParent && endParent);

  if (startParent == endParent) {
    // the selection begins and ends in the same node
    res = CreateTxnsToDeleteBetween(startParent, startOffset, endOffset);
    NS_ENSURE_SUCCESS(res, res);
  } else {
    // the selection ends in a different node from where it started.  delete
    // the relevant content in the start node
    res = CreateTxnsToDeleteContent(startParent, startOffset, nsIEditor::eNext);
    NS_ENSURE_SUCCESS(res, res);
    // delete the intervening nodes
    res = CreateTxnsToDeleteNodesBetween();
    NS_ENSURE_SUCCESS(res, res);
    // delete the relevant content in the end node
    res = CreateTxnsToDeleteContent(endParent, endOffset, nsIEditor::ePrevious);
    NS_ENSURE_SUCCESS(res, res);
  }

  // if we've successfully built this aggregate transaction, then do it.
  res = EditAggregateTransaction::DoTransaction();
  NS_ENSURE_SUCCESS(res, res);

  // only set selection to deletion point if editor gives permission
  bool bAdjustSelection;
  mEditorBase->ShouldTxnSetSelection(&bAdjustSelection);
  if (bAdjustSelection) {
    RefPtr<Selection> selection = mEditorBase->GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    res = selection->Collapse(startParent, startOffset);
    NS_ENSURE_SUCCESS(res, res);
  }
  // else do nothing - dom range gravity will adjust selection

  return NS_OK;
}

NS_IMETHODIMP
DeleteRangeTransaction::UndoTransaction()
{
  MOZ_ASSERT(mRange && mEditorBase);

  return EditAggregateTransaction::UndoTransaction();
}

NS_IMETHODIMP
DeleteRangeTransaction::RedoTransaction()
{
  MOZ_ASSERT(mRange && mEditorBase);

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

    RefPtr<DeleteTextTransaction> transaction =
      new DeleteTextTransaction(*mEditorBase, *charDataNode, aStartOffset,
                                numToDel, mRangeUpdater);

    nsresult res = transaction->Init();
    NS_ENSURE_SUCCESS(res, res);

    AppendChild(transaction);
    return NS_OK;
  }

  nsCOMPtr<nsIContent> child = aNode->GetChildAt(aStartOffset);
  NS_ENSURE_STATE(child);

  nsresult res = NS_OK;
  for (int32_t i = aStartOffset; i < aEndOffset; ++i) {
    RefPtr<DeleteNodeTransaction> transaction = new DeleteNodeTransaction();
    res = transaction->Init(mEditorBase, child, mRangeUpdater);
    if (NS_SUCCEEDED(res)) {
      AppendChild(transaction);
    }

    child = child->GetNextSibling();
  }

  NS_ENSURE_SUCCESS(res, res);
  return NS_OK;
}

nsresult
DeleteRangeTransaction::CreateTxnsToDeleteContent(nsINode* aNode,
                                                  int32_t aOffset,
                                                  nsIEditor::EDirection aAction)
{
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
      RefPtr<DeleteTextTransaction> transaction =
        new DeleteTextTransaction(*mEditorBase, *dataNode, start, numToDelete,
                                  mRangeUpdater);

      nsresult res = transaction->Init();
      NS_ENSURE_SUCCESS(res, res);

      AppendChild(transaction);
    }
  }

  return NS_OK;
}

nsresult
DeleteRangeTransaction::CreateTxnsToDeleteNodesBetween()
{
  nsCOMPtr<nsIContentIterator> iter = NS_NewContentSubtreeIterator();

  nsresult res = iter->Init(mRange);
  NS_ENSURE_SUCCESS(res, res);

  while (!iter->IsDone()) {
    nsCOMPtr<nsINode> node = iter->GetCurrentNode();
    NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

    RefPtr<DeleteNodeTransaction> transaction = new DeleteNodeTransaction();
    res = transaction->Init(mEditorBase, node, mRangeUpdater);
    NS_ENSURE_SUCCESS(res, res);
    AppendChild(transaction);

    iter->Next();
  }
  return NS_OK;
}

} // namespace mozilla
