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
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#ifdef XP_WIN
#include "gfxWindowsPlatform.h"         // for gfxWindowsPlatform
#include "mozilla/layers/TextureD3D11.h"
#endif
#include "gfxUtils.h"
#include "IPDLActor.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
CompositableClient::InitIPDL(const CompositableHandle& aHandle)
{
  MOZ_ASSERT(aHandle);

  mForwarder->AssertInForwarderThread();

  mHandle = aHandle;
  mIsAsync = !NS_IsMainThread();
}

CompositableClient::CompositableClient(CompositableForwarder* aForwarder,
                                       TextureFlags aTextureFlags)
: mForwarder(aForwarder)
, mTextureFlags(aTextureFlags)
, mIsAsync(false)
{
}

CompositableClient::~CompositableClient()
{
  Destroy();
}

LayersBackend
CompositableClient::GetCompositorBackendType() const
{
  return mForwarder->GetCompositorBackendType();
}

bool
CompositableClient::Connect(ImageContainer* aImageContainer)
{
  MOZ_ASSERT(!mHandle);
  if (!GetForwarder() || mHandle) {
    return false;
  }

  GetForwarder()->AssertInForwarderThread();
  GetForwarder()->Connect(this, aImageContainer);
  return true;
}

bool
CompositableClient::IsConnected() const
{
  // CanSend() is only reliable in the same thread as the IPDL channel.
  mForwarder->AssertInForwarderThread();
  return !!mHandle;
}

void
CompositableClient::Destroy()
{
  if (mTextureClientRecycler) {
    mTextureClientRecycler->Destroy();
  }

  if (mHandle) {
    mForwarder->ReleaseCompositable(mHandle);
    mHandle = CompositableHandle();
  }
}

CompositableHandle
CompositableClient::GetAsyncHandle() const
{
  if (mIsAsync) {
    return mHandle;
  }
  return CompositableHandle();
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
  return TextureClient::CreateFromSurface(GetForwarder(),
                                          aSurface,
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

  if(!mForwarder->GetTextureForwarder()->UsesImageBridge()) {
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

  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
    "layers::CompositableClient::GetTextureClientRecycler", [&]() {
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
