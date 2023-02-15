/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EditAggregateTransaction_h
#define EditAggregateTransaction_h

#include "EditTransactionBase.h"

#include "mozilla/OwningNonNull.h"
#include "mozilla/RefPtr.h"

#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsISupportsImpl.h"

namespace mozilla {

/**
 * base class for all document editing transactions that require aggregation.
 * provides a list of child transactions.
 */
class EditAggregateTransaction : public EditTransactionBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(EditAggregateTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction() override;
  NS_IMETHOD Merge(nsITransaction* aOtherTransaction, bool* aDidMerge) override;

  /**
   * Get the name assigned to this transaction.
   */
  nsAtom* GetName() const;

  const nsTArray<OwningNonNull<EditTransactionBase>>& ChildTransactions()
      const {
    return mChildren;
  }

 protected:
  EditAggregateTransaction() = default;
  virtual ~EditAggregateTransaction() = default;

  nsTArray<OwningNonNull<EditTransactionBase>> mChildren;
  RefPtr<nsAtom> mName;
};

}  // namespace mozilla

#endif  // #ifndef EditAggregateTransaction_h
