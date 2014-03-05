/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#ifdef USE_SKIA_GPU
#include "skia/GrContext.h"
#include "skia/GrGLInterface.h"
#endif

#include "skia/SkCanvas.h"

#include "2D.h"
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
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetSkia)
  DrawTargetSkia();
  virtual ~DrawTargetSkia();

  virtual BackendType GetType() const { return BackendType::SKIA; }
  virtual TemporaryRef<SourceSurface> Snapshot();
  virtual IntSize GetSize() { return mSize; }
  virtual void Flush();
  virtual void DrawSurface(SourceSurface *aSurface,
                           const Rect &aDest,
                           const Rect &aSource,
                           const DrawSurfaceOptions &aSurfOptions = DrawSurfaceOptions(),
                           const DrawOptions &aOptions = DrawOptions());
  virtual void DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions = DrawOptions());
  virtual void DrawSurfaceWithShadow(SourceSurface *aSurface,
                                     const Point &aDest,
                                     const Color &aColor,
                                     const Point &aOffset,
                                     Float aSigma,
                                     CompositionOp aOperator);
  virtual void ClearRect(const Rect &aRect);
  virtual void CopySurface(SourceSurface *aSurface,
                           const IntRect &aSourceRect,
                           const IntPoint &aDestination);
  virtual void FillRect(const Rect &aRect,
                        const Pattern &aPattern,
                        const DrawOptions &aOptions = DrawOptions());
  virtual void StrokeRect(const Rect &aRect,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions = StrokeOptions(),
                          const DrawOptions &aOptions = DrawOptions());
  virtual void StrokeLine(const Point &aStart,
                          const Point &aEnd,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions = StrokeOptions(),
                          const DrawOptions &aOptions = DrawOptions());
  virtual void Stroke(const Path *aPath,
                      const Pattern &aPattern,
                      const StrokeOptions &aStrokeOptions = StrokeOptions(),
                      const DrawOptions &aOptions = DrawOptions());
  virtual void Fill(const Path *aPath,
                    const Pattern &aPattern,
                    const DrawOptions &aOptions = DrawOptions());
  virtual void FillGlyphs(ScaledFont *aFont,
                          const GlyphBuffer &aBuffer,
                          const Pattern &aPattern,
                          const DrawOptions &aOptions = DrawOptions(),
                          const GlyphRenderingOptions *aRenderingOptions = nullptr);
  virtual void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions = DrawOptions());
  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions());
  virtual void PushClip(const Path *aPath);
  virtual void PushClipRect(const Rect& aRect);
  virtual void PopClip();
  virtual TemporaryRef<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                            const IntSize &aSize,
                                                            int32_t aStride,
                                                            SurfaceFormat aFormat) const;
  virtual TemporaryRef<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const;
  virtual TemporaryRef<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const;
  virtual TemporaryRef<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const;
  virtual TemporaryRef<PathBuilder> CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const;
  virtual TemporaryRef<GradientStops> CreateGradientStops(GradientStop *aStops, uint32_t aNumStops, ExtendMode aExtendMode = ExtendMode::CLAMP) const;
  virtual TemporaryRef<FilterNode> CreateFilter(FilterType aType);
  virtual void SetTransform(const Matrix &aTransform);
  virtual void *GetNativeSurface(NativeSurfaceType aType);

  bool Init(const IntSize &aSize, SurfaceFormat aFormat);
  void Init(unsigned char* aData, const IntSize &aSize, int32_t aStride, SurfaceFormat aFormat);

#ifdef USE_SKIA_GPU
  void InitWithGrContext(GrContext* aGrContext,
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

#ifdef USE_SKIA_GPU
  SkRefPtr<GrContext> mGrContext;
  uint32_t mTexture;
#endif

  IntSize mSize;
  SkRefPtr<SkCanvas> mCanvas;
  SourceSurfaceSkia* mSnapshot;
};

}
}
