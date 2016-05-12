/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_TEXTUREFORWARDER
#define MOZILLA_LAYERS_TEXTUREFORWARDER

#include <stdint.h>                     // for int32_t, uint64_t
#include "gfxTypes.h"
#include "mozilla/Attributes.h"         // for override
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "nsRegion.h"                   // for nsIntRegion
#include "mozilla/gfx/Rect.h"

namespace mozilla {
namespace layers {

class TextureForwarder : public ClientIPCAllocator
{
public:

  TextureForwarder()
  {}

  /**
   * Create a TextureChild/Parent pair as as well as the TextureHost on the parent side.
   */
  virtual PTextureChild* CreateTexture(
    const SurfaceDescriptor& aSharedData,
    LayersBackend aLayersBackend,
    TextureFlags aFlags) = 0;

  virtual TextureForwarder* AsTextureForwarder() override { return this; }

  virtual int32_t GetMaxTextureSize() const override
  {
    return mTextureFactoryIdentifier.mMaxTextureSize;
  }

  /**
   * Returns the type of backend that is used off the main thread.
   * We only don't allow changing the backend type at runtime so this value can
   * be queried once and will not change until Gecko is restarted.
   */
  LayersBackend GetCompositorBackendType() const
  {
    return mTextureFactoryIdentifier.mParentBackend;
  }

  bool SupportsTextureBlitting() const
  {
    return mTextureFactoryIdentifier.mSupportsTextureBlitting;
  }

  bool SupportsPartialUploads() const
  {
    return mTextureFactoryIdentifier.mSupportsPartialUploads;
  }

  const TextureFactoryIdentifier& GetTextureFactoryIdentifier() const
  {
    return mTextureFactoryIdentifier;
  }

  virtual FixedSizeSmallShmemSectionAllocator* GetTileLockAllocator() { return nullptr; };

protected:
  TextureFactoryIdentifier mTextureFactoryIdentifier;
};

} // namespace layers
} // namespace mozilla

#endif
