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

class nsIDOMCharacterData;

/**
 * A transaction that changes an attribute of a content node. 
 * This transaction covers add, remove, and change attribute.
 */
class DeleteTextTxn : public EditTxn
{
public:

  DeleteTextTxn(nsEditor *aEditor,
                nsIDOMCharacterData *aElement,
                PRUint32 aOffset,
                PRUint32 aNumCharsToDelete);

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
  
  /** the offset into mElement where the deletion is to take place */
  PRUint32 mOffset;

  /** the number of characters to delete */
  PRUint32 mNumCharsToDelete;

  /** the text that was deleted */
  nsString mDeletedText;

};

#endif
