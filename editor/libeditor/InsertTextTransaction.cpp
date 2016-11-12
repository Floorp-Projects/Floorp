/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InsertTextTransaction.h"

#include "mozilla/EditorBase.h"         // mEditorBase
#include "mozilla/SelectionState.h"     // RangeUpdater
#include "mozilla/dom/Selection.h"      // Selection local var
#include "mozilla/dom/Text.h"           // mTextNode
#include "nsAString.h"                  // nsAString parameter
#include "nsDebug.h"                    // for NS_ASSERTION, etc.
#include "nsError.h"                    // for NS_OK, etc.
#include "nsQueryObject.h"              // for do_QueryObject

namespace mozilla {

using namespace dom;

InsertTextTransaction::InsertTextTransaction(Text& aTextNode,
                                             uint32_t aOffset,
                                             const nsAString& aStringToInsert,
                                             EditorBase& aEditorBase,
                                             RangeUpdater* aRangeUpdater)
  : mTextNode(&aTextNode)
  , mOffset(aOffset)
  , mStringToInsert(aStringToInsert)
  , mEditorBase(aEditorBase)
  , mRangeUpdater(aRangeUpdater)
{
}

InsertTextTransaction::~InsertTextTransaction()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(InsertTextTransaction, EditTransactionBase,
                                   mTextNode)

NS_IMPL_ADDREF_INHERITED(InsertTextTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(InsertTextTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InsertTextTransaction)
  if (aIID.Equals(NS_GET_IID(InsertTextTransaction))) {
    foundInterface = static_cast<nsITransaction*>(this);
  } else
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)


NS_IMETHODIMP
InsertTextTransaction::DoTransaction()
{
  nsresult rv = mTextNode->InsertData(mOffset, mStringToInsert);
  NS_ENSURE_SUCCESS(rv, rv);

  // Only set selection to insertion point if editor gives permission
  if (mEditorBase.GetShouldTxnSetSelection()) {
    RefPtr<Selection> selection = mEditorBase.GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    DebugOnly<nsresult> rv =
      selection->Collapse(mTextNode, mOffset + mStringToInsert.Length());
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Selection could not be collapsed after insert");
  } else {
    // Do nothing - DOM Range gravity will adjust selection
  }
  mRangeUpdater->SelAdjInsertText(*mTextNode, mOffset, mStringToInsert);

  return NS_OK;
}

NS_IMETHODIMP
InsertTextTransaction::UndoTransaction()
{
  return mTextNode->DeleteData(mOffset, mStringToInsert.Length());
}

NS_IMETHODIMP
InsertTextTransaction::Merge(nsITransaction* aTransaction,
                             bool* aDidMerge)
{
  if (!aTransaction || !aDidMerge) {
    return NS_OK;
  }
  // Set out param default value
  *aDidMerge = false;

  // If aTransaction is a InsertTextTransaction, and if the selection hasn't
  // changed, then absorb it.
  RefPtr<InsertTextTransaction> otherTransaction = do_QueryObject(aTransaction);
  if (otherTransaction && IsSequentialInsert(*otherTransaction)) {
    nsAutoString otherData;
    otherTransaction->GetData(otherData);
    mStringToInsert += otherData;
    *aDidMerge = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
InsertTextTransaction::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("InsertTextTransaction: ");
  aString += mStringToInsert;
  return NS_OK;
}

/* ============ private methods ================== */

void
InsertTextTransaction::GetData(nsString& aResult)
{
  aResult = mStringToInsert;
}

bool
InsertTextTransaction::IsSequentialInsert(
                         InsertTextTransaction& aOtherTransaction)
{
  return aOtherTransaction.mTextNode == mTextNode &&
         aOtherTransaction.mOffset == mOffset + mStringToInsert.Length();
}

} // namespace mozilla
