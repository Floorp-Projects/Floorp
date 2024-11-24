/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLVertexArrayFake.h"

#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"

namespace mozilla {

static void BindBuffer(gl::GLContext* const gl, const GLenum target,
                       WebGLBuffer* const buffer) {
  gl->fBindBuffer(target, buffer ? buffer->mGLName : 0);
}

WebGLVertexArrayFake::WebGLVertexArrayFake(WebGLContext* webgl)
    : WebGLVertexArray(webgl) {}

void WebGLVertexArray::DoVertexAttrib(const uint32_t index,
                                      const uint32_t vertOffset) const {
  const auto& gl = mContext->gl;

  const bool useDivisor =
      mContext->IsWebGL2() ||
      mContext->IsExtensionEnabled(WebGLExtensionID::ANGLE_instanced_arrays);

  const auto& binding = mBindings.at(index);
  const auto& desc = mDescs.at(index);

  if (binding.layout.isArray) {
    gl->fEnableVertexAttribArray(index);
  } else {
    gl->fDisableVertexAttribArray(index);
  }

  if (useDivisor) {
    gl->fVertexAttribDivisor(index, binding.layout.divisor);
  }

  static_assert(IsBufferTargetLazilyBound(LOCAL_GL_ARRAY_BUFFER));
  BindBuffer(gl, LOCAL_GL_ARRAY_BUFFER, binding.buffer);

  auto desc2 = desc;
  if (!binding.layout.divisor) {
    desc2.byteOffset += binding.layout.byteStride * vertOffset;
  }
  DoVertexAttribPointer(*gl, index, desc2);

  BindBuffer(gl, LOCAL_GL_ARRAY_BUFFER, nullptr);
}

void WebGLVertexArrayFake::BindVertexArray() {
  // Go through and re-bind all buffers and setup all
  // vertex attribute pointers
  const auto& gl = mContext->gl;

  mContext->mBoundVertexArray = this;

  static_assert(IsBufferTargetLazilyBound(LOCAL_GL_ARRAY_BUFFER));
  static_assert(!IsBufferTargetLazilyBound(LOCAL_GL_ELEMENT_ARRAY_BUFFER));

  BindBuffer(gl, LOCAL_GL_ELEMENT_ARRAY_BUFFER, mElementArrayBuffer);

  for (const auto i : IntegerRange(mContext->MaxVertexAttribs())) {
    DoVertexAttrib(i);
  }
}

}  // namespace mozilla
