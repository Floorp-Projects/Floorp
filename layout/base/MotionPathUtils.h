/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MotionPathUtils_h
#define mozilla_MotionPathUtils_h

#include "mozilla/gfx/Point.h"
#include "mozilla/Maybe.h"
#include "mozilla/ServoStyleConsts.h"
#include "Units.h"

class nsIFrame;

namespace mozilla {

using RayFunction = StyleRayFunction<StyleAngle>;

struct MotionPathData {
  gfx::Point mTranslate;
  float mRotate;
  // The delta value between transform-origin and offset-anchor.
  gfx::Point mShift;
};

struct RayReferenceData {
  // The initial position related to the containing block.
  CSSPoint mInitialPosition;
  // The rect of the containing block.
  CSSRect mContainingBlockRect;

  explicit RayReferenceData(const nsIFrame* aFrame);
};

// The collected information for offset-path. We preprocess the value of
// offset-path and use this data for resolving motion path.
struct OffsetPathData {
  enum class Type : uint8_t {
    None,
    Path,
    Ray,
  };

  struct PathData {
    RefPtr<gfx::Path> mGfxPath;
    bool mIsClosedIntervals;
  };

  struct RayData {
    const RayFunction* mRay;
    RayReferenceData mData;
  };

  Type mType;
  union {
    PathData mPath;
    RayData mRay;
  };

  static OffsetPathData None() { return OffsetPathData(); }
  static OffsetPathData Path(const StyleSVGPathData& aPath,
                             gfx::PathBuilder* aPathBuilder);
  static OffsetPathData Ray(const RayFunction& aRay,
                            const RayReferenceData& aData) {
    return OffsetPathData(&aRay, aData);
  }
  static OffsetPathData Ray(const RayFunction& aRay, RayReferenceData&& aData) {
    return OffsetPathData(&aRay, std::move(aData));
  }

  bool IsNone() const { return mType == Type::None; }
  bool IsPath() const { return mType == Type::Path; }
  bool IsRay() const { return mType == Type::Ray; }

  const PathData& AsPath() const {
    MOZ_ASSERT(IsPath());
    return mPath;
  }

  const RayData& AsRay() const {
    MOZ_ASSERT(IsRay());
    return mRay;
  }

  ~OffsetPathData() {
    switch (mType) {
      case Type::Path:
        mPath.~PathData();
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
      case Type::Path:
        mPath = aOther.mPath;
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
      case Type::Path:
        mPath = std::move(aOther.mPath);
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
  OffsetPathData(const RefPtr<gfx::Path>& aPath, bool aIsClosed)
      : mType(Type::Path), mPath{aPath, aIsClosed} {}
  OffsetPathData(const RayFunction* aRay, RayReferenceData&& aRef)
      : mType(Type::Ray), mRay{aRay, std::move(aRef)} {}
  OffsetPathData(const RayFunction* aRay, const RayReferenceData& aRef)
      : mType(Type::Ray), mRay{aRay, aRef} {}
  OffsetPathData& operator=(const OffsetPathData&) = delete;
  OffsetPathData& operator=(OffsetPathData&&) = delete;
};

// MotionPathUtils is a namespace class containing utility functions related to
// processing motion path in the [motion-1].
// https://drafts.fxtf.org/motion-1/
class MotionPathUtils final {
 public:
  /**
   * Generate the motion path transform result. This function may be called on
   * the compositor thread.
   **/
  static Maybe<MotionPathData> ResolveMotionPath(
      const OffsetPathData& aPath, const LengthPercentage& aDistance,
      const StyleOffsetRotate& aRotate, const StylePositionOrAuto& aAnchor,
      const CSSPoint& aTransformOrigin, const CSSSize& aFrameSize,
      const Maybe<CSSPoint>& aFramePosition);

  /**
   * Generate the motion path transform result with |nsIFrame|.
   **/
  static Maybe<MotionPathData> ResolveMotionPath(const nsIFrame* aFrame);
};

}  // namespace mozilla

#endif  // mozilla_MotionPathUtils_h
