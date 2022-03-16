/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/WebRenderImageHost.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"
#include <map>
#include <utility>

namespace mozilla::layers {

/**
 * CompostaibleInProcessManager is responsible for tracking textures that the
 * content process knows nothing about, beyond the CompositableHandle itself
 * for the purpose of binding to an AsyncImagePipeline in the display list.
 *
 * Hence the compositor process is responsible for creating and updating the
 * WebRenderImageHost object. This will allow frames to be composited without
 * interacting with the content process.
 */
class CompositableInProcessManager final {
 public:
  static void Initialize(uint32_t aNamespace);
  static void Shutdown();

  static RefPtr<WebRenderImageHost> Add(const CompositableHandle& aHandle,
                                        base::ProcessId aForPid,
                                        const TextureInfo& aTextureInfo);
  static RefPtr<WebRenderImageHost> Find(const CompositableHandle& aHandle,
                                         base::ProcessId aForPid);
  static void Release(const CompositableHandle& aHandle,
                      base::ProcessId aForPid);

  static CompositableHandle GetNextHandle() {
    return CompositableHandle(sNextHandle++);
  }

  static uint32_t GetNextResourceId() {
    uint32_t resourceId = sNextResourceId++;
    MOZ_RELEASE_ASSERT(resourceId != 0);
    return resourceId;
  }

  static wr::ExternalImageId GetNextExternalImageId() {
    return wr::ToExternalImageId(GetShiftedNamespace() | GetNextResourceId());
  }

 private:
  static uint64_t GetShiftedNamespace() {
    MOZ_ASSERT(sNamespace != 0);
    return static_cast<uint64_t>(sNamespace) << 32;
  }

  static std::map<std::pair<base::ProcessId, uint64_t>,
                  RefPtr<WebRenderImageHost>>
      sCompositables;
  static StaticMutex sMutex MOZ_UNANNOTATED;

  static uint32_t sNamespace;
  static Atomic<uint32_t> sNextResourceId;
  static Atomic<uint64_t> sNextHandle;
};

}  // namespace mozilla::layers
