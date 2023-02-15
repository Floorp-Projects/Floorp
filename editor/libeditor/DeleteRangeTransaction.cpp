/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteRangeTransaction.h"

#include "DeleteContentTransactionBase.h"
#include "DeleteNodeTransaction.h"
#include "DeleteTextTransaction.h"
#include "EditTransactionBase.h"
#include "EditorBase.h"
#include "EditorDOMPoint.h"
#include "EditorUtils.h"
#include "HTMLEditUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/Logging.h"
#include "mozilla/mozalloc.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/StaticPrefs_editor.h"
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
                                   mRangeToDelete, mPointToPutCaret)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteRangeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditAggregateTransaction)

void DeleteRangeTransaction::AppendChild(
    DeleteContentTransactionBase& aTransaction) {
  mChildren.AppendElement(aTransaction);
}

nsresult
DeleteRangeTransaction::MaybeExtendDeletingRangeWithSurroundingWhitespace(
    nsRange& aRange) const {
  if (!mEditorBase->mEditActionData->SelectionCreatedByDoubleclick() ||
      !StaticPrefs::
          editor_word_select_delete_space_after_doubleclick_selection()) {
    return NS_OK;
  }
  EditorRawDOMPoint startPoint(aRange.StartRef());
  EditorRawDOMPoint endPoint(aRange.EndRef());
  const bool maybeRangeStartsAfterWhiteSpace =
      startPoint.IsInTextNode() && !startPoint.IsStartOfContainer();
  const bool maybeRangeEndsAtWhiteSpace =
      endPoint.IsInTextNode() && !endPoint.IsEndOfContainer();

  if (!maybeRangeStartsAfterWhiteSpace && !maybeRangeEndsAtWhiteSpace) {
    // no whitespace before or after word => nothing to do here.
    return NS_OK;
  }

  const bool precedingCharIsWhitespace =
      maybeRangeStartsAfterWhiteSpace
          ? startPoint.IsPreviousCharASCIISpaceOrNBSP()
          : false;
  const bool trailingCharIsWhitespace =
      maybeRangeEndsAtWhiteSpace ? endPoint.IsCharASCIISpaceOrNBSP() : false;

  // if possible, try to remove the preceding whitespace
  // so the caret is at the end of the previous word.
  if (precedingCharIsWhitespace) {
    // "one [two]", "one [two] three" or "one [two], three"
    ErrorResult err;
    aRange.SetStart(startPoint.PreviousPoint(), err);
    if (auto rv = err.StealNSResult(); NS_FAILED(rv)) {
      NS_WARNING(
          "DeleteRangeTransaction::"
          "MaybeExtendDeletingRangeWithSurroundingWhitespace"
          " failed to update the start of the deleting range");
      return rv;
    }
  } else if (trailingCharIsWhitespace) {
    // "[one] two"
    ErrorResult err;
    aRange.SetEnd(endPoint.NextPoint(), err);
    if (auto rv = err.StealNSResult(); NS_FAILED(rv)) {
      NS_WARNING(
          "DeleteRangeTransaction::"
          "MaybeExtendDeletingRangeWithSurroundingWhitespace"
          " failed to update the end of the deleting range");
      return rv;
    }
  }

  return NS_OK;
}

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

  MaybeExtendDeletingRangeWithSurroundingWhitespace(*rangeToDelete);

  // build the child transactions
  // XXX We should move this to the constructor.  Then, we don't need to make
  //     this class has mRangeToDelete with nullptr since transaction instances
  //     may be created a lot and live long.
  {
    EditorRawDOMRange extendedRange(*rangeToDelete);
    MOZ_ASSERT(extendedRange.IsPositionedAndValid());

    if (extendedRange.InSameContainer()) {
      // the selection begins and ends in the same node
      nsresult rv = AppendTransactionsToDeleteIn(extendedRange);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "DeleteRangeTransaction::AppendTransactionsToDeleteIn() failed");
        return rv;
      }
    } else {
      // If the range ends at end of a node, we may need to extend the range to
      // make ContentSubtreeIterator iterates close tag of the unnecessary nodes
      // in AppendTransactionsToDeleteNodesEntirelyIn.
      for (EditorRawDOMPoint endOfRange = extendedRange.EndRef();
           endOfRange.IsInContentNode() && endOfRange.IsEndOfContainer() &&
           endOfRange.GetContainer() != extendedRange.StartRef().GetContainer();
           endOfRange = extendedRange.EndRef()) {
        extendedRange.SetEnd(
            EditorRawDOMPoint::After(*endOfRange.ContainerAs<nsIContent>()));
      }

      // the selection ends in a different node from where it started.  delete
      // the relevant content in the start node
      nsresult rv = AppendTransactionToDeleteText(extendedRange.StartRef(),
                                                  nsIEditor::eNext);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "DeleteRangeTransaction::AppendTransactionToDeleteText() failed");
        return rv;
      }
      // delete the intervening nodes
      rv = AppendTransactionsToDeleteNodesWhoseEndBoundaryIn(extendedRange);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "DeleteRangeTransaction::"
            "AppendTransactionsToDeleteNodesWhoseEndBoundaryIn() failed");
        return rv;
      }
      // delete the relevant content in the end node
      rv = AppendTransactionToDeleteText(extendedRange.EndRef(),
                                         nsIEditor::ePrevious);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "DeleteRangeTransaction::AppendTransactionToDeleteText() failed");
        return rv;
      }
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

  mPointToPutCaret = rangeToDelete->StartRef();
  if (MOZ_UNLIKELY(!mPointToPutCaret.IsSetAndValid())) {
    for (const OwningNonNull<EditTransactionBase>& transaction :
         Reversed(mChildren)) {
      if (const DeleteContentTransactionBase* deleteContentTransaction =
              transaction->GetAsDeleteContentTransactionBase()) {
        mPointToPutCaret = deleteContentTransaction->SuggestPointToPutCaret();
        if (mPointToPutCaret.IsSetAndValid()) {
          break;
        }
        continue;
      }
      MOZ_ASSERT_UNREACHABLE(
          "Child transactions must be DeleteContentTransactionBase");
    }
  }
  return NS_OK;
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

nsresult DeleteRangeTransaction::AppendTransactionsToDeleteIn(
    const EditorRawDOMRange& aRangeToDelete) {
  if (NS_WARN_IF(!aRangeToDelete.IsPositionedAndValid())) {
    return NS_ERROR_INVALID_ARG;
  }
  MOZ_ASSERT(aRangeToDelete.InSameContainer());

  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // see what kind of node we have
  if (Text* textNode = aRangeToDelete.StartRef().GetContainerAs<Text>()) {
    if (mEditorBase->IsHTMLEditor() &&
        NS_WARN_IF(
            !EditorUtils::IsEditableContent(*textNode, EditorType::HTML))) {
      // Just ignore to append the transaction for non-editable node.
      return NS_OK;
    }
    // if the node is a chardata node, then delete chardata content
    uint32_t textLengthToDelete;
    if (aRangeToDelete.Collapsed()) {
      textLengthToDelete = 1;
    } else {
      textLengthToDelete =
          aRangeToDelete.EndRef().Offset() - aRangeToDelete.StartRef().Offset();
      MOZ_DIAGNOSTIC_ASSERT(textLengthToDelete > 0);
    }

    RefPtr<DeleteTextTransaction> deleteTextTransaction =
        DeleteTextTransaction::MaybeCreate(*mEditorBase, *textNode,
                                           aRangeToDelete.StartRef().Offset(),
                                           textLengthToDelete);
    // If the text node isn't editable, it should be never undone/redone.
    // So, the transaction shouldn't be recorded.
    if (!deleteTextTransaction) {
      NS_WARNING("DeleteTextTransaction::MaybeCreate() failed");
      return NS_ERROR_FAILURE;
    }
    AppendChild(*deleteTextTransaction);
    return NS_OK;
  }

  MOZ_ASSERT(mEditorBase->IsHTMLEditor());

  // Even if we detect invalid range, we should ignore it for removing
  // specified range's nodes as far as possible.
  // XXX This is super expensive.  Probably, we should make
  //     DeleteNodeTransaction() can treat multiple siblings.
  for (nsIContent* child = aRangeToDelete.StartRef().GetChild();
       child && child != aRangeToDelete.EndRef().GetChild();
       child = child->GetNextSibling()) {
    if (NS_WARN_IF(!HTMLEditUtils::IsRemovableNode(*child))) {
      continue;  // Should we abort?
    }
    RefPtr<DeleteNodeTransaction> deleteNodeTransaction =
        DeleteNodeTransaction::MaybeCreate(*mEditorBase, *child);
    if (deleteNodeTransaction) {
      AppendChild(*deleteNodeTransaction);
    }
  }

  return NS_OK;
}

nsresult DeleteRangeTransaction::AppendTransactionToDeleteText(
    const EditorRawDOMPoint& aMaybePointInText, nsIEditor::EDirection aAction) {
  if (NS_WARN_IF(!aMaybePointInText.IsSetAndValid())) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!aMaybePointInText.IsInTextNode()) {
    return NS_OK;
  }

  // If the node is a text node, then delete text before or after the point.
  Text& textNode = *aMaybePointInText.ContainerAs<Text>();
  uint32_t startOffset, numToDelete;
  if (nsIEditor::eNext == aAction) {
    startOffset = aMaybePointInText.Offset();
    numToDelete = textNode.TextDataLength() - startOffset;
  } else {
    startOffset = 0;
    numToDelete = aMaybePointInText.Offset();
  }

  if (!numToDelete) {
    return NS_OK;
  }

  RefPtr<DeleteTextTransaction> deleteTextTransaction =
      DeleteTextTransaction::MaybeCreate(*mEditorBase, textNode, startOffset,
                                         numToDelete);
  // If the text node isn't editable, it should be never undone/redone.
  // So, the transaction shouldn't be recorded.
  if (MOZ_UNLIKELY(!deleteTextTransaction)) {
    NS_WARNING("DeleteTextTransaction::MaybeCreate() failed");
    return NS_ERROR_FAILURE;
  }
  AppendChild(*deleteTextTransaction);
  return NS_OK;
}

nsresult
DeleteRangeTransaction::AppendTransactionsToDeleteNodesWhoseEndBoundaryIn(
    const EditorRawDOMRange& aRangeToDelete) {
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ContentSubtreeIterator subtreeIter;
  nsresult rv = subtreeIter.Init(aRangeToDelete.StartRef().ToRawRangeBoundary(),
                                 aRangeToDelete.EndRef().ToRawRangeBoundary());
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
      AppendChild(*deleteNodeTransaction);
    }
  }
  return NS_OK;
}

}  // namespace mozilla
