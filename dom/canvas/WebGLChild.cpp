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

Maybe<Range<uint8_t>> WebGLChild::AllocPendingCmdBytes(const size_t size) {
  if (!mPendingCmdsShmem) {
    size_t capacity = mDefaultCmdsShmemSize;
    if (capacity < size) {
      capacity = size;
    }

    auto shmem = webgl::RaiiShmem::Alloc(this, capacity);
    if (!shmem) {
      NS_WARNING("Failed to alloc shmem for AllocPendingCmdBytes.");
      return {};
    }
    mPendingCmdsShmem = std::move(shmem);
    mPendingCmdsPos = 0;

    if (kIsDebug) {
      const auto range = mPendingCmdsShmem.ByteRange();
      const auto initialOffset =
          AlignmentOffset(kUniversalAlignment, range.begin().get());
      MOZ_ALWAYS_TRUE(!initialOffset);
    }
  }
  const auto range = mPendingCmdsShmem.ByteRange();

  auto itr = range.begin() + mPendingCmdsPos;
  const auto offset = AlignmentOffset(kUniversalAlignment, itr.get());
  mPendingCmdsPos += offset;
  const auto required = mPendingCmdsPos + size;
  if (required > range.length()) {
    FlushPendingCmds();
    return AllocPendingCmdBytes(size);
  }
  itr = range.begin() + mPendingCmdsPos;
  const auto remaining = Range<uint8_t>{itr, range.end()};
  mPendingCmdsPos += size;
  return Some(Range<uint8_t>{remaining.begin(), remaining.begin() + size});
}

void WebGLChild::FlushPendingCmds() {
  if (!mPendingCmdsShmem) return;

  const auto byteSize = mPendingCmdsPos;
  SendDispatchCommands(mPendingCmdsShmem.Extract(), byteSize);

  mFlushedCmdInfo.flushes += 1;
  mFlushedCmdInfo.flushedCmdBytes += byteSize;

  if (gl::GLContext::ShouldSpew()) {
    printf_stderr("[WebGLChild] Flushed %zu bytes. (%zu over %zu flushes)\n",
                  byteSize, mFlushedCmdInfo.flushedCmdBytes,
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
