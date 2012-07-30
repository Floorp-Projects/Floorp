/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULColorPickerAccessible.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "nsAccTreeWalker.h"
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
// XULColorPickerTileAccessible: nsIAccessible

void
XULColorPickerTileAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::color, aValue);
}

////////////////////////////////////////////////////////////////////////////////
// XULColorPickerTileAccessible: Accessible

role
XULColorPickerTileAccessible::NativeRole()
{
  return roles::PUSHBUTTON;
}

PRUint64
XULColorPickerTileAccessible::NativeState()
{
  PRUint64 state = AccessibleWrap::NativeState();
  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::selected))
    state |= states::SELECTED;

  return state;
}

PRUint64
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
  mFlags |= eMenuButtonAccessible;
}

////////////////////////////////////////////////////////////////////////////////
// XULColorPickerAccessible: Accessible

PRUint64
XULColorPickerAccessible::NativeState()
{
  PRUint64 state = AccessibleWrap::NativeState();
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
// XULColorPickerAccessible: protected Accessible

void
XULColorPickerAccessible::CacheChildren()
{
  NS_ENSURE_TRUE(mDoc,);

  nsAccTreeWalker walker(mDoc, mContent, true);

  Accessible* child = nullptr;
  while ((child = walker.NextChild())) {
    PRUint32 role = child->Role();

    // Get an accessible for menupopup or panel elements.
    if (role == roles::ALERT) {
      AppendChild(child);
      return;
    }

    // Unbind rejected accessibles from the document.
    Document()->UnbindFromDocument(child);
  }
}
