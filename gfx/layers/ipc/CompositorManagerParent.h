/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITORMANAGERPARENT_H
#define MOZILLA_GFX_COMPOSITORMANAGERPARENT_H

#include <map>
#include <stdint.h>                 // for uint32_t
#include "mozilla/Attributes.h"     // for override
#include "mozilla/StaticPtr.h"      // for StaticRefPtr
#include "mozilla/StaticMonitor.h"  // for StaticMonitor
#include "mozilla/RefPtr.h"         // for already_AddRefed
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/layers/PCompositorManagerParent.h"
#include "nsTArray.h"  // for AutoTArray

namespace mozilla {

namespace gfx {
class SourceSurfaceSharedData;
}

namespace layers {

class CompositorBridgeParent;
class CompositorThreadHolder;
class RemoteTextureTxnScheduler;
class SharedSurfacesHolder;

class CompositorManagerParent final : public PCompositorManagerParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorManagerParent, final)

 public:
  static already_AddRefed<CompositorManagerParent> CreateSameProcess(
      uint32_t aNamespace);
  static bool Create(Endpoint<PCompositorManagerParent>&& aEndpoint,
                     dom::ContentParentId aContentId, uint32_t aNamespace,
                     bool aIsRoot);
  static void Shutdown();

  static already_AddRefed<CompositorBridgeParent>
  CreateSameProcessWidgetCompositorBridge(CSSToLayoutDeviceScale aScale,
                                          const CompositorOptions& aOptions,
                                          bool aUseExternalSurfaceSize,
                                          const gfx::IntSize& aSurfaceSize,
                                          uint64_t aInnerWindowId);

  static void WaitForSharedSurface(const wr::ExternalImageId& aId);

  static void AddSharedSurface(const wr::ExternalImageId& aId,
                               gfx::SourceSurfaceSharedData* aSurface);

  mozilla::ipc::IPCResult RecvAddSharedSurface(const wr::ExternalImageId& aId,
                                               SurfaceDescriptorShared&& aDesc);
  mozilla::ipc::IPCResult RecvRemoveSharedSurface(
      const wr::ExternalImageId& aId);
  mozilla::ipc::IPCResult RecvReportSharedSurfacesMemory(
      ReportSharedSurfacesMemoryResolver&&);

  mozilla::ipc::IPCResult RecvNotifyMemoryPressure();

  mozilla::ipc::IPCResult RecvReportMemory(ReportMemoryResolver&&);

  mozilla::ipc::IPCResult RecvInitCanvasManager(
      Endpoint<PCanvasManagerParent>&&);

  void BindComplete(bool aIsRoot);
  void ActorDestroy(ActorDestroyReason aReason) override;

  already_AddRefed<PCompositorBridgeParent> AllocPCompositorBridgeParent(
      const CompositorBridgeOptions& aOpt);

  static void NotifyWebRenderError(wr::WebRenderError aError);

  const dom::ContentParentId& GetContentId() const { return mContentId; }

  bool OwnsExternalImageId(const wr::ExternalImageId& aId) const {
    return mNamespace == static_cast<uint32_t>(wr::AsUint64(aId) >> 32);
  }

 private:
  static StaticMonitor sMonitor;
  static StaticRefPtr<CompositorManagerParent> sInstance
      MOZ_GUARDED_BY(sMonitor);

  // Indexed by namespace.
  using ManagerMap = std::map<uint32_t, CompositorManagerParent*>;
  static ManagerMap sManagers MOZ_GUARDED_BY(sMonitor);

  static void ShutdownInternal();

  CompositorManagerParent(dom::ContentParentId aContentId, uint32_t aNamespace);
  virtual ~CompositorManagerParent();

  void Bind(Endpoint<PCompositorManagerParent>&& aEndpoint, bool aIsRoot);

  void DeferredDestroy();

  RefPtr<CompositorThreadHolder> mCompositorThreadHolder;
  RefPtr<SharedSurfacesHolder> mSharedSurfacesHolder;
  AutoTArray<RefPtr<CompositorBridgeParent>, 1> mPendingCompositorBridges;
  const dom::ContentParentId mContentId;
  const uint32_t mNamespace;
  uint32_t mLastSharedSurfaceResourceId MOZ_GUARDED_BY(sMonitor) = 0;
  RefPtr<RemoteTextureTxnScheduler> mRemoteTextureTxnScheduler;
};

}  // namespace layers
}  // namespace mozilla

#endif
