/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLSync.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLSync::WebGLSync(WebGLContext* webgl, GLenum condition, GLbitfield flags)
    : WebGLContextBoundObject(webgl),
      mGLName(mContext->gl->fFenceSync(condition, flags)),
      mFenceId(mContext->mNextFenceId) {
  mContext->mNextFenceId += 1;
}

WebGLSync::~WebGLSync() {
  if (!mContext) return;
  mContext->gl->fDeleteSync(mGLName);
}

ClientWaitSyncResult WebGLSync::ClientWaitSync(const GLbitfield flags,
                                               const GLuint64 timeout) {
  if (!mContext) return ClientWaitSyncResult::WAIT_FAILED;
  if (IsKnownComplete()) return ClientWaitSyncResult::ALREADY_SIGNALED;

  auto ret = ClientWaitSyncResult::WAIT_FAILED;
  bool newlyComplete = false;
  const auto status = static_cast<ClientWaitSyncResult>(
      mContext->gl->fClientWaitSync(mGLName, 0, 0));
  switch (status) {
    case ClientWaitSyncResult::TIMEOUT_EXPIRED:  // Poll() -> false
    case ClientWaitSyncResult::WAIT_FAILED:      // Error
      ret = status;
      break;
    case ClientWaitSyncResult::CONDITION_SATISFIED:  // Should never happen, but
                                                     // tolerate it.
    case ClientWaitSyncResult::ALREADY_SIGNALED:     // Poll() -> true
      newlyComplete = true;
      ret = status;
      break;
  }

  if (newlyComplete) {
    if (mContext->mCompletedFenceId < mFenceId) {
      mContext->mCompletedFenceId = mFenceId;
    }
    MOZ_RELEASE_ASSERT(mOnCompleteTasks);
    for (const auto& task : *mOnCompleteTasks) {
      (*task)();
    }
    mOnCompleteTasks = {};
  }
  return ret;
}

}  // namespace mozilla
