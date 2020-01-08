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

MaybeWebGLVariant WebGL2Context::GetParameter(GLenum pname) {
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
      return AsSomeVariant(bool(b));
    }

    case LOCAL_GL_TRANSFORM_FEEDBACK_ACTIVE:
      return AsSomeVariant(mBoundTransformFeedback->mIsActive);
    case LOCAL_GL_TRANSFORM_FEEDBACK_PAUSED:
      return AsSomeVariant(mBoundTransformFeedback->mIsPaused);

    /* GLenum */
    case LOCAL_GL_READ_BUFFER: {
      if (!mBoundReadFramebuffer)
        return AsSomeVariant(std::move(mDefaultFB_ReadBuffer));

      if (!mBoundReadFramebuffer->ColorReadBuffer())
        return AsSomeVariant(LOCAL_GL_NONE);

      return AsSomeVariant(std::move(
          mBoundReadFramebuffer->ColorReadBuffer()->mAttachmentPoint));
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
    case LOCAL_GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
    case LOCAL_GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
    case LOCAL_GL_MAX_UNIFORM_BUFFER_BINDINGS:
    case LOCAL_GL_MAX_VERTEX_OUTPUT_COMPONENTS:
    case LOCAL_GL_MAX_VERTEX_UNIFORM_BLOCKS:
    case LOCAL_GL_MAX_VERTEX_UNIFORM_COMPONENTS:
    case LOCAL_GL_MIN_PROGRAM_TEXEL_OFFSET:
    case LOCAL_GL_PACK_ROW_LENGTH:
    case LOCAL_GL_PACK_SKIP_PIXELS:
    case LOCAL_GL_PACK_SKIP_ROWS:
    case LOCAL_GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
    case LOCAL_GL_UNPACK_IMAGE_HEIGHT:
    case LOCAL_GL_UNPACK_ROW_LENGTH: {
      GLint val;
      gl->fGetIntegerv(pname, &val);
      return AsSomeVariant(val);
    }

    case LOCAL_GL_UNPACK_SKIP_IMAGES:
      return AsSomeVariant(mPixelStore.mUnpackSkipImages);

    case LOCAL_GL_UNPACK_SKIP_PIXELS:
      return AsSomeVariant(mPixelStore.mUnpackSkipPixels);

    case LOCAL_GL_UNPACK_SKIP_ROWS:
      return AsSomeVariant(mPixelStore.mUnpackSkipRows);

    case LOCAL_GL_MAX_3D_TEXTURE_SIZE:
      return AsSomeVariant(mGLMax3DTextureSize);

    case LOCAL_GL_MAX_ARRAY_TEXTURE_LAYERS:
      return AsSomeVariant(mGLMaxArrayTextureLayers);

    case LOCAL_GL_MAX_VARYING_COMPONENTS: {
      // On OS X Core Profile this is buggy.  The spec says that the
      // value is 4 * GL_MAX_VARYING_VECTORS
      GLint val;
      gl->fGetIntegerv(LOCAL_GL_MAX_VARYING_VECTORS, &val);
      return AsSomeVariant(4 * val);
    }

    /* GLint64 */
    case LOCAL_GL_MAX_CLIENT_WAIT_TIMEOUT_WEBGL:
      return AsSomeVariant(kMaxClientWaitSyncTimeoutNS);

    case LOCAL_GL_MAX_ELEMENT_INDEX:
      // GL_MAX_ELEMENT_INDEX becomes available in GL 4.3 or via ES3
      // compatibility
      if (!gl->IsSupported(gl::GLFeature::ES3_compatibility))
        return AsSomeVariant(UINT32_MAX);

      /*** fall through to fGetInteger64v ***/
      [[fallthrough]];

    case LOCAL_GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
    case LOCAL_GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
    case LOCAL_GL_MAX_UNIFORM_BLOCK_SIZE: {
      GLint64 val;
      gl->fGetInteger64v(pname, &val);
      return AsSomeVariant(static_cast<double>(val));
    }

    /* GLuint64 */
    case LOCAL_GL_MAX_SERVER_WAIT_TIMEOUT: {
      GLuint64 val;
      gl->fGetInteger64v(pname, (GLint64*)&val);
      return AsSomeVariant(static_cast<double>(val));
    }

    case LOCAL_GL_COPY_READ_BUFFER_BINDING:
      return AsSomeVariant(std::move(mBoundCopyReadBuffer.get()));

    case LOCAL_GL_COPY_WRITE_BUFFER_BINDING:
      return AsSomeVariant(std::move(mBoundCopyWriteBuffer.get()));

    case LOCAL_GL_PIXEL_PACK_BUFFER_BINDING:
      return AsSomeVariant(std::move(mBoundPixelPackBuffer.get()));

    case LOCAL_GL_PIXEL_UNPACK_BUFFER_BINDING:
      return AsSomeVariant(std::move(mBoundPixelUnpackBuffer.get()));

    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
      return AsSomeVariant(std::move(mBoundTransformFeedbackBuffer.get()));

    case LOCAL_GL_UNIFORM_BUFFER_BINDING:
      return AsSomeVariant(std::move(mBoundUniformBuffer.get()));

    // DRAW_FRAMEBUFFER_BINDING is the same as FRAMEBUFFER_BINDING.
    case LOCAL_GL_READ_FRAMEBUFFER_BINDING:
      return AsSomeVariant(std::move(mBoundReadFramebuffer.get()));

    case LOCAL_GL_SAMPLER_BINDING:
      return AsSomeVariant(std::move(mBoundSamplers[mActiveTexture].get()));

    case LOCAL_GL_TEXTURE_BINDING_2D_ARRAY:
      return AsSomeVariant(
          std::move(mBound2DArrayTextures[mActiveTexture].get()));

    case LOCAL_GL_TEXTURE_BINDING_3D:
      return AsSomeVariant(std::move(mBound3DTextures[mActiveTexture].get()));

    case LOCAL_GL_TRANSFORM_FEEDBACK_BINDING: {
      WebGLTransformFeedback* tf = mBoundTransformFeedback;
      if (tf == mDefaultTransformFeedback) {
        tf = nullptr;
      }
      return AsSomeVariant(std::move(tf));
    }

    case LOCAL_GL_VERTEX_ARRAY_BINDING: {
      WebGLVertexArray* vao = (mBoundVertexArray != mDefaultVertexArray)
                                  ? mBoundVertexArray.get()
                                  : nullptr;
      return AsSomeVariant(std::move(vao));
    }

    case LOCAL_GL_VERSION:
      return AsSomeVariant(nsCString("WebGL 2.0"));

    case LOCAL_GL_SHADING_LANGUAGE_VERSION:
      return AsSomeVariant(nsCString("WebGL GLSL ES 3.00"));

    default:
      return WebGLContext::GetParameter(pname);
  }
}

}  // namespace mozilla
