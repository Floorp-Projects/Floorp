/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageClient.h"
#include <stdint.h>                     // for uint32_t
#include "ImageContainer.h"             // for Image, PlanarYCbCrImage, etc
#include "ImageTypes.h"                 // for ImageFormat::PLANAR_YCBCR, etc
#include "SharedTextureImage.h"         // for SharedTextureImage::Data, etc
#include "gfx2DGlue.h"                  // for ImageFormatToSurfaceFormat
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr, TemporaryRef
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Types.h"          // for SurfaceFormat, etc
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/CompositorTypes.h"  // for CompositableType, etc
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder
#include "mozilla/layers/SharedPlanarYCbCrImage.h"
#include "mozilla/layers/SharedRGBImage.h"
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "mozilla/layers/TextureClientOGL.h"  // for SharedTextureClientOGL
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_WARNING, NS_ASSERTION
#include "nsISupportsImpl.h"            // for Image::Release, etc
#include "nsRect.h"                     // for nsIntRect
#include "mozilla/gfx/2D.h"
#ifdef MOZ_WIDGET_GONK
#include "GrallocImages.h"
#endif

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

/* static */ TemporaryRef<ImageClient>
ImageClient::CreateImageClient(CompositableType aCompositableHostType,
                               CompositableForwarder* aForwarder,
                               TextureFlags aFlags)
{
  RefPtr<ImageClient> result = nullptr;
  switch (aCompositableHostType) {
  case COMPOSITABLE_IMAGE:
  case BUFFER_IMAGE_SINGLE:
    if (gfxPlatform::GetPlatform()->UseDeprecatedTextures()) {
      result = new DeprecatedImageClientSingle(aForwarder, aFlags, BUFFER_IMAGE_SINGLE);
    } else {
      result = new ImageClientSingle(aForwarder, aFlags, COMPOSITABLE_IMAGE);
    }
    break;
  case BUFFER_IMAGE_BUFFERED:
    if (gfxPlatform::GetPlatform()->UseDeprecatedTextures()) {
      result = new DeprecatedImageClientSingle(aForwarder, aFlags, BUFFER_IMAGE_BUFFERED);
    } else {
      result = new ImageClientBuffered(aForwarder, aFlags, COMPOSITABLE_IMAGE);
    }
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

ImageClientSingle::ImageClientSingle(CompositableForwarder* aFwd,
                                     TextureFlags aFlags,
                                     CompositableType aType)
  : ImageClient(aFwd, aFlags, aType)
{
}

ImageClientBuffered::ImageClientBuffered(CompositableForwarder* aFwd,
                                         TextureFlags aFlags,
                                         CompositableType aType)
  : ImageClientSingle(aFwd, aFlags, aType)
{
}

TextureInfo ImageClientSingle::GetTextureInfo() const
{
  return TextureInfo(COMPOSITABLE_IMAGE);
}

void
ImageClientSingle::FlushAllImages(bool aExceptFront)
{
  if (!aExceptFront && mFrontBuffer) {
    GetForwarder()->RemoveTextureFromCompositable(this, mFrontBuffer);
    mFrontBuffer = nullptr;
  }
}

void
ImageClientBuffered::FlushAllImages(bool aExceptFront)
{
  if (!aExceptFront && mFrontBuffer) {
    GetForwarder()->RemoveTextureFromCompositable(this, mFrontBuffer);
    mFrontBuffer = nullptr;
  }
  if (mBackBuffer) {
    GetForwarder()->RemoveTextureFromCompositable(this, mBackBuffer);
    mBackBuffer = nullptr;
  }
}

bool
ImageClientSingle::UpdateImage(ImageContainer* aContainer,
                               uint32_t aContentFlags)
{
  bool isSwapped = false;
  return UpdateImageInternal(aContainer, aContentFlags, &isSwapped);
}

bool
ImageClientSingle::UpdateImageInternal(ImageContainer* aContainer,
                                       uint32_t aContentFlags, bool* aIsSwapped)
{
  AutoLockImage autoLock(aContainer);
  *aIsSwapped = false;

  Image *image = autoLock.GetImage();
  if (!image) {
    return false;
  }

  if (mLastPaintedImageSerial == image->GetSerial()) {
    return true;
  }

  if (image->AsSharedImage() && image->AsSharedImage()->GetTextureClient(this)) {
    // fast path: no need to allocate and/or copy image data
    RefPtr<TextureClient> texture = image->AsSharedImage()->GetTextureClient(this);


    if (mFrontBuffer) {
      GetForwarder()->RemoveTextureFromCompositable(this, mFrontBuffer);
    }
    mFrontBuffer = texture;
    if (!AddTextureClient(texture)) {
      mFrontBuffer = nullptr;
      return false;
    }
    GetForwarder()->UpdatedTexture(this, texture, nullptr);
    GetForwarder()->UseTexture(this, texture);
  } else if (image->GetFormat() == ImageFormat::PLANAR_YCBCR) {
    PlanarYCbCrImage* ycbcr = static_cast<PlanarYCbCrImage*>(image);
    const PlanarYCbCrData* data = ycbcr->GetData();
    if (!data) {
      return false;
    }

    if (mFrontBuffer && mFrontBuffer->IsImmutable()) {
      GetForwarder()->RemoveTextureFromCompositable(this, mFrontBuffer);
      mFrontBuffer = nullptr;
    }

    bool bufferCreated = false;
    if (!mFrontBuffer) {
      mFrontBuffer = CreateBufferTextureClient(gfx::SurfaceFormat::YUV, TEXTURE_FLAGS_DEFAULT);
      gfx::IntSize ySize(data->mYSize.width, data->mYSize.height);
      gfx::IntSize cbCrSize(data->mCbCrSize.width, data->mCbCrSize.height);
      if (!mFrontBuffer->AsTextureClientYCbCr()->AllocateForYCbCr(ySize, cbCrSize, data->mStereoMode)) {
        mFrontBuffer = nullptr;
        return false;
      }
      bufferCreated = true;
    }

    if (!mFrontBuffer->Lock(OPEN_WRITE_ONLY)) {
      return false;
    }
    bool status = mFrontBuffer->AsTextureClientYCbCr()->UpdateYCbCr(*data);
    mFrontBuffer->Unlock();

    if (bufferCreated) {
      if (!AddTextureClient(mFrontBuffer)) {
        mFrontBuffer = nullptr;
        return false;
      }
    }

    if (status) {
      GetForwarder()->UpdatedTexture(this, mFrontBuffer, nullptr);
      GetForwarder()->UseTexture(this, mFrontBuffer);
    } else {
      MOZ_ASSERT(false);
      return false;
    }

  } else if (image->GetFormat() == ImageFormat::SHARED_TEXTURE) {
    SharedTextureImage* sharedImage = static_cast<SharedTextureImage*>(image);
    const SharedTextureImage::Data *data = sharedImage->GetData();
    gfx::IntSize size = gfx::IntSize(image->GetSize().width, image->GetSize().height);

    if (mFrontBuffer) {
      GetForwarder()->RemoveTextureFromCompositable(this, mFrontBuffer);
      mFrontBuffer = nullptr;
    }

    RefPtr<SharedTextureClientOGL> buffer = new SharedTextureClientOGL(mTextureFlags);
    buffer->InitWith(data->mHandle, size, data->mShareType, data->mInverted);
    mFrontBuffer = buffer;
    if (!AddTextureClient(mFrontBuffer)) {
      mFrontBuffer = nullptr;
      return false;
    }

    GetForwarder()->UseTexture(this, mFrontBuffer);
  } else {
    RefPtr<gfx::SourceSurface> surface = image->GetAsSourceSurface();
    MOZ_ASSERT(surface);

    gfx::IntSize size = image->GetSize();

    if (mFrontBuffer &&
        (mFrontBuffer->IsImmutable() || mFrontBuffer->GetSize() != size)) {
      GetForwarder()->RemoveTextureFromCompositable(this, mFrontBuffer);
      mFrontBuffer = nullptr;
    }

    bool bufferCreated = false;
    if (!mFrontBuffer) {
      gfxImageFormat format
        = gfxPlatform::GetPlatform()->OptimalFormatForContent(gfx::ContentForFormat(surface->GetFormat()));
      mFrontBuffer = CreateTextureClientForDrawing(gfx::ImageFormatToSurfaceFormat(format),
                                                   mTextureFlags, gfx::BackendType::NONE, size);
      MOZ_ASSERT(mFrontBuffer->AsTextureClientDrawTarget());
      if (!mFrontBuffer->AsTextureClientDrawTarget()->AllocateForSurface(size)) {
        mFrontBuffer = nullptr;
        return false;
      }

      bufferCreated = true;
    }

    if (!mFrontBuffer->Lock(OPEN_WRITE_ONLY)) {
      return false;
    }

    {
      // We must not keep a reference to the DrawTarget after it has been unlocked.
      RefPtr<DrawTarget> dt = mFrontBuffer->AsTextureClientDrawTarget()->GetAsDrawTarget();
      MOZ_ASSERT(surface.get());
      dt->CopySurface(surface, IntRect(IntPoint(), surface->GetSize()), IntPoint());
    }

    mFrontBuffer->Unlock();

    if (bufferCreated) {
      if (!AddTextureClient(mFrontBuffer)) {
        mFrontBuffer = nullptr;
        return false;
      }
    }

    GetForwarder()->UpdatedTexture(this, mFrontBuffer, nullptr);
    GetForwarder()->UseTexture(this, mFrontBuffer);
  }

  UpdatePictureRect(image->GetPictureRect());

  mLastPaintedImageSerial = image->GetSerial();
  aContainer->NotifyPaintedImage(image);
  *aIsSwapped = true;
  return true;
}

bool
ImageClientBuffered::UpdateImage(ImageContainer* aContainer,
                                 uint32_t aContentFlags)
{
  RefPtr<TextureClient> temp = mFrontBuffer;
  mFrontBuffer = mBackBuffer;
  mBackBuffer = temp;

  bool isSwapped = false;
  bool ret = ImageClientSingle::UpdateImageInternal(aContainer, aContentFlags, &isSwapped);

  if (!isSwapped) {
    // If buffer swap did not happen at Host side, swap back the buffers.
    RefPtr<TextureClient> temp = mFrontBuffer;
    mFrontBuffer = mBackBuffer;
    mBackBuffer = temp;
  }
  return ret;
}

bool
ImageClientSingle::AddTextureClient(TextureClient* aTexture)
{
  MOZ_ASSERT((mTextureFlags & aTexture->GetFlags()) == mTextureFlags);
  return CompositableClient::AddTextureClient(aTexture);
}

void
ImageClientSingle::OnDetach()
{
  mFrontBuffer = nullptr;
}

void
ImageClientBuffered::OnDetach()
{
  mFrontBuffer = nullptr;
  mBackBuffer = nullptr;
}

ImageClient::ImageClient(CompositableForwarder* aFwd, TextureFlags aFlags,
                         CompositableType aType)
: CompositableClient(aFwd, aFlags)
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

DeprecatedImageClientSingle::DeprecatedImageClientSingle(CompositableForwarder* aFwd,
                                                         TextureFlags aFlags,
                                                         CompositableType aType)
  : ImageClient(aFwd, aFlags, aType)
  , mTextureInfo(aType)
{
  mTextureInfo.mTextureFlags = aFlags;
}

bool
DeprecatedImageClientSingle::EnsureDeprecatedTextureClient(DeprecatedTextureClientType aType)
{
  // We should not call this method if using ImageBridge or tiled texture
  // clients since SupportsType always fails
  if (mDeprecatedTextureClient && mDeprecatedTextureClient->SupportsType(aType)) {
    return true;
  }
  mDeprecatedTextureClient = CreateDeprecatedTextureClient(aType);
  return !!mDeprecatedTextureClient;
}

bool
DeprecatedImageClientSingle::UpdateImage(ImageContainer* aContainer,
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

  if (image->GetFormat() == ImageFormat::PLANAR_YCBCR &&
      EnsureDeprecatedTextureClient(TEXTURE_YCBCR)) {
    PlanarYCbCrImage* ycbcr = static_cast<PlanarYCbCrImage*>(image);

    if (ycbcr->AsDeprecatedSharedPlanarYCbCrImage()) {
      AutoLockDeprecatedTextureClient lock(mDeprecatedTextureClient);

      SurfaceDescriptor sd;
      if (!ycbcr->AsDeprecatedSharedPlanarYCbCrImage()->ToSurfaceDescriptor(sd)) {
        return false;
      }

      if (IsSurfaceDescriptorValid(*lock.GetSurfaceDescriptor())) {
        GetForwarder()->DestroySharedSurface(lock.GetSurfaceDescriptor());
      }

      *lock.GetSurfaceDescriptor() = sd;
    } else {
      AutoLockYCbCrClient clientLock(mDeprecatedTextureClient);

      if (!clientLock.Update(ycbcr)) {
        NS_WARNING("failed to update DeprecatedTextureClient (YCbCr)");
        return false;
      }
    }
  } else if (image->GetFormat() == ImageFormat::SHARED_TEXTURE &&
             EnsureDeprecatedTextureClient(TEXTURE_SHARED_GL_EXTERNAL)) {
    SharedTextureImage* sharedImage = static_cast<SharedTextureImage*>(image);
    const SharedTextureImage::Data *data = sharedImage->GetData();

    SharedTextureDescriptor texture(data->mShareType,
                                    data->mHandle,
                                    data->mSize,
                                    data->mInverted);
    mDeprecatedTextureClient->SetDescriptor(SurfaceDescriptor(texture));
  } else if (image->GetFormat() == ImageFormat::SHARED_RGB &&
             EnsureDeprecatedTextureClient(TEXTURE_SHMEM)) {
    nsIntRect rect(0, 0,
                   image->GetSize().width,
                   image->GetSize().height);
    UpdatePictureRect(rect);

    AutoLockDeprecatedTextureClient lock(mDeprecatedTextureClient);

    SurfaceDescriptor desc;
    if (!static_cast<DeprecatedSharedRGBImage*>(image)->ToSurfaceDescriptor(desc)) {
      return false;
    }
    mDeprecatedTextureClient->SetDescriptor(desc);
#ifdef MOZ_WIDGET_GONK
  } else if (image->GetFormat() == ImageFormat::GRALLOC_PLANAR_YCBCR) {
    EnsureDeprecatedTextureClient(TEXTURE_SHARED_GL_EXTERNAL);

    nsIntRect rect(0, 0,
                   image->GetSize().width,
                   image->GetSize().height);
    UpdatePictureRect(rect);

    AutoLockDeprecatedTextureClient lock(mDeprecatedTextureClient);

    SurfaceDescriptor desc = static_cast<GrallocImage*>(image)->GetSurfaceDescriptor();
    if (!IsSurfaceDescriptorValid(desc)) {
      return false;
    }
    mDeprecatedTextureClient->SetDescriptor(desc);
#endif
  } else {
    nsRefPtr<gfxASurface> surface = image->DeprecatedGetAsSurface();
    MOZ_ASSERT(surface);

    EnsureDeprecatedTextureClient(TEXTURE_SHMEM);
    MOZ_ASSERT(mDeprecatedTextureClient, "Failed to create texture client");

    AutoLockShmemClient clientLock(mDeprecatedTextureClient);
    if (!clientLock.Update(image, aContentFlags, surface)) {
      NS_WARNING("failed to update DeprecatedTextureClient");
      return false;
    }
  }

  Updated();

  if (image->GetFormat() == ImageFormat::PLANAR_YCBCR) {
    PlanarYCbCrImage* ycbcr = static_cast<PlanarYCbCrImage*>(image);
    UpdatePictureRect(ycbcr->GetData()->GetPictureRect());
  }

  mLastPaintedImageSerial = image->GetSerial();
  aContainer->NotifyPaintedImage(image);
  return true;
}

void
DeprecatedImageClientSingle::Updated()
{
  mForwarder->UpdateTexture(this, 1, mDeprecatedTextureClient->LockSurfaceDescriptor());
}

ImageClientBridge::ImageClientBridge(CompositableForwarder* aFwd,
                                     TextureFlags aFlags)
: ImageClient(aFwd, aFlags, BUFFER_BRIDGE)
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
ImageClientSingle::CreateImage(ImageFormat aFormat)
{
  nsRefPtr<Image> img;
  switch (aFormat) {
    case ImageFormat::PLANAR_YCBCR:
      img = new SharedPlanarYCbCrImage(this);
      return img.forget();
    case ImageFormat::SHARED_RGB:
      img = new SharedRGBImage(this);
      return img.forget();
#ifdef MOZ_WIDGET_GONK
    case ImageFormat::GRALLOC_PLANAR_YCBCR:
      img = new GrallocImage();
      return img.forget();
#endif
    default:
      return nullptr;
  }
}

already_AddRefed<Image>
DeprecatedImageClientSingle::CreateImage(ImageFormat aFormat)
{
  nsRefPtr<Image> img;
  switch (aFormat) {
    case ImageFormat::PLANAR_YCBCR:
      img = new DeprecatedSharedPlanarYCbCrImage(GetForwarder());
      return img.forget();
    case ImageFormat::SHARED_RGB:
      img = new DeprecatedSharedRGBImage(GetForwarder());
      return img.forget();
#ifdef MOZ_WIDGET_GONK
    case ImageFormat::GRALLOC_PLANAR_YCBCR:
      img = new GrallocImage();
      return img.forget();
#endif
    default:
      return nullptr;
  }
}


}
}
