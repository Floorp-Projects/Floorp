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
  mPendingSyncs.emplace_back(globj);
  EnsurePollPendingSyncs_Pending();
  return globj;
}

GLenum WebGL2Context::ClientWaitSync(WebGLSync& sync, GLbitfield flags,
                                     GLuint64 timeout) {
  const FuncScope funcScope(*this, "clientWaitSync");
  if (IsContextLost()) return LOCAL_GL_WAIT_FAILED;

  if (!ValidateObject("sync", sync)) return LOCAL_GL_WAIT_FAILED;

  if (flags != 0 && flags != LOCAL_GL_SYNC_FLUSH_COMMANDS_BIT) {
    ErrorInvalidValue("`flags` must be SYNC_FLUSH_COMMANDS_BIT or 0.");
    return LOCAL_GL_WAIT_FAILED;
  }

  const auto ret = sync.ClientWaitSync(flags, timeout);
  return UnderlyingValue(ret);
}

void WebGLContext::EnsurePollPendingSyncs_Pending() const {
  if (mPollPendingSyncs_Pending) return;
  mPollPendingSyncs_Pending = NS_NewRunnableFunction(
      "WebGLContext::PollPendingSyncs", [weak = WeakPtr{this}]() {
        if (const auto strong = RefPtr{weak.get()}) {
          strong->mPollPendingSyncs_Pending = nullptr;
          strong->PollPendingSyncs();
          if (strong->mPendingSyncs.size()) {
            // Not done yet...
            strong->EnsurePollPendingSyncs_Pending();
          }
        }
      });
  if (const auto eventTarget = GetCurrentSerialEventTarget()) {
    eventTarget->DelayedDispatch(do_AddRef(mPollPendingSyncs_Pending),
                                 kPollPendingSyncs_DelayMs);
  } else {
    NS_WARNING(
        "[EnsurePollPendingSyncs_Pending] GetCurrentSerialEventTarget() -> "
        "nullptr");
  }
}

void WebGLContext::PollPendingSyncs() const {
  const FuncScope funcScope(*this, "<pollPendingSyncs>");
  if (IsContextLost()) return;

  while (mPendingSyncs.size()) {
    if (const auto sync = RefPtr{mPendingSyncs.front().get()}) {
      const auto res = sync->ClientWaitSync(0, 0);
      switch (res) {
        case ClientWaitSyncResult::WAIT_FAILED:
        case ClientWaitSyncResult::TIMEOUT_EXPIRED:
          return;
        case ClientWaitSyncResult::CONDITION_SATISFIED:
        case ClientWaitSyncResult::ALREADY_SIGNALED:
          // Communication back to child happens in sync->lientWaitSync.
          break;
      }
    }
    mPendingSyncs.pop_front();
  }
}

}  // namespace mozilla
