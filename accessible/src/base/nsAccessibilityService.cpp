/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsIAccessibilityService.h"
#include "nsAccessibilityService.h"
#include "nsAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsRootAccessible.h"
#include "nsINameSpaceManager.h"
#include "nsMutableAccessible.h"

nsAccessibilityService::nsAccessibilityService()
{
	NS_INIT_REFCNT();
}

nsAccessibilityService::~nsAccessibilityService()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAccessibilityService, nsIAccessibilityService);


/*
class nsHTMLTextAccessible : public nsIAccessible
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLE

  nsHTMLTextAccessible(nsIDOMNode* aNode);

  virtual ~nsHTMLTextAccessible();

  nsCOMPtr<nsIDOMNode> mNode;
};

NS_IMPL_ISUPPORTS1(nsHTMLTextAccessible, nsIAccessible)

nsHTMLTextAccessible::nsHTMLTextAccessible(nsIDOMNode* aNode)
{
  NS_INIT_ISUPPORTS();
  mNode = aNode;
}

nsHTMLTextAccessible::~nsHTMLTextAccessible()
{
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccParent(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccNextSibling(nsIAccessible **_retval)
{ 
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccLastChild(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccChildCount(PRInt32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccName(PRUnichar **_retval)
{
  if (mNode) {
    nsAutoString value;
    mNode->GetNodeValue(value);
    *_retval = value.ToNewUnicode();
    return NS_OK;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccValue(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::SetAccName(const PRUnichar *name)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::SetAccValue(const PRUnichar *value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccDescription(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccRole(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccState(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccDefaultAction(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccHelp(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccFocused(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::AccGetAt(PRInt32 x, PRInt32 y, nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::AccNavigateRight(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::AccNavigateLeft(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::AccNavigateUp(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::AccNavigateDown(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::AccAddSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::AccRemoveSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::AccExtendSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::AccTakeSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::AccTakeFocus()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextAccessible::AccDoDefaultAction()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
*/


////////////////////////////////////////////////////////////////////////////////
// nsIAccessibilityService methods:

NS_IMETHODIMP 
nsAccessibilityService::CreateRootAccessible(nsISupports* aPresContext, nsIAccessible **_retval)
{
  nsCOMPtr<nsIPresContext> c = do_QueryInterface(aPresContext);
  NS_ASSERTION(c,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> s;
  c->GetShell(getter_AddRefs(s));

  NS_ASSERTION(s,"Error not presshell!!");

  *_retval = new nsRootAccessible(s);

  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateMutableAccessible(nsIDOMNode* aNode, nsIMutableAccessible **_retval)
{
  *_retval = new nsMutableAccessible(aNode);
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLBlockAccessible(nsIAccessible* aAccessible, nsIDOMNode* node, nsISupports* aPresContext, nsIAccessible **_retval)
{
  nsCOMPtr<nsIContent> n = do_QueryInterface(node);
  NS_ASSERTION(n,"Error non nsIContent passed to accessible factory!!!");

  nsCOMPtr<nsIPresContext> c = do_QueryInterface(aPresContext);
  NS_ASSERTION(c,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> s;
  c->GetShell(getter_AddRefs(s));

  NS_ASSERTION(s,"Error not presshell!!");


  *_retval = new nsHTMLBlockAccessible(aAccessible, n,s);
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////

nsresult
NS_NewAccessibilityService(nsIAccessibilityService** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsAccessibilityService* a = new nsAccessibilityService();
    if (a == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(a);
    *aResult = a;
    return NS_OK;
}

