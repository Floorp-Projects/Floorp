/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_QUERY_H_
#define WEBGL_QUERY_H_

#include "WebGLObjectModel.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace webgl {
class AvailabilityRunnable;
}  // namespace webgl

class WebGLQuery final : public WebGLContextBoundObject {
  friend class webgl::AvailabilityRunnable;

  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(WebGLQuery, override)

 public:
  const GLuint mGLName;

 private:
  GLenum mTarget;
  RefPtr<WebGLQuery>* mActiveSlot;

  bool mCanBeAvailable = false;  // Track whether the event loop has spun

  ////
 public:
  GLenum Target() const { return mTarget; }
  bool IsActive() const { return bool(mActiveSlot); }

  ////

  explicit WebGLQuery(WebGLContext* webgl);

 private:
  ~WebGLQuery() override;

 public:
  ////

  void BeginQuery(GLenum target, RefPtr<WebGLQuery>& slot);
  void EndQuery();
  Maybe<double> GetQueryParameter(GLenum pname) const;
  void QueryCounter();
};

}  // namespace mozilla

#endif  // WEBGL_QUERY_H_
