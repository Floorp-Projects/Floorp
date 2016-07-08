/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InsertNodeTransaction.h"

#include "mozilla/dom/Selection.h"      // for Selection

#include "nsAString.h"
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsEditor.h"                   // for nsEditor
#include "nsError.h"                    // for NS_ERROR_NULL_POINTER, etc
#include "nsIContent.h"                 // for nsIContent
#include "nsMemory.h"                   // for nsMemory
#include "nsReadableUtils.h"            // for ToNewCString
#include "nsString.h"                   // for nsString

namespace mozilla {

using namespace dom;

InsertNodeTransaction::InsertNodeTransaction(nsIContent& aNode,
                                             nsINode& aParent,
                                             int32_t aOffset,
                                             nsEditor& aEditor)
  : mNode(&aNode)
  , mParent(&aParent)
  , mOffset(aOffset)
  , mEditor(aEditor)
{
}

InsertNodeTransaction::~InsertNodeTransaction()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(InsertNodeTransaction, EditTransactionBase,
                                   mNode,
                                   mParent)

NS_IMPL_ADDREF_INHERITED(InsertNodeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(InsertNodeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InsertNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP
InsertNodeTransaction::DoTransaction()
{
  MOZ_ASSERT(mNode && mParent);

  uint32_t count = mParent->GetChildCount();
  if (mOffset > static_cast<int32_t>(count) || mOffset == -1) {
    // -1 is sentinel value meaning "append at end"
    mOffset = count;
  }

  // Note, it's ok for ref to be null. That means append.
  nsCOMPtr<nsIContent> ref = mParent->GetChildAt(mOffset);

  mEditor.MarkNodeDirty(GetAsDOMNode(mNode));

  ErrorResult rv;
  mParent->InsertBefore(*mNode, ref, rv);
  NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());

  // Only set selection to insertion point if editor gives permission
  if (mEditor.GetShouldTxnSetSelection()) {
    RefPtr<Selection> selection = mEditor.GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    // Place the selection just after the inserted element
    selection->Collapse(mParent, mOffset + 1);
  } else {
    // Do nothing - DOM Range gravity will adjust selection
  }
  return NS_OK;
}

NS_IMETHODIMP
InsertNodeTransaction::UndoTransaction()
{
  MOZ_ASSERT(mNode && mParent);

  ErrorResult rv;
  mParent->RemoveChild(*mNode, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
InsertNodeTransaction::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("InsertNodeTransaction");
  return NS_OK;
}

} // namespace mozilla
