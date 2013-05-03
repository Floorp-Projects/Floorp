/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureClientOGL.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/CompositableForwarder.h"
#ifdef XP_WIN
#include "mozilla/layers/TextureD3D11.h"
#include "gfxWindowsPlatform.h"
#endif

namespace mozilla {
namespace layers {

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

TemporaryRef<TextureClient>
CompositableClient::CreateTextureClient(TextureClientType aTextureClientType)
{
  MOZ_ASSERT(GetForwarder(), "Can't create a texture client if the compositable is not connected to the compositor.");
  LayersBackend parentBackend = GetForwarder()->GetCompositorBackendType();
  RefPtr<TextureClient> result;

  switch (aTextureClientType) {
  case TEXTURE_SHARED_GL:
    if (parentBackend == LAYERS_OPENGL) {
      result = new TextureClientSharedOGL(GetForwarder(), GetTextureInfo());
    }
     break;
  case TEXTURE_SHARED_GL_EXTERNAL:
    if (parentBackend == LAYERS_OPENGL) {
      result = new TextureClientSharedOGLExternal(GetForwarder(), GetTextureInfo());
    }
    break;
  case TEXTURE_STREAM_GL:
    if (parentBackend == LAYERS_OPENGL) {
      result = new TextureClientStreamOGL(GetForwarder(), GetTextureInfo());
    }
    break;
  case TEXTURE_YCBCR:
    if (parentBackend == LAYERS_OPENGL || parentBackend == LAYERS_D3D11) {
      result = new TextureClientShmemYCbCr(GetForwarder(), GetTextureInfo());
    }
    break;
  case TEXTURE_CONTENT:
#ifdef XP_WIN
    if (parentBackend == LAYERS_D3D11 && gfxWindowsPlatform::GetPlatform()->GetD2DDevice()) {
      result = new TextureClientD3D11(GetForwarder(), GetTextureInfo());
      break;
    }
#endif
     // fall through to TEXTURE_SHMEM
  case TEXTURE_SHMEM:
    result = new TextureClientShmem(GetForwarder(), GetTextureInfo());
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

  MOZ_ASSERT(result->SupportsType(aTextureClientType),
             "Created the wrong texture client?");
  result->SetFlags(GetTextureInfo().mTextureFlags);

  return result.forget();
}

} // namespace layers
} // namespace mozilla
