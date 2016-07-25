/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetSkia.h"
#include "SourceSurfaceSkia.h"
#include "ScaledFontBase.h"
#include "ScaledFontCairo.h"
#include "skia/include/core/SkBitmapDevice.h"
#include "FilterNodeSoftware.h"
#include "HelpersSkia.h"

#include "skia/include/core/SkSurface.h"
#include "skia/include/core/SkTypeface.h"
#include "skia/include/effects/SkGradientShader.h"
#include "skia/include/core/SkColorFilter.h"
#include "skia/include/effects/SkBlurImageFilter.h"
#include "skia/include/effects/SkLayerRasterizer.h"
#include "Blur.h"
#include "Logging.h"
#include "Tools.h"
#include "DataSurfaceHelpers.h"
#include <algorithm>

#ifdef USE_SKIA_GPU
#include "GLDefs.h"
#include "skia/include/gpu/SkGr.h"
#include "skia/include/gpu/GrContext.h"
#include "skia/include/gpu/gl/GrGLInterface.h"
#endif

#ifdef MOZ_WIDGET_COCOA
#include <ApplicationServices/ApplicationServices.h>
#include "mozilla/Vector.h"
#include "ScaledFontMac.h"
#include "DrawTargetCG.h"
#include "CGTextDrawing.h"
#endif

namespace mozilla {
namespace gfx {

class GradientStopsSkia : public GradientStops
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GradientStopsSkia)
  GradientStopsSkia(const std::vector<GradientStop>& aStops, uint32_t aNumStops, ExtendMode aExtendMode)
    : mCount(aNumStops)
    , mExtendMode(aExtendMode)
  {
    if (mCount == 0) {
      return;
    }

    // Skia gradients always require a stop at 0.0 and 1.0, insert these if
    // we don't have them.
    uint32_t shift = 0;
    if (aStops[0].offset != 0) {
      mCount++;
      shift = 1;
    }
    if (aStops[aNumStops-1].offset != 1) {
      mCount++;
    }
    mColors.resize(mCount);
    mPositions.resize(mCount);
    if (aStops[0].offset != 0) {
      mColors[0] = ColorToSkColor(aStops[0].color, 1.0);
      mPositions[0] = 0;
    }
    for (uint32_t i = 0; i < aNumStops; i++) {
      mColors[i + shift] = ColorToSkColor(aStops[i].color, 1.0);
      mPositions[i + shift] = SkFloatToScalar(aStops[i].offset);
    }
    if (aStops[aNumStops-1].offset != 1) {
      mColors[mCount-1] = ColorToSkColor(aStops[aNumStops-1].color, 1.0);
      mPositions[mCount-1] = SK_Scalar1;
    }
  }

  BackendType GetBackendType() const { return BackendType::SKIA; }

  std::vector<SkColor> mColors;
  std::vector<SkScalar> mPositions;
  int mCount;
  ExtendMode mExtendMode;
};

/**
 * When constructing a temporary SkBitmap via GetBitmapForSurface, we may also
 * have to construct a temporary DataSourceSurface, which must live as long as
 * the SkBitmap. We attach this temporary surface to the bitmap's pixelref, so
 * that it can be released once the pixelref is freed.
 */
static void
ReleaseTemporarySurface(void* aPixels, void* aContext)
{
  DataSourceSurface* surf = static_cast<DataSourceSurface*>(aContext);
  if (surf) {
    surf->Release();
  }
}

static SkBitmap
GetBitmapForSurface(SourceSurface* aSurface)
{
  SkBitmap bitmap;

  if (!aSurface) {
    gfxDebug() << "Creating empty Skia bitmap from null SourceSurface";
    return bitmap;
  }

  if (aSurface->GetType() == SurfaceType::SKIA) {
    bitmap = static_cast<SourceSurfaceSkia*>(aSurface)->GetBitmap();
    return bitmap;
  }

  DataSourceSurface* surf = aSurface->GetDataSurface().take();
  if (!surf) {
    gfxDevCrash(LogReason::SourceSurfaceIncompatible) << "Non-Skia SourceSurfaces need to be DataSourceSurfaces";
    return bitmap;
  }

  if (!bitmap.installPixels(MakeSkiaImageInfo(surf->GetSize(), surf->GetFormat()),
                            surf->GetData(), surf->Stride(), nullptr,
                            ReleaseTemporarySurface, surf)) {
    gfxDebug() << "Failed installing pixels on Skia bitmap for temporary surface";
  }

  return bitmap;
}

DrawTargetSkia::DrawTargetSkia()
  : mSnapshot(nullptr)
#ifdef MOZ_WIDGET_COCOA
  , mCG(nullptr)
  , mColorSpace(nullptr)
  , mCanvasData(nullptr)
  , mCGSize(0, 0)
#endif
{
}

DrawTargetSkia::~DrawTargetSkia()
{
#ifdef MOZ_WIDGET_COCOA
  if (mCG) {
    CGContextRelease(mCG);
    mCG = nullptr;
  }

  if (mColorSpace) {
    CGColorSpaceRelease(mColorSpace);
    mColorSpace = nullptr;
  }
#endif
}

already_AddRefed<SourceSurface>
DrawTargetSkia::Snapshot()
{
  RefPtr<SourceSurfaceSkia> snapshot = mSnapshot;
  if (!snapshot) {
    snapshot = new SourceSurfaceSkia();
    mSnapshot = snapshot;
    if (!snapshot->InitFromCanvas(mCanvas.get(), mFormat, this))
      return nullptr;
  }

  return snapshot.forget();
}

bool
DrawTargetSkia::LockBits(uint8_t** aData, IntSize* aSize,
                         int32_t* aStride, SurfaceFormat* aFormat,
                         IntPoint* aOrigin)
{
  // Ensure the layer is at the origin if required.
  SkIPoint origin = mCanvas->getTopDevice()->getOrigin();
  if (!aOrigin && !origin.isZero()) {
    return false;
  }

  /* Test if the canvas' device has accessible pixels first, as actually
   * accessing the pixels may trigger side-effects, even if it fails.
   */
  if (!mCanvas->peekPixels(nullptr)) {
    return false;
  }

  SkImageInfo info;
  size_t rowBytes;
  void* pixels = mCanvas->accessTopLayerPixels(&info, &rowBytes);
  if (!pixels) {
    return false;
  }

  MarkChanged();

  *aData = reinterpret_cast<uint8_t*>(pixels);
  *aSize = IntSize(info.width(), info.height());
  *aStride = int32_t(rowBytes);
  *aFormat = SkiaColorTypeToGfxFormat(info.colorType());
  if (aOrigin) {
    *aOrigin = IntPoint(origin.x(), origin.y());
  }
  return true;
}

void
DrawTargetSkia::ReleaseBits(uint8_t* aData)
{
}

static void
SetPaintPattern(SkPaint& aPaint, const Pattern& aPattern, Float aAlpha = 1.0)
{
  switch (aPattern.GetType()) {
    case PatternType::COLOR: {
      Color color = static_cast<const ColorPattern&>(aPattern).mColor;
      aPaint.setColor(ColorToSkColor(color, aAlpha));
      break;
    }
    case PatternType::LINEAR_GRADIENT: {
      const LinearGradientPattern& pat = static_cast<const LinearGradientPattern&>(aPattern);
      GradientStopsSkia *stops = static_cast<GradientStopsSkia*>(pat.mStops.get());
      if (!stops || stops->mCount < 2 ||
          !pat.mBegin.IsFinite() || !pat.mEnd.IsFinite()) {
        aPaint.setColor(SK_ColorTRANSPARENT);
      } else {
        SkShader::TileMode mode = ExtendModeToTileMode(stops->mExtendMode, Axis::BOTH);
        SkPoint points[2];
        points[0] = SkPoint::Make(SkFloatToScalar(pat.mBegin.x), SkFloatToScalar(pat.mBegin.y));
        points[1] = SkPoint::Make(SkFloatToScalar(pat.mEnd.x), SkFloatToScalar(pat.mEnd.y));

        SkMatrix mat;
        GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
        sk_sp<SkShader> shader = SkGradientShader::MakeLinear(points,
                                                              &stops->mColors.front(),
                                                              &stops->mPositions.front(),
                                                              stops->mCount,
                                                              mode, 0, &mat);
        aPaint.setShader(shader);
      }
      break;
    }
    case PatternType::RADIAL_GRADIENT: {
      const RadialGradientPattern& pat = static_cast<const RadialGradientPattern&>(aPattern);
      GradientStopsSkia *stops = static_cast<GradientStopsSkia*>(pat.mStops.get());
      if (!stops || stops->mCount < 2 ||
          !pat.mCenter1.IsFinite() || !IsFinite(pat.mRadius1) ||
          !pat.mCenter2.IsFinite() || !IsFinite(pat.mRadius2)) {
        aPaint.setColor(SK_ColorTRANSPARENT);
      } else {
        SkShader::TileMode mode = ExtendModeToTileMode(stops->mExtendMode, Axis::BOTH);
        SkPoint points[2];
        points[0] = SkPoint::Make(SkFloatToScalar(pat.mCenter1.x), SkFloatToScalar(pat.mCenter1.y));
        points[1] = SkPoint::Make(SkFloatToScalar(pat.mCenter2.x), SkFloatToScalar(pat.mCenter2.y));

        SkMatrix mat;
        GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
        sk_sp<SkShader> shader = SkGradientShader::MakeTwoPointConical(points[0],
                                                                       SkFloatToScalar(pat.mRadius1),
                                                                       points[1],
                                                                       SkFloatToScalar(pat.mRadius2),
                                                                       &stops->mColors.front(),
                                                                       &stops->mPositions.front(),
                                                                       stops->mCount,
                                                                       mode, 0, &mat);
        aPaint.setShader(shader);
      }
      break;
    }
    case PatternType::SURFACE: {
      const SurfacePattern& pat = static_cast<const SurfacePattern&>(aPattern);
      SkBitmap bitmap = GetBitmapForSurface(pat.mSurface);

      SkMatrix mat;
      GfxMatrixToSkiaMatrix(pat.mMatrix, mat);

      if (!pat.mSamplingRect.IsEmpty()) {
        SkIRect rect = IntRectToSkIRect(pat.mSamplingRect);
        bitmap.extractSubset(&bitmap, rect);
        mat.preTranslate(rect.x(), rect.y());
      }

      SkShader::TileMode xTileMode = ExtendModeToTileMode(pat.mExtendMode, Axis::X_AXIS);
      SkShader::TileMode yTileMode = ExtendModeToTileMode(pat.mExtendMode, Axis::Y_AXIS);

      sk_sp<SkShader> shader = SkShader::MakeBitmapShader(bitmap, xTileMode, yTileMode, &mat);
      aPaint.setShader(shader);
      if (pat.mSamplingFilter == SamplingFilter::POINT) {
        aPaint.setFilterQuality(kNone_SkFilterQuality);
      }
      break;
    }
  }
}

static inline Rect
GetClipBounds(SkCanvas *aCanvas)
{
  SkRect clipBounds;
  if (!aCanvas->getClipBounds(&clipBounds)) {
    return Rect();
  }
  return SkRectToRect(clipBounds);
}

struct AutoPaintSetup {
  AutoPaintSetup(SkCanvas *aCanvas, const DrawOptions& aOptions, const Pattern& aPattern, const Rect* aMaskBounds = nullptr)
    : mNeedsRestore(false), mAlpha(1.0)
  {
    Init(aCanvas, aOptions, aMaskBounds);
    SetPaintPattern(mPaint, aPattern, mAlpha);
  }

  AutoPaintSetup(SkCanvas *aCanvas, const DrawOptions& aOptions, const Rect* aMaskBounds = nullptr)
    : mNeedsRestore(false), mAlpha(1.0)
  {
    Init(aCanvas, aOptions, aMaskBounds);
  }

  ~AutoPaintSetup()
  {
    if (mNeedsRestore) {
      mCanvas->restore();
    }
  }

  void Init(SkCanvas *aCanvas, const DrawOptions& aOptions, const Rect* aMaskBounds)
  {
    mPaint.setXfermodeMode(GfxOpToSkiaOp(aOptions.mCompositionOp));
    mCanvas = aCanvas;

    //TODO: Can we set greyscale somehow?
    if (aOptions.mAntialiasMode != AntialiasMode::NONE) {
      mPaint.setAntiAlias(true);
    } else {
      mPaint.setAntiAlias(false);
    }

    bool needsGroup = !IsOperatorBoundByMask(aOptions.mCompositionOp) &&
                      (!aMaskBounds || !aMaskBounds->Contains(GetClipBounds(aCanvas)));

    // TODO: We could skip the temporary for operator_source and just
    // clear the clip rect. The other operators would be harder
    // but could be worth it to skip pushing a group.
    if (needsGroup) {
      mPaint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
      SkPaint temp;
      temp.setXfermodeMode(GfxOpToSkiaOp(aOptions.mCompositionOp));
      temp.setAlpha(ColorFloatToByte(aOptions.mAlpha));
      //TODO: Get a rect here
      mCanvas->saveLayer(nullptr, &temp);
      mNeedsRestore = true;
    } else {
      mPaint.setAlpha(ColorFloatToByte(aOptions.mAlpha));
      mAlpha = aOptions.mAlpha;
    }
    mPaint.setFilterQuality(kLow_SkFilterQuality);
  }

  // TODO: Maybe add an operator overload to access this easier?
  SkPaint mPaint;
  bool mNeedsRestore;
  SkCanvas* mCanvas;
  Float mAlpha;
};

void
DrawTargetSkia::Flush()
{
  mCanvas->flush();
}

void
DrawTargetSkia::DrawSurface(SourceSurface *aSurface,
                            const Rect &aDest,
                            const Rect &aSource,
                            const DrawSurfaceOptions &aSurfOptions,
                            const DrawOptions &aOptions)
{
  RefPtr<SourceSurface> dataSurface;

  if (!(aSurface->GetType() == SurfaceType::SKIA || aSurface->GetType() == SurfaceType::DATA)) {
    dataSurface = aSurface->GetDataSurface();
    if (!dataSurface) {
      gfxDebug() << *this << ": DrawSurface() can't draw surface";
      return;
    }
    aSurface = dataSurface.get();
  }

  if (aSource.IsEmpty()) {
    return;
  }

  MarkChanged();

  SkRect destRect = RectToSkRect(aDest);
  SkRect sourceRect = RectToSkRect(aSource);

  SkBitmap bitmap = GetBitmapForSurface(aSurface);

  AutoPaintSetup paint(mCanvas.get(), aOptions, &aDest);
  if (aSurfOptions.mSamplingFilter == SamplingFilter::POINT) {
    paint.mPaint.setFilterQuality(kNone_SkFilterQuality);
  }

  mCanvas->drawBitmapRect(bitmap, sourceRect, destRect, &paint.mPaint);
}

DrawTargetType
DrawTargetSkia::GetType() const
{
#ifdef USE_SKIA_GPU
  if (mGrContext) {
    return DrawTargetType::HARDWARE_RASTER;
  }
#endif
  return DrawTargetType::SOFTWARE_RASTER;
}

void
DrawTargetSkia::DrawFilter(FilterNode *aNode,
                           const Rect &aSourceRect,
                           const Point &aDestPoint,
                           const DrawOptions &aOptions)
{
  FilterNodeSoftware* filter = static_cast<FilterNodeSoftware*>(aNode);
  filter->Draw(this, aSourceRect, aDestPoint, aOptions);
}

void
DrawTargetSkia::DrawSurfaceWithShadow(SourceSurface *aSurface,
                                      const Point &aDest,
                                      const Color &aColor,
                                      const Point &aOffset,
                                      Float aSigma,
                                      CompositionOp aOperator)
{
  if (!(aSurface->GetType() == SurfaceType::SKIA || aSurface->GetType() == SurfaceType::DATA) ||
      aSurface->GetSize().IsEmpty()) {
    return;
  }

  MarkChanged();

  mCanvas->save();
  mCanvas->resetMatrix();

  SkBitmap bitmap = GetBitmapForSurface(aSurface);

  SkPaint paint;
  paint.setXfermodeMode(GfxOpToSkiaOp(aOperator));

  // bug 1201272
  // We can't use the SkDropShadowImageFilter here because it applies the xfer
  // mode first to render the bitmap to a temporary layer, and then implicitly
  // uses src-over to composite the resulting shadow.
  // The canvas spec, however, states that the composite op must be used to
  // composite the resulting shadow, so we must instead use a SkBlurImageFilter
  // to blur the image ourselves.

  SkPaint shadowPaint;
  shadowPaint.setXfermodeMode(GfxOpToSkiaOp(aOperator));

  IntPoint shadowDest = RoundedToInt(aDest + aOffset);

  SkBitmap blurMask;
  if (!UsingSkiaGPU() &&
      bitmap.extractAlpha(&blurMask)) {
    // Prefer using our own box blur instead of Skia's when we're
    // not using the GPU. It currently performs much better than
    // SkBlurImageFilter or SkBlurMaskFilter on the CPU.
    AlphaBoxBlur blur(Rect(0, 0, blurMask.width(), blurMask.height()),
                      int32_t(blurMask.rowBytes()),
                      aSigma, aSigma);
    blurMask.lockPixels();
    blur.Blur(reinterpret_cast<uint8_t*>(blurMask.getPixels()));
    blurMask.unlockPixels();
    blurMask.notifyPixelsChanged();

    shadowPaint.setColor(ColorToSkColor(aColor, 1.0f));

    mCanvas->drawBitmap(blurMask, shadowDest.x, shadowDest.y, &shadowPaint);
  } else {
    sk_sp<SkImageFilter> blurFilter(SkBlurImageFilter::Make(aSigma, aSigma, nullptr));
    sk_sp<SkColorFilter> colorFilter(
      SkColorFilter::MakeModeFilter(ColorToSkColor(aColor, 1.0f), SkXfermode::kSrcIn_Mode));

    shadowPaint.setImageFilter(blurFilter);
    shadowPaint.setColorFilter(colorFilter);

    mCanvas->drawBitmap(bitmap, shadowDest.x, shadowDest.y, &shadowPaint);
  }

  // Composite the original image after the shadow
  IntPoint dest = RoundedToInt(aDest);
  mCanvas->drawBitmap(bitmap, dest.x, dest.y, &paint);

  mCanvas->restore();
}

void
DrawTargetSkia::FillRect(const Rect &aRect,
                         const Pattern &aPattern,
                         const DrawOptions &aOptions)
{
  MarkChanged();
  SkRect rect = RectToSkRect(aRect);
  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern, &aRect);

  mCanvas->drawRect(rect, paint.mPaint);
}

void
DrawTargetSkia::Stroke(const Path *aPath,
                       const Pattern &aPattern,
                       const StrokeOptions &aStrokeOptions,
                       const DrawOptions &aOptions)
{
  MarkChanged();
  MOZ_ASSERT(aPath, "Null path");
  if (aPath->GetBackendType() != BackendType::SKIA) {
    return;
  }

  const PathSkia *skiaPath = static_cast<const PathSkia*>(aPath);


  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern);
  if (!StrokeOptionsToPaint(paint.mPaint, aStrokeOptions)) {
    return;
  }

  if (!skiaPath->GetPath().isFinite()) {
    return;
  }

  mCanvas->drawPath(skiaPath->GetPath(), paint.mPaint);
}

void
DrawTargetSkia::StrokeRect(const Rect &aRect,
                           const Pattern &aPattern,
                           const StrokeOptions &aStrokeOptions,
                           const DrawOptions &aOptions)
{
  MarkChanged();
  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern);
  if (!StrokeOptionsToPaint(paint.mPaint, aStrokeOptions)) {
    return;
  }

  mCanvas->drawRect(RectToSkRect(aRect), paint.mPaint);
}

void
DrawTargetSkia::StrokeLine(const Point &aStart,
                           const Point &aEnd,
                           const Pattern &aPattern,
                           const StrokeOptions &aStrokeOptions,
                           const DrawOptions &aOptions)
{
  MarkChanged();
  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern);
  if (!StrokeOptionsToPaint(paint.mPaint, aStrokeOptions)) {
    return;
  }

  mCanvas->drawLine(SkFloatToScalar(aStart.x), SkFloatToScalar(aStart.y),
                    SkFloatToScalar(aEnd.x), SkFloatToScalar(aEnd.y),
                    paint.mPaint);
}

void
DrawTargetSkia::Fill(const Path *aPath,
                    const Pattern &aPattern,
                    const DrawOptions &aOptions)
{
  MarkChanged();
  if (aPath->GetBackendType() != BackendType::SKIA) {
    return;
  }

  const PathSkia *skiaPath = static_cast<const PathSkia*>(aPath);

  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern);

  if (!skiaPath->GetPath().isFinite()) {
    return;
  }

  mCanvas->drawPath(skiaPath->GetPath(), paint.mPaint);
}

bool
DrawTargetSkia::ShouldLCDRenderText(FontType aFontType, AntialiasMode aAntialiasMode)
{
  // For non-opaque surfaces, only allow subpixel AA if explicitly permitted.
  if (!IsOpaque(mFormat) && !mPermitSubpixelAA) {
    return false;
  }

  if (aAntialiasMode == AntialiasMode::DEFAULT) {
    switch (aFontType) {
      case FontType::MAC:
      case FontType::GDI:
      case FontType::DWRITE:
      case FontType::FONTCONFIG:
        return true;
      default:
        // TODO: Figure out what to do for the other platforms.
        return false;
    }
  }
  return (aAntialiasMode == AntialiasMode::SUBPIXEL);
}

#ifdef MOZ_WIDGET_COCOA
class CGClipApply : public SkCanvas::ClipVisitor {
public:
  explicit CGClipApply(CGContextRef aCGContext)
    : mCG(aCGContext) {}
  void clipRect(const SkRect& aRect, SkRegion::Op op, bool antialias) override {
    CGRect rect = CGRectMake(aRect.x(), aRect.y(), aRect.width(), aRect.height());
    CGContextClipToRect(mCG, rect);
  }

  void clipRRect(const SkRRect& rrect, SkRegion::Op op, bool antialias) override {
    SkPath path;
    path.addRRect(rrect);
    clipPath(path, op, antialias);
  }

  void clipPath(const SkPath& aPath, SkRegion::Op, bool antialias) override {
    SkPath::Iter iter(aPath, true);
    SkPoint source[4];
    SkPath::Verb verb;
    RefPtr<PathBuilderCG> pathBuilder =
        new PathBuilderCG(GetFillRule(aPath.getFillType()));

    while ((verb = iter.next(source)) != SkPath::kDone_Verb) {
      switch (verb) {
      case SkPath::kMove_Verb:
      {
        SkPoint dest = source[0];
        pathBuilder->MoveTo(Point(dest.fX, dest.fY));
        break;
      }
      case SkPath::kLine_Verb:
      {
        // The first point should be the end point of whatever
        // verb we got to get here.
        SkPoint second = source[1];
        pathBuilder->LineTo(Point(second.fX, second.fY));
        break;
      }
      case SkPath::kQuad_Verb:
      {
        SkPoint second = source[1];
        SkPoint third = source[2];

        pathBuilder->QuadraticBezierTo(Point(second.fX, second.fY),
                                       Point(third.fX, third.fY));
        break;
      }
      case SkPath::kCubic_Verb:
      {
        SkPoint second = source[1];
        SkPoint third = source[2];
        SkPoint fourth = source[2];

        pathBuilder->BezierTo(Point(second.fX, second.fY),
                              Point(third.fX, third.fY),
                              Point(fourth.fX, fourth.fY));
        break;
      }
      case SkPath::kClose_Verb:
      {
        pathBuilder->Close();
        break;
      }
      default:
      {
        SkDEBUGFAIL("unknown verb");
        break;
      }
      } // end switch
    } // end while

    RefPtr<Path> path = pathBuilder->Finish();
    PathCG* cgPath = static_cast<PathCG*>(path.get());

    // Weirdly, CoreGraphics clips empty paths as all shown
    // but empty rects as all clipped.  We detect this situation and
    // workaround it appropriately
    if (CGPathIsEmpty(cgPath->GetPath())) {
      CGContextClipToRect(mCG, CGRectZero);
      return;
    }

    CGContextBeginPath(mCG);
    CGContextAddPath(mCG, cgPath->GetPath());

    if (cgPath->GetFillRule() == FillRule::FILL_EVEN_ODD) {
      CGContextEOClip(mCG);
    } else {
      CGContextClip(mCG);
    }
  }

private:
  CGContextRef mCG;
};

/***
 * We have to do a lot of work to draw glyphs with CG because
 * CG assumes that the origin of rects are in the bottom left
 * while every other DrawTarget assumes the top left is the origin.
 * This means we have to transform the CGContext to have rects
 * actually be applied in top left fashion. We do this by:
 *
 * 1) Translating the context up by the height of the canvas
 * 2) Flipping the context by the Y axis so it's upside down.
 *
 * These two transforms put the origin in the top left.
 * Transforms are better understood thinking about them from right to left order (mathematically).
 *
 * Consider a point we want to draw at (0, 10) in normal cartesian planes with
 * a box of (100, 100). in CG terms, this would be at (0, 10).
 * Positive Y values point up.
 * In our DrawTarget terms, positive Y values point down, so (0, 10) would be
 * at (0, 90) in cartesian plane terms. That means our point at (0, 10) in DrawTarget
 * terms should end up at (0, 90). How does this work with the current transforms?
 *
 * Going right to left with the transforms, a CGPoint of (0, 10) has cartesian coordinates
 * of (0, 10). The first flip of the Y axis puts the point now at (0, -10);
 * Next, we translate the context up by the size of the canvas (Positive Y values go up in CG
 * coordinates but down in our draw target coordinates). Since our canvas size is (100, 100),
 * the resulting coordinate becomes (0, 90), which is what we expect from our DrawTarget code.
 * These two transforms put the CG context equal to what every other DrawTarget expects.
 *
 * Next, we need two more transforms for actual text. IF we left the transforms as is,
 * the text would be drawn upside down, so we need another flip of the Y axis
 * to draw the text right side up. However, with only the flip, the text would be drawn
 * in the wrong place. Thus we also have to invert the Y position of the glyphs to get them
 * in the right place.
 *
 * Thus we have the following transforms:
 * 1) Translation of the context up
 * 2) Flipping the context around the Y axis
 * 3) Flipping the context around the Y axis
 * 4) Inverting the Y position of each glyph
 *
 * We cannot cancel out (2) and (3) as we have to apply the clips and transforms
 * of DrawTargetSkia between (2) and (3).
 *
 * Consider the example letter P, drawn at (0, 20) in CG coordinates in a (100, 100) rect.
 * Again, going right to left of the transforms. We'd get:
 *
 * 1) The letter P drawn at (0, -20) due to the inversion of the Y axis
 * 2) The letter P upside down (b) at (0, 20) due to the second flip
 * 3) The letter P right side up at (0, -20) due to the first flip
 * 4) The letter P right side up at (0, 80) due to the translation
 *
 * tl;dr - CGRects assume origin is bottom left, DrawTarget rects assume top left.
 */
static bool
SetupCGContext(DrawTargetSkia* aDT,
               CGContextRef aCGContext,
               RefPtrSkia<SkCanvas> aCanvas)
{
  // DrawTarget expects the origin to be at the top left, but CG
  // expects it to be at the bottom left. Transform to set the origin to
  // the top left. Have to set this before we do anything else.
  // This is transform (1) up top
  CGContextTranslateCTM(aCGContext, 0, aDT->GetSize().height);

  // Transform (2) from the comments.
  CGContextScaleCTM(aCGContext, 1, -1);

  // Want to apply clips BEFORE the transform since the transform
  // will apply to the clips we apply.
  // CGClipApply applies clips in device space, so it would be a mistake
  // to transform these clips.
  CGClipApply clipApply(aCGContext);
  aCanvas->replayClips(&clipApply);

  CGContextConcatCTM(aCGContext, GfxMatrixToCGAffineTransform(aDT->GetTransform()));
  return true;
}

static bool
SetupCGGlyphs(CGContextRef aCGContext,
              const GlyphBuffer& aBuffer,
              Vector<CGGlyph,32>& aGlyphs,
              Vector<CGPoint,32>& aPositions)
{
  // Flip again so we draw text in right side up. Transform (3) from the top
  CGContextScaleCTM(aCGContext, 1, -1);

  if (!aGlyphs.resizeUninitialized(aBuffer.mNumGlyphs) ||
      !aPositions.resizeUninitialized(aBuffer.mNumGlyphs)) {
    gfxDevCrash(LogReason::GlyphAllocFailedCG) << "glyphs/positions allocation failed";
    return false;
  }

  for (unsigned int i = 0; i < aBuffer.mNumGlyphs; i++) {
    aGlyphs[i] = aBuffer.mGlyphs[i].mIndex;

    // Flip the y coordinates so that text ends up in the right spot after the (3) flip
    // Inversion from (4) in the comments.
    aPositions[i] = CGPointMake(aBuffer.mGlyphs[i].mPosition.x,
                                -aBuffer.mGlyphs[i].mPosition.y);
  }

  return true;
}
// End long comment about transforms. SetupCGContext and SetupCGGlyphs should stay
// next to each other.

// The context returned from this method will have the origin
// in the top left and will hvae applied all the neccessary clips
// and transforms to the CGContext. See the comment above
// SetupCGContext.
CGContextRef
DrawTargetSkia::BorrowCGContext(const DrawOptions &aOptions)
{
  int32_t stride;
  SurfaceFormat format;
  IntSize size;

  uint8_t* aSurfaceData = nullptr;
  if (!LockBits(&aSurfaceData, &size, &stride, &format)) {
    NS_WARNING("Could not lock skia bits to wrap CG around");
    return nullptr;
  }

  if ((aSurfaceData == mCanvasData) && mCG && (mCGSize == size)) {
    // If our canvas data still points to the same data,
    // we can reuse the CG Context
    CGContextSaveGState(mCG);
    CGContextSetAlpha(mCG, aOptions.mAlpha);
    SetupCGContext(this, mCG, mCanvas);
    return mCG;
  }

  if (!mColorSpace) {
    mColorSpace = CGColorSpaceCreateDeviceRGB();
  }

  if (mCG) {
    // Release the old CG context since it's no longer valid.
    CGContextRelease(mCG);
  }

  mCanvasData = aSurfaceData;
  mCGSize = size;

  mCG = CGBitmapContextCreateWithData(mCanvasData,
                                      mCGSize.width,
                                      mCGSize.height,
                                      8, /* bits per component */
                                      stride,
                                      mColorSpace,
                                      kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst,
                                      NULL, /* Callback when released */
                                      NULL);
  if (!mCG) {
    ReleaseBits(mCanvasData);
    NS_WARNING("Could not create bitmap around skia data\n");
    return nullptr;
  }

  CGContextSetAlpha(mCG, aOptions.mAlpha);
  CGContextSetShouldAntialias(mCG, aOptions.mAntialiasMode != AntialiasMode::NONE);
  CGContextSetShouldSmoothFonts(mCG, true);
  CGContextSetTextDrawingMode(mCG, kCGTextFill);
  CGContextSaveGState(mCG);
  SetupCGContext(this, mCG, mCanvas);
  return mCG;
}

void
DrawTargetSkia::ReturnCGContext(CGContextRef aCGContext)
{
  MOZ_ASSERT(aCGContext == mCG);
  ReleaseBits(mCanvasData);
  CGContextRestoreGState(aCGContext);
}

static void
SetFontColor(CGContextRef aCGContext, CGColorSpaceRef aColorSpace, const Pattern& aPattern)
{
  const Color& color = static_cast<const ColorPattern&>(aPattern).mColor;
  CGColorRef textColor = ColorToCGColor(aColorSpace, color);
  CGContextSetFillColorWithColor(aCGContext, textColor);
  CGColorRelease(textColor);
}

/***
 * We need this to support subpixel AA text on OS X in two cases:
 * text in DrawTargets that are not opaque and text over vibrant backgrounds.
 * Skia normally doesn't support subpixel AA text on transparent backgrounds.
 * To get around this, we have to wrap the Skia bytes with a CGContext and ask
 * CG to draw the text.
 * In vibrancy cases, we have to use a private API,
 * CGContextSetFontSmoothingBackgroundColor, which sets the expected
 * background color the text will draw onto so that CG can render the text
 * properly. After that, we have to go back and fixup the pixels
 * such that their alpha values are correct.
 */
bool
DrawTargetSkia::FillGlyphsWithCG(ScaledFont *aFont,
                                 const GlyphBuffer &aBuffer,
                                 const Pattern &aPattern,
                                 const DrawOptions &aOptions,
                                 const GlyphRenderingOptions *aRenderingOptions)
{
  MOZ_ASSERT(aFont->GetType() == FontType::MAC);
  MOZ_ASSERT(aPattern.GetType() == PatternType::COLOR);

  CGContextRef cgContext = BorrowCGContext(aOptions);
  if (!cgContext) {
    return false;
  }

  Vector<CGGlyph,32> glyphs;
  Vector<CGPoint,32> positions;
  if (!SetupCGGlyphs(cgContext, aBuffer, glyphs, positions)) {
    ReturnCGContext(cgContext);
    return false;
  }

  SetFontSmoothingBackgroundColor(cgContext, mColorSpace, aRenderingOptions);
  SetFontColor(cgContext, mColorSpace, aPattern);

  ScaledFontMac* macFont = static_cast<ScaledFontMac*>(aFont);
  if (ScaledFontMac::CTFontDrawGlyphsPtr != nullptr) {
    ScaledFontMac::CTFontDrawGlyphsPtr(macFont->mCTFont, glyphs.begin(),
                                       positions.begin(),
                                       aBuffer.mNumGlyphs, cgContext);
  } else {
    CGContextSetFont(cgContext, macFont->mFont);
    CGContextSetFontSize(cgContext, macFont->mSize);
    CGContextShowGlyphsAtPositions(cgContext, glyphs.begin(), positions.begin(),
                                   aBuffer.mNumGlyphs);
  }

  // Calculate the area of the text we just drew
  CGRect *bboxes = new CGRect[aBuffer.mNumGlyphs];
  CTFontGetBoundingRectsForGlyphs(macFont->mCTFont, kCTFontDefaultOrientation,
                                  glyphs.begin(), bboxes, aBuffer.mNumGlyphs);
  CGRect extents = ComputeGlyphsExtents(bboxes, positions.begin(), aBuffer.mNumGlyphs, 1.0f);
  delete[] bboxes;

  CGAffineTransform cgTransform = CGContextGetCTM(cgContext);
  extents = CGRectApplyAffineTransform(extents, cgTransform);

  // Have to round it out to ensure we fully cover all pixels
  Rect rect(extents.origin.x, extents.origin.y, extents.size.width, extents.size.height);
  rect.RoundOut();
  extents = CGRectMake(rect.x, rect.y, rect.width, rect.height);

  EnsureValidPremultipliedData(cgContext, extents);

  ReturnCGContext(cgContext);
  return true;
}

static bool
HasFontSmoothingBackgroundColor(const GlyphRenderingOptions* aRenderingOptions)
{
  // This should generally only be true if we have a popup context menu
  if (aRenderingOptions && aRenderingOptions->GetType() == FontType::MAC) {
    Color fontSmoothingBackgroundColor =
      static_cast<const GlyphRenderingOptionsCG*>(aRenderingOptions)->FontSmoothingBackgroundColor();
    return fontSmoothingBackgroundColor.a > 0;
  }

  return false;
}

static bool
ShouldUseCGToFillGlyphs(const GlyphRenderingOptions* aOptions, const Pattern& aPattern)
{
  return HasFontSmoothingBackgroundColor(aOptions) &&
          aPattern.GetType() == PatternType::COLOR;
}

#endif

void
DrawTargetSkia::FillGlyphs(ScaledFont *aFont,
                           const GlyphBuffer &aBuffer,
                           const Pattern &aPattern,
                           const DrawOptions &aOptions,
                           const GlyphRenderingOptions *aRenderingOptions)
{
  switch (aFont->GetType()) {
  case FontType::SKIA:
  case FontType::CAIRO:
  case FontType::FONTCONFIG:
  case FontType::MAC:
  case FontType::GDI:
  case FontType::DWRITE:
    break;
  default:
    return;
  }

  MarkChanged();

#ifdef MOZ_WIDGET_COCOA
  if (ShouldUseCGToFillGlyphs(aRenderingOptions, aPattern)) {
    if (FillGlyphsWithCG(aFont, aBuffer, aPattern, aOptions, aRenderingOptions)) {
      return;
    }
  }
#endif

  ScaledFontBase* skiaFont = static_cast<ScaledFontBase*>(aFont);
  SkTypeface* typeface = skiaFont->GetSkTypeface();
  if (!typeface) {
    return;
  }

  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern);
  paint.mPaint.setTypeface(typeface);
  paint.mPaint.setTextSize(SkFloatToScalar(skiaFont->mSize));
  paint.mPaint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

  bool shouldLCDRenderText = ShouldLCDRenderText(aFont->GetType(), aOptions.mAntialiasMode);
  paint.mPaint.setLCDRenderText(shouldLCDRenderText);

  bool useSubpixelText = true;

  switch (aFont->GetType()) {
  case FontType::SKIA:
  case FontType::CAIRO:
  case FontType::FONTCONFIG:
    // SkFontHost_cairo does not support subpixel text positioning,
    // so only enable it for other font hosts.
    useSubpixelText = false;
    break;
  case FontType::MAC:
    if (aOptions.mAntialiasMode == AntialiasMode::GRAY) {
      // Normally, Skia enables LCD FontSmoothing which creates thicker fonts
      // and also enables subpixel AA. CoreGraphics without font smoothing
      // explicitly creates thinner fonts and grayscale AA.
      // CoreGraphics doesn't support a configuration that produces thicker
      // fonts with grayscale AA as LCD Font Smoothing enables or disables both.
      // However, Skia supports it by enabling font smoothing (producing subpixel AA)
      // and converts it to grayscale AA. Since Skia doesn't support subpixel AA on
      // transparent backgrounds, we still want font smoothing for the thicker fonts,
      // even if it is grayscale AA.
      //
      // With explicit Grayscale AA (from -moz-osx-font-smoothing:grayscale),
      // we want to have grayscale AA with no smoothing at all. This means
      // disabling the LCD font smoothing behaviour.
      // To accomplish this we have to explicitly disable hinting,
      // and disable LCDRenderText.
      paint.mPaint.setHinting(SkPaint::kNo_Hinting);
    }
    break;
  case FontType::GDI:
    if (!shouldLCDRenderText) {
      // If we have non LCD GDI text, Cairo currently always uses cleartype fonts and
      // converts them to grayscale. Force Skia to do the same, otherwise we use
      // GDI fonts with the ANTIALIASED_QUALITY which is generally bolder than
      // Cleartype fonts.
      paint.mPaint.setFlags(paint.mPaint.getFlags() | SkPaint::kGenA8FromLCD_Flag);
    }
    break;
  default:
    break;
  }

  paint.mPaint.setSubpixelText(useSubpixelText);

  std::vector<uint16_t> indices;
  std::vector<SkPoint> offsets;
  indices.resize(aBuffer.mNumGlyphs);
  offsets.resize(aBuffer.mNumGlyphs);

  for (unsigned int i = 0; i < aBuffer.mNumGlyphs; i++) {
    indices[i] = aBuffer.mGlyphs[i].mIndex;
    offsets[i].fX = SkFloatToScalar(aBuffer.mGlyphs[i].mPosition.x);
    offsets[i].fY = SkFloatToScalar(aBuffer.mGlyphs[i].mPosition.y);
  }

  mCanvas->drawPosText(&indices.front(), aBuffer.mNumGlyphs*2, &offsets.front(), paint.mPaint);
}

void
DrawTargetSkia::Mask(const Pattern &aSource,
                     const Pattern &aMask,
                     const DrawOptions &aOptions)
{
  MarkChanged();
  AutoPaintSetup paint(mCanvas.get(), aOptions, aSource);

  SkPaint maskPaint;
  SetPaintPattern(maskPaint, aMask);

  SkLayerRasterizer::Builder builder;
  builder.addLayer(maskPaint);
  sk_sp<SkLayerRasterizer> raster(builder.detach());
  paint.mPaint.setRasterizer(raster);

  mCanvas->drawPaint(paint.mPaint);
}

void
DrawTargetSkia::MaskSurface(const Pattern &aSource,
                            SourceSurface *aMask,
                            Point aOffset,
                            const DrawOptions &aOptions)
{
  MarkChanged();
  AutoPaintSetup paint(mCanvas.get(), aOptions, aSource);

  SkBitmap bitmap = GetBitmapForSurface(aMask);
  if (bitmap.colorType() != kAlpha_8_SkColorType &&
      !bitmap.extractAlpha(&bitmap)) {
    gfxDebug() << *this << ": MaskSurface() failed to extract alpha for mask";
    return;
  }

  if (aOffset != Point(0, 0) &&
      paint.mPaint.getShader()) {
    SkMatrix transform;
    transform.setTranslate(PointToSkPoint(-aOffset));
    sk_sp<SkShader> matrixShader = paint.mPaint.getShader()->makeWithLocalMatrix(transform);
    paint.mPaint.setShader(matrixShader);
  }

  mCanvas->drawBitmap(bitmap, aOffset.x, aOffset.y, &paint.mPaint);
}

bool
DrawTarget::Draw3DTransformedSurface(SourceSurface* aSurface, const Matrix4x4& aMatrix)
{
  // Composite the 3D transform with the DT's transform.
  Matrix4x4 fullMat = aMatrix * Matrix4x4::From2D(mTransform);
  if (fullMat.IsSingular()) {
    return false;
  }
  // Transform the surface bounds and clip to this DT.
  IntRect xformBounds =
    RoundedOut(
      fullMat.TransformAndClipBounds(Rect(Point(0, 0), Size(aSurface->GetSize())),
                                     Rect(Point(0, 0), Size(GetSize()))));
  if (xformBounds.IsEmpty()) {
    return true;
  }
  // Offset the matrix by the transformed origin.
  fullMat.PostTranslate(-xformBounds.x, -xformBounds.y, 0);

  // Read in the source data.
  SkBitmap srcBitmap = GetBitmapForSurface(aSurface);

  // Set up an intermediate destination surface only the size of the transformed bounds.
  // Try to pass through the source's format unmodified in both the BGRA and ARGB cases.
  RefPtr<DataSourceSurface> dstSurf =
    Factory::CreateDataSourceSurface(xformBounds.Size(),
                                     srcBitmap.alphaType() == kPremul_SkAlphaType ?
                                       aSurface->GetFormat() : SurfaceFormat::A8R8G8B8_UINT32,
                                     true);
  if (!dstSurf) {
    return false;
  }
  SkAutoTUnref<SkCanvas> dstCanvas(
    SkCanvas::NewRasterDirect(
      SkImageInfo::Make(xformBounds.width, xformBounds.height,
                        srcBitmap.alphaType() == kPremul_SkAlphaType ?
                          srcBitmap.colorType() : kBGRA_8888_SkColorType,
                        kPremul_SkAlphaType),
      dstSurf->GetData(), dstSurf->Stride()));
  if (!dstCanvas) {
    return false;
  }

  // Do the transform.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setFilterQuality(kLow_SkFilterQuality);
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);

  SkMatrix xform;
  GfxMatrixToSkiaMatrix(fullMat, xform);
  dstCanvas->setMatrix(xform);

  dstCanvas->drawBitmap(srcBitmap, 0, 0, &paint);
  dstCanvas->flush();

  // Temporarily reset the DT's transform, since it has already been composed above.
  Matrix origTransform = mTransform;
  SetTransform(Matrix());

  // Draw the transformed surface within the transformed bounds.
  DrawSurface(dstSurf, Rect(xformBounds), Rect(Point(0, 0), Size(xformBounds.Size())));

  SetTransform(origTransform);

  return true;
}

bool
DrawTargetSkia::Draw3DTransformedSurface(SourceSurface* aSurface, const Matrix4x4& aMatrix)
{
  if (aMatrix.IsSingular()) {
    return false;
  }

  MarkChanged();

  SkBitmap bitmap = GetBitmapForSurface(aSurface);

  mCanvas->save();

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setFilterQuality(kLow_SkFilterQuality);

  SkMatrix xform;
  GfxMatrixToSkiaMatrix(aMatrix, xform);
  mCanvas->concat(xform);

  mCanvas->drawBitmap(bitmap, 0, 0, &paint);

  mCanvas->restore();

  return true;
}

already_AddRefed<SourceSurface>
DrawTargetSkia::CreateSourceSurfaceFromData(unsigned char *aData,
                                            const IntSize &aSize,
                                            int32_t aStride,
                                            SurfaceFormat aFormat) const
{
  RefPtr<SourceSurfaceSkia> newSurf = new SourceSurfaceSkia();

  if (!newSurf->InitFromData(aData, aSize, aStride, aFormat)) {
    gfxDebug() << *this << ": Failure to create source surface from data. Size: " << aSize;
    return nullptr;
  }

  return newSurf.forget();
}

already_AddRefed<DrawTarget>
DrawTargetSkia::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  RefPtr<DrawTargetSkia> target = new DrawTargetSkia();
#ifdef USE_SKIA_GPU
  if (UsingSkiaGPU()) {
    // Try to create a GPU draw target first if we're currently using the GPU.
    // Mark the DT as cached so that shadow DTs, extracted subrects, and similar can be reused.
    if (target->InitWithGrContext(mGrContext.get(), aSize, aFormat, true)) {
      return target.forget();
    }
    // Otherwise, just fall back to a software draw target.
  }
#endif
  if (!target->Init(aSize, aFormat)) {
    return nullptr;
  }
  return target.forget();
}

bool
DrawTargetSkia::UsingSkiaGPU() const
{
#ifdef USE_SKIA_GPU
  return !!mGrContext;
#else
  return false;
#endif
}

already_AddRefed<SourceSurface>
DrawTargetSkia::OptimizeSourceSurface(SourceSurface *aSurface) const
{
#ifdef USE_SKIA_GPU
  if (UsingSkiaGPU()) {
    // Check if the underlying SkBitmap already has an associated GrTexture.
    if (aSurface->GetType() == SurfaceType::SKIA &&
        static_cast<SourceSurfaceSkia*>(aSurface)->GetBitmap().getTexture()) {
      RefPtr<SourceSurface> surface(aSurface);
      return surface.forget();
    }

    SkBitmap bitmap = GetBitmapForSurface(aSurface);

    // Upload the SkBitmap to a GrTexture otherwise.
    SkAutoTUnref<GrTexture> texture(
      GrRefCachedBitmapTexture(mGrContext.get(), bitmap, GrTextureParams::ClampBilerp()));

    if (texture) {
      // Create a new SourceSurfaceSkia whose SkBitmap contains the GrTexture.
      RefPtr<SourceSurfaceSkia> surface = new SourceSurfaceSkia();
      if (surface->InitFromGrTexture(texture, aSurface->GetSize(), aSurface->GetFormat())) {
        return surface.forget();
      }
    }

    // The data was too big to fit in a GrTexture.
    if (aSurface->GetType() == SurfaceType::SKIA) {
      // It is already a Skia source surface, so just reuse it as-is.
      RefPtr<SourceSurface> surface(aSurface);
      return surface.forget();
    }

    // Wrap it in a Skia source surface so that can do tiled uploads on-demand.
    RefPtr<SourceSurfaceSkia> surface = new SourceSurfaceSkia();
    surface->InitFromBitmap(bitmap);
    return surface.forget();
  }
#endif

  if (aSurface->GetType() == SurfaceType::SKIA) {
    RefPtr<SourceSurface> surface(aSurface);
    return surface.forget();
  }

  // If we're not using skia-gl then drawing doesn't require any
  // uploading, so any data surface is fine. Call GetDataSurface
  // to trigger any required readback so that it only happens
  // once.
  return aSurface->GetDataSurface();
}

already_AddRefed<SourceSurface>
DrawTargetSkia::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
#if USE_SKIA_GPU
  if (aSurface.mType == NativeSurfaceType::OPENGL_TEXTURE && UsingSkiaGPU()) {
    // Wrap the OpenGL texture id in a Skia texture handle.
    GrBackendTextureDesc texDesc;
    texDesc.fWidth = aSurface.mSize.width;
    texDesc.fHeight = aSurface.mSize.height;
    texDesc.fOrigin = kTopLeft_GrSurfaceOrigin;
    texDesc.fConfig = GfxFormatToGrConfig(aSurface.mFormat);

    GrGLTextureInfo texInfo;
    texInfo.fTarget = LOCAL_GL_TEXTURE_2D;
    texInfo.fID = (GrGLuint)(uintptr_t)aSurface.mSurface;
    texDesc.fTextureHandle = reinterpret_cast<GrBackendObject>(&texInfo);

    SkAutoTUnref<GrTexture> texture(mGrContext->textureProvider()->wrapBackendTexture(texDesc, kAdopt_GrWrapOwnership));

    RefPtr<SourceSurfaceSkia> newSurf = new SourceSurfaceSkia();
    if (newSurf->InitFromGrTexture(texture, aSurface.mSize, aSurface.mFormat)) {
      return newSurf.forget();
    }
    return nullptr;
  }
#endif

  return nullptr;
}

void
DrawTargetSkia::CopySurface(SourceSurface *aSurface,
                            const IntRect& aSourceRect,
                            const IntPoint &aDestination)
{
  if (aSurface->GetType() != SurfaceType::SKIA && aSurface->GetType() != SurfaceType::DATA) {
    return;
  }

  MarkChanged();

  SkBitmap bitmap = GetBitmapForSurface(aSurface);

  mCanvas->save();
  mCanvas->setMatrix(SkMatrix::MakeTrans(SkIntToScalar(aDestination.x), SkIntToScalar(aDestination.y)));
  mCanvas->clipRect(SkRect::MakeIWH(aSourceRect.width, aSourceRect.height), SkRegion::kReplace_Op);

  SkPaint paint;
  if (!bitmap.isOpaque()) {
    // Keep the xfermode as SOURCE_OVER for opaque bitmaps
    // http://code.google.com/p/skia/issues/detail?id=628
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  }
  // drawBitmap with A8 bitmaps ends up doing a mask operation
  // so we need to clear before
  if (bitmap.colorType() == kAlpha_8_SkColorType) {
    mCanvas->clear(SK_ColorTRANSPARENT);
  }
  mCanvas->drawBitmap(bitmap, -SkIntToScalar(aSourceRect.x), -SkIntToScalar(aSourceRect.y), &paint);
  mCanvas->restore();
}

bool
DrawTargetSkia::Init(const IntSize &aSize, SurfaceFormat aFormat)
{
  if (size_t(std::max(aSize.width, aSize.height)) > GetMaxSurfaceSize()) {
    return false;
  }

  // we need to have surfaces that have a stride aligned to 4 for interop with cairo
  int stride = (BytesPerPixel(aFormat)*aSize.width + (4-1)) & -4;

  SkBitmap bitmap;
  bitmap.setInfo(MakeSkiaImageInfo(aSize, aFormat), stride);
  if (!bitmap.tryAllocPixels()) {
    return false;
  }

  bitmap.eraseColor(SK_ColorTRANSPARENT);

  mCanvas.adopt(new SkCanvas(bitmap));
  mSize = aSize;

  mFormat = aFormat;
  return true;
}

#ifdef USE_SKIA_GPU
/** Indicating a DT should be cached means that space will be reserved in Skia's cache
 * for the render target at creation time, with any unused resources exceeding the cache
 * limits being purged. When the DT is freed, it will then be guaranteed to be kept around
 * for subsequent allocations until it gets incidentally purged.
 *
 * If it is not marked as cached, no space will be purged to make room for the render
 * target in the cache. When the DT is freed, If there is space within the resource limits
 * it may be added to the cache, otherwise it will be freed immediately if the cache is
 * already full.
 *
 * If you want to ensure that the resources will be kept around for reuse, it is better
 * to mark them as cached. Such resources should be short-lived to ensure they don't
 * permanently tie up cache resource limits. Long-lived resources should generally be
 * left as uncached.
 *
 * In neither case will cache resource limits affect whether the resource allocation
 * succeeds. The amount of in-use GPU resources is allowed to exceed the size of the cache.
 * Thus, only hard GPU out-of-memory conditions will cause resource allocation to fail.
 */
bool
DrawTargetSkia::InitWithGrContext(GrContext* aGrContext,
                                  const IntSize &aSize,
                                  SurfaceFormat aFormat,
                                  bool aCached)
{
  MOZ_ASSERT(aGrContext, "null GrContext");

  if (size_t(std::max(aSize.width, aSize.height)) > GetMaxSurfaceSize()) {
    return false;
  }

  // Create a GPU rendertarget/texture using the supplied GrContext.
  // NewRenderTarget also implicitly clears the underlying texture on creation.
  sk_sp<SkSurface> gpuSurface =
    SkSurface::MakeRenderTarget(aGrContext,
                                SkBudgeted(aCached),
                                MakeSkiaImageInfo(aSize, aFormat));
  if (!gpuSurface) {
    return false;
  }

  mGrContext = aGrContext;
  mSize = aSize;
  mFormat = aFormat;

  mCanvas = gpuSurface->getCanvas();

  return true;
}

#endif

#ifdef DEBUG
bool
VerifyRGBXFormat(uint8_t* aData, const IntSize &aSize, const int32_t aStride, SurfaceFormat aFormat)
{
  // We should've initialized the data to be opaque already
  // On debug builds, verify that this is actually true.
  int height = aSize.height;
  int width = aSize.width;

  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; column += 4) {
#ifdef IS_BIG_ENDIAN
      MOZ_ASSERT(aData[column] == 0xFF);
#else
      MOZ_ASSERT(aData[column + 3] == 0xFF);
#endif
    }
    aData += aStride;
  }

  return true;
}
#endif

void
DrawTargetSkia::Init(unsigned char* aData, const IntSize &aSize, int32_t aStride, SurfaceFormat aFormat, bool aUninitialized)
{
  MOZ_ASSERT((aFormat != SurfaceFormat::B8G8R8X8) ||
              aUninitialized || VerifyRGBXFormat(aData, aSize, aStride, aFormat));

  SkBitmap bitmap;
  bitmap.setInfo(MakeSkiaImageInfo(aSize, aFormat), aStride);
  bitmap.setPixels(aData);

  mCanvas.adopt(new SkCanvas(bitmap));

  mSize = aSize;
  mFormat = aFormat;
}

void
DrawTargetSkia::SetTransform(const Matrix& aTransform)
{
  SkMatrix mat;
  GfxMatrixToSkiaMatrix(aTransform, mat);
  mCanvas->setMatrix(mat);
  mTransform = aTransform;
}

void*
DrawTargetSkia::GetNativeSurface(NativeSurfaceType aType)
{
#ifdef USE_SKIA_GPU
  if (aType == NativeSurfaceType::OPENGL_TEXTURE) {
    // Get the current texture backing the GPU device.
    // Beware - this texture is only guaranteed to valid after a draw target flush.
    GrRenderTarget* rt = mCanvas->getDevice()->accessRenderTarget();
    if (rt) {
      GrTexture* tex = rt->asTexture();
      if (tex) {
        return (void*)(uintptr_t)reinterpret_cast<GrGLTextureInfo *>(tex->getTextureHandle())->fID;
      }
    }
  }
#endif
  return nullptr;
}


already_AddRefed<PathBuilder>
DrawTargetSkia::CreatePathBuilder(FillRule aFillRule) const
{
  return MakeAndAddRef<PathBuilderSkia>(aFillRule);
}

void
DrawTargetSkia::ClearRect(const Rect &aRect)
{
  MarkChanged();
  mCanvas->save();
  mCanvas->clipRect(RectToSkRect(aRect), SkRegion::kIntersect_Op, true);
  mCanvas->clear(SK_ColorTRANSPARENT);
  mCanvas->restore();
}

void
DrawTargetSkia::PushClip(const Path *aPath)
{
  if (aPath->GetBackendType() != BackendType::SKIA) {
    return;
  }

  const PathSkia *skiaPath = static_cast<const PathSkia*>(aPath);
  mCanvas->save();
  mCanvas->clipPath(skiaPath->GetPath(), SkRegion::kIntersect_Op, true);
}

void
DrawTargetSkia::PushClipRect(const Rect& aRect)
{
  SkRect rect = RectToSkRect(aRect);

  mCanvas->save();
  mCanvas->clipRect(rect, SkRegion::kIntersect_Op, true);
}

void
DrawTargetSkia::PopClip()
{
  mCanvas->restore();
}

// Image filter that just passes the source through to the result unmodified.
class CopyLayerImageFilter : public SkImageFilter
{
public:
  CopyLayerImageFilter()
    : SkImageFilter(0, nullptr)
  {}

  virtual bool onFilterImageDeprecated(Proxy*, const SkBitmap& src, const Context&,
                                       SkBitmap* result, SkIPoint* offset) const override {
    *result = src;
    offset->set(0, 0);
    return true;
  }

  SK_TO_STRING_OVERRIDE()
  SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(CopyLayerImageFilter)
};

sk_sp<SkFlattenable>
CopyLayerImageFilter::CreateProc(SkReadBuffer& buffer)
{
  SK_IMAGEFILTER_UNFLATTEN_COMMON(common, 0);
  return sk_make_sp<CopyLayerImageFilter>();
}

#ifndef SK_IGNORE_TO_STRING
void
CopyLayerImageFilter::toString(SkString* str) const
{
  str->append("CopyLayerImageFilter: ()");
}
#endif

void
DrawTargetSkia::PushLayer(bool aOpaque, Float aOpacity, SourceSurface* aMask,
                          const Matrix& aMaskTransform, const IntRect& aBounds,
                          bool aCopyBackground)
{
  PushedLayer layer(GetPermitSubpixelAA(), aOpaque, aOpacity, aMask, aMaskTransform);
  mPushedLayers.push_back(layer);

  SkPaint paint;

  // If we have a mask, set the opacity to 0 so that SkCanvas::restore skips
  // implicitly drawing the layer so that we can properly mask it in PopLayer.
  paint.setAlpha(aMask ? 0 : ColorFloatToByte(aOpacity));

  SkRect bounds = IntRectToSkRect(aBounds);

  SkAutoTUnref<SkImageFilter> backdrop(aCopyBackground ? new CopyLayerImageFilter : nullptr);

  SkCanvas::SaveLayerRec saveRec(aBounds.IsEmpty() ? nullptr : &bounds,
                                 &paint,
                                 backdrop.get(),
                                 aOpaque ? SkCanvas::kIsOpaque_SaveLayerFlag : 0);

  mCanvas->saveLayer(saveRec);

  SetPermitSubpixelAA(aOpaque);

#ifdef MOZ_WIDGET_COCOA
  CGContextRelease(mCG);
  mCG = nullptr;
#endif
}

void
DrawTargetSkia::PopLayer()
{
  MarkChanged();

  MOZ_ASSERT(mPushedLayers.size());
  const PushedLayer& layer = mPushedLayers.back();

  if (layer.mMask) {
    // If we have a mask, take a reference to the layer's bitmap device so that
    // we can mask it ourselves. This assumes we forced SkCanvas::restore to
    // skip implicitly drawing the layer.
    SkAutoTUnref<SkBaseDevice> layerDevice(SkSafeRef(mCanvas->getTopDevice()));
    SkIRect layerBounds = layerDevice->getGlobalBounds();
    SkBitmap layerBitmap = layerDevice->accessBitmap(false);

    // Restore the background with the layer's device left alive.
    mCanvas->restore();

    SkPaint paint;
    paint.setAlpha(ColorFloatToByte(layer.mOpacity));

    SkMatrix maskMat, layerMat;
    // Get the total transform affecting the mask, considering its pattern
    // transform and the current canvas transform.
    GfxMatrixToSkiaMatrix(layer.mMaskTransform, maskMat);
    maskMat.postConcat(mCanvas->getTotalMatrix());
    if (!maskMat.invert(&layerMat)) {
      gfxDebug() << *this << ": PopLayer() failed to invert mask transform";
    } else {
      // The layer should not be affected by the current canvas transform,
      // even though the mask is. So first we use the inverse of the transform
      // affecting the mask, then add back on the layer's origin.
      layerMat.preTranslate(layerBounds.x(), layerBounds.y());
      sk_sp<SkShader> shader = SkShader::MakeBitmapShader(layerBitmap,
                                                          SkShader::kClamp_TileMode,
                                                          SkShader::kClamp_TileMode,
                                                          &layerMat);
      paint.setShader(shader);

      SkBitmap mask = GetBitmapForSurface(layer.mMask);
      if (mask.colorType() != kAlpha_8_SkColorType &&
          !mask.extractAlpha(&mask)) {
        gfxDebug() << *this << ": PopLayer() failed to extract alpha for mask";
      } else {
        mCanvas->save();

        // The layer may be smaller than the canvas size, so make sure drawing is
        // clipped to within the bounds of the layer.
        mCanvas->resetMatrix();
        mCanvas->clipRect(SkRect::Make(layerBounds));

        mCanvas->setMatrix(maskMat);
        mCanvas->drawBitmap(mask, 0, 0, &paint);

        mCanvas->restore();
      }
    }
  } else {
    mCanvas->restore();
  }

  SetPermitSubpixelAA(layer.mOldPermitSubpixelAA);

  mPushedLayers.pop_back();

#ifdef MOZ_WIDGET_COCOA
  CGContextRelease(mCG);
  mCG = nullptr;
#endif
}

already_AddRefed<GradientStops>
DrawTargetSkia::CreateGradientStops(GradientStop *aStops, uint32_t aNumStops, ExtendMode aExtendMode) const
{
  std::vector<GradientStop> stops;
  stops.resize(aNumStops);
  for (uint32_t i = 0; i < aNumStops; i++) {
    stops[i] = aStops[i];
  }
  std::stable_sort(stops.begin(), stops.end());

  return MakeAndAddRef<GradientStopsSkia>(stops, aNumStops, aExtendMode);
}

already_AddRefed<FilterNode>
DrawTargetSkia::CreateFilter(FilterType aType)
{
  return FilterNodeSoftware::Create(aType);
}

void
DrawTargetSkia::MarkChanged()
{
  if (mSnapshot) {
    mSnapshot->DrawTargetWillChange();
    mSnapshot = nullptr;
  }
}

void
DrawTargetSkia::SnapshotDestroyed()
{
  mSnapshot = nullptr;
}

} // namespace gfx
} // namespace mozilla
