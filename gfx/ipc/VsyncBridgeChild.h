/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_gfx_ipc_VsyncBridgeChild_h
#define include_gfx_ipc_VsyncBridgeChild_h

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/PVsyncBridgeChild.h"

namespace mozilla {
namespace gfx {

class VsyncIOThreadHolder;

class VsyncBridgeChild final : public PVsyncBridgeChild
{
  friend class NotifyVsyncTask;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncBridgeChild)

  static RefPtr<VsyncBridgeChild> Create(RefPtr<VsyncIOThreadHolder> aThread,
                                         const uint64_t& aProcessToken,
                                         Endpoint<PVsyncBridgeChild>&& aEndpoint);

  void Close();

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeallocPVsyncBridgeChild() override;
  void ProcessingError(Result aCode, const char* aReason) override;

  void NotifyVsync(TimeStamp aTimeStamp, const layers::LayersId& aLayersId);

  virtual void HandleFatalError(const char* aMsg) const override;

private:
  VsyncBridgeChild(RefPtr<VsyncIOThreadHolder>, const uint64_t& aProcessToken);
  ~VsyncBridgeChild();

  void Open(Endpoint<PVsyncBridgeChild>&& aEndpoint);

  void NotifyVsyncImpl(TimeStamp aTimeStamp, const layers::LayersId& aLayersId);

  bool IsOnVsyncIOThread() const;

private:
  RefPtr<VsyncIOThreadHolder> mThread;
  MessageLoop* mLoop;
  uint64_t mProcessToken;
};

} // namespace gfx
} // namespace mozilla

#endif // include_gfx_ipc_VsyncBridgeChild_h
