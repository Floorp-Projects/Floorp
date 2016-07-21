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
#include <list>

namespace mozilla {
namespace layers {

class ISurfaceAllocator;
class TextureForwarder;
class TextureReadLock;

class TextureClientAllocator
{
protected:
  virtual ~TextureClientAllocator() {}
public:
  NS_INLINE_DECL_REFCOUNTING(TextureClientAllocator)

  virtual already_AddRefed<TextureClient> GetTextureClient() = 0;

  /**
   * Return a TextureClient that is not yet ready to be reused, but will be
   * imminently.
   */
  virtual void ReturnTextureClientDeferred(TextureClient *aClient) = 0;

  virtual void ReportClientLost() = 0;
};

class TextureClientPool final : public TextureClientAllocator
{
  ~TextureClientPool();

public:
  TextureClientPool(LayersBackend aBackend,
                    gfx::SurfaceFormat aFormat,
                    gfx::IntSize aSize,
                    TextureFlags aFlags,
                    uint32_t aInitialPoolSize,
                    uint32_t aPoolIncrementSize,
                    TextureForwarder* aAllocator);

  /**
   * Gets an allocated TextureClient of size and format that are determined
   * by the initialisation parameters given to the pool. This will either be
   * a cached client that was returned to the pool, or a newly allocated
   * client if one isn't available.
   *
   * All clients retrieved by this method should be returned using the return
   * functions, or reported lost so that the pool can manage its size correctly.
   */
  already_AddRefed<TextureClient> GetTextureClient() override;

  /**
   * Return a TextureClient that is no longer being used and is ready for
   * immediate re-use or destruction.
   */
  void ReturnTextureClient(TextureClient *aClient);

  /**
   * Return a TextureClient that is not yet ready to be reused, but will be
   * imminently.
   */
  void ReturnTextureClientDeferred(TextureClient *aClient) override;

  /**
   * Return any clients to the pool that were previously returned in
   * ReturnTextureClientDeferred.
   */
  void ReturnDeferredClients();

  /**
   * Attempt to shrink the pool so that there are no more than
   * mInitialPoolSize outstanding.
   */
  void ShrinkToMaximumSize();

  /**
   * Report that a client retrieved via GetTextureClient() has become
   * unusable, so that it will no longer be tracked.
   */
  virtual void ReportClientLost() override;

  /**
   * Calling this will cause the pool to attempt to relinquish any unused
   * clients.
   */
  void Clear();

  LayersBackend GetBackend() const { return mBackend; }
  gfx::SurfaceFormat GetFormat() { return mFormat; }
  TextureFlags GetFlags() const { return mFlags; }

  /**
   * Clear the pool and put it in a state where it won't recycle any new texture.
   */
  void Destroy();

private:
  void ReturnUnlockedClients();

  /// We maintain a number of unused texture clients for immediate return from
  /// GetTextureClient(). This will normally be called if there are no
  /// TextureClients available in the pool, which ideally should only ever
  /// be at startup.
  void AllocateTextureClients(size_t aSize);

  /// Backend passed to the TextureClient for buffer creation.
  LayersBackend mBackend;

  /// Format is passed to the TextureClient for buffer creation.
  gfx::SurfaceFormat mFormat;

  /// The width and height of the tiles to be used.
  gfx::IntSize mSize;

  /// Flags passed to the TextureClient for buffer creation.
  const TextureFlags mFlags;

  // The initial number of unused texture clients to seed the pool with
  // on construction
  uint32_t mInitialPoolSize;

  // How many unused texture clients to try and keep around if we go over
  // the initial allocation
  uint32_t mPoolIncrementSize;

  /// This is a total number of clients in the wild and in the stack of
  /// deferred clients (see below).  So, the total number of clients in
  /// existence is always mOutstandingClients + the size of mTextureClients.
  uint32_t mOutstandingClients;

  // On b2g gonk, std::queue might be a better choice.
  // On ICS, fence wait happens implicitly before drawing.
  // Since JB, fence wait happens explicitly when fetching a client from the pool.
  std::stack<RefPtr<TextureClient> > mTextureClients;

  std::list<RefPtr<TextureClient>> mTextureClientsDeferred;
  RefPtr<nsITimer> mTimer;
  // This mSurfaceAllocator owns us, so no need to hold a ref to it
  TextureForwarder* mSurfaceAllocator;

  // Keep track of whether this pool has been destroyed or not. If it has,
  // we won't accept returns of TextureClients anymore, and the refcounting
  // should take care of their destruction.
  bool mDestroyed;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_TEXTURECLIENTPOOL_H */
