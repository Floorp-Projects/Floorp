/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBitmapRenderingContext.h"
#include "gfxPlatform.h"
#include "gfx2DGlue.h"
#include "mozilla/dom/ImageBitmapRenderingContextBinding.h"
#include "nsComponentManagerUtils.h"
#include "nsRegion.h"
#include "ImageContainer.h"

namespace mozilla::dom {

ImageBitmapRenderingContext::ImageBitmapRenderingContext()
    : mWidth(0), mHeight(0), mIsCapturedFrameInvalid(false) {}

ImageBitmapRenderingContext::~ImageBitmapRenderingContext() {
  RemovePostRefreshObserver();
}

JSObject* ImageBitmapRenderingContext::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return ImageBitmapRenderingContext_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<layers::Image>
ImageBitmapRenderingContext::ClipToIntrinsicSize() {
  if (!mImage) {
    return nullptr;
  }

  // If image is larger than canvas intrinsic size, clip it to the intrinsic
  // size.
  RefPtr<gfx::SourceSurface> surface;
  RefPtr<layers::Image> result;
  if (mWidth < mImage->GetSize().width || mHeight < mImage->GetSize().height) {
    surface = MatchWithIntrinsicSize();
  } else {
    surface = mImage->GetAsSourceSurface();
  }
  if (!surface) {
    return nullptr;
  }
  result =
      new layers::SourceSurfaceImage(gfx::IntSize(mWidth, mHeight), surface);
  return result.forget();
}

void ImageBitmapRenderingContext::TransferImageBitmap(
    ImageBitmap& aImageBitmap) {
  TransferFromImageBitmap(aImageBitmap);
}

void ImageBitmapRenderingContext::TransferFromImageBitmap(
    ImageBitmap& aImageBitmap) {
  Reset();
  mImage = aImageBitmap.TransferAsImage();

  if (!mImage) {
    return;
  }

  if (aImageBitmap.IsWriteOnly() && mCanvasElement) {
    mCanvasElement->SetWriteOnly();
  }

  Redraw(gfxRect(0, 0, mWidth, mHeight));
}

NS_IMETHODIMP
ImageBitmapRenderingContext::SetDimensions(int32_t aWidth, int32_t aHeight) {
  mWidth = aWidth;
  mHeight = aHeight;
  return NS_OK;
}

NS_IMETHODIMP
ImageBitmapRenderingContext::InitializeWithDrawTarget(
    nsIDocShell* aDocShell, NotNull<gfx::DrawTarget*> aTarget) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

already_AddRefed<gfx::DataSourceSurface>
ImageBitmapRenderingContext::MatchWithIntrinsicSize() {
  RefPtr<gfx::SourceSurface> surface = mImage->GetAsSourceSurface();
  RefPtr<gfx::DataSourceSurface> temp = gfx::Factory::CreateDataSourceSurface(
      gfx::IntSize(mWidth, mHeight), surface->GetFormat());
  if (!temp) {
    return nullptr;
  }

  gfx::DataSourceSurface::ScopedMap map(temp,
                                        gfx::DataSourceSurface::READ_WRITE);
  if (!map.IsMapped()) {
    return nullptr;
  }

  RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateDrawTargetForData(
      gfxPlatform::GetPlatform()->GetSoftwareBackend(), map.GetData(),
      temp->GetSize(), map.GetStride(), temp->GetFormat());
  if (!dt || !dt->IsValid()) {
    gfxWarning()
        << "ImageBitmapRenderingContext::MatchWithIntrinsicSize failed";
    return nullptr;
  }

  dt->ClearRect(gfx::Rect(0, 0, mWidth, mHeight));
  dt->CopySurface(
      surface,
      gfx::IntRect(0, 0, surface->GetSize().width, surface->GetSize().height),
      gfx::IntPoint(0, 0));

  return temp.forget();
}

mozilla::UniquePtr<uint8_t[]> ImageBitmapRenderingContext::GetImageBuffer(
    int32_t* aFormat) {
  *aFormat = 0;

  if (!mImage) {
    return nullptr;
  }

  RefPtr<gfx::SourceSurface> surface = mImage->GetAsSourceSurface();
  RefPtr<gfx::DataSourceSurface> data = surface->GetDataSurface();
  if (!data) {
    return nullptr;
  }

  if (data->GetSize() != gfx::IntSize(mWidth, mHeight)) {
    data = MatchWithIntrinsicSize();
    if (!data) {
      return nullptr;
    }
  }

  *aFormat = imgIEncoder::INPUT_FORMAT_HOSTARGB;
  return gfx::SurfaceToPackedBGRA(data);
}

NS_IMETHODIMP
ImageBitmapRenderingContext::GetInputStream(const char* aMimeType,
                                            const nsAString& aEncoderOptions,
                                            nsIInputStream** aStream) {
  nsCString enccid("@mozilla.org/image/encoder;2?type=");
  enccid += aMimeType;
  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(enccid.get());
  if (!encoder) {
    return NS_ERROR_FAILURE;
  }

  int32_t format = 0;
  UniquePtr<uint8_t[]> imageBuffer = GetImageBuffer(&format);
  if (!imageBuffer) {
    return NS_ERROR_FAILURE;
  }

  return ImageEncoder::GetInputStream(mWidth, mHeight, imageBuffer.get(),
                                      format, encoder, aEncoderOptions,
                                      aStream);
}

already_AddRefed<mozilla::gfx::SourceSurface>
ImageBitmapRenderingContext::GetSurfaceSnapshot(
    gfxAlphaType* const aOutAlphaType) {
  if (!mImage) {
    return nullptr;
  }

  if (aOutAlphaType) {
    *aOutAlphaType =
        (GetIsOpaque() ? gfxAlphaType::Opaque : gfxAlphaType::Premult);
  }

  RefPtr<gfx::SourceSurface> surface = mImage->GetAsSourceSurface();
  if (surface->GetSize() != gfx::IntSize(mWidth, mHeight)) {
    return MatchWithIntrinsicSize();
  }

  return surface.forget();
}

void ImageBitmapRenderingContext::SetOpaqueValueFromOpaqueAttr(
    bool aOpaqueAttrValue) {
  // ignored
}

bool ImageBitmapRenderingContext::GetIsOpaque() { return false; }

NS_IMETHODIMP
ImageBitmapRenderingContext::Reset() {
  if (mCanvasElement) {
    mCanvasElement->InvalidateCanvas();
  }

  mImage = nullptr;
  mIsCapturedFrameInvalid = false;
  return NS_OK;
}

bool ImageBitmapRenderingContext::UpdateWebRenderCanvasData(
    nsDisplayListBuilder* aBuilder, WebRenderCanvasData* aCanvasData) {
  if (!mImage) {
    // No DidTransactionCallback will be received, so mark the context clean
    // now so future invalidations will be dispatched.
    MarkContextClean();
    return false;
  }

  RefPtr<layers::ImageContainer> imageContainer =
      aCanvasData->GetImageContainer();
  AutoTArray<layers::ImageContainer::NonOwningImage, 1> imageList;
  RefPtr<layers::Image> image = ClipToIntrinsicSize();
  if (!image) {
    return false;
  }

  imageList.AppendElement(layers::ImageContainer::NonOwningImage(image));
  imageContainer->SetCurrentImages(imageList);
  return true;
}

void ImageBitmapRenderingContext::MarkContextClean() {}

NS_IMETHODIMP
ImageBitmapRenderingContext::Redraw(const gfxRect& aDirty) {
  mIsCapturedFrameInvalid = true;

  if (mOffscreenCanvas) {
    RefPtr<layers::ImageContainer> imageContainer =
        mOffscreenCanvas->GetImageContainer();
    if (imageContainer) {
      RefPtr<layers::Image> image = ClipToIntrinsicSize();
      if (image) {
        AutoTArray<layers::ImageContainer::NonOwningImage, 1> imageList;
        imageList.AppendElement(layers::ImageContainer::NonOwningImage(image));
        imageContainer->SetCurrentImages(imageList);
      } else {
        imageContainer->ClearAllImages();
      }
    }
    mOffscreenCanvas->CommitFrameToCompositor();
  }

  if (!mCanvasElement) {
    return NS_OK;
  }

  mozilla::gfx::Rect rect = ToRect(aDirty);
  mCanvasElement->InvalidateCanvasContent(&rect);
  return NS_OK;
}

NS_IMETHODIMP
ImageBitmapRenderingContext::SetIsIPC(bool aIsIPC) { return NS_OK; }

void ImageBitmapRenderingContext::DidRefresh() {}

void ImageBitmapRenderingContext::MarkContextCleanForFrameCapture() {
  mIsCapturedFrameInvalid = false;
}

bool ImageBitmapRenderingContext::IsContextCleanForFrameCapture() {
  return !mIsCapturedFrameInvalid;
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(ImageBitmapRenderingContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImageBitmapRenderingContext)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ImageBitmapRenderingContext,
                                      mCanvasElement, mOffscreenCanvas)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageBitmapRenderingContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

}  // namespace mozilla::dom
