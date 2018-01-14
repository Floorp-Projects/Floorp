/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditTransactionBase_h
#define mozilla_EditTransactionBase_h

#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsITransaction.h"
#include "nscore.h"

namespace mozilla {

/**
 * Base class for all document editing transactions.
 */
class EditTransactionBase : public nsITransaction
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(EditTransactionBase, nsITransaction)

  NS_IMETHOD RedoTransaction(void) override;
  NS_IMETHOD GetIsTransient(bool* aIsTransient) override;
  NS_IMETHOD Merge(nsITransaction* aTransaction, bool* aDidMerge) override;

protected:
  virtual ~EditTransactionBase();
};

} // namespace mozilla

#define NS_DECL_EDITTRANSACTIONBASE \
  NS_IMETHOD DoTransaction() override; \
  NS_IMETHOD UndoTransaction() override;

#endif // #ifndef mozilla_EditTransactionBase_h
