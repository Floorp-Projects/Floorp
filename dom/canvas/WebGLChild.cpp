/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLChild.h"

#include "ClientWebGLContext.h"
#include "WebGLMethodDispatcher.h"

namespace mozilla {
namespace dom {

WebGLChild::WebGLChild(ClientWebGLContext& context)
    : PcqActor(this), mContext(context) {}

WebGLChild::~WebGLChild() { (void)Send__delete__(this); }

// -

Maybe<Range<uint8_t>> WebGLChild::AllocPendingCmdBytes(const size_t size) {
  if (!mPendingCmds) {
    mPendingCmds.reset(new webgl::ShmemCmdBuffer);
    size_t capacity = 1000 * 1000;
    if (capacity < size) {
      capacity = size;
    }

    if (!PWebGLChild::AllocShmem(
            capacity, mozilla::ipc::SharedMemory::SharedMemoryType::TYPE_BASIC,
            &(mPendingCmds->mShmem))) {
      return {};
    }
  }

  auto remaining = mPendingCmds->Remaining();
  if (size > remaining.length()) {
    FlushPendingCmds();
    return AllocPendingCmdBytes(size);
  }
  mPendingCmds->mPos += size;
  return Some(Range<uint8_t>{remaining.begin(), remaining.begin() + size});
}

void WebGLChild::FlushPendingCmds() {
  if (!mPendingCmds) return;

  const auto cmdBytes = mPendingCmds->mPos;
  SendDispatchCommands(std::move(mPendingCmds->mShmem), cmdBytes);
  mPendingCmds = nullptr;

  mFlushedCmdInfo.flushes += 1;
  mFlushedCmdInfo.flushedCmdBytes += cmdBytes;
}

// -

mozilla::ipc::IPCResult WebGLChild::RecvJsWarning(
    const std::string& text) const {
  mContext.JsWarning(text);
  return IPC_OK();
}

mozilla::ipc::IPCResult WebGLChild::RecvOnContextLoss(
    const webgl::ContextLossReason reason) const {
  mContext.OnContextLoss(reason);
  return IPC_OK();
}

/* static */
IpdlQueueProtocol WebGLChild::GetIpdlQueueProtocol(size_t aCmd, ...) {
  bool isSync =
      WebGLMethodDispatcher<>::SyncType(aCmd) == CommandSyncType::SYNC;
  return isSync ? IpdlQueueProtocol::kSync : IpdlQueueProtocol::kBufferedAsync;
}

}  // namespace dom
}  // namespace mozilla
