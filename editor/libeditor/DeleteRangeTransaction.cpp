/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteRangeTransaction.h"

#include "DeleteNodeTransaction.h"
#include "DeleteTextTransaction.h"
#include "EditorUtils.h"
#include "HTMLEditUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditorBase.h"
#include "mozilla/Logging.h"
#include "mozilla/mozalloc.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/ToString.h"
#include "mozilla/dom/Selection.h"

#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsAString.h"

namespace mozilla {

using namespace dom;

using EditorType = EditorUtils::EditorType;

DeleteRangeTransaction::DeleteRangeTransaction(EditorBase& aEditorBase,
                                               const nsRange& aRangeToDelete)
    : mEditorBase(&aEditorBase), mRangeToDelete(aRangeToDelete.CloneRange()) {}

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeleteRangeTransaction,
                                   EditAggregateTransaction, mEditorBase,
                                   mRangeToDelete)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteRangeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditAggregateTransaction)

NS_IMETHODIMP DeleteRangeTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteRangeTransaction::%s this={ mName=%s } "
           "Start==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));

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
    if (NS_FAILED(rv)) {
      NS_WARNING("DeleteRangeTransaction::CreateTxnsToDeleteBetween() failed");
      return rv;
    }
  } else {
    // the selection ends in a different node from where it started.  delete
    // the relevant content in the start node
    nsresult rv = CreateTxnsToDeleteContent(startRef.AsRaw(), nsIEditor::eNext);
    if (NS_FAILED(rv)) {
      NS_WARNING("DeleteRangeTransaction::CreateTxnsToDeleteContent() failed");
      return rv;
    }
    // delete the intervening nodes
    rv = CreateTxnsToDeleteNodesBetween(rangeToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "DeleteRangeTransaction::CreateTxnsToDeleteNodesBetween() failed");
      return rv;
    }
    // delete the relevant content in the end node
    rv = CreateTxnsToDeleteContent(endRef.AsRaw(), nsIEditor::ePrevious);
    if (NS_FAILED(rv)) {
      NS_WARNING("DeleteRangeTransaction::CreateTxnsToDeleteContent() failed");
      return rv;
    }
  }

  // if we've successfully built this aggregate transaction, then do it.
  nsresult rv = EditAggregateTransaction::DoTransaction();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditAggregateTransaction::DoTransaction() failed");
    return rv;
  }

  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteRangeTransaction::%s this={ mName=%s } "
           "End==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));

  if (!mEditorBase->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  RefPtr<Selection> selection = mEditorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  rv = selection->CollapseInLimiter(startRef.AsRaw());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Selection::CollapseInLimiter() failed");
  return rv;
}

NS_IMETHODIMP DeleteRangeTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteRangeTransaction::%s this={ mName=%s } "
           "Start==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));

  nsresult rv = EditAggregateTransaction::UndoTransaction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditAggregateTransaction::UndoTransaction() failed");

  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteRangeTransaction::%s this={ mName=%s } "
           "End==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
  return rv;
}

NS_IMETHODIMP DeleteRangeTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteRangeTransaction::%s this={ mName=%s } "
           "Start==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));

  nsresult rv = EditAggregateTransaction::RedoTransaction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditAggregateTransaction::RedoTransaction() failed");

  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteRangeTransaction::%s this={ mName=%s } "
           "End==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
  return rv;
}

nsresult DeleteRangeTransaction::CreateTxnsToDeleteBetween(
    const RawRangeBoundary& aStart, const RawRangeBoundary& aEnd) {
  if (NS_WARN_IF(!aStart.IsSetAndValid()) ||
      NS_WARN_IF(!aEnd.IsSetAndValid()) ||
      NS_WARN_IF(aStart.Container() != aEnd.Container())) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // see what kind of node we have
  if (Text* textNode = Text::FromNode(aStart.Container())) {
    if (mEditorBase->IsHTMLEditor() &&
        NS_WARN_IF(
            !EditorUtils::IsEditableContent(*textNode, EditorType::HTML))) {
      // Just ignore to append the transaction for non-editable node.
      return NS_OK;
    }
    // if the node is a chardata node, then delete chardata content
    uint32_t textLengthToDelete;
    if (aStart == aEnd) {
      textLengthToDelete = 1;
    } else {
      textLengthToDelete =
          *aEnd.Offset(RawRangeBoundary::OffsetFilter::kValidOffsets) -
          *aStart.Offset(RawRangeBoundary::OffsetFilter::kValidOffsets);
      MOZ_DIAGNOSTIC_ASSERT(textLengthToDelete > 0);
    }

    RefPtr<DeleteTextTransaction> deleteTextTransaction =
        DeleteTextTransaction::MaybeCreate(
            *mEditorBase, *textNode,
            *aStart.Offset(RawRangeBoundary::OffsetFilter::kValidOffsets),
            textLengthToDelete);
    // If the text node isn't editable, it should be never undone/redone.
    // So, the transaction shouldn't be recorded.
    if (!deleteTextTransaction) {
      NS_WARNING("DeleteTextTransaction::MaybeCreate() failed");
      return NS_ERROR_FAILURE;
    }
    DebugOnly<nsresult> rvIgnored = AppendChild(deleteTextTransaction);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "DeleteRangeTransaction::AppendChild() failed, but ignored");
    return NS_OK;
  }

  if (mEditorBase->IsTextEditor()) {
    // XXX(krosylight): We only want to delete anything within the text node in
    // TextEditor, but there are things that still try deleting the text node
    // itself (bug 1716714).  Do this until we are 100% sure nothing does so.
    MOZ_ASSERT(aStart.Container() == mEditorBase->GetRoot() &&
               aEnd.Container() == mEditorBase->GetRoot());
    MOZ_ASSERT(
        aStart.Offset(RawRangeBoundary::OffsetFilter::kValidOffsets).value() ==
        0);
    MOZ_ASSERT(
        aEnd.Offset(RawRangeBoundary::OffsetFilter::kValidOffsets).value() ==
        mEditorBase->GetRoot()->Length());

    RefPtr<Text> textNode =
        Text::FromNodeOrNull(aStart.Container()->GetFirstChild());
    MOZ_ASSERT(textNode);

    RefPtr<DeleteTextTransaction> deleteTextTransaction =
        DeleteTextTransaction::MaybeCreate(*mEditorBase, *textNode, 0,
                                           textNode->TextDataLength());
    // If the text node isn't editable, it should be never undone/redone.
    // So, the transaction shouldn't be recorded.
    if (!deleteTextTransaction) {
      NS_WARNING("DeleteTextTransaction::MaybeCreate() failed");
      return NS_ERROR_FAILURE;
    }
    DebugOnly<nsresult> rvIgnored = AppendChild(deleteTextTransaction);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "DeleteRangeTransaction::AppendChild() failed, but ignored");
    return NS_OK;
  }

  // Even if we detect invalid range, we should ignore it for removing
  // specified range's nodes as far as possible.
  // XXX This is super expensive.  Probably, we should make
  //     DeleteNodeTransaction() can treat multiple siblings.
  for (nsIContent* child = aStart.GetChildAtOffset();
       child && child != aEnd.GetChildAtOffset();
       child = child->GetNextSibling()) {
    if (NS_WARN_IF(!HTMLEditUtils::IsRemovableNode(*child))) {
      continue;  // Should we abort?
    }
    RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
        DeleteNodeTransaction::MaybeCreate(*mEditorBase, *child);
    if (deleteNodeTransaction) {
      DebugOnly<nsresult> rvIgnored = AppendChild(deleteNodeTransaction);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "DeleteRangeTransaction::AppendChild() failed, but ignored");
    }
  }

  return NS_OK;
}

nsresult DeleteRangeTransaction::CreateTxnsToDeleteContent(
    const RawRangeBoundary& aPoint, nsIEditor::EDirection aAction) {
  if (NS_WARN_IF(!aPoint.IsSetAndValid())) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  Text* textNode = Text::FromNode(aPoint.Container());
  if (!textNode) {
    return NS_OK;
  }

  // If the node is a chardata node, then delete chardata content
  uint32_t startOffset, numToDelete;
  if (nsIEditor::eNext == aAction) {
    startOffset = *aPoint.Offset(RawRangeBoundary::OffsetFilter::kValidOffsets);
    numToDelete = aPoint.Container()->Length() - startOffset;
  } else {
    startOffset = 0;
    numToDelete = *aPoint.Offset(RawRangeBoundary::OffsetFilter::kValidOffsets);
  }

  if (!numToDelete) {
    return NS_OK;
  }

  RefPtr<DeleteTextTransaction> deleteTextTransaction =
      DeleteTextTransaction::MaybeCreate(*mEditorBase, *textNode, startOffset,
                                         numToDelete);
  NS_WARNING_ASSERTION(deleteTextTransaction,
                       "DeleteTextTransaction::MaybeCreate() failed");
  // If the text node isn't editable, it should be never undone/redone.
  // So, the transaction shouldn't be recorded.
  if (!deleteTextTransaction) {
    return NS_ERROR_FAILURE;
  }
  DebugOnly<nsresult> rvIgnored = AppendChild(deleteTextTransaction);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "DeleteRangeTransaction::AppendChild() failed, but ignored");
  return NS_OK;
}

nsresult DeleteRangeTransaction::CreateTxnsToDeleteNodesBetween(
    nsRange* aRangeToDelete) {
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ContentSubtreeIterator subtreeIter;
  nsresult rv = subtreeIter.Init(aRangeToDelete);
  if (NS_FAILED(rv)) {
    NS_WARNING("ContentSubtreeIterator::Init() failed");
    return rv;
  }

  for (; !subtreeIter.IsDone(); subtreeIter.Next()) {
    nsINode* node = subtreeIter.GetCurrentNode();
    if (NS_WARN_IF(!node) || NS_WARN_IF(!node->IsContent())) {
      return NS_ERROR_FAILURE;
    }

    if (NS_WARN_IF(!HTMLEditUtils::IsRemovableNode(*node->AsContent()))) {
      continue;
    }
    RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
        DeleteNodeTransaction::MaybeCreate(*mEditorBase, *node->AsContent());
    if (deleteNodeTransaction) {
      DebugOnly<nsresult> rvIgnored = AppendChild(deleteNodeTransaction);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "DeleteRangeTransaction::AppendChild() failed, but ignored");
    }
  }
  return NS_OK;
}

}  // namespace mozilla
