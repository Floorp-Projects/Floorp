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
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#include "nsXULSliderAccessible.h"

#include "nsIDOMDocument.h"
#include "nsIDOMDocumentXBL.h"

// nsXULSliderAccessible

nsXULSliderAccessible::nsXULSliderAccessible(nsIDOMNode* aNode,
                                             nsIWeakReference* aShell) :
  nsAccessibleWrap(aNode, aShell)
{
}

// nsIAccessible

NS_IMETHODIMP
nsXULSliderAccessible::GetRole(PRUint32 *aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);

  *aRole = nsIAccessibleRole::ROLE_SLIDER;
  return NS_OK;
}

NS_IMETHODIMP
nsXULSliderAccessible::GetValue(nsAString& aValue)
{
  return GetSliderAttr(nsAccessibilityAtoms::curpos, aValue);
}

// nsIAccessibleValue

NS_IMETHODIMP
nsXULSliderAccessible::GetMaximumValue(double *aValue)
{
  nsresult rv = nsAccessibleWrap::GetMaximumValue(aValue);

  // ARIA redefined maximum value.
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  return GetSliderAttr(nsAccessibilityAtoms::maxpos, aValue);
}

NS_IMETHODIMP
nsXULSliderAccessible::GetMinimumValue(double *aValue)
{
  nsresult rv = nsAccessibleWrap::GetMinimumValue(aValue);

  // ARIA redefined minmum value.
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  return GetSliderAttr(nsAccessibilityAtoms::minpos, aValue);
}

NS_IMETHODIMP
nsXULSliderAccessible::GetMinimumIncrement(double *aValue)
{
  nsresult rv = nsAccessibleWrap::GetMinimumIncrement(aValue);

  // ARIA redefined minimum increment value.
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  return GetSliderAttr(nsAccessibilityAtoms::increment, aValue);
}

NS_IMETHODIMP
nsXULSliderAccessible::GetCurrentValue(double *aValue)
{
  nsresult rv = nsAccessibleWrap::GetCurrentValue(aValue);

  // ARIA redefined current value.
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  return GetSliderAttr(nsAccessibilityAtoms::curpos, aValue);
}

NS_IMETHODIMP
nsXULSliderAccessible::SetCurrentValue(double aValue)
{
  nsresult rv = nsAccessibleWrap::SetCurrentValue(aValue);

  // ARIA redefined current value.
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  return SetSliderAttr(nsAccessibilityAtoms::curpos, aValue);
}

// nsPIAccessible
NS_IMETHODIMP
nsXULSliderAccessible::GetAllowsAnonChildAccessibles(PRBool *aAllowsAnonChildren)
{
  NS_ENSURE_ARG_POINTER(aAllowsAnonChildren);

  // Allow anonymous xul:thumb inside xul:slider.
  *aAllowsAnonChildren = PR_TRUE;
  return NS_OK;
}

// Utils

already_AddRefed<nsIContent>
nsXULSliderAccessible::GetSliderNode()
{
  if (!mDOMNode)
    return nsnull;

  if (!mSliderNode) {
    nsCOMPtr<nsIDOMDocument> document;
    mDOMNode->GetOwnerDocument(getter_AddRefs(document));
    if (!document)
      return nsnull;

    nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(document));
    if (!xblDoc)
      return nsnull;

    // XXX: we depend on anonymous content.
    nsCOMPtr<nsIDOMElement> domElm(do_QueryInterface(mDOMNode));
    if (!domElm)
      return nsnull;

    xblDoc->GetAnonymousElementByAttribute(domElm, NS_LITERAL_STRING("anonid"),
                                           NS_LITERAL_STRING("slider"),
                                           getter_AddRefs(mSliderNode));
  }

  nsIContent *sliderNode = nsnull;
  nsresult rv = CallQueryInterface(mSliderNode, &sliderNode);
  return NS_FAILED(rv) ? nsnull : sliderNode;
}

nsresult
nsXULSliderAccessible::GetSliderAttr(nsIAtom *aName, nsAString& aValue)
{
  aValue.Truncate();

  if (!mDOMNode)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> sliderNode(GetSliderNode());
  NS_ENSURE_STATE(sliderNode);

  sliderNode->GetAttr(kNameSpaceID_None, aName, aValue);
  return NS_OK;
}

nsresult
nsXULSliderAccessible::SetSliderAttr(nsIAtom *aName, const nsAString& aValue)
{
  if (!mDOMNode)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> sliderNode(GetSliderNode());
  NS_ENSURE_STATE(sliderNode);

  sliderNode->SetAttr(kNameSpaceID_None, aName, aValue, PR_TRUE);
  return NS_OK;
}

nsresult
nsXULSliderAccessible::GetSliderAttr(nsIAtom *aName, double *aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;

  nsAutoString value;
  nsresult rv = GetSliderAttr(aName, value);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 error = NS_OK;
  *aValue = value.ToFloat(&error);
  return error;
}

nsresult
nsXULSliderAccessible::SetSliderAttr(nsIAtom *aName, double aValue)
{
  nsAutoString value;
  value.AppendFloat(aValue);

  return SetSliderAttr(aName, value);
}


// nsXULThumbAccessible

nsXULThumbAccessible::nsXULThumbAccessible(nsIDOMNode* aNode,
                                           nsIWeakReference* aShell) :
  nsAccessibleWrap(aNode, aShell) {}

// nsIAccessible

NS_IMETHODIMP
nsXULThumbAccessible::GetRole(PRUint32 *aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);

  *aRole = nsIAccessibleRole::ROLE_INDICATOR;
  return NS_OK;
}

