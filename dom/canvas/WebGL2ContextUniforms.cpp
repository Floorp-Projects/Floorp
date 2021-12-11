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

  if (IsExtensionEnabled(WebGLExtensionID::OES_draw_buffers_indexed)) {
    switch (pname) {
      case LOCAL_GL_BLEND_EQUATION_RGB:
      case LOCAL_GL_BLEND_EQUATION_ALPHA:
      case LOCAL_GL_BLEND_SRC_RGB:
      case LOCAL_GL_BLEND_SRC_ALPHA:
      case LOCAL_GL_BLEND_DST_RGB:
      case LOCAL_GL_BLEND_DST_ALPHA:
      case LOCAL_GL_COLOR_WRITEMASK: {
        const auto limit = MaxValidDrawBuffers();
        if (index >= limit) {
          ErrorInvalidValue("`index` (%u) must be < %s (%u)", index,
                            "MAX_DRAW_BUFFERS", limit);
          return {};
        }

        std::array<GLint, 4> data = {};
        gl->fGetIntegeri_v(pname, index, data.data());
        auto val = data[0];
        if (pname == LOCAL_GL_COLOR_WRITEMASK) {
          val = (bool(data[0]) << 0 | bool(data[1]) << 1 | bool(data[2]) << 2 |
                 bool(data[3]) << 3);
        }
        return Some(val);
      }
    }
  }

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
      ErrorInvalidEnumArg("pname", pname);
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
