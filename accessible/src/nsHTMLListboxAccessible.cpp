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
 * Original Author: Eric Vaughan (evaughan@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsHTMLListboxAccessible.h"

#include "nsCOMPtr.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIFrame.h"
#include "nsLayoutAtoms.h"

/** ----- nsHTMLListboxAccessible ----- */

/**
  * Constructor -- create the nsHTMLAccessible
  */
nsHTMLListboxAccessible::nsHTMLListboxAccessible(nsIDOMNode* aDOMNode, 
                                                   nsIWeakReference* aShell)
                        :nsAccessible(aDOMNode, aShell)
{
}

/** Inherit the ISupports impl from nsAccessible */
NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLListboxAccessible, nsAccessible)

/** 
  * Tell our caller we are a list ( there isn't a listbox value )
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_LIST;
  return NS_OK;
}

/** 
  * Through the arg, pass back our last child, a nsHTMLListboxWindowAccessible object
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectListAccessible(this, mDOMNode, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/** 
  * Through the arg, pass back our first child, a nsHTMLListboxWindowAccessible object
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectListAccessible(this, mDOMNode, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/**
  * We always have 1 child: SelectList.
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

/**
  * nsIAccessibleSelectable method. This needs to get from the select the list
  *     of select options and then iterate through that pulling out the selected
  *     items and creating IAccessible objects for them. Put the IAccessibles in
  *     the nsISupportsArray and return them.
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetSelectedChildren(nsISupportsArray **_retval)
{
  *_retval = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}

/**
  * Our value is the value of our ( first ) selected child. SelectElement
  *     returns this by default with GetValue().
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccValue(nsAWritableString& _retval)
{
  nsCOMPtr<nsIDOMHTMLSelectElement> select (do_QueryInterface(mDOMNode));
  if ( select ) {
    select->GetValue(_retval);  
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/**
  * As a nsHTMLListboxAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_READONLY
  *     STATE_FOCUSABLE
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccState(PRUint32 *_retval)
{
  // this sets either STATE_FOCUSED or 0
  nsAccessible::GetAccState(_retval);

  *_retval |= STATE_READONLY | STATE_FOCUSABLE;

  return NS_OK;
}



