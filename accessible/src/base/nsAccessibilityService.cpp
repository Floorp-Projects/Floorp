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
#include "nsLayoutAtoms.h"
#include "nsSelectAccessible.h"

//--------------------


nsAccessibilityService::nsAccessibilityService()
{
	NS_INIT_REFCNT();
}

nsAccessibilityService::~nsAccessibilityService()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAccessibilityService, nsIAccessibilityService);




////////////////////////////////////////////////////////////////////////////////
// nsIAccessibilityService methods:

NS_IMETHODIMP 
nsAccessibilityService::CreateRootAccessible(nsISupports* aPresContext, nsISupports* aFrame, nsIAccessible **_retval)
{
  nsIFrame* f = (nsIFrame*)aFrame;

  nsCOMPtr<nsIPresContext> c = do_QueryInterface(aPresContext);
  NS_ASSERTION(c,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> s;
  c->GetShell(getter_AddRefs(s));

  NS_ASSERTION(s,"Error not presshell!!");

  nsCOMPtr<nsIWeakReference> wr = getter_AddRefs(NS_GetWeakReference(s));

  *_retval = new nsRootAccessible(wr,f);
  NS_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateMutableAccessible(nsISupports* aNode, nsIMutableAccessible **_retval)
{
  *_retval = new nsMutableAccessible(aNode);
  NS_ADDREF(*_retval);
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

  nsCOMPtr<nsIWeakReference> wr = getter_AddRefs(NS_GetWeakReference(s));

  *_retval = new nsHTMLBlockAccessible(aAccessible, n,wr);
  NS_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLSelectAccessible(nsIAtom* aPopupAtom, nsIDOMNode* node, nsISupports* aPresContext, nsIAccessible **_retval)
{
  nsCOMPtr<nsIContent> n = do_QueryInterface(node);
  NS_ASSERTION(n,"Error non nsIContent passed to accessible factory!!!");

  nsCOMPtr<nsIPresContext> c = do_QueryInterface(aPresContext);
  NS_ASSERTION(c,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> s;
  c->GetShell(getter_AddRefs(s)); 

  nsCOMPtr<nsIWeakReference> wr = getter_AddRefs(NS_GetWeakReference(s));

  *_retval = new nsSelectAccessible(aPopupAtom, nsnull, n, wr);
  NS_ADDREF(*_retval);
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

