/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SplitNodeTransaction.h"

#include "mozilla/EditorBase.h"         // for EditorBase
#include "mozilla/EditorDOMPoint.h"     // for RangeBoundary, EditorRawDOMPoint
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_ASSERTION, etc.
#include "nsError.h"                    // for NS_ERROR_NOT_INITIALIZED, etc.
#include "nsIContent.h"                 // for nsIContent

namespace mozilla {

using namespace dom;

// static
already_AddRefed<SplitNodeTransaction>
SplitNodeTransaction::Create(EditorBase& aEditorBase,
                             const EditorRawDOMPoint& aStartOfRightNode)
{
  RefPtr<SplitNodeTransaction> transaction =
    new SplitNodeTransaction(aEditorBase, aStartOfRightNode);
  return transaction.forget();
}

SplitNodeTransaction::SplitNodeTransaction(
                        EditorBase& aEditorBase,
                        const EditorRawDOMPoint& aStartOfRightNode)
  : mEditorBase(&aEditorBase)
  , mStartOfRightNode(aStartOfRightNode)
{
  MOZ_DIAGNOSTIC_ASSERT(aStartOfRightNode.IsSet());
  MOZ_DIAGNOSTIC_ASSERT(aStartOfRightNode.GetContainerAsContent());
}

SplitNodeTransaction::~SplitNodeTransaction()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(SplitNodeTransaction, EditTransactionBase,
                                   mEditorBase,
                                   mStartOfRightNode,
                                   mParent,
                                   mNewLeftNode)

NS_IMPL_ADDREF_INHERITED(SplitNodeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(SplitNodeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SplitNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP
SplitNodeTransaction::DoTransaction()
{
  if (NS_WARN_IF(!mEditorBase) ||
      NS_WARN_IF(!mStartOfRightNode.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  MOZ_ASSERT(mStartOfRightNode.IsSetAndValid());

  // Create a new node
  ErrorResult error;
  // Don't use .downcast directly because AsContent has an assertion we want
  nsCOMPtr<nsINode> clone =
    mStartOfRightNode.GetContainer()->CloneNode(false, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  if (NS_WARN_IF(!clone)) {
    return NS_ERROR_UNEXPECTED;
  }
  mNewLeftNode = dont_AddRef(clone.forget().take()->AsContent());
  mEditorBase->MarkNodeDirty(mStartOfRightNode.GetContainerAsDOMNode());

  // Get the parent node
  mParent = mStartOfRightNode.GetContainer()->GetParentNode();
  if (NS_WARN_IF(!mParent)) {
    return NS_ERROR_FAILURE;
  }

  // Insert the new node
  mEditorBase->SplitNodeImpl(EditorDOMPoint(mStartOfRightNode),
                             *mNewLeftNode, error);
  // XXX Really odd.  The result of SplitNodeImpl() is respected only when
  //     we shouldn't set selection.  Otherwise, it's overridden by the
  //     result of Selection.Collapse().
  if (mEditorBase->GetShouldTxnSetSelection()) {
    NS_WARNING_ASSERTION(!mEditorBase->Destroyed(),
      "The editor has gone but SplitNodeTransaction keeps trying to modify "
      "Selection");
    RefPtr<Selection> selection = mEditorBase->GetSelection();
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_FAILURE;
    }
    if (NS_WARN_IF(error.Failed())) {
      // XXX This must be a bug.
      error.SuppressException();
    }
    MOZ_ASSERT(mStartOfRightNode.Offset() == mNewLeftNode->Length());
    EditorRawDOMPoint atEndOfLeftNode;
    atEndOfLeftNode.SetToEndOf(mNewLeftNode);
    selection->Collapse(atEndOfLeftNode, error);
  }

  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

NS_IMETHODIMP
SplitNodeTransaction::UndoTransaction()
{
  if (NS_WARN_IF(!mEditorBase) ||
      NS_WARN_IF(!mNewLeftNode) ||
      NS_WARN_IF(!mParent) ||
      NS_WARN_IF(!mStartOfRightNode.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // This assumes Do inserted the new node in front of the prior existing node
  // XXX Perhaps, we should reset mStartOfRightNode with current first child
  //     of the right node.
  return mEditorBase->JoinNodesImpl(mStartOfRightNode.GetContainer(),
                                    mNewLeftNode, mParent);
}

/* Redo cannot simply resplit the right node, because subsequent transactions
 * on the redo stack may depend on the left node existing in its previous
 * state.
 */
NS_IMETHODIMP
SplitNodeTransaction::RedoTransaction()
{
  if (NS_WARN_IF(!mNewLeftNode) ||
      NS_WARN_IF(!mParent) ||
      NS_WARN_IF(!mStartOfRightNode.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // First, massage the existing node so it is in its post-split state
  if (mStartOfRightNode.IsInTextNode()) {
    Text* rightNodeAsText = mStartOfRightNode.GetContainerAsText();
    MOZ_DIAGNOSTIC_ASSERT(rightNodeAsText);
    nsresult rv =
      rightNodeAsText->DeleteData(0, mStartOfRightNode.Offset());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    nsCOMPtr<nsIContent> child =
      mStartOfRightNode.GetContainer()->GetFirstChild();
    nsCOMPtr<nsIContent> nextSibling;
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
      ErrorResult error;
      mStartOfRightNode.GetContainer()->RemoveChild(*child, error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
      mNewLeftNode->AppendChild(*child, error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
      child = nextSibling;
    }
  }
  // Second, re-insert the left node into the tree
  ErrorResult error;
  mParent->InsertBefore(*mNewLeftNode, mStartOfRightNode.GetContainer(), error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

nsIContent*
SplitNodeTransaction::GetNewNode()
{
  return mNewLeftNode;
}

} // namespace mozilla
