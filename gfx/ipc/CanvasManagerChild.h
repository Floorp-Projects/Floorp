/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_gfx_ipc_CanvasManagerChild_h__
#define _include_gfx_ipc_CanvasManagerChild_h__

#include "mozilla/Atomics.h"
#include "mozilla/gfx/PCanvasManagerChild.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/ThreadLocal.h"
#include <set>

namespace mozilla {
namespace dom {
class CanvasRenderingContext2D;
class ThreadSafeWorkerRef;
class WorkerPrivate;
}  // namespace dom

namespace layers {
class CanvasChild;
class ActiveResourceTracker;
}  // namespace layers

namespace webgpu {
class WebGPUChild;
}  // namespace webgpu

namespace gfx {
class DataSourceSurface;

class CanvasManagerChild final : public PCanvasManagerChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CanvasManagerChild, override);

  explicit CanvasManagerChild(uint32_t aId);
  uint32_t Id() const { return mId; }
  already_AddRefed<DataSourceSurface> GetSnapshot(
      uint32_t aManagerId, int32_t aProtocolId,
      const Maybe<RemoteTextureOwnerId>& aOwnerId, SurfaceFormat aFormat,
      bool aPremultiply, bool aYFlip);
  void ActorDestroy(ActorDestroyReason aReason) override;

  static CanvasManagerChild* Get();
  static CanvasManagerChild* MaybeGet();
  static void Shutdown();
  static bool CreateParent(
      mozilla::ipc::Endpoint<PCanvasManagerParent>&& aEndpoint);

  void AddShutdownObserver(dom::CanvasRenderingContext2D* aCanvas);
  void RemoveShutdownObserver(dom::CanvasRenderingContext2D* aCanvas);

  bool IsCanvasActive() { return mActive; }
  void EndCanvasTransaction();
  void ClearCachedResources();
  void DeactivateCanvas();
  void BlockCanvas();

  RefPtr<layers::CanvasChild> GetCanvasChild();

  RefPtr<webgpu::WebGPUChild> GetWebGPUChild();

  layers::ActiveResourceTracker* GetActiveResourceTracker();

 private:
  ~CanvasManagerChild();
  void DestroyInternal();
  void Destroy();

  RefPtr<mozilla::dom::ThreadSafeWorkerRef> mWorkerRef;
  RefPtr<layers::CanvasChild> mCanvasChild;
  RefPtr<webgpu::WebGPUChild> mWebGPUChild;
  UniquePtr<layers::ActiveResourceTracker> mActiveResourceTracker;
  std::set<dom::CanvasRenderingContext2D*> mActiveCanvas;
  const uint32_t mId;
  bool mActive = true;
  bool mBlocked = false;

  static MOZ_THREAD_LOCAL(CanvasManagerChild*) sLocalManager;
  static Atomic<uint32_t> sNextId;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _include_gfx_ipc_CanvasManagerChild_h__
