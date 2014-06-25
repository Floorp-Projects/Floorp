/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EditTxn_h__
#define EditTxn_h__

#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsITransaction.h"
#include "nsPIEditorTransaction.h"
#include "nscore.h"

/**
 * Base class for all document editing transactions.
 */
class EditTxn : public nsITransaction,
                public nsPIEditorTransaction
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(EditTxn, nsITransaction)

  virtual void LastRelease() {}

  NS_IMETHOD RedoTransaction(void);
  NS_IMETHOD GetIsTransient(bool *aIsTransient);
  NS_IMETHOD Merge(nsITransaction *aTransaction, bool *aDidMerge);

protected:
  virtual ~EditTxn();
};

#define NS_DECL_EDITTXN \
  NS_IMETHOD DoTransaction(); \
  NS_IMETHOD UndoTransaction(); \
  NS_IMETHOD GetTxnDescription(nsAString& aTxnDescription);

#endif
