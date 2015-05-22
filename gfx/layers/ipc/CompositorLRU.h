/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CompositorLRU_h
#define mozilla_CompositorLRU_h

#include "mozilla/StaticPtr.h"

#include "nsISupportsImpl.h"
#include "nsTArray.h"

#include <utility>

namespace mozilla {
namespace layers {

class PCompositorParent;

class CompositorLRU final
{
  typedef std::pair<PCompositorParent*, uint64_t> CompositorLayerPair;
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorLRU)

  static void Init();
  static CompositorLRU* GetSingleton();

  /**
   * Adds the (PCompositorParent, LayerId) pair to the LRU pool. If
   * the pool size grows over mLRUSize, the oldest PCompositorParent
   * is evicted.
   */
  void Add(PCompositorParent* aCompositor, const uint64_t& id);

  /**
   * Remove the (PCompositorParent, LayersId) pair from the LRU pool.
   */
  void Remove(PCompositorParent* aCompositor, const uint64_t& id);

  /**
   * Remove all PCompositorParents from the LRU pool.
   */
  void Remove(PCompositorParent* aCompositor);

private:
  static StaticRefPtr<CompositorLRU> sSingleton;

  CompositorLRU();
  ~CompositorLRU();
  uint32_t mLRUSize;
  nsTArray<CompositorLayerPair> mLRU;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_CompositorLRU_h
