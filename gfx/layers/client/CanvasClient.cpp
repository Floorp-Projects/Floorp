/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CanvasClient.h"
#include "ClientCanvasLayer.h"          // for ClientCanvasLayer
#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "Layers.h"                     // for Layer, etc
#include "SurfaceStream.h"              // for SurfaceStream
#include "SurfaceTypes.h"               // for SurfaceStreamHandle
#include "gfx2DGlue.h"                  // for ImageFormatToSurfaceFormat
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for printf_stderr, NS_ASSERTION
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#ifdef MOZ_WIDGET_GONK
#include "SharedSurfaceGralloc.h"
#endif

using namespace mozilla::gl;

namespace mozilla {
namespace gfx {
class SharedSurface;
}
}

namespace mozilla {
namespace layers {

/* static */ TemporaryRef<CanvasClient>
CanvasClient::CreateCanvasClient(CanvasClientType aType,
                                 CompositableForwarder* aForwarder,
                                 TextureFlags aFlags)
{
  if (aType == CanvasClientGLContext &&
      aForwarder->GetCompositorBackendType() == LAYERS_OPENGL) {
    return new DeprecatedCanvasClientSurfaceStream(aForwarder, aFlags);
  }
  if (gfxPlatform::GetPlatform()->UseDeprecatedTextures()) {
    return new DeprecatedCanvasClient2D(aForwarder, aFlags);
  }
  return new CanvasClient2D(aForwarder, aFlags);
}

void
CanvasClient2D::Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer)
{
  if (mBuffer &&
      (mBuffer->IsImmutable() || mBuffer->GetSize() != aSize)) {
    RemoveTextureClient(mBuffer);
    mBuffer = nullptr;
  }

  bool bufferCreated = false;
  if (!mBuffer) {
    bool isOpaque = (aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE);
    gfxASurface::gfxContentType contentType = isOpaque
                                                ? gfxASurface::CONTENT_COLOR
                                                : gfxASurface::CONTENT_COLOR_ALPHA;
    gfxASurface::gfxImageFormat format
      = gfxPlatform::GetPlatform()->OptimalFormatForContent(contentType);
    mBuffer = CreateBufferTextureClient(gfx::ImageFormatToSurfaceFormat(format));
    MOZ_ASSERT(mBuffer->AsTextureClientSurface());
    mBuffer->AsTextureClientSurface()->AllocateForSurface(aSize);

    bufferCreated = true;
  }

  if (!mBuffer->Lock(OPEN_READ_WRITE)) {
    return;
  }

  nsRefPtr<gfxASurface> surface = mBuffer->AsTextureClientSurface()->GetAsSurface();
  if (surface) {
    aLayer->UpdateSurface(surface);
  }

  mBuffer->Unlock();

  if (bufferCreated) {
    AddTextureClient(mBuffer);
  }

  if (surface) {
    GetForwarder()->UpdatedTexture(this, mBuffer, nullptr);
    GetForwarder()->UseTexture(this, mBuffer);
  }
}

TemporaryRef<BufferTextureClient>
CanvasClient2D::CreateBufferTextureClient(gfx::SurfaceFormat aFormat)
{
  return CompositableClient::CreateBufferTextureClient(aFormat,
                                                       mTextureInfo.mTextureFlags);
}

void
DeprecatedCanvasClient2D::Updated()
{
  mForwarder->UpdateTexture(this, 1, mDeprecatedTextureClient->GetDescriptor());
}


DeprecatedCanvasClient2D::DeprecatedCanvasClient2D(CompositableForwarder* aFwd,
                                                   TextureFlags aFlags)
: CanvasClient(aFwd, aFlags)
{
  mTextureInfo.mCompositableType = BUFFER_IMAGE_SINGLE;
}

void
DeprecatedCanvasClient2D::Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer)
{
  if (!mDeprecatedTextureClient) {
    mDeprecatedTextureClient = CreateDeprecatedTextureClient(TEXTURE_CONTENT);
    MOZ_ASSERT(mDeprecatedTextureClient, "Failed to create texture client");
  }

  bool isOpaque = (aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE);
  gfxASurface::gfxContentType contentType = isOpaque
                                              ? gfxASurface::CONTENT_COLOR
                                              : gfxASurface::CONTENT_COLOR_ALPHA;

  mDeprecatedTextureClient->EnsureAllocated(aSize, contentType);
  if (!mDeprecatedTextureClient->EnsureAllocated(aSize, contentType)) {
    mDeprecatedTextureClient = CreateDeprecatedTextureClient(TEXTURE_FALLBACK);
    MOZ_ASSERT(mDeprecatedTextureClient, "Failed to create texture client");
    if (!mDeprecatedTextureClient->EnsureAllocated(aSize, contentType)) {
      NS_WARNING("Could not allocate texture client");
      return;
    }
  }

  gfxASurface* surface = mDeprecatedTextureClient->LockSurface();
  aLayer->UpdateSurface(surface);
  mDeprecatedTextureClient->Unlock();
}

void
DeprecatedCanvasClientSurfaceStream::Updated()
{
  mForwarder->UpdateTextureNoSwap(this, 1, mDeprecatedTextureClient->GetDescriptor());
}


DeprecatedCanvasClientSurfaceStream::DeprecatedCanvasClientSurfaceStream(CompositableForwarder* aFwd,
                                                                         TextureFlags aFlags)
: CanvasClient(aFwd, aFlags)
{
  mTextureInfo.mCompositableType = BUFFER_IMAGE_SINGLE;
}

void
DeprecatedCanvasClientSurfaceStream::Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer)
{
  if (!mDeprecatedTextureClient) {
    mDeprecatedTextureClient = CreateDeprecatedTextureClient(TEXTURE_STREAM_GL);
    MOZ_ASSERT(mDeprecatedTextureClient, "Failed to create texture client");
  }

  NS_ASSERTION(aLayer->mGLContext, "CanvasClientSurfaceStream should only be used with GL canvases");

  // the content type won't be used
  mDeprecatedTextureClient->EnsureAllocated(aSize, gfxASurface::CONTENT_COLOR);

  GLScreenBuffer* screen = aLayer->mGLContext->Screen();
  SurfaceStream* stream = screen->Stream();

  bool isCrossProcess = !(XRE_GetProcessType() == GeckoProcessType_Default);
  if (isCrossProcess) {
    // swap staging -> consumer so we can send it to the compositor
    SharedSurface* surf = stream->SwapConsumer();
    if (!surf) {
      printf_stderr("surf is null post-SwapConsumer!\n");
      return;
    }

#ifdef MOZ_WIDGET_GONK
    if (surf->Type() != SharedSurfaceType::Gralloc) {
      printf_stderr("Unexpected non-Gralloc SharedSurface in IPC path!");
      return;
    }

    SharedSurface_Gralloc* grallocSurf = SharedSurface_Gralloc::Cast(surf);
    mDeprecatedTextureClient->SetDescriptor(grallocSurf->GetDescriptor());
#else
    printf_stderr("isCrossProcess, but not MOZ_WIDGET_GONK! Someone needs to write some code!");
    MOZ_ASSERT(false);
#endif
  } else {
    SurfaceStreamHandle handle = stream->GetShareHandle();
    SurfaceDescriptor *desc = mDeprecatedTextureClient->GetDescriptor();
    if (desc->type() != SurfaceDescriptor::TSurfaceStreamDescriptor ||
        desc->get_SurfaceStreamDescriptor().handle() != handle) {
      *desc = SurfaceStreamDescriptor(handle, false);

      // Bug 894405
      //
      // Ref this so the SurfaceStream doesn't disappear unexpectedly. The
      // Compositor will need to unref it when finished.
      aLayer->mGLContext->AddRef();
    }
  }

  aLayer->Painted();
}

}
}
