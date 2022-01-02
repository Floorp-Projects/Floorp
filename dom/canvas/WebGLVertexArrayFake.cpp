/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLVertexArrayFake.h"

#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLVertexArrayFake::WebGLVertexArrayFake(WebGLContext* webgl)
    : WebGLVertexArray(webgl) {}

void WebGLVertexArrayFake::BindVertexArray() {
  // Go through and re-bind all buffers and setup all
  // vertex attribute pointers
  const auto& gl = mContext->gl;

  mContext->mBoundVertexArray = this;

  static_assert(IsBufferTargetLazilyBound(LOCAL_GL_ARRAY_BUFFER));
  static_assert(!IsBufferTargetLazilyBound(LOCAL_GL_ELEMENT_ARRAY_BUFFER));

  const auto fnBind = [&](const GLenum target, WebGLBuffer* const buffer) {
    gl->fBindBuffer(target, buffer ? buffer->mGLName : 0);
  };

  fnBind(LOCAL_GL_ELEMENT_ARRAY_BUFFER, mElementArrayBuffer);

  const bool useDivisor =
      mContext->IsWebGL2() ||
      mContext->IsExtensionEnabled(WebGLExtensionID::ANGLE_instanced_arrays);

  for (const auto i : IntegerRange(mContext->MaxVertexAttribs())) {
    const auto& binding = mBindings[i];
    const auto& desc = mDescs[i];

    if (binding.layout.isArray) {
      gl->fEnableVertexAttribArray(i);
    } else {
      gl->fDisableVertexAttribArray(i);
    }

    if (useDivisor) {
      gl->fVertexAttribDivisor(i, binding.layout.divisor);
    }

    fnBind(LOCAL_GL_ARRAY_BUFFER, binding.buffer);
    DoVertexAttribPointer(*gl, i, desc);
  }

  fnBind(LOCAL_GL_ARRAY_BUFFER, nullptr);
}

}  // namespace mozilla
