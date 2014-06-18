/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CanvasClient.h"
#include "ClientCanvasLayer.h"          // for ClientCanvasLayer
#include "CompositorChild.h"            // for CompositorChild
#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "SurfaceStream.h"              // for SurfaceStream
#include "SurfaceTypes.h"               // for SurfaceStreamHandle
#include "gfx2DGlue.h"                  // for ImageFormatToSurfaceFormat
#include "gfxPlatform.h"                // for gfxPlatform
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
  if (aType == CanvasClientGLContext &&
      aForwarder->GetCompositorBackendType() == LayersBackend::LAYERS_OPENGL) {
    aFlags |= TextureFlags::DEALLOCATE_CLIENT;
    return new CanvasClientSurfaceStream(aForwarder, aFlags);
  }
  return new CanvasClient2D(aForwarder, aFlags);
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
    MOZ_ASSERT(mBuffer->CanExposeDrawTarget());
    mBuffer->AllocateForSurface(aSize);

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
      mBuffer->GetAsDrawTarget();
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
    return CreateBufferTextureClient(aFormat, aFlags, BackendType::CAIRO);
  }

  gfx::BackendType backend = gfxPlatform::GetPlatform()->GetPreferredCanvasBackend();
#ifdef XP_WIN
  return CreateTextureClientForDrawing(aFormat, aFlags, backend, aSize);
#else
  // XXX - We should use CreateTextureClientForDrawing, but we first need
  // to use double buffering.
  return CreateBufferTextureClient(aFormat, aFlags, backend);
#endif
}

CanvasClientSurfaceStream::CanvasClientSurfaceStream(CompositableForwarder* aLayerForwarder,
                                                     TextureFlags aFlags)
  : CanvasClient(aLayerForwarder, aFlags)
{
}

void
CanvasClientSurfaceStream::Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer)
{
  aLayer->mGLContext->MakeCurrent();
  GLScreenBuffer* screen = aLayer->mGLContext->Screen();
  SurfaceStream* stream = nullptr;

  if (aLayer->mStream) {
    stream = aLayer->mStream;

    // Copy our current surface to the current producer surface in our stream, then
    // call SwapProducer to make a new buffer ready.
    stream->CopySurfaceToProducer(aLayer->mTextureSurface, aLayer->mFactory);
    stream->SwapProducer(aLayer->mFactory, gfx::IntSize(aSize.width, aSize.height));
  } else {
    stream = screen->Stream();
  }

  bool isCrossProcess = !(XRE_GetProcessType() == GeckoProcessType_Default);
  bool bufferCreated = false;
  if (isCrossProcess) {
#ifdef MOZ_WIDGET_GONK
    SharedSurface* surf = stream->SwapConsumer();
    if (!surf) {
      printf_stderr("surf is null post-SwapConsumer!\n");
      return;
    }

    if (surf->Type() != SharedSurfaceType::Gralloc) {
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
    printf_stderr("isCrossProcess, but not MOZ_WIDGET_GONK! Someone needs to write some code!");
    MOZ_ASSERT(false);
#endif
  } else {
    if (!mBuffer) {
      StreamTextureClientOGL* textureClient =
        new StreamTextureClientOGL(mTextureInfo.mTextureFlags);
      textureClient->InitWith(stream);
      mBuffer = textureClient;
      bufferCreated = true;
    }

    if (bufferCreated && !AddTextureClient(mBuffer)) {
      mBuffer = nullptr;
    }

    if (mBuffer) {
      GetForwarder()->UseTexture(this, mBuffer);
    }
  }

  aLayer->Painted();
}

}
}
