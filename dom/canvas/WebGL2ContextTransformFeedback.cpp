/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "WebGLActiveInfo.h"
#include "WebGLProgram.h"
#include "WebGLTransformFeedback.h"
#include "GLContext.h"

namespace mozilla {

// -------------------------------------------------------------------------
// Transform Feedback

already_AddRefed<WebGLTransformFeedback>
WebGL2Context::CreateTransformFeedback() {
  const FuncScope funcScope(*this, "createTransformFeedback");
  if (IsContextLost()) return nullptr;

  GLuint tf = 0;
  gl->fGenTransformFeedbacks(1, &tf);

  RefPtr<WebGLTransformFeedback> ret = new WebGLTransformFeedback(this, tf);
  return ret.forget();
}

void WebGL2Context::DeleteTransformFeedback(WebGLTransformFeedback* tf) {
  const FuncScope funcScope(*this, "deleteTransformFeedback");
  if (!ValidateDeleteObject(tf)) return;

  if (tf->mIsActive) {
    ErrorInvalidOperation("Cannot delete active transform feedbacks.");
    return;
  }

  if (mBoundTransformFeedback == tf) {
    BindTransformFeedback(LOCAL_GL_TRANSFORM_FEEDBACK, nullptr);
  }

  tf->RequestDelete();
}

bool WebGL2Context::IsTransformFeedback(
    const WebGLTransformFeedback* const obj) {
  const FuncScope funcScope(*this, "isTransformFeedback");
  if (!ValidateIsObject(obj)) return false;

  if (obj->IsDeleteRequested()) return false;

  return obj->mHasBeenBound;
}

void WebGL2Context::BindTransformFeedback(GLenum target,
                                          WebGLTransformFeedback* tf) {
  const FuncScope funcScope(*this, "bindTransformFeedback");
  if (IsContextLost()) return;

  if (target != LOCAL_GL_TRANSFORM_FEEDBACK)
    return ErrorInvalidEnum("`target` must be TRANSFORM_FEEDBACK.");

  if (tf && !ValidateObject("tf", *tf)) return;

  if (mBoundTransformFeedback->mIsActive &&
      !mBoundTransformFeedback->mIsPaused) {
    ErrorInvalidOperation(
        "Currently bound transform feedback is active and not"
        " paused.");
    return;
  }

  ////

  mBoundTransformFeedback = (tf ? tf : mDefaultTransformFeedback);

  gl->fBindTransformFeedback(target, mBoundTransformFeedback->mGLName);

  if (mBoundTransformFeedback) {
    mBoundTransformFeedback->mHasBeenBound = true;
  }
}

void WebGL2Context::BeginTransformFeedback(GLenum primMode) {
  const FuncScope funcScope(*this, "beginTransformFeedback");
  if (IsContextLost()) return;

  mBoundTransformFeedback->BeginTransformFeedback(primMode);
}

void WebGL2Context::EndTransformFeedback() {
  const FuncScope funcScope(*this, "endTransformFeedback");
  if (IsContextLost()) return;

  mBoundTransformFeedback->EndTransformFeedback();
}

void WebGL2Context::PauseTransformFeedback() {
  const FuncScope funcScope(*this, "pauseTransformFeedback");
  if (IsContextLost()) return;

  mBoundTransformFeedback->PauseTransformFeedback();
}

void WebGL2Context::ResumeTransformFeedback() {
  const FuncScope funcScope(*this, "resumeTransformFeedback");
  if (IsContextLost()) return;

  mBoundTransformFeedback->ResumeTransformFeedback();
}

void WebGL2Context::TransformFeedbackVaryings(
    WebGLProgram& program, const nsTArray<nsString>& varyings,
    GLenum bufferMode) {
  const FuncScope funcScope(*this, "transformFeedbackVaryings");
  if (IsContextLost()) return;

  if (!ValidateObject("program", program)) return;

  program.TransformFeedbackVaryings(varyings, bufferMode);
}

Maybe<WebGLActiveInfo> WebGL2Context::GetTransformFeedbackVarying(
    const WebGLProgram& program, GLuint index) {
  const FuncScope funcScope(*this, "getTransformFeedbackVarying");
  if (IsContextLost()) return Nothing();

  if (!ValidateObject("program", program)) return Nothing();

  return program.GetTransformFeedbackVarying(index);
}

}  // namespace mozilla
