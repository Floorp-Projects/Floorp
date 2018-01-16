/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteTextTransaction.h"

#include "mozilla/Assertions.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/SelectionState.h"
#include "mozilla/dom/Selection.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIEditor.h"
#include "nsISupportsImpl.h"
#include "nsAString.h"

namespace mozilla {

using namespace dom;

// static
already_AddRefed<DeleteTextTransaction>
DeleteTextTransaction::MaybeCreate(EditorBase& aEditorBase,
                                   nsGenericDOMDataNode& aCharData,
                                   uint32_t aOffset,
                                   uint32_t aLengthToDelete)
{
  RefPtr<DeleteTextTransaction> transaction =
    new DeleteTextTransaction(aEditorBase, aCharData, aOffset, aLengthToDelete);
  return transaction.forget();
}

// static
already_AddRefed<DeleteTextTransaction>
DeleteTextTransaction::MaybeCreateForPreviousCharacter(
                         EditorBase& aEditorBase,
                         nsGenericDOMDataNode& aCharData,
                         uint32_t aOffset)
{
  if (NS_WARN_IF(!aOffset)) {
    return nullptr;
  }

  nsAutoString data;
  aCharData.GetData(data);
  if (NS_WARN_IF(data.IsEmpty())) {
    return nullptr;
  }

  uint32_t length = 1;
  uint32_t offset = aOffset - 1;
  if (offset &&
      NS_IS_LOW_SURROGATE(data[offset]) &&
      NS_IS_HIGH_SURROGATE(data[offset - 1])) {
    ++length;
    --offset;
  }
  return DeleteTextTransaction::MaybeCreate(aEditorBase, aCharData,
                                            offset, length);
}

// static
already_AddRefed<DeleteTextTransaction>
DeleteTextTransaction::MaybeCreateForNextCharacter(
                         EditorBase& aEditorBase,
                         nsGenericDOMDataNode& aCharData,
                         uint32_t aOffset)
{
  nsAutoString data;
  aCharData.GetData(data);
  if (NS_WARN_IF(aOffset >= data.Length()) ||
      NS_WARN_IF(data.IsEmpty())) {
    return nullptr;
  }

  uint32_t length = 1;
  if (aOffset + 1 < data.Length() &&
      NS_IS_HIGH_SURROGATE(data[aOffset]) &&
      NS_IS_LOW_SURROGATE(data[aOffset + 1])) {
    ++length;
  }
  return DeleteTextTransaction::MaybeCreate(aEditorBase, aCharData,
                                            aOffset, length);
}

DeleteTextTransaction::DeleteTextTransaction(
                         EditorBase& aEditorBase,
                         nsGenericDOMDataNode& aCharData,
                         uint32_t aOffset,
                         uint32_t aLengthToDelete)
  : mEditorBase(&aEditorBase)
  , mCharData(&aCharData)
  , mOffset(aOffset)
  , mLengthToDelete(aLengthToDelete)
{
  NS_ASSERTION(mCharData->Length() >= aOffset + aLengthToDelete,
               "Trying to delete more characters than in node");
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeleteTextTransaction, EditTransactionBase,
                                   mEditorBase,
                                   mCharData)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteTextTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

bool
DeleteTextTransaction::CanDoIt() const
{
  if (NS_WARN_IF(!mCharData) || NS_WARN_IF(!mEditorBase)) {
    return false;
  }
  return mEditorBase->IsModifiableNode(mCharData);
}

NS_IMETHODIMP
DeleteTextTransaction::DoTransaction()
{
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mCharData)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Get the text that we're about to delete
  nsresult rv = mCharData->SubstringData(mOffset, mLengthToDelete,
                                         mDeletedText);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  rv = mCharData->DeleteData(mOffset, mLengthToDelete);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mEditorBase->RangeUpdaterRef().
                 SelAdjDeleteText(mCharData, mOffset, mLengthToDelete);

  // Only set selection to deletion point if editor gives permission
  if (mEditorBase->GetShouldTxnSetSelection()) {
    RefPtr<Selection> selection = mEditorBase->GetSelection();
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_FAILURE;
    }
    ErrorResult error;
    selection->Collapse(EditorRawDOMPoint(mCharData, mOffset), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }
  // Else do nothing - DOM Range gravity will adjust selection
  return NS_OK;
}

//XXX: We may want to store the selection state and restore it properly.  Was
//     it an insertion point or an extended selection?
NS_IMETHODIMP
DeleteTextTransaction::UndoTransaction()
{
  if (NS_WARN_IF(!mCharData)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return mCharData->InsertData(mOffset, mDeletedText);
}

} // namespace mozilla
