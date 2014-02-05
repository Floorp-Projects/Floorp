/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositableClient.h"
#include <stdint.h>                     // for uint64_t, uint32_t
#include "gfxPlatform.h"                // for gfxPlatform
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/TextureClient.h"  // for DeprecatedTextureClient, etc
#include "mozilla/layers/TextureClientOGL.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "gfxASurface.h"                // for gfxContentType
#ifdef XP_WIN
#include "mozilla/layers/TextureD3D9.h"
#include "mozilla/layers/TextureD3D11.h"
#include "gfxWindowsPlatform.h"
#include "gfx2DGlue.h"
#endif
#ifdef MOZ_X11
#include "mozilla/layers/TextureClientX11.h"
#endif

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

CompositableClient::CompositableClient(CompositableForwarder* aForwarder)
: mCompositableChild(nullptr)
, mForwarder(aForwarder)
{
  MOZ_COUNT_CTOR(CompositableClient);
}


CompositableClient::~CompositableClient()
{
  MOZ_COUNT_DTOR(CompositableClient);
  Destroy();
}

LayersBackend
CompositableClient::GetCompositorBackendType() const
{
  return mForwarder->GetCompositorBackendType();
}

void
CompositableClient::SetIPDLActor(CompositableChild* aChild)
{
  mCompositableChild = aChild;
}

CompositableChild*
CompositableClient::GetIPDLActor() const
{
  return mCompositableChild;
}

bool
CompositableClient::Connect()
{
  if (!GetForwarder() || GetIPDLActor()) {
    return false;
  }
  GetForwarder()->Connect(this);
  return true;
}

void
CompositableClient::Destroy()
{
  if (!mCompositableChild) {
    return;
  }
  mCompositableChild->SetClient(nullptr);
  mCompositableChild->Destroy();
  mCompositableChild = nullptr;
}

uint64_t
CompositableClient::GetAsyncID() const
{
  if (mCompositableChild) {
    return mCompositableChild->GetAsyncID();
  }
  return 0; // zero is always an invalid async ID
}


void
CompositableChild::Destroy()
{
  Send__delete__(this);
}

TemporaryRef<DeprecatedTextureClient>
CompositableClient::CreateDeprecatedTextureClient(DeprecatedTextureClientType aDeprecatedTextureClientType,
                                                  gfxContentType aContentType)
{
  MOZ_ASSERT(GetForwarder(), "Can't create a texture client if the compositable is not connected to the compositor.");
  LayersBackend parentBackend = GetForwarder()->GetCompositorBackendType();
  RefPtr<DeprecatedTextureClient> result;

  switch (aDeprecatedTextureClientType) {
  case TEXTURE_SHARED_GL:
  case TEXTURE_SHARED_GL_EXTERNAL:
  case TEXTURE_STREAM_GL:
    MOZ_CRASH("Unsupported. this should not be reached");
  case TEXTURE_YCBCR:
    if (parentBackend == LayersBackend::LAYERS_D3D9 ||
        parentBackend == LayersBackend::LAYERS_D3D11 ||
        parentBackend == LayersBackend::LAYERS_BASIC) {
      result = new DeprecatedTextureClientShmemYCbCr(GetForwarder(), GetTextureInfo());
    } else {
      MOZ_CRASH("Unsupported. this should not be reached");
    }
    break;
  case TEXTURE_CONTENT:
#ifdef XP_WIN
    if (parentBackend == LayersBackend::LAYERS_D3D11 && gfxWindowsPlatform::GetPlatform()->GetD2DDevice()) {
      result = new DeprecatedTextureClientD3D11(GetForwarder(), GetTextureInfo());
      break;
    }
    if (parentBackend == LayersBackend::LAYERS_D3D9 &&
        !GetForwarder()->ForwardsToDifferentProcess()) {
      // We can't use a d3d9 texture for an RGBA surface because we cannot get a DC for
      // for a gfxWindowsSurface.
      // We have to wait for the compositor thread to create a d3d9 device before we
      // can create d3d9 textures on the main thread (because we need to reset on the
      // compositor thread, and the d3d9 device must be reset on the same thread it was
      // created on).
      if (aContentType == gfxContentType::COLOR_ALPHA ||
          !gfxWindowsPlatform::GetPlatform()->GetD3D9Device()) {
        result = new DeprecatedTextureClientDIB(GetForwarder(), GetTextureInfo());
      } else {
        result = new DeprecatedTextureClientD3D9(GetForwarder(), GetTextureInfo());
      }
      break;
    }
#endif
     // fall through to TEXTURE_SHMEM
  case TEXTURE_SHMEM:
    result = new DeprecatedTextureClientShmem(GetForwarder(), GetTextureInfo());
    break;
  case TEXTURE_FALLBACK:
#ifdef XP_WIN
    if (parentBackend == LayersBackend::LAYERS_D3D11 ||
        parentBackend == LayersBackend::LAYERS_D3D9) {
      result = new DeprecatedTextureClientShmem(GetForwarder(), GetTextureInfo());
    }
#endif
    break;
  default:
    MOZ_ASSERT(false, "Unhandled texture client type");
  }

  // If we couldn't create an appropriate texture client,
  // then return nullptr so the caller can chose another
  // type.
  if (!result) {
    return nullptr;
  }

  MOZ_ASSERT(result->SupportsType(aDeprecatedTextureClientType),
             "Created the wrong texture client?");
  result->SetFlags(GetTextureInfo().mTextureFlags);

  return result.forget();
}

TemporaryRef<BufferTextureClient>
CompositableClient::CreateBufferTextureClient(SurfaceFormat aFormat,
                                              TextureFlags aTextureFlags)
{
// XXX - Once bug 908196 is fixed, we can use gralloc textures here which will
// improve performances of videos using SharedPlanarYCbCrImage on b2g.
//#ifdef MOZ_WIDGET_GONK
//  {
//    RefPtr<BufferTextureClient> result = new GrallocTextureClientOGL(this,
//                                                                     aFormat,
//                                                                     aTextureFlags);
//    return result.forget();
//  }
//#endif
  if (gfxPlatform::GetPlatform()->PreferMemoryOverShmem()) {
    RefPtr<BufferTextureClient> result = new MemoryTextureClient(this, aFormat, aTextureFlags);
    return result.forget();
  }
  RefPtr<BufferTextureClient> result = new ShmemTextureClient(this, aFormat, aTextureFlags);
  return result.forget();
}

TemporaryRef<TextureClient>
CompositableClient::CreateTextureClientForDrawing(SurfaceFormat aFormat,
                                                  TextureFlags aTextureFlags)
{
  RefPtr<TextureClient> result;

#ifdef XP_WIN
  LayersBackend parentBackend = GetForwarder()->GetCompositorBackendType();
  if (parentBackend == LayersBackend::LAYERS_D3D11 && gfxWindowsPlatform::GetPlatform()->GetD2DDevice() &&
      !(aTextureFlags & TEXTURE_ALLOC_FALLBACK)) {
    result = new TextureClientD3D11(aFormat, aTextureFlags);
  }
  if (parentBackend == LayersBackend::LAYERS_D3D9 &&
      !GetForwarder()->ForwardsToDifferentProcess() &&
      !(aTextureFlags & TEXTURE_ALLOC_FALLBACK)) {
    // non-DIB textures don't work with alpha, see notes in TextureD3D9.
    if (ContentForFormat(aFormat) != gfxContentType::COLOR) {
      result = new DIBTextureClientD3D9(aFormat, aTextureFlags);
    } else {
      result = new CairoTextureClientD3D9(aFormat, aTextureFlags);
    }
  }
#endif

#ifdef MOZ_X11
  LayersBackend parentBackend = GetForwarder()->GetCompositorBackendType();
  if (parentBackend == LayersBackend::LAYERS_BASIC &&
      gfxPlatform::GetPlatform()->ScreenReferenceSurface()->GetType() == gfxSurfaceType::Xlib &&
      !(aTextureFlags & TEXTURE_ALLOC_FALLBACK))
  {
    result = new TextureClientX11(aFormat, aTextureFlags);
  }
#endif

  // Can't do any better than a buffer texture client.
  if (!result) {
    result = CreateBufferTextureClient(aFormat, aTextureFlags);
  }

  MOZ_ASSERT(!result || result->AsTextureClientDrawTarget(),
             "Not a TextureClientDrawTarget?");
  return result;
}

bool
CompositableClient::AddTextureClient(TextureClient* aClient)
{
  return aClient->InitIPDLActor(mForwarder);
}

void
CompositableClient::OnTransaction()
{
}

} // namespace layers
} // namespace mozilla
