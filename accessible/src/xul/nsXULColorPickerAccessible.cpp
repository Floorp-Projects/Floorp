/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULColorPickerAccessible.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "nsAccTreeWalker.h"
#include "nsCoreUtils.h"
#include "nsDocAccessible.h"
#include "Role.h"
#include "States.h"

#include "nsIDOMElement.h"
#include "nsMenuPopupFrame.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerTileAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULColorPickerTileAccessible::
  nsXULColorPickerTileAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsAccessibleWrap(aContent, aDoc)
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
// nsXULColorPickerTileAccessible: nsAccessible

role
nsXULColorPickerTileAccessible::NativeRole()
{
  return roles::PUSHBUTTON;
}

PRUint64
nsXULColorPickerTileAccessible::NativeState()
{
  PRUint64 state = nsAccessibleWrap::NativeState();
  if (!(state & states::UNAVAILABLE))
    state |= states::FOCUSABLE | states::SELECTABLE;

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::selected))
    state |= states::SELECTED;

  return state;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerTileAccessible: Widgets

nsAccessible*
nsXULColorPickerTileAccessible::ContainerWidget() const
{
  nsAccessible* parent = Parent();
  if (parent) {
    nsAccessible* grandParent = parent->Parent();
    if (grandParent && grandParent->IsMenuButton())
      return grandParent;
  }
  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULColorPickerAccessible::
  nsXULColorPickerAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXULColorPickerTileAccessible(aContent, aDoc)
{
  mFlags |= eMenuButtonAccessible;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerAccessible: nsAccessible

PRUint64
nsXULColorPickerAccessible::NativeState()
{
  // Possible states: focused, focusable, unavailable(disabled).

  // get focus and disable status from base class
  PRUint64 states = nsAccessibleWrap::NativeState();

  states |= states::FOCUSABLE | states::HASPOPUP;

  return states;
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
  nsAccessible* menuPopup = mChildren.SafeElementAt(0, nsnull);
  if (menuPopup) {
    nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(menuPopup->GetFrame());
    return menuPopupFrame && menuPopupFrame->IsOpen();
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerAccessible: protected nsAccessible

void
nsXULColorPickerAccessible::CacheChildren()
{
  NS_ENSURE_TRUE(mDoc,);

  nsAccTreeWalker walker(mDoc, mContent, true);

  nsAccessible* child = nsnull;
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
