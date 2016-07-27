/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CANVASCLIENT_H
#define MOZILLA_GFX_CANVASCLIENT_H

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr, already_AddRefed
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "mozilla/layers/PersistentBufferProvider.h"

// Fix X11 header brain damage that conflicts with MaybeOneOf::None
#undef None
#include "mozilla/MaybeOneOf.h"

#include "mozilla/mozalloc.h"           // for operator delete

#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Types.h"          // for SurfaceFormat

namespace mozilla {
namespace layers {

class AsyncCanvasRenderer;
class ClientCanvasLayer;
class CompositableForwarder;
class ShadowableLayer;
class SharedSurfaceTextureClient;

/**
 * Compositable client for 2d and webgl canvas.
 */
class CanvasClient : public CompositableClient
{
public:
  typedef MaybeOneOf<ClientCanvasLayer*, AsyncCanvasRenderer*> Renderer;

  /**
   * Creates, configures, and returns a new canvas client. If necessary, a
   * message will be sent to the compositor to create a corresponding image
   * host.
   */
  enum CanvasClientType {
    CanvasClientSurface,
    CanvasClientGLContext,
    CanvasClientTypeShSurf,
    CanvasClientAsync, // webgl on workers
  };
  static already_AddRefed<CanvasClient> CreateCanvasClient(CanvasClientType aType,
                                                       CompositableForwarder* aFwd,
                                                       TextureFlags aFlags);

  CanvasClient(CompositableForwarder* aFwd, TextureFlags aFlags)
    : CompositableClient(aFwd, aFlags)
    , mFrameID(0)
  {
    mTextureFlags = aFlags;
  }

  virtual ~CanvasClient() {}

  virtual void Clear() {};

  virtual void Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer) = 0;

  virtual bool AddTextureClient(TextureClient* aTexture) override
  {
    ++mFrameID;
    return CompositableClient::AddTextureClient(aTexture);
  }

  virtual void UpdateAsync(AsyncCanvasRenderer* aRenderer) {}

  virtual void UpdateFromTexture(TextureClient* aTexture) {}

  virtual void Updated() { }

protected:
  int32_t mFrameID;
};

// Used for 2D canvases and WebGL canvas on non-GL systems where readback is requried.
class CanvasClient2D : public CanvasClient
{
public:
  CanvasClient2D(CompositableForwarder* aLayerForwarder,
                 TextureFlags aFlags)
    : CanvasClient(aLayerForwarder, aFlags)
  {
  }

  TextureInfo GetTextureInfo() const override
  {
    return TextureInfo(CompositableType::IMAGE, mTextureFlags);
  }

  virtual void Clear() override
  {
    mBackBuffer = mFrontBuffer = nullptr;
  }

  virtual void Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer) override;

  virtual void UpdateFromTexture(TextureClient* aBuffer) override;

  virtual bool AddTextureClient(TextureClient* aTexture) override
  {
    return CanvasClient::AddTextureClient(aTexture);
  }

  virtual void OnDetach() override
  {
    mBackBuffer = mFrontBuffer = nullptr;
  }

private:
  already_AddRefed<TextureClient>
    CreateTextureClientForCanvas(gfx::SurfaceFormat aFormat,
                                 gfx::IntSize aSize,
                                 TextureFlags aFlags,
                                 ClientCanvasLayer* aLayer);

  RefPtr<TextureClient> mBackBuffer;
  RefPtr<TextureClient> mFrontBuffer;
  // We store this texture separately to make sure it is not written into
  // in Update() if for some silly reason we end up alternating between
  // UpdateFromTexture and Update.
  // This code is begging for a cleanup. The situation described above should
  // not be made possible.
  RefPtr<TextureClient> mBufferProviderTexture;
};

// Used for GL canvases where we don't need to do any readback, i.e., with a
// GL backend.
class CanvasClientSharedSurface : public CanvasClient
{
private:
  RefPtr<SharedSurfaceTextureClient> mShSurfClient;
  RefPtr<TextureClient> mReadbackClient;
  RefPtr<TextureClient> mFront;
  RefPtr<TextureClient> mNewFront;

  void ClearSurfaces();

public:
  CanvasClientSharedSurface(CompositableForwarder* aLayerForwarder,
                            TextureFlags aFlags);

  ~CanvasClientSharedSurface();

  virtual TextureInfo GetTextureInfo() const override {
    return TextureInfo(CompositableType::IMAGE);
  }

  virtual void Clear() override {
    ClearSurfaces();
  }

  virtual void Update(gfx::IntSize aSize,
                      ClientCanvasLayer* aLayer) override;
  void UpdateRenderer(gfx::IntSize aSize, Renderer& aRenderer);

  virtual void UpdateAsync(AsyncCanvasRenderer* aRenderer) override;

  virtual void Updated() override;

  virtual void OnDetach() override;
};

/**
 * Used for OMT<canvas> uploads using the image bridge protocol.
 * Actual CanvasClient is on the ImageBridgeChild thread, so we
 * only forward its AsyncID here
 */
class CanvasClientBridge final : public CanvasClient
{
public:
  CanvasClientBridge(CompositableForwarder* aLayerForwarder,
                     TextureFlags aFlags)
    : CanvasClient(aLayerForwarder, aFlags)
    , mAsyncID(0)
    , mLayer(nullptr)
  {
  }

  TextureInfo GetTextureInfo() const override
  {
    return TextureInfo(CompositableType::IMAGE);
  }

  virtual void Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer) override
  {
  }

  virtual void UpdateAsync(AsyncCanvasRenderer* aRenderer) override;

  void SetLayer(ShadowableLayer* aLayer)
  {
    mLayer = aLayer;
  }

protected:
  uint64_t mAsyncID;
  ShadowableLayer* mLayer;
};

} // namespace layers
} // namespace mozilla

#endif
