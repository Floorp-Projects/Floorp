/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasClient.h"

#include "ClientCanvasLayer.h"          // for ClientCanvasLayer
#include "CompositorChild.h"            // for CompositorChild
#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "ScopedGLHelpers.h"
#include "gfx2DGlue.h"                  // for ImageFormatToSurfaceFormat
#include "gfxPlatform.h"                // for gfxPlatform
#include "GLReadTexImageHelper.h"
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "mozilla/layers/TextureClientOGL.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for printf_stderr, NS_ASSERTION
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#ifdef MOZ_WIDGET_GONK
#include "SharedSurfaceGralloc.h"
#endif

using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

/* static */ TemporaryRef<CanvasClient>
CanvasClient::CreateCanvasClient(CanvasClientType aType,
                                 CompositableForwarder* aForwarder,
                                 TextureFlags aFlags)
{
#ifndef MOZ_WIDGET_GONK
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    NS_WARNING("Most platforms still need an optimized way to share GL cross process.");
    return new CanvasClient2D(aForwarder, aFlags);
  }
#endif

  switch (aType) {
  case CanvasClientTypeShSurf:
    return new CanvasClientSharedSurface(aForwarder, aFlags);

  default:
    return new CanvasClient2D(aForwarder, aFlags);
  }
}

void
CanvasClient2D::Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer)
{
  AutoRemoveTexture autoRemove(this);
  if (mBuffer &&
      (mBuffer->IsImmutable() || mBuffer->GetSize() != aSize)) {
    autoRemove.mTexture = mBuffer;
    mBuffer = nullptr;
  }

  bool bufferCreated = false;
  if (!mBuffer) {
    bool isOpaque = (aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE);
    gfxContentType contentType = isOpaque
                                                ? gfxContentType::COLOR
                                                : gfxContentType::COLOR_ALPHA;
    gfxImageFormat format
      = gfxPlatform::GetPlatform()->OptimalFormatForContent(contentType);
    TextureFlags flags = TextureFlags::DEFAULT;
    if (mTextureFlags & TextureFlags::NEEDS_Y_FLIP) {
      flags |= TextureFlags::NEEDS_Y_FLIP;
    }

    gfx::SurfaceFormat surfaceFormat = gfx::ImageFormatToSurfaceFormat(format);
    mBuffer = CreateTextureClientForCanvas(surfaceFormat, aSize, flags, aLayer);
    if (!mBuffer) {
      NS_WARNING("Failed to allocate the TextureClient");
      return;
    }
    MOZ_ASSERT(mBuffer->CanExposeDrawTarget());

    bufferCreated = true;
  }

  if (!mBuffer->Lock(OpenMode::OPEN_WRITE_ONLY)) {
    mBuffer = nullptr;
    return;
  }

  bool updated = false;
  {
    // Restrict drawTarget to a scope so that terminates before Unlock.
    RefPtr<DrawTarget> target =
      mBuffer->BorrowDrawTarget();
    if (target) {
      aLayer->UpdateTarget(target);
      updated = true;
    }
  }
  mBuffer->Unlock();

  if (bufferCreated && !AddTextureClient(mBuffer)) {
    mBuffer = nullptr;
    return;
  }

  if (updated) {
    GetForwarder()->UpdatedTexture(this, mBuffer, nullptr);
    GetForwarder()->UseTexture(this, mBuffer);
  }
}

TemporaryRef<TextureClient>
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
                                                   mTextureInfo.mTextureFlags | aFlags);
  }

  gfx::BackendType backend = gfxPlatform::GetPlatform()->GetPreferredCanvasBackend();
#ifdef XP_WIN
  return CreateTextureClientForDrawing(aFormat, aSize, backend, aFlags);
#else
  // XXX - We should use CreateTextureClientForDrawing, but we first need
  // to use double buffering.
  return TextureClient::CreateForRawBufferAccess(GetForwarder(),
                                                 aFormat, aSize, backend,
                                                 mTextureInfo.mTextureFlags | aFlags);
#endif
}
/*
void
CanvasClientSurfaceStream::Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer)
{
  aLayer->mGLContext->MakeCurrent();

  SurfaceStream* stream = aLayer->mStream;
  MOZ_ASSERT(stream);

  // Copy our current surface to the current producer surface in our stream, then
  // call SwapProducer to make a new buffer ready.
  stream->CopySurfaceToProducer(aLayer->mTextureSurface.get(),
                                aLayer->mFactory.get());
  stream->SwapProducer(aLayer->mFactory.get(),
                       gfx::IntSize(aSize.width, aSize.height));

#ifdef MOZ_WIDGET_GONK
  SharedSurface* surf = stream->SwapConsumer();
  if (!surf) {
    printf_stderr("surf is null post-SwapConsumer!\n");
    return;
  }

  if (surf->mType != SharedSurfaceType::Gralloc) {
    printf_stderr("Unexpected non-Gralloc SharedSurface in IPC path!");
    MOZ_ASSERT(false);
    return;
  }

  SharedSurface_Gralloc* grallocSurf = SharedSurface_Gralloc::Cast(surf);

  RefPtr<GrallocTextureClientOGL> grallocTextureClient =
    static_cast<GrallocTextureClientOGL*>(grallocSurf->GetTextureClient());

  // If IPDLActor is null means this TextureClient didn't AddTextureClient yet
  if (!grallocTextureClient->GetIPDLActor()) {
    grallocTextureClient->SetTextureFlags(mTextureInfo.mTextureFlags);
    AddTextureClient(grallocTextureClient);
  }

  if (grallocTextureClient->GetIPDLActor()) {
    UseTexture(grallocTextureClient);
  }

  if (mBuffer) {
    // remove old buffer from CompositableHost
    RefPtr<AsyncTransactionTracker> tracker = new RemoveTextureFromCompositableTracker();
    // Hold TextureClient until transaction complete.
    tracker->SetTextureClient(mBuffer);
    mBuffer->SetRemoveFromCompositableTracker(tracker);
    // RemoveTextureFromCompositableAsync() expects CompositorChild's presence.
    GetForwarder()->RemoveTextureFromCompositableAsync(tracker, this, mBuffer);
  }
  mBuffer = grallocTextureClient;
#else
  bool isCrossProcess = !(XRE_GetProcessType() == GeckoProcessType_Default);
  if (isCrossProcess) {
    printf_stderr("isCrossProcess, but not MOZ_WIDGET_GONK! Someone needs to write some code!");
    MOZ_ASSERT(false);
  } else {
    bool bufferCreated = false;
    if (!mBuffer) {
      // We need to dealloc in the client.
      TextureFlags flags = GetTextureFlags() |
                           TextureFlags::DEALLOCATE_CLIENT;
      StreamTextureClient* texClient = new StreamTextureClient(flags);
      texClient->InitWith(stream);
      mBuffer = texClient;
      bufferCreated = true;
    }

    if (bufferCreated && !AddTextureClient(mBuffer)) {
      mBuffer = nullptr;
    }

    if (mBuffer) {
      GetForwarder()->UpdatedTexture(this, mBuffer, nullptr);
      GetForwarder()->UseTexture(this, mBuffer);
    }
  }
#endif

  aLayer->Painted();
}
*/

////////////////////////////////////////////////////////////////////////

CanvasClientSharedSurface::CanvasClientSharedSurface(CompositableForwarder* aLayerForwarder,
                                                     TextureFlags aFlags)
  : CanvasClient(aLayerForwarder, aFlags)
{
}

////////////////////////////////////////
// Accelerated backends

static TemporaryRef<TextureClient>
TexClientFromShSurf(SharedSurface* surf, TextureFlags flags)
{
  switch (surf->mType) {
    case SharedSurfaceType::Basic:
      return nullptr;

#ifdef MOZ_WIDGET_GONK
    case SharedSurfaceType::Gralloc:
      return GrallocTextureClientOGL::FromShSurf(surf, flags);
#endif

    default:
      return new SharedSurfaceTextureClient(flags, surf);
  }
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
  ISurfaceAllocator* const mAllocator;
  const bool mHasAlpha;
  const gfx::IntSize mSize;
  const gfx::BackendType mBackendType;
  const TextureFlags mBaseTexFlags;
  const LayersBackend mLayersBackend;

public:
  TexClientFactory(ISurfaceAllocator* allocator, bool hasAlpha,
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
  TemporaryRef<BufferTextureClient> Create(gfx::SurfaceFormat format) {
    return TextureClient::CreateForRawBufferAccess(mAllocator, format,
                                                   mSize, mBackendType,
                                                   mBaseTexFlags);
  }

public:
  TemporaryRef<BufferTextureClient> CreateB8G8R8AX8() {
    gfx::SurfaceFormat format = mHasAlpha ? gfx::SurfaceFormat::B8G8R8A8
                                          : gfx::SurfaceFormat::B8G8R8X8;
    return Create(format);
  }

  TemporaryRef<BufferTextureClient> CreateR8G8B8AX8() {
    RefPtr<BufferTextureClient> ret;

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

static TemporaryRef<TextureClient>
TexClientFromReadback(SharedSurface* src, ISurfaceAllocator* allocator,
                      TextureFlags baseFlags, LayersBackend layersBackend)
{
  auto backendType = gfx::BackendType::CAIRO;
  TexClientFactory factory(allocator, src->mHasAlpha, src->mSize, backendType,
                           baseFlags, layersBackend);

  RefPtr<BufferTextureClient> texClient;

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
      MOZ_CRASH("Bad `read{Format,Type}`.");
    }

    MOZ_ASSERT(texClient);
    if (!texClient)
        return nullptr;

    // With a texClient, we can lock for writing.
    MOZ_ALWAYS_TRUE( texClient->Lock(OpenMode::OPEN_WRITE) );

    uint8_t* lockedBytes = texClient->GetLockedData();

    // ReadPixels from the current FB into lockedBits.
    auto width = src->mSize.width;
    auto height = src->mSize.height;

    {
      ScopedPackAlignment autoAlign(gl, 4);

      gl->raw_fReadPixels(0, 0, width, height, readFormat, readType, lockedBytes);
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
      uint8_t* itr = lockedBytes;
      for (size_t i = 0; i < pixels; i++) {
        SwapRB_R8G8B8A8(itr);
        itr += 4;
      }

      texClient->RemoveFlags(TextureFlags::RB_SWAPPED);
    }

    texClient->Unlock();
  }

  return texClient.forget();
}

////////////////////////////////////////

static TemporaryRef<gl::ShSurfHandle>
CloneSurface(gl::SharedSurface* src, gl::SurfaceFactory* factory)
{
    RefPtr<gl::ShSurfHandle> dest = factory->NewShSurfHandle(src->mSize);
    SharedSurface::ProdCopy(src, dest->Surf(), factory);
    return dest.forget();
}

void
CanvasClientSharedSurface::Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer)
{
  if (mFront) {
    mPrevFront = mFront;
    mFront = nullptr;
  }

  auto gl = aLayer->mGLContext;
  gl->MakeCurrent();

  if (aLayer->mGLFrontbuffer) {
    mFront = CloneSurface(aLayer->mGLFrontbuffer.get(), aLayer->mFactory.get());
    if (mFront)
      mFront->Surf()->Fence();
  } else {
    mFront = gl->Screen()->Front();
    if (!mFront)
      return;
  }
  MOZ_ASSERT(mFront);

  // Alright, now sort out the IPC goop.
  SharedSurface* surf = mFront->Surf();
  auto forwarder = GetForwarder();
  auto flags = GetTextureFlags() | TextureFlags::IMMUTABLE;

  // Get a TexClient from our surf.
  RefPtr<TextureClient> newTex = TexClientFromShSurf(surf, flags);
  if (!newTex) {
    auto manager = aLayer->ClientManager();
    auto shadowForwarder = manager->AsShadowForwarder();
    auto layersBackend = shadowForwarder->GetCompositorBackendType();

    newTex = TexClientFromReadback(surf, forwarder, flags, layersBackend);
  }
  MOZ_ASSERT(newTex);

  // Add the new TexClient.
  MOZ_ALWAYS_TRUE( newTex->InitIPDLActor(forwarder) );
  MOZ_ASSERT(newTex->GetIPDLActor());

  // Remove the old TexClient.
  if (mFrontTex) {
    // remove old buffer from CompositableHost
    RefPtr<AsyncTransactionTracker> tracker = new RemoveTextureFromCompositableTracker();
    // Hold TextureClient until transaction complete.
    tracker->SetTextureClient(mFrontTex);
    mFrontTex->SetRemoveFromCompositableTracker(tracker);
    // RemoveTextureFromCompositableAsync() expects CompositorChild's presence.
    GetForwarder()->RemoveTextureFromCompositableAsync(tracker, this, mFrontTex);

    mFrontTex = nullptr;
  }

  // Use the new TexClient.
  mFrontTex = newTex;

  forwarder->UpdatedTexture(this, mFrontTex, nullptr);
  forwarder->UseTexture(this, mFrontTex);

  aLayer->Painted();
}

}
}
