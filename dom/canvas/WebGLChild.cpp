/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLChild.h"

#include "ClientWebGLContext.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsQueryObject.h"

namespace mozilla {

namespace dom {

mozilla::ipc::IPCResult WebGLChild::RecvQueueFailed() {
  mozilla::ClientWebGLContext* context = GetContext();
  if (context) {
    context->OnQueueFailed();
  }
  return Send__delete__(this) ? IPC_OK() : IPC_FAIL_NO_REASON(this);
}

mozilla::ClientWebGLContext* WebGLChild::GetContext() {
  nsCOMPtr<nsICanvasRenderingContextInternal> ret = do_QueryReferent(mContext);
  if (!ret) {
    return nullptr;
  }
  return static_cast<mozilla::ClientWebGLContext*>(ret.get());
}

void WebGLChild::SetContext(mozilla::ClientWebGLContext* aContext) {
  mContext = do_GetWeakReference(aContext);
}

}  // namespace dom
}  // namespace mozilla
