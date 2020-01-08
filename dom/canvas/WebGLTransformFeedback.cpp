/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTransformFeedback.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "mozilla/IntegerRange.h"
#include "WebGL2Context.h"
#include "WebGLProgram.h"

namespace mozilla {

WebGLTransformFeedback::WebGLTransformFeedback(WebGLContext* webgl, GLuint tf)
    : WebGLContextBoundObject(webgl),
      mGLName(tf),
      mIndexedBindings(webgl->Limits().maxTransformFeedbackSeparateAttribs),
      mIsPaused(false),
      mIsActive(false) {}

WebGLTransformFeedback::~WebGLTransformFeedback() {
  if (!mContext) return;
  if (mGLName) {
    mContext->gl->fDeleteTransformFeedbacks(1, &mGLName);
  }
}

////////////////////////////////////////

void WebGLTransformFeedback::BeginTransformFeedback(GLenum primMode) {
  if (mIsActive) return mContext->ErrorInvalidOperation("Already active.");

  switch (primMode) {
    case LOCAL_GL_POINTS:
    case LOCAL_GL_LINES:
    case LOCAL_GL_TRIANGLES:
      break;
    default:
      mContext->ErrorInvalidEnum(
          "`primitiveMode` must be one of POINTS, LINES, or"
          " TRIANGLES.");
      return;
  }

  const auto& prog = mContext->mCurrentProgram;
  if (!prog || !prog->IsLinked() ||
      prog->LinkInfo()->componentsPerTFVert.empty()) {
    mContext->ErrorInvalidOperation(
        "Current program not valid for transform"
        " feedback.");
    return;
  }

  const auto& linkInfo = prog->LinkInfo();
  const auto& componentsPerTFVert = linkInfo->componentsPerTFVert;

  size_t minVertCapacity = SIZE_MAX;
  for (size_t i = 0; i < componentsPerTFVert.size(); i++) {
    const auto& indexedBinding = mIndexedBindings[i];
    const auto& componentsPerVert = componentsPerTFVert[i];

    const auto& buffer = indexedBinding.mBufferBinding;
    if (!buffer) {
      mContext->ErrorInvalidOperation(
          "No buffer attached to required transform"
          " feedback index %u.",
          (uint32_t)i);
      return;
    }

    for (const auto iBound : IntegerRange(mIndexedBindings.size())) {
      const auto& bound = mIndexedBindings[iBound].mBufferBinding.get();
      if (iBound != i && buffer == bound) {
        mContext->GenErrorIllegalUse(
            LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER, static_cast<uint32_t>(i),
            LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER, static_cast<uint32_t>(iBound));
        return;
      }
    }

    const size_t vertCapacity = buffer->ByteLength() / 4 / componentsPerVert;
    minVertCapacity = std::min(minVertCapacity, vertCapacity);
  }

  ////

  const auto& gl = mContext->gl;
  gl->fBeginTransformFeedback(primMode);

  ////

  mIsActive = true;
  MOZ_ASSERT(!mIsPaused);

  mActive_Program = prog;
  mActive_PrimMode = primMode;
  mActive_VertPosition = 0;
  mActive_VertCapacity = minVertCapacity;

  ////

  mActive_Program->mNumActiveTFOs++;
}

void WebGLTransformFeedback::EndTransformFeedback() {
  if (!mIsActive) return mContext->ErrorInvalidOperation("Not active.");

  ////

  const auto& gl = mContext->gl;
  gl->fEndTransformFeedback();

  if (gl->WorkAroundDriverBugs()) {
#ifdef XP_MACOSX
    // Multi-threaded GL on mac will generate INVALID_OP in some cases for at
    // least BindBufferBase after an EndTransformFeedback if there is not a
    // flush between the two. Single-threaded GL does not have this issue. This
    // is likely due to not synchronizing client/server state, and erroring in
    // BindBufferBase because the client thinks we're still in transform
    // feedback.
    gl->fFlush();
#endif
  }

  ////

  mIsActive = false;
  mIsPaused = false;

  ////

  mActive_Program->mNumActiveTFOs--;
}

void WebGLTransformFeedback::PauseTransformFeedback() {
  if (!mIsActive || mIsPaused) {
    mContext->ErrorInvalidOperation("Not active or is paused.");
    return;
  }

  ////

  const auto& gl = mContext->gl;
  gl->fPauseTransformFeedback();

  ////

  mIsPaused = true;
}

void WebGLTransformFeedback::ResumeTransformFeedback() {
  if (!mIsPaused) return mContext->ErrorInvalidOperation("Not paused.");

  if (mContext->mCurrentProgram != mActive_Program) {
    mContext->ErrorInvalidOperation("Active program differs from original.");
    return;
  }

  ////

  const auto& gl = mContext->gl;
  gl->fResumeTransformFeedback();

  ////

  MOZ_ASSERT(mIsActive);
  mIsPaused = false;
}

}  // namespace mozilla
