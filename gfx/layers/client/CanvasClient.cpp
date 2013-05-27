/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CanvasClient.h"
#include "mozilla/layers/TextureClient.h"
#include "ClientCanvasLayer.h"
#include "mozilla/layers/ShadowLayers.h"
#include "SharedTextureImage.h"
#include "nsXULAppAPI.h"
#include "GLContext.h"
#include "SurfaceStream.h"
#include "SharedSurface.h"
#ifdef MOZ_B2G
#include "SharedSurfaceGralloc.h"
#endif

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

/* static */ TemporaryRef<CanvasClient>
CanvasClient::CreateCanvasClient(CompositableType aCompositableHostType,
                                 CompositableForwarder* aForwarder,
                                 TextureFlags aFlags)
{
  if (aCompositableHostType == BUFFER_IMAGE_SINGLE) {
    return new CanvasClient2D(aForwarder, aFlags);
  }
  if (aCompositableHostType == BUFFER_IMAGE_BUFFERED) {
    if (aForwarder->GetCompositorBackendType() == LAYERS_OPENGL) {
      return new CanvasClientWebGL(aForwarder, aFlags);
    }
    return new CanvasClient2D(aForwarder, aFlags);
  }
  return nullptr;
}

void
CanvasClient::Updated()
{
  mForwarder->UpdateTexture(this, 1, mTextureClient->GetDescriptor());
}


CanvasClient2D::CanvasClient2D(CompositableForwarder* aFwd,
                               TextureFlags aFlags)
: CanvasClient(aFwd, aFlags)
{
  mTextureInfo.mCompositableType = BUFFER_IMAGE_SINGLE;
}

void
CanvasClient2D::Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer)
{
  if (!mTextureClient) {
    mTextureClient = CreateTextureClient(TEXTURE_SHMEM);
    MOZ_ASSERT(mTextureClient, "Failed to create texture client");
  }

  bool isOpaque = (aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE);
  gfxASurface::gfxContentType contentType = isOpaque
                                              ? gfxASurface::CONTENT_COLOR
                                              : gfxASurface::CONTENT_COLOR_ALPHA;
  mTextureClient->EnsureAllocated(aSize, contentType);

  gfxASurface* surface = mTextureClient->LockSurface();
  aLayer->UpdateSurface(surface);
  mTextureClient->Unlock();
}

void
CanvasClientWebGL::Updated()
{
  mForwarder->UpdateTextureNoSwap(this, 1, mTextureClient->GetDescriptor());
}


CanvasClientWebGL::CanvasClientWebGL(CompositableForwarder* aFwd,
                                     TextureFlags aFlags)
: CanvasClient(aFwd, aFlags)
{
  mTextureInfo.mCompositableType = BUFFER_IMAGE_BUFFERED;
}

void
CanvasClientWebGL::Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer)
{
  if (!mTextureClient) {
    mTextureClient = CreateTextureClient(TEXTURE_STREAM_GL);
    MOZ_ASSERT(mTextureClient, "Failed to create texture client");
  }

  NS_ASSERTION(aLayer->mGLContext, "CanvasClientWebGL should only be used with GL canvases");

  // the content type won't be used
  mTextureClient->EnsureAllocated(aSize, gfxASurface::CONTENT_COLOR);

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

#ifdef MOZ_B2G
    if (surf->Type() != SharedSurfaceType::Gralloc) {
      printf_stderr("Unexpected non-Gralloc SharedSurface in IPC path!");
      return;
    }

    SharedSurface_Gralloc* grallocSurf = SharedSurface_Gralloc::Cast(surf);
    mTextureClient->SetDescriptor(grallocSurf->GetDescriptor());
#else
    printf_stderr("isCrossProcess, but not MOZ_B2G! Someone needs to write some code!");
    MOZ_ASSERT(false);
#endif
  } else {
    SurfaceStreamHandle handle = stream->GetShareHandle();
    mTextureClient->SetDescriptor(SurfaceStreamDescriptor(handle, false));
  }

  aLayer->Painted();
}

}
}
