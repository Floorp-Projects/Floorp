/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_VIEWPORT_META_DATA_H_
#define DOM_VIEWPORT_META_DATA_H_

#include "nsString.h"
#include "nsAString.h"

namespace mozilla {

namespace dom {
struct ViewportMetaData {
  // https://drafts.csswg.org/css-device-adapt/#meta-properties
  nsString mWidth;
  nsString mHeight;
  nsString mInitialScale;
  nsString mMinimumScale;
  nsString mMaximumScale;
  nsString mUserScalable;
  nsString mViewportFit;

  bool operator==(const ViewportMetaData& aOther) const {
    return mWidth == aOther.mWidth && mHeight == aOther.mHeight &&
           mInitialScale == aOther.mInitialScale &&
           mMinimumScale == aOther.mMinimumScale &&
           mMaximumScale == aOther.mMaximumScale &&
           mUserScalable == aOther.mUserScalable &&
           mViewportFit == aOther.mViewportFit;
  }
  bool operator!=(const ViewportMetaData& aOther) const {
    return !(*this == aOther);
  }

  ViewportMetaData() = default;
  /* Process viewport META data. This gives us information for the scale
   * and zoom of a page on mobile devices. We stick the information in
   * the document header and use it later on after rendering.
   *
   * See Bug #436083
   */
  explicit ViewportMetaData(const nsAString& aViewportInfo);
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_VIEWPORT_META_DATA_H_
