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
 * Original Author: John Gaunt (jgaunt@netscape.com)
 * Contributor(s):
 *   Aaron Leventhal (aaronl@netscape.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
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
#include "nsIDOMXULLabelElement.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsXULFormControlAccessible.h"
#include "nsIAccessibilityService.h"
#include "nsIServiceManager.h"

/**
  * XUL Button: can contain arbitrary HTML content
  */

/**
  * Default Constructor
  */

// Don't inherit from nsFormControlAccessible - it doesn't allow children and a button can have a dropmarker child
nsXULButtonAccessible::nsXULButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsXULButtonAccessible::GetAccName(nsAString& aResult)
{
  return GetXULAccName(aResult);
}

/**
  * Only one actions available
  */
NS_IMETHODIMP nsXULButtonAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;;
}

/**
  * Return the name of our only action
  */
NS_IMETHODIMP nsXULButtonAccessible::GetAccActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("press"), _retval); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/**
  * Tell the button to do it's action
  */
NS_IMETHODIMP nsXULButtonAccessible::AccDoAction(PRUint8 index)
{
  if (index == 0) {
    nsCOMPtr<nsIDOMXULButtonElement> buttonElement(do_QueryInterface(mDOMNode));
    if ( buttonElement )
    {
      buttonElement->Click();
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

/**
  * We are a pushbutton
  */
NS_IMETHODIMP nsXULButtonAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_PUSHBUTTON;
  return NS_OK;
}

/**
  * Possible states: focused, focusable, unavailable(disabled)
  */
NS_IMETHODIMP nsXULButtonAccessible::GetAccState(PRUint32 *_retval)
{
  // get focus and disable status from base class
  nsAccessible::GetAccState(_retval);

  PRBool disabled = PR_FALSE;
  nsCOMPtr<nsIDOMXULControlElement> xulFormElement(do_QueryInterface(mDOMNode));  
  if (xulFormElement) {
    xulFormElement->GetDisabled(&disabled);
    if (disabled)
      *_retval |= STATE_UNAVAILABLE;
    else 
      *_retval |= STATE_FOCUSABLE;
  }

  // Buttons can be checked -- they simply appear pressed in rather than checked
  nsCOMPtr<nsIDOMXULButtonElement> xulButtonElement(do_QueryInterface(mDOMNode));
  if (xulButtonElement) {
    PRBool checked = PR_FALSE;
    PRInt32 checkState = 0;
    xulButtonElement->GetChecked(&checked);
    if (checked) {
      *_retval |= STATE_PRESSED;
      xulButtonElement->GetCheckState(&checkState);
      if (checkState == nsIDOMXULButtonElement::CHECKSTATE_MIXED)  
        *_retval |= STATE_MIXED;
    }
  }

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No nsIDOMElement for button node!");
  PRBool isDefault = PR_FALSE;
  element->HasAttribute(NS_LITERAL_STRING("default"), &isDefault) ;
  if (isDefault)
    *_retval |= STATE_DEFAULT;

  return NS_OK;
}

/**
  * Perhaps 1 child - if there's a <dropmarker>
  */
NS_IMETHODIMP nsXULButtonAccessible::GetAccFirstChild(nsIAccessible **aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsIAccessible> testAccessible;
  nsAccessible::GetAccLastChild(getter_AddRefs(testAccessible));

  // If the anonymous tree walker can find accessible children, and the last one is a push button, 
  // then use it as the only accessible child -- because this is the scenario where we have a dropmarker child

  if (testAccessible) {    
    PRUint32 role;
    if (NS_SUCCEEDED(testAccessible->GetAccRole(&role)) && role == ROLE_PUSHBUTTON) {
      *aResult = testAccessible;
      NS_ADDREF(*aResult);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULButtonAccessible::GetAccLastChild(nsIAccessible **aResult)
{
  return GetAccFirstChild(aResult);
}

NS_IMETHODIMP nsXULButtonAccessible::GetAccChildCount(PRInt32 *aResult)
{
  *aResult = 0;

  nsCOMPtr<nsIAccessible> accessible;
  GetAccFirstChild(getter_AddRefs(accessible));
  if (accessible)
    *aResult = 1;

  return NS_OK;
}

/**
  * XUL Dropmarker: can contain arbitrary HTML content
  */

/**
  * Default Constructor
  */
nsXULDropmarkerAccessible::nsXULDropmarkerAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsFormControlAccessible(aNode, aShell)
{ 
}

/**
  * Only one actions available
  */
NS_IMETHODIMP nsXULDropmarkerAccessible::GetAccNumActions(PRUint8 *aResult)
{
  *aResult = eSingle_Action;
  return NS_OK;;
}

PRBool nsXULDropmarkerAccessible::DropmarkerOpen(PRBool aToggleOpen)
{
  PRBool isOpen = PR_FALSE;

  nsCOMPtr<nsIDOMNode> parentButtonNode;
  mDOMNode->GetParentNode(getter_AddRefs(parentButtonNode));
  nsCOMPtr<nsIDOMXULButtonElement> parentButtonElement(do_QueryInterface(parentButtonNode));

  if (parentButtonElement) {
    parentButtonElement->GetOpen(&isOpen);
    if (aToggleOpen)
      parentButtonElement->SetOpen(!isOpen);
  }
  else {
    nsCOMPtr<nsIDOMXULMenuListElement> parentMenuListElement(do_QueryInterface(parentButtonNode));
    if (parentMenuListElement) {
      parentMenuListElement->GetOpen(&isOpen);
      if (aToggleOpen)
        parentMenuListElement->SetOpen(!isOpen);
    }
  }

  return isOpen;
}

/**
  * Return the name of our only action
  */
NS_IMETHODIMP nsXULDropmarkerAccessible::GetAccActionName(PRUint8 index, nsAString& aResult)
{
  if (index == eAction_Click) {
    if (DropmarkerOpen(PR_FALSE))
      aResult = NS_LITERAL_STRING("close");
    else
      aResult = NS_LITERAL_STRING("open");
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

/**
  * Tell the Dropmarker to do it's action
  */
NS_IMETHODIMP nsXULDropmarkerAccessible::AccDoAction(PRUint8 index)
{
  if (index == eAction_Click) {
    DropmarkerOpen(PR_TRUE); // Reverse the open attribute
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/**
  * We are a pushbutton
  */
NS_IMETHODIMP nsXULDropmarkerAccessible::GetAccRole(PRUint32 *aResult)
{
  *aResult = ROLE_PUSHBUTTON;
  return NS_OK;
}

NS_IMETHODIMP nsXULDropmarkerAccessible::GetAccState(PRUint32 *aResult)
{
  *aResult = 0;
  
  if (DropmarkerOpen(PR_FALSE))
    *aResult = STATE_PRESSED;

  return NS_OK;
}

/**
  * XUL checkbox
  */

/**
  * Default Constructor
  */
nsXULCheckboxAccessible::nsXULCheckboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsFormControlAccessible(aNode, aShell)
{ 
}

/**
  * We are a CheckButton
  */
NS_IMETHODIMP nsXULCheckboxAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_CHECKBUTTON;
  return NS_OK;
}

/**
  * Only one action available
  */
NS_IMETHODIMP nsXULCheckboxAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}

/**
  * Return the name of our only action
  */
NS_IMETHODIMP nsXULCheckboxAccessible::GetAccActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {
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

/**
  * Tell the checkbox to do its only action -- check( or uncheck) itself
  */
NS_IMETHODIMP nsXULCheckboxAccessible::AccDoAction(PRUint8 index)
{
  if (index == eAction_Click) {
    PRBool checked = PR_FALSE;
    nsCOMPtr<nsIDOMXULCheckboxElement> xulCheckboxElement(do_QueryInterface(mDOMNode));
    if (xulCheckboxElement) {
      xulCheckboxElement->Click();
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

/**
  * Possible states: focused, focusable, unavailable(disabled), checked
  */
NS_IMETHODIMP nsXULCheckboxAccessible::GetAccState(PRUint32 *_retval)
{
  // Get focus and disable status from base class
  nsFormControlAccessible::GetAccState(_retval);

  // Determine Checked state
  nsCOMPtr<nsIDOMXULCheckboxElement> xulCheckboxElement(do_QueryInterface(mDOMNode));
  if (xulCheckboxElement) {
    PRBool checked = PR_FALSE;
    xulCheckboxElement->GetChecked(&checked);
    if (checked) {
      *_retval |= STATE_CHECKED;
      PRInt32 checkState = 0;
      xulCheckboxElement->GetCheckState(&checkState);
      if (checkState == nsIDOMXULCheckboxElement::CHECKSTATE_MIXED)   
        *_retval |= STATE_MIXED;
    }
  }
  
  return NS_OK;
}

/**
  * XUL groupbox
  */

nsXULGroupboxAccessible::nsXULGroupboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsXULGroupboxAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_GROUPING;
  return NS_OK;
}

NS_IMETHODIMP nsXULGroupboxAccessible::GetAccState(PRUint32 *_retval)
{
  // Groupbox doesn't support any states!
  *_retval = 0;

  return NS_OK;
}

NS_IMETHODIMP nsXULGroupboxAccessible::GetAccName(nsAString& _retval)
{
  _retval.Assign(NS_LITERAL_STRING(""));  // Default name is blank 

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  if (element) {
    nsCOMPtr<nsIDOMNodeList> captions;
    element->GetElementsByTagName(NS_LITERAL_STRING("caption"), getter_AddRefs(captions));
    if (captions) {
      nsCOMPtr<nsIDOMNode> captionNode;
      captions->Item(0, getter_AddRefs(captionNode));
      if (captionNode) {
        element = do_QueryInterface(captionNode);
        NS_ASSERTION(element, "No nsIDOMElement for caption node!");
        element->GetAttribute(NS_LITERAL_STRING("label"), _retval) ;
      }
    }
  }
  return NS_OK;
}

/**
  * progressmeter
  */
NS_IMPL_ISUPPORTS_INHERITED1(nsXULProgressMeterAccessible, nsFormControlAccessible, nsIAccessibleValue)

nsXULProgressMeterAccessible::nsXULProgressMeterAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsFormControlAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsXULProgressMeterAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_PROGRESSBAR;
  return NS_OK;
}

/**
  * No states supported for progressmeter
  */
NS_IMETHODIMP nsXULProgressMeterAccessible::GetAccState(PRUint32 *_retval)
{
  *_retval =0;
  return NS_OK;
}

NS_IMETHODIMP nsXULProgressMeterAccessible::GetAccValue(nsAString& _retval)
{
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No element for DOM node!");
  element->GetAttribute(NS_LITERAL_STRING("value"), _retval);
  if (!_retval.IsEmpty() && _retval.Last() != '%')
    _retval.Append(NS_LITERAL_STRING("%"));
  return NS_OK;
}

/* readonly attribute double maximumValue; */
NS_IMETHODIMP nsXULProgressMeterAccessible::GetMaximumValue(double *aMaximumValue)
{
  *aMaximumValue = 1; // 100% = 1;
  return NS_OK;
}

/* readonly attribute double minimumValue; */
NS_IMETHODIMP nsXULProgressMeterAccessible::GetMinimumValue(double *aMinimumValue)
{
  *aMinimumValue = 0;
  return NS_OK;
}

/* readonly attribute double currentValue; */
NS_IMETHODIMP nsXULProgressMeterAccessible::GetCurrentValue(double *aCurrentValue)
{
  nsAutoString currentValue;
  GetAccValue(currentValue);
  PRInt32 error;
  *aCurrentValue = currentValue.ToFloat(&error) / 100;
  return NS_OK;
}

/* boolean setCurrentValue (in double value); */
NS_IMETHODIMP nsXULProgressMeterAccessible::SetCurrentValue(double aValue, PRBool *_retval)
{
  //Here I do not suppose the min/max are 0/1.00 because I want
  // these part of code to be more extensible.
  *_retval = PR_FALSE;
  double min, max;
  GetMinimumValue(&min);
  GetMaximumValue(&max);
  if (aValue > max || aValue < min)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No element for DOM node!");
  PRUint32 value = PRUint32(aValue * 100.0 + 0.5);
  nsAutoString valueString;
  valueString.AppendInt(value);
  valueString.Append(NS_LITERAL_STRING("%"));
  if (NS_SUCCEEDED(element->SetAttribute(NS_LITERAL_STRING("value"), valueString))) {
    *_retval = PR_TRUE;
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/**
  * XUL Radio Button
  */

/** Constructor */
nsXULRadioButtonAccessible::nsXULRadioButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsRadioButtonAccessible(aNode, aShell)
{ 
}

/** Our only action is to click */
NS_IMETHODIMP nsXULRadioButtonAccessible::AccDoAction(PRUint8 index)
{
  if (index == eAction_Click) {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> radioButton(do_QueryInterface(mDOMNode));
    if (radioButton) {
      radioButton->Click();
      return NS_OK;
    }
  }
  return NS_ERROR_INVALID_ARG;
}

/** We are Focusable and can be Checked and focused */
NS_IMETHODIMP nsXULRadioButtonAccessible::GetAccState(PRUint32 *_retval)
{
  nsFormControlAccessible::GetAccState(_retval);
  PRBool selected = PR_FALSE;   // Radio buttons can be selected

  nsCOMPtr<nsIDOMXULSelectControlItemElement> radioButton(do_QueryInterface(mDOMNode));
  if (radioButton) 
    radioButton->GetSelected(&selected);

  if (selected) {
    *_retval |= STATE_CHECKED;
    // If our parent radio group is focused, then consider this radio button focused
    nsCOMPtr<nsIDOMNode> parentNode;
    mDOMNode->GetParentNode(getter_AddRefs(parentNode));
    if (parentNode) {
      nsCOMPtr<nsIDOMNode> focusedNode;
      GetFocusedNode(getter_AddRefs(focusedNode));
      if (focusedNode == parentNode)
        *_retval |= STATE_FOCUSED;
    }
  }

  return NS_OK;
}

/**
  * This gets the parent of the RadioGroup (our grandparent) and sets it 
  *  as our parent, for future calls. 
  */
NS_IMETHODIMP nsXULRadioButtonAccessible::GetAccParent(nsIAccessible **  aAccParent)
{
  if (! mParent) {
    nsCOMPtr<nsIAccessible> tempParent;
    nsAccessible::GetAccParent(getter_AddRefs(tempParent));
    if (tempParent)
      tempParent->GetAccParent(getter_AddRefs(mParent));
  }
  NS_ASSERTION(mParent,"Whoa! This RadioButtonAcc doesn't have a parent! Better find out why.");
  *aAccParent = mParent;
  NS_ADDREF(*aAccParent);
  return NS_OK;
}

/**
  * XUL Radio Group
  *   The Radio Group proxies for the Radio Buttons themselves. The Group gets
  *   focus whereas the Buttons do not. So we only have an accessible object for
  *   this for the purpose of getting the proper RadioButton. Need this here to 
  *   avoid circular reference problems when navigating the accessible tree and
  *   for getting to the radiobuttons.
  */

/** Constructor */
nsXULRadioGroupAccessible::nsXULRadioGroupAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsXULRadioGroupAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_GROUPING;
  return NS_OK;
}

NS_IMETHODIMP nsXULRadioGroupAccessible::GetAccState(PRUint32 *_retval)
{
  // The radio group is not focusable.
  // Sometimes the focus controller will report that it is focused.
  // That means that the actual selected radio button should be considered focused
  nsAccessible::GetAccState(_retval);
  *_retval &= ~(STATE_FOCUSABLE | STATE_FOCUSED);
  return NS_OK;
}
/**
  * XUL StatusBar: can contain arbitrary HTML content
  */

/**
  * Default Constructor
  */
nsXULStatusBarAccessible::nsXULStatusBarAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessible(aNode, aShell)
{ 
}

/**
  * We are a statusbar
  */
NS_IMETHODIMP nsXULStatusBarAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_STATUSBAR;
  return NS_OK;
}

NS_IMETHODIMP nsXULStatusBarAccessible::GetAccState(PRUint32 *_retval)
{
  *_retval = 0;  // no special state flags for status bar
  return NS_OK;
}

/**
  * XUL ToolBar
  */

nsXULToolbarAccessible::nsXULToolbarAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsXULToolbarAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_TOOLBAR;
  return NS_OK;
}

NS_IMETHODIMP nsXULToolbarAccessible::GetAccState(PRUint32 *_retval)
{
  nsAccessible::GetAccState(_retval);
  *_retval &= ~STATE_FOCUSABLE;  // toolbar is not focusable
  return NS_OK;
}

/**
  * XUL Toolbar Separator
  */

nsXULToolbarSeparatorAccessible::nsXULToolbarSeparatorAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsLeafAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsXULToolbarSeparatorAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_SEPARATOR;
  return NS_OK;
}

NS_IMETHODIMP nsXULToolbarSeparatorAccessible::GetAccState(PRUint32 *_retval)
{
  *_retval = 0;  // no special state flags for toolbar separator
  return NS_OK;
}
