/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DrawTargetCG.h"
#include "SourceSurfaceCG.h"
#include "Rect.h"
#include "ScaledFontMac.h"
#include "Tools.h"
#include <vector>
#include "QuartzSupport.h"

//CG_EXTERN void CGContextSetCompositeOperation (CGContextRef, PrivateCGCompositeMode);

// A private API that Cairo has been using for a long time
CG_EXTERN void CGContextSetCTM(CGContextRef, CGAffineTransform);

namespace mozilla {
namespace gfx {

static CGRect RectToCGRect(Rect r)
{
  return CGRectMake(r.x, r.y, r.width, r.height);
}

static CGRect IntRectToCGRect(IntRect r)
{
  return CGRectMake(r.x, r.y, r.width, r.height);
}

CGBlendMode ToBlendMode(CompositionOp op)
{
  CGBlendMode mode;
  switch (op) {
    case OP_OVER:
      mode = kCGBlendModeNormal;
      break;
    case OP_ADD:
      mode = kCGBlendModePlusLighter;
      break;
    case OP_ATOP:
      mode = kCGBlendModeSourceAtop;
      break;
    case OP_OUT:
      mode = kCGBlendModeSourceOut;
      break;
    case OP_IN:
      mode = kCGBlendModeSourceIn;
      break;
    case OP_SOURCE:
      mode = kCGBlendModeCopy;
      break;
    case OP_DEST_IN:
      mode = kCGBlendModeDestinationIn;
      break;
    case OP_DEST_OUT:
      mode = kCGBlendModeDestinationOut;
      break;
    case OP_DEST_OVER:
      mode = kCGBlendModeDestinationOver;
      break;
    case OP_DEST_ATOP:
      mode = kCGBlendModeDestinationAtop;
      break;
    case OP_XOR:
      mode = kCGBlendModeXOR;
      break;
      /*
    case OP_CLEAR:
      mode = kCGBlendModeClear;
      break;*/
    default:
      mode = kCGBlendModeNormal;
  }
  return mode;
}



DrawTargetCG::DrawTargetCG() : mSnapshot(NULL)
{
}

DrawTargetCG::~DrawTargetCG()
{
  MarkChanged();

  // We need to conditionally release these because Init can fail without initializing these.
  if (mColorSpace)
    CGColorSpaceRelease(mColorSpace);
  if (mCg)
    CGContextRelease(mCg);
  free(mData);
}

BackendType
DrawTargetCG::GetType() const
{
  // It may be worth spliting Bitmap and IOSurface DrawTarget
  // into seperate classes.
  if (GetContextType(mCg) == CG_CONTEXT_TYPE_IOSURFACE) {
    return BACKEND_COREGRAPHICS_ACCELERATED;
  } else {
    return BACKEND_COREGRAPHICS;
  }
}

TemporaryRef<SourceSurface>
DrawTargetCG::Snapshot()
{
  if (!mSnapshot) {
    if (GetContextType(mCg) == CG_CONTEXT_TYPE_IOSURFACE) {
      return new SourceSurfaceCGIOSurfaceContext(this);
    } else {
      mSnapshot = new SourceSurfaceCGBitmapContext(this);
    }
  }

  return mSnapshot;
}

TemporaryRef<DrawTarget>
DrawTargetCG::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  // XXX: in thebes we use CGLayers to do this kind of thing. It probably makes sense
  // to add that in somehow, but at a higher level
  RefPtr<DrawTargetCG> newTarget = new DrawTargetCG();
  if (newTarget->Init(GetType(), aSize, aFormat)) {
    return newTarget;
  } else {
    return NULL;
  }
}

TemporaryRef<SourceSurface>
DrawTargetCG::CreateSourceSurfaceFromData(unsigned char *aData,
                                           const IntSize &aSize,
                                           int32_t aStride,
                                           SurfaceFormat aFormat) const
{
  RefPtr<SourceSurfaceCG> newSurf = new SourceSurfaceCG();

 if (!newSurf->InitFromData(aData, aSize, aStride, aFormat)) {
    return NULL;
  }

  return newSurf;
}

static CGImageRef
GetImageFromSourceSurface(SourceSurface *aSurface)
{
  if (aSurface->GetType() == SURFACE_COREGRAPHICS_IMAGE)
    return static_cast<SourceSurfaceCG*>(aSurface)->GetImage();
  else if (aSurface->GetType() == SURFACE_COREGRAPHICS_CGCONTEXT)
    return static_cast<SourceSurfaceCGContext*>(aSurface)->GetImage();
  else if (aSurface->GetType() == SURFACE_DATA)
    return static_cast<DataSourceSurfaceCG*>(aSurface)->GetImage();
  abort();
}

TemporaryRef<SourceSurface>
DrawTargetCG::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  return NULL;
}

class UnboundnessFixer
{
    CGRect mClipBounds;
    CGLayerRef mLayer;
    CGContextRef mCg;
  public:
    UnboundnessFixer() : mCg(NULL) {}

    CGContextRef Check(CGContextRef baseCg, CompositionOp blend)
    {
      if (!IsOperatorBoundByMask(blend)) {
        mClipBounds = CGContextGetClipBoundingBox(baseCg);
        // TransparencyLayers aren't blended using the blend mode so
        // we are forced to use CGLayers

        //XXX: The size here is in default user space units, of the layer relative to the graphics context.
        // is the clip bounds still correct if, for example, we have a scale applied to the context?
        mLayer = CGLayerCreateWithContext(baseCg, mClipBounds.size, NULL);
        //XXX: if the size is 0x0 we get a NULL CGContext back from GetContext
        mCg = CGLayerGetContext(mLayer);
        // CGContext's default to have the origin at the bottom left
        // so flip it to the top left and adjust for the origin
        // of the layer
        CGContextTranslateCTM(mCg, -mClipBounds.origin.x, mClipBounds.origin.y + mClipBounds.size.height);
        CGContextScaleCTM(mCg, 1, -1);

        return mCg;
      } else {
        return baseCg;
      }
    }

    void Fix(CGContextRef baseCg)
    {
        if (mCg) {
            CGContextTranslateCTM(baseCg, 0, mClipBounds.size.height);
            CGContextScaleCTM(baseCg, 1, -1);
            mClipBounds.origin.y *= -1;
            CGContextDrawLayerAtPoint(baseCg, mClipBounds.origin, mLayer);
            CGContextRelease(mCg);
        }
    }
};

void
DrawTargetCG::DrawSurface(SourceSurface *aSurface,
                           const Rect &aDest,
                           const Rect &aSource,
                           const DrawSurfaceOptions &aSurfOptions,
                           const DrawOptions &aDrawOptions)
{
  MarkChanged();

  CGImageRef image;
  CGImageRef subimage = NULL;
  CGContextSaveGState(mCg);

  CGContextSetBlendMode(mCg, ToBlendMode(aDrawOptions.mCompositionOp));
  UnboundnessFixer fixer;
  CGContextRef cg = fixer.Check(mCg, aDrawOptions.mCompositionOp);
  CGContextSetAlpha(cg, aDrawOptions.mAlpha);

  CGContextConcatCTM(cg, GfxMatrixToCGAffineTransform(mTransform));
  image = GetImageFromSourceSurface(aSurface);
  /* we have two options here:
   *  - create a subimage -- this is slower
   *  - fancy things with clip and different dest rects */
  {
    subimage = CGImageCreateWithImageInRect(image, RectToCGRect(aSource));
    image = subimage;
  }

  CGContextScaleCTM(cg, 1, -1);

  CGRect flippedRect = CGRectMake(aDest.x, -(aDest.y + aDest.height),
                                  aDest.width, aDest.height);

  //XXX: we should implement this for patterns too
  if (aSurfOptions.mFilter == FILTER_POINT)
    CGContextSetInterpolationQuality(cg, kCGInterpolationNone);

  CGContextDrawImage(cg, flippedRect, image);

  fixer.Fix(mCg);

  CGContextRestoreGState(mCg);

  CGImageRelease(subimage);
}

static CGColorRef ColorToCGColor(CGColorSpaceRef aColorSpace, const Color& aColor)
{
  CGFloat components[4] = {aColor.r, aColor.g, aColor.b, aColor.a};
  return CGColorCreate(aColorSpace, components);
}

class GradientStopsCG : public GradientStops
{
  public:
  //XXX: The skia backend uses a vector and passes in aNumStops. It should do better
  GradientStopsCG(GradientStop* aStops, uint32_t aNumStops, ExtendMode aExtendMode)
  {
    //XXX: do the stops need to be in any particular order?
    // what should we do about the color space here? we certainly shouldn't be
    // recreating it all the time
    std::vector<CGFloat> colors;
    std::vector<CGFloat> offsets;
    colors.reserve(aNumStops*4);
    offsets.reserve(aNumStops);

    for (uint32_t i = 0; i < aNumStops; i++) {
      colors.push_back(aStops[i].color.r);
      colors.push_back(aStops[i].color.g);
      colors.push_back(aStops[i].color.b);
      colors.push_back(aStops[i].color.a);

      offsets.push_back(aStops[i].offset);
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    mGradient = CGGradientCreateWithColorComponents(colorSpace,
                                                    &colors.front(),
                                                    &offsets.front(),
                                                    aNumStops);
    CGColorSpaceRelease(colorSpace);
  }
  virtual ~GradientStopsCG() {
    CGGradientRelease(mGradient);
  }
  // Will always report BACKEND_COREGRAPHICS, but it is compatible
  // with BACKEND_COREGRAPHICS_ACCELERATED
  BackendType GetBackendType() const { return BACKEND_COREGRAPHICS; }
  CGGradientRef mGradient;
};

TemporaryRef<GradientStops>
DrawTargetCG::CreateGradientStops(GradientStop *aStops, uint32_t aNumStops,
                                  ExtendMode aExtendMode) const
{
  return new GradientStopsCG(aStops, aNumStops, aExtendMode);
}

static void
DrawGradient(CGContextRef cg, const Pattern &aPattern)
{
  if (aPattern.GetType() == PATTERN_LINEAR_GRADIENT) {
    const LinearGradientPattern& pat = static_cast<const LinearGradientPattern&>(aPattern);
    GradientStopsCG *stops = static_cast<GradientStopsCG*>(pat.mStops.get());
    // XXX: we should take the m out of the properties of LinearGradientPatterns
    CGPoint startPoint = { pat.mBegin.x, pat.mBegin.y };
    CGPoint endPoint   = { pat.mEnd.x,   pat.mEnd.y };

    // Canvas spec states that we should avoid drawing degenerate gradients (XXX: should this be in common code?)
    //if (startPoint.x == endPoint.x && startPoint.y == endPoint.y)
    //  return;

    CGContextDrawLinearGradient(cg, stops->mGradient, startPoint, endPoint,
                                kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
  } else if (aPattern.GetType() == PATTERN_RADIAL_GRADIENT) {
    const RadialGradientPattern& pat = static_cast<const RadialGradientPattern&>(aPattern);
    GradientStopsCG *stops = static_cast<GradientStopsCG*>(pat.mStops.get());

    // XXX: we should take the m out of the properties of RadialGradientPatterns
    CGPoint startCenter = { pat.mCenter1.x, pat.mCenter1.y };
    CGFloat startRadius = pat.mRadius1;
    CGPoint endCenter   = { pat.mCenter2.x, pat.mCenter2.y };
    CGFloat endRadius   = pat.mRadius2;

    //XXX: are there degenerate radial gradients that we should avoid drawing?
    CGContextDrawRadialGradient(cg, stops->mGradient, startCenter, startRadius, endCenter, endRadius,
                                kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
  } else {
    assert(0);
  }

}

static void
drawPattern(void *info, CGContextRef context)
{
  CGImageRef image = static_cast<CGImageRef>(info);
  CGRect rect = {{0, 0},
    {static_cast<CGFloat>(CGImageGetWidth(image)),
     static_cast<CGFloat>(CGImageGetHeight(image))}};
  CGContextDrawImage(context, rect, image);
}

static void
releaseInfo(void *info)
{
  CGImageRef image = static_cast<CGImageRef>(info);
  CGImageRelease(image);
}

CGPatternCallbacks patternCallbacks = {
  0,
  drawPattern,
  releaseInfo
};

static bool
isGradient(const Pattern &aPattern)
{
  return aPattern.GetType() == PATTERN_LINEAR_GRADIENT || aPattern.GetType() == PATTERN_RADIAL_GRADIENT;
}

/* CoreGraphics patterns ignore the userspace transform so
 * we need to multiply it in */
static CGPatternRef
CreateCGPattern(const Pattern &aPattern, CGAffineTransform aUserSpace)
{
  const SurfacePattern& pat = static_cast<const SurfacePattern&>(aPattern);
  // XXX: is .get correct here?
  CGImageRef image = GetImageFromSourceSurface(pat.mSurface.get());
  CGFloat xStep, yStep;
  switch (pat.mExtendMode) {
    case EXTEND_CLAMP:
      // The 1 << 22 comes from Webkit see Pattern::createPlatformPattern() in PatternCG.cpp for more info
      xStep = static_cast<CGFloat>(1 << 22);
      yStep = static_cast<CGFloat>(1 << 22);
      break;
    case EXTEND_REFLECT:
      assert(0);
    case EXTEND_REPEAT:
      xStep = static_cast<CGFloat>(CGImageGetWidth(image));
      yStep = static_cast<CGFloat>(CGImageGetHeight(image));
      // webkit uses wkCGPatternCreateWithImageAndTransform a wrapper around CGPatternCreateWithImage2
      // this is done to avoid pixel-cracking along pattern boundaries
      // (see https://bugs.webkit.org/show_bug.cgi?id=53055)
      // typedef enum {
      //    wkPatternTilingNoDistortion,
      //    wkPatternTilingConstantSpacingMinimalDistortion,
      //    wkPatternTilingConstantSpacing
      // } wkPatternTiling;
      // extern CGPatternRef (*wkCGPatternCreateWithImageAndTransform)(CGImageRef, CGAffineTransform, int);
  }

  //XXX: We should be using CGContextDrawTiledImage when we can. Even though it
  // creates a pattern, it seems to go down a faster path than using a delegate
  // like we do below
  CGRect bounds = {
    {0, 0,},
    {static_cast<CGFloat>(CGImageGetWidth(image)), static_cast<CGFloat>(CGImageGetHeight(image))}
  };
  CGAffineTransform transform = CGAffineTransformConcat(CGAffineTransformMakeScale(1, -1), aUserSpace);
  transform = CGAffineTransformTranslate(transform, 0, -static_cast<float>(CGImageGetHeight(image)));
  return CGPatternCreate(CGImageRetain(image), bounds, transform, xStep, yStep, kCGPatternTilingConstantSpacing,
                         true, &patternCallbacks);
}

static void
SetFillFromPattern(CGContextRef cg, CGColorSpaceRef aColorSpace, const Pattern &aPattern)
{
  assert(!isGradient(aPattern));
  if (aPattern.GetType() == PATTERN_COLOR) {

    const Color& color = static_cast<const ColorPattern&>(aPattern).mColor;
    //XXX: we should cache colors
    CGColorRef cgcolor = ColorToCGColor(aColorSpace, color);
    CGContextSetFillColorWithColor(cg, cgcolor);
    CGColorRelease(cgcolor);
  } else if (aPattern.GetType() == PATTERN_SURFACE) {

    CGColorSpaceRef patternSpace;
    patternSpace = CGColorSpaceCreatePattern (NULL);
    CGContextSetFillColorSpace(cg, patternSpace);
    CGColorSpaceRelease(patternSpace);

    CGPatternRef pattern = CreateCGPattern(aPattern, CGContextGetCTM(cg));
    CGFloat alpha = 1.;
    CGContextSetFillPattern(cg, pattern, &alpha);
    CGPatternRelease(pattern);
  }
}

static void
SetStrokeFromPattern(CGContextRef cg, CGColorSpaceRef aColorSpace, const Pattern &aPattern)
{
  assert(!isGradient(aPattern));
  if (aPattern.GetType() == PATTERN_COLOR) {
    const Color& color = static_cast<const ColorPattern&>(aPattern).mColor;
    //XXX: we should cache colors
    CGColorRef cgcolor = ColorToCGColor(aColorSpace, color);
    CGContextSetStrokeColorWithColor(cg, cgcolor);
    CGColorRelease(cgcolor);
  } else if (aPattern.GetType() == PATTERN_SURFACE) {
    CGColorSpaceRef patternSpace;
    patternSpace = CGColorSpaceCreatePattern (NULL);
    CGContextSetStrokeColorSpace(cg, patternSpace);
    CGColorSpaceRelease(patternSpace);

    CGPatternRef pattern = CreateCGPattern(aPattern, CGContextGetCTM(cg));
    CGFloat alpha = 1.;
    CGContextSetStrokePattern(cg, pattern, &alpha);
    CGPatternRelease(pattern);
  }

}


void
DrawTargetCG::FillRect(const Rect &aRect,
                        const Pattern &aPattern,
                        const DrawOptions &aDrawOptions)
{
  MarkChanged();

  CGContextSaveGState(mCg);

  UnboundnessFixer fixer;
  CGContextRef cg = fixer.Check(mCg, aDrawOptions.mCompositionOp);
  CGContextSetAlpha(mCg, aDrawOptions.mAlpha);
  CGContextSetBlendMode(mCg, ToBlendMode(aDrawOptions.mCompositionOp));

  CGContextConcatCTM(cg, GfxMatrixToCGAffineTransform(mTransform));

  if (isGradient(aPattern)) {
    CGContextClipToRect(cg, RectToCGRect(aRect));
    DrawGradient(cg, aPattern);
  } else {
    SetFillFromPattern(cg, mColorSpace, aPattern);
    CGContextFillRect(cg, RectToCGRect(aRect));
  }

  fixer.Fix(mCg);
  CGContextRestoreGState(mCg);
}

void
DrawTargetCG::StrokeLine(const Point &p1, const Point &p2, const Pattern &aPattern, const StrokeOptions &aStrokeOptions, const DrawOptions &aDrawOptions)
{
  MarkChanged();

  CGContextSaveGState(mCg);

  UnboundnessFixer fixer;
  CGContextRef cg = fixer.Check(mCg, aDrawOptions.mCompositionOp);
  CGContextSetAlpha(mCg, aDrawOptions.mAlpha);
  CGContextSetBlendMode(mCg, ToBlendMode(aDrawOptions.mCompositionOp));

  CGContextConcatCTM(cg, GfxMatrixToCGAffineTransform(mTransform));

  CGContextBeginPath(cg);
  CGContextMoveToPoint(cg, p1.x, p1.y);
  CGContextAddLineToPoint(cg, p2.x, p2.y);

  SetStrokeOptions(cg, aStrokeOptions);

  if (isGradient(aPattern)) {
    CGContextReplacePathWithStrokedPath(cg);
    //XXX: should we use EO clip here?
    CGContextClip(cg);
    DrawGradient(cg, aPattern);
  } else {
    SetStrokeFromPattern(cg, mColorSpace, aPattern);
    CGContextStrokePath(cg);
  }

  fixer.Fix(mCg);
  CGContextRestoreGState(mCg);
}

void
DrawTargetCG::StrokeRect(const Rect &aRect,
                         const Pattern &aPattern,
                         const StrokeOptions &aStrokeOptions,
                         const DrawOptions &aDrawOptions)
{
  MarkChanged();

  CGContextSaveGState(mCg);

  UnboundnessFixer fixer;
  CGContextRef cg = fixer.Check(mCg, aDrawOptions.mCompositionOp);
  CGContextSetAlpha(mCg, aDrawOptions.mAlpha);
  CGContextSetBlendMode(mCg, ToBlendMode(aDrawOptions.mCompositionOp));

  CGContextConcatCTM(cg, GfxMatrixToCGAffineTransform(mTransform));

  // we don't need to set all of the stroke state because
  // it doesn't apply when stroking rects
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

  if (isGradient(aPattern)) {
    // There's no CGContextClipStrokeRect so we do it by hand
    CGContextBeginPath(cg);
    CGContextAddRect(cg, RectToCGRect(aRect));
    CGContextReplacePathWithStrokedPath(cg);
    //XXX: should we use EO clip here?
    CGContextClip(cg);
    DrawGradient(cg, aPattern);
  } else {
    SetStrokeFromPattern(cg, mColorSpace, aPattern);
    CGContextStrokeRect(cg, RectToCGRect(aRect));
  }

  fixer.Fix(mCg);
  CGContextRestoreGState(mCg);
}


void
DrawTargetCG::ClearRect(const Rect &aRect)
{
  MarkChanged();

  CGContextSaveGState(mCg);
  CGContextConcatCTM(mCg, GfxMatrixToCGAffineTransform(mTransform));

  CGContextClearRect(mCg, RectToCGRect(aRect));

  CGContextRestoreGState(mCg);
}

void
DrawTargetCG::Stroke(const Path *aPath, const Pattern &aPattern, const StrokeOptions &aStrokeOptions, const DrawOptions &aDrawOptions)
{
  MarkChanged();

  CGContextSaveGState(mCg);

  UnboundnessFixer fixer;
  CGContextRef cg = fixer.Check(mCg, aDrawOptions.mCompositionOp);
  CGContextSetAlpha(mCg, aDrawOptions.mAlpha);
  CGContextSetBlendMode(mCg, ToBlendMode(aDrawOptions.mCompositionOp));

  CGContextConcatCTM(cg, GfxMatrixToCGAffineTransform(mTransform));


  CGContextBeginPath(cg);

  assert(aPath->GetBackendType() == BACKEND_COREGRAPHICS);
  const PathCG *cgPath = static_cast<const PathCG*>(aPath);
  CGContextAddPath(cg, cgPath->GetPath());

  SetStrokeOptions(cg, aStrokeOptions);

  if (isGradient(aPattern)) {
    CGContextReplacePathWithStrokedPath(cg);
    //XXX: should we use EO clip here?
    CGContextClip(cg);
    DrawGradient(cg, aPattern);
  } else {
    CGContextBeginPath(cg);
    // XXX: we could put fill mode into the path fill rule if we wanted
    const PathCG *cgPath = static_cast<const PathCG*>(aPath);
    CGContextAddPath(cg, cgPath->GetPath());

    SetStrokeFromPattern(cg, mColorSpace, aPattern);
    CGContextStrokePath(cg);
  }

  fixer.Fix(mCg);
  CGContextRestoreGState(mCg);
}

void
DrawTargetCG::Fill(const Path *aPath, const Pattern &aPattern, const DrawOptions &aDrawOptions)
{
  MarkChanged();

  assert(aPath->GetBackendType() == BACKEND_COREGRAPHICS);

  CGContextSaveGState(mCg);

  CGContextSetBlendMode(mCg, ToBlendMode(aDrawOptions.mCompositionOp));
  UnboundnessFixer fixer;
  CGContextRef cg = fixer.Check(mCg, aDrawOptions.mCompositionOp);
  CGContextSetAlpha(cg, aDrawOptions.mAlpha);

  CGContextConcatCTM(cg, GfxMatrixToCGAffineTransform(mTransform));

  CGContextBeginPath(cg);
  // XXX: we could put fill mode into the path fill rule if we wanted
  const PathCG *cgPath = static_cast<const PathCG*>(aPath);

  if (isGradient(aPattern)) {
    // setup a clip to draw the gradient through
    if (CGPathIsEmpty(cgPath->GetPath())) {
      // Adding an empty path will cause us not to clip
      // so clip everything explicitly
      CGContextClipToRect(mCg, CGRectZero);
    } else {
      CGContextAddPath(cg, cgPath->GetPath());
      if (cgPath->GetFillRule() == FILL_EVEN_ODD)
        CGContextEOClip(mCg);
      else
        CGContextClip(mCg);
    }

    DrawGradient(cg, aPattern);
  } else {
    CGContextAddPath(cg, cgPath->GetPath());

    SetFillFromPattern(cg, mColorSpace, aPattern);

    if (cgPath->GetFillRule() == FILL_EVEN_ODD)
      CGContextEOFillPath(cg);
    else
      CGContextFillPath(cg);
  }

  fixer.Fix(mCg);
  CGContextRestoreGState(mCg);
}


void
DrawTargetCG::FillGlyphs(ScaledFont *aFont, const GlyphBuffer &aBuffer, const Pattern &aPattern, const DrawOptions &aDrawOptions,
                         const GlyphRenderingOptions*)
{
  MarkChanged();

  assert(aBuffer.mNumGlyphs);
  CGContextSaveGState(mCg);

  CGContextSetBlendMode(mCg, ToBlendMode(aDrawOptions.mCompositionOp));
  UnboundnessFixer fixer;
  CGContextRef cg = fixer.Check(mCg, aDrawOptions.mCompositionOp);
  CGContextSetAlpha(cg, aDrawOptions.mAlpha);

  CGContextConcatCTM(cg, GfxMatrixToCGAffineTransform(mTransform));

  ScaledFontMac* cgFont = static_cast<ScaledFontMac*>(aFont);
  CGContextSetFont(cg, cgFont->mFont);
  CGContextSetFontSize(cg, cgFont->mSize);

  //XXX: we should use a stack vector here when we have a class like that
  std::vector<CGGlyph> glyphs;
  std::vector<CGPoint> positions;
  glyphs.resize(aBuffer.mNumGlyphs);
  positions.resize(aBuffer.mNumGlyphs);

  // Handle the flip
  CGAffineTransform matrix = CGAffineTransformMakeScale(1, -1);
  CGContextConcatCTM(cg, matrix);
  // CGContextSetTextMatrix works differently with kCGTextClip && kCGTextFill
  // It seems that it transforms the positions with TextFill and not with TextClip
  // Therefore we'll avoid it. See also:
  // http://cgit.freedesktop.org/cairo/commit/?id=9c0d761bfcdd28d52c83d74f46dd3c709ae0fa69

  for (unsigned int i = 0; i < aBuffer.mNumGlyphs; i++) {
    glyphs[i] = aBuffer.mGlyphs[i].mIndex;
    // XXX: CGPointMake might not be inlined
    positions[i] = CGPointMake(aBuffer.mGlyphs[i].mPosition.x,
                              -aBuffer.mGlyphs[i].mPosition.y);
  }

  //XXX: CGContextShowGlyphsAtPositions is 10.5+ for older versions use CGContextShowGlyphsWithAdvances
  if (isGradient(aPattern)) {
    CGContextSetTextDrawingMode(cg, kCGTextClip);
    CGContextShowGlyphsAtPositions(cg, &glyphs.front(), &positions.front(), aBuffer.mNumGlyphs);
    DrawGradient(cg, aPattern);
  } else {
    //XXX: with CoreGraphics we can stroke text directly instead of going
    // through GetPath. It would be nice to add support for using that
    CGContextSetTextDrawingMode(cg, kCGTextFill);
    SetFillFromPattern(cg, mColorSpace, aPattern);
    CGContextShowGlyphsAtPositions(cg, &glyphs.front(), &positions.front(), aBuffer.mNumGlyphs);
  }

  fixer.Fix(mCg);
  CGContextRestoreGState(cg);
}

extern "C" {
void
CGContextResetClip(CGContextRef);
};

void
DrawTargetCG::CopySurface(SourceSurface *aSurface,
                          const IntRect& aSourceRect,
                          const IntPoint &aDestination)
{
  MarkChanged();

  CGImageRef image;
  CGImageRef subimage = NULL;
  if (aSurface->GetType() == SURFACE_COREGRAPHICS_IMAGE) {
    image = GetImageFromSourceSurface(aSurface);
    /* we have two options here:
     *  - create a subimage -- this is slower
     *  - fancy things with clip and different dest rects */
    {
      subimage = CGImageCreateWithImageInRect(image, IntRectToCGRect(aSourceRect));
      image = subimage;
    }
    // XXX: it might be more efficient for us to do the copy directly if we have access to the bits

    CGContextSaveGState(mCg);

    // CopySurface ignores the clip, so we need to use private API to temporarily reset it
    CGContextResetClip(mCg);
    CGContextSetBlendMode(mCg, kCGBlendModeCopy);

    CGContextScaleCTM(mCg, 1, -1);

    CGRect flippedRect = CGRectMake(aDestination.x, -(aDestination.y + aSourceRect.height),
                                    aSourceRect.width, aSourceRect.height);

    CGContextDrawImage(mCg, flippedRect, image);

    CGContextRestoreGState(mCg);

    CGImageRelease(subimage);
  }
}

void
DrawTargetCG::DrawSurfaceWithShadow(SourceSurface *aSurface, const Point &aDest, const Color &aColor, const Point &aOffset, Float aSigma, CompositionOp aOperator)
{
  MarkChanged();

  CGImageRef image;
  image = GetImageFromSourceSurface(aSurface);

  IntSize size = aSurface->GetSize();
  CGContextSaveGState(mCg);
  //XXX do we need to do the fixup here?
  CGContextSetBlendMode(mCg, ToBlendMode(aOperator));

  CGContextScaleCTM(mCg, 1, -1);

  CGRect flippedRect = CGRectMake(aDest.x, -(aDest.y + size.height),
                                  size.width, size.height);

  CGColorRef color = ColorToCGColor(mColorSpace, aColor);
  CGSize offset = {aOffset.x, -aOffset.y};
  // CoreGraphics needs twice sigma as it's amount of blur
  CGContextSetShadowWithColor(mCg, offset, 2*aSigma, color);
  CGColorRelease(color);

  CGContextDrawImage(mCg, flippedRect, image);

  CGContextRestoreGState(mCg);

}

bool
DrawTargetCG::Init(BackendType aType,
                   unsigned char* aData,
                   const IntSize &aSize,
                   int32_t aStride,
                   SurfaceFormat aFormat)
{
  // XXX: we should come up with some consistent semantics for dealing
  // with zero area drawtargets
  if (aSize.width <= 0 || aSize.height <= 0 ||
      // 32767 is the maximum size supported by cairo
      // we clamp to that to make it easier to interoperate
      aSize.width > 32767 || aSize.height > 32767) {
    mColorSpace = NULL;
    mCg = NULL;
    mData = NULL;
    return false;
  }

  //XXX: handle SurfaceFormat

  //XXX: we'd be better off reusing the Colorspace across draw targets
  mColorSpace = CGColorSpaceCreateDeviceRGB();

  if (aData == NULL && aType != BACKEND_COREGRAPHICS_ACCELERATED) {
    // XXX: Currently, Init implicitly clears, that can often be a waste of time
    mData = calloc(aSize.height * aStride, 1);
    aData = static_cast<unsigned char*>(mData);  
  } else {
    // mData == NULL means DrawTargetCG doesn't own the image data and will not
    // delete it in the destructor
    mData = NULL;
  }

  mSize = aSize;

  if (aType == BACKEND_COREGRAPHICS_ACCELERATED) {
    RefPtr<MacIOSurface> ioSurface = MacIOSurface::CreateIOSurface(aSize.width, aSize.height);
    mCg = ioSurface->CreateIOSurfaceContext();
    // If we don't have the symbol for 'CreateIOSurfaceContext' mCg will be null
    // and we will fallback to software below
    mData = NULL;
  }

  if (!mCg || aType == BACKEND_COREGRAPHICS) {
    int bitsPerComponent = 8;

    CGBitmapInfo bitinfo;
    bitinfo = kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst;

    // XXX: what should we do if this fails?
    mCg = CGBitmapContextCreate (aData,
                                 mSize.width,
                                 mSize.height,
                                 bitsPerComponent,
                                 aStride,
                                 mColorSpace,
                                 bitinfo);
  }

  assert(mCg);
  // CGContext's default to have the origin at the bottom left
  // so flip it to the top left
  CGContextTranslateCTM(mCg, 0, mSize.height);
  CGContextScaleCTM(mCg, 1, -1);
  // See Bug 722164 for performance details
  // Medium or higher quality lead to expensive interpolation
  // for canvas we want to use low quality interpolation
  // to have competitive performance with other canvas
  // implementation.
  // XXX: Create input parameter to control interpolation and
  //      use the default for content.
  CGContextSetInterpolationQuality(mCg, kCGInterpolationLow);

  // XXX: set correct format
  mFormat = FORMAT_B8G8R8A8;

  if (aType == BACKEND_COREGRAPHICS_ACCELERATED) {
    // The bitmap backend uses callac to clear, we can't do that without
    // reading back the surface. This should trigger something equivilent
    // to glClear.
    ClearRect(Rect(0, 0, mSize.width, mSize.height));
  }

  return true;
}

void
DrawTargetCG::Flush()
{
  CGContextFlush(mCg);
}

bool
DrawTargetCG::Init(CGContextRef cgContext, const IntSize &aSize)
{
  // XXX: we should come up with some consistent semantics for dealing
  // with zero area drawtargets
  if (aSize.width == 0 || aSize.height == 0) {
    mColorSpace = NULL;
    mCg = NULL;
    mData = NULL;
    return false;
  }

  //XXX: handle SurfaceFormat

  //XXX: we'd be better off reusing the Colorspace across draw targets
  mColorSpace = CGColorSpaceCreateDeviceRGB();

  mSize = aSize;

  mCg = cgContext;

  mData = NULL;

  assert(mCg);
  // CGContext's default to have the origin at the bottom left
  // so flip it to the top left
  CGContextTranslateCTM(mCg, 0, mSize.height);
  CGContextScaleCTM(mCg, 1, -1);

  //XXX: set correct format
  mFormat = FORMAT_B8G8R8A8;

  return true;
}

bool
DrawTargetCG::Init(BackendType aType, const IntSize &aSize, SurfaceFormat &aFormat)
{
  int stride = aSize.width*4;
  
  // Calling Init with aData == NULL will allocate.
  return Init(aType, NULL, aSize, stride, aFormat);
}

TemporaryRef<PathBuilder>
DrawTargetCG::CreatePathBuilder(FillRule aFillRule) const
{
  RefPtr<PathBuilderCG> pb = new PathBuilderCG(aFillRule);
  return pb;
}

void*
DrawTargetCG::GetNativeSurface(NativeSurfaceType aType)
{
  if (aType == NATIVE_SURFACE_CGCONTEXT && GetContextType(mCg) == CG_CONTEXT_TYPE_BITMAP ||
      aType == NATIVE_SURFACE_CGCONTEXT_ACCELERATED && GetContextType(mCg) == CG_CONTEXT_TYPE_IOSURFACE) {
    return mCg;
  } else {
    return NULL;
  }
}

void
DrawTargetCG::Mask(const Pattern &aSource,
                   const Pattern &aMask,
                   const DrawOptions &aDrawOptions)
{
  MarkChanged();

  CGContextSaveGState(mCg);

  if (isGradient(aMask)) {
    assert(0);
  } else {
    if (aMask.GetType() == PATTERN_COLOR) {
      DrawOptions drawOptions(aDrawOptions);
      const Color& color = static_cast<const ColorPattern&>(aMask).mColor;
      drawOptions.mAlpha *= color.a;
      assert(0);
      // XXX: we need to get a rect that when transformed covers the entire surface
      //Rect
      //FillRect(rect, aSource, drawOptions);
    } else if (aMask.GetType() == PATTERN_SURFACE) {
      const SurfacePattern& pat = static_cast<const SurfacePattern&>(aMask);
      CGImageRef mask = GetImageFromSourceSurface(pat.mSurface.get());
      Rect rect(0,0, CGImageGetWidth(mask), CGImageGetHeight(mask));
      // XXX: probably we need to do some flipping of the image or something
      CGContextClipToMask(mCg, RectToCGRect(rect), mask);
      FillRect(rect, aSource, aDrawOptions);
    }
  }

  CGContextRestoreGState(mCg);
}

void
DrawTargetCG::PushClipRect(const Rect &aRect)
{
  CGContextSaveGState(mCg);

  /* We go through a bit of trouble to temporarilly set the transform
   * while we add the path */
  CGAffineTransform previousTransform = CGContextGetCTM(mCg);
  CGContextConcatCTM(mCg, GfxMatrixToCGAffineTransform(mTransform));
  CGContextClipToRect(mCg, RectToCGRect(aRect));
  CGContextSetCTM(mCg, previousTransform);
}


void
DrawTargetCG::PushClip(const Path *aPath)
{
  CGContextSaveGState(mCg);

  CGContextBeginPath(mCg);
  assert(aPath->GetBackendType() == BACKEND_COREGRAPHICS);

  const PathCG *cgPath = static_cast<const PathCG*>(aPath);

  // Weirdly, CoreGraphics clips empty paths as all shown
  // but emtpy rects as all clipped.  We detect this situation and
  // workaround it appropriately
  if (CGPathIsEmpty(cgPath->GetPath())) {
    // XXX: should we return here?
    CGContextClipToRect(mCg, CGRectZero);
  }


  /* We go through a bit of trouble to temporarilly set the transform
   * while we add the path. XXX: this could be improved if we keep
   * the CTM as resident state on the DrawTarget. */
  CGContextSaveGState(mCg);
  CGContextConcatCTM(mCg, GfxMatrixToCGAffineTransform(mTransform));
  CGContextAddPath(mCg, cgPath->GetPath());
  CGContextRestoreGState(mCg);

  if (cgPath->GetFillRule() == FILL_EVEN_ODD)
    CGContextEOClip(mCg);
  else
    CGContextClip(mCg);
}

void
DrawTargetCG::PopClip()
{
  CGContextRestoreGState(mCg);
}

void
DrawTargetCG::MarkChanged()
{
  if (mSnapshot) {
    if (mSnapshot->refCount() > 1) {
      // We only need to worry about snapshots that someone else knows about
      mSnapshot->DrawTargetWillChange();
    }
    mSnapshot = NULL;
  }
}



}
}
