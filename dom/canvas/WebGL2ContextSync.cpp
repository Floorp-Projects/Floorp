/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLSync.h"
#include "mozilla/StaticPrefs_webgl.h"

namespace mozilla {

// -------------------------------------------------------------------------
// Sync objects

RefPtr<WebGLSync> WebGL2Context::FenceSync(GLenum condition, GLbitfield flags) {
  const FuncScope funcScope(*this, "fenceSync");
  if (IsContextLost()) return nullptr;

  if (condition != LOCAL_GL_SYNC_GPU_COMMANDS_COMPLETE) {
    ErrorInvalidEnum("condition must be SYNC_GPU_COMMANDS_COMPLETE");
    return nullptr;
  }

  if (flags != 0) {
    ErrorInvalidValue("flags must be 0");
    return nullptr;
  }

  RefPtr<WebGLSync> globj = new WebGLSync(this, condition, flags);

  const auto& availRunnable = EnsureAvailabilityRunnable();
  availRunnable->mSyncs.push_back(globj);

  return globj;
}

GLenum WebGL2Context::ClientWaitSync(const WebGLSync& sync, GLbitfield flags,
                                     GLuint64 timeout) {
  const FuncScope funcScope(*this, "clientWaitSync");
  if (IsContextLost()) return LOCAL_GL_WAIT_FAILED;

  if (!ValidateObject("sync", sync)) return LOCAL_GL_WAIT_FAILED;

  if (flags != 0 && flags != LOCAL_GL_SYNC_FLUSH_COMMANDS_BIT) {
    ErrorInvalidValue("`flags` must be SYNC_FLUSH_COMMANDS_BIT or 0.");
    return LOCAL_GL_WAIT_FAILED;
  }

  if (timeout > kMaxClientWaitSyncTimeoutNS) {
    ErrorInvalidOperation("`timeout` must not exceed %s nanoseconds.",
                          "MAX_CLIENT_WAIT_TIMEOUT_WEBGL");
    return LOCAL_GL_WAIT_FAILED;
  }

  const bool canBeAvailable =
      (sync.mCanBeAvailable || StaticPrefs::webgl_allow_immediate_queries());
  if (!canBeAvailable) {
    if (timeout) {
      GenerateWarning(
          "Sync object not yet queryable. Please wait for the event"
          " loop.");
    }
    return LOCAL_GL_WAIT_FAILED;
  }

  const auto ret = gl->fClientWaitSync(sync.mGLName, flags, timeout);

  if (ret == LOCAL_GL_CONDITION_SATISFIED || ret == LOCAL_GL_ALREADY_SIGNALED) {
    sync.MarkSignaled();
  }

  return ret;
}

}  // namespace mozilla
