/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/ImageClient.h"
#include "BasicLayers.h"
#include "mozilla/layers/ShadowLayers.h"
#include "SharedTextureImage.h"
#include "ImageContainer.h" // For PlanarYCbCrImage
#include "mozilla/layers/SharedRGBImage.h"
#include "mozilla/layers/SharedPlanarYCbCrImage.h"

#ifdef MOZ_WIDGET_GONK
#include "GonkIOSurfaceImage.h"
#include "GrallocImages.h"
#endif

namespace mozilla {
namespace layers {

/* static */ TemporaryRef<ImageClient>
ImageClient::CreateImageClient(CompositableType aCompositableHostType,
                               CompositableForwarder* aForwarder,
                               TextureFlags aFlags)
{
  RefPtr<ImageClient> result = nullptr;
  switch (aCompositableHostType) {
  case BUFFER_IMAGE_SINGLE:
    result = new ImageClientSingle(aForwarder, aFlags, BUFFER_IMAGE_SINGLE);
    break;
  case BUFFER_IMAGE_BUFFERED:
    result = new ImageClientSingle(aForwarder, aFlags, BUFFER_IMAGE_BUFFERED);
    break;
  case BUFFER_BRIDGE:
    result = new ImageClientBridge(aForwarder, aFlags);
    break;
  case BUFFER_UNKNOWN:
    result = nullptr;
    break;
  default:
    MOZ_CRASH("unhandled program type");
  }

  NS_ASSERTION(result, "Failed to create ImageClient");

  return result.forget();
}


ImageClient::ImageClient(CompositableForwarder* aFwd, CompositableType aType)
: CompositableClient(aFwd)
, mFilter(gfxPattern::FILTER_GOOD)
, mType(aType)
, mLastPaintedImageSerial(0)
{}

void
ImageClient::UpdatePictureRect(nsIntRect aRect)
{
  if (mPictureRect == aRect) {
    return;
  }
  mPictureRect = aRect;
  MOZ_ASSERT(mForwarder);
  GetForwarder()->UpdatePictureRect(this, aRect);
}

ImageClientSingle::ImageClientSingle(CompositableForwarder* aFwd,
                                     TextureFlags aFlags,
                                     CompositableType aType)
  : ImageClient(aFwd, aType)
  , mTextureInfo(aType)
{
  mTextureInfo.mTextureFlags = aFlags;
}

bool
ImageClientSingle::EnsureTextureClient(TextureClientType aType)
{
  // We should not call this method if using ImageBridge or tiled texture
  // clients since SupportsType always fails
  if (mTextureClient && mTextureClient->SupportsType(aType)) {
    return true;
  }
  mTextureClient = CreateTextureClient(aType);
  return !!mTextureClient;
}

bool
ImageClientSingle::UpdateImage(ImageContainer* aContainer,
                               uint32_t aContentFlags)
{
  AutoLockImage autoLock(aContainer);

  Image *image = autoLock.GetImage();
  if (!image) {
    return false;
  }

  if (mLastPaintedImageSerial == image->GetSerial()) {
    return true;
  }

  if (image->GetFormat() == PLANAR_YCBCR &&
      EnsureTextureClient(TEXTURE_YCBCR)) {
    PlanarYCbCrImage* ycbcr = static_cast<PlanarYCbCrImage*>(image);

    if (ycbcr->AsSharedPlanarYCbCrImage()) {
      AutoLockTextureClient lock(mTextureClient);

      SurfaceDescriptor sd;
      if (!ycbcr->AsSharedPlanarYCbCrImage()->ToSurfaceDescriptor(sd)) {
        return false;
      }

      if (IsSurfaceDescriptorValid(*lock.GetSurfaceDescriptor())) {
        GetForwarder()->DestroySharedSurface(lock.GetSurfaceDescriptor());
      }

      *lock.GetSurfaceDescriptor() = sd;
    } else {
      AutoLockYCbCrClient clientLock(mTextureClient);

      if (!clientLock.Update(ycbcr)) {
        NS_WARNING("failed to update TextureClient (YCbCr)");
        return false;
      }
    }
  } else if (image->GetFormat() == SHARED_TEXTURE &&
             EnsureTextureClient(TEXTURE_SHARED_GL_EXTERNAL)) {
    SharedTextureImage* sharedImage = static_cast<SharedTextureImage*>(image);
    const SharedTextureImage::Data *data = sharedImage->GetData();

    SharedTextureDescriptor texture(data->mShareType,
                                    data->mHandle,
                                    data->mSize,
                                    data->mInverted);
    mTextureClient->SetDescriptor(SurfaceDescriptor(texture));
  } else if (image->GetFormat() == SHARED_RGB &&
             EnsureTextureClient(TEXTURE_SHMEM)) {
    nsIntRect rect(0, 0,
                   image->GetSize().width,
                   image->GetSize().height);
    UpdatePictureRect(rect);

    AutoLockTextureClient lock(mTextureClient);

    SurfaceDescriptor desc;
    if (!static_cast<SharedRGBImage*>(image)->ToSurfaceDescriptor(desc)) {
      return false;
    }
    mTextureClient->SetDescriptor(desc);
#ifdef MOZ_WIDGET_GONK
  } else if (image->GetFormat() == GONK_IO_SURFACE &&
             EnsureTextureClient(TEXTURE_SHARED_GL_EXTERNAL)) {
    nsIntRect rect(0, 0,
                   image->GetSize().width,
                   image->GetSize().height);
    UpdatePictureRect(rect);

    AutoLockTextureClient lock(mTextureClient);

    SurfaceDescriptor desc = static_cast<GonkIOSurfaceImage*>(image)->GetSurfaceDescriptor();
    if (!IsSurfaceDescriptorValid(desc)) {
      return false;
    }
    mTextureClient->SetDescriptor(desc);
  } else if (image->GetFormat() == GRALLOC_PLANAR_YCBCR) {
    EnsureTextureClient(TEXTURE_SHARED_GL_EXTERNAL);

    nsIntRect rect(0, 0,
                   image->GetSize().width,
                   image->GetSize().height);
    UpdatePictureRect(rect);

    AutoLockTextureClient lock(mTextureClient);

    SurfaceDescriptor desc = static_cast<GrallocPlanarYCbCrImage*>(image)->GetSurfaceDescriptor();
    if (!IsSurfaceDescriptorValid(desc)) {
      return false;
    }
    mTextureClient->SetDescriptor(desc);
#endif
  } else {
    nsRefPtr<gfxASurface> surface = image->GetAsSurface();
    MOZ_ASSERT(surface);

    EnsureTextureClient(TEXTURE_SHMEM);
    MOZ_ASSERT(mTextureClient, "Failed to create texture client");

    nsRefPtr<gfxPattern> pattern = new gfxPattern(surface);
    pattern->SetFilter(mFilter);

    AutoLockShmemClient clientLock(mTextureClient);
    if (!clientLock.Update(image, aContentFlags, pattern)) {
      NS_WARNING("failed to update TextureClient");
      return false;
    }
  }

  Updated();

  if (image->GetFormat() == PLANAR_YCBCR) {
    PlanarYCbCrImage* ycbcr = static_cast<PlanarYCbCrImage*>(image);
    UpdatePictureRect(ycbcr->GetData()->GetPictureRect());
  }

  mLastPaintedImageSerial = image->GetSerial();
  aContainer->NotifyPaintedImage(image);
  return true;
}

void
ImageClientSingle::Updated()
{
  mForwarder->UpdateTexture(this, 1, mTextureClient->GetDescriptor());
}

ImageClientBridge::ImageClientBridge(CompositableForwarder* aFwd,
                                     TextureFlags aFlags)
: ImageClient(aFwd, BUFFER_BRIDGE)
, mAsyncContainerID(0)
, mLayer(nullptr)
{
}

bool
ImageClientBridge::UpdateImage(ImageContainer* aContainer, uint32_t aContentFlags)
{
  if (!GetForwarder() || !mLayer) {
    return false;
  }
  if (mAsyncContainerID == aContainer->GetAsyncContainerID()) {
    return true;
  }
  mAsyncContainerID = aContainer->GetAsyncContainerID();
  static_cast<ShadowLayerForwarder*>(GetForwarder())->AttachAsyncCompositable(mAsyncContainerID, mLayer);
  AutoLockImage autoLock(aContainer);
  aContainer->NotifyPaintedImage(autoLock.GetImage());
  Updated();
  return true;
}

already_AddRefed<Image>
ImageClient::CreateImage(const uint32_t *aFormats,
                         uint32_t aNumFormats)
{
  nsRefPtr<Image> img;
  for (uint32_t i = 0; i < aNumFormats; i++) {
    switch (aFormats[i]) {
      case PLANAR_YCBCR:
        img = new SharedPlanarYCbCrImage(GetForwarder());
        return img.forget();
      case SHARED_RGB:
        img = new SharedRGBImage(GetForwarder());
        return img.forget();
#ifdef MOZ_WIDGET_GONK
      case GONK_IO_SURFACE:
        img = new GonkIOSurfaceImage();
        return img.forget();
      case GRALLOC_PLANAR_YCBCR:
        img = new GrallocPlanarYCbCrImage();
        return img.forget();
#endif
    }
  }
  return nullptr;
}

}
}
