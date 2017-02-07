/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_gfx_ipc_UiCompositorControllerChild_h
#define include_gfx_ipc_UiCompositorControllerChild_h

#include "mozilla/layers/PUiCompositorControllerChild.h"
#include "mozilla/RefPtr.h"
#include <nsThread.h>

namespace mozilla {
namespace layers {

class UiCompositorControllerChild final : public PUiCompositorControllerChild
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UiCompositorControllerChild)

  static bool IsInitialized();
  static void Shutdown();
  static UiCompositorControllerChild* Get();
  static void InitSameProcess(RefPtr<nsThread> aThread);
  static void InitWithGPUProcess(RefPtr<nsThread> aThread,
                                 const uint64_t& aProcessToken,
                                 Endpoint<PUiCompositorControllerChild>&& aEndpoint);

  static void CacheSurfaceResize(int64_t aId, int32_t aWidth, int32_t aHeight);
  void Close();

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeallocPUiCompositorControllerChild() override;
  void ProcessingError(Result aCode, const char* aReason) override;

  virtual void HandleFatalError(const char* aName, const char* aMsg) const override;

  bool IsOnUiThread() const;
private:
  UiCompositorControllerChild(RefPtr<nsThread> aThread, const uint64_t& aProcessToken);
  ~UiCompositorControllerChild();
  void OpenForSameProcess();
  void OpenForGPUProcess(Endpoint<PUiCompositorControllerChild>&& aEndpoint);

  RefPtr<nsThread> mUiThread;
  uint64_t mProcessToken;
};

} // namespace layers
} // namespace mozilla

#endif // include_gfx_ipc_UiCompositorControllerChild_h
