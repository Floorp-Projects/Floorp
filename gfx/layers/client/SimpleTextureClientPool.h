/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SIMPLETEXTURECLIENTPOOL_H
#define MOZILLA_GFX_SIMPLETEXTURECLIENTPOOL_H

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

class SimpleTextureClientPool
{
  ~SimpleTextureClientPool()
  {
    for (auto it = mOutstandingTextureClients.begin(); it != mOutstandingTextureClients.end(); ++it) {
      (*it)->ClearRecycleCallback();
    }
  }

public:
  NS_INLINE_DECL_REFCOUNTING(SimpleTextureClientPool)

  SimpleTextureClientPool(gfx::SurfaceFormat aFormat, gfx::IntSize aSize,
                          uint32_t aMaxTextureClients,
                          uint32_t aShrinkTimeoutMsec,
                          ISurfaceAllocator *aAllocator);

  /**
   * If a TextureClient is AutoRecycled, when the last reference is
   * released this object will be automatically return to the pool as
   * soon as the compositor informs us it is done with it.
   */
  TemporaryRef<TextureClient> GetTextureClient(bool aAutoRecycle = false);
  TemporaryRef<TextureClient> GetTextureClientWithAutoRecycle() { return GetTextureClient(true); }

  void ReturnTextureClient(TextureClient *aClient);

  void ShrinkToMinimumSize();

  void Clear();

private:
  // The minimum size of the pool (the number of tiles that will be kept after
  // shrinking).
  static const uint32_t sMinCacheSize = 16;

  static void ShrinkCallback(nsITimer *aTimer, void *aClosure);
  static void RecycleCallback(TextureClient* aClient, void* aClosure);
  static void WaitForCompositorRecycleCallback(TextureClient* aClient, void* aClosure);

  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;

  // This is the number of cached texture clients we don't want to exceed, even
  // temporarily (pre-shrink)
  uint32_t mMaxTextureClients;

  // The time in milliseconds before the pool will be shrunk to the minimum
  // size after returning a client.
  uint32_t mShrinkTimeoutMsec;

  // We use a std::stack and make sure to use it the following way:
  //   new (available to be used) elements are push()'d to the front
  //   requests are served from the front via pop()
  //     -- the thinking is that recently-used elements are most likely
  //     to be in any data cache, so we can get some wins there
  //     -- the converse though is that if there is some GPU locking going on
  //     the most recently used elements may also have the most contention;
  //     if we see that, then we should use push_back() to add new elements
  //   when we shrink this list, we use pop(), but should use pop_back() to
  //   nuke the oldest.
  // We may need to switch to a std::deque
  // On b2g gonk, std::queue might be a better choice.
  // On ICS, fence wait happens implicitly before drawing.
  // Since JB, fence wait happens explicitly when fetching a client from the pool.
  std::stack<RefPtr<TextureClient> > mAvailableTextureClients;
  std::list<RefPtr<TextureClient> > mOutstandingTextureClients;

  nsRefPtr<nsITimer> mTimer;
  RefPtr<ISurfaceAllocator> mSurfaceAllocator;
};

}
}
#endif /* MOZILLA_GFX_SIMPLETEXTURECLIENTPOOL_H */
