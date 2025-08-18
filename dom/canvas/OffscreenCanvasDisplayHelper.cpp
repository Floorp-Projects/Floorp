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
#include "mozilla/StaticPrefs_gfx.h"
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

OffscreenCanvasDisplayHelper::~OffscreenCanvasDisplayHelper() {
  MutexAutoLock lock(mMutex);
  NS_ReleaseOnMainThread("OffscreenCanvas::mExpandedReader",
                         mExpandedReader.forget());
}

void OffscreenCanvasDisplayHelper::DestroyElement() {
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);
  if (mImageContainer) {
    mImageContainer->ClearAllImages();
    mImageContainer = nullptr;
  }
  mFrontBufferSurface = nullptr;
  mCanvasElement = nullptr;
}

void OffscreenCanvasDisplayHelper::DestroyCanvas() {
  if (auto* cm = gfx::CanvasManagerChild::Get()) {
    cm->EndCanvasTransaction();
  }

  MutexAutoLock lock(mMutex);
  if (mImageContainer) {
    mImageContainer->ClearAllImages();
    mImageContainer = nullptr;
  }
  mFrontBufferSurface = nullptr;
  mOffscreenCanvas = nullptr;
  mWorkerRef = nullptr;
}

void OffscreenCanvasDisplayHelper::SetWriteOnly(nsIPrincipal* aExpandedReader) {
  MutexAutoLock lock(mMutex);
  NS_ReleaseOnMainThread("OffscreenCanvasDisplayHelper::mExpandedReader",
                         mExpandedReader.forget());
  mExpandedReader = aExpandedReader;
  mIsWriteOnly = true;
}

bool OffscreenCanvasDisplayHelper::CallerCanRead(
    nsIPrincipal& aPrincipal) const {
  MutexAutoLock lock(mMutex);
  if (!mIsWriteOnly) {
    return true;
  }

  // If mExpandedReader is set, this canvas was tainted only by
  // mExpandedReader's resources. So allow reading if the subject
  // principal subsumes mExpandedReader.
  if (mExpandedReader && aPrincipal.Subsumes(mExpandedReader)) {
    return true;
  }

  return nsContentUtils::PrincipalHasPermission(aPrincipal,
                                                nsGkAtoms::all_urlsPermission);
}

bool OffscreenCanvasDisplayHelper::CanElementCaptureStream() const {
  MutexAutoLock lock(mMutex);
  return !!mWorkerRef;
}

bool OffscreenCanvasDisplayHelper::UsingElementCaptureStream() const {
  MutexAutoLock lock(mMutex);

  if (NS_WARN_IF(!NS_IsMainThread())) {
    MOZ_ASSERT_UNREACHABLE("Should not call off main-thread!");
    return !!mCanvasElement;
  }

  return mCanvasElement && mCanvasElement->UsingCaptureStream();
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
      MakeRefPtr<layers::ImageContainer>(
          layers::ImageUsageType::OffscreenCanvas,
          layers::ImageContainer::ASYNCHRONOUS);

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

  class FlushWorkerRunnable final : public WorkerThreadRunnable {
   public:
    FlushWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                        OffscreenCanvasDisplayHelper* aDisplayHelper)
        : WorkerThreadRunnable("FlushWorkerRunnable"),
          mDisplayHelper(aDisplayHelper) {}

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
  task->Dispatch(mWorkerRef->Private());
}

bool OffscreenCanvasDisplayHelper::CommitFrameToCompositor(
    nsICanvasRenderingContextInternal* aContext,
    layers::TextureType aTextureType,
    const Maybe<OffscreenCanvasDisplayData>& aData) {
  auto endTransaction = MakeScopeExit([&]() {
    if (auto* cm = gfx::CanvasManagerChild::Get()) {
      cm->EndCanvasTransaction();
    }
  });

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
  RefPtr<layers::FwdTransactionTracker> tracker;

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
      if (hasRemoteTextureDesc) {
        tracker = aContext->UseCompositableForwarder(imageBridge);
        if (tracker) {
          flags |= layers::TextureFlags::WAIT_FOR_REMOTE_TEXTURE_OWNER;
        }
      }
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
                                    textureDesc.ownerId(), mData.mSize, flags,
                                    tracker);
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

  class SnapshotWorkerRunnable final : public MainThreadWorkerRunnable {
   public:
    SnapshotWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                           OffscreenCanvasDisplayHelper* aDisplayHelper)
        : MainThreadWorkerRunnable("SnapshotWorkerRunnable"),
          mMonitor("SnapshotWorkerRunnable::mMonitor"),
          mDisplayHelper(aDisplayHelper) {}

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

      // Now that we are on the correct thread, we can extract the snapshot. If
      // it is a Skia surface, perform a copy to threading issues.
      RefPtr<gfx::SourceSurface> surface;
      if (canvas) {
        if (auto* context = canvas->GetContext()) {
          surface =
              context->GetFrontBufferSnapshot(/* requireAlphaPremult */ false);
          if (surface && surface->GetType() == gfx::SurfaceType::SKIA) {
            surface = gfx::Factory::CopyDataSourceSurface(
                static_cast<gfx::DataSourceSurface*>(surface.get()));
          }
        }
      }

      MonitorAutoLock lock(mMonitor);
      mSurface = std::move(surface);
      mComplete = true;
      lock.NotifyAll();
      return true;
    }

    already_AddRefed<gfx::SourceSurface> Wait(int32_t aTimeoutMs) {
      MonitorAutoLock lock(mMonitor);

      TimeDuration timeout = TimeDuration::FromMilliseconds(aTimeoutMs);
      while (!mComplete) {
        if (lock.Wait(timeout) == CVStatus::Timeout) {
          return nullptr;
        }
      }

      return mSurface.forget();
    }

   private:
    Monitor mMonitor;
    RefPtr<OffscreenCanvasDisplayHelper> mDisplayHelper;
    RefPtr<gfx::SourceSurface> mSurface MOZ_GUARDED_BY(mMonitor);
    bool mComplete MOZ_GUARDED_BY(mMonitor) = false;
  };

  bool hasAlpha;
  bool isAlphaPremult;
  gl::OriginPos originPos;
  HTMLCanvasElement* canvasElement;
  RefPtr<gfx::SourceSurface> surface;
  RefPtr<SnapshotWorkerRunnable> workerRunnable;

  {
    MutexAutoLock lock(mMutex);
#ifdef MOZ_WIDGET_ANDROID
    // On Android, we cannot both display a GL context and read back the pixels.
    if (mCanvasElement) {
      return nullptr;
    }
#endif

    hasAlpha = !mData.mIsOpaque;
    isAlphaPremult = mData.mIsAlphaPremult;
    originPos = mData.mOriginPos;
    canvasElement = mCanvasElement;
    if (mWorkerRef) {
      workerRunnable =
          MakeRefPtr<SnapshotWorkerRunnable>(mWorkerRef->Private(), this);
      workerRunnable->Dispatch(mWorkerRef->Private());
    }
  }

  if (workerRunnable) {
    // We transferred to a DOM worker, so we need to do the readback on the
    // owning thread and wait for the result.
    surface = workerRunnable->Wait(
        StaticPrefs::gfx_offscreencanvas_snapshot_timeout_ms());
  } else if (canvasElement) {
    // If we have a context, it is owned by the main thread.
    const auto* offscreenCanvas = canvasElement->GetOffscreenCanvas();
    if (nsICanvasRenderingContextInternal* context =
            offscreenCanvas->GetContext()) {
      surface =
          context->GetFrontBufferSnapshot(/* requireAlphaPremult */ false);
    }
  }

  return TransformSurface(surface, hasAlpha, isAlphaPremult, originPos);
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
