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
