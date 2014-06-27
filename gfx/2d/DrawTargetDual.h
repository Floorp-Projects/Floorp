/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
     
#ifndef MOZILLA_GFX_DRAWTARGETDUAL_H_
#define MOZILLA_GFX_DRAWTARGETDUAL_H_
     
#include <vector>
#include <sstream>

#include "SourceSurfaceDual.h"
     
#include "2D.h"
#include "Filters.h"
     
namespace mozilla {
namespace gfx {
     
#define FORWARD_FUNCTION(funcName) \
  virtual void funcName() { mA->funcName(); mB->funcName(); }
#define FORWARD_FUNCTION1(funcName, var1Type, var1Name) \
  virtual void funcName(var1Type var1Name) { mA->funcName(var1Name); mB->funcName(var1Name); }

/* This is a special type of DrawTarget. It duplicates all drawing calls
 * accross two drawtargets. An exception to this is when a snapshot of another
 * dual DrawTarget is used as the source for any surface data. In this case
 * the snapshot of the first source DrawTarget is used as a source for the call
 * to the first destination DrawTarget (mA) and the snapshot of the second
 * source DrawTarget is used at the source for the second destination
 * DrawTarget (mB). This class facilitates black-background/white-background
 * drawing for per-component alpha extraction for backends which do not support
 * native component alpha.
 */
class DrawTargetDual : public DrawTarget
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetDual)
  DrawTargetDual(DrawTarget *aA, DrawTarget *aB)
    : mA(aA)
    , mB(aB)
  { 
    mFormat = aA->GetFormat();
  }
     
  virtual DrawTargetType GetType() const MOZ_OVERRIDE { return mA->GetType(); }
  virtual BackendType GetBackendType() const { return mA->GetBackendType(); }
  virtual TemporaryRef<SourceSurface> Snapshot() { return new SourceSurfaceDual(mA, mB); }
  virtual IntSize GetSize() { return mA->GetSize(); }
     
  FORWARD_FUNCTION(Flush)
  FORWARD_FUNCTION1(PushClip, const Path *, aPath)
  FORWARD_FUNCTION1(PushClipRect, const Rect &, aRect)
  FORWARD_FUNCTION(PopClip)
  FORWARD_FUNCTION1(ClearRect, const Rect &, aRect)

  virtual void SetTransform(const Matrix &aTransform) {
    mTransform = aTransform;
    mA->SetTransform(aTransform);
    mB->SetTransform(aTransform);
  }

  virtual void DrawSurface(SourceSurface *aSurface, const Rect &aDest, const Rect & aSource,
                           const DrawSurfaceOptions &aSurfOptions, const DrawOptions &aOptions);

  virtual void DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions = DrawOptions())
  {
    mA->DrawFilter(aNode, aSourceRect, aDestPoint, aOptions);
    mB->DrawFilter(aNode, aSourceRect, aDestPoint, aOptions);
  }

  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions());

  virtual void DrawSurfaceWithShadow(SourceSurface *aSurface, const Point &aDest,
                                     const Color &aColor, const Point &aOffset,
                                     Float aSigma, CompositionOp aOp);

  virtual void CopySurface(SourceSurface *aSurface, const IntRect &aSourceRect,
                           const IntPoint &aDestination);

  virtual void FillRect(const Rect &aRect, const Pattern &aPattern, const DrawOptions &aOptions);

  virtual void StrokeRect(const Rect &aRect, const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions);

  virtual void StrokeLine(const Point &aStart, const Point &aEnd, const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions);

  virtual void Stroke(const Path *aPath, const Pattern &aPattern,
                      const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions);

  virtual void Fill(const Path *aPath, const Pattern &aPattern, const DrawOptions &aOptions);

  virtual void FillGlyphs(ScaledFont *aScaledFont, const GlyphBuffer &aBuffer,
                          const Pattern &aPattern, const DrawOptions &aOptions,
                          const GlyphRenderingOptions *aRenderingOptions);
  
  virtual void Mask(const Pattern &aSource, const Pattern &aMask, const DrawOptions &aOptions);
     
  virtual TemporaryRef<SourceSurface>
    CreateSourceSurfaceFromData(unsigned char *aData,
                                const IntSize &aSize,
                                int32_t aStride,
                                SurfaceFormat aFormat) const
  {
    return mA->CreateSourceSurfaceFromData(aData, aSize, aStride, aFormat);
  }
     
  virtual TemporaryRef<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const
  {
    return mA->OptimizeSourceSurface(aSurface);
  }
     
  virtual TemporaryRef<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
  {
    return mA->CreateSourceSurfaceFromNativeSurface(aSurface);
  }
     
  virtual TemporaryRef<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const;
     
  virtual TemporaryRef<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const
  {
    return mA->CreatePathBuilder(aFillRule);
  }
     
  virtual TemporaryRef<GradientStops>
    CreateGradientStops(GradientStop *aStops,
                        uint32_t aNumStops,
                        ExtendMode aExtendMode = ExtendMode::CLAMP) const
  {
    return mA->CreateGradientStops(aStops, aNumStops, aExtendMode);
  }

  virtual TemporaryRef<FilterNode> CreateFilter(FilterType aType)
  {
    return mA->CreateFilter(aType);
  }

  virtual void *GetNativeSurface(NativeSurfaceType aType)
  {
    return nullptr;
  }

  virtual bool IsDualDrawTarget()
  {
    return true;
  }
     
private:
  RefPtr<DrawTarget> mA;
  RefPtr<DrawTarget> mB;
};
     
}
}
     
#endif /* MOZILLA_GFX_DRAWTARGETDUAL_H_ */ 
