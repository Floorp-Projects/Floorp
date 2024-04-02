/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_SYNC_H_
#define WEBGL_SYNC_H_

#include "WebGLObjectModel.h"

namespace mozilla {
namespace webgl {
class AvailabilityRunnable;

struct Task {
  virtual ~Task() = default;
  virtual void operator()() const = 0;
};

template <class F>
struct FnTask : public Task {
  const F fn;

  explicit FnTask(F&& fn) : fn(std::move(fn)) {}

  virtual void operator()() const override { fn(); }
};
}  // namespace webgl

enum class ClientWaitSyncResult : GLenum {
  WAIT_FAILED = LOCAL_GL_WAIT_FAILED,
  TIMEOUT_EXPIRED = LOCAL_GL_TIMEOUT_EXPIRED,
  CONDITION_SATISFIED = LOCAL_GL_CONDITION_SATISFIED,
  ALREADY_SIGNALED = LOCAL_GL_ALREADY_SIGNALED,
};

class WebGLSync final : public WebGLContextBoundObject, public SupportsWeakPtr {
  friend class WebGL2Context;
  friend class webgl::AvailabilityRunnable;

  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(WebGLSync, override)

  const GLsync mGLName;
  const uint64_t mFenceId;
  bool mCanBeAvailable = false;

  std::optional<std::vector<std::unique_ptr<webgl::Task>>> mOnCompleteTasks =
      std::vector<std::unique_ptr<webgl::Task>>{};

 public:
  WebGLSync(WebGLContext* webgl, GLenum condition, GLbitfield flags);

  ClientWaitSyncResult ClientWaitSync(GLbitfield flags, GLuint64 timeout);

  template <class F>
  void OnCompleteTaskAdd(F&& fn) {
    MOZ_RELEASE_ASSERT(mOnCompleteTasks);
    auto task = std::make_unique<webgl::FnTask<F>>(std::move(fn));
    mOnCompleteTasks->push_back(std::move(task));
  }

  bool IsKnownComplete() const { return !mOnCompleteTasks; }

 private:
  ~WebGLSync() override;
};

}  // namespace mozilla

#endif  // WEBGL_SYNC_H_
