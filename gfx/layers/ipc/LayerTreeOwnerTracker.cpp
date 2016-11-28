/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerTreeOwnerTracker.h"

#include "mozilla/StaticPtr.h"              // for StaticAutoPtr
#include "mozilla/dom/ContentParent.h"      // for ContentParent
#include "mozilla/gfx/GPUChild.h"           // for GPUChild
#include "mozilla/gfx/GPUProcessManager.h"  // for GPUProcessManager

#include <functional>
#include <utility> // for std::make_pair

namespace mozilla {
namespace layers {

static StaticAutoPtr<LayerTreeOwnerTracker> sSingleton;

LayerTreeOwnerTracker::LayerTreeOwnerTracker() :
  mLayerIdsLock("LayerTreeOwnerTrackerLock")
{
}

void
LayerTreeOwnerTracker::Initialize()
{
  MOZ_ASSERT(!sSingleton);
  sSingleton = new LayerTreeOwnerTracker();
}

void
LayerTreeOwnerTracker::Shutdown()
{
  sSingleton = nullptr;
}

LayerTreeOwnerTracker*
LayerTreeOwnerTracker::Get()
{
  return sSingleton;
}

void
LayerTreeOwnerTracker::Map(uint64_t aLayersId, base::ProcessId aProcessId)
{
  MutexAutoLock lock(mLayerIdsLock);

  // Add the mapping to the list
  mLayerIds[aLayersId] = aProcessId;
}

void
LayerTreeOwnerTracker::Unmap(uint64_t aLayersId, base::ProcessId aProcessId)
{
  MutexAutoLock lock(mLayerIdsLock);

  MOZ_ASSERT(mLayerIds[aLayersId] == aProcessId);
  mLayerIds.erase(aLayersId);
}

bool
LayerTreeOwnerTracker::IsMapped(uint64_t aLayersId, base::ProcessId aProcessId)
{
  MutexAutoLock lock(mLayerIdsLock);

  auto iter = mLayerIds.find(aLayersId);
  return iter != mLayerIds.end() && iter->second == aProcessId;
}

void
LayerTreeOwnerTracker::Iterate(std::function<void(uint64_t aLayersId, base::ProcessId aProcessId)> aCallback)
{
  MutexAutoLock lock(mLayerIdsLock);

  for (const auto& iter : mLayerIds) {
    aCallback(iter.first, iter.second);
  }
}

} // namespace layers
} // namespace mozilla
