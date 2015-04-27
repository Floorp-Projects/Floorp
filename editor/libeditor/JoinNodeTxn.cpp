/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JoinNodeTxn.h"
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsEditor.h"                   // for nsEditor
#include "nsError.h"                    // for NS_ERROR_NULL_POINTER, etc
#include "nsIContent.h"                 // for nsIContent
#include "nsIDOMCharacterData.h"        // for nsIDOMCharacterData
#include "nsIEditor.h"                  // for nsEditor::IsModifiableNode
#include "nsISupportsImpl.h"            // for EditTxn::QueryInterface, etc

using namespace mozilla;
using namespace mozilla::dom;

JoinNodeTxn::JoinNodeTxn(nsEditor& aEditor, nsINode& aLeftNode,
                         nsINode& aRightNode)
  : EditTxn()
  , mEditor(aEditor)
  , mLeftNode(&aLeftNode)
  , mRightNode(&aRightNode)
  , mOffset(0)
  , mParent(nullptr)
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(JoinNodeTxn, EditTxn,
                                   mLeftNode,
                                   mRightNode,
                                   mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JoinNodeTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

nsresult
JoinNodeTxn::CheckValidity()
{
  if (!mEditor.IsModifiableNode(mLeftNode->GetParentNode())) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// After DoTransaction() and RedoTransaction(), the left node is removed from
// the content tree and right node remains.
NS_IMETHODIMP
JoinNodeTxn::DoTransaction()
{
  // Get the parent node
  nsCOMPtr<nsINode> leftParent = mLeftNode->GetParentNode();
  NS_ENSURE_TRUE(leftParent, NS_ERROR_NULL_POINTER);

  // Verify that mLeftNode and mRightNode have the same parent
  if (leftParent != mRightNode->GetParentNode()) {
    NS_ASSERTION(false, "Nodes do not have same parent");
    return NS_ERROR_INVALID_ARG;
  }

  // Set this instance's mParent.  Other methods will see a non-null mParent
  // and know all is well
  mParent = leftParent;
  mOffset = mLeftNode->Length();

  return mEditor.JoinNodesImpl(mRightNode, mLeftNode, mParent);
}

//XXX: What if instead of split, we just deleted the unneeded children of
//     mRight and re-inserted mLeft?
NS_IMETHODIMP
JoinNodeTxn::UndoTransaction()
{
  MOZ_ASSERT(mParent);

  // First, massage the existing node so it is in its post-split state
  ErrorResult rv;
  if (mRightNode->GetAsText()) {
    rv = mRightNode->GetAsText()->DeleteData(0, mOffset);
  } else {
    nsCOMPtr<nsIContent> child = mRightNode->GetFirstChild();
    for (uint32_t i = 0; i < mOffset; i++) {
      if (rv.Failed()) {
        return rv.StealNSResult();
      }
      if (!child) {
        return NS_ERROR_NULL_POINTER;
      }
      nsCOMPtr<nsIContent> nextSibling = child->GetNextSibling();
      mLeftNode->AppendChild(*child, rv);
      child = nextSibling;
    }
  }
  // Second, re-insert the left node into the tree
  mParent->InsertBefore(*mLeftNode, mRightNode, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
JoinNodeTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("JoinNodeTxn");
  return NS_OK;
}
