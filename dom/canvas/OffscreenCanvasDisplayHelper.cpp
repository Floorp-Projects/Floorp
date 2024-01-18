/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OffscreenCanvasDisplayHelper.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/CanvasManagerChild.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/PersistentBufferProvider.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "mozilla/layers/TextureWrapperImage.h"
#include "mozilla/SVGObserverUtils.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsRFPService.h"

namespace mozilla::dom {

OffscreenCanvasDisplayHelper::OffscreenCanvasDisplayHelper(
    HTMLCanvasElement* aCanvasElement, uint32_t aWidth, uint32_t aHeight)
    : mMutex("mozilla::dom::OffscreenCanvasDisplayHelper"),
      mCanvasElement(aCanvasElement),
      mImageProducerID(layers::ImageContainer::AllocateProducerID()) {
  mData.mSize.width = aWidth;
  mData.mSize.height = aHeight;
}

OffscreenCanvasDisplayHelper::~OffscreenCanvasDisplayHelper() = default;

void OffscreenCanvasDisplayHelper::DestroyElement() {
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);
  mCanvasElement = nullptr;
}

void OffscreenCanvasDisplayHelper::DestroyCanvas() {
  MutexAutoLock lock(mMutex);
  mOffscreenCanvas = nullptr;
  mWorkerRef = nullptr;
}

CanvasContextType OffscreenCanvasDisplayHelper::GetContextType() const {
  MutexAutoLock lock(mMutex);
  return mType;
}

RefPtr<layers::ImageContainer> OffscreenCanvasDisplayHelper::GetImageContainer()
    const {
  MutexAutoLock lock(mMutex);
  return mImageContainer;
}

void OffscreenCanvasDisplayHelper::UpdateContext(
    OffscreenCanvas* aOffscreenCanvas, RefPtr<ThreadSafeWorkerRef>&& aWorkerRef,
    CanvasContextType aType, const Maybe<int32_t>& aChildId) {
  RefPtr<layers::ImageContainer> imageContainer =
      MakeRefPtr<layers::ImageContainer>(layers::ImageContainer::ASYNCHRONOUS);

  MutexAutoLock lock(mMutex);

  mOffscreenCanvas = aOffscreenCanvas;
  mWorkerRef = std::move(aWorkerRef);
  mType = aType;
  mContextChildId = aChildId;
  mImageContainer = std::move(imageContainer);

  if (aChildId) {
    mContextManagerId = Some(gfx::CanvasManagerChild::Get()->Id());
  } else {
    mContextManagerId.reset();
  }

  MaybeQueueInvalidateElement();
}

void OffscreenCanvasDisplayHelper::FlushForDisplay() {
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);

  // Without an OffscreenCanvas object bound to us, we either have not drawn
  // using the canvas at all, or we have already destroyed it.
  if (!mOffscreenCanvas) {
    return;
  }

  // We assign/destroy the worker ref at the same time as the OffscreenCanvas
  // ref, so we can only have the canvas without a worker ref if it exists on
  // the main thread.
  if (!mWorkerRef) {
    // We queue to ensure that we have the same asynchronous update behaviour
    // for a main thread and a worker based OffscreenCanvas.
    mOffscreenCanvas->QueueCommitToCompositor();
    return;
  }

  class FlushWorkerRunnable final : public WorkerRunnable {
   public:
    FlushWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                        OffscreenCanvasDisplayHelper* aDisplayHelper)
        : WorkerRunnable(aWorkerPrivate), mDisplayHelper(aDisplayHelper) {}

    bool WorkerRun(JSContext*, WorkerPrivate*) override {
      // The OffscreenCanvas can only be freed on the worker thread, so we
      // cannot be racing with an OffscreenCanvas::DestroyCanvas call and its
      // destructor. We just need to make sure we don't call into
      // OffscreenCanvas while holding the lock since it calls back into the
      // OffscreenCanvasDisplayHelper.
      RefPtr<OffscreenCanvas> canvas;
      {
        MutexAutoLock lock(mDisplayHelper->mMutex);
        canvas = mDisplayHelper->mOffscreenCanvas;
      }

      if (canvas) {
        canvas->CommitFrameToCompositor();
      }
      return true;
    }

   private:
    RefPtr<OffscreenCanvasDisplayHelper> mDisplayHelper;
  };

  // Otherwise we are calling from the main thread during painting to a canvas
  // on a worker thread.
  auto task = MakeRefPtr<FlushWorkerRunnable>(mWorkerRef->Private(), this);
  task->Dispatch();
}

bool OffscreenCanvasDisplayHelper::CommitFrameToCompositor(
    nsICanvasRenderingContextInternal* aContext,
    layers::TextureType aTextureType,
    const Maybe<OffscreenCanvasDisplayData>& aData) {
  MutexAutoLock lock(mMutex);

  gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
  layers::TextureFlags flags = layers::TextureFlags::IMMUTABLE;

  if (!mCanvasElement) {
    // Our weak reference to the canvas element has been cleared, so we cannot
    // present directly anymore.
    return false;
  }

  if (aData) {
    mData = aData.ref();
    MaybeQueueInvalidateElement();
  }

  if (mData.mOwnerId.isSome()) {
    // No need to update the ImageContainer as the presentation itself is
    // handled in the compositor process.
    return true;
  }

  if (!mImageContainer) {
    return false;
  }

  if (mData.mIsOpaque) {
    flags |= layers::TextureFlags::IS_OPAQUE;
    format = gfx::SurfaceFormat::B8G8R8X8;
  } else if (!mData.mIsAlphaPremult) {
    flags |= layers::TextureFlags::NON_PREMULTIPLIED;
  }

  switch (mData.mOriginPos) {
    case gl::OriginPos::BottomLeft:
      flags |= layers::TextureFlags::ORIGIN_BOTTOM_LEFT;
      break;
    case gl::OriginPos::TopLeft:
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled origin position!");
      break;
  }

  auto imageBridge = layers::ImageBridgeChild::GetSingleton();
  if (!imageBridge) {
    return false;
  }

  bool paintCallbacks = mData.mDoPaintCallbacks;
  bool hasRemoteTextureDesc = false;
  RefPtr<layers::Image> image;
  RefPtr<layers::TextureClient> texture;
  RefPtr<gfx::SourceSurface> surface;
  Maybe<layers::SurfaceDescriptor> desc;

  {
    MutexAutoUnlock unlock(mMutex);
    if (paintCallbacks) {
      aContext->OnBeforePaintTransaction();
    }

    desc = aContext->PresentFrontBuffer(nullptr, aTextureType);
    if (desc) {
      hasRemoteTextureDesc =
          desc->type() ==
          layers::SurfaceDescriptor::TSurfaceDescriptorRemoteTexture;
    } else {
      if (layers::PersistentBufferProvider* provider =
              aContext->GetBufferProvider()) {
        texture = provider->GetTextureClient();
      }

      if (!texture) {
        surface =
            aContext->GetFrontBufferSnapshot(/* requireAlphaPremult */ false);
        if (surface && surface->GetType() == gfx::SurfaceType::WEBGL) {
          // Ensure we can map in the surface. If we get a SourceSurfaceWebgl
          // surface, then it may not be backed by raw pixels yet. We need to
          // map it on the owning thread rather than the ImageBridge thread.
          gfx::DataSourceSurface::ScopedMap map(
              static_cast<gfx::DataSourceSurface*>(surface.get()),
              gfx::DataSourceSurface::READ);
          if (!map.IsMapped()) {
            surface = nullptr;
          }
        }
      }
    }

    if (paintCallbacks) {
      aContext->OnDidPaintTransaction();
    }
  }

  // We save any current surface because we might need it in GetSnapshot. If we
  // are on a worker thread and not WebGL, then this will be the only way we can
  // access the pixel data on the main thread.
  mFrontBufferSurface = surface;

  // We do not use the ImageContainer plumbing with remote textures, so if we
  // have that, we can just early return here.
  if (hasRemoteTextureDesc) {
    const auto& textureDesc = desc->get_SurfaceDescriptorRemoteTexture();
    imageBridge->UpdateCompositable(mImageContainer, textureDesc.textureId(),
                                    textureDesc.ownerId(), mData.mSize, flags);
    return true;
  }

  if (surface) {
    auto surfaceImage = MakeRefPtr<layers::SourceSurfaceImage>(surface);
    surfaceImage->SetTextureFlags(flags);
    image = surfaceImage;
  } else {
    if (desc && !texture) {
      texture = layers::SharedSurfaceTextureData::CreateTextureClient(
          *desc, format, mData.mSize, flags, imageBridge);
    }
    if (texture) {
      image = new layers::TextureWrapperImage(
          texture, gfx::IntRect(gfx::IntPoint(0, 0), texture->GetSize()));
    }
  }

  if (image) {
    AutoTArray<layers::ImageContainer::NonOwningImage, 1> imageList;
    imageList.AppendElement(layers::ImageContainer::NonOwningImage(
        image, TimeStamp(), mLastFrameID++, mImageProducerID));
    mImageContainer->SetCurrentImages(imageList);
  } else {
    mImageContainer->ClearAllImages();
  }

  return true;
}

void OffscreenCanvasDisplayHelper::MaybeQueueInvalidateElement() {
  if (!mPendingInvalidate) {
    mPendingInvalidate = true;
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "OffscreenCanvasDisplayHelper::InvalidateElement",
        [self = RefPtr{this}] { self->InvalidateElement(); }));
  }
}

void OffscreenCanvasDisplayHelper::InvalidateElement() {
  MOZ_ASSERT(NS_IsMainThread());

  HTMLCanvasElement* canvasElement;
  gfx::IntSize size;

  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mPendingInvalidate);
    mPendingInvalidate = false;
    canvasElement = mCanvasElement;
    size = mData.mSize;
  }

  if (canvasElement) {
    SVGObserverUtils::InvalidateDirectRenderingObservers(canvasElement);
    canvasElement->InvalidateCanvasPlaceholder(size.width, size.height);
    canvasElement->InvalidateCanvasContent(nullptr);
  }
}

already_AddRefed<gfx::SourceSurface>
OffscreenCanvasDisplayHelper::TransformSurface(gfx::SourceSurface* aSurface,
                                               bool aHasAlpha,
                                               bool aIsAlphaPremult,
                                               gl::OriginPos aOriginPos) const {
  if (!aSurface) {
    return nullptr;
  }

  if (aOriginPos == gl::OriginPos::TopLeft && (!aHasAlpha || aIsAlphaPremult)) {
    // If we don't need to y-flip, and it is either opaque or premultiplied,
    // we can just return the same surface.
    return do_AddRef(aSurface);
  }

  // Otherwise we need to copy and apply the necessary transformations.
  RefPtr<gfx::DataSourceSurface> srcSurface = aSurface->GetDataSurface();
  if (!srcSurface) {
    return nullptr;
  }

  const auto size = srcSurface->GetSize();
  const auto format = srcSurface->GetFormat();

  RefPtr<gfx::DataSourceSurface> dstSurface =
      gfx::Factory::CreateDataSourceSurface(size, format, /* aZero */ false);
  if (!dstSurface) {
    return nullptr;
  }

  gfx::DataSourceSurface::ScopedMap srcMap(srcSurface,
                                           gfx::DataSourceSurface::READ);
  gfx::DataSourceSurface::ScopedMap dstMap(dstSurface,
                                           gfx::DataSourceSurface::WRITE);
  if (!srcMap.IsMapped() || !dstMap.IsMapped()) {
    return nullptr;
  }

  bool success;
  switch (aOriginPos) {
    case gl::OriginPos::BottomLeft:
      if (aHasAlpha && !aIsAlphaPremult) {
        success = gfx::PremultiplyYFlipData(
            srcMap.GetData(), srcMap.GetStride(), format, dstMap.GetData(),
            dstMap.GetStride(), format, size);
      } else {
        success = gfx::SwizzleYFlipData(srcMap.GetData(), srcMap.GetStride(),
                                        format, dstMap.GetData(),
                                        dstMap.GetStride(), format, size);
      }
      break;
    case gl::OriginPos::TopLeft:
      if (aHasAlpha && !aIsAlphaPremult) {
        success = gfx::PremultiplyData(srcMap.GetData(), srcMap.GetStride(),
                                       format, dstMap.GetData(),
                                       dstMap.GetStride(), format, size);
      } else {
        success = gfx::SwizzleData(srcMap.GetData(), srcMap.GetStride(), format,
                                   dstMap.GetData(), dstMap.GetStride(), format,
                                   size);
      }
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled origin position!");
      success = false;
      break;
  }

  if (!success) {
    return nullptr;
  }

  return dstSurface.forget();
}

already_AddRefed<gfx::SourceSurface>
OffscreenCanvasDisplayHelper::GetSurfaceSnapshot() {
  MOZ_ASSERT(NS_IsMainThread());

  bool hasAlpha;
  bool isAlphaPremult;
  gl::OriginPos originPos;
  Maybe<uint32_t> managerId;
  Maybe<int32_t> childId;
  HTMLCanvasElement* canvasElement;
  RefPtr<gfx::SourceSurface> surface;
  Maybe<layers::RemoteTextureOwnerId> ownerId;

  {
    MutexAutoLock lock(mMutex);
    hasAlpha = !mData.mIsOpaque;
    isAlphaPremult = mData.mIsAlphaPremult;
    originPos = mData.mOriginPos;
    ownerId = mData.mOwnerId;
    managerId = mContextManagerId;
    childId = mContextChildId;
    canvasElement = mCanvasElement;
    surface = mFrontBufferSurface;
  }

  if (surface) {
    // We already have a copy of the front buffer in our process.
    return TransformSurface(surface, hasAlpha, isAlphaPremult, originPos);
  }

#ifdef MOZ_WIDGET_ANDROID
  // On Android, we cannot both display a GL context and read back the pixels.
  if (canvasElement) {
    return nullptr;
  }
#endif

  if (managerId && childId) {
    // We don't have a usable surface, and the context lives in the compositor
    // process.
    return gfx::CanvasManagerChild::Get()->GetSnapshot(
        managerId.value(), childId.value(), ownerId,
        hasAlpha ? gfx::SurfaceFormat::R8G8B8A8 : gfx::SurfaceFormat::R8G8B8X8,
        hasAlpha && !isAlphaPremult, originPos == gl::OriginPos::BottomLeft);
  }

  if (!canvasElement) {
    return nullptr;
  }

  // If we don't have any protocol IDs, or an existing surface, it is possible
  // it is a main thread OffscreenCanvas instance. If so, then the element's
  // OffscreenCanvas is not neutered and has access to the context. We can use
  // that to get the snapshot directly.
  const auto* offscreenCanvas = canvasElement->GetOffscreenCanvas();
  if (nsICanvasRenderingContextInternal* context =
          offscreenCanvas->GetContext()) {
    surface = context->GetFrontBufferSnapshot(/* requireAlphaPremult */ false);
    surface = TransformSurface(surface, hasAlpha, isAlphaPremult, originPos);
    if (surface) {
      return surface.forget();
    }
  }

  // Finally, we can try peeking into the image container to see if we are able
  // to do a readback via the TextureClient.
  if (layers::ImageContainer* container = canvasElement->GetImageContainer()) {
    AutoTArray<layers::ImageContainer::OwningImage, 1> images;
    uint32_t generationCounter;
    container->GetCurrentImages(&images, &generationCounter);
    if (!images.IsEmpty()) {
      if (layers::Image* image = images.LastElement().mImage) {
        surface = image->GetAsSourceSurface();
        return TransformSurface(surface, hasAlpha, isAlphaPremult, originPos);
      }
    }
  }

  return nullptr;
}

already_AddRefed<layers::Image> OffscreenCanvasDisplayHelper::GetAsImage() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<gfx::SourceSurface> surface = GetSurfaceSnapshot();
  if (!surface) {
    return nullptr;
  }
  return MakeAndAddRef<layers::SourceSurfaceImage>(surface);
}

UniquePtr<uint8_t[]> OffscreenCanvasDisplayHelper::GetImageBuffer(
    int32_t* aOutFormat, gfx::IntSize* aOutImageSize) {
  RefPtr<gfx::SourceSurface> surface = GetSurfaceSnapshot();
  if (!surface) {
    return nullptr;
  }

  RefPtr<gfx::DataSourceSurface> dataSurface = surface->GetDataSurface();
  if (!dataSurface) {
    return nullptr;
  }

  *aOutFormat = imgIEncoder::INPUT_FORMAT_HOSTARGB;
  *aOutImageSize = dataSurface->GetSize();

  UniquePtr<uint8_t[]> imageBuffer = gfx::SurfaceToPackedBGRA(dataSurface);
  if (!imageBuffer) {
    return nullptr;
  }

  bool resistFingerprinting;
  nsICookieJarSettings* cookieJarSettings = nullptr;
  {
    MutexAutoLock lock(mMutex);
    if (mCanvasElement) {
      Document* doc = mCanvasElement->OwnerDoc();
      resistFingerprinting =
          doc->ShouldResistFingerprinting(RFPTarget::CanvasRandomization);
      if (resistFingerprinting) {
        cookieJarSettings = doc->CookieJarSettings();
      }
    } else {
      resistFingerprinting = nsContentUtils::ShouldResistFingerprinting(
          "Fallback", RFPTarget::CanvasRandomization);
    }
  }

  if (resistFingerprinting) {
    nsRFPService::RandomizePixels(
        cookieJarSettings, imageBuffer.get(), dataSurface->GetSize().width,
        dataSurface->GetSize().height,
        dataSurface->GetSize().width * dataSurface->GetSize().height * 4,
        gfx::SurfaceFormat::A8R8G8B8_UINT32);
  }
  return imageBuffer;
}

}  // namespace mozilla::dom
