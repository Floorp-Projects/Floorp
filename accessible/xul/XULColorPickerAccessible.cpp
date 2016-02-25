/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULColorPickerAccessible.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "DocAccessible.h"
#include "Role.h"
#include "States.h"

#include "nsIDOMElement.h"
#include "nsMenuPopupFrame.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULColorPickerTileAccessible
////////////////////////////////////////////////////////////////////////////////

XULColorPickerTileAccessible::
  XULColorPickerTileAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

////////////////////////////////////////////////////////////////////////////////
// XULColorPickerTileAccessible: Accessible

void
XULColorPickerTileAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::color, aValue);
}

role
XULColorPickerTileAccessible::NativeRole()
{
  return roles::PUSHBUTTON;
}

uint64_t
XULColorPickerTileAccessible::NativeState()
{
  uint64_t state = AccessibleWrap::NativeState();
  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::selected))
    state |= states::SELECTED;

  return state;
}

uint64_t
XULColorPickerTileAccessible::NativeInteractiveState() const
{
  return NativelyUnavailable() ?
    states::UNAVAILABLE : states::FOCUSABLE | states::SELECTABLE;
}

////////////////////////////////////////////////////////////////////////////////
// XULColorPickerTileAccessible: Widgets

Accessible*
XULColorPickerTileAccessible::ContainerWidget() const
{
  Accessible* parent = Parent();
  if (parent) {
    Accessible* grandParent = parent->Parent();
    if (grandParent && grandParent->IsMenuButton())
      return grandParent;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// XULColorPickerAccessible
////////////////////////////////////////////////////////////////////////////////

XULColorPickerAccessible::
  XULColorPickerAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULColorPickerTileAccessible(aContent, aDoc)
{
  mGenericTypes |= eMenuButton;
}

////////////////////////////////////////////////////////////////////////////////
// XULColorPickerAccessible: Accessible

uint64_t
XULColorPickerAccessible::NativeState()
{
  uint64_t state = AccessibleWrap::NativeState();
  return state | states::HASPOPUP;
}

role
XULColorPickerAccessible::NativeRole()
{
  return roles::BUTTONDROPDOWNGRID;
}

////////////////////////////////////////////////////////////////////////////////
// XULColorPickerAccessible: Widgets

bool
XULColorPickerAccessible::IsWidget() const
{
  return true;
}

bool
XULColorPickerAccessible::IsActiveWidget() const
{
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
XULColorPickerAccessible::AreItemsOperable() const
{
  Accessible* menuPopup = mChildren.SafeElementAt(0, nullptr);
  if (menuPopup) {
    nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(menuPopup->GetFrame());
    return menuPopupFrame && menuPopupFrame->IsOpen();
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// XULColorPickerAccessible: Accessible

bool
XULColorPickerAccessible::IsAcceptableChild(nsIContent* aEl) const
{
  nsAutoString role;
  nsCoreUtils::XBLBindingRole(aEl, role);
  return role.EqualsLiteral("xul:panel") &&
    aEl->AttrValueIs(kNameSpaceID_None, nsGkAtoms::noautofocus,
                     nsGkAtoms::_true, eCaseMatters);
}
