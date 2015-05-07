/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGECLIENT_H
#define MOZILLA_GFX_IMAGECLIENT_H

#include <stdint.h>                     // for uint32_t, uint64_t
#include <sys/types.h>                  // for int32_t
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr, TemporaryRef
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "mozilla/layers/AsyncTransactionTracker.h" // for AsyncTransactionTracker
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/CompositorTypes.h"  // for CompositableType, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsRect.h"                     // for mozilla::gfx::IntRect

namespace mozilla {
namespace layers {

class CompositableForwarder;
class AsyncTransactionTracker;
class Image;
class ImageContainer;
class ShadowableLayer;

/**
 * Image clients are used by basic image layers on the content thread, they
 * always match with an ImageHost on the compositor thread. See
 * CompositableClient.h for information on connecting clients to hosts.
 */
class ImageClient : public CompositableClient
{
public:
  /**
   * Creates, configures, and returns a new image client. If necessary, a
   * message will be sent to the compositor to create a corresponding image
   * host.
   */
  static TemporaryRef<ImageClient> CreateImageClient(CompositableType aImageHostType,
                                                     CompositableForwarder* aFwd,
                                                     TextureFlags aFlags);

  virtual ~ImageClient() {}

  /**
   * Update this ImageClient from aContainer in aLayer
   * returns false if this is the wrong kind of ImageClient for aContainer.
   * Note that returning true does not necessarily imply success
   */
  virtual bool UpdateImage(ImageContainer* aContainer, uint32_t aContentFlags) = 0;

  /**
   * The picture rect is the area of the texture which makes up the image. That
   * is, the area that should be composited. In texture space.
   */
  virtual void UpdatePictureRect(gfx::IntRect aPictureRect);

  virtual already_AddRefed<Image> CreateImage(ImageFormat aFormat) = 0;

  /**
   * Create AsyncTransactionTracker that is used for FlushAllImagesAsync().
   */
  virtual TemporaryRef<AsyncTransactionTracker> PrepareFlushAllImages() { return nullptr; }

  /**
   * asynchronously remove all the textures used by the image client.
   *
   */
  virtual void FlushAllImages(bool aExceptFront,
                              AsyncTransactionTracker* aAsyncTransactionTracker) {}

  virtual void RemoveTexture(TextureClient* aTexture) override;

  void RemoveTextureWithTracker(TextureClient* aTexture,
                                AsyncTransactionTracker* aAsyncTransactionTracker = nullptr);

protected:
  ImageClient(CompositableForwarder* aFwd, TextureFlags aFlags,
              CompositableType aType);

  CompositableType mType;
  int32_t mLastPaintedImageSerial;
  gfx::IntRect mPictureRect;
};

/**
 * An image client which uses a single texture client.
 */
class ImageClientSingle : public ImageClient
{
public:
  ImageClientSingle(CompositableForwarder* aFwd,
                    TextureFlags aFlags,
                    CompositableType aType);

  virtual bool UpdateImage(ImageContainer* aContainer, uint32_t aContentFlags) override;

  virtual void OnDetach() override;

  virtual bool AddTextureClient(TextureClient* aTexture) override;

  virtual TextureInfo GetTextureInfo() const override;

  virtual already_AddRefed<Image> CreateImage(ImageFormat aFormat) override;

  virtual TemporaryRef<AsyncTransactionTracker> PrepareFlushAllImages() override;

  virtual void FlushAllImages(bool aExceptFront,
                              AsyncTransactionTracker* aAsyncTransactionTracker) override;

protected:
  RefPtr<TextureClient> mFrontBuffer;
};

/**
 * Image class to be used for async image uploads using the image bridge
 * protocol.
 * We store the ImageBridge id in the TextureClientIdentifier.
 */
class ImageClientBridge : public ImageClient
{
public:
  ImageClientBridge(CompositableForwarder* aFwd,
                    TextureFlags aFlags);

  virtual bool UpdateImage(ImageContainer* aContainer, uint32_t aContentFlags) override;
  virtual bool Connect() override { return false; }
  virtual void Updated() {}
  void SetLayer(ShadowableLayer* aLayer)
  {
    mLayer = aLayer;
  }

  virtual TextureInfo GetTextureInfo() const override
  {
    return TextureInfo(mType);
  }

  virtual void SetIPDLActor(CompositableChild* aChild) override
  {
    MOZ_ASSERT(!aChild, "ImageClientBridge should not have IPDL actor");
  }

  virtual already_AddRefed<Image> CreateImage(ImageFormat aFormat) override
  {
    NS_WARNING("Should not create an image through an ImageClientBridge");
    return nullptr;
  }

protected:
  uint64_t mAsyncContainerID;
  ShadowableLayer* mLayer;
};

#ifdef MOZ_WIDGET_GONK
/**
 * And ImageClient to handle opaque video stream.
 * Such video stream does not upload new Image for each frame.
 * Gecko have no way to get the buffer content from the Image, since the Image
 * does not contain the real buffer.
 * It need special hardware to display the Image
 */
class ImageClientOverlay : public ImageClient
{
public:
  ImageClientOverlay(CompositableForwarder* aFwd,
                     TextureFlags aFlags);

  virtual bool UpdateImage(ImageContainer* aContainer, uint32_t aContentFlags);
  virtual already_AddRefed<Image> CreateImage(ImageFormat aFormat);
  TextureInfo GetTextureInfo() const override
  {
    return TextureInfo(CompositableType::IMAGE_OVERLAY);
  }
};
#endif

}
}

#endif
