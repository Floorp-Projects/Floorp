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
#include "nsIDOMCharacterData.h"
#include "nsCOMPtr.h"

#define DELETE_TEXT_TXN_IID \
{/* 4d3a2720-ac49-11d2-86d8-000064657374 */ \
0x4d3a2720, 0xac49, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

/**
 * A transaction that changes an attribute of a content node. 
 * This transaction covers add, remove, and change attribute.
 */
class DeleteTextTxn : public EditTxn
{
public:

  virtual nsresult Init(nsIDOMCharacterData *aElement,
                        PRUint32 aOffset,
                        PRUint32 aNumCharsToDelete);

private:
  DeleteTextTxn();

public:
	
  virtual nsresult Do(void);

  virtual nsresult Undo(void);

  virtual nsresult GetIsTransient(PRBool *aIsTransient);

  virtual nsresult Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  virtual nsresult Write(nsIOutputStream *aOutputStream);

  virtual nsresult GetUndoString(nsString **aString);

  virtual nsresult GetRedoString(nsString **aString);

protected:
  
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
