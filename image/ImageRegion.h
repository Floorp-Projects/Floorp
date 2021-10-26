/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_ImageRegion_h
#define mozilla_image_ImageRegion_h

#include "gfxMatrix.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "gfxTypes.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Types.h"
#include "nsSize.h"

namespace mozilla {
namespace image {

/**
 * An axis-aligned rectangle in tiled image space, with an optional sampling
 * restriction rect. The drawing code ensures that if a sampling restriction
 * rect is present, any pixels sampled during the drawing process are found
 * within that rect.
 *
 * The sampling restriction rect exists primarily for callers which perform
 * pixel snapping. Other callers should generally use one of the Create()
 * overloads.
 */
class ImageRegion {
  typedef mozilla::gfx::ExtendMode ExtendMode;

 public:
  static ImageRegion Empty() {
    return ImageRegion(gfxRect(), ExtendMode::CLAMP);
  }

  static ImageRegion Create(const gfxRect& aRect,
                            ExtendMode aExtendMode = ExtendMode::CLAMP) {
    return ImageRegion(aRect, aExtendMode);
  }

  static ImageRegion Create(const gfxSize& aSize,
                            ExtendMode aExtendMode = ExtendMode::CLAMP) {
    return ImageRegion(gfxRect(0, 0, aSize.width, aSize.height), aExtendMode);
  }

  static ImageRegion Create(const nsIntSize& aSize,
                            ExtendMode aExtendMode = ExtendMode::CLAMP) {
    return ImageRegion(gfxRect(0, 0, aSize.width, aSize.height), aExtendMode);
  }

  static ImageRegion CreateWithSamplingRestriction(
      const gfxRect& aRect, const gfxRect& aRestriction,
      ExtendMode aExtendMode = ExtendMode::CLAMP) {
    return ImageRegion(aRect, aRestriction, aExtendMode);
  }

  bool IsRestricted() const { return mIsRestricted; }
  const gfxRect& Rect() const { return mRect; }

  const gfxRect& Restriction() const {
    MOZ_ASSERT(mIsRestricted);
    return mRestriction;
  }

  bool RestrictionContains(const gfxRect& aRect) const {
    if (!mIsRestricted) {
      return true;
    }
    return mRestriction.Contains(aRect);
  }

  ImageRegion Intersect(const gfxRect& aRect) const {
    if (mIsRestricted) {
      return CreateWithSamplingRestriction(aRect.Intersect(mRect),
                                           aRect.Intersect(mRestriction));
    }
    return Create(aRect.Intersect(mRect));
  }

  gfxRect IntersectAndRestrict(const gfxRect& aRect) const {
    gfxRect intersection = mRect.Intersect(aRect);
    if (mIsRestricted) {
      intersection = mRestriction.Intersect(intersection);
    }
    return intersection;
  }

  void MoveBy(gfxFloat dx, gfxFloat dy) {
    mRect.MoveBy(dx, dy);
    if (mIsRestricted) {
      mRestriction.MoveBy(dx, dy);
    }
  }

  void Scale(gfxFloat sx, gfxFloat sy) {
    mRect.Scale(sx, sy);
    if (mIsRestricted) {
      mRestriction.Scale(sx, sy);
    }
  }

  void TransformBy(const gfxMatrix& aMatrix) {
    mRect = aMatrix.TransformRect(mRect);
    if (mIsRestricted) {
      mRestriction = aMatrix.TransformRect(mRestriction);
    }
  }

  void TransformBoundsBy(const gfxMatrix& aMatrix) {
    mRect = aMatrix.TransformBounds(mRect);
    if (mIsRestricted) {
      mRestriction = aMatrix.TransformBounds(mRestriction);
    }
  }

  ImageRegion operator-(const gfxPoint& aPt) const {
    if (mIsRestricted) {
      return CreateWithSamplingRestriction(mRect - aPt, mRestriction - aPt);
    }
    return Create(mRect - aPt);
  }

  ImageRegion operator+(const gfxPoint& aPt) const {
    if (mIsRestricted) {
      return CreateWithSamplingRestriction(mRect + aPt, mRestriction + aPt);
    }
    return Create(mRect + aPt);
  }

  gfx::ExtendMode GetExtendMode() const { return mExtendMode; }

  /* ImageRegion() : mIsRestricted(false) { } */

 private:
  explicit ImageRegion(const gfxRect& aRect, ExtendMode aExtendMode)
      : mRect(aRect), mExtendMode(aExtendMode), mIsRestricted(false) {}

  ImageRegion(const gfxRect& aRect, const gfxRect& aRestriction,
              ExtendMode aExtendMode)
      : mRect(aRect),
        mRestriction(aRestriction),
        mExtendMode(aExtendMode),
        mIsRestricted(true) {}

  gfxRect mRect;
  gfxRect mRestriction;
  ExtendMode mExtendMode;
  bool mIsRestricted;
};

/**
 * An axis-aligned rectangle in tiled image space, with an optional sampling
 * restriction rect. The drawing code ensures that if a sampling restriction
 * rect is present, any pixels sampled during the drawing process are found
 * within that rect.
 *
 * The sampling restriction rect exists primarily for callers which perform
 * pixel snapping. Other callers should generally use one of the Create()
 * overloads.
 */
class ImageIntRegion {
  typedef mozilla::gfx::ExtendMode ExtendMode;

 public:
  static ImageIntRegion Empty() {
    return ImageIntRegion(mozilla::gfx::IntRect(), ExtendMode::CLAMP);
  }

  static ImageIntRegion Create(const mozilla::gfx::IntRect& aRect,
                               ExtendMode aExtendMode = ExtendMode::CLAMP) {
    return ImageIntRegion(aRect, aExtendMode);
  }

  static ImageIntRegion Create(const mozilla::gfx::IntSize& aSize,
                               ExtendMode aExtendMode = ExtendMode::CLAMP) {
    return ImageIntRegion(
        mozilla::gfx::IntRect(0, 0, aSize.width, aSize.height), aExtendMode);
  }

  static ImageIntRegion CreateWithSamplingRestriction(
      const mozilla::gfx::IntRect& aRect,
      const mozilla::gfx::IntRect& aRestriction,
      ExtendMode aExtendMode = ExtendMode::CLAMP) {
    return ImageIntRegion(aRect, aRestriction, aExtendMode);
  }

  bool IsRestricted() const { return mIsRestricted; }
  const mozilla::gfx::IntRect& Rect() const { return mRect; }

  const mozilla::gfx::IntRect& Restriction() const {
    MOZ_ASSERT(mIsRestricted);
    return mRestriction;
  }

  bool RestrictionContains(const mozilla::gfx::IntRect& aRect) const {
    if (!mIsRestricted) {
      return true;
    }
    return mRestriction.Contains(aRect);
  }

  ImageIntRegion Intersect(const mozilla::gfx::IntRect& aRect) const {
    if (mIsRestricted) {
      return CreateWithSamplingRestriction(aRect.Intersect(mRect),
                                           aRect.Intersect(mRestriction));
    }
    return Create(aRect.Intersect(mRect));
  }

  mozilla::gfx::IntRect IntersectAndRestrict(
      const mozilla::gfx::IntRect& aRect) const {
    mozilla::gfx::IntRect intersection = mRect.Intersect(aRect);
    if (mIsRestricted) {
      intersection = mRestriction.Intersect(intersection);
    }
    return intersection;
  }

  gfx::ExtendMode GetExtendMode() const { return mExtendMode; }

  ImageRegion ToImageRegion() const {
    if (mIsRestricted) {
      return ImageRegion::CreateWithSamplingRestriction(
          gfxRect(mRect.x, mRect.y, mRect.width, mRect.height),
          gfxRect(mRestriction.x, mRestriction.y, mRestriction.width,
                  mRestriction.height),
          mExtendMode);
    }
    return ImageRegion::Create(
        gfxRect(mRect.x, mRect.y, mRect.width, mRect.height), mExtendMode);
  }

  bool operator==(const ImageIntRegion& aOther) const {
    return mExtendMode == aOther.mExtendMode &&
           mIsRestricted == aOther.mIsRestricted &&
           mRect.IsEqualEdges(aOther.mRect) &&
           (!mIsRestricted || mRestriction.IsEqualEdges(aOther.mRestriction));
  }

  /* ImageIntRegion() : mIsRestricted(false) { } */

 private:
  explicit ImageIntRegion(const mozilla::gfx::IntRect& aRect,
                          ExtendMode aExtendMode)
      : mRect(aRect), mExtendMode(aExtendMode), mIsRestricted(false) {}

  ImageIntRegion(const mozilla::gfx::IntRect& aRect,
                 const mozilla::gfx::IntRect& aRestriction,
                 ExtendMode aExtendMode)
      : mRect(aRect),
        mRestriction(aRestriction),
        mExtendMode(aExtendMode),
        mIsRestricted(true) {}

  mozilla::gfx::IntRect mRect;
  mozilla::gfx::IntRect mRestriction;
  ExtendMode mExtendMode;
  bool mIsRestricted;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_ImageRegion_h
