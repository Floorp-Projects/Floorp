/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteRangeTxn_h__
#define DeleteRangeTxn_h__

#include "EditAggregateTxn.h"
#include "nsIDOMNode.h"
#include "nsIDOMRange.h"
#include "nsIEditor.h"
#include "nsCOMPtr.h"

class nsIDOMRange;
class nsIEditor;
class nsRangeUpdater;

/**
 * A transaction that deletes an entire range in the content tree
 */
class DeleteRangeTxn : public EditAggregateTxn
{
public:
  /** initialize the transaction.
    * @param aEditor the object providing basic editing operations
    * @param aRange  the range to delete
    */
  NS_IMETHOD Init(nsIEditor *aEditor, 
                  nsIDOMRange *aRange,
                  nsRangeUpdater *aRangeUpdater);

  DeleteRangeTxn();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteRangeTxn, EditAggregateTxn)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction();

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

  /** range updater object */
  nsRangeUpdater *mRangeUpdater;
};

#endif
