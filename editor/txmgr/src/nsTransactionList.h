/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsTransactionList_h__
#define nsTransactionList_h__

#include "nsWeakReference.h"
#include "nsITransactionList.h"

class nsITransaction;
class nsITransactionManager;
class nsTransactionItem;
class nsTransactionStack;
class nsTransactionRedoStack;

/** implementation of a transaction list object.
 *
 */
class nsTransactionList : public nsITransactionList
{
private:

  nsWeakPtr           mTxnMgr;
  nsTransactionStack *mTxnStack;
  nsTransactionItem  *mTxnItem;

public:

  nsTransactionList(nsITransactionManager *aTxnMgr, nsTransactionStack *aTxnStack);
  nsTransactionList(nsITransactionManager *aTxnMgr, nsTransactionItem *aTxnItem);

  virtual ~nsTransactionList();

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_ISUPPORTS

  /* nsITransactionManager method implementations. */
  NS_DECL_NSITRANSACTIONLIST

  /* nsTransactionList specific methods. */
};

#endif // nsTransactionList_h__
