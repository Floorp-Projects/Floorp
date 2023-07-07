/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleImage.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED(xpcAccessibleImage, xpcAccessibleGeneric,
                            nsIAccessibleImage)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleImage

NS_IMETHODIMP
xpcAccessibleImage::GetImagePosition(uint32_t aCoordType, int32_t* aX,
                                     int32_t* aY) {
  NS_ENSURE_ARG_POINTER(aX);
  *aX = 0;
  NS_ENSURE_ARG_POINTER(aY);
  *aY = 0;

  if (!mIntl) return NS_ERROR_FAILURE;

  LayoutDeviceIntPoint point = mIntl->Position(aCoordType);
  *aX = point.x;
  *aY = point.y;
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleImage::GetImageSize(int32_t* aWidth, int32_t* aHeight) {
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;

  if (!mIntl) return NS_ERROR_FAILURE;

  LayoutDeviceIntSize size = mIntl->Size();
  *aWidth = size.width;
  *aHeight = size.height;
  return NS_OK;
}
