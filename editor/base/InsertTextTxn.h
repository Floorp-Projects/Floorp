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

#ifndef InsertTextTxn_h__
#define InsertTextTxn_h__

#include "EditTxn.h"
#include "nsIDOMCharacterData.h"
#include "nsCOMPtr.h"

#define INSERT_TEXT_TXN_IID \
{/* 93276f00-ab2c-11d2-8f4b-006008159b0c*/ \
0x93276f00, 0xab2c, 0x11d2, \
{0x8f, 0xb4, 0x0, 0x60, 0x8, 0x15, 0x9b, 0xc} }

class nsIPresShell;

/**
  * A transaction that inserts text into a content node. 
  */
class InsertTextTxn : public EditTxn
{
public:

  /** used to name aggregate transactions that consist only of a single InsertTextTxn,
    * or a DeleteSelection followed by an InsertTextTxn.
    */
  static nsIAtom *gInsertTextTxnName;
	
  /** initialize the transaction
    * @param aElement the text content node
    * @param aOffset  the location in aElement to do the insertion
    * @param aString  the new text to insert
    * @param aPresShell used to get and set the selection
    */
  virtual nsresult Init(nsIDOMCharacterData *aElement,
                        PRUint32 aOffset,
                        const nsString& aString,
                        nsIPresShell* aPresShell);

private:
	
	InsertTextTxn();

public:
	
  virtual nsresult Do(void);

  virtual nsresult Undo(void);

  virtual nsresult Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  virtual nsresult Write(nsIOutputStream *aOutputStream);

  virtual nsresult GetUndoString(nsString **aString);

  virtual nsresult GetRedoString(nsString **aString);

// nsISupports declarations

  // override QueryInterface to handle InsertTextTxn request
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  static const nsIID& IID() { static nsIID iid = INSERT_TEXT_TXN_IID; return iid; }


  /** return the string data associated with this transaction */
  virtual nsresult GetData(nsString& aResult);

  /** must be called before any InsertTextTxn is instantiated */
  static nsresult ClassInit();

protected:

  /** return PR_TRUE if aOtherTxn immediately follows this txn */
  virtual PRBool IsSequentialInsert(InsertTextTxn *aOtherTxn);
  
  /** the text element to operate upon */
  nsCOMPtr<nsIDOMCharacterData> mElement;
  
  /** the offset into mElement where the insertion is to take place */
  PRUint32 mOffset;

  /** the text to insert into mElement at mOffset */
  nsString mStringToInsert;

  /** the presentation shell, which we'll need to get the selection */
  nsIPresShell* mPresShell;

  friend class TransactionFactory;

};

#endif
