/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditAggregateTransaction.h"
#include "nsAString.h"
#include "nsCOMPtr.h"          // for nsCOMPtr
#include "nsError.h"           // for NS_OK, etc.
#include "nsISupportsUtils.h"  // for NS_ADDREF
#include "nsITransaction.h"    // for nsITransaction
#include "nsString.h"          // for nsAutoString

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION_INHERITED(EditAggregateTransaction,
                                   EditTransactionBase, mChildren)

NS_IMPL_ADDREF_INHERITED(EditAggregateTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(EditAggregateTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EditAggregateTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP EditAggregateTransaction::DoTransaction() {
  // FYI: It's legal (but not very useful) to have an empty child list.
  AutoTArray<OwningNonNull<EditTransactionBase>, 10> children(mChildren);
  for (auto& childTransaction : children) {
    nsresult rv = childTransaction->DoTransaction();
    if (NS_FAILED(rv)) {
      NS_WARNING("EditTransactionBase::DoTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTransaction::UndoTransaction() {
  // FYI: It's legal (but not very useful) to have an empty child list.
  // Undo goes through children backwards.
  AutoTArray<OwningNonNull<EditTransactionBase>, 10> children(mChildren);
  for (auto& childTransaction : Reversed(children)) {
    nsresult rv = childTransaction->UndoTransaction();
    if (NS_FAILED(rv)) {
      NS_WARNING("EditTransactionBase::UndoTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTransaction::RedoTransaction() {
  // It's legal (but not very useful) to have an empty child list.
  AutoTArray<OwningNonNull<EditTransactionBase>, 10> children(mChildren);
  for (auto& childTransaction : children) {
    nsresult rv = childTransaction->RedoTransaction();
    if (NS_FAILED(rv)) {
      NS_WARNING("EditTransactionBase::RedoTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTransaction::Merge(nsITransaction* aTransaction,
                                              bool* aDidMerge) {
  if (aDidMerge) {
    *aDidMerge = false;
  }
  if (mChildren.IsEmpty()) {
    return NS_OK;
  }
  // FIXME: Is this really intended not to loop?  It looks like the code
  // that used to be here sort of intended to loop, but didn't.
  return mChildren[0]->Merge(aTransaction, aDidMerge);
}

NS_IMETHODIMP EditAggregateTransaction::AppendChild(
    EditTransactionBase* aTransaction) {
  if (NS_WARN_IF(!aTransaction)) {
    return NS_ERROR_INVALID_ARG;
  }

  mChildren.AppendElement(*aTransaction);
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTransaction::GetName(nsAtom** aName) {
  if (NS_WARN_IF(!aName)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!mName)) {
    return NS_ERROR_FAILURE;
  }
  *aName = do_AddRef(mName).take();
  return NS_OK;
}

}  // namespace mozilla
