/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef EditAggregateTxn_h__
#define EditAggregateTxn_h__

#include "EditTxn.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"

#define EDIT_AGGREGATE_TXN_IID \
{/* 345921a0-ac49-11d2-86d8-000064657374 */ \
0x345921a0, 0xac49, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

class nsVoidArray;

/**
 * base class for all document editing transactions that require aggregation.
 * provides a list of child transactions.
 */
class EditAggregateTxn : public EditTxn
{
public:

  NS_DECL_ISUPPORTS_INHERITED

  static const nsIID& GetIID() { static nsIID iid = EDIT_AGGREGATE_TXN_IID; return iid; }

  EditAggregateTxn();

  virtual ~EditAggregateTxn();

  NS_IMETHOD Do(void);

  NS_IMETHOD Undo(void);

  NS_IMETHOD Redo(void);

  NS_IMETHOD GetIsTransient(PRBool *aIsTransient);

  NS_IMETHOD Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  NS_IMETHOD Write(nsIOutputStream *aOutputStream);

  NS_IMETHOD GetUndoString(nsString **aString);

  NS_IMETHOD GetRedoString(nsString **aString);

  /** append a transaction to this aggregate */
  NS_IMETHOD AppendChild(EditTxn *aTxn);

  /** get the number of nested txns.  
    * This is the number of top-level txns, it does not do recursive decent.
    */
  NS_IMETHOD GetCount(PRInt32 *aCount);

  /** get the txn at index aIndex.
    * returns NS_ERROR_UNEXPECTED if there is no txn at aIndex.
    */
  NS_IMETHOD GetTxnAt(PRInt32 aIndex, EditTxn **aTxn);

  /** set the name assigned to this txn */
  NS_IMETHOD SetName(nsIAtom *aName);

  /** get the name assigned to this txn */
  NS_IMETHOD GetName(nsIAtom **aName);

protected:

  //XXX: if this was an nsISupportsArray, it would handle refcounting for us
  nsVoidArray * mChildren;
  nsCOMPtr<nsIAtom> mName;
};

#endif
