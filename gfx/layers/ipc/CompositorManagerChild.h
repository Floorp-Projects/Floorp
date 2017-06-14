/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITORMANAGERCHILD_H
#define MOZILLA_GFX_COMPOSITORMANAGERCHILD_H

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint64_t
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for already_AddRefed
#include "mozilla/StaticPtr.h"          // for StaticRefPtr
#include "mozilla/layers/PCompositorManagerChild.h"

namespace mozilla {
namespace layers {

class CompositorManagerParent;
class LayerManager;

class CompositorManagerChild : public PCompositorManagerChild
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorManagerChild)

public:
  static bool IsInitialized();
  static bool InitSameProcess(uint32_t aNamespace);
  static bool Init(Endpoint<PCompositorManagerChild>&& aEndpoint,
                   uint32_t aNamespace);
  static void Shutdown();

  static bool
  CreateContentCompositorBridge(uint32_t aNamespace);

  static already_AddRefed<CompositorBridgeChild>
  CreateWidgetCompositorBridge(uint64_t aProcessToken,
                               LayerManager* aLayerManager,
                               uint32_t aNamespace,
                               CSSToLayoutDeviceScale aScale,
                               const CompositorOptions& aOptions,
                               bool aUseExternalSurfaceSize,
                               const gfx::IntSize& aSurfaceSize);

  static already_AddRefed<CompositorBridgeChild>
  CreateSameProcessWidgetCompositorBridge(LayerManager* aLayerManager,
                                          uint32_t aNamespace);

  uint32_t GetNextResourceId()
  {
    return ++mResourceId;
  }

  uint32_t GetNamespace() const
  {
    return mNamespace;
  }

  void ActorDestroy(ActorDestroyReason aReason) override;

  void HandleFatalError(const char* aName, const char* aMsg) const override;

  void ProcessingError(Result aCode, const char* aReason) override;

private:
  static StaticRefPtr<CompositorManagerChild> sInstance;

  CompositorManagerChild(CompositorManagerParent* aParent,
                         uint32_t aNamespace);

  CompositorManagerChild(Endpoint<PCompositorManagerChild>&& aEndpoint,
                         uint32_t aNamespace);

  ~CompositorManagerChild() override
  {
  }

  bool CanSend() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mCanSend;
  }

  void DeallocPCompositorManagerChild() override;

  already_AddRefed<nsIEventTarget>
  GetSpecificMessageEventTarget(const Message& aMsg) override;

  bool mCanSend;
  uint32_t mNamespace;
  uint32_t mResourceId;
};

} // namespace layers
} // namespace mozilla

#endif
