/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsEditor.h"

#include "nsCSSStyleSheet.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsISelectionController.h"


#include "nsStyleSheetTxns.h"

static void
AddStyleSheet(nsIEditor* aEditor, nsIStyleSheet* aSheet)
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
RemoveStyleSheet(nsIEditor *aEditor, nsIStyleSheet *aSheet)
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

AddStyleSheetTxn::AddStyleSheetTxn()
:  EditTxn()
,  mEditor(NULL)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(AddStyleSheetTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AddStyleSheetTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSheet)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AddStyleSheetTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mSheet, nsIStyleSheet)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AddStyleSheetTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP
AddStyleSheetTxn::Init(nsIEditor *aEditor, nsCSSStyleSheet *aSheet)
{
  NS_ENSURE_TRUE(aEditor && aSheet, NS_ERROR_INVALID_ARG);

  mEditor = aEditor;
  mSheet = aSheet;
  
  return NS_OK;
}


NS_IMETHODIMP
AddStyleSheetTxn::DoTransaction()
{
  NS_ENSURE_TRUE(mEditor && mSheet, NS_ERROR_NOT_INITIALIZED);
  
  AddStyleSheet(mEditor, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
AddStyleSheetTxn::UndoTransaction()
{
  NS_ENSURE_TRUE(mEditor && mSheet, NS_ERROR_NOT_INITIALIZED);
  
  RemoveStyleSheet(mEditor, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
AddStyleSheetTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("AddStyleSheetTxn");
  return NS_OK;
}


RemoveStyleSheetTxn::RemoveStyleSheetTxn()
:  EditTxn()
,  mEditor(NULL)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(RemoveStyleSheetTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(RemoveStyleSheetTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSheet)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(RemoveStyleSheetTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mSheet, nsIStyleSheet)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RemoveStyleSheetTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP
RemoveStyleSheetTxn::Init(nsIEditor *aEditor, nsCSSStyleSheet *aSheet)
{
  NS_ENSURE_TRUE(aEditor && aSheet, NS_ERROR_INVALID_ARG);

  mEditor = aEditor;
  mSheet = aSheet;

  return NS_OK;
}


NS_IMETHODIMP
RemoveStyleSheetTxn::DoTransaction()
{
  NS_ENSURE_TRUE(mEditor && mSheet, NS_ERROR_NOT_INITIALIZED);

  RemoveStyleSheet(mEditor, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
RemoveStyleSheetTxn::UndoTransaction()
{
  NS_ENSURE_TRUE(mEditor && mSheet, NS_ERROR_NOT_INITIALIZED);

  AddStyleSheet(mEditor, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
RemoveStyleSheetTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("RemoveStyleSheetTxn");
  return NS_OK;
}
