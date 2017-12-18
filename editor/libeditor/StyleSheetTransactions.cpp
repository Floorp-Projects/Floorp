/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StyleSheetTransactions.h"

#include <stddef.h>                     // for nullptr

#include "nsAString.h"
#include "nsCOMPtr.h"                   // for nsCOMPtr, do_QueryInterface, etc.
#include "mozilla/StyleSheet.h"   // for mozilla::StyleSheet
#include "mozilla/StyleSheetInlines.h"
#include "nsDebug.h"                    // for NS_ENSURE_TRUE
#include "nsError.h"                    // for NS_OK, etc.
#include "nsIDocument.h"                // for nsIDocument
#include "nsIDocumentObserver.h"        // for UPDATE_STYLE

namespace mozilla {

static void
AddStyleSheet(EditorBase& aEditor, StyleSheet* aSheet)
{
  nsCOMPtr<nsIDocument> doc = aEditor.GetDocument();
  if (doc) {
    doc->BeginUpdate(UPDATE_STYLE);
    doc->AddStyleSheet(aSheet);
    doc->EndUpdate(UPDATE_STYLE);
  }
}

static void
RemoveStyleSheet(EditorBase& aEditor, StyleSheet* aSheet)
{
  nsCOMPtr<nsIDocument> doc = aEditor.GetDocument();
  if (doc) {
    doc->BeginUpdate(UPDATE_STYLE);
    doc->RemoveStyleSheet(aSheet);
    doc->EndUpdate(UPDATE_STYLE);
  }
}

/******************************************************************************
 * AddStyleSheetTransaction
 ******************************************************************************/

AddStyleSheetTransaction::AddStyleSheetTransaction(EditorBase& aEditorBase,
                                                   StyleSheet& aStyleSheet)
  : mEditorBase(&aEditorBase)
  , mSheet(&aStyleSheet)
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(AddStyleSheetTransaction,
                                   EditTransactionBase,
                                   mEditorBase,
                                   mSheet)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AddStyleSheetTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP
AddStyleSheetTransaction::DoTransaction()
{
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mSheet)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  AddStyleSheet(*mEditorBase, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
AddStyleSheetTransaction::UndoTransaction()
{
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mSheet)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  RemoveStyleSheet(*mEditorBase, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
AddStyleSheetTransaction::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("AddStyleSheetTransaction");
  return NS_OK;
}

/******************************************************************************
 * RemoveStyleSheetTransaction
 ******************************************************************************/

RemoveStyleSheetTransaction::RemoveStyleSheetTransaction(
                               EditorBase& aEditorBase,
                               StyleSheet& aStyleSheet)
  : mEditorBase(&aEditorBase)
  , mSheet(&aStyleSheet)
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(RemoveStyleSheetTransaction,
                                   EditTransactionBase,
                                   mEditorBase,
                                   mSheet)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RemoveStyleSheetTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP
RemoveStyleSheetTransaction::DoTransaction()
{
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mSheet)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  RemoveStyleSheet(*mEditorBase, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
RemoveStyleSheetTransaction::UndoTransaction()
{
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mSheet)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  AddStyleSheet(*mEditorBase, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
RemoveStyleSheetTransaction::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("RemoveStyleSheetTransaction");
  return NS_OK;
}

} // namespace mozilla
