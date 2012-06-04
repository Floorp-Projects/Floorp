/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULColorPickerAccessible.h"

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
// nsXULColorPickerTileAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULColorPickerTileAccessible::
  nsXULColorPickerTileAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerTileAccessible: nsIAccessible

void
nsXULColorPickerTileAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::color, aValue);
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerTileAccessible: Accessible

role
nsXULColorPickerTileAccessible::NativeRole()
{
  return roles::PUSHBUTTON;
}

PRUint64
nsXULColorPickerTileAccessible::NativeState()
{
  PRUint64 state = AccessibleWrap::NativeState();
  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::selected))
    state |= states::SELECTED;

  return state;
}

PRUint64
nsXULColorPickerTileAccessible::NativeInteractiveState() const
{
  return NativelyUnavailable() ?
    states::UNAVAILABLE : states::FOCUSABLE | states::SELECTABLE;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerTileAccessible: Widgets

Accessible*
nsXULColorPickerTileAccessible::ContainerWidget() const
{
  Accessible* parent = Parent();
  if (parent) {
    Accessible* grandParent = parent->Parent();
    if (grandParent && grandParent->IsMenuButton())
      return grandParent;
  }
  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULColorPickerAccessible::
  nsXULColorPickerAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXULColorPickerTileAccessible(aContent, aDoc)
{
  mFlags |= eMenuButtonAccessible;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerAccessible: Accessible

PRUint64
nsXULColorPickerAccessible::NativeState()
{
  PRUint64 state = AccessibleWrap::NativeState();
  return state | states::HASPOPUP;
}

role
nsXULColorPickerAccessible::NativeRole()
{
  return roles::BUTTONDROPDOWNGRID;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerAccessible: Widgets

bool
nsXULColorPickerAccessible::IsWidget() const
{
  return true;
}

bool
nsXULColorPickerAccessible::IsActiveWidget() const
{
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
nsXULColorPickerAccessible::AreItemsOperable() const
{
  Accessible* menuPopup = mChildren.SafeElementAt(0, nsnull);
  if (menuPopup) {
    nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(menuPopup->GetFrame());
    return menuPopupFrame && menuPopupFrame->IsOpen();
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerAccessible: protected Accessible

void
nsXULColorPickerAccessible::CacheChildren()
{
  NS_ENSURE_TRUE(mDoc,);

  nsAccTreeWalker walker(mDoc, mContent, true);

  Accessible* child = nsnull;
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
