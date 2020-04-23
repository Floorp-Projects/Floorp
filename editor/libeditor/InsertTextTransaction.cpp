/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InsertTextTransaction.h"

#include "mozilla/EditorBase.h"      // mEditorBase
#include "mozilla/SelectionState.h"  // RangeUpdater
#include "mozilla/dom/Selection.h"   // Selection local var
#include "mozilla/dom/Text.h"        // mTextNode
#include "nsAString.h"               // nsAString parameter
#include "nsDebug.h"                 // for NS_ASSERTION, etc.
#include "nsError.h"                 // for NS_OK, etc.
#include "nsQueryObject.h"           // for do_QueryObject

namespace mozilla {

using namespace dom;

// static
already_AddRefed<InsertTextTransaction> InsertTextTransaction::Create(
    EditorBase& aEditorBase, const nsAString& aStringToInsert,
    const EditorDOMPointInText& aPointToInsert) {
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());
  RefPtr<InsertTextTransaction> transaction =
      new InsertTextTransaction(aEditorBase, aStringToInsert, aPointToInsert);
  return transaction.forget();
}

InsertTextTransaction::InsertTextTransaction(
    EditorBase& aEditorBase, const nsAString& aStringToInsert,
    const EditorDOMPointInText& aPointToInsert)
    : mTextNode(aPointToInsert.ContainerAsText()),
      mOffset(aPointToInsert.Offset()),
      mStringToInsert(aStringToInsert),
      mEditorBase(&aEditorBase) {}

NS_IMPL_CYCLE_COLLECTION_INHERITED(InsertTextTransaction, EditTransactionBase,
                                   mEditorBase, mTextNode)

NS_IMPL_ADDREF_INHERITED(InsertTextTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(InsertTextTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InsertTextTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP InsertTextTransaction::DoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTextNode)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  OwningNonNull<EditorBase> editorBase = *mEditorBase;
  OwningNonNull<Text> textNode = *mTextNode;

  ErrorResult error;
  editorBase->DoInsertText(textNode, mOffset, mStringToInsert, error);
  if (error.Failed()) {
    NS_WARNING("EditorBase::DoInsertText() failed");
    return error.StealNSResult();
  }

  // Only set selection to insertion point if editor gives permission
  if (editorBase->AllowsTransactionsToChangeSelection()) {
    RefPtr<Selection> selection = editorBase->GetSelection();
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_FAILURE;
    }
    DebugOnly<nsresult> rvIgnored =
        selection->Collapse(textNode, mOffset + mStringToInsert.Length());
    NS_ASSERTION(NS_SUCCEEDED(rvIgnored),
                 "Selection::Collapse() failed, but ignored");
  } else {
    // Do nothing - DOM Range gravity will adjust selection
  }
  // XXX Other transactions do not do this but its callers do.
  //     Why do this transaction do this by itself?
  editorBase->RangeUpdaterRef().SelAdjInsertText(textNode, mOffset,
                                                 mStringToInsert);

  return NS_OK;
}

NS_IMETHODIMP InsertTextTransaction::UndoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTextNode)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  OwningNonNull<EditorBase> editorBase = *mEditorBase;
  OwningNonNull<Text> textNode = *mTextNode;
  ErrorResult error;
  editorBase->DoDeleteText(textNode, mOffset, mStringToInsert.Length(), error);
  NS_WARNING_ASSERTION(!error.Failed(), "EditorBase::DoDeleteText() failed");
  return error.StealNSResult();
}

NS_IMETHODIMP InsertTextTransaction::Merge(nsITransaction* aOtherTransaction,
                                           bool* aDidMerge) {
  if (NS_WARN_IF(!aOtherTransaction) || NS_WARN_IF(!aDidMerge)) {
    return NS_ERROR_INVALID_ARG;
  }
  // Set out param default value
  *aDidMerge = false;

  RefPtr<EditTransactionBase> otherTransactionBase =
      aOtherTransaction->GetAsEditTransactionBase();
  if (!otherTransactionBase) {
    return NS_OK;
  }

  // If aTransaction is a InsertTextTransaction, and if the selection hasn't
  // changed, then absorb it.
  InsertTextTransaction* otherInsertTextTransaction =
      otherTransactionBase->GetAsInsertTextTransaction();
  if (!otherInsertTextTransaction ||
      !IsSequentialInsert(*otherInsertTextTransaction)) {
    return NS_OK;
  }

  nsAutoString otherData;
  otherInsertTextTransaction->GetData(otherData);
  mStringToInsert += otherData;
  *aDidMerge = true;
  return NS_OK;
}

/* ============ private methods ================== */

void InsertTextTransaction::GetData(nsString& aResult) {
  aResult = mStringToInsert;
}

bool InsertTextTransaction::IsSequentialInsert(
    InsertTextTransaction& aOtherTransaction) {
  return aOtherTransaction.mTextNode == mTextNode &&
         aOtherTransaction.mOffset == mOffset + mStringToInsert.Length();
}

}  // namespace mozilla
