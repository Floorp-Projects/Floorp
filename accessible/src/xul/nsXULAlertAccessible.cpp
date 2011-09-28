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
 * The Initial Developer of the Original Code is IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: Aaron Leventhal (aleventh@us.ibm.com)
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

#include "nsXULAlertAccessible.h"

#include "States.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsXULAlertAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULAlertAccessible::
  nsXULAlertAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsXULAlertAccessible, nsAccessible)

PRUint32
nsXULAlertAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_ALERT;
}

PRUint64
nsXULAlertAccessible::NativeState()
{
  return nsAccessible::NativeState() | states::ALERT;
}

NS_IMETHODIMP
nsXULAlertAccessible::GetName(nsAString& aName)
{
  // Screen readers need to read contents of alert, not the accessible name.
  // If we have both some screen readers will read the alert twice.
  aName.Truncate();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Widgets

bool
nsXULAlertAccessible::IsWidget() const
{
  return true;
}

nsAccessible*
nsXULAlertAccessible::ContainerWidget() const
{
  // If a part of colorpicker widget.
  if (mParent && mParent->IsMenuButton())
    return mParent;
  return nsnull;
}
