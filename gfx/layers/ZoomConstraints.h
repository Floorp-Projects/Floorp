/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ZOOMCONSTRAINTS_H
#define GFX_ZOOMCONSTRAINTS_H

#include <iosfwd>

#include "Units.h"  // for CSSRect, CSSPixel, etc
#include "mozilla/Maybe.h"
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid

namespace mozilla {
namespace layers {

struct ZoomConstraints {
  bool mAllowZoom;
  bool mAllowDoubleTapZoom;
  CSSToParentLayerScale mMinZoom;
  CSSToParentLayerScale mMaxZoom;

  ZoomConstraints() : mAllowZoom(true), mAllowDoubleTapZoom(true) {
    MOZ_COUNT_CTOR(ZoomConstraints);
  }

  ZoomConstraints(bool aAllowZoom, bool aAllowDoubleTapZoom,
                  const CSSToParentLayerScale& aMinZoom,
                  const CSSToParentLayerScale& aMaxZoom)
      : mAllowZoom(aAllowZoom),
        mAllowDoubleTapZoom(aAllowDoubleTapZoom),
        mMinZoom(aMinZoom),
        mMaxZoom(aMaxZoom) {
    MOZ_COUNT_CTOR(ZoomConstraints);
  }

  ZoomConstraints(const ZoomConstraints& other)
      : mAllowZoom(other.mAllowZoom),
        mAllowDoubleTapZoom(other.mAllowDoubleTapZoom),
        mMinZoom(other.mMinZoom),
        mMaxZoom(other.mMaxZoom) {
    MOZ_COUNT_CTOR(ZoomConstraints);
  }

  MOZ_COUNTED_DTOR(ZoomConstraints)

  bool operator==(const ZoomConstraints& other) const {
    return mAllowZoom == other.mAllowZoom &&
           mAllowDoubleTapZoom == other.mAllowDoubleTapZoom &&
           mMinZoom == other.mMinZoom && mMaxZoom == other.mMaxZoom;
  }

  bool operator!=(const ZoomConstraints& other) const {
    return !(*this == other);
  }

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const ZoomConstraints& z);
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_ZOOMCONSTRAINTS_H */
