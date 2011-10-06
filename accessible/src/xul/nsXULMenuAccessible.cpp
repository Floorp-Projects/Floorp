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
#include "nsDocAccessible.h"
#include "nsXULFormControlAccessible.h"
#include "States.h"

#include "nsIDOMElement.h"
#include "nsIDOMXULElement.h"
#include "nsIMutableArray.h"
#include "nsIDOMXULContainerElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMKeyEvent.h"
#include "nsIServiceManager.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsGUIEvent.h"
#include "nsMenuBarFrame.h"
#include "nsMenuPopupFrame.h"

#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::a11y;

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

  bool isItemSelected = false;
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

  bool isItemSelected = false;
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

  bool isItemSelected = false;
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
// nsXULSelectableAccessible: Widgets

nsAccessible*
nsXULSelectableAccessible::CurrentItem()
{
  if (!mSelectControl)
    return nsnull;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> currentItemElm;
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);
  if (multiSelectControl)
    multiSelectControl->GetCurrentItem(getter_AddRefs(currentItemElm));
  else
    mSelectControl->GetSelectedItem(getter_AddRefs(currentItemElm));

  nsCOMPtr<nsINode> DOMNode;
  if (currentItemElm)
    DOMNode = do_QueryInterface(currentItemElm);

  if (DOMNode) {
    nsDocAccessible* document = GetDocAccessible();
    if (document)
      return document->GetAccessible(DOMNode);
  }

  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULMenuitemAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULMenuitemAccessible::
  nsXULMenuitemAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

PRUint64
nsXULMenuitemAccessible::NativeState()
{
  PRUint64 state = nsAccessible::NativeState();

  // Has Popup?
  if (mContent->NodeInfo()->Equals(nsGkAtoms::menu, kNameSpaceID_XUL)) {
    state |= states::HASPOPUP;
    if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::open))
      state |= states::EXPANDED;
    else
      state |= states::COLLAPSED;
  }

  // Checkable/checked?
  static nsIContent::AttrValuesArray strings[] =
    { &nsGkAtoms::radio, &nsGkAtoms::checkbox, nsnull };

  if (mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::type, strings,
                                eCaseMatters) >= 0) {

    // Checkable?
    state |= states::CHECKABLE;

    // Checked?
    if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::checked,
                              nsGkAtoms::_true, eCaseMatters))
      state |= states::CHECKED;
  }

  // Combo box listitem
  bool isComboboxOption = (Role() == nsIAccessibleRole::ROLE_COMBOBOX_OPTION);
  if (isComboboxOption) {
    // Is selected?
    bool isSelected = false;
    nsCOMPtr<nsIDOMXULSelectControlItemElement>
      item(do_QueryInterface(mContent));
    NS_ENSURE_TRUE(item, state);
    item->GetSelected(&isSelected);

    // Is collapsed?
    bool isCollapsed = false;
    nsAccessible* parent = Parent();
    if (parent && parent->State() & states::INVISIBLE)
      isCollapsed = PR_TRUE;

    if (isSelected) {
      state |= states::SELECTED;

      // Selected and collapsed?
      if (isCollapsed) {
        // Set selected option offscreen/invisible according to combobox state
        nsAccessible* grandParent = parent->Parent();
        if (!grandParent)
          return state;
        NS_ASSERTION(grandParent->Role() == nsIAccessibleRole::ROLE_COMBOBOX,
                     "grandparent of combobox listitem is not combobox");
        PRUint64 grandParentState = grandParent->State();
        state &= ~(states::OFFSCREEN | states::INVISIBLE);
        state |= (grandParentState & states::OFFSCREEN) |
                 (grandParentState & states::INVISIBLE) |
                 (grandParentState & states::OPAQUE1);
      } // isCollapsed
    } // isSelected
  } // ROLE_COMBOBOX_OPTION

  // Set focusable and selectable for items that are available
  // and whose metric setting does allow disabled items to be focused.
  if (state & states::UNAVAILABLE) {
    // Honour the LookAndFeel metric.
    PRInt32 skipDisabledMenuItems =
      LookAndFeel::GetInt(LookAndFeel::eIntID_SkipNavigatingDisabledMenuItem);
    // We don't want the focusable and selectable states for combobox items,
    // so exclude them here as well.
    if (skipDisabledMenuItems || isComboboxOption) {
      return state;
    }
  }

  state |= (states::FOCUSABLE | states::SELECTABLE);
  if (FocusMgr()->IsFocused(this))
    state |= states::FOCUSED;

  return state;
}

nsresult
nsXULMenuitemAccessible::GetNameInternal(nsAString& aName)
{
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, aName);
  return NS_OK;
}

void
nsXULMenuitemAccessible::Description(nsString& aDescription)
{
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::description,
                    aDescription);
}

KeyBinding
nsXULMenuitemAccessible::AccessKey() const
{
  // Return menu accesskey: N or Alt+F.
  static PRInt32 gMenuAccesskeyModifier = -1;  // magic value of -1 indicates unitialized state

  // We do not use nsCoreUtils::GetAccesskeyFor() because accesskeys for
  // menu are't registered by nsEventStateManager.
  nsAutoString accesskey;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey,
                    accesskey);
  if (accesskey.IsEmpty())
    return KeyBinding();

  PRUint32 modifierKey = 0;

  nsAccessible* parentAcc = Parent();
  if (parentAcc) {
    if (parentAcc->NativeRole() == nsIAccessibleRole::ROLE_MENUBAR) {
      // If top level menu item, add Alt+ or whatever modifier text to string
      // No need to cache pref service, this happens rarely
      if (gMenuAccesskeyModifier == -1) {
        // Need to initialize cached global accesskey pref
        gMenuAccesskeyModifier = Preferences::GetInt("ui.key.menuAccessKey", 0);
      }

      switch (gMenuAccesskeyModifier) {
        case nsIDOMKeyEvent::DOM_VK_CONTROL:
          modifierKey = KeyBinding::kControl;
          break;
        case nsIDOMKeyEvent::DOM_VK_ALT:
          modifierKey = KeyBinding::kAlt;
          break;
        case nsIDOMKeyEvent::DOM_VK_META:
          modifierKey = KeyBinding::kMeta;
          break;
      }
    }
  }

  return KeyBinding(accesskey[0], modifierKey);
}

KeyBinding
nsXULMenuitemAccessible::KeyboardShortcut() const
{
  nsAutoString keyElmId;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::key, keyElmId);
  if (keyElmId.IsEmpty())
    return KeyBinding();

  nsIDocument* document = mContent->GetOwnerDoc();
  if (!document)
    return KeyBinding();

  nsIContent* keyElm = document->GetElementById(keyElmId);
  if (!keyElm)
    return KeyBinding();

  PRUint32 key = 0;

  nsAutoString keyStr;
  keyElm->GetAttr(kNameSpaceID_None, nsGkAtoms::key, keyStr);
  if (keyStr.IsEmpty()) {
    nsAutoString keyCodeStr;
    keyElm->GetAttr(kNameSpaceID_None, nsGkAtoms::keycode, keyCodeStr);
    PRUint32 errorCode;
    key = keyStr.ToInteger(&errorCode, kAutoDetect);
  } else {
    key = keyStr[0];
  }

  nsAutoString modifiersStr;
  keyElm->GetAttr(kNameSpaceID_None, nsGkAtoms::modifiers, modifiersStr);

  PRUint32 modifierMask = 0;
  if (modifiersStr.Find("shift") != -1)
    modifierMask |= KeyBinding::kShift;
  if (modifiersStr.Find("alt") != -1)
    modifierMask |= KeyBinding::kAlt;
  if (modifiersStr.Find("meta") != -1)
    modifierMask |= KeyBinding::kMeta;
  if (modifiersStr.Find("control") != -1)
    modifierMask |= KeyBinding::kControl;
  if (modifiersStr.Find("accel") != -1) {
    // Get the accelerator key value from prefs, overriding the default.
    switch (Preferences::GetInt("ui.key.accelKey", 0)) {
      case nsIDOMKeyEvent::DOM_VK_META:
        modifierMask |= KeyBinding::kMeta;
        break;

      case nsIDOMKeyEvent::DOM_VK_ALT:
        modifierMask |= KeyBinding::kAlt;
        break;

      case nsIDOMKeyEvent::DOM_VK_CONTROL:
        modifierMask |= KeyBinding::kControl;
        break;

      default:
#ifdef XP_MACOSX
        modifierMask |= KeyBinding::kMeta;
#else
        modifierMask |= KeyBinding::kControl;
#endif
    }
  }

  return KeyBinding(key, modifierMask);
}

PRUint32
nsXULMenuitemAccessible::NativeRole()
{
  nsCOMPtr<nsIDOMXULContainerElement> xulContainer(do_QueryInterface(mContent));
  if (xulContainer)
    return nsIAccessibleRole::ROLE_PARENT_MENUITEM;

  if (mParent && mParent->Role() == nsIAccessibleRole::ROLE_COMBOBOX_LIST)
    return nsIAccessibleRole::ROLE_COMBOBOX_OPTION;

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                            nsGkAtoms::radio, eCaseMatters)) {
    return nsIAccessibleRole::ROLE_RADIO_MENU_ITEM;
  }

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                            nsGkAtoms::checkbox,
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

bool
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

PRUint8
nsXULMenuitemAccessible::ActionCount()
{
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULMenuitemAccessible: Widgets

bool
nsXULMenuitemAccessible::IsActiveWidget() const
{
  // Parent menu item is a widget, it's active when its popup is open.
  nsIContent* menuPopupContent = mContent->GetFirstChild();
  if (menuPopupContent) {
    nsMenuPopupFrame* menuPopupFrame =
      do_QueryFrame(menuPopupContent->GetPrimaryFrame());
    return menuPopupFrame && menuPopupFrame->IsOpen();
  }
  return false;
}

bool
nsXULMenuitemAccessible::AreItemsOperable() const
{
  // Parent menu item is a widget, its items are operable when its popup is open.
  nsIContent* menuPopupContent = mContent->GetFirstChild();
  if (menuPopupContent) {
    nsMenuPopupFrame* menuPopupFrame =
      do_QueryFrame(menuPopupContent->GetPrimaryFrame());
    return menuPopupFrame && menuPopupFrame->IsOpen();
  }
  return false;
}

nsAccessible*
nsXULMenuitemAccessible::ContainerWidget() const
{
  nsMenuFrame* menuFrame = do_QueryFrame(GetFrame());
  if (menuFrame) {
    nsMenuParent* menuParent = menuFrame->GetMenuParent();
    if (menuParent) {
      if (menuParent->IsMenuBar()) // menubar menu
        return mParent;

      // a menupoup or parent menu item
      if (menuParent->IsMenu())
        return mParent;

      // otherwise it's different kind of popups (like panel or tooltip), it
      // shouldn't be a real case.
    }
  }
  return nsnull;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULMenuSeparatorAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULMenuSeparatorAccessible::
  nsXULMenuSeparatorAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXULMenuitemAccessible(aContent, aShell)
{
}

PRUint64
nsXULMenuSeparatorAccessible::NativeState()
{
  // Isn't focusable, but can be offscreen/invisible -- only copy those states
  return nsXULMenuitemAccessible::NativeState() &
    (states::OFFSCREEN | states::INVISIBLE);
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

PRUint8
nsXULMenuSeparatorAccessible::ActionCount()
{
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULMenupopupAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULMenupopupAccessible::
  nsXULMenupopupAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXULSelectableAccessible(aContent, aShell)
{
  nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(GetFrame());
  if (menuPopupFrame && menuPopupFrame->IsMenu())
    mFlags |= eMenuPopupAccessible;

  // May be the anonymous <menupopup> inside <menulist> (a combobox)
  mSelectControl = do_QueryInterface(mContent->GetParent());
}

PRUint64
nsXULMenupopupAccessible::NativeState()
{
  PRUint64 state = nsAccessible::NativeState();

#ifdef DEBUG_A11Y
  // We are onscreen if our parent is active
  bool isActive = mContent->HasAttr(kNameSpaceID_None,
                                      nsGkAtoms::menuactive);
  if (!isActive) {
    nsAccessible* parent = Parent();
    if (!parent)
      return state;

    nsIContent *parentContent = parnet->GetContent();
    NS_ENSURE_TRUE(parentContent, state);

    isActive = parentContent->HasAttr(kNameSpaceID_None,
                                      nsGkAtoms::open);
  }

  NS_ASSERTION(isActive || states & states::INVISIBLE,
               "XULMenupopup doesn't have INVISIBLE when it's inactive");
#endif

  if (state & states::INVISIBLE)
    state |= states::OFFSCREEN | states::COLLAPSED;

  return state;
}

nsresult
nsXULMenupopupAccessible::GetNameInternal(nsAString& aName)
{
  nsIContent *content = mContent;
  while (content && aName.IsEmpty()) {
    content->GetAttr(kNameSpaceID_None, nsGkAtoms::label, aName);
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
      nsAccessible* grandParent = mParent->Parent();
      if (grandParent &&
          grandParent->Role() == nsIAccessibleRole::ROLE_AUTOCOMPLETE)
        return nsIAccessibleRole::ROLE_COMBOBOX_LIST;
    }
  }

  return nsIAccessibleRole::ROLE_MENUPOPUP;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULMenupopupAccessible: Widgets

bool
nsXULMenupopupAccessible::IsWidget() const
{
  return true;
}

bool
nsXULMenupopupAccessible::IsActiveWidget() const
{
  // If menupopup is a widget (the case of context menus) then active when open.
  nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(GetFrame());
  return menuPopupFrame && menuPopupFrame->IsOpen();
}

bool
nsXULMenupopupAccessible::AreItemsOperable() const
{
  nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(GetFrame());
  return menuPopupFrame && menuPopupFrame->IsOpen();
}

nsAccessible*
nsXULMenupopupAccessible::ContainerWidget() const
{
  nsDocAccessible* document = GetDocAccessible();

  nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(GetFrame());
  while (menuPopupFrame) {
    nsAccessible* menuPopup =
      document->GetAccessible(menuPopupFrame->GetContent());
    if (!menuPopup) // shouldn't be a real case
      return nsnull;

    nsMenuFrame* menuFrame = menuPopupFrame->GetParentMenu();
    if (!menuFrame) // context menu or popups
      return nsnull;

    nsMenuParent* menuParent = menuFrame->GetMenuParent();
    if (!menuParent) // menulist or menubutton
      return menuPopup->Parent();

    if (menuParent->IsMenuBar()) { // menubar menu
      nsMenuBarFrame* menuBarFrame = static_cast<nsMenuBarFrame*>(menuParent);
      return document->GetAccessible(menuBarFrame->GetContent());
    }

    // different kind of popups like panel or tooltip
    if (!menuParent->IsMenu())
      return nsnull;

    menuPopupFrame = static_cast<nsMenuPopupFrame*>(menuParent);
  }

  NS_NOTREACHED("Shouldn't be a real case.");
  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULMenubarAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULMenubarAccessible::
  nsXULMenubarAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

PRUint64
nsXULMenubarAccessible::NativeState()
{
  PRUint64 state = nsAccessible::NativeState();

  // Menu bar itself is not actually focusable
  state &= ~states::FOCUSABLE;
  return state;
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

////////////////////////////////////////////////////////////////////////////////
// nsXULMenubarAccessible: Widgets

bool
nsXULMenubarAccessible::IsActiveWidget() const
{
  nsMenuBarFrame* menuBarFrame = do_QueryFrame(GetFrame());
  return menuBarFrame && menuBarFrame->IsActive();
}

bool
nsXULMenubarAccessible::AreItemsOperable() const
{
  return true;
}

nsAccessible*
nsXULMenubarAccessible::CurrentItem()
{
  nsMenuBarFrame* menuBarFrame = do_QueryFrame(GetFrame());
  if (menuBarFrame) {
    nsMenuFrame* menuFrame = menuBarFrame->GetCurrentMenuItem();
    if (menuFrame) {
      nsIContent* menuItemNode = menuFrame->GetContent();
      return GetAccService()->GetAccessible(menuItemNode);
    }
  }
  return nsnull;
}
