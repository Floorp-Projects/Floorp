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
#include "nsIDOMSelection.h"

#ifdef NS_DEBUG
static PRBool gNoisy = PR_TRUE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

DeleteTextTxn::DeleteTextTxn()
  : EditTxn()
{
}

DeleteTextTxn::~DeleteTextTxn()
{
}

NS_IMETHODIMP DeleteTextTxn::Init(nsIEditor *aEditor,
                                  nsIDOMCharacterData *aElement,
                                  PRUint32 aOffset,
                                  PRUint32 aNumCharsToDelete)
{
  NS_ASSERTION(aEditor&&aElement, "bad arg");
  mEditor = do_QueryInterface(aEditor);
  mElement = do_QueryInterface(aElement);
  mOffset = aOffset;
  mNumCharsToDelete = aNumCharsToDelete;
  NS_ASSERTION(0!=aNumCharsToDelete, "bad arg, numCharsToDelete");
  PRUint32 count;
  aElement->GetLength(&count);
  NS_ASSERTION(count>=aNumCharsToDelete, "bad arg, numCharsToDelete.  Not enough characters in node");
  NS_ASSERTION(count>=aOffset+aNumCharsToDelete, "bad arg, numCharsToDelete.  Not enough characters in node");
  mDeletedText = "";
  return NS_OK;
}

NS_IMETHODIMP DeleteTextTxn::Do(void)
{
  if (gNoisy) { printf("Do Delete Text\n"); }
  nsresult result = NS_ERROR_NULL_POINTER;
  if (mEditor && mElement)
  {
    // get the text that we're about to delete
    result = mElement->SubstringData(mOffset, mNumCharsToDelete, mDeletedText);
    NS_ASSERTION(NS_SUCCEEDED(result), "could not get text to delete.");
    result = mElement->DeleteData(mOffset, mNumCharsToDelete);
    if (NS_SUCCEEDED(result))
    {
      nsCOMPtr<nsIDOMSelection> selection;
      nsresult selectionResult = mEditor->GetSelection(getter_AddRefs(selection));
      if (NS_SUCCEEDED(selectionResult) && selection) {
        selectionResult = selection->Collapse(mElement, mOffset);
        NS_ASSERTION((NS_SUCCEEDED(selectionResult)), "selection could not be collapsed after undo of insert.");
      }
    }
  }
  return result;
}

//XXX: we may want to store the selection state and restore it properly
//     was it an insertion point or an extended selection?
NS_IMETHODIMP DeleteTextTxn::Undo(void)
{
  if (gNoisy) { printf("Undo Delete Text\n"); }
  nsresult result = NS_ERROR_NULL_POINTER;
  if (mEditor && mElement)
  {
    result = mElement->InsertData(mOffset, mDeletedText);
    if (NS_SUCCEEDED(result))
    {
      nsCOMPtr<nsIDOMSelection> selection;
      nsresult selectionResult = mEditor->GetSelection(getter_AddRefs(selection));
      if (NS_SUCCEEDED(selectionResult) && selection) {
        selectionResult = selection->Collapse(mElement, mOffset);
        NS_ASSERTION((NS_SUCCEEDED(selectionResult)), "selection could not be collapsed after undo of insert.");
      }
    }
  }
  return result;
}

NS_IMETHODIMP DeleteTextTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP DeleteTextTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP DeleteTextTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Insert Text: ";
    **aString += mDeletedText;
  }
  return NS_OK;
}

NS_IMETHODIMP DeleteTextTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove Text: ";
    **aString += mDeletedText;
  }
  return NS_OK;
}
