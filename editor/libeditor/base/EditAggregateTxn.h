/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EditAggregateTxn_h__
#define EditAggregateTxn_h__

#include "EditTxn.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"

/**
 * base class for all document editing transactions that require aggregation.
 * provides a list of child transactions.
 */
class EditAggregateTxn : public EditTxn
{
public:
  EditAggregateTxn();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(EditAggregateTxn, EditTxn)

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction();
  NS_IMETHOD Merge(nsITransaction *aTransaction, bool *aDidMerge);

  /** append a transaction to this aggregate */
  NS_IMETHOD AppendChild(EditTxn *aTxn);

  /** get the name assigned to this txn */
  NS_IMETHOD GetName(nsIAtom **aName);

protected:

  nsTArray< nsRefPtr<EditTxn> > mChildren;
  nsCOMPtr<nsIAtom> mName;
};

#endif
