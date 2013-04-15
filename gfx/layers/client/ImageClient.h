/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGECLIENT_H
#define MOZILLA_GFX_IMAGECLIENT_H

#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/TextureClient.h"
#include "gfxPattern.h"

namespace mozilla {
namespace layers {

class ImageContainer;
class ImageLayer;
class PlanarYCbCrImage;

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
  static TemporaryRef<ImageClient> CreateImageClient(LayersBackend aBackendType,
                                                     CompositableType aImageHostType,
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
   * Notify the compositor that this image client has been updated
   */
  virtual void Updated() = 0;

  /**
   * The picture rect is the area of the texture which makes up the image. That
   * is, the area that should be composited. In texture space.
   */
  virtual void UpdatePictureRect(nsIntRect aPictureRect);

  virtual already_AddRefed<Image> CreateImage(const uint32_t *aFormats,
                                              uint32_t aNumFormats);

protected:
  ImageClient(CompositableForwarder* aFwd, CompositableType aType);

  gfxPattern::GraphicsFilter mFilter;
  CompositableType mType;
  int32_t mLastPaintedImageSerial;
  nsIntRect mPictureRect;
};

/**
 * An image client which uses a single texture client, may be single or double
 * buffered. (As opposed to using two texture clients for buffering, as in
 * ContentClientDoubleBuffered, or using multiple clients for YCbCr or tiled
 * images).
 */
class ImageClientSingle : public ImageClient
{
public:
  ImageClientSingle(CompositableForwarder* aFwd,
                     TextureFlags aFlags,
                     CompositableType aType);

  virtual bool UpdateImage(ImageContainer* aContainer, uint32_t aContentFlags);

  void EnsureTextureClient(TextureClientType aType);

  virtual void Updated();

  virtual void SetDescriptorFromReply(TextureIdentifier aTextureId,
                                      const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE
  {
    mTextureClient->SetDescriptorFromReply(aDescriptor);
  }

  virtual TextureInfo GetTextureInfo() const MOZ_OVERRIDE
  {
    return mTextureInfo;
  }

private:
  RefPtr<TextureClient> mTextureClient;
  TextureInfo mTextureInfo;
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

  virtual bool UpdateImage(ImageContainer* aContainer, uint32_t aContentFlags);
  virtual bool Connect() { return false; }
  virtual void Updated() {}
  void SetLayer(ShadowableLayer* aLayer)
  {
    mLayer = aLayer;
  }

  virtual TextureInfo GetTextureInfo() const MOZ_OVERRIDE
  {
    return TextureInfo(mType);
  }

protected:
  uint64_t mAsyncContainerID;
  ShadowableLayer* mLayer;
};

}
}

#endif
