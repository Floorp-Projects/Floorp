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

#include "nsIComponentManager.h"
#include "nsServiceManagerUtils.h"

nsApplicationAccessible::nsApplicationAccessible():
    nsAccessibleWrap(nsnull, nsnull), mChildren(nsnull)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(nsApplicationAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsApplicationAccessible,
                                                  nsAccessible)

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  tmp->mChildren->Enumerate(getter_AddRefs(enumerator));

  nsCOMPtr<nsIWeakReference> childWeakRef;
  nsCOMPtr<nsIAccessible> accessible;

  PRBool hasMoreElements;
  while(NS_SUCCEEDED(enumerator->HasMoreElements(&hasMoreElements))
        && hasMoreElements) {

    enumerator->GetNext(getter_AddRefs(childWeakRef));
    accessible = do_QueryReferent(childWeakRef);
    if (accessible) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "nsApplicationAccessible child");
      cb.NoteXPCOMChild(accessible);
    }
  }
  
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsApplicationAccessible,
                                                nsAccessible)
  tmp->mChildren->Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsApplicationAccessible)
NS_INTERFACE_MAP_END_INHERITING(nsAccessible)

NS_IMPL_ADDREF_INHERITED(nsApplicationAccessible, nsAccessible)
NS_IMPL_RELEASE_INHERITED(nsApplicationAccessible, nsAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessNode

nsresult
nsApplicationAccessible::Init()
{
  nsresult rv;
  mChildren = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  return rv;
}

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
nsApplicationAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_APP_ROOT;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetFinalRole(PRUint32 *aFinalRole)
{
  return GetRole(aFinalRole);
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

NS_IMETHODIMP
nsApplicationAccessible::GetParent(nsIAccessible **aParent)
{
  *aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetChildAt(PRInt32 aChildNum, nsIAccessible **aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);
  *aChild = nsnull;

  PRUint32 count = 0;
  nsresult rv = NS_OK;

  if (mChildren) {
    rv = mChildren->GetLength(&count);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aChildNum >= static_cast<PRInt32>(count) || count == 0)
    return NS_ERROR_INVALID_ARG;

  if (aChildNum < 0)
    aChildNum = count - 1;

  nsCOMPtr<nsIWeakReference> childWeakRef;
  rv = mChildren->QueryElementAt(aChildNum, NS_GET_IID(nsIWeakReference),
                                 getter_AddRefs(childWeakRef));
  NS_ENSURE_SUCCESS(rv, rv);

  if (childWeakRef) {
    nsCOMPtr<nsIAccessible> childAcc(do_QueryReferent(childWeakRef));
    NS_IF_ADDREF(*aChild = childAcc);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetNextSibling(nsIAccessible **aNextSibling)
{
  NS_ENSURE_ARG_POINTER(aNextSibling);

  *aNextSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetPreviousSibling(nsIAccessible **aPreviousSibling)
{
  NS_ENSURE_ARG_POINTER(aPreviousSibling);

  *aPreviousSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetIndexInParent(PRInt32 *aIndexInParent)
{
  NS_ENSURE_ARG_POINTER(aIndexInParent);

  *aIndexInParent = -1;
  return NS_OK;
}

void
nsApplicationAccessible::CacheChildren()
{
  if (!mChildren) {
    mAccChildCount = eChildCountUninitialized;
    return;
  }

  if (mAccChildCount == eChildCountUninitialized) {
    mAccChildCount = 0;// Prevent reentry
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    mChildren->Enumerate(getter_AddRefs(enumerator));

    nsCOMPtr<nsIWeakReference> childWeakRef;
    nsCOMPtr<nsIAccessible> accessible;
    nsCOMPtr<nsPIAccessible> previousAccessible;
    PRBool hasMoreElements;
    while(NS_SUCCEEDED(enumerator->HasMoreElements(&hasMoreElements))
          && hasMoreElements) {
      enumerator->GetNext(getter_AddRefs(childWeakRef));
      accessible = do_QueryReferent(childWeakRef);
      if (accessible) {
        if (previousAccessible)
          previousAccessible->SetNextSibling(accessible);
        else
          SetFirstChild(accessible);

        previousAccessible = do_QueryInterface(accessible);
        previousAccessible->SetParent(this);
      }
    }

    PRUint32 count = 0;
    mChildren->GetLength(&count);
    mAccChildCount = static_cast<PRInt32>(count);
  }
}

// nsApplicationAccessible

nsresult
nsApplicationAccessible::AddRootAccessible(nsIAccessible *aRootAccessible)
{
  NS_ENSURE_ARG_POINTER(aRootAccessible);

  // add by weak reference
  nsresult rv = mChildren->AppendElement(aRootAccessible, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  InvalidateChildren();
  return NS_OK;
}

nsresult
nsApplicationAccessible::RemoveRootAccessible(nsIAccessible *aRootAccessible)
{
  NS_ENSURE_ARG_POINTER(aRootAccessible);

  PRUint32 index = 0;

  // we must use weak ref to get the index
  nsCOMPtr<nsIWeakReference> weakPtr = do_GetWeakReference(aRootAccessible);
  nsresult rv = mChildren->IndexOf(0, weakPtr, &index);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mChildren->RemoveElementAt(index);
  NS_ENSURE_SUCCESS(rv, rv);

  InvalidateChildren();
  return NS_OK;
}

