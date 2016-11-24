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
#include "mozilla/layers/AsyncCanvasRenderer.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#include "gfxPrefs.h"                   // for WebGLForceLayersReadback

using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

ClientCanvasLayer::~ClientCanvasLayer()
{
  MOZ_COUNT_DTOR(ClientCanvasLayer);
  if (mBufferProvider) {
    mBufferProvider->ClearCachedResources();
  }
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

  auto forwarder = ClientManager()->AsShadowForwarder();

  mFlags = TextureFlags::ORIGIN_BOTTOM_LEFT;
  if (!aData.mIsGLAlphaPremult) {
    mFlags |= TextureFlags::NON_PREMULTIPLIED;
  }

  UniquePtr<SurfaceFactory> factory = GLScreenBuffer::CreateFactory(mGLContext, caps, forwarder, mFlags);

  if (mGLFrontbuffer || aData.mIsMirror) {
    // We're using a source other than the one in the default screen.
    // (SkiaGL)
    mFactory = Move(factory);
    if (!mFactory) {
      // Absolutely must have a factory here, so create a basic one
      mFactory = MakeUnique<SurfaceFactory_Basic>(mGLContext, caps, mFlags);
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

  RenderMaskLayers(this);

  if (!mCanvasClient) {
    TextureFlags flags = TextureFlags::DEFAULT;
    if (mOriginPos == gl::OriginPos::BottomLeft) {
      flags |= TextureFlags::ORIGIN_BOTTOM_LEFT;
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
      if (mAsyncRenderer) {
        static_cast<CanvasClientBridge*>(mCanvasClient.get())->SetLayer(this);
      } else {
        mCanvasClient->Connect();
        ClientManager()->AsShadowForwarder()->Attach(mCanvasClient, this);
      }
    }
  }

  if (mCanvasClient && mAsyncRenderer) {
    mCanvasClient->UpdateAsync(mAsyncRenderer);
  }

  if (!IsDirty()) {
    return;
  }
  Painted();

  FirePreTransactionCallback();
  if (mBufferProvider && mBufferProvider->GetTextureClient()) {
    if (!mBufferProvider->SetForwarder(ClientManager()->AsShadowForwarder())) {
      gfxCriticalNote << "BufferProvider::SetForwarder failed";
      return;
    }
    mCanvasClient->UpdateFromTexture(mBufferProvider->GetTextureClient());
  } else {
    mCanvasClient->Update(gfx::IntSize(mBounds.width, mBounds.height), this);
  }

  FireDidTransactionCallback();

  ClientManager()->Hold(this);
  mCanvasClient->Updated();
}

bool
ClientCanvasLayer::UpdateTarget(DrawTarget* aDestTarget)
{
  MOZ_ASSERT(aDestTarget);
  if (!aDestTarget) {
    return false;
  }

  RefPtr<SourceSurface> surface;

  if (!mGLContext) {
    AutoReturnSnapshot autoReturn;

    if (mAsyncRenderer) {
      surface = mAsyncRenderer->GetSurface();
    } else if (mBufferProvider) {
      surface = mBufferProvider->BorrowSnapshot();
      autoReturn.mSnapshot = &surface;
      autoReturn.mBufferProvider = mBufferProvider;
    }

    MOZ_ASSERT(surface);
    if (!surface) {
      return false;
    }

    aDestTarget->CopySurface(surface,
                             IntRect(0, 0, mBounds.width, mBounds.height),
                             IntPoint(0, 0));
    return true;
  }

  SharedSurface* frontbuffer = nullptr;
  if (mGLFrontbuffer) {
    frontbuffer = mGLFrontbuffer.get();
  } else {
    GLScreenBuffer* screen = mGLContext->Screen();
    const auto& front = screen->Front();
    if (front) {
      frontbuffer = front->Surf();
    }
  }

  if (!frontbuffer) {
    NS_WARNING("Null frame received.");
    return false;
  }

  IntSize readSize(frontbuffer->mSize);
  SurfaceFormat format = (GetContentFlags() & CONTENT_OPAQUE)
                          ? SurfaceFormat::B8G8R8X8
                          : SurfaceFormat::B8G8R8A8;
  bool needsPremult = frontbuffer->mHasAlpha && !mIsAlphaPremultiplied;

  // Try to read back directly into aDestTarget's output buffer
  uint8_t* destData;
  IntSize destSize;
  int32_t destStride;
  SurfaceFormat destFormat;
  if (aDestTarget->LockBits(&destData, &destSize, &destStride, &destFormat)) {
    if (destSize == readSize && destFormat == format) {
      RefPtr<DataSourceSurface> data =
        Factory::CreateWrappingDataSourceSurface(destData, destStride, destSize, destFormat);
      mGLContext->Readback(frontbuffer, data);
      if (needsPremult) {
        gfxUtils::PremultiplyDataSurface(data, data);
      }
      aDestTarget->ReleaseBits(destData);
      return true;
    }
    aDestTarget->ReleaseBits(destData);
  }

  RefPtr<DataSourceSurface> resultSurf = GetTempSurface(readSize, format);
  // There will already be a warning from inside of GetTempSurface, but
  // it doesn't hurt to complain:
  if (NS_WARN_IF(!resultSurf)) {
    return false;
  }

  // Readback handles Flush/MarkDirty.
  mGLContext->Readback(frontbuffer, resultSurf);
  if (needsPremult) {
    gfxUtils::PremultiplyDataSurface(resultSurf, resultSurf);
  }

  aDestTarget->CopySurface(resultSurf,
                           IntRect(0, 0, readSize.width, readSize.height),
                           IntPoint(0, 0));

  return true;
}

CanvasClient::CanvasClientType
ClientCanvasLayer::GetCanvasClientType()
{
  if (mAsyncRenderer) {
    return CanvasClient::CanvasClientAsync;
  }

  if (mGLContext) {
    return CanvasClient::CanvasClientTypeShSurf;
  }
  return CanvasClient::CanvasClientSurface;
}

already_AddRefed<CanvasLayer>
ClientLayerManager::CreateCanvasLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  RefPtr<ClientCanvasLayer> layer =
    new ClientCanvasLayer(this);
  CREATE_SHADOW(Canvas);
  return layer.forget();
}

} // namespace layers
} // namespace mozilla
