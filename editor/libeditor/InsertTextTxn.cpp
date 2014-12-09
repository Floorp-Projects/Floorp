/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InsertTextTxn.h"

#include "mozilla/dom/Selection.h"      // Selection local var
#include "mozilla/dom/Text.h"           // mTextNode
#include "nsAString.h"                  // nsAString parameter
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsEditor.h"                   // mEditor
#include "nsError.h"                    // for NS_OK, etc

using namespace mozilla;
using namespace mozilla::dom;

class nsITransaction;

InsertTextTxn::InsertTextTxn(Text& aTextNode, uint32_t aOffset,
                             const nsAString& aStringToInsert,
                             nsEditor& aEditor)
  : EditTxn()
  , mTextNode(&aTextNode)
  , mOffset(aOffset)
  , mStringToInsert(aStringToInsert)
  , mEditor(aEditor)
{
}

InsertTextTxn::~InsertTextTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(InsertTextTxn, EditTxn,
                                   mTextNode)

NS_IMPL_ADDREF_INHERITED(InsertTextTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(InsertTextTxn, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InsertTextTxn)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsITransaction, InsertTextTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)


NS_IMETHODIMP
InsertTextTxn::DoTransaction()
{
  nsresult res = mTextNode->InsertData(mOffset, mStringToInsert);
  NS_ENSURE_SUCCESS(res, res);

  // Only set selection to insertion point if editor gives permission
  if (mEditor.GetShouldTxnSetSelection()) {
    nsRefPtr<Selection> selection = mEditor.GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    res = selection->Collapse(mTextNode,
                              mOffset + mStringToInsert.Length());
    NS_ASSERTION(NS_SUCCEEDED(res),
                 "Selection could not be collapsed after insert");
  } else {
    // Do nothing - DOM Range gravity will adjust selection
  }

  return NS_OK;
}

NS_IMETHODIMP
InsertTextTxn::UndoTransaction()
{
  return mTextNode->DeleteData(mOffset, mStringToInsert.Length());
}

NS_IMETHODIMP
InsertTextTxn::Merge(nsITransaction* aTransaction, bool* aDidMerge)
{
  if (!aTransaction || !aDidMerge) {
    return NS_OK;
  }
  // Set out param default value
  *aDidMerge = false;

  // If aTransaction is a InsertTextTxn, and if the selection hasn't changed,
  // then absorb it
  nsRefPtr<InsertTextTxn> otherInsTxn = do_QueryObject(aTransaction);
  if (otherInsTxn && IsSequentialInsert(*otherInsTxn)) {
    nsAutoString otherData;
    otherInsTxn->GetData(otherData);
    mStringToInsert += otherData;
    *aDidMerge = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
InsertTextTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("InsertTextTxn: ");
  aString += mStringToInsert;
  return NS_OK;
}

/* ============ private methods ================== */

void
InsertTextTxn::GetData(nsString& aResult)
{
  aResult = mStringToInsert;
}

bool
InsertTextTxn::IsSequentialInsert(InsertTextTxn& aOtherTxn)
{
  return aOtherTxn.mTextNode == mTextNode &&
         aOtherTxn.mOffset == mOffset + mStringToInsert.Length();
}
