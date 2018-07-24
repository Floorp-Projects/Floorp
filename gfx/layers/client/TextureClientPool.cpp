/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClientPool.h"
#include "CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/TextureForwarder.h"
#include "mozilla/layers/TiledContentClient.h"

#include "gfxPrefs.h"

#include "nsComponentManagerUtils.h"

#define TCP_LOG(...)
//#define TCP_LOG(...) printf_stderr(__VA_ARGS__);

namespace mozilla {
namespace layers {

// We want to shrink to our maximum size of N unused tiles
// after a timeout to allow for short-term budget requirements
static void
ShrinkCallback(nsITimer *aTimer, void *aClosure)
{
  static_cast<TextureClientPool*>(aClosure)->ShrinkToMaximumSize();
}

// After a certain amount of inactivity, let's clear the pool so that
// we don't hold onto tiles needlessly. In general, allocations are
// cheap enough that re-allocating isn't an issue unless we're allocating
// at an inopportune time (e.g. mid-animation).
static void
ClearCallback(nsITimer *aTimer, void *aClosure)
{
  static_cast<TextureClientPool*>(aClosure)->Clear();
}

TextureClientPool::TextureClientPool(LayersBackend aLayersBackend,
                                     bool aSupportsTextureDirectMapping,
                                     int32_t aMaxTextureSize,
                                     gfx::SurfaceFormat aFormat,
                                     gfx::IntSize aSize,
                                     TextureFlags aFlags,
                                     uint32_t aShrinkTimeoutMsec,
                                     uint32_t aClearTimeoutMsec,
                                     uint32_t aInitialPoolSize,
                                     uint32_t aPoolUnusedSize,
                                     TextureForwarder* aAllocator)
  : mBackend(aLayersBackend)
  , mMaxTextureSize(aMaxTextureSize)
  , mFormat(aFormat)
  , mSize(aSize)
  , mFlags(aFlags)
  , mShrinkTimeoutMsec(aShrinkTimeoutMsec)
  , mClearTimeoutMsec(aClearTimeoutMsec)
  , mInitialPoolSize(aInitialPoolSize)
  , mPoolUnusedSize(aPoolUnusedSize)
  , mOutstandingClients(0)
  , mSurfaceAllocator(aAllocator)
  , mDestroyed(false)
  , mSupportsTextureDirectMapping(aSupportsTextureDirectMapping)
{
  TCP_LOG("TexturePool %p created with maximum unused texture clients %u\n",
      this, mInitialPoolSize);
  mShrinkTimer = NS_NewTimer();
  mClearTimer = NS_NewTimer();
  if (aFormat == gfx::SurfaceFormat::UNKNOWN) {
    gfxWarning() << "Creating texture pool for SurfaceFormat::UNKNOWN format";
  }
}

TextureClientPool::~TextureClientPool()
{
  mShrinkTimer->Cancel();
  mClearTimer->Cancel();
}

#ifdef GFX_DEBUG_TRACK_CLIENTS_IN_POOL
static bool TestClientPool(const char* what,
                           TextureClient* aClient,
                           TextureClientPool* aPool)
{
  if (!aClient || !aPool) {
    return false;
  }

  TextureClientPool* actual = aClient->mPoolTracker;
  bool ok = (actual == aPool);
  if (ok) {
    ok = (aClient->GetFormat() == aPool->GetFormat());
  }

  if (!ok) {
    if (actual) {
      gfxCriticalError() << "Pool error(" << what << "): "
                   << aPool << "-" << aPool->GetFormat() << ", "
                   << actual << "-" << actual->GetFormat() << ", "
                   << aClient->GetFormat();
      MOZ_CRASH("GFX: Crashing with actual");
    } else {
      gfxCriticalError() << "Pool error(" << what << "): "
                   << aPool << "-" << aPool->GetFormat() << ", nullptr, "
                   << aClient->GetFormat();
      MOZ_CRASH("GFX: Crashing without actual");
    }
  }
  return ok;
}
#endif

already_AddRefed<TextureClient>
TextureClientPool::GetTextureClient()
{
  // Try to fetch a client from the pool
  RefPtr<TextureClient> textureClient;

  // We initially allocate mInitialPoolSize for our pool. If we run
  // out of TextureClients, we allocate additional TextureClients to try and keep around
  // mPoolUnusedSize
  if (mTextureClients.empty()) {
    AllocateTextureClient();
  }

  if (mTextureClients.empty()) {
    // All our allocations failed, return nullptr
    return nullptr;
  }

  mOutstandingClients++;
  textureClient = mTextureClients.top();
  mTextureClients.pop();
#ifdef GFX_DEBUG_TRACK_CLIENTS_IN_POOL
  if (textureClient) {
    textureClient->mPoolTracker = this;
  }
  DebugOnly<bool> ok = TestClientPool("fetch", textureClient, this);
  MOZ_ASSERT(ok);
#endif
  TCP_LOG("TexturePool %p giving %p from pool; size %u outstanding %u\n",
      this, textureClient.get(), mTextureClients.size(), mOutstandingClients);

  return textureClient.forget();
}

void
TextureClientPool::AllocateTextureClient()
{
  TCP_LOG("TexturePool %p allocating TextureClient, outstanding %u\n",
      this, mOutstandingClients);

  TextureAllocationFlags allocFlags = ALLOC_DEFAULT;

  if (mSupportsTextureDirectMapping && std::max(mSize.width, mSize.height) <= mMaxTextureSize) {
    allocFlags = TextureAllocationFlags(allocFlags | ALLOC_ALLOW_DIRECT_MAPPING);
  }

  RefPtr<TextureClient> newClient;
  if (gfxPrefs::ForceShmemTiles()) {
    // gfx::BackendType::NONE means use the content backend
    newClient =
      TextureClient::CreateForRawBufferAccess(mSurfaceAllocator,
                                              mFormat, mSize,
                                              gfx::BackendType::NONE,
                                              mBackend,
                                              mFlags, allocFlags);
  } else {
    newClient =
      TextureClient::CreateForDrawing(mSurfaceAllocator,
                                      mFormat, mSize,
                                      mBackend,
                                      mMaxTextureSize,
                                      BackendSelector::Content,
                                      mFlags, allocFlags);
  }

  if (newClient) {
    mTextureClients.push(newClient);
  }
}

void
TextureClientPool::ResetTimers()
{
  // Shrink down if we're beyond our maximum size
  if (mShrinkTimeoutMsec &&
      mTextureClients.size() + mTextureClientsDeferred.size() > mPoolUnusedSize) {
    TCP_LOG("TexturePool %p scheduling a shrink-to-max-size\n", this);
    mShrinkTimer->InitWithNamedFuncCallback(
      ShrinkCallback,
      this,
      mShrinkTimeoutMsec,
      nsITimer::TYPE_ONE_SHOT,
      "layers::TextureClientPool::ResetTimers");
  }

  // Clear pool after a period of inactivity to reduce memory consumption
  if (mClearTimeoutMsec) {
    TCP_LOG("TexturePool %p scheduling a clear\n", this);
    mClearTimer->InitWithNamedFuncCallback(
      ClearCallback,
      this,
      mClearTimeoutMsec,
      nsITimer::TYPE_ONE_SHOT,
      "layers::TextureClientPool::ResetTimers");
  }
}

void
TextureClientPool::ReturnTextureClient(TextureClient *aClient)
{
  if (!aClient || mDestroyed) {
    return;
  }
#ifdef GFX_DEBUG_TRACK_CLIENTS_IN_POOL
  DebugOnly<bool> ok = TestClientPool("return", aClient, this);
  MOZ_ASSERT(ok);
#endif
  // Add the client to the pool:
  MOZ_ASSERT(mOutstandingClients > mTextureClientsDeferred.size());
  mOutstandingClients--;
  mTextureClients.push(aClient);
  TCP_LOG("TexturePool %p had client %p returned; size %u outstanding %u\n",
      this, aClient, mTextureClients.size(), mOutstandingClients);

  ResetTimers();
}

void
TextureClientPool::ReturnTextureClientDeferred(TextureClient* aClient)
{
  if (!aClient || mDestroyed) {
    return;
  }
  MOZ_ASSERT(aClient->GetReadLock());
#ifdef GFX_DEBUG_TRACK_CLIENTS_IN_POOL
  DebugOnly<bool> ok = TestClientPool("defer", aClient, this);
  MOZ_ASSERT(ok);
#endif
  mTextureClientsDeferred.push_back(aClient);
  TCP_LOG("TexturePool %p had client %p defer-returned, size %u outstanding %u\n",
      this, aClient, mTextureClientsDeferred.size(), mOutstandingClients);

  ResetTimers();
}

void
TextureClientPool::ShrinkToMaximumSize()
{
  // We're over our desired maximum size, immediately shrink down to the
  // maximum.
  //
  // We cull from the deferred TextureClients first, as we can't reuse those
  // until they get returned.
  uint32_t totalUnusedTextureClients = mTextureClients.size() + mTextureClientsDeferred.size();

  // If we have > mInitialPoolSize outstanding, then we want to keep around
  // mPoolUnusedSize at a maximum. If we have fewer than mInitialPoolSize
  // outstanding, then keep around the entire initial pool size.
  uint32_t targetUnusedClients;
  if (mOutstandingClients > mInitialPoolSize) {
    targetUnusedClients = mPoolUnusedSize;
  } else {
    targetUnusedClients = mInitialPoolSize;
  }

  TCP_LOG("TexturePool %p shrinking to maximum unused size %u; current pool size %u; total outstanding %u\n",
      this, targetUnusedClients, totalUnusedTextureClients, mOutstandingClients);

  while (totalUnusedTextureClients > targetUnusedClients) {
    if (!mTextureClientsDeferred.empty()) {
      mOutstandingClients--;
      TCP_LOG("TexturePool %p dropped deferred client %p; %u remaining\n",
          this, mTextureClientsDeferred.front().get(),
          mTextureClientsDeferred.size() - 1);
      mTextureClientsDeferred.pop_front();
    } else {
      TCP_LOG("TexturePool %p dropped non-deferred client %p; %u remaining\n",
          this, mTextureClients.top().get(), mTextureClients.size() - 1);
      mTextureClients.pop();
    }
    totalUnusedTextureClients--;
  }
}

void
TextureClientPool::ReturnDeferredClients()
{
  if (mTextureClientsDeferred.empty()) {
    return;
  }

  TCP_LOG("TexturePool %p returning %u deferred clients to pool\n",
      this, mTextureClientsDeferred.size());

  ReturnUnlockedClients();
  ShrinkToMaximumSize();
}

void
TextureClientPool::ReturnUnlockedClients()
{
  for (auto it = mTextureClientsDeferred.begin(); it != mTextureClientsDeferred.end();) {
    MOZ_ASSERT((*it)->GetReadLock()->AsNonBlockingLock()->GetReadCount() >= 1);
    // Last count is held by the lock itself.
    if (!(*it)->IsReadLocked()) {
      mTextureClients.push(*it);
      it = mTextureClientsDeferred.erase(it);

      MOZ_ASSERT(mOutstandingClients > 0);
      mOutstandingClients--;
    } else {
      it++;
    }
  }
}

void
TextureClientPool::ReportClientLost()
{
  MOZ_ASSERT(mOutstandingClients > mTextureClientsDeferred.size());
  mOutstandingClients--;
  TCP_LOG("TexturePool %p getting report client lost; down to %u outstanding\n",
      this, mOutstandingClients);
}

void
TextureClientPool::Clear()
{
  TCP_LOG("TexturePool %p getting cleared\n", this);
  while (!mTextureClients.empty()) {
    TCP_LOG("TexturePool %p releasing client %p\n",
        this, mTextureClients.top().get());
    mTextureClients.pop();
  }
  while (!mTextureClientsDeferred.empty()) {
    MOZ_ASSERT(mOutstandingClients > 0);
    mOutstandingClients--;
    TCP_LOG("TexturePool %p releasing deferred client %p\n",
        this, mTextureClientsDeferred.front().get());
    mTextureClientsDeferred.pop_front();
  }
}

void TextureClientPool::Destroy()
{
  Clear();
  mDestroyed = true;
  mInitialPoolSize = 0;
  mPoolUnusedSize = 0;
}

} // namespace layers
} // namespace mozilla
