/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_gfx_ipc_CanvasManagerParent_h__
#define _include_gfx_ipc_CanvasManagerParent_h__

#include "mozilla/gfx/PCanvasManagerParent.h"
#include "mozilla/StaticMonitor.h"
#include "mozilla/UniquePtr.h"
#include "nsHashtablesFwd.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {
class CanvasTranslator;
class SurfaceDescriptor;
}  // namespace layers

namespace gfx {

class CanvasManagerParent final : public PCanvasManagerParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CanvasManagerParent, override);

  static void Init(Endpoint<PCanvasManagerParent>&& aEndpoint);

  static void Shutdown();

  static void DisableRemoteCanvas();

  static void AddReplayTexture(layers::CanvasTranslator* aOwner,
                               int64_t aTextureId,
                               layers::TextureData* aTextureData);

  static void RemoveReplayTexture(layers::CanvasTranslator* aOwner,
                                  int64_t aTextureId);

  static void RemoveReplayTextures(layers::CanvasTranslator* aOwner);

  static UniquePtr<layers::SurfaceDescriptor> WaitForReplayTexture(
      base::ProcessId aOtherPid, int64_t aTextureId);

  CanvasManagerParent();

  void Bind(Endpoint<PCanvasManagerParent>&& aEndpoint);
  void ActorDestroy(ActorDestroyReason aWhy) override;

  already_AddRefed<dom::PWebGLParent> AllocPWebGLParent();
  already_AddRefed<webgpu::PWebGPUParent> AllocPWebGPUParent();
  already_AddRefed<layers::PCanvasParent> AllocPCanvasParent();

  mozilla::ipc::IPCResult RecvInitialize(const uint32_t& aId);
  mozilla::ipc::IPCResult RecvGetSnapshot(
      const uint32_t& aManagerId, const int32_t& aProtocolId,
      const Maybe<RemoteTextureOwnerId>& aOwnerId,
      webgl::FrontBufferSnapshotIpc* aResult);

 private:
  static UniquePtr<layers::SurfaceDescriptor> TakeReplayTexture(
      base::ProcessId aOtherPid, int64_t aTextureId)
      MOZ_REQUIRES(sReplayTexturesMonitor);

  static void ShutdownInternal();
  static void DisableRemoteCanvasInternal();

  ~CanvasManagerParent() override;

  uint32_t mId = 0;

  using ManagerSet = nsTHashSet<RefPtr<CanvasManagerParent>>;
  static ManagerSet sManagers;

  struct ReplayTexture {
    RefPtr<layers::CanvasTranslator> mOwner;
    int64_t mId;
    UniquePtr<layers::SurfaceDescriptor> mDesc;
  };

  static StaticMonitor sReplayTexturesMonitor;
  static nsTArray<ReplayTexture> sReplayTextures
      MOZ_GUARDED_BY(sReplayTexturesMonitor);
  static bool sReplayTexturesEnabled MOZ_GUARDED_BY(sReplayTexturesMonitor);
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _include_gfx_ipc_CanvasManagerParent_h__
