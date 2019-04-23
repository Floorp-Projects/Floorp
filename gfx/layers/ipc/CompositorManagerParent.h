/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITORMANAGERPARENT_H
#define MOZILLA_GFX_COMPOSITORMANAGERPARENT_H

#include <stdint.h>               // for uint32_t
#include "mozilla/Attributes.h"   // for override
#include "mozilla/StaticPtr.h"    // for StaticRefPtr
#include "mozilla/StaticMutex.h"  // for StaticMutex
#include "mozilla/RefPtr.h"       // for already_AddRefed
#include "mozilla/layers/PCompositorManagerParent.h"
#include "nsTArray.h"  // for AutoTArray

namespace mozilla {
namespace layers {

class CompositorBridgeParent;
class CompositorThreadHolder;

#ifndef DEBUG
#  define COMPOSITOR_MANAGER_PARENT_EXPLICIT_SHUTDOWN
#endif

class CompositorManagerParent final : public PCompositorManagerParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorManagerParent)

 public:
  static already_AddRefed<CompositorManagerParent> CreateSameProcess();
  static bool Create(Endpoint<PCompositorManagerParent>&& aEndpoint,
                     bool aIsRoot);
  static void Shutdown();

  static already_AddRefed<CompositorBridgeParent>
  CreateSameProcessWidgetCompositorBridge(CSSToLayoutDeviceScale aScale,
                                          const CompositorOptions& aOptions,
                                          bool aUseExternalSurfaceSize,
                                          const gfx::IntSize& aSurfaceSize);

  mozilla::ipc::IPCResult RecvAddSharedSurface(
      const wr::ExternalImageId& aId, const SurfaceDescriptorShared& aDesc);
  mozilla::ipc::IPCResult RecvRemoveSharedSurface(
      const wr::ExternalImageId& aId);
  mozilla::ipc::IPCResult RecvReportSharedSurfacesMemory(
      ReportSharedSurfacesMemoryResolver&&);

  mozilla::ipc::IPCResult RecvNotifyMemoryPressure();

  mozilla::ipc::IPCResult RecvReportMemory(ReportMemoryResolver&&);

  void BindComplete(bool aIsRoot);
  void ActorDestroy(ActorDestroyReason aReason) override;

  bool DeallocPCompositorBridgeParent(PCompositorBridgeParent* aActor);
  PCompositorBridgeParent* AllocPCompositorBridgeParent(
      const CompositorBridgeOptions& aOpt);

  static void NotifyWebRenderError(wr::WebRenderError aError);

 private:
  static StaticRefPtr<CompositorManagerParent> sInstance;
  static StaticMutex sMutex;

#ifdef COMPOSITOR_MANAGER_PARENT_EXPLICIT_SHUTDOWN
  static StaticAutoPtr<nsTArray<CompositorManagerParent*>> sActiveActors;
  static void ShutdownInternal();
#endif

  CompositorManagerParent();
  virtual ~CompositorManagerParent();

  void Bind(Endpoint<PCompositorManagerParent>&& aEndpoint, bool aIsRoot);

  void DeallocPCompositorManagerParent() override;

  void DeferredDestroy();

  RefPtr<CompositorThreadHolder> mCompositorThreadHolder;

  AutoTArray<RefPtr<CompositorBridgeParent>, 1> mPendingCompositorBridges;
};

}  // namespace layers
}  // namespace mozilla

#endif
