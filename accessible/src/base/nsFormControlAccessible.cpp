/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     John Gaunt (jgaunt@netscape.com)
 *     Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// NOTE: alphabetically ordered
#include "nsFormControlAccessible.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLLabelElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULCheckboxElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULLabelElement.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsReadableUtils.h"
#include "nsString.h"

/**
  * nsFormControlAccessible
  */

nsFormControlAccessible::nsFormControlAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessible(aNode, aShell)
{ 
}

/**
  * XUL states: focused, unavailable(disabled), focusable, ?protected?
  * HTML states: focused, unabailable(disabled), focusable, protected
  */
NS_IMETHODIMP nsFormControlAccessible::GetAccState(PRUint32 *_retval)
{
  // Get the focused state from the nsAccessible
  nsAccessible::GetAccState(_retval);

  PRBool disabled = PR_FALSE;
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIDOMHTMLInputElement> htmlFormElement(do_QueryInterface(mDOMNode, &rv));
  if (NS_SUCCEEDED(rv) && htmlFormElement) {
    htmlFormElement->GetDisabled(&disabled);
    nsAutoString typeString;
    htmlFormElement->GetType(typeString);
    if (typeString.EqualsIgnoreCase("password"))
      *_retval |= STATE_PROTECTED;
  }
  else {
    nsCOMPtr<nsIDOMXULControlElement> xulFormElement(do_QueryInterface(mDOMNode, &rv));  
    if (NS_SUCCEEDED(rv) && xulFormElement) {
      xulFormElement->GetDisabled(&disabled);
      /* XXX jgaunt do XUL elements support password fields? */
    }
  }
  if (disabled)
    *_retval |= STATE_UNAVAILABLE;
  else 
    *_retval |= STATE_FOCUSABLE;

  return NS_OK;
}

/**
  * Will be called by both HTML and XUL elements, this method
  *  merely checks who is calling and then calls the appropriate
  *  protected method for the XUL or HTML element.
  */
NS_IMETHODIMP nsFormControlAccessible::GetAccName(nsAWritableString& _retval)
{
  nsCOMPtr<nsIDOMXULElement> xulFormElement(do_QueryInterface(mDOMNode));
  if (xulFormElement)
    return GetXULAccName(_retval);
  else
    return GetHTMLAccName(_retval);
}

/**
  * No Children
  */
NS_IMETHODIMP nsFormControlAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsFormControlAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsFormControlAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

// ------------
// Radio button
// ------------

nsRadioButtonAccessible::nsRadioButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsFormControlAccessible(aNode, aShell)
{ 
}

/**
  *
  */
NS_IMETHODIMP nsRadioButtonAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}

/**
  *
  */
NS_IMETHODIMP nsRadioButtonAccessible::GetAccActionName(PRUint8 index, nsAWritableString& _retval)
{
  if (index == eAction_Click) {
    _retval = NS_LITERAL_STRING("select");
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/**
  *
  */
NS_IMETHODIMP nsRadioButtonAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_RADIOBUTTON;

  return NS_OK;
}

// ------------
// Text Accessibles
// ------------

nsTextAccessible::nsTextAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsLinkableAccessible(aDomNode, aShell)
{ 
}

/**
  * We are text
  */
NS_IMETHODIMP nsTextAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_TEXT;

  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsTextAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsTextAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsTextAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

