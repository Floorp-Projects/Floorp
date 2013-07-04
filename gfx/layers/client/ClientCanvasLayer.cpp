/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientCanvasLayer.h"
#include "gfxPlatform.h"
#include "SurfaceStream.h"
#include "SharedSurfaceGL.h"
#include "SharedSurfaceEGL.h"
#ifdef MOZ_WIDGET_GONK
#include "SharedSurfaceGralloc.h"
#endif

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

void
ClientCanvasLayer::Initialize(const Data& aData)
{
  CopyableCanvasLayer::Initialize(aData);
 
  mCanvasClient = nullptr;

  if (mGLContext) {
    GLScreenBuffer* screen = mGLContext->Screen();
    SurfaceStreamType streamType =
        SurfaceStream::ChooseGLStreamType(SurfaceStream::OffMainThread,
                                          screen->PreserveBuffer());
    SurfaceFactory_GL* factory = nullptr;
    if (!mForceReadback) {
      if (ClientManager()->GetCompositorBackendType() == mozilla::layers::LAYERS_OPENGL) {
        if (mGLContext->GetEGLContext()) {
          bool isCrossProcess = !(XRE_GetProcessType() == GeckoProcessType_Default);

          if (!isCrossProcess) {
            // [Basic/OGL Layers, OMTC] WebGL layer init.
            factory = SurfaceFactory_EGLImage::Create(mGLContext, screen->Caps());
          } else {
            // [Basic/OGL Layers, OOPC] WebGL layer init. (Out Of Process Compositing)
#ifdef MOZ_WIDGET_GONK
            factory = new SurfaceFactory_Gralloc(mGLContext, screen->Caps(), ClientManager());
#else
            // we could do readback here maybe
            NS_NOTREACHED("isCrossProcess but not on native B2G!");
#endif
          }
        } else {
          // [Basic Layers, OMTC] WebGL layer init.
          // Well, this *should* work...
          factory = new SurfaceFactory_GLTexture(mGLContext, nullptr, screen->Caps());
        }
      }
    }

    if (factory) {
      screen->Morph(factory, streamType);
    }
  }
}

void
ClientCanvasLayer::RenderLayer()
{
  PROFILER_LABEL("ClientCanvasLayer", "Paint");
  if (!IsDirty()) {
    return;
  }

  if (GetMaskLayer()) {
    ToClientLayer(GetMaskLayer())->RenderLayer();
  }
  
  if (!mCanvasClient) {
    TextureFlags flags = 0;
    if (mNeedsYFlip) {
      flags |= NeedsYFlip;
    }

    bool isCrossProcess = !(XRE_GetProcessType() == GeckoProcessType_Default);
    //Append OwnByClient flag for streaming buffer under OOPC case
    if (isCrossProcess && mGLContext) {
      GLScreenBuffer* screen = mGLContext->Screen();
      if (screen && screen->Stream()) {
        flags |= OwnByClient;
      }
    }
    mCanvasClient = CanvasClient::CreateCanvasClient(GetCompositableClientType(),
                                                     ClientManager(), flags);
    if (!mCanvasClient) {
      return;
    }
    if (HasShadow()) {
      mCanvasClient->Connect();
      ClientManager()->Attach(mCanvasClient, this);
    }
  }
  
  FirePreTransactionCallback();
  mCanvasClient->Update(gfx::IntSize(mBounds.width, mBounds.height), this);

  FireDidTransactionCallback();

  ClientManager()->Hold(this);
  mCanvasClient->Updated();
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
