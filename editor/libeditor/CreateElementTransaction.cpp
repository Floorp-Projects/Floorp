/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CreateElementTransaction.h"

#include <algorithm>
#include <stdio.h>

#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"

#include "mozilla/Casting.h"
#include "mozilla/EditorBase.h"

#include "nsAlgorithm.h"
#include "nsAString.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIEditor.h"
#include "nsINode.h"
#include "nsISupportsUtils.h"
#include "nsMemory.h"
#include "nsReadableUtils.h"
#include "nsStringFwd.h"
#include "nsString.h"

namespace mozilla {

using namespace dom;

CreateElementTransaction::CreateElementTransaction(EditorBase& aEditorBase,
                                                   nsIAtom& aTag,
                                                   nsINode& aParent,
                                                   int32_t aOffsetInParent)
  : EditTransactionBase()
  , mEditorBase(&aEditorBase)
  , mTag(&aTag)
  , mParent(&aParent)
  , mOffsetInParent(aOffsetInParent)
{
}

CreateElementTransaction::~CreateElementTransaction()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(CreateElementTransaction,
                                   EditTransactionBase,
                                   mParent,
                                   mNewNode,
                                   mRefNode)

NS_IMPL_ADDREF_INHERITED(CreateElementTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(CreateElementTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CreateElementTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)


NS_IMETHODIMP
CreateElementTransaction::DoTransaction()
{
  MOZ_ASSERT(mEditorBase && mTag && mParent);

  mNewNode = mEditorBase->CreateHTMLContent(mTag);
  NS_ENSURE_STATE(mNewNode);

  // Try to insert formatting whitespace for the new node:
  mEditorBase->MarkNodeDirty(GetAsDOMNode(mNewNode));

  // Insert the new node
  ErrorResult rv;
  if (mOffsetInParent == -1) {
    mParent->AppendChild(*mNewNode, rv);
    return rv.StealNSResult();
  }

  mOffsetInParent = std::min(mOffsetInParent,
                             static_cast<int32_t>(mParent->GetChildCount()));

  // Note, it's ok for mRefNode to be null. That means append
  mRefNode = mParent->GetChildAt(mOffsetInParent);

  nsCOMPtr<nsIContent> refNode = mRefNode;
  mParent->InsertBefore(*mNewNode, refNode, rv);
  NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());

  // Only set selection to insertion point if editor gives permission
  if (!mEditorBase->GetShouldTxnSetSelection()) {
    // Do nothing - DOM range gravity will adjust selection
    return NS_OK;
  }

  RefPtr<Selection> selection = mEditorBase->GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  rv = selection->CollapseNative(mParent, mParent->IndexOf(mNewNode) + 1);
  NS_ASSERTION(!rv.Failed(),
               "selection could not be collapsed after insert");
  return NS_OK;
}

NS_IMETHODIMP
CreateElementTransaction::UndoTransaction()
{
  MOZ_ASSERT(mEditorBase && mParent);

  ErrorResult rv;
  mParent->RemoveChild(*mNewNode, rv);

  return rv.StealNSResult();
}

NS_IMETHODIMP
CreateElementTransaction::RedoTransaction()
{
  MOZ_ASSERT(mEditorBase && mParent);

  // First, reset mNewNode so it has no attributes or content
  // XXX We never actually did this, we only cleared mNewNode's contents if it
  // was a CharacterData node (which it's not, it's an Element)

  // Now, reinsert mNewNode
  ErrorResult rv;
  nsCOMPtr<nsIContent> refNode = mRefNode;
  mParent->InsertBefore(*mNewNode, refNode, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
CreateElementTransaction::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("CreateElementTransaction: ");
  aString += nsDependentAtomString(mTag);
  return NS_OK;
}

already_AddRefed<Element>
CreateElementTransaction::GetNewNode()
{
  return nsCOMPtr<Element>(mNewNode).forget();
}

} // namespace mozilla
