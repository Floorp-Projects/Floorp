/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBitmapRenderingContext.h"
#include "mozilla/dom/ImageBitmapRenderingContextBinding.h"
#include "ImageContainer.h"
#include "ImageLayers.h"

namespace mozilla {
namespace dom {

ImageBitmapRenderingContext::ImageBitmapRenderingContext()
  : mWidth(0)
  , mHeight(0)
{
}

ImageBitmapRenderingContext::~ImageBitmapRenderingContext()
{
  RemovePostRefreshObserver();
}

JSObject*
ImageBitmapRenderingContext::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ImageBitmapRenderingContextBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<layers::Image>
ImageBitmapRenderingContext::ClipToIntrinsicSize()
{
  if (!mImage) {
    return nullptr;
  }

  // If image is larger than canvas intrinsic size, clip it to the intrinsic size.
  RefPtr<gfx::SourceSurface> surface;
  RefPtr<layers::Image> result;
  if (mWidth < mImage->GetSize().width ||
      mHeight < mImage->GetSize().height) {
    surface = MatchWithIntrinsicSize();
  } else {
    surface = mImage->GetAsSourceSurface();
  }
  result = new layers::SourceSurfaceImage(gfx::IntSize(mWidth, mHeight), surface);
  return result.forget();
}

void
ImageBitmapRenderingContext::TransferImageBitmap(ImageBitmap& aImageBitmap)
{
  Reset();
  mImage = aImageBitmap.TransferAsImage();

  if (!mImage) {
    return;
  }

  Redraw(gfxRect(0, 0, mWidth, mHeight));
}

int32_t
ImageBitmapRenderingContext::GetWidth() const
{
  return mWidth;
}

int32_t
ImageBitmapRenderingContext::GetHeight() const
{
  return mHeight;
}

NS_IMETHODIMP
ImageBitmapRenderingContext::SetDimensions(int32_t aWidth, int32_t aHeight)
{
  mWidth = aWidth;
  mHeight = aHeight;
  return NS_OK;
}

NS_IMETHODIMP
ImageBitmapRenderingContext::InitializeWithSurface(nsIDocShell* aDocShell,
                                                   gfxASurface* aSurface,
                                                   int32_t aWidth,
                                                   int32_t aHeight)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

already_AddRefed<DataSourceSurface>
ImageBitmapRenderingContext::MatchWithIntrinsicSize()
{
  RefPtr<SourceSurface> surface = mImage->GetAsSourceSurface();
  RefPtr<DataSourceSurface> temp =
    Factory::CreateDataSourceSurface(IntSize(mWidth, mHeight), surface->GetFormat());
  if (!temp) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap map(temp, DataSourceSurface::READ_WRITE);
  if (!map.IsMapped()) {
    return nullptr;
  }

  RefPtr<DrawTarget> dt =
    Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                     map.GetData(),
                                     temp->GetSize(),
                                     map.GetStride(),
                                     temp->GetFormat());
  if (!dt) {
    return nullptr;
  }


  dt->ClearRect(Rect(0, 0, mWidth, mHeight));
  dt->CopySurface(surface,
                  IntRect(0, 0, surface->GetSize().width,
                                surface->GetSize().height),
                  IntPoint(0, 0));

  return temp.forget();
}

mozilla::UniquePtr<uint8_t[]>
ImageBitmapRenderingContext::GetImageBuffer(int32_t* aFormat)
{
  *aFormat = 0;

  if (!mImage) {
    return nullptr;
  }

  RefPtr<SourceSurface> surface = mImage->GetAsSourceSurface();
  RefPtr<DataSourceSurface> data = surface->GetDataSurface();
  if (!data) {
    return nullptr;
  }

  if (data->GetSize() != IntSize(mWidth, mHeight)) {
    data = MatchWithIntrinsicSize();
  }

  *aFormat = imgIEncoder::INPUT_FORMAT_HOSTARGB;
  return SurfaceToPackedBGRA(data);
}

NS_IMETHODIMP
ImageBitmapRenderingContext::GetInputStream(const char* aMimeType,
                                            const char16_t* aEncoderOptions,
                                            nsIInputStream** aStream)
{
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

  return ImageEncoder::GetInputStream(mWidth, mHeight, imageBuffer.get(), format,
                                      encoder, aEncoderOptions, aStream);
}

already_AddRefed<mozilla::gfx::SourceSurface>
ImageBitmapRenderingContext::GetSurfaceSnapshot(bool* aPremultAlpha)
{
  if (!mImage) {
    return nullptr;
  }

  if (aPremultAlpha) {
    *aPremultAlpha = true;
  }

  RefPtr<SourceSurface> surface = mImage->GetAsSourceSurface();
  if (surface->GetSize() != IntSize(mWidth, mHeight)) {
    return MatchWithIntrinsicSize();
  }

  return surface.forget();
}

NS_IMETHODIMP
ImageBitmapRenderingContext::SetIsOpaque(bool aIsOpaque)
{
  return NS_OK;
}

bool
ImageBitmapRenderingContext::GetIsOpaque()
{
  return false;
}

NS_IMETHODIMP
ImageBitmapRenderingContext::Reset()
{
  if (mCanvasElement) {
    mCanvasElement->InvalidateCanvas();
  }

  mImage = nullptr;

  return NS_OK;
}

already_AddRefed<Layer>
ImageBitmapRenderingContext::GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                            Layer* aOldLayer,
                                            LayerManager* aManager)
{
  if (!mImage) {
    // No DidTransactionCallback will be received, so mark the context clean
    // now so future invalidations will be dispatched.
    MarkContextClean();
    return nullptr;
  }

  RefPtr<ImageLayer> imageLayer;

  if (aOldLayer) {
    imageLayer = static_cast<ImageLayer*>(aOldLayer);
  } else {
    imageLayer = aManager->CreateImageLayer();
  }

  RefPtr<ImageContainer> imageContainer = imageLayer->GetContainer();
  if (!imageContainer) {
    imageContainer = aManager->CreateImageContainer();
    imageLayer->SetContainer(imageContainer);
  }

  nsAutoTArray<ImageContainer::NonOwningImage, 1> imageList;
  RefPtr<layers::Image> image = ClipToIntrinsicSize();
  imageList.AppendElement(ImageContainer::NonOwningImage(image));
  imageContainer->SetCurrentImages(imageList);

  return imageLayer.forget();
}

void
ImageBitmapRenderingContext::MarkContextClean()
{
}

NS_IMETHODIMP
ImageBitmapRenderingContext::Redraw(const gfxRect& aDirty)
{
  if (!mCanvasElement) {
    return NS_OK;
  }

  mozilla::gfx::Rect rect = ToRect(aDirty);
  mCanvasElement->InvalidateCanvasContent(&rect);
  return NS_OK;
}

NS_IMETHODIMP
ImageBitmapRenderingContext::SetIsIPC(bool aIsIPC)
{
  return NS_OK;
}

void
ImageBitmapRenderingContext::DidRefresh()
{
}

void
ImageBitmapRenderingContext::MarkContextCleanForFrameCapture()
{
}

bool
ImageBitmapRenderingContext::IsContextCleanForFrameCapture()
{
  return true;
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(ImageBitmapRenderingContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImageBitmapRenderingContext)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ImageBitmapRenderingContext,
  mCanvasElement,
  mOffscreenCanvas)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageBitmapRenderingContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

}
}
