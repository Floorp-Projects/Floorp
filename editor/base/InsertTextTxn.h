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

#ifndef InsertTextTxn_h__
#define InsertTextTxn_h__

#include "EditTxn.h"
#include "nsIDOMCharacterData.h"
#include "nsIEditor.h"
#include "nsCOMPtr.h"

#define INSERT_TEXT_TXN_CID \
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

  static const nsIID& GetCID() { static nsIID iid = INSERT_TEXT_TXN_CID; return iid; }

  virtual ~InsertTextTxn();

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
  NS_IMETHOD Init(nsIDOMCharacterData *aElement,
                  PRUint32 aOffset,
                  const nsAReadableString& aString,
                  nsIEditor *aEditor);

private:
	
	InsertTextTxn();

public:
	
  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

// nsISupports declarations

  // override QueryInterface to handle InsertTextTxn request
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  /** return the string data associated with this transaction */
  NS_IMETHOD GetData(nsString& aResult);

  /** must be called before any InsertTextTxn is instantiated */
  static nsresult ClassInit();

  /** must be called once we are guaranteed all InsertTextTxn have completed */
  static nsresult ClassShutdown();

protected:

  /** return PR_TRUE if aOtherTxn immediately follows this txn */
  virtual PRBool IsSequentialInsert(InsertTextTxn *aOtherTxn);
  
  /** the text element to operate upon */
  nsCOMPtr<nsIDOMCharacterData> mElement;
  
  /** the offset into mElement where the insertion is to take place */
  PRUint32 mOffset;

  /** the text to insert into mElement at mOffset */
  nsString mStringToInsert;

  /** the editor, which we'll need to get the selection */
  nsIEditor *mEditor;   

  friend class TransactionFactory;

  friend class nsDerivedSafe<InsertTextTxn>; // work around for a compiler bug

};

#endif
