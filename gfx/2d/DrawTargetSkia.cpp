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
{
}

DrawTargetSkia::~DrawTargetSkia()
{
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
  if (!mCanvas->peekPixels(nullptr, nullptr)) {
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
      SkShader::TileMode mode = ExtendModeToTileMode(stops->mExtendMode, Axis::BOTH);

      if (stops->mCount >= 2) {
        SkPoint points[2];
        points[0] = SkPoint::Make(SkFloatToScalar(pat.mBegin.x), SkFloatToScalar(pat.mBegin.y));
        points[1] = SkPoint::Make(SkFloatToScalar(pat.mEnd.x), SkFloatToScalar(pat.mEnd.y));

        SkMatrix mat;
        GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
        SkShader* shader = SkGradientShader::CreateLinear(points,
                                                          &stops->mColors.front(),
                                                          &stops->mPositions.front(),
                                                          stops->mCount,
                                                          mode, 0, &mat);
        SkSafeUnref(aPaint.setShader(shader));
      } else {
        aPaint.setColor(SK_ColorTRANSPARENT);
      }
      break;
    }
    case PatternType::RADIAL_GRADIENT: {
      const RadialGradientPattern& pat = static_cast<const RadialGradientPattern&>(aPattern);
      GradientStopsSkia *stops = static_cast<GradientStopsSkia*>(pat.mStops.get());
      SkShader::TileMode mode = ExtendModeToTileMode(stops->mExtendMode, Axis::BOTH);

      if (stops->mCount >= 2) {
        SkPoint points[2];
        points[0] = SkPoint::Make(SkFloatToScalar(pat.mCenter1.x), SkFloatToScalar(pat.mCenter1.y));
        points[1] = SkPoint::Make(SkFloatToScalar(pat.mCenter2.x), SkFloatToScalar(pat.mCenter2.y));

        SkMatrix mat;
        GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
        SkShader* shader = SkGradientShader::CreateTwoPointConical(points[0],
                                                                   SkFloatToScalar(pat.mRadius1),
                                                                   points[1],
                                                                   SkFloatToScalar(pat.mRadius2),
                                                                   &stops->mColors.front(),
                                                                   &stops->mPositions.front(),
                                                                   stops->mCount,
                                                                   mode, 0, &mat);
        SkSafeUnref(aPaint.setShader(shader));
      } else {
        aPaint.setColor(SK_ColorTRANSPARENT);
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

      SkShader* shader = SkShader::CreateBitmapShader(bitmap, xTileMode, yTileMode, &mat);
      SkSafeUnref(aPaint.setShader(shader));
      if (pat.mFilter == Filter::POINT) {
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
  if (aSurfOptions.mFilter == Filter::POINT) {
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
  if (!(aSurface->GetType() == SurfaceType::SKIA || aSurface->GetType() == SurfaceType::DATA)) {
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
  shadowPaint.setXfermode(paint.getXfermode());

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
    SkAutoTUnref<SkImageFilter> blurFilter(SkBlurImageFilter::Create(aSigma, aSigma));
    SkAutoTUnref<SkColorFilter> colorFilter(
      SkColorFilter::CreateModeFilter(ColorToSkColor(aColor, 1.0f), SkXfermode::kSrcIn_Mode));

    shadowPaint.setImageFilter(blurFilter.get());
    shadowPaint.setColorFilter(colorFilter.get());

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
        return true;
      default:
        // TODO: Figure out what to do for the other platforms.
        return false;
    }
  }
  return (aAntialiasMode == AntialiasMode::SUBPIXEL);
}

void
DrawTargetSkia::FillGlyphs(ScaledFont *aFont,
                           const GlyphBuffer &aBuffer,
                           const Pattern &aPattern,
                           const DrawOptions &aOptions,
                           const GlyphRenderingOptions *aRenderingOptions)
{
  if (aFont->GetType() != FontType::MAC &&
      aFont->GetType() != FontType::SKIA &&
      aFont->GetType() != FontType::GDI &&
      aFont->GetType() != FontType::DWRITE) {
    return;
  }

  MarkChanged();

  ScaledFontBase* skiaFont = static_cast<ScaledFontBase*>(aFont);

  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern);
  paint.mPaint.setTypeface(skiaFont->GetSkTypeface());
  paint.mPaint.setTextSize(SkFloatToScalar(skiaFont->mSize));
  paint.mPaint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

  bool shouldLCDRenderText = ShouldLCDRenderText(aFont->GetType(), aOptions.mAntialiasMode);
  paint.mPaint.setLCDRenderText(shouldLCDRenderText);

  if (aRenderingOptions && aRenderingOptions->GetType() == FontType::CAIRO) {
    const GlyphRenderingOptionsCairo* cairoOptions =
      static_cast<const GlyphRenderingOptionsCairo*>(aRenderingOptions);

    paint.mPaint.setHinting(GfxHintingToSkiaHinting(cairoOptions->GetHinting()));

    if (cairoOptions->GetAutoHinting()) {
      paint.mPaint.setAutohinted(true);
    }

    if (cairoOptions->GetAntialiasMode() == AntialiasMode::NONE) {
      paint.mPaint.setAntiAlias(false);
    }
  } else {
    // SkFontHost_cairo does not support subpixel text, so only enable it for other font hosts.
    paint.mPaint.setSubpixelText(true);

    if (aFont->GetType() == FontType::MAC && shouldLCDRenderText) {
      // SkFontHost_mac only supports subpixel antialiasing when hinting is turned off.
      paint.mPaint.setHinting(SkPaint::kNo_Hinting);
    } else {
      paint.mPaint.setHinting(SkPaint::kNormal_Hinting);
    }
  }

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
  SkAutoTUnref<SkRasterizer> raster(builder.detachRasterizer());
  paint.mPaint.setRasterizer(raster.get());

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
    SkShader* matrixShader = paint.mPaint.getShader()->newWithLocalMatrix(transform);
    SkSafeUnref(paint.mPaint.setShader(matrixShader));
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

    SkAutoTUnref<GrTexture> texture(mGrContext->textureProvider()->wrapBackendTexture(texDesc));

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
  mCanvas->resetMatrix();
  SkRect dest = IntRectToSkRect(IntRect(aDestination.x, aDestination.y, aSourceRect.width, aSourceRect.height));
  SkIRect source = IntRectToSkIRect(aSourceRect);
  mCanvas->clipRect(dest, SkRegion::kReplace_Op);

  SkPaint paint;
  if (!bitmap.isOpaque()) {
    // Keep the xfermode as SOURCE_OVER for opaque bitmaps
    // http://code.google.com/p/skia/issues/detail?id=628
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  }
  // drawBitmapRect with A8 bitmaps ends up doing a mask operation
  // so we need to clear before
  if (bitmap.colorType() == kAlpha_8_SkColorType) {
    mCanvas->clear(SK_ColorTRANSPARENT);
  }
  mCanvas->drawBitmapRect(bitmap, source, dest, &paint);
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
  SkAutoTUnref<SkSurface> gpuSurface(
    SkSurface::NewRenderTarget(aGrContext,
                               SkSurface::Budgeted(aCached),
                               MakeSkiaImageInfo(aSize, aFormat)));
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

  virtual bool onFilterImage(Proxy*, const SkBitmap& src, const Context&,
                             SkBitmap* result, SkIPoint* offset) const override {
    *result = src;
    offset->set(0, 0);
    return true;
  }

  SK_TO_STRING_OVERRIDE()
  SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(CopyLayerImageFilter)
};

SkFlattenable*
CopyLayerImageFilter::CreateProc(SkReadBuffer& buffer)
{
  SK_IMAGEFILTER_UNFLATTEN_COMMON(common, 0);
  return new CopyLayerImageFilter;
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
      SkShader* shader = SkShader::CreateBitmapShader(layerBitmap,
                                                      SkShader::kClamp_TileMode,
                                                      SkShader::kClamp_TileMode,
                                                      &layerMat);
      SkSafeUnref(paint.setShader(shader));

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
