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
#endif

namespace mozilla {
namespace layers {

CompositableClient::CompositableClient(CompositableForwarder* aForwarder)
: mNextTextureID(1)
, mCompositableChild(nullptr)
, mForwarder(aForwarder)
{
  MOZ_COUNT_CTOR(CompositableClient);
}


CompositableClient::~CompositableClient()
{
  MOZ_COUNT_DTOR(CompositableClient);
  Destroy();

  FlushTexturesToRemoveCallbacks();
  MOZ_ASSERT(mTexturesToRemove.Length() == 0, "would leak textures pending for deletion");
}

void
CompositableClient::FlushTexturesToRemoveCallbacks()
{
  std::map<uint64_t,TextureClientData*>::iterator it
    = mTexturesToRemoveCallbacks.begin();
  std::map<uint64_t,TextureClientData*>::iterator stop
    = mTexturesToRemoveCallbacks.end();
  for (; it != stop; ++it) {
    it->second->DeallocateSharedData(GetForwarder());
    delete it->second;
  }
  mTexturesToRemoveCallbacks.clear();
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
    if (parentBackend == LAYERS_OPENGL) {
      result = new DeprecatedTextureClientSharedOGL(GetForwarder(), GetTextureInfo());
    }
     break;
  case TEXTURE_SHARED_GL_EXTERNAL:
    if (parentBackend == LAYERS_OPENGL) {
      result = new DeprecatedTextureClientSharedOGLExternal(GetForwarder(), GetTextureInfo());
    }
    break;
  case TEXTURE_STREAM_GL:
    if (parentBackend == LAYERS_OPENGL) {
      result = new DeprecatedTextureClientStreamOGL(GetForwarder(), GetTextureInfo());
    }
    break;
  case TEXTURE_YCBCR:
    if (parentBackend == LAYERS_OPENGL ||
        parentBackend == LAYERS_D3D9 ||
        parentBackend == LAYERS_D3D11 ||
        parentBackend == LAYERS_BASIC) {
      result = new DeprecatedTextureClientShmemYCbCr(GetForwarder(), GetTextureInfo());
    }
    break;
  case TEXTURE_CONTENT:
#ifdef XP_WIN
    if (parentBackend == LAYERS_D3D11 && gfxWindowsPlatform::GetPlatform()->GetD2DDevice()) {
      result = new DeprecatedTextureClientD3D11(GetForwarder(), GetTextureInfo());
      break;
    }
    if (parentBackend == LAYERS_D3D9 &&
        !GetForwarder()->ForwardsToDifferentProcess()) {
      if (aContentType == GFX_CONTENT_COLOR_ALPHA) {
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
    if (parentBackend == LAYERS_D3D11 ||
        parentBackend == LAYERS_D3D9) {
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
CompositableClient::CreateBufferTextureClient(gfx::SurfaceFormat aFormat,
                                              uint32_t aTextureFlags)
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

TemporaryRef<BufferTextureClient>
CompositableClient::CreateBufferTextureClient(gfx::SurfaceFormat aFormat)
{
  return CreateBufferTextureClient(aFormat, TEXTURE_FLAGS_DEFAULT);
}

void
CompositableClient::AddTextureClient(TextureClient* aClient)
{
  ++mNextTextureID;
  // 0 is always an invalid ID
  if (mNextTextureID == 0) {
    ++mNextTextureID;
  }
  aClient->SetID(mNextTextureID);
  mForwarder->AddTexture(this, aClient);
}

void
CompositableClient::RemoveTextureClient(TextureClient* aClient)
{
  MOZ_ASSERT(aClient);
  mTexturesToRemove.AppendElement(TextureIDAndFlags(aClient->GetID(),
                                                    aClient->GetFlags()));
  if (!(aClient->GetFlags() & TEXTURE_DEALLOCATE_HOST)) {
    TextureClientData* data = aClient->DropTextureData();
    if (data) {
      mTexturesToRemoveCallbacks[aClient->GetID()] = data;
    }
  }
  aClient->ClearID();
  aClient->MarkInvalid();
}

void
CompositableClient::OnReplyTextureRemoved(uint64_t aTextureID)
{
  std::map<uint64_t,TextureClientData*>::iterator it
    = mTexturesToRemoveCallbacks.find(aTextureID);
  if (it != mTexturesToRemoveCallbacks.end()) {
    it->second->DeallocateSharedData(GetForwarder());
    delete it->second;
    mTexturesToRemoveCallbacks.erase(it);
  }
}

void
CompositableClient::OnTransaction()
{
  for (unsigned i = 0; i < mTexturesToRemove.Length(); ++i) {
    const TextureIDAndFlags& texture = mTexturesToRemove[i];
    mForwarder->RemoveTexture(this, texture.mID, texture.mFlags);
  }
  mTexturesToRemove.Clear();
}


void
CompositableChild::ActorDestroy(ActorDestroyReason why)
{
  if (mCompositableClient) {
    mCompositableClient->OnActorDestroy();
  }
}
} // namespace layers
} // namespace mozilla
