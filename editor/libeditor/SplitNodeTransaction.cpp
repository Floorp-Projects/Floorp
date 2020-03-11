/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SplitNodeTransaction.h"

#include "mozilla/EditorBase.h"      // for EditorBase
#include "mozilla/EditorDOMPoint.h"  // for RangeBoundary, EditorRawDOMPoint
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsDebug.h"     // for NS_ASSERTION, etc.
#include "nsError.h"     // for NS_ERROR_NOT_INITIALIZED, etc.
#include "nsIContent.h"  // for nsIContent

namespace mozilla {

using namespace dom;

template already_AddRefed<SplitNodeTransaction> SplitNodeTransaction::Create(
    EditorBase& aEditorBase, const EditorDOMPoint& aStartOfRightNode);
template already_AddRefed<SplitNodeTransaction> SplitNodeTransaction::Create(
    EditorBase& aEditorBase, const EditorRawDOMPoint& aStartOfRightNode);

// static
template <typename PT, typename CT>
already_AddRefed<SplitNodeTransaction> SplitNodeTransaction::Create(
    EditorBase& aEditorBase,
    const EditorDOMPointBase<PT, CT>& aStartOfRightNode) {
  RefPtr<SplitNodeTransaction> transaction =
      new SplitNodeTransaction(aEditorBase, aStartOfRightNode);
  return transaction.forget();
}

template <typename PT, typename CT>
SplitNodeTransaction::SplitNodeTransaction(
    EditorBase& aEditorBase,
    const EditorDOMPointBase<PT, CT>& aStartOfRightNode)
    : mEditorBase(&aEditorBase), mStartOfRightNode(aStartOfRightNode) {
  MOZ_DIAGNOSTIC_ASSERT(aStartOfRightNode.IsSet());
  MOZ_DIAGNOSTIC_ASSERT(aStartOfRightNode.GetContainerAsContent());
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(SplitNodeTransaction, EditTransactionBase,
                                   mEditorBase, mStartOfRightNode, mParent,
                                   mNewLeftNode)

NS_IMPL_ADDREF_INHERITED(SplitNodeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(SplitNodeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SplitNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
SplitNodeTransaction::DoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mStartOfRightNode.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  MOZ_ASSERT(mStartOfRightNode.IsSetAndValid());

  // Create a new node
  ErrorResult error;
  // Don't use .downcast directly because AsContent has an assertion we want
  nsCOMPtr<nsINode> cloneOfRightContainer =
      mStartOfRightNode.GetContainer()->CloneNode(false, error);
  if (error.Failed()) {
    NS_WARNING("nsINode::CloneNode() failed");
    return error.StealNSResult();
  }
  if (NS_WARN_IF(!cloneOfRightContainer)) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<EditorBase> editorBase(mEditorBase);

  mNewLeftNode = cloneOfRightContainer->AsContent();
  if (RefPtr<Element> startOfRightNode =
          mStartOfRightNode.GetContainerAsElement()) {
    nsresult rv = editorBase->MarkElementDirty(*startOfRightNode);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditorBase::ToGenericNSResult(rv);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::MarkElementDirty() failed, but ignored");
  }

  // Get the parent node
  mParent = mStartOfRightNode.GetContainerParent();
  if (!mParent) {
    NS_WARNING("Right container was an orphan node");
    return NS_ERROR_FAILURE;
  }

  // Insert the new node
  nsCOMPtr<nsIContent> newLeftNode = mNewLeftNode;
  editorBase->DoSplitNode(EditorDOMPoint(mStartOfRightNode), *newLeftNode,
                          error);
  if (error.Failed()) {
    NS_WARNING("EditorBase::DoSplitNode() failed");
    return error.StealNSResult();
  }

  if (!editorBase->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  NS_WARNING_ASSERTION(
      !editorBase->Destroyed(),
      "The editor has gone but SplitNodeTransaction keeps trying to modify "
      "Selection");
  RefPtr<Selection> selection = editorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }
  EditorRawDOMPoint atEndOfLeftNode(EditorRawDOMPoint::AtEndOf(mNewLeftNode));
  selection->Collapse(atEndOfLeftNode, error);
  NS_WARNING_ASSERTION(!error.Failed(), "Selection::Collapse() failed");
  return error.StealNSResult();
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
SplitNodeTransaction::UndoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mNewLeftNode) ||
      NS_WARN_IF(!mParent) || NS_WARN_IF(!mStartOfRightNode.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // This assumes Do inserted the new node in front of the prior existing node
  // XXX Perhaps, we should reset mStartOfRightNode with current first child
  //     of the right node.
  RefPtr<EditorBase> editorBase = mEditorBase;
  nsCOMPtr<nsINode> container = mStartOfRightNode.GetContainer();
  nsCOMPtr<nsINode> newLeftNode = mNewLeftNode;
  nsCOMPtr<nsINode> parent = mParent;
  nsresult rv = editorBase->DoJoinNodes(container, newLeftNode, parent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "EditorBase::DoJoinNodes() failed");
  return rv;
}

/* Redo cannot simply resplit the right node, because subsequent transactions
 * on the redo stack may depend on the left node existing in its previous
 * state.
 */
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
SplitNodeTransaction::RedoTransaction() {
  if (NS_WARN_IF(!mNewLeftNode) || NS_WARN_IF(!mParent) ||
      NS_WARN_IF(!mStartOfRightNode.IsSet()) || NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // First, massage the existing node so it is in its post-split state
  ErrorResult error;
  if (mStartOfRightNode.IsInTextNode()) {
    RefPtr<EditorBase> editorBase = mEditorBase;
    RefPtr<Text> rightNodeAsText = mStartOfRightNode.GetContainerAsText();
    MOZ_DIAGNOSTIC_ASSERT(rightNodeAsText);
    editorBase->DoDeleteText(*rightNodeAsText, 0, mStartOfRightNode.Offset(),
                             error);
    if (error.Failed()) {
      NS_WARNING("EditorBase::DoDeleteText() failed");
      return error.StealNSResult();
    }
  } else {
    nsCOMPtr<nsIContent> child =
        mStartOfRightNode.GetContainer()->GetFirstChild();
    nsCOMPtr<nsIContent> nextSibling;
    ErrorResult error;
    for (uint32_t i = 0; i < mStartOfRightNode.Offset(); i++) {
      // XXX This must be bad behavior.  Perhaps, we should work with
      //     mStartOfRightNode::GetChild().  Even if some children
      //     before the right node have been inserted or removed, we should
      //     move all children before the right node because user must focus
      //     on the right node, so, it must be the expected behavior.
      if (NS_WARN_IF(!child)) {
        return NS_ERROR_NULL_POINTER;
      }
      nextSibling = child->GetNextSibling();
      mStartOfRightNode.GetContainer()->RemoveChild(*child, error);
      if (error.Failed()) {
        NS_WARNING("nsINode::RemoveChild() failed");
        return error.StealNSResult();
      }
      mNewLeftNode->AppendChild(*child, error);
      if (error.Failed()) {
        NS_WARNING("nsINode::AppendChild() failed");
        return error.StealNSResult();
      }
      child = nextSibling;
    }
  }
  MOZ_ASSERT(!error.Failed());
  // Second, re-insert the left node into the tree
  mParent->InsertBefore(*mNewLeftNode, mStartOfRightNode.GetContainer(), error);
  NS_WARNING_ASSERTION(!error.Failed(), "nsINode::InsertBefore() failed");
  return error.StealNSResult();
}

nsIContent* SplitNodeTransaction::GetNewNode() { return mNewLeftNode; }

}  // namespace mozilla
