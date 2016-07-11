/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EditAggregateTransaction_h
#define EditAggregateTransaction_h

#include "mozilla/EditTransactionBase.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIAtom.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nscore.h"

class nsITransaction;

namespace mozilla {

/**
 * base class for all document editing transactions that require aggregation.
 * provides a list of child transactions.
 */
class EditAggregateTransaction : public EditTransactionBase
{
public:
  EditAggregateTransaction();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(EditAggregateTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE

  NS_IMETHOD RedoTransaction() override;
  NS_IMETHOD Merge(nsITransaction* aTransaction, bool* aDidMerge) override;

  /**
   * Append a transaction to this aggregate.
   */
  NS_IMETHOD AppendChild(EditTransactionBase* aTransaction);

  /**
   * Get the name assigned to this transaction.
   */
  NS_IMETHOD GetName(nsIAtom** aName);

protected:
  virtual ~EditAggregateTransaction();

  nsTArray<RefPtr<EditTransactionBase>> mChildren;
  nsCOMPtr<nsIAtom> mName;
};

} // namespace mozilla

#endif // #ifndef EditAggregateTransaction_h
