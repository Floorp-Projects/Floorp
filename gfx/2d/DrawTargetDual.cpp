/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
     
#include "DrawTargetDual.h"
#include "Tools.h"

namespace mozilla {
namespace gfx {

class DualSurface
{
public:
  inline DualSurface(SourceSurface *aSurface)
  {
    if (aSurface->GetType() != SurfaceType::DUAL_DT) {
      mA = mB = aSurface;
      return;
    }

    SourceSurfaceDual *ssDual =
      static_cast<SourceSurfaceDual*>(aSurface);
    mA = ssDual->mA;
    mB = ssDual->mB;
  }

  SourceSurface *mA;
  SourceSurface *mB;
};

/* This only needs to split patterns up for SurfacePatterns. Only in that
 * case can we be dealing with a 'dual' source (SourceSurfaceDual) and do
 * we need to pass separate patterns into our destination DrawTargets.
 */
class DualPattern
{
public:
  inline DualPattern(const Pattern &aPattern)
    : mPatternsInitialized(false)
  {
    if (aPattern.GetType() != PatternType::SURFACE) {
      mA = mB = &aPattern;
      return;
    }

    const SurfacePattern *surfPat =
      static_cast<const SurfacePattern*>(&aPattern);

    if (surfPat->mSurface->GetType() != SurfaceType::DUAL_DT) {
      mA = mB = &aPattern;
      return;
    }

    const SourceSurfaceDual *ssDual =
      static_cast<const SourceSurfaceDual*>(surfPat->mSurface.get());
    mA = new (mSurfPatA.addr()) SurfacePattern(ssDual->mA, surfPat->mExtendMode,
                                               surfPat->mMatrix, surfPat->mFilter);
    mB = new (mSurfPatB.addr()) SurfacePattern(ssDual->mB, surfPat->mExtendMode,
                                               surfPat->mMatrix, surfPat->mFilter);
    mPatternsInitialized = true;
  }

  inline ~DualPattern()
  {
    if (mPatternsInitialized) {
      mA->~Pattern();
      mB->~Pattern();
    }
  }

  ClassStorage<SurfacePattern> mSurfPatA;
  ClassStorage<SurfacePattern> mSurfPatB;

  const Pattern *mA;
  const Pattern *mB;

  bool mPatternsInitialized;
};

void
DrawTargetDual::DrawSurface(SourceSurface *aSurface, const Rect &aDest, const Rect &aSource,
                            const DrawSurfaceOptions &aSurfOptions, const DrawOptions &aOptions)
{
  DualSurface surface(aSurface);
  mA->DrawSurface(surface.mA, aDest, aSource, aSurfOptions, aOptions);
  mB->DrawSurface(surface.mB, aDest, aSource, aSurfOptions, aOptions);
}

void
DrawTargetDual::DrawSurfaceWithShadow(SourceSurface *aSurface, const Point &aDest,
                                      const Color &aColor, const Point &aOffset,
                                      Float aSigma, CompositionOp aOp)
{
  DualSurface surface(aSurface);
  mA->DrawSurfaceWithShadow(surface.mA, aDest, aColor, aOffset, aSigma, aOp);
  mB->DrawSurfaceWithShadow(surface.mB, aDest, aColor, aOffset, aSigma, aOp);
}

void
DrawTargetDual::MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions)
{
  DualPattern source(aSource);
  DualSurface mask(aMask);
  mA->MaskSurface(*source.mA, mask.mA, aOffset, aOptions);
  mB->MaskSurface(*source.mB, mask.mB, aOffset, aOptions);
}

void
DrawTargetDual::CopySurface(SourceSurface *aSurface, const IntRect &aSourceRect,
                            const IntPoint &aDestination)
{
  DualSurface surface(aSurface);
  mA->CopySurface(surface.mA, aSourceRect, aDestination);
  mB->CopySurface(surface.mB, aSourceRect, aDestination);
}

void
DrawTargetDual::FillRect(const Rect &aRect, const Pattern &aPattern, const DrawOptions &aOptions)
{
  DualPattern pattern(aPattern);
  mA->FillRect(aRect, *pattern.mA, aOptions);
  mB->FillRect(aRect, *pattern.mB, aOptions);
}

void
DrawTargetDual::StrokeRect(const Rect &aRect, const Pattern &aPattern,
                           const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions)
{
  DualPattern pattern(aPattern);
  mA->StrokeRect(aRect, *pattern.mA, aStrokeOptions, aOptions);
  mB->StrokeRect(aRect, *pattern.mB, aStrokeOptions, aOptions);
}

void
DrawTargetDual::StrokeLine(const Point &aStart, const Point &aEnd, const Pattern &aPattern,
                           const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions)
{
  DualPattern pattern(aPattern);
  mA->StrokeLine(aStart, aEnd, *pattern.mA, aStrokeOptions, aOptions);
  mB->StrokeLine(aStart, aEnd, *pattern.mB, aStrokeOptions, aOptions);
}

void
DrawTargetDual::Stroke(const Path *aPath, const Pattern &aPattern,
                       const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions)
{
  DualPattern pattern(aPattern);
  mA->Stroke(aPath, *pattern.mA, aStrokeOptions, aOptions);
  mB->Stroke(aPath, *pattern.mB, aStrokeOptions, aOptions);
}

void
DrawTargetDual::Fill(const Path *aPath, const Pattern &aPattern, const DrawOptions &aOptions)
{
  DualPattern pattern(aPattern);
  mA->Fill(aPath, *pattern.mA, aOptions);
  mB->Fill(aPath, *pattern.mB, aOptions);
}

void
DrawTargetDual::FillGlyphs(ScaledFont *aScaledFont, const GlyphBuffer &aBuffer,
                           const Pattern &aPattern, const DrawOptions &aOptions,
                           const GlyphRenderingOptions *aRenderingOptions)
{
  DualPattern pattern(aPattern);
  mA->FillGlyphs(aScaledFont, aBuffer, *pattern.mA, aOptions, aRenderingOptions);
  mB->FillGlyphs(aScaledFont, aBuffer, *pattern.mB, aOptions, aRenderingOptions);
}

void
DrawTargetDual::Mask(const Pattern &aSource, const Pattern &aMask, const DrawOptions &aOptions)
{
  DualPattern source(aSource);
  DualPattern mask(aMask);
  mA->Mask(*source.mA, *mask.mA, aOptions);
  mB->Mask(*source.mB, *mask.mB, aOptions);
}

TemporaryRef<DrawTarget>
DrawTargetDual::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  RefPtr<DrawTarget> dtA = mA->CreateSimilarDrawTarget(aSize, aFormat);
  RefPtr<DrawTarget> dtB = mB->CreateSimilarDrawTarget(aSize, aFormat);

  return new DrawTargetDual(dtA, dtB);
}

}
}
