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

#include "nsHTMLFormControlAccessible.h"
#include "nsWeakReference.h"
#include "nsIFrame.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsReadableUtils.h"

nsHTMLFormControlAccessible::nsHTMLFormControlAccessible(nsIPresShell* aShell, nsIDOMNode* aNode):
nsLeafDOMAccessible(aShell, aNode)
{ 
}

/* wstring getAccName (); */
NS_IMETHODIMP nsHTMLFormControlAccessible::GetAccName(PRUnichar **_retval)
{
  // go up tree get name of label if there is one.
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccState (); */
NS_IMETHODIMP nsHTMLFormControlAccessible::GetAccState(PRUint32 *_retval)
{
    // can be
    // focusable, focused, checked
    nsCOMPtr<nsIDOMHTMLInputElement> element = do_QueryInterface(mNode);

    PRBool checked = PR_FALSE;
    element->GetChecked(&checked);
    *_retval = (checked ? STATE_CHECKED : 0);
    return NS_OK;
}

// --- checkbox -----

nsHTMLCheckboxAccessible::nsHTMLCheckboxAccessible(nsIPresShell* aShell, nsIDOMNode* aNode):
nsHTMLFormControlAccessible(aShell, aNode)
{ 
}

/* wstring getAccRole (); */
NS_IMETHODIMP nsHTMLCheckboxAccessible::GetAccRole(PRUnichar **_retval)
{
    *_retval = ToNewUnicode(NS_LITERAL_STRING("check box"));
    return NS_OK;
}

/* wstring getAccDefaultAction (); */
NS_IMETHODIMP nsHTMLCheckboxAccessible::GetAccDefaultAction(PRUnichar **_retval)
{
    // check or uncheck
    nsCOMPtr<nsIDOMHTMLInputElement> element = do_QueryInterface(mNode);

    PRBool checked = PR_FALSE;
    element->GetChecked(&checked);
    *_retval = ToNewUnicode(checked ? NS_LITERAL_STRING("Check") : NS_LITERAL_STRING("UnCheck"));

    return NS_OK;
}

/* void accDoDefaultAction (); */
NS_IMETHODIMP nsHTMLCheckboxAccessible::AccDoDefaultAction()
{
  nsCOMPtr<nsIDOMHTMLInputElement> element = do_QueryInterface(mNode);
  PRBool checked = PR_FALSE;
  element->GetChecked(&checked);
  element->SetChecked(checked ? PR_FALSE : PR_TRUE);

  return NS_OK;
}


//------ Radio button -------

nsHTMLRadioButtonAccessible::nsHTMLRadioButtonAccessible(nsIPresShell* aShell, nsIDOMNode* aNode):
nsHTMLFormControlAccessible(aShell, aNode)
{ 
}

/* wstring getAccDefaultAction (); */
NS_IMETHODIMP nsHTMLRadioButtonAccessible::GetAccDefaultAction(PRUnichar **_retval)
{
    *_retval = ToNewUnicode(NS_LITERAL_STRING("Select"));
    return NS_OK;
}

/* wstring getAccRole (); */
NS_IMETHODIMP nsHTMLRadioButtonAccessible::GetAccRole(PRUnichar **_retval)
{
    *_retval = ToNewUnicode(NS_LITERAL_STRING("radio button"));

    return NS_OK;
}

NS_IMETHODIMP nsHTMLRadioButtonAccessible::AccDoDefaultAction()
{
  nsCOMPtr<nsIDOMHTMLInputElement> element = do_QueryInterface(mNode);
  element->Click();

  return NS_OK;
}

// ----- Button -----

nsHTMLButtonAccessible::nsHTMLButtonAccessible(nsIPresShell* aShell, nsIDOMNode* aNode):
nsHTMLFormControlAccessible(aShell, aNode)
{ 
}

/* wstring getAccDefaultAction (); */
NS_IMETHODIMP nsHTMLButtonAccessible::GetAccDefaultAction(PRUnichar **_retval)
{
    *_retval = ToNewUnicode(NS_LITERAL_STRING("Press"));
    return NS_OK;
}

/* wstring getAccRole (); */
NS_IMETHODIMP nsHTMLButtonAccessible::GetAccRole(PRUnichar **_retval)
{
    *_retval = ToNewUnicode(NS_LITERAL_STRING("push button"));
    return NS_OK;
}

/* wstring getAccRole (); */
NS_IMETHODIMP nsHTMLButtonAccessible::GetAccName(PRUnichar **_retval)
{
    *_retval = nsnull;
    nsCOMPtr<nsIDOMHTMLInputElement> button = do_QueryInterface(mNode);

    if (!button)
      return NS_ERROR_FAILURE;

    nsAutoString name;
    button->GetValue(name);
    name.CompressWhitespace();

    *_retval = name.ToNewUnicode();

    return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonAccessible::AccDoDefaultAction()
{
  nsCOMPtr<nsIDOMHTMLInputElement> element = do_QueryInterface(mNode);
  element->Click();

  return NS_OK;
}
