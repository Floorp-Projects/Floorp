/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation..
 * Portions created by Netscape Communications Corporation. are
 * Copyright (C) 1998 Netscape Communications Corporation.. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Original Author: Eric Vaughan (evaughan@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsHTMLListboxAccessible.h"

#include "nsCOMPtr.h"
#include "nsIAccessibilityService.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIFrame.h"
#include "nsIServiceManager.h"
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
NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLListboxAccessible, nsAccessible, nsIAccessibleSelectable)

/** 
  * Tell our caller we are a list ( there isn't a listbox value )
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_WINDOW;
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
  * retval will be nsnull if:
  *    - there are no options in the select
  *    - there are options but none are selected
  *    - the DOMNode is not a nsIDOMHTMLSelectElement ( shouldn't happen )
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetSelectedChildren(nsISupportsArray **_retval)
{
  nsCOMPtr<nsIDOMHTMLSelectElement> select(do_QueryInterface(mDOMNode));
  if(select) {
    nsCOMPtr<nsIDOMHTMLCollection> options;
    // get all the options in the select
    select->GetOptions(getter_AddRefs(options));
    if (options) {
      // set up variables we need to get the selected options and to get their nsIAccessile objects
      PRUint32 length;
      options->GetLength(&length);
      nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
      nsCOMPtr<nsISupportsArray> selectedAccessibles;
      NS_NewISupportsArray(getter_AddRefs(selectedAccessibles));
      if (!selectedAccessibles || !accService)
        return NS_ERROR_FAILURE;
      // find the selected options and get the accessible objects;
      PRBool isSelected = PR_FALSE;
      nsCOMPtr<nsIPresContext> context;
      GetPresContext(context);
      for (PRUint32 i = 0 ; i < length ; i++) {
        nsCOMPtr<nsIDOMNode> tempNode;
        options->Item(i,getter_AddRefs(tempNode));
        if (tempNode) {
          nsCOMPtr<nsIDOMHTMLOptionElement> tempOption(do_QueryInterface(tempNode));
          if (tempOption)
            tempOption->GetSelected(&isSelected);
          if (isSelected) {
            nsCOMPtr<nsIAccessible> tempAccess;
            accService->CreateHTMLSelectOptionAccessible(tempOption, this, context, getter_AddRefs(tempAccess));
            if ( tempAccess )
              selectedAccessibles->AppendElement(tempAccess);
            isSelected = PR_FALSE;
          }
        }
      }
      selectedAccessibles->Count(&length); // reusing length
      if ( length != 0 ) { // length of nsISupportsArray containing selected options
        *_retval = selectedAccessibles;
        NS_IF_ADDREF(*_retval);
        return NS_OK;
      }
    }
  }
  // no options, not a select or none of the options are selected
  *_retval = nsnull;
  return NS_OK;
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



