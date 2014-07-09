/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "CreateElementTxn.h"
#include "mozilla/dom/Element.h"
#include "nsAlgorithm.h"
#include "nsDebug.h"
#include "nsEditor.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIDOMCharacterData.h"
#include "nsIEditor.h"
#include "nsINode.h"
#include "nsISelection.h"
#include "nsISupportsUtils.h"
#include "nsMemory.h"
#include "nsReadableUtils.h"
#include "nsStringFwd.h"
#include "nsString.h"
#include "nsAString.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

CreateElementTxn::CreateElementTxn()
  : EditTxn()
{
}

CreateElementTxn::~CreateElementTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(CreateElementTxn, EditTxn,
                                   mParent,
                                   mNewNode,
                                   mRefNode)

NS_IMPL_ADDREF_INHERITED(CreateElementTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(CreateElementTxn, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CreateElementTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)
NS_IMETHODIMP CreateElementTxn::Init(nsEditor      *aEditor,
                                     const nsAString &aTag,
                                     nsIDOMNode     *aParent,
                                     uint32_t        aOffsetInParent)
{
  NS_ASSERTION(aEditor&&aParent, "null args");
  if (!aEditor || !aParent) { return NS_ERROR_NULL_POINTER; }

  mEditor = aEditor;
  mTag = aTag;
  mParent = do_QueryInterface(aParent);
  mOffsetInParent = aOffsetInParent;
  return NS_OK;
}


NS_IMETHODIMP CreateElementTxn::DoTransaction(void)
{
  NS_ASSERTION(mEditor && mParent, "bad state");
  NS_ENSURE_TRUE(mEditor && mParent, NS_ERROR_NOT_INITIALIZED);

  ErrorResult rv;
  nsCOMPtr<Element> newContent = mEditor->CreateHTMLContent(mTag, rv);
  NS_ENSURE_SUCCESS(rv.ErrorCode(), rv.ErrorCode());
  NS_ENSURE_STATE(newContent);

  mNewNode = newContent->AsDOMNode();
  // Try to insert formatting whitespace for the new node:
  mEditor->MarkNodeDirty(mNewNode);

  // insert the new node
  if (CreateElementTxn::eAppend == int32_t(mOffsetInParent)) {
    nsCOMPtr<nsIDOMNode> resultNode;
    return mParent->AppendChild(mNewNode, getter_AddRefs(resultNode));
  }

  nsCOMPtr<nsINode> parent = do_QueryInterface(mParent);
  NS_ENSURE_STATE(parent);

  mOffsetInParent = std::min(mOffsetInParent, parent->GetChildCount());

  // note, it's ok for mRefNode to be null.  that means append
  nsIContent* refNode = parent->GetChildAt(mOffsetInParent);
  mRefNode = refNode ? refNode->AsDOMNode() : nullptr;

  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->InsertBefore(mNewNode, mRefNode, getter_AddRefs(resultNode));
  NS_ENSURE_SUCCESS(result, result); 

  // only set selection to insertion point if editor gives permission
  bool bAdjustSelection;
  mEditor->ShouldTxnSetSelection(&bAdjustSelection);
  if (!bAdjustSelection) {
    // do nothing - dom range gravity will adjust selection
    return NS_OK;
  }

  nsCOMPtr<nsISelection> selection;
  result = mEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContent> parentContent = do_QueryInterface(mParent);
  NS_ENSURE_STATE(parentContent);

  result = selection->CollapseNative(parentContent,
                                     parentContent->IndexOf(newContent) + 1);
  NS_ASSERTION((NS_SUCCEEDED(result)), "selection could not be collapsed after insert.");
  return result;
}

NS_IMETHODIMP CreateElementTxn::UndoTransaction(void)
{
  NS_ASSERTION(mEditor && mParent, "bad state");
  NS_ENSURE_TRUE(mEditor && mParent, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDOMNode> resultNode;
  return mParent->RemoveChild(mNewNode, getter_AddRefs(resultNode));
}

NS_IMETHODIMP CreateElementTxn::RedoTransaction(void)
{
  NS_ASSERTION(mEditor && mParent, "bad state");
  NS_ENSURE_TRUE(mEditor && mParent, NS_ERROR_NOT_INITIALIZED);

  // first, reset mNewNode so it has no attributes or content
  nsCOMPtr<nsIDOMCharacterData>nodeAsText = do_QueryInterface(mNewNode);
  if (nodeAsText)
  {
    nodeAsText->SetData(EmptyString());
  }
  
  // now, reinsert mNewNode
  nsCOMPtr<nsIDOMNode> resultNode;
  return mParent->InsertBefore(mNewNode, mRefNode, getter_AddRefs(resultNode));
}

NS_IMETHODIMP CreateElementTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("CreateElementTxn: ");
  aString += mTag;
  return NS_OK;
}

NS_IMETHODIMP CreateElementTxn::GetNewNode(nsIDOMNode **aNewNode)
{
  NS_ENSURE_TRUE(aNewNode, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(mNewNode, NS_ERROR_NOT_INITIALIZED);
  *aNewNode = mNewNode;
  NS_ADDREF(*aNewNode);
  return NS_OK;
}
