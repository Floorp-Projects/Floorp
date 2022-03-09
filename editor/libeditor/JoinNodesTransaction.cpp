/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JoinNodesTransaction.h"

#include "EditorDOMPoint.h"  // for EditorDOMPoint, etc.
#include "HTMLEditor.h"      // for HTMLEditor
#include "HTMLEditUtils.h"

#include "mozilla/Logging.h"
#include "mozilla/ToString.h"
#include "mozilla/dom/Text.h"

#include "nsAString.h"
#include "nsDebug.h"          // for NS_ASSERTION, etc.
#include "nsError.h"          // for NS_ERROR_NULL_POINTER, etc.
#include "nsIContent.h"       // for nsIContent
#include "nsISupportsImpl.h"  // for QueryInterface, etc.

namespace mozilla {

using namespace dom;

// static
already_AddRefed<JoinNodesTransaction> JoinNodesTransaction::MaybeCreate(
    HTMLEditor& aHTMLEditor, nsIContent& aLeftContent,
    nsIContent& aRightContent) {
  RefPtr<JoinNodesTransaction> transaction =
      new JoinNodesTransaction(aHTMLEditor, aLeftContent, aRightContent);
  if (NS_WARN_IF(!transaction->CanDoIt())) {
    return nullptr;
  }
  return transaction.forget();
}

JoinNodesTransaction::JoinNodesTransaction(HTMLEditor& aHTMLEditor,
                                           nsIContent& aLeftContent,
                                           nsIContent& aRightContent)
    : mHTMLEditor(&aHTMLEditor),
      mRemovedContent(&aLeftContent),
      mKeepingContent(&aRightContent) {
  // printf("JoinNodesTransaction size: %zu\n", sizeof(JoinNodesTransaction));
  static_assert(sizeof(JoinNodesTransaction) <= 64,
                "Transaction classes may be created a lot and may be alive "
                "long so that keep the foot print smaller as far as possible");
}

std::ostream& operator<<(std::ostream& aStream,
                         const JoinNodesTransaction& aTransaction) {
  aStream << "{ mParentNode=" << aTransaction.mParentNode.get();
  if (aTransaction.mParentNode) {
    aStream << " (" << *aTransaction.mParentNode << ")";
  }
  aStream << ", mRemovedContent=" << aTransaction.mRemovedContent.get();
  if (aTransaction.mRemovedContent) {
    aStream << " (" << *aTransaction.mRemovedContent << ")";
  }
  aStream << ", mKeepingContent=" << aTransaction.mKeepingContent.get();
  if (aTransaction.mKeepingContent) {
    aStream << " (" << *aTransaction.mKeepingContent << ")";
  }
  aStream << ", mJoinedOffset=" << aTransaction.mJoinedOffset
          << ", mHTMLEditor=" << aTransaction.mHTMLEditor.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(JoinNodesTransaction, EditTransactionBase,
                                   mHTMLEditor, mParentNode, mRemovedContent,
                                   mKeepingContent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JoinNodesTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

bool JoinNodesTransaction::CanDoIt() const {
  if (NS_WARN_IF(!mKeepingContent) || NS_WARN_IF(!mRemovedContent) ||
      NS_WARN_IF(!mHTMLEditor) ||
      NS_WARN_IF(mRemovedContent->IsBeingRemoved()) ||
      !mKeepingContent->IsInComposedDoc()) {
    return false;
  }
  return HTMLEditUtils::IsRemovableFromParentNode(*mRemovedContent);
}

// After DoTransaction() and RedoTransaction(), the left node is removed from
// the content tree and right node remains.
NS_IMETHODIMP JoinNodesTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p JoinNodesTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  return DoTransactionInternal(RedoingTransaction::No);
}

nsresult JoinNodesTransaction::DoTransactionInternal(
    RedoingTransaction aRedoingTransaction) {
  if (MOZ_UNLIKELY(NS_WARN_IF(!mHTMLEditor) || NS_WARN_IF(!mKeepingContent) ||
                   NS_WARN_IF(!mRemovedContent) ||
                   NS_WARN_IF(mRemovedContent->IsBeingRemoved()))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsINode* removingContentParentNode = mRemovedContent->GetParentNode();
  if (MOZ_UNLIKELY(NS_WARN_IF(!removingContentParentNode))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Verify that the joining content nodes have the same parent
  if (MOZ_UNLIKELY(removingContentParentNode !=
                   mKeepingContent->GetParentNode())) {
    NS_ASSERTION(false, "Nodes do not have same parent");
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Set this instance's mParentNode.  Other methods will see a non-null
  // mParentNode and know all is well
  mParentNode = removingContentParentNode;
  // For now, setting mJoinedOffset to removed content length so that
  // CreateJoinedPoint returns a point in mKeepingContent whose offset is
  // the result if all content in mRemovedContent are moved to start of
  // mKeepingContent.  The offset will be adjusted below.
  mJoinedOffset = mRemovedContent->Length();

  const OwningNonNull<HTMLEditor> htmlEditor = *mHTMLEditor;
  const OwningNonNull<nsIContent> removingContent = *mRemovedContent;
  const OwningNonNull<nsIContent> keepingContent = *mKeepingContent;
  // FYI: ComputeIndexInParentNode() never returns Nothing here because it's not
  //      being removed (i.e., it's in the parent child node chain).
  const uint32_t removingContentOffset =
      *removingContent->ComputeIndexInParentNode();
  nsresult rv;
  // Let's try to get actual joined point with the tacker.
  EditorDOMPoint joinNodesPoint(mKeepingContent, 0u);
  {
    AutoTrackDOMPoint trackJoinNodePoint(htmlEditor->RangeUpdaterRef(),
                                         &joinNodesPoint);
    rv = htmlEditor->DoJoinNodes(keepingContent, removingContent);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "HTMLEditor::DoJoinNodes() failed");

    DebugOnly<nsresult> rvIgnored =
        htmlEditor->RangeUpdaterRef().SelAdjJoinNodes(
            CreateJoinedPoint<EditorRawDOMPoint>(), removingContent,
            removingContentOffset, JoinNodesDirection::LeftNodeIntoRightNode);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "RangeUpdater::SelAdjJoinNodes() failed, but ignored");
  }
  // Adjust join node offset to the actual offset where the original first
  // content of the right node is.
  mJoinedOffset = joinNodesPoint.Offset();

  if (aRedoingTransaction == RedoingTransaction::No) {
    htmlEditor->DidJoinNodesTransaction(*this, rv);
  }

  return rv;
}

// XXX: What if instead of split, we just deleted the unneeded children of
//     mRight and re-inserted mLeft?
NS_IMETHODIMP JoinNodesTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p JoinNodesTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mParentNode) || NS_WARN_IF(!mKeepingContent) ||
      NS_WARN_IF(!mRemovedContent) || NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  const OwningNonNull<HTMLEditor> htmlEditor = *mHTMLEditor;
  const OwningNonNull<nsIContent> removedContent = *mRemovedContent;

  const SplitNodeResult splitNodeResult = htmlEditor->DoSplitNode(
      EditorDOMPoint(mKeepingContent,
                     std::min(mJoinedOffset, mKeepingContent->Length())),
      removedContent);
  NS_WARNING_ASSERTION(splitNodeResult.Succeeded(),
                       "HTMLEditor::DoSplitNode() failed");
  return splitNodeResult.Rv();
}

NS_IMETHODIMP JoinNodesTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p JoinNodesTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));
  return DoTransactionInternal(RedoingTransaction::Yes);
}

}  // namespace mozilla
