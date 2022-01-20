/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleImage.h"

#include "AccessibleImage_i.c"

#include "ImageAccessibleWrap.h"
#include "IUnknownImpl.h"
#include "nsIAccessibleTypes.h"

#include "nsString.h"

using namespace mozilla;
using namespace mozilla::a11y;

ImageAccessible* ia2AccessibleImage::ImageAcc() {
  AccessibleWrap* acc = static_cast<MsaaAccessible*>(this)->LocalAcc();
  return static_cast<ImageAccessible*>(acc);
}

// IUnknown
IMPL_IUNKNOWN_QUERY_HEAD(ia2AccessibleImage)
IMPL_IUNKNOWN_QUERY_IFACE(IAccessibleImage)
IMPL_IUNKNOWN_QUERY_TAIL_INHERITED(MsaaAccessible)

// IAccessibleImage

STDMETHODIMP
ia2AccessibleImage::get_description(BSTR* aDescription) {
  if (!aDescription) return E_INVALIDARG;

  *aDescription = nullptr;

  ImageAccessible* acc = ImageAcc();
  if (!acc) return CO_E_OBJNOTCONNECTED;

  nsAutoString description;
  acc->Name(description);
  if (description.IsEmpty()) return S_FALSE;

  *aDescription = ::SysAllocStringLen(description.get(), description.Length());
  return *aDescription ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
ia2AccessibleImage::get_imagePosition(enum IA2CoordinateType aCoordType,
                                      long* aX, long* aY) {
  if (!aX || !aY) return E_INVALIDARG;

  *aX = 0;
  *aY = 0;

  ImageAccessible* imageAcc = ImageAcc();
  if (!imageAcc) return CO_E_OBJNOTCONNECTED;

  uint32_t geckoCoordType =
      (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE)
          ? nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE
          : nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  nsIntPoint pos = imageAcc->Position(geckoCoordType);
  *aX = pos.x;
  *aY = pos.y;
  return S_OK;
}

STDMETHODIMP
ia2AccessibleImage::get_imageSize(long* aHeight, long* aWidth) {
  if (!aHeight || !aWidth) return E_INVALIDARG;

  *aHeight = 0;
  *aWidth = 0;

  ImageAccessible* imageAcc = ImageAcc();
  if (!imageAcc) return CO_E_OBJNOTCONNECTED;

  nsIntSize size = imageAcc->Size();
  *aHeight = size.width;
  *aWidth = size.height;
  return S_OK;
}
