/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
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
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "CAccessibleComponent.h"

#include "AccessibleComponent_i.c"

#include "nsAccessible.h"
#include "nsCoreUtils.h"
#include "nsWinUtils.h"
#include "States.h"

#include "nsString.h"

#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIDOMNSRGBAColor.h"

using namespace mozilla::a11y;

enum {
  IA2AlphaShift = 24,
  IA2RedShift = 16,
  IA2GreenShift = 8,
  IA2BlueShift = 0
};

// IUnknown

STDMETHODIMP
CAccessibleComponent::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleComponent == iid) {
    *ppv = static_cast<IAccessibleComponent*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// IAccessibleComponent

STDMETHODIMP
CAccessibleComponent::get_locationInParent(long *aX, long *aY)
{
__try {
  *aX = 0;
  *aY = 0;

  nsRefPtr<nsAccessible> acc(do_QueryObject(this));
  if (!acc)
    return E_FAIL;

  // If the object is not on any screen the returned position is (0,0).
  PRUint64 state = acc->State();
  if (state & states::INVISIBLE)
    return S_OK;

  PRInt32 x = 0, y = 0, width = 0, height = 0;
  nsresult rv = acc->GetBounds(&x, &y, &width, &height);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  nsCOMPtr<nsIAccessible> parentAcc;
  rv = acc->GetParent(getter_AddRefs(parentAcc));
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  // The coordinates of the returned position are relative to this object's
  // parent or relative to the screen on which this object is rendered if it
  // has no parent.
  if (!parentAcc) {
    *aX = x;
    *aY = y;
    return S_OK;
  }

  // The coordinates of the bounding box are given relative to the parent's
  // coordinate system.
  PRInt32 parentx = 0, parenty = 0;
  rv = acc->GetBounds(&parentx, &parenty, &width, &height);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aX = x - parentx;
  *aY = y - parenty;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleComponent::get_foreground(IA2Color *aForeground)
{
__try {
  return GetARGBValueFromCSSProperty(NS_LITERAL_STRING("color"), aForeground);
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_FAIL;
}

STDMETHODIMP
CAccessibleComponent::get_background(IA2Color *aBackground)
{
__try {
  return GetARGBValueFromCSSProperty(NS_LITERAL_STRING("background-color"),
                                     aBackground);
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_FAIL;
}

HRESULT
CAccessibleComponent::GetARGBValueFromCSSProperty(const nsAString& aPropName,
                                                  IA2Color *aColorValue)
{
__try {
  *aColorValue = 0;

  nsRefPtr<nsAccessible> acc(do_QueryObject(this));
  if (acc->IsDefunct())
    return E_FAIL;

  nsCOMPtr<nsIDOMCSSStyleDeclaration> styleDecl =
    nsWinUtils::GetComputedStyleDeclaration(acc->GetContent());
  NS_ENSURE_STATE(styleDecl);

  nsCOMPtr<nsIDOMCSSValue> cssGenericValue;
  styleDecl->GetPropertyCSSValue(aPropName, getter_AddRefs(cssGenericValue));

  nsCOMPtr<nsIDOMCSSPrimitiveValue> cssValue =
    do_QueryInterface(cssGenericValue);
  if (!cssValue)
    return E_FAIL;

  nsCOMPtr<nsIDOMRGBColor> rgbColor;
  nsresult rv = cssValue->GetRGBColorValue(getter_AddRefs(rgbColor));
  if (NS_FAILED(rv) || !rgbColor)
    return GetHRESULT(rv);

  nsCOMPtr<nsIDOMNSRGBAColor> rgbaColor(do_QueryInterface(rgbColor));
  if (!rgbaColor)
    return GetHRESULT(rv);

  // get alpha
  nsCOMPtr<nsIDOMCSSPrimitiveValue> alphaValue;
  rv = rgbaColor->GetAlpha(getter_AddRefs(alphaValue));
  if (NS_FAILED(rv) || !alphaValue)
    return GetHRESULT(rv);

  float alpha = 0.0;
  rv = alphaValue->GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, &alpha);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  // get red
  nsCOMPtr<nsIDOMCSSPrimitiveValue> redValue;
  rv = rgbaColor->GetRed(getter_AddRefs(redValue));
  if (NS_FAILED(rv) || !redValue)
    return GetHRESULT(rv);

  float red = 0.0;
  rv = redValue->GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, &red);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  // get green
  nsCOMPtr<nsIDOMCSSPrimitiveValue> greenValue;
  rv = rgbaColor->GetGreen(getter_AddRefs(greenValue));
  if (NS_FAILED(rv) || !greenValue)
    return GetHRESULT(rv);

  float green = 0.0;
  rv = greenValue->GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, &green);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  // get blue
  nsCOMPtr<nsIDOMCSSPrimitiveValue> blueValue;
  rv = rgbaColor->GetBlue(getter_AddRefs(blueValue));
  if (NS_FAILED(rv) || !blueValue)
    return GetHRESULT(rv);

  float blue = 0.0;
  rv = blueValue->GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, &blue);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  // compose ARGB value
  *aColorValue = (((IA2Color) blue) << IA2BlueShift) |
                 (((IA2Color) green) << IA2GreenShift) |
                 (((IA2Color) red) << IA2RedShift) |
                 (((IA2Color) (alpha * 0xff)) << IA2AlphaShift);
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

