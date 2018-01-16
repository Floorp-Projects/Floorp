/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditAggregateTransaction.h"
#include "nsAString.h"
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsError.h"                    // for NS_OK, etc.
#include "nsISupportsUtils.h"           // for NS_ADDREF
#include "nsITransaction.h"             // for nsITransaction
#include "nsString.h"                   // for nsAutoString

namespace mozilla {

EditAggregateTransaction::EditAggregateTransaction()
{
}

EditAggregateTransaction::~EditAggregateTransaction()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(EditAggregateTransaction,
                                   EditTransactionBase,
                                   mChildren)

NS_IMPL_ADDREF_INHERITED(EditAggregateTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(EditAggregateTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EditAggregateTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP
EditAggregateTransaction::DoTransaction()
{
  // FYI: It's legal (but not very useful) to have an empty child list.
  for (uint32_t i = 0, length = mChildren.Length(); i < length; ++i) {
    nsITransaction *txn = mChildren[i];
    if (!txn) {
      return NS_ERROR_NULL_POINTER;
    }
    nsresult rv = txn->DoTransaction();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EditAggregateTransaction::UndoTransaction()
{
  // FYI: It's legal (but not very useful) to have an empty child list.
  // Undo goes through children backwards.
  for (uint32_t i = mChildren.Length(); i--; ) {
    nsITransaction *txn = mChildren[i];
    if (!txn) {
      return NS_ERROR_NULL_POINTER;
    }
    nsresult rv = txn->UndoTransaction();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EditAggregateTransaction::RedoTransaction()
{
  // It's legal (but not very useful) to have an empty child list.
  for (uint32_t i = 0, length = mChildren.Length(); i < length; ++i) {
    nsITransaction *txn = mChildren[i];
    if (!txn) {
      return NS_ERROR_NULL_POINTER;
    }
    nsresult rv = txn->RedoTransaction();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EditAggregateTransaction::Merge(nsITransaction* aTransaction,
                                bool* aDidMerge)
{
  if (aDidMerge) {
    *aDidMerge = false;
  }
  if (mChildren.IsEmpty()) {
    return NS_OK;
  }
  // FIXME: Is this really intended not to loop?  It looks like the code
  // that used to be here sort of intended to loop, but didn't.
  nsITransaction *txn = mChildren[0];
  if (!txn) {
    return NS_ERROR_NULL_POINTER;
  }
  return txn->Merge(aTransaction, aDidMerge);
}

NS_IMETHODIMP
EditAggregateTransaction::AppendChild(EditTransactionBase* aTransaction)
{
  if (!aTransaction) {
    return NS_ERROR_NULL_POINTER;
  }

  RefPtr<EditTransactionBase>* slot = mChildren.AppendElement();
  if (!slot) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *slot = aTransaction;
  return NS_OK;
}

NS_IMETHODIMP
EditAggregateTransaction::GetName(nsAtom** aName)
{
  if (aName && mName) {
    *aName = mName;
    NS_ADDREF(*aName);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

} // namespace mozilla
