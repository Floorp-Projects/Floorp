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

#include "nsInsertHTMLTxn.h"
#include "nsIDOMSelection.h"
#include "nsIContent.h"
#include "nsIDOMNSRange.h"


nsInsertHTMLTxn::nsInsertHTMLTxn() : EditTxn(), mSrc("")
{
}

NS_IMETHODIMP nsInsertHTMLTxn::Init(nsString& aSrc, nsIEditor* aEditor)
{
  if (!aEditor)
    return NS_ERROR_NULL_POINTER;

  mSrc = aSrc;
  mEditor = do_QueryInterface(aEditor);
  return NS_OK;
}


nsInsertHTMLTxn::~nsInsertHTMLTxn()
{
  //NS_RELEASE(mStr); // do nsStrings have to be released?
}

NS_IMETHODIMP nsInsertHTMLTxn::Do(void)
{
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult res = mEditor->GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(res) && selection)
  {
    // Get the first range in the selection, and save it in mRange:
    res = selection->GetRangeAt(0, getter_AddRefs(mRange));
    if (NS_SUCCEEDED(res))
    {
      nsCOMPtr<nsIDOMNSRange> nsrange (do_QueryInterface(mRange));
      if (nsrange)
      {
#ifdef DEBUG_akkana
        char* str = mSrc.ToNewCString();
        printf("Calling nsInsertHTMLTxn::Do(%s)\n", str);
        delete[] str;
#endif /* DEBUG_akkana */
        res = nsrange->InsertFragment(mSrc);
        // XXX NOTE: this part isn't going through the transaction manager!
      }
    }
#ifdef DEBUG_akkana
    else printf("nsInsertHTMLTxn::Do: Couldn't get selection range!\n");
#endif
  }

  return res;
}

NS_IMETHODIMP nsInsertHTMLTxn::Undo(void)
{
#ifdef DEBUG_akkana
  printf("%p Undo Insert HTML\n", this);
#endif /* DEBUG_akkana */

  if (!mRange)
    return NS_ERROR_NULL_POINTER;

  return mRange->DeleteContents();
}

NS_IMETHODIMP nsInsertHTMLTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull != aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsInsertHTMLTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsInsertHTMLTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove HTML: ";
  }
  return NS_OK;
}

NS_IMETHODIMP nsInsertHTMLTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Insert HTML: ";
  }
  return NS_OK;
}
