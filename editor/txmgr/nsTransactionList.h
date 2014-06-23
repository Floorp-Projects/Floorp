/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTransactionList_h__
#define nsTransactionList_h__

#include "nsAutoPtr.h"
#include "nsISupportsImpl.h"
#include "nsITransactionList.h"
#include "nsIWeakReferenceUtils.h"

class nsITransaction;
class nsITransactionManager;
class nsTransactionItem;
class nsTransactionStack;

/** implementation of a transaction list object.
 *
 */
class nsTransactionList : public nsITransactionList
{
private:

  nsWeakPtr                   mTxnMgr;
  nsTransactionStack         *mTxnStack;
  nsRefPtr<nsTransactionItem> mTxnItem;

protected:
  virtual ~nsTransactionList();

public:

  nsTransactionList(nsITransactionManager *aTxnMgr, nsTransactionStack *aTxnStack);
  nsTransactionList(nsITransactionManager *aTxnMgr, nsTransactionItem *aTxnItem);

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_ISUPPORTS

  /* nsITransactionManager method implementations. */
  NS_DECL_NSITRANSACTIONLIST

  /* nsTransactionList specific methods. */
};

#endif // nsTransactionList_h__
