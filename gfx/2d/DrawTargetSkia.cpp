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

#include "mozilla/ArrayUtils.h"

#include "skia/include/core/SkSurface.h"
#include "skia/include/core/SkTypeface.h"
#include "skia/include/effects/SkGradientShader.h"
#include "skia/include/core/SkColorFilter.h"
#include "skia/include/effects/SkBlurImageFilter.h"
#include "skia/include/effects/SkLayerRasterizer.h"
#include "skia/src/core/SkSpecialImage.h"
#include "Blur.h"
#include "Logging.h"
#include "Tools.h"
#include "DataSurfaceHelpers.h"
#include <algorithm>

#ifdef USE_SKIA_GPU
#include "GLDefs.h"
#include "skia/include/gpu/SkGr.h"
#include "skia/include/gpu/GrContext.h"
#include "skia/include/gpu/GrDrawContext.h"
#include "skia/include/gpu/gl/GrGLInterface.h"
#include "skia/src/image/SkImage_Gpu.h"
#endif

#ifdef MOZ_WIDGET_COCOA
#include "BorrowedContext.h"
#include <ApplicationServices/ApplicationServices.h>
#include "mozilla/Vector.h"
#include "ScaledFontMac.h"
#include "CGTextDrawing.h"
#endif

#ifdef XP_WIN
#include "ScaledFontDWrite.h"
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
 * When constructing a temporary SkImage via GetSkImageForSurface, we may also
 * have to construct a temporary DataSourceSurface, which must live as long as
 * the SkImage. We attach this temporary surface to the image's pixelref, so
 * that it can be released once the pixelref is freed.
 */
static void
ReleaseTemporarySurface(const void* aPixels, void* aContext)
{
  DataSourceSurface* surf = static_cast<DataSourceSurface*>(aContext);
  if (surf) {
    surf->Release();
  }
}

#ifdef IS_BIG_ENDIAN
static const int kARGBAlphaOffset = 0;
#else
static const int kARGBAlphaOffset = 3;
#endif

static void
WriteRGBXFormat(uint8_t* aData, const IntSize &aSize,
                const int32_t aStride, SurfaceFormat aFormat)
{
  if (aFormat != SurfaceFormat::B8G8R8X8 || aSize.IsEmpty()) {
    return;
  }

  int height = aSize.height;
  int width = aSize.width * 4;

  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; column += 4) {
      aData[column + kARGBAlphaOffset] = 0xFF;
    }
    aData += aStride;
  }

  return;
}

#ifdef DEBUG
static bool
VerifyRGBXFormat(uint8_t* aData, const IntSize &aSize, const int32_t aStride, SurfaceFormat aFormat)
{
  if (aFormat != SurfaceFormat::B8G8R8X8 || aSize.IsEmpty()) {
    return true;
  }
  // We should've initialized the data to be opaque already
  // On debug builds, verify that this is actually true.
  int height = aSize.height;
  int width = aSize.width * 4;

  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; column += 4) {
      if (aData[column + kARGBAlphaOffset] != 0xFF) {
        gfxCriticalError() << "RGBX pixel at (" << column << "," << row << ") in "
                           << width << "x" << height << " surface is not opaque: "
                           << int(aData[column]) << ","
                           << int(aData[column+1]) << ","
                           << int(aData[column+2]) << ","
                           << int(aData[column+3]);
      }
    }
    aData += aStride;
  }

  return true;
}

// Since checking every pixel is expensive, this only checks the four corners and center
// of a surface that their alpha value is 0xFF.
static bool
VerifyRGBXCorners(uint8_t* aData, const IntSize &aSize, const int32_t aStride, SurfaceFormat aFormat)
{
  if (aFormat != SurfaceFormat::B8G8R8X8 || aSize.IsEmpty()) {
    return true;
  }

  int height = aSize.height;
  int width = aSize.width;
  const int pixelSize = 4;
  const int strideDiff = aStride - (width * pixelSize);
  MOZ_ASSERT(width * pixelSize <= aStride);

  const int topLeft = 0;
  const int topRight = width * pixelSize - pixelSize;
  const int bottomRight = aStride * height - strideDiff - pixelSize;
  const int bottomLeft = aStride * height - aStride;

  // Lastly the center pixel
  int middleRowHeight = height / 2;
  int middleRowWidth = (width / 2) * pixelSize;
  const int middle = aStride * middleRowHeight + middleRowWidth;

  const int offsets[] = { topLeft, topRight, bottomRight, bottomLeft, middle };
  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(offsets); i++) {
    int offset = offsets[i];
    if (aData[offset + kARGBAlphaOffset] != 0xFF) {
        int row = offset / aStride;
        int column = (offset % aStride) / pixelSize;
        gfxCriticalError() << "RGBX corner pixel at (" << column << "," << row << ") in "
                           << width << "x" << height << " surface is not opaque: "
                           << int(aData[offset]) << ","
                           << int(aData[offset+1]) << ","
                           << int(aData[offset+2]) << ","
                           << int(aData[offset+3]);
    }
  }

  return true;
}
#endif

static sk_sp<SkImage>
GetSkImageForSurface(SourceSurface* aSurface)
{
  if (!aSurface) {
    gfxDebug() << "Creating null Skia image from null SourceSurface";
    return nullptr;
  }

  if (aSurface->GetType() == SurfaceType::SKIA) {
    return static_cast<SourceSurfaceSkia*>(aSurface)->GetImage();
  }

  DataSourceSurface* surf = aSurface->GetDataSurface().take();
  if (!surf) {
    gfxWarning() << "Failed getting DataSourceSurface for Skia image";
    return nullptr;
  }

  SkPixmap pixmap(MakeSkiaImageInfo(surf->GetSize(), surf->GetFormat()),
                  surf->GetData(), surf->Stride());
  sk_sp<SkImage> image = SkImage::MakeFromRaster(pixmap, ReleaseTemporarySurface, surf);
  if (!image) {
    ReleaseTemporarySurface(nullptr, surf);
    gfxDebug() << "Failed making Skia raster image for temporary surface";
  }

  // Skia doesn't support RGBX surfaces so ensure that the alpha value is opaque white.
  MOZ_ASSERT(VerifyRGBXCorners(surf->GetData(), surf->GetSize(),
                               surf->Stride(), surf->GetFormat()));
  return image;
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
    sk_sp<SkImage> image;
    // If the surface is raster, making a snapshot may trigger a pixel copy.
    // Instead, try to directly make a raster image referencing the surface pixels.
    SkPixmap pixmap;
    if (mSurface->peekPixels(&pixmap)) {
      image = SkImage::MakeFromRaster(pixmap, nullptr, nullptr);
    } else {
      image = mSurface->makeImageSnapshot(SkBudgeted::kNo);
    }
    if (!snapshot->InitFromImage(image, mFormat, this)) {
      return nullptr;
    }
    mSnapshot = snapshot;
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
  *aFormat = SkiaColorTypeToGfxFormat(info.colorType(), info.alphaType());
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
ReleaseImage(const void* aPixels, void* aContext)
{
  SkImage* image = static_cast<SkImage*>(aContext);
  SkSafeUnref(image);
}

static sk_sp<SkImage>
ExtractSubset(sk_sp<SkImage> aImage, const IntRect& aRect)
{
  SkIRect subsetRect = IntRectToSkIRect(aRect);
  if (aImage->bounds() == subsetRect) {
    return aImage;
  }
  // makeSubset is slow, so prefer to use SkPixmap::extractSubset where possible.
  SkPixmap pixmap, subsetPixmap;
  if (aImage->peekPixels(&pixmap) &&
      pixmap.extractSubset(&subsetPixmap, subsetRect)) {
    // Release the original image reference so only the subset image keeps it alive.
    return SkImage::MakeFromRaster(subsetPixmap, ReleaseImage, aImage.release());
  }
  return aImage->makeSubset(subsetRect);
}

static inline bool
SkImageIsMask(const sk_sp<SkImage>& aImage)
{
  SkPixmap pixmap;
  if (aImage->peekPixels(&pixmap)) {
    return pixmap.colorType() == kAlpha_8_SkColorType;
#ifdef USE_SKIA_GPU
  } else if (GrTexture* tex = aImage->getTexture()) {
    return GrPixelConfigIsAlphaOnly(tex->config());
#endif
  } else {
    return false;
  }
}

static bool
ExtractAlphaBitmap(sk_sp<SkImage> aImage, SkBitmap* aResultBitmap)
{
  SkImageInfo info = SkImageInfo::MakeA8(aImage->width(), aImage->height());
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(info, SkAlign4(info.minRowBytes())) ||
      !aImage->readPixels(bitmap.info(), bitmap.getPixels(), bitmap.rowBytes(), 0, 0)) {
    gfxWarning() << "Failed reading alpha pixels for Skia bitmap";
    return false;
  }

  *aResultBitmap = bitmap;
  return true;
}

static sk_sp<SkImage>
ExtractAlphaForSurface(SourceSurface* aSurface)
{
  sk_sp<SkImage> image = GetSkImageForSurface(aSurface);
  if (!image) {
    return nullptr;
  }
  if (SkImageIsMask(image)) {
    return image;
  }

  SkBitmap bitmap;
  if (!ExtractAlphaBitmap(image, &bitmap)) {
    return nullptr;
  }

  // Mark the bitmap immutable so that it will be shared rather than copied.
  bitmap.setImmutable();
  return SkImage::MakeFromBitmap(bitmap);
}

static void
SetPaintPattern(SkPaint& aPaint, const Pattern& aPattern, Float aAlpha = 1.0, Point aOffset = Point(0, 0))
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
        mat.postTranslate(SkFloatToScalar(aOffset.x), SkFloatToScalar(aOffset.y));
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
        mat.postTranslate(SkFloatToScalar(aOffset.x), SkFloatToScalar(aOffset.y));
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
      sk_sp<SkImage> image = GetSkImageForSurface(pat.mSurface);

      SkMatrix mat;
      GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
      mat.postTranslate(SkFloatToScalar(aOffset.x), SkFloatToScalar(aOffset.y));

      if (!pat.mSamplingRect.IsEmpty()) {
        image = ExtractSubset(image, pat.mSamplingRect);
        mat.preTranslate(pat.mSamplingRect.x, pat.mSamplingRect.y);
      }

      SkShader::TileMode xTileMode = ExtendModeToTileMode(pat.mExtendMode, Axis::X_AXIS);
      SkShader::TileMode yTileMode = ExtendModeToTileMode(pat.mExtendMode, Axis::Y_AXIS);

      if (image) {
        aPaint.setShader(image->makeShader(xTileMode, yTileMode, &mat));
      } else {
        aPaint.setColor(SK_ColorTRANSPARENT);
      }

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
  // Use a manually transformed getClipDeviceBounds instead of
  // getClipBounds because getClipBounds inflates the the bounds
  // by a pixel in each direction to compensate for antialiasing.
  SkIRect deviceBounds;
  if (!aCanvas->getClipDeviceBounds(&deviceBounds)) {
    return Rect();
  }
  SkMatrix inverseCTM;
  if (!aCanvas->getTotalMatrix().invert(&inverseCTM)) {
    return Rect();
  }
  SkRect localBounds;
  inverseCTM.mapRect(&localBounds, SkRect::Make(deviceBounds));
  return SkRectToRect(localBounds);
}

struct AutoPaintSetup {
  AutoPaintSetup(SkCanvas *aCanvas, const DrawOptions& aOptions, const Pattern& aPattern, const Rect* aMaskBounds = nullptr, Point aOffset = Point(0, 0))
    : mNeedsRestore(false), mAlpha(1.0)
  {
    Init(aCanvas, aOptions, aMaskBounds, false);
    SetPaintPattern(mPaint, aPattern, mAlpha, aOffset);
  }

  AutoPaintSetup(SkCanvas *aCanvas, const DrawOptions& aOptions, const Rect* aMaskBounds = nullptr, bool aForceGroup = false)
    : mNeedsRestore(false), mAlpha(1.0)
  {
    Init(aCanvas, aOptions, aMaskBounds, aForceGroup);
  }

  ~AutoPaintSetup()
  {
    if (mNeedsRestore) {
      mCanvas->restore();
    }
  }

  void Init(SkCanvas *aCanvas, const DrawOptions& aOptions, const Rect* aMaskBounds, bool aForceGroup)
  {
    mPaint.setBlendMode(GfxOpToSkiaOp(aOptions.mCompositionOp));
    mCanvas = aCanvas;

    //TODO: Can we set greyscale somehow?
    if (aOptions.mAntialiasMode != AntialiasMode::NONE) {
      mPaint.setAntiAlias(true);
    } else {
      mPaint.setAntiAlias(false);
    }

    bool needsGroup = aForceGroup ||
                      (!IsOperatorBoundByMask(aOptions.mCompositionOp) &&
                       (!aMaskBounds || !aMaskBounds->Contains(GetClipBounds(aCanvas))));

    // TODO: We could skip the temporary for operator_source and just
    // clear the clip rect. The other operators would be harder
    // but could be worth it to skip pushing a group.
    if (needsGroup) {
      mPaint.setBlendMode(SkBlendMode::kSrcOver);
      SkPaint temp;
      temp.setBlendMode(GfxOpToSkiaOp(aOptions.mCompositionOp));
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
  if (aSource.IsEmpty()) {
    return;
  }

  MarkChanged();

  sk_sp<SkImage> image = GetSkImageForSurface(aSurface);
  if (!image) {
    return;
  }

  SkRect destRect = RectToSkRect(aDest);
  SkRect sourceRect = RectToSkRect(aSource);
  bool forceGroup = SkImageIsMask(image) &&
                    aOptions.mCompositionOp != CompositionOp::OP_OVER;

  AutoPaintSetup paint(mCanvas.get(), aOptions, &aDest, forceGroup);
  if (aSurfOptions.mSamplingFilter == SamplingFilter::POINT) {
    paint.mPaint.setFilterQuality(kNone_SkFilterQuality);
  }

  mCanvas->drawImageRect(image, sourceRect, destRect, &paint.mPaint);
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
  if (aSurface->GetSize().IsEmpty()) {
    return;
  }

  MarkChanged();

  sk_sp<SkImage> image = GetSkImageForSurface(aSurface);
  if (!image) {
    return;
  }

  mCanvas->save();
  mCanvas->resetMatrix();

  SkPaint paint;
  paint.setBlendMode(GfxOpToSkiaOp(aOperator));

  // bug 1201272
  // We can't use the SkDropShadowImageFilter here because it applies the xfer
  // mode first to render the bitmap to a temporary layer, and then implicitly
  // uses src-over to composite the resulting shadow.
  // The canvas spec, however, states that the composite op must be used to
  // composite the resulting shadow, so we must instead use a SkBlurImageFilter
  // to blur the image ourselves.

  SkPaint shadowPaint;
  shadowPaint.setBlendMode(GfxOpToSkiaOp(aOperator));

  auto shadowDest = IntPoint::Round(aDest + aOffset);

  SkBitmap blurMask;
  if (!UsingSkiaGPU() &&
      ExtractAlphaBitmap(image, &blurMask)) {
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
      SkColorFilter::MakeModeFilter(ColorToSkColor(aColor, 1.0f), SkBlendMode::kSrcIn));

    shadowPaint.setImageFilter(blurFilter);
    shadowPaint.setColorFilter(colorFilter);

    mCanvas->drawImage(image, shadowDest.x, shadowDest.y, &shadowPaint);
  }

  // Composite the original image after the shadow
  auto dest = IntPoint::Round(aDest);
  mCanvas->drawImage(image, dest.x, dest.y, &paint);

  mCanvas->restore();
}

void
DrawTargetSkia::FillRect(const Rect &aRect,
                         const Pattern &aPattern,
                         const DrawOptions &aOptions)
{
  // The sprite blitting path in Skia can be faster than the shader blitter for
  // operators other than source (or source-over with opaque surface). So, when
  // possible/beneficial, route to DrawSurface which will use the sprite blitter.
  if (aPattern.GetType() == PatternType::SURFACE &&
      aOptions.mCompositionOp != CompositionOp::OP_SOURCE) {
    const SurfacePattern& pat = static_cast<const SurfacePattern&>(aPattern);
    // Verify there is a valid surface and a pattern matrix without skew.
    if (pat.mSurface &&
        (aOptions.mCompositionOp != CompositionOp::OP_OVER ||
         GfxFormatToSkiaAlphaType(pat.mSurface->GetFormat()) != kOpaque_SkAlphaType) &&
        !pat.mMatrix.HasNonAxisAlignedTransform()) {
      // Bound the sampling to smaller of the bounds or the sampling rect.
      IntRect srcRect(IntPoint(0, 0), pat.mSurface->GetSize());
      if (!pat.mSamplingRect.IsEmpty()) {
        srcRect = srcRect.Intersect(pat.mSamplingRect);
      }
      // Transform the destination rectangle by the inverse of the pattern
      // matrix so that it is in pattern space like the source rectangle.
      Rect patRect = aRect - pat.mMatrix.GetTranslation();
      patRect.Scale(1.0f / pat.mMatrix._11, 1.0f / pat.mMatrix._22);
      // Verify the pattern rectangle will not tile or clamp.
      if (!patRect.IsEmpty() && srcRect.Contains(RoundedOut(patRect))) {
        // The pattern is a surface with an axis-aligned source rectangle
        // fitting entirely in its bounds, so just treat it as a DrawSurface.
        DrawSurface(pat.mSurface, aRect, patRect,
                    DrawSurfaceOptions(pat.mSamplingFilter),
                    aOptions);
        return;
      }
    }
  }

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
  void clipRect(const SkRect& aRect, SkCanvas::ClipOp op, bool antialias) override {
    CGRect rect = CGRectMake(aRect.x(), aRect.y(), aRect.width(), aRect.height());
    CGContextClipToRect(mCG, rect);
  }

  void clipRRect(const SkRRect& rrect, SkCanvas::ClipOp op, bool antialias) override {
    SkPath path;
    path.addRRect(rrect);
    clipPath(path, op, antialias);
  }

  void clipPath(const SkPath& aPath, SkCanvas::ClipOp, bool antialias) override {
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

static inline CGAffineTransform
GfxMatrixToCGAffineTransform(const Matrix &m)
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
               sk_sp<SkCanvas> aCanvas)
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
    mColorSpace = (format == SurfaceFormat::A8) ?
                  CGColorSpaceCreateDeviceGray() : CGColorSpaceCreateDeviceRGB();
  }

  if (mCG) {
    // Release the old CG context since it's no longer valid.
    CGContextRelease(mCG);
  }

  mCanvasData = aSurfaceData;
  mCGSize = size;

  uint32_t bitmapInfo = (format == SurfaceFormat::A8) ?
                        kCGImageAlphaOnly :
                        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;

  mCG = CGBitmapContextCreateWithData(mCanvasData,
                                      mCGSize.width,
                                      mCGSize.height,
                                      8, /* bits per component */
                                      stride,
                                      mColorSpace,
                                      bitmapInfo,
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

CGContextRef
BorrowedCGContext::BorrowCGContextFromDrawTarget(DrawTarget *aDT)
{
  DrawTargetSkia* skiaDT = static_cast<DrawTargetSkia*>(aDT);
  return skiaDT->BorrowCGContext(DrawOptions());
}

void
BorrowedCGContext::ReturnCGContextToDrawTarget(DrawTarget *aDT, CGContextRef cg)
{
  DrawTargetSkia* skiaDT = static_cast<DrawTargetSkia*>(aDT);
  skiaDT->ReturnCGContext(cg);
  return;
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

static bool
CanDrawFont(ScaledFont* aFont)
{
  switch (aFont->GetType()) {
  case FontType::SKIA:
  case FontType::CAIRO:
  case FontType::FONTCONFIG:
  case FontType::MAC:
  case FontType::GDI:
  case FontType::DWRITE:
    return true;
  default:
    return false;
  }
}

void
DrawTargetSkia::FillGlyphs(ScaledFont *aFont,
                           const GlyphBuffer &aBuffer,
                           const Pattern &aPattern,
                           const DrawOptions &aOptions,
                           const GlyphRenderingOptions *aRenderingOptions)
{
  if (!CanDrawFont(aFont)) {
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
  AntialiasMode aaMode = aFont->GetDefaultAAMode();
  if (aOptions.mAntialiasMode != AntialiasMode::DEFAULT) {
    aaMode = aOptions.mAntialiasMode;
  }
  bool aaEnabled = aaMode != AntialiasMode::NONE;

  paint.mPaint.setAntiAlias(aaEnabled);
  paint.mPaint.setTypeface(sk_ref_sp(typeface));
  paint.mPaint.setTextSize(SkFloatToScalar(skiaFont->mSize));
  paint.mPaint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

  bool shouldLCDRenderText = ShouldLCDRenderText(aFont->GetType(), aaMode);
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
    if (aaMode == AntialiasMode::GRAY) {
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
  {
    if (!shouldLCDRenderText && aaEnabled) {
      // If we have non LCD GDI text, render the fonts as cleartype and convert them
      // to grayscale. This seems to be what Chrome and IE are doing on Windows 7.
      // This also applies if cleartype is disabled system wide.
      paint.mPaint.setFlags(paint.mPaint.getFlags() | SkPaint::kGenA8FromLCD_Flag);
    }
    break;
  }
#ifdef XP_WIN
  case FontType::DWRITE:
  {
    ScaledFontDWrite* dwriteFont = static_cast<ScaledFontDWrite*>(aFont);
    paint.mPaint.setEmbeddedBitmapText(dwriteFont->UseEmbeddedBitmaps());

    if (dwriteFont->ForceGDIMode()) {
      paint.mPaint.setEmbeddedBitmapText(true);
      useSubpixelText = false;
    }
    break;
  }
#endif
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
  AutoPaintSetup paint(mCanvas.get(), aOptions, aSource, nullptr, -aOffset);

  sk_sp<SkImage> alphaMask = ExtractAlphaForSurface(aMask);
  if (!alphaMask) {
    gfxDebug() << *this << ": MaskSurface() failed to extract alpha for mask";
    return;
  }

  mCanvas->drawImage(alphaMask, aOffset.x, aOffset.y, &paint.mPaint);
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
  sk_sp<SkImage> srcImage = GetSkImageForSurface(aSurface);
  if (!srcImage) {
    return true;
  }

  // Set up an intermediate destination surface only the size of the transformed bounds.
  // Try to pass through the source's format unmodified in both the BGRA and ARGB cases.
  RefPtr<DataSourceSurface> dstSurf =
    Factory::CreateDataSourceSurface(xformBounds.Size(),
                                     !srcImage->isOpaque() ?
                                       aSurface->GetFormat() : SurfaceFormat::A8R8G8B8_UINT32,
                                     true);
  if (!dstSurf) {
    return false;
  }
  sk_sp<SkCanvas> dstCanvas(
    SkCanvas::NewRasterDirect(
      SkImageInfo::Make(xformBounds.width, xformBounds.height,
                        GfxFormatToSkiaColorType(dstSurf->GetFormat()),
                        kPremul_SkAlphaType),
      dstSurf->GetData(), dstSurf->Stride()));
  if (!dstCanvas) {
    return false;
  }

  // Do the transform.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setFilterQuality(kLow_SkFilterQuality);
  paint.setBlendMode(SkBlendMode::kSrc);

  SkMatrix xform;
  GfxMatrixToSkiaMatrix(fullMat, xform);
  dstCanvas->setMatrix(xform);

  dstCanvas->drawImage(srcImage, 0, 0, &paint);
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

  sk_sp<SkImage> image = GetSkImageForSurface(aSurface);
  if (!image) {
    return true;
  }

  mCanvas->save();

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setFilterQuality(kLow_SkFilterQuality);

  SkMatrix xform;
  GfxMatrixToSkiaMatrix(aMatrix, xform);
  mCanvas->concat(xform);

  mCanvas->drawImage(image, 0, 0, &paint);

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
  // Check that our SkCanvas isn't backed by vector storage such as PDF.  If it
  // is then we want similar storage to avoid losing fidelity.
  MOZ_ASSERT(mCanvas->imageInfo().colorType() != kUnknown_SkColorType,
             "Not backed by pixels - we need to handle PDF backed SkCanvas");
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

#ifdef USE_SKIA_GPU
already_AddRefed<SourceSurface>
DrawTargetSkia::OptimizeGPUSourceSurface(SourceSurface *aSurface) const
{
  // Check if the underlying SkImage already has an associated GrTexture.
  sk_sp<SkImage> image = GetSkImageForSurface(aSurface);
  if (!image || image->isTextureBacked()) {
    RefPtr<SourceSurface> surface(aSurface);
    return surface.forget();
  }

  // Upload the SkImage to a GrTexture otherwise.
  sk_sp<SkImage> texture = image->makeTextureImage(mGrContext.get());
  if (texture) {
    // Create a new SourceSurfaceSkia whose SkImage contains the GrTexture.
    RefPtr<SourceSurfaceSkia> surface = new SourceSurfaceSkia();
    if (surface->InitFromImage(texture, aSurface->GetFormat())) {
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
  surface->InitFromImage(image);
  return surface.forget();
}
#endif

already_AddRefed<SourceSurface>
DrawTargetSkia::OptimizeSourceSurfaceForUnknownAlpha(SourceSurface *aSurface) const
{
#ifdef USE_SKIA_GPU
  if (UsingSkiaGPU()) {
    return OptimizeGPUSourceSurface(aSurface);
  }
#endif

  if (aSurface->GetType() == SurfaceType::SKIA) {
    RefPtr<SourceSurface> surface(aSurface);
    return surface.forget();
  }

  RefPtr<DataSourceSurface> dataSurface = aSurface->GetDataSurface();

  // For plugins, GDI can sometimes just write 0 to the alpha channel
  // even for RGBX formats. In this case, we have to manually write
  // the alpha channel to make Skia happy with RGBX and in case GDI
  // writes some bad data. Luckily, this only happens on plugins.
  WriteRGBXFormat(dataSurface->GetData(), dataSurface->GetSize(),
                  dataSurface->Stride(), dataSurface->GetFormat());
  return dataSurface.forget();
}

already_AddRefed<SourceSurface>
DrawTargetSkia::OptimizeSourceSurface(SourceSurface *aSurface) const
{
#ifdef USE_SKIA_GPU
  if (UsingSkiaGPU()) {
    return OptimizeGPUSourceSurface(aSurface);
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
  RefPtr<DataSourceSurface> dataSurface = aSurface->GetDataSurface();
  MOZ_ASSERT(VerifyRGBXFormat(dataSurface->GetData(), dataSurface->GetSize(),
                              dataSurface->Stride(), dataSurface->GetFormat()));
  return dataSurface.forget();
}

already_AddRefed<SourceSurface>
DrawTargetSkia::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
#ifdef USE_SKIA_GPU
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

    sk_sp<SkImage> texture =
      SkImage::MakeFromAdoptedTexture(mGrContext.get(), texDesc,
                                      GfxFormatToSkiaAlphaType(aSurface.mFormat));
    RefPtr<SourceSurfaceSkia> newSurf = new SourceSurfaceSkia();
    if (texture && newSurf->InitFromImage(texture, aSurface.mFormat)) {
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
  MarkChanged();

  sk_sp<SkImage> image = GetSkImageForSurface(aSurface);
  if (!image) {
    return;
  }

  mCanvas->save();
  mCanvas->setMatrix(SkMatrix::MakeTrans(SkIntToScalar(aDestination.x), SkIntToScalar(aDestination.y)));
  mCanvas->clipRect(SkRect::MakeIWH(aSourceRect.width, aSourceRect.height), kReplace_SkClipOp);

  SkPaint paint;
  if (!image->isOpaque()) {
    // Keep the xfermode as SOURCE_OVER for opaque bitmaps
    // http://code.google.com/p/skia/issues/detail?id=628
    paint.setBlendMode(SkBlendMode::kSrc);
  }
  // drawImage with A8 images ends up doing a mask operation
  // so we need to clear before
  if (SkImageIsMask(image)) {
    mCanvas->clear(SK_ColorTRANSPARENT);
  }
  mCanvas->drawImage(image, -SkIntToScalar(aSourceRect.x), -SkIntToScalar(aSourceRect.y), &paint);
  mCanvas->restore();
}

bool
DrawTargetSkia::Init(const IntSize &aSize, SurfaceFormat aFormat)
{
  if (size_t(std::max(aSize.width, aSize.height)) > GetMaxSurfaceSize()) {
    return false;
  }

  // we need to have surfaces that have a stride aligned to 4 for interop with cairo
  SkImageInfo info = MakeSkiaImageInfo(aSize, aFormat);
  size_t stride = SkAlign4(info.minRowBytes());
  mSurface = SkSurface::MakeRaster(info, stride, nullptr);
  if (!mSurface) {
    return false;
  }

  mSize = aSize;
  mFormat = aFormat;
  mCanvas = sk_ref_sp(mSurface->getCanvas());

  if (info.isOpaque()) {
    mCanvas->clear(SK_ColorBLACK);
  }
  return true;
}

bool
DrawTargetSkia::Init(SkCanvas* aCanvas)
{
  mCanvas = sk_ref_sp(aCanvas);

  SkImageInfo imageInfo = mCanvas->imageInfo();

  // If the canvas is backed by pixels we clear it to be on the safe side.  If
  // it's not (for example, for PDF output) we don't.
  bool isBackedByPixels = imageInfo.colorType() != kUnknown_SkColorType;
  if (isBackedByPixels) {
    // Note for PDF backed SkCanvas |alphaType == kUnknown_SkAlphaType|.
    SkColor clearColor = imageInfo.isOpaque() ? SK_ColorBLACK : SK_ColorTRANSPARENT;
    mCanvas->clear(clearColor);
  }

  SkISize size = mCanvas->getBaseLayerSize();
  mSize.width = size.width();
  mSize.height = size.height();
  mFormat = SkiaColorTypeToGfxFormat(imageInfo.colorType(),
                                     imageInfo.alphaType());
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
  mSurface =
    SkSurface::MakeRenderTarget(aGrContext,
                                SkBudgeted(aCached),
                                MakeSkiaImageInfo(aSize, aFormat));
  if (!mSurface) {
    return false;
  }

  mGrContext = sk_ref_sp(aGrContext);
  mSize = aSize;
  mFormat = aFormat;
  mCanvas = sk_ref_sp(mSurface->getCanvas());
  return true;
}

#endif

bool
DrawTargetSkia::Init(unsigned char* aData, const IntSize &aSize, int32_t aStride, SurfaceFormat aFormat, bool aUninitialized)
{
  MOZ_ASSERT((aFormat != SurfaceFormat::B8G8R8X8) ||
              aUninitialized || VerifyRGBXFormat(aData, aSize, aStride, aFormat));

  mSurface = SkSurface::MakeRasterDirect(MakeSkiaImageInfo(aSize, aFormat), aData, aStride);
  if (!mSurface) {
    return false;
  }

  mSize = aSize;
  mFormat = aFormat;
  mCanvas = sk_ref_sp(mSurface->getCanvas());
  return true;
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
    GrBackendObject handle = mSurface->getTextureHandle(SkSurface::kFlushRead_BackendHandleAccess);
    if (handle) {
      return (void*)(uintptr_t)reinterpret_cast<GrGLTextureInfo *>(handle)->fID;
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
  mCanvas->clipRect(RectToSkRect(aRect), kIntersect_SkClipOp, true);
  SkColor clearColor = (mFormat == SurfaceFormat::B8G8R8X8) ? SK_ColorBLACK : SK_ColorTRANSPARENT;
  mCanvas->clear(clearColor);
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
  mCanvas->clipPath(skiaPath->GetPath(), kIntersect_SkClipOp, true);
}

void
DrawTargetSkia::PushDeviceSpaceClipRects(const IntRect* aRects, uint32_t aCount)
{
  // Build a region by unioning all the rects together.
  SkRegion region;
  for (uint32_t i = 0; i < aCount; i++) {
    region.op(IntRectToSkIRect(aRects[i]), SkRegion::kUnion_Op);
  }

  // Clip with the resulting region. clipRegion does not transform
  // this region by the current transform, unlike the other SkCanvas
  // clip methods, so it is just passed through in device-space.
  mCanvas->save();
  mCanvas->clipRegion(region, kIntersect_SkClipOp);
}

void
DrawTargetSkia::PushClipRect(const Rect& aRect)
{
  SkRect rect = RectToSkRect(aRect);

  mCanvas->save();
  mCanvas->clipRect(rect, kIntersect_SkClipOp, true);
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
    : SkImageFilter(nullptr, 0, nullptr)
  {}

  virtual sk_sp<SkSpecialImage> onFilterImage(SkSpecialImage* source,
                                              const Context& ctx,
                                              SkIPoint* offset) const override {
    offset->set(0, 0);
    return sk_ref_sp(source);
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

  sk_sp<SkImageFilter> backdrop(aCopyBackground ? new CopyLayerImageFilter : nullptr);

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
    // If we have a mask, take a reference to the top layer's device so that
    // we can mask it ourselves. This assumes we forced SkCanvas::restore to
    // skip implicitly drawing the layer.
    sk_sp<SkBaseDevice> layerDevice = sk_ref_sp(mCanvas->getTopDevice());
    SkIRect layerBounds = layerDevice->getGlobalBounds();
    sk_sp<SkImage> layerImage;
    SkPixmap layerPixmap;
    if (layerDevice->peekPixels(&layerPixmap)) {
      layerImage = SkImage::MakeFromRaster(layerPixmap, nullptr, nullptr);
#ifdef USE_SKIA_GPU
    } else if (GrDrawContext* drawCtx = mCanvas->internal_private_accessTopLayerDrawContext()) {
      drawCtx->prepareForExternalIO();
      if (GrTexture* tex = drawCtx->accessRenderTarget()->asTexture()) {
        layerImage = sk_make_sp<SkImage_Gpu>(layerBounds.width(), layerBounds.height(),
                                             kNeedNewImageUniqueID,
                                             layerDevice->imageInfo().alphaType(),
                                             tex, nullptr, SkBudgeted::kNo);
      }
#endif
    }

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

      if (layerImage) {
        paint.setShader(layerImage->makeShader(SkShader::kClamp_TileMode, SkShader::kClamp_TileMode, &layerMat));
      } else {
        paint.setColor(SK_ColorTRANSPARENT);
      }

      sk_sp<SkImage> alphaMask = ExtractAlphaForSurface(layer.mMask);
      if (!alphaMask) {
        gfxDebug() << *this << ": PopLayer() failed to extract alpha for mask";
      } else {
        mCanvas->save();

        // The layer may be smaller than the canvas size, so make sure drawing is
        // clipped to within the bounds of the layer.
        mCanvas->resetMatrix();
        mCanvas->clipRect(SkRect::Make(layerBounds));

        mCanvas->setMatrix(maskMat);
        mCanvas->drawImage(alphaMask, 0, 0, &paint);

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

    // Handle copying of any image snapshots bound to the surface.
    mSurface->notifyContentWillChange(SkSurface::kRetain_ContentChangeMode);
  }
}

void
DrawTargetSkia::SnapshotDestroyed()
{
  mSnapshot = nullptr;
}

} // namespace gfx
} // namespace mozilla
