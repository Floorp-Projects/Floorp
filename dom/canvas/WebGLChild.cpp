/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLChild.h"

#include "ClientWebGLContext.h"
#include "WebGLMethodDispatcher.h"

namespace mozilla {
namespace dom {

WebGLChild::WebGLChild(ClientWebGLContext& context) : mContext(context) {}

WebGLChild::~WebGLChild() { (void)Send__delete__(this); }

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
bool WebGLChild::ShouldSendSync(size_t aCmd, ...) {
  return WebGLMethodDispatcher<>::SyncType(aCmd) == CommandSyncType::SYNC;
}

}  // namespace dom
}  // namespace mozilla
