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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "DeleteTextTxn.h"
#include "nsIDOMCharacterData.h"
#include "nsISelection.h"

#ifdef NS_DEBUG
static PRBool gNoisy = PR_FALSE;
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
  if (!aEditor || !aElement) { return NS_ERROR_NULL_POINTER; }

  mEditor = aEditor;
  mElement = do_QueryInterface(aElement);
  mOffset = aOffset;
  mNumCharsToDelete = aNumCharsToDelete;
  NS_ASSERTION(0!=aNumCharsToDelete, "bad arg, numCharsToDelete");
  PRUint32 count;
  aElement->GetLength(&count);
  NS_ASSERTION(count>=aNumCharsToDelete, "bad arg, numCharsToDelete.  Not enough characters in node");
  NS_ASSERTION(count>=aOffset+aNumCharsToDelete, "bad arg, numCharsToDelete.  Not enough characters in node");
  mDeletedText.SetLength(0);
  return NS_OK;
}

NS_IMETHODIMP DeleteTextTxn::DoTransaction(void)
{
  if (gNoisy) { printf("Do Delete Text\n"); }
  NS_ASSERTION(mEditor && mElement, "bad state");
  if (!mEditor || !mElement) { return NS_ERROR_NOT_INITIALIZED; }
  // get the text that we're about to delete
  nsresult result = mElement->SubstringData(mOffset, mNumCharsToDelete, mDeletedText);
  NS_ASSERTION(NS_SUCCEEDED(result), "could not get text to delete.");
  result = mElement->DeleteData(mOffset, mNumCharsToDelete);
  if (NS_FAILED(result)) return result;

  // only set selection to deletion point if editor gives permission
  PRBool bAdjustSelection;
  mEditor->ShouldTxnSetSelection(&bAdjustSelection);
  if (bAdjustSelection)
  {
    nsCOMPtr<nsISelection> selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) return result;
    if (!selection) return NS_ERROR_NULL_POINTER;
    result = selection->Collapse(mElement, mOffset);
    NS_ASSERTION((NS_SUCCEEDED(result)), "selection could not be collapsed after undo of deletetext.");
  }
  else
  {
    // do nothing - dom range gravity will adjust selection
  }
  return result;
}

//XXX: we may want to store the selection state and restore it properly
//     was it an insertion point or an extended selection?
NS_IMETHODIMP DeleteTextTxn::UndoTransaction(void)
{
  if (gNoisy) { printf("Undo Delete Text\n"); }
  NS_ASSERTION(mEditor && mElement, "bad state");
  if (!mEditor || !mElement) { return NS_ERROR_NOT_INITIALIZED; }

  nsresult result;
  result = mElement->InsertData(mOffset, mDeletedText);
  return result;
}

NS_IMETHODIMP DeleteTextTxn::Merge(nsITransaction *aTransaction, PRBool *aDidMerge)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP DeleteTextTxn::GetTxnDescription(nsAWritableString& aString)
{
  aString.Assign(NS_LITERAL_STRING("DeleteTextTxn: "));
  aString += mDeletedText;
  return NS_OK;
}
