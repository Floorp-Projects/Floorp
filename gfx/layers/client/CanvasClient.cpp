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
#ifdef MOZ_WIDGET_GONK
#include "SharedSurfaceGralloc.h"
#endif

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

/* static */ TemporaryRef<CanvasClient>
CanvasClient::CreateCanvasClient(CanvasClientType aType,
                                 CompositableForwarder* aForwarder,
                                 TextureFlags aFlags)
{
  if (aType == CanvasClientGLContext &&
      aForwarder->GetCompositorBackendType() == LAYERS_OPENGL) {
    return new CanvasClientSurfaceStream(aForwarder, aFlags);
  }
  return new CanvasClient2D(aForwarder, aFlags);
}

void
CanvasClient::Updated()
{
  mForwarder->UpdateTexture(this, 1, mDeprecatedTextureClient->GetDescriptor());
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
  if (!mDeprecatedTextureClient) {
    mDeprecatedTextureClient = CreateDeprecatedTextureClient(TEXTURE_CONTENT);
    MOZ_ASSERT(mDeprecatedTextureClient, "Failed to create texture client");
  }

  bool isOpaque = (aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE);
  gfxASurface::gfxContentType contentType = isOpaque
                                              ? gfxASurface::CONTENT_COLOR
                                              : gfxASurface::CONTENT_COLOR_ALPHA;
  mDeprecatedTextureClient->EnsureAllocated(aSize, contentType);

  gfxASurface* surface = mDeprecatedTextureClient->LockSurface();
  aLayer->UpdateSurface(surface);
  mDeprecatedTextureClient->Unlock();
}

void
CanvasClientSurfaceStream::Updated()
{
  if (mNeedsUpdate) {
    mForwarder->UpdateTextureNoSwap(this, 1, mDeprecatedTextureClient->GetDescriptor());
    mNeedsUpdate = false;
  }
}


CanvasClientSurfaceStream::CanvasClientSurfaceStream(CompositableForwarder* aFwd,
                                                     TextureFlags aFlags)
: CanvasClient(aFwd, aFlags)
, mNeedsUpdate(false)
{
  mTextureInfo.mCompositableType = BUFFER_IMAGE_SINGLE;
}

void
CanvasClientSurfaceStream::Update(gfx::IntSize aSize, ClientCanvasLayer* aLayer)
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
    mNeedsUpdate = true;
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
      mNeedsUpdate = true;
    }
  }

  aLayer->Painted();
}

}
}
