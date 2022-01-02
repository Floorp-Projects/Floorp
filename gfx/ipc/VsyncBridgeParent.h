/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_gfx_ipc_VsyncBridgeParent_h
#define include_gfx_ipc_VsyncBridgeParent_h

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/PVsyncBridgeParent.h"

namespace mozilla {
namespace layers {
class CompositorThreadHolder;
}  // namespace layers

namespace gfx {

class VsyncBridgeParent final : public PVsyncBridgeParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncBridgeParent)

  static RefPtr<VsyncBridgeParent> Start(
      Endpoint<PVsyncBridgeParent>&& aEndpoint);

  mozilla::ipc::IPCResult RecvNotifyVsync(const VsyncEvent& aVsync,
                                          const LayersId& aLayersId);
  void ActorDestroy(ActorDestroyReason aWhy) override;
  void ActorDealloc() override;

  void Shutdown();

 private:
  VsyncBridgeParent();
  ~VsyncBridgeParent();

  void Open(Endpoint<PVsyncBridgeParent>&& aEndpoint);
  void ShutdownImpl();

 private:
  bool mOpen;
  RefPtr<layers::CompositorThreadHolder> mCompositorThreadRef;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // include_gfx_ipc_VsyncBridgeParent_h
