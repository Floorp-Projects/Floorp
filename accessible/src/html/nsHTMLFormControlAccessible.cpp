/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Author: Eric Vaughan (evaughan@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// NOTE: alphabetically ordered
#include "nsAccessibleTreeWalker.h"
#include "nsHTMLFormControlAccessible.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSHTMLButtonElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIFrame.h"
#include "nsISelectionController.h"

// --- checkbox -----

nsHTMLCheckboxAccessible::nsHTMLCheckboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsFormControlAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_CHECKBUTTON;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {    // 0 is the magic value for default action
    // check or uncheck
    PRUint32 state;
    GetState(&state);

    if (state & STATE_CHECKED)
      nsAccessible::GetTranslatedString(NS_LITERAL_STRING("uncheck"), _retval); 
    else
      nsAccessible::GetTranslatedString(NS_LITERAL_STRING("check"), _retval); 

    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::DoAction(PRUint8 index)
{
  if (index == 0) {   // 0 is the magic value for default action
    nsCOMPtr<nsIDOMHTMLInputElement> htmlCheckboxElement(do_QueryInterface(mDOMNode));
    if (htmlCheckboxElement) {
      htmlCheckboxElement->Click();
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetState(PRUint32 *_retval)
{
  nsFormControlAccessible::GetState(_retval);
  PRBool checked = PR_FALSE;   // Radio buttons and check boxes can be checked

  nsCOMPtr<nsIDOMHTMLInputElement> htmlCheckboxElement(do_QueryInterface(mDOMNode));
  if (htmlCheckboxElement) 
    htmlCheckboxElement->GetChecked(&checked);

  if (checked) 
    *_retval |= STATE_CHECKED;
  
  return NS_OK;
}

//------ Radio button -------

nsHTMLRadioButtonAccessible::nsHTMLRadioButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsRadioButtonAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLRadioButtonAccessible::DoAction(PRUint8 index)
{
  if (index == eAction_Click) {
    nsCOMPtr<nsIDOMHTMLInputElement> element(do_QueryInterface(mDOMNode));
    if (element) {
      element->Click();
      return NS_OK;
    }
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLRadioButtonAccessible::GetState(PRUint32 *_retval)
{
  nsFormControlAccessible::GetState(_retval);
  PRBool checked = PR_FALSE;   // Radio buttons and check boxes can be checked

  nsCOMPtr<nsIDOMHTMLInputElement> htmlRadioElement(do_QueryInterface(mDOMNode));
  if (htmlRadioElement) 
    htmlRadioElement->GetChecked(&checked);

  if (checked) 
    *_retval |= STATE_CHECKED;

  return NS_OK;
}

// ----- Button -----

nsHTMLButtonAccessible::nsHTMLButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsFormControlAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("press"), _retval); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLButtonAccessible::DoAction(PRUint8 index)
{
  if (index == eAction_Click) {
    nsCOMPtr<nsIDOMHTMLInputElement> element(do_QueryInterface(mDOMNode));
    if (element) {
      element->Click();
      return NS_OK;
    }
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetState(PRUint32 *_retval)
{
  nsFormControlAccessible::GetState(_retval);
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No nsIDOMElement for button node!");

  nsAutoString buttonType;
  element->GetAttribute(NS_LITERAL_STRING("type"), buttonType);
  if (buttonType.LowerCaseEqualsLiteral("submit"))
    *_retval |= STATE_DEFAULT;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_PUSHBUTTON;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetName(nsAString& _retval)
{
  nsCOMPtr<nsIDOMHTMLInputElement> button(do_QueryInterface(mDOMNode));

  if (!button)
    return NS_ERROR_FAILURE;

  nsAutoString name;
  button->GetValue(name);
  name.CompressWhitespace();
  if (name.IsEmpty()) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
    elt->GetAttribute(NS_LITERAL_STRING("title"), name);
  }

  _retval.Assign(name);

  return NS_OK;
}


// ----- HTML 4 Button: can contain arbitrary HTML content -----

nsHTML4ButtonAccessible::nsHTML4ButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsLeafAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("press"), _retval); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::DoAction(PRUint8 index)
{
  if (index == 0) {
    nsCOMPtr<nsIDOMNSHTMLButtonElement> buttonElement(do_QueryInterface(mDOMNode));
    if ( buttonElement )
    {
      buttonElement->Click();
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_PUSHBUTTON;
  return NS_OK;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetState(PRUint32 *_retval)
{
  nsAccessible::GetState(_retval);
  *_retval |= STATE_FOCUSABLE;

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No nsIDOMElement for button node!");

  nsAutoString buttonType;
  element->GetAttribute(NS_LITERAL_STRING("type"), buttonType);
  if (buttonType.LowerCaseEqualsLiteral("submit"))
    *_retval |= STATE_DEFAULT;

  return NS_OK;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetName(nsAString& _retval)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));

  nsAutoString name;
  if (content)
    rv = AppendFlatStringFromSubtree(content, &name);

  if (NS_SUCCEEDED(rv)) {
    // Temp var needed until CompressWhitespace built for nsAString
    name.CompressWhitespace();
    _retval.Assign(name);
  }

  return rv;
}


// --- textfield -----

nsHTMLTextFieldAccessible::nsHTMLTextFieldAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsFormControlAccessible(aNode, aShell)
{ 
}

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLTextFieldAccessible, nsFormControlAccessible)

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_TEXT;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetValue(nsAString& _retval)
{
  PRUint32 state;
  GetState(&state);
  if (state & STATE_PROTECTED)    // Don't return password text!
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea(do_QueryInterface(mDOMNode));
  if (textArea) {
    return textArea->GetValue(_retval);
  }
  
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(mDOMNode));
  if (inputElement) {
    return inputElement->GetValue(_retval);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetState(PRUint32 *_retval)
{
  // can be
  // focusable, focused, protected. readonly, unavailable, selected
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;  // Node has been Shutdown()
  }

  nsAccessible::GetState(_retval);
  *_retval |= STATE_FOCUSABLE;

  nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea(do_QueryInterface(mDOMNode));
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(mDOMNode));

  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  PRBool isReadOnly = PR_FALSE;
  elt->HasAttribute(NS_LITERAL_STRING("readonly"), &isReadOnly);
  if (isReadOnly)
    *_retval |= STATE_READONLY;

  nsIFrame *frame = GetFrame();
  if (frame) {
    nsCOMPtr<nsIPresContext> context(GetPresContext());
    NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);
    nsCOMPtr<nsISelectionController> selCon;
    frame->GetSelectionController(context,getter_AddRefs(selCon));
    if (selCon) {
      nsCOMPtr<nsISelection> domSel;
      selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
      if (domSel) {
        PRBool isCollapsed = PR_TRUE;
        domSel->GetIsCollapsed(&isCollapsed);
        if (!isCollapsed)
          *_retval |=STATE_SELECTED;
      }
    }
  }


  if (!textArea) {
    if (inputElement) {
      /////// ====== Must be a password field, so it uses nsIDOMHTMLFormControl ==== ///////
      PRUint32 moreStates = 0;
      nsresult rv = nsFormControlAccessible::GetState(&moreStates);
      *_retval |= moreStates;
      return rv;
    }
    return NS_ERROR_FAILURE;
  }

  PRBool disabled = PR_FALSE;
  textArea->GetDisabled(&disabled);
  if (disabled)
    *_retval |= STATE_UNAVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("activate"), _retval);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::DoAction(PRUint8 index)
{
  if (index == 0) {
    nsCOMPtr<nsIDOMHTMLInputElement> element(do_QueryInterface(mDOMNode));
    if ( element )
    {
      element->Focus();
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

// --- groupbox  -----

/*
 * The HTML for this is <fieldset> <legend>box-title</legend> form elements </fieldset> 
 */

nsHTMLGroupboxAccessible::nsHTMLGroupboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessibleWrap(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLGroupboxAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_GROUPING;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLGroupboxAccessible::GetState(PRUint32 *_retval)
{
  // Groupbox doesn't support any states!
  *_retval = 0;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLGroupboxAccessible::GetName(nsAString& _retval)
{
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  if (element) {
    nsCOMPtr<nsIDOMNodeList> legends;
    nsAutoString nameSpaceURI;
    element->GetNamespaceURI(nameSpaceURI);
    element->GetElementsByTagNameNS(nameSpaceURI, NS_LITERAL_STRING("legend"),
                                  getter_AddRefs(legends));
    if (legends) {
      nsCOMPtr<nsIDOMNode> legendNode;
      legends->Item(0, getter_AddRefs(legendNode));
      nsCOMPtr<nsIContent> legendContent(do_QueryInterface(legendNode));
      if (legendContent) {
        _retval.Truncate();  // Default name is blank 
        return AppendFlatStringFromSubtree(legendContent, &_retval);
      }
    }
  }
  return NS_OK;
}

void nsHTMLGroupboxAccessible::CacheChildren(PRBool aWalkAnonContent)
{
  if (!mWeakShell) {
    // This node has been shut down
    mAccChildCount = -1;
    return;
  }

  if (mAccChildCount == eChildCountUninitialized) {
    nsAccessibleTreeWalker walker(mWeakShell, mDOMNode, aWalkAnonContent);
    walker.mState.frame = GetFrame();
    mAccChildCount = 0;
    walker.GetFirstChild();
    // Check for <legend> and skip it if it's there
    if (walker.mState.accessible && walker.mState.domNode) {
      nsCOMPtr<nsIDOMNode> mightBeLegendNode;
      walker.mState.domNode->GetParentNode(getter_AddRefs(mightBeLegendNode));
      nsCOMPtr<nsIDOMHTMLLegendElement> legend(do_QueryInterface(mightBeLegendNode));
      if (legend) {
        walker.GetNextSibling();      // Skip the legend
      }
    }
    SetFirstChild(walker.mState.accessible);
    nsCOMPtr<nsPIAccessible> privatePrevAccessible;
    while (walker.mState.accessible) {
      ++mAccChildCount;
      privatePrevAccessible = do_QueryInterface(walker.mState.accessible);
      privatePrevAccessible->SetParent(this);
      walker.GetNextSibling();
      privatePrevAccessible->SetNextSibling(walker.mState.accessible);
    }
  }
}
