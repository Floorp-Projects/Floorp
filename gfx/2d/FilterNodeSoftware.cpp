/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define _USE_MATH_DEFINES

#include <cmath>
#include "DataSurfaceHelpers.h"
#include "FilterNodeSoftware.h"
#include "2D.h"
#include "Tools.h"
#include "Blur.h"
#include <map>
#include "FilterProcessing.h"
#include "Logging.h"
#include "mozilla/PodOperations.h"
#include "mozilla/DebugOnly.h"

// #define DEBUG_DUMP_SURFACES

#ifdef DEBUG_DUMP_SURFACES
#include "gfxUtils.h" // not part of Moz2D
#endif

namespace mozilla {
namespace gfx {

namespace {

/**
 * This class provides a way to get a pow() results in constant-time. It works
 * by caching 256 values for bases between 0 and 1 and a fixed exponent.
 **/
class PowCache
{
public:
  PowCache()
  {
    CacheForExponent(0.0f);
  }

  void CacheForExponent(Float aExponent)
  {
    mExponent = aExponent;
    int numPreSquares = 0;
    while (numPreSquares < 5 && mExponent > (1 << (numPreSquares + 2))) {
      numPreSquares++;
    }
    mNumPowTablePreSquares = numPreSquares;
    for (size_t i = 0; i < sCacheSize; i++) {
      // sCacheSize is chosen in such a way that a takes values
      // from 0.0 to 1.0 inclusive.
      Float a = i / Float(1 << sCacheIndexPrecisionBits);
      MOZ_ASSERT(0.0f <= a && a <= 1.0f, "We only want to cache for bases between 0 and 1.");

      for (int j = 0; j < mNumPowTablePreSquares; j++) {
        a = sqrt(a);
      }
      uint32_t cachedInt = pow(a, mExponent) * (1 << sOutputIntPrecisionBits);
      MOZ_ASSERT(cachedInt < (1 << (sizeof(mPowTable[i]) * 8)), "mPowCache integer type too small");

      mPowTable[i] = cachedInt;
    }
  }

  uint16_t Pow(uint16_t aBase)
  {
    // Results should be similar to what the following code would produce:
    // Float x = Float(aBase) / (1 << sInputIntPrecisionBits);
    // return uint16_t(pow(x, mExponent) * (1 << sOutputIntPrecisionBits));

    MOZ_ASSERT(aBase <= (1 << sInputIntPrecisionBits), "aBase needs to be between 0 and 1!");

    uint32_t a = aBase;
    for (int j = 0; j < mNumPowTablePreSquares; j++) {
      a = a * a >> sInputIntPrecisionBits;
    }
    uint32_t i = a >> (sInputIntPrecisionBits - sCacheIndexPrecisionBits);
    MOZ_ASSERT(i < sCacheSize, "out-of-bounds mPowTable access");
    return mPowTable[i];
  }

  static const int sInputIntPrecisionBits = 15;
  static const int sOutputIntPrecisionBits = 15;
  static const int sCacheIndexPrecisionBits = 7;

private:
  static const size_t sCacheSize = (1 << sCacheIndexPrecisionBits) + 1;

  Float mExponent;
  int mNumPowTablePreSquares;
  uint16_t mPowTable[sCacheSize];
};

class PointLightSoftware
{
public:
  bool SetAttribute(uint32_t aIndex, Float) { return false; }
  bool SetAttribute(uint32_t aIndex, const Point3D &);
  void Prepare() {}
  Point3D GetVectorToLight(const Point3D &aTargetPoint);
  uint32_t GetColor(uint32_t aLightColor, const Point3D &aVectorToLight);

private:
  Point3D mPosition;
};

class SpotLightSoftware
{
public:
  SpotLightSoftware();
  bool SetAttribute(uint32_t aIndex, Float);
  bool SetAttribute(uint32_t aIndex, const Point3D &);
  void Prepare();
  Point3D GetVectorToLight(const Point3D &aTargetPoint);
  uint32_t GetColor(uint32_t aLightColor, const Point3D &aVectorToLight);

private:
  Point3D mPosition;
  Point3D mPointsAt;
  Point3D mVectorFromFocusPointToLight;
  Float mSpecularFocus;
  Float mLimitingConeAngle;
  Float mLimitingConeCos;
  PowCache mPowCache;
};

class DistantLightSoftware
{
public:
  DistantLightSoftware();
  bool SetAttribute(uint32_t aIndex, Float);
  bool SetAttribute(uint32_t aIndex, const Point3D &) { return false; }
  void Prepare();
  Point3D GetVectorToLight(const Point3D &aTargetPoint);
  uint32_t GetColor(uint32_t aLightColor, const Point3D &aVectorToLight);

private:
  Float mAzimuth;
  Float mElevation;
  Point3D mVectorToLight;
};

class DiffuseLightingSoftware
{
public:
  DiffuseLightingSoftware();
  bool SetAttribute(uint32_t aIndex, Float);
  void Prepare() {}
  uint32_t LightPixel(const Point3D &aNormal, const Point3D &aVectorToLight,
                      uint32_t aColor);

private:
  Float mDiffuseConstant;
};

class SpecularLightingSoftware
{
public:
  SpecularLightingSoftware();
  bool SetAttribute(uint32_t aIndex, Float);
  void Prepare();
  uint32_t LightPixel(const Point3D &aNormal, const Point3D &aVectorToLight,
                      uint32_t aColor);

private:
  Float mSpecularConstant;
  Float mSpecularExponent;
  uint32_t mSpecularConstantInt;
  PowCache mPowCache;
};

} // unnamed namespace

// from xpcom/ds/nsMathUtils.h
static int32_t
NS_lround(double x)
{
  return x >= 0.0 ? int32_t(x + 0.5) : int32_t(x - 0.5);
}

already_AddRefed<DataSourceSurface>
CloneAligned(DataSourceSurface* aSource)
{
  return CreateDataSourceSurfaceByCloning(aSource);
}

static void
FillRectWithPixel(DataSourceSurface *aSurface, const IntRect &aFillRect, IntPoint aPixelPos)
{
  MOZ_ASSERT(!aFillRect.Overflows());
  MOZ_ASSERT(IntRect(IntPoint(), aSurface->GetSize()).Contains(aFillRect),
             "aFillRect needs to be completely inside the surface");
  MOZ_ASSERT(SurfaceContainsPoint(aSurface, aPixelPos),
             "aPixelPos needs to be inside the surface");

  DataSourceSurface::ScopedMap surfMap(aSurface, DataSourceSurface::READ_WRITE);
  if(MOZ2D_WARN_IF(!surfMap.IsMapped())) {
    return;
  }
  uint8_t* sourcePixelData = DataAtOffset(aSurface, surfMap.GetMappedSurface(), aPixelPos);
  uint8_t* data = DataAtOffset(aSurface, surfMap.GetMappedSurface(), aFillRect.TopLeft());
  int bpp = BytesPerPixel(aSurface->GetFormat());

  // Fill the first row by hand.
  if (bpp == 4) {
    uint32_t sourcePixel = *(uint32_t*)sourcePixelData;
    for (int32_t x = 0; x < aFillRect.width; x++) {
      *((uint32_t*)data + x) = sourcePixel;
    }
  } else if (BytesPerPixel(aSurface->GetFormat()) == 1) {
    uint8_t sourcePixel = *sourcePixelData;
    memset(data, sourcePixel, aFillRect.width);
  }

  // Copy the first row into the other rows.
  for (int32_t y = 1; y < aFillRect.height; y++) {
    PodCopy(data + y * surfMap.GetStride(), data, aFillRect.width * bpp);
  }
}

static void
FillRectWithVerticallyRepeatingHorizontalStrip(DataSourceSurface *aSurface,
                                               const IntRect &aFillRect,
                                               const IntRect &aSampleRect)
{
  MOZ_ASSERT(!aFillRect.Overflows());
  MOZ_ASSERT(!aSampleRect.Overflows());
  MOZ_ASSERT(IntRect(IntPoint(), aSurface->GetSize()).Contains(aFillRect),
             "aFillRect needs to be completely inside the surface");
  MOZ_ASSERT(IntRect(IntPoint(), aSurface->GetSize()).Contains(aSampleRect),
             "aSampleRect needs to be completely inside the surface");

  DataSourceSurface::ScopedMap surfMap(aSurface, DataSourceSurface::READ_WRITE);
  if (MOZ2D_WARN_IF(!surfMap.IsMapped())) {
    return;
  }

  uint8_t* sampleData = DataAtOffset(aSurface, surfMap.GetMappedSurface(), aSampleRect.TopLeft());
  uint8_t* data = DataAtOffset(aSurface, surfMap.GetMappedSurface(), aFillRect.TopLeft());
  if (BytesPerPixel(aSurface->GetFormat()) == 4) {
    for (int32_t y = 0; y < aFillRect.height; y++) {
      PodCopy((uint32_t*)data, (uint32_t*)sampleData, aFillRect.width);
      data += surfMap.GetStride();
    }
  } else if (BytesPerPixel(aSurface->GetFormat()) == 1) {
    for (int32_t y = 0; y < aFillRect.height; y++) {
      PodCopy(data, sampleData, aFillRect.width);
      data += surfMap.GetStride();
    }
  }
}

static void
FillRectWithHorizontallyRepeatingVerticalStrip(DataSourceSurface *aSurface,
                                               const IntRect &aFillRect,
                                               const IntRect &aSampleRect)
{
  MOZ_ASSERT(!aFillRect.Overflows());
  MOZ_ASSERT(!aSampleRect.Overflows());
  MOZ_ASSERT(IntRect(IntPoint(), aSurface->GetSize()).Contains(aFillRect),
             "aFillRect needs to be completely inside the surface");
  MOZ_ASSERT(IntRect(IntPoint(), aSurface->GetSize()).Contains(aSampleRect),
             "aSampleRect needs to be completely inside the surface");

  DataSourceSurface::ScopedMap surfMap(aSurface, DataSourceSurface::READ_WRITE);
  if (MOZ2D_WARN_IF(!surfMap.IsMapped())) {
    return;
  }

  uint8_t* sampleData = DataAtOffset(aSurface, surfMap.GetMappedSurface(), aSampleRect.TopLeft());
  uint8_t* data = DataAtOffset(aSurface, surfMap.GetMappedSurface(), aFillRect.TopLeft());
  if (BytesPerPixel(aSurface->GetFormat()) == 4) {
    for (int32_t y = 0; y < aFillRect.height; y++) {
      int32_t sampleColor = *((uint32_t*)sampleData);
      for (int32_t x = 0; x < aFillRect.width; x++) {
        *((uint32_t*)data + x) = sampleColor;
      }
      data += surfMap.GetStride();
      sampleData += surfMap.GetStride();
    }
  } else if (BytesPerPixel(aSurface->GetFormat()) == 1) {
    for (int32_t y = 0; y < aFillRect.height; y++) {
      uint8_t sampleColor = *sampleData;
      memset(data, sampleColor, aFillRect.width);
      data += surfMap.GetStride();
      sampleData += surfMap.GetStride();
    }
  }
}

static void
DuplicateEdges(DataSourceSurface* aSurface, const IntRect &aFromRect)
{
  MOZ_ASSERT(!aFromRect.Overflows());
  MOZ_ASSERT(IntRect(IntPoint(), aSurface->GetSize()).Contains(aFromRect),
             "aFromRect needs to be completely inside the surface");

  IntSize size = aSurface->GetSize();
  IntRect fill;
  IntRect sampleRect;
  for (int32_t ix = 0; ix < 3; ix++) {
    switch (ix) {
      case 0:
        fill.x = 0;
        fill.width = aFromRect.x;
        sampleRect.x = fill.XMost();
        sampleRect.width = 1;
        break;
      case 1:
        fill.x = aFromRect.x;
        fill.width = aFromRect.width;
        sampleRect.x = fill.x;
        sampleRect.width = fill.width;
        break;
      case 2:
        fill.x = aFromRect.XMost();
        fill.width = size.width - fill.x;
        sampleRect.x = fill.x - 1;
        sampleRect.width = 1;
        break;
    }
    if (fill.width <= 0) {
      continue;
    }
    bool xIsMiddle = (ix == 1);
    for (int32_t iy = 0; iy < 3; iy++) {
      switch (iy) {
        case 0:
          fill.y = 0;
          fill.height = aFromRect.y;
          sampleRect.y = fill.YMost();
          sampleRect.height = 1;
          break;
        case 1:
          fill.y = aFromRect.y;
          fill.height = aFromRect.height;
          sampleRect.y = fill.y;
          sampleRect.height = fill.height;
          break;
        case 2:
          fill.y = aFromRect.YMost();
          fill.height = size.height - fill.y;
          sampleRect.y = fill.y - 1;
          sampleRect.height = 1;
          break;
      }
      if (fill.height <= 0) {
        continue;
      }
      bool yIsMiddle = (iy == 1);
      if (!xIsMiddle && !yIsMiddle) {
        // Corner
        FillRectWithPixel(aSurface, fill, sampleRect.TopLeft());
      }
      if (xIsMiddle && !yIsMiddle) {
        // Top middle or bottom middle
        FillRectWithVerticallyRepeatingHorizontalStrip(aSurface, fill, sampleRect);
      }
      if (!xIsMiddle && yIsMiddle) {
        // Left middle or right middle
        FillRectWithHorizontallyRepeatingVerticalStrip(aSurface, fill, sampleRect);
      }
    }
  }
}

static IntPoint
TileIndex(const IntRect &aFirstTileRect, const IntPoint &aPoint)
{
  return IntPoint(int32_t(floor(double(aPoint.x - aFirstTileRect.x) / aFirstTileRect.width)),
                  int32_t(floor(double(aPoint.y - aFirstTileRect.y) / aFirstTileRect.height)));
}

static void
TileSurface(DataSourceSurface* aSource, DataSourceSurface* aTarget, const IntPoint &aOffset)
{
  IntRect sourceRect(aOffset, aSource->GetSize());
  IntRect targetRect(IntPoint(0, 0), aTarget->GetSize());
  IntPoint startIndex = TileIndex(sourceRect, targetRect.TopLeft());
  IntPoint endIndex = TileIndex(sourceRect, targetRect.BottomRight());

  for (int32_t ix = startIndex.x; ix <= endIndex.x; ix++) {
    for (int32_t iy = startIndex.y; iy <= endIndex.y; iy++) {
      IntPoint destPoint(sourceRect.x + ix * sourceRect.width,
                         sourceRect.y + iy * sourceRect.height);
      IntRect destRect(destPoint, sourceRect.Size());
      destRect = destRect.Intersect(targetRect);
      IntRect srcRect = destRect - destPoint;
      CopyRect(aSource, aTarget, srcRect, destRect.TopLeft());
    }
  }
}

static already_AddRefed<DataSourceSurface>
GetDataSurfaceInRect(SourceSurface *aSurface,
                     const IntRect &aSurfaceRect,
                     const IntRect &aDestRect,
                     ConvolveMatrixEdgeMode aEdgeMode)
{
  MOZ_ASSERT(aSurface ? aSurfaceRect.Size() == aSurface->GetSize() : aSurfaceRect.IsEmpty());

  if (aSurfaceRect.Overflows() || aDestRect.Overflows()) {
    // We can't rely on the intersection calculations below to make sense when
    // XMost() or YMost() overflow. Bail out.
    return nullptr;
  }

  IntRect sourceRect = aSurfaceRect;

  if (sourceRect.IsEqualEdges(aDestRect)) {
    return aSurface ? aSurface->GetDataSurface() : nullptr;
  }

  IntRect intersect = sourceRect.Intersect(aDestRect);
  IntRect intersectInSourceSpace = intersect - sourceRect.TopLeft();
  IntRect intersectInDestSpace = intersect - aDestRect.TopLeft();
  SurfaceFormat format = aSurface ? aSurface->GetFormat() : SurfaceFormat(SurfaceFormat::B8G8R8A8);

  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aDestRect.Size(), format, true);
  if (MOZ2D_WARN_IF(!target)) {
    return nullptr;
  }

  if (!aSurface) {
    return target.forget();
  }

  RefPtr<DataSourceSurface> dataSource = aSurface->GetDataSurface();
  MOZ_ASSERT(dataSource);

  if (aEdgeMode == EDGE_MODE_WRAP) {
    TileSurface(dataSource, target, intersectInDestSpace.TopLeft());
    return target.forget();
  }

  CopyRect(dataSource, target, intersectInSourceSpace,
           intersectInDestSpace.TopLeft());

  if (aEdgeMode == EDGE_MODE_DUPLICATE) {
    DuplicateEdges(target, intersectInDestSpace);
  }

  return target.forget();
}

/* static */ already_AddRefed<FilterNode>
FilterNodeSoftware::Create(FilterType aType)
{
  RefPtr<FilterNodeSoftware> filter;
  switch (aType) {
    case FilterType::BLEND:
      filter = new FilterNodeBlendSoftware();
      break;
    case FilterType::TRANSFORM:
      filter = new FilterNodeTransformSoftware();
      break;
    case FilterType::MORPHOLOGY:
      filter = new FilterNodeMorphologySoftware();
      break;
    case FilterType::COLOR_MATRIX:
      filter = new FilterNodeColorMatrixSoftware();
      break;
    case FilterType::FLOOD:
      filter = new FilterNodeFloodSoftware();
      break;
    case FilterType::TILE:
      filter = new FilterNodeTileSoftware();
      break;
    case FilterType::TABLE_TRANSFER:
      filter = new FilterNodeTableTransferSoftware();
      break;
    case FilterType::DISCRETE_TRANSFER:
      filter = new FilterNodeDiscreteTransferSoftware();
      break;
    case FilterType::LINEAR_TRANSFER:
      filter = new FilterNodeLinearTransferSoftware();
      break;
    case FilterType::GAMMA_TRANSFER:
      filter = new FilterNodeGammaTransferSoftware();
      break;
    case FilterType::CONVOLVE_MATRIX:
      filter = new FilterNodeConvolveMatrixSoftware();
      break;
    case FilterType::DISPLACEMENT_MAP:
      filter = new FilterNodeDisplacementMapSoftware();
      break;
    case FilterType::TURBULENCE:
      filter = new FilterNodeTurbulenceSoftware();
      break;
    case FilterType::ARITHMETIC_COMBINE:
      filter = new FilterNodeArithmeticCombineSoftware();
      break;
    case FilterType::COMPOSITE:
      filter = new FilterNodeCompositeSoftware();
      break;
    case FilterType::GAUSSIAN_BLUR:
      filter = new FilterNodeGaussianBlurSoftware();
      break;
    case FilterType::DIRECTIONAL_BLUR:
      filter = new FilterNodeDirectionalBlurSoftware();
      break;
    case FilterType::CROP:
      filter = new FilterNodeCropSoftware();
      break;
    case FilterType::PREMULTIPLY:
      filter = new FilterNodePremultiplySoftware();
      break;
    case FilterType::UNPREMULTIPLY:
      filter = new FilterNodeUnpremultiplySoftware();
      break;
    case FilterType::POINT_DIFFUSE:
      filter = new FilterNodeLightingSoftware<PointLightSoftware, DiffuseLightingSoftware>("FilterNodeLightingSoftware<PointLight, DiffuseLighting>");
      break;
    case FilterType::POINT_SPECULAR:
      filter = new FilterNodeLightingSoftware<PointLightSoftware, SpecularLightingSoftware>("FilterNodeLightingSoftware<PointLight, SpecularLighting>");
      break;
    case FilterType::SPOT_DIFFUSE:
      filter = new FilterNodeLightingSoftware<SpotLightSoftware, DiffuseLightingSoftware>("FilterNodeLightingSoftware<SpotLight, DiffuseLighting>");
      break;
    case FilterType::SPOT_SPECULAR:
      filter = new FilterNodeLightingSoftware<SpotLightSoftware, SpecularLightingSoftware>("FilterNodeLightingSoftware<SpotLight, SpecularLighting>");
      break;
    case FilterType::DISTANT_DIFFUSE:
      filter = new FilterNodeLightingSoftware<DistantLightSoftware, DiffuseLightingSoftware>("FilterNodeLightingSoftware<DistantLight, DiffuseLighting>");
      break;
    case FilterType::DISTANT_SPECULAR:
      filter = new FilterNodeLightingSoftware<DistantLightSoftware, SpecularLightingSoftware>("FilterNodeLightingSoftware<DistantLight, SpecularLighting>");
      break;
  }
  return filter.forget();
}

void
FilterNodeSoftware::Draw(DrawTarget* aDrawTarget,
                         const Rect &aSourceRect,
                         const Point &aDestPoint,
                         const DrawOptions &aOptions)
{
#ifdef DEBUG_DUMP_SURFACES
  printf("<style>section{margin:10px;}</style><pre>\nRendering filter %s...\n", GetName());
#endif

  Rect renderRect = aSourceRect;
  renderRect.RoundOut();
  IntRect renderIntRect;
  if (!renderRect.ToIntRect(&renderIntRect)) {
#ifdef DEBUG_DUMP_SURFACES
    printf("render rect overflowed, not painting anything\n");
    printf("</pre>\n");
#endif
    return;
  }

  IntRect outputRect = GetOutputRectInRect(renderIntRect);
  if (outputRect.Overflows()) {
#ifdef DEBUG_DUMP_SURFACES
    printf("output rect overflowed, not painting anything\n");
    printf("</pre>\n");
#endif
    return;
  }

  RefPtr<DataSourceSurface> result;
  if (!outputRect.IsEmpty()) {
    result = GetOutput(outputRect);
  }

  if (!result) {
    // Null results are allowed and treated as transparent. Don't draw anything.
#ifdef DEBUG_DUMP_SURFACES
    printf("output returned null\n");
    printf("</pre>\n");
#endif
    return;
  }

#ifdef DEBUG_DUMP_SURFACES
  printf("output from %s:\n", GetName());
  printf("<img src='"); gfxUtils::DumpAsDataURL(result); printf("'>\n");
  printf("</pre>\n");
#endif

  Point sourceToDestOffset = aDestPoint - aSourceRect.TopLeft();
  Rect renderedSourceRect = Rect(outputRect).Intersect(aSourceRect);
  Rect renderedDestRect = renderedSourceRect + sourceToDestOffset;
  if (result->GetFormat() == SurfaceFormat::A8) {
    // Interpret the result as having implicitly black color channels.
    aDrawTarget->PushClipRect(renderedDestRect);
    aDrawTarget->MaskSurface(ColorPattern(Color(0.0, 0.0, 0.0, 1.0)),
                             result,
                             Point(outputRect.TopLeft()) + sourceToDestOffset,
                             aOptions);
    aDrawTarget->PopClip();
  } else {
    aDrawTarget->DrawSurface(result, renderedDestRect,
                             renderedSourceRect - Point(outputRect.TopLeft()),
                             DrawSurfaceOptions(), aOptions);
  }
}

already_AddRefed<DataSourceSurface>
FilterNodeSoftware::GetOutput(const IntRect &aRect)
{
  MOZ_ASSERT(GetOutputRectInRect(aRect).Contains(aRect));

  if (aRect.Overflows()) {
    return nullptr;
  }

  if (!mCachedRect.Contains(aRect)) {
    RequestRect(aRect);
    mCachedOutput = Render(mRequestedRect);
    if (!mCachedOutput) {
      mCachedRect = IntRect();
      mRequestedRect = IntRect();
      return nullptr;
    }
    mCachedRect = mRequestedRect;
    mRequestedRect = IntRect();
  } else {
    MOZ_ASSERT(mCachedOutput, "cached rect but no cached output?");
  }
  return GetDataSurfaceInRect(mCachedOutput, mCachedRect, aRect, EDGE_MODE_NONE);
}

void
FilterNodeSoftware::RequestRect(const IntRect &aRect)
{
  if (mRequestedRect.Contains(aRect)) {
    // Bail out now. Otherwise pathological filters can spend time exponential
    // in the number of primitives, e.g. if each primitive takes the
    // previous primitive as its two inputs.
    return;
  }
  mRequestedRect = mRequestedRect.Union(aRect);
  RequestFromInputsForRect(aRect);
}

void
FilterNodeSoftware::RequestInputRect(uint32_t aInputEnumIndex, const IntRect &aRect)
{
  if (aRect.Overflows()) {
    return;
  }

  int32_t inputIndex = InputIndex(aInputEnumIndex);
  if (inputIndex < 0 || (uint32_t)inputIndex >= NumberOfSetInputs()) {
    MOZ_CRASH();
  }
  if (mInputSurfaces[inputIndex]) {
    return;
  }
  RefPtr<FilterNodeSoftware> filter = mInputFilters[inputIndex];
  MOZ_ASSERT(filter, "missing input");
  filter->RequestRect(filter->GetOutputRectInRect(aRect));
}

SurfaceFormat
FilterNodeSoftware::DesiredFormat(SurfaceFormat aCurrentFormat,
                                  FormatHint aFormatHint)
{
  if (aCurrentFormat == SurfaceFormat::A8 && aFormatHint == CAN_HANDLE_A8) {
    return SurfaceFormat::A8;
  }
  return SurfaceFormat::B8G8R8A8;
}

already_AddRefed<DataSourceSurface>
FilterNodeSoftware::GetInputDataSourceSurface(uint32_t aInputEnumIndex,
                                              const IntRect& aRect,
                                              FormatHint aFormatHint,
                                              ConvolveMatrixEdgeMode aEdgeMode,
                                              const IntRect *aTransparencyPaddedSourceRect)
{
  if (aRect.Overflows()) {
    return nullptr;
  }

#ifdef DEBUG_DUMP_SURFACES
  printf("<section><h1>GetInputDataSourceSurface with aRect: %d, %d, %d, %d</h1>\n",
         aRect.x, aRect.y, aRect.width, aRect.height);
#endif
  int32_t inputIndex = InputIndex(aInputEnumIndex);
  if (inputIndex < 0 || (uint32_t)inputIndex >= NumberOfSetInputs()) {
    MOZ_CRASH();
    return nullptr;
  }

  if (aRect.IsEmpty()) {
    return nullptr;
  }

  RefPtr<SourceSurface> surface;
  IntRect surfaceRect;

  if (mInputSurfaces[inputIndex]) {
    // Input from input surface
    surface = mInputSurfaces[inputIndex];
#ifdef DEBUG_DUMP_SURFACES
    printf("input from input surface:\n");
#endif
    surfaceRect = IntRect(IntPoint(0, 0), surface->GetSize());
  } else {
    // Input from input filter
#ifdef DEBUG_DUMP_SURFACES
    printf("getting input from input filter %s...\n", mInputFilters[inputIndex]->GetName());
#endif
    RefPtr<FilterNodeSoftware> filter = mInputFilters[inputIndex];
    MOZ_ASSERT(filter, "missing input");
    IntRect inputFilterOutput = filter->GetOutputRectInRect(aRect);
    if (!inputFilterOutput.IsEmpty()) {
      surface = filter->GetOutput(inputFilterOutput);
    }
#ifdef DEBUG_DUMP_SURFACES
    printf("input from input filter %s:\n", mInputFilters[inputIndex]->GetName());
#endif
    surfaceRect = inputFilterOutput;
    MOZ_ASSERT(!surface || surfaceRect.Size() == surface->GetSize());
  }

  if (surface && surface->GetFormat() == SurfaceFormat::UNKNOWN) {
#ifdef DEBUG_DUMP_SURFACES
    printf("wrong input format</section>\n\n");
#endif
    return nullptr;
  }

  if (!surfaceRect.IsEmpty() && !surface) {
#ifdef DEBUG_DUMP_SURFACES
    printf(" -- no input --</section>\n\n");
#endif
    return nullptr;
  }

  if (aTransparencyPaddedSourceRect && !aTransparencyPaddedSourceRect->IsEmpty()) {
    IntRect srcRect = aTransparencyPaddedSourceRect->Intersect(aRect);
    surface = GetDataSurfaceInRect(surface, surfaceRect, srcRect, EDGE_MODE_NONE);
    surfaceRect = srcRect;
  }

  RefPtr<DataSourceSurface> result =
    GetDataSurfaceInRect(surface, surfaceRect, aRect, aEdgeMode);

  if (result) {
    // TODO: This isn't safe since we don't have a guarantee
    // that future Maps will have the same stride
    DataSourceSurface::MappedSurface map;
    if (result->Map(DataSourceSurface::READ, &map)) {
       // Unmap immediately since CloneAligned hasn't been updated
       // to use the Map API yet. We can still read the stride/data
       // values as long as we don't try to dereference them.
      result->Unmap();
      if (map.mStride != GetAlignedStride<16>(map.mStride) ||
          reinterpret_cast<uintptr_t>(map.mData) % 16 != 0) {
        // Align unaligned surface.
        result = CloneAligned(result);
      }
    } else {
      result = nullptr;
    }
  }


  if (!result) {
#ifdef DEBUG_DUMP_SURFACES
    printf(" -- no input --</section>\n\n");
#endif
    return nullptr;
  }

  SurfaceFormat currentFormat = result->GetFormat();
  if (DesiredFormat(currentFormat, aFormatHint) == SurfaceFormat::B8G8R8A8 &&
      currentFormat != SurfaceFormat::B8G8R8A8) {
    result = FilterProcessing::ConvertToB8G8R8A8(result);
  }

#ifdef DEBUG_DUMP_SURFACES
  printf("<img src='"); gfxUtils::DumpAsDataURL(result); printf("'></section>");
#endif

  MOZ_ASSERT(!result || result->GetSize() == aRect.Size(), "wrong surface size");

  return result.forget();
}

IntRect
FilterNodeSoftware::GetInputRectInRect(uint32_t aInputEnumIndex,
                                       const IntRect &aInRect)
{
  if (aInRect.Overflows()) {
    return IntRect();
  }

  int32_t inputIndex = InputIndex(aInputEnumIndex);
  if (inputIndex < 0 || (uint32_t)inputIndex >= NumberOfSetInputs()) {
    MOZ_CRASH();
    return IntRect();
  }
  if (mInputSurfaces[inputIndex]) {
    return aInRect.Intersect(IntRect(IntPoint(0, 0),
                                     mInputSurfaces[inputIndex]->GetSize()));
  }
  RefPtr<FilterNodeSoftware> filter = mInputFilters[inputIndex];
  MOZ_ASSERT(filter, "missing input");
  return filter->GetOutputRectInRect(aInRect);
}

size_t
FilterNodeSoftware::NumberOfSetInputs()
{
  return std::max(mInputSurfaces.size(), mInputFilters.size());
}

void
FilterNodeSoftware::AddInvalidationListener(FilterInvalidationListener* aListener)
{
  MOZ_ASSERT(aListener, "null listener");
  mInvalidationListeners.push_back(aListener);
}

void
FilterNodeSoftware::RemoveInvalidationListener(FilterInvalidationListener* aListener)
{
  MOZ_ASSERT(aListener, "null listener");
  std::vector<FilterInvalidationListener*>::iterator it =
    std::find(mInvalidationListeners.begin(), mInvalidationListeners.end(), aListener);
  mInvalidationListeners.erase(it);
}

void
FilterNodeSoftware::FilterInvalidated(FilterNodeSoftware* aFilter)
{
  Invalidate();
}

void
FilterNodeSoftware::Invalidate()
{
  mCachedOutput = nullptr;
  mCachedRect = IntRect();
  for (std::vector<FilterInvalidationListener*>::iterator it = mInvalidationListeners.begin();
       it != mInvalidationListeners.end(); it++) {
    (*it)->FilterInvalidated(this);
  }
}

FilterNodeSoftware::~FilterNodeSoftware()
{
  MOZ_ASSERT(!mInvalidationListeners.size(),
             "All invalidation listeners should have unsubscribed themselves by now!");

  for (std::vector<RefPtr<FilterNodeSoftware> >::iterator it = mInputFilters.begin();
       it != mInputFilters.end(); it++) {
    if (*it) {
      (*it)->RemoveInvalidationListener(this);
    }
  }
}

void
FilterNodeSoftware::SetInput(uint32_t aIndex, FilterNode *aFilter)
{
  if (aFilter && aFilter->GetBackendType() != FILTER_BACKEND_SOFTWARE) {
    MOZ_ASSERT(false, "can only take software filters as inputs");
    return;
  }
  SetInput(aIndex, nullptr, static_cast<FilterNodeSoftware*>(aFilter));
}

void
FilterNodeSoftware::SetInput(uint32_t aIndex, SourceSurface *aSurface)
{
  SetInput(aIndex, aSurface, nullptr);
}

void
FilterNodeSoftware::SetInput(uint32_t aInputEnumIndex,
                             SourceSurface *aSurface,
                             FilterNodeSoftware *aFilter)
{
  int32_t inputIndex = InputIndex(aInputEnumIndex);
  if (inputIndex < 0) {
    MOZ_CRASH();
    return;
  }
  if ((uint32_t)inputIndex >= NumberOfSetInputs()) {
    mInputSurfaces.resize(inputIndex + 1);
    mInputFilters.resize(inputIndex + 1);
  }
  mInputSurfaces[inputIndex] = aSurface;
  if (mInputFilters[inputIndex]) {
    mInputFilters[inputIndex]->RemoveInvalidationListener(this);
  }
  if (aFilter) {
    aFilter->AddInvalidationListener(this);
  }
  mInputFilters[inputIndex] = aFilter;
  if (!aSurface && !aFilter && (size_t)inputIndex == NumberOfSetInputs()) {
    mInputSurfaces.resize(inputIndex);
    mInputFilters.resize(inputIndex);
  }
  Invalidate();
}

FilterNodeBlendSoftware::FilterNodeBlendSoftware()
 : mBlendMode(BLEND_MODE_MULTIPLY)
{}

int32_t
FilterNodeBlendSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_BLEND_IN: return 0;
    case IN_BLEND_IN2: return 1;
    default: return -1;
  }
}

void
FilterNodeBlendSoftware::SetAttribute(uint32_t aIndex, uint32_t aBlendMode)
{
  MOZ_ASSERT(aIndex == ATT_BLEND_BLENDMODE);
  mBlendMode = static_cast<BlendMode>(aBlendMode);
  Invalidate();
}

static CompositionOp ToBlendOp(BlendMode aOp)
{
  switch (aOp) {
  case BLEND_MODE_MULTIPLY:
    return CompositionOp::OP_MULTIPLY;
  case BLEND_MODE_SCREEN:
    return CompositionOp::OP_SCREEN;
  case BLEND_MODE_OVERLAY:
    return CompositionOp::OP_OVERLAY;
  case BLEND_MODE_DARKEN:
    return CompositionOp::OP_DARKEN;
  case BLEND_MODE_LIGHTEN:
    return CompositionOp::OP_LIGHTEN;
  case BLEND_MODE_COLOR_DODGE:
    return CompositionOp::OP_COLOR_DODGE;
  case BLEND_MODE_COLOR_BURN:
    return CompositionOp::OP_COLOR_BURN;
  case BLEND_MODE_HARD_LIGHT:
    return CompositionOp::OP_HARD_LIGHT;
  case BLEND_MODE_SOFT_LIGHT:
    return CompositionOp::OP_SOFT_LIGHT;
  case BLEND_MODE_DIFFERENCE:
    return CompositionOp::OP_DIFFERENCE;
  case BLEND_MODE_EXCLUSION:
    return CompositionOp::OP_EXCLUSION;
  case BLEND_MODE_HUE:
    return CompositionOp::OP_HUE;
  case BLEND_MODE_SATURATION:
    return CompositionOp::OP_SATURATION;
  case BLEND_MODE_COLOR:
    return CompositionOp::OP_COLOR;
  case BLEND_MODE_LUMINOSITY:
    return CompositionOp::OP_LUMINOSITY;
  default:
    return CompositionOp::OP_OVER;
  }

  return CompositionOp::OP_OVER;
}

already_AddRefed<DataSourceSurface>
FilterNodeBlendSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input1 =
    GetInputDataSourceSurface(IN_BLEND_IN, aRect, NEED_COLOR_CHANNELS);
  RefPtr<DataSourceSurface> input2 =
    GetInputDataSourceSurface(IN_BLEND_IN2, aRect, NEED_COLOR_CHANNELS);

  // Null inputs need to be treated as transparent.

  // First case: both are transparent.
  if (!input1 && !input2) {
    // Then the result is transparent, too.
    return nullptr;
  }

  // Second case: one of them is transparent. Return the non-transparent one.
  if (!input1 || !input2) {
    return input1 ? input1.forget() : input2.forget();
  }

  // Third case: both are non-transparent.
  // Apply normal filtering.
  RefPtr<DataSourceSurface> target = FilterProcessing::ApplyBlending(input1, input2, mBlendMode);
  if (target != nullptr) {
    return target.forget();
  }

  IntSize size = input1->GetSize();
  target =
    Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
  if (MOZ2D_WARN_IF(!target)) {
    return nullptr;
  }

  CopyRect(input1, target, IntRect(IntPoint(), size), IntPoint());

  // This needs to stay in scope until the draw target has been flushed.
  DataSourceSurface::ScopedMap targetMap(target, DataSourceSurface::READ_WRITE);
  if (MOZ2D_WARN_IF(!targetMap.IsMapped())) {
    return nullptr;
  }

  RefPtr<DrawTarget> dt =
    Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                     targetMap.GetData(),
                                     target->GetSize(),
                                     targetMap.GetStride(),
                                     target->GetFormat());

  if (!dt) {
    gfxWarning() << "FilterNodeBlendSoftware::Render failed in CreateDrawTargetForData";
    return nullptr;
  }

  Rect r(0, 0, size.width, size.height);
  dt->DrawSurface(input2, r, r, DrawSurfaceOptions(), DrawOptions(1.0f, ToBlendOp(mBlendMode)));
  dt->Flush();
  return target.forget();
}

void
FilterNodeBlendSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_BLEND_IN, aRect);
  RequestInputRect(IN_BLEND_IN2, aRect);
}

IntRect
FilterNodeBlendSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_BLEND_IN, aRect).Union(
    GetInputRectInRect(IN_BLEND_IN2, aRect)).Intersect(aRect);
}

FilterNodeTransformSoftware::FilterNodeTransformSoftware()
 : mFilter(Filter::GOOD)
{}

int32_t
FilterNodeTransformSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_TRANSFORM_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeTransformSoftware::SetAttribute(uint32_t aIndex, uint32_t aFilter)
{
  MOZ_ASSERT(aIndex == ATT_TRANSFORM_FILTER);
  mFilter = static_cast<Filter>(aFilter);
  Invalidate();
}

void
FilterNodeTransformSoftware::SetAttribute(uint32_t aIndex, const Matrix &aMatrix)
{
  MOZ_ASSERT(aIndex == ATT_TRANSFORM_MATRIX);
  mMatrix = aMatrix;
  Invalidate();
}

IntRect
FilterNodeTransformSoftware::SourceRectForOutputRect(const IntRect &aRect)
{
  if (aRect.IsEmpty()) {
    return IntRect();
  }

  Matrix inverted(mMatrix);
  if (!inverted.Invert()) {
    return IntRect();
  }

  Rect neededRect = inverted.TransformBounds(Rect(aRect));
  neededRect.RoundOut();
  IntRect neededIntRect;
  if (!neededRect.ToIntRect(&neededIntRect)) {
    return IntRect();
  }
  return GetInputRectInRect(IN_TRANSFORM_IN, neededIntRect);
}

already_AddRefed<DataSourceSurface>
FilterNodeTransformSoftware::Render(const IntRect& aRect)
{
  IntRect srcRect = SourceRectForOutputRect(aRect);

  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_TRANSFORM_IN, srcRect);

  if (!input) {
    return nullptr;
  }

  Matrix transform = Matrix::Translation(srcRect.x, srcRect.y) * mMatrix *
                     Matrix::Translation(-aRect.x, -aRect.y);
  if (transform.IsIdentity() && srcRect.Size() == aRect.Size()) {
    return input.forget();
  }

  RefPtr<DataSourceSurface> surf =
    Factory::CreateDataSourceSurface(aRect.Size(), input->GetFormat(), true);

  if (!surf) {
    return nullptr;
  }

  DataSourceSurface::MappedSurface mapping;
  if (!surf->Map(DataSourceSurface::MapType::WRITE, &mapping)) {
    gfxCriticalError() << "FilterNodeTransformSoftware::Render failed to map surface";
    return nullptr;
  }

  RefPtr<DrawTarget> dt =
    Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                     mapping.mData,
                                     surf->GetSize(),
                                     mapping.mStride,
                                     surf->GetFormat());
  if (!dt) {
    gfxWarning() << "FilterNodeTransformSoftware::Render failed in CreateDrawTargetForData";
    return nullptr;
  }

  Rect r(0, 0, srcRect.width, srcRect.height);
  dt->SetTransform(transform);
  dt->DrawSurface(input, r, r, DrawSurfaceOptions(mFilter));

  dt->Flush();
  surf->Unmap();
  return surf.forget();
}

void
FilterNodeTransformSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_TRANSFORM_IN, SourceRectForOutputRect(aRect));
}

IntRect
FilterNodeTransformSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect srcRect = SourceRectForOutputRect(aRect);
  if (srcRect.IsEmpty()) {
    return IntRect();
  }

  Rect outRect = mMatrix.TransformBounds(Rect(srcRect));
  outRect.RoundOut();
  IntRect outIntRect;
  if (!outRect.ToIntRect(&outIntRect)) {
    return IntRect();
  }
  return outIntRect.Intersect(aRect);
}

FilterNodeMorphologySoftware::FilterNodeMorphologySoftware()
 : mOperator(MORPHOLOGY_OPERATOR_ERODE)
{}

int32_t
FilterNodeMorphologySoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_MORPHOLOGY_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeMorphologySoftware::SetAttribute(uint32_t aIndex,
                                           const IntSize &aRadii)
{
  MOZ_ASSERT(aIndex == ATT_MORPHOLOGY_RADII);
  mRadii.width = std::min(std::max(aRadii.width, 0), 100000);
  mRadii.height = std::min(std::max(aRadii.height, 0), 100000);
  Invalidate();
}

void
FilterNodeMorphologySoftware::SetAttribute(uint32_t aIndex,
                                           uint32_t aOperator)
{
  MOZ_ASSERT(aIndex == ATT_MORPHOLOGY_OPERATOR);
  mOperator = static_cast<MorphologyOperator>(aOperator);
  Invalidate();
}

static already_AddRefed<DataSourceSurface>
ApplyMorphology(const IntRect& aSourceRect, DataSourceSurface* aInput,
                const IntRect& aDestRect, int32_t rx, int32_t ry,
                MorphologyOperator aOperator)
{
  IntRect srcRect = aSourceRect - aDestRect.TopLeft();
  IntRect destRect = aDestRect - aDestRect.TopLeft();
  IntRect tmpRect(destRect.x, srcRect.y, destRect.width, srcRect.height);
#ifdef DEBUG
  IntMargin margin = srcRect - destRect;
  MOZ_ASSERT(margin.top >= ry && margin.right >= rx &&
             margin.bottom >= ry && margin.left >= rx, "insufficient margin");
#endif

  RefPtr<DataSourceSurface> tmp;
  if (rx == 0) {
    tmp = aInput;
  } else {
    tmp = Factory::CreateDataSourceSurface(tmpRect.Size(), SurfaceFormat::B8G8R8A8);
    if (MOZ2D_WARN_IF(!tmp)) {
      return nullptr;
    }

    DataSourceSurface::ScopedMap sourceMap(aInput, DataSourceSurface::READ);
    DataSourceSurface::ScopedMap tmpMap(tmp, DataSourceSurface::WRITE);
    if (MOZ2D_WARN_IF(!sourceMap.IsMapped() || !tmpMap.IsMapped())) {
      return nullptr;
    }
    uint8_t* sourceData = DataAtOffset(aInput, sourceMap.GetMappedSurface(),
                                       destRect.TopLeft() - srcRect.TopLeft());
    uint8_t* tmpData = DataAtOffset(tmp, tmpMap.GetMappedSurface(),
                                    destRect.TopLeft() - tmpRect.TopLeft());

    FilterProcessing::ApplyMorphologyHorizontal(
      sourceData, sourceMap.GetStride(), tmpData, tmpMap.GetStride(), tmpRect, rx, aOperator);
  }

  RefPtr<DataSourceSurface> dest;
  if (ry == 0) {
    dest = tmp;
  } else {
    dest = Factory::CreateDataSourceSurface(destRect.Size(), SurfaceFormat::B8G8R8A8);
    if (MOZ2D_WARN_IF(!dest)) {
      return nullptr;
    }

    DataSourceSurface::ScopedMap tmpMap(tmp, DataSourceSurface::READ);
    DataSourceSurface::ScopedMap destMap(dest, DataSourceSurface::WRITE);
    if (MOZ2D_WARN_IF(!tmpMap.IsMapped() || !destMap.IsMapped())) {
      return nullptr;
    }
    int32_t tmpStride = tmpMap.GetStride();
    uint8_t* tmpData = DataAtOffset(tmp, tmpMap.GetMappedSurface(), destRect.TopLeft() - tmpRect.TopLeft());

    int32_t destStride = destMap.GetStride();
    uint8_t* destData = destMap.GetData();

    FilterProcessing::ApplyMorphologyVertical(
      tmpData, tmpStride, destData, destStride, destRect, ry, aOperator);
  }

  return dest.forget();
}

already_AddRefed<DataSourceSurface>
FilterNodeMorphologySoftware::Render(const IntRect& aRect)
{
  IntRect srcRect = aRect;
  srcRect.Inflate(mRadii);

  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_MORPHOLOGY_IN, srcRect, NEED_COLOR_CHANNELS);
  if (!input) {
    return nullptr;
  }

  int32_t rx = mRadii.width;
  int32_t ry = mRadii.height;

  if (rx == 0 && ry == 0) {
    return input.forget();
  }

  return ApplyMorphology(srcRect, input, aRect, rx, ry, mOperator);
}

void
FilterNodeMorphologySoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  IntRect srcRect = aRect;
  srcRect.Inflate(mRadii);
  RequestInputRect(IN_MORPHOLOGY_IN, srcRect);
}

IntRect
FilterNodeMorphologySoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect inflatedSourceRect = aRect;
  inflatedSourceRect.Inflate(mRadii);
  IntRect inputRect = GetInputRectInRect(IN_MORPHOLOGY_IN, inflatedSourceRect);
  if (mOperator == MORPHOLOGY_OPERATOR_ERODE) {
    inputRect.Deflate(mRadii);
  } else {
    inputRect.Inflate(mRadii);
  }
  return inputRect.Intersect(aRect);
}

int32_t
FilterNodeColorMatrixSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_COLOR_MATRIX_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeColorMatrixSoftware::SetAttribute(uint32_t aIndex,
                                            const Matrix5x4 &aMatrix)
{
  MOZ_ASSERT(aIndex == ATT_COLOR_MATRIX_MATRIX);
  mMatrix = aMatrix;
  Invalidate();
}

void
FilterNodeColorMatrixSoftware::SetAttribute(uint32_t aIndex,
                                            uint32_t aAlphaMode)
{
  MOZ_ASSERT(aIndex == ATT_COLOR_MATRIX_ALPHA_MODE);
  mAlphaMode = (AlphaMode)aAlphaMode;
  Invalidate();
}

static already_AddRefed<DataSourceSurface>
Premultiply(DataSourceSurface* aSurface)
{
  if (aSurface->GetFormat() == SurfaceFormat::A8) {
    RefPtr<DataSourceSurface> surface(aSurface);
    return surface.forget();
  }

  IntSize size = aSurface->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
  if (MOZ2D_WARN_IF(!target)) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap inputMap(aSurface, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap targetMap(target, DataSourceSurface::WRITE);
  if (MOZ2D_WARN_IF(!inputMap.IsMapped() || !targetMap.IsMapped())) {
    return nullptr;
  }

  uint8_t* inputData = inputMap.GetData();
  int32_t inputStride = inputMap.GetStride();
  uint8_t* targetData = targetMap.GetData();
  int32_t targetStride = targetMap.GetStride();

  FilterProcessing::DoPremultiplicationCalculation(
    size, targetData, targetStride, inputData, inputStride);

  return target.forget();
}

static already_AddRefed<DataSourceSurface>
Unpremultiply(DataSourceSurface* aSurface)
{
  if (aSurface->GetFormat() == SurfaceFormat::A8) {
    RefPtr<DataSourceSurface> surface(aSurface);
    return surface.forget();
  }

  IntSize size = aSurface->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
  if (MOZ2D_WARN_IF(!target)) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap inputMap(aSurface, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap targetMap(target, DataSourceSurface::WRITE);
  if (MOZ2D_WARN_IF(!inputMap.IsMapped() || !targetMap.IsMapped())) {
    return nullptr;
  }

  uint8_t* inputData = inputMap.GetData();
  int32_t inputStride = inputMap.GetStride();
  uint8_t* targetData = targetMap.GetData();
  int32_t targetStride = targetMap.GetStride();

  FilterProcessing::DoUnpremultiplicationCalculation(
    size, targetData, targetStride, inputData, inputStride);

  return target.forget();
}

already_AddRefed<DataSourceSurface>
FilterNodeColorMatrixSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_COLOR_MATRIX_IN, aRect, NEED_COLOR_CHANNELS);
  if (!input) {
    return nullptr;
  }

  if (mAlphaMode == ALPHA_MODE_PREMULTIPLIED) {
    input = Unpremultiply(input);
  }

  RefPtr<DataSourceSurface> result =
    FilterProcessing::ApplyColorMatrix(input, mMatrix);

  if (mAlphaMode == ALPHA_MODE_PREMULTIPLIED) {
    result = Premultiply(result);
  }

  return result.forget();
}

void
FilterNodeColorMatrixSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_COLOR_MATRIX_IN, aRect);
}

IntRect
FilterNodeColorMatrixSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  if (mMatrix._54 > 0.0f) {
    return aRect;
  }
  return GetInputRectInRect(IN_COLOR_MATRIX_IN, aRect);
}

void
FilterNodeFloodSoftware::SetAttribute(uint32_t aIndex, const Color &aColor)
{
  MOZ_ASSERT(aIndex == ATT_FLOOD_COLOR);
  mColor = aColor;
  Invalidate();
}

static uint32_t
ColorToBGRA(const Color& aColor)
{
  union {
    uint32_t color;
    uint8_t components[4];
  };
  components[B8G8R8A8_COMPONENT_BYTEOFFSET_R] = NS_lround(aColor.r * aColor.a * 255.0f);
  components[B8G8R8A8_COMPONENT_BYTEOFFSET_G] = NS_lround(aColor.g * aColor.a * 255.0f);
  components[B8G8R8A8_COMPONENT_BYTEOFFSET_B] = NS_lround(aColor.b * aColor.a * 255.0f);
  components[B8G8R8A8_COMPONENT_BYTEOFFSET_A] = NS_lround(aColor.a * 255.0f);
  return color;
}

static SurfaceFormat
FormatForColor(Color aColor)
{
  if (aColor.r == 0 && aColor.g == 0 && aColor.b == 0) {
    return SurfaceFormat::A8;
  }
  return SurfaceFormat::B8G8R8A8;
}

already_AddRefed<DataSourceSurface>
FilterNodeFloodSoftware::Render(const IntRect& aRect)
{
  SurfaceFormat format = FormatForColor(mColor);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), format);
  if (MOZ2D_WARN_IF(!target)) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap targetMap(target, DataSourceSurface::WRITE);
  if (MOZ2D_WARN_IF(!targetMap.IsMapped())) {
    return nullptr;
  }

  uint8_t* targetData = targetMap.GetData();
  int32_t stride = targetMap.GetStride();

  if (format == SurfaceFormat::B8G8R8A8) {
    uint32_t color = ColorToBGRA(mColor);
    for (int32_t y = 0; y < aRect.height; y++) {
      for (int32_t x = 0; x < aRect.width; x++) {
        *((uint32_t*)targetData + x) = color;
      }
      targetData += stride;
    }
  } else if (format == SurfaceFormat::A8) {
    uint8_t alpha = NS_lround(mColor.a * 255.0f);
    for (int32_t y = 0; y < aRect.height; y++) {
      for (int32_t x = 0; x < aRect.width; x++) {
        targetData[x] = alpha;
      }
      targetData += stride;
    }
  } else {
    MOZ_CRASH();
  }

  return target.forget();
}

// Override GetOutput to get around caching. Rendering simple floods is
// comparatively fast.
already_AddRefed<DataSourceSurface>
FilterNodeFloodSoftware::GetOutput(const IntRect& aRect)
{
  return Render(aRect);
}

IntRect
FilterNodeFloodSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  if (mColor.a == 0.0f) {
    return IntRect();
  }
  return aRect;
}

int32_t
FilterNodeTileSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_TILE_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeTileSoftware::SetAttribute(uint32_t aIndex,
                                     const IntRect &aSourceRect)
{
  MOZ_ASSERT(aIndex == ATT_TILE_SOURCE_RECT);
  mSourceRect = IntRect(int32_t(aSourceRect.x), int32_t(aSourceRect.y),
                        int32_t(aSourceRect.width), int32_t(aSourceRect.height));
  Invalidate();
}

namespace {
struct CompareIntRects
{
  bool operator()(const IntRect& a, const IntRect& b) const
  {
    if (a.x != b.x) {
      return a.x < b.x;
    }
    if (a.y != b.y) {
      return a.y < b.y;
    }
    if (a.width != b.width) {
      return a.width < b.width;
    }
    return a.height < b.height;
  }
};

} // namespace

already_AddRefed<DataSourceSurface>
FilterNodeTileSoftware::Render(const IntRect& aRect)
{
  if (mSourceRect.IsEmpty()) {
    return nullptr;
  }

  if (mSourceRect.Contains(aRect)) {
    return GetInputDataSourceSurface(IN_TILE_IN, aRect);
  }

  RefPtr<DataSourceSurface> target;

  typedef std::map<IntRect, RefPtr<DataSourceSurface>, CompareIntRects> InputMap;
  InputMap inputs;

  IntPoint startIndex = TileIndex(mSourceRect, aRect.TopLeft());
  IntPoint endIndex = TileIndex(mSourceRect, aRect.BottomRight());
  for (int32_t ix = startIndex.x; ix <= endIndex.x; ix++) {
    for (int32_t iy = startIndex.y; iy <= endIndex.y; iy++) {
      IntPoint sourceToDestOffset(ix * mSourceRect.width,
                                  iy * mSourceRect.height);
      IntRect destRect = aRect.Intersect(mSourceRect + sourceToDestOffset);
      IntRect srcRect = destRect - sourceToDestOffset;
      if (srcRect.IsEmpty()) {
        continue;
      }

      RefPtr<DataSourceSurface> input;
      InputMap::iterator it = inputs.find(srcRect);
      if (it == inputs.end()) {
        input = GetInputDataSourceSurface(IN_TILE_IN, srcRect);
        inputs[srcRect] = input;
      } else {
        input = it->second;
      }
      if (!input) {
        return nullptr;
      }
      if (!target) {
        // We delay creating the target until now because we want to use the
        // same format as our input filter, and we do not actually know the
        // input format before we call GetInputDataSourceSurface.
        target = Factory::CreateDataSourceSurface(aRect.Size(), input->GetFormat());
        if (MOZ2D_WARN_IF(!target)) {
          return nullptr;
        }
      }

      if (input->GetFormat() != target->GetFormat()) {
        // Different rectangles of the input can have different formats. If
        // that happens, just convert everything to B8G8R8A8.
        target = FilterProcessing::ConvertToB8G8R8A8(target);
        input = FilterProcessing::ConvertToB8G8R8A8(input);
        if (MOZ2D_WARN_IF(!target) || MOZ2D_WARN_IF(!input)) {
          return nullptr;
        }
      }

      CopyRect(input, target, srcRect - srcRect.TopLeft(), destRect.TopLeft() - aRect.TopLeft());
    }
  }

  return target.forget();
}

void
FilterNodeTileSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  // Do not request anything.
  // Source rects for the tile filter can be discontinuous with large gaps
  // between them. Requesting those from our input filter might cause it to
  // render the whole bounding box of all of them, which would be wasteful.
}

IntRect
FilterNodeTileSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return aRect;
}

FilterNodeComponentTransferSoftware::FilterNodeComponentTransferSoftware()
 : mDisableR(true)
 , mDisableG(true)
 , mDisableB(true)
 , mDisableA(true)
{}

void
FilterNodeComponentTransferSoftware::SetAttribute(uint32_t aIndex,
                                                  bool aDisable)
{
  switch (aIndex) {
    case ATT_TRANSFER_DISABLE_R:
      mDisableR = aDisable;
      break;
    case ATT_TRANSFER_DISABLE_G:
      mDisableG = aDisable;
      break;
    case ATT_TRANSFER_DISABLE_B:
      mDisableB = aDisable;
      break;
    case ATT_TRANSFER_DISABLE_A:
      mDisableA = aDisable;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

void
FilterNodeComponentTransferSoftware::GenerateLookupTable(ptrdiff_t aComponent,
                                                         uint8_t aTables[4][256],
                                                         bool aDisabled)
{
  if (aDisabled) {
    static uint8_t sIdentityLookupTable[256];
    static bool sInitializedIdentityLookupTable = false;
    if (!sInitializedIdentityLookupTable) {
      for (int32_t i = 0; i < 256; i++) {
        sIdentityLookupTable[i] = i;
      }
      sInitializedIdentityLookupTable = true;
    }
    memcpy(aTables[aComponent], sIdentityLookupTable, 256);
  } else {
    FillLookupTable(aComponent, aTables[aComponent]);
  }
}

template<uint32_t BytesPerPixel>
static void TransferComponents(DataSourceSurface* aInput,
                               DataSourceSurface* aTarget,
                               const uint8_t aLookupTables[BytesPerPixel][256])
{
  MOZ_ASSERT(aInput->GetFormat() == aTarget->GetFormat(), "different formats");
  IntSize size = aInput->GetSize();

  DataSourceSurface::ScopedMap sourceMap(aInput, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap targetMap(aTarget, DataSourceSurface::WRITE);
  if (MOZ2D_WARN_IF(!sourceMap.IsMapped() || !targetMap.IsMapped())) {
    return;
  }

  uint8_t* sourceData = sourceMap.GetData();
  int32_t sourceStride = sourceMap.GetStride();
  uint8_t* targetData = targetMap.GetData();
  int32_t targetStride = targetMap.GetStride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      uint32_t sourceIndex = y * sourceStride + x * BytesPerPixel;
      uint32_t targetIndex = y * targetStride + x * BytesPerPixel;
      for (uint32_t i = 0; i < BytesPerPixel; i++) {
        targetData[targetIndex + i] = aLookupTables[i][sourceData[sourceIndex + i]];
      }
    }
  }
}

bool
IsAllZero(uint8_t aLookupTable[256])
{
  for (int32_t i = 0; i < 256; i++) {
    if (aLookupTable[i] != 0) {
      return false;
    }
  }
  return true;
}

already_AddRefed<DataSourceSurface>
FilterNodeComponentTransferSoftware::Render(const IntRect& aRect)
{
  if (mDisableR && mDisableG && mDisableB && mDisableA) {
    return GetInputDataSourceSurface(IN_TRANSFER_IN, aRect);
  }

  uint8_t lookupTables[4][256];
  GenerateLookupTable(B8G8R8A8_COMPONENT_BYTEOFFSET_R, lookupTables, mDisableR);
  GenerateLookupTable(B8G8R8A8_COMPONENT_BYTEOFFSET_G, lookupTables, mDisableG);
  GenerateLookupTable(B8G8R8A8_COMPONENT_BYTEOFFSET_B, lookupTables, mDisableB);
  GenerateLookupTable(B8G8R8A8_COMPONENT_BYTEOFFSET_A, lookupTables, mDisableA);

  bool needColorChannels =
    lookupTables[B8G8R8A8_COMPONENT_BYTEOFFSET_R][0] != 0 ||
    lookupTables[B8G8R8A8_COMPONENT_BYTEOFFSET_G][0] != 0 ||
    lookupTables[B8G8R8A8_COMPONENT_BYTEOFFSET_B][0] != 0;

  FormatHint pref = needColorChannels ? NEED_COLOR_CHANNELS : CAN_HANDLE_A8;

  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_TRANSFER_IN, aRect, pref);
  if (!input) {
    return nullptr;
  }

  if (input->GetFormat() == SurfaceFormat::B8G8R8A8 && !needColorChannels) {
    bool colorChannelsBecomeBlack =
      IsAllZero(lookupTables[B8G8R8A8_COMPONENT_BYTEOFFSET_R]) &&
      IsAllZero(lookupTables[B8G8R8A8_COMPONENT_BYTEOFFSET_G]) &&
      IsAllZero(lookupTables[B8G8R8A8_COMPONENT_BYTEOFFSET_B]);

    if (colorChannelsBecomeBlack) {
      input = FilterProcessing::ExtractAlpha(input);
    }
  }

  SurfaceFormat format = input->GetFormat();
  if (format == SurfaceFormat::A8 && mDisableA) {
    return input.forget();
  }

  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), format);
  if (MOZ2D_WARN_IF(!target)) {
    return nullptr;
  }

  if (format == SurfaceFormat::A8) {
    TransferComponents<1>(input, target, &lookupTables[B8G8R8A8_COMPONENT_BYTEOFFSET_A]);
  } else {
    TransferComponents<4>(input, target, lookupTables);
  }

  return target.forget();
}

void
FilterNodeComponentTransferSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_TRANSFER_IN, aRect);
}

IntRect
FilterNodeComponentTransferSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  if (mDisableA) {
    return GetInputRectInRect(IN_TRANSFER_IN, aRect);
  }
  return aRect;
}

int32_t
FilterNodeComponentTransferSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_TRANSFER_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeTableTransferSoftware::SetAttribute(uint32_t aIndex,
                                              const Float* aFloat,
                                              uint32_t aSize)
{
  std::vector<Float> table(aFloat, aFloat + aSize);
  switch (aIndex) {
    case ATT_TABLE_TRANSFER_TABLE_R:
      mTableR = table;
      break;
    case ATT_TABLE_TRANSFER_TABLE_G:
      mTableG = table;
      break;
    case ATT_TABLE_TRANSFER_TABLE_B:
      mTableB = table;
      break;
    case ATT_TABLE_TRANSFER_TABLE_A:
      mTableA = table;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

void
FilterNodeTableTransferSoftware::FillLookupTable(ptrdiff_t aComponent,
                                                 uint8_t aTable[256])
{
  switch (aComponent) {
    case B8G8R8A8_COMPONENT_BYTEOFFSET_R:
      FillLookupTableImpl(mTableR, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_G:
      FillLookupTableImpl(mTableG, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_B:
      FillLookupTableImpl(mTableB, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_A:
      FillLookupTableImpl(mTableA, aTable);
      break;
    default:
      MOZ_ASSERT(false, "unknown component");
      break;
  }
}

void
FilterNodeTableTransferSoftware::FillLookupTableImpl(std::vector<Float>& aTableValues,
                                                     uint8_t aTable[256])
{
  uint32_t tvLength = aTableValues.size();
  if (tvLength < 2) {
    return;
  }

  for (size_t i = 0; i < 256; i++) {
    uint32_t k = (i * (tvLength - 1)) / 255;
    Float v1 = aTableValues[k];
    Float v2 = aTableValues[std::min(k + 1, tvLength - 1)];
    int32_t val =
      int32_t(255 * (v1 + (i/255.0f - k/float(tvLength-1))*(tvLength - 1)*(v2 - v1)));
    val = std::min(255, val);
    val = std::max(0, val);
    aTable[i] = val;
  }
}

void
FilterNodeDiscreteTransferSoftware::SetAttribute(uint32_t aIndex,
                                              const Float* aFloat,
                                              uint32_t aSize)
{
  std::vector<Float> discrete(aFloat, aFloat + aSize);
  switch (aIndex) {
    case ATT_DISCRETE_TRANSFER_TABLE_R:
      mTableR = discrete;
      break;
    case ATT_DISCRETE_TRANSFER_TABLE_G:
      mTableG = discrete;
      break;
    case ATT_DISCRETE_TRANSFER_TABLE_B:
      mTableB = discrete;
      break;
    case ATT_DISCRETE_TRANSFER_TABLE_A:
      mTableA = discrete;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

void
FilterNodeDiscreteTransferSoftware::FillLookupTable(ptrdiff_t aComponent,
                                                    uint8_t aTable[256])
{
  switch (aComponent) {
    case B8G8R8A8_COMPONENT_BYTEOFFSET_R:
      FillLookupTableImpl(mTableR, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_G:
      FillLookupTableImpl(mTableG, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_B:
      FillLookupTableImpl(mTableB, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_A:
      FillLookupTableImpl(mTableA, aTable);
      break;
    default:
      MOZ_ASSERT(false, "unknown component");
      break;
  }
}

void
FilterNodeDiscreteTransferSoftware::FillLookupTableImpl(std::vector<Float>& aTableValues,
                                                        uint8_t aTable[256])
{
  uint32_t tvLength = aTableValues.size();
  if (tvLength < 1) {
    return;
  }

  for (size_t i = 0; i < 256; i++) {
    uint32_t k = (i * tvLength) / 255;
    k = std::min(k, tvLength - 1);
    Float v = aTableValues[k];
    int32_t val = NS_lround(255 * v);
    val = std::min(255, val);
    val = std::max(0, val);
    aTable[i] = val;
  }
}

FilterNodeLinearTransferSoftware::FilterNodeLinearTransferSoftware()
 : mSlopeR(0)
 , mSlopeG(0)
 , mSlopeB(0)
 , mSlopeA(0)
 , mInterceptR(0)
 , mInterceptG(0)
 , mInterceptB(0)
 , mInterceptA(0)
{}

void
FilterNodeLinearTransferSoftware::SetAttribute(uint32_t aIndex,
                                               Float aValue)
{
  switch (aIndex) {
    case ATT_LINEAR_TRANSFER_SLOPE_R:
      mSlopeR = aValue;
      break;
    case ATT_LINEAR_TRANSFER_INTERCEPT_R:
      mInterceptR = aValue;
      break;
    case ATT_LINEAR_TRANSFER_SLOPE_G:
      mSlopeG = aValue;
      break;
    case ATT_LINEAR_TRANSFER_INTERCEPT_G:
      mInterceptG = aValue;
      break;
    case ATT_LINEAR_TRANSFER_SLOPE_B:
      mSlopeB = aValue;
      break;
    case ATT_LINEAR_TRANSFER_INTERCEPT_B:
      mInterceptB = aValue;
      break;
    case ATT_LINEAR_TRANSFER_SLOPE_A:
      mSlopeA = aValue;
      break;
    case ATT_LINEAR_TRANSFER_INTERCEPT_A:
      mInterceptA = aValue;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

void
FilterNodeLinearTransferSoftware::FillLookupTable(ptrdiff_t aComponent,
                                                  uint8_t aTable[256])
{
  switch (aComponent) {
    case B8G8R8A8_COMPONENT_BYTEOFFSET_R:
      FillLookupTableImpl(mSlopeR, mInterceptR, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_G:
      FillLookupTableImpl(mSlopeG, mInterceptG, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_B:
      FillLookupTableImpl(mSlopeB, mInterceptB, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_A:
      FillLookupTableImpl(mSlopeA, mInterceptA, aTable);
      break;
    default:
      MOZ_ASSERT(false, "unknown component");
      break;
  }
}

void
FilterNodeLinearTransferSoftware::FillLookupTableImpl(Float aSlope,
                                                      Float aIntercept,
                                                      uint8_t aTable[256])
{
  for (size_t i = 0; i < 256; i++) {
    int32_t val = NS_lround(aSlope * i + 255 * aIntercept);
    val = std::min(255, val);
    val = std::max(0, val);
    aTable[i] = val;
  }
}

FilterNodeGammaTransferSoftware::FilterNodeGammaTransferSoftware()
 : mAmplitudeR(0)
 , mAmplitudeG(0)
 , mAmplitudeB(0)
 , mAmplitudeA(0)
 , mExponentR(0)
 , mExponentG(0)
 , mExponentB(0)
 , mExponentA(0)
{}

void
FilterNodeGammaTransferSoftware::SetAttribute(uint32_t aIndex,
                                              Float aValue)
{
  switch (aIndex) {
    case ATT_GAMMA_TRANSFER_AMPLITUDE_R:
      mAmplitudeR = aValue;
      break;
    case ATT_GAMMA_TRANSFER_EXPONENT_R:
      mExponentR = aValue;
      break;
    case ATT_GAMMA_TRANSFER_OFFSET_R:
      mOffsetR = aValue;
      break;
    case ATT_GAMMA_TRANSFER_AMPLITUDE_G:
      mAmplitudeG = aValue;
      break;
    case ATT_GAMMA_TRANSFER_EXPONENT_G:
      mExponentG = aValue;
      break;
    case ATT_GAMMA_TRANSFER_OFFSET_G:
      mOffsetG = aValue;
      break;
    case ATT_GAMMA_TRANSFER_AMPLITUDE_B:
      mAmplitudeB = aValue;
      break;
    case ATT_GAMMA_TRANSFER_EXPONENT_B:
      mExponentB = aValue;
      break;
    case ATT_GAMMA_TRANSFER_OFFSET_B:
      mOffsetB = aValue;
      break;
    case ATT_GAMMA_TRANSFER_AMPLITUDE_A:
      mAmplitudeA = aValue;
      break;
    case ATT_GAMMA_TRANSFER_EXPONENT_A:
      mExponentA = aValue;
      break;
    case ATT_GAMMA_TRANSFER_OFFSET_A:
      mOffsetA = aValue;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

void
FilterNodeGammaTransferSoftware::FillLookupTable(ptrdiff_t aComponent,
                                                 uint8_t aTable[256])
{
  switch (aComponent) {
    case B8G8R8A8_COMPONENT_BYTEOFFSET_R:
      FillLookupTableImpl(mAmplitudeR, mExponentR, mOffsetR, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_G:
      FillLookupTableImpl(mAmplitudeG, mExponentG, mOffsetG, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_B:
      FillLookupTableImpl(mAmplitudeB, mExponentB, mOffsetB, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_A:
      FillLookupTableImpl(mAmplitudeA, mExponentA, mOffsetA, aTable);
      break;
    default:
      MOZ_ASSERT(false, "unknown component");
      break;
  }
}

void
FilterNodeGammaTransferSoftware::FillLookupTableImpl(Float aAmplitude,
                                                     Float aExponent,
                                                     Float aOffset,
                                                     uint8_t aTable[256])
{
  for (size_t i = 0; i < 256; i++) {
    int32_t val = NS_lround(255 * (aAmplitude * pow(i / 255.0f, aExponent) + aOffset));
    val = std::min(255, val);
    val = std::max(0, val);
    aTable[i] = val;
  }
}

FilterNodeConvolveMatrixSoftware::FilterNodeConvolveMatrixSoftware()
 : mDivisor(0)
 , mBias(0)
 , mEdgeMode(EDGE_MODE_DUPLICATE)
 , mPreserveAlpha(false)
{}

int32_t
FilterNodeConvolveMatrixSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_CONVOLVE_MATRIX_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               const IntSize &aKernelSize)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_KERNEL_SIZE);
  mKernelSize = aKernelSize;
  Invalidate();
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               const Float *aMatrix,
                                               uint32_t aSize)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_KERNEL_MATRIX);
  mKernelMatrix = std::vector<Float>(aMatrix, aMatrix + aSize);
  Invalidate();
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_CONVOLVE_MATRIX_DIVISOR:
      mDivisor = aValue;
      break;
    case ATT_CONVOLVE_MATRIX_BIAS:
      mBias = aValue;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex, const Size &aKernelUnitLength)
{
  switch (aIndex) {
    case ATT_CONVOLVE_MATRIX_KERNEL_UNIT_LENGTH:
      mKernelUnitLength = aKernelUnitLength;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               const IntPoint &aTarget)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_TARGET);
  mTarget = aTarget;
  Invalidate();
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               const IntRect &aSourceRect)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_SOURCE_RECT);
  mSourceRect = aSourceRect;
  Invalidate();
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               uint32_t aEdgeMode)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_EDGE_MODE);
  mEdgeMode = static_cast<ConvolveMatrixEdgeMode>(aEdgeMode);
  Invalidate();
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               bool aPreserveAlpha)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_PRESERVE_ALPHA);
  mPreserveAlpha = aPreserveAlpha;
  Invalidate();
}

#ifdef DEBUG
static bool sColorSamplingAccessControlEnabled = false;
static uint8_t* sColorSamplingAccessControlStart = nullptr;
static uint8_t* sColorSamplingAccessControlEnd = nullptr;

struct DebugOnlyAutoColorSamplingAccessControl
{
  explicit DebugOnlyAutoColorSamplingAccessControl(DataSourceSurface* aSurface)
  {
    sColorSamplingAccessControlStart = aSurface->GetData();
    sColorSamplingAccessControlEnd = sColorSamplingAccessControlStart +
      aSurface->Stride() * aSurface->GetSize().height;
    sColorSamplingAccessControlEnabled = true;
  }

  ~DebugOnlyAutoColorSamplingAccessControl()
  {
    sColorSamplingAccessControlEnabled = false;
  }
};

static inline void
DebugOnlyCheckColorSamplingAccess(const uint8_t* aSampleAddress)
{
  if (sColorSamplingAccessControlEnabled) {
    MOZ_ASSERT(aSampleAddress >= sColorSamplingAccessControlStart, "accessing before start");
    MOZ_ASSERT(aSampleAddress < sColorSamplingAccessControlEnd, "accessing after end");
  }
}
#else
typedef DebugOnly<DataSourceSurface*> DebugOnlyAutoColorSamplingAccessControl;
#define DebugOnlyCheckColorSamplingAccess(address)
#endif

static inline uint8_t
ColorComponentAtPoint(const uint8_t *aData, int32_t aStride, int32_t x, int32_t y, size_t bpp, ptrdiff_t c)
{
  DebugOnlyCheckColorSamplingAccess(&aData[y * aStride + bpp * x + c]);
  return aData[y * aStride + bpp * x + c];
}

static inline int32_t
ColorAtPoint(const uint8_t *aData, int32_t aStride, int32_t x, int32_t y)
{
  DebugOnlyCheckColorSamplingAccess(aData + y * aStride + 4 * x);
  return *(uint32_t*)(aData + y * aStride + 4 * x);
}

// Accepts fractional x & y and does bilinear interpolation.
// Only call this if the pixel (floor(x)+1, floor(y)+1) is accessible.
static inline uint8_t
ColorComponentAtPoint(const uint8_t *aData, int32_t aStride, Float x, Float y, size_t bpp, ptrdiff_t c)
{
  const uint32_t f = 256;
  const int32_t lx = floor(x);
  const int32_t ly = floor(y);
  const int32_t tux = uint32_t((x - lx) * f);
  const int32_t tlx = f - tux;
  const int32_t tuy = uint32_t((y - ly) * f);
  const int32_t tly = f - tuy;
  const uint8_t &cll = ColorComponentAtPoint(aData, aStride, lx,     ly,     bpp, c);
  const uint8_t &cul = ColorComponentAtPoint(aData, aStride, lx + 1, ly,     bpp, c);
  const uint8_t &clu = ColorComponentAtPoint(aData, aStride, lx,     ly + 1, bpp, c);
  const uint8_t &cuu = ColorComponentAtPoint(aData, aStride, lx + 1, ly + 1, bpp, c);
  return ((cll * tlx + cul * tux) * tly +
          (clu * tlx + cuu * tux) * tuy + f * f / 2) / (f * f);
}

static int32_t
ClampToNonZero(int32_t a)
{
  return a * (a >= 0);
}

template<typename CoordType>
static void
ConvolvePixel(const uint8_t *aSourceData,
              uint8_t *aTargetData,
              int32_t aWidth, int32_t aHeight,
              int32_t aSourceStride, int32_t aTargetStride,
              int32_t aX, int32_t aY,
              const int32_t *aKernel,
              int32_t aBias, int32_t shiftL, int32_t shiftR,
              bool aPreserveAlpha,
              int32_t aOrderX, int32_t aOrderY,
              int32_t aTargetX, int32_t aTargetY,
              CoordType aKernelUnitLengthX,
              CoordType aKernelUnitLengthY)
{
  int32_t sum[4] = {0, 0, 0, 0};
  int32_t offsets[4] = { B8G8R8A8_COMPONENT_BYTEOFFSET_R,
                         B8G8R8A8_COMPONENT_BYTEOFFSET_G,
                         B8G8R8A8_COMPONENT_BYTEOFFSET_B,
                         B8G8R8A8_COMPONENT_BYTEOFFSET_A };
  int32_t channels = aPreserveAlpha ? 3 : 4;
  int32_t roundingAddition = shiftL == 0 ? 0 : 1 << (shiftL - 1);

  for (int32_t y = 0; y < aOrderY; y++) {
    CoordType sampleY = aY + (y - aTargetY) * aKernelUnitLengthY;
    for (int32_t x = 0; x < aOrderX; x++) {
      CoordType sampleX = aX + (x - aTargetX) * aKernelUnitLengthX;
      for (int32_t i = 0; i < channels; i++) {
        sum[i] += aKernel[aOrderX * y + x] *
          ColorComponentAtPoint(aSourceData, aSourceStride,
                                sampleX, sampleY, 4, offsets[i]);
      }
    }
  }
  for (int32_t i = 0; i < channels; i++) {
    int32_t clamped = umin(ClampToNonZero(sum[i] + aBias), 255 << shiftL >> shiftR);
    aTargetData[aY * aTargetStride + 4 * aX + offsets[i]] =
      (clamped + roundingAddition) << shiftR >> shiftL;
  }
  if (aPreserveAlpha) {
    aTargetData[aY * aTargetStride + 4 * aX + B8G8R8A8_COMPONENT_BYTEOFFSET_A] =
      aSourceData[aY * aSourceStride + 4 * aX + B8G8R8A8_COMPONENT_BYTEOFFSET_A];
  }
}

already_AddRefed<DataSourceSurface>
FilterNodeConvolveMatrixSoftware::Render(const IntRect& aRect)
{
  if (mKernelUnitLength.width == floor(mKernelUnitLength.width) &&
      mKernelUnitLength.height == floor(mKernelUnitLength.height)) {
    return DoRender(aRect, (int32_t)mKernelUnitLength.width, (int32_t)mKernelUnitLength.height);
  }
  return DoRender(aRect, mKernelUnitLength.width, mKernelUnitLength.height);
}

static std::vector<Float>
ReversedVector(const std::vector<Float> &aVector)
{
  size_t length = aVector.size();
  std::vector<Float> result(length, 0);
  for (size_t i = 0; i < length; i++) {
    result[length - 1 - i] = aVector[i];
  }
  return result;
}

static std::vector<Float>
ScaledVector(const std::vector<Float> &aVector, Float aDivisor)
{
  size_t length = aVector.size();
  std::vector<Float> result(length, 0);
  for (size_t i = 0; i < length; i++) {
    result[i] = aVector[i] / aDivisor;
  }
  return result;
}

static Float
MaxVectorSum(const std::vector<Float> &aVector)
{
  Float sum = 0;
  size_t length = aVector.size();
  for (size_t i = 0; i < length; i++) {
    if (aVector[i] > 0) {
      sum += aVector[i];
    }
  }
  return sum;
}

// Returns shiftL and shiftR in such a way that
// a << shiftL >> shiftR is roughly a * aFloat.
static void
TranslateDoubleToShifts(double aDouble, int32_t &aShiftL, int32_t &aShiftR)
{
  aShiftL = 0;
  aShiftR = 0;
  if (aDouble <= 0) {
    MOZ_CRASH();
  }
  if (aDouble < 1) {
    while (1 << (aShiftR + 1) < 1 / aDouble) {
      aShiftR++;
    }
  } else {
    while (1 << (aShiftL + 1) < aDouble) {
      aShiftL++;
    }
  }
}

template<typename CoordType>
already_AddRefed<DataSourceSurface>
FilterNodeConvolveMatrixSoftware::DoRender(const IntRect& aRect,
                                           CoordType aKernelUnitLengthX,
                                           CoordType aKernelUnitLengthY)
{
  if (mKernelSize.width <= 0 || mKernelSize.height <= 0 ||
      mKernelMatrix.size() != uint32_t(mKernelSize.width * mKernelSize.height) ||
      !IntRect(IntPoint(0, 0), mKernelSize).Contains(mTarget) ||
      mDivisor == 0) {
    return Factory::CreateDataSourceSurface(aRect.Size(), SurfaceFormat::B8G8R8A8);
  }

  IntRect srcRect = InflatedSourceRect(aRect);

  // Inflate the source rect by another pixel because the bilinear filtering in
  // ColorComponentAtPoint may want to access the margins.
  srcRect.Inflate(1);

  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_CONVOLVE_MATRIX_IN, srcRect, NEED_COLOR_CHANNELS, mEdgeMode, &mSourceRect);

  if (!input) {
    return nullptr;
  }

  DebugOnlyAutoColorSamplingAccessControl accessControl(input);

  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), SurfaceFormat::B8G8R8A8, true);
  if (MOZ2D_WARN_IF(!target)) {
    return nullptr;
  }

  IntPoint offset = aRect.TopLeft() - srcRect.TopLeft();

  DataSourceSurface::ScopedMap sourceMap(input, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap targetMap(target, DataSourceSurface::WRITE);
  if (MOZ2D_WARN_IF(!sourceMap.IsMapped() || !targetMap.IsMapped())) {
    return nullptr;
  }

  uint8_t* sourceData = DataAtOffset(input, sourceMap.GetMappedSurface(), offset);
  int32_t sourceStride = sourceMap.GetStride();
  uint8_t* targetData = targetMap.GetData();
  int32_t targetStride = targetMap.GetStride();

  // Why exactly are we reversing the kernel?
  std::vector<Float> kernel = ReversedVector(mKernelMatrix);
  kernel = ScaledVector(kernel, mDivisor);
  Float maxResultAbs = std::max(MaxVectorSum(kernel) + mBias,
                                MaxVectorSum(ScaledVector(kernel, -1)) - mBias);
  maxResultAbs = std::max(maxResultAbs, 1.0f);

  double idealFactor = INT32_MAX / 2.0 / maxResultAbs / 255.0 * 0.999;
  MOZ_ASSERT(255.0 * maxResultAbs * idealFactor <= INT32_MAX / 2.0, "badly chosen float-to-int scale");
  int32_t shiftL, shiftR;
  TranslateDoubleToShifts(idealFactor, shiftL, shiftR);
  double factorFromShifts = Float(1 << shiftL) / Float(1 << shiftR);
  MOZ_ASSERT(255.0 * maxResultAbs * factorFromShifts <= INT32_MAX / 2.0, "badly chosen float-to-int scale");

  int32_t* intKernel = new int32_t[kernel.size()];
  for (size_t i = 0; i < kernel.size(); i++) {
    intKernel[i] = NS_lround(kernel[i] * factorFromShifts);
  }
  int32_t bias = NS_lround(mBias * 255 * factorFromShifts);

  for (int32_t y = 0; y < aRect.height; y++) {
    for (int32_t x = 0; x < aRect.width; x++) {
      ConvolvePixel(sourceData, targetData,
                    aRect.width, aRect.height, sourceStride, targetStride,
                    x, y, intKernel, bias, shiftL, shiftR, mPreserveAlpha,
                    mKernelSize.width, mKernelSize.height, mTarget.x, mTarget.y,
                    aKernelUnitLengthX, aKernelUnitLengthY);
    }
  }
  delete[] intKernel;

  return target.forget();
}

void
FilterNodeConvolveMatrixSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_CONVOLVE_MATRIX_IN, InflatedSourceRect(aRect));
}

IntRect
FilterNodeConvolveMatrixSoftware::InflatedSourceRect(const IntRect &aDestRect)
{
  if (aDestRect.IsEmpty()) {
    return IntRect();
  }

  IntMargin margin;
  margin.left = ceil(mTarget.x * mKernelUnitLength.width);
  margin.top = ceil(mTarget.y * mKernelUnitLength.height);
  margin.right = ceil((mKernelSize.width - mTarget.x - 1) * mKernelUnitLength.width);
  margin.bottom = ceil((mKernelSize.height - mTarget.y - 1) * mKernelUnitLength.height);

  IntRect srcRect = aDestRect;
  srcRect.Inflate(margin);
  return srcRect;
}

IntRect
FilterNodeConvolveMatrixSoftware::InflatedDestRect(const IntRect &aSourceRect)
{
  if (aSourceRect.IsEmpty()) {
    return IntRect();
  }

  IntMargin margin;
  margin.left = ceil((mKernelSize.width - mTarget.x - 1) * mKernelUnitLength.width);
  margin.top = ceil((mKernelSize.height - mTarget.y - 1) * mKernelUnitLength.height);
  margin.right = ceil(mTarget.x * mKernelUnitLength.width);
  margin.bottom = ceil(mTarget.y * mKernelUnitLength.height);

  IntRect destRect = aSourceRect;
  destRect.Inflate(margin);
  return destRect;
}

IntRect
FilterNodeConvolveMatrixSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect srcRequest = InflatedSourceRect(aRect);
  IntRect srcOutput = GetInputRectInRect(IN_COLOR_MATRIX_IN, srcRequest);
  return InflatedDestRect(srcOutput).Intersect(aRect);
}

FilterNodeDisplacementMapSoftware::FilterNodeDisplacementMapSoftware()
 : mScale(0.0f)
 , mChannelX(COLOR_CHANNEL_R)
 , mChannelY(COLOR_CHANNEL_G)
{}

int32_t
FilterNodeDisplacementMapSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_DISPLACEMENT_MAP_IN: return 0;
    case IN_DISPLACEMENT_MAP_IN2: return 1;
    default: return -1;
  }
}

void
FilterNodeDisplacementMapSoftware::SetAttribute(uint32_t aIndex,
                                                Float aScale)
{
  MOZ_ASSERT(aIndex == ATT_DISPLACEMENT_MAP_SCALE);
  mScale = aScale;
  Invalidate();
}

void
FilterNodeDisplacementMapSoftware::SetAttribute(uint32_t aIndex, uint32_t aValue)
{
  switch (aIndex) {
    case ATT_DISPLACEMENT_MAP_X_CHANNEL:
      mChannelX = static_cast<ColorChannel>(aValue);
      break;
    case ATT_DISPLACEMENT_MAP_Y_CHANNEL:
      mChannelY = static_cast<ColorChannel>(aValue);
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

already_AddRefed<DataSourceSurface>
FilterNodeDisplacementMapSoftware::Render(const IntRect& aRect)
{
  IntRect srcRect = InflatedSourceOrDestRect(aRect);
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_DISPLACEMENT_MAP_IN, srcRect, NEED_COLOR_CHANNELS);
  RefPtr<DataSourceSurface> map =
    GetInputDataSourceSurface(IN_DISPLACEMENT_MAP_IN2, aRect, NEED_COLOR_CHANNELS);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), SurfaceFormat::B8G8R8A8);
  if (MOZ2D_WARN_IF(!(input && map && target))) {
    return nullptr;
  }

  IntPoint offset = aRect.TopLeft() - srcRect.TopLeft();

  DataSourceSurface::ScopedMap inputMap(input, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap mapMap(map, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap targetMap(target, DataSourceSurface::WRITE);
  if (MOZ2D_WARN_IF(!(inputMap.IsMapped() && mapMap.IsMapped() && targetMap.IsMapped()))) {
    return nullptr;
  }

  uint8_t* sourceData = DataAtOffset(input, inputMap.GetMappedSurface(), offset);
  int32_t sourceStride = inputMap.GetStride();
  uint8_t* mapData = mapMap.GetData();
  int32_t mapStride = mapMap.GetStride();
  uint8_t* targetData = targetMap.GetData();
  int32_t targetStride = targetMap.GetStride();

  static const ptrdiff_t channelMap[4] = {
                             B8G8R8A8_COMPONENT_BYTEOFFSET_R,
                             B8G8R8A8_COMPONENT_BYTEOFFSET_G,
                             B8G8R8A8_COMPONENT_BYTEOFFSET_B,
                             B8G8R8A8_COMPONENT_BYTEOFFSET_A };
  uint16_t xChannel = channelMap[mChannelX];
  uint16_t yChannel = channelMap[mChannelY];

  float scaleOver255 = mScale / 255.0f;
  float scaleAdjustment = -0.5f * mScale;

  for (int32_t y = 0; y < aRect.height; y++) {
    for (int32_t x = 0; x < aRect.width; x++) {
      uint32_t mapIndex = y * mapStride + 4 * x;
      uint32_t targIndex = y * targetStride + 4 * x;
      int32_t sourceX = x +
        scaleOver255 * mapData[mapIndex + xChannel] + scaleAdjustment;
      int32_t sourceY = y +
        scaleOver255 * mapData[mapIndex + yChannel] + scaleAdjustment;
      *(uint32_t*)(targetData + targIndex) =
        ColorAtPoint(sourceData, sourceStride, sourceX, sourceY);
    }
  }

  return target.forget();
}

void
FilterNodeDisplacementMapSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_DISPLACEMENT_MAP_IN, InflatedSourceOrDestRect(aRect));
  RequestInputRect(IN_DISPLACEMENT_MAP_IN2, aRect);
}

IntRect
FilterNodeDisplacementMapSoftware::InflatedSourceOrDestRect(const IntRect &aDestOrSourceRect)
{
  IntRect sourceOrDestRect = aDestOrSourceRect;
  sourceOrDestRect.Inflate(ceil(fabs(mScale) / 2));
  return sourceOrDestRect;
}

IntRect
FilterNodeDisplacementMapSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect srcRequest = InflatedSourceOrDestRect(aRect);
  IntRect srcOutput = GetInputRectInRect(IN_DISPLACEMENT_MAP_IN, srcRequest);
  return InflatedSourceOrDestRect(srcOutput).Intersect(aRect);
}

FilterNodeTurbulenceSoftware::FilterNodeTurbulenceSoftware()
 : mNumOctaves(0)
 , mSeed(0)
 , mStitchable(false)
 , mType(TURBULENCE_TYPE_TURBULENCE)
{}

int32_t
FilterNodeTurbulenceSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  return -1;
}

void
FilterNodeTurbulenceSoftware::SetAttribute(uint32_t aIndex, const Size &aBaseFrequency)
{
  switch (aIndex) {
    case ATT_TURBULENCE_BASE_FREQUENCY:
      mBaseFrequency = aBaseFrequency;
      break;
    default:
      MOZ_CRASH();
      break;
  }
  Invalidate();
}

void
FilterNodeTurbulenceSoftware::SetAttribute(uint32_t aIndex, const IntRect &aRect)
{
  switch (aIndex) {
    case ATT_TURBULENCE_RECT:
      mRenderRect = aRect;
      break;
    default:
      MOZ_CRASH();
      break;
  }
  Invalidate();
}

void
FilterNodeTurbulenceSoftware::SetAttribute(uint32_t aIndex, bool aStitchable)
{
  MOZ_ASSERT(aIndex == ATT_TURBULENCE_STITCHABLE);
  mStitchable = aStitchable;
  Invalidate();
}

void
FilterNodeTurbulenceSoftware::SetAttribute(uint32_t aIndex, uint32_t aValue)
{
  switch (aIndex) {
    case ATT_TURBULENCE_NUM_OCTAVES:
      mNumOctaves = aValue;
      break;
    case ATT_TURBULENCE_SEED:
      mSeed = aValue;
      break;
    case ATT_TURBULENCE_TYPE:
      mType = static_cast<TurbulenceType>(aValue);
      break;
    default:
      MOZ_CRASH();
      break;
  }
  Invalidate();
}

already_AddRefed<DataSourceSurface>
FilterNodeTurbulenceSoftware::Render(const IntRect& aRect)
{
  return FilterProcessing::RenderTurbulence(
    aRect.Size(), aRect.TopLeft(), mBaseFrequency,
    mSeed, mNumOctaves, mType, mStitchable, Rect(mRenderRect));
}

IntRect
FilterNodeTurbulenceSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return aRect.Intersect(mRenderRect);
}

FilterNodeArithmeticCombineSoftware::FilterNodeArithmeticCombineSoftware()
 : mK1(0), mK2(0), mK3(0), mK4(0)
{
}

int32_t
FilterNodeArithmeticCombineSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_ARITHMETIC_COMBINE_IN: return 0;
    case IN_ARITHMETIC_COMBINE_IN2: return 1;
    default: return -1;
  }
}

void
FilterNodeArithmeticCombineSoftware::SetAttribute(uint32_t aIndex,
                                                  const Float* aFloat,
                                                  uint32_t aSize)
{
  MOZ_ASSERT(aIndex == ATT_ARITHMETIC_COMBINE_COEFFICIENTS);
  MOZ_ASSERT(aSize == 4);

  mK1 = aFloat[0];
  mK2 = aFloat[1];
  mK3 = aFloat[2];
  mK4 = aFloat[3];

  Invalidate();
}

already_AddRefed<DataSourceSurface>
FilterNodeArithmeticCombineSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input1 =
    GetInputDataSourceSurface(IN_ARITHMETIC_COMBINE_IN, aRect, NEED_COLOR_CHANNELS);
  RefPtr<DataSourceSurface> input2 =
    GetInputDataSourceSurface(IN_ARITHMETIC_COMBINE_IN2, aRect, NEED_COLOR_CHANNELS);
  if (!input1 && !input2) {
    return nullptr;
  }

  // If one input is null, treat it as transparent by adjusting the factors.
  Float k1 = mK1, k2 = mK2, k3 = mK3, k4 = mK4;
  if (!input1) {
    k1 = 0.0f;
    k2 = 0.0f;
    input1 = input2;
  }

  if (!input2) {
    k1 = 0.0f;
    k3 = 0.0f;
    input2 = input1;
  }

  return FilterProcessing::ApplyArithmeticCombine(input1, input2, k1, k2, k3, k4);
}

void
FilterNodeArithmeticCombineSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_ARITHMETIC_COMBINE_IN, aRect);
  RequestInputRect(IN_ARITHMETIC_COMBINE_IN2, aRect);
}

IntRect
FilterNodeArithmeticCombineSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  if (mK4 > 0.0f) {
    return aRect;
  }
  IntRect rectFrom1 = GetInputRectInRect(IN_ARITHMETIC_COMBINE_IN, aRect).Intersect(aRect);
  IntRect rectFrom2 = GetInputRectInRect(IN_ARITHMETIC_COMBINE_IN2, aRect).Intersect(aRect);
  IntRect result;
  if (mK1 > 0.0f) {
    result = rectFrom1.Intersect(rectFrom2);
  }
  if (mK2 > 0.0f) {
    result = result.Union(rectFrom1);
  }
  if (mK3 > 0.0f) {
    result = result.Union(rectFrom2);
  }
  return result;
}

FilterNodeCompositeSoftware::FilterNodeCompositeSoftware()
 : mOperator(COMPOSITE_OPERATOR_OVER)
{}

int32_t
FilterNodeCompositeSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  return aInputEnumIndex - IN_COMPOSITE_IN_START;
}

void
FilterNodeCompositeSoftware::SetAttribute(uint32_t aIndex, uint32_t aCompositeOperator)
{
  MOZ_ASSERT(aIndex == ATT_COMPOSITE_OPERATOR);
  mOperator = static_cast<CompositeOperator>(aCompositeOperator);
  Invalidate();
}

already_AddRefed<DataSourceSurface>
FilterNodeCompositeSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> start =
    GetInputDataSourceSurface(IN_COMPOSITE_IN_START, aRect, NEED_COLOR_CHANNELS);
  RefPtr<DataSourceSurface> dest =
    Factory::CreateDataSourceSurface(aRect.Size(), SurfaceFormat::B8G8R8A8, true);
  if (MOZ2D_WARN_IF(!dest)) {
    return nullptr;
  }

  if (start) {
    CopyRect(start, dest, aRect - aRect.TopLeft(), IntPoint());
  }

  for (size_t inputIndex = 1; inputIndex < NumberOfSetInputs(); inputIndex++) {
    RefPtr<DataSourceSurface> input =
      GetInputDataSourceSurface(IN_COMPOSITE_IN_START + inputIndex, aRect, NEED_COLOR_CHANNELS);
    if (input) {
      FilterProcessing::ApplyComposition(input, dest, mOperator);
    } else {
      // We need to treat input as transparent. Depending on the composite
      // operator, different things happen to dest.
      switch (mOperator) {
        case COMPOSITE_OPERATOR_OVER:
        case COMPOSITE_OPERATOR_ATOP:
        case COMPOSITE_OPERATOR_XOR:
          // dest is unchanged.
          break;
        case COMPOSITE_OPERATOR_OUT:
          // dest is now transparent, but it can become non-transparent again
          // when compositing additional inputs.
          ClearDataSourceSurface(dest);
          break;
        case COMPOSITE_OPERATOR_IN:
          // Transparency always wins. We're completely transparent now and
          // no additional input can get rid of that transparency.
          return nullptr;
      }
    }
  }
  return dest.forget();
}

void
FilterNodeCompositeSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  for (size_t inputIndex = 0; inputIndex < NumberOfSetInputs(); inputIndex++) {
    RequestInputRect(IN_COMPOSITE_IN_START + inputIndex, aRect);
  }
}

IntRect
FilterNodeCompositeSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect rect;
  for (size_t inputIndex = 0; inputIndex < NumberOfSetInputs(); inputIndex++) {
    IntRect inputRect = GetInputRectInRect(IN_COMPOSITE_IN_START + inputIndex, aRect);
    if (mOperator == COMPOSITE_OPERATOR_IN && inputIndex > 0) {
      rect = rect.Intersect(inputRect);
    } else {
      rect = rect.Union(inputRect);
    }
  }
  return rect;
}

int32_t
FilterNodeBlurXYSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_GAUSSIAN_BLUR_IN: return 0;
    default: return -1;
  }
}

already_AddRefed<DataSourceSurface>
FilterNodeBlurXYSoftware::Render(const IntRect& aRect)
{
  Size sigmaXY = StdDeviationXY();
  IntSize d = AlphaBoxBlur::CalculateBlurRadius(Point(sigmaXY.width, sigmaXY.height));

  if (d.width == 0 && d.height == 0) {
    return GetInputDataSourceSurface(IN_GAUSSIAN_BLUR_IN, aRect);
  }

  IntRect srcRect = InflatedSourceOrDestRect(aRect);
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_GAUSSIAN_BLUR_IN, srcRect);
  if (!input) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> target;
  Rect r(0, 0, srcRect.width, srcRect.height);

  if (input->GetFormat() == SurfaceFormat::A8) {
    target = Factory::CreateDataSourceSurface(srcRect.Size(), SurfaceFormat::A8);
    if (MOZ2D_WARN_IF(!target)) {
      return nullptr;
    }
    CopyRect(input, target, IntRect(IntPoint(), input->GetSize()), IntPoint());

    DataSourceSurface::ScopedMap targetMap(target, DataSourceSurface::READ_WRITE);
    if (MOZ2D_WARN_IF(!targetMap.IsMapped())) {
      return nullptr;
    }
    AlphaBoxBlur blur(r, targetMap.GetStride(), sigmaXY.width, sigmaXY.height);
    blur.Blur(targetMap.GetData());
  } else {
    RefPtr<DataSourceSurface> channel0, channel1, channel2, channel3;
    FilterProcessing::SeparateColorChannels(input, channel0, channel1, channel2, channel3);
    if (MOZ2D_WARN_IF(!(channel0 && channel1 && channel2 && channel3))) {
      return nullptr;
    }
    {
      DataSourceSurface::ScopedMap channel0Map(channel0, DataSourceSurface::READ_WRITE);
      DataSourceSurface::ScopedMap channel1Map(channel1, DataSourceSurface::READ_WRITE);
      DataSourceSurface::ScopedMap channel2Map(channel2, DataSourceSurface::READ_WRITE);
      DataSourceSurface::ScopedMap channel3Map(channel3, DataSourceSurface::READ_WRITE);
      if (MOZ2D_WARN_IF(!(channel0Map.IsMapped() && channel1Map.IsMapped() &&
                          channel2Map.IsMapped() && channel3Map.IsMapped()))) {
        return nullptr;
      }

      AlphaBoxBlur blur(r, channel0Map.GetStride(), sigmaXY.width, sigmaXY.height);
      blur.Blur(channel0Map.GetData());
      blur.Blur(channel1Map.GetData());
      blur.Blur(channel2Map.GetData());
      blur.Blur(channel3Map.GetData());
    }
    target = FilterProcessing::CombineColorChannels(channel0, channel1, channel2, channel3);
  }

  return GetDataSurfaceInRect(target, srcRect, aRect, EDGE_MODE_NONE);
}

void
FilterNodeBlurXYSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_GAUSSIAN_BLUR_IN, InflatedSourceOrDestRect(aRect));
}

IntRect
FilterNodeBlurXYSoftware::InflatedSourceOrDestRect(const IntRect &aDestRect)
{
  Size sigmaXY = StdDeviationXY();
  IntSize d = AlphaBoxBlur::CalculateBlurRadius(Point(sigmaXY.width, sigmaXY.height));
  IntRect srcRect = aDestRect;
  srcRect.Inflate(d);
  return srcRect;
}

IntRect
FilterNodeBlurXYSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect srcRequest = InflatedSourceOrDestRect(aRect);
  IntRect srcOutput = GetInputRectInRect(IN_GAUSSIAN_BLUR_IN, srcRequest);
  return InflatedSourceOrDestRect(srcOutput).Intersect(aRect);
}

FilterNodeGaussianBlurSoftware::FilterNodeGaussianBlurSoftware()
 : mStdDeviation(0)
{}

static float
ClampStdDeviation(float aStdDeviation)
{
  // Cap software blur radius for performance reasons.
  return std::min(std::max(0.0f, aStdDeviation), 100.0f);
}

void
FilterNodeGaussianBlurSoftware::SetAttribute(uint32_t aIndex,
                                             float aStdDeviation)
{
  switch (aIndex) {
    case ATT_GAUSSIAN_BLUR_STD_DEVIATION:
      mStdDeviation = ClampStdDeviation(aStdDeviation);
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

Size
FilterNodeGaussianBlurSoftware::StdDeviationXY()
{
  return Size(mStdDeviation, mStdDeviation);
}

FilterNodeDirectionalBlurSoftware::FilterNodeDirectionalBlurSoftware()
 : mBlurDirection(BLUR_DIRECTION_X)
{}

void
FilterNodeDirectionalBlurSoftware::SetAttribute(uint32_t aIndex,
                                                Float aStdDeviation)
{
  switch (aIndex) {
    case ATT_DIRECTIONAL_BLUR_STD_DEVIATION:
      mStdDeviation = ClampStdDeviation(aStdDeviation);
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

void
FilterNodeDirectionalBlurSoftware::SetAttribute(uint32_t aIndex,
                                                uint32_t aBlurDirection)
{
  switch (aIndex) {
    case ATT_DIRECTIONAL_BLUR_DIRECTION:
      mBlurDirection = (BlurDirection)aBlurDirection;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

Size
FilterNodeDirectionalBlurSoftware::StdDeviationXY()
{
  float sigmaX = mBlurDirection == BLUR_DIRECTION_X ? mStdDeviation : 0;
  float sigmaY = mBlurDirection == BLUR_DIRECTION_Y ? mStdDeviation : 0;
  return Size(sigmaX, sigmaY);
}

int32_t
FilterNodeCropSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_CROP_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeCropSoftware::SetAttribute(uint32_t aIndex,
                                     const Rect &aSourceRect)
{
  MOZ_ASSERT(aIndex == ATT_CROP_RECT);
  Rect srcRect = aSourceRect;
  srcRect.Round();
  if (!srcRect.ToIntRect(&mCropRect)) {
    mCropRect = IntRect();
  }
  Invalidate();
}

already_AddRefed<DataSourceSurface>
FilterNodeCropSoftware::Render(const IntRect& aRect)
{
  return GetInputDataSourceSurface(IN_CROP_IN, aRect.Intersect(mCropRect));
}

void
FilterNodeCropSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_CROP_IN, aRect.Intersect(mCropRect));
}

IntRect
FilterNodeCropSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_CROP_IN, aRect).Intersect(mCropRect);
}

int32_t
FilterNodePremultiplySoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_PREMULTIPLY_IN: return 0;
    default: return -1;
  }
}

already_AddRefed<DataSourceSurface>
FilterNodePremultiplySoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_PREMULTIPLY_IN, aRect);
  return input ? Premultiply(input) : nullptr;
}

void
FilterNodePremultiplySoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_PREMULTIPLY_IN, aRect);
}

IntRect
FilterNodePremultiplySoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_PREMULTIPLY_IN, aRect);
}

int32_t
FilterNodeUnpremultiplySoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_UNPREMULTIPLY_IN: return 0;
    default: return -1;
  }
}

already_AddRefed<DataSourceSurface>
FilterNodeUnpremultiplySoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_UNPREMULTIPLY_IN, aRect);
  return input ? Unpremultiply(input) : nullptr;
}

void
FilterNodeUnpremultiplySoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_UNPREMULTIPLY_IN, aRect);
}

IntRect
FilterNodeUnpremultiplySoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_UNPREMULTIPLY_IN, aRect);
}

bool
PointLightSoftware::SetAttribute(uint32_t aIndex, const Point3D &aPoint)
{
  switch (aIndex) {
    case ATT_POINT_LIGHT_POSITION:
      mPosition = aPoint;
      break;
    default:
      return false;
  }
  return true;
}

SpotLightSoftware::SpotLightSoftware()
 : mSpecularFocus(0)
 , mLimitingConeAngle(0)
 , mLimitingConeCos(1)
{
}

bool
SpotLightSoftware::SetAttribute(uint32_t aIndex, const Point3D &aPoint)
{
  switch (aIndex) {
    case ATT_SPOT_LIGHT_POSITION:
      mPosition = aPoint;
      break;
    case ATT_SPOT_LIGHT_POINTS_AT:
      mPointsAt = aPoint;
      break;
    default:
      return false;
  }
  return true;
}

bool
SpotLightSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_SPOT_LIGHT_LIMITING_CONE_ANGLE:
      mLimitingConeAngle = aValue;
      break;
    case ATT_SPOT_LIGHT_FOCUS:
      mSpecularFocus = aValue;
      break;
    default:
      return false;
  }
  return true;
}

DistantLightSoftware::DistantLightSoftware()
 : mAzimuth(0)
 , mElevation(0)
{
}

bool
DistantLightSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_DISTANT_LIGHT_AZIMUTH:
      mAzimuth = aValue;
      break;
    case ATT_DISTANT_LIGHT_ELEVATION:
      mElevation = aValue;
      break;
    default:
      return false;
  }
  return true;
}

static inline Point3D Normalized(const Point3D &vec) {
  Point3D copy(vec);
  copy.Normalize();
  return copy;
}

template<typename LightType, typename LightingType>
FilterNodeLightingSoftware<LightType, LightingType>::FilterNodeLightingSoftware(const char* aTypeName)
 : mSurfaceScale(0)
#if defined(MOZILLA_INTERNAL_API) && (defined(DEBUG) || defined(FORCE_BUILD_REFCNT_LOGGING))
 , mTypeName(aTypeName)
#endif
{}

template<typename LightType, typename LightingType>
int32_t
FilterNodeLightingSoftware<LightType, LightingType>::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_LIGHTING_IN: return 0;
    default: return -1;
  }
}

template<typename LightType, typename LightingType>
void
FilterNodeLightingSoftware<LightType, LightingType>::SetAttribute(uint32_t aIndex, const Point3D &aPoint)
{
  if (mLight.SetAttribute(aIndex, aPoint)) {
    Invalidate();
    return;
  }
  MOZ_CRASH();
}

template<typename LightType, typename LightingType>
void
FilterNodeLightingSoftware<LightType, LightingType>::SetAttribute(uint32_t aIndex, Float aValue)
{
  if (mLight.SetAttribute(aIndex, aValue) ||
      mLighting.SetAttribute(aIndex, aValue)) {
    Invalidate();
    return;
  }
  switch (aIndex) {
    case ATT_LIGHTING_SURFACE_SCALE:
      mSurfaceScale = aValue;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

template<typename LightType, typename LightingType>
void
FilterNodeLightingSoftware<LightType, LightingType>::SetAttribute(uint32_t aIndex, const Size &aKernelUnitLength)
{
  switch (aIndex) {
    case ATT_LIGHTING_KERNEL_UNIT_LENGTH:
      mKernelUnitLength = aKernelUnitLength;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

template<typename LightType, typename LightingType>
void
FilterNodeLightingSoftware<LightType, LightingType>::SetAttribute(uint32_t aIndex, const Color &aColor)
{
  MOZ_ASSERT(aIndex == ATT_LIGHTING_COLOR);
  mColor = aColor;
  Invalidate();
}

template<typename LightType, typename LightingType>
IntRect
FilterNodeLightingSoftware<LightType, LightingType>::GetOutputRectInRect(const IntRect& aRect)
{
  return aRect;
}

Point3D
PointLightSoftware::GetVectorToLight(const Point3D &aTargetPoint)
{
  return Normalized(mPosition - aTargetPoint);
}

uint32_t
PointLightSoftware::GetColor(uint32_t aLightColor, const Point3D &aVectorToLight)
{
  return aLightColor;
}

void
SpotLightSoftware::Prepare()
{
  mVectorFromFocusPointToLight = Normalized(mPointsAt - mPosition);
  mLimitingConeCos = std::max<double>(cos(mLimitingConeAngle * M_PI/180.0), 0.0);
  mPowCache.CacheForExponent(mSpecularFocus);
}

Point3D
SpotLightSoftware::GetVectorToLight(const Point3D &aTargetPoint)
{
  return Normalized(mPosition - aTargetPoint);
}

uint32_t
SpotLightSoftware::GetColor(uint32_t aLightColor, const Point3D &aVectorToLight)
{
  union {
    uint32_t color;
    uint8_t colorC[4];
  };
  color = aLightColor;
  Float dot = -aVectorToLight.DotProduct(mVectorFromFocusPointToLight);
  uint16_t doti = dot * (dot >= 0) * (1 << PowCache::sInputIntPrecisionBits);
  uint32_t tmp = mPowCache.Pow(doti) * (dot >= mLimitingConeCos);
  MOZ_ASSERT(tmp <= (1 << PowCache::sOutputIntPrecisionBits), "pow() result must not exceed 1.0");
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_R] = uint8_t((colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_R] * tmp) >> PowCache::sOutputIntPrecisionBits);
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_G] = uint8_t((colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_G] * tmp) >> PowCache::sOutputIntPrecisionBits);
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_B] = uint8_t((colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_B] * tmp) >> PowCache::sOutputIntPrecisionBits);
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_A] = 255;
  return color;
}

void
DistantLightSoftware::Prepare()
{
  const double radPerDeg = M_PI / 180.0;
  mVectorToLight.x = cos(mAzimuth * radPerDeg) * cos(mElevation * radPerDeg);
  mVectorToLight.y = sin(mAzimuth * radPerDeg) * cos(mElevation * radPerDeg);
  mVectorToLight.z = sin(mElevation * radPerDeg);
}

Point3D
DistantLightSoftware::GetVectorToLight(const Point3D &aTargetPoint)
{
  return mVectorToLight;
}

uint32_t
DistantLightSoftware::GetColor(uint32_t aLightColor, const Point3D &aVectorToLight)
{
  return aLightColor;
}

template<typename CoordType>
static Point3D
GenerateNormal(const uint8_t *data, int32_t stride,
               int32_t x, int32_t y, float surfaceScale,
               CoordType dx, CoordType dy)
{
  const uint8_t *index = data + y * stride + x;

  CoordType zero = 0;

  // See this for source of constants:
  //   http://www.w3.org/TR/SVG11/filters.html#feDiffuseLightingElement
  int16_t normalX =
    -1 * ColorComponentAtPoint(index, stride, -dx, -dy, 1, 0) +
     1 * ColorComponentAtPoint(index, stride, dx, -dy, 1, 0) +
    -2 * ColorComponentAtPoint(index, stride, -dx, zero, 1, 0) +
     2 * ColorComponentAtPoint(index, stride, dx, zero, 1, 0) +
    -1 * ColorComponentAtPoint(index, stride, -dx, dy, 1, 0) +
     1 * ColorComponentAtPoint(index, stride, dx, dy, 1, 0);

  int16_t normalY =
    -1 * ColorComponentAtPoint(index, stride, -dx, -dy, 1, 0) +
    -2 * ColorComponentAtPoint(index, stride, zero, -dy, 1, 0) +
    -1 * ColorComponentAtPoint(index, stride, dx, -dy, 1, 0) +
     1 * ColorComponentAtPoint(index, stride, -dx, dy, 1, 0) +
     2 * ColorComponentAtPoint(index, stride, zero, dy, 1, 0) +
     1 * ColorComponentAtPoint(index, stride, dx, dy, 1, 0);

  Point3D normal;
  normal.x = -surfaceScale * normalX / 4.0f;
  normal.y = -surfaceScale * normalY / 4.0f;
  normal.z = 255;
  return Normalized(normal);
}

template<typename LightType, typename LightingType>
already_AddRefed<DataSourceSurface>
FilterNodeLightingSoftware<LightType, LightingType>::Render(const IntRect& aRect)
{
  if (mKernelUnitLength.width == floor(mKernelUnitLength.width) &&
      mKernelUnitLength.height == floor(mKernelUnitLength.height)) {
    return DoRender(aRect, (int32_t)mKernelUnitLength.width, (int32_t)mKernelUnitLength.height);
  }
  return DoRender(aRect, mKernelUnitLength.width, mKernelUnitLength.height);
}

template<typename LightType, typename LightingType>
void
FilterNodeLightingSoftware<LightType, LightingType>::RequestFromInputsForRect(const IntRect &aRect)
{
  IntRect srcRect = aRect;
  srcRect.Inflate(ceil(mKernelUnitLength.width),
                  ceil(mKernelUnitLength.height));
  RequestInputRect(IN_LIGHTING_IN, srcRect);
}

template<typename LightType, typename LightingType> template<typename CoordType>
already_AddRefed<DataSourceSurface>
FilterNodeLightingSoftware<LightType, LightingType>::DoRender(const IntRect& aRect,
                                                              CoordType aKernelUnitLengthX,
                                                              CoordType aKernelUnitLengthY)
{
  IntRect srcRect = aRect;
  IntSize size = aRect.Size();
  srcRect.Inflate(ceil(float(aKernelUnitLengthX)),
                  ceil(float(aKernelUnitLengthY)));

  // Inflate the source rect by another pixel because the bilinear filtering in
  // ColorComponentAtPoint may want to access the margins.
  srcRect.Inflate(1);

  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_LIGHTING_IN, srcRect, CAN_HANDLE_A8,
                              EDGE_MODE_NONE);

  if (!input) {
    return nullptr;
  }

  if (input->GetFormat() != SurfaceFormat::A8) {
    input = FilterProcessing::ExtractAlpha(input);
  }

  DebugOnlyAutoColorSamplingAccessControl accessControl(input);

  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
  if (MOZ2D_WARN_IF(!target)) {
    return nullptr;
  }

  IntPoint offset = aRect.TopLeft() - srcRect.TopLeft();


  DataSourceSurface::ScopedMap sourceMap(input, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap targetMap(target, DataSourceSurface::WRITE);
  if (MOZ2D_WARN_IF(!(sourceMap.IsMapped() && targetMap.IsMapped()))) {
    return nullptr;
  }

  uint8_t* sourceData = DataAtOffset(input, sourceMap.GetMappedSurface(), offset);
  int32_t sourceStride = sourceMap.GetStride();
  uint8_t* targetData = targetMap.GetData();
  int32_t targetStride = targetMap.GetStride();

  uint32_t lightColor = ColorToBGRA(mColor);
  mLight.Prepare();
  mLighting.Prepare();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      int32_t sourceIndex = y * sourceStride + x;
      int32_t targetIndex = y * targetStride + 4 * x;

      Point3D normal = GenerateNormal(sourceData, sourceStride,
                                      x, y, mSurfaceScale,
                                      aKernelUnitLengthX, aKernelUnitLengthY);

      IntPoint pointInFilterSpace(aRect.x + x, aRect.y + y);
      Float Z = mSurfaceScale * sourceData[sourceIndex] / 255.0f;
      Point3D pt(pointInFilterSpace.x, pointInFilterSpace.y, Z);
      Point3D rayDir = mLight.GetVectorToLight(pt);
      uint32_t color = mLight.GetColor(lightColor, rayDir);

      *(uint32_t*)(targetData + targetIndex) = mLighting.LightPixel(normal, rayDir, color);
    }
  }

  return target.forget();
}

DiffuseLightingSoftware::DiffuseLightingSoftware()
 : mDiffuseConstant(0)
{
}

bool
DiffuseLightingSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_DIFFUSE_LIGHTING_DIFFUSE_CONSTANT:
      mDiffuseConstant = aValue;
      break;
    default:
      return false;
  }
  return true;
}

uint32_t
DiffuseLightingSoftware::LightPixel(const Point3D &aNormal,
                                    const Point3D &aVectorToLight,
                                    uint32_t aColor)
{
  Float dotNL = std::max(0.0f, aNormal.DotProduct(aVectorToLight));
  Float diffuseNL = mDiffuseConstant * dotNL;

  union {
    uint32_t bgra;
    uint8_t components[4];
  } color = { aColor };
  color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_B] =
    umin(uint32_t(diffuseNL * color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_B]), 255U);
  color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_G] =
    umin(uint32_t(diffuseNL * color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_G]), 255U);
  color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_R] =
    umin(uint32_t(diffuseNL * color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_R]), 255U);
  color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_A] = 255;
  return color.bgra;
}

SpecularLightingSoftware::SpecularLightingSoftware()
 : mSpecularConstant(0)
 , mSpecularExponent(0)
{
}

bool
SpecularLightingSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_SPECULAR_LIGHTING_SPECULAR_CONSTANT:
      mSpecularConstant = std::min(std::max(aValue, 0.0f), 255.0f);
      break;
    case ATT_SPECULAR_LIGHTING_SPECULAR_EXPONENT:
      mSpecularExponent = std::min(std::max(aValue, 1.0f), 128.0f);
      break;
    default:
      return false;
  }
  return true;
}

void
SpecularLightingSoftware::Prepare()
{
  mPowCache.CacheForExponent(mSpecularExponent);
  mSpecularConstantInt = uint32_t(mSpecularConstant * (1 << 8));
}

uint32_t
SpecularLightingSoftware::LightPixel(const Point3D &aNormal,
                                     const Point3D &aVectorToLight,
                                     uint32_t aColor)
{
  Point3D vectorToEye(0, 0, 1);
  Point3D halfwayVector = Normalized(aVectorToLight + vectorToEye);
  Float dotNH = aNormal.DotProduct(halfwayVector);
  uint16_t dotNHi = uint16_t(dotNH * (dotNH >= 0) * (1 << PowCache::sInputIntPrecisionBits));
  uint32_t specularNHi = uint32_t(mSpecularConstantInt) * mPowCache.Pow(dotNHi) >> 8;

  union {
    uint32_t bgra;
    uint8_t components[4];
  } color = { aColor };
  color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_B] =
    umin(
      (specularNHi * color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_B]) >> PowCache::sOutputIntPrecisionBits, 255U);
  color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_G] =
    umin(
      (specularNHi * color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_G]) >> PowCache::sOutputIntPrecisionBits, 255U);
  color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_R] =
    umin(
      (specularNHi * color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_R]) >> PowCache::sOutputIntPrecisionBits, 255U);

  color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_A] =
    umax(color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_B],
      umax(color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_G],
               color.components[B8G8R8A8_COMPONENT_BYTEOFFSET_R]));
  return color.bgra;
}

} // namespace gfx
} // namespace mozilla
