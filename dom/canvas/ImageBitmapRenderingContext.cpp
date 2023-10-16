/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBitmapRenderingContext.h"
#include "gfxPlatform.h"
#include "gfx2DGlue.h"
#include "mozilla/dom/ImageBitmapRenderingContextBinding.h"
#include "mozilla/gfx/Types.h"
#include "nsComponentManagerUtils.h"
#include "nsRegion.h"
#include "ImageContainer.h"

namespace mozilla::dom {

ImageBitmapRenderingContext::ImageBitmapRenderingContext()
    : mWidth(0),
      mHeight(0),
      mFrameCaptureState(FrameCaptureState::CLEAN,
                         "ImageBitmapRenderingContext::mFrameCaptureState") {}

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

void ImageBitmapRenderingContext::GetCanvas(
    Nullable<OwningHTMLCanvasElementOrOffscreenCanvas>& retval) const {
  if (mCanvasElement && !mCanvasElement->IsInNativeAnonymousSubtree()) {
    retval.SetValue().SetAsHTMLCanvasElement() = mCanvasElement;
  } else if (mOffscreenCanvas) {
    retval.SetValue().SetAsOffscreenCanvas() = mOffscreenCanvas;
  } else {
    retval.SetNull();
  }
}

void ImageBitmapRenderingContext::TransferImageBitmap(ImageBitmap& aImageBitmap,
                                                      ErrorResult& aRv) {
  TransferFromImageBitmap(&aImageBitmap, aRv);
}

void ImageBitmapRenderingContext::TransferFromImageBitmap(
    ImageBitmap* aImageBitmap, ErrorResult& aRv) {
  ResetBitmap();

  if (aImageBitmap) {
    mImage = aImageBitmap->TransferAsImage();

    if (!mImage) {
      aRv.ThrowInvalidStateError("The input ImageBitmap has been detached");
      return;
    }

    // Note that this is reentrant and will call back into SetDimensions.
    if (mCanvasElement) {
      mCanvasElement->SetSize(mImage->GetSize(), aRv);
    } else if (mOffscreenCanvas) {
      mOffscreenCanvas->SetSize(mImage->GetSize(), aRv);
    }

    if (NS_WARN_IF(aRv.Failed())) {
      mImage = nullptr;
      return;
    }

    if (aImageBitmap->IsWriteOnly()) {
      if (mCanvasElement) {
        mCanvasElement->SetWriteOnly();
      } else if (mOffscreenCanvas) {
        mOffscreenCanvas->SetWriteOnly();
      }
    }
  }

  Redraw(gfxRect(0, 0, mWidth, mHeight));
}

NS_IMETHODIMP
ImageBitmapRenderingContext::SetDimensions(int32_t aWidth, int32_t aHeight) {
  mWidth = aWidth;
  mHeight = aHeight;

  if (mOffscreenCanvas) {
    OffscreenCanvasDisplayData data;
    data.mSize = {mWidth, mHeight};
    data.mIsOpaque = GetIsOpaque();
    data.mIsAlphaPremult = true;
    data.mDoPaintCallbacks = false;
    mOffscreenCanvas->UpdateDisplayData(data);
  }

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
  if (!surface) {
    return nullptr;
  }
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
    int32_t* aFormat, gfx::IntSize* aImageSize) {
  *aFormat = 0;
  *aImageSize = {};

  if (!mImage) {
    return nullptr;
  }

  RefPtr<gfx::SourceSurface> surface = mImage->GetAsSourceSurface();
  if (!surface) {
    return nullptr;
  }
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
  *aImageSize = data->GetSize();

  UniquePtr<uint8_t[]> ret = gfx::SurfaceToPackedBGRA(data);

  if (ret && ShouldResistFingerprinting(RFPTarget::CanvasRandomization)) {
    nsRFPService::RandomizePixels(
        GetCookieJarSettings(), ret.get(), data->GetSize().width,
        data->GetSize().height,
        data->GetSize().width * data->GetSize().height * 4,
        gfx::SurfaceFormat::A8R8G8B8_UINT32);
  }
  return ret;
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
  gfx::IntSize imageSize = {};
  UniquePtr<uint8_t[]> imageBuffer = GetImageBuffer(&format, &imageSize);
  if (!imageBuffer) {
    return NS_ERROR_FAILURE;
  }

  return ImageEncoder::GetInputStream(imageSize.width, imageSize.height,
                                      imageBuffer.get(), format, encoder,
                                      aEncoderOptions, aStream);
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
  if (!surface) {
    return nullptr;
  }

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

void ImageBitmapRenderingContext::ResetBitmap() {
  if (mCanvasElement) {
    mCanvasElement->InvalidateCanvas();
  }

  mImage = nullptr;
  mFrameCaptureState = FrameCaptureState::CLEAN;
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
  mFrameCaptureState = FrameCaptureState::DIRTY;

  if (mOffscreenCanvas) {
    mOffscreenCanvas->CommitFrameToCompositor();
  } else if (mCanvasElement) {
    mozilla::gfx::Rect rect = ToRect(aDirty);
    mCanvasElement->InvalidateCanvasContent(&rect);
  }

  return NS_OK;
}

void ImageBitmapRenderingContext::DidRefresh() {}

NS_IMPL_CYCLE_COLLECTING_ADDREF(ImageBitmapRenderingContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImageBitmapRenderingContext)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK_PTR(ImageBitmapRenderingContext,
                                               mCanvasElement, mOffscreenCanvas)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageBitmapRenderingContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

}  // namespace mozilla::dom
