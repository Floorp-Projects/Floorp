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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

NS_IMETHODIMP DeleteElementTxn::DoTransaction(void)
{
  if (gNoisy) { printf("%p Do Delete Element element = %p\n", this, mElement.get()); }
  if (!mElement) return NS_ERROR_NOT_INITIALIZED;

  nsresult result = mElement->GetParentNode(getter_AddRefs(mParent));
  if (NS_FAILED(result)) { return result; }
  if (!mParent) { return NS_OK; }  // this is a no-op, there's no parent to delete mElement from

#ifdef NS_DEBUG
  // begin debug output
  nsCOMPtr<nsIDOMElement> element;
  element = do_QueryInterface(mElement);
  nsAutoString elementTag; elementTag.AssignWithConversion("text node");
  if (element)
    element->GetTagName(elementTag);
  nsCOMPtr<nsIDOMElement> parentElement;
  parentElement = do_QueryInterface(mParent);
  nsAutoString parentElementTag; parentElementTag.AssignWithConversion("text node");
  if (parentElement)
    parentElement->GetTagName(parentElementTag);
  char *c, *p;
  c = elementTag.ToNewCString();
  p = parentElementTag.ToNewCString();
  if (c&&p)
  {
    if (gNoisy)
      printf("  DeleteElementTxn:  deleting child %s from parent %s\n", c, p); 
    nsCRT::free(c);
    nsCRT::free(p);
  }
  // end debug output
#endif

  // remember which child mElement was (by remembering which child was next)
  result = mElement->GetNextSibling(getter_AddRefs(mRefNode));  // can return null mRefNode

  nsCOMPtr<nsIDOMNode> resultNode;
  result = mParent->RemoveChild(mElement, getter_AddRefs(resultNode));
  return result;
}

NS_IMETHODIMP DeleteElementTxn::UndoTransaction(void)
{
  if (gNoisy) { printf("%p Undo Delete Element element = %p, parent = %p\n", this, mElement.get(), mParent.get()); }
  if (!mParent) { return NS_OK; } // this is a legal state, the txn is a no-op
  if (!mElement) { return NS_ERROR_NULL_POINTER; }

#ifdef NS_DEBUG
  // begin debug output
  nsCOMPtr<nsIDOMElement> element;
  element = do_QueryInterface(mElement);
  nsAutoString elementTag; elementTag.AssignWithConversion("text node");
  if (element)
    element->GetTagName(elementTag);
  nsCOMPtr<nsIDOMElement> parentElement;
  parentElement = do_QueryInterface(mParent);
  nsAutoString parentElementTag; parentElementTag.AssignWithConversion("text node");
  if (parentElement)
    parentElement->GetTagName(parentElementTag);
  char *c, *p;
  c = elementTag.ToNewCString();
  p = parentElementTag.ToNewCString();
  if (c&&p)
  {
    if (gNoisy)
      printf("  DeleteElementTxn:  inserting child %s back into parent %s\n", c, p); 
    nsCRT::free(c);
    nsCRT::free(p);
  }
  // end debug output
#endif

  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->InsertBefore(mElement, mRefNode, getter_AddRefs(resultNode));
  return result;
}

NS_IMETHODIMP DeleteElementTxn::RedoTransaction(void)
{
  if (gNoisy) { printf("%p Redo Delete Element element = %p, parent = %p\n", this, mElement.get(), mParent.get()); }
  if (!mParent) { return NS_OK; } // this is a legal state, the txn is a no-op
  if (!mElement) { return NS_ERROR_NULL_POINTER; }

  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->RemoveChild(mElement, getter_AddRefs(resultNode));
  return result;
}


NS_IMETHODIMP DeleteElementTxn::Merge(nsITransaction *aTransaction, PRBool *aDidMerge)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP DeleteElementTxn::GetTxnDescription(nsAWritableString& aString)
{
  aString.Assign(NS_LITERAL_STRING("DeleteElementTxn"));
  return NS_OK;
}
