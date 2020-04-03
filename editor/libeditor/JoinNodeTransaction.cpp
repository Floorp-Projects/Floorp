/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JoinNodeTransaction.h"

#include "mozilla/EditorBase.h"  // for EditorBase
#include "mozilla/dom/Text.h"
#include "nsAString.h"
#include "nsDebug.h"          // for NS_ASSERTION, etc.
#include "nsError.h"          // for NS_ERROR_NULL_POINTER, etc.
#include "nsIContent.h"       // for nsIContent
#include "nsISupportsImpl.h"  // for QueryInterface, etc.

namespace mozilla {

using namespace dom;

// static
already_AddRefed<JoinNodeTransaction> JoinNodeTransaction::MaybeCreate(
    EditorBase& aEditorBase, nsINode& aLeftNode, nsINode& aRightNode) {
  RefPtr<JoinNodeTransaction> transaction =
      new JoinNodeTransaction(aEditorBase, aLeftNode, aRightNode);
  if (NS_WARN_IF(!transaction->CanDoIt())) {
    return nullptr;
  }
  return transaction.forget();
}

JoinNodeTransaction::JoinNodeTransaction(EditorBase& aEditorBase,
                                         nsINode& aLeftNode,
                                         nsINode& aRightNode)
    : mEditorBase(&aEditorBase),
      mLeftNode(&aLeftNode),
      mRightNode(&aRightNode),
      mOffset(0) {}

NS_IMPL_CYCLE_COLLECTION_INHERITED(JoinNodeTransaction, EditTransactionBase,
                                   mEditorBase, mLeftNode, mRightNode, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JoinNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

bool JoinNodeTransaction::CanDoIt() const {
  if (NS_WARN_IF(!mLeftNode) || NS_WARN_IF(!mRightNode) ||
      NS_WARN_IF(!mEditorBase) || !mLeftNode->GetParentNode()) {
    return false;
  }
  return mEditorBase->IsModifiableNode(*mLeftNode->GetParentNode());
}

// After DoTransaction() and RedoTransaction(), the left node is removed from
// the content tree and right node remains.
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP JoinNodeTransaction::DoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mLeftNode) ||
      NS_WARN_IF(!mRightNode)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Get the parent node
  nsCOMPtr<nsINode> leftNodeParent = mLeftNode->GetParentNode();
  if (NS_WARN_IF(!leftNodeParent)) {
    return NS_ERROR_FAILURE;
  }

  // Verify that mLeftNode and mRightNode have the same parent
  if (leftNodeParent != mRightNode->GetParentNode()) {
    NS_ASSERTION(false, "Nodes do not have same parent");
    return NS_ERROR_INVALID_ARG;
  }

  // Set this instance's mParent.  Other methods will see a non-null mParent
  // and know all is well
  mParent = leftNodeParent;
  mOffset = mLeftNode->Length();

  RefPtr<EditorBase> editorBase = mEditorBase;
  nsCOMPtr<nsINode> leftNode = mLeftNode;
  nsCOMPtr<nsINode> rightNode = mRightNode;
  nsresult rv = editorBase->DoJoinNodes(rightNode, leftNode, leftNodeParent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "EditorBase::DoJoinNodes() failed");
  return rv;
}

// XXX: What if instead of split, we just deleted the unneeded children of
//     mRight and re-inserted mLeft?
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
JoinNodeTransaction::UndoTransaction() {
  if (NS_WARN_IF(!mParent) || NS_WARN_IF(!mLeftNode) ||
      NS_WARN_IF(!mRightNode) || NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // First, massage the existing node so it is in its post-split state
  ErrorResult error;
  if (mRightNode->GetAsText()) {
    RefPtr<EditorBase> editorBase = mEditorBase;
    RefPtr<Text> rightNodeAsText = mRightNode->GetAsText();
    editorBase->DoDeleteText(*rightNodeAsText, 0, mOffset, error);
    if (error.Failed()) {
      NS_WARNING("EditorBase::DoDeleteText() failed");
      return error.StealNSResult();
    }
  } else {
    nsCOMPtr<nsIContent> child = mRightNode->GetFirstChild();
    for (uint32_t i = 0; i < mOffset; i++) {
      if (error.Failed()) {
        return error.StealNSResult();
      }
      if (!child) {
        return NS_ERROR_NULL_POINTER;
      }
      nsCOMPtr<nsIContent> nextSibling = child->GetNextSibling();
      mLeftNode->AppendChild(*child, error);
      NS_WARNING_ASSERTION(!error.Failed(), "nsINode::AppendChild() failed");
      child = nextSibling;
    }
  }

  NS_WARNING_ASSERTION(!error.Failed(), "The previous error was ignored");

  // Second, re-insert the left node into the tree
  nsCOMPtr<nsINode> refNode = mRightNode;
  mParent->InsertBefore(*mLeftNode, refNode, error);
  NS_WARNING_ASSERTION(!error.Failed(), "nsINode::InsertBefore() failed");
  return error.StealNSResult();
}

}  // namespace mozilla
