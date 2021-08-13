/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetSkia.h"
#include "SourceSurfaceSkia.h"
#include "ScaledFontBase.h"
#include "FilterNodeSoftware.h"
#include "HelpersSkia.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Vector.h"

#include "skia/include/core/SkFont.h"
#include "skia/include/core/SkTextBlob.h"
#include "skia/include/core/SkSurface.h"
#include "skia/include/core/SkTypeface.h"
#include "skia/include/effects/SkGradientShader.h"
#include "skia/include/core/SkColorFilter.h"
#include "skia/include/core/SkRegion.h"
#include "skia/include/effects/SkBlurImageFilter.h"
#include "Blur.h"
#include "Logging.h"
#include "Tools.h"
#include "DataSurfaceHelpers.h"
#include "PathHelpers.h"
#include "SourceSurfaceCapture.h"
#include "Swizzle.h"
#include <algorithm>

#ifdef MOZ_WIDGET_COCOA
#  include "BorrowedContext.h"
#  include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef XP_WIN
#  include "ScaledFontDWrite.h"
#endif

namespace mozilla::gfx {

class GradientStopsSkia : public GradientStops {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GradientStopsSkia, override)

  GradientStopsSkia(const std::vector<GradientStop>& aStops, uint32_t aNumStops,
                    ExtendMode aExtendMode)
      : mCount(aNumStops), mExtendMode(aExtendMode) {
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
    if (aStops[aNumStops - 1].offset != 1) {
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
    if (aStops[aNumStops - 1].offset != 1) {
      mColors[mCount - 1] = ColorToSkColor(aStops[aNumStops - 1].color, 1.0);
      mPositions[mCount - 1] = SK_Scalar1;
    }
  }

  BackendType GetBackendType() const override { return BackendType::SKIA; }

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
static void ReleaseTemporarySurface(const void* aPixels, void* aContext) {
  DataSourceSurface* surf = static_cast<DataSourceSurface*>(aContext);
  if (surf) {
    surf->Release();
  }
}

static void ReleaseTemporaryMappedSurface(const void* aPixels, void* aContext) {
  DataSourceSurface* surf = static_cast<DataSourceSurface*>(aContext);
  if (surf) {
    surf->Unmap();
    surf->Release();
  }
}

static void WriteRGBXFormat(uint8_t* aData, const IntSize& aSize,
                            const int32_t aStride, SurfaceFormat aFormat) {
  if (aFormat != SurfaceFormat::B8G8R8X8 || aSize.IsEmpty()) {
    return;
  }

  SwizzleData(aData, aStride, SurfaceFormat::X8R8G8B8_UINT32, aData, aStride,
              SurfaceFormat::A8R8G8B8_UINT32, aSize);
}

#ifdef DEBUG
static IntRect CalculateSurfaceBounds(const IntSize& aSize, const Rect* aBounds,
                                      const Matrix* aMatrix) {
  IntRect surfaceBounds(IntPoint(0, 0), aSize);
  if (!aBounds) {
    return surfaceBounds;
  }

  MOZ_ASSERT(aMatrix);
  Matrix inverse(*aMatrix);
  if (!inverse.Invert()) {
    return surfaceBounds;
  }

  IntRect bounds;
  Rect sampledBounds = inverse.TransformBounds(*aBounds);
  if (!sampledBounds.ToIntRect(&bounds)) {
    return surfaceBounds;
  }

  return surfaceBounds.Intersect(bounds);
}

static const int kARGBAlphaOffset =
    SurfaceFormat::A8R8G8B8_UINT32 == SurfaceFormat::B8G8R8A8 ? 3 : 0;

static bool VerifyRGBXFormat(uint8_t* aData, const IntSize& aSize,
                             const int32_t aStride, SurfaceFormat aFormat) {
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
        gfxCriticalError() << "RGBX pixel at (" << column << "," << row
                           << ") in " << width << "x" << height
                           << " surface is not opaque: " << int(aData[column])
                           << "," << int(aData[column + 1]) << ","
                           << int(aData[column + 2]) << ","
                           << int(aData[column + 3]);
      }
    }
    aData += aStride;
  }

  return true;
}

// Since checking every pixel is expensive, this only checks the four corners
// and center of a surface that their alpha value is 0xFF.
static bool VerifyRGBXCorners(uint8_t* aData, const IntSize& aSize,
                              const int32_t aStride, SurfaceFormat aFormat,
                              const Rect* aBounds = nullptr,
                              const Matrix* aMatrix = nullptr) {
  if (aFormat != SurfaceFormat::B8G8R8X8 || aSize.IsEmpty()) {
    return true;
  }

  IntRect bounds = CalculateSurfaceBounds(aSize, aBounds, aMatrix);
  if (bounds.IsEmpty()) {
    return true;
  }

  const int height = bounds.Height();
  const int width = bounds.Width();
  const int pixelSize = 4;
  MOZ_ASSERT(aSize.width * pixelSize <= aStride);

  const int translation = bounds.Y() * aStride + bounds.X() * pixelSize;
  const int topLeft = translation;
  const int topRight = topLeft + (width - 1) * pixelSize;
  const int bottomLeft = translation + (height - 1) * aStride;
  const int bottomRight = bottomLeft + (width - 1) * pixelSize;

  // Lastly the center pixel
  const int middleRowHeight = height / 2;
  const int middleRowWidth = (width / 2) * pixelSize;
  const int middle = translation + aStride * middleRowHeight + middleRowWidth;

  const int offsets[] = {topLeft, topRight, bottomRight, bottomLeft, middle};
  for (int offset : offsets) {
    if (aData[offset + kARGBAlphaOffset] != 0xFF) {
      int row = offset / aStride;
      int column = (offset % aStride) / pixelSize;
      gfxCriticalError() << "RGBX corner pixel at (" << column << "," << row
                         << ") in " << aSize.width << "x" << aSize.height
                         << " surface, bounded by "
                         << "(" << bounds.X() << "," << bounds.Y() << ","
                         << width << "," << height
                         << ") is not opaque: " << int(aData[offset]) << ","
                         << int(aData[offset + 1]) << ","
                         << int(aData[offset + 2]) << ","
                         << int(aData[offset + 3]);
    }
  }

  return true;
}
#endif

static sk_sp<SkImage> GetSkImageForSurface(SourceSurface* aSurface,
                                           Maybe<MutexAutoLock>* aLock,
                                           const Rect* aBounds = nullptr,
                                           const Matrix* aMatrix = nullptr) {
  if (!aSurface) {
    gfxDebug() << "Creating null Skia image from null SourceSurface";
    return nullptr;
  }

  if (aSurface->GetType() == SurfaceType::CAPTURE) {
    SourceSurfaceCapture* capture =
        static_cast<SourceSurfaceCapture*>(aSurface);
    RefPtr<SourceSurface> resolved = capture->Resolve(BackendType::SKIA);
    if (!resolved) {
      return nullptr;
    }
    MOZ_ASSERT(resolved->GetType() != SurfaceType::CAPTURE);
    return GetSkImageForSurface(resolved, aLock, aBounds, aMatrix);
  }

  if (aSurface->GetType() == SurfaceType::SKIA) {
    return static_cast<SourceSurfaceSkia*>(aSurface)->GetImage(aLock);
  }

  RefPtr<DataSourceSurface> dataSurface = aSurface->GetDataSurface();
  if (!dataSurface) {
    gfxWarning() << "Failed getting DataSourceSurface for Skia image";
    return nullptr;
  }

  DataSourceSurface::MappedSurface map;
  SkImage::RasterReleaseProc releaseProc;
  if (dataSurface->GetType() == SurfaceType::DATA_SHARED_WRAPPER) {
    // Technically all surfaces should be mapped and unmapped explicitly but it
    // appears SourceSurfaceSkia and DataSourceSurfaceWrapper have issues with
    // this. For now, we just map SourceSurfaceSharedDataWrapper to ensure we
    // don't unmap the data during the transaction (for blob images).
    if (!dataSurface->Map(DataSourceSurface::MapType::READ, &map)) {
      gfxWarning() << "Failed mapping DataSourceSurface for Skia image";
      return nullptr;
    }
    releaseProc = ReleaseTemporaryMappedSurface;
  } else {
    map.mData = dataSurface->GetData();
    map.mStride = dataSurface->Stride();
    releaseProc = ReleaseTemporarySurface;
  }

  DataSourceSurface* surf = dataSurface.forget().take();

  // Skia doesn't support RGBX surfaces so ensure that the alpha value is opaque
  // white.
  MOZ_ASSERT(VerifyRGBXCorners(map.mData, surf->GetSize(), map.mStride,
                               surf->GetFormat(), aBounds, aMatrix));

  SkPixmap pixmap(MakeSkiaImageInfo(surf->GetSize(), surf->GetFormat()),
                  map.mData, map.mStride);
  sk_sp<SkImage> image = SkImage::MakeFromRaster(pixmap, releaseProc, surf);
  if (!image) {
    ReleaseTemporarySurface(nullptr, surf);
    gfxDebug() << "Failed making Skia raster image for temporary surface";
  }

  return image;
}

DrawTargetSkia::DrawTargetSkia()
    : mCanvas(nullptr), mSnapshot(nullptr), mSnapshotLock {
  "DrawTargetSkia::mSnapshotLock"
}
#ifdef MOZ_WIDGET_COCOA
, mCG(nullptr), mColorSpace(nullptr), mCanvasData(nullptr), mCGSize(0, 0),
    mNeedLayer(false)
#endif
{
}

DrawTargetSkia::~DrawTargetSkia() {
  if (mSnapshot) {
    MutexAutoLock lock(mSnapshotLock);
    // We're going to go away, hand our SkSurface to the SourceSurface.
    mSnapshot->GiveSurface(mSurface);
  }

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

already_AddRefed<SourceSurface> DrawTargetSkia::Snapshot() {
  // Without this lock, this could cause us to get out a snapshot and race with
  // Snapshot::~Snapshot() actually destroying itself.
  MutexAutoLock lock(mSnapshotLock);
  RefPtr<SourceSurfaceSkia> snapshot = mSnapshot;
  if (mSurface && !snapshot) {
    snapshot = new SourceSurfaceSkia();
    sk_sp<SkImage> image;
    // If the surface is raster, making a snapshot may trigger a pixel copy.
    // Instead, try to directly make a raster image referencing the surface
    // pixels.
    SkPixmap pixmap;
    if (mSurface->peekPixels(&pixmap)) {
      image = SkImage::MakeFromRaster(pixmap, nullptr, nullptr);
    } else {
      image = mSurface->makeImageSnapshot();
    }
    if (!snapshot->InitFromImage(image, mFormat, this)) {
      return nullptr;
    }
    mSnapshot = snapshot;
  }

  return snapshot.forget();
}

already_AddRefed<SourceSurface> DrawTargetSkia::GetBackingSurface() {
  if (mBackingSurface) {
    RefPtr<SourceSurface> snapshot = mBackingSurface;
    return snapshot.forget();
  }
  return Snapshot();
}

bool DrawTargetSkia::LockBits(uint8_t** aData, IntSize* aSize, int32_t* aStride,
                              SurfaceFormat* aFormat, IntPoint* aOrigin) {
  SkImageInfo info;
  size_t rowBytes;
  SkIPoint origin;
  void* pixels = mCanvas->accessTopLayerPixels(&info, &rowBytes, &origin);
  if (!pixels ||
      // Ensure the layer is at the origin if required.
      (!aOrigin && !origin.isZero())) {
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

void DrawTargetSkia::ReleaseBits(uint8_t* aData) {}

static void ReleaseImage(const void* aPixels, void* aContext) {
  SkImage* image = static_cast<SkImage*>(aContext);
  SkSafeUnref(image);
}

static sk_sp<SkImage> ExtractSubset(sk_sp<SkImage> aImage,
                                    const IntRect& aRect) {
  SkIRect subsetRect = IntRectToSkIRect(aRect);
  if (aImage->bounds() == subsetRect) {
    return aImage;
  }
  // makeSubset is slow, so prefer to use SkPixmap::extractSubset where
  // possible.
  SkPixmap pixmap, subsetPixmap;
  if (aImage->peekPixels(&pixmap) &&
      pixmap.extractSubset(&subsetPixmap, subsetRect)) {
    // Release the original image reference so only the subset image keeps it
    // alive.
    return SkImage::MakeFromRaster(subsetPixmap, ReleaseImage,
                                   aImage.release());
  }
  return aImage->makeSubset(subsetRect);
}

static void FreeBitmapPixels(void* aBuf, void*) { sk_free(aBuf); }

static bool ExtractAlphaBitmap(const sk_sp<SkImage>& aImage,
                               SkBitmap* aResultBitmap) {
  SkImageInfo info = SkImageInfo::MakeA8(aImage->width(), aImage->height());
  // Skia does not fully allocate the last row according to stride.
  // Since some of our algorithms (i.e. blur) depend on this, we must allocate
  // the bitmap pixels manually.
  size_t stride = SkAlign4(info.minRowBytes());
  CheckedInt<size_t> size = stride;
  size *= info.height();
  if (size.isValid()) {
    void* buf = sk_malloc_flags(size.value(), 0);
    if (buf) {
      SkBitmap bitmap;
      if (bitmap.installPixels(info, buf, stride, FreeBitmapPixels, nullptr) &&
          aImage->readPixels(bitmap.info(), bitmap.getPixels(),
                             bitmap.rowBytes(), 0, 0)) {
        *aResultBitmap = bitmap;
        return true;
      }
    }
  }

  gfxWarning() << "Failed reading alpha pixels for Skia bitmap";
  return false;
}

static sk_sp<SkImage> ExtractAlphaForSurface(SourceSurface* aSurface,
                                             Maybe<MutexAutoLock>& aLock) {
  sk_sp<SkImage> image = GetSkImageForSurface(aSurface, &aLock);
  if (!image) {
    return nullptr;
  }
  if (image->isAlphaOnly()) {
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

static void SetPaintPattern(SkPaint& aPaint, const Pattern& aPattern,
                            Maybe<MutexAutoLock>& aLock, Float aAlpha = 1.0,
                            const SkMatrix* aMatrix = nullptr,
                            const Rect* aBounds = nullptr) {
  switch (aPattern.GetType()) {
    case PatternType::COLOR: {
      DeviceColor color = static_cast<const ColorPattern&>(aPattern).mColor;
      aPaint.setColor(ColorToSkColor(color, aAlpha));
      break;
    }
    case PatternType::LINEAR_GRADIENT: {
      const LinearGradientPattern& pat =
          static_cast<const LinearGradientPattern&>(aPattern);
      GradientStopsSkia* stops =
          static_cast<GradientStopsSkia*>(pat.mStops.get());
      if (!stops || stops->mCount < 2 || !pat.mBegin.IsFinite() ||
          !pat.mEnd.IsFinite() || pat.mBegin == pat.mEnd) {
        aPaint.setColor(SK_ColorTRANSPARENT);
      } else {
        SkTileMode mode = ExtendModeToTileMode(stops->mExtendMode, Axis::BOTH);
        SkPoint points[2];
        points[0] = SkPoint::Make(SkFloatToScalar(pat.mBegin.x),
                                  SkFloatToScalar(pat.mBegin.y));
        points[1] = SkPoint::Make(SkFloatToScalar(pat.mEnd.x),
                                  SkFloatToScalar(pat.mEnd.y));

        SkMatrix mat;
        GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
        if (aMatrix) {
          mat.postConcat(*aMatrix);
        }
        sk_sp<SkShader> shader = SkGradientShader::MakeLinear(
            points, &stops->mColors.front(), &stops->mPositions.front(),
            stops->mCount, mode, 0, &mat);
        if (shader) {
          aPaint.setShader(shader);
        } else {
          aPaint.setColor(SK_ColorTRANSPARENT);
        }
      }
      break;
    }
    case PatternType::RADIAL_GRADIENT: {
      const RadialGradientPattern& pat =
          static_cast<const RadialGradientPattern&>(aPattern);
      GradientStopsSkia* stops =
          static_cast<GradientStopsSkia*>(pat.mStops.get());
      if (!stops || stops->mCount < 2 || !pat.mCenter1.IsFinite() ||
          !IsFinite(pat.mRadius1) || !pat.mCenter2.IsFinite() ||
          !IsFinite(pat.mRadius2) ||
          (pat.mCenter1 == pat.mCenter2 && pat.mRadius1 == pat.mRadius2)) {
        aPaint.setColor(SK_ColorTRANSPARENT);
      } else {
        SkTileMode mode = ExtendModeToTileMode(stops->mExtendMode, Axis::BOTH);
        SkPoint points[2];
        points[0] = SkPoint::Make(SkFloatToScalar(pat.mCenter1.x),
                                  SkFloatToScalar(pat.mCenter1.y));
        points[1] = SkPoint::Make(SkFloatToScalar(pat.mCenter2.x),
                                  SkFloatToScalar(pat.mCenter2.y));

        SkMatrix mat;
        GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
        if (aMatrix) {
          mat.postConcat(*aMatrix);
        }
        sk_sp<SkShader> shader = SkGradientShader::MakeTwoPointConical(
            points[0], SkFloatToScalar(pat.mRadius1), points[1],
            SkFloatToScalar(pat.mRadius2), &stops->mColors.front(),
            &stops->mPositions.front(), stops->mCount, mode, 0, &mat);
        if (shader) {
          aPaint.setShader(shader);
        } else {
          aPaint.setColor(SK_ColorTRANSPARENT);
        }
      }
      break;
    }
    case PatternType::CONIC_GRADIENT: {
      const ConicGradientPattern& pat =
          static_cast<const ConicGradientPattern&>(aPattern);
      GradientStopsSkia* stops =
          static_cast<GradientStopsSkia*>(pat.mStops.get());
      if (!stops || stops->mCount < 2 || !pat.mCenter.IsFinite() ||
          !IsFinite(pat.mAngle)) {
        aPaint.setColor(SK_ColorTRANSPARENT);
      } else {
        SkMatrix mat;
        GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
        if (aMatrix) {
          mat.postConcat(*aMatrix);
        }

        SkScalar cx = SkFloatToScalar(pat.mCenter.x);
        SkScalar cy = SkFloatToScalar(pat.mCenter.y);

        // Skia's sweep gradient angles are relative to the x-axis, not the
        // y-axis.
        Float angle = (pat.mAngle * 180.0 / M_PI) - 90.0;
        if (angle != 0.0) {
          mat.preRotate(angle, cx, cy);
        }

        SkTileMode mode = ExtendModeToTileMode(stops->mExtendMode, Axis::BOTH);
        sk_sp<SkShader> shader = SkGradientShader::MakeSweep(
            cx, cy, &stops->mColors.front(), &stops->mPositions.front(),
            stops->mCount, mode, 360 * pat.mStartOffset, 360 * pat.mEndOffset,
            0, &mat);

        if (shader) {
          aPaint.setShader(shader);
        } else {
          aPaint.setColor(SK_ColorTRANSPARENT);
        }
      }
      break;
    }
    case PatternType::SURFACE: {
      const SurfacePattern& pat = static_cast<const SurfacePattern&>(aPattern);
      sk_sp<SkImage> image =
          GetSkImageForSurface(pat.mSurface, &aLock, aBounds, &pat.mMatrix);
      if (!image) {
        aPaint.setColor(SK_ColorTRANSPARENT);
        break;
      }

      SkMatrix mat;
      GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
      if (aMatrix) {
        mat.postConcat(*aMatrix);
      }

      if (!pat.mSamplingRect.IsEmpty()) {
        image = ExtractSubset(image, pat.mSamplingRect);
        mat.preTranslate(pat.mSamplingRect.X(), pat.mSamplingRect.Y());
      }

      SkTileMode xTile = ExtendModeToTileMode(pat.mExtendMode, Axis::X_AXIS);
      SkTileMode yTile = ExtendModeToTileMode(pat.mExtendMode, Axis::Y_AXIS);

      aPaint.setShader(image->makeShader(xTile, yTile, &mat));

      if (pat.mSamplingFilter == SamplingFilter::POINT) {
        aPaint.setFilterQuality(kNone_SkFilterQuality);
      }
      break;
    }
  }
}

static inline Rect GetClipBounds(SkCanvas* aCanvas) {
  // Use a manually transformed getClipDeviceBounds instead of
  // getClipBounds because getClipBounds inflates the the bounds
  // by a pixel in each direction to compensate for antialiasing.
  SkIRect deviceBounds;
  if (!aCanvas->getDeviceClipBounds(&deviceBounds)) {
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
  AutoPaintSetup(SkCanvas* aCanvas, const DrawOptions& aOptions,
                 const Pattern& aPattern, const Rect* aMaskBounds = nullptr,
                 const SkMatrix* aMatrix = nullptr,
                 const Rect* aSourceBounds = nullptr)
      : mNeedsRestore(false), mAlpha(1.0) {
    Init(aCanvas, aOptions, aMaskBounds, false);
    SetPaintPattern(mPaint, aPattern, mLock, mAlpha, aMatrix, aSourceBounds);
  }

  AutoPaintSetup(SkCanvas* aCanvas, const DrawOptions& aOptions,
                 const Rect* aMaskBounds = nullptr, bool aForceGroup = false)
      : mNeedsRestore(false), mAlpha(1.0) {
    Init(aCanvas, aOptions, aMaskBounds, aForceGroup);
  }

  ~AutoPaintSetup() {
    if (mNeedsRestore) {
      mCanvas->restore();
    }
  }

  void Init(SkCanvas* aCanvas, const DrawOptions& aOptions,
            const Rect* aMaskBounds, bool aForceGroup) {
    mPaint.setBlendMode(GfxOpToSkiaOp(aOptions.mCompositionOp));
    mCanvas = aCanvas;

    // TODO: Can we set greyscale somehow?
    if (aOptions.mAntialiasMode != AntialiasMode::NONE) {
      mPaint.setAntiAlias(true);
    } else {
      mPaint.setAntiAlias(false);
    }

    bool needsGroup =
        aForceGroup ||
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
      // TODO: Get a rect here
      SkCanvas::SaveLayerRec rec(nullptr, &temp,
                                 SkCanvas::kPreserveLCDText_SaveLayerFlag);
      mCanvas->saveLayer(rec);
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
  Maybe<MutexAutoLock> mLock;
  Float mAlpha;
};

void DrawTargetSkia::Flush() { mCanvas->flush(); }

void DrawTargetSkia::DrawSurface(SourceSurface* aSurface, const Rect& aDest,
                                 const Rect& aSource,
                                 const DrawSurfaceOptions& aSurfOptions,
                                 const DrawOptions& aOptions) {
  if (aSource.IsEmpty()) {
    return;
  }

  MarkChanged();

  Maybe<MutexAutoLock> lock;
  sk_sp<SkImage> image = GetSkImageForSurface(aSurface, &lock);
  if (!image) {
    return;
  }

  SkRect destRect = RectToSkRect(aDest);
  SkRect sourceRect = RectToSkRect(aSource - aSurface->GetRect().TopLeft());
  bool forceGroup =
      image->isAlphaOnly() && aOptions.mCompositionOp != CompositionOp::OP_OVER;

  AutoPaintSetup paint(mCanvas, aOptions, &aDest, forceGroup);
  if (aSurfOptions.mSamplingFilter == SamplingFilter::POINT) {
    paint.mPaint.setFilterQuality(kNone_SkFilterQuality);
  }

  mCanvas->drawImageRect(image, sourceRect, destRect, &paint.mPaint);
}

DrawTargetType DrawTargetSkia::GetType() const {
  return DrawTargetType::SOFTWARE_RASTER;
}

void DrawTargetSkia::DrawFilter(FilterNode* aNode, const Rect& aSourceRect,
                                const Point& aDestPoint,
                                const DrawOptions& aOptions) {
  FilterNodeSoftware* filter = static_cast<FilterNodeSoftware*>(aNode);
  filter->Draw(this, aSourceRect, aDestPoint, aOptions);
}

void DrawTargetSkia::DrawSurfaceWithShadow(SourceSurface* aSurface,
                                           const Point& aDest,
                                           const DeviceColor& aColor,
                                           const Point& aOffset, Float aSigma,
                                           CompositionOp aOperator) {
  if (aSurface->GetSize().IsEmpty()) {
    return;
  }

  MarkChanged();

  Maybe<MutexAutoLock> lock;
  sk_sp<SkImage> image = GetSkImageForSurface(aSurface, &lock);
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
  if (ExtractAlphaBitmap(image, &blurMask)) {
    // Prefer using our own box blur instead of Skia's. It currently performs
    // much better than SkBlurImageFilter or SkBlurMaskFilter on the CPU.
    AlphaBoxBlur blur(Rect(0, 0, blurMask.width(), blurMask.height()),
                      int32_t(blurMask.rowBytes()), aSigma, aSigma);
    blur.Blur(reinterpret_cast<uint8_t*>(blurMask.getPixels()));
    blurMask.notifyPixelsChanged();

    shadowPaint.setColor(ColorToSkColor(aColor, 1.0f));

    mCanvas->drawBitmap(blurMask, shadowDest.x, shadowDest.y, &shadowPaint);
  } else {
    sk_sp<SkImageFilter> blurFilter(
        SkBlurImageFilter::Make(aSigma, aSigma, nullptr));
    sk_sp<SkColorFilter> colorFilter(SkColorFilters::Blend(
        ColorToSkColor(aColor, 1.0f), SkBlendMode::kSrcIn));

    shadowPaint.setImageFilter(blurFilter);
    shadowPaint.setColorFilter(colorFilter);

    mCanvas->drawImage(image, shadowDest.x, shadowDest.y, &shadowPaint);
  }

  if (aSurface->GetFormat() != SurfaceFormat::A8) {
    // Composite the original image after the shadow
    auto dest = IntPoint::Round(aDest);
    mCanvas->drawImage(image, dest.x, dest.y, &paint);
  }

  mCanvas->restore();
}

void DrawTargetSkia::FillRect(const Rect& aRect, const Pattern& aPattern,
                              const DrawOptions& aOptions) {
  // The sprite blitting path in Skia can be faster than the shader blitter for
  // operators other than source (or source-over with opaque surface). So, when
  // possible/beneficial, route to DrawSurface which will use the sprite
  // blitter.
  if (aPattern.GetType() == PatternType::SURFACE &&
      aOptions.mCompositionOp != CompositionOp::OP_SOURCE) {
    const SurfacePattern& pat = static_cast<const SurfacePattern&>(aPattern);
    // Verify there is a valid surface and a pattern matrix without skew.
    if (pat.mSurface &&
        (aOptions.mCompositionOp != CompositionOp::OP_OVER ||
         GfxFormatToSkiaAlphaType(pat.mSurface->GetFormat()) !=
             kOpaque_SkAlphaType) &&
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
                    DrawSurfaceOptions(pat.mSamplingFilter), aOptions);
        return;
      }
    }
  }

  MarkChanged();
  SkRect rect = RectToSkRect(aRect);
  AutoPaintSetup paint(mCanvas, aOptions, aPattern, &aRect, nullptr, &aRect);

  mCanvas->drawRect(rect, paint.mPaint);
}

void DrawTargetSkia::Stroke(const Path* aPath, const Pattern& aPattern,
                            const StrokeOptions& aStrokeOptions,
                            const DrawOptions& aOptions) {
  MarkChanged();
  MOZ_ASSERT(aPath, "Null path");
  if (aPath->GetBackendType() != BackendType::SKIA) {
    return;
  }

  const PathSkia* skiaPath = static_cast<const PathSkia*>(aPath);

  AutoPaintSetup paint(mCanvas, aOptions, aPattern);
  if (!StrokeOptionsToPaint(paint.mPaint, aStrokeOptions)) {
    return;
  }

  if (!skiaPath->GetPath().isFinite()) {
    return;
  }

  mCanvas->drawPath(skiaPath->GetPath(), paint.mPaint);
}

static Double DashPeriodLength(const StrokeOptions& aStrokeOptions) {
  Double length = 0;
  for (size_t i = 0; i < aStrokeOptions.mDashLength; i++) {
    length += aStrokeOptions.mDashPattern[i];
  }
  if (aStrokeOptions.mDashLength & 1) {
    // "If an odd number of values is provided, then the list of values is
    // repeated to yield an even number of values."
    // Double the length.
    length += length;
  }
  return length;
}

static inline Double RoundDownToMultiple(Double aValue, Double aFactor) {
  return floor(aValue / aFactor) * aFactor;
}

static Rect UserSpaceStrokeClip(const IntRect& aDeviceClip,
                                const Matrix& aTransform,
                                const StrokeOptions& aStrokeOptions) {
  Matrix inverse = aTransform;
  if (!inverse.Invert()) {
    return Rect();
  }
  Rect deviceClip(aDeviceClip);
  deviceClip.Inflate(MaxStrokeExtents(aStrokeOptions, aTransform));
  return inverse.TransformBounds(deviceClip);
}

static Rect ShrinkClippedStrokedRect(const Rect& aStrokedRect,
                                     const IntRect& aDeviceClip,
                                     const Matrix& aTransform,
                                     const StrokeOptions& aStrokeOptions) {
  Rect userSpaceStrokeClip =
      UserSpaceStrokeClip(aDeviceClip, aTransform, aStrokeOptions);
  RectDouble strokedRectDouble(aStrokedRect.X(), aStrokedRect.Y(),
                               aStrokedRect.Width(), aStrokedRect.Height());
  RectDouble intersection = strokedRectDouble.Intersect(
      RectDouble(userSpaceStrokeClip.X(), userSpaceStrokeClip.Y(),
                 userSpaceStrokeClip.Width(), userSpaceStrokeClip.Height()));
  Double dashPeriodLength = DashPeriodLength(aStrokeOptions);
  if (intersection.IsEmpty() || dashPeriodLength == 0.0f) {
    return Rect(intersection.X(), intersection.Y(), intersection.Width(),
                intersection.Height());
  }

  // Reduce the rectangle side lengths in multiples of the dash period length
  // so that the visible dashes stay in the same place.
  MarginDouble insetBy = strokedRectDouble - intersection;
  insetBy.top = RoundDownToMultiple(insetBy.top, dashPeriodLength);
  insetBy.right = RoundDownToMultiple(insetBy.right, dashPeriodLength);
  insetBy.bottom = RoundDownToMultiple(insetBy.bottom, dashPeriodLength);
  insetBy.left = RoundDownToMultiple(insetBy.left, dashPeriodLength);

  strokedRectDouble.Deflate(insetBy);
  return Rect(strokedRectDouble.X(), strokedRectDouble.Y(),
              strokedRectDouble.Width(), strokedRectDouble.Height());
}

void DrawTargetSkia::StrokeRect(const Rect& aRect, const Pattern& aPattern,
                                const StrokeOptions& aStrokeOptions,
                                const DrawOptions& aOptions) {
  // Stroking large rectangles with dashes is expensive with Skia (fixed
  // overhead based on the number of dashes, regardless of whether the dashes
  // are visible), so we try to reduce the size of the stroked rectangle as
  // much as possible before passing it on to Skia.
  Rect rect = aRect;
  if (aStrokeOptions.mDashLength > 0 && !rect.IsEmpty()) {
    IntRect deviceClip(IntPoint(0, 0), mSize);
    SkIRect clipBounds;
    if (mCanvas->getDeviceClipBounds(&clipBounds)) {
      deviceClip = SkIRectToIntRect(clipBounds);
    }
    rect =
        ShrinkClippedStrokedRect(rect, deviceClip, mTransform, aStrokeOptions);
    if (rect.IsEmpty()) {
      return;
    }
  }

  MarkChanged();
  AutoPaintSetup paint(mCanvas, aOptions, aPattern);
  if (!StrokeOptionsToPaint(paint.mPaint, aStrokeOptions)) {
    return;
  }

  mCanvas->drawRect(RectToSkRect(rect), paint.mPaint);
}

void DrawTargetSkia::StrokeLine(const Point& aStart, const Point& aEnd,
                                const Pattern& aPattern,
                                const StrokeOptions& aStrokeOptions,
                                const DrawOptions& aOptions) {
  MarkChanged();
  AutoPaintSetup paint(mCanvas, aOptions, aPattern);
  if (!StrokeOptionsToPaint(paint.mPaint, aStrokeOptions)) {
    return;
  }

  mCanvas->drawLine(SkFloatToScalar(aStart.x), SkFloatToScalar(aStart.y),
                    SkFloatToScalar(aEnd.x), SkFloatToScalar(aEnd.y),
                    paint.mPaint);
}

void DrawTargetSkia::Fill(const Path* aPath, const Pattern& aPattern,
                          const DrawOptions& aOptions) {
  MarkChanged();
  if (!aPath || aPath->GetBackendType() != BackendType::SKIA) {
    return;
  }

  const PathSkia* skiaPath = static_cast<const PathSkia*>(aPath);

  AutoPaintSetup paint(mCanvas, aOptions, aPattern);

  if (!skiaPath->GetPath().isFinite()) {
    return;
  }

  mCanvas->drawPath(skiaPath->GetPath(), paint.mPaint);
}

#ifdef MOZ_WIDGET_COCOA
static inline CGAffineTransform GfxMatrixToCGAffineTransform(const Matrix& m) {
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
 * Transforms are better understood thinking about them from right to left order
 * (mathematically).
 *
 * Consider a point we want to draw at (0, 10) in normal cartesian planes with
 * a box of (100, 100). in CG terms, this would be at (0, 10).
 * Positive Y values point up.
 * In our DrawTarget terms, positive Y values point down, so (0, 10) would be
 * at (0, 90) in cartesian plane terms. That means our point at (0, 10) in
 * DrawTarget terms should end up at (0, 90). How does this work with the
 * current transforms?
 *
 * Going right to left with the transforms, a CGPoint of (0, 10) has cartesian
 * coordinates of (0, 10). The first flip of the Y axis puts the point now at
 * (0, -10); Next, we translate the context up by the size of the canvas
 * (Positive Y values go up in CG coordinates but down in our draw target
 * coordinates). Since our canvas size is (100, 100), the resulting coordinate
 * becomes (0, 90), which is what we expect from our DrawTarget code. These two
 * transforms put the CG context equal to what every other DrawTarget expects.
 *
 * Next, we need two more transforms for actual text. IF we left the transforms
 * as is, the text would be drawn upside down, so we need another flip of the Y
 * axis to draw the text right side up. However, with only the flip, the text
 * would be drawn in the wrong place. Thus we also have to invert the Y position
 * of the glyphs to get them in the right place.
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
 * Consider the example letter P, drawn at (0, 20) in CG coordinates in a
 * (100, 100) rect.
 * Again, going right to left of the transforms. We'd get:
 *
 * 1) The letter P drawn at (0, -20) due to the inversion of the Y axis
 * 2) The letter P upside down (b) at (0, 20) due to the second flip
 * 3) The letter P right side up at (0, -20) due to the first flip
 * 4) The letter P right side up at (0, 80) due to the translation
 *
 * tl;dr - CGRects assume origin is bottom left, DrawTarget rects assume top
 * left.
 */
static bool SetupCGContext(DrawTargetSkia* aDT, CGContextRef aCGContext,
                           SkCanvas* aCanvas, const IntPoint& aOrigin,
                           const IntSize& aSize, bool aClipped) {
  // DrawTarget expects the origin to be at the top left, but CG
  // expects it to be at the bottom left. Transform to set the origin to
  // the top left. Have to set this before we do anything else.
  // This is transform (1) up top
  CGContextTranslateCTM(aCGContext, -aOrigin.x, aOrigin.y + aSize.height);

  // Transform (2) from the comments.
  CGContextScaleCTM(aCGContext, 1, -1);

  // Want to apply clips BEFORE the transform since the transform
  // will apply to the clips we apply.
  if (aClipped) {
    SkRegion clipRegion;
    aCanvas->temporary_internal_getRgnClip(&clipRegion);
    Vector<CGRect, 8> rects;
    for (SkRegion::Iterator it(clipRegion); !it.done(); it.next()) {
      const SkIRect& rect = it.rect();
      if (!rects.append(
              CGRectMake(rect.x(), rect.y(), rect.width(), rect.height()))) {
        break;
      }
    }
    if (rects.length()) {
      CGContextClipToRects(aCGContext, rects.begin(), rects.length());
    }
  }

  CGContextConcatCTM(aCGContext,
                     GfxMatrixToCGAffineTransform(aDT->GetTransform()));
  return true;
}
// End long comment about transforms.

// The context returned from this method will have the origin
// in the top left and will have applied all the neccessary clips
// and transforms to the CGContext. See the comment above
// SetupCGContext.
CGContextRef DrawTargetSkia::BorrowCGContext(const DrawOptions& aOptions) {
  // Since we can't replay Skia clips, we have to use a layer if we have a
  // complex clip. After saving a layer, the SkCanvas queries for needing a
  // layer change so save if we pushed a layer.
  mNeedLayer = !mCanvas->isClipEmpty() && !mCanvas->isClipRect();
  if (mNeedLayer) {
    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrc);
    SkCanvas::SaveLayerRec rec(nullptr, &paint,
                               SkCanvas::kInitWithPrevious_SaveLayerFlag);
    mCanvas->saveLayer(rec);
  }

  uint8_t* data = nullptr;
  int32_t stride;
  SurfaceFormat format;
  IntSize size;
  IntPoint origin;
  if (!LockBits(&data, &size, &stride, &format, &origin)) {
    NS_WARNING("Could not lock skia bits to wrap CG around");
    return nullptr;
  }

  if (!mNeedLayer && (data == mCanvasData) && mCG && (mCGSize == size)) {
    // If our canvas data still points to the same data,
    // we can reuse the CG Context
    CGContextSetAlpha(mCG, aOptions.mAlpha);
    CGContextSetShouldAntialias(mCG,
                                aOptions.mAntialiasMode != AntialiasMode::NONE);
    CGContextSaveGState(mCG);
    SetupCGContext(this, mCG, mCanvas, origin, size, true);
    return mCG;
  }

  if (!mColorSpace) {
    mColorSpace = (format == SurfaceFormat::A8) ? CGColorSpaceCreateDeviceGray()
                                                : CGColorSpaceCreateDeviceRGB();
  }

  if (mCG) {
    // Release the old CG context since it's no longer valid.
    CGContextRelease(mCG);
  }

  mCanvasData = data;
  mCGSize = size;

  uint32_t bitmapInfo =
      (format == SurfaceFormat::A8)
          ? kCGImageAlphaOnly
          : kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;

  mCG = CGBitmapContextCreateWithData(
      mCanvasData, mCGSize.width, mCGSize.height, 8, /* bits per component */
      stride, mColorSpace, bitmapInfo, NULL, /* Callback when released */
      NULL);
  if (!mCG) {
    if (mNeedLayer) {
      mCanvas->restore();
    }
    ReleaseBits(mCanvasData);
    NS_WARNING("Could not create bitmap around skia data\n");
    return nullptr;
  }

  CGContextSetAlpha(mCG, aOptions.mAlpha);
  CGContextSetShouldAntialias(mCG,
                              aOptions.mAntialiasMode != AntialiasMode::NONE);
  CGContextSetShouldSmoothFonts(mCG, true);
  CGContextSetTextDrawingMode(mCG, kCGTextFill);
  CGContextSaveGState(mCG);
  SetupCGContext(this, mCG, mCanvas, origin, size, !mNeedLayer);
  return mCG;
}

void DrawTargetSkia::ReturnCGContext(CGContextRef aCGContext) {
  MOZ_ASSERT(aCGContext == mCG);
  ReleaseBits(mCanvasData);
  CGContextRestoreGState(aCGContext);

  if (mNeedLayer) {
    // A layer was used for clipping and is about to be popped by the restore.
    // Make sure the CG context referencing it is released first so the popped
    // layer doesn't accidentally get used.
    if (mCG) {
      CGContextRelease(mCG);
      mCG = nullptr;
    }
    mCanvas->restore();
  }
}

CGContextRef BorrowedCGContext::BorrowCGContextFromDrawTarget(DrawTarget* aDT) {
  DrawTargetSkia* skiaDT = static_cast<DrawTargetSkia*>(aDT);
  return skiaDT->BorrowCGContext(DrawOptions());
}

void BorrowedCGContext::ReturnCGContextToDrawTarget(DrawTarget* aDT,
                                                    CGContextRef cg) {
  DrawTargetSkia* skiaDT = static_cast<DrawTargetSkia*>(aDT);
  skiaDT->ReturnCGContext(cg);
}
#endif

static bool CanDrawFont(ScaledFont* aFont) {
  switch (aFont->GetType()) {
    case FontType::FREETYPE:
    case FontType::FONTCONFIG:
    case FontType::MAC:
    case FontType::GDI:
    case FontType::DWRITE:
      return true;
    default:
      return false;
  }
}

void DrawTargetSkia::DrawGlyphs(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                                const Pattern& aPattern,
                                const StrokeOptions* aStrokeOptions,
                                const DrawOptions& aOptions) {
  if (!CanDrawFont(aFont)) {
    return;
  }

  MarkChanged();

  ScaledFontBase* skiaFont = static_cast<ScaledFontBase*>(aFont);
  SkTypeface* typeface = skiaFont->GetSkTypeface();
  if (!typeface) {
    return;
  }

  AutoPaintSetup paint(mCanvas, aOptions, aPattern);
  if (aStrokeOptions && !StrokeOptionsToPaint(paint.mPaint, *aStrokeOptions)) {
    return;
  }

  AntialiasMode aaMode = aFont->GetDefaultAAMode();
  if (aOptions.mAntialiasMode != AntialiasMode::DEFAULT) {
    aaMode = aOptions.mAntialiasMode;
  }
  bool aaEnabled = aaMode != AntialiasMode::NONE;
  paint.mPaint.setAntiAlias(aaEnabled);

  SkFont font(sk_ref_sp(typeface), SkFloatToScalar(skiaFont->mSize));

  bool useSubpixelAA =
      GetPermitSubpixelAA() &&
      (aaMode == AntialiasMode::DEFAULT || aaMode == AntialiasMode::SUBPIXEL);
  font.setEdging(useSubpixelAA ? SkFont::Edging::kSubpixelAntiAlias
                               : (aaEnabled ? SkFont::Edging::kAntiAlias
                                            : SkFont::Edging::kAlias));

  skiaFont->SetupSkFontDrawOptions(font);

  // Limit the amount of internal batch allocations Skia does.
  const uint32_t kMaxGlyphBatchSize = 8192;

  for (uint32_t offset = 0; offset < aBuffer.mNumGlyphs;) {
    uint32_t batchSize =
        std::min(aBuffer.mNumGlyphs - offset, kMaxGlyphBatchSize);
    SkTextBlobBuilder builder;
    auto runBuffer = builder.allocRunPos(font, batchSize);
    for (uint32_t i = 0; i < batchSize; i++, offset++) {
      runBuffer.glyphs[i] = aBuffer.mGlyphs[offset].mIndex;
      runBuffer.points()[i] = PointToSkPoint(aBuffer.mGlyphs[offset].mPosition);
    }

    sk_sp<SkTextBlob> text = builder.make();
    mCanvas->drawTextBlob(text, 0, 0, paint.mPaint);
  }
}

void DrawTargetSkia::FillGlyphs(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                                const Pattern& aPattern,
                                const DrawOptions& aOptions) {
  DrawGlyphs(aFont, aBuffer, aPattern, nullptr, aOptions);
}

void DrawTargetSkia::StrokeGlyphs(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                                  const Pattern& aPattern,
                                  const StrokeOptions& aStrokeOptions,
                                  const DrawOptions& aOptions) {
  DrawGlyphs(aFont, aBuffer, aPattern, &aStrokeOptions, aOptions);
}

void DrawTargetSkia::Mask(const Pattern& aSource, const Pattern& aMask,
                          const DrawOptions& aOptions) {
  SkIRect maskBounds;
  if (!mCanvas->getDeviceClipBounds(&maskBounds)) {
    return;
  }
  SkPoint maskOrigin;
  maskOrigin.iset(maskBounds.fLeft, maskBounds.fTop);

  SkMatrix maskMatrix = mCanvas->getTotalMatrix();
  maskMatrix.postTranslate(-maskOrigin.fX, -maskOrigin.fY);

  MarkChanged();
  AutoPaintSetup paint(mCanvas, aOptions, aSource, nullptr, &maskMatrix);

  Maybe<MutexAutoLock> lock;
  SkPaint maskPaint;
  SetPaintPattern(maskPaint, aMask, lock);

  SkBitmap maskBitmap;
  if (!maskBitmap.tryAllocPixelsFlags(
          SkImageInfo::MakeA8(maskBounds.width(), maskBounds.height()),
          SkBitmap::kZeroPixels_AllocFlag)) {
    return;
  }

  SkCanvas maskCanvas(maskBitmap);
  maskCanvas.setMatrix(maskMatrix);
  maskCanvas.drawPaint(maskPaint);

  mCanvas->save();
  mCanvas->resetMatrix();

  mCanvas->drawBitmap(maskBitmap, maskOrigin.fX, maskOrigin.fY, &paint.mPaint);

  mCanvas->restore();
}

void DrawTargetSkia::MaskSurface(const Pattern& aSource, SourceSurface* aMask,
                                 Point aOffset, const DrawOptions& aOptions) {
  MarkChanged();

  SkMatrix invOffset = SkMatrix::MakeTrans(SkFloatToScalar(-aOffset.x),
                                           SkFloatToScalar(-aOffset.y));
  AutoPaintSetup paint(mCanvas, aOptions, aSource, nullptr, &invOffset);

  Maybe<MutexAutoLock> lock;
  sk_sp<SkImage> alphaMask = ExtractAlphaForSurface(aMask, lock);
  if (!alphaMask) {
    gfxDebug() << *this << ": MaskSurface() failed to extract alpha for mask";
    return;
  }

  mCanvas->drawImage(alphaMask, aOffset.x + aMask->GetRect().x,
                     aOffset.y + aMask->GetRect().y, &paint.mPaint);
}

bool DrawTarget::Draw3DTransformedSurface(SourceSurface* aSurface,
                                          const Matrix4x4& aMatrix) {
  // Composite the 3D transform with the DT's transform.
  Matrix4x4 fullMat = aMatrix * Matrix4x4::From2D(mTransform);
  if (fullMat.IsSingular()) {
    return false;
  }
  // Transform the surface bounds and clip to this DT.
  IntRect xformBounds = RoundedOut(fullMat.TransformAndClipBounds(
      Rect(Point(0, 0), Size(aSurface->GetSize())),
      Rect(Point(0, 0), Size(GetSize()))));
  if (xformBounds.IsEmpty()) {
    return true;
  }
  // Offset the matrix by the transformed origin.
  fullMat.PostTranslate(-xformBounds.X(), -xformBounds.Y(), 0);

  // Read in the source data.
  Maybe<MutexAutoLock> lock;
  sk_sp<SkImage> srcImage = GetSkImageForSurface(aSurface, &lock);
  if (!srcImage) {
    return true;
  }

  // Set up an intermediate destination surface only the size of the transformed
  // bounds. Try to pass through the source's format unmodified in both the BGRA
  // and ARGB cases.
  RefPtr<DataSourceSurface> dstSurf = Factory::CreateDataSourceSurface(
      xformBounds.Size(),
      !srcImage->isOpaque() ? aSurface->GetFormat()
                            : SurfaceFormat::A8R8G8B8_UINT32,
      true);
  if (!dstSurf) {
    return false;
  }

  DataSourceSurface::ScopedMap map(dstSurf, DataSourceSurface::READ_WRITE);
  std::unique_ptr<SkCanvas> dstCanvas(SkCanvas::MakeRasterDirect(
      SkImageInfo::Make(xformBounds.Width(), xformBounds.Height(),
                        GfxFormatToSkiaColorType(dstSurf->GetFormat()),
                        kPremul_SkAlphaType),
      map.GetData(), map.GetStride()));
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

  // Temporarily reset the DT's transform, since it has already been composed
  // above.
  Matrix origTransform = mTransform;
  SetTransform(Matrix());

  // Draw the transformed surface within the transformed bounds.
  DrawSurface(dstSurf, Rect(xformBounds),
              Rect(Point(0, 0), Size(xformBounds.Size())));

  SetTransform(origTransform);

  return true;
}

bool DrawTargetSkia::Draw3DTransformedSurface(SourceSurface* aSurface,
                                              const Matrix4x4& aMatrix) {
  if (aMatrix.IsSingular()) {
    return false;
  }

  MarkChanged();

  Maybe<MutexAutoLock> lock;
  sk_sp<SkImage> image = GetSkImageForSurface(aSurface, &lock);
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

already_AddRefed<SourceSurface> DrawTargetSkia::CreateSourceSurfaceFromData(
    unsigned char* aData, const IntSize& aSize, int32_t aStride,
    SurfaceFormat aFormat) const {
  RefPtr<SourceSurfaceSkia> newSurf = new SourceSurfaceSkia();

  if (!newSurf->InitFromData(aData, aSize, aStride, aFormat)) {
    gfxDebug() << *this
               << ": Failure to create source surface from data. Size: "
               << aSize;
    return nullptr;
  }

  return newSurf.forget();
}

already_AddRefed<DrawTarget> DrawTargetSkia::CreateSimilarDrawTarget(
    const IntSize& aSize, SurfaceFormat aFormat) const {
  RefPtr<DrawTargetSkia> target = new DrawTargetSkia();
#ifdef DEBUG
  if (!IsBackedByPixels(mCanvas)) {
    // If our canvas is backed by vector storage such as PDF then we want to
    // create a new DrawTarget with similar storage to avoid losing fidelity
    // (fidelity will be lost if the returned DT is Snapshot()'ed and drawn
    // back onto us since a raster will be drawn instead of vector commands).
    NS_WARNING("Not backed by pixels - we need to handle PDF backed SkCanvas");
  }
#endif

  if (!target->Init(aSize, aFormat)) {
    return nullptr;
  }
  return target.forget();
}

bool DrawTargetSkia::CanCreateSimilarDrawTarget(const IntSize& aSize,
                                                SurfaceFormat aFormat) const {
  auto minmaxPair = std::minmax(aSize.width, aSize.height);
  return minmaxPair.first > 0 &&
         size_t(minmaxPair.second) < GetMaxSurfaceSize();
}

RefPtr<DrawTarget> DrawTargetSkia::CreateClippedDrawTarget(
    const Rect& aBounds, SurfaceFormat aFormat) {
  SkIRect clipBounds;

  RefPtr<DrawTarget> result;
  // Doing this save()/restore() dance is wasteful
  mCanvas->save();
  if (!aBounds.IsEmpty()) {
    mCanvas->clipRect(RectToSkRect(aBounds), SkClipOp::kIntersect, true);
  }
  if (mCanvas->getDeviceClipBounds(&clipBounds)) {
    RefPtr<DrawTarget> dt = CreateSimilarDrawTarget(
        IntSize(clipBounds.width(), clipBounds.height()), aFormat);
    result = gfx::Factory::CreateOffsetDrawTarget(
        dt, IntPoint(clipBounds.x(), clipBounds.y()));
    result->SetTransform(mTransform);
  } else {
    // Everything is clipped but we still want some kind of surface
    result = CreateSimilarDrawTarget(IntSize(1, 1), aFormat);
  }
  mCanvas->restore();
  return result;
}

already_AddRefed<SourceSurface>
DrawTargetSkia::OptimizeSourceSurfaceForUnknownAlpha(
    SourceSurface* aSurface) const {
  if (aSurface->GetType() == SurfaceType::SKIA) {
    RefPtr<SourceSurface> surface(aSurface);
    return surface.forget();
  }

  RefPtr<DataSourceSurface> dataSurface = aSurface->GetDataSurface();
  DataSourceSurface::ScopedMap map(dataSurface, DataSourceSurface::READ_WRITE);

  // For plugins, GDI can sometimes just write 0 to the alpha channel
  // even for RGBX formats. In this case, we have to manually write
  // the alpha channel to make Skia happy with RGBX and in case GDI
  // writes some bad data. Luckily, this only happens on plugins.
  WriteRGBXFormat(map.GetData(), dataSurface->GetSize(), map.GetStride(),
                  dataSurface->GetFormat());
  return dataSurface.forget();
}

already_AddRefed<SourceSurface> DrawTargetSkia::OptimizeSourceSurface(
    SourceSurface* aSurface) const {
  if (aSurface->GetType() == SurfaceType::SKIA) {
    RefPtr<SourceSurface> surface(aSurface);
    return surface.forget();
  }

  // If we're not using skia-gl then drawing doesn't require any
  // uploading, so any data surface is fine. Call GetDataSurface
  // to trigger any required readback so that it only happens
  // once.
  RefPtr<DataSourceSurface> dataSurface = aSurface->GetDataSurface();
#ifdef DEBUG
  DataSourceSurface::ScopedMap map(dataSurface, DataSourceSurface::READ);
  MOZ_ASSERT(VerifyRGBXFormat(map.GetData(), dataSurface->GetSize(),
                              map.GetStride(), dataSurface->GetFormat()));
#endif
  return dataSurface.forget();
}

already_AddRefed<SourceSurface>
DrawTargetSkia::CreateSourceSurfaceFromNativeSurface(
    const NativeSurface& aSurface) const {
  return nullptr;
}

void DrawTargetSkia::CopySurface(SourceSurface* aSurface,
                                 const IntRect& aSourceRect,
                                 const IntPoint& aDestination) {
  MarkChanged();

  Maybe<MutexAutoLock> lock;
  sk_sp<SkImage> image = GetSkImageForSurface(aSurface, &lock);
  if (!image) {
    return;
  }

  mCanvas->save();
  mCanvas->setMatrix(SkMatrix::MakeTrans(SkIntToScalar(aDestination.x),
                                         SkIntToScalar(aDestination.y)));
  mCanvas->clipRect(SkRect::MakeIWH(aSourceRect.Width(), aSourceRect.Height()),
                    SkClipOp::kReplace_deprecated);

  SkPaint paint;
  if (!image->isOpaque()) {
    // Keep the xfermode as SOURCE_OVER for opaque bitmaps
    // http://code.google.com/p/skia/issues/detail?id=628
    paint.setBlendMode(SkBlendMode::kSrc);
  }
  // drawImage with A8 images ends up doing a mask operation
  // so we need to clear before
  if (image->isAlphaOnly()) {
    mCanvas->clear(SK_ColorTRANSPARENT);
  }
  mCanvas->drawImage(image, -SkIntToScalar(aSourceRect.X()),
                     -SkIntToScalar(aSourceRect.Y()), &paint);
  mCanvas->restore();
}

static inline SkPixelGeometry GetSkPixelGeometry() {
  return Factory::GetBGRSubpixelOrder() ? kBGR_H_SkPixelGeometry
                                        : kRGB_H_SkPixelGeometry;
}

bool DrawTargetSkia::Init(const IntSize& aSize, SurfaceFormat aFormat) {
  if (size_t(std::max(aSize.width, aSize.height)) > GetMaxSurfaceSize()) {
    return false;
  }

  // we need to have surfaces that have a stride aligned to 4 for interop with
  // cairo
  SkImageInfo info = MakeSkiaImageInfo(aSize, aFormat);
  size_t stride = SkAlign4(info.minRowBytes());
  SkSurfaceProps props(0, GetSkPixelGeometry());
  mSurface = SkSurface::MakeRaster(info, stride, &props);
  if (!mSurface) {
    return false;
  }

  mSize = aSize;
  mFormat = aFormat;
  mCanvas = mSurface->getCanvas();
  SetPermitSubpixelAA(IsOpaque(mFormat));

  if (info.isOpaque()) {
    mCanvas->clear(SK_ColorBLACK);
  }
  return true;
}

bool DrawTargetSkia::Init(SkCanvas* aCanvas) {
  mCanvas = aCanvas;

  SkImageInfo imageInfo = mCanvas->imageInfo();

  // If the canvas is backed by pixels we clear it to be on the safe side.  If
  // it's not (for example, for PDF output) we don't.
  if (IsBackedByPixels(mCanvas)) {
    SkColor clearColor =
        imageInfo.isOpaque() ? SK_ColorBLACK : SK_ColorTRANSPARENT;
    mCanvas->clear(clearColor);
  }

  SkISize size = mCanvas->getBaseLayerSize();
  mSize.width = size.width();
  mSize.height = size.height();
  mFormat =
      SkiaColorTypeToGfxFormat(imageInfo.colorType(), imageInfo.alphaType());
  SetPermitSubpixelAA(IsOpaque(mFormat));
  return true;
}

bool DrawTargetSkia::Init(unsigned char* aData, const IntSize& aSize,
                          int32_t aStride, SurfaceFormat aFormat,
                          bool aUninitialized) {
  MOZ_ASSERT((aFormat != SurfaceFormat::B8G8R8X8) || aUninitialized ||
             VerifyRGBXFormat(aData, aSize, aStride, aFormat));

  SkSurfaceProps props(0, GetSkPixelGeometry());
  mSurface = SkSurface::MakeRasterDirect(MakeSkiaImageInfo(aSize, aFormat),
                                         aData, aStride, &props);
  if (!mSurface) {
    return false;
  }

  mSize = aSize;
  mFormat = aFormat;
  mCanvas = mSurface->getCanvas();
  SetPermitSubpixelAA(IsOpaque(mFormat));
  return true;
}

bool DrawTargetSkia::Init(RefPtr<DataSourceSurface>&& aSurface) {
  auto map =
      new DataSourceSurface::ScopedMap(aSurface, DataSourceSurface::READ_WRITE);
  if (!map->IsMapped()) {
    delete map;
    return false;
  }

  SurfaceFormat format = aSurface->GetFormat();
  IntSize size = aSurface->GetSize();
  MOZ_ASSERT((format != SurfaceFormat::B8G8R8X8) ||
             VerifyRGBXFormat(map->GetData(), size, map->GetStride(), format));

  SkSurfaceProps props(0, GetSkPixelGeometry());
  mSurface = SkSurface::MakeRasterDirectReleaseProc(
      MakeSkiaImageInfo(size, format), map->GetData(), map->GetStride(),
      DrawTargetSkia::ReleaseMappedSkSurface, map, &props);
  if (!mSurface) {
    delete map;
    return false;
  }

  // map is now owned by mSurface
  mBackingSurface = std::move(aSurface);
  mSize = size;
  mFormat = format;
  mCanvas = mSurface->getCanvas();
  SetPermitSubpixelAA(IsOpaque(format));
  return true;
}

/* static */ void DrawTargetSkia::ReleaseMappedSkSurface(void* aPixels,
                                                         void* aContext) {
  auto map = reinterpret_cast<DataSourceSurface::ScopedMap*>(aContext);
  delete map;
}

void DrawTargetSkia::SetTransform(const Matrix& aTransform) {
  SkMatrix mat;
  GfxMatrixToSkiaMatrix(aTransform, mat);
  mCanvas->setMatrix(mat);
  mTransform = aTransform;
}

void* DrawTargetSkia::GetNativeSurface(NativeSurfaceType aType) {
  return nullptr;
}

already_AddRefed<PathBuilder> DrawTargetSkia::CreatePathBuilder(
    FillRule aFillRule) const {
  return MakeAndAddRef<PathBuilderSkia>(aFillRule);
}

void DrawTargetSkia::ClearRect(const Rect& aRect) {
  MarkChanged();
  mCanvas->save();
  mCanvas->clipRect(RectToSkRect(aRect), SkClipOp::kIntersect, true);
  SkColor clearColor = (mFormat == SurfaceFormat::B8G8R8X8)
                           ? SK_ColorBLACK
                           : SK_ColorTRANSPARENT;
  mCanvas->clear(clearColor);
  mCanvas->restore();
}

void DrawTargetSkia::PushClip(const Path* aPath) {
  if (aPath->GetBackendType() != BackendType::SKIA) {
    return;
  }

  const PathSkia* skiaPath = static_cast<const PathSkia*>(aPath);
  mCanvas->save();
  mCanvas->clipPath(skiaPath->GetPath(), SkClipOp::kIntersect, true);
}

void DrawTargetSkia::PushDeviceSpaceClipRects(const IntRect* aRects,
                                              uint32_t aCount) {
  // Build a region by unioning all the rects together.
  SkRegion region;
  for (uint32_t i = 0; i < aCount; i++) {
    region.op(IntRectToSkIRect(aRects[i]), SkRegion::kUnion_Op);
  }

  // Clip with the resulting region. clipRegion does not transform
  // this region by the current transform, unlike the other SkCanvas
  // clip methods, so it is just passed through in device-space.
  mCanvas->save();
  mCanvas->clipRegion(region, SkClipOp::kIntersect);
}

void DrawTargetSkia::PushClipRect(const Rect& aRect) {
  SkRect rect = RectToSkRect(aRect);

  mCanvas->save();
  mCanvas->clipRect(rect, SkClipOp::kIntersect, true);
}

void DrawTargetSkia::PopClip() {
  mCanvas->restore();
  SetTransform(GetTransform());
}

void DrawTargetSkia::PushLayer(bool aOpaque, Float aOpacity,
                               SourceSurface* aMask,
                               const Matrix& aMaskTransform,
                               const IntRect& aBounds, bool aCopyBackground) {
  PushLayerWithBlend(aOpaque, aOpacity, aMask, aMaskTransform, aBounds,
                     aCopyBackground, CompositionOp::OP_OVER);
}

void DrawTargetSkia::PushLayerWithBlend(bool aOpaque, Float aOpacity,
                                        SourceSurface* aMask,
                                        const Matrix& aMaskTransform,
                                        const IntRect& aBounds,
                                        bool aCopyBackground,
                                        CompositionOp aCompositionOp) {
  PushedLayer layer(GetPermitSubpixelAA(), aMask);
  mPushedLayers.push_back(layer);

  SkPaint paint;

  paint.setAlpha(ColorFloatToByte(aOpacity));
  paint.setBlendMode(GfxOpToSkiaOp(aCompositionOp));

  // aBounds is supplied in device space, but SaveLayerRec wants local space.
  SkRect bounds = IntRectToSkRect(aBounds);
  if (!bounds.isEmpty()) {
    SkMatrix inverseCTM;
    if (mCanvas->getTotalMatrix().invert(&inverseCTM)) {
      inverseCTM.mapRect(&bounds);
    } else {
      bounds.setEmpty();
    }
  }

  // We don't pass a lock object to GetSkImageForSurface here, to force a
  // copy of the data if this is a copy-on-write snapshot. If we instead held
  // the lock until the corresponding PopLayer, we'd risk deadlocking if someone
  // tried to touch the originating DrawTarget while the layer was pushed.
  sk_sp<SkImage> clipImage =
      aMask ? GetSkImageForSurface(aMask, nullptr) : nullptr;
  SkMatrix clipMatrix;
  GfxMatrixToSkiaMatrix(aMaskTransform, clipMatrix);
  if (aMask) {
    clipMatrix.preTranslate(aMask->GetRect().X(), aMask->GetRect().Y());
  }

  SkCanvas::SaveLayerRec saveRec(
      aBounds.IsEmpty() ? nullptr : &bounds, &paint, nullptr, clipImage.get(),
      &clipMatrix,
      SkCanvas::kPreserveLCDText_SaveLayerFlag |
          (aCopyBackground ? SkCanvas::kInitWithPrevious_SaveLayerFlag : 0));

  mCanvas->saveLayer(saveRec);

  SetPermitSubpixelAA(aOpaque);

#ifdef MOZ_WIDGET_COCOA
  CGContextRelease(mCG);
  mCG = nullptr;
#endif
}

void DrawTargetSkia::PopLayer() {
  MarkChanged();

  MOZ_ASSERT(mPushedLayers.size());
  const PushedLayer& layer = mPushedLayers.back();

  mCanvas->restore();

  SetTransform(GetTransform());
  SetPermitSubpixelAA(layer.mOldPermitSubpixelAA);

  mPushedLayers.pop_back();

#ifdef MOZ_WIDGET_COCOA
  CGContextRelease(mCG);
  mCG = nullptr;
#endif
}

already_AddRefed<GradientStops> DrawTargetSkia::CreateGradientStops(
    GradientStop* aStops, uint32_t aNumStops, ExtendMode aExtendMode) const {
  std::vector<GradientStop> stops;
  stops.resize(aNumStops);
  for (uint32_t i = 0; i < aNumStops; i++) {
    stops[i] = aStops[i];
  }
  std::stable_sort(stops.begin(), stops.end());

  return MakeAndAddRef<GradientStopsSkia>(stops, aNumStops, aExtendMode);
}

already_AddRefed<FilterNode> DrawTargetSkia::CreateFilter(FilterType aType) {
  return FilterNodeSoftware::Create(aType);
}

void DrawTargetSkia::MarkChanged() {
  // I'm not entirely certain whether this lock is needed, as multiple threads
  // should never modify the DrawTarget at the same time anyway, but this seems
  // like the safest.
  MutexAutoLock lock(mSnapshotLock);
  if (mSnapshot) {
    if (mSnapshot->hasOneRef()) {
      // No owners outside of this DrawTarget's own reference. Just dump it.
      mSnapshot = nullptr;
      return;
    }

    mSnapshot->DrawTargetWillChange();
    mSnapshot = nullptr;

    // Handle copying of any image snapshots bound to the surface.
    if (mSurface) {
      mSurface->notifyContentWillChange(SkSurface::kRetain_ContentChangeMode);
    }
  }
}

}  // namespace mozilla::gfx
