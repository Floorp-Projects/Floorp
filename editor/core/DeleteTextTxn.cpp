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

#include "DeleteTextTxn.h"
#include "nsIDOMCharacterData.h"


DeleteTextTxn::DeleteTextTxn()
  : EditTxn()
{
}

nsresult DeleteTextTxn::Init(nsIDOMCharacterData *aElement,
                             PRUint32 aOffset,
                             PRUint32 aNumCharsToDelete)
{
  mElement = aElement;
  mOffset = aOffset;
  mNumCharsToDelete = aNumCharsToDelete;
  mDeletedText = "";
  return NS_OK;
}

nsresult DeleteTextTxn::Do(void)
{
  // get the text that we're about to delete
  nsresult result = mElement->SubstringData(mOffset, mNumCharsToDelete, mDeletedText);
  NS_ASSERTION(NS_SUCCEEDED(result), "could not get text to delete.");
  return (mElement->DeleteData(mOffset, mNumCharsToDelete));
}

nsresult DeleteTextTxn::Undo(void)
{
  return (mElement->InsertData(mOffset, mDeletedText));
}

nsresult DeleteTextTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

nsresult DeleteTextTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

nsresult DeleteTextTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Insert Text: ";
    **aString += mDeletedText;
  }
  return NS_OK;
}

nsresult DeleteTextTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove Text: ";
    **aString += mDeletedText;
  }
  return NS_OK;
}
