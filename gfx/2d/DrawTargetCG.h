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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Muizelaar <jmuizelaar@mozilla.com>
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

#include <ApplicationServices/ApplicationServices.h>

#include "2D.h"
#include "Rect.h"
#include "PathCG.h"
#include "SourceSurfaceCG.h"

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
  if (aStrokeOptions.mDashLength > 1) {
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

  virtual BackendType GetType() const { return BACKEND_COREGRAPHICS; }
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
  bool Init(const IntSize &aSize, SurfaceFormat&);
  bool Init(CGContextRef cgContext, const IntSize &aSize);


  virtual void Flush() {}

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

  void *mData;

  SurfaceFormat mFormat;

  RefPtr<SourceSurfaceCGBitmapContext> mSnapshot;
};

}
}
