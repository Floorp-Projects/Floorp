/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasClient.h"

#include "ClientCanvasLayer.h"          // for ClientCanvasLayer
#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "ScopedGLHelpers.h"
#include "gfx2DGlue.h"                  // for ImageFormatToSurfaceFormat
#include "gfxPlatform.h"                // for gfxPlatform
#include "GLReadTexImageHelper.h"
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/AsyncCanvasRenderer.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/CompositorBridgeChild.h" // for CompositorBridgeChild
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "mozilla/layers/TextureClientOGL.h"
#include "nsDebug.h"                    // for printf_stderr, NS_ASSERTION
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#include "TextureClientSharedSurface.h"

using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

/* static */ already_AddRefed<CanvasClient>
CanvasClient::CreateCanvasClient(CanvasClientType aType,
                                 CompositableForwarder* aForwarder,
                                 TextureFlags aFlags)
{
  switch (aType) {
  case CanvasClientTypeShSurf:
    return MakeAndAddRef<CanvasClientSharedSurface>(aForwarder, aFlags);
  case CanvasClientAsync:
    return MakeAndAddRef<CanvasClientBridge>(aForwarder, aFlags);
  default:
    return MakeAndAddRef<CanvasClient2D>(aForwarder, aFlags);
    break;
  }
}

void
CanvasClientBridge::UpdateAsync(AsyncCanvasRenderer* aRenderer)
{
  if (!GetForwarder() || !mLayer || !aRenderer ||
      !aRenderer->GetCanvasClient()) {
    return;
  }

  uint64_t asyncID = aRenderer->GetCanvasClientAsyncID();
  if (asyncID == 0 || mAsyncID == asyncID) {
    return;
  }

  static_cast<ShadowLayerForwarder*>(GetForwarder())
    ->AttachAsyncCompositable(asyncID, mLayer);
  mAsyncID = asyncID;
}

void
CanvasClient2D::UpdateFromTexture(TextureClient* aTexture)
{
  MOZ_ASSERT(aTexture);

  if (!aTexture->IsSharedWithCompositor()) {
    if (!AddTextureClient(aTexture)) {
      return;
    }
  }

  mBackBuffer = nullptr;
  mFrontBuffer = nullptr;
  mBufferProviderTexture = aTexture;

  AutoTArray<CompositableForwarder::TimedTextureClient,1> textures;
  CompositableForwarder::TimedTextureClient* t = textures.AppendElement();
  t->mTextureClient = aTexture;
  t->mPictureRect = nsIntRect(nsIntPoint(0, 0), aTexture->GetSize());
  t->mFrameID = mFrameID;

  GetForwarder()->UseTextures(this, textures);
  aTexture->SyncWithObject(GetForwarder()->GetSyncObject());
}

void
CanvasClient2D::Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer)
{
  mBufferProviderTexture = nullptr;

  AutoRemoveTexture autoRemove(this);
  if (mBackBuffer && (mBackBuffer->IsReadLocked() || mBackBuffer->GetSize() != aSize)) {
    autoRemove.mTexture = mBackBuffer;
    mBackBuffer = nullptr;
  }

  bool bufferCreated = false;
  if (!mBackBuffer) {
    bool isOpaque = (aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE);
    gfxContentType contentType = isOpaque
                                                ? gfxContentType::COLOR
                                                : gfxContentType::COLOR_ALPHA;
    gfx::SurfaceFormat surfaceFormat
      = gfxPlatform::GetPlatform()->Optimal2DFormatForContent(contentType);
    TextureFlags flags = TextureFlags::DEFAULT;
    if (mTextureFlags & TextureFlags::ORIGIN_BOTTOM_LEFT) {
      flags |= TextureFlags::ORIGIN_BOTTOM_LEFT;
    }

    mBackBuffer = CreateTextureClientForCanvas(surfaceFormat, aSize, flags, aLayer);
    if (!mBackBuffer) {
      NS_WARNING("Failed to allocate the TextureClient");
      return;
    }
    mBackBuffer->EnableReadLock();
    MOZ_ASSERT(mBackBuffer->CanExposeDrawTarget());

    bufferCreated = true;
  }

  bool updated = false;
  {
    TextureClientAutoLock autoLock(mBackBuffer, OpenMode::OPEN_WRITE_ONLY);
    if (!autoLock.Succeeded()) {
      mBackBuffer = nullptr;
      return;
    }

    RefPtr<DrawTarget> target = mBackBuffer->BorrowDrawTarget();
    if (target) {
      if (!aLayer->UpdateTarget(target)) {
        NS_WARNING("Failed to copy the canvas into a TextureClient.");
        return;
      }
      updated = true;
    }
  }

  if (bufferCreated && !AddTextureClient(mBackBuffer)) {
    mBackBuffer = nullptr;
    return;
  }

  if (updated) {
    AutoTArray<CompositableForwarder::TimedTextureClient,1> textures;
    CompositableForwarder::TimedTextureClient* t = textures.AppendElement();
    t->mTextureClient = mBackBuffer;
    t->mPictureRect = nsIntRect(nsIntPoint(0, 0), mBackBuffer->GetSize());
    t->mFrameID = mFrameID;
    GetForwarder()->UseTextures(this, textures);
    mBackBuffer->SyncWithObject(GetForwarder()->GetSyncObject());
  }

  mBackBuffer.swap(mFrontBuffer);
}

already_AddRefed<TextureClient>
CanvasClient2D::CreateTextureClientForCanvas(gfx::SurfaceFormat aFormat,
                                             gfx::IntSize aSize,
                                             TextureFlags aFlags,
                                             ClientCanvasLayer* aLayer)
{
  if (aLayer->IsGLLayer()) {
    // We want a cairo backend here as we don't want to be copying into
    // an accelerated backend and we like LockBits to work. This is currently
    // the most effective way to make this work.
    return TextureClient::CreateForRawBufferAccess(GetForwarder(),
                                                   aFormat, aSize, BackendType::CAIRO,
                                                   mTextureFlags | aFlags);
  }

#ifdef XP_WIN
  return CreateTextureClientForDrawing(aFormat, aSize, BackendSelector::Canvas, aFlags);
#else
  // XXX - We should use CreateTextureClientForDrawing, but we first need
  // to use double buffering.
  gfx::BackendType backend = gfxPlatform::GetPlatform()->GetPreferredCanvasBackend();
  return TextureClient::CreateForRawBufferAccess(GetForwarder(),
                                                 aFormat, aSize, backend,
                                                 mTextureFlags | aFlags);
#endif
}

////////////////////////////////////////////////////////////////////////

CanvasClientSharedSurface::CanvasClientSharedSurface(CompositableForwarder* aLayerForwarder,
                                                     TextureFlags aFlags)
  : CanvasClient(aLayerForwarder, aFlags)
{ }

CanvasClientSharedSurface::~CanvasClientSharedSurface()
{
  ClearSurfaces();
}

////////////////////////////////////////
// Readback

// For formats compatible with R8G8B8A8.
static inline void SwapRB_R8G8B8A8(uint8_t* pixel) {
  // [RR, GG, BB, AA]
  Swap(pixel[0], pixel[2]);
}

class TexClientFactory
{
  CompositableForwarder* const mAllocator;
  const bool mHasAlpha;
  const gfx::IntSize mSize;
  const gfx::BackendType mBackendType;
  const TextureFlags mBaseTexFlags;
  const LayersBackend mLayersBackend;

public:
  TexClientFactory(CompositableForwarder* allocator, bool hasAlpha,
                   const gfx::IntSize& size, gfx::BackendType backendType,
                   TextureFlags baseTexFlags, LayersBackend layersBackend)
    : mAllocator(allocator)
    , mHasAlpha(hasAlpha)
    , mSize(size)
    , mBackendType(backendType)
    , mBaseTexFlags(baseTexFlags)
    , mLayersBackend(layersBackend)
  {
  }

protected:
  already_AddRefed<TextureClient> Create(gfx::SurfaceFormat format) {
    return TextureClient::CreateForRawBufferAccess(mAllocator, format,
                                                   mSize, mBackendType,
                                                   mBaseTexFlags);
  }

public:
  already_AddRefed<TextureClient> CreateB8G8R8AX8() {
    gfx::SurfaceFormat format = mHasAlpha ? gfx::SurfaceFormat::B8G8R8A8
                                          : gfx::SurfaceFormat::B8G8R8X8;
    return Create(format);
  }

  already_AddRefed<TextureClient> CreateR8G8B8AX8() {
    RefPtr<TextureClient> ret;

    bool areRGBAFormatsBroken = mLayersBackend == LayersBackend::LAYERS_BASIC;
    if (!areRGBAFormatsBroken) {
      gfx::SurfaceFormat format = mHasAlpha ? gfx::SurfaceFormat::R8G8B8A8
                                            : gfx::SurfaceFormat::R8G8B8X8;
      ret = Create(format);
    }

    if (!ret) {
      ret = CreateB8G8R8AX8();
      if (ret) {
        ret->AddFlags(TextureFlags::RB_SWAPPED);
      }
    }

    return ret.forget();
  }
};

static already_AddRefed<TextureClient>
TexClientFromReadback(SharedSurface* src, CompositableForwarder* allocator,
                      TextureFlags baseFlags, LayersBackend layersBackend)
{
  auto backendType = gfx::BackendType::CAIRO;
  TexClientFactory factory(allocator, src->mHasAlpha, src->mSize, backendType,
                           baseFlags, layersBackend);

  RefPtr<TextureClient> texClient;

  {
    gl::ScopedReadbackFB autoReadback(src);

    // We have a source FB, now we need a format.
    GLenum destFormat = LOCAL_GL_BGRA;
    GLenum destType = LOCAL_GL_UNSIGNED_BYTE;
    GLenum readFormat;
    GLenum readType;

    // We actually don't care if they match, since we can handle
    // any read{Format,Type} we get.
    auto gl = src->mGL;
    GetActualReadFormats(gl, destFormat, destType, &readFormat, &readType);

    MOZ_ASSERT(readFormat == LOCAL_GL_RGBA ||
               readFormat == LOCAL_GL_BGRA);
    MOZ_ASSERT(readType == LOCAL_GL_UNSIGNED_BYTE);

    // With a format and type, we can create texClient.
    if (readFormat == LOCAL_GL_BGRA &&
        readType == LOCAL_GL_UNSIGNED_BYTE)
    {
      // 0xAARRGGBB
      // In Lendian: [BB, GG, RR, AA]
      texClient = factory.CreateB8G8R8AX8();

    } else if (readFormat == LOCAL_GL_RGBA &&
               readType == LOCAL_GL_UNSIGNED_BYTE)
    {
      // [RR, GG, BB, AA]
      texClient = factory.CreateR8G8B8AX8();
    } else {
      MOZ_CRASH("GFX: Bad `read{Format,Type}`.");
    }

    MOZ_ASSERT(texClient);
    if (!texClient)
      return nullptr;

    // With a texClient, we can lock for writing.
    TextureClientAutoLock autoLock(texClient, OpenMode::OPEN_WRITE);
    DebugOnly<bool> succeeded = autoLock.Succeeded();
    MOZ_ASSERT(succeeded, "texture should have locked");

    MappedTextureData mapped;
    texClient->BorrowMappedData(mapped);

    // ReadPixels from the current FB into mapped.data.
    auto width = src->mSize.width;
    auto height = src->mSize.height;

    {
      ScopedPackState scopedPackState(gl);

      MOZ_ASSERT(mapped.stride/4 == mapped.size.width);
      gl->raw_fReadPixels(0, 0, width, height, readFormat, readType, mapped.data);
    }

    // RB_SWAPPED doesn't work with D3D11. (bug 1051010)
    // RB_SWAPPED doesn't work with Basic. (bug ???????)
    // RB_SWAPPED doesn't work with D3D9. (bug ???????)
    bool layersNeedsManualSwap = layersBackend == LayersBackend::LAYERS_BASIC ||
                                 layersBackend == LayersBackend::LAYERS_D3D9 ||
                                 layersBackend == LayersBackend::LAYERS_D3D11;
    if (texClient->HasFlags(TextureFlags::RB_SWAPPED) &&
        layersNeedsManualSwap)
    {
      size_t pixels = width * height;
      uint8_t* itr = mapped.data;
      for (size_t i = 0; i < pixels; i++) {
        SwapRB_R8G8B8A8(itr);
        itr += 4;
      }

      texClient->RemoveFlags(TextureFlags::RB_SWAPPED);
    }
  }

  return texClient.forget();
}

////////////////////////////////////////

static already_AddRefed<SharedSurfaceTextureClient>
CloneSurface(gl::SharedSurface* src, gl::SurfaceFactory* factory)
{
    RefPtr<SharedSurfaceTextureClient> dest = factory->NewTexClient(src->mSize);
    if (!dest) {
      return nullptr;
    }

    gl::SharedSurface* destSurf = dest->Surf();

    destSurf->ProducerAcquire();
    SharedSurface::ProdCopy(src, dest->Surf(), factory);
    destSurf->ProducerRelease();

    return dest.forget();
}

void
CanvasClientSharedSurface::Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer)
{
  Renderer renderer;
  renderer.construct<ClientCanvasLayer*>(aLayer);
  UpdateRenderer(aSize, renderer);
}

void
CanvasClientSharedSurface::UpdateAsync(AsyncCanvasRenderer* aRenderer)
{
  Renderer renderer;
  renderer.construct<AsyncCanvasRenderer*>(aRenderer);
  UpdateRenderer(aRenderer->GetSize(), renderer);
}

void
CanvasClientSharedSurface::UpdateRenderer(gfx::IntSize aSize, Renderer& aRenderer)
{
  GLContext* gl = nullptr;
  ClientCanvasLayer* layer = nullptr;
  AsyncCanvasRenderer* asyncRenderer = nullptr;
  if (aRenderer.constructed<ClientCanvasLayer*>()) {
    layer = aRenderer.ref<ClientCanvasLayer*>();
    gl = layer->mGLContext;
  } else {
    asyncRenderer = aRenderer.ref<AsyncCanvasRenderer*>();
    gl = asyncRenderer->mGLContext;
  }
  gl->MakeCurrent();

  RefPtr<TextureClient> newFront;

  if (layer && layer->mGLFrontbuffer) {
    mShSurfClient = CloneSurface(layer->mGLFrontbuffer.get(), layer->mFactory.get());
    if (!mShSurfClient) {
      gfxCriticalError() << "Invalid canvas front buffer";
      return;
    }
  } else if (layer && layer->mIsMirror) {
    mShSurfClient = CloneSurface(gl->Screen()->Front()->Surf(), layer->mFactory.get());
    if (!mShSurfClient) {
      return;
    }
  } else {
    mShSurfClient = gl->Screen()->Front();
    if (mShSurfClient && mShSurfClient->GetAllocator() &&
        mShSurfClient->GetAllocator() != GetForwarder()->GetTextureForwarder()) {
      mShSurfClient = CloneSurface(mShSurfClient->Surf(), gl->Screen()->Factory());
    }
    if (!mShSurfClient) {
      return;
    }
  }
  MOZ_ASSERT(mShSurfClient);

  newFront = mShSurfClient;

  SharedSurface* surf = mShSurfClient->Surf();

  // Readback if needed.
  mReadbackClient = nullptr;

  auto forwarder = GetForwarder();

  bool needsReadback = (surf->mType == SharedSurfaceType::Basic);
  if (needsReadback) {
    TextureFlags flags = TextureFlags::IMMUTABLE;

    CompositableForwarder* shadowForwarder = nullptr;
    if (layer) {
      flags |= layer->Flags();
      shadowForwarder = layer->ClientManager()->AsShadowForwarder();
    } else {
      MOZ_ASSERT(asyncRenderer);
      flags |= mTextureFlags;
      shadowForwarder = GetForwarder();
    }

    auto layersBackend = shadowForwarder->GetCompositorBackendType();
    mReadbackClient = TexClientFromReadback(surf, forwarder, flags, layersBackend);

    newFront = mReadbackClient;
  } else {
    mReadbackClient = nullptr;
  }

  if (asyncRenderer) {
    // If surface type is Basic, above codes will readback
    // the GLContext to mReadbackClient in order to send frame to
    // compositor. We copy from this TextureClient directly by
    // calling CopyFromTextureClient().
    // Therefore, if main-thread want the content of GLContext,
    // it doesn't have to readback from GLContext again.
    //
    // Otherwise, if surface type isn't Basic, we will read from
    // SharedSurface directly from main-thread. We still pass
    // mReadbackClient which is nullptr here to tell
    // AsyncCanvasRenderer reset some properties.
    asyncRenderer->CopyFromTextureClient(mReadbackClient);
  }

  MOZ_ASSERT(newFront);
  if (!newFront) {
    // May happen in a release build in case of memory pressure.
    gfxCriticalError() << "Failed to allocate a TextureClient for SharedSurface Canvas. Size: " << aSize;
    return;
  }

  mNewFront = newFront;
}

void
CanvasClientSharedSurface::Updated()
{
  if (!mNewFront) {
    return;
  }

  auto forwarder = GetForwarder();

  mFront = mNewFront;
  mNewFront = nullptr;

  // Add the new TexClient.
  MOZ_ALWAYS_TRUE( AddTextureClient(mFront) );

  AutoTArray<CompositableForwarder::TimedTextureClient,1> textures;
  CompositableForwarder::TimedTextureClient* t = textures.AppendElement();
  t->mTextureClient = mFront;
  t->mPictureRect = nsIntRect(nsIntPoint(0, 0), mFront->GetSize());
  t->mFrameID = mFrameID;
  forwarder->UseTextures(this, textures);
}

void
CanvasClientSharedSurface::OnDetach() {
  ClearSurfaces();
}

void
CanvasClientSharedSurface::ClearSurfaces()
{
  if (mFront) {
    mFront->CancelWaitForRecycle();
  }
  mFront = nullptr;
  mNewFront = nullptr;
  mShSurfClient = nullptr;
  mReadbackClient = nullptr;
}

} // namespace layers
} // namespace mozilla
