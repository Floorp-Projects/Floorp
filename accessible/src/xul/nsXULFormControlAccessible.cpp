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
 * Author: John Gaunt (jgaunt@netscape.com)
 *
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
#include "nsIDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULCheckboxElement.h"
#include "nsIDOMXULDocument.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsXULFormControlAccessible.h"

// ----- XUL Form Control accessible --------

nsXULFormControlAccessible::nsXULFormControlAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsLeafAccessible(aNode, aShell)
{ 
}

/**
  * Checks the label's value first then makes a call to get the 
  *  text from the children if the value is not set.
  */
NS_IMETHODIMP nsXULFormControlAccessible::AppendLabelText(nsIDOMNode *aLabelNode, nsAWritableString& _retval)
{
  NS_ASSERTION(aLabelNode, "Label Node passed in is null");
  nsCOMPtr<nsIDOMXULLabelElement> labelNode(do_QueryInterface(aLabelNode));
  // label's value="foo" is set
  if ( labelNode && NS_SUCCEEDED(labelNode->GetValue(_retval))) {
    if (_retval.IsEmpty()) {
      // label contains children who define it's text -- possibly HTML
      nsCOMPtr<nsIContent> content(do_QueryInterface(labelNode));
      if (content)
        return AppendFlatStringFromSubtree(content, &_retval);
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/**
  * 3 main cases for Xul Controls to be labeled
  *   1 - control contains label="foo"
  *   2 - control has, as a child, a label element
  *        - label has either value="foo" or children
  *   3 - non-child label contains control="controlID"
  *        - label has either value="foo" or children
  * Once a label is found, the search is discontinued, so a control
  *  that has a label child as well as having a label external to
  *  the control that uses the control="controlID" syntax will use
  *  the child label for its Name.
  */
/* wstring getAccName (); */
NS_IMETHODIMP nsXULFormControlAccessible::GetAccName(nsAWritableString& _retval)
{
  nsresult rv;
  nsAutoString label;

  // CASE #1 -- great majority of the cases
  nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(mDOMNode));
  NS_ASSERTION(domElement, "No domElement for accessible DOM node!");
  rv = domElement->GetAttribute(NS_LITERAL_STRING("label"), label) ;

  if (NS_FAILED(rv) || label.IsEmpty() ) {

    // CASE #2 ------ label as a child
    nsCOMPtr<nsIDOMNodeList>labelChildren;
    NS_ASSERTION(domElement, "No domElement for accessible DOM node!");
    if (NS_SUCCEEDED(rv = domElement->GetElementsByTagName(NS_LITERAL_STRING("label"), getter_AddRefs(labelChildren)))) {
      PRUint32 length = 0;
      if (NS_SUCCEEDED(rv = labelChildren->GetLength(&length)) && length > 0) {
        for (PRUint32 i = 0; i < length; ++i) {
          nsCOMPtr<nsIDOMNode> child;
          if (NS_SUCCEEDED(rv = labelChildren->Item(i, getter_AddRefs(child) ))) {
            rv = AppendLabelText(child, label);
          }
        }
      }
    }
    
    if (NS_FAILED(rv) || label.IsEmpty()) {

      // CASE #3 ----- non-child label pointing to control
      //  XXX jgaunt
      //   decided to search the parent's children for labels linked to
      //   this control via the control="controlID" syntax, instead of searching
      //   the entire document with:
      //
      //      nsCOMPtr<nsIDocument> doc;
      //      nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
      //      content->GetDocument(*getter_AddRefs(doc));
      //      nsCOMPtr<nsIDOMXULDocument> xulDoc(do_QueryInterface(doc));
      //      if (xulDoc) {
      //        nsCOMPtr<nsIDOMNodeList>labelList;
      //        if (NS_SUCCEEDED(rv = xulDoc->GetElementsByAttribute(NS_LITERAL_STRING("control"), controlID, getter_AddRefs(labelList))))
      //
      //   This should keep search times down and still get the relevant
      //   labels.
      nsCOMPtr<nsIDOMNode> parent;
      if (NS_SUCCEEDED(rv = mDOMNode->GetParentNode(getter_AddRefs(parent)))) {
        nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(parent));
        NS_ASSERTION(xulElement, "No xulElement for parent DOM Node!");
        if (xulElement) {
          nsAutoString controlID;
          nsCOMPtr<nsIDOMNodeList>labelList;
          if (NS_SUCCEEDED(rv = xulElement->GetElementsByAttribute(NS_LITERAL_STRING("control"), controlID, getter_AddRefs(labelList))))
          {
            PRUint32 length = 0;
            if (NS_SUCCEEDED(rv = labelList->GetLength(&length)) && length > 0) {
              for (PRUint32 i = 0; i < length; ++i) {
                nsCOMPtr<nsIDOMNode> child;
                if (NS_SUCCEEDED(rv = labelList->Item(i, getter_AddRefs(child) ))) {
                  rv = AppendLabelText(child, label);
                }
              }
            }
          }
        }
      }  // End of CASE #3
    }  // END of CASE #2
  }  // END of CASE #1

  label.CompressWhitespace();
  if (label.IsEmpty())
    return nsAccessible::GetAccName(_retval);
  
  _retval.Assign(label);
  return NS_OK;
}

/**
 * XUL Form controls can be focusable, focused, unavailable, ?protected?
 **/
NS_IMETHODIMP nsXULFormControlAccessible::GetAccState(PRUint32 *_retval)
{
  // Get the focused state from the nsAccessible
  nsAccessible::GetAccState(_retval);

  PRBool disabled = PR_FALSE;
  nsCOMPtr<nsIDOMXULControlElement> xulFormElement(do_QueryInterface(mDOMNode));
  if (xulFormElement)
    xulFormElement->GetDisabled(&disabled);

  if (disabled)
    *_retval |= STATE_UNAVAILABLE;
  else 
    *_retval |= STATE_FOCUSABLE;

  return NS_OK;
}

// ----- XUL Button: can contain arbitrary HTML content -----

nsXULButtonAccessible::nsXULButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsBlockAccessible(aNode, aShell)
{ 
}

/* PRUint8 getAccNumActions (); */
NS_IMETHODIMP nsXULButtonAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;;
}

/* wstring getAccActionName (in PRUint8 index); */
NS_IMETHODIMP nsXULButtonAccessible::GetAccActionName(PRUint8 index, nsAWritableString& _retval)
{
  if (index == 0) {
    _retval = NS_LITERAL_STRING("press");
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/* void accDoAction (in PRUint8 index); */
NS_IMETHODIMP nsXULButtonAccessible::AccDoAction(PRUint8 index)
{
  if (index == 0) {
    nsCOMPtr<nsIDOMXULButtonElement> buttonElement(do_QueryInterface(mDOMNode));
    if ( buttonElement )
    {
      buttonElement->DoCommand();
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

/* unsigned long getAccRole (); */
NS_IMETHODIMP nsXULButtonAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_PUSHBUTTON;
  return NS_OK;
}

/* long getAccState (); */
NS_IMETHODIMP nsXULButtonAccessible::GetAccState(PRUint32 *_retval)
{
  nsAccessible::GetAccState(_retval);
  *_retval |= STATE_FOCUSABLE;
  return NS_OK;
}


/* wstring getAccName (); */
NS_IMETHODIMP nsXULButtonAccessible::GetAccName(nsAWritableString& _retval)
{
  nsCOMPtr<nsIDOMXULButtonElement> buttonElement(do_QueryInterface(mDOMNode));
  if ( buttonElement ) {
    return buttonElement->GetLabel(_retval);
  }
  return NS_ERROR_FAILURE;
}

// --- XUL checkbox -----

nsXULCheckboxAccessible::nsXULCheckboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsXULFormControlAccessible(aNode, aShell)
{ 
}

/* unsigned long getAccRole (); */
NS_IMETHODIMP nsXULCheckboxAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_CHECKBUTTON;
  return NS_OK;
}

/* PRUint8 getAccNumActions (); */
NS_IMETHODIMP nsXULCheckboxAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

/* wstring getAccActionName (in PRUint8 index); */
NS_IMETHODIMP nsXULCheckboxAccessible::GetAccActionName(PRUint8 index, nsAWritableString& _retval)
{
  if (index == 0) {    // 0 is the magic value for default action
    // check or uncheck
    PRUint32 state;
    GetAccState(&state);

    if (state & STATE_CHECKED)
      _retval = NS_LITERAL_STRING("uncheck");
    else
      _retval = NS_LITERAL_STRING("check");

    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/* void accDoAction (in PRUint8 index); */
NS_IMETHODIMP nsXULCheckboxAccessible::AccDoAction(PRUint8 index)
{
  if (index == 0) {   // 0 is the magic value for default action
    PRBool checked = PR_FALSE;
    nsCOMPtr<nsIDOMXULCheckboxElement> xulCheckboxElement(do_QueryInterface(mDOMNode));
    if (xulCheckboxElement) {
      xulCheckboxElement->GetChecked(&checked);
      xulCheckboxElement->SetChecked(!checked);
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsXULCheckboxAccessible::GetAccState(PRUint32 *_retval)
{
  // Get focus state from parent
  nsXULFormControlAccessible::GetAccState(_retval);

  // Determine Checked state
  PRBool checked = PR_FALSE;

  nsCOMPtr<nsIDOMXULCheckboxElement> xulCheckboxElement(do_QueryInterface(mDOMNode));
  if (xulCheckboxElement)
      xulCheckboxElement->GetChecked(&checked);

  if (checked) 
    *_retval |= STATE_CHECKED;
  
  return NS_OK;
}

