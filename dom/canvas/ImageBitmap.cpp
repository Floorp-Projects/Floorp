/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/gfx/2D.h"
#include "imgTools.h"
#include "libyuv.h"
#include "nsLayoutUtils.h"

using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace mozilla {
namespace dom {

using namespace workers;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ImageBitmap, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ImageBitmap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImageBitmap)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageBitmap)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/*
 * If either aRect.width or aRect.height are negative, then return a new IntRect
 * which represents the same rectangle as the aRect does but with positive width
 * and height.
 */
static IntRect
FixUpNegativeDimension(const IntRect& aRect, ErrorResult& aRv)
{
  gfx::IntRect rect = aRect;

  // fix up negative dimensions
  if (rect.width < 0) {
    CheckedInt32 checkedX = CheckedInt32(rect.x) + rect.width;

    if (!checkedX.isValid()) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return rect;
    }

    rect.x = checkedX.value();
    rect.width = -(rect.width);
  }

  if (rect.height < 0) {
    CheckedInt32 checkedY = CheckedInt32(rect.y) + rect.height;

    if (!checkedY.isValid()) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return rect;
    }

    rect.y = checkedY.value();
    rect.height = -(rect.height);
  }

  return rect;
}

/*
 * This helper function copies the data of the given DataSourceSurface,
 *  _aSurface_, in the given area, _aCropRect_, into a new DataSourceSurface.
 * This might return null if it can not create a new SourceSurface or it cannot
 * read data from the given _aSurface_.
 *
 * Warning: Even though the area of _aCropRect_ is just the same as the size of
 *          _aSurface_, this function still copy data into a new
 *          DataSourceSurface.
 */
static already_AddRefed<DataSourceSurface>
CropAndCopyDataSourceSurface(DataSourceSurface* aSurface, const IntRect& aCropRect)
{
  MOZ_ASSERT(aSurface);

  // Check the aCropRect
  ErrorResult error;
  const IntRect positiveCropRect = FixUpNegativeDimension(aCropRect, error);
  if (NS_WARN_IF(error.Failed())) {
    return nullptr;
  }

  // Calculate the size of the new SourceSurface.
  // We cannot keep using aSurface->GetFormat() to create new DataSourceSurface,
  // since it might be SurfaceFormat::B8G8R8X8 which does not handle opacity,
  // however the specification explicitly define that "If any of the pixels on
  // this rectangle are outside the area where the input bitmap was placed, then
  // they will be transparent black in output."
  // So, instead, we force the output format to be SurfaceFormat::B8G8R8A8.
  const SurfaceFormat format = SurfaceFormat::B8G8R8A8;
  const int bytesPerPixel = BytesPerPixel(format);
  const IntSize dstSize = IntSize(positiveCropRect.width,
                                  positiveCropRect.height);
  const uint32_t dstStride = dstSize.width * bytesPerPixel;

  // Create a new SourceSurface.
  RefPtr<DataSourceSurface> dstDataSurface =
    Factory::CreateDataSourceSurfaceWithStride(dstSize, format, dstStride, true);

  if (NS_WARN_IF(!dstDataSurface)) {
    return nullptr;
  }

  // Only do copying and cropping when the positiveCropRect intersects with
  // the size of aSurface.
  const IntRect surfRect(IntPoint(0, 0), aSurface->GetSize());
  if (surfRect.Intersects(positiveCropRect)) {
    const IntRect surfPortion = surfRect.Intersect(positiveCropRect);
    const IntPoint dest(std::max(0, surfPortion.X() - positiveCropRect.X()),
                        std::max(0, surfPortion.Y() - positiveCropRect.Y()));

    // Copy the raw data into the newly created DataSourceSurface.
    DataSourceSurface::ScopedMap srcMap(aSurface, DataSourceSurface::READ);
    DataSourceSurface::ScopedMap dstMap(dstDataSurface, DataSourceSurface::WRITE);
    if (NS_WARN_IF(!srcMap.IsMapped()) ||
        NS_WARN_IF(!dstMap.IsMapped())) {
      return nullptr;
    }

    uint8_t* srcBufferPtr = srcMap.GetData() + surfPortion.y * srcMap.GetStride()
                                             + surfPortion.x * bytesPerPixel;
    uint8_t* dstBufferPtr = dstMap.GetData() + dest.y * dstMap.GetStride()
                                             + dest.x * bytesPerPixel;
    const uint32_t copiedBytesPerRaw = surfPortion.width * bytesPerPixel;

    for (int i = 0; i < surfPortion.height; ++i) {
      memcpy(dstBufferPtr, srcBufferPtr, copiedBytesPerRaw);
      srcBufferPtr += srcMap.GetStride();
      dstBufferPtr += dstMap.GetStride();
    }
  }

  return dstDataSurface.forget();
}

/*
 * Encapsulate the given _aSurface_ into a layers::CairoImage.
 */
static already_AddRefed<layers::Image>
CreateImageFromSurface(SourceSurface* aSurface)
{
  MOZ_ASSERT(aSurface);
  RefPtr<layers::CairoImage> image =
    new layers::CairoImage(aSurface->GetSize(), aSurface);

  return image.forget();
}

/*
 * CreateImageFromRawData(), CreateSurfaceFromRawData() and
 * CreateImageFromRawDataInMainThreadSyncTask are helpers for
 * create-from-ImageData case
 */
static already_AddRefed<SourceSurface>
CreateSurfaceFromRawData(const gfx::IntSize& aSize,
                         uint32_t aStride,
                         gfx::SurfaceFormat aFormat,
                         uint8_t* aBuffer,
                         uint32_t aBufferLength,
                         const Maybe<IntRect>& aCropRect)
{
  MOZ_ASSERT(!aSize.IsEmpty());
  MOZ_ASSERT(aBuffer);

  // Wrap the source buffer into a SourceSurface.
  RefPtr<DataSourceSurface> dataSurface =
    Factory::CreateWrappingDataSourceSurface(aBuffer, aStride, aSize, aFormat);

  if (NS_WARN_IF(!dataSurface)) {
    return nullptr;
  }

  // The temporary cropRect variable is equal to the size of source buffer if we
  // do not need to crop, or it equals to the given cropping size.
  const IntRect cropRect = aCropRect.valueOr(IntRect(0, 0, aSize.width, aSize.height));

  // Copy the source buffer in the _cropRect_ area into a new SourceSurface.
  RefPtr<DataSourceSurface> result = CropAndCopyDataSourceSurface(dataSurface, cropRect);

  if (NS_WARN_IF(!result)) {
    return nullptr;
  }

  return result.forget();
}

static already_AddRefed<layers::Image>
CreateImageFromRawData(const gfx::IntSize& aSize,
                       uint32_t aStride,
                       gfx::SurfaceFormat aFormat,
                       uint8_t* aBuffer,
                       uint32_t aBufferLength,
                       const Maybe<IntRect>& aCropRect)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Copy and crop the source buffer into a SourceSurface.
  RefPtr<SourceSurface> rgbaSurface =
    CreateSurfaceFromRawData(aSize, aStride, aFormat,
                             aBuffer, aBufferLength,
                             aCropRect);

  if (NS_WARN_IF(!rgbaSurface)) {
    return nullptr;
  }

  // Convert RGBA to BGRA
  RefPtr<DataSourceSurface> rgbaDataSurface = rgbaSurface->GetDataSurface();
  RefPtr<DataSourceSurface> bgraDataSurface =
    Factory::CreateDataSourceSurfaceWithStride(rgbaDataSurface->GetSize(),
                                               SurfaceFormat::B8G8R8A8,
                                               rgbaDataSurface->Stride());

  DataSourceSurface::MappedSurface rgbaMap;
  DataSourceSurface::MappedSurface bgraMap;

  if (NS_WARN_IF(!rgbaDataSurface->Map(DataSourceSurface::MapType::READ, &rgbaMap)) ||
      NS_WARN_IF(!bgraDataSurface->Map(DataSourceSurface::MapType::WRITE, &bgraMap))) {
    return nullptr;
  }

  libyuv::ABGRToARGB(rgbaMap.mData, rgbaMap.mStride,
                     bgraMap.mData, bgraMap.mStride,
                     bgraDataSurface->GetSize().width,
                     bgraDataSurface->GetSize().height);

  rgbaDataSurface->Unmap();
  bgraDataSurface->Unmap();

  // Create an Image from the BGRA SourceSurface.
  RefPtr<layers::Image> image = CreateImageFromSurface(bgraDataSurface);

  if (NS_WARN_IF(!image)) {
    return nullptr;
  }

  return image.forget();
}

/*
 * This is a synchronous task.
 * This class is used to create a layers::CairoImage from raw data in the main
 * thread. While creating an ImageBitmap from an ImageData, we need to create
 * a SouceSurface from the ImageData's raw data and then set the SourceSurface
 * into a layers::CairoImage. However, the layers::CairoImage asserts the
 * setting operation in the main thread, so if we are going to create an
 * ImageBitmap from an ImageData off the main thread, we post an event to the
 * main thread to create a layers::CairoImage from an ImageData's raw data.
 */
class CreateImageFromRawDataInMainThreadSyncTask final :
  public WorkerMainThreadRunnable
{
public:
  CreateImageFromRawDataInMainThreadSyncTask(uint8_t* aBuffer,
                                             uint32_t aBufferLength,
                                             uint32_t aStride,
                                             gfx::SurfaceFormat aFormat,
                                             const gfx::IntSize& aSize,
                                             const Maybe<IntRect>& aCropRect,
                                             layers::Image** aImage)
  : WorkerMainThreadRunnable(GetCurrentThreadWorkerPrivate())
  , mImage(aImage)
  , mBuffer(aBuffer)
  , mBufferLength(aBufferLength)
  , mStride(aStride)
  , mFormat(aFormat)
  , mSize(aSize)
  , mCropRect(aCropRect)
  {
    MOZ_ASSERT(!(*aImage), "Don't pass an existing Image into CreateImageFromRawDataInMainThreadSyncTask.");
  }

  bool MainThreadRun() override
  {
    RefPtr<layers::Image> image =
      CreateImageFromRawData(mSize, mStride, mFormat,
                             mBuffer, mBufferLength,
                             mCropRect);

    if (NS_WARN_IF(!image)) {
      return false;
    }

    image.forget(mImage);

    return true;
  }

private:
  layers::Image** mImage;
  uint8_t* mBuffer;
  uint32_t mBufferLength;
  uint32_t mStride;
  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;
  const Maybe<IntRect>& mCropRect;
};

static bool
CheckSecurityForHTMLElements(bool aIsWriteOnly, bool aCORSUsed, nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(aPrincipal);

  if (aIsWriteOnly) {
    return false;
  }

  if (!aCORSUsed) {
    nsIGlobalObject* incumbentSettingsObject = GetIncumbentGlobal();
    if (NS_WARN_IF(!incumbentSettingsObject)) {
      return false;
    }

    nsIPrincipal* principal = incumbentSettingsObject->PrincipalOrNull();
    if (NS_WARN_IF(!principal) || !(principal->Subsumes(aPrincipal))) {
      return false;
    }
  }

  return true;
}

static bool
CheckSecurityForHTMLElements(const nsLayoutUtils::SurfaceFromElementResult& aRes)
{
  return CheckSecurityForHTMLElements(aRes.mIsWriteOnly, aRes.mCORSUsed, aRes.mPrincipal);
}

/*
 * A wrapper to the nsLayoutUtils::SurfaceFromElement() function followed by the
 * security checking.
 */
template<class HTMLElementType>
static already_AddRefed<SourceSurface>
GetSurfaceFromElement(nsIGlobalObject* aGlobal, HTMLElementType& aElement, ErrorResult& aRv)
{
  nsLayoutUtils::SurfaceFromElementResult res =
    nsLayoutUtils::SurfaceFromElement(&aElement, nsLayoutUtils::SFE_WANT_FIRST_FRAME);

  // check origin-clean
  if (!CheckSecurityForHTMLElements(res)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  RefPtr<SourceSurface> surface = res.GetSourceSurface();

  if (NS_WARN_IF(!surface)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  return surface.forget();
}

/*
 * The specification doesn't allow to create an ImegeBitmap from a vector image.
 * This function is used to check if the given HTMLImageElement contains a
 * raster image.
 */
static bool
HasRasterImage(HTMLImageElement& aImageEl)
{
  nsresult rv;

  nsCOMPtr<imgIRequest> imgRequest;
  rv = aImageEl.GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                           getter_AddRefs(imgRequest));
  if (NS_SUCCEEDED(rv) && imgRequest) {
    nsCOMPtr<imgIContainer> imgContainer;
    rv = imgRequest->GetImage(getter_AddRefs(imgContainer));
    if (NS_SUCCEEDED(rv) && imgContainer &&
        imgContainer->GetType() == imgIContainer::TYPE_RASTER) {
      return true;
    }
  }

  return false;
}

ImageBitmap::ImageBitmap(nsIGlobalObject* aGlobal, layers::Image* aData)
  : mParent(aGlobal)
  , mData(aData)
  , mSurface(nullptr)
  , mPictureRect(0, 0, aData->GetSize().width, aData->GetSize().height)
{
  MOZ_ASSERT(aData, "aData is null in ImageBitmap constructor.");
}

ImageBitmap::~ImageBitmap()
{
}

JSObject*
ImageBitmap::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ImageBitmapBinding::Wrap(aCx, this, aGivenProto);
}

void
ImageBitmap::SetPictureRect(const IntRect& aRect, ErrorResult& aRv)
{
  mPictureRect = FixUpNegativeDimension(aRect, aRv);
}

already_AddRefed<SourceSurface>
ImageBitmap::PrepareForDrawTarget(gfx::DrawTarget* aTarget)
{
  MOZ_ASSERT(aTarget);

  if (!mSurface) {
    mSurface = mData->GetAsSourceSurface();
  }

  if (!mSurface) {
    return nullptr;
  }

  RefPtr<DrawTarget> target = aTarget;
  IntRect surfRect(0, 0, mSurface->GetSize().width, mSurface->GetSize().height);

  // Check if we still need to crop our surface
  if (!mPictureRect.IsEqualEdges(surfRect)) {

    IntRect surfPortion = surfRect.Intersect(mPictureRect);

    // the crop lies entirely outside the surface area, nothing to draw
    if (surfPortion.IsEmpty()) {
      mSurface = nullptr;
      RefPtr<gfx::SourceSurface> surface(mSurface);
      return surface.forget();
    }

    IntPoint dest(std::max(0, surfPortion.X() - mPictureRect.X()),
                  std::max(0, surfPortion.Y() - mPictureRect.Y()));

    // We must initialize this target with mPictureRect.Size() because the
    // specification states that if the cropping area is given, then return an
    // ImageBitmap with the size equals to the cropping area.
    target = target->CreateSimilarDrawTarget(mPictureRect.Size(),
                                             target->GetFormat());

    if (!target) {
      mSurface = nullptr;
      RefPtr<gfx::SourceSurface> surface(mSurface);
      return surface.forget();
    }

    // We need to fall back to generic copying and cropping for the Windows8.1,
    // D2D1 backend.
    // In the Windows8.1 D2D1 backend, it might trigger "partial upload" from a
    // non-SourceSurfaceD2D1 surface to a D2D1Image in the following
    // CopySurface() step. However, the "partial upload" only supports uploading
    // a rectangle starts from the upper-left point, which means it cannot
    // upload an arbitrary part of the source surface and this causes problems
    // if the mPictureRect is not starts from the upper-left point.
    if (target->GetBackendType() == BackendType::DIRECT2D1_1 &&
        mSurface->GetType() != SurfaceType::D2D1_1_IMAGE) {
      RefPtr<DataSourceSurface> dataSurface = mSurface->GetDataSurface();
      if (NS_WARN_IF(!dataSurface)) {
        mSurface = nullptr;
        RefPtr<gfx::SourceSurface> surface(mSurface);
        return surface.forget();
      }

      mSurface = CropAndCopyDataSourceSurface(dataSurface, mPictureRect);
    } else {
      target->CopySurface(mSurface, surfPortion, dest);
      mSurface = target->Snapshot();
    }

    // Make mCropRect match new surface we've cropped to
    mPictureRect.MoveTo(0, 0);
  }

  // Replace our surface with one optimized for the target we're about to draw
  // to, under the assumption it'll likely be drawn again to that target.
  // This call should be a no-op for already-optimized surfaces
  mSurface = target->OptimizeSourceSurface(mSurface);

  RefPtr<gfx::SourceSurface> surface(mSurface);
  return surface.forget();
}

/* static */ already_AddRefed<ImageBitmap>
ImageBitmap::CreateInternal(nsIGlobalObject* aGlobal, HTMLImageElement& aImageEl,
                            const Maybe<IntRect>& aCropRect, ErrorResult& aRv)
{
  // Check if the image element is completely available or not.
  if (!aImageEl.Complete()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Check if the image element is a bitmap (e.g. it's a vector graphic) or not.
  if (!HasRasterImage(aImageEl)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Get the SourceSurface out from the image element and then do security
  // checking.
  RefPtr<SourceSurface> surface = GetSurfaceFromElement(aGlobal, aImageEl, aRv);

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Create ImageBitmap.
  RefPtr<layers::Image> data = CreateImageFromSurface(surface);

  if (NS_WARN_IF(!data)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  RefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, data);

  // Set the picture rectangle.
  if (ret && aCropRect.isSome()) {
    ret->SetPictureRect(aCropRect.ref(), aRv);
  }

  return ret.forget();
}

/* static */ already_AddRefed<ImageBitmap>
ImageBitmap::CreateInternal(nsIGlobalObject* aGlobal, HTMLVideoElement& aVideoEl,
                            const Maybe<IntRect>& aCropRect, ErrorResult& aRv)
{
  // Check network state.
  if (aVideoEl.NetworkState() == HTMLMediaElement::NETWORK_EMPTY) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Check ready state.
  // Cannot be HTMLMediaElement::HAVE_NOTHING or HTMLMediaElement::HAVE_METADATA.
  if (aVideoEl.ReadyState() <= HTMLMediaElement::HAVE_METADATA) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Check security.
  nsCOMPtr<nsIPrincipal> principal = aVideoEl.GetCurrentPrincipal();
  bool CORSUsed = aVideoEl.GetCORSMode() != CORS_NONE;
  if (!CheckSecurityForHTMLElements(false, CORSUsed, principal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Create ImageBitmap.
  ImageContainer *container = aVideoEl.GetImageContainer();

  if (!container) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  AutoLockImage lockImage(container);
  layers::Image* data = lockImage.GetImage();
  RefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, data);

  // Set the picture rectangle.
  if (ret && aCropRect.isSome()) {
    ret->SetPictureRect(aCropRect.ref(), aRv);
  }

  return ret.forget();
}

/* static */ already_AddRefed<ImageBitmap>
ImageBitmap::CreateInternal(nsIGlobalObject* aGlobal, HTMLCanvasElement& aCanvasEl,
                            const Maybe<IntRect>& aCropRect, ErrorResult& aRv)
{
  if (aCanvasEl.Width() == 0 || aCanvasEl.Height() == 0) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  RefPtr<SourceSurface> surface = GetSurfaceFromElement(aGlobal, aCanvasEl, aRv);

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Crop the source surface if needed.
  RefPtr<SourceSurface> croppedSurface;
  IntRect cropRect = aCropRect.valueOr(IntRect());

  // If the HTMLCanvasElement's rendering context is WebGL, then the snapshot
  // we got from the HTMLCanvasElement is a DataSourceSurface which is a copy
  // of the rendering context. We handle cropping in this case.
  if ((aCanvasEl.GetCurrentContextType() == CanvasContextType::WebGL1 ||
       aCanvasEl.GetCurrentContextType() == CanvasContextType::WebGL2) &&
      aCropRect.isSome()) {
    // The _surface_ must be a DataSourceSurface.
    MOZ_ASSERT(surface->GetType() == SurfaceType::DATA,
               "The snapshot SourceSurface from WebGL rendering contest is not \
               DataSourceSurface.");
    RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface();
    croppedSurface = CropAndCopyDataSourceSurface(dataSurface, cropRect);
    cropRect.MoveTo(0, 0);
  }
  else {
    croppedSurface = surface;
  }

  if (NS_WARN_IF(!croppedSurface)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  // Create an Image from the SourceSurface.
  RefPtr<layers::Image> data = CreateImageFromSurface(croppedSurface);

  if (NS_WARN_IF(!data)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  RefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, data);

  // Set the picture rectangle.
  if (ret && aCropRect.isSome()) {
    ret->SetPictureRect(cropRect, aRv);
  }

  return ret.forget();
}

/* static */ already_AddRefed<ImageBitmap>
ImageBitmap::CreateInternal(nsIGlobalObject* aGlobal, ImageData& aImageData,
                            const Maybe<IntRect>& aCropRect, ErrorResult& aRv)
{
  // Copy data into SourceSurface.
  dom::Uint8ClampedArray array;
  DebugOnly<bool> inited = array.Init(aImageData.GetDataObject());
  MOZ_ASSERT(inited);

  array.ComputeLengthAndData();
  const SurfaceFormat FORMAT = SurfaceFormat::R8G8B8A8;
  const uint32_t BYTES_PER_PIXEL = BytesPerPixel(FORMAT);
  const uint32_t imageWidth = aImageData.Width();
  const uint32_t imageHeight = aImageData.Height();
  const uint32_t imageStride = imageWidth * BYTES_PER_PIXEL;
  const uint32_t dataLength = array.Length();
  const gfx::IntSize imageSize(imageWidth, imageHeight);

  // Check the ImageData is neutered or not.
  if (imageWidth == 0 || imageHeight == 0 ||
      (imageWidth * imageHeight * BYTES_PER_PIXEL) != dataLength) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Create and Crop the raw data into a layers::Image
  RefPtr<layers::Image> data;
  if (NS_IsMainThread()) {
    data = CreateImageFromRawData(imageSize, imageStride, FORMAT,
                                  array.Data(), dataLength,
                                  aCropRect);
  } else {
    RefPtr<CreateImageFromRawDataInMainThreadSyncTask> task
      = new CreateImageFromRawDataInMainThreadSyncTask(array.Data(),
                                                       dataLength,
                                                       imageStride,
                                                       FORMAT,
                                                       imageSize,
                                                       aCropRect,
                                                       getter_AddRefs(data));
    task->Dispatch(aRv);
  }

  if (NS_WARN_IF(!data)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  // Create an ImageBimtap.
  RefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, data);

  // The cropping information has been handled in the CreateImageFromRawData()
  // function.

  return ret.forget();
}

/* static */ already_AddRefed<ImageBitmap>
ImageBitmap::CreateInternal(nsIGlobalObject* aGlobal, CanvasRenderingContext2D& aCanvasCtx,
                            const Maybe<IntRect>& aCropRect, ErrorResult& aRv)
{
  // Check origin-clean.
  if (aCanvasCtx.GetCanvas()->IsWriteOnly()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  RefPtr<SourceSurface> surface = aCanvasCtx.GetSurfaceSnapshot();

  if (NS_WARN_IF(!surface)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  const IntSize surfaceSize = surface->GetSize();
  if (surfaceSize.width == 0 || surfaceSize.height == 0) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  RefPtr<layers::Image> data = CreateImageFromSurface(surface);

  if (NS_WARN_IF(!data)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  RefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, data);

  // Set the picture rectangle.
  if (ret && aCropRect.isSome()) {
    ret->SetPictureRect(aCropRect.ref(), aRv);
  }

  return ret.forget();
}

/* static */ already_AddRefed<ImageBitmap>
ImageBitmap::CreateInternal(nsIGlobalObject* aGlobal, ImageBitmap& aImageBitmap,
                            const Maybe<IntRect>& aCropRect, ErrorResult& aRv)
{
  if (!aImageBitmap.mData) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  RefPtr<layers::Image> data = aImageBitmap.mData;
  RefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, data);

  // Set the picture rectangle.
  if (ret && aCropRect.isSome()) {
    ret->SetPictureRect(aCropRect.ref(), aRv);
  }

  return ret.forget();
}

class FulfillImageBitmapPromise
{
protected:
  FulfillImageBitmapPromise(Promise* aPromise, ImageBitmap* aImageBitmap)
  : mPromise(aPromise)
  , mImageBitmap(aImageBitmap)
  {
    MOZ_ASSERT(aPromise);
  }

  void DoFulfillImageBitmapPromise()
  {
    mPromise->MaybeResolve(mImageBitmap);
  }

private:
  RefPtr<Promise> mPromise;
  RefPtr<ImageBitmap> mImageBitmap;
};

class FulfillImageBitmapPromiseTask final : public nsRunnable,
                                            public FulfillImageBitmapPromise
{
public:
  FulfillImageBitmapPromiseTask(Promise* aPromise, ImageBitmap* aImageBitmap)
  : FulfillImageBitmapPromise(aPromise, aImageBitmap)
  {
  }

  NS_IMETHOD Run() override
  {
    DoFulfillImageBitmapPromise();
    return NS_OK;
  }
};

class FulfillImageBitmapPromiseWorkerTask final : public WorkerSameThreadRunnable,
                                                  public FulfillImageBitmapPromise
{
public:
  FulfillImageBitmapPromiseWorkerTask(Promise* aPromise, ImageBitmap* aImageBitmap)
  : WorkerSameThreadRunnable(GetCurrentThreadWorkerPrivate()),
    FulfillImageBitmapPromise(aPromise, aImageBitmap)
  {
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    DoFulfillImageBitmapPromise();
    return true;
  }
};

static void
AsyncFulfillImageBitmapPromise(Promise* aPromise, ImageBitmap* aImageBitmap)
{
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task =
      new FulfillImageBitmapPromiseTask(aPromise, aImageBitmap);
    NS_DispatchToCurrentThread(task); // Actually, to the main-thread.
  } else {
    RefPtr<FulfillImageBitmapPromiseWorkerTask> task =
      new FulfillImageBitmapPromiseWorkerTask(aPromise, aImageBitmap);
    task->Dispatch(GetCurrentThreadWorkerPrivate()->GetJSContext()); // Actually, to the current worker-thread.
  }
}

static already_AddRefed<SourceSurface>
DecodeBlob(Blob& aBlob)
{
  // Get the internal stream of the blob.
  nsCOMPtr<nsIInputStream> stream;
  ErrorResult error;
  aBlob.Impl()->GetInternalStream(getter_AddRefs(stream), error);
  if (NS_WARN_IF(error.Failed())) {
    return nullptr;
  }

  // Get the MIME type string of the blob.
  // The type will be checked in the DecodeImage() method.
  nsAutoString mimeTypeUTF16;
  aBlob.GetType(mimeTypeUTF16);

  // Get the Component object.
  nsCOMPtr<imgITools> imgtool = do_GetService(NS_IMGTOOLS_CID);
  if (NS_WARN_IF(!imgtool)) {
    return nullptr;
  }

  // Decode image.
  NS_ConvertUTF16toUTF8 mimeTypeUTF8(mimeTypeUTF16); // NS_ConvertUTF16toUTF8 ---|> nsAutoCString
  nsCOMPtr<imgIContainer> imgContainer;
  nsresult rv = imgtool->DecodeImage(stream, mimeTypeUTF8, getter_AddRefs(imgContainer));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  // Get the surface out.
  uint32_t frameFlags = imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_WANT_DATA_SURFACE;
  uint32_t whichFrame = imgIContainer::FRAME_FIRST;
  RefPtr<SourceSurface> surface = imgContainer->GetFrame(whichFrame, frameFlags);

  if (NS_WARN_IF(!surface)) {
    return nullptr;
  }

  return surface.forget();
}

static already_AddRefed<layers::Image>
DecodeAndCropBlob(Blob& aBlob, Maybe<IntRect>& aCropRect)
{
  // Decode the blob into a SourceSurface.
  RefPtr<SourceSurface> surface = DecodeBlob(aBlob);

  if (NS_WARN_IF(!surface)) {
    return nullptr;
  }

  // Crop the source surface if needed.
  RefPtr<SourceSurface> croppedSurface = surface;

  if (aCropRect.isSome()) {
    // The blob is just decoded into a RasterImage and not optimized yet, so the
    // _surface_ we get is a DataSourceSurface which wraps the RasterImage's
    // raw buffer.
    //
    // The _surface_ might already be optimized so that its type is not
    // SurfaceType::DATA. However, we could keep using the generic cropping and
    // copying since the decoded buffer is only used in this ImageBitmap so we
    // should crop it to save memory usage.
    //
    // TODO: Bug1189632 is going to refactor this create-from-blob part to
    //       decode the blob off the main thread. Re-check if we should do
    //       cropping at this moment again there.
    RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface();
    croppedSurface = CropAndCopyDataSourceSurface(dataSurface, aCropRect.ref());
    aCropRect->MoveTo(0, 0);
  }

  if (NS_WARN_IF(!croppedSurface)) {
    return nullptr;
  }

  // Create an Image from the source surface.
  RefPtr<layers::Image> image = CreateImageFromSurface(croppedSurface);

  if (NS_WARN_IF(!image)) {
    return nullptr;
  }

  return image.forget();
}

class CreateImageBitmapFromBlob
{
protected:
  CreateImageBitmapFromBlob(Promise* aPromise,
                            nsIGlobalObject* aGlobal,
                            Blob& aBlob,
                            const Maybe<IntRect>& aCropRect)
  : mPromise(aPromise),
    mGlobalObject(aGlobal),
    mBlob(&aBlob),
    mCropRect(aCropRect)
  {
  }

  virtual ~CreateImageBitmapFromBlob()
  {
  }

  // Returns true on success, false on failure.
  bool DoCreateImageBitmapFromBlob()
  {
    RefPtr<ImageBitmap> imageBitmap = CreateImageBitmap();

    // handle errors while creating ImageBitmap
    // (1) error occurs during reading of the object
    // (2) the image data is not in a supported file format
    // (3) the image data is corrupted
    // All these three cases should reject promise with null value
    if (!imageBitmap) {
      return false;
    }

    if (imageBitmap && mCropRect.isSome()) {
      ErrorResult rv;
      imageBitmap->SetPictureRect(mCropRect.ref(), rv);

      if (rv.Failed()) {
        mPromise->MaybeReject(rv);
        return false;
      }
    }

    mPromise->MaybeResolve(imageBitmap);
    return true;
  }

  // Will return null on failure.  In that case, mPromise will already
  // be rejected with the right thing.
  virtual already_AddRefed<ImageBitmap> CreateImageBitmap() = 0;

  RefPtr<Promise> mPromise;
  nsCOMPtr<nsIGlobalObject> mGlobalObject;
  RefPtr<mozilla::dom::Blob> mBlob;
  Maybe<IntRect> mCropRect;
};

class CreateImageBitmapFromBlobTask final : public nsRunnable,
                                            public CreateImageBitmapFromBlob
{
public:
  CreateImageBitmapFromBlobTask(Promise* aPromise,
                                nsIGlobalObject* aGlobal,
                                Blob& aBlob,
                                const Maybe<IntRect>& aCropRect)
  :CreateImageBitmapFromBlob(aPromise, aGlobal, aBlob, aCropRect)
  {
  }

  NS_IMETHOD Run() override
  {
    DoCreateImageBitmapFromBlob();
    return NS_OK;
  }

private:
  already_AddRefed<ImageBitmap> CreateImageBitmap() override
  {
    RefPtr<layers::Image> data = DecodeAndCropBlob(*mBlob, mCropRect);

    if (NS_WARN_IF(!data)) {
      mPromise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
      return nullptr;
    }

    // Create ImageBitmap object.
    RefPtr<ImageBitmap> imageBitmap = new ImageBitmap(mGlobalObject, data);
    return imageBitmap.forget();
  }
};

class CreateImageBitmapFromBlobWorkerTask final : public WorkerSameThreadRunnable,
                                                  public CreateImageBitmapFromBlob
{
  // This is a synchronous task.
  class DecodeBlobInMainThreadSyncTask final : public WorkerMainThreadRunnable
  {
  public:
    DecodeBlobInMainThreadSyncTask(WorkerPrivate* aWorkerPrivate,
                                   Blob& aBlob,
                                   Maybe<IntRect>& aCropRect,
                                   layers::Image** aImage)
    : WorkerMainThreadRunnable(aWorkerPrivate)
    , mBlob(aBlob)
    , mCropRect(aCropRect)
    , mImage(aImage)
    {
    }

    bool MainThreadRun() override
    {
      RefPtr<layers::Image> image = DecodeAndCropBlob(mBlob, mCropRect);

      if (NS_WARN_IF(!image)) {
        return true;
      }

      image.forget(mImage);

      return true;
    }

  private:
    Blob& mBlob;
    Maybe<IntRect>& mCropRect;
    layers::Image** mImage;
  };

public:
  CreateImageBitmapFromBlobWorkerTask(Promise* aPromise,
                                  nsIGlobalObject* aGlobal,
                                  mozilla::dom::Blob& aBlob,
                                  const Maybe<IntRect>& aCropRect)
  : WorkerSameThreadRunnable(GetCurrentThreadWorkerPrivate()),
    CreateImageBitmapFromBlob(aPromise, aGlobal, aBlob, aCropRect)
  {
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return DoCreateImageBitmapFromBlob();
  }

private:
  already_AddRefed<ImageBitmap> CreateImageBitmap() override
  {
    RefPtr<layers::Image> data;

    ErrorResult rv;
    RefPtr<DecodeBlobInMainThreadSyncTask> task =
      new DecodeBlobInMainThreadSyncTask(mWorkerPrivate, *mBlob, mCropRect,
                                         getter_AddRefs(data));
    task->Dispatch(rv); // This is a synchronous call.

    if (NS_WARN_IF(rv.Failed())) {
      // XXXbz does this really make sense if we're shutting down?  Ah, well.
      mPromise->MaybeReject(rv);
      return nullptr;
    }
    
    if (NS_WARN_IF(!data)) {
      mPromise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
      return nullptr;
    }

    // Create ImageBitmap object.
    RefPtr<ImageBitmap> imageBitmap = new ImageBitmap(mGlobalObject, data);
    return imageBitmap.forget();
  }

};

static void
AsyncCreateImageBitmapFromBlob(Promise* aPromise, nsIGlobalObject* aGlobal,
                               Blob& aBlob, const Maybe<IntRect>& aCropRect)
{
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task =
      new CreateImageBitmapFromBlobTask(aPromise, aGlobal, aBlob, aCropRect);
    NS_DispatchToCurrentThread(task); // Actually, to the main-thread.
  } else {
    RefPtr<CreateImageBitmapFromBlobWorkerTask> task =
      new CreateImageBitmapFromBlobWorkerTask(aPromise, aGlobal, aBlob, aCropRect);
    task->Dispatch(GetCurrentThreadWorkerPrivate()->GetJSContext()); // Actually, to the current worker-thread.
  }
}

/* static */ already_AddRefed<Promise>
ImageBitmap::Create(nsIGlobalObject* aGlobal, const ImageBitmapSource& aSrc,
                    const Maybe<gfx::IntRect>& aCropRect, ErrorResult& aRv)
{
  MOZ_ASSERT(aGlobal);

  RefPtr<Promise> promise = Promise::Create(aGlobal, aRv);

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (aCropRect.isSome() && (aCropRect->Width() == 0 || aCropRect->Height() == 0)) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return promise.forget();
  }

  RefPtr<ImageBitmap> imageBitmap;

  if (aSrc.IsHTMLImageElement()) {
    MOZ_ASSERT(NS_IsMainThread(),
               "Creating ImageBitmap from HTMLImageElement off the main thread.");
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsHTMLImageElement(), aCropRect, aRv);
  } else if (aSrc.IsHTMLVideoElement()) {
    MOZ_ASSERT(NS_IsMainThread(),
               "Creating ImageBitmap from HTMLVideoElement off the main thread.");
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsHTMLVideoElement(), aCropRect, aRv);
  } else if (aSrc.IsHTMLCanvasElement()) {
    MOZ_ASSERT(NS_IsMainThread(),
               "Creating ImageBitmap from HTMLCanvasElement off the main thread.");
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsHTMLCanvasElement(), aCropRect, aRv);
  } else if (aSrc.IsImageData()) {
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsImageData(), aCropRect, aRv);
  } else if (aSrc.IsCanvasRenderingContext2D()) {
    MOZ_ASSERT(NS_IsMainThread(),
               "Creating ImageBitmap from CanvasRenderingContext2D off the main thread.");
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsCanvasRenderingContext2D(), aCropRect, aRv);
  } else if (aSrc.IsImageBitmap()) {
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsImageBitmap(), aCropRect, aRv);
  } else if (aSrc.IsBlob()) {
    AsyncCreateImageBitmapFromBlob(promise, aGlobal, aSrc.GetAsBlob(), aCropRect);
    return promise.forget();
  } else {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

  if (!aRv.Failed()) {
    AsyncFulfillImageBitmapPromise(promise, imageBitmap);
  }

  return promise.forget();
}

/*static*/ JSObject*
ImageBitmap::ReadStructuredClone(JSContext* aCx,
                                 JSStructuredCloneReader* aReader,
                                 nsIGlobalObject* aParent,
                                 const nsTArray<RefPtr<layers::Image>>& aClonedImages,
                                 uint32_t aIndex)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aReader);
  // aParent might be null.

  uint32_t picRectX_;
  uint32_t picRectY_;
  uint32_t picRectWidth_;
  uint32_t picRectHeight_;

  if (!JS_ReadUint32Pair(aReader, &picRectX_, &picRectY_) ||
      !JS_ReadUint32Pair(aReader, &picRectWidth_, &picRectHeight_)) {
    return nullptr;
  }

  int32_t picRectX = BitwiseCast<int32_t>(picRectX_);
  int32_t picRectY = BitwiseCast<int32_t>(picRectY_);
  int32_t picRectWidth = BitwiseCast<int32_t>(picRectWidth_);
  int32_t picRectHeight = BitwiseCast<int32_t>(picRectHeight_);

  // Create a new ImageBitmap.
  MOZ_ASSERT(!aClonedImages.IsEmpty());
  MOZ_ASSERT(aIndex < aClonedImages.Length());

  // RefPtr<ImageBitmap> needs to go out of scope before toObjectOrNull() is
  // called because the static analysis thinks dereferencing XPCOM objects
  // can GC (because in some cases it can!), and a return statement with a
  // JSObject* type means that JSObject* is on the stack as a raw pointer
  // while destructors are running.
  JS::Rooted<JS::Value> value(aCx);
  {
    RefPtr<ImageBitmap> imageBitmap =
      new ImageBitmap(aParent, aClonedImages[aIndex]);

    ErrorResult error;
    imageBitmap->SetPictureRect(IntRect(picRectX, picRectY,
                                        picRectWidth, picRectHeight), error);
    if (NS_WARN_IF(error.Failed())) {
      error.SuppressException();
      return nullptr;
    }

    if (!GetOrCreateDOMReflector(aCx, imageBitmap, &value)) {
      return nullptr;
    }
  }

  return &(value.toObject());
}

/*static*/ bool
ImageBitmap::WriteStructuredClone(JSStructuredCloneWriter* aWriter,
                                  nsTArray<RefPtr<layers::Image>>& aClonedImages,
                                  ImageBitmap* aImageBitmap)
{
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aImageBitmap);

  const uint32_t picRectX = BitwiseCast<uint32_t>(aImageBitmap->mPictureRect.x);
  const uint32_t picRectY = BitwiseCast<uint32_t>(aImageBitmap->mPictureRect.y);
  const uint32_t picRectWidth = BitwiseCast<uint32_t>(aImageBitmap->mPictureRect.width);
  const uint32_t picRectHeight = BitwiseCast<uint32_t>(aImageBitmap->mPictureRect.height);

  // Indexing the cloned images and send the index to the receiver.
  uint32_t index = aClonedImages.Length();

  if (NS_WARN_IF(!JS_WriteUint32Pair(aWriter, SCTAG_DOM_IMAGEBITMAP, index)) ||
      NS_WARN_IF(!JS_WriteUint32Pair(aWriter, picRectX, picRectY)) ||
      NS_WARN_IF(!JS_WriteUint32Pair(aWriter, picRectWidth, picRectHeight))) {
    return false;
  }

  aClonedImages.AppendElement(aImageBitmap->mData);

  return true;
}

} // namespace dom
} // namespace mozilla
