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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Author: Eric Vaughan (evaughan@netscape.com)
 * Contributor(s): 
 */

#include "nsGenericAccessible.h"
#include "nsIEventStateManager.h"
#include "nsIFrame.h"
#include "nsCOMPtr.h"
#include "nsIWeakReference.h"

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsGenericAccessible, nsIAccessible)

nsGenericAccessible::nsGenericAccessible()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsGenericAccessible::~nsGenericAccessible()
{
  /* destructor code */
}

/* nsIAccessible getAccParent (); */
NS_IMETHODIMP nsGenericAccessible::GetAccParent(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccNextSibling (); */
NS_IMETHODIMP nsGenericAccessible::GetAccNextSibling(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccPreviousSibling (); */
NS_IMETHODIMP nsGenericAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccFirstChild (); */
NS_IMETHODIMP nsGenericAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccLastChild (); */
NS_IMETHODIMP nsGenericAccessible::GetAccLastChild(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* long getAccChildCount (); */
NS_IMETHODIMP nsGenericAccessible::GetAccChildCount(PRInt32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccName (); */
NS_IMETHODIMP nsGenericAccessible::GetAccName(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccValue (); */
NS_IMETHODIMP nsGenericAccessible::GetAccValue(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setAccName (in wstring name); */
NS_IMETHODIMP nsGenericAccessible::SetAccName(const PRUnichar *name)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setAccValue (in wstring value); */
NS_IMETHODIMP nsGenericAccessible::SetAccValue(const PRUnichar *value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccDescription (); */
NS_IMETHODIMP nsGenericAccessible::GetAccDescription(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccRole (); */
NS_IMETHODIMP nsGenericAccessible::GetAccRole(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccState (); */
NS_IMETHODIMP nsGenericAccessible::GetAccState(PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccDefaultAction (); */
NS_IMETHODIMP nsGenericAccessible::GetAccDefaultAction(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccHelp (); */
NS_IMETHODIMP nsGenericAccessible::GetAccHelp(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean getAccFocused (); */
NS_IMETHODIMP nsGenericAccessible::GetAccFocused(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accGetAt (in long x, in long y); */
NS_IMETHODIMP nsGenericAccessible::AccGetAt(PRInt32 x, PRInt32 y, nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accNavigateRight (); */
NS_IMETHODIMP nsGenericAccessible::AccNavigateRight(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accNavigateLeft (); */
NS_IMETHODIMP nsGenericAccessible::AccNavigateLeft(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accNavigateUp (); */
NS_IMETHODIMP nsGenericAccessible::AccNavigateUp(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accNavigateDown (); */
NS_IMETHODIMP nsGenericAccessible::AccNavigateDown(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accGetBounds (out long x, out long y, out long width, out long height); */
NS_IMETHODIMP nsGenericAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accAddSelection (); */
NS_IMETHODIMP nsGenericAccessible::AccAddSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accRemoveSelection (); */
NS_IMETHODIMP nsGenericAccessible::AccRemoveSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accExtendSelection (); */
NS_IMETHODIMP nsGenericAccessible::AccExtendSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accTakeSelection (); */
NS_IMETHODIMP nsGenericAccessible::AccTakeSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accTakeFocus (); */
NS_IMETHODIMP nsGenericAccessible::AccTakeFocus()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accDoDefaultAction (); */
NS_IMETHODIMP nsGenericAccessible::AccDoDefaultAction()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long getAccExtState (); */
NS_IMETHODIMP nsGenericAccessible::GetAccExtState(PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------
// nsDOMAccessible
//-------------

nsDOMAccessible::nsDOMAccessible(nsIPresShell* aShell, nsIDOMNode* aNode)
{
  mPresShell = getter_AddRefs(NS_GetWeakReference(aShell));
  mNode = aNode;
}


/* void accRemoveSelection (); */
NS_IMETHODIMP nsDOMAccessible::AccRemoveSelection()
{
    nsCOMPtr<nsISelectionController> control = do_QueryReferent(mPresShell);
    nsCOMPtr<nsISelection> selection;
    nsresult rv = control->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIDOMNode> parent;
    rv = mNode->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(rv))
      return rv;

    rv = selection->Collapse(parent, 0);
    if (NS_FAILED(rv))
      return rv;

    return NS_OK;
}

/* void accTakeSelection (); */
NS_IMETHODIMP nsDOMAccessible::AccTakeSelection()
{
    nsCOMPtr<nsISelectionController> control = do_QueryReferent(mPresShell);
    nsCOMPtr<nsISelection> selection;
    nsresult rv = control->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIDOMNode> parent;
    rv = mNode->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(rv))
      return rv;

    PRInt32 offsetInParent = 0;
    nsCOMPtr<nsIDOMNode> child;
    rv = parent->GetFirstChild(getter_AddRefs(child));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIDOMNode> next; 

    while(child)
    {
      if (child == mNode) {
        // Collapse selection to just before desired element,
        rv = selection->Collapse(parent, offsetInParent);
        if (NS_FAILED(rv))
          return rv;

        // then extend it to just after
        rv = selection->Extend(parent, offsetInParent+1);
        return rv;
      }

       child->GetNextSibling(getter_AddRefs(next));
       child = next;
       offsetInParent++;
    }

    // didn't find a child
    return NS_ERROR_FAILURE;
}

/* void accTakeFocus (); */
NS_IMETHODIMP nsDOMAccessible::AccTakeFocus()
{ 
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShell);
  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsIContent> content = do_QueryInterface(mNode);
  content->SetFocus(context);
  
  return NS_OK;
}

//-------------
// nsLeafFrameAccessible
//-------------

nsLeafDOMAccessible::nsLeafDOMAccessible(nsIPresShell* aShell, nsIDOMNode* aNode):
nsDOMAccessible(aShell, aNode)
{
}

/* nsIAccessible getAccFirstChild (); */
NS_IMETHODIMP nsLeafDOMAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* nsIAccessible getAccLastChild (); */
NS_IMETHODIMP nsLeafDOMAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* long getAccChildCount (); */
NS_IMETHODIMP nsLeafDOMAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

