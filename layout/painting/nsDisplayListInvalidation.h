/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDISPLAYLISTINVALIDATION_H_
#define NSDISPLAYLISTINVALIDATION_H_

#include "mozilla/Attributes.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "ImgDrawResult.h"
#include "nsRect.h"
#include "nsColor.h"
#include "gfxRect.h"
#include "mozilla/gfx/MatrixFwd.h"

namespace mozilla {
class nsDisplayBackgroundImage;
class nsCharClipDisplayItem;
class nsDisplayItem;
class nsDisplayListBuilder;
class nsDisplayTableItem;
class nsDisplayThemedBackground;
class nsDisplayEffectsBase;
class nsDisplayMasksAndClipPaths;
class nsDisplayFilters;

namespace gfx {
struct sRGBColor;
}

/**
 * This stores the geometry of an nsDisplayItem, and the area
 * that will be affected when painting the item.
 *
 * It is used to retain information about display items so they
 * can be compared against new display items in the next paint.
 */
class nsDisplayItemGeometry {
 public:
  nsDisplayItemGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder);
  virtual ~nsDisplayItemGeometry();

  /**
   * Compute the area required to be invalidated if this
   * display item is removed.
   */
  const nsRect& ComputeInvalidationRegion() { return mBounds; }

  /**
   * Shifts all retained areas of the nsDisplayItemGeometry by the given offset.
   *
   * This is used to compensate for scrolling, since the destination buffer
   * can scroll without requiring a full repaint.
   *
   * @param aOffset Offset to shift by.
   */
  virtual void MoveBy(const nsPoint& aOffset) { mBounds.MoveBy(aOffset); }

  /**
   * Bounds of the display item
   */
  nsRect mBounds;
};

/**
 * A default geometry implementation, used by nsDisplayItem. Retains
 * and compares the bounds, and border rect.
 *
 * This should be sufficient for the majority of display items.
 */
class nsDisplayItemGenericGeometry : public nsDisplayItemGeometry {
 public:
  nsDisplayItemGenericGeometry(nsDisplayItem* aItem,
                               nsDisplayListBuilder* aBuilder);

  void MoveBy(const nsPoint& aOffset) override;

  nsRect mBorderRect;
};

bool ShouldSyncDecodeImages(nsDisplayListBuilder* aBuilder);

nsDisplayItemGeometry* GetPreviousGeometry(nsDisplayItem*);

class nsDisplayItemBoundsGeometry : public nsDisplayItemGeometry {
 public:
  nsDisplayItemBoundsGeometry(nsDisplayItem* aItem,
                              nsDisplayListBuilder* aBuilder);

  bool mHasRoundedCorners;
};

class nsDisplayBorderGeometry : public nsDisplayItemGeometry {
 public:
  nsDisplayBorderGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder);
};

class nsDisplayBackgroundGeometry : public nsDisplayItemGeometry {
 public:
  nsDisplayBackgroundGeometry(nsDisplayBackgroundImage* aItem,
                              nsDisplayListBuilder* aBuilder);

  void MoveBy(const nsPoint& aOffset) override;

  nsRect mPositioningArea;
  nsRect mDestRect;
};

class nsDisplayThemedBackgroundGeometry : public nsDisplayItemGeometry {
 public:
  nsDisplayThemedBackgroundGeometry(nsDisplayThemedBackground* aItem,
                                    nsDisplayListBuilder* aBuilder);

  void MoveBy(const nsPoint& aOffset) override;

  nsRect mPositioningArea;
  bool mWindowIsActive;
};

class nsDisplayTreeBodyGeometry : public nsDisplayItemGenericGeometry {
 public:
  nsDisplayTreeBodyGeometry(nsDisplayItem* aItem,
                            nsDisplayListBuilder* aBuilder,
                            bool aWindowIsActive)
      : nsDisplayItemGenericGeometry(aItem, aBuilder),
        mWindowIsActive(aWindowIsActive) {}

  bool mWindowIsActive = false;
};

class nsDisplayBoxShadowInnerGeometry : public nsDisplayItemGeometry {
 public:
  nsDisplayBoxShadowInnerGeometry(nsDisplayItem* aItem,
                                  nsDisplayListBuilder* aBuilder);

  void MoveBy(const nsPoint& aOffset) override;

  nsRect mPaddingRect;
};

class nsDisplaySolidColorGeometry : public nsDisplayItemBoundsGeometry {
 public:
  nsDisplaySolidColorGeometry(nsDisplayItem* aItem,
                              nsDisplayListBuilder* aBuilder, nscolor aColor)
      : nsDisplayItemBoundsGeometry(aItem, aBuilder), mColor(aColor) {}

  nscolor mColor;
};

class nsDisplaySolidColorRegionGeometry : public nsDisplayItemBoundsGeometry {
 public:
  nsDisplaySolidColorRegionGeometry(nsDisplayItem* aItem,
                                    nsDisplayListBuilder* aBuilder,
                                    const nsRegion& aRegion,
                                    mozilla::gfx::sRGBColor aColor)
      : nsDisplayItemBoundsGeometry(aItem, aBuilder),
        mRegion(aRegion),
        mColor(aColor) {}

  void MoveBy(const nsPoint& aOffset) override;

  nsRegion mRegion;
  mozilla::gfx::sRGBColor mColor;
};

class nsDisplaySVGEffectGeometry : public nsDisplayItemGeometry {
 public:
  nsDisplaySVGEffectGeometry(nsDisplayEffectsBase* aItem,
                             nsDisplayListBuilder* aBuilder);

  void MoveBy(const nsPoint& aOffset) override;

  gfxRect mBBox;
  gfxPoint mUserSpaceOffset;
  nsPoint mFrameOffsetToReferenceFrame;
  bool mHandleOpacity;
};

class nsDisplayMasksAndClipPathsGeometry : public nsDisplaySVGEffectGeometry {
 public:
  nsDisplayMasksAndClipPathsGeometry(nsDisplayMasksAndClipPaths* aItem,
                                     nsDisplayListBuilder* aBuilder);

  nsTArray<nsRect> mDestRects;
};

class nsDisplayFiltersGeometry : public nsDisplaySVGEffectGeometry {
 public:
  nsDisplayFiltersGeometry(nsDisplayFilters* aItem,
                           nsDisplayListBuilder* aBuilder);
};

class nsDisplayTableItemGeometry : public nsDisplayItemGenericGeometry {
 public:
  nsDisplayTableItemGeometry(nsDisplayTableItem* aItem,
                             nsDisplayListBuilder* aBuilder,
                             const nsPoint& aFrameOffsetToViewport);

  nsPoint mFrameOffsetToViewport;
};

class nsDisplayOpacityGeometry : public nsDisplayItemGenericGeometry {
 public:
  nsDisplayOpacityGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder,
                           float aOpacity)
      : nsDisplayItemGenericGeometry(aItem, aBuilder), mOpacity(aOpacity) {}

  float mOpacity;
};

class nsDisplayTransformGeometry : public nsDisplayItemGeometry {
 public:
  nsDisplayTransformGeometry(nsDisplayItem* aItem,
                             nsDisplayListBuilder* aBuilder,
                             const mozilla::gfx::Matrix4x4Flagged& aTransform,
                             int32_t aAppUnitsPerDevPixel)
      : nsDisplayItemGeometry(aItem, aBuilder),
        mTransform(aTransform),
        mAppUnitsPerDevPixel(aAppUnitsPerDevPixel) {}

  void MoveBy(const nsPoint& aOffset) override {
    nsDisplayItemGeometry::MoveBy(aOffset);
    mTransform.PostTranslate(
        NSAppUnitsToFloatPixels(aOffset.x, mAppUnitsPerDevPixel),
        NSAppUnitsToFloatPixels(aOffset.y, mAppUnitsPerDevPixel), 0.0f);
  }

  mozilla::gfx::Matrix4x4Flagged mTransform;
  int32_t mAppUnitsPerDevPixel;
};

}  // namespace mozilla

#endif /*NSDISPLAYLISTINVALIDATION_H_*/
