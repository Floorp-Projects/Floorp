/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
  static bool IsInitialized(uint64_t aProcessToken);
  static void InitSameProcess(uint32_t aNamespace, uint64_t aProcessToken);
  static bool Init(Endpoint<PCompositorManagerChild>&& aEndpoint,
                   uint32_t aNamespace,
                   uint64_t aProcessToken = 0);
  static void Shutdown();
  static void OnGPUProcessLost(uint64_t aProcessToken);

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

  static CompositorManagerChild* GetInstance()
  {
    MOZ_ASSERT(NS_IsMainThread());
    return sInstance;
  }

  bool CanSend() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mCanSend;
  }

  uint32_t GetNextResourceId()
  {
    MOZ_ASSERT(NS_IsMainThread());
    return ++mResourceId;
  }

  uint32_t GetNamespace() const
  {
    return mNamespace;
  }

  bool OwnsExternalImageId(const wr::ExternalImageId& aId) const
  {
    return mNamespace == static_cast<uint32_t>(wr::AsUint64(aId) >> 32);
  }

  wr::ExternalImageId GetNextExternalImageId()
  {
    uint64_t id = GetNextResourceId();
    MOZ_RELEASE_ASSERT(id != 0);
    id |= (static_cast<uint64_t>(mNamespace) << 32);
    return wr::ToExternalImageId(id);
  }

  void ActorDestroy(ActorDestroyReason aReason) override;

  void HandleFatalError(const char* aMsg) const override;

  void ProcessingError(Result aCode, const char* aReason) override;

  PCompositorBridgeChild* AllocPCompositorBridgeChild(const CompositorBridgeOptions& aOptions) override;

  bool DeallocPCompositorBridgeChild(PCompositorBridgeChild* aActor) override;

  bool ShouldContinueFromReplyTimeout() override;

private:
  static StaticRefPtr<CompositorManagerChild> sInstance;

  CompositorManagerChild(CompositorManagerParent* aParent,
                         uint64_t aProcessToken,
                         uint32_t aNamespace);

  CompositorManagerChild(Endpoint<PCompositorManagerChild>&& aEndpoint,
                         uint64_t aProcessToken,
                         uint32_t aNamespace);

  ~CompositorManagerChild() override
  {
  }

  void DeallocPCompositorManagerChild() override;

  already_AddRefed<nsIEventTarget>
  GetSpecificMessageEventTarget(const Message& aMsg) override;

  void SetReplyTimeout();

  uint64_t mProcessToken;
  uint32_t mNamespace;
  uint32_t mResourceId;
  bool mCanSend;
};

} // namespace layers
} // namespace mozilla

#endif
