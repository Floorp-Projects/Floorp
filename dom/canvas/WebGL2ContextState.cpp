/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "WebGLContextUtils.h"

namespace mozilla {

JS::Value
WebGL2Context::GetParameter(JSContext* cx, GLenum pname, ErrorResult& rv)
{
  if (IsContextLost())
    return JS::NullValue();

  MakeContextCurrent();

  switch (pname) {
    case LOCAL_GL_MAX_SAMPLES:
    case LOCAL_GL_MAX_UNIFORM_BLOCK_SIZE:
    case LOCAL_GL_MAX_VERTEX_UNIFORM_COMPONENTS: {
      GLint val;
      gl->fGetIntegerv(pname, &val);
      return JS::NumberValue(uint32_t(val));
    }

    case LOCAL_GL_TEXTURE_BINDING_3D:
      return WebGLObjectAsJSValue(cx, mBound3DTextures[mActiveTexture].get(), rv);

      // DRAW_FRAMEBUFFER_BINDING is the same as FRAMEBUFFER_BINDING.
    case LOCAL_GL_READ_FRAMEBUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundReadFramebuffer.get(), rv);

    case LOCAL_GL_PIXEL_PACK_BUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundPixelPackBuffer.get(), rv);

    case LOCAL_GL_PIXEL_UNPACK_BUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundPixelUnpackBuffer.get(), rv);

    case LOCAL_GL_UNIFORM_BUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundUniformBuffer.get(), rv);

    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundTransformFeedbackBuffer.get(), rv);

    case LOCAL_GL_COPY_READ_BUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundCopyReadBuffer.get(), rv);

    case LOCAL_GL_COPY_WRITE_BUFFER_BINDING:
      return WebGLObjectAsJSValue(cx, mBoundCopyWriteBuffer.get(), rv);

    case LOCAL_GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
      return JS::Int32Value(mGLMaxTransformFeedbackSeparateAttribs);

    case LOCAL_GL_VERSION:
      return StringValue(cx, "WebGL 2.0", rv);

    case LOCAL_GL_SHADING_LANGUAGE_VERSION:
      return StringValue(cx, "WebGL GLSL ES 3.00", rv);

    default:
      return WebGLContext::GetParameter(cx, pname, rv);
  }
}

} // namespace mozilla
