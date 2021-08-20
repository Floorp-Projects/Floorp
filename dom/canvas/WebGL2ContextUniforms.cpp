/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "js/Array.h"  // JS::NewArrayObject
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "mozilla/RefPtr.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLProgram.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArray.h"

namespace mozilla {

// -------------------------------------------------------------------------
// Uniform Buffer Objects and Transform Feedback Buffers

Maybe<double> WebGL2Context::GetIndexedParameter(const GLenum pname,
                                                 const uint32_t index) const {
  const FuncScope funcScope(*this, "getIndexedParameter");
  if (IsContextLost()) return {};

  const auto* bindings = &mIndexedUniformBufferBindings;
  const char* limitStr = "MAX_UNIFORM_BUFFER_BINDINGS";
  switch (pname) {
    case LOCAL_GL_UNIFORM_BUFFER_START:
    case LOCAL_GL_UNIFORM_BUFFER_SIZE:
      break;

    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
      bindings = &(mBoundTransformFeedback->mIndexedBindings);
      limitStr = "MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS";
      break;

    default:
      ErrorInvalidEnumInfo("pname", pname);
      return {};
  }

  if (index >= bindings->size()) {
    ErrorInvalidValue("`index` must be < %s.", limitStr);
    return {};
  }
  const auto& binding = (*bindings)[index];

  switch (pname) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case LOCAL_GL_UNIFORM_BUFFER_START:
      return Some(binding.mRangeStart);
      break;

    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case LOCAL_GL_UNIFORM_BUFFER_SIZE:
      return Some(binding.mRangeSize);

    default:
      MOZ_CRASH("impossible");
  }
}

void WebGL2Context::UniformBlockBinding(WebGLProgram& program,
                                        GLuint uniformBlockIndex,
                                        GLuint uniformBlockBinding) {
  const FuncScope funcScope(*this, "uniformBlockBinding");
  if (IsContextLost()) return;

  if (!ValidateObject("program", program)) return;

  program.UniformBlockBinding(uniformBlockIndex, uniformBlockBinding);
}

}  // namespace mozilla
