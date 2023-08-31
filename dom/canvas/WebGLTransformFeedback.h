/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_TRANSFORM_FEEDBACK_H_
#define WEBGL_TRANSFORM_FEEDBACK_H_

#include "WebGLObjectModel.h"
#include "WebGLTypes.h"

namespace mozilla {
namespace webgl {
struct CachedDrawFetchLimits;
}

class WebGLTransformFeedback final : public WebGLContextBoundObject {
  friend class ScopedDrawWithTransformFeedback;
  friend class WebGLContext;
  friend class WebGL2Context;
  friend class WebGLProgram;

  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(WebGLTransformFeedback, override)

  friend const webgl::CachedDrawFetchLimits* ValidateDraw(WebGLContext*, GLenum,
                                                          uint32_t);

 public:
  const GLuint mGLName;
  bool mHasBeenBound = false;

 private:
  // GLES 3.0.4 p267, Table 6.24 "Transform Feedback State"
  // It's not yet in the ES3 spec, but the generic TF buffer bind point has been
  // moved to context state, instead of TFO state.
  std::vector<IndexedBufferBinding> mIndexedBindings;
  bool mIsPaused;
  bool mIsActive;
  // Not in state tables:
  RefPtr<WebGLProgram> mActive_Program;
  MOZ_INIT_OUTSIDE_CTOR GLenum mActive_PrimMode;
  MOZ_INIT_OUTSIDE_CTOR size_t mActive_VertPosition;
  MOZ_INIT_OUTSIDE_CTOR size_t mActive_VertCapacity;

 public:
  WebGLTransformFeedback(WebGLContext* webgl, GLuint tf);

 private:
  ~WebGLTransformFeedback() override;

 public:
  bool IsActiveAndNotPaused() const { return mIsActive && !mIsPaused; }

  // GL Funcs
  void BeginTransformFeedback(GLenum primMode);
  void EndTransformFeedback();
  void PauseTransformFeedback();
  void ResumeTransformFeedback();
};

}  // namespace mozilla

#endif  // WEBGL_TRANSFORM_FEEDBACK_H_
