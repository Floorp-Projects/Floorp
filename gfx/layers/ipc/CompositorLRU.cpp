/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorLRU.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Preferences.h"
#include "mozilla/unused.h"

#include "CompositorParent.h"

namespace mozilla {
namespace layers {

mozilla::StaticRefPtr<CompositorLRU> CompositorLRU::sSingleton;

void
CompositorLRU::Init()
{
  unused << GetSingleton();
}

CompositorLRU*
CompositorLRU::GetSingleton()
{
  if (sSingleton) {
    return sSingleton;
  }
  sSingleton = new CompositorLRU();
  ClearOnShutdown(&sSingleton);

  return sSingleton;
}

CompositorLRU::CompositorLRU()
{
  mLRUSize = Preferences::GetUint("layers.compositor-lru-size", uint32_t(0));
}

CompositorLRU::~CompositorLRU()
{
}

void
CompositorLRU::Add(PCompositorParent* aCompositor, const uint64_t& aId)
{
  auto index = mLRU.IndexOf(std::make_pair(aCompositor, aId));
  if (index != nsTArray<CompositorLayerPair>::NoIndex) {
    return;
  }

  if (mLRUSize == 0) {
    unused << aCompositor->SendClearCachedResources(aId);
    return;
  }

  if (mLRU.Length() == mLRUSize) {
    CompositorLayerPair victim = mLRU.LastElement();
    unused << victim.first->SendClearCachedResources(victim.second);
    mLRU.RemoveElement(victim);
  }
  mLRU.InsertElementAt(0, std::make_pair(aCompositor, aId));
}

void
CompositorLRU::Remove(PCompositorParent* aCompositor, const uint64_t& aId)
{
  if (mLRUSize == 0) {
    return;
  }

  auto index = mLRU.IndexOf(std::make_pair(aCompositor, aId));

  if (index == nsTArray<PCompositorParent*>::NoIndex) {
    return;
  }

  mLRU.RemoveElementAt(index);
}

void
CompositorLRU::Remove(PCompositorParent* aCompositor)
{
  if (mLRUSize == 0) {
    return;
  }

  for (int32_t i = mLRU.Length() - 1; i >= 0; --i) {
    if (mLRU[i].first == aCompositor) {
      mLRU.RemoveElementAt(i);
    }
  }
}

} // namespace layers
} // namespace mozilla
