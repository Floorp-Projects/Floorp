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

#ifndef DeleteRangeTxn_h__
#define DeleteRangeTxn_h__

#include "EditAggregateTxn.h"
#include "nsIDOMNode.h"
#include "nsIDOMRange.h"
#include "nsIEditor.h"
#include "nsCOMPtr.h"

class nsIDOMDocument;

#define DELETE_RANGE_TXN_CID \
{/* 5ec6b260-ac49-11d2-86d8-000064657374 */ \
0x5ec6b260, 0xac49, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

class nsIDOMRange;
class nsIEditor;

/**
 * A transaction that deletes an entire range in the content tree
 */
class DeleteRangeTxn : public EditAggregateTxn
{
public:

  static const nsIID& GetCID() { static nsIID iid = DELETE_RANGE_TXN_CID; return iid; }

  /** initialize the transaction.
    * @param aEditor the object providing basic editing operations
    * @param aRange  the range to delete
    */
  NS_IMETHOD Init(nsIEditor *aEditor, nsIDOMRange *aRange);

private:
  DeleteRangeTxn();

public:

  virtual ~DeleteRangeTxn();

  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

  NS_IMETHOD RedoTransaction(void);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

protected:

  NS_IMETHOD CreateTxnsToDeleteBetween(nsIDOMNode *aStartParent, 
                                             PRUint32    aStartOffset, 
                                             PRUint32    aEndOffset);

  NS_IMETHOD CreateTxnsToDeleteNodesBetween();

  NS_IMETHOD CreateTxnsToDeleteContent(nsIDOMNode *aParent, 
                                             PRUint32 aOffset, 
                                             nsIEditor::EDirection aAction);
  
protected:
  
  /** p1 in the range */
  nsCOMPtr<nsIDOMRange> mRange;			// is this really an owning ptr?

  /** p1 in the range */
  nsCOMPtr<nsIDOMNode> mStartParent;

  /** p1 offset */
  PRInt32 mStartOffset;

  /** p2 in the range */
  nsCOMPtr<nsIDOMNode> mEndParent;

  /** the closest common parent of p1 and p2 */
  nsCOMPtr<nsIDOMNode> mCommonParent;

  /** p2 offset */
  PRInt32 mEndOffset;

  /** the editor for this transaction */
  nsIEditor* mEditor;

  friend class TransactionFactory;

};

#endif
