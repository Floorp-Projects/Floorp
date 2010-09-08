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
 *   Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsXULMenuAccessible.h"

#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsXULFormControlAccessible.h"

#include "nsIDOMElement.h"
#include "nsIDOMXULElement.h"
#include "nsIMutableArray.h"
#include "nsIDOMXULContainerElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMKeyEvent.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManager.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsGUIEvent.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"


static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

////////////////////////////////////////////////////////////////////////////////
// nsXULSelectableAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULSelectableAccessible::
  nsXULSelectableAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
  mSelectControl = do_QueryInterface(aContent);
}

////////////////////////////////////////////////////////////////////////////////
// nsXULSelectableAccessible: nsAccessNode

void
nsXULSelectableAccessible::Shutdown()
{
  mSelectControl = nsnull;
  nsAccessibleWrap::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// nsXULSelectableAccessible: SelectAccessible

bool
nsXULSelectableAccessible::IsSelect()
{
  return !!mSelectControl;
}

// Interface methods
already_AddRefed<nsIArray>
nsXULSelectableAccessible::SelectedItems()
{
  nsCOMPtr<nsIMutableArray> selectedItems =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!selectedItems)
    return nsnull;

  // For XUL multi-select control
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> xulMultiSelect =
    do_QueryInterface(mSelectControl);
  if (xulMultiSelect) {
    PRInt32 length = 0;
    xulMultiSelect->GetSelectedCount(&length);
    for (PRInt32 index = 0; index < length; index++) {
      nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm;
      xulMultiSelect->GetSelectedItem(index, getter_AddRefs(itemElm));
      nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemElm));
      nsAccessible* item =
        GetAccService()->GetAccessibleInWeakShell(itemNode, mWeakShell);
      if (item)
        selectedItems->AppendElement(static_cast<nsIAccessible*>(item),
                                     PR_FALSE);
    }
  }
  else {  // Single select?
    nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm;
    mSelectControl->GetSelectedItem(getter_AddRefs(itemElm));
    nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemElm));
    if(itemNode) {
      nsAccessible* item =
        GetAccService()->GetAccessibleInWeakShell(itemNode, mWeakShell);
      if (item)
        selectedItems->AppendElement(static_cast<nsIAccessible*>(item),
                                     PR_FALSE);
    }
  }

  nsIMutableArray* items = nsnull;
  selectedItems.forget(&items);
  return items;
}

nsAccessible*
nsXULSelectableAccessible::GetSelectedItem(PRUint32 aIndex)
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);

  nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm;
  if (multiSelectControl)
    multiSelectControl->GetSelectedItem(aIndex, getter_AddRefs(itemElm));
  else if (aIndex == 0)
    mSelectControl->GetSelectedItem(getter_AddRefs(itemElm));

  nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemElm));
  return itemNode ?
    GetAccService()->GetAccessibleInWeakShell(itemNode, mWeakShell) : nsnull;
}

PRUint32
nsXULSelectableAccessible::SelectedItemCount()
{
  // For XUL multi-select control
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);
  if (multiSelectControl) {
    PRInt32 count = 0;
    multiSelectControl->GetSelectedCount(&count);
    return count;
  }

  // For XUL single-select control/menulist
  PRInt32 index;
  mSelectControl->GetSelectedIndex(&index);
  return (index >= 0) ? 1 : 0;
}

bool
nsXULSelectableAccessible::AddItemToSelection(PRUint32 aIndex)
{
  nsAccessible* item = GetChildAt(aIndex);
  if (!item)
    return false;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm =
    do_QueryInterface(item->GetContent());
  if (!itemElm)
    return false;

  PRBool isItemSelected = PR_FALSE;
  itemElm->GetSelected(&isItemSelected);
  if (isItemSelected)
    return true;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);

  if (multiSelectControl)
    multiSelectControl->AddItemToSelection(itemElm);
  else
    mSelectControl->SetSelectedItem(itemElm);

  return true;
}

bool
nsXULSelectableAccessible::RemoveItemFromSelection(PRUint32 aIndex)
{
  nsAccessible* item = GetChildAt(aIndex);
  if (!item)
    return false;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm =
      do_QueryInterface(item->GetContent());
  if (!itemElm)
    return false;

  PRBool isItemSelected = PR_FALSE;
  itemElm->GetSelected(&isItemSelected);
  if (!isItemSelected)
    return true;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);

  if (multiSelectControl)
    multiSelectControl->RemoveItemFromSelection(itemElm);
  else
    mSelectControl->SetSelectedItem(nsnull);

  return true;
}

bool
nsXULSelectableAccessible::IsItemSelected(PRUint32 aIndex)
{
  nsAccessible* item = GetChildAt(aIndex);
  if (!item)
    return false;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm =
    do_QueryInterface(item->GetContent());
  if (!itemElm)
    return false;

  PRBool isItemSelected = PR_FALSE;
  itemElm->GetSelected(&isItemSelected);
  return isItemSelected;
}

bool
nsXULSelectableAccessible::UnselectAll()
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);
  multiSelectControl ?
    multiSelectControl->ClearSelection() : mSelectControl->SetSelectedIndex(-1);

  return true;
}

bool
nsXULSelectableAccessible::SelectAll()
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);
  if (multiSelectControl) {
    multiSelectControl->SelectAll();
    return true;
  }

  // otherwise, don't support this method
  return false;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULMenuitemAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULMenuitemAccessible::
  nsXULMenuitemAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

PRBool
nsXULMenuitemAccessible::Init()
{
  if (!nsAccessibleWrap::Init())
    return PR_FALSE;

  nsCoreUtils::GeneratePopupTree(mContent);
  return PR_TRUE;
}

nsresult
nsXULMenuitemAccessible::GetStateInternal(PRUint32 *aState,
                                          PRUint32 *aExtraState)
{
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  // Focused?
  if (mContent->HasAttr(kNameSpaceID_None,
                        nsAccessibilityAtoms::_moz_menuactive))
    *aState |= nsIAccessibleStates::STATE_FOCUSED;

  // Has Popup?
  if (mContent->NodeInfo()->Equals(nsAccessibilityAtoms::menu,
                                   kNameSpaceID_XUL)) {
    *aState |= nsIAccessibleStates::STATE_HASPOPUP;
    if (mContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::open))
      *aState |= nsIAccessibleStates::STATE_EXPANDED;
    else
      *aState |= nsIAccessibleStates::STATE_COLLAPSED;
  }

  // Checkable/checked?
  static nsIContent::AttrValuesArray strings[] =
    { &nsAccessibilityAtoms::radio, &nsAccessibilityAtoms::checkbox, nsnull };

  if (mContent->FindAttrValueIn(kNameSpaceID_None,
                                nsAccessibilityAtoms::type,
                                strings, eCaseMatters) >= 0) {

    // Checkable?
    *aState |= nsIAccessibleStates::STATE_CHECKABLE;

    // Checked?
    if (mContent->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::checked,
                              nsAccessibilityAtoms::_true, eCaseMatters))
      *aState |= nsIAccessibleStates::STATE_CHECKED;
  }

  // Combo box listitem
  PRBool isComboboxOption = (Role() == nsIAccessibleRole::ROLE_COMBOBOX_OPTION);
  if (isComboboxOption) {
    // Is selected?
    PRBool isSelected = PR_FALSE;
    nsCOMPtr<nsIDOMXULSelectControlItemElement>
      item(do_QueryInterface(mContent));
    NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);
    item->GetSelected(&isSelected);

    // Is collapsed?
    PRBool isCollapsed = PR_FALSE;
    nsAccessible* parentAcc = GetParent();
    if (nsAccUtils::State(parentAcc) & nsIAccessibleStates::STATE_INVISIBLE)
      isCollapsed = PR_TRUE;

    if (isSelected) {
      *aState |= nsIAccessibleStates::STATE_SELECTED;

      // Selected and collapsed?
      if (isCollapsed) {
        // Set selected option offscreen/invisible according to combobox state
        nsAccessible* grandParentAcc = parentAcc->GetParent();
        NS_ENSURE_TRUE(grandParentAcc, NS_ERROR_FAILURE);
        NS_ASSERTION(grandParentAcc->Role() == nsIAccessibleRole::ROLE_COMBOBOX,
                     "grandparent of combobox listitem is not combobox");
        PRUint32 grandParentState, grandParentExtState;
        grandParentAcc->GetState(&grandParentState, &grandParentExtState);
        *aState &= ~(nsIAccessibleStates::STATE_OFFSCREEN |
                     nsIAccessibleStates::STATE_INVISIBLE);
        *aState |= (grandParentState & nsIAccessibleStates::STATE_OFFSCREEN) |
                   (grandParentState & nsIAccessibleStates::STATE_INVISIBLE);
        if (aExtraState) {
          *aExtraState |=
            grandParentExtState & nsIAccessibleStates::EXT_STATE_OPAQUE;
        }
      } // isCollapsed
    } // isSelected
  } // ROLE_COMBOBOX_OPTION

  // Set focusable and selectable for items that are available
  // and whose metric setting does allow disabled items to be focused.
  if (*aState & nsIAccessibleStates::STATE_UNAVAILABLE) {
    // Honour the LookAndFeel metric.
    nsCOMPtr<nsILookAndFeel> lookNFeel(do_GetService(kLookAndFeelCID));
    PRInt32 skipDisabledMenuItems = 0;
    lookNFeel->GetMetric(nsILookAndFeel::eMetric_SkipNavigatingDisabledMenuItem,
                         skipDisabledMenuItems);
    // We don't want the focusable and selectable states for combobox items,
    // so exclude them here as well.
    if (skipDisabledMenuItems || isComboboxOption) {
      return NS_OK;
    }
  }
  *aState|= (nsIAccessibleStates::STATE_FOCUSABLE |
             nsIAccessibleStates::STATE_SELECTABLE);

  return NS_OK;
}

nsresult
nsXULMenuitemAccessible::GetNameInternal(nsAString& aName)
{
  mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::label, aName);
  return NS_OK;
}

NS_IMETHODIMP
nsXULMenuitemAccessible::GetDescription(nsAString& aDescription)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::description,
                    aDescription);

  return NS_OK;
}

//return menu accesskey: N or Alt+F
NS_IMETHODIMP
nsXULMenuitemAccessible::GetKeyboardShortcut(nsAString& aAccessKey)
{
  aAccessKey.Truncate();
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  static PRInt32 gMenuAccesskeyModifier = -1;  // magic value of -1 indicates unitialized state

  // We do not use nsCoreUtils::GetAccesskeyFor() because accesskeys for
  // menu are't registered by nsIEventStateManager.
  nsAutoString accesskey;
  mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::accesskey,
                    accesskey);
  if (accesskey.IsEmpty())
    return NS_OK;

  nsAccessible* parentAcc = GetParent();
  if (parentAcc) {
    if (parentAcc->NativeRole() == nsIAccessibleRole::ROLE_MENUBAR) {
      // If top level menu item, add Alt+ or whatever modifier text to string
      // No need to cache pref service, this happens rarely
      if (gMenuAccesskeyModifier == -1) {
        // Need to initialize cached global accesskey pref
        gMenuAccesskeyModifier = 0;
        nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
        if (prefBranch)
          prefBranch->GetIntPref("ui.key.menuAccessKey", &gMenuAccesskeyModifier);
      }

      nsAutoString propertyKey;
      switch (gMenuAccesskeyModifier) {
        case nsIDOMKeyEvent::DOM_VK_CONTROL:
          propertyKey.AssignLiteral("VK_CONTROL");
          break;
        case nsIDOMKeyEvent::DOM_VK_ALT:
          propertyKey.AssignLiteral("VK_ALT");
          break;
        case nsIDOMKeyEvent::DOM_VK_META:
          propertyKey.AssignLiteral("VK_META");
          break;
      }

      if (!propertyKey.IsEmpty())
        nsAccessible::GetFullKeyName(propertyKey, accesskey, aAccessKey);
    }
  }

  if (aAccessKey.IsEmpty())
    aAccessKey = accesskey;

  return NS_OK;
}

//return menu shortcut: Ctrl+F or Ctrl+Shift+L
NS_IMETHODIMP
nsXULMenuitemAccessible::GetDefaultKeyBinding(nsAString& aKeyBinding)
{
  aKeyBinding.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAutoString accelText;
  mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::acceltext,
                    aKeyBinding);

  return NS_OK;
}

PRUint32
nsXULMenuitemAccessible::NativeRole()
{
  nsCOMPtr<nsIDOMXULContainerElement> xulContainer(do_QueryInterface(mContent));
  if (xulContainer)
    return nsIAccessibleRole::ROLE_PARENT_MENUITEM;

  if (mParent && mParent->Role() == nsIAccessibleRole::ROLE_COMBOBOX_LIST)
    return nsIAccessibleRole::ROLE_COMBOBOX_OPTION;

  if (mContent->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                            nsAccessibilityAtoms::radio, eCaseMatters)) {
    return nsIAccessibleRole::ROLE_RADIO_MENU_ITEM;
  }

  if (mContent->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                            nsAccessibilityAtoms::checkbox,
                            eCaseMatters)) {
    return nsIAccessibleRole::ROLE_CHECK_MENU_ITEM;
  }

  return nsIAccessibleRole::ROLE_MENUITEM;
}

PRInt32
nsXULMenuitemAccessible::GetLevelInternal()
{
  return nsAccUtils::GetLevelForXULContainerItem(mContent);
}

void
nsXULMenuitemAccessible::GetPositionAndSizeInternal(PRInt32 *aPosInSet,
                                                    PRInt32 *aSetSize)
{
  nsAccUtils::GetPositionAndSizeForXULContainerItem(mContent, aPosInSet,
                                                    aSetSize);
}

PRBool
nsXULMenuitemAccessible::GetAllowsAnonChildAccessibles()
{
  // That indicates we don't walk anonymous children for menuitems
  return PR_FALSE;
}

NS_IMETHODIMP nsXULMenuitemAccessible::DoAction(PRUint8 index)
{
  if (index == eAction_Click) {   // default action
    DoCommand();
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

/** select us! close combo box if necessary*/
NS_IMETHODIMP nsXULMenuitemAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("click"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsXULMenuitemAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULMenuSeparatorAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULMenuSeparatorAccessible::
  nsXULMenuSeparatorAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXULMenuitemAccessible(aContent, aShell)
{
}

nsresult
nsXULMenuSeparatorAccessible::GetStateInternal(PRUint32 *aState,
                                               PRUint32 *aExtraState)
{
  // Isn't focusable, but can be offscreen/invisible -- only copy those states
  nsresult rv = nsXULMenuitemAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState &= (nsIAccessibleStates::STATE_OFFSCREEN | 
              nsIAccessibleStates::STATE_INVISIBLE);

  return NS_OK;
}

nsresult
nsXULMenuSeparatorAccessible::GetNameInternal(nsAString& aName)
{
  return NS_OK;
}

PRUint32
nsXULMenuSeparatorAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_SEPARATOR;
}

NS_IMETHODIMP nsXULMenuSeparatorAccessible::DoAction(PRUint8 index)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULMenuSeparatorAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULMenuSeparatorAccessible::GetNumActions(PRUint8 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULMenupopupAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULMenupopupAccessible::
  nsXULMenupopupAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXULSelectableAccessible(aContent, aShell)
{ 
  // May be the anonymous <menupopup> inside <menulist> (a combobox)
  mSelectControl = do_QueryInterface(mContent->GetParent());
}

nsresult
nsXULMenupopupAccessible::GetStateInternal(PRUint32 *aState,
                                           PRUint32 *aExtraState)
{
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

#ifdef DEBUG_A11Y
  // We are onscreen if our parent is active
  PRBool isActive = mContent->HasAttr(kNameSpaceID_None,
                                      nsAccessibilityAtoms::menuactive);
  if (!isActive) {
    nsAccessible* parent(GetParent());
    NS_ENSURE_STATE(parent);

    nsIContent *parentContent = parnet->GetContent();
    NS_ENSURE_STATE(parentContent);

    isActive = parentContent->HasAttr(kNameSpaceID_None,
                                      nsAccessibilityAtoms::open);
  }

  NS_ASSERTION(isActive || *aState & nsIAccessibleStates::STATE_INVISIBLE,
               "XULMenupopup doesn't have STATE_INVISIBLE when it's inactive");
#endif

  if (*aState & nsIAccessibleStates::STATE_INVISIBLE)
    *aState |= (nsIAccessibleStates::STATE_OFFSCREEN |
                nsIAccessibleStates::STATE_COLLAPSED);

  return NS_OK;
}

nsresult
nsXULMenupopupAccessible::GetNameInternal(nsAString& aName)
{
  nsIContent *content = mContent;
  while (content && aName.IsEmpty()) {
    content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::label, aName);
    content = content->GetParent();
  }

  return NS_OK;
}

PRUint32
nsXULMenupopupAccessible::NativeRole()
{
  // If accessible is not bound to the tree (this happens while children are
  // cached) return general role.
  if (mParent) {
    PRUint32 role = mParent->Role();
    if (role == nsIAccessibleRole::ROLE_COMBOBOX ||
        role == nsIAccessibleRole::ROLE_AUTOCOMPLETE) {
      return nsIAccessibleRole::ROLE_COMBOBOX_LIST;
    }

    if (role == nsIAccessibleRole::ROLE_PUSHBUTTON) {
      // Some widgets like the search bar have several popups, owned by buttons.
      nsAccessible* grandParent = mParent->GetParent();
      if (grandParent &&
          grandParent->Role() == nsIAccessibleRole::ROLE_AUTOCOMPLETE)
        return nsIAccessibleRole::ROLE_COMBOBOX_LIST;
    }
  }

  return nsIAccessibleRole::ROLE_MENUPOPUP;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULMenubarAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULMenubarAccessible::
  nsXULMenubarAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

nsresult
nsXULMenubarAccessible::GetStateInternal(PRUint32 *aState,
                                         PRUint32 *aExtraState)
{
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  // Menu bar iteself is not actually focusable
  *aState &= ~nsIAccessibleStates::STATE_FOCUSABLE;
  return rv;
}


nsresult
nsXULMenubarAccessible::GetNameInternal(nsAString& aName)
{
  aName.AssignLiteral("Application");
  return NS_OK;
}

PRUint32
nsXULMenubarAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_MENUBAR;
}

