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
  virtual void funcName() override { mA->funcName(); mB->funcName(); }
#define FORWARD_FUNCTION1(funcName, var1Type, var1Name) \
  virtual void funcName(var1Type var1Name) override { mA->funcName(var1Name); mB->funcName(var1Name); }

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
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetDual, override)
  DrawTargetDual(DrawTarget *aA, DrawTarget *aB)
    : mA(aA)
    , mB(aB)
  { 
    mFormat = aA->GetFormat();
  }
     
  virtual DrawTargetType GetType() const override { return mA->GetType(); }
  virtual BackendType GetBackendType() const override { return mA->GetBackendType(); }
  virtual already_AddRefed<SourceSurface> Snapshot() override {
    return MakeAndAddRef<SourceSurfaceDual>(mA, mB);
  }
  virtual IntSize GetSize() override { return mA->GetSize(); }
     
  FORWARD_FUNCTION(Flush)
  FORWARD_FUNCTION1(PushClip, const Path *, aPath)
  FORWARD_FUNCTION1(PushClipRect, const Rect &, aRect)
  FORWARD_FUNCTION(PopClip)
  FORWARD_FUNCTION(PopLayer)
  FORWARD_FUNCTION1(ClearRect, const Rect &, aRect)

  virtual void SetTransform(const Matrix &aTransform) override {
    mTransform = aTransform;
    mA->SetTransform(aTransform);
    mB->SetTransform(aTransform);
  }

  virtual void DrawSurface(SourceSurface *aSurface, const Rect &aDest, const Rect & aSource,
                           const DrawSurfaceOptions &aSurfOptions, const DrawOptions &aOptions) override;

  virtual void DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions = DrawOptions()) override
  {
    mA->DrawFilter(aNode, aSourceRect, aDestPoint, aOptions);
    mB->DrawFilter(aNode, aSourceRect, aDestPoint, aOptions);
  }

  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions()) override;

  virtual void DrawSurfaceWithShadow(SourceSurface *aSurface, const Point &aDest,
                                     const Color &aColor, const Point &aOffset,
                                     Float aSigma, CompositionOp aOp) override;

  virtual void CopySurface(SourceSurface *aSurface, const IntRect &aSourceRect,
                           const IntPoint &aDestination) override;

  virtual void FillRect(const Rect &aRect, const Pattern &aPattern, const DrawOptions &aOptions) override;

  virtual void StrokeRect(const Rect &aRect, const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions) override;

  virtual void StrokeLine(const Point &aStart, const Point &aEnd, const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions) override;

  virtual void Stroke(const Path *aPath, const Pattern &aPattern,
                      const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions) override;

  virtual void Fill(const Path *aPath, const Pattern &aPattern, const DrawOptions &aOptions) override;

  virtual void FillGlyphs(ScaledFont *aScaledFont, const GlyphBuffer &aBuffer,
                          const Pattern &aPattern, const DrawOptions &aOptions,
                          const GlyphRenderingOptions *aRenderingOptions) override;
  
  virtual void Mask(const Pattern &aSource, const Pattern &aMask, const DrawOptions &aOptions) override;

  virtual void PushLayer(bool aOpaque, Float aOpacity,
                         SourceSurface* aMask,
                         const Matrix& aMaskTransform,
                         const IntRect& aBounds = IntRect(),
                         bool aCopyBackground = false) override;

  virtual already_AddRefed<SourceSurface>
    CreateSourceSurfaceFromData(unsigned char *aData,
                                const IntSize &aSize,
                                int32_t aStride,
                                SurfaceFormat aFormat) const override
  {
    return mA->CreateSourceSurfaceFromData(aData, aSize, aStride, aFormat);
  }
     
  virtual already_AddRefed<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const override
  {
    return mA->OptimizeSourceSurface(aSurface);
  }
     
  virtual already_AddRefed<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const override
  {
    return mA->CreateSourceSurfaceFromNativeSurface(aSurface);
  }
     
  virtual already_AddRefed<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const override;
     
  virtual already_AddRefed<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const override
  {
    return mA->CreatePathBuilder(aFillRule);
  }
     
  virtual already_AddRefed<GradientStops>
    CreateGradientStops(GradientStop *aStops,
                        uint32_t aNumStops,
                        ExtendMode aExtendMode = ExtendMode::CLAMP) const override
  {
    return mA->CreateGradientStops(aStops, aNumStops, aExtendMode);
  }

  virtual already_AddRefed<FilterNode> CreateFilter(FilterType aType) override
  {
    return mA->CreateFilter(aType);
  }

  virtual void *GetNativeSurface(NativeSurfaceType aType) override
  {
    return nullptr;
  }

  virtual bool IsDualDrawTarget() const override
  {
    return true;
  }

  virtual bool IsCurrentGroupOpaque() override { return mA->IsCurrentGroupOpaque(); }
     
private:
  RefPtr<DrawTarget> mA;
  RefPtr<DrawTarget> mB;
};
     
} // namespace gfx
} // namespace mozilla
     
#endif /* MOZILLA_GFX_DRAWTARGETDUAL_H_ */ 
