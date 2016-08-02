/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositableClient.h"
#include <stdint.h>                     // for uint64_t, uint32_t
#include "gfxPlatform.h"                // for gfxPlatform
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "mozilla/layers/TextureClientOGL.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "mozilla/layers/PCompositableChild.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
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
      !mTextureClient->GetAllocator()->UsesImageBridge())
  {
    RefPtr<TextureClientReleaseTask> task = new TextureClientReleaseTask(mTextureClient);
    RefPtr<ClientIPCAllocator> allocator = mTextureClient->GetAllocator();
    mTextureClient = nullptr;
    allocator->AsClientAllocator()->GetMessageLoop()->PostTask(task.forget());
  } else {
    mTextureClient = nullptr;
  }
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

  if (mTextureClientRecycler) {
    mTextureClientRecycler->Destroy();
  }
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

already_AddRefed<TextureClient>
CompositableClient::CreateTextureClientFromSurface(gfx::SourceSurface* aSurface,
                                                   BackendSelector aSelector,
                                                   TextureFlags aTextureFlags,
                                                   TextureAllocationFlags aAllocFlags)
{
  return TextureClient::CreateFromSurface(GetForwarder()->AsTextureForwarder(),
                                          aSurface,
                                          GetForwarder()->GetCompositorBackendType(),
                                          aSelector,
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
    mTextureClientRecycler->ShrinkToMinimumSize();
  }
}

void
CompositableClient::HandleMemoryPressure()
{
  if (mTextureClientRecycler) {
    mTextureClientRecycler->ShrinkToMinimumSize();
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

  if(!mForwarder->UsesImageBridge()) {
    MOZ_ASSERT(NS_IsMainThread());
    mTextureClientRecycler = new layers::TextureClientRecycleAllocator(mForwarder);
    return mTextureClientRecycler;
  }

  // Handle a case that mForwarder is ImageBridge

  if (InImageBridgeChildThread()) {
    mTextureClientRecycler = new layers::TextureClientRecycleAllocator(mForwarder);
    return mTextureClientRecycler;
  }

  ReentrantMonitor barrier("CompositableClient::GetTextureClientRecycler");
  ReentrantMonitorAutoEnter mainThreadAutoMon(barrier);
  bool done = false;

  RefPtr<Runnable> runnable =
    NS_NewRunnableFunction([&]() {
      if (!mTextureClientRecycler) {
        mTextureClientRecycler = new layers::TextureClientRecycleAllocator(mForwarder);
      }
      ReentrantMonitorAutoEnter childThreadAutoMon(barrier);
      done = true;
      barrier.NotifyAll();
    });

  ImageBridgeChild::GetSingleton()->GetMessageLoop()->PostTask(runnable.forget());

  // should stop the thread until done.
  while (!done) {
    barrier.Wait();
  }

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
  if (mCompositable && mTexture && mCompositable->IsConnected()) {
    mCompositable->RemoveTexture(mTexture);
  }
}

} // namespace layers
} // namespace mozilla
