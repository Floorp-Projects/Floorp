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

#ifndef DeleteRangeTxn_h__
#define DeleteRangeTxn_h__

#include "EditAggregateTxn.h"
#include "nsIDOMNode.h"
#include "nsIEditor.h"
#include "nsCOMPtr.h"

class nsIDOMDocument;
class nsIDOMRange;
class nsISupportsArray;

#define DELETE_RANGE_TXN_IID \
{/* 5ec6b260-ac49-11d2-86d8-000064657374 */ \
0x5ec6b260, 0xac49, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

/**
 * A transaction that deletes an entire range in the content tree
 */
class DeleteRangeTxn : public EditAggregateTxn
{
public:

  /** initialize the transaction.
    * @param aRange  the range to delete
    */
  virtual nsresult Init(nsIDOMRange *aRange);

private:
  DeleteRangeTxn();

public:

  virtual ~DeleteRangeTxn();

  virtual nsresult Do(void);

  virtual nsresult Undo(void);

  virtual nsresult Redo(void);

  virtual nsresult Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  virtual nsresult Write(nsIOutputStream *aOutputStream);

  virtual nsresult GetUndoString(nsString **aString);

  virtual nsresult GetRedoString(nsString **aString);

protected:

  virtual nsresult CreateTxnsToDeleteBetween(nsIDOMNode *aStartParent, 
                                             PRUint32    aStartOffset, 
                                             PRUint32    aEndOffset);

  virtual nsresult CreateTxnsToDeleteNodesBetween(nsIDOMNode *aParent, 
                                                  nsIDOMNode *aFirstChild,
                                                  nsIDOMNode *aLastChild);

  virtual nsresult CreateTxnsToDeleteContent(nsIDOMNode *aParent, 
                                             PRUint32 aOffset, 
                                             nsIEditor::Direction aDir);
  
  virtual nsresult BuildAncestorList(nsIDOMNode       *aNode, 
                                     nsISupportsArray *aList);

protected:
  
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

  friend class TransactionFactory;

};

#endif
