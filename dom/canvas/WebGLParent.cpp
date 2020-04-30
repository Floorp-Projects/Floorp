/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLParent.h"

#include "WebGLChild.h"
#include "mozilla/dom/WebGLCrossProcessCommandQueue.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "HostWebGLContext.h"

namespace mozilla {

namespace dom {

mozilla::ipc::IPCResult WebGLParent::RecvInitialize(
    const webgl::InitContextDesc& desc,
    UniquePtr<HostWebGLCommandSinkP>&& aSinkP,
    UniquePtr<HostWebGLCommandSinkI>&& aSinkI,
    webgl::InitContextResult* const out) {
  auto remotingData = Some(HostWebGLContext::RemotingData{
      *this, {},  // std::move(commandSink),
  });

  mHost = HostWebGLContext::Create(
      {
          {},
          std::move(remotingData),
      },
      desc, out);

  if (!mHost) {
    return IPC_FAIL(this, "Failed to create HostWebGLContext");
  }

  if (!BeginCommandQueueDrain()) {
    return IPC_FAIL(this, "Failed to start WebGL command queue drain");
  }

  return IPC_OK();
}

WebGLParent::WebGLParent() = default;
WebGLParent::~WebGLParent() = default;

bool WebGLParent::BeginCommandQueueDrain() {
  if (mRunCommandsRunnable) {
    // already running
    return true;
  }

  WeakPtr<WebGLParent> weakThis = this;
  mRunCommandsRunnable = NS_NewRunnableFunction(
      "RunWebGLCommands", [weakThis]() { MaybeRunCommandQueue(weakThis); });
  if (!mRunCommandsRunnable) {
    MOZ_ASSERT_UNREACHABLE("Failed to create RunWebGLCommands Runnable");
    return false;
  }

  // Start the recurring runnable.
  return RunCommandQueue();
}

/* static */ bool WebGLParent::MaybeRunCommandQueue(
    const WeakPtr<WebGLParent>& weakWebGLParent) {
  // We don't have to worry about WebGLParent being deleted from under us
  // as its not thread-safe so we must be the only thread using it.  In fact,
  // WeakRef cannot atomically promote to a RefPtr so it is not thread safe
  // either.
  if (weakWebGLParent) {
    // This will re-issue the task if the queue is still running.
    return weakWebGLParent->RunCommandQueue();
  }

  // Context was deleted.  Do not re-issue the task.
  return true;
}

bool WebGLParent::RunCommandQueue() {
  if (!mRunCommandsRunnable) {
    // The actor finished.  Do not re-issue the task.
    return true;
  }

  MOZ_CRASH("todo");
  /*
  // Drain the queue for up to kMaxWebGLCommandTimeSliceMs, then
  // repeat no sooner than kDrainDelayMs later.
  // TODO: Tune these.
  static const uint32_t kMaxWebGLCommandTimeSliceMs = 1;
  static const uint32_t kDrainDelayMs = 0;

  TimeDuration timeSlice =
      TimeDuration::FromMilliseconds(kMaxWebGLCommandTimeSliceMs);
  CommandResult result = mHost->RunCommandsForDuration(timeSlice);
  bool success = (result == CommandResult::Success) ||
                 (result == CommandResult::QueueEmpty);
  if (!success) {
    // Tell client that this WebGLParent needs to be shut down
    WEBGL_BRIDGE_LOGE("WebGLParent failed while running commands");
    (void)SendOnContextLoss(webgl::ContextLossReason::None);
    mRunCommandsRunnable = nullptr;
    return false;
  }

  // Re-issue the task
  MOZ_ASSERT(mRunCommandsRunnable);
  MOZ_ASSERT(MessageLoop::current());
  MessageLoop::current()->PostDelayedTask(do_AddRef(mRunCommandsRunnable),
                                          kDrainDelayMs);
  */
  return true;
}

mozilla::ipc::IPCResult WebGLParent::Recv__delete__() {
  mHost = nullptr;
  return IPC_OK();
}

void WebGLParent::ActorDestroy(ActorDestroyReason aWhy) { mHost = nullptr; }

mozilla::ipc::IPCResult WebGLParent::RecvUpdateCompositableHandle(
    layers::PLayerTransactionParent* aLayerTransaction,
    const CompositableHandle& aHandle) {
  auto layerTrans =
      static_cast<layers::LayerTransactionParent*>(aLayerTransaction);
  RefPtr<layers::CompositableHost> compositableHost(
      layerTrans->FindCompositable(aHandle));
  if (!compositableHost) {
    return IPC_FAIL(this, "Failed to find CompositableHost for WebGL instance");
  }

  mHost->SetCompositableHost(compositableHost);
  return IPC_OK();
}

RefPtr<layers::SharedSurfaceTextureClient> WebGLParent::GetVRFrame(
    webgl::ObjectId id) {
  if (!mHost) {
    return nullptr;
  }

  return mHost->GetVRFrame(id);
}

}  // namespace dom

}  // namespace mozilla
