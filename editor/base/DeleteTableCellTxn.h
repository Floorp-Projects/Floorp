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

#ifndef DeleteTableCell_h__
#define DeleteTableCell_h__

#include "EditTxn.h"
#include "nsIEditor.h"
#include "nsIDOMCharacterData.h"
#include "nsCOMPtr.h"

#define DELETE_CELL_TXN_IID \
{/*50956DE5-C843-11d2-B1C3-004095E27A10*/ \
0x50956de5, 0xc843, 0x11d2, \
{ 0xb1, 0xc3, 0x0, 0x40, 0x95, 0xe2, 0x7a, 0x10 } }

class nsIPresShell;

/**
  * A transaction that inserts Table into a content node. 
  */
class DeleteTableCellTxn : public EditTxn
{
public:

  /** used to name aggregate transactions that consist only of a single DeleteTableCellTxn,
    * or a DeleteSelection followed by an DeleteTableCellTxn.
    */
  static nsIAtom *gDeleteTableCellTxnName;
	
  /** initialize the transaction
    * @param aElement the current content node
    * @param aOffset  the location in aElement to do the insertion
    * @param aNode    the new Table to insert
    * @param aPresShell used to get and set the selection
    */
  NS_IMETHOD Init(nsIDOMCharacterData *aElement,
                  nsIDOMNode *aNode,
                  nsIPresShell* aPresShell);

private:
	
	DeleteTableCellTxn();

public:
	
  NS_IMETHOD Do(void);

  NS_IMETHOD Undo(void);

  //NS_IMETHOD Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  NS_IMETHOD Write(nsIOutputStream *aOutputStream);

  NS_IMETHOD GetUndoString(nsString **aString);

  NS_IMETHOD GetRedoString(nsString **aString);

// nsISupports declarations

  // override QueryInterface to handle DeleteTableCellTxn request
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  static const nsIID& IID() { static nsIID iid = DELETE_CELL_TXN_IID; return iid; }


  /** return the string data associated with this transaction */
//  NS_IMETHOD GetData(nsString& aResult);

  /** must be called before any DeleteTableCellTxn is instantiated */
  static nsresult ClassInit();

protected:

  // /* return PR_TRUE if aOtherTxn immediately follows this txn */
  // virtual PRBool IsSequentialInsert(DeleteTableCellTxn *aOtherTxn);
  
  /** the element to operate upon */
  nsCOMPtr<nsIDOMCharacterData> mElement;
  
  nsIDOMNode *mNodeToInsert;

  /** the presentation shell, which we'll need to get the selection */
  nsIPresShell* mPresShell;

  friend class TransactionFactory;

};

#endif
