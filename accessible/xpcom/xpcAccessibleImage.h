/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleImage_h_
#define mozilla_a11y_xpcAccessibleImage_h_

#include "nsIAccessibleImage.h"

namespace mozilla {
namespace a11y {

class xpcAccessibleImage : public nsIAccessibleImage
{
public:
  NS_IMETHOD GetImagePosition(uint32_t aCoordType,
                              int32_t* aX, int32_t* aY) MOZ_FINAL;
  NS_IMETHOD GetImageSize(int32_t* aWidth, int32_t* aHeight) MOZ_FINAL;

private:
  friend class ImageAccessible;

  xpcAccessibleImage() { }

  xpcAccessibleImage(const xpcAccessibleImage&) MOZ_DELETE;
  xpcAccessibleImage& operator =(const xpcAccessibleImage&) MOZ_DELETE;
};

} // namespace a11y
} // namespace mozilla

#endif
