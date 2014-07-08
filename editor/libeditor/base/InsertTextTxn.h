/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InsertTextTxn_h__
#define InsertTextTxn_h__

#include "EditTxn.h"                    // for EditTxn, NS_DECL_EDITTXN
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"                       // for nsIID
#include "nsIDOMCharacterData.h"        // for nsIDOMCharacterData
#include "nsISupportsImpl.h"            // for NS_DECL_ISUPPORTS_INHERITED
#include "nsString.h"                   // for nsString
#include "nscore.h"                     // for NS_IMETHOD, nsAString

class nsIEditor;
class nsITransaction;


#define INSERT_TEXT_TXN_CID \
{/* 93276f00-ab2c-11d2-8f4b-006008159b0c*/ \
0x93276f00, 0xab2c, 0x11d2, \
{0x8f, 0xb4, 0x0, 0x60, 0x8, 0x15, 0x9b, 0xc} }

/**
  * A transaction that inserts text into a content node. 
  */
class InsertTextTxn : public EditTxn
{
public:

  static const nsIID& GetCID() { static const nsIID iid = INSERT_TEXT_TXN_CID; return iid; }

  /** initialize the transaction
    * @param aElement the text content node
    * @param aOffset  the location in aElement to do the insertion
    * @param aString  the new text to insert
    * @param aPresShell used to get and set the selection
    */
  NS_IMETHOD Init(nsIDOMCharacterData *aElement,
                  uint32_t aOffset,
                  const nsAString& aString,
                  nsIEditor *aEditor);

  InsertTextTxn();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(InsertTextTxn, EditTxn)
	
  NS_DECL_EDITTXN

  NS_IMETHOD Merge(nsITransaction *aTransaction, bool *aDidMerge);

  /** return the string data associated with this transaction */
  NS_IMETHOD GetData(nsString& aResult);

protected:
  virtual ~InsertTextTxn();

  /** return true if aOtherTxn immediately follows this txn */
  virtual bool IsSequentialInsert(InsertTextTxn *aOtherTxn);
  
  /** the text element to operate upon */
  nsCOMPtr<nsIDOMCharacterData> mElement;
  
  /** the offset into mElement where the insertion is to take place */
  uint32_t mOffset;

  /** the text to insert into mElement at mOffset */
  nsString mStringToInsert;

  /** the editor, which we'll need to get the selection */
  nsIEditor *mEditor;   
};

#endif
