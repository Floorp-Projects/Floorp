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
#include "nsHTMLFormControlAccessible.h"
#include "nsLayoutAtoms.h"
#include "nsSelectAccessible.h"
#include "nsHTMLTextAccessible.h"

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
  nsIFrame* f = NS_STATIC_CAST(nsIFrame*, aFrame);

  nsCOMPtr<nsIPresContext> c = do_QueryInterface(aPresContext);
  NS_ASSERTION(c,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> s;
  c->GetShell(getter_AddRefs(s));

  NS_ASSERTION(s,"Error not presshell!!");

  nsCOMPtr<nsIWeakReference> wr = getter_AddRefs(NS_GetWeakReference(s));

  *_retval = new nsRootAccessible(wr,f);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateMutableAccessible(nsISupports* aNode, nsIMutableAccessible **_retval)
{
  *_retval = new nsMutableAccessible(aNode);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
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
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
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
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsIAccessible createHTMLCheckboxAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLCheckboxAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(shell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLCheckboxAccessible(shell,node);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsIAccessible createHTMLCheckboxAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLRadioButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIPresShell> shell;
  GetInfo(aFrame, &frame, getter_AddRefs(shell), getter_AddRefs(node));

  *_retval = new nsHTMLRadioButtonAccessible(shell,node);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsIAccessible createHTMLCheckboxAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(shell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLButtonAccessible(shell,node);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsIAccessible createHTMLTextAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLTextAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(shell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTextAccessible(shell, node);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsAccessibilityService::GetInfo(nsISupports* aFrame, nsIFrame** aRealFrame, nsIPresShell** aShell, nsIDOMNode** aNode)
{
  *aRealFrame = NS_STATIC_CAST(nsIFrame*, aFrame);
  nsCOMPtr<nsIContent> content;
  (*aRealFrame)->GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(content);
  *aNode = node;
  NS_ADDREF(*aNode);

  nsCOMPtr<nsIDocument> document;
  content->GetDocument(*getter_AddRefs(document));

#ifdef DEBUG
  PRInt32 shells = document->GetNumberOfShells();
  NS_ASSERTION(shells > 0,"Error no shells!");
#endif

  *aShell = document->GetShellAt(0);
  NS_IF_ADDREF(*aShell);

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

