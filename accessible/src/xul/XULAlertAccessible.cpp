/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULAlertAccessible.h"

#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULAlertAccessible
////////////////////////////////////////////////////////////////////////////////

XULAlertAccessible::
  XULAlertAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(XULAlertAccessible, Accessible)

role
XULAlertAccessible::NativeRole()
{
  return roles::ALERT;
}

uint64_t
XULAlertAccessible::NativeState()
{
  return Accessible::NativeState() | states::ALERT;
}

ENameValueFlag
XULAlertAccessible::Name(nsString& aName)
{
  // Screen readers need to read contents of alert, not the accessible name.
  // If we have both some screen readers will read the alert twice.
  aName.Truncate();
  return eNameOK;
}

////////////////////////////////////////////////////////////////////////////////
// Widgets

bool
XULAlertAccessible::IsWidget() const
{
  return true;
}

Accessible*
XULAlertAccessible::ContainerWidget() const
{
  // If a part of colorpicker widget.
  if (mParent && mParent->IsMenuButton())
    return mParent;
  return nullptr;
}
