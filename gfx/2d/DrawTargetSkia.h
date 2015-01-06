/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_SOURCESURFACESKIA_H
#define _MOZILLA_GFX_SOURCESURFACESKIA_H

#ifdef USE_SKIA_GPU
#include "skia/GrContext.h"
#include "skia/GrGLInterface.h"
#endif

#include "skia/SkCanvas.h"

#include "2D.h"
#include "HelpersSkia.h"
#include "Rect.h"
#include "PathSkia.h"
#include <sstream>
#include <vector>

namespace mozilla {
namespace gfx {

class SourceSurfaceSkia;

class DrawTargetSkia : public DrawTarget
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetSkia, MOZ_OVERRIDE)
  DrawTargetSkia();
  virtual ~DrawTargetSkia();

  virtual DrawTargetType GetType() const MOZ_OVERRIDE;
  virtual BackendType GetBackendType() const MOZ_OVERRIDE { return BackendType::SKIA; }
  virtual TemporaryRef<SourceSurface> Snapshot() MOZ_OVERRIDE;
  virtual IntSize GetSize() MOZ_OVERRIDE { return mSize; }
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
                          const DrawOptions &aOptions = DrawOptions(),
                          const GlyphRenderingOptions *aRenderingOptions = nullptr) MOZ_OVERRIDE;
  virtual void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions = DrawOptions()) MOZ_OVERRIDE;
  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions()) MOZ_OVERRIDE;
  virtual void PushClip(const Path *aPath) MOZ_OVERRIDE;
  virtual void PushClipRect(const Rect& aRect) MOZ_OVERRIDE;
  virtual void PopClip() MOZ_OVERRIDE;
  virtual TemporaryRef<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                            const IntSize &aSize,
                                                            int32_t aStride,
                                                            SurfaceFormat aFormat) const MOZ_OVERRIDE;
  virtual TemporaryRef<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const MOZ_OVERRIDE;
  virtual TemporaryRef<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const MOZ_OVERRIDE;
  virtual TemporaryRef<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const MOZ_OVERRIDE;
  virtual TemporaryRef<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const MOZ_OVERRIDE;
  virtual TemporaryRef<GradientStops> CreateGradientStops(GradientStop *aStops, uint32_t aNumStops, ExtendMode aExtendMode = ExtendMode::CLAMP) const MOZ_OVERRIDE;
  virtual TemporaryRef<FilterNode> CreateFilter(FilterType aType) MOZ_OVERRIDE;
  virtual void SetTransform(const Matrix &aTransform) MOZ_OVERRIDE;
  virtual void *GetNativeSurface(NativeSurfaceType aType) MOZ_OVERRIDE;

  bool Init(const IntSize &aSize, SurfaceFormat aFormat);
  void Init(unsigned char* aData, const IntSize &aSize, int32_t aStride, SurfaceFormat aFormat);

#ifdef USE_SKIA_GPU
  bool InitWithGrContext(GrContext* aGrContext,
                         const IntSize &aSize,
                         SurfaceFormat aFormat) MOZ_OVERRIDE;
#endif

  operator std::string() const {
    std::stringstream stream;
    stream << "DrawTargetSkia(" << this << ")";
    return stream.str();
  }

private:
  friend class SourceSurfaceSkia;
  void SnapshotDestroyed();

  void MarkChanged();

  SkRect SkRectCoveringWholeSurface() const;

  bool UsingSkiaGPU() const;

#ifdef USE_SKIA_GPU
  RefPtrSkia<GrContext> mGrContext;
  uint32_t mTexture;
#endif

  IntSize mSize;
  RefPtrSkia<SkCanvas> mCanvas;
  SourceSurfaceSkia* mSnapshot;
};

}
}

#endif // _MOZILLA_GFX_SOURCESURFACESKIA_H
