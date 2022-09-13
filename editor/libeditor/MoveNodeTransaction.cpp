/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MoveNodeTransaction.h"

#include "EditorBase.h"      // for EditorBase
#include "EditorDOMPoint.h"  // for EditorDOMPoint
#include "HTMLEditor.h"      // for HTMLEditor
#include "HTMLEditUtils.h"   // for HTMLEditUtils

#include "mozilla/Likely.h"
#include "mozilla/Logging.h"
#include "mozilla/ToString.h"

#include "nsDebug.h"     // for NS_WARNING, etc.
#include "nsError.h"     // for NS_ERROR_NULL_POINTER, etc.
#include "nsIContent.h"  // for nsIContent
#include "nsString.h"    // for nsString

namespace mozilla {

using namespace dom;

template already_AddRefed<MoveNodeTransaction> MoveNodeTransaction::MaybeCreate(
    HTMLEditor& aHTMLEditor, nsIContent& aContentToMove,
    const EditorDOMPoint& aPointToInsert);
template already_AddRefed<MoveNodeTransaction> MoveNodeTransaction::MaybeCreate(
    HTMLEditor& aHTMLEditor, nsIContent& aContentToMove,
    const EditorRawDOMPoint& aPointToInsert);

// static
template <typename PT, typename CT>
already_AddRefed<MoveNodeTransaction> MoveNodeTransaction::MaybeCreate(
    HTMLEditor& aHTMLEditor, nsIContent& aContentToMove,
    const EditorDOMPointBase<PT, CT>& aPointToInsert) {
  if (NS_WARN_IF(!aContentToMove.GetParentNode()) ||
      NS_WARN_IF(!aPointToInsert.IsSet())) {
    return nullptr;
  }
  // TODO: We should not allow to move a node to improper container element.
  //       However, this is currently used to move invalid parent while
  //       processing the nodes.  Therefore, treating the case as error breaks
  //       a lot.
  if (NS_WARN_IF(!HTMLEditUtils::IsRemovableNode(aContentToMove)) ||
      // The destination should be editable, but it may be in an orphan node or
      // sub-tree to reduce number of DOM mutation events.  In such case, we're
      // okay to move a node into the non-editable content because we can assume
      // that the caller will insert it into an editable element.
      NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(
                     *aPointToInsert.GetContainer()) &&
                 aPointToInsert.GetContainer()->IsInComposedDoc())) {
    return nullptr;
  }
  RefPtr<MoveNodeTransaction> transaction =
      new MoveNodeTransaction(aHTMLEditor, aContentToMove, aPointToInsert);
  return transaction.forget();
}

template <typename PT, typename CT>
MoveNodeTransaction::MoveNodeTransaction(
    HTMLEditor& aHTMLEditor, nsIContent& aContentToMove,
    const EditorDOMPointBase<PT, CT>& aPointToInsert)
    : mContentToMove(&aContentToMove),
      mContainer(aPointToInsert.GetContainer()),
      mReference(aPointToInsert.GetChild()),
      mOldContainer(aContentToMove.GetParentNode()),
      mOldNextSibling(aContentToMove.GetNextSibling()),
      mHTMLEditor(&aHTMLEditor) {
  MOZ_ASSERT(mContainer);
  MOZ_ASSERT(mOldContainer);
  MOZ_ASSERT_IF(mReference, mReference->GetParentNode() == mContainer);
  MOZ_ASSERT_IF(mOldNextSibling,
                mOldNextSibling->GetParentNode() == mOldContainer);
  // printf("MoveNodeTransaction size: %zu\n", sizeof(MoveNodeTransaction));
  static_assert(sizeof(MoveNodeTransaction) <= 72,
                "Transaction classes may be created a lot and may be alive "
                "long so that keep the foot print smaller as far as possible");
}

std::ostream& operator<<(std::ostream& aStream,
                         const MoveNodeTransaction& aTransaction) {
  auto DumpNodeDetails = [&](const nsINode* aNode) {
    if (aNode) {
      if (aNode->IsText()) {
        nsAutoString data;
        aNode->AsText()->GetData(data);
        aStream << " (#text \"" << NS_ConvertUTF16toUTF8(data).get() << "\")";
      } else {
        aStream << " (" << *aNode << ")";
      }
    }
  };
  aStream << "{ mContentToMove=" << aTransaction.mContentToMove.get();
  DumpNodeDetails(aTransaction.mContentToMove);
  aStream << ", mContainer=" << aTransaction.mContainer.get();
  DumpNodeDetails(aTransaction.mContainer);
  aStream << ", mReference=" << aTransaction.mReference.get();
  DumpNodeDetails(aTransaction.mReference);
  aStream << ", mOldContainer=" << aTransaction.mOldContainer.get();
  DumpNodeDetails(aTransaction.mOldContainer);
  aStream << ", mOldNextSibling=" << aTransaction.mOldNextSibling.get();
  DumpNodeDetails(aTransaction.mOldNextSibling);
  aStream << ", mHTMLEditor=" << aTransaction.mHTMLEditor.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(MoveNodeTransaction, EditTransactionBase,
                                   mHTMLEditor, mContentToMove, mContainer,
                                   mReference, mOldContainer, mOldNextSibling)

NS_IMPL_ADDREF_INHERITED(MoveNodeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(MoveNodeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MoveNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP MoveNodeTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p MoveNodeTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));
  return DoTransactionInternal();
}

nsresult MoveNodeTransaction::DoTransactionInternal() {
  MOZ_DIAGNOSTIC_ASSERT(mHTMLEditor);
  MOZ_DIAGNOSTIC_ASSERT(mContentToMove);
  MOZ_DIAGNOSTIC_ASSERT(mContainer);
  MOZ_DIAGNOSTIC_ASSERT(mOldContainer);

  OwningNonNull<HTMLEditor> htmlEditor = *mHTMLEditor;
  OwningNonNull<nsIContent> contentToMove = *mContentToMove;
  OwningNonNull<nsINode> container = *mContainer;
  nsCOMPtr<nsIContent> newNextSibling = mReference;
  if (contentToMove->IsElement()) {
    nsresult rv = htmlEditor->MarkElementDirty(
        MOZ_KnownLive(*contentToMove->AsElement()));
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditorBase::ToGenericNSResult(rv);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::MarkElementDirty() failed, but ignored");
  }

  {
    AutoMoveNodeSelNotify notifyStoredRanges(
        htmlEditor->RangeUpdaterRef(), EditorRawDOMPoint(contentToMove),
        newNextSibling ? EditorRawDOMPoint(newNextSibling)
                       : EditorRawDOMPoint::AtEndOf(container));
    IgnoredErrorResult error;
    container->InsertBefore(contentToMove, newNextSibling, error);
    // InsertBefore() may call MightThrowJSException() even if there is no
    // error. We don't need the flag here.
    error.WouldReportJSException();
    if (error.Failed()) {
      NS_WARNING("nsINode::InsertBefore() failed");
      return error.StealNSResult();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP MoveNodeTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p MoveNodeTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mHTMLEditor) || NS_WARN_IF(!mContentToMove) ||
      NS_WARN_IF(!mOldContainer)) {
    // Perhaps, nulled-out by the cycle collector.
    return NS_ERROR_FAILURE;
  }

  // If the original point has been changed, refer mOldNextSibling if it's
  // renasonable.  Otherwise, use end of the old container.
  if (mOldNextSibling && mOldContainer != mOldNextSibling->GetParentNode()) {
    // TODO: Check whether the new container is proper one for containing
    //       mContentToMove.  However, there are few testcases so that we
    //       shouldn't change here without creating a lot of undo tests.
    if (mOldNextSibling->GetParentNode() &&
        (mOldNextSibling->IsInComposedDoc() ||
         !mOldContainer->IsInComposedDoc())) {
      mOldContainer = mOldNextSibling->GetParentNode();
    } else {
      mOldNextSibling = nullptr;  // end of mOldContainer
    }
  }

  if (MOZ_UNLIKELY(!HTMLEditUtils::IsRemovableNode(*mContentToMove))) {
    NS_WARNING(
        "MoveNodeTransaction::UndoTransaction() couldn't move the "
        "content due to not removable from its current container");
    return NS_ERROR_FAILURE;
  }
  if (MOZ_UNLIKELY(!HTMLEditUtils::IsSimplyEditableNode(*mOldContainer))) {
    NS_WARNING(
        "MoveNodeTransaction::UndoTransaction() couldn't move the "
        "content into the old container due to non-editable one");
    return NS_ERROR_FAILURE;
  }

  // And store the latest node which should be referred at redoing.
  mContainer = mContentToMove->GetParentNode();
  mReference = mContentToMove->GetNextSibling();

  OwningNonNull<HTMLEditor> htmlEditor = *mHTMLEditor;
  OwningNonNull<nsINode> oldContainer = *mOldContainer;
  OwningNonNull<nsIContent> contentToMove = *mContentToMove;
  nsCOMPtr<nsIContent> oldNextSibling = mOldNextSibling;
  if (contentToMove->IsElement()) {
    nsresult rv = htmlEditor->MarkElementDirty(
        MOZ_KnownLive(*contentToMove->AsElement()));
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditorBase::ToGenericNSResult(rv);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::MarkElementDirty() failed, but ignored");
  }

  {
    AutoMoveNodeSelNotify notifyStoredRanges(
        htmlEditor->RangeUpdaterRef(), EditorRawDOMPoint(contentToMove),
        oldNextSibling ? EditorRawDOMPoint(oldNextSibling)
                       : EditorRawDOMPoint::AtEndOf(oldContainer));
    IgnoredErrorResult error;
    oldContainer->InsertBefore(contentToMove, oldNextSibling, error);
    // InsertBefore() may call MightThrowJSException() even if there is no
    // error. We don't need the flag here.
    error.WouldReportJSException();
    if (error.Failed()) {
      NS_WARNING("nsINode::InsertBefore() failed");
      return error.StealNSResult();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP MoveNodeTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p MoveNodeTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mHTMLEditor) || NS_WARN_IF(!mContentToMove) ||
      NS_WARN_IF(!mContainer)) {
    // Perhaps, nulled-out by the cycle collector.
    return NS_ERROR_FAILURE;
  }

  // If the inserting point has been changed, refer mReference if it's
  // renasonable.  Otherwise, use end of the container.
  if (mReference && mContainer != mReference->GetParentNode()) {
    // TODO: Check whether the new container is proper one for containing
    //       mContentToMove.  However, there are few testcases so that we
    //       shouldn't change here without creating a lot of redo tests.
    if (mReference->GetParentNode() &&
        (mReference->IsInComposedDoc() || !mContainer->IsInComposedDoc())) {
      mContainer = mReference->GetParentNode();
    } else {
      mReference = nullptr;  // end of mContainer
    }
  }

  if (MOZ_UNLIKELY(!HTMLEditUtils::IsRemovableNode(*mContentToMove))) {
    NS_WARNING(
        "MoveNodeTransaction::RedoTransaction() couldn't move the "
        "content due to not removable from its current container");
    return NS_ERROR_FAILURE;
  }
  if (MOZ_UNLIKELY(!HTMLEditUtils::IsSimplyEditableNode(*mContainer))) {
    NS_WARNING(
        "MoveNodeTransaction::RedoTransaction() couldn't move the "
        "content into the new container due to non-editable one");
    return NS_ERROR_FAILURE;
  }

  // And store the latest node which should be back.
  mOldContainer = mContentToMove->GetParentNode();
  mOldNextSibling = mContentToMove->GetNextSibling();

  nsresult rv = DoTransactionInternal();
  if (NS_FAILED(rv)) {
    NS_WARNING("MoveNodeTransaction::DoTransactionInternal() failed");
    return rv;
  }

  if (!mHTMLEditor->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  OwningNonNull<HTMLEditor> htmlEditor(*mHTMLEditor);
  rv = htmlEditor->CollapseSelectionTo(
      SuggestPointToPutCaret<EditorRawDOMPoint>());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::CollapseSelectionTo() failed, but ignored");
  return NS_OK;
}

}  // namespace mozilla
