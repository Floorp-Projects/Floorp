/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClientPool.h"
#include "CompositableClient.h"
#include "mozilla/layers/ISurfaceAllocator.h"

#include "gfxPrefs.h"

#include "nsComponentManagerUtils.h"

namespace mozilla {
namespace layers {

static void
ShrinkCallback(nsITimer *aTimer, void *aClosure)
{
  static_cast<TextureClientPool*>(aClosure)->ShrinkToMinimumSize();
}

TextureClientPool::TextureClientPool(gfx::SurfaceFormat aFormat, gfx::IntSize aSize,
                                     uint32_t aMaxTextureClients,
                                     uint32_t aShrinkTimeoutMsec,
                                     ISurfaceAllocator *aAllocator)
  : mFormat(aFormat)
  , mSize(aSize)
  , mMaxTextureClients(aMaxTextureClients)
  , mShrinkTimeoutMsec(aShrinkTimeoutMsec)
  , mOutstandingClients(0)
  , mSurfaceAllocator(aAllocator)
{
  mTimer = do_CreateInstance("@mozilla.org/timer;1");
}

TextureClientPool::~TextureClientPool()
{
  mTimer->Cancel();
}

TemporaryRef<TextureClient>
TextureClientPool::GetTextureClient()
{
  mOutstandingClients++;

  // Try to fetch a client from the pool
  RefPtr<TextureClient> textureClient;
  if (mTextureClients.size()) {
    textureClient = mTextureClients.top();
    mTextureClients.pop();
    return textureClient;
  }

  // We're increasing the number of outstanding TextureClients without reusing a
  // client, we may need to free a deferred-return TextureClient.
  ShrinkToMaximumSize();

  // No unused clients in the pool, create one
  if (gfxPrefs::ForceShmemTiles()) {
    // gfx::BackendType::NONE means use the content backend
    textureClient = TextureClient::CreateForRawBufferAccess(mSurfaceAllocator,
      mFormat, mSize, gfx::BackendType::NONE,
      TextureFlags::IMMEDIATE_UPLOAD, ALLOC_DEFAULT);
  } else {
    textureClient = TextureClient::CreateForDrawing(mSurfaceAllocator,
      mFormat, mSize, gfx::BackendType::NONE, TextureFlags::IMMEDIATE_UPLOAD);
  }

  return textureClient;
}

void
TextureClientPool::ReturnTextureClient(TextureClient *aClient)
{
  if (!aClient) {
    return;
  }
  MOZ_ASSERT(mOutstandingClients);
  mOutstandingClients--;

  // Add the client to the pool and shrink down if we're beyond our maximum size
  mTextureClients.push(aClient);
  ShrinkToMaximumSize();

  // Kick off the pool shrinking timer if there are still more unused texture
  // clients than our desired minimum cache size.
  if (mTextureClients.size() > sMinCacheSize) {
    mTimer->InitWithFuncCallback(ShrinkCallback, this, mShrinkTimeoutMsec,
                                 nsITimer::TYPE_ONE_SHOT);
  }
}

void
TextureClientPool::ReturnTextureClientDeferred(TextureClient *aClient)
{
  mTextureClientsDeferred.push(aClient);
  ShrinkToMaximumSize();
}

void
TextureClientPool::ShrinkToMaximumSize()
{
  uint32_t totalClientsOutstanding = mTextureClients.size() + mOutstandingClients;

  // We're over our desired maximum size, immediately shrink down to the
  // maximum, or zero if we have too many outstanding texture clients.
  // We cull from the deferred TextureClients first, as we can't reuse those
  // until they get returned.
  while (totalClientsOutstanding > mMaxTextureClients) {
    if (mTextureClientsDeferred.size()) {
      mOutstandingClients--;
      mTextureClientsDeferred.pop();
    } else {
      if (!mTextureClients.size()) {
        // Getting here means we're over our desired number of TextureClients
        // with none in the pool. This can happen for pathological cases, or
        // it could mean that mMaxTextureClients needs adjusting for whatever
        // device we're running on.
        break;
      }
      mTextureClients.pop();
    }
    totalClientsOutstanding--;
  }
}

void
TextureClientPool::ShrinkToMinimumSize()
{
  while (mTextureClients.size() > sMinCacheSize) {
    mTextureClients.pop();
  }
}

void
TextureClientPool::ReturnDeferredClients()
{
  while (!mTextureClientsDeferred.empty()) {
    mTextureClients.push(mTextureClientsDeferred.top());
    mTextureClientsDeferred.pop();

    MOZ_ASSERT(mOutstandingClients > 0);
    mOutstandingClients--;
  }
  ShrinkToMinimumSize();
  // Kick off the pool shrinking timer if there are still more unused texture
  // clients than our desired minimum cache size.
  if (mTextureClients.size() > sMinCacheSize) {
    mTimer->InitWithFuncCallback(ShrinkCallback, this, mShrinkTimeoutMsec,
                                 nsITimer::TYPE_ONE_SHOT);
  }
}

void
TextureClientPool::Clear()
{
  while (!mTextureClients.empty()) {
    mTextureClients.pop();
  }
  while (!mTextureClientsDeferred.empty()) {
    mOutstandingClients--;
    mTextureClientsDeferred.pop();
  }
}

}
}
