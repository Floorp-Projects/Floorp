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

#include "DeleteRangeTxn.h"
#include "editor.h"
#include "nsIDOMRange.h"

// note that aEditor is not refcounted
DeleteRangeTxn::DeleteRangeTxn(nsEditor       *aEditor,
                               nsIDOMRange    *aRange)
  : EditTxn(aEditor)
{
  aRange->GetStartParent(getter_AddRefs(mStartParent));
  aRange->GetEndParent(getter_AddRefs(mEndParent));
  aRange->GetStartOffset(&mStartOffset);
  aRange->GetEndOffset(&mEndOffset);
}

DeleteRangeTxn::~DeleteRangeTxn()
{
}

nsresult DeleteRangeTxn::Do(void)
{
  if (!mStartParent  ||  !mEndParent)
    return NS_ERROR_NULL_POINTER;

  nsresult result; 
  return result;
}

nsresult DeleteRangeTxn::Undo(void)
{
  if (!mStartParent  ||  !mEndParent)
    return NS_ERROR_NULL_POINTER;

  nsresult result;
  return result;
}

nsresult DeleteRangeTxn::Redo(void)
{
  if (!mStartParent  ||  !mEndParent)
    return NS_ERROR_NULL_POINTER;

  nsresult result;
  return result;
}

nsresult DeleteRangeTxn::GetIsTransient(PRBool *aIsTransient)
{
  if (nsnull!=aIsTransient)
    *aIsTransient = PR_FALSE;
  return NS_OK;
}

nsresult DeleteRangeTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

nsresult DeleteRangeTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

nsresult DeleteRangeTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Insert Range: ";
  }
  return NS_OK;
}

nsresult DeleteRangeTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove Range: ";
  }
  return NS_OK;
}
