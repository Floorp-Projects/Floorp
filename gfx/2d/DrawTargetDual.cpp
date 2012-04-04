/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
  * ***** BEGIN LICENSE BLOCK *****
  * Version: MPL 1.1/GPL 2.0/LGPL 2.1
  *
  * The contents of this file are subject to the Mozilla Public License Version
  * 1.1 (the "License"); you may not use this file except in compliance with
  * the License. You may obtain a copy of the License at
  * http://www.mozilla.org/MPL/
  *
  * Software distributed under the License is distributed on an "AS IS" basis,
  * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
  * for the specific language governing rights and limitations under the
  * License.
  *
  * The Original Code is Mozilla Corporation code.
  *
  * The Initial Developer of the Original Code is Mozilla Foundation.
  * Portions created by the Initial Developer are Copyright (C) 2011
  * the Initial Developer. All Rights Reserved.
  *
  * Contributor(s):
  *   Bas Schouten <bschouten@mozilla.com>
  *
  * Alternatively, the contents of this file may be used under the terms of
  * either the GNU General Public License Version 2 or later (the "GPL"), or
  * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
  * in which case the provisions of the GPL or the LGPL are applicable instead
  * of those above. If you wish to allow use of your version of this file only
  * under the terms of either the GPL or the LGPL, and not to allow others to
  * use your version of this file under the terms of the MPL, indicate your
  * decision by deleting the provisions above and replace them with the notice
  * and other provisions required by the GPL or the LGPL. If you do not delete
  * the provisions above, a recipient may use your version of this file under
  * the terms of any one of the MPL, the GPL or the LGPL.
  *
  * ***** END LICENSE BLOCK ***** */
     
#include "DrawTargetDual.h"
#include "Tools.h"

namespace mozilla {
namespace gfx {

class DualSurface
{
public:
  inline DualSurface(SourceSurface *aSurface)
  {
    if (aSurface->GetType() != SURFACE_DUAL_DT) {
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
    if (aPattern.GetType() != PATTERN_SURFACE) {
      mA = mB = &aPattern;
      return;
    }

    const SurfacePattern *surfPat =
      static_cast<const SurfacePattern*>(&aPattern);

    if (surfPat->mSurface->GetType() != SURFACE_DUAL_DT) {
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
