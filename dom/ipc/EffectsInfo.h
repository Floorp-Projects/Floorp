/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EffectsInfo_h
#define mozilla_dom_EffectsInfo_h

#include "nsRect.h"
#include "Units.h"

namespace mozilla::dom {

/**
 * An EffectsInfo contains information for a remote browser about the graphical
 * effects that are being applied to it by ancestor browsers in different
 * processes.
 */
class EffectsInfo {
 public:
  EffectsInfo() = default;

  static EffectsInfo VisibleWithinRect(
      const Maybe<nsRect>& aVisibleRect, const Scale2D& aRasterScale,
      const ParentLayerToScreenScale2D& aTransformToAncestorScale) {
    return EffectsInfo{aVisibleRect, aRasterScale, aTransformToAncestorScale};
  }
  static EffectsInfo FullyHidden() { return {}; }

  bool operator==(const EffectsInfo& aOther) const {
    return mVisibleRect == aOther.mVisibleRect &&
           mRasterScale == aOther.mRasterScale &&
           mTransformToAncestorScale == aOther.mTransformToAncestorScale;
  }
  bool operator!=(const EffectsInfo& aOther) const {
    return !(*this == aOther);
  }

  bool IsVisible() const { return mVisibleRect.isSome(); }

  // The visible rect of this browser relative to the root frame. This might be
  // empty in cases where we might still be considered visible, like if we're
  // zero-size but inside the viewport.
  Maybe<nsRect> mVisibleRect;
  // The desired scale factors to apply to rasterized content to match
  // transforms applied in ancestor browsers. This gets propagated into the
  // scale in StackingContextHelper.
  Scale2D mRasterScale;
  // TransformToAncestorScale to be set on FrameMetrics. It includes CSS
  // transform scales and cumulative presshell resolution.
  ParentLayerToScreenScale2D mTransformToAncestorScale;

  // If you add new fields here, you must also update operator== and
  // TabMessageUtils.

 private:
  EffectsInfo(const Maybe<nsRect>& aVisibleRect, const Scale2D& aRasterScale,
              const ParentLayerToScreenScale2D& aTransformToAncestorScale)
      : mVisibleRect(aVisibleRect),
        mRasterScale(aRasterScale),
        mTransformToAncestorScale(aTransformToAncestorScale) {}
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_EffectsInfo_h
