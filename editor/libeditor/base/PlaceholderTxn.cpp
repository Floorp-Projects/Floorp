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

#include "PlaceholderTxn.h"
#include "nsVoidArray.h"

#ifdef NS_DEBUG
static PRBool gNoisy = PR_TRUE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif


PlaceholderTxn::PlaceholderTxn()
  : EditAggregateTxn()
{
  mAbsorb=PR_TRUE;
}


PlaceholderTxn::~PlaceholderTxn()
{
}

NS_IMETHODIMP PlaceholderTxn::Do(void)
{
  if (gNoisy) { printf("PlaceholderTxn Do\n"); }
  return NS_OK;
}

NS_IMETHODIMP PlaceholderTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  // set out param default value
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  nsresult result = NS_OK;
  if ((nsnull!=aDidMerge) && (nsnull!=aTransaction))
  {
    EditTxn *editTxn = (EditTxn*)aTransaction;  //XXX: hack, not safe!  need nsIEditTransaction!
    if (PR_TRUE==mAbsorb)
    { // yep, it's one of ours.  Assimilate it.
      AppendChild(editTxn);
      *aDidMerge = PR_TRUE;
      if (gNoisy) { printf("Placeholder txn assimilated %p\n", aTransaction); }
    }
    else
    { // let our last child txn make the choice
      PRInt32 count = mChildren->Count();
      if (0<count)
      {
        EditTxn *lastTxn = (EditTxn*)(mChildren->ElementAt(count-1));
        if (lastTxn)
        {
          lastTxn->Merge(aDidMerge, aTransaction);
        }
      }
    }
  }
  return result;
}

