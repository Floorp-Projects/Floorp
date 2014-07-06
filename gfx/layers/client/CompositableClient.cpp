/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositableClient.h"
#include <stdint.h>                     // for uint64_t, uint32_t
#include "gfxPlatform.h"                // for gfxPlatform
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "mozilla/layers/TextureClientOGL.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "mozilla/layers/PCompositableChild.h"
#ifdef XP_WIN
#include "gfxWindowsPlatform.h"         // for gfxWindowsPlatform
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/layers/TextureD3D9.h"
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

/**
 * IPDL actor used by CompositableClient to match with its corresponding
 * CompositableHost on the compositor side.
 *
 * CompositableChild is owned by a CompositableClient.
 */
class CompositableChild : public PCompositableChild
                        , public AsyncTransactionTrackersHolder
{
public:
  CompositableChild()
  : mCompositableClient(nullptr), mAsyncID(0)
  {
    MOZ_COUNT_CTOR(CompositableChild);
  }

  virtual ~CompositableChild()
  {
    MOZ_COUNT_DTOR(CompositableChild);
  }

  virtual void ActorDestroy(ActorDestroyReason) MOZ_OVERRIDE {
    DestroyAsyncTransactionTrackersHolder();
    if (mCompositableClient) {
      mCompositableClient->mCompositableChild = nullptr;
    }
  }

  CompositableClient* mCompositableClient;

  uint64_t mAsyncID;
};

void
RemoveTextureFromCompositableTracker::ReleaseTextureClient()
{
  if (mTextureClient) {
    TextureClientReleaseTask* task = new TextureClientReleaseTask(mTextureClient);
    RefPtr<ISurfaceAllocator> allocator = mTextureClient->GetAllocator();
    mTextureClient = nullptr;
    allocator->GetMessageLoop()->PostTask(FROM_HERE, task);
  }
}

/* static */ void
CompositableClient::TransactionCompleteted(PCompositableChild* aActor, uint64_t aTransactionId)
{
  CompositableChild* child = static_cast<CompositableChild*>(aActor);
  child->TransactionCompleteted(aTransactionId);
}

/* static */ void
CompositableClient::HoldUntilComplete(PCompositableChild* aActor, AsyncTransactionTracker* aTracker)
{
  CompositableChild* child = static_cast<CompositableChild*>(aActor);
  child->HoldUntilComplete(aTracker);
}

/* static */ uint64_t
CompositableClient::GetTrackersHolderId(PCompositableChild* aActor)
{
  CompositableChild* child = static_cast<CompositableChild*>(aActor);
  return child->GetId();
}

/* static */ PCompositableChild*
CompositableClient::CreateIPDLActor()
{
  return new CompositableChild();
}

/* static */ bool
CompositableClient::DestroyIPDLActor(PCompositableChild* actor)
{
  delete actor;
  return true;
}

void
CompositableClient::InitIPDLActor(PCompositableChild* aActor, uint64_t aAsyncID)
{
  MOZ_ASSERT(aActor);
  CompositableChild* child = static_cast<CompositableChild*>(aActor);
  mCompositableChild = child;
  child->mCompositableClient = this;
  child->mAsyncID = aAsyncID;
}

/* static */ CompositableClient*
CompositableClient::FromIPDLActor(PCompositableChild* aActor)
{
  MOZ_ASSERT(aActor);
  return static_cast<CompositableChild*>(aActor)->mCompositableClient;
}

CompositableClient::CompositableClient(CompositableForwarder* aForwarder,
                                       TextureFlags aTextureFlags)
: mCompositableChild(nullptr)
, mForwarder(aForwarder)
, mTextureFlags(aTextureFlags)
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

PCompositableChild*
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
  mCompositableChild->mCompositableClient = nullptr;
  PCompositableChild::Send__delete__(mCompositableChild);
  mCompositableChild = nullptr;
}

uint64_t
CompositableClient::GetAsyncID() const
{
  if (mCompositableChild) {
    return mCompositableChild->mAsyncID;
  }
  return 0; // zero is always an invalid async ID
}

TemporaryRef<BufferTextureClient>
CompositableClient::CreateBufferTextureClient(SurfaceFormat aFormat,
                                              TextureFlags aTextureFlags,
                                              gfx::BackendType aMoz2DBackend)
{
  return TextureClient::CreateBufferTextureClient(GetForwarder(), aFormat,
                                                  aTextureFlags | mTextureFlags,
                                                  aMoz2DBackend);
}

TemporaryRef<TextureClient>
CompositableClient::CreateTextureClientForDrawing(SurfaceFormat aFormat,
                                                  TextureFlags aTextureFlags,
                                                  gfx::BackendType aMoz2DBackend,
                                                  const IntSize& aSizeHint)
{
  return TextureClient::CreateTextureClientForDrawing(GetForwarder(), aFormat,
                                                      aTextureFlags | mTextureFlags,
                                                      aMoz2DBackend,
                                                      aSizeHint);
}

bool
CompositableClient::AddTextureClient(TextureClient* aClient)
{
  if(!aClient || !aClient->IsAllocated()) {
    return false;
  }
  return aClient->InitIPDLActor(mForwarder);
}

void
CompositableClient::OnTransaction()
{
}

void
CompositableClient::UseTexture(TextureClient* aTexture)
{
  MOZ_ASSERT(aTexture);
  if (!aTexture) {
    return;
  }

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  FenceHandle handle = aTexture->GetAcquireFenceHandle();
  if (handle.IsValid()) {
    RefPtr<FenceDeliveryTracker> tracker = new FenceDeliveryTracker(handle);
    mForwarder->SendFenceHandle(tracker, aTexture->GetIPDLActor(), handle);
  }
#endif
  mForwarder->UseTexture(this, aTexture);
}

void
CompositableClient::RemoveTexture(TextureClient* aTexture)
{
  mForwarder->RemoveTextureFromCompositable(this, aTexture);
}

} // namespace layers
} // namespace mozilla
