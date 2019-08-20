/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/HTMLMediaElementBinding.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/Mutex.h"
#include "mozilla/ScopeExit.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "ImageUtils.h"
#include "imgLoader.h"
#include "imgTools.h"

using namespace mozilla::gfx;
using namespace mozilla::layers;
using mozilla::dom::HTMLMediaElement_Binding::HAVE_METADATA;
using mozilla::dom::HTMLMediaElement_Binding::NETWORK_EMPTY;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ImageBitmap, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ImageBitmap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImageBitmap)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageBitmap)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* This class observes shutdown notifications and sends that notification
 * to the worker thread if the image bitmap is on a worker thread.
 */
class ImageBitmapShutdownObserver final : public nsIObserver {
 public:
  explicit ImageBitmapShutdownObserver(ImageBitmap* aImageBitmap)
      : mImageBitmap(nullptr) {
    if (NS_IsMainThread()) {
      mImageBitmap = aImageBitmap;
    } else {
      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(workerPrivate);
      mMainThreadEventTarget = workerPrivate->MainThreadEventTarget();
      mSendToWorkerTask = new SendShutdownToWorkerThread(aImageBitmap);
    }
  }

  void RegisterObserver() {
    if (NS_IsMainThread()) {
      nsContentUtils::RegisterShutdownObserver(this);
      return;
    }

    MOZ_ASSERT(mMainThreadEventTarget);
    RefPtr<ImageBitmapShutdownObserver> self = this;
    nsCOMPtr<nsIRunnable> r =
        NS_NewRunnableFunction("ImageBitmapShutdownObserver::RegisterObserver",
                               [self]() { self->RegisterObserver(); });

    mMainThreadEventTarget->Dispatch(r.forget());
  }

  void UnregisterObserver() {
    if (NS_IsMainThread()) {
      nsContentUtils::UnregisterShutdownObserver(this);
      return;
    }

    MOZ_ASSERT(mMainThreadEventTarget);
    RefPtr<ImageBitmapShutdownObserver> self = this;
    nsCOMPtr<nsIRunnable> r =
        NS_NewRunnableFunction("ImageBitmapShutdownObserver::RegisterObserver",
                               [self]() { self->UnregisterObserver(); });

    mMainThreadEventTarget->Dispatch(r.forget());
  }

  void Clear() {
    mImageBitmap = nullptr;
    if (mSendToWorkerTask) {
      mSendToWorkerTask->mImageBitmap = nullptr;
    }
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
 private:
  ~ImageBitmapShutdownObserver() {}

  class SendShutdownToWorkerThread : public MainThreadWorkerControlRunnable {
   public:
    explicit SendShutdownToWorkerThread(ImageBitmap* aImageBitmap)
        : MainThreadWorkerControlRunnable(GetCurrentThreadWorkerPrivate()),
          mImageBitmap(aImageBitmap) {}

    bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
      if (mImageBitmap) {
        mImageBitmap->OnShutdown();
        mImageBitmap = nullptr;
      }
      return true;
    }

    ImageBitmap* mImageBitmap;
  };

  ImageBitmap* mImageBitmap;
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;
  RefPtr<SendShutdownToWorkerThread> mSendToWorkerTask;
};

NS_IMPL_ISUPPORTS(ImageBitmapShutdownObserver, nsIObserver)

NS_IMETHODIMP
ImageBitmapShutdownObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                     const char16_t* aData) {
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    if (mSendToWorkerTask) {
      mSendToWorkerTask->Dispatch();
    } else {
      if (mImageBitmap) {
        mImageBitmap->OnShutdown();
        mImageBitmap = nullptr;
      }
    }
    nsContentUtils::UnregisterShutdownObserver(this);
  }

  return NS_OK;
}

/*
 * If either aRect.width or aRect.height are negative, then return a new IntRect
 * which represents the same rectangle as the aRect does but with positive width
 * and height.
 */
static IntRect FixUpNegativeDimension(const IntRect& aRect, ErrorResult& aRv) {
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
static already_AddRefed<DataSourceSurface> CropAndCopyDataSourceSurface(
    DataSourceSurface* aSurface, const IntRect& aCropRect) {
  MOZ_ASSERT(aSurface);

  // Check the aCropRect
  ErrorResult error;
  const IntRect positiveCropRect = FixUpNegativeDimension(aCropRect, error);
  if (NS_WARN_IF(error.Failed())) {
    error.SuppressException();
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
  const IntSize dstSize =
      IntSize(positiveCropRect.width, positiveCropRect.height);
  const uint32_t dstStride = dstSize.width * bytesPerPixel;

  // Create a new SourceSurface.
  RefPtr<DataSourceSurface> dstDataSurface =
      Factory::CreateDataSourceSurfaceWithStride(dstSize, format, dstStride,
                                                 true);

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
    DataSourceSurface::ScopedMap dstMap(dstDataSurface,
                                        DataSourceSurface::WRITE);
    if (NS_WARN_IF(!srcMap.IsMapped()) || NS_WARN_IF(!dstMap.IsMapped())) {
      return nullptr;
    }

    uint8_t* srcBufferPtr = srcMap.GetData() +
                            surfPortion.y * srcMap.GetStride() +
                            surfPortion.x * bytesPerPixel;
    uint8_t* dstBufferPtr =
        dstMap.GetData() + dest.y * dstMap.GetStride() + dest.x * bytesPerPixel;
    CheckedInt<uint32_t> copiedBytesPerRaw =
        CheckedInt<uint32_t>(surfPortion.width) * bytesPerPixel;
    if (!copiedBytesPerRaw.isValid()) {
      return nullptr;
    }

    for (int i = 0; i < surfPortion.height; ++i) {
      memcpy(dstBufferPtr, srcBufferPtr, copiedBytesPerRaw.value());
      srcBufferPtr += srcMap.GetStride();
      dstBufferPtr += dstMap.GetStride();
    }
  }

  return dstDataSurface.forget();
}

/*
 * Encapsulate the given _aSurface_ into a layers::SourceSurfaceImage.
 */
static already_AddRefed<layers::Image> CreateImageFromSurface(
    SourceSurface* aSurface) {
  MOZ_ASSERT(aSurface);
  RefPtr<layers::SourceSurfaceImage> image =
      new layers::SourceSurfaceImage(aSurface->GetSize(), aSurface);
  return image.forget();
}

/*
 * CreateImageFromRawData(), CreateSurfaceFromRawData() and
 * CreateImageFromRawDataInMainThreadSyncTask are helpers for
 * create-from-ImageData case
 */
static already_AddRefed<SourceSurface> CreateSurfaceFromRawData(
    const gfx::IntSize& aSize, uint32_t aStride, gfx::SurfaceFormat aFormat,
    uint8_t* aBuffer, uint32_t aBufferLength, const Maybe<IntRect>& aCropRect) {
  MOZ_ASSERT(!aSize.IsEmpty());
  MOZ_ASSERT(aBuffer);

  // Wrap the source buffer into a SourceSurface.
  RefPtr<DataSourceSurface> dataSurface =
      Factory::CreateWrappingDataSourceSurface(aBuffer, aStride, aSize,
                                               aFormat);

  if (NS_WARN_IF(!dataSurface)) {
    return nullptr;
  }

  // The temporary cropRect variable is equal to the size of source buffer if we
  // do not need to crop, or it equals to the given cropping size.
  const IntRect cropRect =
      aCropRect.valueOr(IntRect(0, 0, aSize.width, aSize.height));

  // Copy the source buffer in the _cropRect_ area into a new SourceSurface.
  RefPtr<DataSourceSurface> result =
      CropAndCopyDataSourceSurface(dataSurface, cropRect);

  if (NS_WARN_IF(!result)) {
    return nullptr;
  }

  return result.forget();
}

static already_AddRefed<layers::Image> CreateImageFromRawData(
    const gfx::IntSize& aSize, uint32_t aStride, gfx::SurfaceFormat aFormat,
    uint8_t* aBuffer, uint32_t aBufferLength, const Maybe<IntRect>& aCropRect) {
  MOZ_ASSERT(NS_IsMainThread());

  // Copy and crop the source buffer into a SourceSurface.
  RefPtr<SourceSurface> rgbaSurface = CreateSurfaceFromRawData(
      aSize, aStride, aFormat, aBuffer, aBufferLength, aCropRect);

  if (NS_WARN_IF(!rgbaSurface)) {
    return nullptr;
  }

  // Convert RGBA to BGRA
  RefPtr<DataSourceSurface> rgbaDataSurface = rgbaSurface->GetDataSurface();
  DataSourceSurface::ScopedMap rgbaMap(rgbaDataSurface,
                                       DataSourceSurface::READ);
  if (NS_WARN_IF(!rgbaMap.IsMapped())) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> bgraDataSurface =
      Factory::CreateDataSourceSurfaceWithStride(rgbaDataSurface->GetSize(),
                                                 SurfaceFormat::B8G8R8A8,
                                                 rgbaMap.GetStride());
  if (NS_WARN_IF(!bgraDataSurface)) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap bgraMap(bgraDataSurface,
                                       DataSourceSurface::WRITE);
  if (NS_WARN_IF(!bgraMap.IsMapped())) {
    return nullptr;
  }

  SwizzleData(rgbaMap.GetData(), rgbaMap.GetStride(), SurfaceFormat::R8G8B8A8,
              bgraMap.GetData(), bgraMap.GetStride(), SurfaceFormat::B8G8R8A8,
              bgraDataSurface->GetSize());

  // Create an Image from the BGRA SourceSurface.
  RefPtr<layers::Image> image = CreateImageFromSurface(bgraDataSurface);

  if (NS_WARN_IF(!image)) {
    return nullptr;
  }

  return image.forget();
}

/*
 * This is a synchronous task.
 * This class is used to create a layers::SourceSurfaceImage from raw data in
 * the main thread. While creating an ImageBitmap from an ImageData, we need to
 * create a SouceSurface from the ImageData's raw data and then set the
 * SourceSurface into a layers::SourceSurfaceImage. However, the
 * layers::SourceSurfaceImage asserts the setting operation in the main thread,
 * so if we are going to create an ImageBitmap from an ImageData off the main
 * thread, we post an event to the main thread to create a
 * layers::SourceSurfaceImage from an ImageData's raw data.
 */
class CreateImageFromRawDataInMainThreadSyncTask final
    : public WorkerMainThreadRunnable {
 public:
  CreateImageFromRawDataInMainThreadSyncTask(
      uint8_t* aBuffer, uint32_t aBufferLength, uint32_t aStride,
      gfx::SurfaceFormat aFormat, const gfx::IntSize& aSize,
      const Maybe<IntRect>& aCropRect, layers::Image** aImage)
      : WorkerMainThreadRunnable(
            GetCurrentThreadWorkerPrivate(),
            NS_LITERAL_CSTRING("ImageBitmap :: Create Image from Raw Data")),
        mImage(aImage),
        mBuffer(aBuffer),
        mBufferLength(aBufferLength),
        mStride(aStride),
        mFormat(aFormat),
        mSize(aSize),
        mCropRect(aCropRect) {
    MOZ_ASSERT(!(*aImage),
               "Don't pass an existing Image into "
               "CreateImageFromRawDataInMainThreadSyncTask.");
  }

  bool MainThreadRun() override {
    RefPtr<layers::Image> image = CreateImageFromRawData(
        mSize, mStride, mFormat, mBuffer, mBufferLength, mCropRect);

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

/*
 * A wrapper to the nsLayoutUtils::SurfaceFromElement() function followed by the
 * security checking.
 */
template <class ElementType>
static already_AddRefed<SourceSurface> GetSurfaceFromElement(
    nsIGlobalObject* aGlobal, ElementType& aElement, bool* aWriteOnly,
    ErrorResult& aRv) {
  nsLayoutUtils::SurfaceFromElementResult res =
      nsLayoutUtils::SurfaceFromElement(
          &aElement, nsLayoutUtils::SFE_WANT_FIRST_FRAME_IF_IMAGE);

  RefPtr<SourceSurface> surface = res.GetSourceSurface();
  if (NS_WARN_IF(!surface)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  *aWriteOnly = res.mIsWriteOnly;

  return surface.forget();
}

ImageBitmap::ImageBitmap(nsIGlobalObject* aGlobal, layers::Image* aData,
                         bool aWriteOnly, gfxAlphaType aAlphaType)
    : mParent(aGlobal),
      mData(aData),
      mSurface(nullptr),
      mDataWrapper(new ImageUtils(mData)),
      mPictureRect(0, 0, aData->GetSize().width, aData->GetSize().height),
      mAlphaType(aAlphaType),
      mAllocatedImageData(false),
      mWriteOnly(aWriteOnly) {
  MOZ_ASSERT(aData, "aData is null in ImageBitmap constructor.");

  mShutdownObserver = new ImageBitmapShutdownObserver(this);
  mShutdownObserver->RegisterObserver();
}

ImageBitmap::~ImageBitmap() {
  if (mShutdownObserver) {
    mShutdownObserver->Clear();
    mShutdownObserver->UnregisterObserver();
    mShutdownObserver = nullptr;
  }
}

JSObject* ImageBitmap::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return ImageBitmap_Binding::Wrap(aCx, this, aGivenProto);
}

void ImageBitmap::Close() {
  mData = nullptr;
  mSurface = nullptr;
  mDataWrapper = nullptr;
  mPictureRect.SetEmpty();
}

void ImageBitmap::OnShutdown() {
  mShutdownObserver = nullptr;

  Close();
}

void ImageBitmap::SetPictureRect(const IntRect& aRect, ErrorResult& aRv) {
  mPictureRect = FixUpNegativeDimension(aRect, aRv);
}

/*
 * The functionality of PrepareForDrawTarget method:
 * (1) Get a SourceSurface from the mData (which is a layers::Image).
 * (2) Convert the SourceSurface to format B8G8R8A8 if the original format is
 *     R8G8B8, B8G8R8, HSV or Lab.
 *     Note: if the original format is A8 or Depth, then return null directly.
 * (3) Do cropping if the size of SourceSurface does not equal to the
 *     mPictureRect.
 * (4) Pre-multiply alpha if needed.
 */
already_AddRefed<SourceSurface> ImageBitmap::PrepareForDrawTarget(
    gfx::DrawTarget* aTarget) {
  MOZ_ASSERT(aTarget);

  if (!mData) {
    return nullptr;
  }

  if (!mSurface) {
    mSurface = mData->GetAsSourceSurface();

    if (!mSurface) {
      return nullptr;
    }
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

    target->CopySurface(mSurface, surfPortion, dest);
    mSurface = target->Snapshot();

    // Make mCropRect match new surface we've cropped to
    mPictureRect.MoveTo(0, 0);
  }

  // Pre-multiply alpha here.
  // Ignore this step if the source surface does not have alpha channel; this
  // kind of source surfaces might come form layers::PlanarYCbCrImage.
  if (mAlphaType == gfxAlphaType::NonPremult &&
      !IsOpaque(mSurface->GetFormat())) {
    MOZ_ASSERT(mSurface->GetFormat() == SurfaceFormat::R8G8B8A8 ||
               mSurface->GetFormat() == SurfaceFormat::B8G8R8A8 ||
               mSurface->GetFormat() == SurfaceFormat::A8R8G8B8);

    RefPtr<DataSourceSurface> dstSurface = mSurface->GetDataSurface();
    MOZ_ASSERT(dstSurface);

    RefPtr<DataSourceSurface> srcSurface;
    DataSourceSurface::MappedSurface srcMap;
    DataSourceSurface::MappedSurface dstMap;

    if (dstSurface->Map(DataSourceSurface::MapType::READ_WRITE, &dstMap)) {
      srcMap = dstMap;
    } else {
      srcSurface = dstSurface;
      if (!srcSurface->Map(DataSourceSurface::READ, &srcMap)) {
        gfxCriticalError()
            << "Failed to map source surface for premultiplying alpha.";
        return nullptr;
      }

      dstSurface = Factory::CreateDataSourceSurface(srcSurface->GetSize(),
                                                    srcSurface->GetFormat());

      if (!dstSurface ||
          !dstSurface->Map(DataSourceSurface::MapType::WRITE, &dstMap)) {
        gfxCriticalError()
            << "Failed to map destination surface for premultiplying alpha.";
        srcSurface->Unmap();
        return nullptr;
      }
    }

    PremultiplyData(srcMap.mData, srcMap.mStride, mSurface->GetFormat(),
                    dstMap.mData, dstMap.mStride, mSurface->GetFormat(),
                    dstSurface->GetSize());

    dstSurface->Unmap();
    if (srcSurface) {
      srcSurface->Unmap();
    }

    mSurface = dstSurface;
  }

  // Replace our surface with one optimized for the target we're about to draw
  // to, under the assumption it'll likely be drawn again to that target.
  // This call should be a no-op for already-optimized surfaces
  mSurface = target->OptimizeSourceSurface(mSurface);

  RefPtr<gfx::SourceSurface> surface(mSurface);
  return surface.forget();
}

already_AddRefed<layers::Image> ImageBitmap::TransferAsImage() {
  RefPtr<layers::Image> image = mData;
  Close();
  return image.forget();
}

UniquePtr<ImageBitmapCloneData> ImageBitmap::ToCloneData() const {
  if (!mData) {
    // A closed image cannot be cloned.
    return nullptr;
  }

  UniquePtr<ImageBitmapCloneData> result(new ImageBitmapCloneData());
  result->mPictureRect = mPictureRect;
  result->mAlphaType = mAlphaType;
  RefPtr<SourceSurface> surface = mData->GetAsSourceSurface();
  result->mSurface = surface->GetDataSurface();
  MOZ_ASSERT(result->mSurface);
  result->mWriteOnly = mWriteOnly;

  return result;
}

/* static */
already_AddRefed<ImageBitmap> ImageBitmap::CreateFromSourceSurface(
    nsIGlobalObject* aGlobal, gfx::SourceSurface* aSource, ErrorResult& aRv) {
  RefPtr<layers::Image> data = CreateImageFromSurface(aSource);
  RefPtr<ImageBitmap> ret =
      new ImageBitmap(aGlobal, data, false /* writeOnly */);
  ret->mAllocatedImageData = true;
  return ret.forget();
}

/* static */
already_AddRefed<ImageBitmap> ImageBitmap::CreateFromCloneData(
    nsIGlobalObject* aGlobal, ImageBitmapCloneData* aData) {
  RefPtr<layers::Image> data = CreateImageFromSurface(aData->mSurface);

  RefPtr<ImageBitmap> ret =
      new ImageBitmap(aGlobal, data, aData->mWriteOnly, aData->mAlphaType);

  ret->mAllocatedImageData = true;

  ErrorResult rv;
  ret->SetPictureRect(aData->mPictureRect, rv);
  return ret.forget();
}

/* static */
already_AddRefed<ImageBitmap> ImageBitmap::CreateFromOffscreenCanvas(
    nsIGlobalObject* aGlobal, OffscreenCanvas& aOffscreenCanvas,
    ErrorResult& aRv) {
  // Check write-only mode.
  bool writeOnly = aOffscreenCanvas.IsWriteOnly();

  nsLayoutUtils::SurfaceFromElementResult res =
      nsLayoutUtils::SurfaceFromOffscreenCanvas(
          &aOffscreenCanvas, nsLayoutUtils::SFE_WANT_FIRST_FRAME_IF_IMAGE);

  RefPtr<SourceSurface> surface = res.GetSourceSurface();

  if (NS_WARN_IF(!surface)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  RefPtr<layers::Image> data = CreateImageFromSurface(surface);

  RefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, data, writeOnly);

  ret->mAllocatedImageData = true;

  return ret.forget();
}

/* static */
already_AddRefed<ImageBitmap> ImageBitmap::CreateInternal(
    nsIGlobalObject* aGlobal, HTMLImageElement& aImageEl,
    const Maybe<IntRect>& aCropRect, ErrorResult& aRv) {
  // Check if the image element is completely available or not.
  if (!aImageEl.Complete()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  bool writeOnly = true;

  // Get the SourceSurface out from the image element and then do security
  // checking.
  RefPtr<SourceSurface> surface =
      GetSurfaceFromElement(aGlobal, aImageEl, &writeOnly, aRv);

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Create ImageBitmap.
  RefPtr<layers::Image> data = CreateImageFromSurface(surface);

  if (NS_WARN_IF(!data)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  RefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, data, writeOnly);

  // Set the picture rectangle.
  if (ret && aCropRect.isSome()) {
    ret->SetPictureRect(aCropRect.ref(), aRv);
  }

  return ret.forget();
}

/* static */
already_AddRefed<ImageBitmap> ImageBitmap::CreateInternal(
    nsIGlobalObject* aGlobal, SVGImageElement& aImageEl,
    const Maybe<IntRect>& aCropRect, ErrorResult& aRv) {
  bool writeOnly = true;

  // Get the SourceSurface out from the image element and then do security
  // checking.
  RefPtr<SourceSurface> surface =
      GetSurfaceFromElement(aGlobal, aImageEl, &writeOnly, aRv);

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Create ImageBitmap.
  RefPtr<layers::Image> data = CreateImageFromSurface(surface);

  if (NS_WARN_IF(!data)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  RefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, data, writeOnly);

  // Set the picture rectangle.
  if (ret && aCropRect.isSome()) {
    ret->SetPictureRect(aCropRect.ref(), aRv);
  }

  return ret.forget();
}

/* static */
already_AddRefed<ImageBitmap> ImageBitmap::CreateInternal(
    nsIGlobalObject* aGlobal, HTMLVideoElement& aVideoEl,
    const Maybe<IntRect>& aCropRect, ErrorResult& aRv) {
  aVideoEl.MarkAsContentSource(
      mozilla::dom::HTMLVideoElement::CallerAPI::CREATE_IMAGEBITMAP);

  // Check network state.
  if (aVideoEl.NetworkState() == NETWORK_EMPTY) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Check ready state.
  // Cannot be HTMLMediaElement::HAVE_NOTHING or
  // HTMLMediaElement::HAVE_METADATA.
  if (aVideoEl.ReadyState() <= HAVE_METADATA) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Check security.
  nsCOMPtr<nsIPrincipal> principal = aVideoEl.GetCurrentVideoPrincipal();
  bool hadCrossOriginRedirects = aVideoEl.HadCrossOriginRedirects();
  bool CORSUsed = aVideoEl.GetCORSMode() != CORS_NONE;
  bool writeOnly =
      CheckWriteOnlySecurity(CORSUsed, principal, hadCrossOriginRedirects);

  // Create ImageBitmap.
  RefPtr<layers::Image> data = aVideoEl.GetCurrentImage();
  if (!data) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }
  RefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, data, writeOnly);

  // Set the picture rectangle.
  if (ret && aCropRect.isSome()) {
    ret->SetPictureRect(aCropRect.ref(), aRv);
  }

  return ret.forget();
}

/* static */
already_AddRefed<ImageBitmap> ImageBitmap::CreateInternal(
    nsIGlobalObject* aGlobal, HTMLCanvasElement& aCanvasEl,
    const Maybe<IntRect>& aCropRect, ErrorResult& aRv) {
  if (aCanvasEl.Width() == 0 || aCanvasEl.Height() == 0) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  bool writeOnly = true;
  RefPtr<SourceSurface> surface =
      GetSurfaceFromElement(aGlobal, aCanvasEl, &writeOnly, aRv);

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (!writeOnly) {
    writeOnly = aCanvasEl.IsWriteOnly();
  }

  // Crop the source surface if needed.
  RefPtr<SourceSurface> croppedSurface;
  IntRect cropRect = aCropRect.valueOr(IntRect());

  // If the HTMLCanvasElement's rendering context is WebGL, then the snapshot
  // we got from the HTMLCanvasElement is a DataSourceSurface which is a copy
  // of the rendering context. We handle cropping in this case.
  bool needToReportMemoryAllocation = false;
  if ((aCanvasEl.GetCurrentContextType() == CanvasContextType::WebGL1 ||
       aCanvasEl.GetCurrentContextType() == CanvasContextType::WebGL2) &&
      aCropRect.isSome()) {
    RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface();
    croppedSurface = CropAndCopyDataSourceSurface(dataSurface, cropRect);
    cropRect.MoveTo(0, 0);
    needToReportMemoryAllocation = true;
  } else {
    croppedSurface = surface;
  }

  if (NS_WARN_IF(!croppedSurface)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Create an Image from the SourceSurface.
  RefPtr<layers::Image> data = CreateImageFromSurface(croppedSurface);

  if (NS_WARN_IF(!data)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  RefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, data, writeOnly);

  if (needToReportMemoryAllocation) {
    ret->mAllocatedImageData = true;
  }

  // Set the picture rectangle.
  if (ret && aCropRect.isSome()) {
    ret->SetPictureRect(cropRect, aRv);
  }

  return ret.forget();
}

/* static */
already_AddRefed<ImageBitmap> ImageBitmap::CreateInternal(
    nsIGlobalObject* aGlobal, ImageData& aImageData,
    const Maybe<IntRect>& aCropRect, ErrorResult& aRv) {
  // Copy data into SourceSurface.
  dom::Uint8ClampedArray array;
  DebugOnly<bool> inited = array.Init(aImageData.GetDataObject());
  MOZ_ASSERT(inited);

  array.ComputeLengthAndData();
  const SurfaceFormat FORMAT = SurfaceFormat::R8G8B8A8;
  // ImageData's underlying data is not alpha-premultiplied.
  const auto alphaType = gfxAlphaType::NonPremult;
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
    data = CreateImageFromRawData(imageSize, imageStride, FORMAT, array.Data(),
                                  dataLength, aCropRect);
  } else {
    RefPtr<CreateImageFromRawDataInMainThreadSyncTask> task =
        new CreateImageFromRawDataInMainThreadSyncTask(
            array.Data(), dataLength, imageStride, FORMAT, imageSize, aCropRect,
            getter_AddRefs(data));
    task->Dispatch(Canceling, aRv);
  }

  if (NS_WARN_IF(!data)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Create an ImageBimtap.
  RefPtr<ImageBitmap> ret =
      new ImageBitmap(aGlobal, data, false /* write-only */, alphaType);

  ret->mAllocatedImageData = true;

  // The cropping information has been handled in the CreateImageFromRawData()
  // function.

  return ret.forget();
}

/* static */
already_AddRefed<ImageBitmap> ImageBitmap::CreateInternal(
    nsIGlobalObject* aGlobal, CanvasRenderingContext2D& aCanvasCtx,
    const Maybe<IntRect>& aCropRect, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(aGlobal);
  nsGlobalWindowInner* window = nsGlobalWindowInner::Cast(win);
  if (NS_WARN_IF(!window) || !window->GetExtantDoc()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  window->GetExtantDoc()->WarnOnceAbout(
      Document::eCreateImageBitmapCanvasRenderingContext2D);

  // Check write-only mode.
  bool writeOnly =
      aCanvasCtx.GetCanvas()->IsWriteOnly() || aCanvasCtx.IsWriteOnly();

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

  RefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, data, writeOnly);

  ret->mAllocatedImageData = true;

  // Set the picture rectangle.
  if (ret && aCropRect.isSome()) {
    ret->SetPictureRect(aCropRect.ref(), aRv);
  }

  return ret.forget();
}

/* static */
already_AddRefed<ImageBitmap> ImageBitmap::CreateInternal(
    nsIGlobalObject* aGlobal, ImageBitmap& aImageBitmap,
    const Maybe<IntRect>& aCropRect, ErrorResult& aRv) {
  if (!aImageBitmap.mData) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  RefPtr<layers::Image> data = aImageBitmap.mData;
  RefPtr<ImageBitmap> ret = new ImageBitmap(
      aGlobal, data, aImageBitmap.mWriteOnly, aImageBitmap.mAlphaType);

  // Set the picture rectangle.
  if (ret && aCropRect.isSome()) {
    ret->SetPictureRect(aCropRect.ref(), aRv);
  }

  return ret.forget();
}

class FulfillImageBitmapPromise {
 protected:
  FulfillImageBitmapPromise(Promise* aPromise, ImageBitmap* aImageBitmap)
      : mPromise(aPromise), mImageBitmap(aImageBitmap) {
    MOZ_ASSERT(aPromise);
  }

  void DoFulfillImageBitmapPromise() { mPromise->MaybeResolve(mImageBitmap); }

 private:
  RefPtr<Promise> mPromise;
  RefPtr<ImageBitmap> mImageBitmap;
};

class FulfillImageBitmapPromiseTask final : public Runnable,
                                            public FulfillImageBitmapPromise {
 public:
  FulfillImageBitmapPromiseTask(Promise* aPromise, ImageBitmap* aImageBitmap)
      : Runnable("dom::FulfillImageBitmapPromiseTask"),
        FulfillImageBitmapPromise(aPromise, aImageBitmap) {}

  NS_IMETHOD Run() override {
    DoFulfillImageBitmapPromise();
    return NS_OK;
  }
};

class FulfillImageBitmapPromiseWorkerTask final
    : public WorkerSameThreadRunnable,
      public FulfillImageBitmapPromise {
 public:
  FulfillImageBitmapPromiseWorkerTask(Promise* aPromise,
                                      ImageBitmap* aImageBitmap)
      : WorkerSameThreadRunnable(GetCurrentThreadWorkerPrivate()),
        FulfillImageBitmapPromise(aPromise, aImageBitmap) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    DoFulfillImageBitmapPromise();
    return true;
  }
};

static void AsyncFulfillImageBitmapPromise(Promise* aPromise,
                                           ImageBitmap* aImageBitmap) {
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task =
        new FulfillImageBitmapPromiseTask(aPromise, aImageBitmap);
    NS_DispatchToCurrentThread(task);  // Actually, to the main-thread.
  } else {
    RefPtr<FulfillImageBitmapPromiseWorkerTask> task =
        new FulfillImageBitmapPromiseWorkerTask(aPromise, aImageBitmap);
    task->Dispatch();  // Actually, to the current worker-thread.
  }
}

class CreateImageBitmapFromBlobRunnable;

class CreateImageBitmapFromBlob final : public CancelableRunnable,
                                        public imgIContainerCallback,
                                        public nsIInputStreamCallback {
  friend class CreateImageBitmapFromBlobRunnable;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_IMGICONTAINERCALLBACK
  NS_DECL_NSIINPUTSTREAMCALLBACK

  static already_AddRefed<CreateImageBitmapFromBlob> Create(
      Promise* aPromise, nsIGlobalObject* aGlobal, Blob& aBlob,
      const Maybe<IntRect>& aCropRect, nsIEventTarget* aMainThreadEventTarget);

  NS_IMETHOD Run() override {
    MOZ_ASSERT(IsCurrentThread());

    nsresult rv = StartMimeTypeAndDecodeAndCropBlob();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MimeTypeAndDecodeAndCropBlobCompletedMainThread(nullptr, rv);
    }

    return NS_OK;
  }

  // Called by the WorkerRef.
  void WorkerShuttingDown();

 private:
  CreateImageBitmapFromBlob(Promise* aPromise, nsIGlobalObject* aGlobal,
                            already_AddRefed<nsIInputStream> aInputStream,
                            const Maybe<IntRect>& aCropRect,
                            nsIEventTarget* aMainThreadEventTarget)
      : CancelableRunnable("dom::CreateImageBitmapFromBlob"),
        mMutex("dom::CreateImageBitmapFromBlob::mMutex"),
        mPromise(aPromise),
        mGlobalObject(aGlobal),
        mInputStream(std::move(aInputStream)),
        mCropRect(aCropRect),
        mOriginalCropRect(aCropRect),
        mMainThreadEventTarget(aMainThreadEventTarget),
        mThread(PR_GetCurrentThread()) {}

  virtual ~CreateImageBitmapFromBlob() {}

  bool IsCurrentThread() const { return mThread == PR_GetCurrentThread(); }

  // Called on the owning thread.
  nsresult StartMimeTypeAndDecodeAndCropBlob();

  // Will be called when the decoding + cropping is completed on the
  // main-thread. This could the not the owning thread!
  void MimeTypeAndDecodeAndCropBlobCompletedMainThread(layers::Image* aImage,
                                                       nsresult aStatus);

  // Will be called when the decoding + cropping is completed on the owning
  // thread.
  void MimeTypeAndDecodeAndCropBlobCompletedOwningThread(layers::Image* aImage,
                                                         nsresult aStatus);

  // This is called on the main-thread only.
  nsresult MimeTypeAndDecodeAndCropBlob();

  // This is called on the main-thread only.
  nsresult DecodeAndCropBlob(const nsACString& aMimeType);

  // This is called on the main-thread only.
  nsresult GetMimeTypeSync(nsACString& aMimeType);

  // This is called on the main-thread only.
  nsresult GetMimeTypeAsync();

  Mutex mMutex;

  // The access to this object is protected by mutex but is always nullified on
  // the owning thread.
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;

  // Touched only on the owning thread.
  RefPtr<Promise> mPromise;

  // Touched only on the owning thread.
  nsCOMPtr<nsIGlobalObject> mGlobalObject;

  nsCOMPtr<nsIInputStream> mInputStream;
  Maybe<IntRect> mCropRect;
  Maybe<IntRect> mOriginalCropRect;
  IntSize mSourceSize;

  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;
  void* mThread;
};

NS_IMPL_ISUPPORTS_INHERITED(CreateImageBitmapFromBlob, CancelableRunnable,
                            imgIContainerCallback, nsIInputStreamCallback)

class CreateImageBitmapFromBlobRunnable : public WorkerRunnable {
 public:
  explicit CreateImageBitmapFromBlobRunnable(WorkerPrivate* aWorkerPrivate,
                                             CreateImageBitmapFromBlob* aTask,
                                             layers::Image* aImage,
                                             nsresult aStatus)
      : WorkerRunnable(aWorkerPrivate),
        mTask(aTask),
        mImage(aImage),
        mStatus(aStatus) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mTask->MimeTypeAndDecodeAndCropBlobCompletedOwningThread(mImage, mStatus);
    return true;
  }

 private:
  RefPtr<CreateImageBitmapFromBlob> mTask;
  RefPtr<layers::Image> mImage;
  nsresult mStatus;
};

static void AsyncCreateImageBitmapFromBlob(Promise* aPromise,
                                           nsIGlobalObject* aGlobal,
                                           Blob& aBlob,
                                           const Maybe<IntRect>& aCropRect) {
  // Let's identify the main-thread event target.
  nsCOMPtr<nsIEventTarget> mainThreadEventTarget;
  if (NS_IsMainThread()) {
    mainThreadEventTarget = aGlobal->EventTargetFor(TaskCategory::Other);
  } else {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    mainThreadEventTarget = workerPrivate->MainThreadEventTarget();
  }

  RefPtr<CreateImageBitmapFromBlob> task = CreateImageBitmapFromBlob::Create(
      aPromise, aGlobal, aBlob, aCropRect, mainThreadEventTarget);
  if (NS_WARN_IF(!task)) {
    aPromise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  NS_DispatchToCurrentThread(task);
}

/* static */
already_AddRefed<Promise> ImageBitmap::Create(
    nsIGlobalObject* aGlobal, const ImageBitmapSource& aSrc,
    const Maybe<gfx::IntRect>& aCropRect, ErrorResult& aRv) {
  MOZ_ASSERT(aGlobal);

  RefPtr<Promise> promise = Promise::Create(aGlobal, aRv);

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (aCropRect.isSome()) {
    if (aCropRect->Width() == 0) {
      aRv.ThrowRangeError(
          u"The crop rect width passed to createImageBitmap must be nonzero");
      return promise.forget();
    }

    if (aCropRect->Height() == 0) {
      aRv.ThrowRangeError(
          u"The crop rect height passed to createImageBitmap must be nonzero");
      return promise.forget();
    }
  }

  RefPtr<ImageBitmap> imageBitmap;

  if (aSrc.IsHTMLImageElement()) {
    MOZ_ASSERT(
        NS_IsMainThread(),
        "Creating ImageBitmap from HTMLImageElement off the main thread.");
    imageBitmap =
        CreateInternal(aGlobal, aSrc.GetAsHTMLImageElement(), aCropRect, aRv);
  } else if (aSrc.IsSVGImageElement()) {
    MOZ_ASSERT(
        NS_IsMainThread(),
        "Creating ImageBitmap from SVGImageElement off the main thread.");
    imageBitmap =
        CreateInternal(aGlobal, aSrc.GetAsSVGImageElement(), aCropRect, aRv);
  } else if (aSrc.IsHTMLVideoElement()) {
    MOZ_ASSERT(
        NS_IsMainThread(),
        "Creating ImageBitmap from HTMLVideoElement off the main thread.");
    imageBitmap =
        CreateInternal(aGlobal, aSrc.GetAsHTMLVideoElement(), aCropRect, aRv);
  } else if (aSrc.IsHTMLCanvasElement()) {
    MOZ_ASSERT(
        NS_IsMainThread(),
        "Creating ImageBitmap from HTMLCanvasElement off the main thread.");
    imageBitmap =
        CreateInternal(aGlobal, aSrc.GetAsHTMLCanvasElement(), aCropRect, aRv);
  } else if (aSrc.IsImageData()) {
    imageBitmap =
        CreateInternal(aGlobal, aSrc.GetAsImageData(), aCropRect, aRv);
  } else if (aSrc.IsCanvasRenderingContext2D()) {
    MOZ_ASSERT(NS_IsMainThread(),
               "Creating ImageBitmap from CanvasRenderingContext2D off the "
               "main thread.");
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsCanvasRenderingContext2D(),
                                 aCropRect, aRv);
  } else if (aSrc.IsImageBitmap()) {
    imageBitmap =
        CreateInternal(aGlobal, aSrc.GetAsImageBitmap(), aCropRect, aRv);
  } else if (aSrc.IsBlob()) {
    AsyncCreateImageBitmapFromBlob(promise, aGlobal, aSrc.GetAsBlob(),
                                   aCropRect);
    return promise.forget();
  } else {
    MOZ_CRASH("Unsupported type!");
    return nullptr;
  }

  if (!aRv.Failed()) {
    AsyncFulfillImageBitmapPromise(promise, imageBitmap);
  }

  return promise.forget();
}

/*static*/
JSObject* ImageBitmap::ReadStructuredClone(
    JSContext* aCx, JSStructuredCloneReader* aReader, nsIGlobalObject* aParent,
    const nsTArray<RefPtr<DataSourceSurface>>& aClonedSurfaces,
    uint32_t aIndex) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aReader);
  // aParent might be null.

  uint32_t picRectX_;
  uint32_t picRectY_;
  uint32_t picRectWidth_;
  uint32_t picRectHeight_;
  uint32_t alphaType_;
  uint32_t writeOnly;

  if (!JS_ReadUint32Pair(aReader, &picRectX_, &picRectY_) ||
      !JS_ReadUint32Pair(aReader, &picRectWidth_, &picRectHeight_) ||
      !JS_ReadUint32Pair(aReader, &alphaType_, &writeOnly)) {
    return nullptr;
  }

  int32_t picRectX = BitwiseCast<int32_t>(picRectX_);
  int32_t picRectY = BitwiseCast<int32_t>(picRectY_);
  int32_t picRectWidth = BitwiseCast<int32_t>(picRectWidth_);
  int32_t picRectHeight = BitwiseCast<int32_t>(picRectHeight_);
  const auto alphaType = BitwiseCast<gfxAlphaType>(alphaType_);

  // Create a new ImageBitmap.
  MOZ_ASSERT(!aClonedSurfaces.IsEmpty());
  MOZ_ASSERT(aIndex < aClonedSurfaces.Length());

  // RefPtr<ImageBitmap> needs to go out of scope before toObjectOrNull() is
  // called because the static analysis thinks dereferencing XPCOM objects
  // can GC (because in some cases it can!), and a return statement with a
  // JSObject* type means that JSObject* is on the stack as a raw pointer
  // while destructors are running.
  JS::Rooted<JS::Value> value(aCx);
  {
#ifdef FUZZING
    if (aIndex >= aClonedSurfaces.Length()) {
      return nullptr;
    }
#endif
    RefPtr<layers::Image> img = CreateImageFromSurface(aClonedSurfaces[aIndex]);
    RefPtr<ImageBitmap> imageBitmap =
        new ImageBitmap(aParent, img, !!writeOnly, alphaType);

    ErrorResult error;
    imageBitmap->SetPictureRect(
        IntRect(picRectX, picRectY, picRectWidth, picRectHeight), error);
    if (NS_WARN_IF(error.Failed())) {
      error.SuppressException();
      return nullptr;
    }

    if (!GetOrCreateDOMReflector(aCx, imageBitmap, &value)) {
      return nullptr;
    }

    imageBitmap->mAllocatedImageData = true;
  }

  return &(value.toObject());
}

/*static*/
bool ImageBitmap::WriteStructuredClone(
    JSStructuredCloneWriter* aWriter,
    nsTArray<RefPtr<DataSourceSurface>>& aClonedSurfaces,
    ImageBitmap* aImageBitmap) {
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aImageBitmap);

  const uint32_t picRectX = BitwiseCast<uint32_t>(aImageBitmap->mPictureRect.x);
  const uint32_t picRectY = BitwiseCast<uint32_t>(aImageBitmap->mPictureRect.y);
  const uint32_t picRectWidth =
      BitwiseCast<uint32_t>(aImageBitmap->mPictureRect.width);
  const uint32_t picRectHeight =
      BitwiseCast<uint32_t>(aImageBitmap->mPictureRect.height);
  const uint32_t alphaType = BitwiseCast<uint32_t>(aImageBitmap->mAlphaType);

  // Indexing the cloned surfaces and send the index to the receiver.
  uint32_t index = aClonedSurfaces.Length();

  if (NS_WARN_IF(!JS_WriteUint32Pair(aWriter, SCTAG_DOM_IMAGEBITMAP, index)) ||
      NS_WARN_IF(!JS_WriteUint32Pair(aWriter, picRectX, picRectY)) ||
      NS_WARN_IF(!JS_WriteUint32Pair(aWriter, picRectWidth, picRectHeight)) ||
      NS_WARN_IF(
          !JS_WriteUint32Pair(aWriter, alphaType, aImageBitmap->mWriteOnly))) {
    return false;
  }

  RefPtr<SourceSurface> surface = aImageBitmap->mData->GetAsSourceSurface();
  RefPtr<DataSourceSurface> snapshot = surface->GetDataSurface();
  RefPtr<DataSourceSurface> dstDataSurface;
  {
    // DataSourceSurfaceD2D1::GetStride() will call EnsureMapped implicitly and
    // won't Unmap after exiting function. So instead calling GetStride()
    // directly, using ScopedMap to get stride.
    DataSourceSurface::ScopedMap map(snapshot, DataSourceSurface::READ);
    dstDataSurface = Factory::CreateDataSourceSurfaceWithStride(
        snapshot->GetSize(), snapshot->GetFormat(), map.GetStride(), true);
  }
  if (NS_WARN_IF(!dstDataSurface)) {
    return false;
  }
  Factory::CopyDataSourceSurface(snapshot, dstDataSurface);
  aClonedSurfaces.AppendElement(dstDataSurface);
  return true;
}

size_t ImageBitmap::GetAllocatedSize() const {
  if (!mAllocatedImageData) {
    return 0;
  }

  // Calculate how many bytes are used.
  if (mData->GetFormat() == mozilla::ImageFormat::PLANAR_YCBCR) {
    return mData->AsPlanarYCbCrImage()->GetDataSize();
  }

  if (mData->GetFormat() == mozilla::ImageFormat::NV_IMAGE) {
    return mData->AsNVImage()->GetBufferSize();
  }

  RefPtr<SourceSurface> surface = mData->GetAsSourceSurface();
  const int bytesPerPixel = BytesPerPixel(surface->GetFormat());
  return surface->GetSize().height * surface->GetSize().width * bytesPerPixel;
}

size_t BindingJSObjectMallocBytes(ImageBitmap* aBitmap) {
  return aBitmap->GetAllocatedSize();
}

/* static */
already_AddRefed<CreateImageBitmapFromBlob> CreateImageBitmapFromBlob::Create(
    Promise* aPromise, nsIGlobalObject* aGlobal, Blob& aBlob,
    const Maybe<IntRect>& aCropRect, nsIEventTarget* aMainThreadEventTarget) {
  // Get the internal stream of the blob.
  nsCOMPtr<nsIInputStream> stream;
  ErrorResult error;
  aBlob.Impl()->CreateInputStream(getter_AddRefs(stream), error);
  if (NS_WARN_IF(error.Failed())) {
    return nullptr;
  }

  if (!NS_InputStreamIsBuffered(stream)) {
    nsCOMPtr<nsIInputStream> bufferedStream;
    nsresult rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                            stream.forget(), 4096);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    stream = bufferedStream;
  }

  RefPtr<CreateImageBitmapFromBlob> task = new CreateImageBitmapFromBlob(
      aPromise, aGlobal, stream.forget(), aCropRect, aMainThreadEventTarget);

  // Nothing to do for the main-thread.
  if (NS_IsMainThread()) {
    return task.forget();
  }

  // Let's use a WorkerRef to keep the worker alive if this is not the
  // main-thread.
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  RefPtr<StrongWorkerRef> workerRef =
      StrongWorkerRef::Create(workerPrivate, "CreateImageBitmapFromBlob",
                              [task]() { task->WorkerShuttingDown(); });
  if (NS_WARN_IF(!workerRef)) {
    return nullptr;
  }

  task->mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  return task.forget();
}

nsresult CreateImageBitmapFromBlob::StartMimeTypeAndDecodeAndCropBlob() {
  MOZ_ASSERT(IsCurrentThread());

  // Workers.
  if (!NS_IsMainThread()) {
    RefPtr<CreateImageBitmapFromBlob> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "CreateImageBitmapFromBlob::MimeTypeAndDecodeAndCropBlob", [self]() {
          nsresult rv = self->MimeTypeAndDecodeAndCropBlob();
          if (NS_WARN_IF(NS_FAILED(rv))) {
            self->MimeTypeAndDecodeAndCropBlobCompletedMainThread(nullptr, rv);
          }
        });

    return mMainThreadEventTarget->Dispatch(r.forget());
  }

  // Main-thread.
  return MimeTypeAndDecodeAndCropBlob();
}

nsresult CreateImageBitmapFromBlob::MimeTypeAndDecodeAndCropBlob() {
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString mimeType;
  nsresult rv = GetMimeTypeSync(mimeType);
  if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
    return GetMimeTypeAsync();
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return DecodeAndCropBlob(mimeType);
}

nsresult CreateImageBitmapFromBlob::DecodeAndCropBlob(
    const nsACString& aMimeType) {
  // Get the Component object.
  nsCOMPtr<imgITools> imgtool = do_GetService(NS_IMGTOOLS_CID);
  if (NS_WARN_IF(!imgtool)) {
    return NS_ERROR_FAILURE;
  }

  // Decode image.
  nsresult rv = imgtool->DecodeImageAsync(mInputStream, aMimeType, this,
                                          mMainThreadEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

static nsresult sniff_cb(nsIInputStream* aInputStream, void* aClosure,
                         const char* aFromRawSegment, uint32_t aToOffset,
                         uint32_t aCount, uint32_t* aWriteCount) {
  nsACString* mimeType = static_cast<nsACString*>(aClosure);
  MOZ_ASSERT(mimeType);

  if (aCount > 0) {
    imgLoader::GetMimeTypeFromContent(aFromRawSegment, aCount, *mimeType);
  }

  *aWriteCount = 0;

  // We don't want to consume data from the stream.
  return NS_ERROR_FAILURE;
}

nsresult CreateImageBitmapFromBlob::GetMimeTypeSync(nsACString& aMimeType) {
  uint32_t dummy;
  return mInputStream->ReadSegments(sniff_cb, &aMimeType, 128, &dummy);
}

nsresult CreateImageBitmapFromBlob::GetMimeTypeAsync() {
  nsCOMPtr<nsIAsyncInputStream> asyncInputStream =
      do_QueryInterface(mInputStream);
  if (NS_WARN_IF(!asyncInputStream)) {
    // If the stream is not async, why are we here?
    return NS_ERROR_FAILURE;
  }

  return asyncInputStream->AsyncWait(this, 0, 128, mMainThreadEventTarget);
}

NS_IMETHODIMP
CreateImageBitmapFromBlob::OnInputStreamReady(nsIAsyncInputStream* aStream) {
  // The stream should have data now. Let's start from scratch again.
  return MimeTypeAndDecodeAndCropBlob();
}

NS_IMETHODIMP
CreateImageBitmapFromBlob::OnImageReady(imgIContainer* aImgContainer,
                                        nsresult aStatus) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_FAILED(aStatus)) {
    MimeTypeAndDecodeAndCropBlobCompletedMainThread(nullptr, aStatus);
    return NS_OK;
  }

  MOZ_ASSERT(aImgContainer);

  // Get the surface out.
  uint32_t frameFlags = imgIContainer::FLAG_SYNC_DECODE |
                        imgIContainer::FLAG_ASYNC_NOTIFY |
                        imgIContainer::FLAG_WANT_DATA_SURFACE;
  uint32_t whichFrame = imgIContainer::FRAME_FIRST;
  RefPtr<SourceSurface> surface =
      aImgContainer->GetFrame(whichFrame, frameFlags);

  if (NS_WARN_IF(!surface)) {
    MimeTypeAndDecodeAndCropBlobCompletedMainThread(
        nullptr, NS_ERROR_DOM_INVALID_STATE_ERR);
    return NS_OK;
  }

  // Store the sourceSize value for the
  // MimeTypeAndDecodeAndCropBlobCompletedMainThread call.
  mSourceSize = surface->GetSize();

  // Crop the source surface if needed.
  RefPtr<SourceSurface> croppedSurface = surface;

  if (mCropRect.isSome()) {
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
    croppedSurface = CropAndCopyDataSourceSurface(dataSurface, mCropRect.ref());
    mCropRect->MoveTo(0, 0);
  }

  if (NS_WARN_IF(!croppedSurface)) {
    MimeTypeAndDecodeAndCropBlobCompletedMainThread(
        nullptr, NS_ERROR_DOM_INVALID_STATE_ERR);
    return NS_OK;
  }

  // Create an Image from the source surface.
  RefPtr<layers::Image> image = CreateImageFromSurface(croppedSurface);

  if (NS_WARN_IF(!image)) {
    MimeTypeAndDecodeAndCropBlobCompletedMainThread(
        nullptr, NS_ERROR_DOM_INVALID_STATE_ERR);
    return NS_OK;
  }

  MimeTypeAndDecodeAndCropBlobCompletedMainThread(image, NS_OK);
  return NS_OK;
}

void CreateImageBitmapFromBlob::MimeTypeAndDecodeAndCropBlobCompletedMainThread(
    layers::Image* aImage, nsresult aStatus) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsCurrentThread()) {
    MutexAutoLock lock(mMutex);

    if (!mWorkerRef) {
      // The worker is already gone.
      return;
    }

    RefPtr<CreateImageBitmapFromBlobRunnable> r =
        new CreateImageBitmapFromBlobRunnable(mWorkerRef->Private(), this,
                                              aImage, aStatus);
    r->Dispatch();
    return;
  }

  MimeTypeAndDecodeAndCropBlobCompletedOwningThread(aImage, aStatus);
}

void CreateImageBitmapFromBlob::
    MimeTypeAndDecodeAndCropBlobCompletedOwningThread(layers::Image* aImage,
                                                      nsresult aStatus) {
  MOZ_ASSERT(IsCurrentThread());

  if (!mPromise) {
    // The worker is going to be released soon. No needs to continue.
    return;
  }

  // Let's release what has to be released on the owning thread.
  auto raii = MakeScopeExit([&] {
    // Doing this we also release the worker.
    mWorkerRef = nullptr;

    mPromise = nullptr;
    mGlobalObject = nullptr;
  });

  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    mPromise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Create ImageBitmap object.
  RefPtr<ImageBitmap> imageBitmap =
      new ImageBitmap(mGlobalObject, aImage, false /* write-only */);

  if (mCropRect.isSome()) {
    ErrorResult rv;
    imageBitmap->SetPictureRect(mCropRect.ref(), rv);

    if (rv.Failed()) {
      mPromise->MaybeReject(rv);
      return;
    }
  }

  imageBitmap->mAllocatedImageData = true;

  mPromise->MaybeResolve(imageBitmap);
}

void CreateImageBitmapFromBlob::WorkerShuttingDown() {
  MOZ_ASSERT(IsCurrentThread());

  MutexAutoLock lock(mMutex);

  // Let's release all the non-thread-safe objects now.
  mWorkerRef = nullptr;
  mPromise = nullptr;
  mGlobalObject = nullptr;
}

}  // namespace dom
}  // namespace mozilla
