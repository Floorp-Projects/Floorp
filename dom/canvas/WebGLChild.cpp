/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLChild.h"

#include "ClientWebGLContext.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "WebGLMethodDispatcher.h"

namespace mozilla::dom {

WebGLChild::WebGLChild(ClientWebGLContext& context)
    : mContext(&context),
      mDefaultCmdsShmemSize(StaticPrefs::webgl_out_of_process_shmem_size()) {}

WebGLChild::~WebGLChild() { (void)Send__delete__(this); }

void WebGLChild::ActorDestroy(ActorDestroyReason why) {
  mPendingCmdsShmem = {};
}

// -

Maybe<Range<uint8_t>> WebGLChild::AllocPendingCmdBytes(
    const size_t size, const size_t fyiAlignmentOverhead) {
  if (!mPendingCmdsShmem.Size()) {
    size_t capacity = mDefaultCmdsShmemSize;
    if (capacity < size) {
      capacity = size;
    }

    mPendingCmdsShmem = mozilla::ipc::BigBuffer::TryAlloc(capacity);
    if (!mPendingCmdsShmem.Size()) {
      NS_WARNING("Failed to alloc shmem for AllocPendingCmdBytes.");
      return {};
    }
    mPendingCmdsPos = 0;
    mPendingCmdsAlignmentOverhead = 0;

    if (kIsDebug) {
      const auto ptr = mPendingCmdsShmem.Data();
      const auto initialOffset = AlignmentOffset(kUniversalAlignment, ptr);
      MOZ_ALWAYS_TRUE(!initialOffset);
    }
  }

  const auto range = Range<uint8_t>{mPendingCmdsShmem.AsSpan()};

  auto itr = range.begin() + mPendingCmdsPos;
  const auto offset = AlignmentOffset(kUniversalAlignment, itr.get());
  mPendingCmdsPos += offset;
  mPendingCmdsAlignmentOverhead += offset;
  const auto required = mPendingCmdsPos + size;
  if (required > range.length()) {
    FlushPendingCmds();
    return AllocPendingCmdBytes(size, fyiAlignmentOverhead);
  }
  itr = range.begin() + mPendingCmdsPos;
  const auto remaining = Range<uint8_t>{itr, range.end()};
  mPendingCmdsPos += size;
  mPendingCmdsAlignmentOverhead += fyiAlignmentOverhead;
  return Some(Range<uint8_t>{remaining.begin(), remaining.begin() + size});
}

void WebGLChild::FlushPendingCmds() {
  if (!mPendingCmdsShmem.Size()) return;

  const auto byteSize = mPendingCmdsPos;
  SendDispatchCommands(std::move(mPendingCmdsShmem), byteSize);
  mPendingCmdsShmem = {};

  mFlushedCmdInfo.flushes += 1;
  mFlushedCmdInfo.flushedCmdBytes += byteSize;
  mFlushedCmdInfo.overhead += mPendingCmdsAlignmentOverhead;

  // Handle flushesSinceLastCongestionCheck
  mFlushedCmdInfo.flushesSinceLastCongestionCheck += 1;
  const auto startCongestionCheck = 20;
  const auto maybeIPCMessageCongestion = 70;
  const auto eventTarget = GetCurrentSerialEventTarget();
  MOZ_ASSERT(eventTarget);
  RefPtr<WebGLChild> self = this;
  size_t generation = self->mFlushedCmdInfo.congestionCheckGeneration;

  // When ClientWebGLContext uses async remote texture, sync GetFrontBuffer
  // message is not sent in ClientWebGLContext::GetFrontBuffer(). It causes a
  // case that a lot of async DispatchCommands messages are sent to
  // WebGLParent without calling ClientWebGLContext::GetFrontBuffer(). The
  // sending DispatchCommands messages could be faster than receiving message
  // at WebGLParent by WebGLParent::RecvDispatchCommands(). If it happens,
  // pending IPC messages could grow too much until out of resource. To detect
  // the messages congestion, async Ping message is used. If the Ping response
  // is not received until maybeIPCMessageCongestion, IPC message might be
  // congested at WebGLParent. Then sending sync SyncPing flushes all pending
  // messages.
  // Due to the async nature of the async ping, it is possible for the flush
  // check to exceed maybeIPCMessageCongestion, but that it it still bounded.
  if (mFlushedCmdInfo.flushesSinceLastCongestionCheck == startCongestionCheck) {
    SendPing()->Then(eventTarget, __func__, [self, generation]() {
      if (generation == self->mFlushedCmdInfo.congestionCheckGeneration) {
        // Confirmed IPC messages congestion does not happen.
        // Reset flushesSinceLastCongestionCheck for next congestion check.
        self->mFlushedCmdInfo.flushesSinceLastCongestionCheck = 0;
        self->mFlushedCmdInfo.congestionCheckGeneration++;
      }
    });
  } else if (mFlushedCmdInfo.flushesSinceLastCongestionCheck >
             maybeIPCMessageCongestion) {
    // IPC messages congestion might happen, send sync SyncPing for flushing
    // pending messages.
    SendSyncPing();
    // Reset flushesSinceLastCongestionCheck for next congestion check.
    mFlushedCmdInfo.flushesSinceLastCongestionCheck = 0;
    mFlushedCmdInfo.congestionCheckGeneration++;
  }

  if (gl::GLContext::ShouldSpew()) {
    const auto overheadRatio = float(mPendingCmdsAlignmentOverhead) /
                               (byteSize - mPendingCmdsAlignmentOverhead);
    const auto totalOverheadRatio =
        float(mFlushedCmdInfo.overhead) /
        (mFlushedCmdInfo.flushedCmdBytes - mFlushedCmdInfo.overhead);
    printf_stderr(
        "[WebGLChild] Flushed %zu (%zu=%.2f%% overhead) bytes."
        " (%zu (%.2f%% overhead) over %zu flushes)\n",
        byteSize, mPendingCmdsAlignmentOverhead, 100 * overheadRatio,
        mFlushedCmdInfo.flushedCmdBytes, 100 * totalOverheadRatio,
        mFlushedCmdInfo.flushes);
  }
}

// -

mozilla::ipc::IPCResult WebGLChild::RecvJsWarning(
    const std::string& text) const {
  if (!mContext) return IPC_OK();
  mContext->JsWarning(text);
  return IPC_OK();
}

mozilla::ipc::IPCResult WebGLChild::RecvOnContextLoss(
    const webgl::ContextLossReason reason) const {
  if (!mContext) return IPC_OK();
  mContext->OnContextLoss(reason);
  return IPC_OK();
}

}  // namespace mozilla::dom
