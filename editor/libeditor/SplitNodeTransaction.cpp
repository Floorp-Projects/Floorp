/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SplitNodeTransaction.h"

#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsEditor.h"                   // for nsEditor
#include "nsError.h"                    // for NS_ERROR_NOT_INITIALIZED, etc
#include "nsIContent.h"                 // for nsIContent

namespace mozilla {

using namespace dom;

SplitNodeTransaction::SplitNodeTransaction(nsEditor& aEditor,
                                           nsIContent& aNode,
                                           int32_t aOffset)
  : mEditor(aEditor)
  , mExistingRightNode(&aNode)
  , mOffset(aOffset)
{
}

SplitNodeTransaction::~SplitNodeTransaction()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(SplitNodeTransaction, EditTxn,
                                   mParent,
                                   mNewLeftNode)

NS_IMPL_ADDREF_INHERITED(SplitNodeTransaction, EditTxn)
NS_IMPL_RELEASE_INHERITED(SplitNodeTransaction, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SplitNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP
SplitNodeTransaction::DoTransaction()
{
  // Create a new node
  ErrorResult rv;
  // Don't use .downcast directly because AsContent has an assertion we want
  nsCOMPtr<nsINode> clone = mExistingRightNode->CloneNode(false, rv);
  NS_ASSERTION(!rv.Failed() && clone, "Could not create clone");
  NS_ENSURE_TRUE(!rv.Failed() && clone, rv.StealNSResult());
  mNewLeftNode = dont_AddRef(clone.forget().take()->AsContent());
  mEditor.MarkNodeDirty(mExistingRightNode->AsDOMNode());

  // Get the parent node
  mParent = mExistingRightNode->GetParentNode();
  NS_ENSURE_TRUE(mParent, NS_ERROR_NULL_POINTER);

  // Insert the new node
  rv = mEditor.SplitNodeImpl(*mExistingRightNode, mOffset, *mNewLeftNode);
  if (mEditor.GetShouldTxnSetSelection()) {
    RefPtr<Selection> selection = mEditor.GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    rv = selection->Collapse(mNewLeftNode, mOffset);
  }
  return rv.StealNSResult();
}

NS_IMETHODIMP
SplitNodeTransaction::UndoTransaction()
{
  MOZ_ASSERT(mNewLeftNode && mParent);

  // This assumes Do inserted the new node in front of the prior existing node
  return mEditor.JoinNodesImpl(mExistingRightNode, mNewLeftNode, mParent);
}

/* Redo cannot simply resplit the right node, because subsequent transactions
 * on the redo stack may depend on the left node existing in its previous
 * state.
 */
NS_IMETHODIMP
SplitNodeTransaction::RedoTransaction()
{
  MOZ_ASSERT(mNewLeftNode && mParent);

  ErrorResult rv;
  // First, massage the existing node so it is in its post-split state
  if (mExistingRightNode->GetAsText()) {
    rv = mExistingRightNode->GetAsText()->DeleteData(0, mOffset);
    NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());
  } else {
    nsCOMPtr<nsIContent> child = mExistingRightNode->GetFirstChild();
    nsCOMPtr<nsIContent> nextSibling;
    for (int32_t i=0; i < mOffset; i++) {
      if (rv.Failed()) {
        return rv.StealNSResult();
      }
      if (!child) {
        return NS_ERROR_NULL_POINTER;
      }
      nextSibling = child->GetNextSibling();
      mExistingRightNode->RemoveChild(*child, rv);
      if (!rv.Failed()) {
        mNewLeftNode->AppendChild(*child, rv);
      }
      child = nextSibling;
    }
  }
  // Second, re-insert the left node into the tree
  mParent->InsertBefore(*mNewLeftNode, mExistingRightNode, rv);
  return rv.StealNSResult();
}


NS_IMETHODIMP
SplitNodeTransaction::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("SplitNodeTransaction");
  return NS_OK;
}

nsIContent*
SplitNodeTransaction::GetNewNode()
{
  return mNewLeftNode;
}

} // namespace mozilla
