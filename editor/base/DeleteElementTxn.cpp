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

#include "DeleteElementTxn.h"
#ifdef NS_DEBUG
#include "nsIDOMElement.h"
#endif


DeleteElementTxn::DeleteElementTxn()
  : EditTxn()
{
}

nsresult DeleteElementTxn::Init(nsIDOMNode *aElement,
                                nsIDOMNode *aParent)
{
  if ((nsnull!=aElement) && (nsnull!=aParent))
  {
    mElement = aElement;
    mParent = aParent;
    return NS_OK;
  }
  else
    return NS_ERROR_NULL_POINTER;
}


DeleteElementTxn::~DeleteElementTxn()
{
}

nsresult DeleteElementTxn::Do(void)
{
  if (!mParent  ||  !mElement)
    return NS_ERROR_NULL_POINTER;
#ifdef NS_DEBUG
  // begin debug output
  nsCOMPtr<nsIDOMElement> element=mElement.get();
  nsAutoString elementTag="text node";
  if (element)
    element->GetTagName(elementTag);
  nsCOMPtr<nsIDOMElement> parentElement = (nsCOMPtr<nsIDOMElement>)mParent;
  nsAutoString parentElementTag="text node";
  if (parentElement)
    parentElement->GetTagName(parentElementTag);
  char *c, *p;
  c = elementTag.ToNewCString();
  p = parentElementTag.ToNewCString();
  if (c&&p)
  {
    printf("DeleteElementTxn:  deleting child %s from parent %s\n", c, p); 
    delete [] c;
    delete [] p;
  }
  // end debug output
  // begin sanity check 1: parent-child relationship
  nsresult testResult;
  nsCOMPtr<nsIDOMNode> parentNode;
  testResult = mElement->GetParentNode(getter_AddRefs(parentNode));
  NS_ASSERTION((NS_SUCCEEDED(testResult)), "bad mElement, couldn't get parent");
  NS_ASSERTION((parentNode==mParent), "bad mParent, mParent!=mElement->GetParent() ");
  // end sanity check 1.
#endif

  // remember which child mElement was (by remembering which child was next)
  nsresult result = mElement->GetNextSibling(getter_AddRefs(mRefNode));  // can return null mRefNode

  nsCOMPtr<nsIDOMNode> resultNode;
  result = mParent->RemoveChild(mElement, getter_AddRefs(resultNode));
  return result;
}

nsresult DeleteElementTxn::Undo(void)
{
  if (!mParent  ||  !mElement)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->InsertBefore(mElement, mRefNode, getter_AddRefs(resultNode));
  return result;
}

nsresult DeleteElementTxn::Redo(void)
{
  if (!mParent  ||  !mElement)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->RemoveChild(mElement, getter_AddRefs(resultNode));
  return result;
}

nsresult DeleteElementTxn::GetIsTransient(PRBool *aIsTransient)
{
  if (nsnull!=aIsTransient)
    *aIsTransient = PR_FALSE;
  return NS_OK;
}

nsresult DeleteElementTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

nsresult DeleteElementTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

nsresult DeleteElementTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Insert Element: ";
  }
  return NS_OK;
}

nsresult DeleteElementTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove Element: ";
  }
  return NS_OK;
}
