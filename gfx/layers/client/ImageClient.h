/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGECLIENT_H
#define MOZILLA_GFX_IMAGECLIENT_H

#include <stdint.h>                     // for uint32_t, uint64_t
#include <sys/types.h>                  // for int32_t
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr, already_AddRefed
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/CompositorTypes.h"  // for CompositableType, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsRect.h"                     // for mozilla::gfx::IntRect

namespace mozilla {
namespace layers {

class ClientLayer;
class CompositableForwarder;
class Image;
class ImageContainer;
class ShadowableLayer;
class ImageClientSingle;

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
  static already_AddRefed<ImageClient> CreateImageClient(CompositableType aImageHostType,
                                                     CompositableForwarder* aFwd,
                                                     TextureFlags aFlags);

  virtual ~ImageClient() {}

  /**
   * Update this ImageClient from aContainer in aLayer
   * returns false if this is the wrong kind of ImageClient for aContainer.
   * Note that returning true does not necessarily imply success
   */
  virtual bool UpdateImage(ImageContainer* aContainer, uint32_t aContentFlags) = 0;

  void SetLayer(ClientLayer* aLayer) { mLayer = aLayer; }
  ClientLayer* GetLayer() const { return mLayer; }

  /**
   * asynchronously remove all the textures used by the image client.
   *
   */
  virtual void FlushAllImages() {}

  virtual void RemoveTexture(TextureClient* aTexture) override;

  virtual ImageClientSingle* AsImageClientSingle() { return nullptr; }

  static already_AddRefed<TextureClient> CreateTextureClientForImage(Image* aImage, KnowsCompositor* aForwarder);

  uint32_t GetLastUpdateGenerationCounter() { return mLastUpdateGenerationCounter; }

  virtual RefPtr<TextureClient> GetForwardedTexture() { return nullptr; }

protected:
  ImageClient(CompositableForwarder* aFwd, TextureFlags aFlags,
              CompositableType aType);

  ClientLayer* mLayer;
  CompositableType mType;
  uint32_t mLastUpdateGenerationCounter;
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

  virtual void FlushAllImages() override;

  ImageClientSingle* AsImageClientSingle() override { return this; }

  virtual RefPtr<TextureClient> GetForwardedTexture() override;

  bool IsEmpty() { return mBuffers.IsEmpty(); }

protected:
  struct Buffer {
    RefPtr<TextureClient> mTextureClient;
    int32_t mImageSerial;
  };
  nsTArray<Buffer> mBuffers;
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
  virtual bool Connect(ImageContainer* aImageContainer) override { return false; }

  virtual TextureInfo GetTextureInfo() const override
  {
    return TextureInfo(mType);
  }

protected:
  CompositableHandle mAsyncContainerHandle;
};

} // namespace layers
} // namespace mozilla

#endif
