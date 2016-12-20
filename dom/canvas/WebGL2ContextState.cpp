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

JS::Value
WebGL2Context::GetParameter(JSContext* cx, GLenum pname, ErrorResult& rv)
{
  // The following cases are handled in WebGLContext::GetParameter():
  //     case LOCAL_GL_MAX_COLOR_ATTACHMENTS:
  //     case LOCAL_GL_MAX_DRAW_BUFFERS:
  //     case LOCAL_GL_DRAW_BUFFERi:

  if (IsContextLost())
    return JS::NullValue();

  MakeContextCurrent();

  switch (pname) {
    /* GLboolean */
    case LOCAL_GL_RASTERIZER_DISCARD:
    case LOCAL_GL_SAMPLE_ALPHA_TO_COVERAGE:
    case LOCAL_GL_SAMPLE_COVERAGE: {
      realGLboolean b = 0;
      gl->fGetBooleanv(pname, &b);
      return JS::BooleanValue(bool(b));
    }

    case LOCAL_GL_TRANSFORM_FEEDBACK_ACTIVE:
      return JS::BooleanValue(mBoundTransformFeedback->mIsActive);
    case LOCAL_GL_TRANSFORM_FEEDBACK_PAUSED:
      return JS::BooleanValue(mBoundTransformFeedback->mIsPaused);

    /* GLenum */
    case LOCAL_GL_READ_BUFFER: {
      if (!mBoundReadFramebuffer)
        return JS::Int32Value(gl->Screen()->GetReadBufferMode());

      if (!mBoundReadFramebuffer->ColorReadBuffer())
        return JS::Int32Value(LOCAL_GL_NONE);

      return JS::Int32Value(mBoundReadFramebuffer->ColorReadBuffer()->mAttachmentPoint);
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
      return JS::Int32Value(val);
    }

    case LOCAL_GL_UNPACK_SKIP_IMAGES:
      return JS::Int32Value(mPixelStore_UnpackSkipImages);

    case LOCAL_GL_UNPACK_SKIP_PIXELS:
      return JS::Int32Value(mPixelStore_UnpackSkipPixels);

    case LOCAL_GL_UNPACK_SKIP_ROWS:
      return JS::Int32Value(mPixelStore_UnpackSkipRows);

    case LOCAL_GL_MAX_3D_TEXTURE_SIZE:
      return JS::Int32Value(mImplMax3DTextureSize);

    case LOCAL_GL_MAX_ARRAY_TEXTURE_LAYERS:
      return JS::Int32Value(mImplMaxArrayTextureLayers);

    case LOCAL_GL_MAX_VARYING_COMPONENTS: {
      // On OS X Core Profile this is buggy.  The spec says that the
      // value is 4 * GL_MAX_VARYING_VECTORS
      GLint val;
      gl->fGetIntegerv(LOCAL_GL_MAX_VARYING_VECTORS, &val);
      return JS::Int32Value(4*val);
    }

    /* GLint64 */
    case LOCAL_GL_MAX_CLIENT_WAIT_TIMEOUT_WEBGL:
      return JS::NumberValue(kMaxClientWaitSyncTimeoutNS);

    case LOCAL_GL_MAX_ELEMENT_INDEX:
      // GL_MAX_ELEMENT_INDEX becomes available in GL 4.3 or via ES3
      // compatibility
      if (!gl->IsSupported(gl::GLFeature::ES3_compatibility))
        return JS::NumberValue(UINT32_MAX);

      /*** fall through to fGetInteger64v ***/
      MOZ_FALLTHROUGH;

    case LOCAL_GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
    case LOCAL_GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
    case LOCAL_GL_MAX_UNIFORM_BLOCK_SIZE: {
      GLint64 val;
      gl->fGetInteger64v(pname, &val);
      return JS::DoubleValue(static_cast<double>(val));
    }


    /* GLuint64 */
    case LOCAL_GL_MAX_SERVER_WAIT_TIMEOUT: {
      GLuint64 val;
      gl->fGetInteger64v(pname, (GLint64*) &val);
      return JS::DoubleValue(static_cast<double>(val));
    }

    case LOCAL_GL_COPY_READ_BUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundCopyReadBuffer.get(), rv);

    case LOCAL_GL_COPY_WRITE_BUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundCopyWriteBuffer.get(), rv);

    case LOCAL_GL_PIXEL_PACK_BUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundPixelPackBuffer.get(), rv);

    case LOCAL_GL_PIXEL_UNPACK_BUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundPixelUnpackBuffer.get(), rv);

    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
      {
        const auto& tf = mBoundTransformFeedback;
        return WebGLObjectAsJSValue(cx, tf->mGenericBufferBinding.get(), rv);
      }

    case LOCAL_GL_UNIFORM_BUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundUniformBuffer.get(), rv);

    // DRAW_FRAMEBUFFER_BINDING is the same as FRAMEBUFFER_BINDING.
    case LOCAL_GL_READ_FRAMEBUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundReadFramebuffer.get(), rv);

    case LOCAL_GL_SAMPLER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundSamplers[mActiveTexture].get(), rv);

    case LOCAL_GL_TEXTURE_BINDING_2D_ARRAY:
      return WebGLObjectAsJSValue(cx, mBound2DArrayTextures[mActiveTexture].get(), rv);

    case LOCAL_GL_TEXTURE_BINDING_3D:
      return WebGLObjectAsJSValue(cx, mBound3DTextures[mActiveTexture].get(), rv);

    case LOCAL_GL_TRANSFORM_FEEDBACK_BINDING:
      {
        const WebGLTransformFeedback* tf = mBoundTransformFeedback;
        if (tf == mDefaultTransformFeedback) {
          tf = nullptr;
        }
        return WebGLObjectAsJSValue(cx, tf, rv);
      }

    case LOCAL_GL_VERTEX_ARRAY_BINDING: {
      WebGLVertexArray* vao =
        (mBoundVertexArray != mDefaultVertexArray) ? mBoundVertexArray.get() : nullptr;
      return WebGLObjectAsJSValue(cx, vao, rv);
    }

    case LOCAL_GL_VERSION:
      return StringValue(cx, "WebGL 2.0", rv);

    case LOCAL_GL_SHADING_LANGUAGE_VERSION:
      return StringValue(cx, "WebGL GLSL ES 3.00", rv);

    default:
      return WebGLContext::GetParameter(cx, pname, rv);
  }
}

} // namespace mozilla
