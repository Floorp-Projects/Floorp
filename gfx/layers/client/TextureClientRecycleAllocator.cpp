/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPlatform.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "TextureClientRecycleAllocator.h"

namespace mozilla {
namespace layers {

// Used to keep TextureClient's reference count stable as not to disrupt recycling.
class TextureClientHolder
{
  ~TextureClientHolder() {}
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TextureClientHolder)

  explicit TextureClientHolder(TextureClient* aClient)
    : mTextureClient(aClient)
  {}

  TextureClient* GetTextureClient()
  {
    return mTextureClient;
  }

  void ClearTextureClient() { mTextureClient = nullptr; }
protected:
  RefPtr<TextureClient> mTextureClient;
};

TextureClientRecycleAllocator::TextureClientRecycleAllocator(CompositableForwarder* aAllocator)
  : mSurfaceAllocator(aAllocator)
  , mMaxPooledSize(kMaxPooledSized)
  , mLock("TextureClientRecycleAllocatorImp.mLock")
{
}

TextureClientRecycleAllocator::~TextureClientRecycleAllocator()
{
  MutexAutoLock lock(mLock);
  while (!mPooledClients.empty()) {
    mPooledClients.pop();
  }
  MOZ_ASSERT(mInUseClients.empty());
}

void
TextureClientRecycleAllocator::SetMaxPoolSize(uint32_t aMax)
{
  mMaxPooledSize = aMax;
}

class TextureClientRecycleTask : public Task
{
public:
  explicit TextureClientRecycleTask(TextureClient* aClient, TextureFlags aFlags)
    : mTextureClient(aClient)
    , mFlags(aFlags)
  {}

  virtual void Run() override
  {
    mTextureClient->RecycleTexture(mFlags);
  }

private:
  mozilla::RefPtr<TextureClient> mTextureClient;
  TextureFlags mFlags;
};

already_AddRefed<TextureClient>
TextureClientRecycleAllocator::CreateOrRecycle(gfx::SurfaceFormat aFormat,
                                               gfx::IntSize aSize,
                                               BackendSelector aSelector,
                                               TextureFlags aTextureFlags,
                                               TextureAllocationFlags aAllocFlags)
{
  // TextureAllocationFlags is actually used only by ContentClient.
  // This class does not handle ConteClient's TextureClient allocation.
  MOZ_ASSERT(aAllocFlags == TextureAllocationFlags::ALLOC_DEFAULT ||
             aAllocFlags == TextureAllocationFlags::ALLOC_DISALLOW_BUFFERTEXTURECLIENT);
  MOZ_ASSERT(!(aTextureFlags & TextureFlags::RECYCLE));
  aTextureFlags = aTextureFlags | TextureFlags::RECYCLE; // Set recycle flag

  RefPtr<TextureClientHolder> textureHolder;

  {
    MutexAutoLock lock(mLock);
    if (!mPooledClients.empty()) {
      textureHolder = mPooledClients.top();
      mPooledClients.pop();
      Task* task = nullptr;
      // If a pooled TextureClient is not compatible, release it.
      if (textureHolder->GetTextureClient()->GetFormat() != aFormat ||
          textureHolder->GetTextureClient()->GetSize() != aSize) {
        // Release TextureClient.
        task = new TextureClientReleaseTask(textureHolder->GetTextureClient());
        textureHolder->ClearTextureClient();
        textureHolder = nullptr;
      } else {
        task = new TextureClientRecycleTask(textureHolder->GetTextureClient(), aTextureFlags);
      }
      mSurfaceAllocator->GetMessageLoop()->PostTask(FROM_HERE, task);
    }
  }

  if (!textureHolder) {
    // Allocate new TextureClient
    RefPtr<TextureClient> texture = Allocate(aFormat, aSize, aSelector, aTextureFlags, aAllocFlags);
    if (!texture) {
      return nullptr;
    }
    textureHolder = new TextureClientHolder(texture);
  }

  {
    MutexAutoLock lock(mLock);
    MOZ_ASSERT(mInUseClients.find(textureHolder->GetTextureClient()) == mInUseClients.end());
    // Register TextureClient
    mInUseClients[textureHolder->GetTextureClient()] = textureHolder;
  }
  RefPtr<TextureClient> client(textureHolder->GetTextureClient());

  // Make sure the texture holds a reference to us, and ask it to call RecycleTextureClient when its
  // ref count drops to 1.
  client->SetRecycleAllocator(this);
  return client.forget();
}

already_AddRefed<TextureClient>
TextureClientRecycleAllocator::Allocate(gfx::SurfaceFormat aFormat,
                                        gfx::IntSize aSize,
                                        BackendSelector aSelector,
                                        TextureFlags aTextureFlags,
                                        TextureAllocationFlags aAllocFlags)
{
  return TextureClient::CreateForDrawing(mSurfaceAllocator, aFormat, aSize, aSelector,
                                         aTextureFlags, aAllocFlags);
}

void
TextureClientRecycleAllocator::RecycleTextureClient(TextureClient* aClient)
{
  // Clearing the recycle allocator drops a reference, so make sure we stay alive
  // for the duration of this function.
  RefPtr<TextureClientRecycleAllocator> kungFuDeathGrip(this);
  aClient->SetRecycleAllocator(nullptr);

  RefPtr<TextureClientHolder> textureHolder;
  {
    MutexAutoLock lock(mLock);
    if (mInUseClients.find(aClient) != mInUseClients.end()) {
      textureHolder = mInUseClients[aClient]; // Keep reference count of TextureClientHolder within lock.
      if (mPooledClients.size() < mMaxPooledSize) {
        mPooledClients.push(textureHolder);
      }
      mInUseClients.erase(aClient);
    }
  }
}

} // namespace layers
} // namespace mozilla
