/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULMenuAccessible.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "Role.h"
#include "States.h"
#include "XULFormControlAccessible.h"

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
// nsXULMenuitemAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULMenuitemAccessible::
  nsXULMenuitemAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

PRUint64
nsXULMenuitemAccessible::NativeState()
{
  PRUint64 state = Accessible::NativeState();

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
  bool isComboboxOption = (Role() == roles::COMBOBOX_OPTION);
  if (isComboboxOption) {
    // Is selected?
    bool isSelected = false;
    nsCOMPtr<nsIDOMXULSelectControlItemElement>
      item(do_QueryInterface(mContent));
    NS_ENSURE_TRUE(item, state);
    item->GetSelected(&isSelected);

    // Is collapsed?
    bool isCollapsed = false;
    Accessible* parent = Parent();
    if (parent && parent->State() & states::INVISIBLE)
      isCollapsed = true;

    if (isSelected) {
      state |= states::SELECTED;

      // Selected and collapsed?
      if (isCollapsed) {
        // Set selected option offscreen/invisible according to combobox state
        Accessible* grandParent = parent->Parent();
        if (!grandParent)
          return state;
        NS_ASSERTION(grandParent->Role() == roles::COMBOBOX,
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

  Accessible* parentAcc = Parent();
  if (parentAcc) {
    if (parentAcc->NativeRole() == roles::MENUBAR) {
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

  nsIContent* keyElm = mContent->OwnerDoc()->GetElementById(keyElmId);
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

role
nsXULMenuitemAccessible::NativeRole()
{
  nsCOMPtr<nsIDOMXULContainerElement> xulContainer(do_QueryInterface(mContent));
  if (xulContainer)
    return roles::PARENT_MENUITEM;

  if (mParent && mParent->Role() == roles::COMBOBOX_LIST)
    return roles::COMBOBOX_OPTION;

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                            nsGkAtoms::radio, eCaseMatters)) 
    return roles::RADIO_MENU_ITEM;

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                            nsGkAtoms::checkbox,
                            eCaseMatters)) 
    return roles::CHECK_MENU_ITEM;

  return roles::MENUITEM;
}

PRInt32
nsXULMenuitemAccessible::GetLevelInternal()
{
  return nsAccUtils::GetLevelForXULContainerItem(mContent);
}

bool
nsXULMenuitemAccessible::CanHaveAnonChildren()
{
  // That indicates we don't walk anonymous children for menuitems
  return false;
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

Accessible*
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
  nsXULMenuSeparatorAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXULMenuitemAccessible(aContent, aDoc)
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

role
nsXULMenuSeparatorAccessible::NativeRole()
{
  return roles::SEPARATOR;
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
  nsXULMenupopupAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULSelectControlAccessible(aContent, aDoc)
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
  PRUint64 state = Accessible::NativeState();

#ifdef DEBUG
  // We are onscreen if our parent is active
  bool isActive = mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::menuactive);
  if (!isActive) {
    Accessible* parent = Parent();
    if (parent) {
      nsIContent* parentContent = parent->GetContent();
      if (parentContent)
        isActive = parentContent->HasAttr(kNameSpaceID_None, nsGkAtoms::open);
    }
  }

  NS_ASSERTION(isActive || (state & states::INVISIBLE),
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

role
nsXULMenupopupAccessible::NativeRole()
{
  // If accessible is not bound to the tree (this happens while children are
  // cached) return general role.
  if (mParent) {
    roles::Role role = mParent->Role();
    if (role == roles::COMBOBOX || role == roles::AUTOCOMPLETE)
      return roles::COMBOBOX_LIST;

    if (role == roles::PUSHBUTTON) {
      // Some widgets like the search bar have several popups, owned by buttons.
      Accessible* grandParent = mParent->Parent();
      if (grandParent && grandParent->Role() == roles::AUTOCOMPLETE)
        return roles::COMBOBOX_LIST;
    }
  }

  return roles::MENUPOPUP;
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

Accessible*
nsXULMenupopupAccessible::ContainerWidget() const
{
  DocAccessible* document = Document();

  nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(GetFrame());
  while (menuPopupFrame) {
    Accessible* menuPopup =
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
  nsXULMenubarAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

PRUint64
nsXULMenubarAccessible::NativeState()
{
  PRUint64 state = Accessible::NativeState();

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

role
nsXULMenubarAccessible::NativeRole()
{
  return roles::MENUBAR;
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

Accessible*
nsXULMenubarAccessible::CurrentItem()
{
  nsMenuBarFrame* menuBarFrame = do_QueryFrame(GetFrame());
  if (menuBarFrame) {
    nsMenuFrame* menuFrame = menuBarFrame->GetCurrentMenuItem();
    if (menuFrame) {
      nsIContent* menuItemNode = menuFrame->GetContent();
      return mDoc->GetAccessible(menuItemNode);
    }
  }
  return nsnull;
}

void
nsXULMenubarAccessible::SetCurrentItem(Accessible* aItem)
{
  NS_ERROR("nsXULMenubarAccessible::SetCurrentItem not implemented");
}
