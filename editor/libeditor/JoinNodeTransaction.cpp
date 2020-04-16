/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JoinNodeTransaction.h"

#include "HTMLEditUtils.h"
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
    EditorBase& aEditorBase, nsIContent& aLeftContent,
    nsIContent& aRightContent) {
  RefPtr<JoinNodeTransaction> transaction =
      new JoinNodeTransaction(aEditorBase, aLeftContent, aRightContent);
  if (NS_WARN_IF(!transaction->CanDoIt())) {
    return nullptr;
  }
  return transaction.forget();
}

JoinNodeTransaction::JoinNodeTransaction(EditorBase& aEditorBase,
                                         nsIContent& aLeftContent,
                                         nsIContent& aRightContent)
    : mEditorBase(&aEditorBase),
      mLeftContent(&aLeftContent),
      mRightContent(&aRightContent),
      mOffset(0) {}

NS_IMPL_CYCLE_COLLECTION_INHERITED(JoinNodeTransaction, EditTransactionBase,
                                   mEditorBase, mLeftContent, mRightContent,
                                   mParentNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JoinNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

bool JoinNodeTransaction::CanDoIt() const {
  if (NS_WARN_IF(!mLeftContent) || NS_WARN_IF(!mRightContent) ||
      NS_WARN_IF(!mEditorBase) || !mLeftContent->GetParentNode()) {
    return false;
  }
  return mEditorBase->IsTextEditor() ||
         HTMLEditUtils::IsRemovableFromParentNode(*mLeftContent);
}

// After DoTransaction() and RedoTransaction(), the left node is removed from
// the content tree and right node remains.
NS_IMETHODIMP JoinNodeTransaction::DoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mLeftContent) ||
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

  OwningNonNull<EditorBase> editorBase = *mEditorBase;
  OwningNonNull<nsIContent> leftContent = *mLeftContent;
  OwningNonNull<nsIContent> rightContent = *mRightContent;
  nsresult rv = editorBase->DoJoinNodes(rightContent, leftContent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "EditorBase::DoJoinNodes() failed");
  return rv;
}

// XXX: What if instead of split, we just deleted the unneeded children of
//     mRight and re-inserted mLeft?
NS_IMETHODIMP JoinNodeTransaction::UndoTransaction() {
  if (NS_WARN_IF(!mParentNode) || NS_WARN_IF(!mLeftContent) ||
      NS_WARN_IF(!mRightContent) || NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  OwningNonNull<nsIContent> leftContent = *mLeftContent;
  OwningNonNull<nsIContent> rightContent = *mRightContent;
  OwningNonNull<nsINode> parentNode = *mParentNode;

  // First, massage the existing node so it is in its post-split state
  ErrorResult error;
  if (Text* rightTextNode = rightContent->GetAsText()) {
    OwningNonNull<EditorBase> editorBase = *mEditorBase;
    editorBase->DoDeleteText(MOZ_KnownLive(*rightTextNode), 0, mOffset, error);
    if (error.Failed()) {
      NS_WARNING("EditorBase::DoDeleteText() failed");
      return error.StealNSResult();
    }
  } else {
    AutoTArray<OwningNonNull<nsIContent>, 24> movingChildren;
    if (nsIContent* child = mRightContent->GetFirstChild()) {
      movingChildren.AppendElement(*child);
      for (uint32_t i = 0; i < mOffset; i++) {
        child = child->GetNextSibling();
        if (!child) {
          break;
        }
        movingChildren.AppendElement(*child);
      }
    }
    for (OwningNonNull<nsIContent>& child : movingChildren) {
      leftContent->AppendChild(child, error);
      if (error.Failed()) {
        NS_WARNING("nsINode::AppendChild() failed");
        return error.StealNSResult();
      }
    }
  }

  NS_WARNING_ASSERTION(!error.Failed(), "The previous error was ignored");

  // Second, re-insert the left node into the tree
  parentNode->InsertBefore(leftContent, rightContent, error);
  NS_WARNING_ASSERTION(!error.Failed(), "nsINode::InsertBefore() failed");
  return error.StealNSResult();
}

}  // namespace mozilla
