/*-*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDISPLAYLISTINVALIDATION_H_
#define NSDISPLAYLISTINVALIDATION_H_

#include "mozilla/Attributes.h"
#include "FrameLayerBuilder.h"
#include "imgIContainer.h"
#include "nsRect.h"
#include "nsColor.h"
#include "gfxRect.h"

class nsDisplayBackgroundImage;
class nsCharClipDisplayItem;
class nsDisplayItem;
class nsDisplayListBuilder;
class nsDisplaySVGEffects;
class nsDisplayTableItem;
class nsDisplayThemedBackground;

/**
 * This stores the geometry of an nsDisplayItem, and the area
 * that will be affected when painting the item.
 *
 * It is used to retain information about display items so they
 * can be compared against new display items in the next paint.
 */
class nsDisplayItemGeometry
{
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
  virtual void MoveBy(const nsPoint& aOffset)
  {
    mBounds.MoveBy(aOffset);
  }

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
class nsDisplayItemGenericGeometry : public nsDisplayItemGeometry
{
public:
  nsDisplayItemGenericGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder);

  virtual void MoveBy(const nsPoint& aOffset) override;

  nsRect mBorderRect;
};

bool ShouldSyncDecodeImages(nsDisplayListBuilder* aBuilder);

/**
 * nsImageGeometryMixin is a mixin for geometry items that draw images. Geometry
 * items that include this mixin can track drawing results and use that
 * information to inform invalidation decisions.
 *
 * This mixin uses CRTP; its template parameter should be the type of the class
 * that is inheriting from it. See nsDisplayItemGenericImageGeometry for an
 * example.
 */
template <typename T>
class nsImageGeometryMixin
{
public:
  nsImageGeometryMixin(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
    : mLastDrawResult(mozilla::image::DrawResult::NOT_READY)
    , mWaitingForPaint(false)
  {
    // Transfer state from the previous version of this geometry item.
    auto lastGeometry =
      static_cast<T*>(mozilla::FrameLayerBuilder::GetMostRecentGeometry(aItem));
    if (lastGeometry) {
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
                               mozilla::image::DrawResult aResult)
  {
    auto lastGeometry =
      static_cast<T*>(mozilla::FrameLayerBuilder::GetMostRecentGeometry(aItem));
    if (lastGeometry) {
      lastGeometry->mLastDrawResult = aResult;
      lastGeometry->mWaitingForPaint = false;
    }
  }

  bool ShouldInvalidateToSyncDecodeImages() const
  {
    if (mWaitingForPaint) {
      // We previously invalidated for sync decoding and haven't gotten painted
      // since them. This suggests that our display item is completely occluded
      // and there's no point in invalidating again - and because the reftest
      // harness takes a new snapshot every time we invalidate, doing so might
      // lead to an invalidation loop if we're in a reftest.
      return false;
    }

    if (mLastDrawResult == mozilla::image::DrawResult::SUCCESS ||
        mLastDrawResult == mozilla::image::DrawResult::BAD_IMAGE) {
      return false;
    }

    return true;
  }

private:
  mozilla::image::DrawResult mLastDrawResult;
  bool mWaitingForPaint;
};

/**
 * nsDisplayItemGenericImageGeometry is a generic geometry item class that
 * includes nsImageGeometryMixin.
 *
 * This should be sufficient for most display items that draw images.
 */
class nsDisplayItemGenericImageGeometry
  : public nsDisplayItemGenericGeometry
  , public nsImageGeometryMixin<nsDisplayItemGenericImageGeometry>
{
public:
  nsDisplayItemGenericImageGeometry(nsDisplayItem* aItem,
                                    nsDisplayListBuilder* aBuilder)
    : nsDisplayItemGenericGeometry(aItem, aBuilder)
    , nsImageGeometryMixin(aItem, aBuilder)
  { }
};

class nsDisplayItemBoundsGeometry : public nsDisplayItemGeometry
{
public:
  nsDisplayItemBoundsGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder);

  bool mHasRoundedCorners;
};

class nsDisplayBorderGeometry : public nsDisplayItemGeometry
{
public:
  nsDisplayBorderGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder);

  virtual void MoveBy(const nsPoint& aOffset) override;

  nsRect mContentRect;
};

class nsDisplayBackgroundGeometry
  : public nsDisplayItemGeometry
  , public nsImageGeometryMixin<nsDisplayBackgroundGeometry>
{
public:
  nsDisplayBackgroundGeometry(nsDisplayBackgroundImage* aItem, nsDisplayListBuilder* aBuilder);

  virtual void MoveBy(const nsPoint& aOffset) override;

  nsRect mPositioningArea;
};

class nsDisplayThemedBackgroundGeometry : public nsDisplayItemGeometry
{
public:
  nsDisplayThemedBackgroundGeometry(nsDisplayThemedBackground* aItem, nsDisplayListBuilder* aBuilder);

  virtual void MoveBy(const nsPoint& aOffset) override;

  nsRect mPositioningArea;
  bool mWindowIsActive;
};

class nsDisplayBoxShadowInnerGeometry : public nsDisplayItemGeometry
{
public:
  nsDisplayBoxShadowInnerGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder);
  
  virtual void MoveBy(const nsPoint& aOffset) override;

  nsRect mPaddingRect;
};

class nsDisplayBoxShadowOuterGeometry : public nsDisplayItemGenericGeometry
{
public:
  nsDisplayBoxShadowOuterGeometry(nsDisplayItem* aItem,
                                  nsDisplayListBuilder* aBuilder,
                                  float aOpacity);

  float mOpacity;
};

class nsDisplaySolidColorGeometry : public nsDisplayItemBoundsGeometry
{
public:
  nsDisplaySolidColorGeometry(nsDisplayItem* aItem,
                              nsDisplayListBuilder* aBuilder,
                              nscolor aColor)
    : nsDisplayItemBoundsGeometry(aItem, aBuilder)
    , mColor(aColor)
  { }

  nscolor mColor;
};

class nsDisplaySVGEffectsGeometry : public nsDisplayItemGeometry
{
public:
  nsDisplaySVGEffectsGeometry(nsDisplaySVGEffects* aItem, nsDisplayListBuilder* aBuilder);

  virtual void MoveBy(const nsPoint& aOffset) override;

  gfxRect mBBox;
  gfxPoint mUserSpaceOffset;
  nsPoint mFrameOffsetToReferenceFrame;
};

class nsCharClipGeometry : public nsDisplayItemGenericGeometry
{
public:
  nsCharClipGeometry(nsCharClipDisplayItem* aItem, nsDisplayListBuilder* aBuilder);

  nscoord mVisIStartEdge;
  nscoord mVisIEndEdge;
};

class nsDisplayTableItemGeometry
  : public nsDisplayItemGenericGeometry
  , public nsImageGeometryMixin<nsDisplayTableItemGeometry>
{
public:
  nsDisplayTableItemGeometry(nsDisplayTableItem* aItem,
                             nsDisplayListBuilder* aBuilder,
                             const nsPoint& aFrameOffsetToViewport);

  nsPoint mFrameOffsetToViewport;
};

#endif /*NSDISPLAYLISTINVALIDATION_H_*/
