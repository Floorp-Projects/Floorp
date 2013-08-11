/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientCanvasLayer.h"
#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "GeckoProfilerImpl.h"          // for PROFILER_LABEL
#include "SharedSurfaceEGL.h"           // for SurfaceFactory_EGLImage
#include "SharedSurfaceGL.h"            // for SurfaceFactory_GLTexture, etc
#include "SurfaceStream.h"              // for SurfaceStream, etc
#include "SurfaceTypes.h"               // for SurfaceStreamType
#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#ifdef MOZ_WIDGET_GONK
#include "SharedSurfaceGralloc.h"
#endif
#ifdef XP_MACOSX
#include "SharedSurfaceIO.h"
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
#ifdef XP_MACOSX
          factory = new SurfaceFactory_IOSurface(mGLContext, screen->Caps());
#else
          factory = new SurfaceFactory_GLTexture(mGLContext, nullptr, screen->Caps());
#endif
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
    TextureFlags flags = TEXTURE_IMMEDIATE_UPLOAD;
    if (mNeedsYFlip) {
      flags |= TEXTURE_NEEDS_Y_FLIP;
    }

    bool isCrossProcess = !(XRE_GetProcessType() == GeckoProcessType_Default);
    //Append TEXTURE_DEALLOCATE_CLIENT flag for streaming buffer under OOPC case
    if (isCrossProcess && mGLContext) {
      GLScreenBuffer* screen = mGLContext->Screen();
      if (screen && screen->Stream()) {
        flags |= TEXTURE_DEALLOCATE_CLIENT;
      }
    }
    mCanvasClient = CanvasClient::CreateCanvasClient(GetCanvasClientType(),
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
  mCanvasClient->OnTransaction();
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
