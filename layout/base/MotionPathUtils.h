/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MotionPathUtils_h
#define mozilla_MotionPathUtils_h

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/Maybe.h"
#include "mozilla/ServoStyleConsts.h"
#include "Units.h"

class nsIFrame;

namespace nsStyleTransformMatrix {
class TransformReferenceBox;
}

namespace mozilla {

namespace layers {
class MotionPathData;
class PathCommand;
}  // namespace layers

struct ResolvedMotionPathData {
  gfx::Point mTranslate;
  float mRotate;
  // The delta value between transform-origin and offset-anchor.
  gfx::Point mShift;
};

// The collected information for offset-path. We preprocess the value of
// offset-path and use this data for resolving motion path.
struct OffsetPathData {
  enum class Type : uint8_t {
    None,
    Shape,
    Ray,
  };

  struct ShapeData {
    RefPtr<gfx::Path> mGfxPath;
    // The current position of this transfromed box in the coordinate system of
    // its containing block.
    nsPoint mCurrentPosition;
    // True if it is a closed loop; false if it is a unclosed interval.
    // https://drafts.fxtf.org/motion/#path-distance
    bool mIsClosedLoop;
  };

  struct RayData {
    const StyleRayFunction* mRay;
    // The coord box of the containing block.
    nsRect mCoordBox;
    // The current position of this transfromed box in the coordinate system of
    // its containing block.
    nsPoint mCurrentPosition;
    // The reference length for computing ray(contain).
    CSSCoord mContainReferenceLength;
  };

  Type mType;
  union {
    ShapeData mShape;
    RayData mRay;
  };

  static OffsetPathData None() { return OffsetPathData(); }
  static OffsetPathData Shape(already_AddRefed<gfx::Path>&& aGfxPath,
                              nsPoint&& aCurrentPosition, bool aIsClosedPath) {
    return OffsetPathData(std::move(aGfxPath), std::move(aCurrentPosition),
                          aIsClosedPath);
  }
  static OffsetPathData Ray(const StyleRayFunction& aRay, nsRect&& aCoordBox,
                            nsPoint&& aPosition,
                            CSSCoord&& aContainReferenceLength) {
    return OffsetPathData(&aRay, std::move(aCoordBox), std::move(aPosition),
                          std::move(aContainReferenceLength));
  }
  static OffsetPathData Ray(const StyleRayFunction& aRay,
                            const nsRect& aCoordBox, const nsPoint& aPosition,
                            const CSSCoord& aContainReferenceLength) {
    return OffsetPathData(&aRay, aCoordBox, aPosition, aContainReferenceLength);
  }

  bool IsNone() const { return mType == Type::None; }
  bool IsShape() const { return mType == Type::Shape; }
  bool IsRay() const { return mType == Type::Ray; }

  const ShapeData& AsShape() const {
    MOZ_ASSERT(IsShape());
    return mShape;
  }

  const RayData& AsRay() const {
    MOZ_ASSERT(IsRay());
    return mRay;
  }

  ~OffsetPathData() {
    switch (mType) {
      case Type::Shape:
        mShape.~ShapeData();
        break;
      case Type::Ray:
        mRay.~RayData();
        break;
      default:
        break;
    }
  }

  OffsetPathData(const OffsetPathData& aOther) : mType(aOther.mType) {
    switch (mType) {
      case Type::Shape:
        mShape = aOther.mShape;
        break;
      case Type::Ray:
        mRay = aOther.mRay;
        break;
      default:
        break;
    }
  }

  OffsetPathData(OffsetPathData&& aOther) : mType(aOther.mType) {
    switch (mType) {
      case Type::Shape:
        mShape = std::move(aOther.mShape);
        break;
      case Type::Ray:
        mRay = std::move(aOther.mRay);
        break;
      default:
        break;
    }
  }

 private:
  OffsetPathData() : mType(Type::None) {}
  OffsetPathData(already_AddRefed<gfx::Path>&& aPath,
                 nsPoint&& aCurrentPosition, bool aIsClosed)
      : mType(Type::Shape),
        mShape{std::move(aPath), std::move(aCurrentPosition), aIsClosed} {}
  OffsetPathData(const StyleRayFunction* aRay, nsRect&& aCoordBox,
                 nsPoint&& aPosition, CSSCoord&& aContainReferenceLength)
      : mType(Type::Ray),
        mRay{aRay, std::move(aCoordBox), std::move(aPosition),
             std::move(aContainReferenceLength)} {}
  OffsetPathData(const StyleRayFunction* aRay, const nsRect& aCoordBox,
                 const nsPoint& aPosition,
                 const CSSCoord& aContainReferenceLength)
      : mType(Type::Ray),
        mRay{aRay, aCoordBox, aPosition, aContainReferenceLength} {}
  OffsetPathData& operator=(const OffsetPathData&) = delete;
  OffsetPathData& operator=(OffsetPathData&&) = delete;
};

// MotionPathUtils is a namespace class containing utility functions related to
// processing motion path in the [motion-1].
// https://drafts.fxtf.org/motion-1/
class MotionPathUtils final {
  using TransformReferenceBox = nsStyleTransformMatrix::TransformReferenceBox;

 public:
  /**
   * SVG frames (unlike other frames) have a reference box that can be (and
   * typically is) offset from the TopLeft() of the frame.
   *
   * In motion path, we have to make sure the object is aligned with offset-path
   * when using content area, so we should tweak the anchor point by a given
   * offset.
   */
  static CSSPoint ComputeAnchorPointAdjustment(const nsIFrame& aFrame);

  /**
   * In CSS context, this returns the the box being referenced from the element
   * that establishes the containing block for this element.
   * In SVG context, we always use view-box.
   * https://drafts.fxtf.org/motion-1/#valdef-offset-path-coord-box
   */
  static const nsIFrame* GetOffsetPathReferenceBox(const nsIFrame* aFrame,
                                                   nsRect& aOutputRect);

  /**
   * Return the width or the height of the element’s border box, whichever is
   * larger. This is for computing the ray() with "contain" keyword.
   */
  static CSSCoord GetRayContainReferenceSize(nsIFrame* aFrame);

  /**
   * Get the resolved radius for inset(0 round X), where X is the parameter of
   * |aRadius|.
   * This returns an empty array if we cannot compute the radii; otherwise, it
   * returns an array with 8 elements.
   */
  static nsTArray<nscoord> ComputeBorderRadii(
      const StyleBorderRadius& aBorderRadius, const nsRect& aCoordBox);

  /**
   * Generate the motion path transform result. This function may be called on
   * the compositor thread.
   */
  static Maybe<ResolvedMotionPathData> ResolveMotionPath(
      const OffsetPathData& aPath, const LengthPercentage& aDistance,
      const StyleOffsetRotate& aRotate, const StylePositionOrAuto& aAnchor,
      const StyleOffsetPosition& aPosition, const CSSPoint& aTransformOrigin,
      TransformReferenceBox&, const CSSPoint& aAnchorPointAdjustment);

  /**
   * Generate the motion path transform result with |nsIFrame|. This is only
   * called in the main thread.
   */
  static Maybe<ResolvedMotionPathData> ResolveMotionPath(
      const nsIFrame* aFrame, TransformReferenceBox&);

  /**
   * Generate the motion path transfrom result with styles and
   * layers::MotionPathData.
   * This is only called by the compositor.
   */
  static Maybe<ResolvedMotionPathData> ResolveMotionPath(
      const StyleOffsetPath* aPath, const StyleLengthPercentage* aDistance,
      const StyleOffsetRotate* aRotate, const StylePositionOrAuto* aAnchor,
      const StyleOffsetPosition* aPosition,
      const Maybe<layers::MotionPathData>& aMotionPathData,
      TransformReferenceBox&, gfx::Path* aCachedMotionPath);

  /**
   * Build a gfx::Path from the svg path data. We should give it a path builder.
   * If |aPathBuilder| is nullptr, we return null path.
   * This can be used on the main thread or on the compositor thread.
   */
  static already_AddRefed<gfx::Path> BuildSVGPath(
      const StyleSVGPathData& aPath, gfx::PathBuilder* aPathBuilder);

  /**
   * Build a gfx::Path from the computed basic shape.
   */
  static already_AddRefed<gfx::Path> BuildPath(const StyleBasicShape&,
                                               const StyleOffsetPosition&,
                                               const nsRect& aCoordBox,
                                               const nsPoint& aCurrentPosition,
                                               gfx::PathBuilder*);

  /**
   * Get a path builder for motion path on the main thread.
   */
  static already_AddRefed<gfx::PathBuilder> GetPathBuilder();

  /**
   * Get a path builder for compositor.
   */
  static already_AddRefed<gfx::PathBuilder> GetCompositorPathBuilder();
};

}  // namespace mozilla

#endif  // mozilla_MotionPathUtils_h
