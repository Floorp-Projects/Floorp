/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_gfx_ipc_CanvasManagerParent_h__
#define _include_gfx_ipc_CanvasManagerParent_h__

#include "mozilla/gfx/PCanvasManagerParent.h"
#include "nsHashtablesFwd.h"

namespace mozilla::gfx {

class CanvasManagerParent final : public PCanvasManagerParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CanvasManagerParent, override);

  static void Init(Endpoint<PCanvasManagerParent>&& aEndpoint);

  static void Shutdown();

  CanvasManagerParent();

  void Bind(Endpoint<PCanvasManagerParent>&& aEndpoint);
  void ActorDestroy(ActorDestroyReason aWhy) override;

  already_AddRefed<PWebGLParent> AllocPWebGLParent();
  already_AddRefed<PWebGPUParent> AllocPWebGPUParent();

  mozilla::ipc::IPCResult RecvInitialize(const uint32_t& aId);
  mozilla::ipc::IPCResult RecvGetSnapshot(
      const uint32_t& aManagerId, const int32_t& aProtocolId,
      const Maybe<RemoteTextureOwnerId>& aOwnerId,
      webgl::FrontBufferSnapshotIpc* aResult);

 private:
  static void ShutdownInternal();

  ~CanvasManagerParent() override;

  uint32_t mId = 0;

  using ManagerSet = nsTHashSet<CanvasManagerParent*>;
  static ManagerSet sManagers;
};

}  // namespace mozilla::gfx

#endif  // _include_gfx_ipc_CanvasManagerParent_h__
