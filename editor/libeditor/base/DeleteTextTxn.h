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

#ifndef DeleteTextTxn_h__
#define DeleteTextTxn_h__

#include "EditTxn.h"
#include "nsIEditor.h"
#include "nsIDOMCharacterData.h"
#include "nsCOMPtr.h"

#define DELETE_TEXT_TXN_CID \
{/* 4d3a2720-ac49-11d2-86d8-000064657374 */ \
0x4d3a2720, 0xac49, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

/**
 * A transaction that removes text from a content node. 
 */
class DeleteTextTxn : public EditTxn
{
public:

  static const nsIID& GetCID() { static nsIID iid = DELETE_TEXT_TXN_CID; return iid; }

  /** initialize the transaction.
    * @param aEditor  the provider of basic editing operations
    * @param aElement the content node to remove text from
    * @param aOffset  the location in aElement to begin the deletion
    * @param aNumCharsToDelete  the number of characters to delete.  Not the number of bytes!
    */
  NS_IMETHOD Init(nsIEditor *aEditor,
                  nsIDOMCharacterData *aElement,
                  PRUint32 aOffset,
                  PRUint32 aNumCharsToDelete);

private:
  DeleteTextTxn();

public:
	virtual ~DeleteTextTxn();

  NS_IMETHOD Do(void);

  NS_IMETHOD Undo(void);

  NS_IMETHOD Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  NS_IMETHOD Write(nsIOutputStream *aOutputStream);

  NS_IMETHOD GetUndoString(nsString *aString);

  NS_IMETHOD GetRedoString(nsString *aString);

protected:

  /** the provider of basic editing operations */
  nsIEditor* mEditor;

  /** the text element to operate upon */
  nsCOMPtr<nsIDOMCharacterData> mElement;
  
  /** the offset into mElement where the deletion is to take place */
  PRUint32 mOffset;

  /** the number of characters to delete */
  PRUint32 mNumCharsToDelete;

  /** the text that was deleted */
  nsString mDeletedText;

  friend class TransactionFactory;

};

#endif
