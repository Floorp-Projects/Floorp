/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleImage.h"

#include "AccessibleImage_i.c"

#include "nsHTMLImageAccessibleWrap.h"
#include "nsHTMLImageAccessible.h"

#include "nsIAccessible.h"
#include "nsIAccessibleImage.h"
#include "nsIAccessibleTypes.h"
#include "nsAccessNodeWrap.h"

#include "nsString.h"

// IUnknown

STDMETHODIMP
ia2AccessibleImage::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleImage == iid) {
    *ppv = static_cast<IAccessibleImage*>(this);
    (static_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// IAccessibleImage

STDMETHODIMP
ia2AccessibleImage::get_description(BSTR* aDescription)
{
__try {
  *aDescription = NULL;

  nsHTMLImageAccessibleWrap* acc =
    static_cast<nsHTMLImageAccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString description;
  nsresult rv = acc->GetName(description);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (description.IsEmpty())
    return S_FALSE;

  *aDescription = ::SysAllocStringLen(description.get(), description.Length());
  return *aDescription ? S_OK : E_OUTOFMEMORY;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
ia2AccessibleImage::get_imagePosition(enum IA2CoordinateType aCoordType,
                                      long* aX,
                                      long* aY)
{
__try {
  *aX = 0;
  *aY = 0;

  nsHTMLImageAccessibleWrap* imageAcc =
    static_cast<nsHTMLImageAccessibleWrap*>(this);
  if (imageAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  PRUint32 geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;
  PRInt32 x = 0, y = 0;

  nsresult rv = imageAcc->GetImagePosition(geckoCoordType, &x, &y);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aX = x;
  *aY = y;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_FAIL;
}

STDMETHODIMP
ia2AccessibleImage::get_imageSize(long* aHeight, long* aWidth)
{
__try {
  *aHeight = 0;
  *aWidth = 0;

  nsHTMLImageAccessibleWrap* imageAcc =
    static_cast<nsHTMLImageAccessibleWrap*>(this);
  if (imageAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  PRInt32 width = 0, height = 0;
  nsresult rv = imageAcc->GetImageSize(&width, &height);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aHeight = width;
  *aWidth = height;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

