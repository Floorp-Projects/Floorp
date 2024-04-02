/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLSampler.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArray.h"

namespace mozilla {

Maybe<double> WebGL2Context::GetParameter(GLenum pname) {
  const FuncScope funcScope(*this, "getParameter");
  // The following cases are handled in WebGLContext::GetParameter():
  //     case LOCAL_GL_MAX_COLOR_ATTACHMENTS:
  //     case LOCAL_GL_MAX_DRAW_BUFFERS:
  //     case LOCAL_GL_DRAW_BUFFERi:

  if (IsContextLost()) return Nothing();

  switch (pname) {
    /* GLboolean */
    case LOCAL_GL_RASTERIZER_DISCARD:
    case LOCAL_GL_SAMPLE_ALPHA_TO_COVERAGE:
    case LOCAL_GL_SAMPLE_COVERAGE: {
      realGLboolean b = 0;
      gl->fGetBooleanv(pname, &b);
      return Some(bool(b));
    }

    case LOCAL_GL_TRANSFORM_FEEDBACK_ACTIVE:
      return Some(mBoundTransformFeedback->mIsActive);
    case LOCAL_GL_TRANSFORM_FEEDBACK_PAUSED:
      return Some(mBoundTransformFeedback->mIsPaused);

    /* GLenum */
    case LOCAL_GL_READ_BUFFER: {
      if (!mBoundReadFramebuffer) return Some(mDefaultFB_ReadBuffer);

      if (!mBoundReadFramebuffer->ColorReadBuffer()) return Some(LOCAL_GL_NONE);

      return Some(mBoundReadFramebuffer->ColorReadBuffer()->mAttachmentPoint);
    }

    case LOCAL_GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
      /* fall through */

    /* GLint */
    case LOCAL_GL_MAX_COMBINED_UNIFORM_BLOCKS:
    case LOCAL_GL_MAX_ELEMENTS_INDICES:
    case LOCAL_GL_MAX_ELEMENTS_VERTICES:
    case LOCAL_GL_MAX_FRAGMENT_INPUT_COMPONENTS:
    case LOCAL_GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
    case LOCAL_GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
    case LOCAL_GL_MAX_PROGRAM_TEXEL_OFFSET:
    case LOCAL_GL_MAX_SAMPLES:
    case LOCAL_GL_MAX_TEXTURE_LOD_BIAS:
    case LOCAL_GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
    case LOCAL_GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
    case LOCAL_GL_MAX_VERTEX_OUTPUT_COMPONENTS:
    case LOCAL_GL_MAX_VERTEX_UNIFORM_BLOCKS:
    case LOCAL_GL_MAX_VERTEX_UNIFORM_COMPONENTS:
    case LOCAL_GL_MIN_PROGRAM_TEXEL_OFFSET: {
      GLint val;
      gl->fGetIntegerv(pname, &val);
      return Some(val);
    }

    case LOCAL_GL_MAX_VARYING_COMPONENTS: {
      // On OS X Core Profile this is buggy.  The spec says that the
      // value is 4 * GL_MAX_VARYING_VECTORS
      GLint val;
      gl->fGetIntegerv(LOCAL_GL_MAX_VARYING_VECTORS, &val);
      return Some(4 * val);
    }

    case LOCAL_GL_MAX_ELEMENT_INDEX:
      // GL_MAX_ELEMENT_INDEX becomes available in GL 4.3 or via ES3
      // compatibility
      if (!gl->IsSupported(gl::GLFeature::ES3_compatibility))
        return Some(UINT32_MAX);

      /*** fall through to fGetInteger64v ***/
      [[fallthrough]];

    case LOCAL_GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
    case LOCAL_GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
    case LOCAL_GL_MAX_UNIFORM_BLOCK_SIZE: {
      GLint64 val;
      gl->fGetInteger64v(pname, &val);
      return Some(static_cast<double>(val));
    }

    /* GLuint64 */
    case LOCAL_GL_MAX_SERVER_WAIT_TIMEOUT: {
      GLuint64 val;
      gl->fGetInteger64v(pname, (GLint64*)&val);
      return Some(static_cast<double>(val));
    }

    default:
      return WebGLContext::GetParameter(pname);
  }
}

}  // namespace mozilla
