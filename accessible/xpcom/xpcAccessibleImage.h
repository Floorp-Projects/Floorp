/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleImage_h_
#define mozilla_a11y_xpcAccessibleImage_h_

#include "nsIAccessibleImage.h"

#include "xpcAccessibleGeneric.h"

namespace mozilla {
namespace a11y {

class xpcAccessibleImage : public xpcAccessibleGeneric,
                           public nsIAccessibleImage {
 public:
  explicit xpcAccessibleImage(Accessible* aIntl)
      : xpcAccessibleGeneric(aIntl) {}

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD GetImagePosition(uint32_t aCoordType, int32_t* aX,
                              int32_t* aY) final;
  NS_IMETHOD GetImageSize(int32_t* aWidth, int32_t* aHeight) final;

 protected:
  virtual ~xpcAccessibleImage() {}

 private:
  ImageAccessible* Intl() {
    return mIntl->IsLocal() ? mIntl->AsLocal()->AsImage() : nullptr;
  }

  xpcAccessibleImage(const xpcAccessibleImage&) = delete;
  xpcAccessibleImage& operator=(const xpcAccessibleImage&) = delete;
};

}  // namespace a11y
}  // namespace mozilla

#endif
