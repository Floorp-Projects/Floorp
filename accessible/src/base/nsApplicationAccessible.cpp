/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin <bolian.yin@sun.com>
 *   Ginn Chen <ginn.chen@sun.com>
 *   Alexander Surkov <surkov.alexander@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 
#include "nsApplicationAccessible.h"

#include "nsAccessibilityService.h"

#include "nsIComponentManager.h"
#include "nsServiceManagerUtils.h"

nsApplicationAccessible::nsApplicationAccessible() :
  nsAccessibleWrap(nsnull, nsnull)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED0(nsApplicationAccessible, nsAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

NS_IMETHODIMP
nsApplicationAccessible::GetName(nsAString& aName)
{
  aName.Truncate();

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID);

  NS_ASSERTION(bundleService, "String bundle service must be present!");
  NS_ENSURE_STATE(bundleService);

  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = bundleService->CreateBundle("chrome://branding/locale/brand.properties",
                                            getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLString appName;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                 getter_Copies(appName));
  if (NS_FAILED(rv) || appName.IsEmpty()) {
    NS_WARNING("brandShortName not found, using default app name");
    appName.AssignLiteral("Gecko based application");
  }

  aName.Assign(appName);
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetDescription(nsAString& aValue)
{
  aValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetRole(PRUint32 *aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);

  return GetRoleInternal(aRole);
}

NS_IMETHODIMP
nsApplicationAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  NS_ENSURE_ARG_POINTER(aState);
  *aState = 0;

  if (aExtraState)
    *aExtraState = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetParent(nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  return IsDefunct() ? NS_ERROR_FAILURE : NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessNode public methods

PRBool
nsApplicationAccessible::IsDefunct()
{
  return nsAccessibilityService::gIsShutdown;
}

nsresult
nsApplicationAccessible::Init()
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessible public methods

nsresult
nsApplicationAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_APP_ROOT;
  return NS_OK;
}

nsresult
nsApplicationAccessible::GetStateInternal(PRUint32 *aState,
                                          PRUint32 *aExtraState)
{
  *aState = 0;
  if (aExtraState)
    *aExtraState = 0;

  return NS_OK;
}

nsAccessible*
nsApplicationAccessible::GetParent()
{
  return nsnull;
}

void
nsApplicationAccessible::InvalidateChildren()
{
  // Do nothing because application children are kept updated by
  // AddRootAccessible() and RemoveRootAccessible() method calls.
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessible protected methods

void
nsApplicationAccessible::CacheChildren()
{
  // Nothing to do. Children are keeped up to dated by Add/RemoveRootAccessible
  // method calls.
}

nsIAccessible*
nsApplicationAccessible::GetSiblingAtOffset(PRInt32 aOffset, nsresult* aError)
{
  if (IsDefunct()) {
    if (aError)
      *aError = NS_ERROR_FAILURE;

    return nsnull;
  }

  if (aError)
    *aError = NS_OK; // fail peacefully

  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// Public methods

nsresult
nsApplicationAccessible::AddRootAccessible(nsIAccessible *aRootAccessible)
{
  NS_ENSURE_ARG_POINTER(aRootAccessible);

  nsRefPtr<nsAccessible> rootAcc =
    nsAccUtils::QueryObject<nsAccessible>(aRootAccessible);

  if (!mChildren.AppendElement(rootAcc))
    return NS_ERROR_FAILURE;

  rootAcc->SetParent(this);

  return NS_OK;
}

nsresult
nsApplicationAccessible::RemoveRootAccessible(nsIAccessible *aRootAccessible)
{
  NS_ENSURE_ARG_POINTER(aRootAccessible);

  // It's not needed to void root accessible parent because this method is
  // called on root accessible shutdown and its parent will be cleared
  // properly.
  return mChildren.RemoveElement(aRootAccessible) ? NS_OK : NS_ERROR_FAILURE;
}
