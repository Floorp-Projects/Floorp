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
#include "nsHTMLFormControlAccessible.h"
#include "nsLayoutAtoms.h"
#include "nsSelectAccessible.h"
#include "nsHTMLTextAccessible.h"
#include "nsHTMLTableAccessible.h"
#include "nsHTMLImageAccessible.h"
#include "nsHTMLAreaAccessible.h"
#include "nsHTMLLinkAccessible.h"

// IFrame
#include "nsIDocShell.h"
#include "nsHTMLIFrameRootAccessible.h"

//--------------------


nsAccessibilityService::nsAccessibilityService()
{
  NS_INIT_REFCNT();
  //printf("################################## nsAccessibilityService\n");
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

  nsCOMPtr<nsIPresContext> c(do_QueryInterface(aPresContext));
  NS_ASSERTION(c,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> s;
  c->GetShell(getter_AddRefs(s));

  NS_ASSERTION(s,"Error not presshell!!");

  nsCOMPtr<nsIWeakReference> wr (getter_AddRefs(NS_GetWeakReference(s)));

  //printf("################################## CreateRootAccessible\n");
  *_retval = new nsRootAccessible(wr);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLSelectAccessible(nsIAtom* aPopupAtom, nsIDOMNode* node, nsISupports* aPresContext, nsIAccessible **_retval)
{
  /*
  nsCOMPtr<nsIContent> n(do_QueryInterface(node));
  NS_ASSERTION(n,"Error non nsIContent passed to accessible factory!!!");

  nsCOMPtr<nsIPresContext> c(do_QueryInterface(aPresContext));
  NS_ASSERTION(c,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> s;
  c->GetShell(getter_AddRefs(s)); 

  nsCOMPtr<nsIWeakReference> wr = getter_AddRefs(NS_GetWeakReference(s));

  *_retval = new nsSelectAccessible(aPopupAtom, nsnull, node, wr);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
  */

  return NS_ERROR_FAILURE;
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

/* nsIAccessible createHTMRadioButtonAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLRadioButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(shell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLRadioButtonAccessible(shell,node);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsIAccessible createHTMLButtonAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
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

/* nsIAccessible createHTML4ButtonAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTML4ButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(shell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTML4ButtonAccessible(shell,node);
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

  //printf("################################## CreateHTMLTextAccessible\n");
  *_retval = new nsHTMLTextAccessible(shell, node);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
}


/* nsIAccessible createHTMLTableAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLTableAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(shell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTableAccessible(shell, node);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsIAccessible createHTMLTableCellAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLTableCellAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(shell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTableCellAccessible(shell, node);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsIAccessible createHTMLImageAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLImageAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(shell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIImageFrame> imageFrame(do_QueryInterface(aFrame));
  if (!imageFrame)
    return NS_ERROR_FAILURE;

  *_retval = new nsHTMLImageAccessible(shell, node, imageFrame);
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsIAccessible createHTMLAreaAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLAreaAccessible(nsISupports *aShell, nsIDOMNode *aDOMNode, nsIAccessible *aAccParent, 
                                                               nsIAccessible **_retval)
{
  nsCOMPtr<nsIPresShell> shell(do_QueryInterface(aShell));

  *_retval = new nsHTMLAreaAccessible(shell, aDOMNode, aAccParent);

  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  } else 
    return NS_ERROR_OUT_OF_MEMORY;

}

/* nsIAccessible createHTMLTextFieldAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLTextFieldAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(shell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTextFieldAccessible(shell, node);
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
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
  *aNode = node;
  NS_ADDREF(*aNode);

  nsCOMPtr<nsIDocument> document;
  content->GetDocument(*getter_AddRefs(document));
  if (!document)
    return NS_ERROR_FAILURE;
  if (!document)
     return NS_ERROR_FAILURE;

#ifdef DEBUG
  PRInt32 shells = document->GetNumberOfShells();
  NS_ASSERTION(shells > 0,"Error no shells!");
#endif

  return document->GetShellAt(0, aShell);
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLIFrameAccessible(nsIDOMNode* node, nsISupports* aPresContext, nsIAccessible **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  NS_ASSERTION(content,"Error non nsIContent passed to accessible factory!!!");

  nsCOMPtr<nsIPresContext> presContext(do_QueryInterface(aPresContext));
  NS_ASSERTION(presContext,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> presShell;
  presContext->GetShell(getter_AddRefs(presShell)); 
  NS_ASSERTION(presShell,"Error non PresShell passed to accessible factory!!!");

  nsCOMPtr<nsIWeakReference> weakRef (getter_AddRefs(NS_GetWeakReference(presShell)));

  nsCOMPtr<nsIDocument> doc;
  if (NS_SUCCEEDED(content->GetDocument(*getter_AddRefs(doc))) && doc) {
    nsCOMPtr<nsIPresShell> presShell;
    doc->GetShellAt(0, getter_AddRefs(presShell));
    if (presShell) {
      nsCOMPtr<nsISupports> supps;
      presShell->GetSubShellFor(content, getter_AddRefs(supps));
      if (supps) {
        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(supps));
        if (docShell) {
          nsCOMPtr<nsIPresShell> ps;
          docShell->GetPresShell(getter_AddRefs(ps));
          if (ps) {
            nsCOMPtr<nsIWeakReference> wr (getter_AddRefs(NS_GetWeakReference(ps)));
            //printf("################################## CreateHTMLIFrameAccessible\n");

            nsCOMPtr<nsIAccessible> root = new nsHTMLIFrameRootAccessible(wr,node);
            *_retval = new nsHTMLIFrameAccessible(presShell, node, root);
            NS_ADDREF(*_retval);
            return NS_OK;
          }
        }
      }
    }
  }
  return NS_ERROR_FAILURE;
}


//////////////////////////////////////////////////////////////////////
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

