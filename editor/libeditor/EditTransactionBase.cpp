/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EditTransactionBase.h"
#include "nsError.h"
#include "nsISupportsBase.h"

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION_CLASS(EditTransactionBase)

NS_IMPL_CYCLE_COLLECTION_UNLINK_0(EditTransactionBase)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(EditTransactionBase)
  // We don't have anything to traverse, but some of our subclasses do.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EditTransactionBase)
  NS_INTERFACE_MAP_ENTRY(nsITransaction)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITransaction)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(EditTransactionBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(EditTransactionBase)

EditTransactionBase::~EditTransactionBase()
{
}

NS_IMETHODIMP
EditTransactionBase::RedoTransaction()
{
  return DoTransaction();
}

NS_IMETHODIMP
EditTransactionBase::GetIsTransient(bool* aIsTransient)
{
  *aIsTransient = false;

  return NS_OK;
}

NS_IMETHODIMP
EditTransactionBase::Merge(nsITransaction* aTransaction, bool* aDidMerge)
{
  *aDidMerge = false;

  return NS_OK;
}

} // namespace mozilla
