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

#ifdef NS_DEBUG
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif


DeleteElementTxn::DeleteElementTxn()
  : EditTxn()
{
}

NS_IMETHODIMP DeleteElementTxn::Init(nsIDOMNode *aElement)
{
  if (nsnull!=aElement)  {
    mElement = do_QueryInterface(aElement);
  }
  else 
    return NS_ERROR_NULL_POINTER;
  return NS_OK;
}


DeleteElementTxn::~DeleteElementTxn()
{
}

NS_IMETHODIMP DeleteElementTxn::Do(void)
{
  if (gNoisy) { printf("%p Do Delete Element element = %p\n", this, mElement.get()); }
  if (!mElement)
    return NS_ERROR_NULL_POINTER;

  nsresult result = mElement->GetParentNode(getter_AddRefs(mParent));
  if (NS_FAILED(result)) {
    return result;
  }
  if (!mParent) {
    return NS_OK;   // this is a no-op, there's no parent to delete mElement from
  }

#ifdef NS_DEBUG
  // begin debug output
  nsCOMPtr<nsIDOMElement> element;
  element = do_QueryInterface(mElement);
  nsAutoString elementTag="text node";
  if (element)
    element->GetTagName(elementTag);
  nsCOMPtr<nsIDOMElement> parentElement;
  parentElement = do_QueryInterface(mParent);
  nsAutoString parentElementTag="text node";
  if (parentElement)
    parentElement->GetTagName(parentElementTag);
  char *c, *p;
  c = elementTag.ToNewCString();
  p = parentElementTag.ToNewCString();
  if (c&&p)
  {
    if (gNoisy)
      printf("  DeleteElementTxn:  deleting child %s from parent %s\n", c, p); 
    delete [] c;
    delete [] p;
  }
  // end debug output
#endif

  // remember which child mElement was (by remembering which child was next)
  result = mElement->GetNextSibling(getter_AddRefs(mRefNode));  // can return null mRefNode

  nsCOMPtr<nsIDOMNode> resultNode;
  result = mParent->RemoveChild(mElement, getter_AddRefs(resultNode));
  return result;
}

NS_IMETHODIMP DeleteElementTxn::Undo(void)
{
  if (gNoisy) { printf("%p Undo Delete Element element = %p, parent = %p\n", this, mElement.get(), mParent.get()); }
  if (!mParent) { return NS_OK; } // this is a legal state, the txn is a no-op
  if (!mElement) { return NS_ERROR_NULL_POINTER; }

#ifdef NS_DEBUG
  // begin debug output
  nsCOMPtr<nsIDOMElement> element;
  element = do_QueryInterface(mElement);
  nsAutoString elementTag="text node";
  if (element)
    element->GetTagName(elementTag);
  nsCOMPtr<nsIDOMElement> parentElement;
  parentElement = do_QueryInterface(mParent);
  nsAutoString parentElementTag="text node";
  if (parentElement)
    parentElement->GetTagName(parentElementTag);
  char *c, *p;
  c = elementTag.ToNewCString();
  p = parentElementTag.ToNewCString();
  if (c&&p)
  {
    if (gNoisy)
      printf("  DeleteElementTxn:  inserting child %s back into parent %s\n", c, p); 
    delete [] c;
    delete [] p;
  }
  // end debug output
#endif

  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->InsertBefore(mElement, mRefNode, getter_AddRefs(resultNode));
  return result;
}

NS_IMETHODIMP DeleteElementTxn::Redo(void)
{
  if (gNoisy) { printf("%p Redo Delete Element element = %p, parent = %p\n", this, mElement.get(), mParent.get()); }
  if (!mParent) { return NS_OK; } // this is a legal state, the txn is a no-op
  if (!mElement) { return NS_ERROR_NULL_POINTER; }

  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->RemoveChild(mElement, getter_AddRefs(resultNode));
  return result;
}


NS_IMETHODIMP DeleteElementTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP DeleteElementTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP DeleteElementTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Insert Element: ";
  }
  return NS_OK;
}

NS_IMETHODIMP DeleteElementTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove Element: ";
  }
  return NS_OK;
}
