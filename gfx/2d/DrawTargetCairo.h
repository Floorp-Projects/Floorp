/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_DRAWTARGET_CAIRO_H_
#define _MOZILLA_GFX_DRAWTARGET_CAIRO_H_

#include "2D.h"
#include "cairo.h"
#include "PathCairo.h"

#include <vector>

namespace mozilla {
namespace gfx {

class SourceSurfaceCairo;

class GradientStopsCairo : public GradientStops
{
  public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GradientStopsCairo)
    GradientStopsCairo(GradientStop* aStops, uint32_t aNumStops,
                       ExtendMode aExtendMode)
     : mExtendMode(aExtendMode)
    {
      for (uint32_t i = 0; i < aNumStops; ++i) {
        mStops.push_back(aStops[i]);
      }
    }

    virtual ~GradientStopsCairo() {}

    const std::vector<GradientStop>& GetStops() const
    {
      return mStops;
    }

    ExtendMode GetExtendMode() const
    {
      return mExtendMode;
    }

    virtual BackendType GetBackendType() const { return BackendType::CAIRO; }

  private:
    std::vector<GradientStop> mStops;
    ExtendMode mExtendMode;
};

class DrawTargetCairo : public DrawTarget
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetCairo, MOZ_OVERRIDE)
  friend class BorrowedCairoContext;

  DrawTargetCairo();
  virtual ~DrawTargetCairo();

  virtual DrawTargetType GetType() const MOZ_OVERRIDE;
  virtual BackendType GetBackendType() const MOZ_OVERRIDE { return BackendType::CAIRO; }
  virtual TemporaryRef<SourceSurface> Snapshot() MOZ_OVERRIDE;
  virtual IntSize GetSize() MOZ_OVERRIDE;

  virtual void SetPermitSubpixelAA(bool aPermitSubpixelAA) MOZ_OVERRIDE;

  virtual bool LockBits(uint8_t** aData, IntSize* aSize,
                        int32_t* aStride, SurfaceFormat* aFormat) MOZ_OVERRIDE;
  virtual void ReleaseBits(uint8_t* aData) MOZ_OVERRIDE;

  virtual void Flush() MOZ_OVERRIDE;
  virtual void DrawSurface(SourceSurface *aSurface,
                           const Rect &aDest,
                           const Rect &aSource,
                           const DrawSurfaceOptions &aSurfOptions = DrawSurfaceOptions(),
                           const DrawOptions &aOptions = DrawOptions()) MOZ_OVERRIDE;
  virtual void DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions = DrawOptions()) MOZ_OVERRIDE;
  virtual void DrawSurfaceWithShadow(SourceSurface *aSurface,
                                     const Point &aDest,
                                     const Color &aColor,
                                     const Point &aOffset,
                                     Float aSigma,
                                     CompositionOp aOperator) MOZ_OVERRIDE;

  virtual void ClearRect(const Rect &aRect) MOZ_OVERRIDE;

  virtual void CopySurface(SourceSurface *aSurface,
                           const IntRect &aSourceRect,
                           const IntPoint &aDestination) MOZ_OVERRIDE;
  virtual void CopyRect(const IntRect &aSourceRect,
                        const IntPoint &aDestination) MOZ_OVERRIDE;

  virtual void FillRect(const Rect &aRect,
                        const Pattern &aPattern,
                        const DrawOptions &aOptions = DrawOptions()) MOZ_OVERRIDE;
  virtual void StrokeRect(const Rect &aRect,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions = StrokeOptions(),
                          const DrawOptions &aOptions = DrawOptions()) MOZ_OVERRIDE;
  virtual void StrokeLine(const Point &aStart,
                          const Point &aEnd,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions = StrokeOptions(),
                          const DrawOptions &aOptions = DrawOptions()) MOZ_OVERRIDE;

  virtual void Stroke(const Path *aPath,
                      const Pattern &aPattern,
                      const StrokeOptions &aStrokeOptions = StrokeOptions(),
                      const DrawOptions &aOptions = DrawOptions()) MOZ_OVERRIDE;

  virtual void Fill(const Path *aPath,
                    const Pattern &aPattern,
                    const DrawOptions &aOptions = DrawOptions()) MOZ_OVERRIDE;

  virtual void FillGlyphs(ScaledFont *aFont,
                          const GlyphBuffer &aBuffer,
                          const Pattern &aPattern,
                          const DrawOptions &aOptions,
                          const GlyphRenderingOptions *aRenderingOptions = nullptr) MOZ_OVERRIDE;
  virtual void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions = DrawOptions()) MOZ_OVERRIDE;
  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions()) MOZ_OVERRIDE;

  virtual void PushClip(const Path *aPath) MOZ_OVERRIDE;
  virtual void PushClipRect(const Rect &aRect) MOZ_OVERRIDE;
  virtual void PopClip() MOZ_OVERRIDE;

  virtual TemporaryRef<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const MOZ_OVERRIDE;

  virtual TemporaryRef<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                            const IntSize &aSize,
                                                            int32_t aStride,
                                                            SurfaceFormat aFormat) const MOZ_OVERRIDE;
  virtual TemporaryRef<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const MOZ_OVERRIDE;
  virtual TemporaryRef<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const MOZ_OVERRIDE;
  virtual TemporaryRef<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const MOZ_OVERRIDE;
  virtual TemporaryRef<DrawTarget>
    CreateShadowDrawTarget(const IntSize &aSize, SurfaceFormat aFormat,
                           float aSigma) const MOZ_OVERRIDE;

  virtual TemporaryRef<GradientStops>
    CreateGradientStops(GradientStop *aStops,
                        uint32_t aNumStops,
                        ExtendMode aExtendMode = ExtendMode::CLAMP) const MOZ_OVERRIDE;

  virtual TemporaryRef<FilterNode> CreateFilter(FilterType aType) MOZ_OVERRIDE;

  virtual void *GetNativeSurface(NativeSurfaceType aType) MOZ_OVERRIDE;

  bool Init(cairo_surface_t* aSurface, const IntSize& aSize, SurfaceFormat* aFormat = nullptr);
  bool Init(const IntSize& aSize, SurfaceFormat aFormat);
  bool Init(unsigned char* aData, const IntSize &aSize, int32_t aStride, SurfaceFormat aFormat);

  virtual void SetTransform(const Matrix& aTransform) MOZ_OVERRIDE;

  // Call to set up aContext for drawing (with the current transform, etc).
  // Pass the path you're going to be using if you have one.
  // Implicitly calls WillChange(aPath).
  void PrepareForDrawing(cairo_t* aContext, const Path* aPath = nullptr);

  static cairo_surface_t *GetDummySurface();

  // Cairo hardcodes this as its maximum surface size.
  static size_t GetMaxSurfaceSize() {
    return 32767;
  }

private: // methods
  // Init cairo surface without doing a cairo_surface_reference() call.
  bool InitAlreadyReferenced(cairo_surface_t* aSurface, const IntSize& aSize, SurfaceFormat* aFormat = nullptr);
  enum DrawPatternType { DRAW_FILL, DRAW_STROKE };
  void DrawPattern(const Pattern& aPattern,
                   const StrokeOptions& aStrokeOptions,
                   const DrawOptions& aOptions,
                   DrawPatternType aDrawType,
                   bool aPathBoundsClip = false);

  void CopySurfaceInternal(cairo_surface_t* aSurface,
                           const IntRect& aSource,
                           const IntPoint& aDest);

  Rect GetUserSpaceClip();

  // Call before you make any changes to the backing surface with which this
  // context is associated. Pass the path you're going to be using if you have
  // one.
  void WillChange(const Path* aPath = nullptr);

  // Call if there is any reason to disassociate the snapshot from this draw
  // target; for example, because we're going to be destroyed.
  void MarkSnapshotIndependent();

  // If the current operator is "source" then clear the destination before we
  // draw into it, to simulate the effect of an unbounded source operator.
  void ClearSurfaceForUnboundedSource(const CompositionOp &aOperator);

private: // data
  cairo_t* mContext;
  cairo_surface_t* mSurface;
  IntSize mSize;

  uint8_t* mLockedBits;

  // The latest snapshot of this surface. This needs to be told when this
  // target is modified. We keep it alive as a cache.
  RefPtr<SourceSurfaceCairo> mSnapshot;
  static cairo_surface_t *mDummySurface;
};

}
}

#endif // _MOZILLA_GFX_DRAWTARGET_CAIRO_H_
