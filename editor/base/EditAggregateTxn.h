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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef EditAggregateTxn_h__
#define EditAggregateTxn_h__

#include "EditTxn.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"

#define EDIT_AGGREGATE_TXN_CID \
{/* 345921a0-ac49-11d2-86d8-000064657374 */ \
0x345921a0, 0xac49, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }


/**
 * base class for all document editing transactions that require aggregation.
 * provides a list of child transactions.
 */
class EditAggregateTxn : public EditTxn
{
public:

  NS_DECL_ISUPPORTS_INHERITED

  static const nsIID& GetCID() { static nsIID cid = EDIT_AGGREGATE_TXN_CID; return cid; }

  EditAggregateTxn();

  virtual ~EditAggregateTxn();

  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

  NS_IMETHOD RedoTransaction(void);

  NS_IMETHOD GetIsTransient(PRBool *aIsTransient);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

  /** append a transaction to this aggregate */
  NS_IMETHOD AppendChild(EditTxn *aTxn);

  /** get the number of nested txns.  
    * This is the number of top-level txns, it does not do recursive decent.
    */
  NS_IMETHOD GetCount(PRUint32 *aCount);

  /** get the txn at index aIndex.
    * returns NS_ERROR_UNEXPECTED if there is no txn at aIndex.
    */
  NS_IMETHOD GetTxnAt(PRInt32 aIndex, EditTxn **aTxn);

  /** set the name assigned to this txn */
  NS_IMETHOD SetName(nsIAtom *aName);

  /** get the name assigned to this txn */
  NS_IMETHOD GetName(nsIAtom **aName);

protected:

  nsCOMPtr<nsISupportsArray> mChildren;
  nsCOMPtr<nsIAtom> mName;
};

#endif
