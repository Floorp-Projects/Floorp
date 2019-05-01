/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_PATTERNHELPERS_H
#define _MOZILLA_GFX_PATTERNHELPERS_H

#include "mozilla/Alignment.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace gfx {

/**
 * This class is used to allow general pattern creation functions to return
 * any type of pattern via an out-paramater without allocating a pattern
 * instance on the free-store (an instance of this class being created on the
 * stack before passing it in to the creation function). Without this class
 * writing pattern creation functions would be a pain since Pattern objects are
 * not reference counted, making lifetime management of instances created on
 * the free-store and returned from a creation function hazardous. Besides
 * that, in the case that ColorPattern's are expected to be common, it is
 * particularly desirable to avoid the overhead of allocating on the
 * free-store.
 */
class GeneralPattern final {
 public:
  explicit GeneralPattern() = default;

  GeneralPattern(const GeneralPattern& aOther) {}

  ~GeneralPattern() {
    if (mPattern) {
      mPattern->~Pattern();
    }
  }

  Pattern* Init(const Pattern& aPattern) {
    MOZ_ASSERT(!mPattern);
    switch (aPattern.GetType()) {
      case PatternType::COLOR:
        mPattern = new (mColorPattern.addr())
            ColorPattern(static_cast<const ColorPattern&>(aPattern));
        break;
      case PatternType::LINEAR_GRADIENT:
        mPattern = new (mLinearGradientPattern.addr()) LinearGradientPattern(
            static_cast<const LinearGradientPattern&>(aPattern));
        break;
      case PatternType::RADIAL_GRADIENT:
        mPattern = new (mRadialGradientPattern.addr()) RadialGradientPattern(
            static_cast<const RadialGradientPattern&>(aPattern));
        break;
      case PatternType::SURFACE:
        mPattern = new (mSurfacePattern.addr())
            SurfacePattern(static_cast<const SurfacePattern&>(aPattern));
        break;
      default:
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unknown pattern type");
    }
    return mPattern;
  }

  ColorPattern* InitColorPattern(const Color& aColor) {
    MOZ_ASSERT(!mPattern);
    mPattern = new (mColorPattern.addr()) ColorPattern(aColor);
    return mColorPattern.addr();
  }

  LinearGradientPattern* InitLinearGradientPattern(
      const Point& aBegin, const Point& aEnd, GradientStops* aStops,
      const Matrix& aMatrix = Matrix()) {
    MOZ_ASSERT(!mPattern);
    mPattern = new (mLinearGradientPattern.addr())
        LinearGradientPattern(aBegin, aEnd, aStops, aMatrix);
    return mLinearGradientPattern.addr();
  }

  RadialGradientPattern* InitRadialGradientPattern(
      const Point& aCenter1, const Point& aCenter2, Float aRadius1,
      Float aRadius2, GradientStops* aStops, const Matrix& aMatrix = Matrix()) {
    MOZ_ASSERT(!mPattern);
    mPattern = new (mRadialGradientPattern.addr()) RadialGradientPattern(
        aCenter1, aCenter2, aRadius1, aRadius2, aStops, aMatrix);
    return mRadialGradientPattern.addr();
  }

  SurfacePattern* InitSurfacePattern(
      SourceSurface* aSourceSurface, ExtendMode aExtendMode,
      const Matrix& aMatrix = Matrix(),
      SamplingFilter aSamplingFilter = SamplingFilter::GOOD,
      const IntRect& aSamplingRect = IntRect()) {
    MOZ_ASSERT(!mPattern);
    mPattern = new (mSurfacePattern.addr()) SurfacePattern(
        aSourceSurface, aExtendMode, aMatrix, aSamplingFilter, aSamplingRect);
    return mSurfacePattern.addr();
  }

  Pattern* GetPattern() { return mPattern; }

  const Pattern* GetPattern() const { return mPattern; }

  operator Pattern&() {
    if (!mPattern) {
      MOZ_CRASH("GFX: GeneralPattern not initialized");
    }
    return *mPattern;
  }

 private:
  union {
    AlignedStorage2<ColorPattern> mColorPattern;
    AlignedStorage2<LinearGradientPattern> mLinearGradientPattern;
    AlignedStorage2<RadialGradientPattern> mRadialGradientPattern;
    AlignedStorage2<SurfacePattern> mSurfacePattern;
  };
  Pattern* mPattern = nullptr;
};

}  // namespace gfx
}  // namespace mozilla

#endif  //  _MOZILLA_GFX_PATTERNHELPERS_H
