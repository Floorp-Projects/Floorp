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

#define INSERTTEXTTXN_IID \
{/* 93276f00-ab2c-11d2-8f4b-006008159b0c*/ \
0x93276f00, 0xab2c, 0x11d2, \
{0x8f, 0xb4, 0x0, 0x60, 0x8, 0x15, 0x9b, 0xc} }

class nsIDOMCharacterData;

/**
 * A transaction that changes an attribute of a content node. 
 * This transaction covers add, remove, and change attribute.
 */
class InsertTextTxn : public EditTxn
{
public:

  InsertTextTxn(nsEditor *aEditor,
                nsIDOMCharacterData *aElement,
                PRUint32 aOffset,
                const nsString& aStringToInsert);

  virtual nsresult Do(void);

  virtual nsresult Undo(void);

  virtual nsresult GetIsTransient(PRBool *aIsTransient);

  virtual nsresult Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  virtual nsresult Write(nsIOutputStream *aOutputStream);

  virtual nsresult GetUndoString(nsString **aString);

  virtual nsresult GetRedoString(nsString **aString);

// nsISupports declarations

  // override QueryInterface to handle InsertTextTxn request
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  static const nsIID& IID() { static nsIID iid = INSERTTEXTTXN_IID; return iid; }


  virtual nsresult GetData(nsString& aResult);

protected:
  
  /** the text element to operate upon */
  nsIDOMCharacterData *mElement;
  
  /** the offset into mElement where the insertion is to take place */
  PRUint32 mOffset;

  /** the value to set the attribute to (ignored if mRemoveAttribute==PR_TRUE) */
  nsString mValue;

  /** the text to insert into mElement at mOffset */
  nsString mStringToInsert;

};

#endif
