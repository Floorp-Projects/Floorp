/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENTPOOL_H
#define MOZILLA_GFX_TEXTURECLIENTPOOL_H

#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/RefPtr.h"
#include "TextureClient.h"
#include "nsITimer.h"
#include <stack>

namespace mozilla {
namespace layers {

class ISurfaceAllocator;

class TextureClientPool final
{
  ~TextureClientPool();

public:
  NS_INLINE_DECL_REFCOUNTING(TextureClientPool)

  TextureClientPool(gfx::SurfaceFormat aFormat, gfx::IntSize aSize,
                    uint32_t aMaxTextureClients,
                    uint32_t aShrinkTimeoutMsec,
                    ISurfaceAllocator *aAllocator);

  /**
   * Gets an allocated TextureClient of size and format that are determined
   * by the initialisation parameters given to the pool. This will either be
   * a cached client that was returned to the pool, or a newly allocated
   * client if one isn't available.
   *
   * All clients retrieved by this method should be returned using the return
   * functions, or reported lost so that the pool can manage its size correctly.
   */
  already_AddRefed<TextureClient> GetTextureClient();

  /**
   * Return a TextureClient that is no longer being used and is ready for
   * immediate re-use or destruction.
   */
  void ReturnTextureClient(TextureClient *aClient);

  /**
   * Return a TextureClient that is not yet ready to be reused, but will be
   * imminently.
   */
  void ReturnTextureClientDeferred(TextureClient *aClient);

  /**
   * Attempt to shrink the pool so that there are no more than
   * mMaxTextureClients clients outstanding.
   */
  void ShrinkToMaximumSize();

  /**
   * Attempt to shrink the pool so that there are no more than sMinCacheSize
   * unused clients.
   */
  void ShrinkToMinimumSize();

  /**
   * Return any clients to the pool that were previously returned in
   * ReturnTextureClientDeferred.
   */
  void ReturnDeferredClients();

  /**
   * Report that a client retrieved via GetTextureClient() has become
   * unusable, so that it will no longer be tracked.
   */
  void ReportClientLost();

  /**
   * Calling this will cause the pool to attempt to relinquish any unused
   * clients.
   */
  void Clear();

  gfx::SurfaceFormat GetFormat() { return mFormat; }

private:
  // The minimum size of the pool (the number of tiles that will be kept after
  // shrinking).
  static const uint32_t sMinCacheSize = 0;

  /// Format is passed to the TextureClient for buffer creation.
  gfx::SurfaceFormat mFormat;

  /// The width and height of the tiles to be used.
  gfx::IntSize mSize;

  // The maximum number of texture clients managed by this pool that we want
  // to remain active.
  uint32_t mMaxTextureClients;

  // The time in milliseconds before the pool will be shrunk to the minimum
  // size after returning a client.
  uint32_t mShrinkTimeoutMsec;

  /// This is a total number of clients in the wild and in the stack of
  /// deferred clients (see below).  So, the total number of clients in
  /// existence is always mOutstandingClients + the size of mTextureClients.
  uint32_t mOutstandingClients;

  // On b2g gonk, std::queue might be a better choice.
  // On ICS, fence wait happens implicitly before drawing.
  // Since JB, fence wait happens explicitly when fetching a client from the pool.
  std::stack<RefPtr<TextureClient> > mTextureClients;
  std::stack<RefPtr<TextureClient> > mTextureClientsDeferred;
  nsRefPtr<nsITimer> mTimer;
  RefPtr<ISurfaceAllocator> mSurfaceAllocator;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_TEXTURECLIENTPOOL_H */
