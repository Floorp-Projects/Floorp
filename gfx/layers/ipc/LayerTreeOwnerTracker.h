/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_LayerTreeOwnerTracker_h
#define mozilla_layers_LayerTreeOwnerTracker_h

#include "base/process.h"  // for base::ProcessId
#include "mozilla/Mutex.h" // for mozilla::Mutex
#include "mozilla/Function.h"

#include <map>

namespace mozilla {

namespace dom {
  class ContentParent;
}

namespace layers {

/**
 * A utility class for tracking which content processes should be allowed
 * to access which layer trees.
 *
 * ProcessId's are used to track which content process can access the layer
 * tree, and in the case of nested browser's we use the top level content
 * processes' ProcessId.
 *
 * This class is only available in the main process and gpu process. Mappings
 * are synced from main process to the gpu process. The actual syncing happens
 * in GPUProcessManager, and so this class should not be used directly.
 */
class LayerTreeOwnerTracker final
{
public:
  static void Initialize();
  static void Shutdown();
  static LayerTreeOwnerTracker* Get();

  /**
   * Map aLayersId and aProcessId together so that that process
   * can access that layer tree.
   */
  void Map(uint64_t aLayersId, base::ProcessId aProcessId);

  /**
  * Remove an existing mapping.
  */
  void Unmap(uint64_t aLayersId, base::ProcessId aProcessId);

  /**
   * Checks whether it is okay for aProcessId to access aLayersId.
   */
  bool IsMapped(uint64_t aLayersId, base::ProcessId aProcessId);

  void Iterate(function<void(uint64_t aLayersId, base::ProcessId aProcessId)> aCallback);

private:
  LayerTreeOwnerTracker();

  mozilla::Mutex mLayerIdsLock;
  std::map<uint64_t, base::ProcessId> mLayerIds;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_LayerTreeOwnerTracker_h
