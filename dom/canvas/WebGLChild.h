/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCHILD_H_
#define WEBGLCHILD_H_

#include "mozilla/dom/PWebGLChild.h"
#include "mozilla/WeakPtr.h"

#include <string>

namespace mozilla {

class ClientWebGLContext;

namespace dom {

struct FlushedCmdInfo final {
  size_t flushes = 0;
  size_t flushedCmdBytes = 0;
};

class WebGLChild final : public PWebGLChild, public SupportsWeakPtr {
  const WeakPtr<ClientWebGLContext> mContext;
  const size_t mDefaultCmdsShmemSize;
  webgl::RaiiShmem mPendingCmdsShmem;
  size_t mPendingCmdsPos = 0;
  FlushedCmdInfo mFlushedCmdInfo;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebGLChild, override);

  explicit WebGLChild(ClientWebGLContext&);

  Maybe<Range<uint8_t>> AllocPendingCmdBytes(size_t);
  void FlushPendingCmds();
  void ActorDestroy(ActorDestroyReason why) override;

 private:
  friend PWebGLChild;
  virtual ~WebGLChild();

 public:
  mozilla::ipc::IPCResult RecvJsWarning(const std::string&) const;
  mozilla::ipc::IPCResult RecvOnContextLoss(webgl::ContextLossReason) const;
};

}  // namespace dom
}  // namespace mozilla

#endif  // WEBGLCHILD_H_
