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

#include "InsertElementTxn.h"

#ifdef NS_DEBUG
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif


InsertElementTxn::InsertElementTxn()
  : EditTxn()
{
}

nsresult InsertElementTxn::Init(nsIDOMNode *aNode,
                                nsIDOMNode *aParent,
                                PRInt32     aOffset)
{
  if (!aNode || !aParent)
    return NS_ERROR_NULL_POINTER;

  mNode = do_QueryInterface(aNode);
  mParent = do_QueryInterface(aParent);
  mOffset = aOffset;
  return NS_OK;
}


InsertElementTxn::~InsertElementTxn()
{
}

nsresult InsertElementTxn::Do(void)
{
  if (!mNode || !mParent)
    return NS_ERROR_NULL_POINTER;

  nsresult result;
  nsCOMPtr<nsIDOMNode>refNode;
  if (0!=mOffset)
  { // get a ref node
    PRInt32 i=0;
    result = mParent->GetFirstChild(getter_AddRefs(refNode));
    for (; i<mOffset; i++)
    {
      nsCOMPtr<nsIDOMNode>nextSib;
      result = refNode->GetNextSibling(getter_AddRefs(nextSib));  
      if (NS_FAILED(result)) {
        break;  // couldn't get a next sibling, so make aNode the first child
      }
      refNode = do_QueryInterface(nextSib);
      if (!refNode) {
        break;  // couldn't get a next sibling, so make aNode the first child
      }
    }
  }

  nsCOMPtr<nsIDOMNode> resultNode;
  result = mParent->InsertBefore(mNode, refNode, getter_AddRefs(resultNode));
  return result;
}

nsresult InsertElementTxn::Undo(void)
{
  if (!mNode || !mParent)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->RemoveChild(mNode, getter_AddRefs(resultNode));
  return result;
}

nsresult InsertElementTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

nsresult InsertElementTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

nsresult InsertElementTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove Element: ";
  }
  return NS_OK;
}

nsresult InsertElementTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Insert Element: ";
  }
  return NS_OK;
}
