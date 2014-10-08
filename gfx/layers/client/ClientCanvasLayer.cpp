/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientCanvasLayer.h"
#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "SharedSurfaceEGL.h"           // for SurfaceFactory_EGLImage
#include "SharedSurfaceGL.h"            // for SurfaceFactory_GLTexture, etc
#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#include "gfxPrefs.h"                   // for WebGLForceLayersReadback

#ifdef XP_WIN
#include "SharedSurfaceANGLE.h"         // for SurfaceFactory_ANGLEShareHandle
#endif

#ifdef MOZ_WIDGET_GONK
#include "SharedSurfaceGralloc.h"
#endif

#ifdef XP_MACOSX
#include "SharedSurfaceIO.h"
#endif

using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

ClientCanvasLayer::~ClientCanvasLayer()
{
  MOZ_COUNT_DTOR(ClientCanvasLayer);
  if (mCanvasClient) {
    mCanvasClient->OnDetach();
    mCanvasClient = nullptr;
  }
}

void
ClientCanvasLayer::Initialize(const Data& aData)
{
  CopyableCanvasLayer::Initialize(aData);

  mCanvasClient = nullptr;

  if (!mGLContext)
    return;

  GLScreenBuffer* screen = mGLContext->Screen();

  SurfaceCaps caps;
  if (mGLFrontbuffer) {
    // The screen caps are irrelevant if we're using a separate frontbuffer.
    caps = mGLFrontbuffer->mHasAlpha ? SurfaceCaps::ForRGBA()
                                     : SurfaceCaps::ForRGB();
  } else {
    MOZ_ASSERT(screen);
    caps = screen->mCaps;
  }
  MOZ_ASSERT(caps.alpha == aData.mHasAlpha);

  UniquePtr<SurfaceFactory> factory;

  if (!gfxPrefs::WebGLForceLayersReadback()) {
    switch (ClientManager()->AsShadowForwarder()->GetCompositorBackendType()) {
      case mozilla::layers::LayersBackend::LAYERS_OPENGL: {
        if (mGLContext->GetContextType() == GLContextType::EGL) {
#ifdef MOZ_WIDGET_GONK
          TextureFlags flags = TextureFlags::DEALLOCATE_CLIENT |
                               TextureFlags::NEEDS_Y_FLIP;
          if (!aData.mIsGLAlphaPremult) {
            flags |= TextureFlags::NON_PREMULTIPLIED;
          }
          factory = MakeUnique<SurfaceFactory_Gralloc>(mGLContext,
                                                       caps,
                                                       flags,
                                                       ClientManager()->AsShadowForwarder());
#else
          bool isCrossProcess = !(XRE_GetProcessType() == GeckoProcessType_Default);
          if (!isCrossProcess) {
            // [Basic/OGL Layers, OMTC] WebGL layer init.
            factory = SurfaceFactory_EGLImage::Create(mGLContext, caps);
          } else {
            // we could do readback here maybe
            NS_NOTREACHED("isCrossProcess but not on native B2G!");
          }
#endif
        } else {
          // [Basic Layers, OMTC] WebGL layer init.
          // Well, this *should* work...
#ifdef XP_MACOSX
          factory = SurfaceFactory_IOSurface::Create(mGLContext, caps);
#else
          GLContext* nullConsGL = nullptr; // Bug 1050044.
          factory = MakeUnique<SurfaceFactory_GLTexture>(mGLContext, nullConsGL, caps);
#endif
        }
        break;
      }
      case mozilla::layers::LayersBackend::LAYERS_D3D10:
      case mozilla::layers::LayersBackend::LAYERS_D3D11: {
#ifdef XP_WIN
        if (mGLContext->IsANGLE()) {
          factory = SurfaceFactory_ANGLEShareHandle::Create(mGLContext, caps);
        }
#endif
        break;
      }
      default:
        break;
    }
  }

  if (mGLFrontbuffer) {
    // We're using a source other than the one in the default screen.
    // (SkiaGL)
    mFactory = Move(factory);
    if (!mFactory) {
      // Absolutely must have a factory here, so create a basic one
      mFactory = MakeUnique<SurfaceFactory_Basic>(mGLContext, caps);
    }
  } else {
    if (factory)
      screen->Morph(Move(factory));
  }
}

void
ClientCanvasLayer::RenderLayer()
{
  PROFILER_LABEL("ClientCanvasLayer", "RenderLayer",
    js::ProfileEntry::Category::GRAPHICS);

  if (GetMaskLayer()) {
    ToClientLayer(GetMaskLayer())->RenderLayer();
  }

  if (!IsDirty()) {
    return;
  }

  if (!mCanvasClient) {
    TextureFlags flags = TextureFlags::IMMEDIATE_UPLOAD;
    if (mNeedsYFlip) {
      flags |= TextureFlags::NEEDS_Y_FLIP;
    }

    if (!mGLContext) {
      // We don't support locking for buffer surfaces currently
      flags |= TextureFlags::IMMEDIATE_UPLOAD;
    }

    if (!mIsAlphaPremultiplied) {
      flags |= TextureFlags::NON_PREMULTIPLIED;
    }

    mCanvasClient = CanvasClient::CreateCanvasClient(GetCanvasClientType(),
                                                     ClientManager()->AsShadowForwarder(),
                                                     flags);
    if (!mCanvasClient) {
      return;
    }
    if (HasShadow()) {
      mCanvasClient->Connect();
      ClientManager()->AsShadowForwarder()->Attach(mCanvasClient, this);
    }
  }

  FirePreTransactionCallback();
  mCanvasClient->Update(gfx::IntSize(mBounds.width, mBounds.height), this);

  FireDidTransactionCallback();

  ClientManager()->Hold(this);
  mCanvasClient->Updated();
  mCanvasClient->OnTransaction();
}

CanvasClient::CanvasClientType
ClientCanvasLayer::GetCanvasClientType()
{
  if (mGLContext) {
    return CanvasClient::CanvasClientTypeShSurf;
  }
  return CanvasClient::CanvasClientSurface;
}

already_AddRefed<CanvasLayer>
ClientLayerManager::CreateCanvasLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ClientCanvasLayer> layer =
    new ClientCanvasLayer(this);
  CREATE_SHADOW(Canvas);
  return layer.forget();
}

}
}
