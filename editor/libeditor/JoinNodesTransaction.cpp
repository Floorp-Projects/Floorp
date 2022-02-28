/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JoinNodesTransaction.h"

#include "HTMLEditor.h"  // for HTMLEditor
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
      mLeftContent(&aLeftContent),
      mRightContent(&aRightContent),
      mOffset(0) {
  // printf("JoinNodesTransaction size: %zu\n", sizeof(JoinNodesTransaction));
  static_assert(sizeof(JoinNodesTransaction) <= 64,
                "Transaction classes may be created a lot and may be alive "
                "long so that keep the foot print smaller as far as possible");
}

std::ostream& operator<<(std::ostream& aStream,
                         const JoinNodesTransaction& aTransaction) {
  aStream << "{ mLeftContent=" << aTransaction.mLeftContent.get();
  if (aTransaction.mLeftContent) {
    aStream << " (" << *aTransaction.mLeftContent << ")";
  }
  aStream << ", mRightContent=" << aTransaction.mRightContent.get();
  if (aTransaction.mRightContent) {
    aStream << " (" << *aTransaction.mRightContent << ")";
  }
  aStream << ", mParentNode=" << aTransaction.mParentNode.get();
  if (aTransaction.mParentNode) {
    aStream << " (" << *aTransaction.mParentNode << ")";
  }
  aStream << ", mOffset=" << aTransaction.mOffset
          << ", mHTMLEditor=" << aTransaction.mHTMLEditor.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(JoinNodesTransaction, EditTransactionBase,
                                   mHTMLEditor, mLeftContent, mRightContent,
                                   mParentNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JoinNodesTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

bool JoinNodesTransaction::CanDoIt() const {
  if (NS_WARN_IF(!mLeftContent) || NS_WARN_IF(!mRightContent) ||
      NS_WARN_IF(!mHTMLEditor) || !mLeftContent->GetParentNode()) {
    return false;
  }
  return HTMLEditUtils::IsRemovableFromParentNode(*mLeftContent);
}

// After DoTransaction() and RedoTransaction(), the left node is removed from
// the content tree and right node remains.
NS_IMETHODIMP JoinNodesTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p JoinNodesTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mHTMLEditor) || NS_WARN_IF(!mLeftContent) ||
      NS_WARN_IF(!mRightContent)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Get the parent node
  nsINode* leftContentParent = mLeftContent->GetParentNode();
  if (NS_WARN_IF(!leftContentParent)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Verify that mLeftContent and mRightContent have the same parent
  if (leftContentParent != mRightContent->GetParentNode()) {
    NS_ASSERTION(false, "Nodes do not have same parent");
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Set this instance's mParentNode.  Other methods will see a non-null
  // mParentNode and know all is well
  mParentNode = leftContentParent;
  mOffset = mLeftContent->Length();

  OwningNonNull<HTMLEditor> htmlEditor = *mHTMLEditor;
  OwningNonNull<nsIContent> leftContent = *mLeftContent;
  OwningNonNull<nsIContent> rightContent = *mRightContent;
  nsresult rv = htmlEditor->DoJoinNodes(rightContent, leftContent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "HTMLEditor::DoJoinNodes() failed");
  return rv;
}

// XXX: What if instead of split, we just deleted the unneeded children of
//     mRight and re-inserted mLeft?
NS_IMETHODIMP JoinNodesTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p JoinNodesTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mParentNode) || NS_WARN_IF(!mLeftContent) ||
      NS_WARN_IF(!mRightContent) || NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  OwningNonNull<HTMLEditor> htmlEditor = *mHTMLEditor;
  OwningNonNull<nsIContent> leftContent = *mLeftContent;

  SplitNodeResult splitNodeResult = htmlEditor->DoSplitNode(
      EditorDOMPoint(mRightContent, std::min(mOffset, mRightContent->Length())),
      leftContent);
  NS_WARNING_ASSERTION(splitNodeResult.Succeeded(),
                       "HTMLEditor::DoSplitNode() failed");
  return splitNodeResult.Rv();
}

NS_IMETHODIMP JoinNodesTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p JoinNodesTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));
  return DoTransaction();
}

}  // namespace mozilla
