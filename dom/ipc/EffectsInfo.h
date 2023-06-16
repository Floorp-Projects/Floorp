/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EffectsInfo_h
#define mozilla_dom_EffectsInfo_h

#include "nsRect.h"

namespace mozilla::dom {

/**
 * An EffectsInfo contains information for a remote browser about the graphical
 * effects that are being applied to it by ancestor browsers in different
 * processes.
 */
class EffectsInfo {
 public:
  EffectsInfo() { *this = EffectsInfo::FullyHidden(); }

  static EffectsInfo VisibleWithinRect(
      const nsRect& aVisibleRect, const Scale2D& aRasterScale,
      const ParentLayerToScreenScale2D& aTransformToAncestorScale) {
    return EffectsInfo{aVisibleRect, aRasterScale, aTransformToAncestorScale};
  }
  static EffectsInfo FullyHidden() {
    return EffectsInfo{nsRect(), Scale2D(), ParentLayerToScreenScale2D()};
  }

  bool operator==(const EffectsInfo& aOther) const {
    return mVisibleRect == aOther.mVisibleRect &&
           mRasterScale == aOther.mRasterScale &&
           mTransformToAncestorScale == aOther.mTransformToAncestorScale;
  }
  bool operator!=(const EffectsInfo& aOther) const {
    return !(*this == aOther);
  }

  bool IsVisible() const { return !mVisibleRect.IsEmpty(); }

  // The visible rect of this browser relative to the root frame. If this is
  // empty then the browser can be considered invisible.
  nsRect mVisibleRect;
  // The desired scale factors to apply to rasterized content to match
  // transforms applied in ancestor browsers. This gets propagated into the
  // scale in StackingContextHelper.
  Scale2D mRasterScale;
  // TransformToAncestorScale to be set on FrameMetrics. It includes CSS
  // transform scales and cumulative presshell resolution.
  ParentLayerToScreenScale2D mTransformToAncestorScale;
  // The difference between mScaleX/Y and mTransformToAncestorScale is the way
  // that CSS transforms contribute to the scale. mTransformToAncestorScale
  // includes the exact scale factors of the combined CSS transform whereas
  // mScaleX/Y tries to take into account animating transform scales by picking
  // a larger scale so that we don't have to re-rasterize every frame but rather
  // we can just scale down  content rasterized on a previous frame.

  // If you add new fields here, you must also update operator== and
  // TabMessageUtils.

 private:
  EffectsInfo(const nsRect& aVisibleRect, const Scale2D& aRasterScale,
              const ParentLayerToScreenScale2D& aTransformToAncestorScale)
      : mVisibleRect(aVisibleRect),
        mRasterScale(aRasterScale),
        mTransformToAncestorScale(aTransformToAncestorScale) {}
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_EffectsInfo_h
