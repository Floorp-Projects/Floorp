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
 *   Aaron Leventhal (aaronl@netscape.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
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

#include "nsXULFormControlAccessible.h"

#include "nsAccessibilityAtoms.h"
#include "nsAccUtils.h"
#include "nsAccTreeWalker.h"
#include "nsCoreUtils.h"
#include "nsRelUtils.h"

// NOTE: alphabetically ordered
#include "nsHTMLFormControlAccessible.h"
#include "nsXULMenuAccessible.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSEditableElement.h"
#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULCheckboxElement.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULTextboxElement.h"
#include "nsIEditor.h"
#include "nsIFrame.h"
#include "nsINameSpaceManager.h"
#include "nsITextControlFrame.h"

////////////////////////////////////////////////////////////////////////////////
// nsXULButtonAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULButtonAccessible::
  nsXULButtonAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsXULButtonAccessible: nsISupports

NS_IMPL_ISUPPORTS_INHERITED0(nsXULButtonAccessible, nsAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsXULButtonAccessible: nsIAccessible

NS_IMETHODIMP
nsXULButtonAccessible::GetNumActions(PRUint8 *aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);

  *aCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsXULButtonAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("press"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsXULButtonAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != 0)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULButtonAccessible: nsAccessNode

PRBool
nsXULButtonAccessible::Init()
{
  if (!nsAccessibleWrap::Init())
    return PR_FALSE;

  if (ContainsMenu())
    nsCoreUtils::GeneratePopupTree(mContent);

  return PR_TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULButtonAccessible: nsAccessible

PRUint32
nsXULButtonAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_PUSHBUTTON;
}

nsresult
nsXULButtonAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  // Possible states: focused, focusable, unavailable(disabled).

  // get focus and disable status from base class
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  PRBool disabled = PR_FALSE;
  nsCOMPtr<nsIDOMXULControlElement> xulFormElement(do_QueryInterface(mContent));
  if (xulFormElement) {
    xulFormElement->GetDisabled(&disabled);
    if (disabled)
      *aState |= nsIAccessibleStates::STATE_UNAVAILABLE;
    else 
      *aState |= nsIAccessibleStates::STATE_FOCUSABLE;
  }

  // Buttons can be checked -- they simply appear pressed in rather than checked
  nsCOMPtr<nsIDOMXULButtonElement> xulButtonElement(do_QueryInterface(mContent));
  if (xulButtonElement) {
    nsAutoString type;
    xulButtonElement->GetType(type);
    if (type.EqualsLiteral("checkbox") || type.EqualsLiteral("radio")) {
      *aState |= nsIAccessibleStates::STATE_CHECKABLE;
      PRBool checked = PR_FALSE;
      PRInt32 checkState = 0;
      xulButtonElement->GetChecked(&checked);
      if (checked) {
        *aState |= nsIAccessibleStates::STATE_PRESSED;
        xulButtonElement->GetCheckState(&checkState);
        if (checkState == nsIDOMXULButtonElement::CHECKSTATE_MIXED) { 
          *aState |= nsIAccessibleStates::STATE_MIXED;
        }
      }
    }
  }

  if (ContainsMenu())
    *aState |= nsIAccessibleStates::STATE_HASPOPUP;

  if (mContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::_default))
    *aState |= nsIAccessibleStates::STATE_DEFAULT;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULButtonAccessible: nsAccessible protected

void
nsXULButtonAccessible::CacheChildren()
{
  // In general XUL button has not accessible children. Nevertheless menu
  // buttons can have button (@type="menu-button") and popup accessibles
  // (@type="menu-button" or @type="menu").

  // XXX: no children until the button is menu button. Probably it's not
  // totally correct but in general AT wants to have leaf buttons.
  PRBool isMenu = mContent->AttrValueIs(kNameSpaceID_None,
                                       nsAccessibilityAtoms::type,
                                       nsAccessibilityAtoms::menu,
                                       eCaseMatters);

  PRBool isMenuButton = isMenu ?
    PR_FALSE :
    mContent->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                          nsAccessibilityAtoms::menuButton, eCaseMatters);

  if (!isMenu && !isMenuButton)
    return;

  nsRefPtr<nsAccessible> menupopupAccessible;
  nsRefPtr<nsAccessible> buttonAccessible;

  nsAccTreeWalker walker(mWeakShell, mContent, PR_TRUE);

  nsRefPtr<nsAccessible> child;
  while ((child = walker.GetNextChild())) {
    PRUint32 role = child->Role();

    if (role == nsIAccessibleRole::ROLE_MENUPOPUP) {
      // Get an accessbile for menupopup or panel elements.
      menupopupAccessible.swap(child);

    } else if (isMenuButton && role == nsIAccessibleRole::ROLE_PUSHBUTTON) {
      // Button type="menu-button" contains a real button. Get an accessible
      // for it. Ignore dropmarker button what is placed as a last child.
      buttonAccessible.swap(child);
      break;
    }
  }

  if (!menupopupAccessible)
    return;

  AppendChild(menupopupAccessible);
  if (buttonAccessible)
    AppendChild(buttonAccessible);
}

////////////////////////////////////////////////////////////////////////////////
// nsXULButtonAccessible protected

PRBool
nsXULButtonAccessible::ContainsMenu()
{
  static nsIContent::AttrValuesArray strings[] =
    {&nsAccessibilityAtoms::menu, &nsAccessibilityAtoms::menuButton, nsnull};

  return mContent->FindAttrValueIn(kNameSpaceID_None,
                                   nsAccessibilityAtoms::type,
                                   strings, eCaseMatters) >= 0;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULDropmarkerAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULDropmarkerAccessible::
  nsXULDropmarkerAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsFormControlAccessible(aContent, aShell)
{
}

NS_IMETHODIMP nsXULDropmarkerAccessible::GetNumActions(PRUint8 *aResult)
{
  *aResult = 1;
  return NS_OK;
}

PRBool nsXULDropmarkerAccessible::DropmarkerOpen(PRBool aToggleOpen)
{
  PRBool isOpen = PR_FALSE;

  nsCOMPtr<nsIDOMXULButtonElement> parentButtonElement =
    do_QueryInterface(mContent->GetParent());

  if (parentButtonElement) {
    parentButtonElement->GetOpen(&isOpen);
    if (aToggleOpen)
      parentButtonElement->SetOpen(!isOpen);
  }
  else {
    nsCOMPtr<nsIDOMXULMenuListElement> parentMenuListElement =
      do_QueryInterface(parentButtonElement);
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
NS_IMETHODIMP nsXULDropmarkerAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    if (DropmarkerOpen(PR_FALSE))
      aName.AssignLiteral("close");
    else
      aName.AssignLiteral("open");
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

/**
  * Tell the Dropmarker to do its action
  */
NS_IMETHODIMP nsXULDropmarkerAccessible::DoAction(PRUint8 index)
{
  if (index == eAction_Click) {
    DropmarkerOpen(PR_TRUE); // Reverse the open attribute
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

PRUint32
nsXULDropmarkerAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_PUSHBUTTON;
}

nsresult
nsXULDropmarkerAccessible::GetStateInternal(PRUint32 *aState,
                                            PRUint32 *aExtraState)
{
  *aState = 0;

  if (IsDefunct()) {
    if (aExtraState)
      *aExtraState = nsIAccessibleStates::EXT_STATE_DEFUNCT;

    return NS_OK_DEFUNCT_OBJECT;
  }

  if (aExtraState)
    *aExtraState = 0;

  if (DropmarkerOpen(PR_FALSE))
    *aState = nsIAccessibleStates::STATE_PRESSED;

  return NS_OK;
}

                      
////////////////////////////////////////////////////////////////////////////////
// nsXULCheckboxAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULCheckboxAccessible::
  nsXULCheckboxAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsFormControlAccessible(aContent, aShell)
{
}

PRUint32
nsXULCheckboxAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_CHECKBUTTON;
}

NS_IMETHODIMP nsXULCheckboxAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

/**
  * Return the name of our only action
  */
NS_IMETHODIMP nsXULCheckboxAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    // check or uncheck
    PRUint32 state;
    nsresult rv = GetStateInternal(&state, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    if (state & nsIAccessibleStates::STATE_CHECKED)
      aName.AssignLiteral("uncheck");
    else
      aName.AssignLiteral("check");

    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/**
  * Tell the checkbox to do its only action -- check( or uncheck) itself
  */
NS_IMETHODIMP
nsXULCheckboxAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}

/**
  * Possible states: focused, focusable, unavailable(disabled), checked
  */
nsresult
nsXULCheckboxAccessible::GetStateInternal(PRUint32 *aState,
                                          PRUint32 *aExtraState)
{
  // Get focus and disable status from base class
  nsresult rv = nsFormControlAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);
  
  *aState |= nsIAccessibleStates::STATE_CHECKABLE;
  
  // Determine Checked state
  nsCOMPtr<nsIDOMXULCheckboxElement> xulCheckboxElement =
    do_QueryInterface(mContent);
  if (xulCheckboxElement) {
    PRBool checked = PR_FALSE;
    xulCheckboxElement->GetChecked(&checked);
    if (checked) {
      *aState |= nsIAccessibleStates::STATE_CHECKED;
      PRInt32 checkState = 0;
      xulCheckboxElement->GetCheckState(&checkState);
      if (checkState == nsIDOMXULCheckboxElement::CHECKSTATE_MIXED)
        *aState |= nsIAccessibleStates::STATE_MIXED;
    }
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULGroupboxAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULGroupboxAccessible::
  nsXULGroupboxAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

PRUint32
nsXULGroupboxAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_GROUPING;
}

nsresult
nsXULGroupboxAccessible::GetNameInternal(nsAString& aName)
{
  // XXX: we use the first related accessible only.
  nsCOMPtr<nsIAccessible> label =
    nsRelUtils::GetRelatedAccessible(this, nsIAccessibleRelation::RELATION_LABELLED_BY);

  if (label) {
    return label->GetName(aName);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULGroupboxAccessible::GetRelationByType(PRUint32 aRelationType,
                                           nsIAccessibleRelation **aRelation)
{
  nsresult rv = nsAccessibleWrap::GetRelationByType(aRelationType, aRelation);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aRelationType == nsIAccessibleRelation::RELATION_LABELLED_BY) {
    // The label for xul:groupbox is generated from xul:label that is
    // inside the anonymous content of the xul:caption.
    // The xul:label has an accessible object but the xul:caption does not
    PRInt32 childCount = GetChildCount();
    for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
      nsAccessible *childAcc = GetChildAt(childIdx);
      if (childAcc->Role() == nsIAccessibleRole::ROLE_LABEL) {
        // Ensure that it's our label
        // XXX: we'll fail if group accessible expose more than one relation
        // targets.
        nsCOMPtr<nsIAccessible> testGroupboxAccessible =
          nsRelUtils::GetRelatedAccessible(childAcc,
                                           nsIAccessibleRelation::RELATION_LABEL_FOR);

        if (testGroupboxAccessible == this) {
          // The <label> points back to this groupbox
          return nsRelUtils::
            AddTarget(aRelationType, aRelation, childAcc);
        }
      }
    }
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULProgressMeterAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULProgressMeterAccessible::
  nsXULProgressMeterAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsFormControlAccessible(aContent, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXULProgressMeterAccessible,
                             nsFormControlAccessible,
                             nsIAccessibleValue)

// nsAccessible

PRUint32
nsXULProgressMeterAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_PROGRESSBAR;
}

// nsIAccessibleValue

NS_IMETHODIMP
nsXULProgressMeterAccessible::GetValue(nsAString& aValue)
{
  nsresult rv = nsFormControlAccessible::GetValue(aValue);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aValue.IsEmpty())
    return NS_OK;

  double maxValue = 0;
  rv = GetMaximumValue(&maxValue);
  NS_ENSURE_SUCCESS(rv, rv);

  if (maxValue != 1) {
    double curValue = 0;
    rv = GetCurrentValue(&curValue);
    NS_ENSURE_SUCCESS(rv, rv);

    double percentValue = (curValue / maxValue) * 100;
    nsAutoString value;
    value.AppendFloat(percentValue); // AppendFloat isn't available on nsAString
    value.AppendLiteral("%");
    aValue = value;
    return NS_OK;
  }

  mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::value, aValue);
  if (aValue.IsEmpty())
    aValue.AppendLiteral("0");  // Empty value for progress meter = 0%

  aValue.AppendLiteral("%");
  return NS_OK;
}

NS_IMETHODIMP
nsXULProgressMeterAccessible::GetMaximumValue(double *aMaximumValue)
{
  nsresult rv = nsFormControlAccessible::GetMaximumValue(aMaximumValue);
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  nsAutoString value;
  if (mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::max, value)) {
    PRInt32 result = NS_OK;
    *aMaximumValue = value.ToFloat(&result);
    return result;
  }

  *aMaximumValue = 1; // 100% = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsXULProgressMeterAccessible::GetMinimumValue(double *aMinimumValue)
{
  nsresult rv = nsFormControlAccessible::GetMinimumValue(aMinimumValue);
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  *aMinimumValue = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsXULProgressMeterAccessible::GetMinimumIncrement(double *aMinimumIncrement)
{
  nsresult rv = nsFormControlAccessible::GetMinimumIncrement(aMinimumIncrement);
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  *aMinimumIncrement = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsXULProgressMeterAccessible::GetCurrentValue(double *aCurrentValue)
{
  nsresult rv = nsFormControlAccessible::GetCurrentValue(aCurrentValue);
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  nsAutoString attrValue;
  mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::value, attrValue);

  // Return zero value if there is no attribute or its value is empty.
  if (attrValue.IsEmpty())
    return NS_OK;

  PRInt32 error = NS_OK;
  double value = attrValue.ToFloat(&error);
  if (NS_FAILED(error))
    return NS_OK; // Zero value because of wrong markup.

  // If no max value then value is between 0 and 1 (refer to GetMaximumValue()
  // method where max value is assumed to be equal to 1 in this case).
  if (!mContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::max))
    value /= 100;

  *aCurrentValue = value;
  return NS_OK;
}

NS_IMETHODIMP
nsXULProgressMeterAccessible::SetCurrentValue(double aValue)
{
  return NS_ERROR_FAILURE; // Progress meters are readonly!
}


////////////////////////////////////////////////////////////////////////////////
// nsXULRadioButtonAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULRadioButtonAccessible::
  nsXULRadioButtonAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsRadioButtonAccessible(aContent, aShell)
{
}

/** We are Focusable and can be Checked and focused */
nsresult
nsXULRadioButtonAccessible::GetStateInternal(PRUint32 *aState,
                                             PRUint32 *aExtraState)
{
  nsresult rv = nsFormControlAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState |= nsIAccessibleStates::STATE_CHECKABLE;
  
  PRBool selected = PR_FALSE;   // Radio buttons can be selected

  nsCOMPtr<nsIDOMXULSelectControlItemElement> radioButton =
    do_QueryInterface(mContent);
  if (radioButton) {
    radioButton->GetSelected(&selected);
    if (selected) {
      *aState |= nsIAccessibleStates::STATE_CHECKED;
    }
  }

  return NS_OK;
}

void
nsXULRadioButtonAccessible::GetPositionAndSizeInternal(PRInt32 *aPosInSet,
                                                       PRInt32 *aSetSize)
{
  nsAccUtils::GetPositionAndSizeForXULSelectControlItem(mContent, aPosInSet,
                                                        aSetSize);
}


////////////////////////////////////////////////////////////////////////////////
// nsXULRadioGroupAccessible
////////////////////////////////////////////////////////////////////////////////

/**
  * XUL Radio Group
  *   The Radio Group proxies for the Radio Buttons themselves. The Group gets
  *   focus whereas the Buttons do not. So we only have an accessible object for
  *   this for the purpose of getting the proper RadioButton. Need this here to 
  *   avoid circular reference problems when navigating the accessible tree and
  *   for getting to the radiobuttons.
  */

nsXULRadioGroupAccessible::
  nsXULRadioGroupAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXULSelectableAccessible(aContent, aShell)
{ 
}

PRUint32
nsXULRadioGroupAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_GROUPING;
}

nsresult
nsXULRadioGroupAccessible::GetStateInternal(PRUint32 *aState,
                                            PRUint32 *aExtraState)
{
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  // The radio group is not focusable. Sometimes the focus controller will
  // report that it is focused. That means that the actual selected radio button
  // should be considered focused.
  *aState &= ~(nsIAccessibleStates::STATE_FOCUSABLE |
               nsIAccessibleStates::STATE_FOCUSED);

  return NS_OK;
}

                      
////////////////////////////////////////////////////////////////////////////////
// nsXULStatusBarAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULStatusBarAccessible::
  nsXULStatusBarAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

PRUint32
nsXULStatusBarAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_STATUSBAR;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULToolbarButtonAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULToolbarButtonAccessible::
  nsXULToolbarButtonAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXULButtonAccessible(aContent, aShell)
{
}

void
nsXULToolbarButtonAccessible::GetPositionAndSizeInternal(PRInt32 *aPosInSet,
                                                         PRInt32 *aSetSize)
{
  PRInt32 setSize = 0;
  PRInt32 posInSet = 0;

  nsAccessible* parent(GetParent());
  NS_ENSURE_TRUE(parent,);

  PRInt32 childCount = parent->GetChildCount();
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsAccessible* child = parent->GetChildAt(childIdx);
    if (IsSeparator(child)) { // end of a group of buttons
      if (posInSet)
        break; // we've found our group, so we're done

      setSize = 0; // not our group, so start a new group

    } else {
      setSize++; // another button in the group

      if (child == this)
        posInSet = setSize; // we've found our button
    }
  }

  *aPosInSet = posInSet;
  *aSetSize = setSize;
}

PRBool
nsXULToolbarButtonAccessible::IsSeparator(nsAccessible *aAccessible)
{
  nsCOMPtr<nsIDOMNode> domNode;
  aAccessible->GetDOMNode(getter_AddRefs(domNode));
  nsCOMPtr<nsIContent> contentDomNode(do_QueryInterface(domNode));

  if (!contentDomNode)
    return PR_FALSE;

  return (contentDomNode->Tag() == nsAccessibilityAtoms::toolbarseparator) ||
         (contentDomNode->Tag() == nsAccessibilityAtoms::toolbarspacer) ||
         (contentDomNode->Tag() == nsAccessibilityAtoms::toolbarspring);
}


////////////////////////////////////////////////////////////////////////////////
// nsXULToolbarAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULToolbarAccessible::
  nsXULToolbarAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

PRUint32
nsXULToolbarAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_TOOLBAR;
}

nsresult
nsXULToolbarAccessible::GetNameInternal(nsAString& aName)
{
  nsAutoString name;
  if (mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::toolbarname,
                        name)) {
    name.CompressWhitespace();
    aName = name;
  }

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULToolbarAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULToolbarSeparatorAccessible::
  nsXULToolbarSeparatorAccessible(nsIContent *aContent,
                                  nsIWeakReference *aShell) :
  nsLeafAccessible(aContent, aShell)
{
}

PRUint32
nsXULToolbarSeparatorAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_SEPARATOR;
}

nsresult
nsXULToolbarSeparatorAccessible::GetStateInternal(PRUint32 *aState,
                                                  PRUint32 *aExtraState)
{
  *aState = 0;  // no special state flags for toolbar separator

  if (IsDefunct()) {
    if (aExtraState)
      *aExtraState = nsIAccessibleStates::EXT_STATE_DEFUNCT;

    return NS_OK_DEFUNCT_OBJECT;
  }

  if (aExtraState)
    *aExtraState = 0;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTextFieldAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTextFieldAccessible::
 nsXULTextFieldAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
 nsHyperTextAccessibleWrap(aContent, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED3(nsXULTextFieldAccessible, nsAccessible, nsHyperTextAccessible, nsIAccessibleText, nsIAccessibleEditableText)

////////////////////////////////////////////////////////////////////////////////
// nsXULTextFieldAccessible: nsIAccessible

NS_IMETHODIMP nsXULTextFieldAccessible::GetValue(nsAString& aValue)
{
  PRUint32 state;
  nsresult rv = GetStateInternal(&state, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  if (state & nsIAccessibleStates::STATE_PROTECTED)    // Don't return password text!
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULTextBoxElement> textBox(do_QueryInterface(mContent));
  if (textBox) {
    return textBox->GetValue(aValue);
  }
  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mContent));
  if (menuList) {
    return menuList->GetLabel(aValue);
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsXULTextFieldAccessible::GetARIAState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsHyperTextAccessibleWrap::GetARIAState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  nsStateMapEntry::MapToStates(mContent, aState, aExtraState, eARIAAutoComplete);

  return NS_OK;
}

nsresult
nsXULTextFieldAccessible::GetStateInternal(PRUint32 *aState,
                                           PRUint32 *aExtraState)
{
  nsresult rv = nsHyperTextAccessibleWrap::GetStateInternal(aState,
                                                            aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> inputField(GetInputField());
  NS_ENSURE_TRUE(inputField, NS_ERROR_FAILURE);

  // Create a temporary accessible from the HTML text field
  // to get the accessible state from. Doesn't add to cache
  // because Init() is not called.
  nsHTMLTextFieldAccessible* tempAccessible =
    new nsHTMLTextFieldAccessible(inputField, mWeakShell);
  if (!tempAccessible)
    return NS_ERROR_OUT_OF_MEMORY;
  nsCOMPtr<nsIAccessible> kungFuDeathGrip = tempAccessible;
  rv = tempAccessible->GetStateInternal(aState, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  if (gLastFocusedNode == mContent)
    *aState |= nsIAccessibleStates::STATE_FOCUSED;

  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mContent));
  if (menuList) {
    // <xul:menulist droppable="false">
    if (!mContent->AttrValueIs(kNameSpaceID_None,
                               nsAccessibilityAtoms::editable,
                               nsAccessibilityAtoms::_true, eIgnoreCase)) {
      *aState |= nsIAccessibleStates::STATE_READONLY;
    }
  }
  else {
    // <xul:textbox>
    if (mContent->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                              nsAccessibilityAtoms::password, eIgnoreCase)) {
      *aState |= nsIAccessibleStates::STATE_PROTECTED;
    }
    if (mContent->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::readonly,
                              nsAccessibilityAtoms::_true, eIgnoreCase)) {
      *aState |= nsIAccessibleStates::STATE_READONLY;
    }
  }

  if (!aExtraState)
    return NS_OK;

  PRBool isMultiLine = mContent->HasAttr(kNameSpaceID_None,
                                         nsAccessibilityAtoms::multiline);

  if (isMultiLine) {
    *aExtraState |= nsIAccessibleStates::EXT_STATE_MULTI_LINE;
  }
  else {
    *aExtraState |= nsIAccessibleStates::EXT_STATE_SINGLE_LINE;
  }

  return NS_OK;
}

PRUint32
nsXULTextFieldAccessible::NativeRole()
{
  if (mContent->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                            nsAccessibilityAtoms::password, eIgnoreCase))
    return nsIAccessibleRole::ROLE_PASSWORD_TEXT;
  return nsIAccessibleRole::ROLE_ENTRY;
}


/**
  * Only one actions available
  */
NS_IMETHODIMP nsXULTextFieldAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

/**
  * Return the name of our only action
  */
NS_IMETHODIMP nsXULTextFieldAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("activate"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/**
  * Tell the button to do its action
  */
NS_IMETHODIMP nsXULTextFieldAccessible::DoAction(PRUint8 index)
{
  if (index == 0) {
    nsCOMPtr<nsIDOMXULElement> element(do_QueryInterface(mContent));
    if (element)
    {
      element->Focus();
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

PRBool
nsXULTextFieldAccessible::GetAllowsAnonChildAccessibles()
{
  return PR_FALSE;
}

NS_IMETHODIMP nsXULTextFieldAccessible::GetAssociatedEditor(nsIEditor **aEditor)
{
  *aEditor = nsnull;

  nsCOMPtr<nsIContent> inputField = GetInputField();
  nsCOMPtr<nsIDOMNSEditableElement> editableElt(do_QueryInterface(inputField));
  NS_ENSURE_TRUE(editableElt, NS_ERROR_FAILURE);
  return editableElt->GetEditor(aEditor);
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTextFieldAccessible: nsAccessible protected

void
nsXULTextFieldAccessible::CacheChildren()
{
  // Create child accessibles for native anonymous content of underlying HTML
  // input element.
  nsCOMPtr<nsIContent> inputContent(GetInputField());
  if (!inputContent)
    return;

  nsAccTreeWalker walker(mWeakShell, inputContent, PR_FALSE);

  nsRefPtr<nsAccessible> child;
  while ((child = walker.GetNextChild()) && AppendChild(child));
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTextFieldAccessible protected

already_AddRefed<nsIContent>
nsXULTextFieldAccessible::GetInputField() const
{
  nsCOMPtr<nsIDOMNode> inputFieldDOMNode;
  nsCOMPtr<nsIDOMXULTextBoxElement> textBox = do_QueryInterface(mContent);
  if (textBox) {
    textBox->GetInputField(getter_AddRefs(inputFieldDOMNode));

  } else {
    // <xul:menulist droppable="false">
    nsCOMPtr<nsIDOMXULMenuListElement> menuList = do_QueryInterface(mContent);
    if (menuList)
      menuList->GetInputField(getter_AddRefs(inputFieldDOMNode));
  }

  NS_ASSERTION(inputFieldDOMNode, "No input field for nsXULTextFieldAccessible");

  nsIContent* inputField = nsnull;
  if (inputFieldDOMNode)
    CallQueryInterface(inputFieldDOMNode, &inputField);

  return inputField;
}
