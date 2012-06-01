/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAlertAccessible.h"

#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsXULAlertAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULAlertAccessible::
  nsXULAlertAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsXULAlertAccessible, Accessible)

role
nsXULAlertAccessible::NativeRole()
{
  return roles::ALERT;
}

PRUint64
nsXULAlertAccessible::NativeState()
{
  return Accessible::NativeState() | states::ALERT;
}

ENameValueFlag
nsXULAlertAccessible::Name(nsString& aName)
{
  // Screen readers need to read contents of alert, not the accessible name.
  // If we have both some screen readers will read the alert twice.
  aName.Truncate();
  return eNameOK;
}

////////////////////////////////////////////////////////////////////////////////
// Widgets

bool
nsXULAlertAccessible::IsWidget() const
{
  return true;
}

Accessible*
nsXULAlertAccessible::ContainerWidget() const
{
  // If a part of colorpicker widget.
  if (mParent && mParent->IsMenuButton())
    return mParent;
  return nsnull;
}
