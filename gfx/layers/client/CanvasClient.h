/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CANVASCLIENT_H
#define MOZILLA_GFX_CANVASCLIENT_H

#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/CompositableClient.h"

namespace mozilla {

namespace layers {

class BasicCanvasLayer;

/**
 * Compositable client for 2d and webgl canvas.
 */
class CanvasClient : public CompositableClient
{
public:
  /**
   * Creates, configures, and returns a new canvas client. If necessary, a
   * message will be sent to the compositor to create a corresponding image
   * host.
   */
  static TemporaryRef<CanvasClient> CreateCanvasClient(CompositableType aImageHostType,
                                                       CompositableForwarder* aFwd,
                                                       TextureFlags aFlags);

  CanvasClient(CompositableForwarder* aFwd, TextureFlags aFlags)
    : CompositableClient(aFwd)
  {
    mTextureInfo.mTextureFlags = aFlags;
  }

  virtual ~CanvasClient() {}

  virtual void Update(gfx::IntSize aSize, BasicCanvasLayer* aLayer) = 0;

  virtual void Updated();

  virtual void SetDescriptorFromReply(TextureIdentifier aTextureId,
                                      const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE
  {
    mTextureClient->SetDescriptorFromReply(aDescriptor);
  }
protected:
  RefPtr<TextureClient> mTextureClient;
  TextureInfo mTextureInfo;
};

// Used for 2D canvases and WebGL canvas on non-GL systems where readback is requried.
class CanvasClient2D : public CanvasClient
{
public:
  CanvasClient2D(CompositableForwarder* aLayerForwarder,
                 TextureFlags aFlags);

  TextureInfo GetTextureInfo() const MOZ_OVERRIDE
  {
    return mTextureInfo;
  }

  virtual void Update(gfx::IntSize aSize, BasicCanvasLayer* aLayer);
};

// Used for GL canvases where we don't need to do any readback, i.e., with a
// GL backend.
class CanvasClientWebGL : public CanvasClient
{
public:
  CanvasClientWebGL(CompositableForwarder* aFwd,
                    TextureFlags aFlags);

  TextureInfo GetTextureInfo() const MOZ_OVERRIDE
  {
    return mTextureInfo;
  }

  virtual void Update(gfx::IntSize aSize, BasicCanvasLayer* aLayer);
};

}
}

#endif
