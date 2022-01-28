/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OffscreenCanvasDisplayHelper.h"
#include "mozilla/gfx/CanvasManagerChild.h"
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "mozilla/layers/TextureWrapperImage.h"
#include "mozilla/SVGObserverUtils.h"
#include "nsICanvasRenderingContextInternal.h"

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

void OffscreenCanvasDisplayHelper::Destroy() {
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);
  mCanvasElement = nullptr;
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
    CanvasContextType aType, const Maybe<int32_t>& aChildId) {
  MutexAutoLock lock(mMutex);
  mImageContainer =
      MakeRefPtr<layers::ImageContainer>(layers::ImageContainer::ASYNCHRONOUS);
  mType = aType;
  mContextChildId = aChildId;

  if (aChildId) {
    mContextManagerId = Some(gfx::CanvasManagerChild::Get()->Id());
  } else {
    mContextManagerId.reset();
  }

  MaybeQueueInvalidateElement();
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

  if (mData.mDoPaintCallbacks) {
    aContext->OnBeforePaintTransaction();
  }

  RefPtr<layers::Image> image;
  RefPtr<gfx::SourceSurface> surface;

  Maybe<layers::SurfaceDescriptor> desc =
      aContext->PresentFrontBuffer(nullptr, aTextureType);
  if (desc) {
    RefPtr<layers::TextureClient> texture =
        layers::SharedSurfaceTextureData::CreateTextureClient(
            *desc, format, mData.mSize, flags, imageBridge);
    if (texture) {
      image = new layers::TextureWrapperImage(
          texture, gfx::IntRect(gfx::IntPoint(0, 0), mData.mSize));
    }
  } else {
    surface = aContext->GetFrontBufferSnapshot(/* requireAlphaPremult */ false);
    if (surface) {
      auto surfaceImage = MakeRefPtr<layers::SourceSurfaceImage>(surface);
      surfaceImage->SetTextureFlags(flags);
      image = surfaceImage;
    }
  }

  if (mData.mDoPaintCallbacks) {
    aContext->OnDidPaintTransaction();
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
  mMutex.AssertCurrentThreadOwns();

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
OffscreenCanvasDisplayHelper::GetSurfaceSnapshot() {
  MOZ_ASSERT(NS_IsMainThread());

  Maybe<layers::SurfaceDescriptor> desc;

  bool hasAlpha;
  gl::OriginPos originPos;
  Maybe<uint32_t> managerId;
  Maybe<int32_t> childId;
  HTMLCanvasElement* canvasElement;

  {
    MutexAutoLock lock(mMutex);
    hasAlpha = !mData.mIsOpaque;
    originPos = mData.mOriginPos;
    managerId = mContextManagerId;
    childId = mContextChildId;
    canvasElement = mCanvasElement;
  }

  if (managerId && childId) {
    // The context lives in the compositor process.
    return gfx::CanvasManagerChild::Get()->GetSnapshot(
        managerId.value(), childId.value(), hasAlpha);
  }

  // If we don't have any protocol IDs, it is possible it is a main thread
  // OffscreenCanvas instance. If so, then the element's OffscreenCanvas is
  // not neutered and has access to the context. We can use that to get the
  // snapshot directly.
  if (!canvasElement) {
    return nullptr;
  }

  const auto* offscreenCanvas = canvasElement->GetOffscreenCanvas();
  nsICanvasRenderingContextInternal* context = offscreenCanvas->GetContext();
  if (!context) {
    return nullptr;
  }

  RefPtr<gfx::SourceSurface> surface =
      context->GetFrontBufferSnapshot(/* requireAlphaPremult */ true);
  if (!surface) {
    return nullptr;
  }

  switch (originPos) {
    case gl::OriginPos::BottomLeft: {
      RefPtr<gfx::DataSourceSurface> dataSurface = surface->GetDataSurface();
      if (!dataSurface) {
        return nullptr;
      }

      gfx::DataSourceSurface::ScopedMap map(dataSurface,
                                            gfx::DataSourceSurface::READ_WRITE);
      if (!map.IsMapped()) {
        return nullptr;
      }

      // The buffer we read back from WebGL has its rows inverted.
      const gfx::SurfaceFormat format = surface->GetFormat();
      const gfx::IntSize size = surface->GetSize();
      if (!gfx::SwizzleYFlipData(map.GetData(), map.GetStride(), format,
                                 map.GetData(), map.GetStride(), format,
                                 size)) {
        return nullptr;
      }
    } break;
    case gl::OriginPos::TopLeft:
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled origin position!");
      break;
  }

  return surface.forget();
}

already_AddRefed<layers::Image> OffscreenCanvasDisplayHelper::GetAsImage() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<gfx::SourceSurface> surface = GetSurfaceSnapshot();
  if (!surface) {
    return nullptr;
  }
  return MakeAndAddRef<layers::SourceSurfaceImage>(surface);
}

}  // namespace mozilla::dom
