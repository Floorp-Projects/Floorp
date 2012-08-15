/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ApplicationServices/ApplicationServices.h>

#include "2D.h"
#include "Rect.h"
#include "PathCG.h"
#include "SourceSurfaceCG.h"
#include "GLDefs.h"

namespace mozilla {
namespace gfx {

static inline CGAffineTransform
GfxMatrixToCGAffineTransform(Matrix m)
{
  CGAffineTransform t;
  t.a = m._11;
  t.b = m._12;
  t.c = m._21;
  t.d = m._22;
  t.tx = m._31;
  t.ty = m._32;
  return t;
}

static inline Rect
CGRectToRect(CGRect rect)
{
  return Rect(rect.origin.x,
              rect.origin.y,
              rect.size.width,
              rect.size.height);
}

static inline void
SetStrokeOptions(CGContextRef cg, const StrokeOptions &aStrokeOptions)
{
  switch (aStrokeOptions.mLineCap)
  {
    case CAP_BUTT:
      CGContextSetLineCap(cg, kCGLineCapButt);
      break;
    case CAP_ROUND:
      CGContextSetLineCap(cg, kCGLineCapRound);
      break;
    case CAP_SQUARE:
      CGContextSetLineCap(cg, kCGLineCapSquare);
      break;
  }

  switch (aStrokeOptions.mLineJoin)
  {
    case JOIN_BEVEL:
      CGContextSetLineJoin(cg, kCGLineJoinBevel);
      break;
    case JOIN_ROUND:
      CGContextSetLineJoin(cg, kCGLineJoinRound);
      break;
    case JOIN_MITER:
    case JOIN_MITER_OR_BEVEL:
      CGContextSetLineJoin(cg, kCGLineJoinMiter);
      break;
  }

  CGContextSetLineWidth(cg, aStrokeOptions.mLineWidth);
  CGContextSetMiterLimit(cg, aStrokeOptions.mMiterLimit);

  // XXX: rename mDashLength to dashLength
  if (aStrokeOptions.mDashLength > 0) {
    // we use a regular array instead of a std::vector here because we don't want to leak the <vector> include
    CGFloat *dashes = new CGFloat[aStrokeOptions.mDashLength];
    for (size_t i=0; i<aStrokeOptions.mDashLength; i++) {
      dashes[i] = aStrokeOptions.mDashPattern[i];
    }
    CGContextSetLineDash(cg, aStrokeOptions.mDashOffset, dashes, aStrokeOptions.mDashLength);
    delete[] dashes;
  }
}


class DrawTargetCG : public DrawTarget
{
public:
  DrawTargetCG();
  virtual ~DrawTargetCG();

  virtual BackendType GetType() const;
  virtual TemporaryRef<SourceSurface> Snapshot();

  virtual void DrawSurface(SourceSurface *aSurface,
                           const Rect &aDest,
                           const Rect &aSource,
                           const DrawSurfaceOptions &aSurfOptions = DrawSurfaceOptions(),
                           const DrawOptions &aOptions = DrawOptions());

  virtual void FillRect(const Rect &aRect,
                        const Pattern &aPattern,
                        const DrawOptions &aOptions = DrawOptions());


  //XXX: why do we take a reference to SurfaceFormat?
  bool Init(BackendType aType, const IntSize &aSize, SurfaceFormat&);
  bool Init(BackendType aType, unsigned char* aData, const IntSize &aSize, int32_t aStride, SurfaceFormat aFormat);
  bool Init(CGContextRef cgContext, const IntSize &aSize);

  // Flush if using IOSurface context
  virtual void Flush();

  virtual void DrawSurfaceWithShadow(SourceSurface *, const Point &, const Color &, const Point &, Float, CompositionOp);
  virtual void ClearRect(const Rect &);
  virtual void CopySurface(SourceSurface *, const IntRect&, const IntPoint&);
  virtual void StrokeRect(const Rect &, const Pattern &, const StrokeOptions&, const DrawOptions&);
  virtual void StrokeLine(const Point &, const Point &, const Pattern &, const StrokeOptions &, const DrawOptions &);
  virtual void Stroke(const Path *, const Pattern &, const StrokeOptions &, const DrawOptions &);
  virtual void Fill(const Path *, const Pattern &, const DrawOptions &);
  virtual void FillGlyphs(ScaledFont *, const GlyphBuffer&, const Pattern &, const DrawOptions &, const GlyphRenderingOptions *);
  virtual void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions = DrawOptions());
  virtual void PushClip(const Path *);
  virtual void PushClipRect(const Rect &aRect);
  virtual void PopClip();
  virtual TemporaryRef<SourceSurface> CreateSourceSurfaceFromNativeSurface(const NativeSurface&) const { return NULL;}
  virtual TemporaryRef<DrawTarget> CreateSimilarDrawTarget(const IntSize &, SurfaceFormat) const;
  virtual TemporaryRef<PathBuilder> CreatePathBuilder(FillRule) const;
  virtual TemporaryRef<GradientStops> CreateGradientStops(GradientStop *, uint32_t,
                                                          ExtendMode aExtendMode = EXTEND_CLAMP) const;

  virtual void *GetNativeSurface(NativeSurfaceType);

  virtual IntSize GetSize() { return mSize; }


  /* This is for creating good compatible surfaces */
  virtual TemporaryRef<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                            const IntSize &aSize,
                                                            int32_t aStride,
                                                            SurfaceFormat aFormat) const;
  virtual TemporaryRef<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const;
  CGContextRef GetCGContext() {
      return mCg;
  }
private:
  void MarkChanged();

  IntSize mSize;
  CGColorSpaceRef mColorSpace;
  CGContextRef mCg;

  GLuint mIOSurfaceTexture;

  /**
   * A pointer to the image buffer if the buffer is owned by this class (set to
   * NULL otherwise).
   * The data is not considered owned by DrawTargetCG if the DrawTarget was 
   * created for a pre-existing buffer or if the buffer's lifetime is managed
   * by CoreGraphics.
   * Data owned by DrawTargetCG will be deallocated in the destructor. 
   */
  void *mData;

  RefPtr<SourceSurfaceCGContext> mSnapshot;
};

}
}
