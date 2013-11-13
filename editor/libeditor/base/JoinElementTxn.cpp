/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>                      // for printf

#include "JoinElementTxn.h"
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsEditor.h"                   // for nsEditor
#include "nsError.h"                    // for NS_ERROR_NULL_POINTER, etc
#include "nsIContent.h"                 // for nsIContent
#include "nsIDOMCharacterData.h"        // for nsIDOMCharacterData
#include "nsIEditor.h"                  // for nsEditor::IsModifiableNode
#include "nsISupportsImpl.h"            // for EditTxn::QueryInterface, etc

using namespace mozilla;

JoinElementTxn::JoinElementTxn()
  : EditTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED_3(JoinElementTxn, EditTxn,
                                     mLeftNode,
                                     mRightNode,
                                     mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JoinElementTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP JoinElementTxn::Init(nsEditor   *aEditor,
                                   nsINode *aLeftNode,
                                   nsINode *aRightNode)
{
  NS_PRECONDITION((aEditor && aLeftNode && aRightNode), "null arg");
  if (!aEditor || !aLeftNode || !aRightNode) { return NS_ERROR_NULL_POINTER; }
  mEditor = aEditor;
  mLeftNode = aLeftNode;
  nsCOMPtr<nsINode> leftParent = mLeftNode->GetParentNode();
  if (!mEditor->IsModifiableNode(leftParent)) {
    return NS_ERROR_FAILURE;
  }
  mRightNode = aRightNode;
  mOffset = 0;
  return NS_OK;
}

// After DoTransaction() and RedoTransaction(), the left node is removed from the content tree and right node remains.
NS_IMETHODIMP JoinElementTxn::DoTransaction(void)
{
  NS_PRECONDITION((mEditor && mLeftNode && mRightNode), "null arg");
  if (!mEditor || !mLeftNode || !mRightNode) { return NS_ERROR_NOT_INITIALIZED; }

  // get the parent node
  nsCOMPtr<nsINode> leftParent = mLeftNode->GetParentNode();
  NS_ENSURE_TRUE(leftParent, NS_ERROR_NULL_POINTER);

  // verify that mLeftNode and mRightNode have the same parent
  nsCOMPtr<nsINode> rightParent = mRightNode->GetParentNode();
  NS_ENSURE_TRUE(rightParent, NS_ERROR_NULL_POINTER);

  if (leftParent != rightParent) {
    NS_ASSERTION(false, "2 nodes do not have same parent");
    return NS_ERROR_INVALID_ARG;
  }

  // set this instance mParent. 
  // Other methods will see a non-null mParent and know all is well
  mParent = leftParent;
  mOffset = mLeftNode->Length();

  return mEditor->JoinNodesImpl(mRightNode, mLeftNode, mParent);
}

//XXX: what if instead of split, we just deleted the unneeded children of mRight
//     and re-inserted mLeft?
NS_IMETHODIMP JoinElementTxn::UndoTransaction(void)
{
  NS_ASSERTION(mRightNode && mLeftNode && mParent, "bad state");
  if (!mRightNode || !mLeftNode || !mParent) { return NS_ERROR_NOT_INITIALIZED; }
  // first, massage the existing node so it is in its post-split state
  nsCOMPtr<nsIDOMCharacterData>rightNodeAsText = do_QueryInterface(mRightNode);
  ErrorResult rv;
  if (rightNodeAsText)
  {
    rv = rightNodeAsText->DeleteData(0, mOffset);
    NS_ENSURE_SUCCESS(rv.ErrorCode(), rv.ErrorCode());
  }
  else
  {
    for (nsCOMPtr<nsINode> child = mRightNode->GetFirstChild();
         child;
         child = child->GetNextSibling())
    {
      mLeftNode->AppendChild(*child, rv);
      NS_ENSURE_SUCCESS(rv.ErrorCode(), rv.ErrorCode());
    }
  }
  // second, re-insert the left node into the tree
  mParent->InsertBefore(*mLeftNode, mRightNode, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP JoinElementTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("JoinElementTxn");
  return NS_OK;
}
