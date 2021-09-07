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

  virtual bool InvalidateForSyncDecodeImages() const { return false; }

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

/**
 * nsImageGeometryMixin is a mixin for geometry items that draw images.
 * Geometry items that include this mixin can track drawing results and use
 * that information to inform invalidation decisions.
 *
 * This mixin uses CRTP; its template parameter should be the type of the class
 * that is inheriting from it. See nsDisplayItemGenericImageGeometry for an
 * example.
 */
template <typename T>
class nsImageGeometryMixin {
 public:
  nsImageGeometryMixin(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
      : mLastDrawResult(mozilla::image::ImgDrawResult::NOT_READY),
        mWaitingForPaint(false) {
    // Transfer state from the previous version of this geometry item.
    if (auto lastGeometry = static_cast<T*>(GetPreviousGeometry(aItem))) {
      mLastDrawResult = lastGeometry->mLastDrawResult;
      mWaitingForPaint = lastGeometry->mWaitingForPaint;
    }

    // If our display item is going to invalidate to trigger sync decoding of
    // images, mark ourselves as waiting for a paint. If we actually get
    // painted, UpdateDrawResult will get called, and we'll clear the flag.
    if (ShouldSyncDecodeImages(aBuilder) &&
        ShouldInvalidateToSyncDecodeImages()) {
      mWaitingForPaint = true;
    }
  }

  static void UpdateDrawResult(nsDisplayItem* aItem,
                               mozilla::image::ImgDrawResult aResult) {
    MOZ_ASSERT(aResult != mozilla::image::ImgDrawResult::NOT_SUPPORTED,
               "ImgDrawResult::NOT_SUPPORTED should be handled already!");

    if (auto lastGeometry = static_cast<T*>(GetPreviousGeometry(aItem))) {
      lastGeometry->mLastDrawResult = aResult;
      lastGeometry->mWaitingForPaint = false;
    }
  }

  bool ShouldInvalidateToSyncDecodeImages() const {
    if (mWaitingForPaint) {
      // We previously invalidated for sync decoding and haven't gotten painted
      // since them. This suggests that our display item is completely occluded
      // and there's no point in invalidating again - and because the reftest
      // harness takes a new snapshot every time we invalidate, doing so might
      // lead to an invalidation loop if we're in a reftest.
      return false;
    }

    if (mLastDrawResult == mozilla::image::ImgDrawResult::SUCCESS ||
        mLastDrawResult == mozilla::image::ImgDrawResult::BAD_IMAGE) {
      return false;
    }

    return true;
  }

 private:
  mozilla::image::ImgDrawResult mLastDrawResult;
  bool mWaitingForPaint;
};

/**
 * nsDisplayItemGenericImageGeometry is a generic geometry item class that
 * includes nsImageGeometryMixin.
 *
 * This should be sufficient for most display items that draw images.
 */
class nsDisplayItemGenericImageGeometry
    : public nsDisplayItemGenericGeometry,
      public nsImageGeometryMixin<nsDisplayItemGenericImageGeometry> {
 public:
  nsDisplayItemGenericImageGeometry(nsDisplayItem* aItem,
                                    nsDisplayListBuilder* aBuilder)
      : nsDisplayItemGenericGeometry(aItem, aBuilder),
        nsImageGeometryMixin(aItem, aBuilder) {}

  bool InvalidateForSyncDecodeImages() const override {
    return ShouldInvalidateToSyncDecodeImages();
  }
};

class nsDisplayItemBoundsGeometry : public nsDisplayItemGeometry {
 public:
  nsDisplayItemBoundsGeometry(nsDisplayItem* aItem,
                              nsDisplayListBuilder* aBuilder);

  bool mHasRoundedCorners;
};

class nsDisplayBorderGeometry
    : public nsDisplayItemGeometry,
      public nsImageGeometryMixin<nsDisplayBorderGeometry> {
 public:
  nsDisplayBorderGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder);

  bool InvalidateForSyncDecodeImages() const override {
    return ShouldInvalidateToSyncDecodeImages();
  }
};

class nsDisplayBackgroundGeometry
    : public nsDisplayItemGeometry,
      public nsImageGeometryMixin<nsDisplayBackgroundGeometry> {
 public:
  nsDisplayBackgroundGeometry(nsDisplayBackgroundImage* aItem,
                              nsDisplayListBuilder* aBuilder);

  void MoveBy(const nsPoint& aOffset) override;

  bool InvalidateForSyncDecodeImages() const override {
    return ShouldInvalidateToSyncDecodeImages();
  }

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

class nsDisplayTreeBodyGeometry
    : public nsDisplayItemGenericGeometry,
      public nsImageGeometryMixin<nsDisplayTreeBodyGeometry> {
 public:
  nsDisplayTreeBodyGeometry(nsDisplayItem* aItem,
                            nsDisplayListBuilder* aBuilder,
                            bool aWindowIsActive)
      : nsDisplayItemGenericGeometry(aItem, aBuilder),
        nsImageGeometryMixin(aItem, aBuilder),
        mWindowIsActive(aWindowIsActive) {}

  bool InvalidateForSyncDecodeImages() const override {
    return ShouldInvalidateToSyncDecodeImages();
  }

  bool mWindowIsActive = false;
};

class nsDisplayBoxShadowInnerGeometry : public nsDisplayItemGeometry {
 public:
  nsDisplayBoxShadowInnerGeometry(nsDisplayItem* aItem,
                                  nsDisplayListBuilder* aBuilder);

  void MoveBy(const nsPoint& aOffset) override;

  nsRect mPaddingRect;
};

class nsDisplayBoxShadowOuterGeometry : public nsDisplayItemGenericGeometry {
 public:
  nsDisplayBoxShadowOuterGeometry(nsDisplayItem* aItem,
                                  nsDisplayListBuilder* aBuilder,
                                  float aOpacity);

  float mOpacity;
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
  float mOpacity;
  bool mHandleOpacity;
};

class nsDisplayMasksAndClipPathsGeometry
    : public nsDisplaySVGEffectGeometry,
      public nsImageGeometryMixin<nsDisplayMasksAndClipPathsGeometry> {
 public:
  nsDisplayMasksAndClipPathsGeometry(nsDisplayMasksAndClipPaths* aItem,
                                     nsDisplayListBuilder* aBuilder);

  bool InvalidateForSyncDecodeImages() const override {
    return ShouldInvalidateToSyncDecodeImages();
  }

  nsTArray<nsRect> mDestRects;
};

class nsDisplayFiltersGeometry
    : public nsDisplaySVGEffectGeometry,
      public nsImageGeometryMixin<nsDisplayFiltersGeometry> {
 public:
  nsDisplayFiltersGeometry(nsDisplayFilters* aItem,
                           nsDisplayListBuilder* aBuilder);

  bool InvalidateForSyncDecodeImages() const override {
    return ShouldInvalidateToSyncDecodeImages();
  }
};

class nsDisplayTableItemGeometry
    : public nsDisplayItemGenericGeometry,
      public nsImageGeometryMixin<nsDisplayTableItemGeometry> {
 public:
  nsDisplayTableItemGeometry(nsDisplayTableItem* aItem,
                             nsDisplayListBuilder* aBuilder,
                             const nsPoint& aFrameOffsetToViewport);

  bool InvalidateForSyncDecodeImages() const override {
    return ShouldInvalidateToSyncDecodeImages();
  }

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
