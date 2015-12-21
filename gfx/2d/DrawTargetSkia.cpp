/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetSkia.h"
#include "SourceSurfaceCairo.h"
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
#include "Logging.h"
#include "Tools.h"
#include "DataSurfaceHelpers.h"
#include <algorithm>

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
 * the SkBitmap. So we return a pair of the SkBitmap and the (optional)
 * temporary surface.
 */
struct TempBitmap
{
  SkBitmap mBitmap;
  RefPtr<SourceSurface> mTmpSurface;
};

static TempBitmap
GetBitmapForSurface(SourceSurface* aSurface)
{
  TempBitmap result;

  if (aSurface->GetType() == SurfaceType::SKIA) {
    result.mBitmap = static_cast<SourceSurfaceSkia*>(aSurface)->GetBitmap();
    return result;
  }

  RefPtr<DataSourceSurface> surf = aSurface->GetDataSurface();
  if (!surf) {
    gfxDevCrash(LogReason::SourceSurfaceIncompatible) << "Non-skia SourceSurfaces need to be DataSourceSurfaces";
    return result;
  }

  SkAlphaType alphaType = (surf->GetFormat() == SurfaceFormat::B8G8R8X8) ?
    kOpaque_SkAlphaType : kPremul_SkAlphaType;

  SkImageInfo info = SkImageInfo::Make(surf->GetSize().width,
                                       surf->GetSize().height,
                                       GfxFormatToSkiaColorType(surf->GetFormat()),
                                       alphaType);
  result.mBitmap.setInfo(info, surf->Stride());

  result.mBitmap.setPixels(surf->GetData());
  result.mTmpSurface = surf.forget();
  return result;
}

DrawTargetSkia::DrawTargetSkia()
  :
#ifdef USE_SKIA_GPU
 mTexture(0),
#endif
 mSnapshot(nullptr)
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
                          int32_t* aStride, SurfaceFormat* aFormat)
{
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
  return true;
}

void
DrawTargetSkia::ReleaseBits(uint8_t* aData)
{
}

static void
SetPaintPattern(SkPaint& aPaint, const Pattern& aPattern, TempBitmap& aTmpBitmap,
                Float aAlpha = 1.0)
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

        SkShader* shader = SkGradientShader::CreateLinear(points,
                                                          &stops->mColors.front(),
                                                          &stops->mPositions.front(),
                                                          stops->mCount,
                                                          mode);

        if (shader) {
            SkMatrix mat;
            GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
            SkShader* matrixShader = SkShader::CreateLocalMatrixShader(shader, mat);
            SkSafeUnref(shader);
            SkSafeUnref(aPaint.setShader(matrixShader));
        }

      } else {
        aPaint.setColor(SkColorSetARGB(0, 0, 0, 0));
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

        SkShader* shader = SkGradientShader::CreateTwoPointConical(points[0],
                                                                   SkFloatToScalar(pat.mRadius1),
                                                                   points[1],
                                                                   SkFloatToScalar(pat.mRadius2),
                                                                   &stops->mColors.front(),
                                                                   &stops->mPositions.front(),
                                                                   stops->mCount,
                                                                   mode);
        if (shader) {
            SkMatrix mat;
            GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
            SkShader* matrixShader = SkShader::CreateLocalMatrixShader(shader, mat);
            SkSafeUnref(shader);
            SkSafeUnref(aPaint.setShader(matrixShader));
        }

      } else {
        aPaint.setColor(SkColorSetARGB(0, 0, 0, 0));
      }
      break;
    }
    case PatternType::SURFACE: {
      const SurfacePattern& pat = static_cast<const SurfacePattern&>(aPattern);
      aTmpBitmap = GetBitmapForSurface(pat.mSurface);
      SkBitmap& bitmap = aTmpBitmap.mBitmap;

      SkMatrix mat;
      GfxMatrixToSkiaMatrix(pat.mMatrix, mat);

      if (!pat.mSamplingRect.IsEmpty()) {
        SkIRect rect = IntRectToSkIRect(pat.mSamplingRect);
        bitmap.extractSubset(&bitmap, rect);
        mat.preTranslate(rect.x(), rect.y());
      }

      SkShader::TileMode xTileMode = ExtendModeToTileMode(pat.mExtendMode, Axis::X_AXIS);
      SkShader::TileMode yTileMode = ExtendModeToTileMode(pat.mExtendMode, Axis::Y_AXIS);

      SkShader* shader = SkShader::CreateBitmapShader(bitmap, xTileMode, yTileMode);
      SkShader* matrixShader = SkShader::CreateLocalMatrixShader(shader, mat);
      SkSafeUnref(shader);
      SkSafeUnref(aPaint.setShader(matrixShader));
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
  aCanvas->getClipBounds(&clipBounds);
  return SkRectToRect(clipBounds);
}

struct AutoPaintSetup {
  AutoPaintSetup(SkCanvas *aCanvas, const DrawOptions& aOptions, const Pattern& aPattern, const Rect* aMaskBounds = nullptr)
    : mNeedsRestore(false), mAlpha(1.0)
  {
    Init(aCanvas, aOptions, aMaskBounds);
    SetPaintPattern(mPaint, aPattern, mTmpBitmap, mAlpha);
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

    Rect clipBounds = GetClipBounds(aCanvas);
    bool needsGroup = !IsOperatorBoundByMask(aOptions.mCompositionOp) &&
                      (!aMaskBounds || !aMaskBounds->Contains(clipBounds));

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
  TempBitmap mTmpBitmap;
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

  TempBitmap bitmap = GetBitmapForSurface(aSurface);

  AutoPaintSetup paint(mCanvas.get(), aOptions, &aDest);
  if (aSurfOptions.mFilter == Filter::POINT) {
    paint.mPaint.setFilterQuality(kNone_SkFilterQuality);
  }

  mCanvas->drawBitmapRect(bitmap.mBitmap, sourceRect, destRect, &paint.mPaint);
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

  TempBitmap bitmap = GetBitmapForSurface(aSurface);

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
  SkAutoTUnref<SkImageFilter> blurFilter(SkBlurImageFilter::Create(aSigma, aSigma));
  SkAutoTUnref<SkColorFilter> colorFilter(
    SkColorFilter::CreateModeFilter(ColorToSkColor(aColor, 1.0), SkXfermode::kSrcIn_Mode));

  shadowPaint.setXfermode(paint.getXfermode());
  shadowPaint.setImageFilter(blurFilter.get());
  shadowPaint.setColorFilter(colorFilter.get());

  // drawBitmap implicitly calls saveLayer with a src-over xfer mode if given
  // an image filter, whereas the supplied xfer mode gets used to render into
  // the layer, which is the wrong order. We instead must use drawSprite which
  // applies the image filter directly to the bitmap without rendering it first,
  // then uses the xfer mode to composite it.
  IntPoint shadowDest = RoundedToInt(aDest + aOffset);
  mCanvas->drawSprite(bitmap.mBitmap, shadowDest.x, shadowDest.y, &shadowPaint);

  // Composite the original image after the shadow
  IntPoint dest = RoundedToInt(aDest);
  mCanvas->drawSprite(bitmap.mBitmap, dest.x, dest.y, &paint);

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
      aFont->GetType() != FontType::GDI) {
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
  paint.mPaint.setSubpixelText(true);

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
  } else if (aFont->GetType() == FontType::MAC && shouldLCDRenderText) {
    // SkFontHost_mac only supports subpixel antialiasing when hinting is turned off.
    paint.mPaint.setHinting(SkPaint::kNo_Hinting);
  } else {
    paint.mPaint.setHinting(SkPaint::kNormal_Hinting);
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
  TempBitmap tmpBitmap;
  SetPaintPattern(maskPaint, aMask, tmpBitmap);

  SkLayerRasterizer::Builder builder;
  builder.addLayer(maskPaint);
  SkAutoTUnref<SkRasterizer> raster(builder.detachRasterizer());
  paint.mPaint.setRasterizer(raster.get());

  mCanvas->drawRect(SkRectCoveringWholeSurface(), paint.mPaint);
}

void
DrawTargetSkia::MaskSurface(const Pattern &aSource,
                            SourceSurface *aMask,
                            Point aOffset,
                            const DrawOptions &aOptions)
{
  MarkChanged();
  AutoPaintSetup paint(mCanvas.get(), aOptions, aSource);

  TempBitmap bitmap = GetBitmapForSurface(aMask);
  if (bitmap.mBitmap.colorType() == kAlpha_8_SkColorType) {
    if (aOffset != Point(0, 0)) {
      SkMatrix transform;
      transform.setTranslate(SkFloatToScalar(-aOffset.x), SkFloatToScalar(-aOffset.y));
      SkShader* matrixShader = SkShader::CreateLocalMatrixShader(paint.mPaint.getShader(), transform);
      SkSafeUnref(paint.mPaint.setShader(matrixShader));
    }

    mCanvas->drawBitmap(bitmap.mBitmap, aOffset.x, aOffset.y, &paint.mPaint);
  } else {
    SkPaint maskPaint;
    TempBitmap tmpBitmap;
    SetPaintPattern(maskPaint,
                    SurfacePattern(aMask, ExtendMode::CLAMP, Matrix::Translation(aOffset)),
                    tmpBitmap);

    SkLayerRasterizer::Builder builder;
    builder.addLayer(maskPaint);
    SkAutoTUnref<SkRasterizer> raster(builder.detachRasterizer());
    paint.mPaint.setRasterizer(raster.get());

    IntSize size = aMask->GetSize();
    Rect rect = Rect(aOffset.x, aOffset.y, size.width, size.height);
    mCanvas->drawRect(RectToSkRect(rect), paint.mPaint);
  }
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
  if (!target->Init(aSize, aFormat)) {
    return nullptr;
  }
  return target.forget();
}

bool
DrawTargetSkia::UsingSkiaGPU() const
{
#ifdef USE_SKIA_GPU
  return !!mTexture;
#else
  return false;
#endif
}

already_AddRefed<SourceSurface>
DrawTargetSkia::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  if (aSurface->GetType() == SurfaceType::SKIA) {
    RefPtr<SourceSurface> surface(aSurface);
    return surface.forget();
  }

  if (!UsingSkiaGPU()) {
    // If we're not using skia-gl then drawing doesn't require any
    // uploading, so any data surface is fine. Call GetDataSurface
    // to trigger any required readback so that it only happens
    // once.
    return aSurface->GetDataSurface();
  }

  // If we are using skia-gl then we want to copy into a surface that
  // will cache the uploaded gl texture.
  RefPtr<DataSourceSurface> dataSurf = aSurface->GetDataSurface();
  DataSourceSurface::MappedSurface map;
  if (!dataSurf->Map(DataSourceSurface::READ, &map)) {
    return nullptr;
  }

  RefPtr<SourceSurface> result = CreateSourceSurfaceFromData(map.mData,
                                                             dataSurf->GetSize(),
                                                             map.mStride,
                                                             dataSurf->GetFormat());
  dataSurf->Unmap();
  return result.forget();
}

already_AddRefed<SourceSurface>
DrawTargetSkia::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
  if (aSurface.mType == NativeSurfaceType::CAIRO_SURFACE) {
    if (aSurface.mSize.width <= 0 ||
        aSurface.mSize.height <= 0) {
      gfxWarning() << "Can't create a SourceSurface without a valid size";
      return nullptr;
    }
    cairo_surface_t* surf = static_cast<cairo_surface_t*>(aSurface.mSurface);
    return MakeAndAddRef<SourceSurfaceCairo>(surf, aSurface.mSize, aSurface.mFormat);
#if USE_SKIA_GPU
  } else if (aSurface.mType == NativeSurfaceType::OPENGL_TEXTURE && UsingSkiaGPU()) {
    RefPtr<SourceSurfaceSkia> newSurf = new SourceSurfaceSkia();
    unsigned int texture = (unsigned int)((uintptr_t)aSurface.mSurface);
    if (newSurf->InitFromTexture((DrawTargetSkia*)this, texture, aSurface.mSize, aSurface.mFormat)) {
      return newSurf.forget();
    }
    return nullptr;
#endif
  }

  return nullptr;
}

void
DrawTargetSkia::CopySurface(SourceSurface *aSurface,
                            const IntRect& aSourceRect,
                            const IntPoint &aDestination)
{
  //TODO: We could just use writePixels() here if the sourceRect is the entire source

  if (aSurface->GetType() != SurfaceType::SKIA && aSurface->GetType() != SurfaceType::DATA) {
    return;
  }

  MarkChanged();

  TempBitmap bitmap = GetBitmapForSurface(aSurface);

  // This is a fast path that is disabled for now to mimimize risk
  if (false && !bitmap.mBitmap.getTexture() && mCanvas->imageInfo() == bitmap.mBitmap.info()) {
    SkBitmap bm(bitmap.mBitmap);
    bm.lockPixels();
    if (bm.getPixels()) {
      SkImageInfo info = bm.info().makeWH(aSourceRect.width, aSourceRect.height);
      uint8_t* pixels = static_cast<uint8_t*>(bm.getPixels());
      // adjust pixels for the source offset
      pixels += aSourceRect.x + aSourceRect.y*bm.rowBytes();
      mCanvas->writePixels(info, pixels, bm.rowBytes(), aDestination.x, aDestination.y);
      return;
    }
  }

  mCanvas->save();
  mCanvas->resetMatrix();
  SkRect dest = IntRectToSkRect(IntRect(aDestination.x, aDestination.y, aSourceRect.width, aSourceRect.height));
  SkIRect source = IntRectToSkIRect(aSourceRect);
  mCanvas->clipRect(dest, SkRegion::kReplace_Op);
  SkPaint paint;

  if (mCanvas->imageInfo().colorType() == kRGB_565_SkColorType) {
    // Set the xfermode to SOURCE_OVER to workaround
    // http://code.google.com/p/skia/issues/detail?id=628
    // RGB565 is opaque so they're equivalent anyway
    paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  } else {
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  }
  // drawBitmapRect with A8 bitmaps ends up doing a mask operation
  // so we need to clear before
  if (bitmap.mBitmap.colorType() == kAlpha_8_SkColorType) {
    SkPaint clearPaint;
    clearPaint.setColor(SkColorSetARGB(0, 0, 0, 0));
    clearPaint.setXfermodeMode(SkXfermode::kSrc_Mode);
    mCanvas->drawPaint(clearPaint);
  }
  mCanvas->drawBitmapRect(bitmap.mBitmap, source, dest, &paint);
  mCanvas->restore();
}

bool
DrawTargetSkia::Init(const IntSize &aSize, SurfaceFormat aFormat)
{
  if (size_t(std::max(aSize.width, aSize.height)) > GetMaxSurfaceSize()) {
    return false;
  }

  SkAlphaType alphaType = (aFormat == SurfaceFormat::B8G8R8X8) ?
    kOpaque_SkAlphaType : kPremul_SkAlphaType;

  SkImageInfo skiInfo = SkImageInfo::Make(
        aSize.width, aSize.height,
        GfxFormatToSkiaColorType(aFormat),
        alphaType);
  // we need to have surfaces that have a stride aligned to 4 for interop with cairo
  int stride = (BytesPerPixel(aFormat)*aSize.width + (4-1)) & -4;

  SkBitmap bitmap;
  bitmap.setInfo(skiInfo, stride);
  if (!bitmap.tryAllocPixels()) {
    return false;
  }

  bitmap.eraseARGB(0, 0, 0, 0);

  mCanvas.adopt(new SkCanvas(bitmap));
  mSize = aSize;

  mFormat = aFormat;
  return true;
}

#ifdef USE_SKIA_GPU
bool
DrawTargetSkia::InitWithGrContext(GrContext* aGrContext,
                                  const IntSize &aSize,
                                  SurfaceFormat aFormat)
{
  MOZ_ASSERT(aGrContext, "null GrContext");

  if (size_t(std::max(aSize.width, aSize.height)) > GetMaxSurfaceSize()) {
    return false;
  }

  mGrContext = aGrContext;
  mSize = aSize;
  mFormat = aFormat;

  GrSurfaceDesc targetDescriptor;

  targetDescriptor.fFlags = kRenderTarget_GrSurfaceFlag;
  targetDescriptor.fWidth = mSize.width;
  targetDescriptor.fHeight = mSize.height;
  targetDescriptor.fConfig = GfxFormatToGrConfig(mFormat);
  targetDescriptor.fOrigin = kBottomLeft_GrSurfaceOrigin;
  targetDescriptor.fSampleCnt = 0;

  SkAutoTUnref<GrTexture> skiaTexture(mGrContext->textureProvider()->createTexture(targetDescriptor, SkSurface::kNo_Budgeted, nullptr, 0));
  if (!skiaTexture) {
    return false;
  }

  SkAutoTUnref<SkSurface> gpuSurface(SkSurface::NewRenderTargetDirect(skiaTexture->asRenderTarget()));
  if (!gpuSurface) {
    return false;
  }

  mTexture = reinterpret_cast<GrGLTextureInfo *>(skiaTexture->getTextureHandle())->fID;

  mCanvas = gpuSurface->getCanvas();

  return true;
}

#endif

void
DrawTargetSkia::Init(unsigned char* aData, const IntSize &aSize, int32_t aStride, SurfaceFormat aFormat)
{
  SkAlphaType alphaType = kPremul_SkAlphaType;
  if (aFormat == SurfaceFormat::B8G8R8X8) {
    // We have to manually set the A channel to be 255 as Skia doesn't understand BGRX
    ConvertBGRXToBGRA(aData, aSize, aStride);
    alphaType = kOpaque_SkAlphaType;
  }

  SkBitmap bitmap;

  SkImageInfo info = SkImageInfo::Make(aSize.width,
                                       aSize.height,
                                       GfxFormatToSkiaColorType(aFormat),
                                       alphaType);
  bitmap.setInfo(info, aStride);
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
    return (void*)((uintptr_t)mTexture);
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
  SkPaint paint;
  mCanvas->save();
  mCanvas->clipRect(RectToSkRect(aRect), SkRegion::kIntersect_Op, true);
  paint.setColor(SkColorSetARGB(0, 0, 0, 0));
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  mCanvas->drawPaint(paint);
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

// Return a rect (in user space) that covers the entire surface by applying
// the inverse of GetTransform() to (0, 0, mSize.width, mSize.height).
SkRect
DrawTargetSkia::SkRectCoveringWholeSurface() const
{
  return RectToSkRect(mTransform.TransformBounds(Rect(0, 0, mSize.width, mSize.height)));
}

void
DrawTargetSkia::SnapshotDestroyed()
{
  mSnapshot = nullptr;
}

} // namespace gfx
} // namespace mozilla
