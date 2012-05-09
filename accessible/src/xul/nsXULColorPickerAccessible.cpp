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
 *   Author: John Gaunt (jgaunt@netscape.com)
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
