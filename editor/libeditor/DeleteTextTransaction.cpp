/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteTextTransaction.h"

#include "mozilla/Assertions.h"
#include "mozilla/EditorBase.h"
#include "mozilla/SelectionState.h"
#include "mozilla/dom/Selection.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIEditor.h"
#include "nsISupportsImpl.h"
#include "nsAString.h"

namespace mozilla {

using namespace dom;

DeleteTextTransaction::DeleteTextTransaction(
                         EditorBase& aEditorBase,
                         nsGenericDOMDataNode& aCharData,
                         uint32_t aOffset,
                         uint32_t aNumCharsToDelete,
                         RangeUpdater* aRangeUpdater)
  : mEditorBase(aEditorBase)
  , mCharData(&aCharData)
  , mOffset(aOffset)
  , mNumCharsToDelete(aNumCharsToDelete)
  , mRangeUpdater(aRangeUpdater)
{
  NS_ASSERTION(mCharData->Length() >= aOffset + aNumCharsToDelete,
               "Trying to delete more characters than in node");
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeleteTextTransaction, EditTransactionBase,
                                   mCharData)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteTextTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

nsresult
DeleteTextTransaction::Init()
{
  // Do nothing if the node is read-only
  if (!mEditorBase.IsModifiableNode(mCharData)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
DeleteTextTransaction::DoTransaction()
{
  MOZ_ASSERT(mCharData);

  // Get the text that we're about to delete
  nsresult res = mCharData->SubstringData(mOffset, mNumCharsToDelete,
                                          mDeletedText);
  MOZ_ASSERT(NS_SUCCEEDED(res));
  res = mCharData->DeleteData(mOffset, mNumCharsToDelete);
  NS_ENSURE_SUCCESS(res, res);

  if (mRangeUpdater) {
    mRangeUpdater->SelAdjDeleteText(mCharData, mOffset, mNumCharsToDelete);
  }

  // Only set selection to deletion point if editor gives permission
  if (mEditorBase.GetShouldTxnSetSelection()) {
    RefPtr<Selection> selection = mEditorBase.GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    res = selection->Collapse(mCharData, mOffset);
    NS_ASSERTION(NS_SUCCEEDED(res),
                 "Selection could not be collapsed after undo of deletetext");
    NS_ENSURE_SUCCESS(res, res);
  }
  // Else do nothing - DOM Range gravity will adjust selection
  return NS_OK;
}

//XXX: We may want to store the selection state and restore it properly.  Was
//     it an insertion point or an extended selection?
NS_IMETHODIMP
DeleteTextTransaction::UndoTransaction()
{
  MOZ_ASSERT(mCharData);

  return mCharData->InsertData(mOffset, mDeletedText);
}

NS_IMETHODIMP
DeleteTextTransaction::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("DeleteTextTransaction: ");
  aString += mDeletedText;
  return NS_OK;
}

} // namespace mozilla
