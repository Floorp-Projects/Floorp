/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StyleSheetTransactions.h"

#include <stddef.h>                     // for nullptr

#include "nsAString.h"
#include "nsCOMPtr.h"                   // for nsCOMPtr, do_QueryInterface, etc.
#include "mozilla/StyleSheetHandle.h"   // for mozilla::StyleSheetHandle
#include "mozilla/StyleSheetHandleInlines.h"
#include "nsDebug.h"                    // for NS_ENSURE_TRUE
#include "nsError.h"                    // for NS_OK, etc.
#include "nsIDOMDocument.h"             // for nsIDOMDocument
#include "nsIDocument.h"                // for nsIDocument
#include "nsIDocumentObserver.h"        // for UPDATE_STYLE
#include "nsIEditor.h"                  // for nsIEditor

namespace mozilla {

static void
AddStyleSheet(nsIEditor* aEditor, StyleSheetHandle aSheet)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  aEditor->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (doc) {
    doc->BeginUpdate(UPDATE_STYLE);
    doc->AddStyleSheet(aSheet);
    doc->EndUpdate(UPDATE_STYLE);
  }
}

static void
RemoveStyleSheet(nsIEditor* aEditor, StyleSheetHandle aSheet)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  aEditor->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (doc) {
    doc->BeginUpdate(UPDATE_STYLE);
    doc->RemoveStyleSheet(aSheet);
    doc->EndUpdate(UPDATE_STYLE);
  }
}

/******************************************************************************
 * AddStyleSheetTransaction
 ******************************************************************************/

AddStyleSheetTransaction::AddStyleSheetTransaction()
  : mEditor(nullptr)
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(AddStyleSheetTransaction,
                                   EditTransactionBase,
                                   mSheet)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AddStyleSheetTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP
AddStyleSheetTransaction::Init(nsIEditor* aEditor,
                               StyleSheetHandle aSheet)
{
  NS_ENSURE_TRUE(aEditor && aSheet, NS_ERROR_INVALID_ARG);

  mEditor = aEditor;
  mSheet = aSheet;

  return NS_OK;
}


NS_IMETHODIMP
AddStyleSheetTransaction::DoTransaction()
{
  NS_ENSURE_TRUE(mEditor && mSheet, NS_ERROR_NOT_INITIALIZED);

  AddStyleSheet(mEditor, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
AddStyleSheetTransaction::UndoTransaction()
{
  NS_ENSURE_TRUE(mEditor && mSheet, NS_ERROR_NOT_INITIALIZED);

  RemoveStyleSheet(mEditor, mSheet);
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

RemoveStyleSheetTransaction::RemoveStyleSheetTransaction()
  : mEditor(nullptr)
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(RemoveStyleSheetTransaction,
                                   EditTransactionBase,
                                   mSheet)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RemoveStyleSheetTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP
RemoveStyleSheetTransaction::Init(nsIEditor* aEditor,
                                  StyleSheetHandle aSheet)
{
  NS_ENSURE_TRUE(aEditor && aSheet, NS_ERROR_INVALID_ARG);

  mEditor = aEditor;
  mSheet = aSheet;

  return NS_OK;
}


NS_IMETHODIMP
RemoveStyleSheetTransaction::DoTransaction()
{
  NS_ENSURE_TRUE(mEditor && mSheet, NS_ERROR_NOT_INITIALIZED);

  RemoveStyleSheet(mEditor, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
RemoveStyleSheetTransaction::UndoTransaction()
{
  NS_ENSURE_TRUE(mEditor && mSheet, NS_ERROR_NOT_INITIALIZED);

  AddStyleSheet(mEditor, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
RemoveStyleSheetTransaction::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("RemoveStyleSheetTransaction");
  return NS_OK;
}

} // namespace mozilla
