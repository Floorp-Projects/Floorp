/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditAggregateTransaction.h"

#include "mozilla/Logging.h"
#include "mozilla/ReverseIterator.h"  // for Reversed
#include "nsAString.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"  // for nsCOMPtr
#include "nsError.h"   // for NS_OK, etc.
#include "nsGkAtoms.h"
#include "nsISupportsUtils.h"  // for NS_ADDREF
#include "nsString.h"          // for nsAutoString

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION_INHERITED(EditAggregateTransaction,
                                   EditTransactionBase, mChildren)

NS_IMPL_ADDREF_INHERITED(EditAggregateTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(EditAggregateTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EditAggregateTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP EditAggregateTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p EditAggregateTransaction::%s this={ mName=%s, mChildren=%zu } "
           "Start==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get(),
           mChildren.Length()));
  // FYI: It's legal (but not very useful) to have an empty child list.
  for (const OwningNonNull<EditTransactionBase>& childTransaction :
       CopyableAutoTArray<OwningNonNull<EditTransactionBase>, 10>(mChildren)) {
    nsresult rv = MOZ_KnownLive(childTransaction)->DoTransaction();
    if (NS_FAILED(rv)) {
      NS_WARNING("EditTransactionBase::DoTransaction() failed");
      return rv;
    }
  }
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p EditAggregateTransaction::%s this={ mName=%s } "
           "End================================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p EditAggregateTransaction::%s this={ mName=%s, mChildren=%zu } "
           "Start==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get(),
           mChildren.Length()));
  // FYI: It's legal (but not very useful) to have an empty child list.
  // Undo goes through children backwards.
  const CopyableAutoTArray<OwningNonNull<EditTransactionBase>, 10> children(
      mChildren);
  for (const OwningNonNull<EditTransactionBase>& childTransaction :
       Reversed(children)) {
    nsresult rv = MOZ_KnownLive(childTransaction)->UndoTransaction();
    if (NS_FAILED(rv)) {
      NS_WARNING("EditTransactionBase::UndoTransaction() failed");
      return rv;
    }
  }
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p EditAggregateTransaction::%s this={ mName=%s } "
           "End================================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p EditAggregateTransaction::%s this={ mName=%s, mChildren=%zu } "
           "Start==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get(),
           mChildren.Length()));
  // It's legal (but not very useful) to have an empty child list.
  const CopyableAutoTArray<OwningNonNull<EditTransactionBase>, 10> children(
      mChildren);
  for (const OwningNonNull<EditTransactionBase>& childTransaction : children) {
    nsresult rv = MOZ_KnownLive(childTransaction)->RedoTransaction();
    if (NS_FAILED(rv)) {
      NS_WARNING("EditTransactionBase::RedoTransaction() failed");
      return rv;
    }
  }
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p EditAggregateTransaction::%s this={ mName=%s } "
           "End================================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTransaction::Merge(nsITransaction* aOtherTransaction,
                                              bool* aDidMerge) {
  if (aDidMerge) {
    *aDidMerge = false;
  }
  if (mChildren.IsEmpty()) {
    MOZ_LOG(GetLogModule(), LogLevel::Debug,
            ("%p EditAggregateTransaction::%s this={ mName=%s } returned false "
             "due to no children",
             this, __FUNCTION__,
             nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
    return NS_OK;
  }
  // FIXME: Is this really intended not to loop?  It looks like the code
  // that used to be here sort of intended to loop, but didn't.
  return mChildren[0]->Merge(aOtherTransaction, aDidMerge);
}

nsAtom* EditAggregateTransaction::GetName() const { return mName; }

}  // namespace mozilla
