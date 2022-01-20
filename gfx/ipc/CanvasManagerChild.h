/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_gfx_ipc_CanvasManagerChild_h__
#define _include_gfx_ipc_CanvasManagerChild_h__

#include "mozilla/Atomics.h"
#include "mozilla/gfx/PCanvasManagerChild.h"
#include "mozilla/ThreadLocal.h"

namespace mozilla {
namespace dom {
class IPCWorkerRef;
class WorkerPrivate;
}  // namespace dom

namespace gfx {
class DataSourceSurface;

class CanvasManagerChild final : public PCanvasManagerChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CanvasManagerChild, override);

  explicit CanvasManagerChild(uint32_t aId);
  uint32_t Id() const { return mId; }
  already_AddRefed<DataSourceSurface> GetSnapshot(uint32_t aManagerId,
                                                  int32_t aProtocolId,
                                                  bool aHasAlpha);
  void ActorDestroy(ActorDestroyReason aReason) override;

  static CanvasManagerChild* Get();
  static void Shutdown();
  static bool CreateParent(
      mozilla::ipc::Endpoint<PCanvasManagerParent>&& aEndpoint);

 private:
  ~CanvasManagerChild();
  void Destroy();

  RefPtr<mozilla::dom::IPCWorkerRef> mWorkerRef;
  const uint32_t mId;

  static MOZ_THREAD_LOCAL(CanvasManagerChild*) sLocalManager;
  static Atomic<uint32_t> sNextId;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _include_gfx_ipc_CanvasManagerChild_h__
