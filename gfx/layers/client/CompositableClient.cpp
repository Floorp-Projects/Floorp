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
#include "gfxUtils.h"
#include "IPDLActor.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

/**
 * IPDL actor used by CompositableClient to match with its corresponding
 * CompositableHost on the compositor side.
 *
 * CompositableChild is owned by a CompositableClient.
 */
class CompositableChild : public ChildActor<PCompositableChild>
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

  virtual void ActorDestroy(ActorDestroyReason) override {
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
  if (mTextureClient &&
      mTextureClient->GetAllocator() &&
      !mTextureClient->GetAllocator()->IsImageBridgeChild())
  {
    TextureClientReleaseTask* task = new TextureClientReleaseTask(mTextureClient);
    RefPtr<ISurfaceAllocator> allocator = mTextureClient->GetAllocator();
    mTextureClient = nullptr;
    allocator->GetMessageLoop()->PostTask(FROM_HERE, task);
  } else {
    mTextureClient = nullptr;
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
CompositableClient::Connect(ImageContainer* aImageContainer)
{
  MOZ_ASSERT(!mCompositableChild);
  if (!GetForwarder() || GetIPDLActor()) {
    return false;
  }
  GetForwarder()->Connect(this, aImageContainer);
  return true;
}

bool
CompositableClient::IsConnected() const
{
  return mCompositableChild && mCompositableChild->CanSend();
}

void
CompositableClient::Destroy()
{
  if (!IsConnected()) {
    return;
  }

  // Send pending AsyncMessages before deleting CompositableChild since the former
  // might have references to the latter.
  mForwarder->SendPendingAsyncMessges();

  mCompositableChild->mCompositableClient = nullptr;
  mCompositableChild->Destroy(mForwarder);
  mCompositableChild = nullptr;
}

bool
CompositableClient::DestroyFallback(PCompositableChild* aActor)
{
  return aActor->SendDestroySync();
}

uint64_t
CompositableClient::GetAsyncID() const
{
  if (mCompositableChild) {
    return mCompositableChild->mAsyncID;
  }
  return 0; // zero is always an invalid async ID
}

already_AddRefed<TextureClient>
CompositableClient::CreateBufferTextureClient(gfx::SurfaceFormat aFormat,
                                              gfx::IntSize aSize,
                                              gfx::BackendType aMoz2DBackend,
                                              TextureFlags aTextureFlags)
{
  return TextureClient::CreateForRawBufferAccess(GetForwarder(),
                                                 aFormat, aSize, aMoz2DBackend,
                                                 aTextureFlags | mTextureFlags);
}

already_AddRefed<TextureClient>
CompositableClient::CreateTextureClientForDrawing(gfx::SurfaceFormat aFormat,
                                                  gfx::IntSize aSize,
                                                  BackendSelector aSelector,
                                                  TextureFlags aTextureFlags,
                                                  TextureAllocationFlags aAllocFlags)
{
  return TextureClient::CreateForDrawing(GetForwarder(),
                                         aFormat, aSize, aSelector,
                                         aTextureFlags | mTextureFlags,
                                         aAllocFlags);
}

bool
CompositableClient::AddTextureClient(TextureClient* aClient)
{
  if(!aClient) {
    return false;
  }
  aClient->SetAddedToCompositableClient();
  return aClient->InitIPDLActor(mForwarder);
}

void
CompositableClient::ClearCachedResources()
{
  if (mTextureClientRecycler) {
    mTextureClientRecycler = nullptr;
  }
}

void
CompositableClient::RemoveTexture(TextureClient* aTexture)
{
  mForwarder->RemoveTextureFromCompositable(this, aTexture);
}

TextureClientRecycleAllocator*
CompositableClient::GetTextureClientRecycler()
{
  if (mTextureClientRecycler) {
    return mTextureClientRecycler;
  }

  if (!mForwarder) {
    return nullptr;
  }

  mTextureClientRecycler =
    new layers::TextureClientRecycleAllocator(mForwarder);
  return mTextureClientRecycler;
}

void
CompositableClient::DumpTextureClient(std::stringstream& aStream,
                                      TextureClient* aTexture,
                                      TextureDumpMode aCompress)
{
  if (!aTexture) {
    return;
  }
  RefPtr<gfx::DataSourceSurface> dSurf = aTexture->GetAsSurface();
  if (!dSurf) {
    return;
  }
  if (aCompress == TextureDumpMode::Compress) {
    aStream << gfxUtils::GetAsLZ4Base64Str(dSurf).get();
  } else {
    aStream << gfxUtils::GetAsDataURI(dSurf).get();
  }
}

AutoRemoveTexture::~AutoRemoveTexture()
{
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  if (mCompositable && mTexture && mCompositable->IsConnected()) {
    // remove old buffer from CompositableHost
    RefPtr<AsyncTransactionWaiter> waiter = new AsyncTransactionWaiter();
    RefPtr<AsyncTransactionTracker> tracker =
        new RemoveTextureFromCompositableTracker(waiter);
    // Hold TextureClient until transaction complete.
    tracker->SetTextureClient(mTexture);
    mTexture->SetRemoveFromCompositableWaiter(waiter);
    // RemoveTextureFromCompositableAsync() expects CompositorChild's presence.
    mCompositable->GetForwarder()->RemoveTextureFromCompositableAsync(tracker, mCompositable, mTexture);
  }
#else
  if (mCompositable && mTexture) {
    mCompositable->RemoveTexture(mTexture);
  }
#endif
}

} // namespace layers
} // namespace mozilla
