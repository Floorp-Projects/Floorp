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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "nsXFormsAccessible.h"

#include "nscore.h"
#include "nsServiceManagerUtils.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIXFormsUtilityService.h"

// nsXFormsAccessible

nsIXFormsUtilityService *nsXFormsAccessible::sXFormsService = nsnull;

nsXFormsAccessible::
nsXFormsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
  nsAccessibleWrap(aNode, aShell)
{
  if (!sXFormsService) {
    nsresult rv = CallGetService("@mozilla.org/xforms-utility-service;1",
                                 &sXFormsService);
    if (NS_FAILED(rv))
      NS_WARNING("No XForms utility service.");
  }
}

nsresult
nsXFormsAccessible::GetBoundChildElementValue(const nsAString& aTagName,
                                              nsAString& aValue)
{
  NS_ENSURE_TRUE(sXFormsService, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mDOMNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNodeList> nodes;
  nsresult rv = mDOMNode->GetChildNodes(getter_AddRefs(nodes));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 length;
  rv = nodes->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 index = 0; index < length; index++) {
    nsCOMPtr<nsIDOMNode> node;
    rv = nodes->Item(index, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContent> content = do_QueryInterface(node);
    NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

    if (content->NodeInfo()->Equals(aTagName) &&
        content->NodeInfo()->NamespaceEquals(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS))) {
      return sXFormsService->GetValue(node, aValue);
    }
  }

  aValue.Truncate();
  return NS_OK;
}

// nsIAccessible

NS_IMETHODIMP
nsXFormsAccessible::GetValue(nsAString& aValue)
{
  NS_ENSURE_TRUE(sXFormsService, NS_ERROR_FAILURE);
  return sXFormsService->GetValue(mDOMNode, aValue);
}

NS_IMETHODIMP
nsXFormsAccessible::GetState(PRUint32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  *aState = 0;

  NS_ENSURE_TRUE(sXFormsService, NS_ERROR_FAILURE);

  PRBool isRelevant = PR_FALSE;
  nsresult rv = sXFormsService->IsRelevant(mDOMNode, &isRelevant);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isReadonly = PR_FALSE;
  rv = sXFormsService->IsReadonly(mDOMNode, &isReadonly);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isRequired = PR_FALSE;
  rv = sXFormsService->IsRequired(mDOMNode, &isRequired);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isValid = PR_FALSE;
  rv = sXFormsService->IsValid(mDOMNode, &isValid);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsAccessible::GetState(aState);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!isRelevant)
    *aState |= STATE_UNAVAILABLE;

  if (isReadonly)
    *aState |= STATE_READONLY;

  if (isRequired)
    *aState |= STATE_REQUIRED;

  if (!isValid)
    *aState |= STATE_INVALID;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsAccessible::GetName(nsAString& aName)
{
  nsAutoString name;
  nsresult rv = GetTextFromRelationID(nsAccessibilityAtoms::labelledby, name);
  if (NS_SUCCEEDED(rv) && !name.IsEmpty()) {
    aName = name;
    return NS_OK;
  }

  // search the xforms:label element
  return GetBoundChildElementValue(NS_LITERAL_STRING("label"), aName);
}

NS_IMETHODIMP
nsXFormsAccessible::GetDescription(nsAString& aDescription)
{
  nsAutoString description;
  nsresult rv = GetTextFromRelationID(nsAccessibilityAtoms::describedby,
                                      description);

  if (NS_SUCCEEDED(rv) && !description.IsEmpty()) {
    aDescription = description;
    return NS_OK;
  }

  // search the xforms:hint element
  return GetBoundChildElementValue(NS_LITERAL_STRING("hint"), aDescription);
}

NS_IMETHODIMP
nsXFormsAccessible::GetAllowsAnonChildAccessibles(PRBool *aAllowsAnonChildren)
{
  NS_ENSURE_ARG_POINTER(aAllowsAnonChildren);

  *aAllowsAnonChildren = PR_FALSE;
  return NS_OK;
}

